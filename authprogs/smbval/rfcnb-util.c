/* UNIX RFCNB (RFC1001/RFC1002) NetBIOS implementation

   Version 1.0
   RFCNB Utility Routines ...

   Copyright (C) Richard Sharpe 1996

*/

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>

#include "rfcnb-priv.h"
#include "rfcnb-util.h"
#include "rfcnb-io.h"

#ifndef INADDR_NONE
# define INADDR_NONE -1
#endif

extern void (*Prot_Print_Routine)(); /* Pointer to protocol print routine */

/* Convert name and pad to 16 chars as needed */
/* Name 1 is a C string with null termination, name 2 may not be */
/* If SysName is true, then put a <00> on end, else space>       */

void RFCNB_CvtPad_Name(char *name1, char *name2)

{ char c, c1, c2;
  int i, len;

  len = strlen(name1);

  for (i = 0; i < 16; i++) {

    if (i >= len) {

     c1 = 'C'; c2 = 'A'; /* CA is a space */
 
    } else {

      c = name1[i];
      c1 = (char)((int)c/16 + (int)'A');
      c2 = (char)((int)c%16 + (int)'A');
    }

    name2[i*2] = c1;
    name2[i*2+1] = c2;

  }

  name2[32] = 0;   /* Put in the nll ...*/

}

/* Get a packet of size n */

struct RFCNB_Pkt *RFCNB_Alloc_Pkt(int n)

{ RFCNB_Pkt *pkt;

  if ((pkt = (struct RFCNB_Pkt *)malloc(sizeof(struct RFCNB_Pkt))) == NULL) {

    RFCNB_errno = RFCNBE_NoSpace;
    RFCNB_saved_errno = errno;
    return(NULL);

  }

  pkt -> next = NULL;
  pkt -> len = n;

  if (n == 0) return(pkt);

  if ((pkt -> data = (char *)malloc(n)) == NULL) {

    RFCNB_errno = RFCNBE_NoSpace;
    RFCNB_saved_errno = errno;
    free(pkt);
    return(NULL);

  }

  return(pkt);

}

/* Free up a packet */

int RFCNB_Free_Pkt(struct RFCNB_Pkt *pkt)

{ struct RFCNB_Pkt *pkt_next; char *data_ptr;

  while (pkt != NULL) {

    pkt_next = pkt -> next;

    data_ptr = pkt -> data;

    if (data_ptr != NULL)
      free(data_ptr);

    free(pkt);

    pkt = pkt_next;

  }

}

/* Resolve a name into an address */

int RFCNB_Name_To_IP(char *host, struct in_addr *Dest_IP)

{ int addr;         /* Assumes IP4, 32 bit network addresses */
  struct hostent *hp;

        /* Use inet_addr to try to convert the address */

  if ((addr = inet_addr(host)) == INADDR_NONE) { /* Oh well, a good try :-) */

        /* Now try a name look up with gethostbyname */

    if ((hp = gethostbyname(host)) == NULL) { /* Not in DNS */

        /* Try NetBIOS name lookup, how the hell do we do that? */

      RFCNB_errno = RFCNBE_BadName;   /* Is this right? */
      RFCNB_saved_errno = errno;
      return(RFCNBE_Bad);

    }
    else {  /* We got a name */

       memcpy((void *)Dest_IP, (void *)hp -> h_addr_list[0], sizeof(struct in_addr));

    }
  }
  else { /* It was an IP address */

    memcpy((void *)Dest_IP, (void *)&addr, sizeof(struct in_addr));

  }

  return 0;

}

/* Disconnect the TCP connection to the server */

int RFCNB_Close(int socket)

{

  close(socket);

  /* If we want to do error recovery, here is where we put it */

  return 0;

}

/* Connect to the server specified in the IP address.
   Not sure how to handle socket options etc.         */

int RFCNB_IP_Connect(struct in_addr Dest_IP, int port)

{ struct sockaddr_in Socket;
  int fd;

  /* Create a socket */

  if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) { /* Handle the error */

    RFCNB_errno = RFCNBE_BadSocket;
    RFCNB_saved_errno = errno;
    return(RFCNBE_Bad);
    } 

  bzero((char *)&Socket, sizeof(Socket));
  memcpy((char *)&Socket.sin_addr, (char *)&Dest_IP, sizeof(Dest_IP));

  Socket.sin_port = htons(port);
  Socket.sin_family = PF_INET;

  /* Now connect to the destination */

  if (connect(fd, (struct sockaddr *)&Socket, sizeof(Socket)) < 0) { /* Error */

    close(fd);
    RFCNB_errno = RFCNBE_ConnectFailed;
    RFCNB_saved_errno = errno;
    return(RFCNBE_Bad);
    }

  return(fd);

}

/* handle the details of establishing the RFCNB session with remote 
   end 

*/

int RFCNB_Session_Req(struct RFCNB_Con *con, 
		      char *Called_Name, 
		      char *Calling_Name,
		      bool *redirect,
		      struct in_addr *Dest_IP,
		      int * port)

{ char *sess_pkt;

  /* Response packet should be no more than 9 bytes, make 16 jic */

  char ln1[16], ln2[16], n1[32], n2[32], resp[16];
  int len;
  struct RFCNB_Pkt *pkt, res_pkt;

  /* We build and send the session request, then read the response */

  pkt = RFCNB_Alloc_Pkt(RFCNB_Pkt_Sess_Len);

  if (pkt == NULL) {

    return(RFCNBE_Bad);  /* Leave the error that RFCNB_Alloc_Pkt gives) */

  }

  sess_pkt = pkt -> data;    /* Get pointer to packet proper */

  sess_pkt[RFCNB_Pkt_Type_Offset]  = RFCNB_SESSION_REQUEST;
  RFCNB_Put_Pkt_Len(sess_pkt, RFCNB_Pkt_Sess_Len-RFCNB_Pkt_Hdr_Len);
  sess_pkt[RFCNB_Pkt_N1Len_Offset] = 32;
  sess_pkt[RFCNB_Pkt_N2Len_Offset] = 32;

  RFCNB_CvtPad_Name(Called_Name, (sess_pkt + RFCNB_Pkt_Called_Offset));
  RFCNB_CvtPad_Name(Calling_Name, (sess_pkt + RFCNB_Pkt_Calling_Offset));

  /* Now send the packet */

  if ((len = RFCNB_Put_Pkt(con, pkt, RFCNB_Pkt_Sess_Len)) < 0) {

    return(RFCNBE_Bad);       /* Should be able to write that lot ... */

    }

  res_pkt.data = resp;
  res_pkt.len  = sizeof(resp);
  res_pkt.next = NULL;

  if ((len = RFCNB_Get_Pkt(con, &res_pkt, sizeof(resp))) < 0) {

    return(RFCNBE_Bad);

  }

  /* Now analyze the packet ... */

  switch (RFCNB_Pkt_Type(resp)) {

    case RFCNB_SESSION_REJ:         /* Didnt like us ... too bad */

      /* Why did we get rejected ? */
    
      switch (CVAL(resp,RFCNB_Pkt_Error_Offset)) {

      case 0x80: 
	RFCNB_errno = RFCNBE_CallRejNLOCN;
	break;
      case 0x81:
	RFCNB_errno = RFCNBE_CallRejNLFCN;
	break;
      case 0x82:
	RFCNB_errno = RFCNBE_CallRejCNNP;
	break;
      case 0x83:
	RFCNB_errno = RFCNBE_CallRejInfRes;
	break;
      case 0x8F:
	RFCNB_errno = RFCNBE_CallRejUnSpec;
	break;
      default:
	RFCNB_errno = RFCNBE_ProtErr;
	break;
      }

      return(RFCNBE_Bad);
      break;

    case RFCNB_SESSION_ACK:        /* Got what we wanted ...      */

      return(0);
      break;

    case RFCNB_SESSION_RETARGET:   /* Go elsewhere                */

      *redirect = TRUE;       /* Copy port and ip addr       */

      memcpy(Dest_IP, (resp + RFCNB_Pkt_IP_Offset), sizeof(struct in_addr));
      *port = SVAL(resp, RFCNB_Pkt_Port_Offset);

      return(0);
      break;

    default:  /* A protocol error */

      RFCNB_errno = RFCNBE_ProtErr;
      return(RFCNBE_Bad);
      break;
    }
}
