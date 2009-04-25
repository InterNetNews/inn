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
        $article, $charset_from, $charset_to, $exclusionpats,
        $droppats, $maxchanges) = @_;
    my ($newsgrouppats) = @$par;
    my $head = $article->head;
    my @headers = split(/\r?\n/, $head->stringify);
    my @body = split(/\r?\n/, $article->stringify_body);
    my @newbody;

    my $charset_message;
    if (defined $head->mime_attr('Content-Type.charset')) {
        $charset_message = $head->mime_attr('Content-Type.charset');
    }

    foreach (@body) {
        my ($ngname, $ngdesc) = split(/\s+/, $_, 2);
        my $charset_newsgroup = $charset_message;

        next if ($ngname =~ /$exclusionpats/ or $ngname =~ /$droppats/);

        # Find the right charset if absent or forced by control.ctl.
        foreach (@$charset_from) {
            my ($group, $charset) = split /:/;
            if ($ngname =~ /$group/) {
                if (not defined $charset_newsgroup or $charset =~ /=force/) {
                    $charset_newsgroup = $charset;
                    $charset_newsgroup =~ s/\^(.+)\$/$1/;
                    $charset_newsgroup =~ s/\\//g;
                    $charset_newsgroup =~ s/=force//;
                }
                last;
            }
        }

        if (not defined $charset_newsgroup
            or not defined Encode::find_encoding($charset_newsgroup)) {
            $charset_newsgroup = "cp1252";  # Default charset, when undefined.
        }

        # Properly encode the newsgroup description.
        Encode::from_to($ngdesc, $charset_newsgroup, $charset_to);
        push(@newbody, $ngname."\t".$ngdesc);
    }

    if ($action eq 'mail' and $#newbody >= 0) {
        my $mail = sendmail("checkgroups by $sender");
        print $mail "$sender posted the following checkgroups message:\n\n";
        print $mail map { s/^~/~~/; "$_\n" } @headers;
        print $mail <<END;

If you want to process it, feed the body
of the message to docheckgroups while logged
in as user ID "$INN::Config::newsuser":

$INN::Config::pathbin/docheckgroups -u '$newsgrouppats' '$exclusionpats' <<zRbJ
END
        print $mail map { s/^~/~~/; "$_\n" } @newbody;
        print $mail "zRbJ\n";
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            # The checkgroups is written unprocessed (@body and not @newbody).
            logger($log, "checkgroups by $sender", $article);
        } else {
            logmsg("checkgroups by $sender");
        }
    } elsif ($action eq 'doit') {
        if (defined &local_docheckgroups) {
            local_docheckgroups(\@newbody, $newsgrouppats, $exclusionpats,
                                $maxchanges, $log, $sender);
        } else {
            docheckgroups(\@newbody, $newsgrouppats, $exclusionpats,
                          $maxchanges, $log, $sender);
        }
    }
}

sub docheckgroups {
    my ($body, $newsgrouppats, $exclusionpats, $maxchanges, $log, $sender) = @_;

    my $tempfile = "$INN::Config::tmpdir/checkgroups.$$";
    open(TEMPART, ">$tempfile.art")
        or logdie("Cannot open $tempfile.art: $!");
    print TEMPART map { s/^~/~~/; "$_\n" } @$body;
    close TEMPART;

    open(OLDIN, '<&STDIN') or die $!;
    open(OLDOUT, '>&STDOUT') or die $!;
    open(STDIN, "$tempfile.art") or die $!;
    open(STDOUT, ">$tempfile") or die $!;
    my $st = system("$INN::Config::pathbin/docheckgroups", "-u",
                    $newsgrouppats, $exclusionpats);
    logdie('Cannot run docheckgroups: ' . $!) if $st == -1;
    logdie('docheckgroups returned status ' . ($st & 255)) if $st > 0;
    close(STDIN);
    close(STDOUT);
    open(STDIN, '<&OLDIN') or die $!;
    open(STDOUT, '>&OLDOUT') or die $!;

    open(TEMPFILE, $tempfile) or logdie("Cannot open $tempfile: $!");
    my @output = <TEMPFILE>;
    chop @output;
    # There is no need to send an empty mail.
    if ($#output > 0) {
        my $dochanges = 1;
        my @newmaxchanges = @$maxchanges;
        foreach my $line (@output) {
            last if !$dochanges;
            if ($line =~ /^\s*\S*ctlinnd \S+ (\S+)/) {
                my $ngname = $1;
                foreach my $i (0..$#newmaxchanges) {
                    my ($group, $value) = split (/:/, $newmaxchanges[$i]);
                    if ($ngname =~ /$group/) {
                        $value--;
                        if ($value < 0) {
                            $dochanges = 0;
                        }
                        $newmaxchanges[$i] = "$group:$value";
                        last;
                    }
                }
            }
        }

        if ($dochanges) {
            if ($log) {
                logger($log, "checkgroups by $sender processed (changes applied)",
                       \@output);
            }
            my $st = system("$INN::Config::pathbin/mod-active",
                            $tempfile);
            logdie('Cannot run mod-active: ' . $!) if $st == -1;
            logdie('mod-active returned status ' . ($st & 255)) if $st > 0;
        } else {
            logger($log || 'mail', "checkgroups by $sender *not* processed (too many changes)",
                   \@output);
        }
    } else {
        logmsg("checkgroups by $sender processed (no change)");
    }
    close TEMPFILE;
    unlink($tempfile, "$tempfile.art");
}

1;
