#!/usr/bin/perl
#
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Wed Jan  3 00:09:01 1996
# Project:      INN -- innfeed
# File:         testListener.pl
# Description:  Generate news files for testing the innfeed feeder.
#
#               Run like this:
#
#                       testListener.pl -t 30 -d tmp | innfeed
#
#               or like this:
#
#                       innfeed -s 'perl testListener.pl -t 30 -d tmp'
#

$0 =~ s!.*/!!;

use Getopt::Std;

# Define variables Getopt::Std uses for --help and --version.
$Getopt::Std::STANDARD_HELP_VERSION = 1;
our $VERSION = $INN::Config::version;
$VERSION =~ s/INN //;

my $usage = "Usage:
  $0 [-ahru] [-b name] [-c count] [-d directory] [-l number]
  [-t sleep-amt] peers

Options:
  -a is for duplicate article id's periodically
  -b add bogus peername periodically
  -c is how many articles to create (0 the default means no limit)
  -d is the directory where articles show be written
  -h prints this usage message
  -l is the number of extra lines to generate in articles (default is 50)
  -r is to have articles be created in NNTP ready format
  -t is the number of seconds to sleep between each article
  -u is for random unlinking of article
";

sub HELP_MESSAGE {
    print $usage;
    exit(0);
}

getopts("ab:c:d:hl:t:ru") || die $usage;

HELP_MESSAGE() if defined($opt_h);
$total = $opt_c;

$sleepAmt = 1;
$sleepAmt = $opt_t if ($opt_t =~ /^[\d\.]+/);

$lineCount = 50;
$lineCount = $opt_l if ($opt_l =~ /^\d+$/);

$directory = ".";
$directory = $opt_d if $opt_d;

$bogus = $opt_b;
if ($bogus && $bogus !~ /^[a-zA-Z]+$/) {
    print "The bogus peername must contain only letters\n";
}

$cr = ($opt_r ? "\r" : "");

$SIG{'INT'} = 'IGNORE';
$SIG{'TERM'} = 'sigHup';
$SIG{'QUIT'} = 'sigHup';
$SIG{'HUP'} = 'sigHup';

sub sigHup {
    exit(1);
}

$monstr = "JanFebMarAprMayJunJulAugSepOctNovDec";
$letstr = "abcdefghijklmnopqrstuvwxyz";

sub createArticle {
    local ($counter) = @_;
    local ($filename, $msgid, $i);
    local ($time) = $^T;
    local ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)
      = gmtime($time);
    local ($index) = $counter;

    if ($opt_a && ((int(rand(4)) % 2) == 0)) {
        $index = int($index / 2);
    }

    $msgid = "<$index.$$.$time\@home.octet.com.au>";

    $filename = sprintf("%s/SampleArticle.%06d", $directory, $index);

    open(ARTICLE, ">$filename")
      || die "open ($filename): $!\n";
    print ARTICLE "Path: home.octet.com.au!not-for-mail$cr\n";
    print ARTICLE "From: brister\@home.octet.com.au$cr\n";
    print ARTICLE "Newsgroups: junk,local.test$cr\n";
    print ARTICLE "Subject: Test$cr\n";
    print ARTICLE "Date: ";

    printf ARTICLE "%d %s %d %02d:%02d:%02d UTC$cr\n",
      $mday, substr($monstr, $mon * 3, 3), $year + 1900,
      $hour, $min, $sec;

    print ARTICLE "Organization: None that I can think of$cr\n";
    print ARTICLE "Distribution: world$cr\n";
    print ARTICLE "Message-ID: $msgid$cr\n";
    print ARTICLE "NNTP-Posting-Host: localhost$cr\n";
    print ARTICLE "$cr\n";

    for ($i = 0; $i < $lineCount; $i++) {
        print ARTICLE "x" x ($lineCount + 1), "$cr\n";
    }
    print ARTICLE ".This line has a leading dot.$cr\n";
    print ARTICLE "And the next line only has a dot.$cr\n";
    if ($opt_r) {
        print ARTICLE "..$cr\n";
    } else {
        print ARTICLE ".$cr\n";
    }
    print ARTICLE "And the next line has just two dots...$cr\n";
    print ARTICLE "...$cr\n";
    print ARTICLE "foo$cr\n";
    print ARTICLE "And the next line is the last line of the article$cr\n";
    print ARTICLE "and it only has a single dot on it.$cr\n";
    if ($opt_r) {
        print ARTICLE "..$cr\n";
    } else {
        print ARTICLE ".$cr\n";
    }

    close(ARTICLE);

    return ($msgid, $filename);
}

srand;

$| = 1;

if (!-t STDERR) {
    open(STDERR, ">>/tmp/TESTLISTENER.LOG") || die;
}

srand;
$sleepAmt = 1 if ($sleepAmt < 0);

foreach $peer (@ARGV) {
    $PEERS{$peer} = 1;
}

die "Must give peernames on command line:\n\n$usage" if (!@ARGV);

for ($i = 0; $total == 0 || $i < $total; $i++) {
    ($msgid, $filename) = &createArticle($i);
    if ($opt_a && ((rand(3) % 3) == 0)) {
        print TTY "Removing file $filename\n";
        unlink($filename) if $opt_u;
    }
    print "$filename $msgid @ARGV";
    print " $bogus" if ($bogus && (rand(5) % 5) == 0);
    print "\n";

    select(undef, undef, undef, (rand($sleepAmt - 1) + 1)) if $sleepAmt;
}

sleep 11500 unless -f STDOUT;
