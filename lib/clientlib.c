/*  $Id$
**
**  Routines compatible with the NNTP "clientlib" routines.
*/
#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "libinn.h"
#include "nntp.h"


FILE	*ser_rd_fp = NULL;
FILE	*ser_wr_fp = NULL;
char	ser_line[NNTP_STRLEN + 2];


/*
**  Get the name of the NNTP server.  Ignore the filename; we use
**  our own configuration stuff.  Return pointer to static data.
*/
char *
getserverbyfile(char *file UNUSED)
{
    static char	buff[256];

    strlcpy(buff, innconf->server, sizeof(buff));
    return buff;
}


/*
**  Get a connection to the remote news server.  Return server's reply
**  code or -1 on error.
*/
int
server_init(char *host, int port)
{
    char	line2[NNTP_STRLEN];

    /* This interface may be used by clients that assume C News behavior and
       won't read inn.conf themselves. */
    if (innconf == NULL)
        if (!innconf_read(NULL))
            return -1;

    if (NNTPconnect(host, port, &ser_rd_fp, &ser_wr_fp, ser_line) < 0) {
	if (ser_line[0] == '\0')
	    /* I/O problem. */
	    return -1;

	/* Server rejected connection; return it's reply code. */
	return atoi(ser_line);
    }

    /* Send the INN command; if understood, use that reply. */
    put_server("mode reader");
    if (get_server(line2, (int)sizeof line2) < 0)
	return -1;
    if (atoi(line2) != NNTP_BAD_COMMAND_VAL)
	strlcpy(ser_line, line2, sizeof(ser_line));

    /* Connected; return server's reply code. */
    return atoi(ser_line);
}


#define CANTPOST	\
    "NOTE:  This machine does not have permission to post articles"
#define CANTUSE		\
	"This machine does not have permission to use the %s news server.\n"
/*
**  Print a message based on the the server's initial response.
**  Return -1 if server wants us to go away.
*/
int
handle_server_response(int response, char *host)
{
    char	*p;

    switch (response) {
    default:
	printf("Unknown response code %d from %s.\n", response, host);
	return -1;
     case NNTP_GOODBYE_VAL:
	if (atoi(ser_line) == response) {
	    p = &ser_line[strlen(ser_line) - 1];
	    if (*p == '\n' && *--p == '\r')
		*p = '\0';
	    if (p > &ser_line[3]) {
		printf("News server %s unavailable: %s\n", host,
			&ser_line[4]);
		return -1;
	    }
	}
	printf("News server %s unavailable, try later.\n", host);
	return -1;
    case NNTP_ACCESS_VAL:
	printf(CANTUSE, host);
	return -1;
    case NNTP_NOPOSTOK_VAL:
	printf("%s.\n", CANTPOST);
	/* FALLTHROUGH */
    case NNTP_POSTOK_VAL:
	break;
    }
    return 0;
}


/*
**  Send a line of text to the server.
*/
void
put_server(const char *buff)
{
    fprintf(ser_wr_fp, "%s\r\n", buff);
    fflush(ser_wr_fp);
}


/*
**  Get a line of text from the server, strip trailing \r\n.
**  Return -1 on error.
*/
int
get_server(char *buff, int buffsize)
{
    char	*p;

    if (fgets(buff, buffsize, ser_rd_fp) == NULL)
	return -1;
    p = &buff[strlen(buff)];
    if (p >= &buff[2] && p[-2] == '\r' && p[-1] == '\n')
	p[-2] = '\0';
    return 0;
}


/*
**  Send QUIT and close the server.
*/
void
close_server(void)
{
    char	buff[NNTP_STRLEN];

    if (ser_wr_fp != NULL && ser_rd_fp != NULL) {
	put_server("QUIT");
	fclose(ser_wr_fp);
	ser_wr_fp = NULL;

	get_server(buff, (int)sizeof buff);
	fclose(ser_rd_fp);
	ser_rd_fp = NULL;
    }
}
