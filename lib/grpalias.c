#include <sys/types.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include "configdata.h"
#include "clibrary.h"
#include "macros.h"
#include "libinn.h"
#include "paths.h"
#include "qio.h"

#define TABLESIZE       1024;

typedef struct _AE {
    char                *name;    /* Name of the pseudogroup */
    HASH                hash;     /* Hash of the name of the pseudogroup */
    char                *newname; /* Name of the real group this points to */
    HASH                newhash;  /* Hash of newname */
    struct _AE          *next;
} ALIASENTRY;

ALIASENTRY              *AliasTable[1024];

void AliasAdd(const char *alias, const char *group) {
    ALIASENTRY          *newentry;
    unsigned int        i;

    newentry = NEW(ALIASENTRY, 1);
    newentry->name = COPY(alias);
    newentry->hash = Hash(alias, strlen(alias));
    newentry->newname = COPY(group);
    newentry->newhash = Hash(group, strlen(group));
    memcpy(&i, &newentry->hash, sizeof(i));
    i %= TABLESIZE;
    newentry->next = AliasTable[i];
    AliasTable[i] = newentry;
}

BOOL LoadGroupAliases(void) {
    QIOSTATE            *qp;
    char                *line;
    char                linebuf[1024];
    char                *p, *q;
    int                 lineno = 0;
    
    if ((qp = QIOopen(cpcatpath(innconf->pathetc, "group.aliases"))) == NULL)
	return TRUE;
    memset(AliasTable, '\0', sizeof(AliasTable));
    while ((line = QIOread(qp)) != NULL) {
	lineno++;
	if (strlen(line) > 1024) {
	    syslog(L_FATAL, "line %d is too long", lineno);
	    return FALSE;
	}
	for (p = line, q = linebuf; *p; p++) {
	    if (*p == '#')
		break;
	    if (isspace((int)*p))
		continue;
	    *q++ = *p;
	}
	*q = '\0';
	if ((p = strchr(linebuf, '=')) == NULL) {
	    syslog(L_FATAL, "Missing '=' on line %d", lineno);
	    return FALSE;
	}
	*p++ = '\0';
	AliasAdd(linebuf, p);
    }
    QIOclose(qp);
    return TRUE;
}

const char  *Aliasgetnamebygroup(const char *group) {
    HASH                hash;
    unsigned int        i;
    ALIASENTRY          *ae;

    hash = Hash(group, strlen(group));
    memcpy(&i, &hash, sizeof(hash));
    i %= TABLESIZE;
    for (ae = AliasTable[i]; ae != NULL; ae = ae->next) {
	if (memcmp(&hash, &ae->hash, sizeof(HASH)) == 0) {
	    return ae->newname;
	}
    }
    return NULL;
}

HASH Aliasgethashbygroup(const char *group) {
    HASH                hash;
    unsigned int        i;
    ALIASENTRY          *ae;

    hash = Hash(group, strlen(group));
    memcpy(&i, &hash, sizeof(hash));
    i %= TABLESIZE;
    for (ae = AliasTable[i]; ae != NULL; ae = ae->next) {
	if (memcmp(&hash, &ae->hash, sizeof(HASH)) == 0) {
	    return ae->newhash;
	}
    }
    HashClear(&hash);
    return hash;
}

HASH Aliasgethashbyhash(const HASH hash) {
    unsigned int        i;
    ALIASENTRY          *ae;
    HASH                empty;

    memcpy(&i, &hash, sizeof(hash));
    i %= TABLESIZE;
    for (ae = AliasTable[i]; ae != NULL; ae = ae->next) {
	if (memcmp(&hash, &ae->hash, sizeof(HASH)) == 0) {
	    return ae->newhash;
	}
    }
    HashClear(&empty);
    return empty;
}

const char  *Aliasgetnamebyhash(const HASH hash) {
    unsigned int        i;
    ALIASENTRY          *ae;

    memcpy(&i, &hash, sizeof(hash));
    i %= TABLESIZE;
    for (ae = AliasTable[i]; ae != NULL; ae = ae->next) {
	if (memcmp(&hash, &ae->hash, sizeof(HASH)) == 0) {
	    return ae->newname;
	}
    }
    return NULL;
}
