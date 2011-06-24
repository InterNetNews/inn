/*  $Id$
**
**  The public interface to the Endpoint class.
**
**  Written by James Brister <brister@vix.com>
**
**  The EndPoint objects are encapsulations of file descriptors that normally
**  do blocking I/O (i.e. NOT fd's hooked to a disk file).  The EndPoint class
**  provides methods for requesting read/writes to happen when next possible
**  and for the requestor to be notified when the I/O is complete (or failed
**  for some reason).  Facilities for timeout notifications are provided too.
**
**  We should add a way to cancel prepared read/writes.
*/

#if ! defined ( endpoint_h__ )
#define endpoint_h__

#include "misc.h"


#define clearTimer(timerId) \
	do {\
		if((timerId)!=0) { \
			removeTimeout(timerId); \
			timerId=0; \
		} \
	}while(0)



/* These typedefs really live in misc.h.
 *
 *****************************************
 *             
 * The basic (opaque to the outside world) type:
 *              
 *      typedef struct endpoint_s *EndPoint ;
 *
 *****************************************
 *
 * The return status of an I/O request:
 *
 *      typedef enum {
 *          IoDone,                     I/O completed successfully 
 *          IoIncomplete,               I/O still got more to read/write
 *          IoProgress,                 I/O still got more to read/write
 *          IoFailed                    I/O failed
 *      } IoStatus ;
 *
 * The completion callbacks are never called with the status IoIncomplete or
 * IoProgress.
 *
 *****************************************
 *
 * typedef for function callback when I/O is complete (or failed).
 *      E is the EndPoint
 *      I is the status of the IO
 *      B is the buffer the IO was to read to or write from
 *      D is the client data originally given to prepare{Write,Read}
 *
 * typedef void (*EndpRWCB) (EndPoint e, IoStatus i, Buffer b, void *d) ;
 *
 *****************************************
 *
 * typedef for function callback when a timer has gone off.  D is the client
 * data given to prepare{Sleep,Wake}:
 *
 * typedef void (*EndpTCB) (void *d) ;
 *
 */

/* create a new EndPoint hooked up to the given file descriptor */
EndPoint newEndPoint (int fd) ;

/* shutdown the file descriptor and delete the endpoint. */
void delEndPoint (EndPoint ep);

/* return the file descriptor the endpoint is managing */
int endPointFd (EndPoint endp) ;

/* Request a read when available. Reads MINLEN bytes into the
 * buffers in BUFFERS. BUFFERS is an array of Buffers, the last of which
 * must be NULL. Note that ownership of BUFFERS is never asserted, but
 * the ownership of the Buffers in it is. So if an EndPoint is
 * deleted while a read is pending the Buffers will be released, but
 * the array won't be. If MINLEN is 0 then the buffers must be
 * filled. The FUNC function gets called when the read is
 * complete. CLIENTDATA is simply passed back to the
 * callback. Returns non-zero if can be scheduled for processing.
 */
int prepareRead (EndPoint endp,
                 Buffer *buffers,
                 EndpRWCB func,
                 void *clientData,
                 int minlen) ;

/* Request a write when possible. All the data in the buffers in
 * BUFFERS will be written out the endpoint. BUFFERS is a NULL
 * terminated array of Buffers. See prepareRead for a discussion on
 * the ownership of BUFFERS and the Buffers inside BUFFERS. The PROGRESS
 * callback function will be called and the CLIENTDATA value will be
 * passed through to it whenever any data is written except for the
 * final write.  The DONE callback function will be called and the
 * CLIENTDATA value will be passed through to it after the final write.
 * Returns non-zero if scheduled succesfully.
 */
int prepareWrite (EndPoint endp,
                  Buffer *buffers,
                  EndpRWCB progress,
                  EndpRWCB done,
                  void *clientData) ;

/* cancel any outstanding reads. */
void cancelRead (EndPoint endp) ;

/* cancel any outstanding writes. */
void cancelWrite (EndPoint endp, char *buffer, size_t *len) ;

/* return true if prepareRead has been called, but not serviced yet */
bool readIsPending (EndPoint endp) ;

/* Request a wakeup at a given time. */
TimeoutId prepareWake (EndpTCB func,
                       time_t timeToWake,
                       void *clientData) ;

/* return true if prepareWrite has been called, but not serviced yet */
bool writeIsPending (EndPoint endp) ;

/* Requests a wakeup TIMETOSLEEP seconds from now. */
TimeoutId prepareSleep (EndpTCB func,
                        int timeToSleep,
                        void *clientData) ;

/* Updates tid to wakeup TIMETOSLEEP seconds from now. */
TimeoutId updateSleep (TimeoutId tid,
                       EndpTCB func,
                       int timeToSleep,
                       void *clientData) ;

  /* Set up a function to be called whenever the endpoint's file descriptor
     is NOT ready. This is called after all other fd-ready endpoints have
     been serviced. */
void *addWorkCallback (EndPoint endp, EndpWorkCbk cbk, void *data) ;

void setSigHandler (int signum, void (*ptr)(int));

/* remove the timeout that was previously requested. Returns true if
   succesfully removed, false otherwise. 0 is a legal parameter value, in
   which case the function simply returns. */
bool removeTimeout (TimeoutId tid) ;

/* start the select loop. An initial prepare(Read|Write) or a timeout
   better have been setup. Doesn't return unless stopRun called */
void Run (void) ;

/* stops the Run loop and makes Run() return */
void stopRun (void) ;

/* returns the errno the endpoint got. */
int endPointErrno (EndPoint endp) ;

/* Tell the EndPoint class that the given EndPoint should always be
   considered first for servicing (i.e. the EndPoint connectied to innd) */
void setMainEndPoint (EndPoint endp) ;

/* returns the fd of the main endpoint */
int getMainEndPointFd (void) ;

void freeTimeoutQueue (void) ;

int endpointConfigLoadCbk (void *data) ;


/*
 * kre's cool idea for reducing the number of times time() is called.
 */
extern time_t  PrivateTime;

#define theTime()       (PrivateTime ? PrivateTime : time(&PrivateTime))
#define timePasses()    (PrivateTime = 0)

#endif /* endpoint_h__ */
