/*
**
**  Common headers for authenticators and resolvers.
**
*/

#include "config.h"
#include "portable/socket.h"


/*********************** Authenticators ************************/

/* Holds the authentication information from nnrpd. */
struct authinfo {
    char *username;
    char *password;
};

/* Reads authentication information from nnrpd and returns a new authinfo
   struct, or returns NULL on failure.  Note that both the username and the
   password may be empty.  The client is responsible for freeing the authinfo
   struct. */
extern struct authinfo *get_auth(void);



/*********************** Resolvers ************************/

#define IPNAME "ClientIP: "
#define PORTNAME "ClientPort: "
#define LOCIP "LocalIP: "
#define LOCPORT "LocalPort: "

#define GOTCLIADDR 0x1
#define GOTCLIPORT 0x2
#define GOTLOCADDR 0x4
#define GOTLOCPORT 0x8
#define GOT_ALL (GOTCLIADDR | GOTCLIPORT | GOTLOCADDR | GOTLOCPORT)


/*
 * Parses the client and local IP and ports from stdin and stores them
 * into the argument structs.  Returns the OR of the GOT... constants
 * defined above for those fields which were found.  Returns GOT_ALL
 * if all fields wre found.
 */
extern char
get_res(struct sockaddr* loc,
	struct sockaddr* cli);
