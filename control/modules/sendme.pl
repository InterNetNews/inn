# $Id$
# Copyright 2001 by Marco d'Itri <md@linux.it>
use strict;
sub control_sendme {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;

    if ($action eq 'mail') {
        my $mail = sendmail("sendme by $sender");
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie('Cannot send mail: ' . $!);
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "sendme $sender", $headers, $body);
        } else {
            logmsg("sendme from $sender");
        }
    } elsif ($action eq 'doit') {
        my $tempfile = "$inn::tmpdir/sendme.$$";
        open(GREPHIST, "|grephistory -s > $tempfile")
            or logdie("Cannot run grephistory: $!");
        print GREPHIST "$_\n" foreach @$body;
        close GREPHIST or logdie("Cannot run grephistory: $!");

        if (-s $tempfile and $site =~ /^[a-zA-Z0-9.-_]+$/) {
            open(TEMPFILE, $tempfile) or logdie("Cannot open $tempfile: $!");
            open(BATCH, ">>$inn::batch/$site.work")
                or logdie("Cannot open $inn::batch/$site.work: $!");
            print BATCH $_ while <TEMPFILE>;
            close BATCH;
            close TEMPFILE;
        }
        unlink $tempfile;
    }
}

1;
