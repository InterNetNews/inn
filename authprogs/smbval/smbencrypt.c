/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Modified by Jeremy Allison 1995.
   
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

typedef unsigned char uchar;

void strupper(char *s);

/*
   This implements the X/Open SMB password encryption
   It takes a password, a 8 byte "crypt key" and puts 24 bytes of 
   encrypted password into p24 */
void SMBencrypt(uchar *passwd, uchar *c8, uchar *p24)
{
  uchar p14[15], p21[21];

  memset(p21,'\0',21);
  memset(p14,'\0',14);
  strlcpy((char *) p14, (char *) passwd, sizeof(p14));

  strupper((char *)p14);
  E_P16(p14, p21); 
  E_P24(p21, c8, p24);
}

void strupper(char *s)
{
  while (*s)
  {
    {
      if (islower(*s))
        *s = toupper(*s);
      s++;
    }
  }
}                      
