# $Id$
# Copyright 2001 by Marco d'Itri <md@linux.it>
use strict;
sub control_ihave {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;

    if ($action eq 'mail') {
        my $mail = sendmail("ihave by $sender");
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie('Cannot send mail: ' . $!);
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "ihave $sender", $headers, $body);
        } else {
            logmsg("ihave $sender");
        }
    } elsif ($action eq 'doit') {
        my $tempfile = "$inn::tmpdir/ihave.$$";
        open(GREPHIST, "|grephistory -i > $tempfile")
            or logdie('Cannot run grephistory: ' . $!);
        print GREPHIST "$_\n" foreach @$body;
        close GREPHIST;

        if (-s $tempfile) {
            my $inews = open("$inn::inews -h")
                or logdie('Cannot run inews: ' . $!);
            print $inews "Newsgroups: to.$site\n"
               . "Subject: cmsg sendme $inn::pathhost\n"
               . "Control: sendme $inn::pathhost\n\n";
            open(TEMPFILE, $tempfile) or logdie("Cannot open $tempfile: $!");
            print $inews $_ while <TEMPFILE>;  
            close $inews or die $!;
            close TEMPFILE;
        }
        unlink $tempfile;
    }
}

1;
