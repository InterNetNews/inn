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
int sasl_ssf = 0, sasl_maxout = NNTP_STRLEN;

sasl_callback_t sasl_callbacks[] = {
    /* XXX do we want a proxy callback? */
    /* XXX add a getopt callback? */
    { SASL_CB_LIST_END, NULL, NULL }
};

#define BASE64_BUF_SIZE 21848	/* per RFC 2222bis: ((16K / 3) + 1) * 4  */

void
SASLauth(int ac, char *av[])
{
    const char *mech;
    const char *clientin = NULL;
    unsigned int clientinlen = 0;
    const char *serverout = NULL;
    unsigned int serveroutlen;
    char base64[BASE64_BUF_SIZE+1];
    const char *canon_user;
    const int *ssfp, *maxoutp;
    const void *property;
    int r = SASL_OK;

    if (ac < 3 || ac > 4) {
	Reply("%d AUTHINFO SASL <MECH> [<INIT-RESP]>\r\n", NNTP_BAD_COMMAND_VAL);
	return;
    }

    mech = av[2];
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
	Reply("%d %s\r\n", NNTP_AUTH_SASL_CHAL_VAL,
	      serveroutlen ? base64 : "=");
	fflush(stdout);

	/* get response from the client */
	r = line_read(&NNTPline, PERMaccessconf->clienttimeout,
		      &clientin, &clientinlen);
	switch (r) {
	case RTok:
	    if (clientinlen <= BASE64_BUF_SIZE) break;
	    /* FALLTHROUGH */
	case RTlong:
	    warn("%s response too long in authinfo sasl", ClientHost);
	    ExitWithStats(1, false);
	    break;
	case RTtimeout:
	    warn("%s timeout in authinfo sasl", ClientHost);
	    ExitWithStats(1, false);
	    break;
	case RTeof:
	    warn("%s eof in authinfo sasl", ClientHost);
	    ExitWithStats(1, false);
	    break;
	default:
	    warn("%s internal %d in authinfo sasl", ClientHost, r);
	    ExitWithStats(1, false);
	    break;
	}

	/* check if client cancelled */
	if (strcmp(clientin, "*") == 0) {
	    Reply("%d Client cancelled authentication\r\n", NNTP_AUTH_BAD_VAL);
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

	syslog(L_NOTICE, "%s user %s", ClientHost, PERMuser);

	if (serveroutlen)
	    Reply("%d %s\r\n", NNTP_AUTH_SASL_OK_VAL, base64);
	else
	    Reply("%d Authentication succeeded\r\n", NNTP_AUTH_OK_VAL);

	/* save info about the negotiated security layer for I/O functions */
	sasl_ssf = *ssfp;
	sasl_maxout =
	    (*maxoutp == 0 || *maxoutp > NNTP_STRLEN) ? NNTP_STRLEN : *maxoutp;
    }
    else {
	/* failure */
	int resp_code;
	const char *errstring = sasl_errstring(r, NULL, NULL);

	syslog(L_NOTICE, "%s bad_auth", ClientHost);

	switch (r) {
	case SASL_BADPROT:
	    resp_code = NNTP_AUTH_REJECT_VAL;
	    break;
	case SASL_NOMECH:
	case SASL_TOOWEAK:
	    resp_code = NNTP_SYNTAX_VAL;
	    break;
	case SASL_ENCRYPT:
	    resp_code = NNTP_STARTTLS_DONE_VAL;
	    break;
	default:
	    resp_code = NNTP_AUTH_BAD_VAL;
	    break;
	}

	Reply("%d %s\r\n",
	      resp_code, errstring ? errstring : "Authentication failed");
    }
}

#endif /* HAVE_SASL */
