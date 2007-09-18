##  $Id$
##
##  sendsys control message handler.
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

sub control_sendsys {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($where) = @$par;

    if ($action eq 'mail') {
        my $mail = sendmail("sendsys $sender");
        print $mail <<END;
$sender has requested that you send a copy
of your newsgroups file.

If this is acceptable, type:
  $INN::Config::mailcmd -s "sendsys reply from $INN::Config::pathhost" $replyto < $INN::Config::newsfeeds

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "sendsys $sender", $headers, $body);
        } else {
            logmsg("sendsys $sender");
        }
    } elsif ($action =~ /^(doit|doifarg)$/) {
        if ($action eq 'doifarg' and $where ne $INN::Config::pathhost) {
            logmsg("skipped sendsys $sender");
            return;
        }
        my $mail = sendmail("sendsys reply from $INN::Config::pathhost", $replyto);
        open(NEWSFEEDS, $INN::Config::newsfeeds)
            or logdie("Cannot open $INN::Config::newsfeeds: $!");
        print $mail $_ while <NEWSFEEDS>;
        print $mail "\n";
        close NEWSFEEDS;
        close $mail or logdie("Cannot send mail: $!");

        logger($log, "sendsys $sender to $replyto", $headers, $body) if $log;
    }
}

1;
