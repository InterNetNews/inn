/* UNIX SMBlib NetBIOS implementation

   Version 1.0
   SMBlib Defines

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

/* Just define all the entry points */

/* Initialize the library. */

int SMB_Init(void);

/* Connect to a server, but do not do a tree con etc ... */

void *SMB_Connect_Server(void *Con, char *server, char *NTdomain);

/* Negotiate a protocol                                                  */

int SMB_Negotiate(void *Con_Handle, char *Prots[]);

/* Disconnect from server. Has flag to specify whether or not we keep the */
/* handle.                                                                */

int SMB_Discon(void *Con, bool KeepHandle);

/* Log on to a server. */

int SMB_Logon_Server(SMB_Handle_Type Con_Handle, char *UserName,
                     char *PassWord);
