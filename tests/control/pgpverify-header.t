#! /bin/sh
# Check duplicate signed-header detection in pgpverify.

for dir in ../control ../../control control; do
    if test -r "$dir/pgpverify.in"; then
        pgpverify="$dir/pgpverify.in"
        break
    fi
done
if test -z "$pgpverify" || ! perl -e 1 2>/dev/null; then
    echo "1..0 # skip pgpverify or Perl not found"
    exit 0
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/pgpverify-header.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' 0 1 2 3 15
cat >"$tmp/gpgv" <<EOF
#! /bin/sh
touch "$tmp/invoked"
exit 1
EOF
chmod +x "$tmp/gpgv"
config="\$gpgv = '$tmp/gpgv'; \$tmpdir = '$tmp'; \$syslog_method = '';"
sed "s|^# End of configuration section\.\$|$config|" "$pgpverify" \
    >"$tmp/pgpverify"

sig=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
output=$(perl "$tmp/pgpverify" 2>&1 <<EOF
Control: newgroup example.test
Subject: 0
Subject: replacement
Message-ID: <test@example.com>
Date: Mon, 23 Nov 2026 00:00:00 +0000
From: test@example.com
Sender: test@example.com
X-PGP-Sig: 2.6.2 Subject,Control,Message-ID,Date,From,Sender
	$sig
	AAAAAAAA==
	=AAAA

example.test	A test newsgroup.
EOF
)
status=$?

echo 1
if test "$status" = 255 \
    && test ! -e "$tmp/invoked" \
    && printf '%s\n' "$output" \
        | grep -F 'duplicate signed Subject header' >/dev/null; then
    echo "ok 1 - duplicate signed header with a false-valued first copy"
else
    echo "not ok 1 - duplicate signed header with a false-valued first copy"
    echo "# got status $status and output: $output"
    echo "# expected status 255, no verifier invocation, and the duplicate-header diagnostic"
fi
