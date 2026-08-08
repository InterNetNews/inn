/*  Test suite for CAF file layout validation and cleaning.
**
**  Written by Kevin Bowling in 2026.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "inn/libinn.h"
#include "tap/basic.h"

#define CAF_INNARDS 1
#include "../../storage/timecaf/caf.h"

extern int CAFClean(char *, int, double);


static CAFHEADER
test_header(void)
{
    CAFHEADER head;

    memset(&head, 0, sizeof(head));
    memcpy(head.Magic, CAF_MAGIC, CAF_MAGIC_LEN);
    head.Low = 1;
    head.High = 1;
    head.NumSlots = 10;
    head.BlockSize = CAF_DEFAULT_BLOCKSIZE;
    head.FreeZoneIndexSize = head.BlockSize - sizeof(CAFHEADER);
    head.FreeZoneTabSize =
        head.FreeZoneIndexSize + head.BlockSize * head.FreeZoneIndexSize * 8;
    head.StartDataBlock =
        CAFRoundOffsetUp(sizeof(CAFHEADER) + head.FreeZoneTabSize
                             + head.NumSlots * sizeof(CAFTOCENT),
                         head.BlockSize);
    return head;
}


static void
write_test_file(const char *path, const CAFHEADER *head,
                const CAFTOCENT *entry, off_t length)
{
    int fd;

    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0600);
    if (fd < 0)
        sysbail("cannot create %s", path);
    if (xwrite(fd, head, sizeof(*head)) != (ssize_t) sizeof(*head))
        sysbail("cannot write CAF header");
    if (entry != NULL) {
        if (lseek(fd, sizeof(*head) + head->FreeZoneTabSize, SEEK_SET) < 0)
            sysbail("cannot seek to CAF TOC");
        if (xwrite(fd, entry, sizeof(*entry)) != (ssize_t) sizeof(*entry))
            sysbail("cannot write CAF TOC entry");
    }
    if (ftruncate(fd, length) < 0)
        sysbail("cannot set CAF test file size");
    close(fd);
}


int
main(void)
{
    static char path[] = "caf-test";
    CAFHEADER head, read_head;
    CAFBITMAP *bitmap;
    CAFTOCENT entry, *toc;
    ARTNUM art;
    size_t length, bitmap_size;
    uintmax_t bitmap_bytes;
    off_t bitmap_offset, bitmap_block, bitmap_end;
    unsigned int bitmap_index;
    int error, fd, status;
    char bit = 1;

    plan(23);
    unlink(path);
    memset(&entry, 0, sizeof(entry));

    head = test_header();
    head.High = head.Low + head.NumSlots;
    write_test_file(path, &head, &entry, head.StartDataBlock);
    fd = CAFOpenArtRead(path, head.Low, &length);
    error = caf_error;
    is_int(-1, fd, "reject article lookup with High outside NumSlots");
    is_int(CAF_ERR_BADFILE, error, "invalid article range is a bad file");
    if (fd >= 0)
        close(fd);

    head = test_header();
    head.StartDataBlock = sizeof(CAFHEADER);
    write_test_file(path, &head, &entry,
                    sizeof(CAFHEADER) + sizeof(CAFTOCENT));
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    error = caf_error;
    is_int(-1, fd, "reject a TOC overlapping the data region");
    is_int(CAF_ERR_BADFILE, error, "overlapping TOC is a bad file");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    head.FreeZoneIndexSize--;
    write_test_file(path, &head, &entry, head.StartDataBlock);
    art = head.Low;
    status = CAFRemoveMultArts(path, 1, &art);
    error = caf_error;
    is_int(-1, status, "cancel rejects inconsistent bitmap metadata");
    is_int(CAF_ERR_BADFILE, error, "invalid cancel metadata is a bad file");

    head = test_header();
    write_test_file(path, &head, NULL, sizeof(CAFHEADER));
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    error = caf_error;
    is_int(-1, fd, "reject a TOC truncated by end of file");
    is_int(CAF_ERR_BADFILE, error, "truncated TOC is a bad file");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    write_test_file(path, &head, &entry, head.StartDataBlock);
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    ok(fd >= 0, "accept a valid TOC layout");
    ok(fd >= 0 && toc[0].Size == 0, "read the valid TOC entry");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    head.FreeZoneIndexSize--;
    write_test_file(path, &head, &entry, head.StartDataBlock);
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    error = caf_error;
    is_int(-1, fd, "reject inconsistent free bitmap metadata");
    is_int(CAF_ERR_BADFILE, error, "invalid bitmap layout is a bad file");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    head.BlockSize = 0;
    write_test_file(path, &head, &entry, head.StartDataBlock);
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    ok(fd >= 0, "accept a legacy zero block size");
    is_int(CAF_DEFAULT_BLOCKSIZE, read_head.BlockSize,
           "normalize a legacy zero block size");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    entry.Offset = head.StartDataBlock + 100;
    entry.Size = 10;
    write_test_file(path, &head, &entry, head.StartDataBlock);
    status = CAFClean(path, 0, 10.0);
    is_int(0, status, "no-op cleaning tolerates a torn article entry");
    ok(access(path, F_OK) == 0, "no-op cleaning preserves the CAF file");

    memset(&entry, 0, sizeof(entry));
    head = test_header();
    entry.Offset = head.StartDataBlock;
    entry.Size = 1;
    head.Free = 2;
    write_test_file(path, &head, &entry, head.StartDataBlock + 1);
    status = CAFClean(path, 0, 10.0);
    is_int(0, status, "cleaning repairs an inflated free-space counter");
    fd = CAFOpenReadTOC(path, &read_head, &toc);
    ok(fd >= 0 && read_head.Free == 0 && toc[0].Size == 1,
       "repaired CAF retains its article and resets free space");
    if (fd >= 0) {
        close(fd);
        free(toc);
    }

    head = test_header();
    entry.Offset = head.StartDataBlock;
    entry.Size = 1;
    head.StartDataBlock += head.BlockSize;
    write_test_file(path, &head, &entry, head.StartDataBlock - head.BlockSize);
    status = CAFClean(path, 0, 10.0);
    is_int(-1, status, "cleaning rejects a non-empty CAF past EOF");
    ok(access(path, F_OK) == 0, "invalid non-empty CAF is preserved");

    memset(&entry, 0, sizeof(entry));
    head = test_header();
    head.StartDataBlock += head.BlockSize;
    write_test_file(path, &head, &entry, head.StartDataBlock - head.BlockSize);
    status = CAFClean(path, 0, 10.0);
    is_int(0, status, "cleaning removes an empty CAF with a torn boundary");
    ok(access(path, F_OK) < 0, "empty orphan CAF is unlinked");

    /* Exercise a bitmap seek beyond UINT_MAX.  The old unsigned expression
       wrapped this offset back into the header before conversion to off_t. */
    bitmap_size = 65536;
    bitmap_index = 65535;
    bitmap_bytes = ((uintmax_t) bitmap_index + 1) * bitmap_size;
    bitmap_offset = (off_t) bitmap_bytes;
    bitmap_block = (off_t) ((uintmax_t) bitmap_index * bitmap_size);
    bitmap_end = (off_t) (bitmap_bytes + bitmap_size);
    if (bitmap_offset < 0 || bitmap_block < 0 || bitmap_end < 0
        || (uintmax_t) bitmap_offset != bitmap_bytes
        || (uintmax_t) bitmap_end != bitmap_bytes + bitmap_size) {
        skip("off_t cannot represent the bitmap seek test");
    } else {
        fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0600);
        if (fd < 0 || ftruncate(fd, bitmap_end) < 0
            || lseek(fd, bitmap_offset, SEEK_SET) < 0
            || xwrite(fd, &bit, 1) != 1)
            sysbail("cannot create sparse bitmap seek test");
        bitmap = xcalloc(1, sizeof(*bitmap));
        bitmap->StartDataBlock = 0;
        bitmap->MaxDataBlock = bitmap_block + (off_t) bitmap_size;
        bitmap->BytesPerBMB = bitmap_size;
        bitmap->BlockSize = bitmap_size;
        bitmap->NumBMB = bitmap_index + 1;
        bitmap->Blocks = xcalloc(bitmap->NumBMB, sizeof(*bitmap->Blocks));
        bitmap->Bits = xcalloc(1, 1);
        is_int(1, CAFIsBlockFree(bitmap, fd, bitmap_block),
               "bitmap block seek does not wrap in unsigned arithmetic");
        CAFDisposeBitmap(bitmap);
        close(fd);
    }

    unlink(path);
    return 0;
}
