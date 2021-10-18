/* Test suite for auth_external. */

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include "inn/messages.h"
#include "tap/basic.h"
#include "tap/messages.h"
#include "tap/string.h"

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

/* Validate the input file against the expected data.  Takes a flag indicating
   whether we ran an authenticator. */
static void
validate_input(bool auth)
{
    char *wanted, *seen;

    if (auth)
        wanted = concat(info, userinfo, ".\r\n", (char *) 0);
    else
        wanted = concat(info, ".\r\n", (char *) 0);
    seen = ReadInFile("input", NULL);
    if (seen == NULL) {
        syswarn("unable to read input");
        ok(false, "no input");
    } else {
        is_string(wanted, seen, "input is as expected");
        free(seen);
    }
    unlink("input");
    free(wanted);
}

/* Run the test authenticator, checking its input and output.  Takes the fake
   client struct, the argument to pass to the authenticator, the expected
   username, and the expected error output.  Tries it both as a resolver and
   an authenticator to be sure there are no surprises.  Returns the next test
   number. */
static void
test_external(struct client *client, const char *arg, const char *user,
              const char *error)
{
    char *auth_test_path, *result, *command;

    diag("mode %s", arg);

    auth_test_path = test_file_path("nnrpd/auth-test");
    if (auth_test_path == NULL)
        bail("cannot find nnrpd/auth-test helper");

    basprintf(&command, "%s %s", auth_test_path, arg);
    errors_capture();
    result = auth_external(client, command, ".", NULL, NULL);
    errors_uncapture();
    validate_input(false);
    is_string(user, result, "user");
    is_string(error, errors, "errors");
    if (errors && (error == NULL || strcmp(error, errors) != 0))
        warn("%s", errors);
    free(errors);
    errors = NULL;

    errors_capture();
    result = auth_external(client, command, ".", "tester", "s0pers3cret");
    errors_uncapture();
    validate_input(true);
    is_string(user, result, "user with username and password");
    is_string(error, errors, "errors with username and password");
    if (errors && (error == NULL || strcmp(error, errors) != 0))
        warn("%s", errors);
    free(errors);
    errors = NULL;

    test_file_path_free(auth_test_path);
}

int
main(void)
{
    struct client *client;

    plan(12 * 6);

    client = client_new();

    test_external(client, "okay", "tester", NULL);
    test_external(client, "garbage", "tester", NULL);
    test_external(client, "error", NULL,
                  "example.com auth: program error: This is an error\n");
    test_external(client, "interspersed", "tester",
                  "example.com auth: program error: This is an error\n");
    test_external(client, "empty", NULL, NULL);
    test_external(client, "empty-error", NULL,
                  "example.com auth: program exited with status 1\n");
    test_external(client, "okay-error", NULL,
                  "example.com auth: program exited with status 1\n");
    test_external(client, "signal", NULL,
                  "example.com auth: program caught signal 1\n");
    test_external(client, "newline", "tester", NULL);
    test_external(client, "partial", "tester", NULL);
    test_external(client, "partial-close", "tester", NULL);
    test_external(client, "partial-error", NULL,
                  "example.com auth: program error: This is an error\n");

    return 0;
}
