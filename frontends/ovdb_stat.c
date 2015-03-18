/*
 * ovdb_stat.c
 * print information about ovdb database
 */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"
#include "inn/ov.h"
#include "inn/paths.h"
#include "inn/storage.h"

#include "../storage/ovdb/ovdb.h"
#include "../storage/ovdb/ovdb-private.h"


#ifndef HAVE_BDB

int main(int argc UNUSED, char **argv UNUSED)
{
    die("Berkeley DB support not compiled");
}

#else /* HAVE_BDB */

static int signalled = 0;
static void sigfunc(int signum UNUSED)
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
    const char *desc;
};

static void display_heading(const char *str)
{
    if(html)
	printf("<h2>%s<h2>\n", str);
    else
	printf("%s\n", str);
}


static void
print_value(int i, void *p, struct datatab *tab, const char *sep)
{
    int mode = 0;
    u_int32_t a = 0, b = 0, c = 0, bytes = 0, mbytes = 0, gbytes = 0;
    double percent;
    char *cp = p;
    char *tmp = NULL;
    time_t tm = 0;
    size_t sz = 0;
    DB_LSN *dl = NULL;
    char buf[SMBUF];

    switch(tab[i].type) {
    case INT32:	/* 'a' points to u_int32_t */
        memcpy(&a, cp + tab[i].a, sizeof(a));
        printf("%16u%s", a, sep);
	break;
    case HEX32:	/* 'a' printed in hex */
        memcpy(&a, cp + tab[i].a, sizeof(a));
        printf("%16x%s", a, sep);
	break;
    case DIFF32:	/* 'a' - 'b' - 'c' */
        memcpy(&a, cp + tab[i].a, sizeof(a));
        memcpy(&b, cp + tab[i].b, sizeof(b));
	if (tab[i].c != -1) {
            memcpy(&c, cp + tab[i].c, sizeof(c));
            printf("%16d%s", a - b - c, sep);
	} else {
            printf("%16d%s", a - b, sep);
	}
	break;
    case PCT32:	/* 100 * 'a' / ('a' + 'b') */
        memcpy(&a, cp + tab[i].a, sizeof(a));
        memcpy(&b, cp + tab[i].b, sizeof(b));
        printf("%16.0f%s%% ", (double) a / (a + b) * 100.0, sep);
	break;
    case FF:	/* 'a' = freebytes, 'b' = npages, 'c' = pagesize */
        memcpy(&a, cp + tab[i].a, sizeof(a));
        memcpy(&b, cp + tab[i].b, sizeof(b));
        memcpy(&c, cp + tab[i].c, sizeof(c));
	if (b == 0) {
            printf("%16.0f%s%% ", 0.0, sep);
	} else {
            percent = (double) ((b * c) - a) / (b * c) * 100;
            printf("%16.0f%s%% ", percent, sep);
	}
	break;
    case BYTES:	/* 'a' = bytes, 'b' = mbytes, 'c' = gbytes */
	if (tab[i].a != -1)
            memcpy(&bytes, cp + tab[i].a, sizeof(bytes));
	else
	    bytes = 0;
	if (tab[i].b != -1)
            memcpy(&mbytes, cp + tab[i].b, sizeof(mbytes));
	else
	    mbytes = 0;
	if (tab[i].c != -1)
            memcpy(&gbytes, cp + tab[i].c, sizeof(gbytes));
	else
	    gbytes = 0;
	if (gbytes > 0 || mbytes > 0) {
	    mbytes += gbytes * 1024;
	    if (bytes > (1024*1024))
		mbytes += bytes / (1024*1024);
            printf("%16u%sMB", mbytes, sep);
	} else {
            printf("%16u%s", bytes, sep);
	}
	break;
    case MODE:	/* 'a' points to int, printed as octal mode */
        memcpy(&mode, cp + tab[i].a, sizeof(mode));
        printf("        %04o%s", mode, sep);
	break;	
    case TIME:	/* 'a' points to time_t, printed as date/time */
        memcpy(&tm, cp + tab[i].a, sizeof(tm));
	if (tm == 0) {
            printf("%16s%s", "none", sep);
	} else {
	    strftime(buf, SMBUF, "%Y-%m-%d %T %Z", localtime(&tm));
            printf("%16s%s", buf, sep);
	}
	break;
    case LSN:	/* 'a' points to DB_LSN */
	dl = (DB_LSN *)(cp + tab[i].a);
	if (dl->file == 0) {
            printf("%16s%s", "none", sep);
	} else {
            snprintf(buf, sizeof(buf), "%u/%u", dl->file, dl->offset);
            printf("%16s%s", buf, sep);
	}
	break;
    case STR:	/* 'a' points to char* */
        memcpy(&tmp, cp + tab[i].a, sizeof(tmp));
        printf("%16s%s", tmp, sep);
	break;
    case SIZE:	/* 'a' points to size_t */
        memcpy(&sz, cp + tab[i].a, sizeof(sz));
        printf("%16lu%s", (unsigned long) sz, sep);
	break;
    case END:
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

    if (html)
	puts("<table border=0 cellpadding=1>");
    for (i = 0; tab[i].type != END; i++) {
        if (html) {
            printf("<tr><td align=right>");
            print_value(i, p, tab, "<td>");
            printf("<td>%s\n", tab[i].desc);
        } else {
            print_value(i, p, tab, "");
            printf(" %s\n", tab[i].desc);
        }
    }
    if (html)
	puts("</table><p>");
}

static void start_table(const char *label, struct datatab *tab)
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

    if (html) {
	puts("<tr>");
	for (i = 0; tab[i].type != END; i++) {
            printf("<td align=right>");
            print_value(i, p, tab, "<td>");
            printf("\n");
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
 { INT32,
   F(st_id),
   -1, -1,           "Last allocated locker ID" },
 { INT32, F(st_maxlocks),      -1, -1,           "Maximum number of locks possible" },
 { INT32, F(st_maxlockers),    -1, -1,           "Maximum number of lockers possible" },
 { INT32, F(st_maxobjects),    -1, -1,           "Maximum number of objects possible" },
 { INT32, F(st_nmodes),        -1, -1,           "Lock modes" },
 { INT32, F(st_nlocks),        -1, -1,           "Current locks" },
 { INT32, F(st_maxnlocks),     -1, -1,           "Maximum locks" },
 { INT32, F(st_nlockers),      -1, -1,           "Current lockers" },
 { INT32, F(st_maxnlockers),   -1, -1,           "Maximum lockers" },
 { INT32, F(st_nobjects),      -1, -1,           "Current objects" },
 { INT32, F(st_maxnobjects),   -1, -1,           "Maximum objects" },
 { INT32, F(st_lock_wait),     -1, -1,           "Lock conflicts" },
 { INT32, F(st_nrequests),     -1, -1,           "Lock requests" },
 { INT32, F(st_nreleases),     -1, -1,           "Lock releases" },
 { DIFF32, F(st_nrequests), F(st_nreleases), F(st_ndeadlocks), "Outstanding locks" },
 { INT32, F(st_lock_nowait),   -1, -1,           "Lock conflicts w/o subsequent wait" },
 { INT32, F(st_ndeadlocks),    -1, -1,           "Deadlocks" },
 { INT32, F(st_nlocktimeouts), -1, -1,           "Lock timeouts" },
 { INT32, F(st_ntxntimeouts),  -1, -1,           "Transaction timeouts" },
 { INT32, F(st_region_nowait), -1, -1,           "Region locks granted without waiting" },
 { INT32, F(st_region_wait),   -1, -1,           "Region locks granted after waiting" },
 { BYTES, F(st_regsize),       -1, -1,           "Lock region size" },
 { END, -1, -1, -1, NULL }
};

static int display_lock(void)
{
    DB_LOCK_STAT *sp;

    if(OVDBenv->lock_stat(OVDBenv, &sp, 0) != 0)
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
 { BYTES, F(st_lg_bsize),          -1, -1, "Log record cache size" },
 { BYTES, F(st_lg_size),           -1, -1, "The current log file size" },
 { BYTES, F(st_w_bytes), F(st_w_mbytes), -1, "Log bytes written" },
 { BYTES, F(st_wc_bytes), F(st_wc_mbytes), -1, "Log bytes written since last checkpoint" },
 { INT32, F(st_wcount),            -1, -1, "Total log writes" },
 { INT32, F(st_wcount_fill),       -1, -1, "Total log writes due to overflow" },
 { INT32, F(st_scount),            -1, -1, "Total log flushes" },
 { INT32, F(st_region_nowait),     -1, -1, "Region locks granted without waiting" },
 { INT32, F(st_region_wait),       -1, -1, "Region locks granted after waiting" },
 { INT32, F(st_cur_file),          -1, -1, "Current log file number" },
 { INT32, F(st_cur_offset),        -1, -1, "Current log file offset" },
 { INT32, F(st_disk_file),         -1, -1, "Known on disk log file number" },
 { INT32, F(st_disk_offset),       -1, -1, "Known on disk log file offset" },
 { BYTES, F(st_regsize),           -1, -1, "Log region size" },
 { INT32, F(st_maxcommitperflush), -1, -1, "Max number of commits in a flush"},
 { INT32, F(st_mincommitperflush), -1, -1, "Min number of commits in a flush"},
 { END, -1, -1, -1, NULL }
};

static int display_log(void)
{
    DB_LOG_STAT *sp;

    if(OVDBenv->log_stat(OVDBenv, &sp, 0) != 0)
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
 { BYTES, F(st_bytes), -1, F(st_gbytes), "Total cache size"},
 { INT32, F(st_ncache),     -1, -1,       "Number of caches"},
 { INT32, F(st_regsize),    -1, -1,       "Pool individual cache size"},
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

    if(OVDBenv->memp_stat(OVDBenv, &gsp, &fsp, 0) != 0)
	return 1;

    display_heading("Memory Pool Statistics");
    display_data(gsp, MEM_tab);

    if(all) {
	DB_MPOOL_FSTAT **p = fsp;

	start_table("Per-database Memory Pool Statistics", MEMF_tab);
	for(; p != NULL && *p != NULL; ++p) {
	    display_row(*p, MEMF_tab);
	}
	end_table();
    }

    free(fsp);
    free(gsp);
    return 0;
}

static int txn_compare(const void *a, const void *b)
{
    if (((const DB_TXN_ACTIVE *)a)->txnid > ((const DB_TXN_ACTIVE *)b)->txnid)
	return 1;
    if (((const DB_TXN_ACTIVE *)a)->txnid < ((const DB_TXN_ACTIVE *)b)->txnid)
	return -1;
    return 0;
}

#undef F
#define F(f) OFFSETOF(DB_TXN_STAT, f)

static struct datatab TXN_tab[] = {
 { LSN, F(st_last_ckp),     -1, -1, "File/offset for last checkpoint LSN" },
 { TIME, F(st_time_ckp),    -1, -1, "Checkpoint timestamp" },
 { HEX32, F(st_last_txnid), -1, -1, "Last transaction ID allocated" },
 { INT32, F(st_maxtxns),    -1, -1, "Maximum active transactions possible" },
 { INT32, F(st_nactive),    -1, -1, "Active transactions" },
 { INT32, F(st_nrestores),  -1, -1, "Restored transactions after recovery" },
 { INT32, F(st_maxnactive), -1, -1, "Maximum active transactions" },
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
 { INT32, F(parentid), -1, -1, "Parent Transaction ID" },
 { LSN,   F(lsn),      -1, -1, "Initial LSN file/offset" },
 { END, -1, -1, -1, NULL }
};

static int display_txn(void)
{
    DB_TXN_STAT *sp;
    u_int32_t i;

    if(OVDBenv->txn_stat(OVDBenv, &sp, 0) != 0)
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

static int display_ver(void)
{
    if(html) puts("<p>");
    printf("ovdb data version: %d\n", ovdb_data_ver);
    if(html) puts("<br>");
    printf("Berkeley DB version: %s\n", db_version(NULL,NULL,NULL));
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
 { INT32, F(bt_nkeys), -1, -1, "Unique keys in the tree" },
 { INT32, F(bt_ndata), -1, -1, "Data items in the tree" },
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

 { INT32, F(bt_free), -1, -1, "Pages on the free list" },
 { END, -1, -1, -1, NULL }
};

static int display_btree(DB *db)
{
    DB_BTREE_STAT *sp;

    if(db->stat(db, NULL, &sp, 0))
	return 1;

    display_heading("Btree Statistics");
    display_data(sp, BTREE_tab);

    free(sp);
    return 0;
}


#undef F
#define F(f) OFFSETOF(DB_HASH_STAT, f)

static struct datatab HASH_tab[] = {
 { HEX32, F(hash_magic), -1, -1, "Hash magic number" },
 { INT32, F(hash_version), -1, -1, "Hash version number" },
 { INT32, F(hash_pagesize), -1, -1, "Database page size" },
 { INT32, F(hash_nkeys), -1, -1, "Keys in the database" },
 { INT32, F(hash_ndata), -1, -1, "Data items in the database" },
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

static int display_hash(DB *db UNUSED)
{
    DB_HASH_STAT *sp;

    if(db->stat(db, NULL, &sp, 0))
	return 1;

    display_heading("Hash Information");
    display_data(sp, HASH_tab);

    return 0;
}

static int display_db(char *dbfile)
{
    int ret;
    DB *db;

    if(db_create(&db, OVDBenv, 0))
	return 1;
    if(db->open(db, NULL, dbfile, NULL, DB_UNKNOWN, DB_RDONLY, 0))
	return 1;

    switch(db->type) {
    case DB_BTREE:
    case DB_RECNO:
	ret = display_btree(db);
	break;
    case DB_HASH:
	ret = display_hash(db);
	break;
    default:
        ret = 1;
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
    if (strlen(str) == (size_t)(c - str + 1)) {
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

static void htwrite(char *data, int len)
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
    message_program_name = "ovdb_stat";

    if (!innconf_read(NULL))
        exit(1);

    if(!ovdb_check_user())
        die("command must be run as runasuser user");
    if(!ovdb_getlock(OVDB_LOCK_ADMIN))
        sysdie("cannot lock database");
    if(!ovdb_open(OV_READ|OVDB_SERVER))
        sysdie("cannot open overview; check syslog for OVDB messages");

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
            warn("option -%c requires an argument", optopt);
	    err++;
	    break;
	case '?':
            warn("unrecognized option -%c", optopt);
	    err++;
	    break;
	}
    }
    if(!gotone) {
	err++;
    } else if(optind == argc && needng) {
        warn("missing newsgroup argument(s)");
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
                s = ovdb_opensearch(argv[o], 1, 0xffffffff);
		if (s != NULL) {
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
			printf("<td>%ld<td>%ld<td>%d", low, high, count);
		    else
			printf("%s:    counted: low: %ld, high: %ld, count: %d\n",
				argv[o], low, high, count);
		}
	    }
	    if(getinfo) {
		int ret;
		struct groupinfo gi;

                ret = ovdb_getgroupinfo(argv[o], &gi, false, NULL, 0);
		if (ret != 0) {
                    warn("%s: ovdb_getgroupinfo error: %s", argv[o],
                         db_strerror(ret));
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
			printf("<td>%s<td>%lu", myctime(&gi.expired),
                               (unsigned long) gi.expiregrouppid);
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
			printf("%s: by process id: %lu\n", argv[o],
                               (unsigned long) gi.expiregrouppid);
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
            s = ovdb_opensearch(argv[o], start, stop);
	    if (s != NULL) {
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

#endif /* HAVE_BDB */

