/* UNIX SMBlib NetBIOS implementation

   Version 1.0
   SMBlib Utility Routines

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

#include "smblib-priv.h"
#include "smblib.h"
#include "rfcnb.h"

/* The following two arrays need to be in step!              */
/* We must make it possible for callers to specify these ... */

const char *SMB_Prots[] = {"PC NETWORK PROGRAM 1.0", 
                           "MICROSOFT NETWORKS 1.03",
                           "MICROSOFT NETWORKS 3.0",
                           "DOS LANMAN1.0",
                           "LANMAN1.0",
                           "DOS LM1.2X002",
                           "LM1.2X002",
                           "DOS LANMAN2.1",
                           "LANMAN2.1",
                           "Samba",
                           "NT LM 0.12",
                           "NT LANMAN 1.0",
                           NULL};

int SMB_Types[] = {SMB_P_Core,
                   SMB_P_CorePlus,
                   SMB_P_DOSLanMan1,
                   SMB_P_DOSLanMan1,
                   SMB_P_LanMan1,
                   SMB_P_DOSLanMan2,
                   SMB_P_LanMan2,
                   SMB_P_LanMan2_1,
                   SMB_P_LanMan2_1,
                   SMB_P_NT1,
                   SMB_P_NT1,
                   SMB_P_NT1,
                   -1};

/* Figure out what protocol was accepted, given the list of dialect strings */
/* We offered, and the index back from the server. We allow for a user      */
/* supplied list, and assume that it is a subset of our list                */

static int SMB_Figure_Protocol(const char *dialects[], int prot_index)

{ int i;

  if (dialects == SMB_Prots) { /* The jobs is easy, just index into table */

    return(SMB_Types[prot_index]);
  }
  else { /* Search through SMB_Prots looking for a match */

    for (i = 0; SMB_Prots[i] != NULL; i++) {

      if (strcmp(dialects[prot_index], SMB_Prots[i]) == 0) { /* A match */

	return(SMB_Types[i]);

      }

    }

    /* If we got here, then we are in trouble, because the protocol was not */
    /* One we understand ...                                                */

    return(SMB_P_Unknown);

  }

}


/* Negotiate the protocol we will use from the list passed in Prots       */
/* we return the index of the accepted protocol in NegProt, -1 indicates  */
/* none acceptible, and our return value is 0 if ok, <0 if problems       */

int 
SMB_Negotiate(void *handle, const char *Prots[])
{
  SMB_Handle_Type Con_Handle = handle;
  struct RFCNB_Pkt *pkt;
  int prots_len, i, pkt_len, prot, alloc_len;
  char *p;

  /* Figure out how long the prot list will be and allocate space for it */

  prots_len = 0;

  for (i = 0; Prots[i] != NULL; i++) {

    prots_len = prots_len + strlen(Prots[i]) + 2; /* Account for null etc */

  }

  /* The -1 accounts for the one byte smb_buf we have because some systems */
  /* don't like char msg_buf[]                                             */

  pkt_len = SMB_negp_len + prots_len;

  /* Make sure that the pkt len is long enough for the max response ...   */
  /* Which is a problem, because the encryption key len eec may be long   */

  if (pkt_len < (SMB_hdr_wct_offset + (19 * 2) + 40)) {

    alloc_len = SMB_hdr_wct_offset + (19 * 2) + 40;

  }
  else {

    alloc_len = pkt_len;

  }

  pkt = (struct RFCNB_Pkt *)RFCNB_Alloc_Pkt(alloc_len);

  if (pkt == NULL) {

    SMBlib_errno = SMBlibE_NoSpace;
    return(SMBlibE_BAD);

  }

  /* Now plug in the bits we need */

  memset(SMB_Hdr(pkt), 0, SMB_negp_len);
  SIVAL(SMB_Hdr(pkt), SMB_hdr_idf_offset, SMB_DEF_IDF);  /* Plunk in IDF */
  *(SMB_Hdr(pkt) + SMB_hdr_com_offset) = SMBnegprot;
  SSVAL(SMB_Hdr(pkt), SMB_hdr_pid_offset, Con_Handle -> pid);
  SSVAL(SMB_Hdr(pkt), SMB_hdr_tid_offset, 0);
  SSVAL(SMB_Hdr(pkt), SMB_hdr_mid_offset, Con_Handle -> mid);
  SSVAL(SMB_Hdr(pkt), SMB_hdr_uid_offset, Con_Handle -> uid);
  *(SMB_Hdr(pkt) + SMB_hdr_wct_offset) = 0;

  SSVAL(SMB_Hdr(pkt), SMB_negp_bcc_offset, prots_len);

  /* Now copy the prot strings in with the right stuff */

  p = (char *)(SMB_Hdr(pkt) + SMB_negp_buf_offset);

  for (i = 0; Prots[i] != NULL; i++) {

    *p = SMBdialectID;
    strcpy(p + 1, Prots[i]);
    p = p + strlen(Prots[i]) + 2; /* Adjust len of p for null plus dialectID */

  }

  /* Now send the packet and sit back ... */

  if (RFCNB_Send(Con_Handle -> Trans_Connect, pkt, pkt_len) < 0){


#ifdef DEBUG
    fprintf(stderr, "Error sending negotiate protocol\n");
#endif

    RFCNB_Free_Pkt(pkt);
    SMBlib_errno = -SMBlibE_SendFailed; /* Failed, check lower layer errno */
    return(SMBlibE_BAD);

  }

  /* Now get the response ... */

  if (RFCNB_Recv(Con_Handle -> Trans_Connect, pkt, alloc_len) < 0) {

#ifdef DEBUG
    fprintf(stderr, "Error receiving response to negotiate\n");
#endif

    RFCNB_Free_Pkt(pkt);
    SMBlib_errno = -SMBlibE_RecvFailed; /* Failed, check lower layer errno */
    return(SMBlibE_BAD);

  }

  if (CVAL(SMB_Hdr(pkt), SMB_hdr_rcls_offset) != SMBC_SUCCESS) {  /* Process error */

#ifdef DEBUG
    fprintf(stderr, "SMB_Negotiate failed with errorclass = %i, Error Code = %i\n",
	    CVAL(SMB_Hdr(pkt), SMB_hdr_rcls_offset),
	    SVAL(SMB_Hdr(pkt), SMB_hdr_err_offset));
#endif

    SMBlib_SMB_Error = IVAL(SMB_Hdr(pkt), SMB_hdr_rcls_offset);
    RFCNB_Free_Pkt(pkt);
    SMBlib_errno = SMBlibE_Remote;
    return(SMBlibE_BAD);

  }
  
  if (SVAL(SMB_Hdr(pkt), SMB_negrCP_idx_offset) == 0xFFFF) {

#ifdef DEBUG
    fprintf(stderr, "None of our protocols was accepted ... ");
#endif

    RFCNB_Free_Pkt(pkt);
    SMBlib_errno = SMBlibE_NegNoProt;
    return(SMBlibE_BAD);

  }

  /* Now, unpack the info from the response, if any and evaluate the proto */
  /* selected. We must make sure it is one we like ...                     */

  Con_Handle -> prot_IDX = prot = SVAL(SMB_Hdr(pkt), SMB_negrCP_idx_offset);
  Con_Handle -> protocol = SMB_Figure_Protocol(Prots, prot);

  if (Con_Handle -> protocol == SMB_P_Unknown) { /* No good ... */

    RFCNB_Free_Pkt(pkt);
    SMBlib_errno = SMBlibE_ProtUnknown;
    return(SMBlibE_BAD);

  }
  
  switch (CVAL(SMB_Hdr(pkt), SMB_hdr_wct_offset)) {

  case 0x01:      /* No more info ... */

    break;

  case 13:        /* Up to and including LanMan 2.1 */

    Con_Handle -> Security = SVAL(SMB_Hdr(pkt), SMB_negrLM_sec_offset);
    Con_Handle -> encrypt_passwords = ((Con_Handle -> Security & SMB_sec_encrypt_mask) != 0x00);
    Con_Handle -> Security = Con_Handle -> Security & SMB_sec_user_mask;

    Con_Handle -> max_xmit = SVAL(SMB_Hdr(pkt), SMB_negrLM_mbs_offset);
    Con_Handle -> MaxMPX = SVAL(SMB_Hdr(pkt), SMB_negrLM_mmc_offset);
    Con_Handle -> MaxVC = SVAL(SMB_Hdr(pkt), SMB_negrLM_mnv_offset);
    Con_Handle -> Raw_Support = SVAL(SMB_Hdr(pkt), SMB_negrLM_rm_offset);
    Con_Handle -> SessionKey = IVAL(SMB_Hdr(pkt), SMB_negrLM_sk_offset);
    Con_Handle -> SvrTZ = SVAL(SMB_Hdr(pkt), SMB_negrLM_stz_offset);
    Con_Handle -> Encrypt_Key_Len = SVAL(SMB_Hdr(pkt), SMB_negrLM_ekl_offset);
    
    p = (SMB_Hdr(pkt) + SMB_negrLM_buf_offset);
    fprintf(stderr, "%p", (char *)(SMB_Hdr(pkt) + SMB_negrLM_buf_offset));
    memcpy(Con_Handle->Encrypt_Key, p, 8);

    p = (SMB_Hdr(pkt) + SMB_negrLM_buf_offset + Con_Handle -> Encrypt_Key_Len);

    strncpy(p, Con_Handle -> Svr_PDom, sizeof(Con_Handle -> Svr_PDom) - 1);

    break;

  case 17:        /* NT LM 0.12 and LN LM 1.0 */

    Con_Handle -> Security = SVAL(SMB_Hdr(pkt), SMB_negrNTLM_sec_offset);
    Con_Handle -> encrypt_passwords = ((Con_Handle -> Security & SMB_sec_encrypt_mask) != 0x00);
    Con_Handle -> Security = Con_Handle -> Security & SMB_sec_user_mask;

    Con_Handle -> max_xmit = IVAL(SMB_Hdr(pkt), SMB_negrNTLM_mbs_offset);
    Con_Handle -> MaxMPX = SVAL(SMB_Hdr(pkt), SMB_negrNTLM_mmc_offset);
    Con_Handle -> MaxVC = SVAL(SMB_Hdr(pkt), SMB_negrNTLM_mnv_offset);
    Con_Handle -> MaxRaw = IVAL(SMB_Hdr(pkt), SMB_negrNTLM_mrs_offset);
    Con_Handle -> SessionKey = IVAL(SMB_Hdr(pkt), SMB_negrNTLM_sk_offset);
    Con_Handle -> SvrTZ = SVAL(SMB_Hdr(pkt), SMB_negrNTLM_stz_offset);
    Con_Handle -> Encrypt_Key_Len = CVAL(SMB_Hdr(pkt), SMB_negrNTLM_ekl_offset);

    p = (SMB_Hdr(pkt) + SMB_negrNTLM_buf_offset );
    memcpy(Con_Handle -> Encrypt_Key, p, 8);
    p = (SMB_Hdr(pkt) + SMB_negrNTLM_buf_offset + Con_Handle -> Encrypt_Key_Len);

    strncpy(p, Con_Handle -> Svr_PDom, sizeof(Con_Handle -> Svr_PDom) - 1);

    break;

  default:

#ifdef DEBUG
    fprintf(stderr, "Unknown NegProt response format ... Ignored\n");
    fprintf(stderr, "  wct = %i\n", CVAL(SMB_Hdr(pkt), SMB_hdr_wct_offset));
#endif

    break;
  }

#ifdef DEBUG
  fprintf(stderr, "Protocol selected is: %i:%s\n", prot, Prots[prot]);
#endif

  RFCNB_Free_Pkt(pkt);
  return(0);

}

/* Get our hostname */

void SMB_Get_My_Name(char *name, int len)

{
  if (gethostname(name, len) < 0) { /* Error getting name */

    strncpy(name, "unknown", len);

    /* Should check the error */

#ifdef DEBUG
    fprintf(stderr, "gethostname in SMB_Get_My_Name returned error:");
    perror("");
#endif

  }

  /* only keep the portion up to the first "." */


}
