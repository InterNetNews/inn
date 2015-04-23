/* $Id$ */
/* Test suite for ternary search tries. */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "inn/tst.h"
#include "inn/libinn.h"
#include "libtest.h"

/* Used for strings of unsigned characters (called SUC instead of U
   because it otherwise conflicts with Unicode strings). */
#define SUC (const unsigned char *)

/* An unsigned char version of strlen. */
#define ustrlen(s) strlen((const char *) s)

int
main(void)
{
    struct tst *tst;
    FILE *words;
    unsigned char buffer[1024];
    bool reported;
    void *existing;
    unsigned char *word;

    char test[] = "test";
    char t[] = "t";
    char foo[] = "foo";
    char testing[] = "testing";
    char Strange[] = "Strange";
    char change[] = "çhange";

    test_init(38);

    tst = tst_init(2);
    ok(1, tst != NULL);
    ok(2, tst_insert(tst, SUC"test", test, 0, NULL) == TST_OK);
    ok_string(3, "test", tst_search(tst, SUC"test"));
    ok(4, tst_insert(tst, SUC"test", foo, 0, &existing) == TST_DUPLICATE_KEY);
    ok_string(5, "test", existing);
    ok(6, tst_insert(tst, SUC"test", foo, TST_REPLACE, &existing) == TST_OK);
    ok_string(7, "test", existing);
    ok_string(8, "foo", tst_search(tst, SUC"test"));
    ok(9, tst_insert(tst, SUC"testing", testing, 0, NULL) == TST_OK);
    ok(10, tst_insert(tst, SUC"t", t, 0, NULL) == TST_OK);
    ok(11, tst_insert(tst, SUC"Strange", Strange, 0, NULL) == TST_OK);
    ok(12, tst_insert(tst, SUC"çhange", change, 0, NULL) == TST_OK);
    ok(13, tst_insert(tst, SUC"", foo, 0, NULL) == TST_NULL_KEY);
    ok(14, tst_insert(tst, NULL, foo, 0, NULL) == TST_NULL_KEY);
    ok_string(15, "testing", tst_search(tst, SUC"testing"));
    ok_string(16, "t", tst_search(tst, SUC"t"));
    ok_string(17, "Strange", tst_search(tst, SUC"Strange"));
    ok_string(18, "çhange", tst_search(tst, SUC"çhange"));
    ok_string(19, "foo", tst_search(tst, SUC"test"));
    ok(20, tst_search(tst, SUC"") == NULL);
    ok(21, tst_search(tst, SUC"Peter") == NULL);
    ok(22, tst_search(tst, SUC"foo") == NULL);
    ok(23, tst_search(tst, SUC"te") == NULL);
    ok_string(24, "Strange", tst_delete(tst, SUC"Strange"));
    ok(25, tst_search(tst, SUC"Strange") == NULL);
    ok_string(26, "t", tst_delete(tst, SUC"t"));
    ok(27, tst_search(tst, SUC"t") == NULL);
    ok_string(28, "testing", tst_search(tst, SUC"testing"));
    ok_string(29, "foo", tst_search(tst, SUC"test"));
    ok_string(30, "testing", tst_delete(tst, SUC"testing"));
    ok_string(31, "foo", tst_search(tst, SUC"test"));
    ok_string(32, "çhange", tst_delete(tst, SUC"çhange"));
    ok_string(33, "foo", tst_delete(tst, SUC"test"));
    ok(34, tst_search(tst, NULL) == NULL);
    ok(35, tst_delete(tst, NULL) == NULL);
    tst_cleanup(tst);
    ok(36, true);

    words = fopen("/usr/dict/words", "r");
    if (words == NULL)
        words = fopen("/usr/share/dict/words", "r");
    if (words == NULL) {
        skip_block(37, 2, "/usr/share/dict/words not available");
        exit(0);
    }

    tst = tst_init(1000);
    reported = false;
    if (tst == NULL)
        reported = true;
    else {
        while (fgets((char *) buffer, sizeof(buffer), words)) {
            buffer[ustrlen(buffer) - 1] = '\0';
            if (buffer[0] == '\0')
                continue;
            word = (unsigned char *) xstrdup((char *) buffer);
            if (tst_insert(tst, buffer, word, 0, NULL) != TST_OK) {
                if (!reported) {
                    diag("Failed insert of word %s\n", word);
                }
                reported = true;
            }
        }
    }
    ok(37, !reported);

    if (fseek(words, 0, SEEK_SET) < 0)
        sysdie("Unable to rewind words file");
    reported = false;
    if (tst == NULL)
        reported = true;
    else {
        while (fgets((char *) buffer, sizeof(buffer), words)) {
            buffer[ustrlen(buffer) - 1] = '\0';
            if (buffer[0] == '\0')
                continue;
            word = tst_search(tst, buffer);
            if (word == NULL || strcmp((char *) word, (char *) buffer) != 0) {
                if (!reported) {
                    diag("Failed search of word %s\n", word);
                }
                reported = true;
            }
            word = tst_delete(tst, buffer);
            if (word == NULL || strcmp((char *) word, (char *) buffer) != 0) {
                if (!reported) {
                    diag("Failed delete of word %s\n", word);
                }
                reported = true;
            }
            free(word);
        }
    }
    tst_cleanup(tst);
    ok(38, !reported);

    return 0;
}
