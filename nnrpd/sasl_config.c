/* sasl_config.c -- Configuration routines
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

#include "config.h"

#ifdef HAVE_SSL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>

#include "paths.h"
#include "sasl_config.h"

extern char *xstrdup (const char *str);

struct configlist {
    char *key;
    char *value;
};

static struct configlist *configlist;
static int nconfiglist;

const char *sasl_config_getstring(key, def)
const char *key;
const char *def;
{
    int opt;

    for (opt = 0; opt < nconfiglist; opt++) {
	if (*key == configlist[opt].key[0] &&
	    !strcmp(key, configlist[opt].key))
	  return configlist[opt].value;
    }
    return def;
}

int sasl_config_getint(key, def)
const char *key;
int def;
{
    const char *val = sasl_config_getstring(key, (char *)0);

    if (!val) return def;
    if (!isdigit(*val) && (*val != '-' || !isdigit(val[1]))) return def;
    return atoi(val);
}

int sasl_config_getswitch(key, def)
const char *key;
int def;
{
    const char *val = sasl_config_getstring(key, (char *)0);

    if (!val) return def;

    if (*val == '0' || *val == 'n' ||
	(*val == 'o' && val[1] == 'f') || *val == 'f') {
	return 0;
    }
    else if (*val == '1' || *val == 'y' ||
	     (*val == 'o' && val[1] == 'n') || *val == 't') {
	return 1;
    }
    return def;
}

const char *sasl_config_partitiondir(partition)
const char *partition;
{
    char buf[80];

    if (strlen(partition) > 70) return 0;
    strcpy(buf, "partition-");
    strcat(buf, partition);

    return sasl_config_getstring(buf, (char *)0);
}

#define CONFIGLISTGROWSIZE 10 /* 100 */
void
sasl_config_read()
{
    FILE *infile;
    int lineno = 0;
    int alloced = 0;
    char buf[4096];
    char *p, *key;

    infile = fopen(_PATH_SASL_CONFIG, "r");
    if (!infile) {
      fprintf(stderr, "can't open configuration file %s\n", _PATH_SASL_CONFIG);
      exit(1);
    }
    
    while (fgets(buf, sizeof(buf), infile)) {
	lineno++;

	if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';
	for (p = buf; *p && isspace(*p); p++);
	if (!*p || *p == '#') continue;

	key = p;
	while (*p && (isalnum(*p) || *p == '-' || *p == '_')) {
	    if (isupper(*p)) *p = tolower(*p);
	    p++;
	}
	if (*p != ':') {
	    fprintf(stderr,
		    "invalid option name on line %d of configuration file\n",
		    lineno);
	    exit(1);
	}
	*p++ = '\0';

	while (*p && isspace(*p)) p++;
	
	if (!*p) {
	    fprintf(stderr, "empty option value on line %d of configuration file\n",
		    lineno);
	    exit(1);
	}

	if (nconfiglist == alloced) {
	    alloced += CONFIGLISTGROWSIZE;
	    configlist = (struct configlist *)
	      xrealloc((char *)configlist, alloced*sizeof(struct configlist));
	}

	configlist[nconfiglist].key = xstrdup(key);
	configlist[nconfiglist].value = xstrdup(p);
	nconfiglist++;
    }
    fclose(infile);
}

/*
 * Call proc (expected to be todo_append in reconstruct.c) with
 * information on each configured partition
 */
void
sasl_config_scanpartition(proc)
void (*proc)();
{
    int opt;
    char *s;

    for (opt = 0; opt < nconfiglist; opt++) {
	if (!strncmp(configlist[opt].key, "partition-", 10)) {
	    s = xstrdup(configlist[opt].value);
	    (*proc)(xstrdup(""), s, configlist[opt].key+10);
	}
    }
}

#endif /* HAVE_SSL */
