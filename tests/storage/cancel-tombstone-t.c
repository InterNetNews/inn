/* Unit tests for SMcanceltombstone (storage/interface.c).
 *
 * Verifies the multi-writer append protocol used by innd's ARTcancel
 * and sm's -r path: the file format is one TokenToText() per line; the
 * write is gated on innconf->expiretombstone; TOKEN_EMPTY is a no-op;
 * concurrent appenders use fcntl POSIX locks via inn_lock_file. */

#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/storage.h"
#include "tap/basic.h"


/* Build a synthetic token whose bytes encode n.  Type/class chosen so
 * IsToken accepts the textual form (the type byte must be a valid hex
 * digit pair when round-tripped). */
static TOKEN
make_token(unsigned char type, unsigned long n)
{
    TOKEN t;

    memset(&t, 0, sizeof(t));
    t.type = type;
    t.class = 0;
    t.token[0] = (n >> 24) & 0xff;
    t.token[1] = (n >> 16) & 0xff;
    t.token[2] = (n >> 8) & 0xff;
    t.token[3] = n & 0xff;
    return t;
}


/* Read the entire file into a malloc'd buffer.  Returns NULL on
 * error.  *out_size set to bytes read. */
static char *
slurp(const char *path, size_t *out_size)
{
    FILE *f;
    struct stat sb;
    char *buf;

    *out_size = 0;
    if (stat(path, &sb) < 0)
        return NULL;
    f = fopen(path, "r");
    if (f == NULL)
        return NULL;
    buf = xmalloc(sb.st_size + 1);
    if (fread(buf, 1, sb.st_size, f) != (size_t) sb.st_size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sb.st_size] = '\0';
    *out_size = sb.st_size;
    fclose(f);
    return buf;
}


/* Count lines in buf (number of '\n' bytes). */
static size_t
count_lines(const char *buf, size_t len)
{
    size_t i, n = 0;
    for (i = 0; i < len; i++)
        if (buf[i] == '\n')
            n++;
    return n;
}


int
main(void)
{
    char tmpdir[64];
    char *cancels_path;
    TOKEN t1, t2, t3, empty;
    char *contents;
    size_t size;
    struct stat sb;

    test_init(15);

    /* Set up a temporary pathdb. */
    strlcpy(tmpdir, "cancel-tombstone-XXXXXX", sizeof(tmpdir));
    if (mkdtemp(tmpdir) == NULL)
        sysbail("can't create temp directory");

    /* Initialize a minimal innconf so SMcanceltombstone has somewhere
     * to write.  We need the fields it consults: expiretombstone,
     * groupbaseexpiry (both required), and pathdb.  Other code paths
     * in this binary read additional fields, so allocate via xcalloc
     * to zero everything. */
    innconf = xcalloc(1, sizeof(*innconf));
    innconf->pathdb = xstrdup(tmpdir);
    innconf->expiretombstone = true;
    innconf->groupbaseexpiry = true;

    cancels_path = concatpath(tmpdir, "cancels.tombstone");

    t1 = make_token(3, 0xdeadbeef);
    t2 = make_token(3, 0xcafebabe);
    t3 = make_token(3, 0x12345678);
    memset(&empty, 0, sizeof(empty));
    empty.type = TOKEN_EMPTY;

    /* 1. First call creates the file with one line. */
    SMcanceltombstone(t1);
    contents = slurp(cancels_path, &size);
    ok(1, contents != NULL);
    ok(2, count_lines(contents, size) == 1);
    ok(3, contents != NULL && strstr(contents, TokenToText(t1)) != NULL);
    free(contents);

    /* 2. Second call appends; file now has two distinct token lines. */
    SMcanceltombstone(t2);
    contents = slurp(cancels_path, &size);
    ok(4, contents != NULL && count_lines(contents, size) == 2);
    ok(5, contents != NULL && strstr(contents, TokenToText(t1)) != NULL
              && strstr(contents, TokenToText(t2)) != NULL);
    free(contents);

    /* 3. expiretombstone=false: third call is a no-op. */
    innconf->expiretombstone = false;
    SMcanceltombstone(t3);
    contents = slurp(cancels_path, &size);
    ok(6, contents != NULL && count_lines(contents, size) == 2);
    ok(7, contents != NULL && strstr(contents, TokenToText(t3)) == NULL);
    free(contents);
    innconf->expiretombstone = true;

    /* 4. TOKEN_EMPTY: no-op (not a real article). */
    SMcanceltombstone(empty);
    contents = slurp(cancels_path, &size);
    ok(8, contents != NULL && count_lines(contents, size) == 2);
    free(contents);

    /* 5. After write completes, the lock has been released; another
     * process (here, just us reusing the same path) can acquire the
     * exclusive write lock without contention. */
    {
        int fd = open(cancels_path, O_RDWR);
        ok(9, fd >= 0);
        if (fd >= 0) {
            ok(10, inn_lock_file(fd, INN_LOCK_WRITE, false));
            close(fd);
        } else {
            ok(10, false);
        }
    }

    /* 6. Each line round-trips: parsing each line through TextToToken
     * yields the original TOKEN.  Read the file and verify. */
    contents = slurp(cancels_path, &size);
    if (contents != NULL) {
        char *p = contents;
        char *line_end;
        TOKEN parsed;
        bool found_t1 = false;
        bool found_t2 = false;

        while ((line_end = strchr(p, '\n')) != NULL) {
            *line_end = '\0';
            if (IsToken(p)) {
                parsed = TextToToken(p);
                if (memcmp(&parsed, &t1, sizeof(TOKEN)) == 0)
                    found_t1 = true;
                if (memcmp(&parsed, &t2, sizeof(TOKEN)) == 0)
                    found_t2 = true;
            }
            p = line_end + 1;
        }
        ok(11, found_t1);
        ok(12, found_t2);
        free(contents);
    } else {
        ok(11, false);
        ok(12, false);
    }

    /* 7. File mode should be 0664 (created with O_CREAT, mode 0664).
     * Check the stored mode bits.  Allow umask-stripped variants. */
    if (stat(cancels_path, &sb) == 0) {
        mode_t mode = sb.st_mode & 0777;
        /* The file was created with mode 0664 but umask may have
         * stripped write bits.  Verify at least owner+group readable. */
        ok(13, (mode & 0640) == 0640);
    } else {
        ok(13, false);
    }

    /* 8. Calling SMcanceltombstone with an unwritable pathdb does
     * not crash (best-effort failure path), does not create any
     * file outside the configured location, and returns false.
     * Simulate by pointing pathdb at a non-existent directory. */
    {
        char *saved_pathdb = innconf->pathdb;
        char *bad_path;
        struct stat sb_bad;
        bool result;
        innconf->pathdb = xstrdup("/nonexistent-tombstone-test-dir");
        result = SMcanceltombstone(t3);
        bad_path = concatpath(innconf->pathdb, "cancels.tombstone");
        ok(14, !result && stat(bad_path, &sb_bad) < 0
                   && errno == ENOENT);
        free(bad_path);
        free(innconf->pathdb);
        innconf->pathdb = saved_pathdb;
    }

    /* Cleanup. */
    if (unlink(cancels_path) < 0 && errno != ENOENT)
        sysdiag("can't unlink %s", cancels_path);
    free(cancels_path);
    if (rmdir(tmpdir) < 0)
        sysdiag("can't rmdir %s", tmpdir);
    /* Verify cleanup actually removed the directory. */
    ok(15, stat(tmpdir, &sb) < 0 && errno == ENOENT);
    free(innconf->pathdb);
    free(innconf);

    return 0;
}
