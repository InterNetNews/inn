/* $Id$ */
/* Test suite for lib/hashtab.c. */

#include "config.h"
#include "clibrary.h"
#include <sys/stat.h>

#include "libinn.h"
#include "inn/hashtab.h"
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
    foo = xstrdup("foo");
    bar = xstrdup("bar");

    puts("33");
    hash = hash_create(4, hash_string, string_key, string_equal,
                       string_delete);
    ok(1, hash != NULL);
    if (hash == NULL)
        die("Unable to create hash, cannot continue");

    ok(2, hash_insert(hash, "test", test));
    ok(3, hash_collisions(hash) == 0);
    ok(4, hash_expansions(hash) == 0);
    ok(5, hash_searches(hash) == 1);
    word = hash_lookup(hash, "test");
    ok(6, word != NULL && !strcmp("test", word));
    ok(7, hash_delete(hash, "test"));
    test = xstrdup("test");
    ok(8, hash_lookup(hash, "test") == NULL);
    ok(9, !hash_delete(hash, "test"));
    ok(10, !hash_replace(hash, "test", testing));
    ok(11, hash_insert(hash, "test", test));
    ok(12, hash_insert(hash, "testing", testing));
    ok(13, hash_insert(hash, "strange", strange));
    ok(14, hash_expansions(hash) == 0);
    ok(15, hash_insert(hash, "change", change));
    ok(16, hash_expansions(hash) == 1);
    word = hash_lookup(hash, "testing");
    ok(17, word != NULL && !strcmp("testing", word));
    word = hash_lookup(hash, "strange");
    ok(18, word != NULL && !strcmp("strange", word));
    ok(19, hash_lookup(hash, "thingie") == NULL);
    ok(20, !hash_delete(hash, "thingie"));
    ok(21, hash_delete(hash, "strange"));
    strange = xstrdup("strange");
    ok(22, hash_lookup(hash, "strange") == NULL);

    hash_traverse(hash, string_traverse, &wordrefs[0]);
    reported = 0;
    for (i = 0; wordrefs[i].word != NULL; i++)
        if (wordrefs[i].count != 1 && !reported) {
            printf("not ");
            reported = 1;
        }
    puts("ok 23");
    ok(24, wordrefs[3].count == 0);

    hash_free(hash);

    /* Test hash creation with an odd size.  This previously could result
       in the wrong table size being allocated. */
    hash = hash_create(5, hash_string, string_key, string_equal,
                       string_delete);
    ok(25, hash != NULL);
    if (hash == NULL)
        die("Unable to create hash, cannot continue");
    ok(26, hash_insert(hash, "test", test));
    ok(27, hash_insert(hash, "testing", testing));
    ok(28, hash_insert(hash, "strange", strange));
    ok(29, hash_insert(hash, "change", change));
    ok(30, hash_insert(hash, "foo", foo));
    ok(31, hash_insert(hash, "bar", bar));
    hash_free(hash);

    words = fopen("/usr/dict/words", "r");
    if (words == NULL)
        words = fopen("/usr/share/dict/words", "r");
    if (words == NULL) {
        puts("ok 32 # skip\nok 33 # skip");
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
    puts("ok 32");

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
    puts("ok 33");

    return 0;
}
