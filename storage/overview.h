/*  $Id$
**
**  unified overview processing header
*/

#ifndef __OVERVIEW_H__
#define __OVERVIEW_H__

typedef enum {OVER_DONE, OVER_NULL, OVER_ERROR} OVERCONFIG;

typedef struct _UNIOVER {
    int			fd;		/* fd for this overview */
    int			newfd;		/* new fd for this overview */
    FILE		*fp;		/* FILE handler for this overview */
    FILE		*newfp;		/* new FILE handler for this overview */
    int			index;		/* Number of the overview index for this
					   subscription */
    int			numpatterns;	/* Number of patterns in patterns */
    char		**patterns;	/* Array of patterns to check against
					   the groups to determine if the
					   article should go to this index */
    char		*base;		/* mmapped address */
    OFFSET_T		len;		/* length of mmapped address */
    OFFSET_T		mappedoffset;	/* offset of mmapped address */
    OFFSET_T		size;		/* overview size */
    OFFSET_T		newsize;	/* new overview size */
    OFFSET_T		offset;		/* overview offset */
    OFFSET_T		newoffset;	/* newoverview offset */
    struct _UNIOVER	*next;
} UNIOVER;

#define MAXMMAPCONFIG	5
#define OVERMMAPLEN	(1048576 * 4)
#define OVERPAGESIZE	16384
typedef struct _OVERMMAP {
    UNIOVER		*config;
    unsigned int	refcount;
} OVERMMAP;

#endif /* __OVERVIEW_H__ */
