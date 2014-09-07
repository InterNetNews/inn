/* $Id$ */
/* Test suite for auth_external. */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "tap/basic.h"
#include "tap/messages.h"

#include "../../nnrpd/nnrpd.h"

/* Need this because nnrpd is still bad about global variables. */
double IDLEtime = 0;

/* The basic information that we always send to the program. */
static const char info[] = "\
ClientHost: example.com\r\n\
ClientIP: 10.10.10.10\r\n\
ClientPort: 50000\r\n\
LocalIP: 10.0.0.2\r\n\
LocalPort: 119\r\n";

/* The additional information sent to authenticators. */
static const char userinfo[] = "\
ClientAuthname: tester\r\n\
ClientPassword: s0pers3cret\r\n";

/* Allocate a new client struct and fill it out with our test data. */
static struct client *
client_new(void)
{
    struct client *client;

    client = xmalloc(sizeof(struct client));
    strlcpy(client->host, "example.com", sizeof(client->host));
    strlcpy(client->ip, "10.10.10.10", sizeof(client->ip));
    strlcpy(client->serverip, "10.0.0.2", sizeof(client->serverip));
    client->port = 50000;
    client->serverport = 119;
    return client;
}

/* Validate the input file against the expected data.  Takes the current test
   number and a flag indicating whether we ran an authenticator. */
static void
validate_input(int n, bool auth)
{
    char *wanted, *seen;

    if (auth)
        wanted = concat(info, userinfo, ".\r\n", (char *) 0);
    else
        wanted = concat(info, ".\r\n", (char *) 0);
    seen = ReadInFile("input", NULL);
    if (seen == NULL) {
        syswarn("unable to read input");
        ok(n, false);
    } else {
        ok_string(n, wanted, seen);
        free(seen);
    }
    unlink("input");
    free(wanted);
}

/* Run the test authenticator, checking its input and output.  Takes the test
   number, the fake client struct, the argument to pass to the authenticator,
   the expected username, and the expected error output.  Tries it both as a
   resolver and an authenticator to be sure there are no surprises.  Returns
   the next test number. */
static int
ok_external(int n, struct client *client, const char *arg, const char *user,
            const char *error)
{
    char *result;
    char *command;

    command = concat("auth-test ", arg, (char *) 0);
    errors_capture();
    result = auth_external(client, command, ".", NULL, NULL);
    errors_uncapture();
    validate_input(n++, false);
    ok_string(n++, user, result);
    ok_string(n++, error, errors);
    if (errors && (error == NULL || strcmp(error, errors) != 0))
        warn(errors);
    free(errors);
    errors = NULL;

    errors_capture();
    result = auth_external(client, command, ".", "tester", "s0pers3cret");
    errors_uncapture();
    validate_input(n++, true);
    ok_string(n++, user, result);
    ok_string(n++, error, errors);
    if (errors && (error == NULL || strcmp(error, errors) != 0))
        warn(errors);
    free(errors);
    errors = NULL;

    return n;
}

int
main(void)
{
    struct client *client;
    int n = 1;

    if (access("auth-test", F_OK) < 0)
        if (access("nnrpd/auth-test", F_OK) == 0)
            chdir("nnrpd");
    client = client_new();

    test_init(11 * 6);

    n = ok_external(n, client, "okay", "tester", NULL);
    n = ok_external(n, client, "garbage", "tester", NULL);
    n = ok_external(n, client, "error", NULL,
                    "example.com auth: program error: This is an error\n");
    n = ok_external(n, client, "interspersed", "tester",
                    "example.com auth: program error: This is an error\n");
    n = ok_external(n, client, "empty", NULL, NULL);
    n = ok_external(n, client, "empty-error", NULL,
                    "example.com auth: program exited with status 1\n");
    n = ok_external(n, client, "okay-error", NULL,
                    "example.com auth: program exited with status 1\n");
    n = ok_external(n, client, "signal", NULL,
                    "example.com auth: program caught signal 1\n");
    n = ok_external(n, client, "newline", "tester", NULL);
    n = ok_external(n, client, "partial", "tester", NULL);
    ok_external(n, client, "partial-error", NULL,
                    "example.com auth: program error: This is an error\n");

    return 0;
}
