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
    char		*addr;		/* mmapped address */
    OFFSET_T		size;		/* overview size */
    OFFSET_T		newsize;	/* new overview size */
    OFFSET_T		offset;		/* overview offset */
    OFFSET_T		newoffset;	/* newoverview offset */
    struct _UNIOVER	*next;
} UNIOVER;

#endif /* __OVERVIEW_H__ */
