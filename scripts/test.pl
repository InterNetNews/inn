#!/usr/bin/perl

use Time::Piece;
use Time::Local;

$line = "2023-07-23T04:15:01.882775+00:00 localhost innd: ctlinnd command E";
#$line = "2023-07-23T04:15:01.882775+06:00 localhost innd: ctlinnd command E";
#$line = "2023-07-23T04:15:01+00:00 localhost innd: ctlinnd command E";

if ($line =~ /^(\d+-\d+-\d+T\d+:\d+:\d+)(\.\d+)?([+-]\d+):?(\d+)/) {
    my $tzone = "$3$4";
    my $t;

    # Retrieve the local time zone, if logging is in UTC.
    if ("$tzone" eq "+0000") {
        my $localt = localtime;
        print $localt->year;
        $tzone = $localt->strftime("%z");
        $t = Time::Piece->strptime("$1 $tzone", "%Y-%m-%dT%T %z");
    } else {
        $t = Time::Piece->strptime("$1", "%Y-%m-%dT%T");
    }

    my $newdate = $t->monname . " " . $t->mday . " " . $t->hms;
    $line =~ s/^\S+/$newdate/;
    print $line;

}

exit;
