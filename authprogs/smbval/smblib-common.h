/* UNIX SMBlib NetBIOS implementation

   Version 1.0
   SMBlib Common Defines

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

/* Error CLASS codes and etc ... */

#define SMBC_SUCCESS        0
#define SMBC_ERRDOS         0x01
#define SMBC_ERRSRV         0x02
#define SMBC_ERRHRD         0x03
#define SMBC_ERRCMD         0xFF

/* Define the protocol types ... */

#define SMB_P_Unknown      -1        /* Hmmm, is this smart? */
#define SMB_P_Core         0
#define SMB_P_CorePlus     1
#define SMB_P_DOSLanMan1   2
#define SMB_P_LanMan1      3
#define SMB_P_DOSLanMan2   4 
#define SMB_P_LanMan2      5
#define SMB_P_DOSLanMan2_1 6
#define SMB_P_LanMan2_1    7
#define SMB_P_NT1          8

/* SMBlib return codes */
/* We want something that indicates whether or not the return code was a   */
/* remote error, a local error in SMBlib or returned from lower layer ...  */
/* Wonder if this will work ...                                            */
/* SMBlibE_Remote = 1 indicates remote error                               */
/* SMBlibE_ values < 0 indicate local error with more info available       */
/* SMBlibE_ values >1 indicate local from SMBlib code errors?              */

#define SMBlibE_Success 0
#define SMBlibE_Remote  1    /* Remote error, get more info from con        */
#define SMBlibE_BAD     -1
#define SMBlibE_LowerLayer 2 /* Lower layer error                           */
#define SMBlibE_NotImpl 3    /* Function not yet implemented                */
#define SMBlibE_ProtLow 4    /* Protocol negotiated does not support req    */
#define SMBlibE_NoSpace 5    /* No space to allocate a structure            */
#define SMBlibE_BadParam 6   /* Bad parameters                              */
#define SMBlibE_NegNoProt 7  /* None of our protocols was liked             */
#define SMBlibE_SendFailed 8 /* Sending an SMB failed                       */
#define SMBlibE_RecvFailed 9 /* Receiving an SMB failed                     */
#define SMBlibE_GuestOnly 10 /* Logged in as guest                          */
#define SMBlibE_CallFailed 11 /* Call remote end failed                     */
#define SMBlibE_ProtUnknown 12 /* Protocol unknown                          */
#define SMBlibE_NoSuchMsg  13 /* Keep this up to date                       */
