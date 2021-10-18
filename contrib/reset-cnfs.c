/* Quick and Dirty Hack to reset a CNFS buffer without having to DD the
 * Entire Thing from /dev/zero again. */
#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>


int
main(int argc, char *argv[])
{
    int fd;
    int i;
    char buf[512];
#ifdef DO_LARGEFILES
    struct stat64 st;
#else
    struct stat st;
#endif
    size_t j, numwr;
    bool status = true;

    memset(buf, 0, sizeof(buf));
    for (i = 1; i < argc; i++) {
#ifdef DO_LARGEFILES
        if ((fd = open(argv[i], O_LARGEFILE | O_RDWR, 0664)) < 0) {
#else
        if ((fd = open(argv[i], O_RDWR, 0664)) < 0) {
#endif
            fprintf(stderr, "Could not open file %s: %s\n", argv[i], strerror(errno));
            status = false;
        } else {
#ifdef DO_LARGEFILES
            if (fstat64(fd, &st) < 0) {
#else
            if (fstat(fd, &st) < 0) {
#endif
                fprintf(stderr, "Could not stat file %s: %s\n", argv[i], strerror(errno));
                status = false;
            } else {
                /* Each bit in the bitfield is 512 bytes of data.  Each byte
                 * has 8 bits, so calculate as 512 * 8 bytes of data, plus
                 * fuzz.  buf has 512 bytes in it, therefore containing data
                 * for (512 * 8) * 512 bytes of data. */
                numwr = (st.st_size / (512*8) / sizeof(buf)) + 50;
                printf("File %s: %lu %lu\n", argv[i],
                       (unsigned long) st.st_size, (unsigned long) numwr);
                status = true;
                for (j = 0; j < numwr; j++) {
                    if (!(j % 100)) {
                        printf("\t%lu/%lu\n", (unsigned long) j,
                               (unsigned long) numwr);
                    }
                    if (write(fd, buf, sizeof(buf)) < (ssize_t) sizeof(buf)) {
                        fprintf(stderr, "Could not write to file %s: %s\n",
                                argv[i], strerror(errno));
                        status = false;
                        break;
                    }
                }
            }
            close(fd);
        }
    }

    if (status) {
        exit(0);
    } else {
        exit(1);
    }
}
