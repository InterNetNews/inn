# $Id$
# Copyright 2001 by Marco d'Itri <md@linux.it>
use strict;
sub control_rmgroup {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($groupname) = @$par;

    # Scan active to see what sort of change we are making.
    open(ACTIVE, $inn::active) or logdie("Cannot open $inn::active: $!");
    my @oldgroup;
    while (<ACTIVE>) {
        next unless /^(\Q$groupname\E)\s\d+\s\d+\s(\w)/;
        @oldgroup = split /\s+/;
        last;
    }
    close ACTIVE;
    my $status;
    if (not @oldgroup) {
        $status = 'no change';
    } elsif (not $approved) {
        $status = 'unapproved';
    } else {
        $status = 'removed';
    }

    if ($action eq 'mail' and $status !~ /(no change|unapproved)/) {
        my $mail = sendmail("rmgroup $groupname $sender");
        print $mail <<END;
$sender asks for $groupname
to be $status.

If this is acceptable, type:
  $inn::newsbin/ctlinnd rmgroup $groupname

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "skipping rmgroup $groupname"
                . " $sender (would be $status)", $headers, $body);
        } else {
            logmsg("skipping rmgroup $groupname $sender (would be $status)");
        }
    } elsif ($action eq 'doit' and $status !~ /(no change|unapproved)/) {
        ctlinnd('rmgroup', $groupname);
        # Update newsgroups too.
        shlock("$inn::locks/LOCK.newsgroups");
        open(NEWSGROUPS, $inn::newsgroups)
            or logdie("Cannot open $inn::newsgroups: $!");
        my $tempfile = "$inn::tmpdir/rmgroup.$$";
        open(TEMPFILE, ">$tempfile") or logdie("Cannot open $tempfile: $!");
        while (<NEWSGROUPS>) {
            print TEMPFILE $_ if not /^\Q$groupname\E\s/;
        }
        close TEMPFILE;
        close NEWSGROUPS;
        rename($tempfile, $inn::newsgroups)
            or logdie("Cannot rename $tempfile: $!");
        unlink "$inn::locks/LOCK.newsgroups";
        unlink $tempfile;

        logger($log, "rmgroup $groupname $status $sender", $headers, $body)
            if $log;
    }
}

1;
