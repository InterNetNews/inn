/* Test bounded incremental discarding of rejected NNTP articles. */

#include "portable/system.h"

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/libinn.h"
#include "tap/basic.h"
#include "tap/messages.h"

#include "../../innd/innd.h"

int
main(void)
{
    CHANNEL cp = {0};
    const size_t body = 8192;
    const size_t wire = body + 5;
    char logline[BUFSIZ];
    bool found;

    test_init(14);

    buffer_resize(&cp.In, wire + 16);
    memset(cp.In.data, 'a', body);
    memcpy(cp.In.data + body, "\r\n.", 3);
    cp.In.used = body + 3;
    cp.In.left = cp.In.size - cp.In.used;

    ok(1, !NCdiscardarticle(&cp));
    ok_int(2, SAVE_AMT, cp.In.used);
    ok_int(3, body + 3 - SAVE_AMT, cp.LargeArtSize);
    ok_int(4, 0, cp.Start);
    ok_int(5, 0, cp.Next);

    memcpy(cp.In.data + cp.In.used, "\r\n", 2);
    cp.In.used += 2;
    cp.In.left -= 2;
    ok(6, NCdiscardarticle(&cp));
    ok_int(7, wire, cp.LargeArtSize + cp.Next - cp.Start);

    cp.LargeArtSize = 0;
    cp.Start = cp.Next = 0;
    memcpy(cp.In.data, ".\r\n", 3);
    cp.In.used = 3;
    cp.In.left = cp.In.size - cp.In.used;
    ok(8, NCdiscardarticle(&cp));
    ok_int(9, 3, cp.Next);

    innconf = xcalloc(1, sizeof(*innconf));
    innconf->maxartsize = body;
    Log = tmpfile();
    if (Log == NULL)
        sysbail("cannot create temporary log");
    cp.Type = CTnntp;
    cp.fd = 7;
    cp.Data.Feedsite = "test-peer";
    cp.LargeArtMessageID = xstrdup("<large@example.invalid>");
    cp.LargeArtFeedsite = xstrdup(cp.Data.Feedsite);
    cp.LargeArtSize = body - 5;
    cp.Start = 0;
    cp.Next = 10;
    strlcpy(cp.Error, "437 Article too large", sizeof(cp.Error));

    errors_capture();
    NCloglargearticle(&cp);
    errors_uncapture();
    ok_string(10,
              "437 Article of 8197 bytes exceeds local limit of 8192 bytes",
              cp.Error);
    ok_string(11,
              "localhost:7 internal rejecting huge article (8197 > 8192)\n",
              errors);
    ok(12, cp.LargeArtFeedsite == NULL);
    rewind(Log);
    if (fgets(logline, sizeof(logline), Log) == NULL)
        sysbail("cannot read temporary log");
    found = strstr(logline,
                   " - test-peer <large@example.invalid> 437 Article of 8197 "
                   "bytes exceeds local limit of 8192 bytes\n")
            != NULL;
    ok(13, found);
    if (!found)
        diag("log line was: %s", logline);
    ok_int(14, wire, cp.LargeArtSize + cp.Next - cp.Start);

    fclose(Log);
    free(errors);
    free(cp.LargeArtMessageID);
    free(innconf);
    free(cp.In.data);
    return 0;
}
