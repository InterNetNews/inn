#!/usr/bin/perl -w

########################################################################
#
# analyze-traffic.pl
#
# Written by Jeffrey M. Vinocur <jeff@litech.org>
# This work is hereby placed in the public domain by its author.
#
# Script for keeping track of which newsgroups are receiving the most
# traffic (by article count or byte usage), and which peer is most
# responsible for the traffic in each high-traffic group.
#
########################################################################
#
# Usage:
#
# 1.  Add an entry in $pathetc/newsfeeds like the one below, and issue
#     `ctlinnd reload newsfeeds traffic` (you can change the path to 
#     whatever you like).
#
#             analyze!\
#                 :*\
#                 :Tf,WgsbmnN:/usr/local/news/log/traffic
#
#     You may find it useful to restrict the articles being logged,
#     either by modifing the wildmat pattern, or by using the /exclude
#     notation to indicate articles that have passed through some
#     servers should not be included.
#
#     Also, if $USE_ALL_GROUPS (see below) is false, you can leave out
#     the N flag, which may eliminate some parsing errors.
#
# 2.  Wait for some data.
#
# 3.  Run analyze-traffic.pl on the logged data (you can pass the
#     filename as an argument or feed the data on standard input).
#     You probably want to pipe it into a file, `less`, or `tail` as the
#     output is a line for every group that has received an article
#     according the input data.
#
#     There are some options hardcoded into the script below, under
#     "Constants" -- check for customization, if you like.
#
# 4.  Be sure to comment out the newsfeeds entry when done, or set
#     up some sort of log rotation, or INN will eventually fill up your
#     disk...
#
########################################################################
#
# Implementation notes and known bugs:
#
# - We try (if $USE_ALL_GROUPS is set, below) to count crossposted
#   towards each listed group (even ones not carried on the server!),
#   but since some articles have funky Newsgroups headers, that can
#   backfire.  So parsing can fail, which usually results in the
#   relevant line being skipped, but occasionally can cause Perl to
#   issue warnings (and perhaps produce funny things in the output).
#
#   A workaround would be to repeat e.g. the Message-ID at the end of
#   the intput format (i.e. WgsbmnNm), and then the script could read as
#   many lines as necessary until that ad hoc end-of-record marker
#   appeared.  I haven't found a need for this yet, though.
#
# - The input format is a sequence of lines, each containing a number of
#   space-separated fields.  Check newsfeeds(5) for what the semantics
#   are, but an example line (wrapped), for reference, looks like:
#
#       rec.aviation.military [space]
#       news-out.maxwell.syr.edu [space]
#       2796 [space]
#       <3Jvua.104184$My6.1642017@twister.tampabay.rr.com> [space]
#       @030247454E45524C31000016AD3100000004@ [space]
#       rec.aviation.military,rec.travel.usa-canada, [no space here]
#           sci.electronics.design,sci.econ,sci.environment
#
# - The output format is a sequence of lines, one for each newsgroup,
#   with three tab-separated fields.  They are sorted by either the
#   second or third field, depending on $SORT_BY_SIZE, below.  The first
#   field is the name of the newsgroup.  The second is the total number
#   of articles appearing in that newsgroup followed by, in parentheses,
#   the short name of the peer (see about $TLD_REGEX below) responsible
#   for the most articles and the percentage it made up.  The third is
#   the total number of kilobytes of (accepted) traffic in that
#   newsgroup, followed similarly by the peer responsible for the most
#   traffic in that group.  It looks something like this:
#   
#       news.lists.filters  1057 arts (63% syr)  7105.9 KB (36% cox)
#
#   The short names are made by taking the last component of the
#   (dot-separated) peer name that doesn't match /$TLD_REGEX/.  The idea
#   is that, for example, "isc.org" would be listed as "isc", and
#   "demon.co.uk" would be listed as "demon".  Adjust $TLD_REGEX as
#   needed to trim the top-level domains in your part of the world.
#
#   If your peers have very long short names, the output may look
#   somewhat funny.  Similar things can happen with newsgroup names, so
#   those longer than $FIELD1_WIDTH will be truncated to fit.  (You can
#   set $FIELD1_WIDTH to '' to skip this truncation, in which case the
#   first column will not be space-padded and the output will look a bit
#   ragged.)
#
########################################################################
#
# Constants:

my $USE_ALL_GROUPS = 1;  # if 0, use only group article is stored under
my $SORT_BY_SIZE = 1;    # if 0, sort output by number of articles
my $FIELD1_WIDTH = 30;   # maximum length of newsgroup name, '' for none

my $TLD_REGEX = '^(?:com|net|org|edu|gov|mil|ac|co|uk|au|ca|de)$';
                         # feel free to add any others as needed


########################################################################

use strict;

my %stats;

while( <> ) {
    my ($group, $peer, $bytes, $id, $token, @Newsgroups) = split;
    next unless ($USE_ALL_GROUPS ? @Newsgroups : $token);  # bad input line

    my @groups = map { split /\s*,\s*/ } @Newsgroups;

    foreach ($USE_ALL_GROUPS && @groups ? @groups : $group) {
        my $s = $stats{$_} ||= { count => 0,
                                 bytes => 0,
                                 peers => {},
                                 };

        $s->{count}++;
        $s->{bytes} += $bytes;
        $s->{peers}->{$peer}->{count}++;
        $s->{peers}->{$peer}->{bytes} += $bytes;
    }
}


my $f = $SORT_BY_SIZE ? 'bytes' : 'count';

foreach (sort { $stats{$a}->{$f} <=> $stats{$b}->{$f} } (keys %stats)) {
    my %s = %{$stats{$_}};

    my ($topcount,$topcountwho) = &max('count', $s{peers});
    my ($topbytes,$topbyteswho) = &max('bytes', $s{peers});
    $topcountwho = &trim($topcountwho);
    $topbyteswho = &trim($topbyteswho);
    my $countf = int(100 * $topcount / $s{count});
    my $bytesf = int(100 * $topbytes / $s{bytes});
    my $kb = 0.1 * int($s{bytes} * 10 / 1024);

    my $ng = $FIELD1_WIDTH eq '' ? $_ : substr($_,0,$FIELD1_WIDTH);
    print +(sprintf("%-${FIELD1_WIDTH}s\t", $ng)),
          "$s{count} arts ($countf% $topcountwho)\t",
          "${kb} KB ($bytesf% $topbyteswho)\n";
}

1;

sub trim {
    my @parts = split(/\./, $_[0]);
    my $part;
    while( defined($part = pop(@parts)) ) {
        last unless $part =~ /$TLD_REGEX/o;
    }
    return defined($part) ? $part : $_[0];
}

sub max {
    my $x = 0;
    my $who;
    my ($field, $listref) = @_;

    while( my ($peer, $stats) = each %{$listref} ) {
        if( $stats->{$field} > $x ) {
            $x = $stats->{$field};
            $who = $peer;
        }
    }

    return ($x, $who);
}

