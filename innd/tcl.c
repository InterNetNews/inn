/*  $Id$
**
**  Support for TCL things
**
**  By Bob Heiney, Network Systems Laboratory, Digital Equipment Corporation
*/

#include "config.h"
#include "clibrary.h"

#include "innd.h"

#if     defined(DO_TCL)

Tcl_Interp       *TCLInterpreter;
bool             TCLFilterActive;
BUFFER           *TCLCurrArticle;
ARTDATA          *TCLCurrData;

static char      *TCLSTARTUP = NULL;
static char      *TCLFILTER = NULL;


void
TCLfilter(value)
    bool value;
{
    TCLFilterActive=value;

    syslog(L_NOTICE, "%s tcl filtering %s", LogName,
	   TCLFilterActive ? "enabled" : "disabled");
}


void
TCLreadfilter(void)
{
    int code;
    
    /* do before reload callback */
    code = Tcl_Eval(TCLInterpreter, "filter_before_reload");
    if (code != TCL_OK) {
	if (strcmp(TCLInterpreter->result,
		   "invalid command name: \"filter_before_reload\"")!=0)
	    syslog(L_ERROR, "%s Tcl filter_before_reload failed: %s",
		   LogName, TCLInterpreter->result);
    }

    /* read the filter file */
    if (TCLFILTER == NULL)
	TCLFILTER = concatpath(innconf->pathfilter, _PATH_TCL_FILTER);
    code = Tcl_EvalFile(TCLInterpreter, TCLFILTER);
    if (code != TCL_OK) {
	syslog(L_ERROR, "%s cant evaluate Tcl filter file: %s", LogName,
	       TCLInterpreter->result);
	TCLfilter(FALSE);
    }

    /* do the after callback, discarding any errors */
    code = Tcl_Eval(TCLInterpreter, "filter_after_reload");
    if (code != TCL_OK) {
	if (strcmp(TCLInterpreter->result,
		   "invalid command name: \"filter_after_reload\"")!=0)
	    syslog(L_ERROR, "%s Tcl filter_after_reload failed: %s",
		   LogName, TCLInterpreter->result);
    }
}


/* makeCheckSum
 *
 * Compute a checksum. This function does a one's-complement addition
 * of a series of 32-bit words. "buflen" is in bytes, not words. This is 
 * hard because the number of bits with which our machine can do arithmetic
 * is the same as the size of the checksum being created, but our hardware
 * is 2's-complement and C has no way to check for integer overflow.
 *
 * Note that the checksum is returned in network byte order and not host
 * byte order; this makes it suitable for putting into packets and for
 * comparing with what is found in packets.
 */

static uint32_t
makechecksum(u_char *sumbuf, int buflen)
{
    register u_char *buf = (u_char *)sumbuf;
    register int32_t len = buflen;
    register int32_t sum;
    uint32_t bwordl,bwordr,bword,suml,sumr;
    int rmdr;
    u_char tbuf[4];
    u_char *ptbuf;

    suml = 0; sumr = 0;

    len = len/4;
    rmdr = buflen - 4*len;

    while (len-- > 0) {
	bwordr = (buf[3] & 0xFF)
	    + ((buf[2] & 0xFF) << 8);
	bwordl = (buf[1] & 0xFF)
	    + ((buf[0] & 0xFF) << 8);
	bword = ( bwordl << 16) | bwordr;
	bword = ntohl(bword);
	bwordl = (bword >> 16) & 0xFFFF;
	bwordr = bword & 0xFFFF;
	sumr += bwordr;
	if (sumr & ~0xFFFF) {
	    sumr &= 0xFFFF;
	    suml++;
	}
	suml += bwordl;
	if (suml & ~0xFFFF) {
	    suml &= 0xFFFF;
	    sumr++;
	}
	buf += 4;
    }
    /* if buffer size was not an even multiple of 4 bytes,
       we have work to do */
    if (rmdr > 0) {
	tbuf[3] = 0; tbuf[2] = 0; tbuf[1] = 0;
	/* tbuf[0] will always be copied into, and tbuf[3] will
	 * always be zero (else this would not be a remainder)
	 */
	ptbuf = tbuf;
	while (rmdr--) *ptbuf++ = *buf++;
	bwordr = (tbuf[3] & 0xFF)
	    + ((tbuf[2] & 0xFF) << 8);
	bwordl = (tbuf[1] & 0xFF)
	    + ((tbuf[0] & 0xFF) << 8);
	bword = ( bwordl << 16) | bwordr;
	bword = ntohl(bword);
	bwordl = (bword >> 16) & 0xFFFF;
	bwordr = bword & 0xFFFF;
	sumr += bwordr;
	if (sumr & ~0xFFFF) {
	    sumr &= 0xFFFF;
	    suml++;
	}
	suml += bwordl;
	if (suml & ~0xFFFF) {
	    suml &= 0xFFFF;
	    sumr++;
	}
    }
    sum = htonl( (suml << 16) | sumr);
    return (~sum);
}


int
TCLCksumArt(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
{
    char buf[100];

    snprintf(buf, sizeof(buf), "%08x",
             makechecksum(TCLCurrData->Body,
                          &TCLCurrArticle->Data[TCLCurrArticle->Used] - 
                          TCLCurrData->Body));
    Tcl_SetResult(interp, buf, TCL_VOLATILE);
    return TCL_OK;
}


void
TCLsetup(void)
{
    int code;
    
    TCLInterpreter = Tcl_CreateInterp();
    if (TCLSTARTUP == NULL)
	TCLSTARTUP = concatpath(innconf->pathfilter, _PATH_TCL_STARTUP);
    code = Tcl_EvalFile(TCLInterpreter, TCLSTARTUP);
    if (code != TCL_OK) {
	syslog(L_FATAL, "%s cant read Tcl startup file: %s", LogName,
	       TCLInterpreter->result);
	exit(1);
    }

    Tcl_CreateCommand(TCLInterpreter, "checksum_article", TCLCksumArt,
		      NULL, NULL);

    TCLfilter(TRUE);
    TCLreadfilter();
}


void
TCLclose(void)
{
}


#endif /* defined(DO_TCL) */
