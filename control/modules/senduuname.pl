##  $Id$
##
##  senduuname control message handler.
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

sub control_senduuname {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($where) = @$par;

    if ($action eq 'mail') {
        my $mail = sendmail("senduuname $sender");
        print $mail <<END;
$sender has requested information about your UUCP name.

If this is acceptable, type:
  uuname | $INN::Config::mailcmd -s "senduuname reply from $INN::Config::pathhost" $replyto

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "senduuname $sender", $headers, $body);
        } else {
            logmsg("senduuname $sender");
        }
    } elsif ($action =~ /^(doit|doifarg)$/) {
        if ($action eq 'doifarg' and $where ne $INN::Config::pathhost) {
            logmsg("skipped senduuname $sender");
            return;
        }
        my $mail = sendmail("senduuname reply from $INN::Config::pathhost", $replyto);
        open(UUNAME, 'uuname|') or logdie("Cannot run uuname: $!");
        print $mail $_ while <UUNAME>;
        close UUNAME or logdie("Cannot run uuname: $!");
        close $mail or logdie("Cannot send mail: $!");

        logger($log, "senduuname $sender to $replyto", $headers, $body) if $log;
    }
}

1;
