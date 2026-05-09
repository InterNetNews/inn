/*
**  Helpers for the expire-tombstone log files.
**
**  These routines are shared between the expire binary (which consumes
**  the logs to skip per-article SMretrieve(RETR_STAT) calls) and the
**  test suite that exercises the consumption logic directly.
**
**  See the inn.conf manual page under "expiretombstone" for the file
**  semantics:
**    - ${pathdb}/expireover.tombstone : written by expireover/expirerm
**      via OVEXPremove, atomic .NEW -> final rename
**    - ${pathdb}/cancels.tombstone    : appended continuously by innd
**      and sm via SMcanceltombstone() for cancels outside the
**      expireover/expirerm pipeline
*/

#ifndef INN_TOMBSTONE_H
#define INN_TOMBSTONE_H

#include "inn/hashtab.h"
#include "inn/portable-macros.h"
#include "inn/portable-stdbool.h"
#include "inn/storage.h"

/*
**  Format marker written at the top of every tombstone file.
**  Readers skip leading comment lines silently so the marker can
**  evolve without breaking earlier consumers.
*/
#define TOMBSTONE_HEADER "# inn-tombstone v1\n"

BEGIN_DECLS

/*
**  Create a hashset suited to holding TOKEN keys (each entry is a
**  TOKEN allocated by the caller; the hashset owns and frees them).
**  size is the initial bucket hint; the hash auto-expands.
*/
struct hash *tombstone_hash_create(size_t size);

/*
**  Read tombstone entries from path into the hashset.  Each non-blank
**  line is parsed with TextToToken and inserted; malformed lines are
**  skipped with a warning.  Duplicates are dropped silently.  Returns
**  the number of valid entries seen, or 0 if the file does not exist
**  (ENOENT is silent; other open errors emit syswarn).
**
**  If out_error is non-NULL, *out_error is set to true when the read
**  was incomplete (open failed for a non-ENOENT reason or fgets hit a
**  stream error mid-file) and to false otherwise.  Callers that cache
**  freshness based on file mtime should not treat a partial read as
**  authoritative.  ENOENT does not set the flag because an absent
**  file is a successful "no entries" read.
*/
unsigned long tombstone_read(struct hash *h, const char *path,
                             bool *out_error);

/*
**  Atomically snapshot a continuously-appended tombstone file by
**  renaming it to "${path}.processing" under an exclusive POSIX lock.
**  This serializes against concurrent appenders that hold the same
**  lock (e.g., SMcanceltombstone()), so any in-progress write either
**  completes before the rename or proceeds against the renamed inode.
**
**  Returns the snapshot path (caller must free and unlink after
**  consumption) or NULL if the source file does not exist or cannot
**  be renamed.
*/
char *tombstone_rename_for_processing(const char *path);

/*
**  Look up a TOKEN in the hashset.  Returns true if present.
*/
bool tombstone_present(struct hash *h, const TOKEN *token);

/*
**  Ensure path exists and starts with TOMBSTONE_HEADER under an
**  exclusive POSIX lock.  Idempotent: a file whose first bytes
**  already match TOMBSTONE_HEADER is left untouched.  Otherwise the
**  helper shifts any existing content right by sizeof(TOMBSTONE_
**  HEADER)-1 bytes via a chunked end-to-start pread/pwrite walk on
**  the same fd, then writes the header at offset 0.  Working
**  memory is bounded by an internal chunk size regardless of file
**  size, so a pathological never-consumed file cannot OOM.
**
**  An empty file (or one freshly created by this call) just gets
**  the header written at offset 0.  An appender that raced a
**  cancel into a new live file between the consumer's unlink and
**  this call has its content preserved verbatim below the header.
**  Best-effort: failures log via syswarn but do not raise.
**
**  Called by expire after consuming cancels.tombstone.processing
**  to leave a header-only file behind so the nnrpd fast path
**  remains active through quiet inter-cancel periods.
*/
void tombstone_ensure_header(const char *path);

END_DECLS

#endif /* INN_TOMBSTONE_H */
