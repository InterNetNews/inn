/* $Id$ */
/* conffile test suite. */

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/stat.h>

#include "conffile.h"
#include "inn/messages.h"
#include "libtest.h"

/* Test valid configuration file. */
static const char valid[] = "test \"test #te\nst\\\"\" {\n  #foo\n test\n\n}";
static const char *const valid_tokens[] = {
    "test", "test #te\nst\\\"", "{", "test", "}"
};

/* Test error file. */
static const char error[] = "test \"test\ntest\ntest";

int
main(void)
{
    FILE *config;
    CONFFILE *parser;
    CONFTOKEN *token;
    unsigned int n, i;

    test_init(16);

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

    unlink(".testout");
    return 0;
}
