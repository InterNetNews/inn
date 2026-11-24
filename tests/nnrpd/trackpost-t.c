/*
**  Tests for tracked post file names.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include "inn/libinn.h"
#include "inn/nntp.h"
#include "tap/basic.h"
#include "tap/string.h"

#include "../../nnrpd/nnrpd.h"


int
main(void)
{
    char maximum[NNTP_MAXLEN_MSGID + 1];
    char *expected, *maximum_name, *normal_name, *slash_name;
    const char *normal = "<a_b@example>";
    const char *slash = "<a/b@example>";

    plan(10);

    ok(IsValidMessageID(normal, false, false),
       "underscore Message-ID is valid");
    ok(IsValidMessageID(slash, false, false), "slash Message-ID is valid");

    normal_name = TrackPostFilename(normal);
    slash_name = TrackPostFilename(slash);
    is_string("track.<a_b@example>", normal_name,
              "ordinary Message-ID keeps the historical file name");
    ok(strchr(slash_name, '/') == NULL,
       "slash Message-ID does not put a separator in the file name");
    ok(strcmp(normal_name, slash_name) != 0,
       "slash and underscore Message-IDs have distinct file names");
    expected = concat("track.", HashToText(HashMessageID(slash)), (char *) 0);
    is_string(expected, slash_name,
              "slash Message-ID uses its canonical hash");
    free(expected);
    free(slash_name);
    free(normal_name);

    memset(maximum, 'a', sizeof(maximum) - 1);
    maximum[0] = '<';
    maximum[NNTP_MAXLEN_MSGID - 3] = '@';
    maximum[NNTP_MAXLEN_MSGID - 2] = 'x';
    maximum[NNTP_MAXLEN_MSGID - 1] = '>';
    maximum[NNTP_MAXLEN_MSGID] = '\0';
    is_int(NNTP_MAXLEN_MSGID, strlen(maximum),
           "maximum Message-ID has the intended length");
    ok(IsValidMessageID(maximum, false, false),
       "maximum-length Message-ID is valid");
    maximum_name = TrackPostFilename(maximum);
    is_int(sizeof("track.") - 1 + sizeof(HASH) * 2, strlen(maximum_name),
           "maximum Message-ID uses the bounded hash name");
    ok(strlen(maximum_name) <= NAME_MAX,
       "maximum Message-ID file name fits in one component");
    free(maximum_name);

    return 0;
}
