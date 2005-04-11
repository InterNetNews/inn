/*  $Id$
**
**  External authenticator support.
**
**  Run an external resolver or authenticator to determine the username of the
**  client and return that information to INN.  For more information about the
**  protocol used, see doc/external-auth.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
#include <errno.h>
#include <signal.h>

#include "inn/buffer.h"
#include "inn/messages.h"
#include "inn/vector.h"
#include "nnrpd.h"

/* Holds the details about a running child process. */
struct process {
    pid_t pid;
    int read_fd;                /* Read from child. */
    int write_fd;               /* Write to child. */
    int error_fd;
};


/*
**  Given the client information struct, a string indicating the program to
**  run (possibly including arguments) and the directory in which to look for
**  the command if it's not fully-qualified, start that program and return a
**  struct process providing the PID and file descriptors.
*/
static struct process *
start_process(struct client *client, const char *command, const char *dir)
{
    struct process *process;
    int rd[2], wr[2], er[2];
    pid_t pid;
    char *path;
    struct vector *args;
    bool allocated = false;

    /* Parse the command and find the path to the binary. */
    args = vector_split_space(command, NULL);
    path = args->strings[0];
    if (path[0] != '/') {
        path = concatpath(dir, path);
        allocated = true;
    }

    /* Set up the pipes and run the program. */
    if (pipe(rd) < 0 || pipe(wr) < 0 || pipe(er) < 0) {
        syswarn("%s auth: cannot create pipe", client->host);
        return NULL;
    }
    pid = fork();
    switch (pid) {
    case -1:
        close(rd[0]); close(rd[1]);
        close(wr[0]); close(wr[1]);
        close(er[0]); close(er[1]);
        syswarn("%s auth: cannot fork", client->host);
        return NULL;
    case 0:
        if (dup2(wr[0], 0) < 0 || dup2(rd[1], 1) < 0 || dup2(er[1], 2) < 0) {
            syswarn("%s auth: cannot set up file descriptors", client->host);
            _exit(1);
        }
        close(rd[0]); close(rd[1]);
        close(wr[0]); close(wr[1]);
        close(er[0]); close(er[1]);
        if (vector_exec(path, args) < 0) {
            syswarn("%s auth: cannot exec %s", client->host, path);
            _exit(1);
        }
    }

    /* In the parent.  Close excess file descriptors and build return. */
    close(rd[1]);
    close(wr[0]);
    close(er[1]);
    process = xmalloc(sizeof(struct process));
    process->pid = pid;
    process->read_fd = rd[0];
    process->write_fd = wr[1];
    process->error_fd = er[0];
    return process;
}


/*
**  Handle an result line from the program which has already been
**  nul-terminated at the end of the line.  If User:<username> is seen, point
**  the second argument at newly allocated space for it.
*/
static void
handle_result(struct client *client UNUSED, const char *line, char **user)
{
    if (strncasecmp(line, "User:", strlen("User:")) == 0) {
        if (*user != NULL)
            free(*user);
        *user = xstrdup(line + strlen("User:"));
    }
}


/*
**  Handle an error line from the program by logging it.
*/
static void
handle_error(struct client *client, const char *line, char **user UNUSED)
{
    notice("%s auth: program error: %s", client->host, line);
}


/*
**  Read a line of data from the given file descriptor.  Return the number of
**  bytes read or -1 on buffer overflow.  Takes the file descriptor, the
**  buffer used for that file descriptor, and the handler function to call for
**  each line.  Points the fourth argument to a username, if one was found.
*/
static ssize_t
output(struct client *client, int fd, struct buffer *buffer,
       void (*handler)(struct client *, const char *, char **), char **user)
{
    char *line;
    char *start;
    ssize_t count;

    /* Read the data. */
    buffer_compact(buffer);
    count = buffer_read(buffer, fd);
    if (buffer->left >= buffer->size - 1)
        return -1;
    if (count < 0)
        return count;

    /* If reached end of file, process anything left as a line. */
    if (count == 0) {
        if (buffer->left > 0) {
            buffer->data[buffer->used + buffer->left] = '\0';
            handler(client, buffer->data + buffer->used, user);
            buffer->used += buffer->left;
            buffer->left = 0;
        }
        return count;
    }

    /* Otherwise, break what we got up into lines and process each one. */
    start = buffer->data + buffer->used;
    line = memchr(start, '\n', buffer->left);
    while (line != NULL) {
        *line = '\0';
        if (line > start && line[-1] == '\r')
            line[-1] = '\0';
        handler(client, start, user);
        buffer->used += line - start + 1;
        buffer->left -= line - start + 1;
        start = buffer->data + buffer->used;
        line = memchr(start, '\n', buffer->left);
    }
    return count;
}


/*
**  Wait for the program to produce output.  For each bit of output, call
**  handle_output with the appropriate handler function.  After end of file or
**  an error, check and report on the exit status.  Returns the username in
**  newly allocated memory, or NULL if none was found.
**
**  Currently, use a hard-coded five-second timeout for all programs.  This
**  might need to be configurable later.
*/
static char *
handle_output(struct client *client, struct process *process)
{
    fd_set fds, rfds;
    struct timeval tv;
    int maxfd, status, fd;
    ssize_t count;
    pid_t result;
    struct buffer *readbuf;
    struct buffer *errorbuf;
    double start, end;
    bool found;
    bool killed = false;
    bool errored = false;
    char *user = NULL;

    FD_ZERO(&fds);
    FD_SET(process->read_fd, &fds);
    FD_SET(process->error_fd, &fds);
    maxfd = process->read_fd > process->error_fd
        ? process->read_fd : process->error_fd;
    readbuf = buffer_new();
    buffer_resize(readbuf, 1024);
    errorbuf = buffer_new();
    buffer_resize(errorbuf, 1024);

    /* Loop until we get an error or end of file. */
    while (1) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rfds = fds;
        start = TMRnow_double();
        status = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        end = TMRnow_double();
        IDLEtime += end - start;
        if (status <= 0) {
            if (status == 0)
                syswarn("%s auth: program timeout", client->host);
            else {
                if (errno == EINTR)
                    continue;
                syswarn("%s auth: select failed", client->host);
            }
            killed = true;
            kill(process->pid, SIGTERM);
            break;
        }
        found = false;
        count = 0;
        if (FD_ISSET(process->read_fd, &rfds)) {
            fd = process->read_fd;
            count = output(client, fd, readbuf, handle_result, &user);
            if (count > 0)
                found = true;
        }
        if (count >= 0 && FD_ISSET(process->error_fd, &rfds)) {
            fd = process->error_fd;
            count = output(client, fd, errorbuf, handle_error, &user);
            if (count > 0) {
                found = true;
                errored = true;
            }
        }
        if (!found) {
            close(process->read_fd);
            close(process->error_fd);
            if (count < 0) {
                warn("%s auth: output too long from program", client->host);
                killed = true;
                kill(process->pid, SIGTERM);
            }
            break;
        }
    }
    buffer_free(readbuf);
    buffer_free(errorbuf);

    /* Wait for the program to exit. */
    do {
        result = waitpid(process->pid, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result != process->pid) {
        syswarn("%s auth: cannot wait for program", client->host);
        goto fail;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return user;
    else {
        if (WIFSIGNALED(status) && (!killed || WTERMSIG(status) != SIGTERM))
            notice("%s auth: program caught signal %d", client->host,
                   WTERMSIG(status));
        else if (WIFEXITED(status) && !errored)
            notice("%s auth: program exited with status %d", client->host,
                   WEXITSTATUS(status));
    }

fail:
    if (user != NULL)
        free(user);
    return NULL;
}


/*
**  Append the standard connection information to the provided buffer.
*/
static void
append_client_info(struct client *client, struct buffer *data)
{
    if (*client->host)
        buffer_sprintf(data, true, "ClientHost: %s\r\n", client->host);
    if (*client->ip)
        buffer_sprintf(data, true, "ClientIP: %s\r\n", client->ip);
    if (client->port != 0)
        buffer_sprintf(data, true, "ClientPort: %hu\r\n", client->port);
    if (*client->serverip)
        buffer_sprintf(data, true, "LocalIP: %s\r\n", client->serverip);
    if (client->serverport != 0)
        buffer_sprintf(data, true, "LocalPort: %hu\r\n", client->serverport);
}


/*
**  Execute a program to get the remote username.  Takes the client info, the
**  command to run, the subdirectory in which to look for programs, and
**  optional username and password information to pass tot he program.
**  Returns the username in newly allocated memory if successful, NULL
**  otherwise.
*/
char *
auth_external(struct client *client, const char *command,
              const char *directory, const char *username,
              const char *password)
{
    char *user;
    struct process *process;
    struct buffer *input;

    /* Start the program. */
    process = start_process(client, command, directory);
    if (process == NULL)
        return NULL;

    /* Feed it data. */
    input = buffer_new();
    append_client_info(client, input);
    if (username != NULL)
        buffer_sprintf(input, true, "ClientAuthname: %s\r\n", username);
    if (password != NULL)
        buffer_sprintf(input, true, "ClientPassword: %s\r\n", password);
    buffer_sprintf(input, true, ".\r\n");
    xwrite(process->write_fd, input->data, input->left);
    close(process->write_fd);
    buffer_free(input);

    /* Get the results. */
    user = handle_output(client, process);
    free(process);
    return user;
}
