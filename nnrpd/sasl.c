/*
 * AUTHINFO SASL functionality
 *
 * $Id$
 */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "nnrpd.h"

#ifdef HAVE_SASL

#include <sasl/sasl.h>
sasl_conn_t *sasl_conn = NULL;
int sasl_ssf = 0, sasl_maxout = NNTP_MAXLEN_COMMAND;

sasl_callback_t sasl_callbacks[] = {
    /* XXX do we want a proxy callback? */
    /* XXX add a getopt callback? */
    { SASL_CB_LIST_END, NULL, NULL }
};

#define BASE64_BUF_SIZE 21848	/* per RFC 2222bis: ((16K / 3) + 1) * 4  */


/*
**  Check if the argument is a valid mechanism according to RFC 4643:
**
**    mechanism = 1*20mech-char
**    mech-char = UPPER / DIGIT / "-" / "_"
*/
static bool
IsValidMechanism(const char *string)
{
    int len = 0;
    const unsigned char *p;
 
    /* Not NULL. */
    if (string == NULL)
        return false;

    p = (const unsigned char *) string;

    for (; *p != '\0'; p++) {
        len++;
        if (!CTYPE(isalnum, *p) && *p != '-' && *p != '_')
            return false;
    }

    if (len > 0 && len < 21)
        return true;
    else
        return false;
}


void
SASLauth(int ac, char *av[])
{
    const char *mech;
    const char *clientin = NULL;
    unsigned int clientinlen = 0;
    const char *serverout = NULL;
    unsigned int serveroutlen;
    char base64[BASE64_BUF_SIZE+1];
    const char *canon_user = NULL;
    const int *ssfp = NULL;
    const int *maxoutp;
    const void *property;
    int r = SASL_OK;

    if (ac < 3 || ac > 4) {
        /* In fact, ac > 4 here. */
        Reply("%d Too many arguments\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    mech = av[2];

    if (!IsValidMechanism(mech)) {
        Reply("%d Syntax error in mechanism\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    if (ac == 4) {
	/* initial response */
	clientin = av[3];

	if (strcmp(clientin, "=") == 0) {
	    /* zero-length initial response */
	    clientin = "";
	} else {
	    /* decode the response */
	    r = sasl_decode64(clientin, strlen(clientin),
			      base64, BASE64_BUF_SIZE, &clientinlen);
	    clientin = base64;
	}
    }

    if (r == SASL_OK) {
	/* start the exchange */
	r = sasl_server_start(sasl_conn, mech, clientin, clientinlen,
			      &serverout, &serveroutlen);
    }

    while (r == SASL_CONTINUE || (r == SASL_OK && serveroutlen)) {
	if (serveroutlen) {
	    /* encode the server challenge */
	    int r1 = sasl_encode64(serverout, serveroutlen,
				   base64, BASE64_BUF_SIZE, NULL);
	    if (r1 != SASL_OK) r = r1;
	}

	/* check for failure or success */
	if (r != SASL_CONTINUE) break;
	    
	/* send the challenge to the client */
	Reply("%d %s\r\n", NNTP_CONT_SASL,
	      serveroutlen ? base64 : "=");
	fflush(stdout);

	/* get response from the client */
	r = line_read(&NNTPline, PERMaccessconf->clienttimeout,
		      &clientin, &clientinlen, NULL);
	switch (r) {
	case RTok:
	    if (clientinlen <= BASE64_BUF_SIZE) break;
	    /* FALLTHROUGH */
	case RTlong:
	    warn("%s response too long in authinfo sasl", Client.host);
	    ExitWithStats(1, false);
	    break;
	case RTtimeout:
	    warn("%s timeout in authinfo sasl", Client.host);
	    ExitWithStats(1, false);
	    break;
	case RTeof:
	    warn("%s EOF in authinfo sasl", Client.host);
	    ExitWithStats(1, false);
	    break;
	default:
	    warn("%s internal %d in authinfo sasl", Client.host, r);
	    ExitWithStats(1, false);
	    break;
	}

	/* check if client cancelled */
	if (strcmp(clientin, "*") == 0) {
	    Reply("%d Client cancelled authentication\r\n", NNTP_FAIL_AUTHINFO_BAD);
	    return;
	}

	/* decode the response */
	r = sasl_decode64(clientin, clientinlen,
			  base64, BASE64_BUF_SIZE, &clientinlen);
	clientin = base64;

	/* do the next step */
	if (r == SASL_OK) {
	    r = sasl_server_step(sasl_conn, clientin, clientinlen,
				 &serverout, &serveroutlen);
	}
    }

    /* fetch the username (authorization id) */
    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_USERNAME, &property);
        canon_user = property;
    }

    /* grab info about the negotiated layer */
    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_SSF, &property);
        ssfp = property;
    }

    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_MAXOUTBUF, &property);
        maxoutp = property;
    }

    if (r == SASL_OK) {
	/* success */
	strlcpy(PERMuser, canon_user, sizeof(PERMuser));
        PERMgetpermissions();
	PERMneedauth = false;
	PERMauthorized = true;
        PERMcanauthenticate = false;

	syslog(L_NOTICE, "%s user %s", Client.host, PERMuser);

	if (serveroutlen)
	    Reply("%d %s\r\n", NNTP_OK_SASL, base64);
	else
	    Reply("%d Authentication succeeded\r\n", NNTP_OK_AUTHINFO);

	/* save info about the negotiated security layer for I/O functions */
	sasl_ssf = *ssfp;
	sasl_maxout =
	    (*maxoutp == 0 || *maxoutp > NNTP_MAXLEN_COMMAND) ? NNTP_MAXLEN_COMMAND : *maxoutp;
    }
    else {
	/* failure */
	int resp_code;
	const char *errstring = sasl_errstring(r, NULL, NULL);

	syslog(L_NOTICE, "%s bad_auth", Client.host);

	switch (r) {
	case SASL_BADPROT:
	    resp_code = NNTP_FAIL_AUTHINFO_REJECT;
	    break;
	case SASL_NOMECH:
	case SASL_TOOWEAK:
	    resp_code = NNTP_ERR_SYNTAX;
	    break;
	case SASL_ENCRYPT:
	    resp_code = NNTP_FAIL_STARTTLS;
	    break;
	default:
	    resp_code = NNTP_FAIL_AUTHINFO_BAD;
	    break;
	}

	Reply("%d %s\r\n",
	      resp_code, errstring ? errstring : "Authentication failed");
    }
}

#endif /* HAVE_SASL */
