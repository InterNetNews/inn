/*
**  Cancel-Lock functionality.
**
**  These functions are used to implement RFC 8315 in InterNetNews.
**  Initial implementation by Julien ÉLIE.
*/

#include "portable/system.h"

#include "inn/libinn.h"
#include "inn/secrets.h"
#include "inn/vector.h"

#if defined(HAVE_CANLOCK)
#    include <libcanlock-3/canlock.h>


/*  Generate c-lock elements for the Cancel-Lock header field.
**  This function expects a Message-ID, a username (possibly NULL) and the
**  address of a pointer where it will write c-lock elements:
**    sha1:<28-byte Base64-encoded hash> sha256:<44-byte Base64-encoded hash>
**  for each admin secret in the canlockadmin vector, and for each user secret
**  in the canlockuser vector if the user has an identity (that is to say if
**  username is not NULL).
**
**  The length of each pair of c-lock elements is: 4+1+28+1+6+1+44=85 bytes
**  plus "\n\t" (2 bytes) for the folding (without '\n') if another c-lock pair
**  is written, and 1 byte for the final '\0'.
**
**  The canbuff string will be allocated by this function, unless it returns
**  false.  This string is suitable to be used as the body of a Cancel-Lock
**  header field.
**  The caller is reponsible for freeing it.
**
**  Returns true if all c-lock elements were written, false on failure.
*/
bool
gen_cancel_lock(const char *msgid, const char *username, char **canbuff)
{
    char *c_lock, *datalock;
    const char *msgidstart, *p;
    size_t i;
    size_t msgidlen;
    size_t canlockseclen;
    size_t canbufflen =
        (85 + 2) * (secrets->canlockadmin->count + secrets->canlockuser->count)
        + 1; /* Computation explained above, with 2 extra bytes. */

    *canbuff = xmalloc(canbufflen);
    **canbuff = '\0';

    /* Grab the message-ID without leading and trailing spaces to obtain the
     * right data for the computation of c-lock elements. */
    msgidstart = msgid;
    for (; ISWHITE(*msgidstart); msgidstart++)
        ;
    p = msgidstart;
    for (; !ISWHITE(*p) && *p != '\n' && *p != '\0'; p++)
        ;
    msgidlen = (p - msgidstart);

    /* Generate admin c-lock elements.
     * Use both sha1 and sha256 schemes for interoperability reasons (not all
     * news software implement the mandatory sha256 scheme per RFC 8315).
     *
     * The keys "K" are calculated with the algorithm recommended by Section 4
     * of RFC 8315.  The same hash function as for <scheme> is used for HMAC in
     * the libcanlock implementation.  This ensures that the keys "K" are
     * always different for different <scheme> values, as required by Section 7
     * of RFC 8315.
     *   K1 = HMAC-SHA1(password, uid+mid)
     *   c-lock-string#1 = Base64(SHA-1(Base64(K1))
     *   K2 = HMAC-SHA256(password, uid+mid)
     *   c-lock-string#2 = Base64(SHA-256(Base64(K2))
     *
     * The input for the HMAC function does not depend on <scheme> and uses the
     * same secret. */
    if (secrets->canlockadmin->strings != NULL) {
        for (i = 0; i < secrets->canlockadmin->count; i++) {
            if (secrets->canlockadmin->strings[i] != NULL) {
                canlockseclen = strlen(secrets->canlockadmin->strings[i]);
                c_lock = cl_get_lock(
                    CL_SHA1,
                    (const unsigned char *) secrets->canlockadmin->strings[i],
                    canlockseclen, (const unsigned char *) msgidstart,
                    msgidlen);
                if (c_lock == NULL) {
                    free(canbuff);
                    return false;
                }

                if (**canbuff != '\0') {
                    strlcat(*canbuff, "\n\t", canbufflen);
                }
                strlcat(*canbuff, c_lock, canbufflen);
                free(c_lock);

                c_lock = cl_get_lock(
                    CL_SHA256,
                    (const unsigned char *) secrets->canlockadmin->strings[i],
                    canlockseclen, (const unsigned char *) msgidstart,
                    msgidlen);
                if (c_lock == NULL) {
                    free(canbuff);
                    return false;
                }

                strlcat(*canbuff, " ", canbufflen);
                strlcat(*canbuff, c_lock, canbufflen);
                free(c_lock);
            }
        }
    }

    /* If the user has an identity, generate specific user c-lock elements. */
    if (username != NULL && username[0] != '\0') {
        size_t usernamelen = strlen(username);

        datalock = concat(username, msgidstart, (char *) 0);

        if (secrets->canlockuser->strings != NULL) {
            for (i = 0; i < secrets->canlockuser->count; i++) {
                if (secrets->canlockuser->strings[i] != NULL) {
                    canlockseclen = strlen(secrets->canlockuser->strings[i]);
                    c_lock = cl_get_lock(CL_SHA1,
                                         (const unsigned char *)
                                             secrets->canlockuser->strings[i],
                                         canlockseclen,
                                         (const unsigned char *) datalock,
                                         usernamelen + msgidlen);
                    if (c_lock == NULL) {
                        free(canbuff);
                        free(datalock);
                        return false;
                    }

                    if (**canbuff != '\0') {
                        strlcat(*canbuff, "\n\t", canbufflen);
                    }
                    strlcat(*canbuff, c_lock, canbufflen);
                    free(c_lock);

                    c_lock = cl_get_lock(CL_SHA256,
                                         (const unsigned char *)
                                             secrets->canlockuser->strings[i],
                                         canlockseclen,
                                         (const unsigned char *) datalock,
                                         usernamelen + msgidlen);
                    if (c_lock == NULL) {
                        free(canbuff);
                        free(datalock);
                        return false;
                    }

                    strlcat(*canbuff, " ", canbufflen);
                    strlcat(*canbuff, c_lock, canbufflen);
                    free(c_lock);
                }
            }
        }
        free(datalock);
    }

    return true;
}


/*  Generate c-key elements for the Cancel-Key header field.
**  This function expects a Control header field body (possibly NULL),
**  a Supersedes header field body (possibly NULL), a username (possibly NULL)
**  and the address of a pointer where it will write c-key elements:
**    sha1:<28-byte Base64-encoded hash> sha256:<44-byte Base64-encoded hash>
**  for each admin secret in the canlockadmin vector if the user does not have
**  an identity (that is to say if username is NULL) or for each user secret
**  in the canlockuser vector if the user has an identity.
**
**  The length of each pair of c-key elements is: 4+1+28+1+6+1+44=85 bytes plus
**  "\n\t" (2 bytes) for the folding (without '\n') if another c-key pair is
**  written, and 1 byte for the final '\0'.
**
**  This function extracts the Message-ID from the Control header field body
**  or, if NULL, from the Supersedes header field body.  If not correctly
**  formatted, or both NULL, no Message-ID is found, which will result in
**  a failure.
**
**  The canbuff string will be allocated by this function, unless it returns
**  false.  This string is suitable to be used as the body of a Cancel-Key
**  header field.
**  The caller is reponsible for freeing it.
**
**  Returns true if all c-key elements were written, false on failure.
*/
bool
gen_cancel_key(const char *hdrcontrol, const char *hdrsupersedes,
               const char *username, char **canbuff)
{
    char *c_key, *datalock;
    const char *msgidstart = NULL;
    const char *p;
    size_t i;
    size_t msgidlen;
    size_t canlockseclen;
    size_t canbufflen =
        (85 + 2) * (secrets->canlockadmin->count + secrets->canlockuser->count)
        + 1; /* Computation explained above, with 2 extra bytes. */
    size_t usernamelen = 0;
    bool gencankey = false;

    *canbuff = xmalloc(canbufflen);
    **canbuff = '\0';

    /* Find the start of the Message-ID. */
    if (hdrcontrol != NULL) {
        msgidstart = hdrcontrol;
        for (; ISWHITE(*msgidstart); msgidstart++)
            ;
        if (strncasecmp(msgidstart, "cancel", 6) == 0
            && ISWHITE(msgidstart[6])) {
            msgidstart += 7;
            gencankey = true;
        }
    } else if (hdrsupersedes != NULL) {
        msgidstart = hdrsupersedes;
        gencankey = true;
    }

    if (gencankey) {
        /* Grab the message-ID to cancel or supersede without leading and
         * trailing spaces to obtain the right data for the computation of
         * c-key elements. */
        for (; ISWHITE(*msgidstart); msgidstart++)
            ;
        p = msgidstart;
        for (; !ISWHITE(*p) && *p != '\n' && *p != '\0'; p++)
            ;
        msgidlen = (p - msgidstart);

        if (username != NULL && username[0] != '\0') {
            usernamelen = strlen(username);
            datalock = concat(username, msgidstart, (char *) 0);

            /* Generate user c-key elements.  See the comments in
             * gen_cancel_lock() about the schemes used. */
            if (secrets->canlockuser->strings != NULL) {
                for (i = 0; i < secrets->canlockuser->count; i++) {
                    if (secrets->canlockuser->strings[i] != NULL) {
                        canlockseclen =
                            strlen(secrets->canlockuser->strings[i]);
                        c_key = cl_get_key(
                            CL_SHA1,
                            (const unsigned char *)
                                secrets->canlockuser->strings[i],
                            canlockseclen, (const unsigned char *) datalock,
                            usernamelen + msgidlen);
                        if (c_key == NULL) {
                            free(canbuff);
                            free(datalock);
                            return false;
                        }

                        if (**canbuff != '\0') {
                            strlcat(*canbuff, "\n\t", canbufflen);
                        }
                        strlcat(*canbuff, c_key, canbufflen);
                        free(c_key);

                        c_key = cl_get_key(
                            CL_SHA256,
                            (const unsigned char *)
                                secrets->canlockuser->strings[i],
                            canlockseclen, (const unsigned char *) datalock,
                            usernamelen + msgidlen);
                        if (c_key == NULL) {
                            free(canbuff);
                            free(datalock);
                            return false;
                        }

                        strlcat(*canbuff, " ", canbufflen);
                        strlcat(*canbuff, c_key, canbufflen);
                        free(c_key);
                    }
                }
            }
            free(datalock);
        } else {
            /* Admin cancel lock with no user name. */
            if (secrets->canlockadmin->strings != NULL) {
                for (i = 0; i < secrets->canlockadmin->count; i++) {
                    if (secrets->canlockadmin->strings[i] != NULL) {
                        canlockseclen =
                            strlen(secrets->canlockadmin->strings[i]);
                        c_key = cl_get_key(
                            CL_SHA1,
                            (const unsigned char *)
                                secrets->canlockadmin->strings[i],
                            canlockseclen, (const unsigned char *) msgidstart,
                            msgidlen);
                        if (c_key == NULL) {
                            free(canbuff);
                            return false;
                        }

                        if (**canbuff != '\0') {
                            strlcat(*canbuff, "\n\t", canbufflen);
                        }
                        strlcat(*canbuff, c_key, canbufflen);
                        free(c_key);

                        c_key = cl_get_key(
                            CL_SHA256,
                            (const unsigned char *)
                                secrets->canlockadmin->strings[i],
                            canlockseclen, (const unsigned char *) msgidstart,
                            msgidlen);
                        if (c_key == NULL) {
                            free(canbuff);
                            return false;
                        }

                        strlcat(*canbuff, " ", canbufflen);
                        strlcat(*canbuff, c_key, canbufflen);
                        free(c_key);
                    }
                }
            }
        }
    }

    return gencankey;
}


/*  Verify that a Cancel-Key header field body contains an element matching one
**  of those present in a Cancel-Lock header field body.
**  This function expects pointers to Cancel-Key and Cancel-Lock header field
**  bodies as arguments.
**
**  Returns true if at least one c-key element matches.
**  Otherwise, false is returned, that is to say the cancel or supersede
**  request cannot be authenticated.
*/
bool
verify_cancel_key(const char *c_key_header, const char *c_lock_header)
{
    char *c_key_list, *c_lock_list;
    int status = -1;

    if (c_key_header == NULL || c_lock_header == NULL)
        return false;

    /* Rewrite header field bodies as space-separated lists of elements. */
    c_key_list = spaced_words_without_cfws(c_key_header);
    c_lock_list = spaced_words_without_cfws(c_lock_header);

    if (c_key_list != NULL && c_lock_list != NULL) {
        /* Now call the function that actually does the check.
         * The status will be 0 if a match is found. */
        status = cl_verify_multi(NULL, c_key_list, c_lock_list);
    }

    free(c_key_list);
    free(c_lock_list);

    return (status == 0);
}

#else

/*  Define stub functions when Cancel-Lock is not available.
**  They all return false.
*/
bool
gen_cancel_lock(const char *msgid UNUSED, const char *username UNUSED,
                char **canbuff UNUSED)
{
    return false;
}

bool
gen_cancel_key(const char *hdrcontrol UNUSED, const char *hdrsupersedes UNUSED,
               const char *username UNUSED, char **canbuff UNUSED)
{
    return false;
}

bool
verify_cancel_key(const char *c_key_header UNUSED,
                  const char *c_lock_header UNUSED)
{
    return false;
}

#endif /* HAVE_CANLOCK */
