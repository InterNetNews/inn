# $Id$
# Copyright 2001 by Marco d'Itri <md@linux.it>
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
  $inn::mailcmd -s "sendsys reply from $inn::pathhost" $replyto < $inn::newsfeeds

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
        if ($action eq 'doifarg' and $where ne $inn::pathhost) {
            logmsg("skipped sendsys $sender");
            return;
        }
        my $mail = sendmail("sendsys reply from $inn::pathhost", $replyto);
        open(NEWSFEEDS, $inn::newsfeeds)
            or logdie("Cannot open $inn::newsfeeds: $!");
        print $mail $_ while <NEWSFEEDS>;
        print $mail "\n";
        close NEWSFEEDS;
        close $mail or logdie("Cannot send mail: $!");

        logger($log, "sendsys $sender to $replyto", $headers, $body) if $log;
    }
}

1;
