# $Id$
# Copyright (c)1998 G.J. Andruk
# rmgroup.pl - The rmgroup control message.
#  Parameters: params sender reply-to token site action[=log] approved
sub control_rmgroup {
  my $artfh;
  
  my @params = split(/\s+/,shift);
  my $sender = shift;
  my $replyto = shift;
  my $token = shift;
  my $site = shift;
  my ($action, $logging) = split(/=/, shift);
  my $approved = shift;
  
  my $groupname = $params[0];
  
  my $pid = $$;
  my $tempfile = "$inn::tmpdir/rmgroup.$pid";
  
  my ($errmsg, $status, $nc, @component, @oldgroup, $locktry,
      $ngname, $ngdesc, $modcmd, $qgroup);
  
  # Scan active to see what sort of change we are making.
  open ACTIVE, $inn::active;
  $qgroup = quotemeta ($groupname);
  @oldgroup = grep(/^$qgroup\s/, <ACTIVE>);
  @oldgroup = split(/\s+/, $oldgroup[0]);
  close ACTIVE;
  if (! (scalar @oldgroup)) {
    $status = "no change";
  } elsif (! $approved) {
    $status = "unapproved";
  } else {
    $status = "removed";
  }
  
  if ($action eq "mail" && ($status !~ /(no change|unapproved)/)) {
    open(TEMPFILE, ">$tempfile");
    print TEMPFILE ("$sender asks for $groupname\n",
		    "to be $status.\n\n",
		    "If this is acceptable, type:\n",
		    "  $inn::newsbin/ctlinnd rmgroup $groupname\n\n",
		    "The control message follows:\n\n");
    
    $artfh = open_article($token);
    next if (!defined($artfh));
    *ARTICLE = $artfh;
    
    print TEMPFILE $_ while <ARTICLE>;  
    close(ARTICLE);
    close(TEMPFILE);
    logger($tempfile, "mail", "rmgroup $groupname $sender\n");
    unlink($tempfile);
  } elsif ($action eq "log") {
    if (!$logging) {
      logmsg ('notice', 'skipping rmgroup %s %s (would be %s)',
	      $groupname, $sender, $status);
    } else {
      logger($token, $logging,
	     "skipping rmgroup $groupname " .
	     "$sender (would be $status)");
    }
  } elsif (($action eq "doit") && ($status !~ /(no change|unapproved)/)) {
    # Remove the group.
    system("$inn::newsbin/ctlinnd", "-s", "rmgroup", $groupname);
    
    # Update newsgroups too.
    # Get a lock on newsgroups
    $locktry = 0;
  GETNGLOCK:
    while ($locktry < 60) {
      if (system("$inn::newsbin/shlock", "-p", $pid, "-f",
		 "$inn::locks/LOCK.newsgroups")) {
	$locktry++;
	sleep(2);
      } else {
	$locktry = -1;
	last GETNGLOCK;
      }
    }
    
    if ($locktry > -1) {
      logmsg ('err', 'Cannot get lock %s', "$inn::locks/LOCK.newsgroups");
    } else {
      open(NEWSGROUPS, "<$inn::newsgroups");
      open(TEMPFILE, ">$tempfile");
      while (<NEWSGROUPS>) {
	print TEMPFILE $_ if ! /$qgroup\s/;  
      }
      open(TEMPFILE, "<$tempfile");
      open(NEWSGROUPS, ">$inn::newsgroups");
      while (<TEMPFILE>) {
	print NEWSGROUPS $_;
      }
      close TEMPFILE;
      close NEWSGROUPS;
      unlink "$inn::locks/LOCK.newsgroups";
      unlink($tempfile);
    }
    
    # Now, log what we did.
    if ($logging) {
      $errmsg = "rmgroup $groupname ";
      $errmsg .=  $status . " ";
      $errmsg .= $sender;
      logger($token, $logging, $errmsg);
    }
  }
}



