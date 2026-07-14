-- Read-only prepared statements for hissqlite (nnrpd and other readers).
--
-- Written by Kevin Bowling in 2026.
--
-- In WAL mode, readers open the SQLite file read-only and query it directly,
-- bypassing any writer process.

pragma query_only = 1;
pragma busy_timeout = 10000;

-- Reader cache + mmap (pragma cache_size / mmap_size) are applied by
-- hissqlite.c from the hissqlitereadercachesize and hissqlitemmapsize inn.conf
-- parameters after this section runs.  nnrpd runs one process PER connection,
-- so the per-connection cache is kept small by default; readers lean on the
-- shared OS page cache instead.

-- .lookup
select arrived, posted, expires, token
    from hist
    where hash = ?1;

-- .check
select 1 from hist where hash = ?1;

-- .walk
select hash, arrived, posted, expires, token from hist;
