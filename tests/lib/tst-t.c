/* $Id$ */
/* Test suite for ternary search tries. */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "inn/tst.h"
#include "libinn.h"
#include "libtest.h"

/* Used for strings of unsigned characters. */
#define U (const unsigned char *)

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
    char change[] = "change";

    puts("36");

    tst = tst_init(2);
    ok(1, tst != NULL);
    ok(2, tst_insert(U"test", test, tst, 0, NULL) == TST_OK);
    ok_string(3, "test", tst_search(U"test", tst));
    ok(4, tst_insert(U"test", foo, tst, 0, &existing) == TST_DUPLICATE_KEY);
    ok_string(5, "test", existing);
    ok(6, tst_insert(U"test", foo, tst, TST_REPLACE, &existing) == TST_OK);
    ok_string(7, "test", existing);
    ok_string(8, "foo", tst_search(U"test", tst));
    ok(9, tst_insert(U"testing", testing, tst, 0, NULL) == TST_OK);
    ok(10, tst_insert(U"t", t, tst, 0, NULL) == TST_OK);
    ok(11, tst_insert(U"Strange", Strange, tst, 0, NULL) == TST_OK);
    ok(12, tst_insert(U"change", change, tst, 0, NULL) == TST_OK);
    ok(13, tst_insert(U"", foo, tst, 0, NULL) == TST_NULL_KEY);
    ok(14, tst_insert(NULL, foo, tst, 0, NULL) == TST_NULL_KEY);
    ok_string(15, "testing", tst_search(U"testing", tst));
    ok_string(16, "t", tst_search(U"t", tst));
    ok_string(17, "Strange", tst_search(U"Strange", tst));
    ok_string(18, "change", tst_search(U"change", tst));
    ok_string(19, "foo", tst_search(U"test", tst));
    ok(20, tst_search(U"", tst) == NULL);
    ok(21, tst_search(U"Peter", tst) == NULL);
    ok(22, tst_search(U"foo", tst) == NULL);
    ok(23, tst_search(U"te", tst) == NULL);
    ok_string(24, "Strange", tst_delete(U"Strange", tst));
    ok(25, tst_search(U"Strange", tst) == NULL);
    ok_string(26, "t", tst_delete(U"t", tst));
    ok(27, tst_search("t", tst) == NULL);
    ok_string(28, "testing", tst_search(U"testing", tst));
    ok_string(29, "foo", tst_search(U"test", tst));
    ok_string(30, "testing", tst_delete(U"testing", tst));
    ok_string(31, "foo", tst_search(U"test", tst));
    ok_string(32, "change", tst_delete(U"change", tst));
    ok_string(33, "foo", tst_delete(U"test", tst));
    tst_cleanup(tst);
    ok(34, true);

    words = fopen("/usr/dict/words", "r");
    if (words == NULL)
        words = fopen("/usr/share/dict/words", "r");
    if (words == NULL) {
        puts("ok 35 # skip\nok 36 # skip");
        exit(0);
    }

    tst = tst_init(1000);
    reported = false;
    if (tst == NULL)
        printf("not ");
    else {
        while (fgets((char *) buffer, sizeof(buffer), words)) {
            buffer[ustrlen(buffer) - 1] = '\0';
            word = (unsigned char *) xstrdup((char *) buffer);
            if (tst_insert(buffer, word, tst, 0, NULL) != TST_OK) {
                if (!reported)
                    printf("not ");
                reported = true;
            }
        }
    }
    puts("ok 35");

    if (fseek(words, 0, SEEK_SET) < 0)
        sysdie("Unable to rewind words file");
    reported = false;
    if (tst == NULL)
        printf("not ");
    else {
        while (fgets((char *) buffer, sizeof(buffer), words)) {
            buffer[ustrlen(buffer) - 1] = '\0';
            word = tst_search(buffer, tst);
            if (word == NULL || strcmp((char *) word, buffer) != 0) {
                if (!reported)
                    printf("not ");
                reported = true;
            }
            word = tst_delete(buffer, tst);
            if (word == NULL || strcmp((char *) word, buffer) != 0) {
                if (!reported)
                    printf("not ");
                reported = true;
            }
            free(word);
        }
    }
    tst_cleanup(tst);
    puts("ok 36");

    return 0;
}
