/* $Id$ */
/* Test suite for ARTparse. */

#include "config.h"
#include "clibrary.h"

#include "inn/buffer.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "inn/libinn.h"
#include "libtest.h"

#include "../../innd/innd.h"

/* A table of paths to articles and corresponding error messages. */
const struct {
    const char *path;
    const char *error;
} articles[] = {
    { "../data/articles/1",         "" },
    { "../data/articles/2",         "" },
    { "../data/articles/3",         "" },
    { "../data/articles/4",         "" },
    { "../data/articles/5",         "" },
    { "../data/articles/bad-msgid", "" },
    { "../data/articles/bad-subj",  "" },
    { "../data/articles/6",
      "437 Article of 8193 bytes exceeds local limit of 8192 bytes" },
    { "../data/articles/bad-empty",
      "437 Empty article" },
    { "../data/articles/bad-hdr-nospc",
      "437 No colon-space in \"Test:<-he: re\" header" },
    { "../data/articles/bad-hdr-space",
      "437 Space before colon in \"Test\" header" },
    { "../data/articles/bad-hdr-trunc",
      "437 No colon-space in \"Test:\" header" },
    { "../data/articles/bad-long-cont",
      "437 Header line too long (1025 bytes)" },
    { "../data/articles/bad-long-hdr",
      "437 Header line too long (1025 bytes)" },
    { "../data/articles/bad-no-body",
      "437 No body" },
    { "../data/articles/bad-no-header",
      "437 No headers" },
    { "../data/articles/bad-nul-body",
      "437 Nul character in body" },
    { "../data/articles/bad-nul-header",
      "437 Nul character in header" }
};

/* Create enough of an innconf struct to be able to run ARTparse.  Set
   logipaddr to false so that we don't have to initialize enough in the
   channel to get RChostname working. */
static void
fake_innconf(void)
{
    if (innconf != NULL) {
        free(innconf->pathetc);
        free(innconf);
    }
    innconf = xmalloc(sizeof(*innconf));
    innconf->logipaddr = false;
    innconf->maxartsize = 8 * 1024;
    innconf->pathetc = xstrdup("../data/etc");
}

/* Create a fake channel with just enough data filled in to be able to use it
   to test article parsing. */
static CHANNEL *
fake_channel(void)
{
    CHANNEL *cp;
    static const CHANNEL CHANnull;

    cp = xmalloc(sizeof(CHANNEL));
    *cp = CHANnull;
    cp->Type = CTnntp;
    cp->State = CSgetheader;
    return cp;
}

/* Initialize things enough to be able to call ARTparse and friends.  This
   only has to be called once. */
static void
initialize(void)
{
    if (access("../data/etc/overview.fmt", F_OK) < 0)
        if (access("data/etc/overview.fmt", F_OK) == 0)
            if (chdir("innd") != 0)
                sysdie("Cannot cd to innd");
    fake_innconf();
    Log = fopen("/dev/null", "w");
    if (Log == NULL)
        sysdie("Cannot open /dev/null");
    fdreserve(4);
    buffer_set(&Path, "", 1);
    ARTsetup();
}

/* Given the test number, a path to an article and an expected error message
   (which may be ""), create a channel, run the article through ARTparse
   either all at once or, if slow is true, one character at a time, and check
   the result.  If shift is true, shift the start of the article in the buffer
   by a random amount.  Produces three test results. */
static void
ok_article(int n, const char *path, const char *error, bool slow, bool shift)
{
    CHANNEL *cp;
    char *article, *wire, *body;
    size_t i, len, wirelen, offset;
    struct stat st;
    bool okay = true;
    enum channel_state expected;

    article = ReadInFile(path, &st);
    len = st.st_size;
    wire = wire_from_native(article, len, &wirelen);
    cp = fake_channel();
    offset = shift ? random() % 50 : 0;
    cp->Start = offset;
    cp->Next = offset;
    buffer_resize(&cp->In, wirelen + offset);
    memset(cp->In.data, '\0', offset);
    cp->In.used = offset;
    ARTprepare(cp);
    if (slow) {
        for (i = 0; i < wirelen; i++) {
            cp->In.data[i + offset] = wire[i];
            cp->In.used++;
            message_handlers_warn(0);
            ARTparse(cp);
            message_handlers_warn(1, message_log_stderr);
            if (i < wirelen - 1 && cp->State != CSeatarticle)
                if (cp->State != CSgetheader && cp->State != CSgetbody) {
                    okay = false;
                    warn("Bad state %d at %ld", cp->State, (long) i);
                    break;
                }
        }
    } else {
        buffer_append(&cp->In, wire, wirelen);
        cp->In.used = offset + wirelen;
        message_handlers_warn(0);
        ARTparse(cp);
        message_handlers_warn(1, message_log_stderr);
    }
    ok(n++, okay);
    if (wirelen > (size_t) innconf->maxartsize)
        expected = CSgotlargearticle;
    else if (wirelen == 5)
        expected = CSnoarticle;
    else
        expected = CSgotarticle;
    ok_int(n++, expected, cp->State);
    ok_int(n++, wirelen, cp->Next - cp->Start);
    body = wire_findbody(wire, wirelen);
    if (body != NULL)
        ok_int(n++, body - wire, cp->Data.Body - offset);
    else
        ok_int(n++, cp->Start, cp->Data.Body);
    ok_string(n++, error, cp->Error);
    free(article);
    free(wire);
    free(cp);
}

int
main(void)
{
    size_t i;
    int n = 1;

    test_init(ARRAY_SIZE(articles) * 5 * 4);
    initialize();
    message_handlers_notice(0);

    for (i = 0; i < ARRAY_SIZE(articles); i++) {
        ok_article(n, articles[i].path, articles[i].error, false, false);
        n += 5;
        ok_article(n, articles[i].path, articles[i].error, true, false);
        n += 5;
        ok_article(n, articles[i].path, articles[i].error, false, true);
        n += 5;
        ok_article(n, articles[i].path, articles[i].error, true, true);
        n += 5;
    }
    return 0;
}
