/*  $Id$
**
**  An extremely lightweight receive-only NNTP server.
**
**  Copyright 2003, 2004 Russ Allbery <eagle@eyrie.org>
**
**  Permission is hereby granted, free of charge, to any person obtaining a
**  copy of this software and associated documentation files (the "Software"),
**  to deal in the Software without restriction, including without limitation
**  the rights to use, copy, modify, merge, publish, distribute, sublicense,
**  and/or sell copies of the Software, and to permit persons to whom the
**  Software is furnished to do so, subject to the following conditions:
**
**  The above copyright notice and this permission notice shall be included in
**  all copies or substantial portions of the Software.
**
**  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
**  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
**  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
**  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
**  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
**  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
**  DEALINGS IN THE SOFTWARE.
*/

#include "config.h"
#include "clibrary.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "inn/dispatch.h"
#include "inn/messages.h"
#include "inn/md5.h"
#include "inn/nntp.h"
#include "inn/utility.h"
#include "inn/vector.h"
#include "inn/version.h"
#include "inn/libinn.h"

/* Prototypes for command callbacks. */
static void command_help(struct cvector *, void *);
static void command_ihave(struct cvector *, void *);
static void command_quit(struct cvector *, void *);

/* The actual command dispatch table for TinyNNTP.  This table MUST be
   sorted. */
const struct dispatch commands[] = {
    { "help",  command_help,  0, 0, NULL },
    { "ihave", command_ihave, 1, 1, NULL },
    { "quit",  command_quit,  0, 0, NULL }
};

/* Global state for the daemon. */
struct state {
    struct nntp *nntp;
    unsigned long count;
    unsigned long duplicates;
    FILE *processor;
};


/*
**  Do a clean shutdown, which mostly just involves closing the open processor
**  pipe, if present, and reporting on any abnormal exit status.
*/
static void
shutdown(struct state *state)
{
    int status;

    if (state->processor != NULL) {
        status = pclose(state->processor);
        if (status == -1)
            syswarn("unable to wait for processor");
        else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            warn("processor exited with status %d", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            warn("processor killed by signal %d", WTERMSIG(status));
    }
    notice("processed %lu articles, rejected %lu duplicates", state->count,
           state->duplicates);
    nntp_free(state->nntp);
    exit(0);
}


/*
**  Handle an IHAVE command.  For the time being, we just ignore the message
**  ID argument.
*/
static void
command_ihave(struct cvector *command, void *cookie)
{
    struct state *state = cookie;
    enum nntp_status status;
    char filename[33], *article, *msgid;
    unsigned char hash[16];
    size_t length;
    int fd, oerrno;

    msgid = xstrdup(command->strings[1]);
    if (!nntp_respond(state->nntp, NNTP_CONT_IHAVE, "Send article"))
        sysdie("cannot flush output");
    status = nntp_read_multiline(state->nntp, &article, &length);
    switch (status) {
    case NNTP_READ_OK:
        break;
    case NNTP_READ_EOF:
        die("connection closed while receiving article");
        shutdown(state);
        break;
    case NNTP_READ_ERROR:
        sysdie("network error while receiving article");
        break;
    case NNTP_READ_TIMEOUT:
        die("network timeout while receiving article");
        break;
    case NNTP_READ_LONG:
        warn("article %s exceeds maximum size", msgid);
        free(msgid);
        return;
    default:
        sysdie("internal: unknown NNTP library status");
        break;
    }
    md5_hash((unsigned char *) msgid, strlen(msgid), hash);
    inn_encode_hex(hash, sizeof(hash), filename, sizeof(filename));
    fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        if (errno == EEXIST) {
            if (!nntp_respond(state->nntp, NNTP_FAIL_IHAVE_REFUSE,
                              "Duplicate"))
                sysdie("cannot flush output");
            state->duplicates++;
            free(msgid);
            return;
        }
        sysdie("unable to create article file %s", filename);
    }
    if (xwrite(fd, article, length) < 0) {
        oerrno = errno;
        if (unlink(filename) < 0)
            syswarn("cannot clean up failed write to %s, remove by hand",
                filename);
        errno = oerrno;
        sysdie("unable to write article %s to file %s", msgid,
               filename);
    }
    close(fd);
    if (state->processor != NULL) {
        fprintf(state->processor, "%s %s\n", filename, msgid);
        if (fflush(state->processor) == EOF || ferror(state->processor))
            sysdie("unable to flush %s to processor", filename);
    }
    state->count++;
    if (!nntp_respond(state->nntp, NNTP_OK_IHAVE, "Article received"))
        sysdie("cannot flush output");
    free(msgid);
}


/*
**  Process a HELP command, sending some useful (?) information.
*/
static void
command_help(struct cvector *command UNUSED, void *cookie)
{
    struct state *state = cookie;

    nntp_respond_noflush(state->nntp, NNTP_INFO_HELP, "tinyfeed from %s",
                         INN_VERSION_STRING);
    nntp_send_line_noflush(state->nntp, "Supported commands:");
    nntp_send_line_noflush(state->nntp, " ");
    nntp_send_line_noflush(state->nntp, "  IHAVE <message-id>");
    nntp_send_line_noflush(state->nntp, "  HELP");
    nntp_send_line_noflush(state->nntp, "  QUIT");
    nntp_send_line_noflush(state->nntp, ".");
    if (!nntp_flush(state->nntp))
        sysdie("cannot flush output");
}


/*
**  Process a QUIT command, closing down the server.
*/
static void
command_quit(struct cvector *command UNUSED, void *cookie)
{
    struct state *state = cookie;

    if (!nntp_respond(state->nntp, NNTP_OK_QUIT, "Goodbye"))
        syswarn("cannot flush output during QUIT");
    shutdown(state);
}


/*
**  Process a syntax error.
*/
static void
command_syntax(struct cvector *command UNUSED, void *cookie)
{
    struct state *state = cookie;

    if (!nntp_respond(state->nntp, NNTP_ERR_SYNTAX, "Syntax error"))
        sysdie("cannot flush output");
}


/*
**  Process an unknown command.
*/
static void
command_unknown(struct cvector *command UNUSED, void *cookie)
{
    struct state *state = cookie;

    if (!nntp_respond(state->nntp, NNTP_ERR_COMMAND, "Unsupported command"))
        sysdie("cannot flush output");
}


int
main(int argc, char *argv[])
{
    struct state state = { NULL, 0, 0, NULL };
    struct cvector *command;
    enum nntp_status status;

    message_program_name = "tinyfeed";
    message_handlers_notice(1, message_log_syslog_info);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_die(1, message_log_syslog_warning);

    /* Change to the spool directory where all articles will be written. */
    if (argc < 2)
        die("no spool directory specified");
    if (chdir(argv[1]) < 0)
        sysdie("cannot change directory to %s", argv[1]);

    /* If a processing command was specified, open a pipe to it. */
    if (argc == 3) {
        state.processor = popen(argv[2], "w");
        if (state.processor == NULL)
            sysdie("cannot open a pipe to %s", argv[2]);
        setvbuf(state.processor, NULL, _IONBF, BUFSIZ);
    }

    /* Go into the main input loop.  The only commands we support, for now,
       are HELP, IHAVE and QUIT. */
    notice("starting");
    state.nntp = nntp_new(STDIN_FILENO, STDOUT_FILENO, 1024 * 1024, 10 * 60);
    nntp_respond(state.nntp, NNTP_OK_BANNER_NOPOST, "tinyfeed ready");
    command = cvector_new();
    while (1) {
        status = nntp_read_command(state.nntp, command);
        switch (status) {
        case NNTP_READ_OK:
            dispatch(command, commands, ARRAY_SIZE(commands), command_unknown,
                     command_syntax, &state);
            break;
        case NNTP_READ_EOF:
            notice("connection closed");
            shutdown(&state);
            break;
        case NNTP_READ_ERROR:
            sysdie("network error");
            break;
        case NNTP_READ_TIMEOUT:
            notice("network timeout");
            shutdown(&state);
            break;
        case NNTP_READ_LONG:
            warn("long line received");
            break;
        default:
            sysdie("internal: unknown NNTP library status");
            break;
        }
    }
}
