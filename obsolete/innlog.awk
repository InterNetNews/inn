##  $Revision$
##  @(#) newsinfo.awk	falcon@cats.ucsc.edu (jon r. luini)	7/4/92
##  Modified by Robert Elz to understand more reports.
##  Modified by Rich $alz for consistent formatting.
##  Modified by Chris Schmidt to sort output.
##  Modified by Chris Lewis to display authentication statistics
BEGIN {
    unknowns = 0;
    batcher = 0;
    client_timeout = 0;
    ctlinnd_new = 0;
    ctlinnd_rmg = 0;
    ctlinnd_chg = 0;
    ctlinnd_pause = 0;
    ctlinnd_throttle = 0;
    ctlinnd_reload = 0;
    ctlinnd_shutdown = 0;
    ctlinnd_running = 0;
    ctlinnd_seen = 0;
    innd = 0;
    innd_bad_ihaves = 0;
    innd_bad_msgids = 0;
    innd_bad_sendmes = 0;
    innd_blockeds = 0;
    innd_points = 0;
    innd_restarts = 0;
    innd_CCreader = 0;
    innxmit = 0;
    invalid = "~~~~~~~~~~~~~~~~~~~~~~~~"
    mthreads = 0;
    mthreads_added = 0;
    mthreads_expired = 0;
    mthreads_started = 0;
    mthreads_turned_off = 0;
    mthreads_turned_on = 0;
    nnrp = 0;
    nnrp_gethostfails = 0;
    nnrp_noperms = 0;
    print_nnrp_readers_stats = 1;	# set to 1 to enable, 0 to disable
    nnrp_readers = 0;
    nnrp_unrecs = 0;
    nnrp_authcmds = 0;
    nntplink = 0;
    rnews_bad = 0;
    rnews_bad_date = 0;
    rnews_bad_dist = 0;
    rnews_bad_ng = 0;
    rnews_host = 0;
    rnews_host = 0;
    rnews_rejects = 0;
    server_timeout = 0;
}


##
##  SYSLOG
##
$5 == "last" && $6 == "message" && $7 == "repeated" {
    ## Skip.
    next;
}


##
##  NNRP
##
$5 ~ /nnrpd.*/ {
    host = $6;

    if ($7 == "connect") {
	nnrp_connect[host]++;
	nnrp = 1;
	next;
    }

    if ($7 == "auth") {
	nnrp_auth[host ":" $8]++;
	nnrp_authhosts[host]++;
	nnrp_authcmds++;
	next;
    }

    if ($7 == "bad_auth") {
	nnrp_auth[host ":" $8] += 0;	# ensure entry exists
	nnrp_badauth[host ":" $8]++;
	nnrp_authhosts[host] += 0;
	nnrp_badauthhosts[host] += 0;
	nnrp_authcmds++;
	next;
    }

    if ($7 == "exit") {
	nnrp_articles[host] += $9;
	nnrp_groups[host] += $11;
	next;
    }

    if ($7 == "posts") {
	nnrp_posts[host] += $9;
	nnrp_rejected[host] += $11;
	next;
    }

    if ($7 == "times") {
	nnrp_times_user[host] += $9;
	nnrp_times_sys[host] += $11;
	nnrp_times_elapsed[host] += $13;
	next;
    }

    if ($7 == "group") {
	nnrp_readers = 1;
	nnrp_group_request[$8]++;
	next;
    }

    if ($7 == "post") {
	##  Skip; handled in "posts" above.
	next;
    }

    if ($7 == "unrecognized") {
	nnrp_unrecs = 1;
	nnrp_unrec[host]++;
	nnrp_unreccmd[$8]++;
	next;
    }

    if ($7 == "no_permission") {
	nnrp_noperms = 1;
	nnrp_noperm[host]++;
	next;
    }

    # Catch the nnrpd's which timed out on the first read
    if ($7 == "timeout" && $8 == "short") {
	client_short = 1;
	nnrp_short[host]++;
	next;
    }

    if ($7 == "timeout") {
	client_timeout = 1;
	nnrp_timeout[host]++;
	next;
    }

    if ($6 == "gethostbyaddr:") {
	nnrp_gethostfails = 1;
	nnrp_gethostfail[$7]++;
	next;
    }

    if ($7 == "cant" && $8 == "gethostbyaddr") {
	##  Skip; handled in "gethostbyaddr:" above.
	next;
    }

    if ($7 == "cant" && $8 == "read" && $9 == "Connection") {
	## ignore
	next;
    }

    if ($7 ~ /MODE=.*/) {
	# ignore slow readrer detecting diag messages
	next;
    }
}


##
## INND
##
$5 == "innd:" {
    innd = 1;
    if ( $6 ~ /.*:.*/ ) {
	n = split($6, path, ":");
	host = path[1];
    } else if ($6 == "ME") {
	host = "localhost";
    } else {
	host = $6;
    }

    if ($7 == "renumbering" || $7 == "renumber") {
	next;
    }

    if ($7 == "connected") {
	##  Account for the fact that innd says "local connected ##"
	##  and then "localhost:## closed"
	if (host == "local") {
	    host = "localhost";
	}
	innd_connect[host]++;
	next;
    }

    if ($7 == "closed") {
	innd_seconds[host] += $9;
	innd_accepted[host] += $11;
	innd_refused[host] += $13;
	innd_rejected[host] += $15;
	innd_chkp[host] = 0;
	if (cp_innd_chkp[host] == 1) {
	    cp_innd_chkp[host] = 0;
	    innd_points--;
	}
	next;
    }
    if ($7 == "checkpoint") {
	cp_innd_seconds[host] = $9;
	cp_innd_accepted[host] = $11;
	cp_innd_refused[host] = $13;
	cp_innd_rejected[host] = $15;
	if (cp_innd_chkp[host] == 0) {
	    cp_innd_chkp[host] = 1;
	    innd_points++;
	}
	next;
    }

    if ($6 ~ /\/.*/) {
	##  Skip; handled by "opened/spawned" below.
	next;
    }

    # Connections rejected because of the limiting incoming connect
    # code can cause two different rejected messages:
    #
    # ... innd: HOSTADDR rejected cant write Broken pipe"
    # ... innd: HOSTADDR rejected 505"
    #
    # They both mean the same thing.  The Broken pipe happens when
    # the other end of the connection goes away first.
    if ($7 == "rejected") {
	if ($8 == "505" ||
	    ($10 == "Broken" && $11 == "pipe")) {
	    innd_505s[host]++;
	    innd_505_flag = 1;
	}
	next;
    }

    if ($7 == "flush") {
	innd_flush[host]++;
	next;
    }

    if ($7 == "timeout") {
	innd_timeout[host]++;
	next;
    }

    if ($7 == "opened" || $7 == "spawned") {
	n = split($8, blarg, ":");
	innd_feed[host] = blarg[n];
	next;
    }

    if ($7 == "newgroup") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_new++;
	grpname = $8 " as " $10;
	ctlinnd_newgroups[grpname]++;
	next;
    }

    if ($7 == "rmgroup") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_rmg++;
	ctlinnd_rmgroups[$8]++;
	next;
    }

    if ($7 == "paused") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_pause++;
	if ($8 == "Expiring") {
	    reason = "expire";
	} else {
	    reason = $8;
	    for (i=9; i <= NF; ++i) {
		reason = reason " " $i;
	    }
	}
	ctlinnd_paused[reason]++;
	next;
    }

    if ($7 == "throttled") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_throttle++;
	reason = $8;
	for (i=9; i <= NF; ++i) {
	    reason = reason " " $i;
	}
	ctlinnd_throttled[reason]++;
	next;
    }

    if ($7 == "reload") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_reload++;
	reason = $8;
	for (i=9; i <= NF; ++i) {
	    reason = reason " " $i;
	}
	ctlinnd_reloads[reason]++;
	next;
    }

    if ($7 == "change_group") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_chg++;
	grpname = $8 " as " $10;
	ctlinnd_chggroup[grpname]++;
	next;
    }

    if ($7 == "shutdown") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	ctlinnd_shutdown++;
	reason = $8;
	for (i=9; i <= NF; ++i) {
	    reason = reason " " $i;
	}
	ctlinnd_shutdowns[reason]++;
	next;
    }

    if ($7 == "running") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	next;
    }

    if ($7 == "cancelled") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	next;
    }

    if ($7 == "execv") {
	ctlinnd_seen++;
	ctlinnd_type[$7]++;
	next;
    }

    if ($7 == "starting") {
	innd_restarts++;

	if (innd_points != 0) {
	    ##  Unfinished checkpoints from before; something crashed.
	    for (s in cp_innd_chkp) {
		if (cp_innd_chkp[s] == 0)
			continue;
		innd_seconds[s] += cp_innd_seconds[s];
		innd_accepted[s] += cp_innd_accepted[s];
		innd_refused[s] += cp_innd_refused[s];
		innd_rejected[s] += cp_innd_rejected[s];
		cp_innd_chkp[s] = 0;
	    }
	    innd_points = 0;
	}
	next;
    }

    if ($7 == "bad_ihave") {
	innd_bad_ihave[host]++;
	innd_bad_ihaves = 1;
	next;
    }

    if ($7 == "ihave_from_me") {
	next;
    }

    if ($7 == "bad_messageid") {
	innd_bad_msgid[host]++;
	innd_bad_msgids = 1;
	next;
    }

    if ($7 == "bad_sendme") {
	innd_bad_sendme[host]++;
	innd_bad_sendmes = 1;
	next;
    }

    if ($7 == "cant" && $8 == "sendto" && $9 == "CCreader") {
	innd_CCreader++;
	next;
    }

    if ($7 == "blocked" && $8 == "sleeping") {
	innd_blocked[host]++;
	innd_blockeds = 1;
	next;
    }

    if ($7 == "exit" && $8 == "0") {
	##  Program or process finished normally; don't care.
	next;
    }

    if ($7 == "wakeup" || ($7 == "cant" && ($8 == "write" || $8 == "read"))) {
	##  Skip; handled by "blocked/sleeping" above.
	next;
    }

    if ($7 == 437) {
	## innd complaint about an article feed to us by another site
	innd_437++;
	innd_437host[host]++;
	if ($8 == "Article") {
	    innd_future[host]++;
	} else if ($8 == "Bad") {
	    innd_baddate[host]++;
	} else if ($8 == "Duplicate") {
	    innd_duplicate[host]++;
	} else if ($8 == "Too") {
	    innd_tooold[host]++;
	} else if ($8 == "Unapproved") {
	    innd_unapproved[host]++;
	} else if ($8 == "Unwanted" && $9 == "newsgroup") {
	    innd_unnewsgrp[host]++;
	} else if ($8 == "Unwanted" && $9 == "distribution") {
	    innd_undistribution[host]++;
	} else {
	    innd_other437[host]++;
	}
	next;
    }

    if ($7 == "inactive"	|| $7 == "descriptors"	\
     || $7 == "outgoing" \
     || $7 == "lcsetup"		|| $7 == "ccsetup"	\
     || $7 == "rcsetup"		|| $7 == "readclose"	\
     || $7 == "flush_all"	|| $7 == "spooling"	\
     || $7 == "restarted"	|| $7 == "exit"		\
     || $7 == "spoolwake"	|| $7 == "NCmode" 	\
     || ($7 == "internal" && $8 == "rejecting"	\
	 && $9 == "huge" && $10 == "article")	\
    ) {
	##  Skip; for now.
	next;
    }
}


##
##  BATCHER
##
$5 ~ /batcher.*/ {
    host = $7;

    ## Since times lines are always followed by stats lines, we
    ## only do the batcher_site[host]++ and batcher=1 once
    if ($8 == "times") {
	batcher_times_user[host] += $10;
	batcher_times_sys[host] += $12;
	batcher_times_elapsed[host] += $14;
	batcher_site[host]++;
	batcher = 1;
	next;
    }

    if ($8 == "stats") {
	batcher_num[host] += $10;
	batcher_articles[host] += $12;
	batcher_bytes[host] += $14;
	next;
    }
}


##
##  INNXMIT
##
$5 ~ /innxmit.*/ {
    if ( $6 ~ /:/ ) {
	n = split($6, path, ":");
	host = path[1];
    }
    else
	host = $6;

    if ($7 == "ihave" && $8 == "failed") {
	## ihave failed occurs when the server rejects an article,
	## and requests it to be resent at the ihave stage - this
	## always indicates a server problem (all it has of the article
	## is its message-id) most commonly "out of space" - in that
	## case we don't really want to count this as an offered
	## article, so remember this happened so we can reduce the
	## offered count below.

	innxmit_ihfail[host] = 1;

	if ($9 == "436" && $11 == "NNTP" && $13 == "out" && $15 ~ /space/) {
	    innxmit_nospace[host]++;
	    next;
	}

	if ($9 == 400) {
	    innxmit_failed[host]++;
	    next;
	}

	if ($9 == 480) {
	    ##  There is no startup marked for this.
	    innxmit_site[host]++;
	    innxmit_afail_host[host]++;
	    next;
	}
    }

    ## Since stats lines are always followed by times lines we
    ## only do the innxmit_site[host]++ and innxmit=1 once
    if ($7 == "stats") {
	innxmit_offered[host] += $9 - innxmit_ihfail[host];
	innxmit_accepted[host] += $11;
	innxmit_rejected[host] += $13;
	innxmit_failed[host] += $15;
	innxmit_ihfail[host] = 0;
	innxmit_site[host]++;
	innxmit = 1;
	next;
    }

    if ($7 == "times") {
	innxmit_times_user[host] += $9;
	innxmit_times_sys[host] += $11;
	innxmit_times_elapsed[host] += $13;
	next;
    }

    if ($7 == "requeued") {
	r = $9;
	for (i = 10; i < NF; i++)
	    r = r " " $i;
	innxmit_reQ_host[host]++;
	innxmit_reQ_reason[r]++;
	next;
    }

    if ($7 == "connect" && $9 == "400" \
     && (($10 == "No" && $11 == "space") \
       || ($11 == "NNTP" && $13 == "out" && $15 ~ /space/))) {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_nospace[host]++;
	next;
    }

    if ($7 == "connect" && $9 == "400" && $10 == "loadav") {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_hiload[host]++;
	next;
    }

    if ($7 == "connect" && $9 == "400" && $0 ~ /[Ee][Xx][Pp][Ii][Rr]/) {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_expire[host]++;
	next;
    }

    if ($7 == "connect" && $9 == "400") {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_crefused[host]++;
	next;
    }

    if ($7 == "connect" && $8 == "failed") {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_cfail_host[host]++;

	##  Later, maybe.
	#r = $9;
	#for (i = 10; i < NF; i++)
	#    r = r " " $i;
	#innxmit_cfail_reason[r]++;
	next;
    }

    if ($7 == "authenticate" && $8 == "failed") {
	##  There is no startup marked for this.
	innxmit_site[host]++;
	innxmit_afail_host[host]++;

	##  Later, maybe.
	#r = $9;
	#for (i = 10; i < NF; i++)
	#	r = r " " $i;
	#innxmit_afail_reason[r]++;
	next;
    }
}

$5 ~ /rnews.*/ {
    if ($6 == "rejected") {
	if ($7 == "connection") {
	    rnews_rejects++;
	    i = 8;
	    if ($8 == "400")
		i++;
	    n = $i;
	    for (i++; i <= NF; i++)
		n = n " " $i;
	    rnews_r_reject[n]++;
	    next;
	}
	if ($7 == "437") {
	    rnews_bad++;
	    if ($8 == "Unwanted" && $9 == "newsgroup") {
		rnews_bad_ng++;
		rnews_bng[$10]++;
		next;
	    }
	    if ($8 == "Unwanted" && $9 == "distribution") {
		rnews_bad_dist++;
		rnews_bdist[$10]++;
		next;
	    }
	    if ($8 == "Bad" && $9 == "\"Date\"") {
		rnews_bad_date++;
		next;
	    }
	}
    }
    if ($6 == "offered") {
	host = $8;
	rnews_hosts[host]++;
	rnews_host = 1;
	next;
    }
}


##
##  NNTPLINK
##
$5 ~ /.*nntplink.*/ {
    if ( $6 ~ /:/ ) {
	n = split($6, path, ":");
	host = path[1];
    }
    else
	host = $6;

    if ($7 == "EOF") {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_eof[host]++;
	next;
    }

    if ($10 == "Broken" && $11 == "pipe") {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_bpipe[host]++;
	next;
    }

    if ($7 == "greeted" && $10 == "400" \
     && (($11 == "No" && $12 == "space") \
       || ($12 == "NNTP" && $14 == "out" && $16 ~ /space/))) {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_nospace[host]++;
	next;
    }

    if ($7 == "greeted" && $10 == "400" && $11 == "loadav") {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_hiload[host]++;
	next;
    }

    if ($7 == "greeted" && $10 == "400" && $0 ~ /[Ee][Xx][Pp][Ii][Rr]/) {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_expire[host]++;
	next;
    }

    if ($7 == "greeted" && $10 == "400") {
	##  Some other failure, or innd throttle for some local reason.
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_fail[host]++;
	next;
    }

    if ($7 == "socket():") {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_sockerr[host]++;
	next;
    }

    if ($7 == "connection" && $8 == "timed" && $9 == "out") {
	##  There is no startup marked for this.  Fake reason.
	nntplink_bpipe[host]++;
	nntplink_site[host]++;
	next;
    }

    if ($7 == "sent" && $8 == "authinfo" && $10 == "exiting") {
	##  There is no startup marked for this.
	nntplink_site[host]++;
	nntplink_auth[host]++;
    }

    if ($7 == "sent" && $8 == "IHAVE") {
	##  An "ihave failure":  site rejected the article after the
	##  IHAVE command; most likely it was out of space.  Don't
	##  count this as an offered article, so remember it.  Since
	##  nntplink keeps trying to send we could get many such
	##  failures in a single connection.  For counting purposes
	##  we pretend that there were several separate connections.
	nntplink_ihfail[host]++;

	if ($11 == "436" && $13 == "NNTP" && $15 == "out" && $17 ~ /space/) {
	    nntplink_fake_connects[host]++;
	    nntplink_nospace[host]++;
	    next;
	}
    }

    ## Since stats lines are always followed by xmit lines, we
    ## only do the nntplink_site[host]++ and nntplink=1 once
    if ($7 == "stats") {
	nntplink_offered[host] += $8 - nntplink_ihfail[host];
	nntplink_accepted[host] += $10;
	nntplink_rejected[host] += $12;
	nntplink_failed[host] += $14;
	nntplink_ihfail[host] = 0;
	if (nntplink_fake_connects[host]) {
	    nntplink_site[host] += nntplink_fake_connects[host];
	    nntplink_fake_connects[host] = 0;
	} else {
	    nntplink_site[host]++;
	}
	nntplink = 1;
	next;
    }

    if ($7 == "xmit") {
	nntplink_times_user[host] += $9;
	nntplink_times_sys[host] += $11;
	nntplink_times_elapsed[host] += $13;
	next;
    }

    if ($7 == "xfer") {
	##  We can compute this if we need it, but there isn't
	##  space in the output table for it
	#nntplink_offer_min[host] += $9;
	#nntplink_accept_min[host] += $11;
	next;
    }

    ##  503 is the code for timeouts.
    if ($11 == "503" || $12 == "Timeout") {
	nntplink_timeout[host]++;
	timeout = 1;
	next;
    }
}


##
##  NNTPD
##
$5 ~ /nntpd.*/ {
    if ( $6 ~ /.*:.*/ ) {
	n = split($6, path, ":");
	host = path[1];
    }
    else
	host = $6;

    if ($7 == "connect") {
	nntpd_connect[host]++;
	nntpd = 1;
	next;
    }

    if ($7 == "times") {
	nntpd_seconds[host] += $9 + $11;
	nntpd_elapsed[host] += $13;
	if (nntpd_connect[host] == 0)
	    nntpd_connect[host]++;
	nntpd_done[host]++;
	next;
    }

    if ($7 == "ihave_stats") {
	nntpd_accepted[host] += $9;
	nntpd_refused[host] += $11;
	nntpd_rejected[host] += $13;
	next;
    }

    if ($7 == "no" && $8 == "space") {
	nntpd_nospace[host]++;
	if (nntpd_connect[host] == 0)
	    nntpd_connect[host]++;
	nntpd_done[host]++;
	next;
    }

    if ($6 == "no" && $7 == "space") {
	next;
    }

    ##  Rest is likely to be reader stuff; skip for now.
}



##
##  MTHREADS
##
$5 ~ /mthreads.*/ {

    if ($6 == "Started" && $7 == "mthreads") {
	mthreads = 1;
	mthreads_started++;
	next;
    }

    if ($6 == "Processed" && $9 == "added" && $12 == "expired") {
	mthreads = 1;
	mthreads_added += $10;
	mthreads_expired += $13;
	next;
    }

    if ($6 == "Turned" && $8 == "groups" && $9 == "on.") {
	mthreads = 1;
	mthreads_turned_on += $7;
	next;
    }

    if ($6 == "Turned" && $8 == "groups" && $9 == "off.") {
	mthreads = 1;
	mthreads_turned_off += $7;
	next;
    }
}


##
##  UNKNOWN
##
{
    if (unknowns == 0) {
	printf("Unknown entries from news log file:\n");
	unknowns = 1;
    }
    print;
}


##
##  SUMMARIZE DATA
##  NOTE: the following are collected but not used right now:
##	innd_flush
##	innd_feed
##	innd_timeout
##
END {
    printf("\n");
    if (innd) {
	##  INND control statistics.
	if (ctlinnd_seen) {
	    printf("Ctlinnd commands:\n");
	    for ( ; ; ) {
		s = invalid;
		for (sortindex in ctlinnd_type)
		    if (ctlinnd_type[sortindex] >= 0 && sortindex < s)
			s = sortindex;
		if (s == invalid)
		    break;
		printf("%-13.13s %4d\n", s, ctlinnd_type[s]);
		ctlinnd_type[s] = -1;
	    }
	    printf("%-13.13s %4d\n", "TOTAL", ctlinnd_seen);
	    printf("\n");

	    if (ctlinnd_new) {
		printf("Newsgroups created:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_newgroups)
			if (ctlinnd_newgroups[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_newgroups[s], s);
		    sum += ctlinnd_newgroups[s];
		    ctlinnd_newgroups[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }

	    if (ctlinnd_chg) {
		printf("Newsgroups changed:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_chggroup)
			if (ctlinnd_chggroup[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_chggroup[s], s);
		    sum += ctlinnd_chggroup[s];
		    ctlinnd_chggroup[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }

	    if (ctlinnd_rmg) {
		printf("Newsgroups removed:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_rmgroups)
			if (ctlinnd_rmgroups[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_rmgroups[s], s);
		    sum += ctlinnd_rmgroups[s];
		    ctlinnd_rmgroups[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }

	    if (ctlinnd_pause) {
		printf("Pauses:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_paused)
			if (ctlinnd_paused[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_paused[s], s);
		    sum += ctlinnd_paused[s];
		    ctlinnd_paused[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }

	    if (ctlinnd_throttle) {
		printf("Throttles:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_throttled)
			if (ctlinnd_throttled[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_throttled[s], s);
		    sum += ctlinnd_throttled[s];
		    ctlinnd_throttled[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }

	    if (ctlinnd_shutdown) {
		printf("Shutdowns:\n");
		sum = 0;
		for ( ; ; ) {
		    s = invalid;
		    for (sortindex in ctlinnd_shutdowns)
			if (ctlinnd_shutdowns[sortindex] >= 0 && sortindex < s)
			    s = sortindex;
		    if (s == invalid)
			break;
		    printf("%3d %-70.70s\n", ctlinnd_shutdowns[s], s);
		    sum += ctlinnd_shutdowns[s];
		    ctlinnd_shutdowns[s] = -1;
		}
		printf("%3d %-70.70s\n", sum, "TOTAL");
		printf("\n");
	    }
	}

	##  INND exchange statistics.
	printf("Articles received by server:\n");
	printf("System             Connects  Offered    Took Tk%  Refuse  Rejct  Accpt  Elapsed\n");
	ninnd_accept = 0;
	for (s in innd_connect)
	    ninnd_accept += innd_accepted[s];
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_connect)
		if (innd_connect[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
	    if (s == invalid)
		break;

	    ninnd_connect += innd_connect[s];
	    ninnd_refuse += innd_refused[s];
	    ninnd_reject += innd_rejected[s];
	    ninnd_ela += innd_seconds[s];
	    offered = innd_accepted[s] + innd_refused[s] + innd_rejected[s];
	    ninnd_offered += offered;
	    if (offered == 0)
		offered = 1;
	    percent_accpt = (innd_accepted[s] * 100) / offered;
	    e_hours      = innd_seconds[s] / 3600;
	    e_sec        = innd_seconds[s] % 3600;
	    e_min        = e_sec / 60;
	    e_sec        %= 60;

	    printf("%-20.20s %6d  %7d %7d %3d %7d %6d  %3d%% %3d:%02d:%02d\n", 
\
		s, innd_connect[s], \
		innd_accepted[s] + innd_refused[s] + innd_rejected[s], \
		innd_accepted[s], (innd_accepted[s] * 100) / ninnd_accept, \
		innd_refused[s], innd_rejected[s], \
		percent_accpt, e_hours, e_min, e_sec);

            innd_connect[s] = -1;
	}

	e_hours      = ninnd_ela / 3600;
	e_sec        = ninnd_ela % 3600;
	e_min        = e_sec / 60;
	e_sec        %= 60;
	they_offered = ninnd_offered;
	if (they_offered == 0)
	    they_offered = 1;
	percent_accpt = (ninnd_accept * 100.0)/ (they_offered + 0.1);

	printf("%-20.20s %6d  %7d %7d %3d %7d %6d  %3d%% %3d:%02d:%02d\n\n", \
	    "TOTAL", ninnd_connect, ninnd_offered, ninnd_accept, 100, \
	    ninnd_refuse, ninnd_reject, percent_accpt, e_hours, e_min, \
	    e_sec);
    }

    ##  NNTPD statistics.
    if (nntpd) {
	printf("Articles received by NNTPD:\n");
	printf("System            Connects NSpc Fail Offered  Took Refuse Rejct  Accpt   Elapsed\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nntpd_connect)
		if (nntpd_connect[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    nnntpd_connect += nntpd_connect[s];
	    nnntpd_nospace += nntpd_nospace[s];
	    nnntpd_accept += nntpd_accepted[s];
	    nnntpd_refuse += nntpd_refused[s];
	    nnntpd_reject += nntpd_rejected[s];
	    nnntpd_ela += nntpd_elapsed[s];
	    nnntpd_done += nntpd_done[s];
	    offered = nntpd_accepted[s] + nntpd_refused[s] + nntpd_rejected[s];
	    nnntpd_offered += offered;
	    if (offered == 0)
		offered = 1;
	    percent_accpt = (nntpd_accepted[s] * 100) / offered;
	    e_hours      = nntpd_elapsed[s] / 3600;
	    e_sec        = nntpd_elapsed[s] % 3600;
	    e_min        = e_sec / 60;
	    e_sec        %= 60;

	    printf("%-20.20s %5d %4d %4d %6d %6d %6d %5d  %3d%% %3d:%02d:%02d\n", \
		s, nntpd_connect[s], nntpd_nospace[s], \
		nntpd_connect[s] - nntpd_done[s], \
		nntpd_accepted[s] + nntpd_refused[s] + nntpd_rejected[s], \
		nntpd_accepted[s], nntpd_refused[s], nntpd_rejected[s], \
		percent_accpt, e_hours, e_min, e_sec);

            nntpd_connect[s] = -1;
	}

	e_hours      = nnntpd_ela / 3600;
	e_sec        = nnntpd_ela % 3600;
	e_min        = e_sec / 60;
	e_sec        %= 60;
	they_offered = nnntpd_offered;
	if (they_offered == 0)
	    they_offered = 1;
	percent_accpt = (nnntpd_accept * 100.0)/ (they_offered + 0.1);

	printf("%-20s %5d %4d %4d %6d %6d %6d %5d  %3d%% %3d:%02d:%02d\n\n", \
	    "TOTAL", nnntpd_connect, nnntpd_nospace, \
	    nnntpd_connect - nnntpd_done, \
	    nnntpd_offered, nnntpd_accept, nnntpd_refuse, nnntpd_reject, \
	    percent_accpt, e_hours, e_min, e_sec);
    }

    ##  Innxmit statistics.
    if (innxmit) {
	printf("Articles sent by innxmit:\n");
	printf("System                  Offrd     Took     Toss   Fail  Pct    Elapsed      CPU\n");

	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innxmit_site)
		if (innxmit_offered[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    we_offered = innxmit_offered[s];
	    if (we_offered == 0)
		we_offered = 1;
	    they_take = (innxmit_accepted[s] * 100.0) / we_offered;
	    e_hours = innxmit_times_elapsed[s] / 3600;
	    e_sec   = innxmit_times_elapsed[s] % 3600;
	    e_min   = e_sec / 60;
	    e_sec   %= 60;
	    c_hours = (innxmit_times_user[s] + innxmit_times_sys[host]) / 3600;
	    c_sec   = (innxmit_times_user[s] + innxmit_times_sys[s]) % 3600;
	    c_min   = c_sec / 60;
	    c_sec   %= 60;
	    elapsed = innxmit_times_elapsed[s];
	    if (elapsed == 0)
		elapsed = 1;
	    pct = ((innxmit_times_user[s] + innxmit_times_sys[s]) * 100.0) / el
apsed;

	    printf("%-20.20s %8d %8d %8d %6d %3d%% %4d:%02d:%02d %2d:%02d:%02d\
n", \
		s, innxmit_offered[s], innxmit_accepted[s], \
		innxmit_rejected[s], innxmit_failed[s], they_take, \
		e_hours, e_min, e_sec, c_hours, c_min, c_sec);

	    ixmt        += innxmit_offered[s];
	    ixmt_accept += innxmit_accepted[s];
	    ixmt_reject += innxmit_rejected[s];
	    ixmt_failed += innxmit_failed[s];
	    ixmt_ela    += innxmit_times_elapsed[s];
	    ixmt_cpu    += innxmit_times_user[s] + innxmit_times_sys[s];

	    innxmit_offered[s] = -1;
	}

	we_offered = ixmt;
	if (we_offered == 0)
	    we_offered = 1;
	they_take = (ixmt_accept * 100) / we_offered;
	e_hours = ixmt_ela / 3600;
	e_sec   = ixmt_ela % 3600;
	e_min   = e_sec / 60;
	e_sec   %= 60;
	c_hours = ixmt_cpu / 3600;
	c_sec   = ixmt_cpu % 3600;
	c_min   = c_sec / 60;
	c_sec   %= 60;
	if (ixmt_ela == 0)
	    ixmt_ela = 1;
	pct = (ixmt_cpu * 100.0) / ixmt_ela;

	printf("%-20.20s %8d %8d %8d %6d %3d%% %4d:%02d:%02d %2d:%02d:%02d\n", 
\
	    "TOTAL", ixmt, ixmt_accept, ixmt_reject, ixmt_failed, \
	    they_take, e_hours, e_min, e_sec, c_hours, c_min, c_sec);

	printf("\n");
	printf("Transmission Connection Attempts --------------errors---------------\n");
	printf("System                Conn    Ok Auth Load Space Expire Connct Other   Pct\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innxmit_site)
		if (innxmit_site[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    tot = innxmit_site[s];
	    if (tot == 0)
	        tot = 1;
	    errs = innxmit_afail_host[s] + innxmit_hiload[s] + \
		innxmit_nospace[s] + innxmit_cfail_host[s] + \
		innxmit_expire[s] + innxmit_crefused[s];
	    ok = (innxmit_site[s] - errs);

	    printf("%-20.20s %5d %5d %4d %4d %5d  %5d  %5d %5d   %3d%%\n", \
		s, innxmit_site[s], ok, innxmit_afail_host[s], \
		innxmit_hiload[s], innxmit_nospace[s], innxmit_expire[s], \
		innxmit_cfail_host[s], innxmit_crefused[s], \
		(100.0 * ok / tot));

	    ict_tot += innxmit_site[s];
	    ict_ok  += ok;
	    ict_afail += innxmit_afail_host[s];
	    ict_hiload += innxmit_hiload[s];
	    ict_nospace += innxmit_nospace[s];
	    ict_expire += innxmit_expire[s];
	    ict_crefused += innxmit_crefused[s];
	    ict_cfail += innxmit_cfail_host[s];

	    innxmit_site[s] = -1;
	}
	tot = ict_tot;
	if (tot == 0)
	    tot = 1;
	errs = ict_afail + ict_nospace + ict_hiload + ict_cfail + ict_crefused;

	printf("%-20.20s %5d %5d %4d %4d %5d  %5d  %5d %5d   %3d%%\n\n", \
	    "TOTAL", ict_tot, ict_ok, ict_afail, ict_hiload, \
	    ict_nospace, ict_expire, ict_cfail, ict_crefused, \
	    (100.0 * ict_ok / tot));
    }

    ##  Nntplink statistics.
    if (nntplink) {
	printf("Articles sent by nntplink:\n");
	printf("System                Offrd   Took   Toss  Fail  Pct   Elapsed CPU  Pct\n");

	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nntplink_site)
		if (nntplink_offered[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    we_offered = nntplink_offered[s];
	    if (we_offered == 0)
		we_offered = 1;
	    they_take = (nntplink_accepted[s] * 100.0) / we_offered;
	    e_hours = nntplink_times_elapsed[s] / 3600;
	    e_sec   = nntplink_times_elapsed[s] % 3600;
	    e_min   = e_sec / 60;
	    e_sec   %= 60;
	    c_hours = (nntplink_times_user[s] + nntplink_times_sys[host]) / 360
0;
	    c_sec   = (nntplink_times_user[s] + nntplink_times_sys[s]) % 3600;
	    c_min   = c_sec / 60;
	    c_sec   %= 60;
	    elapsed = nntplink_times_elapsed[s];
	    if (elapsed == 0)
		elapsed = 1;
	    pct = ((nntplink_times_user[s] + nntplink_times_sys[s]) * 100.0) / elapsed;

	    printf("%-20.20s %6d %6d %6d %5d %3d%% %3d:%02d:%02d %3d:%02d:%02d %3d%%\n", \
		s, nntplink_offered[s], nntplink_accepted[s], \
		nntplink_rejected[s], nntplink_failed[s], they_take, \
		e_hours, e_min, e_sec, c_hours, c_min, c_sec, pct);

	    nxmt        += nntplink_offered[s];
	    nxmt_accept += nntplink_accepted[s];
	    nxmt_reject += nntplink_rejected[s];
	    nxmt_failed += nntplink_failed[s];
	    nxmt_ela    += nntplink_times_elapsed[s];
	    nxmt_cpu    += nntplink_times_user[s] + nntplink_times_sys[s];

	    nntplink_offered[s] = -1;
	}

	we_offered = nxmt;
	if (we_offered == 0)
	    we_offered = 1;
	they_take = (nxmt_accept * 100) / we_offered;
	e_hours = nxmt_ela / 3600;
	e_sec   = nxmt_ela % 3600;
	e_min   = e_sec / 60;
	e_sec   %= 60;
	c_hours = nxmt_cpu / 3600;
	c_sec   = nxmt_cpu % 3600;
	c_min   = c_sec / 60;
	c_sec   %= 60;
	if (nxmt_ela == 0)
	    nxmt_ela = 1;
	pct = (nxmt_cpu * 100.0) / nxmt_ela;

	printf("%-20.20s %6d %6d %6d %5d %3d%% %3d:%02d:%02d %3d:%02d:%02d %3d%%\n\n", \
	    "TOTAL", nxmt, nxmt_accept, nxmt_reject, nxmt_failed, \
	    they_take, e_hours, e_min, e_sec, c_hours, c_min, c_sec, pct);

	printf("Transmission Connection Attempts         ------errors-------:\n");
	printf("System               Conn   Ok EOF Sock Load Bpipe Space  Exp Auth Other  Pct\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nntplink_site)
		if (nntplink_site[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    tot = nntplink_site[s];
	    if (tot == 0)
		tot = 1;
	    errs = nntplink_eof[s] + nntplink_sockerr[s] + \
		 nntplink_hiload[s] + nntplink_bpipe[s] + \
		 nntplink_nospace[s] + nntplink_auth[s] + \
		 nntplink_expire[s] + nntplink_fail[s];
	    ok = (nntplink_site[s] - errs);

	    printf("%-20.20s %4d %4d %3d %4d %4d %5d %5d %4d %4d %5d  %3d%%\n", \
		s, nntplink_site[s], ok, nntplink_eof[s], \
		nntplink_sockerr[s], nntplink_hiload[s], \
		nntplink_bpipe[s], nntplink_nospace[s], \
		nntplink_expire[s], nntplink_auth[s], nntplink_fail[s], \
		(100.0 * ok / tot));

	    ct_tot += nntplink_site[s];
	    ct_ok  += ok;
	    ct_eof += nntplink_eof[s];
	    ct_sockerr += nntplink_sockerr[s];
	    ct_hiload += nntplink_hiload[s];
	    ct_bpipe += nntplink_bpipe[s];
	    ct_nospace += nntplink_nospace[s];
	    ct_auth += nntplink_auth[s];
	    ct_expire += nntplink_expire[s];
	    ct_fail += nntplink_fail[s];

	    nntplink_site[s] = -1;
	}
	tot = ct_tot;
	if (tot == 0)
	    tot = 1;

	printf("%-20.20s %4d %4d %3d %4d %4d %5d %5d %4d %4d %5d  %3d%%\n\n", \
	    "TOTAL", ct_tot, ct_ok, ct_eof, ct_sockerr, ct_hiload, \
	    ct_bpipe, ct_nospace, ct_expire, ct_auth, ct_fail, \
	    (100.0 * ct_ok / tot));
    }

    ##  Batcher statistics.
    if (batcher) {
	printf("UUCP batches created:\n");
	printf("System                Offrd   Arts      Bytes   Elapsed       Cpu  Pct\n");

	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in batcher_site)
		if (batcher_site[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    e_hours = batcher_times_elapsed[s] / 3600;
	    e_sec   = batcher_times_elapsed[s] % 3600;
	    e_min   = e_sec / 60;
	    e_sec   %= 60;
	    c_hours = (batcher_times_user[s] + batcher_times_sys[s]) / 3600;
	    c_sec   = (batcher_times_user[s] + batcher_times_sys[s]) % 3600;
	    c_min   = c_sec / 60;
	    c_sec   %= 60;
	    elapsed = batcher_times_elapsed[s];
	    if (elapsed == 0)
		elapsed = 1;
	    pct = ((batcher_times_user[s] + batcher_times_sys[s]) * 100.0) / elapsed;

	    printf("%-20.20s %6d %6d %10d %3d:%02d:%02d %3d:%02d:%02d %3d%%\n", \
		s, batcher_num[s], batcher_articles[s], batcher_bytes[s], \
		e_hours, e_min, e_sec, c_hours, c_min, c_sec, pct);

	    nbatch          += batcher_num[s];
	    nbatch_articles += batcher_articles[s];
	    nbatch_bytes    += batcher_bytes[s];
	    nbatch_ela      += batcher_times_elapsed[s];
	    nbatch_cpu      += batcher_times_user[s] + batcher_times_sys[s];

	    batcher_site[s] = -1;
	}

	e_hours = nbatch_ela / 3600;
	e_sec   = nbatch_ela % 3600;
	e_min   = e_sec / 60;
	e_sec   %= 60;
	c_hours = nbatch_cpu / 3600;
	c_sec   = nbatch_cpu % 3600;
	c_min   = c_sec / 60;
	c_sec   %= 60;
	if (nbatch_ela == 0)
	    nbatch_ela = 1;
	pct = (nbatch_cpu * 100.0) / nbatch_ela;

	printf("%-20.20s %6d %6d %10d %3d:%02d:%02d %3d:%02d:%02d %3d%%\n\n",\
	    "TOTAL", nbatch, nbatch_articles, nbatch_bytes,\
	    e_hours, e_min, e_sec, c_hours, c_min, c_sec, pct);
    }

    ##  Rnews statistics.
    if (rnews_host) {
	printf("Rnews articles offered from:\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in rnews_hosts)
		if (rnews_hosts[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("\t%6d\t%s\n", rnews_hosts[s], s);

	    rnews_hosts[s] = -1;
        }
	printf("\n");
    }
    if (rnews_rejects) {
	printf("Rnews connections rejected %d times\n", rnews_rejects);
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in rnews_r_reject)
		if (rnews_r_reject[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("\t%6d\t%s\n", rnews_r_reject[s], s);

	    rnews_r_reject[s] = -1;
        }
	printf("\n");
    }
    if (rnews_bad) {
	printf("Rnews bad articles: (Total %d)\n", rnews_bad);
	if (rnews_bad_ng) {
	    printf("Bad newsgroup: %d\n", rnews_bad_ng);
	    for ( ; ; ) {
		s = invalid;
		for (sortindex in rnews_bng)
		    if (rnews_bng[sortindex] >= 0 && sortindex < s)
			s = sortindex;
                if (s == invalid)
		    break;

		printf("\t%5d: %s\n", rnews_bng[s], s);

		rnews_bng[s] = -1;
            }
	}
	if (rnews_bad_dist) {
	    printf("Bad distribution: %d\n", rnews_bad_dist);
	    for ( ; ; ) {
		s = invalid;
		for (sortindex in rnews_bdist)
		    if (rnews_bdist[sortindex] >= 0 && sortindex < s)
			s = sortindex;
                if (s == invalid)
		    break;

		printf("\t%5d: %s\n", rnews_bdist[s], s);

		rnews_bdist[s] = -1;
            }
	}
	if (rnews_bad_date) {
	    printf("Bad date: %d\n", rnews_bad_date);
	}
	printf("\n");
    }

    ##  INND 437 stats per host
    if (innd_437) {
	printf("Rejected articles by server:\n");
	printf("System               Future BadDate Duplicat TooOld Unapprv Unw Grp UnwDst ???\n");
	tot_future = 0;
	tot_baddate = 0;
	tot_duplicate = 0;
	tot_tooold = 0;
	tot_unapproved = 0;
	tot_unnewsgrp = 0;
	tot_undistribution = 0;
	tot_other437 = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_437host)
		if (innd_437host[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
	    if (s == invalid)
		break;

	    tot_future += innd_future[s];
	    tot_baddate += innd_baddate[s];
	    tot_duplicate += innd_duplicate[s];
	    tot_tooold += innd_tooold[s];
	    tot_unapproved += innd_unapproved[s];
	    tot_unnewsgrp += innd_unnewsgrp[s];
	    tot_undistribution += innd_undistribution[s];
	    tot_other437 += innd_other437[s];

	    printf("%-20.20s %6d %7d %8d %6d %7d %6d %6d %3d\n", \
	      s, innd_future[s], innd_baddate[s], innd_duplicate[s], \
	      innd_tooold[s], innd_unapproved[s], innd_unnewsgrp[s], \
	      innd_undistribution[s], innd_other437[s]);

	    innd_437host[s] = -1;
        }
	printf("%-20.20s %6d %7d %8d %6d %7d %6d %6d %3d\n", \
	    "TOTAL", tot_future, tot_baddate, tot_duplicate, \
	    tot_tooold, tot_unapproved, tot_unnewsgrp, \
	    tot_undistribution, tot_other437);
	printf("\n");
    }

    ##  Miscellaneous innd statistics.
    if (innd_bad_msgids) {
	printf("Bad Message-ID's offered:\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_bad_msgid)
		if (innd_bad_msgid[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
	    if (s == invalid)
		break;

	    printf("%-20.20s %5d\n", s, innd_bad_msgid[s]);

	    innd_bad_msgid[s] = -1;
        }
	printf("\n");
    }
    if (innd_bad_ihaves) {
	printf("Bad ihave control messages received:\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_bad_ihave)
	        if (innd_bad_ihave[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-20.20s %5d\n", s, innd_bad_ihave[s]);

	    innd_bad_ihave[s] = -1;
        }
	printf("\n");
    }
    if (innd_bad_sendmes) {
	printf("Ignored sendme control messages received:\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_bad_sendme)
		if (innd_bad_sendme[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-20.20s %5d\n", s, innd_bad_sendme[s]);

	    innd_bad_sendme[s] = -1;
        }
	printf("\n");
    }
    if (innd_blockeds) {
	printf("Blocked server feeds:\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_blocked)
		if (innd_blocked[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-20.20s %5d\n", s, innd_blocked[s]);

	    innd_blocked[s] = -1;
        }
	printf("\n");
    }
    if (innd_CCreader) {
	printf("Cant sendto CCreader: %d\n", innd_CCreader);
	printf("\n");
    }
    if (innd_505_flag) {
	printf("Hosts making too many connects per minute\n");
	printf(" Conn System\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in innd_505s)
		if (innd_505s[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
	    if (s == invalid)
		break;

	    printf("%5d %s\n", innd_505s[s], s);

	    innd_505s[s] = -1;
	}
	printf("\n");
    }


    ##  NNRP statistics.
    if (nnrp && print_nnrp_readers_stats) {
	printf("NNRP readership statistics:\n");
	printf("System               Conn Articles Groups Post  Rej Auth  Elapsed      CPU  Pct\n");

	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_connect)
		if (nnrp_connect[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	   ##  Report curious pokers elsewhere.
	    if (nnrp_groups[s] == 0	\
	     && nnrp_articles[s] == 0	\
	     && nnrp_posts[s] == 0) {
		nnrp_curious[s] += nnrp_connect[s];
		curious = 1;
		nnrp_connect[s] = -1;
		continue;
	    }

	    nconn += nnrp_connect[s];
	    nauth += nnrp_authhosts[s];
	    nart += nnrp_articles[s];
	    ngrp += nnrp_groups[s];
	    npost += nnrp_posts[s];
	    nrej += nnrp_rejected[s];
	    ncpu += (nnrp_times_user[s] + nnrp_times_sys[s]);
	    nela += nnrp_times_elapsed[s];
	    e_hours      = nnrp_times_elapsed[s] / 3600;
	    e_sec        = nnrp_times_elapsed[s] % 3600;
	    e_min        = e_sec / 60;
	    e_sec        %= 60;
	    c_hours      = (nnrp_times_user[s] + nnrp_times_sys[s]) / 3600;
	    c_sec        = (nnrp_times_user[s] + nnrp_times_sys[s]) % 3600;
	    c_min        = c_sec / 60;
	    c_sec        %= 60;
	    elapsed = nnrp_times_elapsed[s];
	    if (elapsed == 0)
		elapsed = 1;
	    pct = ((nnrp_times_user[s] + nnrp_times_sys[s]) * 100.0) / elapsed;

	    printf("%-19.19s %5d %8d %6d %4d %4d %4d %2d:%02d:%02d %2d:%02d:%02d %3d%%\n",\
		s, nnrp_connect[s], nnrp_articles[s], nnrp_groups[s],\
		nnrp_posts[s], nnrp_rejected[s], nnrp_authhosts[s], e_hours,\
		e_min, e_sec, c_hours, c_min, c_sec, pct);

            nnrp_connect[s] = -1;
	}

	e_hours      = nela / 3600;
	e_sec        = nela % 3600;
	e_min        = e_sec / 60;
	e_sec        %= 60;
	c_hours      = ncpu / 3600;
	c_sec        = ncpu % 3600;
	c_min        = c_sec / 60;
	c_sec        %= 60;
	if (nela == 0)
	    nela = 1;
	pct = (ncpu * 100.0) / nela;

	printf("%-19.19s %5d %8d %6d %4d %4d %4d %2d:%02d:%02d %2d:%02d:%02d %3d%%\n\n",\
	    "TOTAL", nconn, nart, ngrp, npost, nrej, nauth, e_hours, e_min, \
	    e_sec, c_hours, c_min, c_sec, pct);
    }

    ##  Miscellaneous NNRP statistics.
    if (curious && print_nnrp_readers_stats) {
	printf("Curious NNRP server explorers:\n");
	printf("System                          Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_curious)
		if (nnrp_curious[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_curious[s]);
	    sum += nnrp_curious[s];

	    nnrp_curious[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }
    if (nnrp_noperms) {
	printf("NNRP no permission clients:\n");
	printf("System                          Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_noperm)
		if (nnrp_noperm[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_noperm[s]);
	    sum += nnrp_noperm[s];

	    nnrp_noperm[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }
    if (nnrp_authcmds) {
	printf("NNRP authentication requests:\n");
	printf("System                         User            Count      Bad\n");
	tauth = 0;
	tbadauth = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_auth)
		if (nnrp_auth[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;
	    
	    n = split(s, ar, ":");

	    printf("%-30.30s %-10.10s      %5d    %5d\n", ar[1], ar[2],\
		nnrp_auth[s], nnrp_badauth[s]);
	    tauth += nnrp_auth[s];
	    tbadauth += nnrp_badauth[s];

	    nnrp_auth[s] = -1;
        }
	printf("%-30.30s %-10.10s      %5d    %5d\n", "TOTAL", "",\
	    tauth, tbadauth);
	printf("\n");
    }
    if (nnrp_unrecs && print_nnrp_readers_stats) {
	printf("NNRP unrecognized commands:\n");
	printf("System                          Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_unrec)
		if (nnrp_unrec[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_unrec[s]);
	    sum += nnrp_unrec[s];

	    nnrp_unrec[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
	printf("NNRP unrecognized command counts:\n");
	printf("Command                         Count\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_unreccmd)
		if (nnrp_unreccmd[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_unreccmd[s]);
	    sum += nnrp_unreccmd[s];

	    nnrp_unreccmd[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }
    if (nnrp_gethostfails) {
	printf("NNRP gethostbyname failures:\n");
	printf("IP                              Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_gethostfail)
		if (nnrp_gethostfail[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_gethostfail[s]);
	    sum += nnrp_gethostfail[s];

	    nnrp_gethostfail[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }
    if (0) {
	printf("NNRP client timeouts:\n");
	printf("System                          Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_timeout)
		if (nnrp_timeout[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nnrp_timeout[s]);
	    sum += nnrp_timeout[s];

	    nnrp_timeout[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }
    # Change the ``0'' to ``client_short'' if you want to see the report
    if (0) {
	printf("NNRP clients which timed out on the first read:\n");
	printf(" Conn System\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_short)
		if (nnrp_short[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%5d %s\n", nnrp_short[s], s);
	    sum += nnrp_short[s];

	    nnrp_short[s] = -1;
        }
	printf("%5d TOTAL\n", sum);
	printf("\n");
    }
    if (server_timeout) {
	printf("NNTPLINK remote server timeouts:\n");
	printf("System                          Conn\n");
	sum = 0;
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nntplink_timeout)
		if (nntplink_timeout[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf("%-30.30s %5d\n", s, nntplink_timeout[s]);
	    sum += nntplink_timeout[s];

	    nntplink_timeout[s] = -1;
        }
	printf("%-30.30s %5d\n", "TOTAL", sum);
	printf("\n");
    }

    ##  MTHREADS statistics.
    if (mthreads) {
	printf("Mthreads:   Starts   Groups on  Groups off    Articles     Expired:\n");
	printf("%18d%12d%12d%12d%12d\n", mthreads_started, mthreads_turned_on, 
\
		mthreads_turned_off, mthreads_added, mthreads_expired);
	printf("\n");
    }

    ##  Group readership statistics.
    if (nnrp_readers && print_nnrp_readers_stats) {
	for (g in nnrp_group_request) {
	    x = length(g);
	    if (x > max)
		max = x;
	    i = index(g, ".");
	    if (i > 0)
		top = substr(g, 1, i - 1);
	    else
		top = g;
	    category[top] += nnrp_group_request[g];
	}
	fmt = sprintf("%%-%ds %%5d\n", max);

	printf("Newsgroup request counts (by category):\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in category)
		if (category[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf(fmt, s, category[s]);

	    category[s] = -1;
        }
	printf("\n");

	printf("Newsgroup request counts (by newsgroup):\n");
	for ( ; ; ) {
	    s = invalid;
	    for (sortindex in nnrp_group_request)
		if (nnrp_group_request[sortindex] >= 0 && sortindex < s)
		    s = sortindex;
            if (s == invalid)
		break;

	    printf(fmt, s, nnrp_group_request[s]);

	    nnrp_group_request[s] = -1;
        }
	printf("\n");
    }

    printf("\n");
}
