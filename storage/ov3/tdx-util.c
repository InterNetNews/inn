/*  $Id$
**
**  Utility for managing a tradindexed overview spool.
**
**  This utility can manipulate a tradindexed overview spool in various ways,
**  including some ways that are useful for recovery from crashes.  It allows
**  the user to view the contents of the various data structures that
**  tradindexed stores on disk.
*/

#include "config.h"
#include "clibrary.h"

#include "libinn.h"
#include "ov.h"
#include "private.h"

static void
dump_index(void)
{
    struct group_index *index;

    index = group_index_open(OV_READ);
    if (index == NULL)
        return;
    group_index_dump(index);
    group_index_close(index);
}

static void
dump_group(const char *group)
{
    struct group_index *index;
    unsigned long high, low, count;
    char flag;

    index = group_index_open(OV_READ);
    if (index == NULL)
        return;
    if (!group_index_info(index, group, &high, &low, &count, &flag))
        return;
    printf("%s %lu %lu %lu %c\n", group, high, low, count, flag);
}

int
main(int argc, char *argv[])
{
    int option;

    error_program_name = "tdx-util";

    if (ReadInnConf() < 0)
        exit(1);

    /* Parse options. */
    while ((option = getopt(argc, argv, "di:")) != EOF) {
        switch (option) {
        case 'd':
            dump_index();
            break;
        case 'i':
            dump_group(optarg);
            break;
        default:
            die("invalid option %c", option);
            break;
        }
    }
    exit(0);
}
