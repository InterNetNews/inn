/* $Id$ */
/* wire test suite. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/messages.h"
#include "inn/wire.h"
#include "libinn.h"
#include "libtest.h"

/* Read in a file and return the contents in newly allocated memory.  Fills in
   the provided stat buffer. */
char *
read_file(const char *name, struct stat *st)
{
    int fd;
    char *article;
    size_t count;

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
    char *article;
    size_t length;
    struct stat st;

    puts("34");

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

    if (access("articles/strange", F_OK) < 0)
        if (access("lib/articles/strange", F_OK) == 0)
            chdir("lib");
    article = read_file("articles/strange", &st);

    p = wire_findbody(article, st.st_size);
    ok(11, strncmp(p, "Path: This is", strlen("Path: This is")) == 0);
    p = wire_nextline(p, article + st.st_size - 1);
    ok(12, strncmp(p, "Second: Not", strlen("Second: Not")) == 0);
    p = wire_nextline(p, article + st.st_size - 1);
    ok(13, p == NULL);
    p = wire_findheader(article, st.st_size, "Path");
    ok(14, p == article + 6);
    p = wire_findheader(article, st.st_size, "From");
    ok(15, strncmp(p, "This is the real", strlen("This is the real")) == 0);
    p = wire_findheader(article, st.st_size, "SUMMARY");
    ok(16, strncmp(p, "First text", strlen("First text")) == 0);
    p = wire_findheader(article, st.st_size, "Header");
    ok(17, strncmp(p, "This one is real", strlen("This one is real")) == 0);
    p = wire_findheader(article, st.st_size, "message-id");
    ok(18, strncmp(p, "<foo@example.com>", strlen("<foo@example.com>")) == 0);
    p = wire_findheader(article, st.st_size, "Second");
    ok(19, p == NULL);
    p = wire_findheader(article, st.st_size, "suBJect");
    ok(20, strncmp(p, "This is\rnot", strlen("This is\rnot")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(21, strncmp(end, "\nFrom: This is", strlen("\nFrom: This is")) == 0);
    p = wire_findheader(article, st.st_size, "keywordS");
    ok(22, strncmp(p, "this is --", strlen("this is --")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(23, strncmp(end, "\nSummary: ", strlen("\nSummary: ")) == 0);
    p = wire_findheader(article, st.st_size, "strange");
    ok(24, strncmp(p, "This is\n\nnot", strlen("This is\n\nnot")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(25, strncmp(end, "\nMessage-ID: ", strlen("\nMessage-ID: ")) == 0);
    p = wire_findheader(article, st.st_size, "Message");
    ok(26, p == NULL);

    free(article);
    article = read_file("articles/no-body", &st);

    ok(27, wire_findbody(article, st.st_size) == NULL);
    p = wire_findheader(article, st.st_size, "message-id");
    ok(28, strncmp(p, "<id1@example.com>\r\n",
                   strlen("<id1@example.com>\r\n")) == 0);
    end = wire_endheader(p, article + st.st_size - 1);
    ok(29, end == article + st.st_size - 1);
    ok(30, wire_nextline(p, article + st.st_size - 1) == NULL);

    free(article);
    article = read_file("articles/truncated", &st);

    ok(31, wire_findbody(article, st.st_size) == NULL);
    p = wire_findheader(article, st.st_size, "date");
    ok(32, strncmp(p, "Mon, 23 Dec", strlen("Mon, 23 Dec")) == 0);
    ok(33, wire_endheader(p, article + st.st_size - 1) == NULL);
    ok(34, wire_nextline(p, article + st.st_size - 1) == NULL);

    free(article);

    return 0;
}
