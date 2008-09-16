##########################################################################
# INN module for innreport (3.*).
#
# Sample file tested with INN 2.5, 2.4, 2.3, 2.2, 1.7.2 and 1.5.1.
#
# (c) 1997-2001 by Fabien Tassin <fta@sofaraway.org>.
# See innreport for more information.
#
# Version 3.1.0.
#
##########################################################################

# TODO: add the map file.

package innreport_inn;
use strict;

my $MIN = 1E10;
my $MAX = -1;

my %ctlinnd = ('a', 'addhist',     'D', 'allow',
	       'b', 'begin',       'c', 'cancel',
	       'u', 'changegroup', 'd', 'checkfile',
	       'e', 'drop',        'f', 'flush',
	       'g', 'flushlogs',   'h', 'go',
	       'i', 'hangup',      's', 'mode',
	       'j', 'name',        'k', 'newgroup',
	       'l', 'param',       'm', 'pause',
	       'v', 'readers',     't', 'refile',
	       'C', 'reject',      'o', 'reload',
	       'n', 'renumber',    'z', 'reserve',
	       'p', 'rmgroup',     'A', 'send',
	       'q', 'shutdown',    'B', 'signal',
	       'r', 'throttle',    'w', 'trace',
	       'x', 'xabort',      'y', 'xexec',
	       'E', 'logmode',     'F', 'feedinfo',
	       'T', 'filter',      'P', 'perl',);

my %timer_names = (idle     => 'idle',
                   hishave  => 'history lookup',
                   hisgrep  => 'history grep',
                   hiswrite => 'history write',
                   hissync  => 'history sync',
                   nntpread => 'nntp read',
                   artparse => 'article parse',
                   artclean => 'article cleanup',
                   artwrite => 'article write',
                   artcncl  => 'article cancel',
                   artlog   => 'article logging',
                   sitesend => 'site send',
                   overv    => 'overview write',
                   perl     => 'perl filter',
                   python   => 'python filter',
                   datamove => 'data move'
);

my %innfeed_timer_names = (
                   'idle'    => 'idle',
		   'blstats' => 'backlog stats',
		   'stsfile' => 'status file',
		   'newart'  => 'article new',
		   'prepart' => 'article prepare',
		   'readart' => 'article read',
		   'read'    => 'data read',
		   'write'   => 'data write',
		   'cb'      => 'callbacks',
);

my %nnrpd_timer_names = (
                   'idle'    => 'idle',
                   'newnews' => 'newnews',
);

our %batcher_articles;
our %batcher_bytes;
our %batcher_elapsed;
our %batcher_offered;
our %cnfsstat;
our %cnfsstat_cycles;
our %cnfsstat_rate;
our %cnfsstat_samples;
our %cnfsstat_size;
our %cnfsstat_time;
our %cnfsstat_used;
our %controlchan_doit;
our %controlchan_ihave_site;
our %controlchan_new;
our %controlchan_ok;
our %controlchan_other;
our %controlchan_rm;
our %controlchan_sendme_site;
our %controlchan_skippgp;
our %controlchan_who;
our %crosspost;
our $crosspost_time;
our %crosspost_times;
our %inn_badart;
our %innd_accepted;
our %innd_accepted_sum;
our %innd_bad_command;
our %innd_bad_ihave;
our %innd_bad_msgid;
our %innd_bad_newsgroup;
our %innd_bad_sendme;
our %innd_blocked;
our %innd_cache;
our %innd_changegroup;
our %innd_connect;
our %innd_connect_sum;
our %innd_control;
our %innd_duplicated_size;
our %innd_duplicated_size_sum;
our %innd_filter_perl;
our %innd_filter_python;
our %innd_his;
our %innd_huge;
our %innd_max_conn;
our %innd_misc;
our %innd_misc_stat;
our %innd_newgroup;
our %innd_no_colon_space;
our %innd_no_permission;
our %innd_offered;
our %innd_offered_size;
our %innd_offered_size_sum;
our %innd_offered_sum;
our %innd_others;
our %innd_posted_future;
our %innd_refused;
our %innd_refused_sum;
our %innd_rejected;
our %innd_rejected_size;
our %innd_rejected_size_sum;
our %innd_rejected_sum;
our %innd_rmgroup;
our %innd_seconds;
our %innd_seconds_sum;
our %innd_stored_size;
our %innd_stored_size_sum;
our %innd_strange_strings;
our %innd_time_max;
our %innd_time_min;
our %innd_time_num;
our %innd_time_time;
our $innd_time_times;
our %innd_too_many_connects_per_minute;
our %inn_duplicate;
our %innfeed_accepted;
our %innfeed_accepted_size;
our %innfeed_connect;
our %innfeed_missing;
our %innfeed_offered;
our %innfeed_refused;
our %innfeed_rejected;
our %innfeed_rejected_size;
our %innfeed_seconds;
our %innfeed_shrunk;
our %innfeed_spooled;
our %innfeed_time_max;
our %innfeed_time_min;
our %innfeed_time_num;
our %innfeed_time_time;
our $innfeed_time_times;
our %inn_flow;
our %inn_flow_labels;
our %inn_flow_size;
our $inn_flow_size_total;
our %inn_flow_time;
our $inn_flow_total;
our %inn_linecount;
our %inn_site_path;
our %inn_tooold;
our %inn_unapproved;
our %inn_unapproved_g;
our %inn_uw_dist;
our %inn_uw_dist_s;
our %inn_uw_ng;
our %inn_uw_ng_s;
our %inn_uw_site;
our %innxmit_accepted;
our %innxmit_accepted_size;
our %innxmit_afail_host;
our %innxmit_badart;
our %innxmit_cfail_host;
our %innxmit_crefused;
our %innxmit_duplicate;
our %innxmit_expire;
our %innxmit_hiload;
our %innxmit_ihfail;
our %innxmit_linecount;
our %innxmit_missing;
our %innxmit_nospace;
our %innxmit_offered;
our %innxmit_others;
our %innxmit_refused;
our %innxmit_rejected;
our %innxmit_rejected_size;
our %innxmit_site;
our %innxmit_times;
our %innxmit_tooold;
our %innxmit_unapproved;
our %innxmit_unapproved_g;
our %innxmit_uw_dist;
our %innxmit_uw_dist_s;
our %innxmit_uw_ng;
our %innxmit_uw_ng_s;
our %innxmit_uw_site;
our %nnrpd_articles;
our %nnrpd_auth;
our %nnrpd_bytes;
our %nnrpd_connect;
our %nnrpd_curious;
our %nnrpd_dom_articles;
our %nnrpd_dom_bytes;
our %nnrpd_dom_connect;
our %nnrpd_dom_groups;
our %nnrpd_dom_no_permission;
our %nnrpd_dom_post_ok;
our %nnrpd_dom_post_rej;
our %nnrpd_dom_reset_peer;
our %nnrpd_dom_timeout;
our %nnrpd_dom_times;
our %nnrpd_dom_unrecognized;
our %nnrpd_gethostbyaddr;
our %nnrpd_group;
our %nnrpd_groups;
our %nnrpd_hierarchy;
our %nnrpd_no_permission;
our %nnrpd_post_error;
our %nnrpd_post_ok;
our %nnrpd_post_rej;
our %nnrpd_reset_peer;
our %nnrpd_resource_elapsed;
our %nnrpd_resource_idle;
our %nnrpd_resource_system;
our %nnrpd_resource_user;
our %nnrpd_sys_times;
our %nnrpd_time_max;
our %nnrpd_time_min;
our %nnrpd_time_num;
our %nnrpd_timeout;
our %nnrpd_times;
our %nnrpd_time_time;
our $nnrpd_time_times;
our %nnrpd_unrecogn_cmd;
our %nnrpd_unrecognized;
our %nnrpd_usr_times;
our %nntplink_accepted;
our %nntplink_auth;
our %nntplink_bpipe;
our %nntplink_connects;
our %nntplink_eof;
our %nntplink_expire;
our %nntplink_fail;
our %nntplink_failed;
our %nntplink_fake_connects;
our %nntplink_hiload;
our %nntplink_ihfail;
our %nntplink_nospace;
our %nntplink_offered;
our %nntplink_rejected;
our %nntplink_selecterr;
our %nntplink_site;
our %nntplink_sockerr;
our %nntplink_times;
our %nocem_badsigs;
our %nocem_goodsigs;
our $nocem_lastid;
our $nocem_newids;
our %nocem_newids;
our $nocem_totalbad;
our $nocem_totalgood;
our $nocem_totalids;
our %nocem_totalids;
our %rnews_bogus_date;
our %rnews_bogus_dist;
our %rnews_bogus_ng;
our $rnews_duplicate;
our %rnews_host;
our $rnews_linecount;
our %rnews_misc;
our $rnews_no_colon_space;
our %rnews_rejected;
our $rnews_too_old;
our %rnews_unapproved;
our $server;

# init innd timer
foreach (values %timer_names) {
  $innd_time_min{$_} = $MIN;
  $innd_time_max{$_} = $MAX;
  $innd_time_time{$_} = 0;   # to avoid a warning... Perl < 5.004
  $innd_time_num{$_} = 0;    # ...
}
$innd_time_times = 0;        # ...

# init innfeed timer
foreach (values %innfeed_timer_names) {
  $innfeed_time_min{$_} = $MIN;
  $innfeed_time_max{$_} = $MAX;
  $innfeed_time_time{$_} = 0;   # to avoid a warning... Perl < 5.004
  $innfeed_time_num{$_} = 0;    # ...
}
$innfeed_time_times = 0;        # ...

# init nnrpd timer
foreach (values %nnrpd_timer_names) {
  $nnrpd_time_min{$_} = $MIN;
  $nnrpd_time_max{$_} = $MAX;
  $nnrpd_time_time{$_} = 0;   # to avoid a warning... Perl < 5.004
  $nnrpd_time_num{$_} = 0;    # ...
}
$nnrpd_time_times = 0;        # ...

my %notice_state;
my %notice_state_ever_closed;
my %innd_cp_accepted;
my %innd_cp_refused;
my %innd_cp_rejected;
my %innd_cp_seconds;
my %innd_cp_stored_size;
my %innd_cp_duplicated_size;
my %innd_cp_rejected_size;

# collect: Used to collect the data.
sub collect($$$$$$) {
  my ($day, $hour, $prog, $res, $left, $CASE_SENSITIVE) = @_;

  return 1 if $left =~ /Reading config from (\S+)$/o;

  ########
  ## inn (from the "news" log file - not from "news.notice")
  ##
  if ($prog eq "inn") {
    # accepted article
    if ($res =~ m/[\+j]/o) {
      $hour =~ s/:.*$//o;
      $inn_flow{"$day $hour"}++;
      $inn_flow_total++;

      # Memorize the size. This can only be done with INN >= 1.5xx and
      # DO_LOG_SIZE = DO.

      # server <msg-id> size [feeds]
      # or
      # server <msg-id> (filename) size [feeds]

      my ($s) = $left =~ /^\S+ \S+ (?:\(\S+\) )?(\d+)(?: |$)/o;
      if ($s) {
	$inn_flow_size{"$day $hour"} += $s;
	$inn_flow_size_total += $s;
      }
      return 1;
    }

    # 437 Duplicate article
    if ($left =~ /(\S+) <[^>]+> 437 Duplicate(?: article)?$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_duplicate{$server}++;
      return 1;
    }
    # 437 Unapproved for
    if ($left =~ /(\S+) <[^>]+> 437 Unapproved for \"([^\"]+)\"$/o) {
      my ($server, $group) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_unapproved{$server}++;
      $inn_unapproved_g{$group}++;
      return 1;
    }
    # 437 Too old -- ...
    if ($left =~ /(\S+) <[^>]+> 437 Too old -- /o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_tooold{$server}++;
      return 1;
    }
    # 437 Unwanted site ... in path
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted site (\S+) in path$/o) {
      my ($server, $site) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_uw_site{$server}++;
      $inn_site_path{$site}++;
      return 1;
    }
    # 437 Unwanted newsgroup "..."
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted newsgroup \"(\S+)\"$/o) {
      my ($server, $group) = ($1, $2);
      ($group) = split(/,/, $group);
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_uw_ng_s{$server}++;
      $inn_uw_ng{$group}++;
      return 1;
    }
    # 437 Unwanted distribution "..."
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted distribution \"(\S+)\"$/o) {
      my ($server, $dist) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_uw_dist_s{$server}++;
      $inn_uw_dist{$dist}++;
      return 1;
    }
    # 437 Linecount x != y +- z
    if ($left =~ /(\S+) <[^>]+> 437 Linecount/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $inn_linecount{$server}++;
      return 1;
    }
    # 437 No colon-space in "xxxx" header
    if ($left =~ /(\S+) <[^>]+> 437 No colon-space in \"[^\"]+\" header/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $innd_others{$server}++;
      $innd_no_colon_space{$server}++;
      return 1;
    }
    # 437 Article posted in the future -- "xxxxx"
    if ($left =~ /(\S+) <[^>]+> 437 Article posted in the future -- \"[^\"]+\"/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_posted_future{$server}++;
      $innd_others{$server}++;
      $inn_badart{$server}++;
      return 1;
    }
    # 437 article includes "....."
    if ($left =~ /(\S+) <[^>]+> 437 article includes/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_strange_strings{$server}++;
      $innd_others{$server}++;
      $inn_badart{$server}++;
      return 1;
    }
    # Cancelling <...>
    if ($left =~ /(\S+) <[^>]+> Cancelling/o) {
      return 1;
    }
    # all others are just counted as "Other"
    if ($left =~ /(\S+) /o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $inn_badart{$server}++;
      $innd_others{$server}++;
      return 1;
    }
  }

  ########
  ## innd
  if ($prog eq "innd") {
    ## Note for innd logs:
    ## there's a lot of entries detected but still not used
    ## (because of a lack of interest).

    # think it's a dotquad
    return 1 if $left =~ /^think it\'s a dotquad$/o;
    if ($left =~ /^SERVER /o) {
      # SERVER perl filtering enabled
      return 1 if $left =~ /^SERVER perl filtering enabled$/o;
      # SERVER perl filtering disabled
      return 1 if $left =~ /^SERVER perl filtering disabled$/o;
      # SERVER Python filtering enabled
      return 1 if $left =~ /^SERVER Python filtering enabled$/o;
      # SERVER Python filtering disabled
      return 1 if $left =~ /^SERVER Python filtering disabled$/o;
      # SERVER cancelled +id
      return 1 if $left =~ /^SERVER cancelled /o;
    }
    # Python filter
    return 1 if $left =~ /^defined python methods$/o;
    return 1 if $left =~ /^reloading pyfilter$/o;
    return 1 if $left =~ /^reloaded pyfilter OK$/o;
    return 1 if $left =~ /^python interpreter initialized OK$/o;
    return 1 if $left =~ /^python method \w+ not found$/o; 
    return 1 if $left =~ /^python: First load, so I can do initialization stuff\.$/o;
    return 1 if $left =~ /^python: filter_before_reload executing\.\.\.$/o;
    return 1 if $left =~ /^python: I\'m just reloading, so skip the formalities\.$/o;
    return 1 if $left =~ /^python: spamfilter successfully hooked into INN$/o;
    return 1 if $left =~ /^python: state change from \w+ to \w+ - /o;
    return 1 if $left =~ /^python: filter_close running, bye!$/o;
    # rejecting[perl]
    if ($left =~ /^rejecting\[perl\] <[^>]+> \d+ (.*)/o) {
      $innd_filter_perl{$1}++;
      return 1;
    }
    # rejecting[python]
    if ($left =~ /^rejecting\[python\] <[^>]+> \d+ (.*)/o) {
      $innd_filter_python{$1}++;
      return 1;
    }
    # closed lost
    return 1 if $left =~ /^\S+ closed lost \d+/o;
    # new control command
    if ($left =~ /^ctlinnd command (\w)(:.*)?/o) {
      my $command = $1;
      my $cmd = $ctlinnd{$command};
      $cmd = $command unless $cmd;
      return 1 if $cmd eq 'flush'; # to avoid a double count
      $innd_control{"$cmd"}++;
      return 1;
    }
    # old control command (by letter)
    if ($left =~ /^(\w)$/o) {
      my $command = $1;
      my $cmd = $ctlinnd{$command};
      $cmd = $command unless $cmd;
      return 1 if $cmd eq 'flush'; # to avoid a double count
      $innd_control{"$cmd"}++;
      return 1;
    }
    # old control command (letter + reason)
    if ($left =~ /^(\w):.*$/o) {
      my $command = $1;
      my $cmd = $ctlinnd{$command};
      $cmd = $command unless $cmd;
      return 1 if $cmd eq 'flush'; # to avoid a double count
      $innd_control{"$cmd"}++;
      return 1;
    }
    # opened
    return 1 if $left =~ /\S+ opened \S+:\d+:file$/o;
    # buffered
    return 1 if $left =~ /\S+ buffered$/o;
    # spawned
    return 1 if $left =~ /\S+ spawned \S+:\d+:proc:\d+$/o;
    return 1 if $left =~ /\S+ spawned \S+:\d+:file$/o;
    # running
    return 1 if $left =~ /\S+ running$/o;
    # sleeping
    if ($left =~ /(\S+):\d+:proc:\d+ sleeping$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_blocked{$server}++;
      return 1;
    }
    # blocked sleeping
    if ($left =~ /(\S+):\d+:proc:\d+ blocked sleeping/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_blocked{$server}++;
      return 1;
    }
    if ($left =~ /(\S+):\d+ blocked sleeping/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_blocked{$server}++;
      return 1;
    }
    # restarted
    return 1 if $left =~ m/^\S+ restarted$/o;
    # starting
    return 1 if $left =~ m/^\S+ starting$/o;
    # readclose
    return 1 if $left =~ m/^\S+:\d+ readclose+$/o;
    # rejected 502
    if ($left =~ m/^(\S+) rejected 502$/) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_no_permission{$server}++;
      return 1;
    }
    # rejected 505
    if ($left =~ m/^(\S+) rejected 505$/) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_too_many_connects_per_minute{$server}++;
      return 1;
    }
    # connected
    #
    # Record <server>:<channel> instead of just <server> as otherwise we may
    # miss some persistent connection newsfeeds that in any given innreport
    # reporting period may not record any connect entries.  We'll accumulate
    # these into totals at the end of processing.
    if ($left =~ /^(\S+) connected (\d+)/o) {
      my $server = "$1:$2";
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_connect{$server}++;
      return 1;
    }

    # logstatus (information written in inn_status.html)
    return 1 if ($left =~ /\S+ status seconds \d+ accepted \d+ refused \d+ rejected \d+ duplicate \d+ accepted size \d+ duplicate size \d+(?: rejected size \d+)?$/o);

    # closed/checkpoint (with times)
    #
    # For checkpoints, store the current checkpoint numbers; if we still have
    # checkpoint numbers left over at the end of the run, we'll add them to
    # our totals.  On a closed, clear any checkpoint numbers and add the close
    # numbers to the totals.  Basically, we want to ignore checkpoints unless
    # we don't get a close before the end of the log.
    if ($left =~ /(\S+:\d+) (checkpoint|closed) seconds (\d+) accepted (\d+) refused (\d+) rejected (\d+) duplicate (\d+) accepted size (\d+) duplicate size (\d+)(?: rejected size (\d+))?$/o) {
      my ($server, $status, $seconds, $accepted, $refused, $rejected, $duplicate, $accptsize, $dupsize, $rjctsize) =
	($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
      $server = lc $server unless $CASE_SENSITIVE;
      $notice_state{$server} = $status;

      if ($status eq 'checkpoint') {
        $innd_cp_accepted{$server}        = $accepted;
        $innd_cp_refused{$server}         = $refused;
        $innd_cp_rejected{$server}        = $rejected;
        $innd_cp_seconds{$server}         = $seconds;
        $innd_cp_stored_size{$server}     = $accptsize;
        $innd_cp_duplicated_size{$server} = $dupsize;
        $innd_cp_rejected_size{$server}   = ($rjctsize || 0);
      } elsif ($status eq "closed") {
        $innd_cp_accepted{$server}        = 0;
        $innd_cp_refused{$server}         = 0;
        $innd_cp_rejected{$server}        = 0;
        $innd_cp_seconds{$server}         = 0;
        $innd_cp_stored_size{$server}     = 0;
        $innd_cp_duplicated_size{$server} = 0;
        $innd_cp_rejected_size{$server}   = 0;

        $notice_state_ever_closed{$server} = "YES";

        $innd_seconds{$server} += $seconds;
        $innd_accepted{$server} += $accepted;
        $innd_refused{$server} += $refused;
        $innd_rejected{$server} += $rejected;
        $innd_stored_size{$server} += $accptsize;
        $innd_duplicated_size{$server} += $dupsize;
        $innd_rejected_size{$server} += ($rjctsize || 0);
      }
      return 1;
    # closed (with times)
    } elsif ($left =~ /(\S+:\d+) closed seconds (\d+) accepted (\d+) refused (\d+) rejected (\d+)$/o) {
      my ($server, $seconds, $accepted, $refused, $rejected) =
	($1, $2, $3, $4, $5);
      $server = lc $server unless $CASE_SENSITIVE;

      $innd_cp_accepted{$server}        = 0;
      $innd_cp_refused{$server}         = 0;
      $innd_cp_rejected{$server}        = 0;
      $innd_cp_seconds{$server}         = 0;
      $innd_cp_stored_size{$server}     = 0;
      $innd_cp_duplicated_size{$server} = 0;
      $innd_cp_rejected_size{$server}   = 0;

      $innd_seconds{$server} += $seconds;
      $innd_accepted{$server} += $accepted;
      $innd_refused{$server} += $refused;
      $innd_rejected{$server} += $rejected;
      return 1;
    }
    # closed (without times (?))
    return 1 if $left =~ m/\S+ closed$/o;
    # closed (for a cancel feed - MODE CANCEL)
    return 1 if $left =~ m/localhost:\d+ closed seconds \d+ cancels \d+$/o;
    # flush
    if ($left =~ /(\S+) flush$/o) {
      $innd_control{"flush"}++;
      return 1;
    }
    # flush-file
    if ($left =~ /flush_file/) {
       $innd_control{"flush_file"}++;
       return 1;
     }
    # too many connections from site
    if ($left =~ /too many connections from (\S+)/o) {
      $innd_max_conn{$1}++;
      return 1;
    }
    # overview exit 0 elapsed 23 pid 28461
    return 1 if $left =~ m/\S+ exit \d+ .*$/o;
    # internal rejecting huge article
    if ($left =~ /(\S+) internal rejecting huge article/o) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_huge{$server}++;
      return 1;
    }
    # internal closing free channel
    if ($left =~ /(\S+) internal closing free channel/o) {
      $innd_misc{"Free channel"}++;
      return 1;
    }
    # internal (other)
    return 1 if $left =~ /\S+ internal/o;
    # wakeup
    return 1 if $left =~ /\S+ wakeup$/o;
    # throttle
    if ($left =~ /(\S+) throttled? /) {
      $innd_control{"throttle"}++;
      return 1;
    }
    # profile timer
    # ME time X nnnn X(X) [...]
    # The exact timers change from various versions of INN, so try to deal
    # with this in a general fashion.
    if ($left =~ m/^\S+\s+                         # ME
                   time\s(\d+)\s+                  # time
                   ((?:\S+\s\d+\(\d+\)\s*)+)       # timer values
                   $/ox) {
      $innd_time_times += $1;
      my $timers = $2;

      while ($timers =~ /(\S+) (\d+)\((\d+)\)\s*/g) {
        my $name = $timer_names{$1} || $1;
        $innd_time_time{$name} += $2;
        if ($3) {
	  my $average = $2 / $3;
	  $innd_time_num{$name} += $3;
          my $min = $innd_time_min{$name};
          $innd_time_min{$name} = $average
            if (!defined($min) || $min > $average);
          my $max = $innd_time_max{$name};
          $innd_time_max{$name} = $average
            if (!defined($max) || $max < $average);
        }
      }
      return 1;
    }
    # ME time xx idle xx(xx)     [ bug ? a part of timer ?]
    return 1 if $left =~ m/^ME time \d+ idle \d+\(\d+\)\s*$/o;
    # ME HISstats x hitpos x hitneg x missed x dne
    #
    # from innd/his.c:
    # HIShitpos: the entry existed in the cache and in history.
    # HIShitneg: the entry existed in the cache but not in history.
    # HISmisses: the entry was not in the cache, but was in the history file.
    # HISdne:    the entry was not in cache or history.
    if ($left =~ m/^ME\ HISstats                  # ME HISstats
	           \ (\d+)\s+hitpos               # hitpos
	           \ (\d+)\s+hitneg               # hitneg
	           \ (\d+)\s+missed               # missed
                   \ (\d+)\s+dne                  # dne
	           $/ox) {
      $innd_his{'Positive hits'} += $1;
      $innd_his{'Negative hits'} += $2;
      $innd_his{'Cache misses'}  += $3;
      $innd_his{'Do not exist'}  += $4;
      return 1;
    }
    # SERVER history cache final: 388656 lookups, 1360 hits
    if ($left =~ m/^SERVER history cache final: (\d+) lookups, (\d+) hits$/) {
      $innd_cache{'Lookups'} += $1;
      $innd_cache{'Hits'}    += $2;
      return 1;
    }
    # bad_hosts (appears after a "cant gesthostbyname" from a feed)
    return 1 if $left =~ m/\S+ bad_hosts /o;
    # cant read
    return 1 if $left =~ m/\S+ cant read/o;
    # cant write
    return 1 if $left =~ m/\S+ cant write/o;
    # cant flush
    return 1 if $left =~ m/\S+ cant flush/o;
    # spoolwake
    return 1 if $left =~ m/\S+ spoolwake$/o;
    # spooling
    return 1 if $left =~ m/\S+ spooling/o;
    # DEBUG
    return 1 if $left =~ m/^DEBUG /o;
    # NCmode
    return 1 if $left =~ m/\S+ NCmode /o;
    # outgoing
    return 1 if $left =~ m/\S+ outgoing/o;
    # inactive
    return 1 if $left =~ m/\S+ inactive/o;
    # timeout
    return 1 if $left =~ m/\S+ timeout/o;
    # lcsetup
    return 1 if $left =~ m/\S+ lcsetup/o;
    # rcsetup
    return 1 if $left =~ m/\S+ rcsetup/o;
    # flush_all
    return 1 if $left =~ m/\S+ flush_all/o;
    # buffered
    return 1 if $left =~ m/\S+ buffered$/o;
    # descriptors
    return 1 if $left =~ m/\S+ descriptors/o;
    # ccsetup
    return 1 if $left =~ m/\S+ ccsetup/o;
    # renumbering
    return 1 if $left =~ m/\S+ renumbering/o;
    # renumber
    return 1 if $left =~ m/\S+ renumber /o;
    # ihave from me
    if ($left =~ m/\S+ ihave_from_me /o) {
      $controlchan_ihave_site{'ME'}++;
      return 1;
    }
    # sendme from me
    if ($left =~ m/\S+ sendme_from_me /o) {
      $controlchan_sendme_site{'ME'}++;
      return 1;
    }
    # newgroup
    if ($left =~ m/\S+ newgroup (\S+) as (\S)/o) {
      $innd_newgroup{$1} = $2;
      return 1;
    }
    # rmgroup
    if ($left =~ m/\S+ rmgroup (\S+)$/o) {
      $innd_rmgroup{$1}++;
      return 1;
    }
    # changegroup
    if ($left =~ m/\S+ change_group (\S+) to (\S)/o) {
      $innd_changegroup{$1} = $2;
      return 1;
    }
    # paused
    if ($left =~ m/(\S+) paused /o) {
      $innd_control{"paused"}++;
      return 1;
    }
    # throttled
    return 1 if $left =~ m/\S+ throttled/o;
    # reload
    if ($left =~ m/(\S+) reload/o) {
      $innd_control{"reload"}++;
      return 1;
    }
    # shutdown
    if ($left =~ m/(\S+) shutdown/o) {
      $innd_control{"shutdown"}++;
      return 1;
    }
    # SERVER servermode paused
    return 1 if ($left =~ /(\S+) servermode paused$/o);
    # SERVER servermode running
    return 1 if ($left =~ /(\S+) servermode running$/o);
    # SERVER flushlogs paused
    if ($left =~ /(\S+) flushlogs /) {
      $innd_control{"flushlogs"}++;
      return 1;
    }
    # think it's a dotquad
    return 1 if $left =~ /think it\'s a dotquad: /o;
    # bad_ihave
    if ($left =~ /(\S+) bad_ihave /) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_bad_ihave{$server}++;
      return 1;
    }
    # bad_messageid
    if ($left =~ /(\S+) bad_messageid/o) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_bad_msgid{$server}++;
      return 1;
    }
    # bad_sendme
    if ($left =~ /(\S+) bad_sendme /o) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_bad_sendme{$server}++;
      return 1;
    }
    # bad_command
    if ($left =~ /(\S+) bad_command /o) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server = lc $server unless $CASE_SENSITIVE;
      $innd_bad_command{$server}++;
      return 1;
    }
    # bad_newsgroup
    if ($left =~ /(\S+) bad_newsgroup /o) {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $innd_bad_newsgroup{$server}++;
      $server = lc $server unless $CASE_SENSITIVE;
      return 1;
    }
    if ($left =~ m/ cant /o) {
      # cant select Bad file number
      if ($left =~ / cant select Bad file number/o) {
	$innd_misc{"Bad file number"}++;
	return 1;
      }
      # cant gethostbyname
      if ($left =~ / cant gethostbyname/o) {
	$innd_misc{"gethostbyname error"}++;
	return 1;
      }
      # cant accept RCreader
      if ($left =~ / cant accept RCreader /o) {
	$innd_misc{"RCreader"}++;
	return 1;
      }
      # cant sendto CCreader
      if ($left =~ / cant sendto CCreader /o) {
	$innd_misc{"CCreader"}++;
	return 1;
      }
      # cant (other) skipped - not particularly interesting
      return 1;
    }
    # bad_newsfeeds no feeding sites
    return 1 if $left =~ /\S+ bad_newsfeeds no feeding sites/o;
    # CNFS: cycbuff rollover - possibly interesting
    return 1 if $left =~ /CNFS(?:-sm)?: cycbuff \S+ rollover to cycle/o;
    # CNFS: CNFSflushallheads: flushing - possibly interesting
    return 1 if $left =~ /CNFS(?:-sm)?: CNFSflushallheads: flushing /o;
    # CNFS: metacycbuff rollover with SEQUENTIAL
    return 1 if $left =~ /CNFS(?:-sm)?: metacycbuff \S+ cycbuff is moved to /o;
    # Cleanfeed status reports
    return 1 if $left =~ /^filter: status/o;
    return 1 if $left =~ /^filter: Reloading bad files/o;
  }
  ########
  ## innfeed
  if ($prog eq "innfeed") {
    # connected
    if ($left =~ /(\S+):\d+ connected$/) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innfeed_connect{$server}++;
      return 1;
    }
    # closed periodic
    return 1 if $left =~ m/\S+:\d+ closed periodic$/o;
    # periodic close
    return 1 if $left =~ m/\S+:\d+ periodic close$/o;
    # final (child)
    return 1 if $left =~ m/\S+:\d+ final seconds \d+ offered \d+ accepted \d+ refused \d+ rejected \d+/o;
    # global (real)
    return 1 if $left =~ m/\S+ global seconds \d+ offered \d+ accepted \d+ refused \d+ rejected \d+ missing \d+/o;
    # final (real) (new format)
    if ($left =~ /(\S+) final seconds (\d+) offered (\d+) accepted (\d+) refused (\d+) rejected (\d+) missing (\d+) accsize (\d+) rejsize (\d+) spooled (\d+)/o) {
      my ($server, $seconds, $offered, $accepted, $refused, $rejected,
	  $missing, $accepted_size, $rejected_size, $spooled) = ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
      $server = lc $server unless $CASE_SENSITIVE;
      $innfeed_seconds{$server} += $seconds;
      $innfeed_offered{$server} += $offered;
      $innfeed_accepted{$server} += $accepted;
      $innfeed_refused{$server} += $refused;
      $innfeed_rejected{$server} += $rejected;
      $innfeed_missing{$server} += $missing;
      $innfeed_spooled{$server} += $spooled;
      $innfeed_accepted_size{$server} += $accepted_size;
      $innfeed_rejected_size{$server} += $rejected_size;
      return 1;
    } elsif ($left =~ /(\S+) final seconds (\d+) offered (\d+) accepted (\d+) refused (\d+) rejected (\d+) missing (\d+) spooled (\d+)/o) {
      my ($server, $seconds, $offered, $accepted, $refused, $rejected,
	  $missing, $spooled) = ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
      $server = lc $server unless $CASE_SENSITIVE;
      $innfeed_seconds{$server} += $seconds;
      $innfeed_offered{$server} += $offered;
      $innfeed_accepted{$server} += $accepted;
      $innfeed_refused{$server} += $refused;
      $innfeed_rejected{$server} += $rejected;
      $innfeed_missing{$server} += $missing;
      $innfeed_spooled{$server} += $spooled;
      return 1;
    }
    # final (only seconds & spooled)
    if ($left =~ /(\S+) final seconds (\d+) spooled (\d+)/o) {
      my ($server, $seconds, $spooled) = ($1, $2, $3);
      $server = lc $server unless $CASE_SENSITIVE;
      $innfeed_seconds{$server} += $seconds;
      $innfeed_spooled{$server} += $spooled;
      return 1;
    }
    # checkpoint
    return 1 if $left =~ m/\S+ checkpoint seconds/o;
    # ME file xxxx shrunk from yyyy to zzz
    if ($left =~ /^ME file (.*)\.output shrunk from (\d+) to (\d+)$/) {
      my ($file, $s1, $s2) = ($1, $2, $3);
      $file =~ s|^.*/([^/]+)$|$1|; # keep only the server name
      $innfeed_shrunk{$file} += $s1 - $s2;
      return 1;
    }
    # profile timer
    # ME time X nnnn X(X) [...]
    return 1 if $left =~ m/backlogstats/;
    if ($left =~ m/^\S+\s+                         # ME
                   time\s(\d+)\s+                  # time
                   ((?:\S+\s\d+\(\d+\)\s*)+)       # timer values
                   $/ox) {
      $innfeed_time_times += $1;
      my $timers = $2;

      while ($timers =~ /(\S+) (\d+)\((\d+)\)\s*/g) {
        my $name = $innfeed_timer_names{$1} || $1;
        $innfeed_time_time{$name} += $2;
	if ($3) {
	  $innfeed_time_num{$name} += $3;
	  my $average = $2 / $3;
	  my $min = $innfeed_time_min{$name};
	  $innfeed_time_min{$name} = $average
	    if (!defined($min) || $min > $average);
	  my $max = $innfeed_time_max{$name};
	  $innfeed_time_max{$name} = $average
	    if (!defined($max) || $max < $average);
        }
      }
      return 1;
    }
    # xxx grabbing external tape file
    return 1 if $left =~ m/ grabbing external tape file/o;
    # hostChkCxns - maxConnections was
    return 1 if $left =~ m/hostChkCxns - maxConnections was /o;
    # cxnsleep
    return 1 if $left =~ m/\S+ cxnsleep .*$/o;
    # idle
    return 1 if $left =~ m/\S+ idle tearing down connection$/o;
    # remote
    return 1 if $left =~ m/\S+ remote .*$/o;
    # spooling
    return 1 if $left =~ m/\S+ spooling no active connections$/o;
    # ME articles total
    return 1 if $left =~ m/(?:SERVER|ME) articles total \d+ bytes \d+/o;
    # ME articles active
    return 1 if $left =~ m/(?:SERVER|ME) articles active \d+ bytes \d+/o;
    # connect : Connection refused
    return 1 if $left =~ m/connect : Connection refused/o;
    # connect : Network is unreachable
    return 1 if $left =~ m/connect : Network is unreachable/o;
    # connect : Address family not supported by protocol
    return 1 if $left =~ m/connect : Address family not supported by protocol/o;
    # connect : No route to host
    return 1 if $left =~ m/connect : No route to host/o;
    # connection vanishing
    return 1 if $left =~ m/connection vanishing/o;
    # can't resolve hostname
    return 1 if $left =~ m/can\'t resolve hostname/o;
    # new hand-prepared backlog file
    return 1 if $left =~ m/new hand-prepared backlog file/o;
    # flush re-connect failed
    return 1 if $left =~ m/flush re-connect failed/o;
    # internal QUIT while write pending
    return 1 if $left =~ m/internal QUIT while write pending/o;
    # ME source lost . Exiting
    return 1 if $left =~ m/(?:SERVER|ME) source lost . Exiting/o;
    # ME starting innfeed (+version & date)
    return 1 if $left =~ m/(?:SERVER|ME) starting (?:innfeed|at)/o;
    # ME finishing at (date)
    return 1 if $left =~ m/(?:SERVER|ME) finishing at /o;
    # mode no-CHECK entered
    return 1 if $left =~ m/mode no-CHECK entered/o;
    # mode no-CHECK exited
    return 1 if $left =~ m/mode no-CHECK exited/o;
    # closed
    return 1 if $left =~ m/^(\S+) closed$/o;
    # global (+ seconds offered accepted refused rejected missing)
    return 1 if $left =~ m/^(\S+) global/o;
    # idle connection still has articles
    return 1 if $left =~ m/^(\S+) idle connection still has articles$/o;
    # missing article for IHAVE-body
    return 1 if $left =~ m/^(\S+) missing article for IHAVE-body$/o;
    # cannot continue
    return 1 if $left =~ m/^cannot continue/o;
    if ($left =~ /^(?:SERVER|ME)/o) {
      # ME dropping articles into ...
      return 1 if $left =~ / dropping articles into /o;
      # ME dropped ...
      return 1 if $left =~ / dropped /o;
      # ME internal bad data in checkpoint file
      return 1 if $left =~ m/ internal bad data in checkpoint/o;
      # ME two filenames for same article
      return 1 if $left =~ m/ two filenames for same article/o;
      # ME unconfigured peer
      return 1 if $left =~ m/ unconfigured peer/o;
      # exceeding maximum article size
      return 1 if $left =~ m/ exceeding maximum article byte/o;
      # no space left on device errors
      return 1 if $left =~ m/ ioerr fclose/o;
      return 1 if $left =~ m/ lock failed for host/o;
      return 1 if $left =~ m/ lock file pid-write/o;
      return 1 if $left =~ m/ locked cannot setup peer/o;
      return 1 if $left =~ m/ received shutdown signal/o;
      # unconfigured peer
      return 1 if $left =~ m/ unconfigured peer/o;
      # ME lock
      return 1 if $left =~ m/ lock/o;
      # ME exception: getsockopt (0): Socket operation on non-socket
      return 1 if $left =~ m/ exception: getsockopt /o;
      # ME config aborting fopen (...) Permission denied
      return 1 if $left =~ m/ config aborting fopen /o;
      # ME cant chmod innfeed.pid....
      return 1 if $left =~ m/ cant chmod \S+\/innfeed.pid/o;
      return 1 if $left =~ m/ tape open failed /o;
      return 1 if $left =~ m/ oserr open checkpoint file:/o;
      # ME finishing (quickly)
      return 1 if $left =~ m/\(quickly\) /o;
      # ME config: value of streaming is not a boolean
      return 1 if $left =~ m/config: value of \S+ is not/o;
    }
    # hostChkCxn - now: x.xx, prev: x.xx, abs: xx, curr: x
    return 1 if $left =~ m/ hostChkCxn - now/o;
    # loading path_to_config_file/innfeed.conf
    return 1 if $left =~ m/loading /o;
    # Finnaly, to avoid problems with strange error lines, ignore them.
    #return 1 if ($left =~ /ME /);
  }
  ########
  ## innxmit
  if ($prog eq "innxmit") {
    # 437 Duplicate article
    if ($left =~ /(\S+) rejected [^\s]+ \(.*?\) 437 Duplicate article$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_duplicate{$server}++;
      return 1;
    }
    # 437 Unapproved for
    if ($left =~ /(\S+) rejected [^\s]+ \(.*\) 437 Unapproved for \"(.*?)\"$/o) {
      my ($server, $group) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_unapproved{$server}++;
      $innxmit_unapproved_g{$group}++;
      return 1;
    }
    # 437 Too old -- ...
    if ($left =~ /(\S+) rejected [^\s]+ \(.*\) 437 Too old -- \".*?\"$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_tooold{$server}++;
      return 1;
    }
    # 437 Unwanted site ... in path
    if ($left =~
      /(\S+) rejected [^\s]+ \(.*?\) 437 Unwanted site (\S+) in path$/o) {
      my ($server, $site) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_uw_site{$server}++;
      # $innxmit_site_path{$site}++;
      return 1;
    }
    # 437 Unwanted newsgroup "..."
    if ($left =~
      /(\S+) rejected [^\s]+ \(.*?\) 437 Unwanted newsgroup \"(\S+)\"$/o) {
      my ($server, $group) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_uw_ng_s{$server}++;
      $innxmit_uw_ng{$group}++;
      return 1;
    }
    # 437 Unwanted distribution "..."
    if ($left =~
      /(\S+) rejected [^\s]+ \(.*?\) 437 Unwanted distribution \"(\S+)\"$/o) {
      my ($server, $dist) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_uw_dist_s{$server}++;
      $innxmit_uw_dist{$dist}++;
      return 1;
    }
    # xx rejected foo.bar/12345 (foo/bar/12345) 437 Unwanted distribution "..."
    if ($left =~ /^(\S+) rejected .* 437 Unwanted distribution \"(\S+)\"$/o) {
      my ($server, $dist) = ($1, $2);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_uw_dist_s{$server}++;
      $innxmit_uw_dist{$dist}++;
      return 1;
    }
    # 437 Linecount x != y +- z
    if ($left =~ /(\S+) rejected [^\s]+ \(.*?\) 437 Linecount/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_badart{$server}++;
      $innxmit_linecount{$server}++;
      return 1;
    }
    # 437 Newsgroup name illegal -- "xxx"
    if ($left =~ /(\S+) rejected .* 437 Newsgroup name illegal -- "[^\"]*"$/) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_others{$server}++;
      $innxmit_badart{$server}++;
      return 1;
    }
    # Streaming retries
    return 1 if ($left =~ /\d+ Streaming retries$/o);
    # ihave failed
    if ($left =~ /(\S+) ihave failed/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_ihfail{$server} = 1;
      if ($left = /436 \S+ NNTP \S+ out of space/o) {
	$innxmit_nospace{$server}++;
	return 1;
      }
      if ($left = /400 \S+ space/o) {
	$innxmit_nospace{$server}++;
	return 1;
      }
      if ($left = /400 Bad file/o) {
	$innxmit_crefused{$server}++;
	return 1;
      }
      if ($left = /480 Transfer permission denied/o) {
	$innxmit_crefused{$server}++;
	return 1;
      }
    }
    # stats (new format)
    if ($left =~
      /(\S+) stats offered (\d+) accepted (\d+) refused (\d+) rejected (\d+) missing (\d+) accsize (\d+) rejsize (\d+)$/o) {
      my ($server, $offered, $accepted, $refused, $rejected, $missing, $accbytes, $rejbytes) =
	($1, $2, $3, $4, $5, $6, $7, $8);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_offered{$server} += $offered;
      $innxmit_offered{$server} -= $innxmit_ihfail{$server}
        if ($innxmit_ihfail{$server});
      $innxmit_accepted{$server} += $accepted;
      $innxmit_refused{$server} += $refused;
      $innxmit_rejected{$server} += $rejected;
      $innxmit_missing{$server} += $missing;
      $innxmit_accepted_size{$server} += $accbytes;
      $innxmit_rejected_size{$server} += $rejbytes;
      $innxmit_site{$server}++;
      $innxmit_ihfail{$server} = 0;
      return 1;
    }
    # stats
    if ($left =~
      /(\S+) stats offered (\d+) accepted (\d+) refused (\d+) rejected (\d+)$/o) {
      my ($server, $offered, $accepted, $refused, $rejected) =
	($1, $2, $3, $4, $5);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_offered{$server} += $offered;
      $innxmit_offered{$server} -= $innxmit_ihfail{$server}
        if ($innxmit_ihfail{$server});
      $innxmit_accepted{$server} += $accepted;
      $innxmit_refused{$server} += $refused;
      $innxmit_rejected{$server} += $rejected;
      $innxmit_site{$server}++;
      $innxmit_ihfail{$server} = 0;
      return 1;
    }
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) elapsed (\S+)$/o) {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_times{$server} += $elapsed;
      return 1;
    }
    # connect & no space
    if ($left =~ /(\S+) connect \S+ 400 No space/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_nospace{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect & NNTP no space
    if ($left =~ /(\S+) connect \S+ 400 \S+ out of space/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_nospace{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect & loadav
    if ($left =~ /(\S+) connect \S+ 400 loadav/o) {
      my $server = $1;
      if ($left =~ /expir/i) {
	$server = lc $server unless $CASE_SENSITIVE;
	$innxmit_expire{$server}++;
	$innxmit_site{$server}++;
	return 1;
      }
    }
    # connect 400 (other)
    if ($left =~ /(\S+) connect \S+ 400/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_crefused{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect failed
    if ($left =~ /(\S+) connect failed/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_cfail_host{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # authenticate failed
    if ($left =~ /(\S+) authenticate failed/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_afail_host{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # xxx ihave failed 400 loadav [innwatch:hiload] yyy gt zzz
    if ($left =~ /^(\S+) ihave failed 400 loadav/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $innxmit_hiload{$server}++;
      return 1;
    }
    # ihave failed
    return 1 if ($left =~ /\S+ ihave failed/o);
    # requeued (....) 436 No space
    return 1 if ($left =~ /\S+ requeued \S+ 436 No space/o);
    # requeued (....) 400 No space
    return 1 if ($left =~ /\S+ requeued \S+ 400 No space/o);
    # requeued (....) 436 Can't write history
    return 1 if ($left =~ /\S+ requeued \S+ 436 Can\'t write history/o);
    # unexpected response code
    return 1 if ($left =~ /unexpected response code /o);
  }

  ########
  ## nntplink
  if ($prog eq "nntplink") {
    $left =~ s/^(\S+):/$1/;
    # EOF
    if ($left =~ /(\S+) EOF /o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_eof{$server}++;
      return 1;
    }
    # Broken pipe
    if ($left =~ /(\S+) Broken pipe$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_bpipe{$server}++;
      return 1;
    }
    # already running - won't die
    return 1 if $left =~ /\S+ nntplink.* already running /o;
    # connection timed out
    if ($left =~ /(\S+) connection timed out/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_bpipe{$server}++;
      return 1;
    }
    # greeted us with 400 No space
    if ($left =~ /(\S+) greeted us with 400 No space/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_nospace{$server}++;
      return 1;
    }
    # greeted us with 400 loadav
    if ($left =~ /(\S+) greeted us with 400 loadav/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_hiload{$server}++;
      return 1;
    }
    # greeted us with 400 (other)
    if ($left =~ /(\S+) greeted us with 400/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      if ($left =~ /expir/i) {
	$nntplink_expire{$server}++;
      } else {
	$nntplink_fail{$server}++;
      }
      return 1;
    }
    # greeted us with 502
    if ($left =~ /(\S+) greeted us with 502/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_auth{$server}++;
      return 1;
    }
    # sent authinfo
    if ($left =~ /(\S+) sent authinfo/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_auth{$server}++;
      return 1;
    }
    # socket()
    if ($left =~ /(\S+) socket\(\): /o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_sockerr{$server}++;
      return 1;
    }
    # select()
    if ($left =~ /(\S+) select\(\) /o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_site{$server}++;
      $nntplink_selecterr{$server}++;
      return 1;
    }
    # sent IHAVE
    if ($left =~ /(\S+) sent IHAVE/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_ihfail{$server}++;
      if (($left =~ / 436 /) && ($left =~ / out of space /)) {
	$nntplink_fake_connects{$server}++;
	$nntplink_nospace{$server}++;
      }
      return 1;
    }
    # article .... failed(saved): 436 No space
    if ($left =~ /(\S+) .* failed\(saved\): 436 No space$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_nospace{$server}++;
      return 1;
    }
    # article .. 400 No space left on device writing article file -- throttling
    if ($left =~ /(\S+) .* 400 No space left on device writing article file -- throttling$/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_nospace{$server}++;
      return 1;
    }
    # stats
    if ($left =~ /(\S+) stats (\d+) offered (\d+) accepted (\d+) rejected (\d+) failed (\d+) connects$/o) {
      my ($server, $offered, $accepted, $rejected, $failed, $connects) =
	($1, $2, $3, $4, $5, $6);
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_offered{$server} += $offered - $nntplink_ihfail{$server}++;
      $nntplink_accepted{$server} += $accepted;
      $nntplink_rejected{$server} += $rejected;
      $nntplink_failed{$server} += $failed;
      $nntplink_connects{$server} += $connects;
      $nntplink_ihfail{$server} = 0;
      if ($nntplink_fake_connects{$server}) {
	$nntplink_site{$server} += $nntplink_fake_connects{$server};
	$nntplink_fake_connects{$server} = 0;
      } else {
	$nntplink_site{$server}++;
      }
      return 1;
    }
    # xmit
    if ($left =~ /(\S+) xmit user (\S+) system (\S+) elapsed (\S+)$/o) {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_times{$server} += $elapsed;
      return 1;
    }
    # xfer
    return 1 if $left =~ /\S+ xfer/o;
    # Links down .. x hours
    if ($left =~ /(\S+) Links* down \S+ \d+/o) {
      # Collected but not used
      # my $server = $1;
      # $server = lc $server unless $CASE_SENSITIVE;
      # $nntplink_down{$server} += $hours;
      return 1;
    }
    # 503 Timeout
    if ($left =~ /^(\S+) \S+ \S+ \S+ 503 Timeout/o) {
      # Collected but not used
      # my $server = $1;
      # $server = lc $server unless $CASE_SENSITIVE;
      # $nntplink_timeout{$server}++;
      return 1;
    }
    # read() error while reading reply
    if ($left =~ /^(\S+): read\(\) error while reading reply/o) {
      my $server = $1;
      $server = lc $server unless $CASE_SENSITIVE;
      $nntplink_failed{$server}++;
      return 1;
    }
    # Password file xxxx not found
    return 1 if $left =~ /^\S+ Password file \S+ not found/;
    # No such
    return 1 if $left =~ /^\S+ \S+ \S+ No such/;
    # already running
    return 1 if $left =~ /^\S+ \S+ already running/;
    # error reading version from datafile
    return 1 if $left =~ /error reading version from datafile/;
  }
  ########
  ## nnrpd
  if ($prog =~ /^nnrpd(?:-ssl)?$/)
  {
    # Fix a small bug of nnrpd (inn 1.4*)
    $left =~ s/^ /\? /o;
    # Another bug (in INN 1.5b1)
    return 1 if $left =~ /^\020\002m$/o; # ^P^Bm
    # bad_history at num for <ref>
    return 1 if $left =~ /bad_history at \d+ for /o;
    # timeout short
    return 1 if $left =~ /\S+ timeout short$/o;
    # < or > + (blablabla)
    return 1 if $left =~ /^\S+ [\<\>] /o;
    # cant opendir ... I/O error
    return 1 if $left =~ /\S+ cant opendir \S+ I\/O error$/o;
    # perl filtering enabled
    return 1 if $left =~ /perl filtering enabled$/o;
    # Python filtering enabled
    return 1 if $left =~ /Python filtering enabled$/o;
    return 1 if $left =~ /^python interpreter initialized OK$/o;
    return 1 if $left =~ /^python method \S+ not found$/o;
    return 1 if $left =~ /^python authenticate method succeeded, return code \d+, error string /o;
    return 1 if $left =~ /^python access method succeeded$/o;
    return 1 if $left =~ /^python dynamic method \(\w+ access\) succeeded, refusion string: /o;
    return 1 if $left =~ /^python: .+ module successfully hooked into nnrpd$/o;
    return 1 if $left =~ /^python: nnrpd .+ class instance created$/o;
    return 1 if $left =~ /^python: n_a authenticate\(\) invoked: hostname \S+, ipaddress \S+, interface \S+, user /o;
    return 1 if $left =~ /^python: n_a access\(\) invoked: hostname \S+, ipaddress \S+, interface \S+, user /o;
    return 1 if $left =~ /^python: n_a dynamic\(\) invoked against type \S+, hostname \S+, ipaddress \S+, interface \S+, user /o;
    return 1 if $left =~ /^python: authentication by username succeeded$/o;
    return 1 if $left =~ /^python: authentication by username failed$/o;
    return 1 if $left =~ /^python: authentication access by IP address succeeded$/o;
    return 1 if $left =~ /^python: authentication access by IP address failed$/o;
    return 1 if $left =~ /^python: dynamic access module successfully hooked into nnrpd$/o;
    return 1 if $left =~ /^python: dynamic authorization access for read access granted$/o;
    return 1 if $left =~ /^python: dynamic authorization access type is not known: /o;
    # connect
    if ($left =~ /(\S+) (\([0-9a-fA-F:.]*\) )?connect$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_connect{$dom}++;
      $nnrpd_connect{$cust}++;
      return 1;
    }
    # group
    if ($left =~ /(\S+) group (\S+) (\d+)$/o) {
      my ($cust, $group, $num) = ($1, $2, $3);
      if ($num) {
	$nnrpd_group{$group} += $num;
	my ($hierarchy) = $group =~ /^([^\.]+).*$/o;
	$nnrpd_hierarchy{$hierarchy} += $num;
      }
      return 1;
    }
    # post failed
    if ($left =~ /(\S+) post failed (.*)$/o) {
      my ($cust, $error) = ($1, $2);
      $nnrpd_post_error{$error}++;
      return 1;
    }
    # post ok
    return 1 if $left =~ /\S+ post ok/o;
    # ihave ok
    return 1 if $left =~ /\S+ ihave ok/o;
    # posts
    if ($left =~ /(\S+) posts received (\d+) rejected (\d+)$/o) {
      my ($cust, $received, $rejected) = ($1, $2, $3);
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_post_ok{$dom} += $received;
      $nnrpd_dom_post_rej{$dom} += $rejected;
      $nnrpd_post_ok{$cust} += $received;
      $nnrpd_post_rej{$cust} += $rejected;
      return 1;
    }
    # noperm post without permission
    if ($left =~ /(\S+) noperm post without permission/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_post_rej{$dom} ++;
      $nnrpd_post_rej{$cust} ++;
      return 1;
    }
    # no_permission
    if ($left =~ /(\S+) no_(permission|access)$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_no_permission{$cust}++;
      $nnrpd_dom_no_permission{$dom}++;
      return 1;
    }
    # bad_auth
    if ($left =~ /(\S+) bad_auth$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_no_permission{$dom}++;
      $nnrpd_no_permission{$cust}++;
      return 1;
    }
    # Authentication failure
    # User not known to the underlying authentication module
    return 1 if $left =~ / ckpasswd: pam_authenticate failed: /o;
    return 1 if $left =~ / ckpasswd: user .+ unknown$/o;
    # authinfo
    if ($left =~ /\S+ user (\S+)$/o) {
      my $user = $1;
      $nnrpd_auth{$user}++;
      return 1;
    }
    # unrecognized + command
    if ($left =~ /(\S+) unrecognized (.*)$/o) {
      my ($cust, $error) = ($1, $2);
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $error = "_null command_" if ($error !~ /\S/);
      $error =~ s/^(xmotd) .*$/$1/i if ($error =~ /^xmotd .*$/i);
      $nnrpd_dom_unrecognized{$dom}++;
      $nnrpd_unrecognized{$cust}++;
      $nnrpd_unrecogn_cmd{$error}++;
      return 1;
    }
    # exit
    if ($left =~ /(\S+) exit articles (\d+) groups (\d+)$/o) {
      my ($cust, $articles, $groups) = ($1, $2, $3);
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      if ($cust eq '?') {
        $nnrpd_connect{$cust}++;
        $nnrpd_dom_connect{$dom}++;
      }
      $nnrpd_groups{$cust} += $groups;
      $nnrpd_dom_groups{$dom} += $groups;
      $nnrpd_articles{$cust} += $articles;
      $nnrpd_dom_articles{$dom} += $articles;
      return 1;
    }
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) idle (\S+) elapsed (\S+)$/o) {
      my ($cust, $user, $system, $idle, $elapsed) = ($1, $2, $3, $4, $5);
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_times{$cust} += $elapsed;
      $nnrpd_resource_user{$cust} += $user;
      $nnrpd_resource_system{$cust} += $system;
      $nnrpd_resource_idle{$cust} += $idle;
      $nnrpd_resource_elapsed{$cust} += $elapsed;
      $nnrpd_dom_times{$dom} += $elapsed;
      return 1;
    }
    # artstats
    if ($left =~ /(\S+) artstats get (\d+) time (\d+) size (\d+)$/o) {
      my ($cust, $articles, $time, $bytes) = ($1, $2, $3, $4);
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_bytes{$cust} += $bytes;
      $nnrpd_dom_bytes{$dom} += $bytes;
      return 1;
    }
    # timeout
    if ($left =~ /(\S+) timeout$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_timeout{$dom}++;
      $nnrpd_timeout{$cust}++;
      return 1;
    }
    # timeout in post
    if ($left =~ /(\S+) timeout in post$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_timeout{$dom}++;
      $nnrpd_timeout{$cust}++;
      return 1;
    }
    # can't read: Connection timed out
    if ($left =~ /(\S+) can\'t read: Connection timed out$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_timeout{$dom}++;
      $nnrpd_timeout{$cust}++;
      return 1;
    }
    # can't read: Operation timed out
    if ($left =~ /(\S+) can\'t read: Operation timed out$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_timeout{$dom}++;
      $nnrpd_timeout{$cust}++;
      return 1;
    }
    # can't read: Connection reset by peer
    if ($left =~ /(\S+) can\'t read: Connection reset by peer$/o) {
      my $cust = $1;
      $cust = lc $cust unless $CASE_SENSITIVE;
      my $dom = &host2dom($cust);
      $nnrpd_dom_reset_peer{$dom}++;
      $nnrpd_reset_peer{$cust}++;
      return 1;
    }
    # can't read: Network is unreachable
    return 1 if $left =~ /(\S+) can\'t read: Network is unreachable$/o;
    # gethostbyaddr: xxx.yyy.zzz != a.b.c.d
    if ($left =~ /^gethostbyaddr: (.*)$/o) {
      my $msg = $1;
      $nnrpd_gethostbyaddr{$msg}++;
      return 1;
    }
    # cant gethostbyaddr
    if ($left =~ /\? cant gethostbyaddr (\S+) .*$/o) {
      my $ip = $1;
      $nnrpd_gethostbyaddr{$ip}++;
      return 1;
    }
    # cant getpeername
    if ($left =~ /\? cant getpeername/o) {
      # $nnrpd_getpeername++;
      $nnrpd_gethostbyaddr{"? (can't getpeername)"}++;
      return 1;
    }
    # can't getsockname
    return 1 if $left =~ /^\S+ can\'t getsockname$/o;
    # reverse lookup failed
    return 1 if $left =~ /^\? reverse lookup for \S+ failed: .* -- using IP address for access$/o;
    # profile timer
    # ME time X nnnn X(X) [...]
    # The exact timers change from various versions of INN, so try to deal
    # with this in a general fashion.
    if ($left =~ m/^\S+\s+                         # ME
	           time\s(\d+)\s+                  # time
                   ((?:\S+\s\d+\(\d+\)\s*)+)       # timer values
                   $/ox) {
      $nnrpd_time_times += $1;
      my $timers = $2;

      while ($timers =~ /(\S+) (\d+)\((\d+)\)\s*/g) {
        my $name = $nnrpd_timer_names{$1} || $1;
        $nnrpd_time_time{$name} += $2;
        if ($3) {
	  my $average = $2 / $3;
	  $nnrpd_time_num{$name} += $3;
          my $min = $nnrpd_time_min{$name};
          $nnrpd_time_min{$name} = $average
            if (!defined($min) || $min > $average);
          my $max = $nnrpd_time_max{$name};
          $nnrpd_time_max{$name} = $average
            if (!defined($max) || $max < $average);
        }
      }
      return 1;
    }
    # ME dropping articles into ...
    return 1 if $left =~ /ME dropping articles into /o;
    # newnews (interesting but ignored till now)
    return 1 if $left =~ /^\S+ newnews /o;
    # cant fopen (ignored too)
    return 1 if $left =~ /^\S+ cant fopen /o;
    # can't read: No route to host
    return 1 if $left =~ /can\'t read: No route to host/o;
    # can't read: Broken pipe
    return 1 if $left =~ /can\'t read: Broken pipe/o;
    # eof in post
    return 1 if $left =~ /^\S+ EOF in post$/o;
    # ioctl: ...
    return 1 if $left =~ /^ioctl: /o;
    # other stats
    return 1 if $left =~ /^\S+ overstats count \d+ hit \d+ miss \d+ time \d+ size \d+ dbz \d+ seek \d+ get \d+ artcheck \d+$/o;
    # starttls
    return 1 if $left =~ /^starttls: \S+ with cipher \S+ \(\d+\/\d+ bits\) no authentication$/o;
  }
  ########
  ## inndstart
  if ($prog eq "inndstart") {
    # cant bind Address already in use
    # cant bind Permission denied
    return 1 if $left =~ /cant bind /o;
    # cant setgroups Operation not permitted
    return 1 if $left =~ /cant setgroups /o;
  }
  ########
  ## overchan
  if ($prog eq "overchan") {
    # times
    if ($left =~ /timings (\d+) arts (\d+) of (\d+) ms$/o) {
      my ($articles, $work_time, $run_time) = ($1, $2, $3);
      # ??? What to do with numbers
      return 1;
    }
  }
  ########
  ## batcher
  if ($prog eq "batcher") {
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) elapsed (\S+)$/o) {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server = lc $server unless $CASE_SENSITIVE;
      # $batcher_user{$server} += $user;
      # $batcher_system{$server} += $system;
      $batcher_elapsed{$server} += $elapsed;
      return 1;
    }
    # stats
    if ($left =~ /(\S+) stats batches (\d+) articles (\d+) bytes (\d+)$/o) {
      my ($server, $batches, $articles, $bytes) = ($1, $2, $3, $4);
      $server = lc $server unless $CASE_SENSITIVE;
      $batcher_offered{$server} += $batches;
      $batcher_articles{$server} += $articles;
      $batcher_bytes{$server} += $bytes;
      return 1;
    }
  }
  ########
  ## rnews
  if ($prog eq "rnews") {
    # rejected connection
    if ($left =~ /rejected connection (.*)$/o) {
      $rnews_rejected{$1}++;
      return 1;
    }
    # cant open_remote
    if ($left =~ /(cant open_remote .*)$/o) {
      $rnews_rejected{$1}++;
      return 1;
    }
    # rejected 437 Unwanted newsgroup
    if ($left =~ /rejected 437 Unwanted newsgroup \"(.*)\"$/o) {
      $rnews_bogus_ng{$1}++;
      return 1;
    }
    # rejected 437 Unapproved for "xx"
    if ($left =~ /rejected 437 Unapproved for \"(.*)\"$/o) {
      $rnews_unapproved{$1}++;
      return 1;
    }
    # rejected 437 Unwanted distribution
    if ($left =~ /rejected 437 Unwanted distribution (.*)$/o) {
      $rnews_bogus_dist{$1}++;
      return 1;
    }
    # rejected 437 Bad "Date"
    if ($left =~ /rejected 437 Bad \"Date\" (.*)$/o) {
      $rnews_bogus_date{$1}++;
      return 1;
    }
    # rejected 437 Article posted in the future
    if ($left =~ /rejected 437 Article posted in the future -- \"(.*)\"$/o) {
      $rnews_bogus_date{"(future) $1"}++;
      return 1;
    }
    # rejected 437 Too old -- "..."
    if ($left =~ /rejected 437 Too old -- (.*)$/o) {
      $rnews_too_old++;
      return 1;
    }
    # rejected 437 Linecount...
    if ($left =~ /rejected 437 (Linecount) \d+ \!= \d+/o) {
      $rnews_linecount++;
      return 1;
    }
    # rejected 437 Duplicate
    if ($left =~ /rejected 437 Duplicate$/o) {
      $rnews_duplicate++;
      return 1;
    }
    # rejected 437 Duplicate article
    if ($left =~ /rejected 437 (Duplicate article)/o) {
      $rnews_duplicate++;
      return 1;
    }
    # rejected 437 No colon-space ...
    if ($left =~ /rejected 437 No colon-space in \"(.*)\" header$/o) {
      $rnews_no_colon_space++;
      return 1;
    }
    # duplicate <msg-id> path..
    if ($left =~ /^duplicate /o) {
      $rnews_duplicate++;
      return 1;
    }
    # offered <msg-id> feed
    if ($left =~ /^offered \S+ (\S+)/o) {
      my $host = $1;
      $host = lc $host unless $CASE_SENSITIVE;
      # Small hack used to join article spooled when innd is throttle.
      # In this situation, the hostname is a 8 hex digits string
      # To avoid confusions with real feeds, the first character is forced
      # to be a '3' or a '4' (will work between 9/7/1995 and 13/7/2012).
      $host = "Local postings" if $host =~ /^[34][0-9a-f]{7}$/;
      $rnews_host{$host}++;
      return 1;
    }
    # rejected 437 ECP rejected
    return 1 if $left =~ m/rejected 437 ECP rejected/o;
    # rejected 437 "Subject" header too long
    return 1 if $left =~ m/header too long/o;
    # rejected 437 Too long line in header 1163 bytes
    return 1 if $left =~ m/rejected 437 Too long line in header/o;
    # rejected 437 Too many newsgroups (meow)
    return 1 if $left =~ m/rejected 437 Too many newsgroups/o;
    # rejected 437 Space before colon in "<a" header
    return 1 if $left =~ m/rejected 437 Space before colon in/o;
    # rejected 437 EMP (phl)
    return 1 if $left =~ m/rejected 437 EMP/o;
    # rejected 437 Scoring filter (8)
    return 1 if $left =~ m/rejected 437 Scoring filter/o;
    # bad_article missing Message-ID
    return 1 if $left =~ m/bad_article missing Message-ID/o;
    # cant unspool saving to xxx
    return 1 if $left =~ m/cant unspool saving to/o;
  }

  ###########
  ## ncmspool
  if ($prog eq "ncmspool") {
    # <article> good signature from foo@bar.com
    if ($left =~ /good signature from (.*)/o) {
      $nocem_goodsigs{$1}++;
      $nocem_totalgood++;
      $nocem_lastid = $1;
      return 1;
    }
    # <article> bad signature from foo@bar.com
    if ($left =~ /bad signature from (.*)/o) {
      $nocem_badsigs{$1}++;
      $nocem_goodsigs{$1} = 0 unless ($nocem_goodsigs{$1});
      $nocem_totalbad++;
      $nocem_lastid = $1;
      return 1;
    }
    # <article> contained 123 new 456 total ids
    if ($left =~ /contained (\d+) new (\d+) total ids/o) {
      $nocem_newids += $1;
      $nocem_newids{$nocem_lastid} += $1;
      $nocem_totalids += $2;
      $nocem_totalids{$nocem_lastid} += $2;
      return 1;
    }
    return 1;
  }

  ########
  ## nocem
  if ($prog eq "nocem") {
    if ($left =~ /processed notice .* by (.*) \((\d+) ids,/o) {
      $nocem_goodsigs{$1}++;
      $nocem_totalgood++;
      $nocem_lastid = $1;
      $nocem_newids += $2;
      $nocem_newids{$nocem_lastid} += $2;
      $nocem_totalids += $2;
      $nocem_totalids{$nocem_lastid} += $2;
      return 1;
    }
    if ($left =~ /Article <[^>]*>: (.*) \(ID [[:xdigit:]]*\) not in keyring/o) {
      $nocem_badsigs{$1}++;
      $nocem_goodsigs{$1} = 0 unless ($nocem_goodsigs{$1});
      $nocem_totalbad++;
      $nocem_lastid = $1;
      return 1;
    }
    if ($left =~ /Article <[^>]*>: bad signature from (.*)/o) {
      $nocem_badsigs{$1}++;
      $nocem_goodsigs{$1} = 0 unless ($nocem_goodsigs{$1});
      $nocem_totalbad++;
      $nocem_lastid = $1;
      return 1;
    }
    if ($left =~ /Article <[^>]*>: malformed signature/o) {
      $nocem_badsigs{'N/A'}++;
      $nocem_goodsigs{'N/A'} = 0 unless ($nocem_goodsigs{'N/A'});
      $nocem_totalbad++;
      $nocem_lastid = 'N/A';
      return 1;
    }

    return 1;
  }

  ###########
  ## controlchan
  if ($prog eq "controlchan") {
    # loaded /x/y/z/foo.pl
    return 1 if $left =~ m/^loaded /;
    # starting
    return 1 if $left =~ m/^starting/;
    # skipping rmgroup x@y (pgpverify failed) in <foo@bar>
    if ($left =~ m/^skipping \S+ (\S+) \(pgpverify failed\) in /) {
      $controlchan_skippgp{$1}++;
      $controlchan_who{$1}++;
      return 1;
    }
    if ($left =~ m/^control_(sendme|ihave), [^,]+, (\S+), doit,/o) {
      if ($1 eq "sendme") {
	$controlchan_sendme_site{$2}++;
      } else {
	$controlchan_ihave_site{$2}++;
      }
      return 1;
    }
    # control_XXgroup, foo.bar [moderated] who who /x/y/12, peer, action, 1
    #
    # Various other random junk can end up in the moderated field, like y,
    # unmoderated, m, etc. depending on what the control message says.  It
    # can even have multiple words, which we still don't handle.
    if ($left =~ m/^control_(\S+),    # type of msg
                  \s(?:\S+)?          # newsgroup name
                  (\s\S+)?            # optional
                  \s(\S+)             # e-mail
                  \s\S+               # e-mail
                  \s\S+,              # filename
                  \s\S+,              # server
                  \s([^=,]+(?:=\S+)?),            # action
                  \s*(.*)             # code
                  /x) {
      if ($1 eq 'newgroup') {
	$controlchan_new{$3}++;
      } elsif ($1 eq 'rmgroup') {
	$controlchan_rm{$3}++;
      } else {
	$controlchan_other{$3}++;
      }
      $controlchan_who{$3}++;
      $controlchan_ok{$3} += $5;
      my $action = $4;
      my $email = $3;
      $action =~ s/=.*//;
      $controlchan_doit{$email}++ if $action eq 'doit';
      return 1;
    }
    # checkgroups processed (no change or not)
    return 1 if $left =~ /^checkgroups by \S+ processed/o;
  }

  ###########
  ## crosspost
  if ($prog eq "crosspost") {
    # seconds 1001 links 3182 0 symlinks 0 0 mkdirs 0 0
    # missing 13 toolong 0 other 0
    if ($left =~ /^seconds\ (\d+)
	           \ links\ (\d+)\ (\d+)
	           \ symlinks\ (\d+)\ (\d+)
	           \ mkdirs\ (\d+)\ (\d+)
	           \ missing\ (\d+)
	           \ toolong\ (\d+)
	           \ other\ (\d+)
	         $/ox) {
      $crosspost_time += $1;
      $crosspost{'Links made'} += $2;
      $crosspost{'Links failed'} += $3;
      $crosspost{'Symlinks made'} += $4;
      $crosspost{'Symlinks failed'} += $5;
      $crosspost{'Mkdirs made'} += $6;
      $crosspost{'Mkdirs failed'} += $7;
      $crosspost{'Files missing'} += $8;
      $crosspost{'Paths too long'} += $9;
      $crosspost{'Others'} += $10;
      return 1;
    }
  }

  ###########
  ## cnfsstat
  if ($prog eq "cnfsstat") {
    # Class ALT for groups matching "alt.*" article size min/max: 0/1048576
    # Buffer T3, len: 1953  Mbytes, used: 483.75 Mbytes (24.8%)   0 cycles
    if ($left =~ m|^Class\ (\S+)\ for\ groups\ matching\ \S+
                    (\ article\ size\ min/max:\ \d+/\d+)?
                    \ Buffer\ (\S+),
                    \ len:\ ([\d.]+)\s+Mbytes,
                    \ used:\ ([\d.]+)\ Mbytes\ \(\s*[\d.]+%\)
                    \s+(\d+)\ cycles\s*
                 $|ox) {
      my ($class, $buffer, $size, $used, $cycles) = ($1, $3, $4, $5, $6);
      my ($h, $m, $s) = $hour =~ m/^(\d+):(\d+):(\d+)$/;
      my $time = $h * 3600 + $m * 60 + $s;
      $size *= 1024 * 1024;
      $used *= 1024 * 1024;
      $cnfsstat{$buffer} = $class;

      # If the size changed, invalidate all of our running fill rate stats.
      if (!exists($cnfsstat_size{$buffer}) ||  $size != $cnfsstat_size{$buffer}) {
        delete $cnfsstat_rate{$buffer};
        delete $cnfsstat_samples{$buffer};
        delete $cnfsstat_time{$buffer};
        $cnfsstat_size{$buffer} = $size;
      }
      elsif ($cnfsstat_time{$buffer}) {
        # We want to gather the rate at which cycbuffs fill.  Store a
        # running total of bytes/second and a total number of samples.
        # Ideally we'd want a weighted average of those samples by the
        # length of the sample period, but we'll ignore that and assume
        # cnfsstat runs at a roughly consistent interval.
        my ($period, $added);
        $period = $time - $cnfsstat_time{$buffer};
        $period = 86400 - $cnfsstat_time{$buffer} + $time if $period <= 0;
        $added = $used - $cnfsstat_used{$buffer};
        if ($cycles > $cnfsstat_cycles{$buffer}) {
          $added += $size * ($cycles - $cnfsstat_cycles{$buffer});
        }
        if ($added > 0) {
          $cnfsstat_rate{$buffer} += $added / $period;
          $cnfsstat_samples{$buffer}++;
        }
      }
      $cnfsstat_used{$buffer} = $used;
      $cnfsstat_cycles{$buffer} = $cycles;
      $cnfsstat_time{$buffer} = $time;
      return 1;
    }
  }

  # Ignore following programs :
  return 1 if ($prog eq "uxfxn");
  return 1 if ($prog eq "beverage");
  return 1 if ($prog eq "newsx");
  return 1 if ($prog eq "demmf");
  return 1 if ($prog eq "nnnn");
  return 1 if ($prog eq "slurp");
  return 0;
}

#################################
# Adjust some values..

sub adjust($$) {
  my ($first_date, $last_date) = @_;

  my $nnrpd_doit = 0;
  my $curious;

  {
    my $serv;
    if (%nnrpd_connect) {
      my @keys = keys (%nnrpd_connect);
      my $c = @keys;
      foreach my $serv (@keys) {
	if ($nnrpd_no_permission{$serv}) {
	  my $dom = &host2dom($serv);
	  $nnrpd_dom_connect{$dom} -= $nnrpd_connect{$serv}
	    if defined $nnrpd_dom_connect{$dom};
	  $nnrpd_dom_groups{$dom}  -= $nnrpd_groups{$serv}
	    if defined $nnrpd_dom_groups{$dom};
	  $nnrpd_dom_times{$dom}   -= $nnrpd_times{$serv}
	    if defined $nnrpd_dom_times{$dom};

          # The message "bad_auth" can occur more then once per session.
          # Subtracting nnrpd_no_permission from nnrpd_connect is
          # broken and can yield negative values for nnrpd_connect.
	  $nnrpd_connect{$serv} -= $nnrpd_no_permission{$serv};

	  $nnrpd_groups{$serv} -= $nnrpd_no_permission{$serv}
	    if defined $nnrpd_groups{$serv};

          # Perl considers negative values to be true. Previously the
          # hash entry was deleted only if the value was exactly 0.
          delete $nnrpd_connect{$serv} unless $nnrpd_connect{$serv} > 0;

	  delete $nnrpd_groups{$serv}  unless $nnrpd_groups{$serv};
	  delete $nnrpd_times{$serv}   unless $nnrpd_times{$serv};
	  delete $nnrpd_usr_times{$serv}   unless $nnrpd_usr_times{$serv};
	  delete $nnrpd_sys_times{$serv}   unless $nnrpd_sys_times{$serv};
	  delete $nnrpd_dom_connect{$dom} unless $nnrpd_dom_connect{$dom};
	  delete $nnrpd_dom_groups{$dom}  unless $nnrpd_dom_groups{$dom};
	  delete $nnrpd_dom_times{$dom}   unless $nnrpd_dom_times{$dom};
	  $c--;
	}
	$nnrpd_doit++
	  if $nnrpd_groups{$serv} || $nnrpd_post_ok{$serv};
      }
      undef %nnrpd_connect unless $c;
    }
    foreach my $serv (keys (%nnrpd_groups)) {
      $curious = "ok" unless $nnrpd_groups{$serv} || $nnrpd_post_ok{$serv} ||
	$nnrpd_articles{$serv};
    }
  }

  # Fill some hashes
  {
    my ($key, $hostname, $channel);

    # If there are any checkpoint counts left over at the end of the file,
    # transfer them to the totals for that server.
    #
    # Also, since the checkpoint counts include entries for all server
    # connections, check to see if any checkpoint server entries are not also
    # in %innd_connect.  Add any missing servers (persistant servers with no
    # connected log lines) to %innd_connect so that incoming totals will be
    # properly computed.
    foreach $server (keys (%innd_cp_accepted)) {
      if (! defined($innd_connect{$server})) {
        $innd_connect{$server} = 0;
      }
      if ($notice_state{$server} eq "checkpoint") {
        $innd_accepted{$server}        += $innd_cp_accepted{$server};
        $innd_refused{$server}         += $innd_cp_refused{$server};
        $innd_rejected{$server}        += $innd_cp_rejected{$server};
        $innd_seconds{$server}         += $innd_cp_seconds{$server};
        $innd_stored_size{$server}     += $innd_cp_stored_size{$server};
        $innd_duplicated_size{$server} += $innd_cp_duplicated_size{$server};
        $innd_rejected_size{$server}   += ($innd_cp_rejected_size{$server} || 0);
      }
    }

    foreach $key (keys (%innd_connect)) {
      $innd_offered{$key} = ($innd_accepted{$key} || 0)
	+ ($innd_refused{$key} || 0)
	+ ($innd_rejected{$key} || 0);
      $innd_offered_size{$key} = ($innd_stored_size{$key} || 0)
	+ ($innd_duplicated_size{$key} || 0) + ($innd_rejected_size{$key} || 0);
    }

    # Sum all incoming traffic for each full server.
    foreach $key (keys (%innd_connect)) {
      ($hostname, $channel) = split(':', $key);
      $innd_seconds_sum{$hostname} += ($innd_seconds{$key} || 0);
      $innd_connect_sum{$hostname} += ($innd_connect{$key} || 0);
      $innd_offered_sum{$hostname} += ($innd_offered{$key} || 0);
      $innd_accepted_sum{$hostname} += ($innd_accepted{$key} || 0);
      $innd_refused_sum{$hostname} += ($innd_refused{$key} || 0);
      $innd_rejected_sum{$hostname} += ($innd_rejected{$key} || 0);
      $innd_stored_size_sum{$hostname} += ($innd_stored_size{$key} || 0);
      $innd_duplicated_size_sum{$hostname} += ($innd_duplicated_size{$key} || 0);
      $innd_offered_size_sum{$hostname} += ($innd_offered_size{$key} || 0);
      $innd_rejected_size_sum{$hostname} += ($innd_rejected_size{$key} || 0);
    }

    # adjust min/max of innd timer stats.
    if (%innd_time_min) {
      foreach $key (keys (%innd_time_min)) {
	$innd_time_min{$key} = 0 if ($innd_time_min{$key} == $MIN);
	$innd_time_max{$key} = 0 if ($innd_time_max{$key} == $MAX);

	#$innd_time_min{$key} /= 1000;
	#$innd_time_max{$key} /= 1000;
      }
    }
    if (%innfeed_time_min) {
      foreach $key (keys (%innfeed_time_min)) {
        $innfeed_time_min{$key} = 0 if ($innfeed_time_min{$key} == $MIN);
        $innfeed_time_max{$key} = 0 if ($innfeed_time_max{$key} == $MAX);
      }
    }
    if (%nnrpd_time_min) {
      foreach $key (keys (%nnrpd_time_min)) {
        $nnrpd_time_min{$key} = 0 if ($nnrpd_time_min{$key} == $MIN);
        $nnrpd_time_max{$key} = 0 if ($nnrpd_time_max{$key} == $MAX);
      }
    }
    # remove the innd timer stats if not used.
    unless ($innd_time_times) {
      undef %innd_time_min;
      undef %innd_time_max;
      undef %innd_time_num;
      undef %innd_time_time;
    }
    # same thing for innfeed timer
    unless ($innfeed_time_times) {
      undef %innfeed_time_min;
      undef %innfeed_time_max;
      undef %innfeed_time_num;
      undef %innfeed_time_time;
    }
    # same thing for nnrpd timer
    unless ($nnrpd_time_times) {
      undef %nnrpd_time_min;
      undef %nnrpd_time_max;
      undef %nnrpd_time_num;
      undef %nnrpd_time_time;
    }

    # adjust the crosspost stats.
    if (%crosspost) {
      foreach $key (keys (%crosspost)) {
	$crosspost_times{$key} = $crosspost_time ?
	  sprintf "%.2f", $crosspost{$key} / $crosspost_time * 60 : "?";
      }
    }
  }

  if (%inn_flow) {
    my ($prev_dd, $prev_d, $prev_h) = ("", -1, -1);
    my $day;
    foreach $day (sort datecmp keys (%inn_flow)) {
      my ($r, $h) = $day =~ /^(.*) (\d+)$/;
      my $d = index ("JanFebMarAprMayJunJulAugSepOctNovDec",
		     substr ($r,0,3)) / 3 * 31 + substr ($r, 4, 2);
      $prev_h = $h if ($prev_h == -1);
      if ($prev_d == -1) {
	$prev_d = $d;
	$prev_dd = $r;
      }
      if ($r eq $prev_dd) { # Same day and same month ?
	if ($h != $prev_h) {
	  if ($h == $prev_h + 1) {
	    $prev_h++;
	  }
	  else {
	    my $j;
	    for ($j = $prev_h + 1; $j < $h; $j++) {
	      my $t = sprintf "%02d", $j;
	      $inn_flow{"$r $t"} = 0;
	    }
	    $prev_h = $h;
	  }
	}
      }
      else {
	my $j;
	# then end of the first day...
	for ($j = ($prev_h == 23) ? 24 : $prev_h + 1; $j < 24; $j++) {
	  my $t = sprintf "%02d", $j;
	  $inn_flow{"$prev_dd $t"} = 0;
	}

	# all the days between (if any)
	# well, we can forget them as it is supposed to be a tool
	# launched daily.

	# the beginning of the last day..
	for ($j = 0; $j < $h; $j++) {
	  my $t = sprintf "%02d", $j;
	  $inn_flow{"$r $t"} = 0;
	}
	$prev_dd = $r;
	$prev_d = $d;
	$prev_h = $h;
      }
    }
    my $first = 1;
    my (%hash, %hash_time, %hash_size, $date, $delay);
    foreach $day (sort datecmp keys (%inn_flow)) {
      my ($r, $h) = $day =~ /^(.*) (\d+)$/o;
      if ($first) {
	$first = 0;
	my ($t) = $first_date =~ m/:(\d\d:\d\d)$/o;
	$date = "$day:$t - $h:59:59";
	$t =~ m/(\d\d):(\d\d)/o;
	$delay = 3600 - $1 * 60 - $2;
      }
      else {
	$date = "$day:00:00 - $h:59:59";
	$delay = 3600;
      }
      $hash{$date} = $inn_flow{$day};
      $hash_size{$date} = $inn_flow_size{$day};
      $inn_flow_labels{$date} = $h;
      $hash_time{$date} = $delay;
    }
    my ($h, $t) = $last_date =~ m/ (\d+):(\d\d:\d\d)$/o;
    my ($h2) = $date =~ m/ (\d+):\d\d:\d\d /o;
    my $date2 = $date;
    $date2 =~ s/$h2:59:59$/$h:$t/;
    $hash{$date2} = $hash{$date};
    delete $hash{"$date"};
    $hash_size{$date2} = $hash_size{$date};
    delete $hash_size{"$date"};
    $t =~ m/(\d\d):(\d\d)/o;
    $hash_time{$date2} = $hash_time{$date} - ($h2 == $h) * 3600 + $1 * 60 + $2;
    delete $hash_time{"$date"};
    $inn_flow_labels{$date2} = $h;
    %inn_flow = %hash;
    %inn_flow_time = %hash_time;
    %inn_flow_size = %hash_size;
  }

  if (%innd_bad_ihave) {
    my $key;
    my $msg = 'Bad ihave control messages received';
    foreach $key (keys %innd_bad_ihave) {
      $innd_misc_stat{$msg}{$key} = $innd_bad_ihave{$key};
    }
  }
  if (%innd_bad_msgid) {
    my $key;
    my $msg = 'Bad Message-ID\'s offered';
    foreach $key (keys %innd_bad_msgid) {
      $innd_misc_stat{$msg}{$key} = $innd_bad_msgid{$key};
    }
  }
  if (%innd_bad_sendme) {
    my $key;
    my $msg = 'Ignored sendme control messages received';
    foreach $key (keys %innd_bad_sendme) {
      $innd_misc_stat{$msg}{$key} = $innd_bad_sendme{$key};
    }
  }
  if (%innd_bad_command) {
    my $key;
    my $msg = 'Bad command received';
    foreach $key (keys %innd_bad_command) {
      $innd_misc_stat{$msg}{$key} = $innd_bad_command{$key};
    }
  }
  if (%innd_bad_newsgroup) {
    my $key;
    my $msg = 'Bad newsgroups received';
    foreach $key (keys %innd_bad_newsgroup) {
      $innd_misc_stat{$msg}{$key} = $innd_bad_newsgroup{$key};
    }
  }
  if (%innd_posted_future) {
    my $key;
    my $msg = 'Article posted in the future';
    foreach $key (keys %innd_posted_future) {
      $innd_misc_stat{$msg}{$key} = $innd_posted_future{$key};
    }
  }
  if (%innd_no_colon_space) {
    my $key;
    my $msg = 'No colon-space in header';
    foreach $key (keys %innd_no_colon_space) {
      $innd_misc_stat{$msg}{$key} = $innd_no_colon_space{$key};
    }
  }
  if (%innd_huge) {
    my $key;
    my $msg = 'Huge articles';
    foreach $key (keys %innd_huge) {
      $innd_misc_stat{$msg}{$key} = $innd_huge{$key};
    }
  }
  if (%innd_blocked) {
    my $key;
    my $msg = 'Blocked server feeds';
    foreach $key (keys %innd_blocked) {
      $innd_misc_stat{$msg}{$key} = $innd_blocked{$key};
    }
  }
  if (%innd_strange_strings) {
    my $key;
    my $msg = 'Including strange strings';
    foreach $key (keys %innd_strange_strings) {
      $innd_misc_stat{$msg}{$key} = $innd_strange_strings{$key};
    }
  }
  if (%rnews_bogus_ng) {
    my $key;
    my $msg = 'Unwanted newsgroups';
    foreach $key (keys %rnews_bogus_ng) {
      $rnews_misc{$msg}{$key} = $rnews_bogus_ng{$key};
    }
  }
  if (%rnews_bogus_dist) {
    my $key;
    my $msg = 'Unwanted distributions';
    foreach $key (keys %rnews_bogus_dist) {
      $rnews_misc{$msg}{$key} = $rnews_bogus_dist{$key};
    }
  }
  if (%rnews_unapproved) {
    my $key;
    my $msg = 'Articles unapproved';
    foreach $key (keys %rnews_unapproved) {
      $rnews_misc{$msg}{$key} = $rnews_unapproved{$key};
    }
  }
  if (%rnews_bogus_date) {
    my $key;
    my $msg = 'Bad Date';
    foreach $key (keys %rnews_bogus_date) {
      $rnews_misc{$msg}{$key} = $rnews_bogus_date{$key};
    }
  }

  $rnews_misc{'Too old'}{'--'} = $rnews_too_old if $rnews_too_old;
  $rnews_misc{'Bad linecount'}{'--'} = $rnews_linecount if $rnews_linecount;
  $rnews_misc{'Duplicate articles'}{'--'} = $rnews_duplicate
    if $rnews_duplicate;
  $rnews_misc{'No colon-space'}{'--'} = $rnews_no_colon_space
    if $rnews_no_colon_space;

  if (%nnrpd_groups) {
    foreach my $key (keys (%nnrpd_connect)) {
      unless ($nnrpd_groups{$key} || $nnrpd_post_ok{$key} ||
	      $nnrpd_articles{$key}) {
	$nnrpd_curious{$key} = $nnrpd_connect{$key};
	delete $nnrpd_connect{$key};
      }
    }
  }
}

sub report_unwanted_ng($) {
  my $file = shift;
  open (FILE, "$file") && do {
    while (<FILE>) {
      my ($c, $n) = $_ =~ m/^\s*(\d+)\s+(.*)$/;
      next unless defined $n;
      $n =~ s/^newsgroup //o; # for pre 1.8 logs
      $inn_uw_ng{$n} += $c;
    }
    close (FILE);
  };

  unlink ("${file}.old");
  rename ($file, "${file}.old");

  open (FILE, "> $file") && do {
    my $g;
    foreach $g (sort {$inn_uw_ng{$b} <=> $inn_uw_ng{$a}} (keys (%inn_uw_ng))) {
      printf FILE "%d %s\n", $inn_uw_ng{$g}, $g;
    }
    close (FILE);
    chmod(0660, "$file");
  };
  unlink ("${file}.old");
}

###########################################################################

# Compare 2 dates (+hour), used with sort (arguments $a and $b)
sub datecmp() {
  # ex: "May 12 06"   for May 12, 6:00am
  local($[) = 0;
  # The 2 dates are near. The range is less than a few days that's why we
  # can cheat to determine the order. It is only important if one date
  # is in January and the other in December.

  my($date1) = substr($a, 4, 2) * 24;
  my($date2) = substr($b, 4, 2) * 24;
  $date1 += index("JanFebMarAprMayJunJulAugSepOctNovDec",substr($a,0,3)) * 288;
  $date2 += index("JanFebMarAprMayJunJulAugSepOctNovDec",substr($b,0,3)) * 288;
  if ($date1 - $date2 > 300 * 24) {
    $date2 += 288 * 3 * 12;
  }
  elsif ($date2 - $date1 > 300 * 24) {
    $date1 += 288 * 3 * 12;
  }
  $date1 += substr($a, 7, 2);
  $date2 += substr($b, 7, 2);
  $date1 - $date2;
}

sub host2dom($) {
  my $host = shift;

  $host =~ m/^[^\.]+(.*)/;
  $host =~ m/^[\d\.]+$/ ? "unresolved" : $1 ? "*$1" : "?";
}

1;
