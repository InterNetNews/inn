/*  $Id$
*/

#ifndef INN_OV_H
#define INN_OV_H 1

#include <inn/defines.h>
#include "storage.h"
#include "inn/history.h"

BEGIN_DECLS

#define OV_READ  1
#define OV_WRITE 2

typedef enum {OVSPACE, OVSORT, OVCUTOFFLOW, OVGROUPBASEDEXPIRE, OVSTATICSEARCH, OVSTATALL, OVCACHEKEEP, OVCACHEFREE} OVCTLTYPE;
#define OV_NOSPACE 100
typedef enum {OVNEWSGROUP, OVARRIVED, OVNOSORT} OVSORTTYPE;
typedef enum {OVADDCOMPLETED, OVADDFAILED, OVADDGROUPNOMATCH} OVADDRESULT;

typedef struct _OVGE {
    bool	delayrm;	  /* append tokens to filename if true */
    bool	usepost;	  /* posting date is used to determine expiry
				     time if true */
    bool	quiet;		  /* statistics will be suppressed if true */
    bool	keep;		  /* keep article so long as any of crossposted
				     newsgroups is not expired if true */
    bool	earliest;	  /* purge article any of crossposted
				     newsgroups is expired if true */
    bool	ignoreselfexpire; /* purge article even if storing method has
				     self expiry */
    char	*filename;	  /* used to append tokens to this file if
				     delayrm is true */
    time_t	now;		  /* used as current time */
    float	timewarp;	  /* used to bias expiry time */
} OVGE;

extern bool	OVstatall;
bool OVopen(int mode);
bool OVgroupstats(char *group, int *lo, int *hi, int *count, int *flag);
bool OVgroupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag);
bool OVgroupdel(char *group);
OVADDRESULT OVadd(TOKEN token, char *data, int len, time_t arrived, time_t expires);
bool OVcancel(TOKEN token);
void *OVopensearch(char *group, int low, int high);
bool OVsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived);
void OVclosesearch(void *handle);
bool OVgetartinfo(char *group, ARTNUM artnum, TOKEN *token);
bool OVexpiregroup(char *group, int *lo, struct history *h);
bool OVctl(OVCTLTYPE type, void *val);
void OVclose(void);

/* Overview data manipulation functions. */
const struct cvector *overview_fields(void);
struct vector *overview_extra_fields(bool hidden);
struct buffer *overview_build(ARTNUM number, const char *article,
                              size_t length, const struct vector *extra,
                              struct buffer *);
bool overview_check(const char *data, size_t length, ARTNUM article);
int overview_index(const char *field, const struct vector *extra);
struct cvector *overview_split(const char *line, size_t length,
			       ARTNUM *number, struct cvector *vector);
char *overview_get_standard_header(const struct cvector *vector, unsigned int element);
char *overview_get_extra_header(const struct cvector *vector, const char *header);

/* Offsets into vectors for standard overview headers. */
#ifndef INN_OVERVIEW_H
enum {
    OVERVIEW_SUBJECT = 0,
    OVERVIEW_FROM,
    OVERVIEW_DATE,
    OVERVIEW_MESSAGE_ID,
    OVERVIEW_REFERENCES,
    OVERVIEW_BYTES,
    OVERVIEW_LINES,
    OVERVIEW_MAX
};
#endif

END_DECLS

#endif /* INN_OV_H */
