Path: nntp.stanford.edu!newsfeed.stanford.edu!logbridge.uoregon.edu!hammer.uoregon.edu!csulb.edu!not-for-mail
From: Jack Bryans <jbryans@swift.csulb.edu>
Newsgroups: news.software.nntp
Subject: batchpurge.pl, an alternative to innxmit -p
Date: 28 May 2000 17:06:22 -0700
Organization: Cal State Long Beach
Lines: 61
Message-ID: <2hya4urzi9.fsf@swift.csulb.edu>
NNTP-Posting-Host: swift.csulb.edu
X-Newsreader: Gnus v5.4.37/XEmacs 19.16
Xref: nntp.stanford.edu news.software.nntp:70020

Using innxmit -p to purge a batchfile of entries that refer to files
that no longer exist took a couple of hours on batchfiles around 200K
lines, time better spent slogging thru the batch.  I tried a different
approach in perl that knocked the hours down to minutes.

Batchpurge.pl discards article numbers below their min in the active
file.  It catches nearly all routinely expired articles (an honored
Expires header can cause exceptions), but doesn't catch canceled
articles.  For old batches, it catches the vast majority of missing
articles.

It recognizes by content innfeed .input and .output files and does the
right thing.  Its output is to stdout so you can use redirection to
stash it anywhere, or, better yet, pipe it thru a sort to improve
efficiency at both ends.

There are three lines to edit in the beginning to adapt to your setup.

#!/usr/local/bin/perl

# Edit the next 3 assignments for your newspool path, the path to your
# active file, and one or more | joined local hierarchies not sent
# offsite.  The exclusions are groups with j, x, or = flags, in addition
# to the local hierarchy

$spool = '/var/spool/news' ;

$active = '/usr/local/news/active' ;

$exclude = '^local|=|[jx]$';

open(BATCH,$ARGV[0]) || die "Can't open $ARGV[0]";

open(ACTIVE,"egrep -v '$exclude' $active |") || die "Can't open $active";

while (<ACTIVE>) {
    ($grp,$max,$min,$flg) = split;
    $ng{$grp} = $min;
}

close ACTIVE;

chdir($spool) || die "Can't cd $spool";

$_ = <BATCH>;

$offset = /^(\d+)\s*$/ ? $1 : 0;

seek(BATCH,$offset,0);

while (<BATCH>) {
    ($art,$rest) = split;
    @grp = split(/\//,$art);
    $art = pop @grp;
    $grp = join('.',@grp);
    print unless $art < $ng{$grp};
}

close BATCH;

Jack
