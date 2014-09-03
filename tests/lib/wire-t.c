/* $Id$ */
/* wire test suite. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/messages.h"
#include "inn/wire.h"
#include "inn/libinn.h"
#include "tap/basic.h"

/* Read in a file and return the contents in newly allocated memory.  Fills in
   the provided stat buffer. */
static char *
read_file(const char *name, struct stat *st)
{
    int fd;
    char *article;
    ssize_t count;

    if (stat(name, st) < 0)
        sysdie("cannot stat %s", name);
    article = xmalloc(st->st_size);
    fd = open(name, O_RDONLY);
    if (fd < 0)
        sysdie("cannot open %s", name);
    count = read(fd, article, st->st_size);
    if (count < st->st_size)
        die("unable to read all of %s", name);
    close(fd);
    return article;
}


/* Test article for wire_findbody. */
const char ta[] = "Path: \r\nFrom: \r\n\r\n";

int
main(void)
{
    const char *p, *end;
    char *article, *wire, *native;
    struct stat st;
    size_t wire_size, native_size, size;

    test_init(58);

    end = ta + sizeof(ta) - 1;
    p = end - 4;
    ok(1, wire_findbody(ta, sizeof(ta) - 1) == end);
    ok(2, wire_findbody(ta, sizeof(ta) - 2) == NULL);
    ok(3, wire_findbody(ta, sizeof(ta) - 3) == NULL);
    ok(4, wire_findbody(ta, sizeof(ta) - 4) == NULL);
    ok(5, wire_findbody(ta, sizeof(ta) - 5) == NULL);
    ok(6, wire_findbody(p, 4) == end);
    ok(7, wire_findbody(p, 3) == NULL);
    ok(8, wire_findbody(p, 2) == NULL);
    ok(9, wire_findbody(p, 1) == NULL);
    ok(10, wire_findbody(p, 0) == NULL);

    if (access("../data/articles/wire-strange", F_OK) == 0)
        chdir("../data");
    else if (access("data/articles/wire-strange", F_OK) == 0)
        chdir("data");
    else if (access("tests/data/articles/wire-strange", F_OK) == 0)
        chdir("tests/data");
    article = read_file("articles/wire-strange", &st);

    p = wire_findbody(article, st.st_size);
    ok(11, strncmp(p, "Path: This is", strlen("Path: This is")) == 0);
    p = wire_nextline(p, article + st.st_size - 1);
    ok(12, strncmp(p, "Second: Not", strlen("Second: Not")) == 0);
    p = wire_nextline(p, article + st.st_size - 1);
    ok(13, p == NULL);
    p = wire_findheader(article, st.st_size, "Path", true);
    ok(14, p == article + 6);
    p = wire_findheader(article, st.st_size, "From", true);
    ok(15, strncmp(p, "This is the real", strlen("This is the real")) == 0);
    p = wire_findheader(article, st.st_size, "SUMMARY", true);
    ok(16, strncmp(p, "First text", strlen("First text")) == 0);
    p = wire_findheader(article, st.st_size, "Header", true);
    ok(17, strncmp(p, "This one is real", strlen("This one is real")) == 0);
    p = wire_findheader(article, st.st_size, "message-id", true);
    ok(18, strncmp(p, "<foo@example.com>", strlen("<foo@example.com>")) == 0);
    p = wire_findheader(article, st.st_size, "Second", true);
    ok(19, p == NULL);
    p = wire_findheader(article, st.st_size, "suBJect", true);
    ok(20, strncmp(p, "This is\rnot", strlen("This is\rnot")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(21, strncmp(end, "\nFrom: This is", strlen("\nFrom: This is")) == 0);
    p = wire_findheader(article, st.st_size, "keywordS", true);
    ok(22, strncmp(p, "this is --", strlen("this is --")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(23, strncmp(end, "\nSummary: ", strlen("\nSummary: ")) == 0);
    p = wire_findheader(article, st.st_size, "strange", true);
    ok(24, strncmp(p, "This is\n\nnot", strlen("This is\n\nnot")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(25, strncmp(end, "\nMessage-ID: ", strlen("\nMessage-ID: ")) == 0);
    p = wire_findheader(article, st.st_size, "Message", true);
    ok(26, p == NULL);

    free(article);
    article = read_file("articles/wire-no-body", &st);

    ok(27, wire_findbody(article, st.st_size) == NULL);
    p = wire_findheader(article, st.st_size, "message-id", true);
    ok(28, strncmp(p, "<bad-body@example.com>\r\n",
                   strlen("<bad-body@example.com>\r\n")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(29, end == article + st.st_size - 1);
    ok(30, wire_nextline(p, article + st.st_size - 1) == NULL);

    free(article);
    article = read_file("articles/wire-truncated", &st);

    ok(31, wire_findbody(article, st.st_size) == NULL);
    p = wire_findheader(article, st.st_size, "date", true);
    ok(32, strncmp(p, "Mon, 23 Dec", strlen("Mon, 23 Dec")) == 0);
    ok(33, wire_endheader(p, article + st.st_size - 1) == NULL);
    ok(34, wire_nextline(p, article + st.st_size - 1) == NULL);

    free(article);

    ok(35, wire_findbody("\r\n.\r\n", 5) == NULL);

    article = xstrdup("\r\nNo header.\r\n.\r\n");
    ok(36, wire_findbody(article, strlen(article)) == article + 2);

    free(article);

    /* Tests for wire to native conversion and vice versa. */
    wire = read_file("articles/wire-7", &st);
    wire_size = st.st_size;
    native = read_file("articles/7", &st);
    native_size = st.st_size;

    article = wire_from_native(native, native_size, &size);
    ok_int(37, wire_size, size);
    ok(38, memcmp(wire, article, wire_size) == 0);
    free(article);

    article = wire_to_native(wire, wire_size, &size);
    ok_int(39, native_size, size);
    ok(40, memcmp(native, article, native_size) == 0);
    free(article);
    free(wire);
    free(native);

    /* Test a few edge cases.  An article that isn't in wire format. */
    wire = xstrdup("From: f@example.com\n\nSome body.\n");
    wire_size = strlen(wire);
    article = wire_to_native(wire, wire_size, &size);
    ok_int(41, wire_size, size);
    ok_string(42, wire, article);
    free(wire);
    free(article);

    /* An empty article. */
    article = wire_to_native("", 0, &size);
    ok_int(43, 0, size);
    ok_string(44, "", article);
    free(article);
    article = wire_to_native(".\r\n", 3, &size);
    ok_int(45, 0, size);
    ok_string(46, "", article);
    article = wire_from_native("", 0, &size);
    ok_int(47, 3, size);
    ok_string(48, ".\r\n", article);
    free(article);

    /* Nasty partial articles. */
    article = wire_to_native("T: f\r\n\r\n.\r", 10, &size);
    ok_int(49, 8, size);
    ok_string(50, "T: f\n\n.\r", article);
    free(article);
    article = wire_to_native("T: f\r\n\r\n.", 9, &size);
    ok_int(51, 7, size);
    ok_string(52, "T: f\n\n.", article);
    free(article);
    article = wire_to_native("..\r\n.\r\n", 7, &size);
    ok_int(53, 2, size);
    ok_string(54, ".\n", article);
    free(article);

    /* Articles containing nul. */
    article = wire_to_native("T: f\0\r\n\r\n..\r\n.\r\n", 16, &size);
    ok_int(55, 9, size);
    ok(56, memcmp("T: f\0\n\n.\n", article, 9) == 0);
    free(article);
    article = wire_from_native("T: f\0\n\n.\n", 9, &size);
    ok_int(57, 16, size);
    ok(58, memcmp("T: f\0\r\n\r\n..\r\n.\r\n", article, 16) == 0);
    free(article);

    return 0;
}
