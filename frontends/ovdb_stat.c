/*
 * ovdb_stat.c
 * print information about ovdb database
 */

#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include "macros.h"
#include "paths.h"
#include "storage.h"
#include "ov.h"
#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"


#ifndef USE_BERKELEY_DB

int main(int argc UNUSED, char **argv UNUSED)
{
    fprintf(stderr, "Error: BerkeleyDB not compiled in.\n");
    exit(1);
}

#else /* USE_BERKELEY_DB */

static int signalled = 0;
static void sigfunc(int signum)
{
    signalled = 1;
}

static int html = 0;

typedef enum {
	END,
	INT32,	/* 'a' points to u_int32_t */
	HEX32,  /* 'a' printed in hex */
        DIFF32,	/* 'a' - 'b' - 'c' */
	PCT32,  /* 100 * 'a' / ('a' + 'b') */
	FF,	/* 'a' = freebytes, 'b' = npages, 'c' = pagesize */
	BYTES,	/* 'a' = bytes, 'b' = mbytes, 'c' = gbytes */
	MODE,	/* 'a' points to int, printed as octal mode */
	TIME,	/* 'a' points to time_t, printed as date/time */
	LSN,	/* 'a' points to DB_LSN */
	STR,	/* 'a' points to char* */
	SIZE	/* 'a' points to size_t */
} DATATYPE;

struct datatab {
    DATATYPE type;
    ssize_t a;
    ssize_t b;
    ssize_t c;
    char *desc;
};

static void display_heading(char *str)
{
    if(html)
	printf("<h2>%s<h2>\n", str);
    else
	printf("%s\n", str);
}


static void getval(int i, void *p, struct datatab *tab, char *val, char *sufx)
{
    int mode;
    u_int32_t a, b, c, bytes, kbytes, mbytes, gbytes;
    char *cp = p;
    time_t tm;
    size_t sz;
    DB_LSN *dl;

    val[0] = 0;
    sufx[0] = 0;

    switch(tab[i].type) {
    case INT32:	/* 'a' points to u_int32_t */
	a = *((u_int32_t *)(cp + tab[i].a));
	sprintf(val, "%u", a);
	break;
    case HEX32:	/* 'a' printed in hex */
	a = *((u_int32_t *)(cp + tab[i].a));
	sprintf(val, "%x", a);
	break;
    case DIFF32:	/* 'a' - 'b' - 'c' */
	a = *((u_int32_t *)(cp + tab[i].a));
	b = *((u_int32_t *)(cp + tab[i].b));
	if(tab[i].c != -1) {
	    c = *((u_int32_t *)(cp + tab[i].c));
	    sprintf(val, "%d", a - b - c);
	} else {
	    sprintf(val, "%d", a - b);
	}
	break;
    case PCT32:	/* 100 * 'a' / ('a' + 'b') */
	a = *((u_int32_t *)(cp + tab[i].a));
	b = *((u_int32_t *)(cp + tab[i].b));
	sprintf(val, "%.0f", (double) a / (a + b) * 100.0);
	strcpy(sufx, "%");
	break;
    case FF:	/* 'a' = freebytes, 'b' = npages, 'c' = pagesize */
	a = *((u_int32_t *)(cp + tab[i].a));
	b = *((u_int32_t *)(cp + tab[i].b));
	c = *((u_int32_t *)(cp + tab[i].c));
	if(b == 0) {
	    sprintf(val, "%.0f", 0.0);
	} else {
	    sprintf(val, "%.0f", (double)((b * c) - a) / (b * c) * 100);
	}
	strcpy(sufx, "%");
	break;
    case BYTES:	/* 'a' = bytes, 'b' = mbytes, 'c' = gbytes */
	if(tab[i].a != -1)
	    bytes = *((u_int32_t *)(cp + tab[i].a));
	else
	    bytes = 0;
	if(tab[i].b != -1)
	    mbytes = *((u_int32_t *)(cp + tab[i].b));
	else
	    mbytes = 0;
	if(tab[i].c != -1)
	    gbytes = *((u_int32_t *)(cp + tab[i].c));
	else
	    gbytes = 0;
	if(gbytes > 0 || mbytes > 0) {
	    mbytes += gbytes * 1024;
	    if(bytes > (1024*1024))
		mbytes += bytes / (1024*1024);
	    sprintf(val, "%u", mbytes);
	    strcpy(sufx, "MB");
	} else {
	    sprintf(val, "%u", bytes);
	}
	break;
    case MODE:	/* 'a' points to int, printed as octal mode */
	mode = *((int *)(cp + tab[i].a));
	sprintf(val, "%04o", mode);
	break;	
    case TIME:	/* 'a' points to time_t, printed as date/time */
	tm = *((time_t *)(cp + tab[i].a));
	if(tm == 0) {
	    strcpy(val, "none");
	} else {
	    strftime(val, SMBUF, "%Y-%m-%d %T %Z", localtime(&tm));
	}
	break;
    case LSN:	/* 'a' points to DB_LSN */
	dl = (DB_LSN *)(cp + tab[i].a);
	if(dl->file == 0) {
	    strcpy(val, "none");
	} else {
	    sprintf(val, "%u/%u", dl->file, dl->offset);
	}
	break;
    case STR:	/* 'a' points to char* */
	strcpy(val, *(char **)(cp + tab[i].a));
	break;
    case SIZE:	/* 'a' points to size_t */
	sz = *((size_t *)(cp + tab[i].a));
	sprintf(val, "%d", sz);
	break;
    }
}

static char *myctime(time_t *tm)
{
    static char val[SMBUF];
    strftime(val, SMBUF, "%Y-%m-%d %T %Z", localtime(tm));
    return val;
}

static void display_data(void *p, struct datatab *tab)
{
    int i;
    char val[SMBUF], sufx[SMBUF];
    if(html)
	puts("<table border=0 cellpadding=1>");
    for(i = 0; tab[i].type != END; i++) {
	getval(i, p, tab, val, sufx);
	if(html)
	    printf("<tr><td align=right>%s<td>%s<td>%s\n", val, sufx, tab[i].desc);
	else
	    printf("%16s%-2s %s\n", val, sufx, tab[i].desc);
    }
    if(html)
	puts("</table><p>");
}

static void start_table(char *label, struct datatab *tab)
{
    int i;
    if(html) {
	printf("<h2>%s</h2>\n", label);
	puts("<table border=0 cellpadding=1>\n<tr bgcolor=#3399aa>");
	for(i = 0; tab[i].type != END; i++)
	    printf("<th colspan=2>%s\n", tab[i].desc);
    }
}

static void display_row(void *p, struct datatab *tab)
{
    int i;
    char val[SMBUF], sufx[SMBUF];
    if(html) {
	puts("<tr>");
	for(i = 0; tab[i].type != END; i++) {
	    getval(i, p, tab, val, sufx);
	    printf("<td align=right>%s<td>%s\n", val, sufx);
	}
    } else {
	puts("---------------------------------------------");
	display_data(p, tab);
    }
}

static void end_table(void)
{
    if(html)
	puts("</table><p>");
}

#define OFFSETOF(type, f) ((char *)&(((type *)0)->f) - (char *)0)

#define F(f) OFFSETOF(DB_LOCK_STAT, f)

static struct datatab LOCK_tab[] = {
#if DB_VERSION_MAJOR >= 3
 { INT32, F(st_lastid),        -1, -1,           "Last allocated locker ID" },
#endif
 { INT32, F(st_maxlocks),      -1, -1,           "Maximum number of locks possible" },
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2)
 { INT32, F(st_maxlockers),    -1, -1,           "Maximum number of lockers possible" },
 { INT32, F(st_maxobjects),    -1, -1,           "Maximum number of objects possible" },
#endif
 { INT32, F(st_nmodes),        -1, -1,           "Lock modes" },
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2)
 { INT32, F(st_nlocks),        -1, -1,           "Current locks" },
 { INT32, F(st_maxnlocks),     -1, -1,           "Maximum locks" },
#endif
 { INT32, F(st_nlockers),      -1, -1,           "Current lockers" },
#if DB_VERSION_MAJOR >= 3
 { INT32, F(st_maxnlockers),   -1, -1,           "Maximum lockers" },
#else
 { INT32, F(st_numobjs),       -1, -1,           "Lock objects" },
#endif
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2)
 { INT32, F(st_nobjects),      -1, -1,           "Current objects" },
 { INT32, F(st_maxnobjects),   -1, -1,           "Maximum objects" },
#endif
 { INT32, F(st_nconflicts),    -1, -1,           "Lock conflicts" },
 { INT32, F(st_nrequests),     -1, -1,           "Lock requests" },
 { INT32, F(st_nreleases),     -1, -1,           "Lock releases" },
 { DIFF32, F(st_nrequests), F(st_nreleases), F(st_ndeadlocks), "Outstanding locks" },
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 0)
 { INT32, F(st_nnowaits),      -1, -1,           "Lock requests that would have waited" },
#endif
 { INT32, F(st_ndeadlocks),    -1, -1,           "Deadlocks" },
#if DB_VERSION_MAJOR >= 4
 { INT32, F(st_nlocktimeouts), -1, -1,           "Lock timeouts" },
 { INT32, F(st_ntxntimeouts),  -1, -1,           "Transaction timeouts" },
#endif
 { INT32, F(st_region_nowait), -1, -1,           "Region locks granted without waiting" },
 { INT32, F(st_region_wait),   -1, -1,           "Region locks granted after waiting" },
 { BYTES, F(st_regsize),       -1, -1,           "Lock region size" },
 { END, -1, -1, -1, NULL }
};

static int display_lock()
{
    DB_LOCK_STAT *sp;

#if DB_VERSION_MAJOR == 2
    if(lock_stat(OVDBenv->lk_info, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
    if(lock_stat(OVDBenv, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3
    if(lock_stat(OVDBenv, &sp) != 0)
#else
    if(OVDBenv->lock_stat(OVDBenv, &sp, 0) != 0)
#endif
	return 1;

    display_heading("Lock Region Statistics");
    display_data(sp, LOCK_tab);

    free(sp);
    return 0;
}


#undef F
#define F(f) OFFSETOF(DB_LOG_STAT, f)

static struct datatab LOG_tab[] = {
 { HEX32, F(st_magic),             -1, -1, "Log magic number" },
 { INT32, F(st_version),           -1, -1, "Log version number" },
 { MODE,  F(st_mode),              -1, -1, "Log file mode" },
#if DB_VERSION_MAJOR >= 3
 { BYTES, F(st_lg_bsize),          -1, -1, "Log record cache size" },
#endif
 { BYTES, F(st_lg_max),            -1, -1, "Max log file size" },
 { BYTES, F(st_w_bytes), F(st_w_mbytes), -1, "Log bytes written" },
 { BYTES, F(st_wc_bytes), F(st_wc_mbytes), -1, "Log bytes written since last checkpoint" },
 { INT32, F(st_wcount),            -1, -1, "Total log writes" },
#if DB_VERSION_MAJOR >= 3
 { INT32, F(st_wcount_fill),       -1, -1, "Total log writes due to overflow" },
#endif
 { INT32, F(st_scount),            -1, -1, "Total log flushes" },
 { INT32, F(st_region_nowait),     -1, -1, "Region locks granted without waiting" },
 { INT32, F(st_region_wait),       -1, -1, "Region locks granted after waiting" },
 { INT32, F(st_cur_file),          -1, -1, "Current log file number" },
 { INT32, F(st_cur_offset),        -1, -1, "Current log file offset" },
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3)
 { INT32, F(st_disk_file),         -1, -1, "Known on disk log file number" },
 { INT32, F(st_disk_offset),       -1, -1, "Known on disk log file offset" },
#endif
 { BYTES, F(st_regsize),           -1, -1, "Log region size" },
#if DB_VERSION_MAJOR >= 4
 { INT32, F(st_flushcommit),       -1, -1, "Flushes containing a commit"},
 { INT32, F(st_maxcommitperflush), -1, -1, "Max number of commits in a flush"},
 { INT32, F(st_mincommitperflush), -1, -1, "Min number of commits in a flush"},
#endif
 { END, -1, -1, -1, NULL }
};

static int display_log()
{
    DB_LOG_STAT *sp;

#if DB_VERSION_MAJOR == 2
    if(log_stat(OVDBenv->lg_info, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
    if(log_stat(OVDBenv, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3
    if(log_stat(OVDBenv, &sp) != 0)
#else
    if(OVDBenv->log_stat(OVDBenv, &sp, 0) != 0)
#endif
	return 1;

    display_heading("Log Region Statistics");
    display_data(sp, LOG_tab);

    free(sp);
    return 0;
}


#undef F
#define F(f) OFFSETOF(DB_MPOOL_STAT, f)

static struct datatab MEM_tab[] = {
 { INT32, F(st_cache_hit),  -1, -1,       "Cache hits"},
 { INT32, F(st_cache_miss), -1, -1,       "Cache misses"},
 { PCT32, F(st_cache_hit), F(st_cache_miss), -1, "Cache hit percentage"},
#if DB_VERSION_MAJOR == 2
 { INT32, F(st_cachesize),  -1, -1,       "Total cache size"},
 { INT32, F(st_regsize),    -1, -1,       "Pool region size"},
#else
 { BYTES, F(st_bytes), -1, F(st_gbytes), "Total cache size"},
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
 { INT32, F(st_regsize),    -1, -1,       "Pool region size"},
#else
 { INT32, F(st_ncache),     -1, -1,       "Number of caches"},
 { INT32, F(st_regsize),    -1, -1,       "Pool individual cache size"},
#endif
#endif
 { INT32, F(st_map),           -1, -1, "Memory mapped pages"},
 { INT32, F(st_page_create),   -1, -1, "Pages created in the cache"},
 { INT32, F(st_page_in),       -1, -1, "Pages read into the cache"},
 { INT32, F(st_page_out),      -1, -1, "Pages written from the cache to the backing file"},
 { INT32, F(st_ro_evict),      -1, -1, "Clean pages forced from the cache"},
 { INT32, F(st_rw_evict),      -1, -1, "Dirty pages forced from the cache"},
 { INT32, F(st_hash_buckets),  -1, -1, "Hash buckets used for page location"},
 { INT32, F(st_hash_searches), -1, -1, "Total hash chain searches"},
 { INT32, F(st_hash_longest),  -1, -1, "Longest hash chain searched"},
 { INT32, F(st_hash_examined), -1, -1, "Total hash entries searched"},
 { INT32, F(st_page_trickle),  -1, -1, "Dirty buffers written by trickle-sync thread"},
 { INT32, F(st_page_clean),    -1, -1, "Current clean buffer count"},
 { INT32, F(st_page_dirty),    -1, -1, "Current dirty buffer count"},
 { INT32, F(st_region_nowait), -1, -1, "Region locks granted without waiting"},
 { INT32, F(st_region_wait),   -1, -1, "Region locks granted after waiting"},
 { END, -1, -1, -1, NULL }
};

#undef F
#define F(f) OFFSETOF(DB_MPOOL_FSTAT, f)

static struct datatab MEMF_tab[] = {
 { STR,    F(file_name),        -1, -1, "Database"},
 { SIZE,   F(st_pagesize),      -1, -1, "Page size"},
 { INT32,  F(st_cache_hit),     -1, -1, "Cache hits"},
 { INT32,  F(st_cache_miss),    -1, -1, "Cache misses"},
 { PCT32,  F(st_cache_hit), F(st_cache_miss), -1, "Cache hit percentage"},
 { INT32,  F(st_map),           -1, -1, "Memory mapped pages"},
 { INT32,  F(st_page_create),   -1, -1, "Pages created in the cache"},
 { INT32,  F(st_page_in),       -1, -1, "Pages read into the cache"},
 { INT32,  F(st_page_out),      -1, -1, "Pages written from the cache to the backing file"},
 { END, -1, -1, -1, NULL }
};

static int display_mem(int all)
{
    DB_MPOOL_FSTAT **fsp;
    DB_MPOOL_STAT *gsp;

#if DB_VERSION_MAJOR == 2
    if(memp_stat(OVDBenv->mp_info, &gsp, &fsp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
    if(memp_stat(OVDBenv, &gsp, &fsp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3
    if(memp_stat(OVDBenv, &gsp, &fsp) != 0)
#else
    if(OVDBenv->memp_stat(OVDBenv, &gsp, &fsp, 0) != 0)
#endif
	return 1;

    display_heading("Memory Pool Statistics");
    display_data(gsp, MEM_tab);

    if(all) {
	start_table("Per-database Memory Pool Statistics", MEMF_tab);
	for(; fsp != NULL && *fsp != NULL; ++fsp) {
	    display_row(*fsp, MEMF_tab);
	}
	end_table();
    }

    free(fsp);
    free(gsp);
    return 0;
}

static int txn_compare(a, b)
        const void *a, *b;
{
    if (((DB_TXN_ACTIVE *)a)->txnid > ((DB_TXN_ACTIVE *)b)->txnid)
	return 1;
    if (((DB_TXN_ACTIVE *)a)->txnid < ((DB_TXN_ACTIVE *)b)->txnid)
	return -1;
    return 0;
}

#undef F
#define F(f) OFFSETOF(DB_TXN_STAT, f)

static struct datatab TXN_tab[] = {
 { LSN, F(st_last_ckp),     -1, -1, "File/offset for last checkpoint LSN" },
 { LSN, F(st_pending_ckp),  -1, -1, "File/offset for last pending checkpoint LSN" },
 { TIME, F(st_time_ckp),    -1, -1, "Checkpoint timestamp" },
 { HEX32, F(st_last_txnid), -1, -1, "Last transaction ID allocated" },
 { INT32, F(st_maxtxns),    -1, -1, "Maximum active transactions possible" },
 { INT32, F(st_nactive),    -1, -1, "Active transactions" },
#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3)
 { INT32, F(st_nrestores),  -1, -1, "Restored transactions after recovery" },
#endif
#if DB_VERSION_MAJOR >= 3
 { INT32, F(st_maxnactive), -1, -1, "Maximum active transactions" },
#endif
 { INT32, F(st_nbegins),    -1, -1, "Transactions started" },
 { INT32, F(st_ncommits),   -1, -1, "Transactions committed" },
 { INT32, F(st_naborts),    -1, -1, "Transactions aborted" },
 { INT32, F(st_region_nowait), -1, -1, "Region locks granted without waiting"},
 { INT32, F(st_region_wait),   -1, -1, "Region locks granted after waiting"},
 { BYTES, F(st_regsize),    -1, -1, "Transaction region size" },
 { END, -1, -1, -1, NULL }
};

#undef F
#define F(f) OFFSETOF(DB_TXN_ACTIVE, f)

static struct datatab TXNA_tab[] = {
 { INT32, F(txnid),    -1, -1, "Transaction ID" },
#if DB_VERSION_MAJOR >= 3
 { INT32, F(parentid), -1, -1, "Parent Transaction ID" },
#endif
 { LSN,   F(lsn),      -1, -1, "Initial LSN file/offset" },
 { END, -1, -1, -1, NULL }
};

static int display_txn()
{
    DB_TXN_STAT *sp;
    u_int32_t i;
    char *p;

#if DB_VERSION_MAJOR == 2
    if(txn_stat(OVDBenv->tx_info, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
    if(txn_stat(OVDBenv, &sp, NULL) != 0)
#elif DB_VERSION_MAJOR == 3
    if(txn_stat(OVDBenv, &sp) != 0)
#else
    if(OVDBenv->txn_stat(OVDBenv, &sp, 0) != 0)
#endif
        return 1;

    display_heading("Transaction Region Statistics");
    display_data(sp, TXN_tab);

    if(sp->st_nactive) {
	qsort(sp->st_txnarray, sp->st_nactive, sizeof(sp->st_txnarray[0]), txn_compare);
	start_table("Active Transactions", TXNA_tab);
	for (i = 0; i < sp->st_nactive; ++i)
	    display_row(&(sp->st_txnarray[i]), TXNA_tab);
	end_table();
    }
    free(sp);
    return 0;
}

static int display_ver()
{
    if(html) puts("<p>");
    printf("ovdb data version: %d\n", DATA_VERSION);
    if(html) puts("<br>");
    printf("BerkeleyDB version: %s\n", db_version(NULL,NULL,NULL));
    if(html) puts("<p>");
    return 0;
}

#undef F
#define F(f) OFFSETOF(DB_BTREE_STAT, f)

static struct datatab BTREE_tab[] = {
 { HEX32, F(bt_magic), -1, -1, "Btree magic number" },
 { INT32, F(bt_version), -1, -1, "Btree version number" },
 { INT32, F(bt_minkey), -1, -1, "Minimum keys per page (minkey)" },
 { INT32, F(bt_pagesize), -1, -1, "Database page size" },
 { INT32, F(bt_levels), -1, -1, "Levels in the tree" },
#if DB_VERSION_MAJOR == 2 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0)
 { INT32, F(bt_nrecs), -1, -1, "Keys in the tree" },
#else
 { INT32, F(bt_nkeys), -1, -1, "Unique keys in the tree" },
 { INT32, F(bt_ndata), -1, -1, "Data items in the tree" },
#endif
 { INT32, F(bt_int_pg), -1, -1, "Tree internal pages" },
 { BYTES, F(bt_int_pgfree), -1, -1, "Bytes free in internal pages" },
 { FF,    F(bt_int_pgfree), F(bt_int_pg), F(bt_pagesize), "Internal page fill factor" },

 { INT32, F(bt_leaf_pg), -1, -1, "Tree leaf pages" },
 { BYTES, F(bt_leaf_pgfree), -1, -1, "Bytes free in leaf pages" },
 { FF,    F(bt_leaf_pgfree), F(bt_leaf_pg), F(bt_pagesize), "Leaf page fill factor" },

 { INT32, F(bt_dup_pg), -1, -1, "Tree duplicate pages" },
 { BYTES, F(bt_dup_pgfree), -1, -1, "Bytes free in duplicate pages" },
 { FF,    F(bt_dup_pgfree), F(bt_dup_pg), F(bt_pagesize), "Duplicate page fill factor" },

 { INT32, F(bt_over_pg), -1, -1, "Tree overflow pages" },
 { BYTES, F(bt_over_pgfree), -1, -1, "Bytes free overflow pages" },
 { FF,    F(bt_over_pgfree), F(bt_over_pg), F(bt_pagesize), "Overflow page fill factor" },

#if DB_VERSION_MAJOR >= 3
 { INT32, F(bt_free), -1, -1, "Pages on the free list" },
#endif
 { END, -1, -1, -1, NULL }
};

static int display_btree(DB *db)
{
    DB_BTREE_STAT *sp;

#if DB_VERSION_MAJOR >= 4 || (DB_VERSION_MAJOR >= 3 && DB_VERSION_MINOR >= 3)
    if(db->stat(db, &sp, 0))
	return 1;
#else
    if(db->stat(db, &sp, NULL, 0))
	return 1;
#endif

    display_heading("Btree Statistics");
    display_data(sp, BTREE_tab);

    free(sp);
    return 0;
}


#if DB_VERSION_MAJOR >= 3

#undef F
#define F(f) OFFSETOF(DB_HASH_STAT, f)

static struct datatab HASH_tab[] = {
 { HEX32, F(hash_magic), -1, -1, "Hash magic number" },
 { INT32, F(hash_version), -1, -1, "Hash version number" },
 { INT32, F(hash_pagesize), -1, -1, "Database page size" },
#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0
 { INT32, F(hash_nrecs), -1, -1, "Keys in the database" },
#else
 { INT32, F(hash_nkeys), -1, -1, "Keys in the database" },
 { INT32, F(hash_ndata), -1, -1, "Data items in the database" },
#endif
 { INT32, F(hash_buckets), -1, -1, "Hash buckets" },
 { BYTES, F(hash_bfree), -1, -1, "Bytes free on bucket pages" },
 { FF,    F(hash_buckets), F(hash_bfree), F(hash_pagesize), "Bucket page fill factor" },

 { INT32, F(hash_bigpages), -1, -1, "Overflow pages" },
 { BYTES, F(hash_big_bfree), -1, -1, "Bytes free on Overflow pages" },
 { FF,    F(hash_bigpages), F(hash_big_bfree), F(hash_pagesize), "Overflow page fill factor" },

 { INT32, F(hash_overflows), -1, -1, "Bucket overflow pages" },
 { BYTES, F(hash_ovfl_free), -1, -1, "Bytes free on bucket overflow pages" },
 { FF,    F(hash_overflows), F(hash_ovfl_free), F(hash_pagesize), "Bucket overflow page fill factor" },

 { INT32, F(hash_dup), -1, -1, "Duplicate pages" },
 { BYTES, F(hash_dup_free), -1, -1, "Bytes free in duplicate pages" },
 { FF,    F(hash_dup), F(hash_dup_free), F(hash_pagesize), "Duplicate page fill factor" },

 { INT32, F(hash_free), -1, -1, "Pages on the free list"},
 { END, -1, -1, -1, NULL }
};
#endif

static int display_hash(DB *db)
{
#if DB_VERSION_MAJOR == 2
    printf("Hash statistics not available.\n");
#else
    DB_HASH_STAT *sp;

#if DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR <= 2
    if(db->stat(db, &sp, NULL, 0))
	return 1;
#else
    if(db->stat(db, &sp, 0))
	return 1;
#endif

    display_heading("Hash Information");
    display_data(sp, HASH_tab);

    return 0;
#endif
}

static int display_db(char *dbfile)
{
    int ret;
    DB *db;

#if DB_VERSION_MAJOR == 2
    if(db_open(dbfile, DB_UNKNOWN, DB_RDONLY, 0, OVDBenv, NULL, &db))
	return 1;
#else
    if(db_create(&db, OVDBenv, 0))
	return 1;
    if(db->open(db, dbfile, NULL, DB_UNKNOWN, DB_RDONLY, 0))
	return 1;
#endif

    switch(db->type) {
    case DB_BTREE:
    case DB_RECNO:
	ret = display_btree(db);
	break;
    case DB_HASH:
	ret = display_hash(db);
	break;
    }
    db->close(db, 0);
    return ret;
}

static int parse_artrange(char *str, ARTNUM *start, ARTNUM *stop)
{
    char *c;
    int i;

    c = strchr(str, '-');
    if(c == NULL) {
	i = atoi(str);
	if(i == 0) {
	    return 1;
	}
	*start = *stop = i;
	return 0;
    }
    if(c == str) {
	*start = 0;
	*stop = atoi(str+1);
	return (*stop == 0);
    }
    if(strlen(str) == (c - str + 1)) {
	*start = atoi(str);
	*stop = 0xffffffff;
	return (*start == 0);
    }
    *start = atoi(str);
    *stop = atoi(c+1);
    if(*start == 0 || *stop == 0 || *start > *stop)
	return 1;

    return 0;
}

void htwrite(char *data, int len)
{
    int i;
    for(i = 0; i < len; i++) {
	switch(data[i]) {
	case '<':
	case '>':
	case '&':
	    printf("&#%d;", (int)data[i]);
	    break;
	default:
	    putchar(data[i]);
	}
    }
}

int main(int argc, char *argv[])
{
    void *s;
    ARTNUM a, start=0, stop=0, low, high;
    char *data, *disp_db = NULL;
    int len, c, count, flag, lowi, highi;
    int getgs=0, getcount=0, getinfo=0, err=0, gotone=0;
    int disp_lock=0, disp_log=0, disp_mem=0, disp_mem_all=0, disp_txn=0, disp_ver=0;
    int needng=0, o;

    openlog("ovdb_stat", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if(ReadInnConf() < 0)
	exit(1);

    if(!ovdb_check_user()) {
	fprintf(stderr, "Error: Only run this command as user " NEWSUSER "\n");
	exit(1);
    }
    if(!ovdb_getlock(OVDB_LOCK_ADMIN)) {
	fprintf(stderr, "Can't lock database: %s\n", strerror(errno));
	exit(1);
    }
    if(!ovdb_open(OV_READ|OVDB_SERVER)) {
	fprintf(stderr, "Can't open overview; check syslog for OVDB messages\n");
	exit(1);
    }

    xsignal(SIGINT, sigfunc);
    xsignal(SIGTERM, sigfunc);
    xsignal(SIGHUP, sigfunc);

    while((c = getopt(argc, argv, ":Hgcir:klmMtvd:")) != -1) {
	switch(c) {
	case 'H':
	    html = 1;
	    break;
	case 'g':
	    getgs = 1;
	    needng = 1;
	    gotone++;
	    break;
	case 'c':
	    getcount = 1;
	    needng = 1;
	    gotone++;
	    break;
	case 'i':
	    getinfo = 1;
	    needng = 1;
	    gotone++;
	    break;
	case 'r':
	    if(parse_artrange(optarg, &start, &stop))
		err++;
	    needng = 1;
	    gotone++;
	    break;
	case 'k':
	    disp_lock = 1;
	    gotone++;
	    break;
	case 'l':
	    disp_log = 1;
	    gotone++;
	    break;
	case 'm':
	    disp_mem = 1;
	    gotone++;
	    break;
	case 'M':
	    disp_mem = 1;
	    disp_mem_all = 1;
	    gotone++;
	    break;
	case 't':
	    disp_txn = 1;
	    gotone++;
	    break;
	case 'v':
	    disp_ver = 1;
	    gotone++;
	    break;
	case 'd':
	    disp_db = optarg;
	    gotone++;
	    break;
	case ':':
	    fprintf(stderr, "Option -%c requires an argument\n", optopt);
	    err++;
	    break;
	case '?':
	    fprintf(stderr, "Unrecognized option: -%c\n", optopt);
	    err++;
	    break;
	}
    }
    if(!gotone) {
	err++;
    } else if(optind == argc && needng) {
	fprintf(stderr, "Missing newsgroup argument(s)\n");
	err++;
    }
    if(err) {
	fprintf(stderr, "\
Usage:\n\
   ovdb_stat -Hgci [-r artnum] newsgroup [newsgroup ...]\n\
      -H              : output in HTML\n\
      -g              : show groupstats info\n\
      -c              : show groupstats info by counting actual records\n\
      -i              : show additional group info\n\
      -r artnum-range : retrieve OV records for article number range\n\
\n\
   ovdb_stat -Hklmtv [-d <database>]\n\
      -H          : output in HTML\n\
      -k          : Display lock region statistics\n\
      -l          : Display log region statistics\n\
      -m          : Display global memory cache statistics\n\
      -M          : Display all memory cache statistics\n\
      -t          : Display transaction statistics\n\
      -v          : Display version information\n\
      -d database : Display statistics of specified database\n");

	goto out;
    }

    if(html)
	puts("<html><head><title>ovdb_stat</title></head><body><p>");
    if(disp_lock)
	display_lock();
    if(disp_log)
	display_log();
    if(disp_mem)
	display_mem(disp_mem_all);
    if(disp_txn)
	display_txn();
    if(disp_ver)
	display_ver();
    if(disp_db)
	display_db(disp_db);

    if(getgs || getcount || getinfo) {
	if(html) {
	    puts("<table border=0 cellpadding=1 width=90%>\n<tr bgcolor=#3399aa>");
	    puts("<th rowspan=2>Group");
	    if(getgs)
		puts("<th colspan=4>Groupstats");
	    if(getcount)
		puts("<th colspan=3>Counted");
	    if(getinfo)
		puts("<th>Status<th colspan=2>Current<th colspan=2>Pending");
	    puts("<th rowspan=2>Expired<th rowspan=2>Expire PID<tr bgcolor=#3399aa>");
	    if(getgs)
		puts("<th>Low<th>High<th>Count<th>Flag");
	    if(getcount)
		puts("<th>Low<th>High<th>Count");
	    if(getinfo)
		puts("<th>Flags<th>GroupID<th>DB<th>GroupID<th>DB");
	}
	for(o = optind ; o < argc; o++) {
	    if(html)
		printf("<tr><td>%s", argv[o]);
	    if(getgs) {
		if(ovdb_groupstats(argv[o], &lowi, &highi, &count, &flag)) {
		    if(html)
			printf("<td>%d<td>%d<td>%d<td>%c", lowi, highi, count, flag);
		    else
			printf("%s: groupstats: low: %d, high: %d, count: %d, flag: %c\n",
				argv[o], lowi, highi, count, flag);
		}
	    }
	    if(getcount) {
		low = high = count = 0;
		if(s = ovdb_opensearch(argv[o], 1, 0xffffffff)) {
		    while(ovdb_search(s, &a, NULL, NULL, NULL, NULL)) {
			if(low == 0 || a < low)
			    low = a;
			if(a > high)
			    high = a;
			count++;
			if(signalled)
			    break;
		    }
		    ovdb_closesearch(s);
		    if(signalled)
			goto out;
		    if(html)
			printf("<td>%d<td>%d<td>%d", low, high, count);
		    else
			printf("%s:    counted: low: %d, high: %d, count: %d\n",
				argv[o], low, high, count);
		}
	    }
	    if(getinfo) {
		int ret;
		struct groupinfo gi;

		if(ret = ovdb_getgroupinfo(argv[o], &gi, FALSE, NULL, 0)) {
		    fprintf(stderr, "%s: ovdb_getgroupinfo error: %s\n", argv[o], db_strerror(ret));
		    continue;
		}
		if(html) {
		    printf("<td>%s%s%s%s",
			(gi.status & GROUPINFO_DELETED) ? "D ":"",
			(gi.status & GROUPINFO_EXPIRING) ? "E ":"",
			(gi.status & GROUPINFO_MOVING) ? "M":"",
			(gi.status == 0) ? "&nbsp;":"");
		    printf("<td>%d<td>ov%05d", gi.current_gid, gi.current_db);
		    if(gi.status & GROUPINFO_MOVING)
			printf("<td>%d<td>ov%05d", gi.new_gid, gi.new_db);
		    else
			printf("<td>&nbsp;<td>&nbsp;");
		    if(gi.expired)
			printf("<td>%s<td>%d", myctime(&gi.expired), gi.expiregrouppid);
		    else
			printf("<td>&nbsp;<td>&nbsp;");
		    putchar('\n');
		} else {
		    printf("%s: flags: %s%s%s%s\n", argv[o],
			(gi.status & GROUPINFO_DELETED) ? "DELETED ":"",
			(gi.status & GROUPINFO_EXPIRING) ? "EXPIRING ":"",
			(gi.status & GROUPINFO_MOVING) ? "MOVING":"",
			(gi.status == 0) ? "none":"");

		    printf("%s: gid: %d;  Stored in: ov%05d\n", argv[o], gi.current_gid, gi.current_db);
		    if(gi.status & GROUPINFO_MOVING)
			printf("%s: pending gid: %d;  pending db: ov%05d\n", argv[o], gi.new_gid, gi.new_db);
		    if(gi.expired) {
			printf("%s: last expired: %s\n", argv[o], myctime(&gi.expired));
			printf("%s: by process id: %d\n", argv[o], gi.expiregrouppid);
		    }
		}
	    }
	    if(signalled)
		goto out;
	}
	if(html)
	    puts("</table><p>");
    }
    if(start || stop) {
	if(html)
	    puts("<pre>");
	for(o = optind ; o < argc; o++) {
	    if(s = ovdb_opensearch(argv[o], start, stop)) {
		while(ovdb_search(s, &a, &data, &len, NULL, NULL)) {
		    if(html)
			htwrite(data, len);
		    else
			fwrite(data, len, 1, stdout);
		    if(signalled)
			break;
		}
		ovdb_closesearch(s);
		if(signalled)
		    goto out;
	    }
	    if(signalled)
		goto out;
	}
	if(html)
	    puts("</pre>");
    }
out:
    if(html)
	puts("<p></body></html>");
    ovdb_close();
    return 0;
}

#endif /* USE_BERKELEY_DB */

