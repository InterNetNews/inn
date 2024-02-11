/*
**  Connect to the NNTP server and feed one article.
*/

#include "portable/system.h"

#include <errno.h>
#include <time.h>

#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/nntp.h"


static FILE *FromServer;
static FILE *ToServer;
static int Tracing;


/*
**  Read a line from the server or die trying.
*/
static void
GetFromServer(char *buff, int size, const char *text)
{
    if (fgets(buff, size, FromServer) == NULL)
        sysdie("%s", text);
    if (Tracing)
        printf("S: %s", buff);
}


/*
**  Flush a stdio FILE; exit if there are any errors.
*/
static void
SafeFlush(FILE *F)
{
    if (fflush(F) == EOF || ferror(F))
        sysdie("cannot send text to server");
}


__attribute__((__noreturn__)) static void
SendQuit(int x)
{
    char buff[BUFSIZ];

    /* Close up. */
    fprintf(ToServer, "quit\r\n");
    SafeFlush(ToServer);
    fclose(ToServer);
    GetFromServer(buff, sizeof buff, "cannot get reply to quit");
    exit(x);
}


__attribute__((__noreturn__)) static void
Usage(void)
{
    fprintf(stderr, "Usage: feedone [-r|-m msgid] [-p] [-t] articlefile\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    static char MESGIDHDR[] = "Message-ID:";
    int i;
    FILE *F;
    char buff[BUFSIZ];
    char *mesgid = NULL;
    char *p;
    char *q;
    bool PostMode;

    /* Set defaults. */
    PostMode = false;
    message_program_name = "feedone";

    /* Parse JCL. */
    while ((i = getopt(ac, av, "m:prt")) != EOF)
        switch (i) {
        default:
            Usage();
            /* NOTREACHED */
        case 'm': /* Specified Message-ID */
            if (*optarg == '<')
                mesgid = optarg;
            else
                mesgid = concat("<", optarg, ">", (char *) 0);
            break;
        case 'p': /* Use POST, not IHAVE */
            PostMode = true;
            break;
        case 'r': /* Random Message-ID */
            xasprintf(&mesgid, "<%ld@%ld>", (long) getpid(),
                      (long) time(NULL));
            break;
        case 't':
            Tracing = true;
            break;
        }
    ac -= optind;
    av += optind;

    /* One argument; the input filename. */
    if (ac != 1)
        Usage();
    if ((F = fopen(av[0], "r")) == NULL)
        sysdie("cannot open input");

    /* Scan for the message-id. */
    if (mesgid == NULL) {
        while (fgets(buff, sizeof buff, F) != NULL)
            if (strncmp(buff, MESGIDHDR, strlen(MESGIDHDR)) == 0) {
                if ((p = strchr(buff, '<')) == NULL
                    || (q = strchr(p, '>')) == NULL)
                    die("bad message ID line");
                q[1] = '\0';
                mesgid = xstrdup(p);
                break;
            }
        if (mesgid == NULL)
            die("no message ID");
    }

    /* Connect to the server. */
    if (NNTPremoteopen(NNTP_PORT, &FromServer, &ToServer, buff, sizeof(buff))
            < 0
        || FromServer == NULL || ToServer == NULL) {
        if (buff[0])
            warn("server says: %s", buff);
        sysdie("cannot connect to server");
    }

    /* Does the server want this article? */
    if (PostMode) {
        fprintf(ToServer, "post\r\n");
        i = NNTP_CONT_POST;
    } else {
        fprintf(ToServer, "ihave %s\r\n", mesgid);
        i = NNTP_CONT_IHAVE;
    }
    SafeFlush(ToServer);
    GetFromServer(buff, sizeof buff, "cannot offer article to server");
    if (atoi(buff) != i) {
        warn("server doesn't want the article: %s", buff);
        SendQuit(1);
    }

    /* Send the file over. */
    fseeko(F, 0, SEEK_SET);
    while (fgets(buff, sizeof buff, F) != NULL) {
        if (strncmp(buff, MESGIDHDR, strlen(MESGIDHDR)) == 0) {
            fprintf(ToServer, "%s %s\r\n", MESGIDHDR, mesgid);
            continue;
        }
        if ((p = strchr(buff, '\n')) != NULL)
            *p = '\0';
        fprintf(ToServer, buff[0] == '.' ? ".%s\r\n" : "%s\r\n", buff);
        SafeFlush(ToServer);
    }
    fprintf(ToServer, ".\r\n");
    SafeFlush(ToServer);
    fclose(F);

    /* How did the server respond? */
    GetFromServer(buff, sizeof buff,
                  "no reply from server after sending the article");
    i = PostMode ? NNTP_OK_POST : NNTP_OK_IHAVE;
    if (atoi(buff) != i)
        sysdie("cannot send article to the server: %s", buff);

    SendQuit(0);

    /* NOTREACHED */
    return 1;
}
