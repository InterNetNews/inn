/* $Id$ */
/* Test suite for list routines. */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "inn/list.h"
#include "inn/libinn.h"
#include "tap/basic.h"

int
main(void)
{
    struct list list;
    struct node a, b, c;

    test_init(28);

    list_new(&list);
    ok(1, list_isempty(&list));

    ok(2, list_addhead(&list, &a) == &a);
    ok(3, !list_isempty(&list));
    ok(4, list_head(&list) == &a);
    ok(5, list_tail(&list) == &a);
    ok(6, list_remhead(&list) == &a);
    ok(7, list_isempty(&list));

    ok(8, list_addhead(&list, &a) == &a);
    ok(9, list_remtail(&list) == &a);
    ok(10, list_isempty(&list));

    ok(11, list_addtail(&list, &a) == &a);
    ok(12, !list_isempty(&list));
    ok(13, list_head(&list) == &a);
    ok(14, list_tail(&list) == &a);
    ok(15, list_remhead(&list) == &a);
    ok(16, list_isempty(&list));

    list_addtail(&list, &a);
    ok(17, list_remtail(&list) == &a);
    ok(18, list_isempty(&list));

    list_addhead(&list, &a);
    ok(19, list_remove(&a) == &a);
    ok(20, list_isempty(&list));

    list_addtail(&list, &a);
    list_addtail(&list, &b);
    list_insert(&list, &c, &a);
    ok(21, list_succ(&c) == &b);
    ok(22, list_pred(&c) == &a);
    list_remove(&c);
    list_insert(&list, &c, &b);
    ok(23, list_succ(&c) == NULL);
    ok(24, list_pred(&c) == &b);
    list_remove(&c);
    list_insert(&list, &c, NULL);
    ok(25, list_succ(&c) == &a);
    ok(26, list_pred(&c) == NULL);
    list_remove(&c);
    ok(27, list_head(&list) == &a);
    ok(28, list_tail(&list) == &b);

    return 0;
}
