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
        $article, $charset_from, $charset_to) = @_;
    my ($groupname, $modflag) = @$par;

    my $head = $article->head;
    my @headers = split(/\r?\n/, $head->stringify);
    my @fullbody = split(/\r?\n/, $article->stringify_body);
    my (@body, $part, $part_head);
    my $mimegroupinfo = 0;

    my $charset_message;
    if (defined $head->mime_attr('Content-Type.charset')) {
        $charset_message = $head->mime_attr('Content-Type.charset');
    }

    # Check if it is a multipart message.  The body is restricted to
    # the application/news-groupinfo part, if any.
    if ($article->parts > 0) {
        foreach $part ($article->parts) {
            $part_head = $part->head;

            if ($part_head->mime_type eq 'application/news-groupinfo') {
                @body = split(/\r?\n/, $part->stringify_body);
                if (defined $part_head->mime_attr('Content-Type.charset')) {
                    $charset_message = $part_head->mime_attr('Content-Type.charset');
                }
                $mimegroupinfo = 1;
            }
        }
    }

    # The newgroup control message can be an application/news-groupinfo
    # entity itself.
    if ($head->mime_type eq 'application/news-groupinfo') {
        @body = @fullbody;
        $mimegroupinfo = 1;
    }

    @body = @fullbody if not $mimegroupinfo;

    # Find the right charset if absent or forced by control.ctl.
    foreach (@$charset_from) {
        my ($group, $charset) = split /:/;
        if ($groupname =~ /$group/) {
            if (not defined $charset_message or $charset =~ /=force/) {
                $charset_message = $charset;
                $charset_message =~ s/\^(.+)\$/$1/;
                $charset_message =~ s/\\//g;
                $charset_message =~ s/=force//;
            }
            last;
        }
    }
    if (not defined $charset_message
        or not defined Encode::find_encoding($charset_message)) {
        $charset_message = "cp1252";  # Default charset, when undefined.
    }

    $modflag ||= '';
    my $modcmd = $modflag eq 'moderated' ? 'm' : 'y';

    # Scan active to see what sort of change we are making.
    open(ACTIVE, $INN::Config::active) or logdie("Cannot open $INN::Config::active: $!");
    my @oldgroup;
    while (<ACTIVE>) {
        next unless /^(\Q$groupname\E)\s\d+\s\d+\s(\w)/;
        @oldgroup = split /\s+/;
        last;
    }
    close ACTIVE;

    my $status;
    my $ngdesc = '';
    my $olddesc = '';
    my $ngname = $groupname;

    # If there is a tag line, search whether the description has changed.
    my $found = 0;
    my $ngline = '';
    foreach (@body) {
        $found = 1 if (($mimegroupinfo)
                       and ($_ !~ /^For your newsgroups file:\s*$/));
        if ($found) {
            # It is the line which contains the description.
            $ngline = $_;
            last;
        }
        $found = 1 if $_ =~ /^For your newsgroups file:\s*$/;
    }

    if ($found) {
      ($ngname, $ngdesc) = split(/\s+/, $ngline, 2);

      # Scan newsgroups to see the previous description, if any.
      open(NEWSGROUPS, $INN::Config::newsgroups)
          or logdie("Cannot open $INN::Config::newsgroups: $!");
      while (<NEWSGROUPS>) {
          if (/^\Q$groupname\E\s+(.*)/) {
              $olddesc = $1;
              last;
          }
      }
      close NEWSGROUPS;
    }

    # Properly encode the newsgroup description.
    Encode::from_to($ngdesc, $charset_message, $charset_to);

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

    # Check whether the group name and the description are not junk.
    my $errmsg;
    if (defined &local_checkgroupname) {
        $errmsg = local_checkgroupname($groupname);
    } else {
        $errmsg = checkgroupname($groupname);
    }
    if (! $errmsg) {
        if (defined &local_checkdescription) {
            $errmsg = local_checkdescription($ngdesc, $modcmd);
        } else {
            $errmsg = checkdescription($ngdesc, $modcmd);
        }
    }
    if ($errmsg) {
        if ($log) {
            logger($log, "skipping newgroup $groupname $modcmd"
                   . " $sender (would $status): $errmsg", $article);
        } else {
            logmsg("skipping newgroup $groupname $modcmd $sender"
                   . " (would $status): $errmsg");
        }
        return;
    }

    if ($action eq 'mail' and $status !~ /(no change|unapproved)/) {
        my $mail = sendmail("newgroup $groupname $modcmd $sender");
        print $mail <<END;
$sender asks for $groupname
to $status.

If this is acceptable, type:
  $INN::Config::newsbin/ctlinnd newgroup $groupname $modcmd $sender

And do not forget to update the corresponding description in your
$INN::Config::pathdb/newsgroups file.

The control message follows:

END
        print $mail map { s/^~/~~/; "$_\n" } @headers;
        print $mail "\n";
        print $mail map { s/^~/~~/; "$_\n" } @fullbody;
        close $mail or logdie("Cannot send mail: $!");
    } elsif ($action eq 'log') {
        if ($log) {
            logger($log, "skipping newgroup $groupname $modcmd"
                . " $sender (would $status)", $article);
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

        if ($log) {
            logger($log, "newgroup $groupname $modcmd $status $sender",
                   $article) if ($log ne 'mail' or $status ne 'no change');
        }
    }
    return;
}

sub update_desc {
    my ($name, $desc) = @_;
    shlock("$INN::Config::locks/LOCK.newsgroups");
    my $tempfile = "$INN::Config::newsgroups.$$";
    open(NEWSGROUPS, $INN::Config::newsgroups)
        or logdie("Cannot open $INN::Config::newsgroups: $!");
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
    rename($tempfile, $INN::Config::newsgroups)
        or logdie("Cannot rename $tempfile: $!");
    unlink("$INN::Config::locks/LOCK.newsgroups", $tempfile);
}

# Check the group name.  This is partially derived from C News.
# Some checks are commented out if I think they're too strict or
# language-dependent.  Your mileage may vary.
sub checkgroupname {
    local $_ = shift;

    # Whole-name checking.
    return 'Empty group name' if (! $_);
    return 'Whitespace in group name' if /\s/;
    return 'Unsafe group name' if /[\`\/:;]/;
    return 'Bad dots in group name' if /^\./ or /\.$/ or /\.\./;
#    return 'Group name does not begin/end with alphanumeric'
#        if (/^[a-zA-Z0-9].+[a-zA-Z0-9]$/;
    return 'Group name begins in control., example., junk. or to.'
        if /^(?:control|example|junk|to)\./;
#    return 'Group name too long' if length $_ > 128;

    my @components = split(/\./);
    # Prevent alt.a.b.c.d.e.f.g.w.x.y.z...
    return 'Too many components' if $#components > 9;

    # Per-component checking.
    for (my $i = 0; $i <= $#components; $i++) {
        local $_ = $components[$i];
        return 'All-numeric name component' if /^[0-9]+$/;
#        return 'Name component starts with non-alphanumeric' if /^[a-zA-Z0-9]/;
#        return 'Name component does not contain letter' if not /[a-zA-Z]/;
        return "`all' or `ctl' used as name component" if /^(?:all|ctl)$/;
#        return 'Name component longer than 30 characters' if length $_ > 30;
        return 'Uppercase letter(s) in name' if /[A-Z]/;
        return 'Illegal character(s) in name' if /[^a-z0-9+_\-]/;
        # Sigh, c++ etc. must be allowed.
        return 'Repeated punctuation in name' if /--|__|\+\+./;
#        return 'Repeated component(s) in name' if ($i + 2 <= $#components
#            and $_ eq $components[$i + 1] and $_ eq $components[$i + 2]);
    }
    return '';
}


# Check the description.
sub checkdescription {
    my ($desc, $flag) = @_;

    # Whole-name checking.
    return 'Empty description' if (! $desc);

    return 'Moderation status mismatch'
        if ($desc =~ / \(Moderated\)$/) and $flag eq 'y';

    return 'Moderation status mismatch'
        if ($desc !~ / \(Moderated\)$/) and $flag eq 'm';

    return 'Reserved "(Moderated)" substring used'
        if ($desc =~ /\(Moderated\).+$/);

    return '';
}

1;
