##  $Id$
##
##  newgroup control message handler.
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

sub control_newgroup {
    my ($par, $sender, $replyto, $site, $action, $log, $approved,
        $headers, $body) = @_;
    my ($groupname, $modflag) = @$par;

    $modflag ||= '';
    my $modcmd = $modflag eq 'moderated' ? 'm' : 'y';

    my $errmsg;
    $errmsg= local_checkgroupname($groupname) if defined &local_checkgroupname;
    if ($errmsg) {
        $errmsg = checkgroupname($groupname) if $errmsg eq 'DONE';

        if ($log) {
            logger($log, "skipping newgroup ($errmsg)", $headers, $body);
        } else {
            logmsg("skipping newgroup ($errmsg)");
        }
        return;
    }

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
    my $ngdesc = 'No description.';
    my $olddesc = '';    
    my $ngname = $groupname;

    # If there is a tag line, search whether the description has changed.
    my $found = 0;
    my $ngline = '';
    foreach (@$body) {
        if ($found) {
            # It is the line which contains the description.
            $ngline = $_;
            last;
        }
        $found = 1 if $_ =~ /^For your newsgroups file:\s*$/;
    }
    
    if ($found) {
      ($ngname, $ngdesc) = split(/\s+/, $ngline, 2);
      if ($ngdesc) {
          $ngdesc =~ s/\s+$//;
          $ngdesc =~ s/\s+\(moderated\)\s*$//i;
          $ngdesc .= ' (Moderated)' if $modflag eq 'moderated';
      }
      # Scan newsgroups to see the previous description, if any.
      open(NEWSGROUPS, $inn::newsgroups)
          or logdie("Cannot open $inn::newsgroups: $!");
      while (<NEWSGROUPS>) {
          if (/^\Q$groupname\E\s+(.*)/) {
              $olddesc = $1;
              last;
          }
      }
      close NEWSGROUPS;
    }

    if (@oldgroup) {
        if ($oldgroup[3] eq 'm' and $modflag ne 'moderated') {
            $status = 'be made unmoderated';
        } elsif ($oldgroup[3] ne 'm' and $modflag eq 'moderated') {
            $status = 'be made moderated';
        } else {
            if ($ngdesc eq $olddesc) {
                $status = 'no change';
            } else {
                $status = 'have a new description';
            }
        }
    } elsif (not $approved) {
        $status = 'unapproved';
    } else {
        $status = 'be created';
    }

    if ($action eq 'mail' and $status !~ /(no change|unapproved)/) {
        my $mail = sendmail("newgroup $groupname $modcmd $sender");
        print $mail <<END;
$sender asks for $groupname
to $status.

If this is acceptable, type:
  $inn::newsbin/ctlinnd newgroup $groupname $modcmd $sender

And do not forget to update the corresponding description in your
newsgroups file.

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @$headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @$body;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "skipping newgroup $groupname $modcmd"
                . " $sender (would $status)", $headers, $body);
        } else {
            logmsg("skipping newgroup $groupname $modcmd $sender"
                . " (would $status)");
        }
    } elsif ($action eq 'doit' and $status ne 'unapproved') {
        if ($status ne 'no change') {
            # The status 'be made (un)moderated' prevails over
            # 'have a new description' so it is executed.
            ctlinnd('newgroup', $groupname, $modcmd, $sender)
                if $status ne 'have a new description';
            # We know the description has changed.
            update_desc($ngname, $ngdesc) if $ngdesc and $ngname eq $groupname;
        }
        
        if ($log ne 'mail' or $status ne 'no change') {
            logger($log, "newgroup $groupname $modcmd $status $sender",
                   $headers, $body) if $log;
        }
    }
    return;
}

sub update_desc {
    my ($name, $desc) = @_;
    shlock("$inn::locks/LOCK.newsgroups");
    my $tempfile = "$inn::newsgroups.$$";
    open(NEWSGROUPS, $inn::newsgroups)
        or logdie("Cannot open $inn::newsgroups: $!");
    open(TEMPFILE, ">$tempfile") or logdie("Cannot open $tempfile: $!");
    while (<NEWSGROUPS>) {
        next if (/^\Q$name\E\s+(.*)/);
        print TEMPFILE $_;
    }
    # We now write a pretty line for the description.
    if (length $name < 8) {
        print TEMPFILE "$name\t\t\t$desc\n";
    } elsif (length $name < 16) {
        print TEMPFILE "$name\t\t$desc\n";
    } else {
        print TEMPFILE "$name\t$desc\n";
    }
    close TEMPFILE;
    close NEWSGROUPS;
    rename($tempfile, $inn::newsgroups)
        or logdie("Cannot rename $tempfile: $!");
    unlink("$inn::locks/LOCK.newsgroups", $tempfile);
}

# Check the group name.  This is partially derived from C News.
# Some checks are commented out if I think they're too strict or
# language-dependent.  Your mileage may vary.
sub checkgroupname {
    local $_ = shift;

    # whole-name checking
    return 'Empty group name' if /^$/;
    return 'Whitespace in group name' if /\s/;
    return 'Unsafe group name' if /[\`\/:;]/;
    return 'Bad dots in group name' if /^\./ or /\.$/ or /\.\./;
#    return 'Group name does not begin/end with alphanumeric'
#        if (/^[a-zA-Z0-9].+[a-zA-Z0-9]$/;
    return 'Group name begins in control., junk. or to.' if /^(?:control|junk|to)\./;
#    return 'Group name too long' if length $_ > 128;

    my @components = split(/\./);
    # prevent alt.a.b.c.d.e.f.g.w.x.y.z...
    return 'Too many components' if $#components > 9;

    # per-component checking
    for (my $i = 0; $i <= $#components; $i++) {
        local $_ = $components[$i];
        return 'all-numeric name component' if /^[0-9]+$/;
#        return 'name component starts with non-alphanumeric' if /^[a-zA-Z0-9]/;
#        return 'name component does not contain letter' if not /[a-zA-Z]/;
        return "`all' or `ctl' used as name component" if /^(?:all|ctl)$/;
#        return 'name component longer than 30 characters' if length $_ > 30;
#        return 'uppercase letter(s) in name' if /[A-Z]/;
        return 'illegal character(s) in name' if /[^a-z0-9+_\-.]/;
        # sigh, c++ etc must be allowed
        return 'repeated punctuation in name' if /--|__|\+\+./;
#        return 'repeated component(s) in name' if ($i + 2 <= $#components
#            and $_ eq $components[$i + 1] and $_ eq $components[$i + 2]);
    }
    return '';
}

1;
