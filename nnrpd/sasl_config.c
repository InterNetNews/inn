/* sasl_config.c -- Configuration routines
   Copyright (C) 2000 Kenichi Okada <okada@opaopa.org>

   Author: Kenichi Okada <okada@opaopa.org>
   Created: 2000-03-04
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>

#include "clibrary.h"
#include "nnrpd.h"
#include "paths.h"
#include "sasl_config.h"

#ifdef HAVE_SSL

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
    snprintf(buf, sizeof(buf), "partition-%s", partition);

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
    static char *SASL_CONFIG = NULL;

    if (!SASL_CONFIG)
	SASL_CONFIG = concatpath(innconf->pathetc, _PATH_SASL_CONFIG);
    infile = fopen(SASL_CONFIG, "r");
    if (!infile) {
      fprintf(stderr, "can't open configuration file %s\n", SASL_CONFIG);
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
	    RENEW(configlist, struct configlist, alloced);
	}

	configlist[nconfiglist].key = COPY(key);
	configlist[nconfiglist].value = COPY(p);
	nconfiglist++;
    }
    fclose(infile);
}

#endif /* HAVE_SSL */
