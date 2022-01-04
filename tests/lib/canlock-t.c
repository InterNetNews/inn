/*
**  Test suite for functions related to Cancel-Lock.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include "inn/libinn.h"
#include "inn/secrets.h"
#include "inn/vector.h"
#include "tap/basic.h"

#define MID         "<12345@mid.example>"
#define USER        "JaneDoe"
#define FOLD        "\n\t"
#define ADMINSECRET "ExampleSecret"
#define USERSECRET  "AnotherSecret"

#define SHA1ADMINLOCK   "sha1:JD+QmQh5LH6lLLToKLcDl+Aemg0="
#define SHA256ADMINLOCK "sha256:s/pmK/3grrz++29ce2/mQydzJuc7iqHn1nqcJiQTPMc="
#define SHA1USERLOCK    "sha1:zdoRY4lJw5jLLtJTfpEqu1epqzc="
#define SHA256USERLOCK  "sha256:NSBTz7BfcQFTCen+U4lQ0VS8VIlZao2b8mxD/xJaaeE="

#define ADMINLOCKLINE SHA1ADMINLOCK " " SHA256ADMINLOCK
#define USERLOCKLINE  SHA1USERLOCK " " SHA256USERLOCK

#define SHA1ADMINKEY   "sha1:8HzrY7F4N+5SXkGQah1mcyW+01g="
#define SHA256ADMINKEY "sha256:qv1VXHYiCGjkX/N1nhfYKcAeUn8bCVhrWhoKuBSnpMA="
#define SHA1USERKEY    "sha1:mHkFFHF4DM97Oi+8vEsCXQb4aM0="
#define SHA256USERKEY  "sha256:yM0ep490Fzt83CLYYAytm3S2HasHhYG4LAeAlmuSEys="

#define ADMINKEYLINE SHA1ADMINKEY " " SHA256ADMINKEY
#define USERKEYLINE  SHA1USERKEY " " SHA256USERKEY


#if defined(HAVE_CANLOCK)
/* Build a stripped-down secrets struct that contains only those settings that
 * Cancel-Lock cares about. */
static void
fake_secrets(void)
{
    secrets = xmalloc(sizeof(*secrets));
    memset(secrets, 0, sizeof(*secrets));
    secrets->canlockadmin = vector_new();
    secrets->canlockuser = vector_new();
}

/* Tests when both canlockadmin and canlockuser are unset. */
static void
testgen_cancel_lock_empty(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_lock(MID, "", &canbuff);
    is_bool(true, result, "no admin-only Cancel-Lock generated");
    is_string(canbuff, "", "...and correctly empty");

    result = gen_cancel_lock(MID, USER, &canbuff);
    is_bool(true, result, "no admin and user Cancel-Lock generated");
    is_string(canbuff, "", "...and correctly empty");

    free(canbuff);
}

/* Tests with 1 secret in canlockadmin, and canlockuser unset. */
static void
testgen_cancel_lock_admin_only(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_lock(MID, "", &canbuff);
    is_bool(true, result, "admin-only Cancel-Lock generated");
    is_string(canbuff, ADMINLOCKLINE, "...and correct");

    result = gen_cancel_lock(MID, USER, &canbuff);
    is_bool(true, result, "admin without user Cancel-Lock generated");
    is_string(canbuff, ADMINLOCKLINE, "...and correct");

    free(canbuff);
}

/* Tests with canlockadmin unset, and 1 secret in canlockuser. */
static void
testgen_cancel_lock_user_only(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_lock(MID, "", &canbuff);
    is_bool(true, result, "no admin-only Cancel-Lock generated");
    is_string(canbuff, "", "...and correctly empty");

    result = gen_cancel_lock(MID, USER, &canbuff);
    is_bool(true, result, "user without admin Cancel-Lock generated");
    is_string(canbuff, USERLOCKLINE, "...and correct");

    free(canbuff);
}

/* Tests with 1 secret in canlockadmin, and 1 secret in canlockuser. */
static void
testgen_cancel_lock(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_lock(MID, "", &canbuff);
    is_bool(true, result, "admin-only Cancel-Lock generated");
    is_string(canbuff, ADMINLOCKLINE, "...and correct");

    result = gen_cancel_lock(MID, USER, &canbuff);
    is_bool(true, result, "admin and user Cancel-Lock generated");
    is_string(canbuff, ADMINLOCKLINE FOLD USERLOCKLINE, "...and correct");

    free(canbuff);
}

/* Tests with 3 secrets in canlockadmin, and 2 secrets in canlockuser. */
static void
testgen_cancel_lock_multi(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_lock(MID, "", &canbuff);
    is_bool(true, result, "multiple admin-only Cancel-Lock generated");
    is_string(canbuff, ADMINLOCKLINE FOLD ADMINLOCKLINE FOLD ADMINLOCKLINE,
              "...and correct");

    result = gen_cancel_lock(MID, USER, &canbuff);
    is_bool(true, result, "multiple admin and user Cancel-Lock generated");
    is_string(canbuff,
              ADMINLOCKLINE FOLD ADMINLOCKLINE FOLD ADMINLOCKLINE FOLD
                  USERLOCKLINE FOLD USERLOCKLINE,
              "...and correct");

    free(canbuff);
}

/* Tests when both canlockadmin and canlockuser are unset. */
static void
testgen_cancel_key_empty(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_key("cancel " MID, NULL, "", &canbuff);
    is_bool(true, result, "no admin-only Cancel-Key generated");
    is_string(canbuff, "", "...and correctly empty");

    result = gen_cancel_key("cancel " MID, NULL, USER, &canbuff);
    is_bool(true, result, "no user-only Cancel-Key generated");
    is_string(canbuff, "", "...and correctly empty");

    free(canbuff);
}

/* Tests with 1 secret in canlockadmin, and canlockuser unset. */
static void
testgen_cancel_key_admin_only(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_key("cancel " MID, NULL, "", &canbuff);
    is_bool(true, result, "admin-only Cancel-Key generated");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result = gen_cancel_key("cancel " MID, NULL, USER, &canbuff);
    is_bool(true, result, "no user-only Cancel-Key generated");
    is_string(canbuff, "", "...and correctly empty");

    free(canbuff);
}

/* Tests with canlockadmin empty, and 1 secret in canlockuser. */
static void
testgen_cancel_key_user_only(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_key("cancel " MID, NULL, "", &canbuff);
    is_bool(true, result, "no admin-only Cancel-Key generated");
    is_string(canbuff, "", "...and correctly empty");

    result = gen_cancel_key("cancel " MID, NULL, USER, &canbuff);
    is_bool(true, result, "user-only Cancel-Key generated");
    is_string(canbuff, USERKEYLINE, "...and correct");

    free(canbuff);
}

/* Tests with 1 secret in canlockadmin, and 1 secret in canlockuser. */
static void
testgen_cancel_key(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_key("cancel " MID, NULL, "", &canbuff);
    is_bool(true, result, "admin-only Cancel-Key generated for a cancel");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result = gen_cancel_key("\tCaNceL \t  " MID "  ", NULL, "", &canbuff);
    is_bool(true, result,
            "admin-only Cancel-Key generated for a cancel with whitespace");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result = gen_cancel_key(NULL, MID, "", &canbuff);
    is_bool(true, result, "admin-only Cancel-Key generated for a supersedes");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result = gen_cancel_key(NULL, "\t " MID "  \t ", "", &canbuff);
    is_bool(
        true, result,
        "admin-only Cancel-Key generated for a supersedes with whitespace");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result =
        gen_cancel_key("cancel " MID, "<another@mid.example>", "", &canbuff);
    is_bool(true, result,
            "admin-only Cancel-Key generated looking for a cancel first");
    is_string(canbuff, ADMINKEYLINE, "...and correct");

    result = gen_cancel_key(MID, NULL, "", &canbuff);
    is_bool(false, result, "no Cancel-Key generated with missing cancel word");

    result = gen_cancel_key("cancelling " MID, NULL, "", &canbuff);
    is_bool(false, result,
            "no Cancel-Key generated with mispelled cancel word");

    result = gen_cancel_key(NULL, NULL, "", &canbuff);
    is_bool(false, result,
            "no Cancel-Key generated if neither cancel nor supersedes");

    result = gen_cancel_key("cancel " MID, NULL, USER, &canbuff);
    is_bool(true, result, "user-only Cancel-Key generated for a cancel");
    is_string(canbuff, USERKEYLINE, "...and correct");

    free(canbuff);
}

/* Tests with 3 secrets in canlockadmin, and 2 secrets in canlockuser. */
static void
testgen_cancel_key_multi(void)
{
    char *canbuff = NULL;
    bool result;

    result = gen_cancel_key("cancel " MID, NULL, "", &canbuff);
    is_bool(true, result, "multiple admin-only Cancel-Key generated");
    is_string(canbuff, ADMINKEYLINE FOLD ADMINKEYLINE FOLD ADMINKEYLINE,
              "...and correct");

    result = gen_cancel_key("cancel " MID, NULL, USER, &canbuff);
    is_bool(true, result, "multiple user-only Cancel-Key generated");
    is_string(canbuff, USERKEYLINE FOLD USERKEYLINE, "...and correct");

    free(canbuff);
}

int
main(void)
{
    plan(4 * 9 + 15);

    fake_secrets();

    /* Without any secrets. */
    testgen_cancel_lock_empty();
    testgen_cancel_key_empty();

    /* With 1 secret in canlockadmin. */
    vector_add(secrets->canlockadmin, ADMINSECRET);
    testgen_cancel_lock_admin_only();
    testgen_cancel_key_admin_only();

    /* With 1 secret in canlockuser. */
    vector_clear(secrets->canlockadmin);
    vector_add(secrets->canlockuser, USERSECRET);
    testgen_cancel_lock_user_only();
    testgen_cancel_key_user_only();

    /* With 1 secret in canlockadmin and 1 secret in canlockuser. */
    vector_add(secrets->canlockadmin, ADMINSECRET);
    testgen_cancel_lock();
    testgen_cancel_key();

    /* With 3 secrets in canlockadmin and 2 secrets in canlockuser. */
    vector_add(secrets->canlockadmin, ADMINSECRET);
    vector_add(secrets->canlockadmin, ADMINSECRET);
    vector_add(secrets->canlockuser, USERSECRET);
    testgen_cancel_lock_multi();
    testgen_cancel_key_multi();

    return 0;
}
#else
int
main(void)
{
    skip_all("libcanlock not available");

    return 0;
}
#endif /* HAVE_CANLOCK */
