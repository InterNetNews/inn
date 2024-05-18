-- ovsqlite schema version 1


create table misc (
    key text
        primary key,
    value
        not null
) without rowid;

-- The misc table holds various settings.
--
-- These values are set only on database creation:
--     * 'version'
--         The value is an integer specifying the schema version.
--     * 'compress'
--         The value is a boolean integer specifying whether or not
--         overview text is stored with zlib compression.
--     * 'basedict'
--         Only present if compress is true.
--         The value is a blob specifying a common prefix for the
--         dictionaries used with overview text compression.
--         It contains some fragments commonly found in overview text
--         and ends with "\tXref: $pathhost ".  For the exact contents,
--         see basedict_format in ovsqlite-server.c.
--
-- Currently, no values that vary over the database lifetime exist.


create table groupinfo (
    groupid integer
        primary key,
    low integer
        not null
        default 1,
    high integer
        not null
        default 0,
    "count" integer
        not null
        default 0,
    expired integer
        not null
        default 0,
    deleted integer
        not null
        default 0,
    groupname blob
        not null,
    flag_alias blob
        not null,

    unique (deleted, groupname));

-- Newsgroup names aren't guaranteed to use any particular encoding,
-- so they have to be stored as blobs rather than text.
--
-- The "flag_alias" column contains the flag in the first byte
-- and any alias group name in the remaining bytes.
--
-- The "expired" column contains the time_t of the last expireover run
-- that included the group; this is used to detect forgotten groups
-- at the end of the run.
--
-- When a group is removed, it is marked as deleted and the actual
-- deletion is deferred until the next expiration.
-- The "deleted" column is not a boolean; it is set to an unused positive
-- value on removal.  This supports repeated removal and re-addition of
-- the same group without any intervening expiration.


create table artinfo (
    groupid integer
        references groupinfo (groupid)
            on update cascade
            on delete restrict,
    artnum integer,
    arrived integer
        not null,
    expires integer
        not null,
    token blob
        not null,
    overview blob
        not null,

    primary key (groupid, artnum)
) without rowid;

-- The "arrived" and "expires" columns contain time_t values.
--
-- The "token" column contains TOKEN values in raw 18-byte format.
--
-- The "overview" column contains the complete overview data
-- including the terminating CRLF.
--
-- Without compression, this is stored unprocessed.
--
-- With compression, a variable width integer specifying
-- the uncompressed data size is followed by the compressed data.
-- In the rare case when compression wouldn't save any space,
-- a data size of 0 is followed by the uncompressed data.
--
-- The size is stored in big-endian order and its width is encoded
-- in the first byte: for width w, the w-1 most significant bits are ones
-- and the next bit is a zero, leaving w*7 bits for the value itself.
-- There are no redundant encodings; width 1 is used for values 0 through 127,
-- width 2 for values 128 through 16511, and so on.
--
-- Let's illustrate it.  Values in the range 0-127 are encoded in one byte
-- with 0 in the most significant bit:
--     encoding: 0aaaaaaa
--     value:    aaaaaaa
--
-- Values in the range 128-16511 encoded in two bytes with 10 in the two most
-- significant bits of the first byte:
--     encoding: 10aaaaaa bbbbbbbb
--     value:    aaaaaabbbbbbbb + 128
--
-- And so on for three, four, and five bytes:
--     encoding: 110aaaaa bbbbbbbb cccccccc
--     value:    aaaaabbbbbbbbcccccccc + 16512
--
--     encoding: 1110aaaa bbbbbbbb cccccccc dddddddd
--     value:    aaaabbbbbbbbccccccccdddddddd + 2113664
--
--     encoding: 11110aaa bbbbbbbb cccccccc dddddddd eeeeeeee
--     value:    aaabbbbbbbbccccccccddddddddeeeeeeee + 270549120
--
-- Compression uses a dictionary formed by concatenating the common
-- prefix (stored in the misc table) with "$groupname:$artnum\r\n".


-- .getpagesize
pragma page_size;

