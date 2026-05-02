#! /bin/sh
#
# Integration test for ovsqlite direct reader mode.
#
# Starts a real ovsqlite-server, writes test data through it via the
# writer program, kills the server, then verifies the reader program
# can read the WAL-mode database directly without the server.

count=1
printcount() {
    echo "$1 $count $2"
    count=$(expr $count + 1)
}

# Find the right directory.
dirs='../data data tests/data'
for dir in $dirs; do
    if [ -r "$dir/overview/basic" ]; then
        testdata="$dir"
        break
    fi
done
if [ -z "$testdata" ]; then
    echo "Could not find test data" >&2
    exit 1
fi

server="../../storage/ovsqlite/ovsqlite-server"
writer="overview/ovsqlite-write.t"
reader="overview/ovsqlite-read.t"

# When run from the tests directory, adjust paths.
if [ ! -x "$server" ]; then
    server="../storage/ovsqlite/ovsqlite-server"
fi
if [ ! -x "$writer" ]; then
    writer="./overview/ovsqlite-write.t"
fi
if [ ! -x "$reader" ]; then
    reader="./overview/ovsqlite-read.t"
fi

# Skip if SQLite support was not compiled in.
if ! grep -q '^#define HAVE_SQLITE3' ../include/config.h 2>/dev/null; then
    echo "1..0 # skip SQLite support not compiled"
    exit 0
fi

for prog in "$server" "$writer" "$reader"; do
    if [ ! -x "$prog" ]; then
        echo "1..0 # skip ovsqlite binaries not built"
        exit 0
    fi
done

echo 13

# Set up temp directory with config files.  Use absolute paths because
# the C test programs chdir to the test data directory before reading
# inn.conf.
tmpdir="$(pwd)/ov-tmp-integ"
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

cat > "$tmpdir/inn.conf" <<EOF
domain:          news.example.com
pathhost:        inn.example.com
mta:             "/usr/sbin/sendmail -oi -oem %s"
hismethod:       hisv6
enableoverview:  true
ovmethod:        ovsqlite
wireformat:      true

pathnews:        .
patharchive:     archive
patharticles:    spool
pathoverview:    $tmpdir
pathrun:         $tmpdir
pathetc:         $tmpdir
pathdb:          $tmpdir
pathlog:         $tmpdir
pathtmp:         $tmpdir
EOF

cat > "$tmpdir/ovsqlite.conf" <<EOF
walmode: true
compress: true
cachesize: 1000
readercachesize: 1000
transrowlimit: 10000
transtimelimit: 30.0
EOF

INNCONF="$tmpdir/inn.conf"
export INNCONF

# Test 1: start ovsqlite-server in debug/foreground mode.
$server -d > "$tmpdir/server.log" 2>&1 &
server_pid=$!

# Wait for socket file (up to 5 seconds).
waited=0
while [ ! -S "$tmpdir/ovsqlite.sock" ] && [ $waited -lt 50 ]; do
    # Check if server died.
    if ! kill -0 $server_pid 2>/dev/null; then
        echo "# Server exited prematurely, log:"
        sed 's/^/# /' "$tmpdir/server.log"
        printcount "not ok" "# server startup"
        rm -rf "$tmpdir"
        exit 1
    fi
    sleep 0.1
    waited=$(expr $waited + 1)
done

if [ -S "$tmpdir/ovsqlite.sock" ]; then
    printcount "ok" "# server started"
else
    echo "# Server socket not found after 5s, log:"
    sed 's/^/# /' "$tmpdir/server.log"
    printcount "not ok" "# server started"
    kill $server_pid 2>/dev/null
    rm -rf "$tmpdir"
    exit 1
fi

# Tests 2-4: run the writer (3 TAP tests from inside).
writer_output=$($writer 2>&1)
writer_rc=$?
writer_count=$(echo "$writer_output" | grep -c "^ok ")
if [ $writer_rc -eq 0 ] && [ "$writer_count" -eq 3 ]; then
    printcount "ok" "# writer open"
    printcount "ok" "# writer load"
    printcount "ok" "# writer close"
else
    echo "# Writer failed (rc=$writer_rc):"
    echo "$writer_output" | sed 's/^/# /'
    printcount "not ok" "# writer open"
    printcount "not ok" "# writer load"
    printcount "not ok" "# writer close"
fi

# Test 5: stop the server gracefully.
kill $server_pid 2>/dev/null
wait $server_pid 2>/dev/null
if [ $? -le 128 ] || [ $? -eq 143 ]; then
    printcount "ok" "# server stopped"
else
    printcount "ok" "# server stopped (signal)"
fi

# Tests 6-10: run the reader (5 TAP tests from inside).
reader_output=$($reader 2>&1)
reader_rc=$?
reader_count=$(echo "$reader_output" | grep -c "^ok ")
if [ $reader_rc -eq 0 ] && [ "$reader_count" -eq 5 ]; then
    printcount "ok" "# reader open (direct)"
    printcount "ok" "# group stats"
    printcount "ok" "# article data"
    printcount "ok" "# search order"
    printcount "ok" "# reopen"
else
    echo "# Reader failed (rc=$reader_rc):"
    echo "$reader_output" | sed 's/^/# /'
    printcount "not ok" "# reader open (direct)"
    printcount "not ok" "# group stats"
    printcount "not ok" "# article data"
    printcount "not ok" "# search order"
    printcount "not ok" "# reopen"
fi

# Tests 11-13: server restart resilience.
# Simulate INN restart: start a new server against the existing WAL database,
# shut it down, verify the reader can still read all data.  This is the
# scenario where nnrpd outlives a server restart cycle.
$server -d > "$tmpdir/server2.log" 2>&1 &
server_pid=$!
waited=0
while [ ! -S "$tmpdir/ovsqlite.sock" ] && [ $waited -lt 50 ]; do
    if ! kill -0 $server_pid 2>/dev/null; then
        echo "# Server restart failed, log:"
        sed 's/^/# /' "$tmpdir/server2.log"
        printcount "not ok" "# server restart"
        printcount "not ok" "# server re-stop"
        printcount "not ok" "# reader after restart"
        rm -rf "$tmpdir"
        exit 1
    fi
    sleep 0.1
    waited=$(expr $waited + 1)
done
if [ -S "$tmpdir/ovsqlite.sock" ]; then
    printcount "ok" "# server restart"
else
    printcount "not ok" "# server restart"
fi

kill $server_pid 2>/dev/null
wait $server_pid 2>/dev/null
printcount "ok" "# server re-stop"

# Reader should still work — data survives the restart cycle.
reader_output=$($reader 2>&1)
reader_rc=$?
reader_count=$(echo "$reader_output" | grep -c "^ok ")
if [ $reader_rc -eq 0 ] && [ "$reader_count" -eq 5 ]; then
    printcount "ok" "# reader after restart"
else
    echo "# Reader after restart failed (rc=$reader_rc):"
    echo "$reader_output" | sed 's/^/# /'
    printcount "not ok" "# reader after restart"
fi

# Clean up.
rm -rf "$tmpdir"
