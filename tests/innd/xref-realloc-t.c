/*
**  Regression test for the use-after-free in ARTassignnumbers (innd/art.c).
**
**  When an article's generated Xref outgrows the initial MED_BUFFER*2+1 (2049)
**  byte buffer, the buffer is xrealloc'd but the saved pointer q is not
**  recomputed.  data->Replic and data->ReplicLength are then derived from the
**  freed pre-realloc buffer: Replic dangles and ReplicLength becomes an
**  arbitrary (often negative) value.  The storage layer (MatchGroups) and
**  site.c then read Replic and use ReplicLength, so this is a use-after-free
**  and, with a negative length, the "cant malloc <huge>" crash.
**
**  Under AddressSanitizer xrealloc always relocates, so the stale read is
**  deterministic: this test fails (and ASan reports heap-use-after-free) on the
**  unfixed code and passes once q is recomputed after the reallocation.
*/

#include "portable/system.h"

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/vector.h"

#define LIBTEST_NEW_FORMAT 1
#include "tap/basic.h"

#include "../../innd/innd.h"

/* Enough crossposted groups that the generated Xref far exceeds 2049 bytes and
   ARTassignnumbers calls xrealloc (one entry is ~28 bytes; 2049/28 ~= 73). */
#define NGROUPS 200

int
main(void)
{
    ARTDATA data;
    NEWSGROUP *groups;
    NEWSGROUP **pointers;
    int i;
    volatile char sink;

    plan(4);

    /* Minimal innconf (ARTassignnumbers needs innconf to be non-NULL). */
    innconf = xcalloc(1, sizeof(*innconf));
    innconf->storeonxref = true;
    innconf->extraoverviewadvertised = vector_new();
    innconf->extraoverviewhidden = vector_new();

    /* ARTassignnumbers seeds the Xref buffer from the global Path prefix. */
    buffer_set(&Path, "example.com!", strlen("example.com!") + 1);
    Path.used = strlen("example.com!") + 1;

    /* Build the crossposted group list and point GroupPointers at it. */
    groups = xcalloc(NGROUPS, sizeof(NEWSGROUP));
    pointers = xcalloc(NGROUPS + 1, sizeof(NEWSGROUP *));
    for (i = 0; i < NGROUPS; i++) {
        char name[64];

        snprintf(name, sizeof(name), "local.uaf.test.group.%04d", i);
        groups[i].Name = xstrdup(name);
        groups[i].NameLength = (int) strlen(name);
        groups[i].Last = 1;
        groups[i].Lastwidth = 10;
        groups[i].LastString = xmalloc(16);
        groups[i].PostCount = 0;
        pointers[i] = &groups[i];
    }
    pointers[NGROUPS] = NULL;
    GroupPointers = pointers;

    memset(&data, 0, sizeof(data)); /* XrefBufLength == 0 => the buffer is allocated */

    ok(ARTassignnumbers(&data), "ARTassignnumbers succeeds");

    /* The replication data must start right after the Path prefix in the
       *current* Xref buffer.  The buggy code leaves Replic pointing into the
       freed pre-realloc buffer, so this address comparison fails. */
    ok(data.Replic == data.Xref + (size_t) Path.used,
       "Replic points into the live Xref buffer");
    ok(data.ReplicLength == data.XrefLength - 2 - (int) Path.used,
       "ReplicLength equals the group-list length");

    /* MatchGroups() and site.c read Replic.  On the buggy code this reads the
       freed buffer -> AddressSanitizer heap-use-after-free; the process aborts
       here and the test fails with the plan incomplete. */
    sink = data.Replic[0];
    (void) sink;
    ok(true, "reading the replication data touches only live memory");

    return 0;
}
