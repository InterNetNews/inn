/*
**  Overview interface header.
**
**  Written by Katsuhiro Kondou in 1999.
**
**  Various bug fixes, code and documentation improvements since then
**  in 1999-2006, 2010, 2011, 2017, 2020, 2021, 2026.
*/

#ifndef OVINTERFACE_H
#define OVINTERFACE_H

#include <stdio.h>

#include "config.h"
#include "inn/history.h"
#include "inn/ov.h"
#include "inn/storage.h"

struct buffer;
struct vector;

typedef struct overview_method {
    const char *name;
    bool (*open)(int mode);
    bool (*groupstats)(const char *group, int *lo, int *hi, int *count,
                       int *flag);
    bool (*groupadd)(const char *group, ARTNUM lo, ARTNUM hi, char *flag);
    bool (*groupdel)(const char *group);
    bool (*add)(const char *group, ARTNUM artnum, TOKEN token, char *data,
                int len, time_t arrived, time_t expires);
    bool (*cancel)(const char *group, ARTNUM artnum);
    void *(*opensearch)(const char *group, int low, int high);
    bool (*search)(void *handle, ARTNUM *artnum, char **data, int *len,
                   TOKEN *token, time_t *arrived);
    void (*closesearch)(void *handle);
    bool (*getartinfo)(const char *group, ARTNUM artnum, TOKEN *token);
    bool (*expiregroup)(const char *group, int *lo, struct history *h);
    bool (*ctl)(OVCTLTYPE type, void *val);
    void (*close)(void);
} OV_METHOD;

bool OVgroupbasedexpire(TOKEN token, const char *group, const char *data,
                        int len, time_t arrived, time_t expires);
struct bloom_filter;
bool OVhisthasmsgid(struct history *, const char *data);
void OVEXPremove(TOKEN token, bool deletedgroups, char **xref, int ngroups);
void OVEXPcleanup(void);

extern struct bloom_filter *OVtokencache;
extern time_t OVnow;
extern FILE *EXPunlinkfile;

/* Tombstone log: when set, OVEXPremove appends the textual form of each
 * successfully cancelled token to this stream.  A subsequent expire run
 * consumes the log to drop history entries for those articles without
 * doing per-article SMretrieve(RETR_STAT) calls.  Set by expireover (and
 * by fastrm when called via expirerm in delayrm mode).  Internal --
 * managed via direct extern access by expireover/fastrm, not via OVctl,
 * because the file lifecycle (atomic .NEW -> final rename on success)
 * does not fit the per-call OVctl pattern. */
extern FILE *OVtombstonefile;

extern bool OVignoreselfexpire;
extern bool OVusepost;
extern bool OVkeep;
extern bool OVearliest;
extern bool OVquiet;
extern time_t OVrealnow;
extern long EXPprocessed;
extern long EXPunlinked;
extern long EXPoverindexdrop;

#define DEFAULT_MAX_XREF_LEN 8192

#endif /* OVINTERFACE_H */
