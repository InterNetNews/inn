/* $Id$ */
/* Test suite for lib/hashtab.c. */

#include "config.h"
#include "clibrary.h"
#include <sys/stat.h>

#include "inn/hashtab.h"
#include "inn/messages.h"
#include "libinn.h"
#include "libtest.h"

struct wordref {
    const char *word;
    int count;
};

static const void *
string_key(const void *entry)
{
    return entry;
}

static bool
string_equal(const void *key, const void *entry)
{
    const char *p, *q;

    p = key;
    q = entry;
    return !strcmp(p, q);
}

static void
string_delete(void *entry)
{
    free(entry);
}

static void
string_traverse(void *entry, void *data)
{
    int i;
    struct wordref *wordrefs = data;

    for (i = 0; wordrefs[i].word != NULL; i++)
        if (!strcmp(entry, wordrefs[i].word)) {
            wordrefs[i].count++;
            return;
        }
    wordrefs[3].count++;
}

int
main(void)
{
    struct hash *hash;
    FILE *words;
    int reported, i;
    char buffer[1024];
    char *word;
    char *test, *testing, *strange, *change, *foo, *bar;

    struct wordref wordrefs[4] = {
        { "test", 0 }, { "testing", 0 }, { "change", 0 }, { NULL, 0 }
    };

    test = xstrdup("test");
    testing = xstrdup("testing");
    strange = xstrdup("strange");
    change = xstrdup("change");

    puts("38");
    hash = hash_create(4, hash_string, string_key, string_equal,
                       string_delete);
    ok(1, hash != NULL);
    if (hash == NULL)
        die("Unable to create hash, cannot continue");

    ok(2, hash_insert(hash, "test", test));
    ok(3, hash_collisions(hash) == 0);
    ok(4, hash_expansions(hash) == 0);
    ok(5, hash_searches(hash) == 1);
    ok(6, hash_count(hash) == 1);
    word = hash_lookup(hash, "test");
    ok(7, word != NULL && !strcmp("test", word));
    ok(8, hash_delete(hash, "test"));
    test = xstrdup("test");
    ok(9, hash_lookup(hash, "test") == NULL);
    ok(10, !hash_delete(hash, "test"));
    ok(11, !hash_replace(hash, "test", testing));
    ok(12, hash_count(hash) == 0);
    ok(13, hash_insert(hash, "test", test));
    ok(14, hash_insert(hash, "testing", testing));
    ok(15, hash_insert(hash, "strange", strange));
    ok(16, hash_expansions(hash) == 0);
    ok(17, hash_insert(hash, "change", change));
    ok(18, hash_expansions(hash) == 1);
    ok(19, hash_count(hash) == 4);
    word = hash_lookup(hash, "testing");
    ok(20, word != NULL && !strcmp("testing", word));
    word = hash_lookup(hash, "strange");
    ok(21, word != NULL && !strcmp("strange", word));
    ok(22, hash_lookup(hash, "thingie") == NULL);
    ok(23, !hash_delete(hash, "thingie"));
    ok(24, hash_delete(hash, "strange"));
    ok(25, hash_lookup(hash, "strange") == NULL);
    ok(26, hash_count(hash) == 3);

    hash_traverse(hash, string_traverse, &wordrefs[0]);
    reported = 0;
    for (i = 0; wordrefs[i].word != NULL; i++)
        if (wordrefs[i].count != 1 && !reported) {
            printf("not ");
            reported = 1;
        }
    puts("ok 27");
    ok(28, wordrefs[3].count == 0);

    hash_free(hash);

    /* Test hash creation with an odd size.  This previously could result
       in the wrong table size being allocated. */
    test = xstrdup("test");
    testing = xstrdup("testing");
    strange = xstrdup("strange");
    change = xstrdup("change");
    foo = xstrdup("foo");
    bar = xstrdup("bar");
    hash = hash_create(5, hash_string, string_key, string_equal,
                       string_delete);
    ok(29, hash != NULL);
    if (hash == NULL)
        die("Unable to create hash, cannot continue");
    ok(30, hash_insert(hash, "test", test));
    ok(31, hash_insert(hash, "testing", testing));
    ok(32, hash_insert(hash, "strange", strange));
    ok(33, hash_insert(hash, "change", change));
    ok(34, hash_insert(hash, "foo", foo));
    ok(35, hash_insert(hash, "bar", bar));
    ok(36, hash_count(hash) == 6);
    hash_free(hash);

    words = fopen("/usr/dict/words", "r");
    if (words == NULL)
        words = fopen("/usr/share/dict/words", "r");
    if (words == NULL) {
        puts("ok 37 # skip\nok 38 # skip");
        exit(0);
    }

    hash = hash_create(4, hash_string, string_key, string_equal,
                       string_delete);
    reported = 0;
    if (hash == NULL)
        printf("not ");
    else {
        while (fgets(buffer, sizeof(buffer), words)) {
            buffer[strlen(buffer) - 1] = '\0';
            word = xstrdup(buffer);
            if (!hash_insert(hash, word, word)) {
                if (!reported)
                    printf("not ");
                reported = 1;
            }
        }
    }
    puts("ok 37");

    if (fseek(words, 0, SEEK_SET) < 0)
        sysdie("Unable to rewind words file");
    reported = 0;
    if (hash == NULL)
        printf("not ");
    else {
        while (fgets(buffer, sizeof(buffer), words)) {
            buffer[strlen(buffer) - 1] = '\0';
            word = hash_lookup(hash, buffer);
            if (!word || strcmp(word, buffer) != 0) {
                if (!reported)
                    printf("not ");
                reported = 1;
            }
        }
    }
    puts("ok 38");

    hash_free(hash);

    return 0;
}
