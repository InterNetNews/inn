##  $Id$
##
##  checkgroups control message handler.
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

sub control_checkgroups {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($newsgrouppats) = @$par;

    if ($action eq 'mail') {
        my $mail = sendmail("checkgroups by $sender");
        print $mail "$sender posted the following checkgroups message:\n";
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail <<END;

If you want to process it, feed the body
of the message to docheckgroups while logged
in as user ID "$inn::newsuser":

$inn::pathbin/docheckgroups '$newsgrouppats' <<zRbJ
END
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        print $mail "zRbJ\n";
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "checkgroups by $sender", $headers, $body);
        } else {
            logmsg("checkgroups by $sender");
        }
    } elsif ($action eq 'doit') {
        if (defined &local_docheckgroups) {
            local_docheckgroups($body, $newsgrouppats, $log, $sender);
        } else {
            docheckgroups($body, $newsgrouppats, $log, $sender);
        }
    }
}

sub docheckgroups {
    my ($body, $newsgrouppats, $log, $sender) = @_;

    my $tempfile = "$inn::tmpdir/checkgroups.$$";
    open(TEMPART, ">$tempfile.art")
        or logdie("Cannot open $tempfile.art: $!");
    print TEMPART map { s/^~/~~/; "$_\n" } @$body;
    close TEMPART;

    open(OLDIN, '<&STDIN') or die $!;
    open(OLDOUT, '>&STDOUT') or die $!;
    open(STDIN, "$tempfile.art") or die $!;
    open(STDOUT, ">$tempfile") or die $!;
    my $st = system("$inn::pathbin/docheckgroups", $newsgrouppats);
    logdie('Cannot run docheckgroups: ' . $!) if $st == -1;
    logdie('docheckgroups returned status ' . ($st & 255)) if $st > 0;
    open(STDIN, '<&OLDIN') or die $!;
    open(STDOUT, '>&OLDOUT') or die $!;

    open(TEMPFILE, $tempfile) or logdie("Cannot open $tempfile: $!");
    my @output = <TEMPFILE>;
    chop @output;
    logger($log || 'mail', "checkgroups by $sender", \@output);
    close TEMPFILE;
    unlink($tempfile, "$tempfile.art");
}

1;
