/* $Id$ */

/* Locks the files given on the command line into memory using mlock.
   This code has only been tested on Solaris and may not work on other
   platforms.

   Contributed by Alex Kiernan <alexk@demon.net>.  */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/stropts.h>

struct mlock {
    const char *path;
    struct stat st;
    void *base;
    off_t offset;
    size_t length;
};

char *progname;

int flush = 0;
int interval = 60000;

void
inn_lock_files(struct mlock *ml)
{
    for (; ml->path != NULL; ++ml) {
	int fd;

	fd = open(ml->path, O_RDONLY);
	if (fd == -1) {
	    fprintf(stderr, "%s: can't open `%s' - %s\n",
		    progname, ml->path, strerror(errno));
	} else {
	    struct stat st;

	    /* check if size, inode or device of the path have
	     * changed, if so unlock the previous file & lock the new
	     * one */
	    if (fstat(fd, &st) != 0) {
		fprintf(stderr, "%s: can't stat `%s' - %s\n",
			progname, ml->path, strerror(errno));
	    } else if (ml->st.st_ino != st.st_ino ||
		     ml->st.st_dev != st.st_dev ||
		     ml->st.st_size != st.st_size) {
		if (ml->base != MAP_FAILED)
		    munmap(ml->base,
			   ml->length ? ml->length : ml->st.st_size);

		/* free everything here, so in case of failure we try
		 * again next time */
		ml->st.st_ino = 0;
		ml->st.st_dev = 0;
		ml->st.st_size = 0;

		ml->base = mmap(NULL,
				ml->length ? ml->length : st.st_size,
				PROT_READ,
				MAP_SHARED, fd, ml->offset);

		if (ml->base == MAP_FAILED) {
		    fprintf(stderr, "%s: can't mmap `%s' - %s\n",
			    progname, ml->path, strerror(errno));
		} else {
		    if (mlock(ml->base,
			      ml->length ? ml->length : st.st_size) != 0) {
			fprintf(stderr, "%s: can't mlock `%s' - %s\n",
				progname, ml->path, strerror(errno));
		    } else {
			ml->st = st;
		    }
		}
	    } else if (flush) {
		msync(ml->base, ml->length ? ml->length : st.st_size, MS_SYNC);
	    }
	}
	close (fd);
    }
}

static void
usage(void)
{
    fprintf(stderr,
	    "usage: %s [-f] [-i interval] file[@offset[:length]] ...\n",
	    progname);
    fprintf(stderr, "    -f\tflush locked bitmaps at interval\n");
    fprintf(stderr, "    -i interval\n\tset interval between checks/flushes\n");
}

int
main(int argc, char *argv[])
{
    struct mlock *ml;
    int i;

    progname = *argv;
    while ((i = getopt(argc, argv, "fi:")) != EOF) {
	switch (i) {
	case 'i':
	    interval = 1000 * atoi(optarg);
	    break;

	case 'f':
	    flush = 1;
	    break;

	default:
	    usage();
	    return EX_USAGE;	    
	}
    }
    argc -= optind;
    argv += optind;

    /* construct list of pathnames which we're to operate on, zero out
     * the "cookies" so we lock it in core first time through */
    ml = malloc((1 + argc) * sizeof ml);
    for (i = 0; argc--; ++i, ++argv) {
	char *at;
	off_t offset = 0;
	size_t length = 0;

	ml[i].path = *argv;
	ml[i].st.st_ino = 0;
	ml[i].st.st_dev = 0;
	ml[i].st.st_size = 0;
	ml[i].base = MAP_FAILED;
	
	/* if we have a filename of the form ...@offset:length, only
	 * map in that portion of the file */
	at = strchr(*argv, '@');
	if (at != NULL) {
	    char *end;

	    *at++ = '\0';
	    errno = 0;
	    offset = strtoull(at, &end, 0);
	    if (errno != 0) {
		fprintf(stderr, "%s: can't parse offset `%s' - %s\n",
			progname, at, strerror(errno));
		return EX_USAGE;
	    }
	    if (*end == ':') {
		at = end + 1;
		errno = 0;
		length = strtoul(at, &end, 0);
		if (errno != 0) {
		    fprintf(stderr, "%s: can't parse length `%s' - %s\n",
			    progname, at, strerror(errno));
		    return EX_USAGE;
		}
	    }
	    if (*end != '\0') {
		fprintf(stderr, "%s: unrecognised separator `%c'\n",
			progname, *end);
		return EX_USAGE;
	    }
	}
	ml[i].offset = offset;
	ml[i].length = length;	    
    }
    ml[i].path = NULL;

    /* loop over the list of paths, sleeping 60s between iterations */
    for (;;) {
	inn_lock_files(ml);
	poll(NULL, 0, interval);
    }
    return EX_OSERR;
}
