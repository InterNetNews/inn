/*
** AUTHINFO SASL functionality.
**
** $Id$
*/

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "nnrpd.h"

/* Outside the ifdef so that make depend works even ifndef HAVE_SSL. */
#include "inn/ov.h"

#ifdef HAVE_SSL
extern int tls_cipher_usebits;
extern char *tls_peer_CN;
extern bool nnrpd_starttls_done;
#endif /* HAVE_SSL */

#ifdef HAVE_SASL

#include <sasl/sasl.h>
sasl_conn_t *sasl_conn = NULL;
int sasl_ssf = 0;
int sasl_maxout = NNTP_MAXLEN_COMMAND;

sasl_callback_t sasl_callbacks[] = {
    /* XXX Do we want a proxy callback? */
    /* XXX Add a getopt callback? */
    { SASL_CB_LIST_END, NULL, NULL }
};

#define BASE64_BUF_SIZE 21848	/* Per RFC 2222bis:  ((16K / 3) + 1) * 4. */


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


/*
**  Create a new SASL server authentication object.
*/
void
SASLnewserver(void)
{
    if (sasl_conn != NULL) {
        sasl_dispose(&sasl_conn);
        sasl_conn = NULL;
        sasl_ssf = 0;
        sasl_maxout = NNTP_MAXLEN_COMMAND;
    }
 
    if (sasl_server_new("nntp", NULL, NULL, NULL, NULL,
                        NULL, SASL_SUCCESS_DATA, &sasl_conn) != SASL_OK) {
        syslog(L_FATAL, "sasl_server_new() failed");
        Reply("%d SASL server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
        ExitWithStats(1, true);
    } else {
        /* XXX Fill in SASL_IPLOCALPORT and SASL_IPREMOTEPORT. */
        sasl_security_properties_t secprops;

        memset(&secprops, 0, sizeof(secprops));
        secprops.security_flags = SASL_SEC_NOANONYMOUS;
        secprops.max_ssf = 256;
        secprops.maxbufsize = NNTP_MAXLEN_COMMAND;
        sasl_setprop(sasl_conn, SASL_SEC_PROPS, &secprops);
#ifdef HAVE_SSL
        /* Tell SASL about the negotiated TLS layer. */
        if (nnrpd_starttls_done) {
            if (sasl_setprop(sasl_conn, SASL_SSF_EXTERNAL,
                             (sasl_ssf_t *) &tls_cipher_usebits) != SASL_OK) {
                syslog(L_NOTICE, "sasl_setprop() failed:  TLS layer for SASL");
            }
            if (sasl_setprop(sasl_conn, SASL_AUTH_EXTERNAL, tls_peer_CN)
                != SASL_OK) {
                syslog(L_NOTICE, "sasl_setprop() failed:  TLS layer for SASL");
            }
        }
#endif
    }
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
    int r1;
    bool base64error = false;

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

    /* 502 if authentication will fail. */
    if (!PERMcanauthenticate) {
        if (PERMauthorized && !PERMneedauth)
            Reply("%d Already authenticated\r\n", NNTP_ERR_ACCESS);
        else
            Reply("%d Authentication will fail\r\n", NNTP_ERR_ACCESS);
        return;
    }

#ifdef HAVE_SSL
    /* Check whether STARTTLS must be used before trying to authenticate. */
    if (PERMcanauthenticate && !PERMcanauthenticatewithoutSSL
        && !nnrpd_starttls_done) {
        Reply("%d Encryption required\r\n", NNTP_FAIL_PRIVACY_NEEDED);
        return;
    }
#endif

    if (ac == 4) {
	/* Initial response. */
	clientin = av[3];

	if (strcmp(clientin, "=") == 0) {
	    /* Zero-length initial response. */
	    clientin = "";
            clientinlen = 0;
	} else {
	    /* Decode the response.  On error, SASL_CONTINUE should not be
             * given.  Use SASL_BADPROT instead, in order to indicate
             * a base64-encoding error. */
            r1 = sasl_decode64(clientin, strlen(clientin),
                               base64, BASE64_BUF_SIZE, &clientinlen);
            clientin = base64;
            r = (r1 == SASL_CONTINUE ? SASL_BADPROT : r1);
            base64error = (r == SASL_BADPROT);
	}
    }

    if (r == SASL_OK) {
	/* Start the exchange. */
	r = sasl_server_start(sasl_conn, mech, clientin, clientinlen,
			      &serverout, &serveroutlen);
    }

    while (r == SASL_CONTINUE || (r == SASL_OK && serveroutlen != 0)) {
	if (serveroutlen != 0) {
	    /* Encode the server challenge. */
            r1 = sasl_encode64(serverout, serveroutlen,
                               base64, BASE64_BUF_SIZE, NULL);
            if (r1 != SASL_OK)
                r = r1;
	}

	/* Check for failure or success. */
        if (r != SASL_CONTINUE)
            break;

	/* Send the challenge to the client. */
	Reply("%d %s\r\n", NNTP_CONT_SASL,
	      serveroutlen != 0 ? base64 : "=");
	fflush(stdout);

	/* Get the response from the client. */
	r1 = line_read(&NNTPline, PERMaccessconf->clienttimeout,
		      &clientin, &clientinlen, NULL);
        
        switch (r1) {
	case RTok:
            if (clientinlen <= BASE64_BUF_SIZE)
                break;
	    /* FALLTHROUGH */
	case RTlong:
	    warn("%s response too long in AUTHINFO SASL", Client.host);
            Reply("%d Too long response\r\n", NNTP_FAIL_TERMINATING);
	    ExitWithStats(1, false);
	    break;
	case RTtimeout:
	    warn("%s timeout in AUTHINFO SASL", Client.host);
            /* No answer. */
	    ExitWithStats(1, false);
	    break;
	case RTeof:
	    warn("%s EOF in AUTHINFO SASL", Client.host);
            Reply("%d EOF\r\n", NNTP_FAIL_TERMINATING);
	    ExitWithStats(1, false);
	    break;
	default:
	    warn("%s internal %d in AUTHINFO SASL", Client.host, r);
            Reply("%d Internal error\r\n", NNTP_FAIL_TERMINATING);
	    ExitWithStats(1, false);
	    break;
	}

	/* Check if client cancelled. */
	if (strcmp(clientin, "*") == 0) {
            /* Restart the SASL server in order to be able to reauthenticate.
             * Call that function before the reply because in case of failure,
             * 400 is sent. */
            SASLnewserver();
	    Reply("%d Client cancelled authentication\r\n", NNTP_FAIL_AUTHINFO_BAD);
	    return;
	}

        if (strcmp(clientin, "=") == 0) {
            /* Zero-length answer. */
            clientin = "";
            clientinlen = 0;
        } else {
            /* Decode the response.  On error, SASL_CONTINUE should not be
             * given.  Use SASL_BADPROT instead, in order to indicate
             * a base64-encoding error. */
            r1 = sasl_decode64(clientin, clientinlen,
                               base64, BASE64_BUF_SIZE, &clientinlen);
            clientin = base64;
            r = (r1 == SASL_CONTINUE ? SASL_BADPROT : r1);
            base64error = (r == SASL_BADPROT);
        }

        /* Do the next step. */
	if (r == SASL_OK) {
	    r = sasl_server_step(sasl_conn, clientin, clientinlen,
				 &serverout, &serveroutlen);
	}
    }

    /* Fetch the username (authorization ID). */
    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_USERNAME, &property);
        canon_user = property;
    }

    /* Grab info about the negotiated layer. */
    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_SSF, &property);
        ssfp = property;
    }

    if (r == SASL_OK) {
	r = sasl_getprop(sasl_conn, SASL_MAXOUTBUF, &property);
        maxoutp = property;
    }

    if (r == SASL_OK) {
	/* Success. */
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

	/* Save info about the negotiated security layer for I/O functions. */
	sasl_ssf = *ssfp;
	sasl_maxout =
	    (*maxoutp == 0 || *maxoutp > NNTP_MAXLEN_COMMAND) ? NNTP_MAXLEN_COMMAND : *maxoutp;

        /* Close out any existing article, report group stats.
         * RFC 4643 requires the reset of any knowledge about the client. */
        if (GRPcur) {
            bool boolval;
            ARTclose();
            GRPreport();
            OVctl(OVCACHEFREE, &boolval);
            free(GRPcur);
            GRPcur = NULL;
            if (ARTcount)
                syslog(L_NOTICE, "%s exit for AUTHINFO SASL articles %ld groups %ld",
                       Client.host, ARTcount, GRPcount);
            GRPcount = 0;
            PERMgroupmadeinvalid = false;
        }
    } else {
	/* Failure. */
	int resp_code;
	const char *errstring = sasl_errstring(r, NULL, NULL);

	syslog(L_NOTICE, "%s bad_auth", Client.host);

	switch (r) {
	case SASL_BADPROT:
            resp_code = (base64error ? NNTP_ERR_BASE64 : NNTP_FAIL_AUTHINFO_REJECT);
            break;
        case SASL_BADPARAM:
        case SASL_NOTDONE:
	    resp_code = NNTP_FAIL_AUTHINFO_REJECT;
	    break;
	case SASL_NOMECH:
            resp_code = NNTP_ERR_UNAVAILABLE;
            break;
	case SASL_ENCRYPT:
	    resp_code = NNTP_FAIL_PRIVACY_NEEDED;
	    break;
	default:
	    resp_code = NNTP_FAIL_AUTHINFO_BAD;
	    break;
	}

        /* Restart the SASL server in order to be able to reauthenticate.
         * Call that function before the reply because in case of failure,
         * 400 is sent. */
        SASLnewserver();
	Reply("%d %s\r\n",
	      resp_code, errstring ? errstring : "Authentication failed");
    }
}

#endif /* HAVE_SASL */
