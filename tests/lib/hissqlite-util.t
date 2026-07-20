#! /bin/sh
#
# Smoke test for hissqlite-util -s, -c, and -v against a minimal history
# database.
#
# Written by Kevin Bowling in 2026.

count=1
printcount() {
    echo "$1 $count $2"
    count=$(expr $count + 1)
}

# runtests preserves its caller's working directory.  Normalize to the test
# build directory so this test works when runtests is called from either the
# repository root or the tests directory.
test_root=${C_TAP_BUILD:-}
if [ -z "$test_root" ]; then
    test_root=$(dirname "$0")
    test_root=$(cd "$test_root/.." && pwd)
fi
if ! cd "$test_root"; then
    echo "Bail out! cannot change to test directory $test_root"
    exit 1
fi

util_source="../history/hissqlite/hissqlite-util.in"

# Skip if SQLite support was not compiled in.
if ! grep -q '^#define HAVE_SQLITE3' ../include/config.h 2>/dev/null; then
    echo "1..0 # skip SQLite support not compiled"
    exit 0
fi

# Run the source directly with -p so the test does not depend on an installed
# INN::Config and innconfval.
if [ ! -r "$util_source" ] \
    || ! perl -MDBI -MDBD::SQLite -e 1 >/dev/null 2>&1; then
    echo "1..0 # skip hissqlite-util dependencies unavailable"
    exit 0
fi

echo 7

tmpdir="$(pwd)/his-tmp-util"
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

# Create a minimal hissqlite schema with one real and one remembered entry,
# then delete many filler rows so freelist pages exist for the vacuum test.
if ! perl -MDBI -MDBD::SQLite -e '
use strict;
use warnings;
my $path = $ARGV[0];
my $dbh = DBI->connect("dbi:SQLite:dbname=$path", "", "",
    { RaiseError => 1, AutoCommit => 1 });
$dbh->do(q{
    create table hist (
        hash blob not null primary key,
        arrived integer not null,
        posted integer,
        expires integer,
        token blob
    ) without rowid
});
$dbh->do(q{
    create index hist_remember on hist(posted, arrived)
        where token is null
});
$dbh->do(q{
    create table misc (
        key text not null primary key,
        value not null
    ) without rowid
});
$dbh->do(
    q{insert into misc(key, value) values(?, ?)},
    undef, "version", 1
);
$dbh->do(q{
    insert into hist(hash, arrived, posted, expires, token)
        values(?, ?, ?, ?, ?)
}, undef, pack("H*", "00112233445566778899aabbccddeeff"),
    1600000000, 1600000000, 1600001000,
    pack("H*", "0102030405060708090a0b0c0d0e0f10"));
$dbh->do(q{
    insert into hist(hash, arrived, posted, expires, token)
        values(?, ?, ?, ?, ?)
}, undef, pack("H*", "ffeeddccbbaa99887766554433221100"),
    1600000001, 1600000001, 0, undef);
# Insert and delete filler rows so VACUUM has freelist pages to reclaim.
my $ins = $dbh->prepare(q{
    insert into hist(hash, arrived, posted, expires, token)
        values(?, ?, ?, ?, ?)
});
for my $i (1 .. 500) {
    my $hash = pack("N", $i) . ("\0" x 12);
    $ins->execute($hash, 1600001000 + $i, 1600001000 + $i, 0, undef);
}
my $keep1 = pack("H*", "00112233445566778899aabbccddeeff");
my $keep2 = pack("H*", "ffeeddccbbaa99887766554433221100");
$dbh->do(q{
    delete from hist where hash != ? and hash != ?
}, undef, $keep1, $keep2);
$dbh->disconnect;
' "$tmpdir/history.sqlite"; then
    printcount "not ok" "# create test database"
    printcount "not ok" "# statistics command"
    printcount "not ok" "# schema version statistic"
    printcount "not ok" "# entry counts"
    printcount "not ok" "# count command"
    printcount "not ok" "# vacuum command"
    printcount "not ok" "# counts after vacuum"
    rm -rf "$tmpdir"
    exit 1
fi
printcount "ok" "# create test database"

stats_output=$(perl "$util_source" -s -p "$tmpdir" 2>&1)
stats_rc=$?
count_output=$(perl "$util_source" -c -p "$tmpdir" 2>&1)
count_rc=$?
vac_output=$(perl "$util_source" -v -p "$tmpdir" 2>&1)
vac_rc=$?
count_after=$(perl "$util_source" -c -p "$tmpdir" 2>&1)
count_after_rc=$?

if [ $stats_rc -eq 0 ]; then
    printcount "ok" "# statistics command"
else
    echo "# hissqlite-util -s failed (rc=$stats_rc):"
    echo "$stats_output" | sed 's/^/# /'
    printcount "not ok" "# statistics command"
fi

if echo "$stats_output" | grep -q '^Schema version: 1$'; then
    printcount "ok" "# schema version statistic"
else
    printcount "not ok" "# schema version statistic"
fi

if echo "$stats_output" | grep -q '^Total entries: 2$' \
    && echo "$stats_output" | grep -q '^Real entries: 1 (50.0%)$' \
    && echo "$stats_output" | grep -q '^Remembered entries: 1 (50.0%)$' \
    && echo "$stats_output" \
    | grep -q '^Real entries with explicit expiration: 1$'; then
    printcount "ok" "# entry counts"
else
    echo "# unexpected -s entry counts:"
    echo "$stats_output" | sed 's/^/# /'
    printcount "not ok" "# entry counts"
fi

if [ $count_rc -eq 0 ] \
    && echo "$count_output" | grep -q '^total 2$' \
    && echo "$count_output" | grep -q '^real 1$' \
    && echo "$count_output" | grep -q '^remembered 1$'; then
    printcount "ok" "# count command"
else
    echo "# hissqlite-util -c failed (rc=$count_rc):"
    echo "$count_output" | sed 's/^/# /'
    printcount "not ok" "# count command"
fi

if [ $vac_rc -eq 0 ] \
    && echo "$vac_output" | grep -q '^Vacuum complete\.$' \
    && echo "$vac_output" | grep -q '^  Freelist pages: 0$'; then
    printcount "ok" "# vacuum command"
else
    echo "# hissqlite-util -v failed (rc=$vac_rc):"
    echo "$vac_output" | sed 's/^/# /'
    printcount "not ok" "# vacuum command"
fi

if [ $count_after_rc -eq 0 ] \
    && echo "$count_after" | grep -q '^total 2$' \
    && echo "$count_after" | grep -q '^real 1$' \
    && echo "$count_after" | grep -q '^remembered 1$'; then
    printcount "ok" "# counts after vacuum"
else
    echo "# counts after vacuum unexpected (rc=$count_after_rc):"
    echo "$count_after" | sed 's/^/# /'
    printcount "not ok" "# counts after vacuum"
fi

rm -rf "$tmpdir"
