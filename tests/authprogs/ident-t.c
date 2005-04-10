/* $Id$ */
/* ident test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/wait.h"
#include <pwd.h>

#include "inn/messages.h"
#include "inn/network.h"
#include "libtest.h"

/* The path to the uninstalled ident resolver. */
static const char ident[] = "../../authprogs/ident";

/* This is the client side of the connection.  It's here just to set up a
   network connection we can ask identd about.  Wait for a line from the
   server and then close the connection. */
static void
client(const char *host)
{
    int fd;
    char buffer[32];

    fd = network_connect_host(host, 11119, NULL);
    if (fd < 0)
        _exit(1);
    read(fd, buffer, sizeof(buffer));
    close(fd);
}

/* The server side of the connection.  Listen on port 11119, fork off a client
   connection, accept the connection, and then run the ident resolver to get
   the results. */
static int
server_ipv4(int n)
{
    int fd, conn;
    pid_t child, result;
    struct sockaddr_in sin;
    socklen_t size;
    unsigned short port;
    int input[2];
    int output[2];
    FILE *infile;
    char buffer[256], wanted[256];
    struct passwd *pwd;
    ssize_t status;
    bool success = true;

    /* Make sure we can figure out our username.  If we can't, ident isn't
       going to be able to do so either. */
    pwd = getpwuid(getuid());
    if (pwd == NULL) {
        skip_block(n, 4, "unknown username");
        return n + 4;
    }
    snprintf(wanted, sizeof(wanted), "User:%s\n", pwd->pw_name);

    /* Create the network connection so ident has something to look at. */
    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd < 0)
        sysdie("cannot bind");
    if (listen(fd, 1) < 0)
        sysdie("cannot listen");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    if (child == 0) {
        client("127.0.0.1");
        _exit(0);
    }
    size = sizeof(sin);
    conn = accept(fd, &sin, &size);
    close(fd);
    port = ntohs(sin.sin_port);

    /* Now, fork off the ident resolver and make sure it works. */
    if (pipe(input) < 0 || pipe(output) < 0)
        sysdie("cannot create pipe");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        if (dup2(input[0], 0) < 0)
            _exit(1);
        close(input[1]);
        if (dup2(output[1], 1) < 0)
            _exit(1);
        if (dup2(output[1], 2) < 0)
            _exit(1);
        close(output[0]);
        if (execl(ident, ident, (char *) 0) < 0)
            _exit(1);
    } else {
        close(input[0]);
        close(output[1]);
        infile = fdopen(input[1], "w");
        if (infile == NULL)
            sysdie("cannot fdopen");
        fprintf(infile, "ClientHost: localhost\r\n");
        fprintf(infile, "ClientIP: 127.0.0.1\r\n");
        fprintf(infile, "ClientPort: %hu\r\n", port);
        fprintf(infile, "LocalIP: 127.0.0.1\r\n");
        fprintf(infile, "LocalPort: 11119\r\n");
        fprintf(infile, ".\r\n");
        if (fclose(infile) == EOF)
            sysdie("cannot flush output to ident");
        ok(n++, true);
        status = read(output[0], buffer, sizeof(buffer) - 1);
        if (status < 0)
            ok(n++, false);
        else {
            buffer[status] = '\0';
            if (strncmp("ident: ", buffer, 7) == 0) {
                skip(n++, "ident server not running or not responding");
                success = false;
            } else
                ok_string(n++, wanted, buffer);
        }
        result = waitpid(child, &status, 0);
        if (result != child)
            die("cannot wait for innbind");
        ok(n++, WIFEXITED(status));
        if (success)
            ok(n++, WEXITSTATUS(status) == 0);
        else
            ok(n++, WEXITSTATUS(status) == 1);
    }
    close(conn);
    return n;
}

int
main(void)
{
    int n;

    if (access("ident.t", F_OK) < 0)
        if (access("authprogs/ident.t", F_OK) == 0)
            chdir("authprogs");

    test_init(4);
    n = server_ipv4(1);

    return 0;
}
