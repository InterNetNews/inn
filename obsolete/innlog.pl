#!/usr/bin/perl
#
# $Id$
#
# This version by Greg Patten <greg@loose.apana.org.au>
#
# If you make changes please mail them to me for inclusion
# in subsequent releases.
#
# All the original hard work was done by:
##  ------
##  Revision: 1.7
##  @(#) newsinfo.awk	falcon@cats.ucsc.edu (jon r. luini)	7/4/92
##  Modified by Robert Elz to understand more reports.
##  Modified by Rich $alz for consistent formatting.
##  Modified by Chris Schmidt to sort output.
##  ------
#
# The original (awk) version printed newgroup requests by category and by
# group.  These are now selected by switches as if you have a stack of
# nnrp readers the output gets ridiculous (and is a really dull read).
#

$[ = 1;			# set array base to 1 (hangover from the awk script)

($PROGNAME = $0) =~ s#.*/##;            # basename of what we're invoked as

require "getopts.pl";

&Getopts("acdghn") || die("Usage $PROGNAME [-a] [-c] [-d] [-g] [-h] [-n]\
	-a	Print nnrp gethostbyaddr failures\
        -c      Print nnrp newsgroup requests by category\
        -d      Print nnrp summary by domain\
        -g      Print nnrp newsgroup requests by group\
	-h	DON'T print some less interesting nnrpd host stats\
	-n      DON'T print per host nnrp readership statistics (only totals)\
");

%ctlinnd = (
    'A', 'send',
    'B', 'signal',
    'C', 'reject',
    'D', 'allow',
    'E', 'logmode',
    'F', 'flushfile',
    'a', 'addhist',
    'b', 'begin',
    'c', 'cancel',
    'd', 'checkfile',
    'e', 'drop',
    'f', 'flush',
    'g', 'flushlogs',
    'h', 'go',
    'i', 'hangup',
    'j', 'name',
    'k', 'newgroup',
    'l', 'param',
    'm', 'pause',
    'n', 'renumber',
    'o', 'reload',
    'p', 'rmgroup',
    'q', 'shutdown',
    'r', 'throttle',
    's', 'mode',
    't', 'refile',
    'u', 'change_group',
    'v', 'readers',
    'w', 'trace',
    'x', 'xabort',
    'y', 'xexec',
    'z', 'reserve',
);

%ctlinnd_info = (
    'A', 'send',
    'B', 'signal',
    'C', 'reject',
    'D', 'allow',
    'E', 'logmode',
    'a', 'addhist',
    'b', 'begin',
    'c', 'cancel',
    'd', 'checkfile',
    'e', 'drop',
    'i', 'hangup',
    'j', 'name',
    'n', 'renumber',
    's', 'mode',
    't', 'refile',
    'v', 'readers',
    'z', 'reserve',
);


##
##  SYSLOG
##

while ($line = <>) 
{
    chop($line);

    $line =~ s/{news\.[a-z]*} //o;	## From the original scanlogs script

    @Fld = split(/\s+/, $line);

    if (!$start_tim) {
    	$start_mon = $Fld[1]; 
    	$start_day = $Fld[2]; 
    	$start_tim = $Fld[3]; 
    }

    next if ($Fld[5] eq 'last' && $Fld[6] eq 'message' && $Fld[7] eq 'repeated');

    ##
    ##  NNRP
    ##
    if ($Fld[5] =~ /nnrpd.*/) {
	# dud messages before cant getpeername (handled below) 
	next if (($Fld[6] eq 'exit' && $Fld[7] eq 'articles') ||
	         ($Fld[6] eq 'times' && $Fld[7] eq 'user'));
	next if ($Fld[7]eq 'cant' && $Fld[8] eq 'getpeername');

	$host = $Fld[6];

	if ($Fld[6] eq 'ident_client:') {
	    if($Fld[7] eq 'sending') {
		$nnrp_identsent++;
	    }
	    else {
		$nnrp_identbad++ if($Fld[12] ne 'USERID');
	    }
	    next;
	}

	if ($Fld[7] eq 'connect') {
	    $nnrp_connect{$host}++;
	    $nnrp = 1;
	    next;
	}

	if ($Fld[7] eq 'exit') {
	    $nnrp_articles{$host} += $Fld[9];
	    $nnrp_groups{$host} += $Fld[11];
	    next;
	}

	if ($Fld[7] eq 'posts') {
	    $nnrp_posts{$host} += $Fld[9];
	    $nnrp_rejected{$host} += $Fld[11];
	    next;
	}

	if ($Fld[7] eq 'times') {
	    $nnrp_times_user{$host} += $Fld[9];
	    $nnrp_times_sys{$host} += $Fld[11];
	    $nnrp_times_elapsed{$host} += $Fld[13];
	    next;
	}

	if ($Fld[7] eq 'group') {
	    $nnrp_readers = 1;
	    $nnrp_group_request{$Fld[8]}++;
	    next;
	}

	##  Skip; handled in "posts" above.
	next if ($Fld[7] eq 'post');

	if ($Fld[7] eq 'unrecognized') {
	    $nnrp_unrecs = 1;
	    $nnrp_unrec{$host}++;
	    $nnrp_unreccmd{$Fld[8]}++;
	    next;
	}

	if (($Fld[7] eq 'no_permission') ||
	   ($Fld[7] eq 'no_access')) {
	    $nnrp_noperms = 1;
	    $nnrp_noperm{$host}++;
	    next;
	}

	if ($Fld[7] eq 'timeout' || 
	   ($Fld[10] eq 'timed' && $Fld[11] eq 'out')) {
	    $client_timeout = 1;
	    $nnrp_timeout{$host}++;
	    next;
	}

	## gethostbyaddr
        if ($Fld[6] eq 'gethostbyaddr:') {
            $nnrp_gethostafails = 1;
            $nnrp_gethostafail{$Fld[7]}++;
            next;
        }
 
        if ($Fld[7] eq 'cant' && $Fld[8] eq 'gethostbyaddr') {
            $nnrp_gethostafails = 1;
            $nnrp_gethostafail{$Fld[9]}++;
            next;
        }
        ## END gethostbyaddr


	## gethostbyname
	if ($Fld[6] eq 'gethostbyname:') {
	    $nnrp_gethostnfails = 1;
	    $nnrp_gethostnfail{$Fld[7]}++;
	    next;
	}

	if ($Fld[7] eq 'cant' && $Fld[8] eq 'gethostbyname') {
	    $nnrp_gethostnfails = 1;
	    $nnrp_gethostnfail{$Fld[9]}++;
	    next;
	}
	## END gethostbyname

	## Broken pipe
	if ($Fld[9] eq 'Broken' && $Fld[10] eq 'pipe') {
	    $nnrp_err = 1;
	    $nnrp_bpipe{$host}++;
	    next;
	}

	if ($Fld[9] eq 'Connection' && $Fld[10] eq 'reset') {
	    $nnrp_err = 1;
	    $nnrp_reset{$host}++;
	    next;
	}

	## Extra auth stuff
	if ($Fld[7] eq 'user') {
	    $nnrp_auth{$host.":".$Fld[8]}++;
	    $nnrp_authhosts{$host}++;
	    $nnrp_authcmds++;
	    next;
	}

	## Auth stuff as per Chris Lewis' mods to innlog.awk
	if ($Fld[7] eq 'auth') {
	    $nnrp_auth{$host.":".$Fld[8]}++;
	    $nnrp_authhosts{$host}++;
	    $nnrp_authcmds++;
	    next;
	}

	## Skip for the moment
	next if ($Fld[7] eq 'newnews' || $line =~ /filtering enabled/o);
    }

    ##
    ## INND
    ##
    if ($Fld[5] =~ /innd/) {
	$innd = 1;
	if ($Fld[6] =~ /^[a-zA-Z]:.*/ || $Fld[6] =~ /^[dgsE]$/) {
	    ##  A ctlinnd command.
	    ##  Note that the last parameter in $6 may be continued in $7 etc
	    undef @ctl;
	    $n = (@ctl = split(/:/, $Fld[6]));
	    unless (defined($ctlinnd{$ctl[1]})) {
		##  Unknown; update this script!
		print "$line\n";
		next;
	    }

	    ## Only remember stuff that's logged via news.info as the
            ## other ctlinnd commands are handled later
            if (defined $ctlinnd_info{$ctl[1]}) {
                $ctlinnd_type{$ctlinnd_info{$ctl[1]}}++;
                $ctlinnd_seen++;
            }

	    if ($ctl[1] eq 'k') {
		##  Newgroup.  Process it here so we can get the creator.
		$ctlinnd_type{'newgroup'}++;
                $ctlinnd_new++;
                $ctlinnd_newgroups{$ctl[2]} = $ctl[3];
                $ctlinnd_newgroups{$ctl[2]} .= sprintf(" by %s", $ctl[4])
                                                if $ctl[4];
		next;
	    }

	    # Skip others for now
	    next;
	}

	if ($Fld[6] =~ /.*:.*/) {
	    $n = (@path = split(/:/, $Fld[6]));
	    $host = $path[1];
	}
	else {
	    $host = $Fld[6];
	}

	if (($Fld[6] eq "ME" || $Fld[6] eq "SERVER") && $Fld[7] eq "cant" &&
	    $Fld[8] eq "select" && $Fld[9] eq "Bad" && $Fld[10] eq "file" &&
	    $Fld[11] eq "number") {
	    $innd_misc_event++;
	    $innd_misc{"file_number"}++;
	    next;
	}

	if (($Fld[6] eq "ME" || $Fld[6] eq "SERVER") && $Fld[7] eq "cant" &&
	    $Fld[8] eq "sendto" && $Fld[9] eq "CCreader") {
	    $innd_misc_event++;
	    $innd_misc{"CCreader"}++;
	    next;
	}

	if (($Fld[6] eq "ME" || $Fld[6] eq "SERVER") && $Fld[7] eq "cant" &&
	    $Fld[8] eq "gethostbyname") {
	    $innd_misc_event++;
	    $innd_misc{"gethostbyname"}++;
	    next;
	}

	if ($Fld[7] eq 'connected') {
	    ##  Account for the fact that innd says "local connected ##"
	    ##  and then "localhost:## closed"
	    $host = 'localhost' if ($host eq 'local');
	    $innd_connect{$host}++;
	    next;
	}

	if ($Fld[7] eq 'closed') {
	    $innd_seconds{$host} += $Fld[9];
	    $innd_accepted{$host} += $Fld[11];
	    $innd_refused{$host} += $Fld[13];
	    $innd_rejected{$host} += $Fld[15];
	    $innd_chkp{$host} = 0;
	    if ($cp_innd_chkp{$host} == 1) {
		$cp_innd_chkp{$host} = 0;
		$innd_points--;
	    }
	    next;
	}

	if ($Fld[7] eq 'checkpoint') {
	    $cp_innd_seconds{$host} = $Fld[9];
	    $cp_innd_accepted{$host} = $Fld[11];
	    $cp_innd_refused{$host} = $Fld[13];
	    $cp_innd_rejected{$host} = $Fld[15];
	    if ($cp_innd_chkp{$host} == 0) {
		$cp_innd_chkp{$host} = 1;
		$innd_points++;
	    }
	    next;
	}

	if ($Fld[6] =~ /\/.*/) {
	    ##  Skip; handled by "opened/spawned" below.
	    next;
	}

	if ($Fld[7] eq 'opened' || $Fld[7] eq 'spawned') {
	    $n = (@blarg = split(/:/, $Fld[8]));
	    $innd_feed{$host} = $blarg[$n];
	    next;
	}

	if ($Fld[6] =~ /free:.*/ && $Fld[7] eq "internal" &&
	    $Fld[8] eq "closing" && $Fld[9] eq "free" && 
	    $Fld[10] eq "channel") {
	    $innd_misc_event++;
	    $innd_misc{"free_channel"}++;
	    next;
	}

	if ($Fld[7] eq 'flush') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_flush{$host}++;
	    next;
	}

 	if ($Fld[7] eq 'flush_file') {
 	    $ctlinnd_type{$Fld[7]}++;
 	    $ctlinnd_seen++;
 	    next;
 	}
 
 	if ($Fld[7] eq "internal" && $Fld[8] eq "rejecting" &&
	    $Fld[9] eq "huge" && $Fld[10] eq "article") {
	    $innd_huge_event++;
	    $innd_huge{$host}++;
	    next;
	}

	if ($Fld[6] eq "rejecting[perl]") {
	    @line = splice(@Fld, 8); 
	    shift(@line) while ($#line >= $[ && $line[$[] ne '437');
	    shift(@line);
	    if ($#line >= 0) {
	    	$innd_perl_reject{join(' ', @line)}++;
	    	$innd_perl_reject++;
	    }
	    next;
	}

	if ($Fld[6] =~ /innfeed.*!/ && $Fld[7] eq "hangup") {
	    $innfeed_misc_event++;
	    $innfeed_misc{"hangup"}++;
	    next;
	}

	## Not used
	if ($Fld[7] eq 'timeout') {
	    $innd_timeout{$host}++;
	    next;
	}

       if ($Fld[7] eq 'newgroup') {
	   ## Check to see if we've already processed this incase we've
	   ## got news.info being logged as well
	   unless ($ctlinnd_newgroups{$Fld[8]}) {
	       $ctlinnd_type{$Fld[7]}++;
	       $ctlinnd_seen++;
	       $ctlinnd_new++;
	       $ctlinnd_newgroups{$Fld[8]} = $Fld[10];
	       $innd_newgroups++;
	   }
           next;
        }

	if ($Fld[7] eq 'rmgroup') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $ctlinnd_rmg++;
	    $ctlinnd_rmgroups{$Fld[8]} = $Fld[10];
	    $innd_rmgroups++;
	    next;
	}

	if ($Fld[7] eq 'paused') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_pauses++;
	    next;
	}

	if ($Fld[7] eq 'throttled') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_throttles++;
	    next;
	}

	if ($Fld[7] eq 'reload') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_reloads++;
	    next;
	}

	if ($Fld[7] eq 'change_group') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_change_groups++;
	    next;
	}

	## Not used
	if ($Fld[7] eq 'shutdown') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    $innd_shutdowns++;
	    next;
	}

	if ($Fld[7] eq 'trace') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    next;
	}
	
	if ($Fld[7] eq 'abort') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    next;
	}
	
	if ($Fld[7] eq 'changed') {
	    $ctlinnd_type{'param'}++;
	    $ctlinnd_seen++;
	    next;
	}

	if ($Fld[7] eq 'starting' || $Fld[7] eq 'restarted') {
	    ## Not used
	    $innd_restarts++;

	    if ($innd_points != 0) {
		##  Unfinished checkpoints from before; something crashed.
		foreach $S (keys %cp_innd_chkp) {
		    next if ($cp_innd_chkp{$S} == 0);
		    $innd_seconds{$S} += $cp_innd_seconds{$S};
		    $innd_accepted{$S} += $cp_innd_accepted{$S};
		    $innd_refused{$S} += $cp_innd_refused{$S};
		    $innd_rejected{$S} += $cp_innd_rejected{$S};
		    $cp_innd_chkp{$S} = 0;
		}
		$innd_points = 0;
	    }
	    next;
	}

	if ($Fld[7] eq 'bad_ihave') {
	    $innd_bad_ihave{$host}++;
	    $innd_bad_ihaves = 1;
	    next;
	}

	if ($Fld[7] eq 'ihave_from_me') {
	    next;
	}

	if ($Fld[7] eq 'bad_messageid') {
	    $innd_bad_msgid{$host}++;
	    $innd_bad_msgids = 1;
	    next;
	}

	if ($Fld[7] eq 'bad_sendme') {
	    $innd_bad_sendme{$host}++;
	    $innd_bad_sendmes = 1;
	    next;
	}

	if ($Fld[7] eq 'bad_command') {
	    $innd_bad_command{$host}++;
	    $innd_bad_commands = 1;
	    next;
	}

	if (($Fld[7] eq 'blocked' && $Fld[8] eq 'sleeping') ||
	    ($Fld[7] eq 'sleeping')) {
	    $innd_blocked{$host}++;
	    $innd_blockeds = 1;
	    next;
	}

	if ($Fld[7] eq 'rejected' && $Fld[8] eq '505') {
	    ##  Too fast connecting clients
	    $fast_clients{$host}++;
	    $fast_clients_all++;
	    next;
	}

	if ($Fld[7] eq 'exit' && $Fld[8] eq '0') {
	    ##  Program or process finished normally; don't care.
	    next;
	}

	if ($Fld[7] eq 'execv') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    next;
	}

	if ($Fld[7] eq 'renumber') {
	    $ctlinnd_type{$Fld[7]}++;
	    $ctlinnd_seen++;
	    next;
	}

	##  Skip handled by "blocked/sleeping" above.
	next if ($Fld[7] eq 'wakeup'   ||
	         $Fld[7] eq 'spooling' || 
	        ($Fld[7] eq 'cant'     && 
	        ($Fld[8] eq 'write' || $Fld[8] eq 'read' || $Fld[8] eq 'flush')
		));

	if ($Fld[7] eq 'inactive' || $Fld[7] eq 'descriptors' ||
	    $Fld[7] eq 'outgoing' || $Fld[6] eq 'DEBUG'       ||
	    $Fld[7] eq 'lcsetup'  || $Fld[7] eq 'ccsetup'     ||
	    $Fld[7] eq 'rcsetup'  || $Fld[7] eq 'readclose'   ||
	    $Fld[7] eq 'flush_all'|| $Fld[7] eq 'renumbering' ||
	    $Fld[7] eq 'restarted'|| $Fld[7] eq 'spooling'    ||
	    $Fld[7] eq 'exit'	  || $Fld[7] eq 'spoolwake'   ||
	    $Fld[7] eq 'running'  || $Fld[7] eq 'NCmode'      ||
	    $Fld[7] eq 'flushlogs' || $Fld[7] eq 'servermode') {
	    # get to here it'll be servermode blah
	    ##  Skip for now - not particularly interesting
	    next;
	}
	if ($Fld[6] eq 'SERVER' && 
	    $Fld[7] eq 'perl' && 
	    $Fld[8] eq 'filtering' && 
	    ($Fld[9] eq 'enabled' || $Fld[9] eq 'disabled')) {
 	    next;
	}		      
    }

    ##
    ##  BATCHER
    ##
    if ($Fld[5] =~ /batcher.*/) {
	$host = $Fld[7];

	## Since times lines are always followed by stats lines, we
	## only do the batcher_site[host]++ and batcher=1 once
	if ($Fld[8] eq 'times') {
	    $batcher_times_user{$host} += $Fld[10];
	    $batcher_times_sys{$host} += $Fld[12];
	    $batcher_times_elapsed{$host} += $Fld[14];
	    $batcher_site{$host}++;
	    $batcher = 1;
	    next;
	}

	if ($Fld[8] eq 'stats') {
	    $batcher_num{$host} += $Fld[10];
	    $batcher_articles{$host} += $Fld[12];
	    $batcher_bytes{$host} += $Fld[14];
	    next;
	}
    }

    ## 
    ## INNFEED
    ##
    if ($Fld[5] =~ /innfeed.*/) {
        $innfeed=1;
        next if ($Fld[6] eq "ME");
	if ($Fld[6] =~ /:/) {
	    $n = (@path = split(/:/, $Fld[6]));
	    $host = $path[1];
	    $colon = 1;
	}
	else {
	    $host = $Fld[6];
	    $colon = 0;
	}
	if ($Fld[7] eq "connected") {
	    $innfeed_connect{$host}++;
	    next;
	}
	if ($Fld[7] eq "checkpoint") {
	    next if $colon;
	    $cp_innfeed_seconds{$host} = $Fld[9];
	    $cp_innfeed_offered{$host} = $Fld[11];
	    $cp_innfeed_accepted{$host} = $Fld[13];
	    $cp_innfeed_refused{$host} = $Fld[15];
	    $cp_innfeed_rejected{$host} = $Fld[17];
	    if ($cp_innfeed_chkp{$host} == 0) {
	        $cp_innfed_chkp{$host} = 1;
	        $innfeed_points++;
	    }
	    next;
	}
	if ($Fld[7] eq "internal" && $Fld[8] eq "QUIT" &&
	    $Fld[9] eq "while" && $Fld[10] eq "write" &&
	    $Fld[11] eq "pending") {
	    $innfeed_misc_event++;
	    $innfeed_misc{"write_pending"}++;
	    next;
	}
	if ($Fld[7] eq "final") {
	    next if $colon;
	    $innfeed_seconds{$host} += $Fld[9];
	    $innfeed_offered{$host} += $Fld[11];
	    $innfeed_accepted{$host} += $Fld[13];
	    $innfeed_refused{$host} += $Fld[15];
	    $innfeed_rejected{$host} += $Fld[17];
	    $innfeed_chkp = 0;
	    if ($cp_innfeed_chkp{$host} == 1) {
	        $cp_innfeed_chkp{$host} = 0;
	        $innfeed_points--;
	    }
	    next;
	}
	if ($Fld[7] eq "starting") {
	    $innfeed_restarts++;
	    if ($innfeed_points) {
	        ## Unfinished checkpoints from before; something crashed.
	        foreach $s (%cp_innfeed_chkp) {
                    next if ($cp_innfeed_chkp{$s} == 0);
                    $innfeed_seconds{$s} += $cp_innfeed_seconds{$s};
                    $innfeed_offered{$s} += $cp_innfeed_offered{$s};
                    $innfeed_accepted{$s} += $cp_innfeed_accepted{$s};
                    $innfeed_refused{$s} += $cp_innfeed_refused{$s};
                    $innfeed_rejected{$s} += $cp_innfeed_rejected{$s};
                    $cp_innfeed_chkp{$s} = 0;
                }
                $innfeed_points = 0;
            }
            next;
        }
        next if ($Fld[7] eq "connect:");        # ignore connect:Connection ref
        next if ($Fld[7] eq "offered");		# one connections stats
        next if ($Fld[7] eq "dropping");	# dropping out of TAKETHIS-only mode
        next if ($Fld[7] eq "switching");	# switching into TAKETHIS-only mode
        next if ($Fld[7] eq "tearing");		# idle connection
        next if ($Fld[7] eq "closed");		# connection closed
        next if ($Fld[7] eq "remote");		# remote suports streaming or not
        next if ($Fld[7] eq "spooling");	# spooling backlog
        next if ($Fld[7] eq "periodic");	# periodic close
        next if ($Fld[7] eq "idle");		# idle connection still has articles
        next if ($Fld[7] eq 'banner');          # banner read failed
	next if ($Fld[7] eq 'failed');          # failed to read resp
        next if ($Fld[7] eq 'no' && $Fld[8] eq 'active');  # no active conn
        next if ($Fld[7] eq 'global');          # global 
	next if ($Fld[8] eq 'no-CHECK');
	next if ($Fld[7] eq 'cxnsleep');
    }

    ##
    ##  INNXMIT
    ##
    if ($Fld[5] =~ /innxmit.*/) {
	if ($Fld[6] =~ /:/) {
	    $n = (@path = split(/:/, $Fld[6]));
	    $host = $path[1];
	}
	else {
	    $host = $Fld[6];
	}
	if ($Fld[7] eq 'ihave' && $Fld[8] eq 'failed') {
	    ## ihave failed occurs when the server rejects an article,
	    ## and requests it to be resent at the ihave stage - this
	    ## always indicates a server problem (all it has of the article
	    ## is its message-id) most commonly "out of space" - in that
	    ## case we don't really want to count this as an offered
	    ## article, so remember this happened so we can reduce the
	    ## offered count below.

	    $innxmit_ihfail{$host} = 1;

	    if ($Fld[9] eq '436' && $Fld[11] eq 'NNTP' && 
		$Fld[13] eq 'out' && $Fld[15] =~ /space/) {
		$innxmit_nospace{$host}++;
		next;
	    }

	    if ($Fld[9] eq '400' && $Fld[11] eq 'space') {
		$innxmit_nospace{$host}++;
		next;
	    }

	    ## Use crefused as it's reported as 'other' in the stats
	    if ($Fld[9] eq '400' && $Fld[10] eq 'Bad' && $Fld[11] eq 'file') {
		$innxmit_crefused{$host}++;
		next;
	    }
	}

	## Since stats lines are always followed by times lines we
	## only do the innxmit_site[host]++ and innxmit=1 once
	if ($Fld[7] eq 'stats') {
	    $innxmit_offered{$host} += $Fld[9] - $innxmit_ihfail{$host};
	    $innxmit_accepted{$host} += $Fld[11];
	    $innxmit_rejected{$host} += $Fld[13];
	    $innxmit_failed{$host} += $Fld[15];
	    $innxmit_ihfail{$host} = 0;
	    $innxmit_site{$host}++;
	    $innxmit = 1;
	    next;
	}

	if ($Fld[7] eq 'times') {
	    $innxmit_times_user{$host} += $Fld[9];
	    $innxmit_times_sys{$host} += $Fld[11];
	    $innxmit_times_elapsed{$host} += $Fld[13];
	    next;
	}

	if ($Fld[8] eq 'Streaming' && $Fld[9] eq 'retries') {
	    $innxmit_retries_event++;
	    $innxmit_retries{$host}++;
	    next;
	}

	if ($Fld[7] eq 'unexpected' && $Fld[8] eq 'response') {
	    $innxmit_unexpected_event++;
	    $innxmit_unexpected{$host}++;
	    next;
	}

	## Not reported
	if ($Fld[7] eq 'requeued') {
	    $r = $Fld[9];
	    for ($i = 10; $i < $#Fld; $i++) {
		$r = $r . ' ' . $Fld[$i];
	    }
	    $innxmit_reQ_host{$host}++;
	    $innxmit_reQ_reason{$r}++;
	    next;
	}

	if ($Fld[7] eq 'connect' && $Fld[9] eq '400' && 
	   (($Fld[10] eq 'No' && $Fld[11] eq 'space') ||
	   ($Fld[11] eq 'NNTP' && $Fld[13] eq 'out' && $Fld[15] =~ /space/))) {
	    $innxmit_site{$host}++;
	    $innxmit_nospace{$host}++;
	    next;
	}

	if ($Fld[7] eq 'connect' && $Fld[9] eq '400' &&

	  $Fld[10] eq 'loadav') {
	    $innxmit_site{$host}++;
	    $innxmit_hiload{$host}++;
	    next;
	}

	if ($Fld[7] eq 'connect' && $Fld[9] eq '400' 
					&& $line =~ /[Ee][Xx][Pp][Ii][Rr]/) {
	    $innxmit_site{$host}++;
	    $innxmit_expire{$host}++;
	    next;
	}

	if ($Fld[7] eq 'connect' && $Fld[9] eq '400') {
	    $innxmit_site{$host}++;
	    $innxmit_crefused{$host}++;
	    next;
	}

	if ($Fld[7] eq 'connect' && $Fld[8] eq 'failed') {
	    $innxmit_site{$host}++;
	    $innxmit_cfail_host{$host}++;
	    next;
	}

	if ($Fld[7] eq 'authenticate' && $Fld[8] eq 'failed') {
	    $innxmit_site{$host}++;
	    $innxmit_afail_host{$host}++;
	    next;
	}

	next if ($Fld[7] eq 'rejected') ; # XXX should keep track
	next if ($Fld[7] eq 'ihave' && $Fld[8] eq 'failed');
    }

    ##
    ##  rnews
    ##
    if ($Fld[5] =~ /rnews.*/) {
	if ($Fld[6] eq 'rejected') {
	    if ($Fld[7] eq 'connection') {
		$rnews_rejects++;
		$i = 8;
		if ($Fld[8] eq '400') {
		    $i++;
		}
		$n = $Fld[$i];
		for ($i++; $i <= $#Fld; $i++) {
		    $n = $n . ' ' . $Fld[$i];
		}
		$rnews_r_reject{$n}++;
		next;
	    }
	    if ($Fld[7] eq '437') {
		$rnews_bad++;
		if ($Fld[8] eq 'Unwanted' && $Fld[9] eq 'newsgroup') {
		    $rnews_bad_ng++;
		    $rnews_bng{$Fld[10]}++;
		    next;
		}
		if ($Fld[8] eq 'Unwanted' && $Fld[9] eq 'distribution') {
		    $rnews_bad_dist++;
		    $rnews_bdist{$Fld[10]}++;
		    next;
		}
		if ($Fld[8] eq 'Bad' && $Fld[9] eq "\"Date\"") {
		    $rnews_bad_date++;
		    next;
		}
		if ($Fld[8] eq 'Bad' && $Fld[9] eq "\"Subject\"") {
		    $rnews_bad_subject++;
		    next;
		}
		if ($Fld[8] eq 'Missing' && $Fld[9] eq "\"Subject\"") {
		    $rnews_bad_subject++;
		    next;
		}
		if ($Fld[11] eq 'header') {
		    $rnews_bad_header++;
		    next;
		}
	    }
	}

	if ($Fld[6] eq 'offered') {
	    $rnews_hosts{$Fld[8]}++;
	    $rnews_host = 1;
	    next;
	}

	if ($Fld[6] eq 'bad_article') {
	    $rnews_bad++;
	    if($Fld[6] eq 'missing' && $Fld[6] eq 'Message-ID'){
	    	$rnews_miss_mesg++;
	    }
	    next;
	}
    }

    ##
    ##  NNTPLINK
    ##
    if ($Fld[5] =~ /.*nntplink.*/) {
	if ($Fld[6] =~ /:/) {
	    $n = (@path = split(/:/, $Fld[6]));
	    $host = $path[1];
	}
	else {
	    $host = $Fld[6];
	}

	if ($Fld[7] eq 'EOF') {
	    $nntplink_site{$host}++;
	    $nntplink_eof{$host}++;
	    next;
	}

	if ($Fld[10] eq 'Broken' && $Fld[11] eq 'pipe') {
	    $nntplink_site{$host}++;
	    $nntplink_bpipe{$host}++;
	    next;
	}

	if($Fld[7] eq 'greeted' && $Fld[10] eq '400' && 
	  (($Fld[11] eq 'No' && $Fld[12] eq 'space') ||
	  ($Fld[12] eq 'NNTP' && $Fld[14] eq 'out' && $Fld[16] =~ /space/))) {
	    $nntplink_site{$host}++;
	    $nntplink_nospace{$host}++;
	    next;
	}

	if($Fld[7] eq 'greeted' && $Fld[10] eq '502') {
	    $nntplink_site{$host}++;
	    $nntplink_auth{$host}++;
	    next;
	}

	if ($Fld[7] eq 'greeted' && $Fld[10] eq '400' && $Fld[11] eq 'loadav') {
	    $nntplink_site{$host}++;
	    $nntplink_hiload{$host}++;
	    next;
	}

	if ($Fld[7] eq 'greeted' && $Fld[10] eq '400' && 
					$line =~ /[Ee][Xx][Pp][Ii][Rr]/) {
	    $nntplink_site{$host}++;
	    $nntplink_expire{$host}++;
	    next;
	}

	if ($Fld[7] eq 'greeted' && $Fld[10] eq '400') {
	    ##  Some other failure, or innd throttle for some local reason.
	    $nntplink_site{$host}++;
	    $nntplink_fail{$host}++;
	    next;
	}

	if ($Fld[7] eq 'socket():') {
	    $nntplink_site{$host}++;
	    $nntplink_sockerr{$host}++;
	    next;
	}

	if ($Fld[7] eq 'select()') {
	    $nntplink_site{$host}++;
	    $nntplink_selecterr{$host}++;
	    next;
	}

	if ($Fld[7] eq 'connection' && $Fld[8] eq 'timed' && $Fld[9] eq 'out') {
	    $nntplink_bpipe{$host}++;
	    $nntplink_site{$host}++;
	    next;
	}

	if ($Fld[7] eq 'sent' && $Fld[8] eq 'authinfo' && $Fld[10] eq 'exiting') {
	    $nntplink_site{$host}++;
	    $nntplink_auth{$host}++;
	}

	if ($Fld[7] eq 'sent' && $Fld[8] eq 'IHAVE') {
	    ##  An "ihave failure":  site rejected the article after the
	    ##  IHAVE command; most likely it was out of space.  Don't
	    ##  count this as an offered article, so remember it.  Since
	    ##  nntplink keeps trying to send we could get many such
	    ##  failures in a single connection.  For counting purposes
	    ##  we pretend that there were several separate connections.
	    $nntplink_ihfail{$host}++;

	    if ($Fld[11] eq '436' && $Fld[13] eq 'NNTP' && 
		$Fld[15] eq 'out' && $Fld[17] =~ /space/) {
		$nntplink_fake_connects{$host}++;
		$nntplink_nospace{$host}++;
		next;
	    }
	}

	## Since stats lines are always followed by xmit lines, we
	## only do the nntplink_site[host]++ and nntplink=1 once
	if ($Fld[7] eq 'stats') {
	    $nntplink_offered{$host} += $Fld[8] - $nntplink_ihfail{$host};
	    $nntplink_accepted{$host} += $Fld[10];
	    $nntplink_rejected{$host} += $Fld[12];
	    $nntplink_failed{$host} += $Fld[14];
	    $nntplink_ihfail{$host} = 0;
	    if ($nntplink_fake_connects{$host}) {
		$nntplink_site{$host} += $nntplink_fake_connects{$host};
		$nntplink_fake_connects{$host} = 0;
	    }
	    else {
		$nntplink_site{$host}++;
	    }
	    $nntplink = 1;
	    next;
	}
	
	if ($Fld[7] eq 'xmit') {
	    $nntplink_times_user{$host} += $Fld[9];
	    $nntplink_times_sys{$host} += $Fld[11];
	    $nntplink_times_elapsed{$host} += $Fld[13];
	    next;
	}

	if ($Fld[7] eq 'xfer') {
	    ##  We can compute this if we need it, but there isn't
	    ##  space in the output table for it
	    #$nntplink_offer_min{$host} += $Fld[9];
	    #$nntplink_accept_min{$host} += $Fld[11];
	    next;
	}

	##  503 is the code for timeouts.
	if ($Fld[11] eq '503' || $Fld[12] eq 'Timeout') {
	    $nntplink_timeout{$host}++;
	    $timeout = 1;
	    next;
	}

	if ($Fld[7] eq 'Link' && $Fld[8] eq 'down') {
	    $nntplink_down{$host} = $Fld[10];
	    $nntplink_downs = 1;
	    next;
	}

	## Ignore `already running' messages
	next if ($Fld[8] eq 'already' && $Fld[9] eq 'running');
	next if ($Fld[9] eq 'No' && $Fld[10] eq 'such');
    }

    ##
    ##  NNTPD
    ##
    if ($Fld[5] =~ /nntpd.*/) {
	if ($Fld[6] =~ /.*:.*/) {
	    $n = (@path = split(/:/, $Fld[6]));
	    $host = $path[1];
	}
	else {
	    $host = $Fld[6];
	}
	if ($Fld[7] eq 'connect') {
	    $nntpd_connect{$host}++;
	    $nntpd = 1;
	    next;
	}

	if ($Fld[7] eq 'times') {
	    ## Not used ?
	    $nntpd_seconds{$host} += $Fld[9] + $Fld[11];
	    $nntpd_elapsed{$host} += $Fld[13];
	    $nntpd_connect{$host}++ if ($nntpd_connect{$host} == 0);
	    $nntpd_done{$host}++;
	    next;
	}

	if ($Fld[7] eq 'ihave_stats') {
	    $nntpd_accepted{$host} += $Fld[9];
	    $nntpd_refused{$host} += $Fld[11];
	    $nntpd_rejected{$host} += $Fld[13];
	    next;
	}

	if ($Fld[7] eq 'no' && $Fld[8] eq 'space') {
	    $nntpd_nospace{$host}++;
	    $nntpd_connect{$host}++ if ($nntpd_connect{$host} == 0);
	    $nntpd_done{$host}++;
	    next;
	}

	if ($Fld[6] eq 'no' && $Fld[7] eq 'space') {
	    next;
	    ##  Rest is likely to be reader stuff; skip for now.
	}
    }

    ##
    ##  MTHREADS
    ##
    if ($Fld[5] =~ /mthreads.*/) {
	if ($Fld[6] eq 'Started' && $Fld[7] eq 'mthreads') {
	    $mthreads = 1;
	    $mthreads_started++;
	    next;
	}

	if ($Fld[6] eq 'Processed' && $Fld[9] eq 'added' &&

	  $Fld[12] eq 'expired') {
	    $mthreads = 1;
	    $mthreads_added += $Fld[10];
	    $mthreads_expired += $Fld[13];
	    next;
	}

	if ($Fld[6] eq 'Turned' && $Fld[8] eq 'groups' && $Fld[9] eq 'on.') {
	    $mthreads = 1;
	    $mthreads_turned_on += $Fld[7];
	    next;
	}

	if ($Fld[6] eq 'Turned' && $Fld[8] eq 'groups' && $Fld[9] eq 'off.') {
	    $mthreads = 1;
	    $mthreads_turned_off += $Fld[7];
	    next;
	}
    }

    ##
    ##  UNKNOWN
    ##

    if ($unknowns == 0) {
	print "Unknown entries from news log file:\n";
	$unknowns = 1;
    }
    print "$line\n";

    ##
    ##  SUMMARIZE DATA
    ##  NOTE: the following are collected but not used right now:
    ##	innd_feed
    ##	innd_change_groups
}
## END main loop

print "\n    News log for host $Fld[4] from $start_mon $start_day $start_tim to $Fld[1] $Fld[2] $Fld[3]\n\n";

if ($innd) 
{
    ##  INND control statistics.
    if ($ctlinnd_seen) {
	printf "Control commands to INND\n";
	$i = 0;
	foreach $type (sort keys %ctlinnd_type) {
	    printf "%-13.13s %5d\n", $type, $ctlinnd_type{$type};
	}
	printf "%-13.13s %5d\n", "TOTAL", $ctlinnd_seen;
	print "\n";

	if ($ctlinnd_new) {
	    print "Newsgroups created:\n";
	    foreach $group (sort keys %ctlinnd_newgroups) {
		printf "\t%-30.30s as %s\n", $group, $ctlinnd_newgroups{$group};
	    }
	    print "\n";
	}

	if ($ctlinnd_rmg) {
	    print "Newsgroups removed:\n";
	    foreach $group (sort keys %ctlinnd_rmgroups) {
		printf "\t%s\n", $group;
	    }
	    print "\n";
	}
    }

    ##  INND exchange statistics.
    printf "Articles received by server\n";
    print
"System            Connects  Offered   Took  Tk%  Refuse Reject Accpt    Elapsed\n";

    foreach $host (keys %innd_connect) {
	$ninnd_accept += $innd_accepted{$host};
    }
    $ninnd_accept = 1 if ($ninnd_accept == 0);

    foreach $host (sort keys %innd_connect) {
	$ninnd_connect += $innd_connect{$host};
	$ninnd_refuse += $innd_refused{$host};
	$ninnd_reject += $innd_rejected{$host};
	$ninnd_ela += $innd_seconds{$host};
	$offered = $innd_accepted{$host} + $innd_refused{$host} + 
							$innd_rejected{$host};
	$ninnd_offered += $offered;
	$offered = 1 if ($offered == 0);
	$percent_accpt = ($innd_accepted{$host} * 100) / $offered;
	$e_hours = $innd_seconds{$host} / 3600;
	$e_sec = $innd_seconds{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;

	printf "%-20.20s %5d  %7d %6d %3d%% %7d %6d  %3d%% %4d:%02d:%02d\n", 
	  $host, $innd_connect{$host},
	  $innd_accepted{$host} + $innd_refused{$host} + $innd_rejected{$host},
	  $innd_accepted{$host}, 
	  (($innd_accepted{$host}*100.0)+0.5)/$ninnd_accept, 
	  $innd_refused{$host}, $innd_rejected{$host},
	  $percent_accpt, $e_hours, $e_min, $e_sec;
    }

    $e_hours = $ninnd_ela / 3600;
    $e_sec = $ninnd_ela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $they_offered = $ninnd_offered;
    if ($they_offered == 0) {
	$they_offered = 1;
    }
    $percent_accpt = ($ninnd_accept * 100.0) / ($they_offered + 0.1);

    printf "\n%-20s %5d  %7d %6d 100%% %7d %6d  %3d%% %4d:%02d:%02d\n\n", 
      'TOTALS', $ninnd_connect, $ninnd_offered, $ninnd_accept, $ninnd_refuse,
      $ninnd_reject, $percent_accpt, $e_hours, $e_min, $e_sec;
}

##  Miscellaneous innd statistics.
if ($innd_bad_msgids) {
    print "Bad Message-ID's offered\n";
    foreach $host (sort keys %innd_bad_msgid) {
	printf "%-20.20s %5d\n", $host, $innd_bad_msgid{$host};
    }
    print "\n";
}

if ($innd_bad_ihaves) 
{
    print "Bad ihave control messages received\n";
    foreach $host (sort keys %innd_bad_ihave) {
	printf "%-20.20s %5d\n", $host, $innd_bad_ihave{$host};
    }
    print "\n";
}

if ($innd_bad_sendmes) 
{
    print "Ignored sendme control messages received\n";
    foreach $host (sort keys %innd_bad_sendme) {
	printf "%-20.20s %5d\n", $host, $innd_bad_sendme{$host};
    }
    print "\n";
}

if ($innd_bad_commands) 
{
    print "Bad commands received\n";
    foreach $host (sort keys %innd_bad_command) {
	printf "%-20.20s %5d\n", $host, $innd_bad_command{$host};
    }
    print "\n";
}

if ($innd_blockeds) 
{
    print "Blocked server feeds\n";
    foreach $host (sort keys %innd_blocked) {
	printf "%-20.20s %5d\n", $host, $innd_blocked{$host};
    }
    print "\n";
}

if ($innd_huge_event)
{
    print "Huge article rejections\n";
    foreach $host (sort keys %innd_huge) {
	printf "%-20.20s %5d\n", $host, $innd_huge{$host};
    }
    printf "%-20.20s %5d\n", "TOTAL", $innd_huge_event;
    print "\n";
}

if ($innd_misc_event) {
    print "INND misc events\n";
    foreach $name (sort keys %innd_misc) {
	printf "%-20.20s %5d\n", $name, $innd_misc{$name};
    }
    printf "%-20.20s %5d\n", "TOTAL", $innd_misc_event;
    print "\n";
}

if ($innd_perl_reject) {
     print "INND articles rejected by the Perl filter: $innd_perl_reject\n";
     foreach $name (sort { $innd_perl_reject{$b} <=> $innd_perl_reject{$a} } 
						    keys %innd_perl_reject) {
       printf "%5d %s\n", $innd_perl_reject{$name}, $name;
    }
    print "\n";
}


## INNFEED statistics
if ($innfeed) {
    print "Articles sent by innfeed\n";
    print "System            Connects   Offered    Took  Refuse  Reject Accpt    Elapsed\n";
    foreach $sortindex (sort(keys %innfeed_connect)) {
        $s = $sortindex if ($innfeed_connect[$sortindex] >= 0);
        last quit if ($s ne $sortindex);
        $innf_connect += $innfeed_connect{$s};
        $innf_seconds += $innfeed_seconds{$s};
        $innf_offered += $innfeed_offered{$s};
        $innf_accepted +=$innfeed_accepted{$s}; 
        $innf_refused += $innfeed_refused{$s};
        $innf_rejected += $innfeed_rejected{$s};
        $innfeed_offered{$s}=1 if ($innfeed_offered{$s} == 0);
        $percent_accpt = ($innfeed_accepted{$s} * 100) / $innfeed_offered{$s};
        $e_hours = $innfeed_seconds{$s} / 3600;
        $e_sec   = $innfeed_seconds{$s} % 3600;
        $e_min   = $e_sec / 60;
        $e_sec  %= 60;
        printf("%-20.20s %5d   %7d %7d %7d %7d  %3d%% %4d:%02d:%02d\n",
            $s, $innfeed_connect{$s}, $innfeed_offered{$s},
            $innfeed_accepted{$s}, $innfeed_refused{$s}, $innfeed_rejected{$s},
            $percent_accpt, $e_hours, $e_min, $e_sec);
        $innfeed_connect{$s} = -1;
    }
    $e_hours = $innf_seconds / 3600;
    $e_sec   = $innf_seconds % 3600;
    $e_min   = $e_sec / 60;
    $e_sec  %= 60;
    $innf_offered = 1 if ($innf_offered == 0);
    $percent_accpt = ($innf_accepted * 100.0) / ($innf_offered + 0.1);
    printf("\n%-20s %5d   %7d %7d %7d %7d  %3d%% %4d:%02d:%02d\n\n",
        "TOTALS", $innf_connect, $innf_offered, $innf_accepted,
        $innf_refused, $innf_rejected, $percent_accpt,
        $e_hours, $e_min, $e_sec);

    if ($innfeed_misc_event) {
	print "Innfeed misc events\n";
	foreach $name (sort keys %innfeed_misc) {
	    printf "%-20.20s %5d\n", $name, $innfeed_misc{$name};
	}
	printf "%-20.20s %5d\n", "TOTAL", $innfeed_misc_event;
    }
    print "\n";
}

##  NNTPD statistics.
if ($nntpd) 
{
    print "Articles received by NNTPD\n";
    print
"System            Connects NSpc Fail Offered  Took Refuse Rejct Accpt   Elapsed\n";

    foreach $host (sort keys %nntpd_connect) {
	$nnntpd_connect += $nntpd_connect{$host};
	$nnntpd_nospace += $nntpd_nospace{$host};
	$nnntpd_accept += $nntpd_accepted{$host};
	$nnntpd_refuse += $nntpd_refused{$host};
	$nnntpd_reject += $nntpd_rejected{$host};
	$nnntpd_ela += $nntpd_elapsed{$host};
	$nnntpd_done += $nntpd_done{$host};
	$offered = $nntpd_accepted{$host} + $nntpd_refused{$host} +
	  						$nntpd_rejected{$host};
	$nnntpd_offered += $offered;

	$offered = 1 if ($offered == 0);

	$percent_accpt = ($nntpd_accepted{$host} * 100) / $offered;
	$e_hours = $nntpd_elapsed{$host} / 3600;
	$e_sec = $nntpd_elapsed{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;

	printf "%-20.20s %5d %4d %4d %6d %6d %6d %5d  %3d%% %3d:%02d:%02d\n",
	    $host, $nntpd_connect{$host}, $nntpd_nospace{$host},
	    $nntpd_connect{$host} - $nntpd_done{$host},
	    $nntpd_accepted{$host} + $nntpd_refused{$host} + 
						$nntpd_rejected{$host},
	    $nntpd_accepted{$host}, $nntpd_refused{$host}, 
	    $nntpd_rejected{$host}, $percent_accpt, $e_hours, $e_min, $e_sec;
    }

    $e_hours = $nnntpd_ela / 3600;
    $e_sec = $nnntpd_ela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $they_offered = $nnntpd_offered;
    if ($they_offered == 0) {
	$they_offered = 1;
    }
    $percent_accpt = ($nnntpd_accept * 100.0) / ($they_offered + 0.1);

    printf "\n%-20s %5d %4d %4d %6d %6d %6d %5d  %3d%% %3d:%02d:%02d\n\n",
      'TOTALS', $nnntpd_connect, $nnntpd_nospace,
      $nnntpd_connect - $nnntpd_done, $nnntpd_offered, $nnntpd_accept,
      $nnntpd_refuse, $nnntpd_reject, $percent_accpt, $e_hours, $e_min, $e_sec;
}

##  Innxmit statistics.
if ($innxmit) {
    print "Articles sent by innxmit\n";
    print
"System                Offrd    Took    Toss   Fail  Pct    Elapsed      CPU  Pct\n";

    foreach $host (sort keys %innxmit_site) {
	$we_offered = $innxmit_offered{$host};
	$we_offered = 1 if ($we_offered == 0);
	$they_take = ($innxmit_accepted{$host} * 100.0) / $we_offered;
	$e_hours = $innxmit_times_elapsed{$host} / 3600;
	$e_sec = $innxmit_times_elapsed{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;
	$c_hours = ($innxmit_times_user{$host}+$innxmit_times_sys{$host})/3600;
	$c_sec = ($innxmit_times_user{$host} + $innxmit_times_sys{$host})%3600;
	$c_min = $c_sec / 60;
	$c_sec %= 60;
	$elapsed = $innxmit_times_elapsed{$host};
	$elapsed = 1 if ($elapsed == 0);
	$pct = 
	    (($innxmit_times_user{$host} + $innxmit_times_sys{$host}) * 100.0) 
	    / $elapsed;

	printf
	  "%-20.20s %6d %7d %7d %6d %3d%% %4d:%02d:%02d %2d:%02d:%02d %3d%%\n",
	  $host, $innxmit_offered{$host}, $innxmit_accepted{$host},
	  $innxmit_rejected{$host}, $innxmit_failed{$host}, $they_take,$e_hours,
	  $e_min, $e_sec, $c_hours, $c_min, $c_sec, $pct;

	$ixmt += $innxmit_offered{$host};
	$ixmt_accept += $innxmit_accepted{$host};
	$ixmt_reject += $innxmit_rejected{$host};
	$ixmt_failed += $innxmit_failed{$host};
	$ixmt_ela += $innxmit_times_elapsed{$host};
	$ixmt_cpu += $innxmit_times_user{$host} + $innxmit_times_sys{$host};
    }

    $we_offered = $ixmt;
    $we_offered = 1 if ($we_offered == 0);
    $they_take = ($ixmt_accept * 100) / $we_offered;
    $e_hours = $ixmt_ela / 3600;
    $e_sec = $ixmt_ela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $c_hours = $ixmt_cpu / 3600;
    $c_sec = $ixmt_cpu % 3600;
    $c_min = $c_sec / 60;
    $c_sec %= 60;
    $ixmt_ela = 1 if ($ixmt_ela == 0);
    $pct = ($ixmt_cpu * 100.0) / $ixmt_ela;

    printf
      "\n%-19.19s %7d %7d %7d %6d %3d%% %4d:%02d:%02d %2d:%02d:%02d %3d%%\n\n",
      'TOTALS', $ixmt, $ixmt_accept, $ixmt_reject, $ixmt_failed, $they_take,
      $e_hours, $e_min, $e_sec, $c_hours, $c_min, $c_sec, $pct;

    print "\n";
    print
"Transmission Connection Attempts           ------errors-------------------\n";

    print
"System                Conn    Ok Auth Load Space Expire Connct Other   Pct\n";

    foreach $host (sort keys %innxmit_site) {
	$tot = $innxmit_site{$host};
	$tot = 1 if ($tot == 0);

	$errs = $innxmit_afail_host{$host} + $innxmit_hiload{$host} +
	        $innxmit_nospace{$host} + $innxmit_cfail_host{$host} + 
		$innxmit_expire{$host} + $innxmit_crefused{$host};

	$ok = $innxmit_site{$host} - $errs;

	printf "%-20.20s %5d %5d %4d %4d %5d  %5d  %5d %5d   %3d%%\n", 
	    $host, $innxmit_site{$host}, $ok, $innxmit_afail_host{$host},
	    $innxmit_hiload{$host}, $innxmit_nospace{$host}, 
	    $innxmit_expire{$host}, $innxmit_cfail_host{$host}, 
	    $innxmit_crefused{$host}, (100.0 * $ok / $tot);

	$ict_tot += $innxmit_site{$host};
	$ict_ok += $ok;
	$ict_afail += $innxmit_afail_host{$host};
	$ict_hiload += $innxmit_hiload{$host};
	$ict_nospace += $innxmit_nospace{$host};
	$ict_expire += $innxmit_expire{$host};
	$ict_crefused += $innxmit_crefused{$host};
	$ict_cfail += $innxmit_cfail_host{$host};
    }

    $tot = $ict_tot;
    $tot = 1 if ($tot == 0);

    $errs = $ict_afail + $ict_nospace + $ict_hiload + $ict_cfail +$ict_crefused;

    printf "\n%-20.20s %5d %5d %4d %4d %5d  %5d  %5d %5d   %3d%%\n\n",
      'TOTALS', $ict_tot, $ict_ok, $ict_afail, $ict_hiload, $ict_nospace,
      $ict_expire, $ict_cfail, $ict_crefused, (100.0 * $ict_ok / $tot);
}

if ($innxmit_retries_event)
{
    print "Innxmit streaming retries\n";
    foreach $host (sort keys %innxmit_retries) {
	printf "%-20.20s %5d\n", $host, $innxmit_retries{$host};
    }
    printf "%-20.20s %5d\n", "TOTAL", $innxmit_retries_event;
    print "\n";
}

if ($innxmit_unexpected_event)
{
    print "Innxmit unexpected response\n";
    foreach $host (sort keys %innxmit_unexpected) {
	printf "%-20.20s %5d\n", $host, $innxmit_unexpected{$host};
    }
    printf "%-20.20s %5d\n", "TOTAL", $innxmit_unexpected_event;
    print "\n";
}

##  Nntplink statistics.
if ($nntplink) 
{
    print "Articles sent by nntplink\n";
    print "System                Offrd   Took   Toss  Fail  Pct   Elapsed       CPU  Pct\n";

    foreach $host (sort keys %nntplink_site) {
	$we_offered = $nntplink_offered{$host};
	$we_offered = 1 if ($we_offered == 0);
	$they_take = ($nntplink_accepted{$host} * 100.0) / $we_offered;
	$e_hours = $nntplink_times_elapsed{$host} / 3600;
	$e_sec = $nntplink_times_elapsed{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;
	$c_hours = 
	    ($nntplink_times_user{$host} + $nntplink_times_sys{$host}) / 3600;
	$c_sec = 
	    ($nntplink_times_user{$host} + $nntplink_times_sys{$host}) % 3600;
	$c_min = $c_sec / 60;
	$c_sec %= 60;
	$elapsed = $nntplink_times_elapsed{$host};
	$elapsed = 1 if ($elapsed == 0);
	$pct = 
    	    (($nntplink_times_user{$host} + $nntplink_times_sys{$host}) * 100.0)
	    / $elapsed;

	printf
	  "%-20.20s %6d %6d %6d %5d %3d%% %3d:%02d:%02d %3d:%02d:%02d %3d%%\n",
	  $host, $nntplink_offered{$host}, $nntplink_accepted{$host},
	  $nntplink_rejected{$host}, $nntplink_failed{$host}, $they_take, 
	  $e_hours, $e_min, $e_sec, $c_hours, $c_min, $c_sec, $pct;

	$nxmt += $nntplink_offered{$host};
	$nxmt_accept += $nntplink_accepted{$host};
	$nxmt_reject += $nntplink_rejected{$host};
	$nxmt_failed += $nntplink_failed{$host};
	$nxmt_ela += $nntplink_times_elapsed{$host};
	$nxmt_cpu += $nntplink_times_user{$host} + $nntplink_times_sys{$host};
    }
    
    
    $we_offered = $nxmt;
    if ($we_offered == 0) {
	$we_offered = 1;
    }
    $they_take = ($nxmt_accept * 100) / $we_offered;
    $e_hours = $nxmt_ela / 3600;
    $e_sec = $nxmt_ela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $c_hours = $nxmt_cpu / 3600;
    $c_sec = $nxmt_cpu % 3600;
    $c_min = $c_sec / 60;
    $c_sec %= 60;
    $nxmt_ela = 1 if ($nxmt_ela == 0);
    $pct = ($nxmt_cpu * 100.0) / $nxmt_ela;

    printf
      "\n%-20.20s %6d %6d %6d %5d %3d%% %3d:%02d:%02d %3d:%02d:%02d %3d%%\n\n",
      'TOTALS', $nxmt, $nxmt_accept, $nxmt_reject, $nxmt_failed, $they_take,
      $e_hours, $e_min, $e_sec, $c_hours, $c_min, $c_sec, $pct;


    print "Transmission Connection Attempts         -------errors------\n";
    print
"System               Conn   Ok EOF Sock Slct Load Bpip Spce  Exp Auth Othr  Pct\n";

    foreach $host (sort keys %nntplink_site) {
	$tot = $nntplink_site{$host};
	$tot = 1 if ($tot == 0);
	$errs = $nntplink_eof{$host} + $nntplink_sockerr{$host} +
	    $nntplink_selecterr{$host} + $nntplink_hiload{$host} + 
	    $nntplink_bpipe{$host} + $nntplink_nospace{$host} + 
	    $nntplink_auth{$host} + $nntplink_expire{$host} + 
	    $nntplink_fail{$host};

	$ok = ($nntplink_site{$host} - $errs);

	printf "%-20.20s %4d %4d %3d %4d %4d %4d %4d %4d %4d %4d %4d %3d%%\n",
	  $host, $nntplink_site{$host}, $ok, $nntplink_eof{$host},
	  $nntplink_sockerr{$host}, $nntplink_selecterr{$host},
	  $nntplink_hiload{$host}, $nntplink_bpipe{$host}, 
	  $nntplink_nospace{$host}, $nntplink_expire{$host}, 
	  $nntplink_auth{$host}, $nntplink_fail{$host}, (100.0 * $ok / $tot);

	$ct_tot += $nntplink_site{$host};
	$ct_ok += $ok;
	$ct_eof += $nntplink_eof{$host};
	$ct_sockerr += $nntplink_sockerr{$host};
	$ct_selecterr += $nntplink_selecterr{$host};
	$ct_hiload += $nntplink_hiload{$host};
	$ct_bpipe += $nntplink_bpipe{$host};
	$ct_nospace += $nntplink_nospace{$host};
	$ct_auth += $nntplink_auth{$host};
	$ct_expire += $nntplink_expire{$host};
	$ct_fail += $nntplink_fail{$host};
    }

    $tot = $ct_tot;
    $tot = 1 if ($tot == 0);

    printf "\n%-20.20s %4d %4d %3d %4d %4d %4d %4d %4d %4d %4d %4d %3d%%\n\n",
      'TOTALS', $ct_tot, $ct_ok, $ct_eof, $ct_sockerr, $ct_selecterr,
      $ct_hiload, $ct_bpipe, $ct_nospace, $ct_expire, $ct_auth, $ct_fail,
      (100.0 * $ct_ok / $tot);
}

##  Batcher statistics.
if ($batcher) {
    print "UUCP batches created\n";
    print
"System                Offrd   Arts      Bytes   Elapsed       Cpu  Pct\n";

    foreach $host (sort keys %batcher_site) {
	$e_hours = $batcher_times_elapsed{$host} / 3600;
	$e_sec = $batcher_times_elapsed{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;
	$c_hours = 
		($batcher_times_user{$host} + $batcher_times_sys{$host}) / 3600;
	$c_sec = 
		($batcher_times_user{$host} + $batcher_times_sys{$host}) % 3600;
	$c_min = $c_sec / 60;
	$c_sec %= 60;
	$elapsed = $batcher_times_elapsed{$host};
	$elapsed = 1 if ($elapsed == 0);
	$pct = 
(($batcher_times_user{$host} + $batcher_times_sys{$host}) * 100.0) / $elapsed;

	printf "%-20.20s %6d %6d %10d %3d:%02d:%02d %3d:%02d:%02d %3d%%\n",
		      $host, $batcher_num{$host}, $batcher_articles{$host}, 
		      $batcher_bytes{$host}, $e_hours, $e_min, $e_sec, $c_hours,
		      $c_min, $c_sec, $pct;

	$nbatch += $batcher_num{$host};
	$nbatch_articles += $batcher_articles{$host};
	$nbatch_bytes += $batcher_bytes{$host};
	$nbatch_ela += $batcher_times_elapsed{$host};
	$nbatch_cpu += $batcher_times_user{$host} + $batcher_times_sys{$host};
    }

    $e_hours = $nbatch_ela / 3600;
    $e_sec = $nbatch_ela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $c_hours = $nbatch_cpu / 3600;
    $c_sec = $nbatch_cpu % 3600;
    $c_min = $c_sec / 60;
    $c_sec %= 60;
    $nbatch_ela = 1 if ($nbatch_ela == 0);
    $pct = ($nbatch_cpu * 100.0) / $nbatch_ela;

    printf "\n%-20.20s %6d %6d %10d %3d:%02d:%02d %3d:%02d:%02d %3d%%\n\n",
      'TOTALS', $nbatch, $nbatch_articles, $nbatch_bytes, $e_hours, $e_min,
      $e_sec, $c_hours, $c_min, $c_sec, $pct;
}

##  Rnews statistics.
if ($rnews_host) 
{
    printf "Rnews articles offered from:\n";
    foreach $host (sort keys %rnews_hosts) {
	printf "\t%6d\t%s\n", $rnews_hosts{$host}, $host;
    }
    print "\n";
}

if ($rnews_rejects) 
{
    printf "Rnews connections rejected %d times\n", $rnews_rejects;

    foreach $host (sort keys %rnews_r_reject) {
	printf "\t%6d\t%s\n", $rnews_r_reject{$host}, $host;
    }
    print "\n";
}

if ($rnews_bad)
{
    printf "Rnews bad articles: (Total %d)\n", $rnews_bad;

    if ($rnews_bad_ng) {
	printf "Bad newsgroup: %d\n", $rnews_bad_ng;
	foreach $host (sort keys %rnews_bng) {
	    printf "\t%5d: %s\n", $rnews_bng{$host}, $host;
	}
    }

    if ($rnews_bad_dist) {
	printf "Bad distribution: %d\n", $rnews_bad_dist;
	foreach $host (sort keys %rnews_bdist) {
	    printf "\t%5d: %s\n", $rnews_bdist{$host}, $host;
	}
    }

    printf "Bad \"Date\":       %4d\n", $rnews_bad_date if ($rnews_bad_date);
    printf "Bad \"Subject\":    %4d\n", $rnews_bad_date if ($rnews_bad_date);
    printf "Bad header:         %4d\n", $rnews_bad_header if ($rnews_bad_header);
    printf "Missing Message-ID: %4d\n", $rnews_miss_mesg if ($rnews_miss_mesg);

    print "\n";
}

##  NNRP statistics.
if ($nnrp) 
{
    print "NNRP readership statistics\
System               Conn Articles Groups Post  Rej Auth  Elapsed      CPU  Pct\n";

    foreach $host (sort keys %nnrp_connect) {
	if ($nnrp_groups{$host} == 0 && $nnrp_articles{$host} == 0 && 
						$nnrp_posts{$host} == 0) {
	    $nnrp_curious{$host} += $nnrp_connect{$host};
	    $curious = 1;
	    $nnrp_connect{$host} = -1;	### LOOK at
	    next;	### LOOK at
	}

	$nclients += 1; 
	$nconn += $nnrp_connect{$host};
	$nauth += $nnrp_authhosts{$host};
	$nart += $nnrp_articles{$host};
	$ngrp += $nnrp_groups{$host};
	$npost += $nnrp_posts{$host};
	$nrej += $nnrp_rejected{$host};
	$ncpu += ($nnrp_times_user{$host} + $nnrp_times_sys{$host});
	$nela += $nnrp_times_elapsed{$host};
	$e_hours = $nnrp_times_elapsed{$host} / 3600;
	$e_sec = $nnrp_times_elapsed{$host} % 3600;
	$e_min = $e_sec / 60;
	$e_sec %= 60;
	$c_hours = ($nnrp_times_user{$host} + $nnrp_times_sys{$host}) / 3600;
	$c_sec = ($nnrp_times_user{$host} + $nnrp_times_sys{$host}) % 3600;
	$c_min = $c_sec / 60;
	$c_sec %= 60;
	$elapsed = $nnrp_times_elapsed{$host};
	$elapsed = 1 if ($elapsed == 0);

	$pct = 
	(($nnrp_times_user{$host} + $nnrp_times_sys{$host}) * 100.0) / $elapsed;

	printf
	 "%-19.19s %5d %8d %6d %4d %4d  %3d %2d:%02d:%02d %2d:%02d:%02d %3d%%\n",
	      $host, $nnrp_connect{$host}, $nnrp_articles{$host}, 
	      $nnrp_groups{$host}, $nnrp_posts{$host}, $nnrp_rejected{$host}, 
	      $nnrp_authhosts{$host}, $e_hours, $e_min, $e_sec, $c_hours, 
	      $c_min, $c_sec, $pct unless $opt_n;

# Collect stats by subdomain, if requested.
	if ($opt_d) {
            $i = index($host, '.');  ### look for FQDN
	    if ($i > 0) {
		if ($host =~ /(\d+\.\d+\.\d+\.)\d+/) {  # dotted quad
	           $dom = $1."0";
	        }
		else {
		    $dom = substr($host, $i + 1);  # Drop host, keep (sub)domain
		}
	        $dhosts{$dom} += 1;
		$dconn{$dom} += $nnrp_connect{$host};
		$dauth{$dom} += $nnrp_authhosts{$host};
		$dart{$dom} += $nnrp_articles{$host};
		$dgrp{$dom} += $nnrp_groups{$host};
		$dpost{$dom} += $nnrp_posts{$host};
		$drej{$dom} += $nnrp_rejected{$host};
		$dcpu{$dom} += $nnrp_times_user{$host} + $nnrp_times_sys{$host};
		$dela{$dom} += $nnrp_times_elapsed{$host};
	   }
        }
    }

    $e_hours = $nela / 3600;
    $e_sec = $nela % 3600;
    $e_min = $e_sec / 60;
    $e_sec %= 60;
    $c_hours = $ncpu / 3600;
    $c_sec = $ncpu % 3600;
    $c_min = $c_sec / 60;
    $c_sec %= 60;
    $nela = 1 if ($nela == 0);
    $pct = ($ncpu * 100.0) / $nela;

    print "\n" unless $opt_n;

    printf
     "%-19.19s %5d %8d %6d %4d %4d  %3d %2d:%02d:%02d %2d:%02d:%02d %3d%%\n\n",
      'TOTALS', $nconn, $nart, $ngrp, $npost, $nrej, $nauth, $e_hours, $e_min,
      $e_sec, $c_hours, $c_min, $c_sec, $pct;

    printf("Total number of NNRP readers: %d\n\n", $nclients);

##  Stats by domain
    if ($opt_d)
    {
        print "NNRP summary by domain\
Domain           Hosts  Conn Articles Groups Post Rej Auth   Elapsed      CPU\n";
        foreach $dom (sort keys %dhosts) {
            $c_hours = $dcpu{$dom} / 3600;
            $c_sec = $dcpu{$dom} % 3600;
            $c_min = $c_sec / 60;
            $c_sec %= 60;
            $e_hours = $dela{$dom} / 3600;
            $e_sec = $dela{$dom} % 3600;
            $e_min = $e_sec / 60;
	    $e_sec %= 60;
            printf 
             "%-16.16s %5d %5d %8d %6d %4d %3d %4d %3d:%02d:%02d %2d:%02d:%02d\n",
             $dom, $dhosts{$dom}, $dconn{$dom}, $dart{$dom},
	     $dgrp{$dom}, $dpost{$dom}, $drej{$dom}, $dauth{$dom},
	     $e_hours, $e_min, $e_sec, $c_hours, $c_min, $c_sec;
        }
        print "\n";
    }
}

##  Miscellaneous NNRP statistics.
if ($nnrp_identsent) 
{
    $identgood = $nnrp_identsent - $nnrp_identbad;
    $identpct = ($identgood / $nnrp_identsent) * 100;
    print "NNRP ident attempts:\nSent  Fail  Good   Pct\n";
    printf "%4d  %4d  %4d  %3d%%\n\n", 
		$nnrp_identsent, $nnrp_identbad, $identgood, $identpct;
}

if ($curious && !$opt_h) 
{
    print "Curious NNRP server explorers\nSystem                          Conn\n";

    foreach $host (sort keys %nnrp_curious) {
	printf "%-30.30s %5d\n", $host, $nnrp_curious{$host};
    }
    print "\n";
}

if ($nnrp_noperms) 
{
    print "NNRP no permission clients\nSystem                          Conn\n";

    foreach $host (sort keys %nnrp_noperm) {
	printf "%-30.30s %5d\n", $host, $nnrp_noperm{$host};
    }
    print "\n";
}

if ($nnrp_authcmds) 
{
    printf("NNRP authentication requests\
System               User            Count      Bad\n");

    foreach $host (sort keys %nnrp_auth) {
	@bit = split(/:/, $host);
	printf("%-20.20s %-10.10s      %5d    %5d\n", $bit[1], $bit[2],
	       $nnrp_auth{$host}, $nnrp_badauth{$host});
	$tauth += $nnrp_auth{$host};
	$tbadauth += $nnrp_badauth{$host};
    }
    printf("\n%-20.20s %-10.10s      %5d    %5d\n", "TOTALS", "", 
	   $tauth, $tbadauth);
    print "\n";
}

if ($nnrp_unrecs) 
{
    if (! $opt_h) {
	print "NNRP unrecognized commands\nSystem  \t\t\tConn\n";

	foreach $host (sort keys %nnrp_unrec) {
	    printf "%-30.30s %5d\n", $host, $nnrp_unrec{$host};
	}
    }

    print "\nNNRP unrecognized command counts\nCommand  \t\t\tCount\n";
    foreach $cmd (sort keys %nnrp_unreccmd) {
	printf "%-30.30s %5d\n", $cmd, $nnrp_unreccmd{$cmd};
    }
    print "\n";
}

##  NNRP errors 
if ($client_timeout && !$opt_h) 
{
    print "NNRP client timeouts\nSystem                          Conn\n";
    foreach $host (sort keys %nnrp_timeout) {
	printf "%-30.30s %5d\n", $host, $nnrp_timeout{$host};
    }
    print "\n";
}

if($opt_a) {
    if ($nnrp_gethostafails) {
	printf "NNRP gethostbyaddr failures\n%-30.30s %5s\n", "IP", "Conn";

	foreach $host (sort keys %nnrp_gethostafail) {
	   printf "%-30.30s %5d\n", $host, $nnrp_gethostafail{$host};
	}
	print "\n";
    }
}

if ($nnrp_gethostnfails) {
    printf "NNRP gethostbyname failures\n-30.30s %5s\n", "System", "Conn";

    foreach $host (sort keys %nnrp_gethostnfail) {
       printf "%-30.30s %5d\n", $host, $nnrp_gethostfail{$host};
    }
    print "\n";
}


if ($server_timeout) 
{
    print "NNTPLINK remote server timeouts\nSystem                          Conn\n";

    foreach $host (sort keys %nntplink_timeout) {
	printf "%-30.30s %5d\n", $host, $nntplink_timeout{$host};
    }
    print "\n";
}

if ($nntplink_downs)
{
    print "Long time nntplink outages\nSystem                         Hours\n";

    foreach $host (sort keys %nntplink_down) {
	printf "%-30.30s %5d\n", $host, $nntplink_down{$host};
    }
    print "\n";
}

##  MTHREADS statistics.
if ($mthreads) 
{
    print
	"Mthreads:   Starts   Groups on  Groups off    Articles     Expired\n";
    printf "%18d%12d%12d%12d%12d\n", $mthreads_started, $mthreads_turned_on,
      		$mthreads_turned_off, $mthreads_added, $mthreads_expired;
    print "\n";
}

##  Too fast clients stastics
if ($fast_clients_all)
{
    print "NNRP clients opening connections too fast\nHost               Count\n";
    foreach $host (sort keys %fast_clients) {
        printf "%-16s%8d\n", $host, $fast_clients{$host};
    }
    print "\n";
}


##  Group readership statistics.
if ($nnrp_readers && ($opt_c || $opt_g))
{
    foreach $g (keys %nnrp_group_request) {
	$X = length($g);
	if ($X > $max) {
	    $max = $X;
	}
	$i = index($g, '.');
	if ($i > 0) {
	    $top = substr($g, 1, $i - 1);
	}
	else {
	    $top = $g;
	}
	$category{$top} += $nnrp_group_request{$g};
    }
    $fmt = sprintf("%%-%ds %%5d\n", $max);

    if($opt_c) {
	print "Newsgroup request counts (by category)\n";

	foreach $cat (sort keys %category) {
	    printf $fmt, $cat, $category{$cat};
	}
	print "\n";
    }

    if($opt_g) {
	print "Newsgroup request counts (by newsgroup)\n";

	foreach $group (sort keys %nnrp_group_request) {
	    printf $fmt, $group, $nnrp_group_request{$group};
	}
	print "\n";
    }
}


