/* $Id$ */

/* Locks the files given on the command line into memory using mlock.
   This code has only been tested on Solaris and may not work on other
   platforms.

   Contributed by Alex Kiernan <alexk@demon.net>.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stropts.h>

struct mlock {
    const char *path;
    struct stat st;
    void *base;
};

char *progname;

void
lock_files(struct mlock *ml)
{
    int i;

    for (; ml->path != NULL; ++ml) {
	int fd;

	fd = open(ml->path, O_RDWR);
	if (fd == -1) {
	    fprintf(stderr, "%s: can't open `%s' - %s\n",
		    progname, ml->path, strerror(errno));
	}
	else {
	    struct stat st;

	    /* check if size, inode or device of the path have
	     * changed, if so unlock the previous file & lock the new
	     * one */
	    if (fstat(fd, &st) != 0) {
		fprintf(stderr, "%s: can't stat `%s' - %s\n",
			progname, ml->path, strerror(errno));
	    }
	    else if (ml->st.st_ino != st.st_ino ||
		     ml->st.st_dev != st.st_dev ||
		     ml->st.st_size != st.st_size) {
		void *p;

		if (ml->base != MAP_FAILED)
		    munmap(ml->base, ml->st.st_size);

		/* free everything here, so in case of failure we try
		 * again next time */
		ml->st.st_ino = 0;
		ml->st.st_dev = 0;
		ml->st.st_size = 0;

		ml->base = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE,
				MAP_SHARED, fd, 0);

		if (p == MAP_FAILED) {
		    fprintf(stderr, "%s: can't mmap `%s' - %s\n",
			    progname, ml->path, strerror(errno));
		}
		else {
		    if (mlock(ml->base, st.st_size) != 0) {
			fprintf(stderr, "%s: can't mlock `%s' - %s\n",
				progname, ml->path, strerror(errno));
		    }
		    else {
			ml->st = st;
		    }
		}
	    }
	}
	close (fd);
    }
}

int
main(int argc, char *argv[])
{
    int didit = 0;
    struct mlock *ml;
    int i;

    progname = *argv;

    /* construct list of pathnames which we're to operate on, zero out
     * the "cookies" so we lock it in core first time through */
    ml = malloc(argc * sizeof ml);
    for (i = 0; --argc; ++i) {
	int fd;

	ml[i].path = *++argv;
	ml[i].st.st_ino = 0;
	ml[i].st.st_dev = 0;
	ml[i].st.st_size = 0;
	ml[i].base = MAP_FAILED;
    }
    ml[i].path = NULL;

    /* loop over the list of paths, sleeping 60s between iterations */
    for (;;) {
	lock_files(ml);
	poll(NULL, 0, 60000);
    }
    return EX_OSERR;
}
