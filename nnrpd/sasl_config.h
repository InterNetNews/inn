/* sasl_config.h
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-03-04

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef SASL_CONFIG_H
#define SASL_CONFIG_H

#ifndef P
#ifdef __STDC__
#define P(x) x
#else
#define P(x) ()
#endif
#endif

extern void sasl_config_read P((void));
extern const char *sasl_config_getstring P((const char *key, const char *def));
extern int sasl_config_getint P((const char *key, int def));
extern int sasl_config_getswitch P((const char *key, int def));
extern const char *sasl_config_partitiondir P((const char *partition));

#endif /* SASL_SASL_CONFIG_H */
