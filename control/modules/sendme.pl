##  $Id$
##
##  sendme control message handler.
##
##  Copyright 2001 by Marco d'Itri <md@linux.it>
##
##  Redistribution and use in source and binary forms, with or without
##  modification, are permitted provided that the following conditions
##  are met:
##
##   1. Redistributions of source code must retain the above copyright
##      notice, this list of conditions and the following disclaimer.
##
##   2. Redistributions in binary form must reproduce the above copyright
##      notice, this list of conditions and the following disclaimer in the
##      documentation and/or other materials provided with the distribution.

use strict;

sub control_sendme {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $article) = @_;

    my @body = split(/\r?\n/, $article->stringify_body);

    if ($action eq 'mail') {
        my $mail = sendmail("sendme by $sender");
        print $mail map { s/^~/~~/; "$_\n" } @body;
        close $mail or logdie('Cannot send mail: ' . $!);
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "sendme $sender", $article);
        } else {
            logmsg("sendme from $sender");
        }
    } elsif ($action eq 'doit') {
        my $tempfile = "$INN::Config::tmpdir/sendme.$$";
        open(GREPHIST, "| $INN::Config::newsbin/grephistory -s > $tempfile")
            or logdie("Cannot run grephistory: $!");
	foreach (@body) {
            print GREPHIST "$_\n";
	}
        close GREPHIST or logdie("Cannot run grephistory: $!");

        if (-s $tempfile and $site =~ /^[a-zA-Z0-9.-_]+$/) {
            open(TEMPFILE, $tempfile) or logdie("Cannot open $tempfile: $!");
            open(BATCH, ">>$INN::Config::batch/$site.work")
                or logdie("Cannot open $INN::Config::batch/$site.work: $!");
            print BATCH $_ while <TEMPFILE>;
            close BATCH;
            close TEMPFILE;
        }
        unlink $tempfile;
    }
}

1;
