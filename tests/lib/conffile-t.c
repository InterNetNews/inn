/*  conffile test suite.
**
**  Written by Russ Allbery in 2004.
**
**  Various bug fixes, code and documentation improvements since then
**  in 2004, 2014, 2021, 2026.
*/

#include "portable/system.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "conffile.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "tap/basic.h"

/* Test valid configuration file. */
static const char valid[] = "test \"test #te\nst\\\"\" {\n  #foo\n test\n\n}";
static const char *const valid_tokens[] = {"test", "test #te\nst\\\"", "{",
                                           "test", "}"};

/* Test error file. */
static const char error[] = "test \"test\ntest\ntest";

int
main(void)
{
    char *array_config[2];
    char *long_line;
    FILE *config;
    CONFFILE *parser;
    CONFTOKEN *token;
    unsigned int n, i;

    test_init(20);

    config = fopen(".testout", "w");
    if (config == NULL)
        sysdie("Can't create .testout");
    fwrite(error, sizeof(error), 1, config);
    fclose(config);

    parser = CONFfopen(".testout");
    ok(1, parser != NULL);
    token = CONFgettoken(NULL, parser);
    ok(2, token != NULL);
    ok_string(3, "test", token->name);
    token = CONFgettoken(NULL, parser);
    ok(4, token == NULL);
    CONFfclose(parser);

    config = fopen(".testout", "w");
    if (config == NULL)
        sysdie("Can't create .testout");
    fwrite(valid, sizeof(valid), 1, config);
    fclose(config);

    parser = CONFfopen(".testout");
    ok(5, parser != NULL);
    n = 6;
    for (i = 0; i < ARRAY_SIZE(valid_tokens); i++) {
        token = CONFgettoken(NULL, parser);
        ok(n++, token != NULL);
        ok_string(n++, valid_tokens[i], token->name);
    }
    token = CONFgettoken(NULL, parser);
    ok(n++, token == NULL);
    CONFfclose(parser);

    /* Array-backed input must stop before indexing past the array when a
       quoted string continues at EOF. */
    array_config[0] = (char *) "test \"unterminated";
    parser = xcalloc(1, sizeof(CONFFILE));
    parser->array = array_config;
    parser->array_len = 1;
    parser->filename = xstrdup("array");
    token = CONFgettoken(NULL, parser);
    ok(n++, token != NULL && strcmp(token->name, "test") == 0);
    token = CONFgettoken(NULL, parser);
    ok(n++, token == NULL);
    CONFfclose(parser);

    /* A continuation element is bounded by the space remaining in buf. */
    long_line = xmalloc(BIG_BUFFER + 1);
    memset(long_line, 'a', BIG_BUFFER);
    long_line[BIG_BUFFER] = '\0';
    array_config[0] = (char *) "test \"";
    array_config[1] = long_line;
    parser = xcalloc(1, sizeof(CONFFILE));
    parser->array = array_config;
    parser->array_len = 2;
    parser->filename = xstrdup("array");
    token = CONFgettoken(NULL, parser);
    ok(n++, token != NULL && strcmp(token->name, "test") == 0);
    token = CONFgettoken(NULL, parser);
    ok(n++, token == NULL);
    CONFfclose(parser);
    free(long_line);

    unlink(".testout");
    return 0;
}
