#!/usr/local/bin/perl

# From: Jack Bryans <jbryans@csulb.edu>

# Using innxmit -p to purge a batchfile of entries that refer to files
# that no longer exist took a couple of hours on batchfiles around 200K
# lines, time better spent slogging thru the batch.  I tried a different
# approach in perl that knocked the hours down to minutes.
# 
# Batchpurge.pl discards article numbers below their min in the active
# file.  It catches nearly all routinely expired articles (an honored
# Expires header can cause exceptions), but doesn't catch canceled
# articles.  For old batches, it catches the vast majority of missing
# articles.
# 
# It recognizes by content innfeed .input and .output files and does the
# right thing.  Its output is to stdout so you can use redirection to
# stash it anywhere, or, better yet, pipe it thru a sort to improve
# efficiency at both ends.

# Edit the next 2 assignments for the path to your active file,
# and one or more | joined local hierarchies not sent offsite.
# The exclusions are groups with j, x, or = flags, in addition 
# to the local hierarchy 

$active = '/usr/local/news/active' ;

$exclude = '^local|=|[jx]$';

open(BATCH,$ARGV[0]) || die "Can't open $ARGV[0]: $!\n";

open(ACTIVE,"egrep -v '$exclude' $active |") || die "Can't open $active: $!\n";

while (<ACTIVE>) {
    ($grp,$max,$min,$flg) = split;
#    $grp =~ tr[.][/];
    $ng{$grp} = $min;
}

close ACTIVE;

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
