-- hissqlite schema version 1
--
-- Run once at database creation.  Schema 1: a single WITHOUT ROWID table
-- clustered on the 16-byte MD5 of the Message-ID, so a Message-ID -> token
-- lookup is one clustered-leaf access (the token lives in the leaf).
--
-- Three logical states of a row:
--     token IS NOT NULL  -> a real, stored article
--     token IS NULL      -> a "remembered" entry (article seen/rejected or
--                           expired; kept so it is not re-accepted)
--     no row             -> never seen
--
-- The token-NULL state is a first-class lifecycle stage, NOT a tombstone for
-- deletion: when a real article reaches its retention it is UPDATEd to
-- remembered (token=NULL), and only DELETEd once the remembered entry passes
-- the /remember/ posting-time threshold.  See hissqlite.c hissqlite_expire().

-- Page size (pragma page_size) is applied by hissqlite.c from the
-- hissqlitepagesize inn.conf parameter before this DDL runs: it must be set
-- before any table exists (changing it later needs a VACUUM).  The default,
-- 4096, matches the common filesystem block (and a ZFS recordsize=4k dataset),
-- so a random single-row insert or a point lookup touches exactly one block --
-- no read-modify-write, no super-block read amplification.  Benchmarking the
-- random-MD5-key workload measured 4k beating 8k on both writes and reads at
-- equal footprint.

create table hist (
    hash    blob not null primary key, -- HashMessageID(): 16-byte MD5
    arrived integer not null,          -- time_t
    posted  integer,                   -- time_t; 0/NULL if unknown
    expires integer,                   -- time_t article expiry; 0/NULL = none
    token   blob                       -- raw TOKEN (sizeof TOKEN);
                                       --                    NULL = remembered
) without rowid;

-- Find remember-threshold delete candidates (remembered -> gone): a partial
-- index over only the token-NULL rows, so expire pass 2 scans the remembered
-- entries without touching the real articles.  (Real-article expiry needs no
-- index: pass 1 evaluates every token-bearing row via the policy callback, in
-- clustered hash order -- see hissqlite_expire().)
create index hist_remember on hist(posted, arrived) where token is null;

-- Schema/version marker, mirroring ovsqlite's misc table.
create table misc (
    key   text not null primary key,
    value not null
) without rowid;

-- .get_version
select value from misc where key = 'version';

-- .set_version
insert into misc(key, value) values('version', ?1)
    on conflict(key) do update set value = excluded.value;
