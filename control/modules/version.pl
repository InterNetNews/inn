# $Id$
# Copyright 2001 by Marco d'Itri <md@linux.it>
use strict;
sub control_version {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($where) = @$par;

    my $version = $inn::version || '(unknown version)';

    if ($action eq 'mail') {
        my $mail = sendmail("version $sender");
        print $mail <<END;
$sender has requested information about your
news software version.

If this is acceptable, type:
  echo "InterNetNews $version" | $inn::mailcmd -s "version reply from $inn::pathhost" $replyto

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "version $sender", $headers, $body);
        } else {
            logmsg("version $sender");
        }
    } elsif ($action =~ /^(doit|doifarg)$/) {
        if ($action eq 'doifarg' and $where ne $inn::pathhost) {
            logmsg("skipped version $sender");
            return;
        }
        sendmail("version reply from $inn::pathhost", $replyto,
            [ "InterNetNews $version\n" ]);

        logger($log, "version $sender to $replyto", $headers, $body) if $log;
    }
}

1;
