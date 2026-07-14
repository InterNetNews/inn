-- Writer prepared statements for hissqlite (used by innd and the offline
-- tools: makehistory, prunehistory, the migration converter).
--
-- Implementation follows ovsqlite's limits/patterns; the operator-facing
-- durability contract matches dbz (lose recent on power loss, never corrupt,
-- peer resends).  WAL + synchronous=NORMAL: a commit is durable across an
-- application crash; a power loss can lose transactions since the last
-- checkpoint, but the DB is never corrupted -- parity with dbz's periodic
-- msync, not a regression.

pragma journal_mode = wal;
pragma synchronous = normal;
pragma busy_timeout = 10000;
pragma foreign_keys = off;

-- Writer cache + mmap (pragma cache_size / mmap_size) are applied by
-- hissqlite.c from the hissqlitecachesize and hissqlitemmapsize inn.conf
-- parameters after this section runs.  innd is a single process, so a larger
-- per-connection page cache is affordable; mmap lets reads come from the
-- shared OS page cache and keeps the random-MD5 B-tree hot.

-- Writes are NOT batched: each statement autocommits (one implicit transaction
-- per write).  Under synchronous=NORMAL a commit is a buffered WAL append, not
-- an fsync, so there is no fsync cost to amortise; batching would only hold
-- the write lock across the batch and starve the other writer (expire) in the
-- two-writer model.  Batching is reserved for the offline bulk load
-- (hissqlite-convert.c).  See hissqlite.c.

-- HISlookup: Message-ID -> token.  A row with token IS NULL (remembered) is
-- reported as not found, matching hisv6: HISlookup means "have the article";
-- HIScheck is the existence test that also counts remembered entries.
-- .lookup
select arrived, posted, expires, token
    from hist
    where hash = ?1;

-- HIScheck: existence only (real OR remembered both count -> refuse
-- re-offers).  his.c keeps an in-memory existence cache in front of this;
-- this is the authoritative fallback.
-- .check
select 1 from hist where hash = ?1;

-- HISwrite: a real, stored article.  The accept path runs HIScheck first, so
-- this is normally a fresh insert.  DO NOTHING on a hash conflict keeps the
-- existing row rather than failing: a duplicate Message-ID during a
-- makehistory rebuild must warn and continue, not abort the whole rebuild,
-- as in hisv6 (hissqlite.c detects the no-op via sqlite3_changes()).
-- .write
insert into hist(hash, arrived, posted, expires, token)
    values(?1, ?2, ?3, ?4, ?5)
    on conflict(hash) do nothing;

-- HISremember: record a seen/rejected Message-ID with no token.  Must NEVER
-- downgrade an existing real article to remembered, hence DO NOTHING.
-- .remember
insert into hist(hash, arrived, posted, expires, token)
    values(?1, ?2, ?3, 0, null)
    on conflict(hash) do nothing;

-- HISreplace: the only token-changing op (prunehistory real->remembered by
-- passing a null token; remembered->real upgrade).  Full upsert.
-- .replace
insert into hist(hash, arrived, posted, expires, token)
    values(?1, ?2, ?3, ?4, ?5)
    on conflict(hash) do update set
        arrived = excluded.arrived,
        posted  = excluded.posted,
        expires = excluded.expires,
        token   = excluded.token;

-- HISwalk: full iteration (feeds the bloom build, dump/migration).  Full
-- clustered B-tree scan: each leaf page read once, semi-sequential.
-- .walk
select hash, arrived, posted, expires, token from hist;

-- HISexpire pass 1: iterate token-bearing entries so the policy callback can
-- decide keep / transition-to-remembered.  (expire.ctl needs every entry
-- evaluated, as hisv6 does.)  Keyset-paginated by the hash PK: ?1 is the last
-- hash of the previous page (empty blob for the first), ?2 the page size.  The
-- caller streams pages so the read snapshot is released between them; the PK
-- range scan is in hash order, and rows transitioned to token=NULL drop out.
-- .expire_scan
select hash, arrived, posted, expires, token
    from hist
    where token is not null
        and hash > ?1
    order by hash
    limit ?2;

-- HISexpire pass 1 action: transition a real article to remembered.
-- .transition_remember
update hist set token = null, expires = 0 where hash = ?1;

-- HISexpire pass 1 action: callback kept the article but rewrote the token.
-- .update_token
update hist set token = ?2, expires = ?3 where hash = ?1;

-- HISexpire pass 2: delete remembered entries older than the /remember/
-- posting-time threshold (indexed by hist_remember).  Bounded to a chunk per
-- step (LIMIT) so each autocommit holds the write lock only briefly; the
-- caller loops until a step deletes nothing.
-- .expire_remembered
delete from hist where hash in (
    select hash from hist
        where token is null
            and (case when posted > 0 then posted else arrived end) < ?1
        limit 10000);
