/* Quick and Dirty Hack to reset a CNFS buffer without having to DD the
 * Entire Thing from /dev/zero again. */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
    int fd;
    int i, j;
    char buf[512];
    struct stat st;
    int numwr;

    bzero(buf, sizeof(buf));
    for (i = 1; i < argc; i++) {
	if ((fd = open(argv[i], O_RDWR, 0664)) < 0)
	    fprintf(stderr, "Could not open file %s: %s\n", argv[i], strerror(errno));
	else {
	    if (fstat(fd, &st) < 0) {
		fprintf(stderr, "Could not stat file %s: %s\n", argv[i], strerror(errno));
	    } else {
		/* each bit in the bitfield is 512 bytes of data.  Each byte
		 * has 8 bits, so calculate as 512 * 8 bytes of data, plus
		 * fuzz. buf has 512 bytes in it, therefore containing data for
		 * (512 * 8) * 512 bytes of data. */
		numwr = (st.st_size / (512*8) / sizeof(buf)) + 50;
		printf("File %s: %u %u\n", argv[i], st.st_size, numwr);
		for (j = 0; j < numwr; j++) {
		    if (!(j % 100))
			printf("\t%d/%d\n", j, numwr);
		    write(fd, buf, sizeof(buf));
		}
	    }
	    close(fd);
	}
    }
}
