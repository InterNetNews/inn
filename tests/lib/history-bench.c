/*
**  Benchmark hisv6 and hissqlite history backends.
**
**  This is intentionally not part of the TAP test suite.  The default run is
**  sized for large local performance testing (100M entries) and can consume a
**  lot of time and disk.
**
**  Fairness / representativeness:
**
**  Both methods are opened exactly as innd opens them, including the in-core
**  index hint (HIS_MMAP when INND_DBZINCORE, else HIS_ONDISK).  Without it,
**  hisv6 would run dbz fully on disk (INCORE_NO plus writethrough), slower
**  than production, while hissqlite, which ignores the storage hint flags,
**  would be unaffected.
**
**  Tuning comes from inn.conf, so each backend uses the operator's real
**  settings.  Point at a config with the INNCONF environment variable:
**
**      INNCONF=/path/to/inn.conf ./history-bench -n 100M
**
**  If neither $INNCONF nor the compiled default inn.conf is present, both
**  backends fall back to their built-in defaults (which equal the inn.conf
**  defaults), so the comparison stays fair either way.  The knobs that most
**  affect these results:
**
**    hissqlitecachesize     writer page cache (KB).  The default (64 MB) is
**                           far smaller than a 100M-entry B-tree, so
**                           random-key inserts thrash; raise it to isolate
**                           "SQLite is slower at random-key writes" from
**                           "under-cached".
**    hissqlitemmapsize      SQLite mmap window (bytes).  Default 0 (no mmap)
**                           reads the DB via pread; set it to compare in-core
**                           against hisv6's mmap'd index on equal footing.
**    hissqlitepagesize      B-tree page size (bytes); affects tree depth and
**                           per-insert WAL write amplification.
**
**  Note the write phase models innd's steady-state, one-autocommit-per-article
**  path, not a makehistory rebuild: bulk loading uses the batched converter
**  (hissqlite-convert.c) and is faster than the per-row figure here.
**
**  Written by Kevin Bowling in 2026.
*/

#include "portable/system.h"

#include "inn/history.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/options.h"
#include "inn/storage.h"

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define DEFAULT_ENTRIES ((uint64_t) 100000000)
#define DEFAULT_RANDOM  ((uint64_t) 1000000)
#define DEFAULT_SYNC    ((uint64_t) 10000)
#define PROGRESS_STEPS  20
#define BASE_TIME       ((time_t) 1700000000)

/*
**  Open hisv6 the way innd does, not with a bare HIS_RDWR/HIS_RDONLY.  Without
**  an in-core hint, hisv6 hands dbz pag_incore = exists_incore = INCORE_NO
**  with writethrough on, its slowest fully-on-disk index mode, so every check
**  and lookup hits the disk and writes thrash the on-disk index.  innd instead
**  opens with HIS_MMAP (INND_DBZINCORE defaults to 1; HIS_ONDISK otherwise),
**  keeping the index mmap()ed in core.  hissqlite ignores these storage hint
**  flags (its journal/cache behaviour is fixed), so passing them does not
**  change hissqlite; it only stops the benchmark from unfairly crippling
**  hisv6.
*/
#define HIS_INCORE_HINT (INND_DBZINCORE ? HIS_MMAP : HIS_ONDISK)

struct bench_config {
    uint64_t entries;
    uint64_t random_lookups;
    uint64_t sync_every;
    const char *root;
    bool keep;
    const char **methods;
    size_t method_count;
};

struct bench_result {
    const char *method;
    uint64_t entries;
    uint64_t random_lookups;
    double write_seconds;
    double sync_seconds;
    double close_seconds;
    double open_ro_seconds;
    double check_seconds;
    double walk_seconds;
    double lookup_seconds;
    double missing_seconds;
    uint64_t bytes;
    const char *path;
    bool skipped;
};

static double
now_seconds(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) < 0)
        sysdie("gettimeofday failed");
    return (double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0);
}

static uint64_t
parse_count(const char *value)
{
    char *end;
    uint64_t count;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value)
        die("invalid count: %s", value);
    count = (uint64_t) parsed;
    if (*end != '\0' && end[1] != '\0')
        die("invalid count suffix: %s", value);
    if (*end == 'k' || *end == 'K')
        count *= 1000;
    else if (*end == 'm' || *end == 'M')
        count *= 1000 * 1000;
    else if (*end == 'g' || *end == 'G')
        count *= 1000 * 1000 * 1000;
    else if (*end != '\0')
        die("invalid count suffix: %s", value);
    if (count == 0)
        die("count must be greater than zero");
    return count;
}

__attribute__((__noreturn__)) static void
usage(int status)
{
    FILE *out;

    out = status == 0 ? stdout : stderr;
    fprintf(out, "usage: history-bench [-k] [-d dir] [-n entries] "
                 "[-r random-lookups] [-s sync-every] [method ...]\n");
    fprintf(out, "\n");
    fprintf(out, "Default: -n 100M -r 1M -s 10K hisv6 hissqlite\n");
    fprintf(out, "Counts accept K, M, and G decimal suffixes.\n");
    fprintf(out, "Use -s 0 to disable periodic HISsync during writes.\n");
    fprintf(out, "Benchmark data is removed unless -k is given.\n");
    exit(status);
}

static char *
join_path(const char *base, const char *name)
{
    size_t len;
    char *path;

    len = strlen(base) + strlen(name) + 2;
    path = (char *) xmalloc(len);
    snprintf(path, len, "%s/%s", base, name);
    return path;
}

static void
make_msgid(char *buf, size_t len, uint64_t n)
{
    snprintf(buf, len, "<%020llu@history-bench.invalid>",
             (unsigned long long) n);
}

static void
make_token(TOKEN *token, uint64_t n)
{
    /* 64-bit golden-ratio constant (floor(2^64 / phi)); used here purely as a
       fixed XOR mask so the two halves of the synthetic token differ rather
       than both being the counter. */
    const uint64_t golden_ratio = UINT64_C(0x9e3779b97f4a7c15);
    uint64_t hi, lo;

    memset(token, 0, sizeof(*token));
    token->type = 1;
    token->class = (STORAGECLASS) (n & 0xff);
    hi = n;
    lo = n ^ golden_ratio;
    memcpy(token->token, &hi, sizeof(hi));
    memcpy(token->token + sizeof(hi), &lo, sizeof(lo));
}

static void
print_progress(const char *method, const char *phase, uint64_t done,
               uint64_t total)
{
    double pct;

    if (total == 0)
        return;
    pct = ((double) done * 100.0) / (double) total;
    fprintf(stderr, "%s %-12s %6.2f%% (%llu/%llu)\n", method, phase, pct,
            (unsigned long long) done, (unsigned long long) total);
}

static bool
progress_due(uint64_t i, uint64_t total)
{
    uint64_t step;

    if (total < PROGRESS_STEPS)
        step = 1;
    else
        step = total / PROGRESS_STEPS;
    return ((i + 1) == total || ((i + 1) % step) == 0);
}

static uint64_t
lcg_next(uint64_t *state)
{
    /* Knuth's MMIX 64-bit linear congruential generator (also the PCG
       defaults): state = state * multiplier + increment.  A cheap, adequate
       PRNG for the benchmark's pseudo-random key sequence. */
    const uint64_t multiplier = UINT64_C(6364136223846793005);
    const uint64_t increment = UINT64_C(1442695040888963407);

    *state = *state * multiplier + increment;
    return *state;
}

static void
sum_tree(const char *path, uint64_t *bytes)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;

    if (lstat(path, &st) < 0)
        return;
    if (S_ISREG(st.st_mode)) {
        if (st.st_size > 0)
            *bytes += (uint64_t) st.st_size;
        return;
    }
    if (!S_ISDIR(st.st_mode))
        return;
    dir = opendir(path);
    if (dir == NULL)
        return;
    while ((entry = readdir(dir)) != NULL) {
        char *child;

        if (strcmp(entry->d_name, ".") == 0
            || strcmp(entry->d_name, "..") == 0)
            continue;
        child = join_path(path, entry->d_name);
        sum_tree(child, bytes);
        free(child);
    }
    closedir(dir);
}

static void
remove_tree(const char *path)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;

    if (lstat(path, &st) < 0)
        return;
    if (S_ISDIR(st.st_mode)) {
        dir = opendir(path);
        if (dir != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                char *child;

                if (strcmp(entry->d_name, ".") == 0
                    || strcmp(entry->d_name, "..") == 0)
                    continue;
                child = join_path(path, entry->d_name);
                remove_tree(child);
                free(child);
            }
            closedir(dir);
        }
        if (rmdir(path) < 0)
            syswarn("cannot remove %s", path);
    } else if (unlink(path) < 0) {
        syswarn("cannot remove %s", path);
    }
}

static struct history *
create_history(const char *method, const char *histpath, uint64_t entries)
{
    struct history *h;
    size_t npairs;

    h = HISopen(NULL, method, HIS_CREAT | HIS_RDWR | HIS_INCORE_HINT);
    if (h == NULL)
        return NULL;
    npairs = (size_t) entries;
    if ((uint64_t) npairs != entries)
        npairs = (size_t) -1;
    if (!HISctl(h, HISCTLS_NPAIRS, &npairs))
        die("%s: cannot set HISCTLS_NPAIRS: %s", method, HISerror(h));
    if (!HISctl(h, HISCTLS_PATH, (void *) histpath))
        die("%s: cannot set history path %s: %s", method, histpath,
            HISerror(h));
    return h;
}

/* HISwalk callback: count every entry.  HISwalk is the full-history scan that
   feeds the expireover bloom-filter rebuild and the migration converter. */
static bool
walk_count(void *cookie, const HASH *hash UNUSED, time_t arrived UNUSED,
           time_t posted UNUSED, time_t expires UNUSED,
           const TOKEN *token UNUSED)
{
    ++*(uint64_t *) cookie;
    return true;
}

static void
benchmark_method(const struct bench_config *config, const char *method,
                 struct bench_result *result)
{
    char *method_dir;
    char *histpath;
    struct history *h;
    TOKEN token, got;
    char msgid[64];
    uint64_t i, misses;
    uint64_t state;
    double start;

    /* Arbitrary but fixed PRNG seeds (any value works): a fixed seed makes
       each phase replay the same pseudo-random order on every run and for
       every method, so the comparison is apples-to-apples; the two phases use
       distinct seeds so they probe in different orders. */
    const uint64_t lookup_seed = UINT64_C(0x123456789abcdef0);
    const uint64_t missing_seed = UINT64_C(0xfedcba9876543210);

    memset(result, 0, sizeof(*result));
    result->method = method;
    result->entries = config->entries;
    result->random_lookups = config->random_lookups;

    method_dir = join_path(config->root, method);
    histpath = join_path(method_dir, "history");
    if (mkdir(method_dir, 0777) < 0)
        sysdie("cannot create %s", method_dir);
    result->path = method_dir;

    fprintf(stderr, "%s create/write %llu entries at %s\n", method,
            (unsigned long long) config->entries, histpath);
    start = now_seconds();
    h = create_history(method, histpath, config->entries);
    if (h == NULL) {
        fprintf(stderr, "%s skipped: %s\n", method,
                HISerror(h) != NULL ? HISerror(h) : "method unavailable");
        result->skipped = true;
        free(histpath);
        return;
    }
    for (i = 0; i < config->entries; i++) {
        make_msgid(msgid, sizeof(msgid), i);
        make_token(&token, i);
        if (!HISwrite(h, msgid, BASE_TIME + (time_t) i, BASE_TIME + (time_t) i,
                      0, &token))
            die("%s: HISwrite %llu failed: %s", method, (unsigned long long) i,
                HISerror(h));
        if (config->sync_every != 0 && (i + 1) % config->sync_every == 0)
            if (!HISsync(h))
                die("%s: periodic HISsync at %llu failed: %s", method,
                    (unsigned long long) (i + 1), HISerror(h));
        if (progress_due(i, config->entries))
            print_progress(method, "write", i + 1, config->entries);
    }
    result->write_seconds = now_seconds() - start;

    start = now_seconds();
    if (!HISsync(h))
        die("%s: HISsync failed: %s", method, HISerror(h));
    result->sync_seconds = now_seconds() - start;

    start = now_seconds();
    if (!HISclose(h))
        die("%s: HISclose failed", method);
    result->close_seconds = now_seconds() - start;

    start = now_seconds();
    h = HISopen(histpath, method, HIS_RDONLY | HIS_INCORE_HINT);
    if (h == NULL)
        die("%s: read-only reopen failed", method);
    result->open_ro_seconds = now_seconds() - start;

    fprintf(stderr, "%s sequential check %llu entries\n", method,
            (unsigned long long) config->entries);
    misses = 0;
    start = now_seconds();
    for (i = 0; i < config->entries; i++) {
        make_msgid(msgid, sizeof(msgid), i);
        if (!HIScheck(h, msgid))
            misses++;
        if (progress_due(i, config->entries))
            print_progress(method, "check", i + 1, config->entries);
    }
    result->check_seconds = now_seconds() - start;
    if (misses != 0)
        die("%s: sequential check missed %llu entries", method,
            (unsigned long long) misses);

    fprintf(stderr, "%s walk %llu entries\n", method,
            (unsigned long long) config->entries);
    {
        uint64_t walked = 0;

        /* NULL reason: a read-only walk must not pause innd.  hisv6 calls
           ICCpause(reason) at EOF when reason is non-NULL, to catch stragglers
           during a rebuild, there is no server here, and hissqlite ignores
           reason regardless. */
        start = now_seconds();
        if (!HISwalk(h, NULL, &walked, walk_count))
            die("%s: HISwalk failed: %s", method, HISerror(h));
        result->walk_seconds = now_seconds() - start;
        if (walked != config->entries)
            die("%s: walk visited %llu of %llu entries", method,
                (unsigned long long) walked,
                (unsigned long long) config->entries);
    }

    fprintf(stderr, "%s random lookup %llu entries\n", method,
            (unsigned long long) config->random_lookups);
    state = lookup_seed;
    misses = 0;
    start = now_seconds();
    for (i = 0; i < config->random_lookups; i++) {
        uint64_t n;

        n = lcg_next(&state) % config->entries;
        make_msgid(msgid, sizeof(msgid), n);
        memset(&got, 0, sizeof(got));
        if (!HISlookup(h, msgid, NULL, NULL, NULL, &got)
            || got.type == TOKEN_EMPTY)
            misses++;
        if (progress_due(i, config->random_lookups))
            print_progress(method, "lookup", i + 1, config->random_lookups);
    }
    result->lookup_seconds = now_seconds() - start;
    if (misses != 0)
        die("%s: random lookup missed %llu entries", method,
            (unsigned long long) misses);

    fprintf(stderr, "%s missing check %llu entries\n", method,
            (unsigned long long) config->random_lookups);
    state = missing_seed;
    misses = 0;
    start = now_seconds();
    for (i = 0; i < config->random_lookups; i++) {
        uint64_t n;

        n = config->entries + (lcg_next(&state) % config->entries);
        make_msgid(msgid, sizeof(msgid), n);
        if (HIScheck(h, msgid))
            misses++;
        if (progress_due(i, config->random_lookups))
            print_progress(method, "missing", i + 1, config->random_lookups);
    }
    result->missing_seconds = now_seconds() - start;
    if (misses != 0)
        die("%s: missing check found %llu unexpected entries", method,
            (unsigned long long) misses);

    if (!HISclose(h))
        die("%s: final HISclose failed", method);

    result->bytes = 0;
    sum_tree(method_dir, &result->bytes);
    free(histpath);
}

static double
rate(uint64_t count, double seconds)
{
    if (seconds <= 0.0)
        return 0.0;
    return (double) count / seconds;
}

static void
print_result(const struct bench_result *result)
{
    if (result->skipped) {
        printf("%-10s skipped\n", result->method);
        return;
    }
    printf("%-10s entries=%llu bytes=%llu path=%s\n", result->method,
           (unsigned long long) result->entries,
           (unsigned long long) result->bytes, result->path);
    printf("  write:   %8.3fs %12.0f entries/s\n", result->write_seconds,
           rate(result->entries, result->write_seconds));
    printf("  sync:    %8.3fs\n", result->sync_seconds);
    printf("  close:   %8.3fs\n", result->close_seconds);
    printf("  open-ro: %8.3fs\n", result->open_ro_seconds);
    printf("  check:   %8.3fs %12.0f entries/s\n", result->check_seconds,
           rate(result->entries, result->check_seconds));
    printf("  walk:    %8.3fs %12.0f entries/s\n", result->walk_seconds,
           rate(result->entries, result->walk_seconds));
    printf("  lookup:  %8.3fs %12.0f entries/s\n", result->lookup_seconds,
           rate(result->random_lookups, result->lookup_seconds));
    printf("  missing: %8.3fs %12.0f entries/s\n", result->missing_seconds,
           rate(result->random_lookups, result->missing_seconds));
}

static void
print_csv_header(void)
{
    printf("\nmethod,entries,random_lookups,bytes,write_s,write_per_s,sync_s,"
           "close_s,open_ro_s,check_s,check_per_s,walk_s,walk_per_s,"
           "lookup_s,lookup_per_s,missing_s,missing_per_s,path\n");
}

static void
print_csv(const struct bench_result *result)
{
    if (result->skipped)
        return;
    printf(
        "%s,%llu,%llu,%llu,%.6f,%.2f,%.6f,%.6f,%.6f,%.6f,%.2f,%.6f,%.2f,"
        "%.6f,%.2f,%.6f,%.2f,%s\n",
        result->method, (unsigned long long) result->entries,
        (unsigned long long) result->random_lookups,
        (unsigned long long) result->bytes, result->write_seconds,
        rate(result->entries, result->write_seconds), result->sync_seconds,
        result->close_seconds, result->open_ro_seconds, result->check_seconds,
        rate(result->entries, result->check_seconds), result->walk_seconds,
        rate(result->entries, result->walk_seconds), result->lookup_seconds,
        rate(result->random_lookups, result->lookup_seconds),
        result->missing_seconds,
        rate(result->random_lookups, result->missing_seconds), result->path);
}

int
main(int argc, char **argv)
{
    static const char *default_methods[] = {"hisv6", "hissqlite"};
    struct bench_config config;
    struct bench_result *results;
    char root_template[] = "history-bench-XXXXXX";
    int i;
    size_t j;

    /* Best-effort: load inn.conf so both backends use the operator's real
       tuning (hissqlite{cachesize,mmapsize,pagesize}, dbz nfs hints).  If no
       inn.conf is reachable, innconf stays NULL and each backend falls back to
       its built-in defaults, which match the inn.conf defaults, so the
       comparison stays fair either way. */
    if (!innconf_read(NULL)) {
        innconf = NULL;
        warn("no inn.conf found; using built-in history defaults for both"
             " methods");
    }

    memset(&config, 0, sizeof(config));
    config.entries = DEFAULT_ENTRIES;
    config.random_lookups = DEFAULT_RANDOM;
    config.sync_every = DEFAULT_SYNC;
    config.methods = default_methods;
    config.method_count = 2;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(0);
        } else if (strcmp(argv[i], "-k") == 0) {
            config.keep = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (++i >= argc)
                usage(1);
            config.entries = parse_count(argv[i]);
        } else if (strcmp(argv[i], "-r") == 0) {
            if (++i >= argc)
                usage(1);
            config.random_lookups = parse_count(argv[i]);
        } else if (strcmp(argv[i], "-s") == 0) {
            if (++i >= argc)
                usage(1);
            if (strcmp(argv[i], "0") == 0)
                config.sync_every = 0;
            else
                config.sync_every = parse_count(argv[i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            if (++i >= argc)
                usage(1);
            config.root = argv[i];
        } else if (argv[i][0] == '-') {
            usage(1);
        } else {
            config.methods = (const char **) &argv[i];
            config.method_count = (size_t) (argc - i);
            break;
        }
    }
    if (config.random_lookups > config.entries)
        config.random_lookups = config.entries;
    if (config.root == NULL) {
        if (mkdtemp(root_template) == NULL)
            sysdie("cannot create benchmark directory");
        config.root = root_template;
    } else if (mkdir(config.root, 0777) < 0) {
        sysdie("cannot create %s", config.root);
    }

    printf("history benchmark root: %s\n", config.root);
    printf("entries: %llu, random lookups: %llu, sync every: %llu\n\n",
           (unsigned long long) config.entries,
           (unsigned long long) config.random_lookups,
           (unsigned long long) config.sync_every);
    fflush(stdout);

    results =
        (struct bench_result *) xcalloc(config.method_count, sizeof(*results));
    for (j = 0; j < config.method_count; j++) {
        benchmark_method(&config, config.methods[j], &results[j]);
        print_result(&results[j]);
        printf("\n");
        fflush(stdout);
    }

    print_csv_header();
    for (j = 0; j < config.method_count; j++)
        print_csv(&results[j]);

    if (config.keep) {
        printf("\nkept benchmark data in %s\n", config.root);
    } else {
        remove_tree(config.root);
        printf("\nremoved benchmark data from %s\n", config.root);
    }
    free(results);
    return 0;
}
