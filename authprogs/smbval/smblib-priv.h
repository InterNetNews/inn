/* UNIX SMBlib NetBIOS implementation

   Version 1.0
   SMBlib private Defines

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

#include "smblib-common.h"
#include <sys/types.h>
#include <unistd.h>

typedef unsigned short uint16;
typedef unsigned int   uint32;

#include "byteorder.h"     /* Hmmm ... hot good */

#define SMB_DEF_IDF 0x424D53FF        /* "\377SMB" */
 
/* The protocol commands and constants we need */
#define SMBnegprot    0x72   /* negotiate protocol */
#define SMBsesssetupX 0x73   /* Session Set Up & X (including User Logon) */
#define SMBdialectID  0x02   /* a dialect id */

typedef unsigned short WORD;
typedef unsigned short UWORD;
typedef unsigned int ULONG;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;

/* Some macros to allow access to actual packet data so that we */
/* can change the underlying representation of packets.         */
/*                                                              */
/* The current formats vying for attention are a fragment       */
/* approach where the SMB header is a fragment linked to the    */
/* data portion with the transport protocol (rfcnb or whatever) */
/* being linked on the front.                                   */
/*                                                              */
/* The other approach is where the whole packet is one array    */
/* of bytes with space allowed on the front for the packet      */
/* headers.                                                     */

#define SMB_Hdr(p) (char *)(p -> data)

/* SMB Hdr def for File Sharing Protocol? From MS and Intel,    */
/* Intel PN 138446 Doc Version 2.0, Nov 7, 1988. This def also  */
/* applies to LANMAN1.0 as well as the Core Protocol            */
/* The spec states that wct and bcc must be present, even if 0  */

/* We define these as offsets into a char SMB[] array for the   */
/* sake of portability                                          */

/* NOTE!. Some of the lenght defines, SMB_<protreq>_len do not include */
/* the data that follows in the SMB packet, so the code will have to   */
/* take that into account.                                             */

#define SMB_hdr_idf_offset    0          /* 0xFF,'SMB' 0-3 */
#define SMB_hdr_com_offset    4          /* BYTE       4   */
#define SMB_hdr_rcls_offset   5          /* BYTE       5   */
#define SMB_hdr_reh_offset    6          /* BYTE       6   */
#define SMB_hdr_err_offset    7          /* WORD       7   */
#define SMB_hdr_reb_offset    9          /* BYTE       9   */
#define SMB_hdr_flg_offset    9          /* same as reb ...*/
#define SMB_hdr_res_offset    10         /* 7 WORDs    10  */
#define SMB_hdr_res0_offset   10         /* WORD       10  */
#define SMB_hdr_flg2_offset   10         /* WORD           */
#define SMB_hdr_res1_offset   12         /* WORD       12  */
#define SMB_hdr_res2_offset   14
#define SMB_hdr_res3_offset   16
#define SMB_hdr_res4_offset   18
#define SMB_hdr_res5_offset   20
#define SMB_hdr_res6_offset   22
#define SMB_hdr_tid_offset    24
#define SMB_hdr_pid_offset    26
#define SMB_hdr_uid_offset    28
#define SMB_hdr_mid_offset    30
#define SMB_hdr_wct_offset    32

#define SMB_hdr_len           33        /* 33 byte header?      */

#define SMB_hdr_axc_offset    33        /* AndX Command         */
#define SMB_hdr_axr_offset    34        /* AndX Reserved        */
#define SMB_hdr_axo_offset    35     /* Offset from start to WCT of AndX cmd */

/* Format of the Negotiate Protocol SMB */

#define SMB_negp_bcc_offset   33
#define SMB_negp_buf_offset   35        /* Where the buffer starts   */
#define SMB_negp_len          35        /* plus the data             */

/* Format of the Negotiate Response SMB, for CoreProtocol, LM1.2 and */
/* NT LM 0.12. wct will be 1 for CoreProtocol, 13 for LM 1.2, and 17 */
/* for NT LM 0.12                                                    */

#define SMB_negrCP_idx_offset   33        /* Response to the neg req */
#define SMB_negrCP_bcc_offset   35
#define SMB_negrLM_idx_offset   33        /* dialect index           */
#define SMB_negrLM_sec_offset   35        /* Security mode           */
#define SMB_sec_user_mask       0x01      /* 0 = share, 1 = user     */
#define SMB_sec_encrypt_mask    0x02      /* pick out encrypt        */
#define SMB_negrLM_mbs_offset   37        /* max buffer size         */
#define SMB_negrLM_mmc_offset   39        /* max mpx count           */
#define SMB_negrLM_mnv_offset   41        /* max number of VCs       */
#define SMB_negrLM_rm_offset    43        /* raw mode support bit vec*/
#define SMB_negrLM_sk_offset    45        /* session key, 32 bits    */
#define SMB_negrLM_st_offset    49        /* Current server time     */
#define SMB_negrLM_sd_offset    51        /* Current server date     */
#define SMB_negrLM_stz_offset   53        /* Server Time Zone        */
#define SMB_negrLM_ekl_offset   55        /* encryption key length   */
#define SMB_negrLM_res_offset   57        /* reserved                */
#define SMB_negrLM_bcc_offset   59        /* bcc                     */
#define SMB_negrLM_len          61        /* 61 bytes ?              */
#define SMB_negrLM_buf_offset   61        /* Where the fun begins    */

#define SMB_negrNTLM_idx_offset 33        /* Selected protocol       */
#define SMB_negrNTLM_sec_offset 35        /* Security more           */
#define SMB_negrNTLM_mmc_offset 36        /* Different format above  */
#define SMB_negrNTLM_mnv_offset 38        /* Max VCs                 */
#define SMB_negrNTLM_mbs_offset 40        /* MBS now a long          */
#define SMB_negrNTLM_mrs_offset 44        /* Max raw size            */
#define SMB_negrNTLM_sk_offset  48        /* Session Key             */
#define SMB_negrNTLM_cap_offset 52        /* Capabilities            */
#define SMB_negrNTLM_stl_offset 56        /* Server time low         */
#define SMB_negrNTLM_sth_offset 60        /* Server time high        */
#define SMB_negrNTLM_stz_offset 64        /* Server time zone        */
#define SMB_negrNTLM_ekl_offset 66        /* Encrypt key len         */
#define SMB_negrNTLM_bcc_offset 67        /* Bcc                     */
#define SMB_negrNTLM_len        69
#define SMB_negrNTLM_buf_offset 69

/* Offsets for Delete file                                           */

#define SMB_delet_sat_offset    33        /* search attribites          */
#define SMB_delet_bcc_offset    35        /* bcc                        */
#define SMB_delet_buf_offset    37
#define SMB_delet_len           37

/* Offsets for SESSION_SETUP_ANDX for both LM and NT LM protocols    */

#define SMB_ssetpLM_mbs_offset  37        /* Max buffer Size, allow for AndX */
#define SMB_ssetpLM_mmc_offset  39        /* max multiplex count             */
#define SMB_ssetpLM_vcn_offset  41        /* VC number if new VC             */
#define SMB_ssetpLM_snk_offset  43        /* Session Key                     */
#define SMB_ssetpLM_pwl_offset  47        /* password length                 */
#define SMB_ssetpLM_res_offset  49        /* reserved                        */
#define SMB_ssetpLM_bcc_offset  53        /* bcc                             */
#define SMB_ssetpLM_len         55        /* before data ...                 */
#define SMB_ssetpLM_buf_offset  55

#define SMB_ssetpNTLM_mbs_offset 37       /* Max Buffer Size for NT LM 0.12  */
                                          /* and above                       */
#define SMB_ssetpNTLM_mmc_offset 39       /* Max Multiplex count             */
#define SMB_ssetpNTLM_vcn_offset 41       /* VC Number                       */
#define SMB_ssetpNTLM_snk_offset 43       /* Session key                     */
#define SMB_ssetpNTLM_cipl_offset 47      /* Case Insensitive PW Len         */
#define SMB_ssetpNTLM_cspl_offset 49      /* Unicode pw len                  */
#define SMB_ssetpNTLM_res_offset 51       /* reserved                        */
#define SMB_ssetpNTLM_cap_offset 55       /* server capabilities             */
#define SMB_ssetpNTLM_bcc_offset 59       /* bcc                             */
#define SMB_ssetpNTLM_len        61       /* before data                     */
#define SMB_ssetpNTLM_buf_offset 61

#define SMB_ssetpr_axo_offset  35         /* Offset of next response ...    */
#define SMB_ssetpr_act_offset  37         /* action, bit 0 = 1 => guest     */
#define SMB_ssetpr_bcc_offset  39         /* bcc                            */
#define SMB_ssetpr_buf_offset  41         /* Native OS etc                  */

/* The following two arrays need to be in step!              */
/* We must make it possible for callers to specify these ... */

extern const char *SMB_Prots[];
extern int SMB_Types[];

typedef struct SMB_Connect_Def * SMB_Handle_Type;

struct SMB_Connect_Def {

  SMB_Handle_Type Next_Con, Prev_Con;          /* Next and previous conn */
  int protocol;                                /* What is the protocol   */
  int prot_IDX;                                /* And what is the index  */
  void *Trans_Connect;                         /* The connection         */

  /* All these strings should be malloc'd */

  char service[80], username[80], password[80], desthost[80], sock_options[80];
  char address[80], myname[80];

  int gid;         /* Group ID, do we need it?                      */
  int mid;         /* Multiplex ID? We might need one per con       */
  int pid;         /* Process ID                                    */

  int uid;         /* Authenticated user id.                        */

                   /* It is pretty clear that we need to bust some of */
                   /* these out into a per TCon record, as there may  */
                   /* be multiple TCon's per server, etc ... later    */

  int port;        /* port to use in case not default, this is a TCPism! */

  int max_xmit;    /* Max xmit permitted by server                  */
  int Security;    /* 0 = share, 1 = user                           */
  int Raw_Support; /* bit 0 = 1 = Read Raw supported, 1 = 1 Write raw */
  bool encrypt_passwords; /* false = don't                          */ 
  int MaxMPX, MaxVC, MaxRaw;
  unsigned int SessionKey, Capabilities;
  int SvrTZ;                                 /* Server Time Zone */
  int Encrypt_Key_Len;
  char Encrypt_Key[80], Domain[80], PDomain[80], OSName[80], LMType[40];
  char Svr_OS[80], Svr_LMType[80], Svr_PDom[80];

};

#define SMBLIB_DEFAULT_OSNAME "UNIX of some type"
#define SMBLIB_DEFAULT_LMTYPE "SMBlib LM2.1 minus a bit"
#define SMBLIB_MAX_XMIT 65535

/* global Variables for the library */

#ifndef SMBLIB_ERRNO
extern int SMBlib_errno;
extern int SMBlib_SMB_Error;          /* last Error             */
#endif

/* From smbdes.c. */
void E_P16(unsigned char *, unsigned char *);
void E_P24(unsigned char *, unsigned char *, unsigned char *);

/* From smblib-util.c. */
void SMB_Get_My_Name(char *name, int len);

/* From smbencrypt.c. */
void SMBencrypt(unsigned char *passwd, unsigned char *, unsigned char *);
