#include	<stdio.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#include	<netdb.h>
#include	<string.h>





int create_udp_socket(int port)
{
	int s;
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	if (make_udp_sockaddr(&sin, "localhost") < 0) {
		return(-1);
	}
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("create_udp_socket: socket");
		return(-1);
	}
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		/* perror("create_udp_socket: bind"); */
		return(-1);
	}
	return(s);
}





int make_udp_sockaddr(struct sockaddr_in *addr, char *ascii)
{
        struct hostent *host;
        int dots = 0;
        int numeric = 0;
        char *str = strdup(ascii);
        char *colon = strrchr(str, ':');
        char *lastdot = strrchr(str, '.');
        char *ptr;

        if (! str) {
                perror("make_udp_sockaddr: malloc");
		return(-1);
        }

        addr->sin_family = AF_INET;

        /* Count the number of dots in the address. */
        for (ptr = str; *ptr; ptr++) {
                if (*ptr == '.') {
                        dots++;
                }
        }

        /* Check if it seems to be numeric. */
        numeric = isdigit(*str);

        /* If numeric and four dots, we have a.b.c.d.6000 */
        if (numeric && dots == 4) {
                *lastdot = '\0';
                addr->sin_port = htons(atoi(lastdot + 1));
        }
        /* If nonnumeric, check if the last part is a port */
        if (! numeric && lastdot && isdigit(*(lastdot + 1))) {
                *lastdot = '\0';
                addr->sin_port = htons(atoi(lastdot + 1));
        }
        /* Now do we have a numeric address */
        if (numeric) {
                addr->sin_addr.s_addr = inet_addr(str);
                free(str);
                return(0);
        }
        /* Or a name */
        if (! (host = gethostbyname(str))) {
                free(str);
                perror("make_udp_sockaddr: gethostbyname");
		return(-1);
        }
        mybcopy(host->h_addr_list[0], (char *)&addr->sin_addr.s_addr, sizeof(addr->sin_addr.s_addr));
        free(str);
        return(0);
}





int connect_udp_socket(int s, char *address, int port)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	if (make_udp_sockaddr(&sin, address) < 0) {
		return(-1);
	}
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("connect_udp_socket: connect");
		return(-1);
	}
	return(0);
}





int write_udp(int s, char *buf, int len)
{
	int rval;

	if ((rval = send(s, buf, len, 0x0)) < 0) {
		perror("write_udp: send");
		return(-1);
	}
	return(rval);
}





int read_udp(int s, char *buf, int len)
{
	int rval;

	if ((rval = recv(s, buf, len, 0x0)) < 0) {
		/* perror("read_udp: recv"); */
		return(-1);
	}
	return(rval);
}

int
mybcopy (src, dest, len)
  register char *src, *dest;
  int len;
{
  if (dest < src)
    while (len--)
      *dest++ = *src++;
  else
    {
      char *lasts = src + (len-1);
      char *lastd = dest + (len-1);
      while (len--)
        *(char *)lastd-- = *(char *)lasts--;
    }
}
