/* Test archive path construction across buffer growth. */

#include "portable/system.h"

#include "inn/buffer.h"
#include "inn/concat.h"
#include "inn/xmalloc.h"
#include "tap/basic.h"

extern struct buffer *archive_test_build_path(const char *, const char *,
                                              const char *);

int
main(void)
{
    const size_t root_length = 1012;
    char *expected;
    char *root;
    struct buffer *path;

    test_init(2);

    root = xmalloc(root_length + 1);
    memset(root, 'a', root_length);
    root[root_length] = '\0';

    path = archive_test_build_path(root, "example.test", "123");
    ok(1, path->size > 1024);

    expected = concat(root, "/example/test/123", (char *) 0);
    ok_string(2, expected, path->data);

    free(expected);
    free(root);
    buffer_free(path);
    return 0;
}
