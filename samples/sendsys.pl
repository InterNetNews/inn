# $Id$
# Copyright (c)1998 G.J. Andruk
# sendsys.pl - The sendsys control message.
#  Parameters: params sender reply-to token site action[=log] approved
sub control_sendsys {
  my $artfh;
  
  my @params = split(/\s+/,shift);
  my $sender = shift;
  my $replyto = shift;
  my $token = shift;
  my $site = shift;
  my ($action, $logging) = split(/=/, shift);
  my $approved = shift;
  
  my $pid = $$;
  my $tempfile = "$inn::tmpdir/sendsys.$pid";
  
  my ($errmsg, $status, $nc, @component, @oldgroup, $locktry,
      $ngname, $ngdesc, $modcmd, $kid);
  
  if ($action eq "mail") {
    open(TEMPFILE, ">$tempfile");
    print TEMPFILE ("$sender has requested that you send a copy\n",
		    "of your newsgroups file.\n\n",
		    "If this is acceptable, type:\n",
		    "  $inn::mailcmd -s \"sendsys reply from ",
		    "$inn::pathhost\" $replyto < $inn::newsfeeds\n\n",
		    "The control message follows:\n\n");
    
    $artfh = open_article($token);
    next if (!defined($artfh));
    *ARTICLE = $artfh;
    
    print TEMPFILE $_ while <ARTICLE>;  
    close(ARTICLE);
    close(TEMPFILE);
    logger($tempfile, "mail", "sendsys $sender\n");
    unlink($tempfile);
  } elsif ($action eq "log") {
    if (!$logging) {
      logmsg ('notice', 'sendsys %s', $sender);
    } else {
      logger($token, $logging, "sendsys $sender");
    }
  } elsif ($action =~ /^(doit|doifarg)$/) {
    if (($action eq "doifarg") && ($params[0] ne $inn::pathhost)) {
      logmsg ('notice', 'skipped sendsys %s', $sender);
    } else {
      # Send the file.
      open NEWSFEEDS, "<$inn::newsfeeds";
      $kid = open2(\*R, \*MAIL, $inn::mailcmd, "-s",
	    "sendsys reply from $inn::pathhost", $replyto);
      print MAIL $_ while <NEWSFEEDS>;  
      print MAIL "\n";
      
      close NEWSFEEDS;
      close R;
      close MAIL;
      waitpid($kid, 0);
      # Now, log what we did.
      if ($logging) {
	$errmsg = "sendsys $sender to $replyto";
	logger($token, $logging, $errmsg);
      }
    }
  }
}
