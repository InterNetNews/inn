/*
**
**  Common headers for authenticators and resolvers.
**
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"


/*********************** Authenticators ************************/

#define NAMESTR "ClientAuthname: "
#define PASSSTR "ClientPassword: "

/* 
 * Takes in two buffers for the results and reads username and 
 * password as passed from nnrpd via stdin.  Exit values:
 * 0 - got nonnull username and password sucessfully
 * 3 - one of the inputs from nnrpd was an empty string
 *     (in this case, the result is usable if desired, just
 *      be aware that one of the strings starts with \0)
 */
extern int
get_auth(char* uname, char* pass);



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
