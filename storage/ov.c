#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "configdata.h"
#include "macros.h"
#include "clibrary.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"
#include "qio.h"
#include "ov.h"
#include "ovmethods.h"
#include "ovinterface.h"

STATIC OV_METHOD	ov;

BOOL OVopen(int mode) {
    int	i;
    BOOL val;

    if (ov.open)
	/* already opened */
	return TRUE;

    /* if innconf isn't already read in, do so. */
    if (innconf == NULL) {
	if (ReadInnConf() < 0) {
	    return FALSE;
	}
    }
    for (i=0;i<NUM_OV_METHODS;i++) {
	if (!strcmp(innconf->ovmethod, ov_methods[i].name))
	    break;
    }
    if (i == NUM_OV_METHODS) {
	syslog(L_FATAL, "%s is not found for ovmethod", innconf->ovmethod);
	(void)fprintf(stderr, "%s is not found for ovmethod", innconf->ovmethod);
	return FALSE;
    }
    ov = ov_methods[i];
    val = (*ov.open)(mode);
    if (atexit(OVclose) < 0) {
	OVclose();
	return FALSE;
    }
    return val;
}

BOOL OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupstats)(group, lo, hi, count, flag));
}

BOOL OVgroupadd(char *group, ARTNUM lo, char *flag) {
    /* lomark should never be changed in each ovmethod if lo is 0 */
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupadd)(group, lo, flag));
}

BOOL OVgroupdel(char *group) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.groupdel)(group));
}

BOOL OVadd(TOKEN token, char *data, int len) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.add)(token, data, len));
}

BOOL OVcancel(TOKEN token) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.cancel)(token));
}

void *OVopensearch(char *group, int low, int high) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.opensearch)(group, low, high));
}

BOOL OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.search)(handle, artnum, data, len, token));
}

void OVclosesearch(void *handle) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return;
    }
    (*ov.closesearch)(handle);
    return;
}

BOOL OVgetartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.getartinfo)(group, artnum, data, len, token));
}

BOOL OVexpiregroup(char *group, int *lo) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    return ((*ov.expiregroup)(group, lo));
}

BOOL OVprobe(OVPROBETYPE type, void *result) {
    if (!ov.open) {
	/* must be opened */
	syslog(L_ERROR, "ovopen must be called first");
	(void)fprintf(stderr, "ovopen must be called first");
	return FALSE;
    }
    switch (type) {
    case OVSPACE:
        return ((*ov.probe)(type, result));
    default:
        return FALSE;
    }
}

void OVclose(void) {
    if (!ov.open)
	return;
    (*ov.close)();
    memset(&ov, '\0', sizeof(ov));
    return;
}
