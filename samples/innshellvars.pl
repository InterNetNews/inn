# 
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Sat, 24 Aug 1996 22:08:19 +0200
# Project:      INN 
# File:         innshellvars.pl
# RCSId:        $Id$
# Description:  Set up any and all variables that an INN perl script
#               might need.

package inn ;



##  =()<$active = "@<_PATH_ACTIVE>@" ;>()=
$active = "/var/news/etc/active" ;

##  =()<$activetimes = "@<_PATH_ACTIVETIMES>@" ;>()=
$activetimes = "/var/news/etc/active.times" ;

##  =()<$awk = "@<_PATH_AWK>@" ;>()=
$awk = "/usr/bin/awk" ;

##  =()<$batch = "@<_PATH_BATCHDIR>@" ;>()=
$batch = "/var/news/spool/out.going" ;

##  =()<$sendmail = "@<_PATH_SENDMAIL>@" ;>()=
$sendmail = "/usr/lib/sendmail -t" ;

##  =()<$compress = "@<_PATH_COMPRESS>@" ;>()=
$compress = "/usr/bin/compress" ;

##  =()<$controlprogs = "@<_PATH_CONTROLPROGS>@" ;>()=
$controlprogs = "/usr/news/bin/control" ;

##  =()<$ctlfile = "@<_PATH_CONTROLCTL>@" ;>()=
$ctlfile = "/var/news/etc/control.ctl" ;

##  =()<$cycles = @<LOG_CYCLES>@ ;>()=
$cycles = 3 ;

##  =()<$daily = "@<_PATH_LOCKS>@/LOCK.news.daily" ;>()=
$daily = "/var/news/locks/LOCK.news.daily" ;

##  =()<$inndf = "@<INNWATCH_DF>@" ;>()=
$inndf = "/bin/df" ;

##  =()<$egrep = "@<_PATH_EGREP>@" ;>()=
$egrep = "/usr/bin/egrep" ;

##  =()<$errlog = "@<_PATH_ERRLOG>@" ;>()=
$errlog = "/var/log/news/errlog" ;

##  =()<$ctlwatch = "@<_PATH_CTLWATCH>@" ;>()=
$ctlwatch = "/var/news/etc/innwatch.ctl" ;

##  =()<$have_uustat = ("@<HAVE_UUSTAT>@" eq "DO" ? 1 : 0) ;>()=
$have_uustat = ("DO" eq "DO" ? 1 : 0) ;

##  =()<$history = "@<_PATH_HISTORY>@" ;>()=
$history = "/var/news/etc/history" ;

##  =()<$incoming = "@<_PATH_SPOOLNEWS>@" ;>()=
$incoming = "/var/news/spool/in.coming" ;

##  =()<$inews = "@<_PATH_INEWS>@" ;>()=
$inews = "/usr/news/bin/inews" ;

##  =()<$innconfval = "@<_PATH_NEWSBIN>@/innconfval" ;>()=
$innconfval = "/usr/news/bin/innconfval" ;

##  =()<$innd = "@<_PATH_INND>@" ;>()=
$innd = "/usr/news/bin/innd" ;

##  =()<$innddir = "@<_PATH_INNDDIR>@" ;>()=
$innddir = "/var/news/run" ;

##  =()<$inndstart = "@<_PATH_INNDSTART>@" ;>()=
$inndstart = "/usr/news/bin/inndstart" ;

##  =()<$innwatch = "@<_PATH_NEWSBIN>@/innwatch" ;>()=
$innwatch = "/usr/news/bin/innwatch" ;

##  =()<$locks = "@<_PATH_LOCKS>@" ;>()=
$locks = "/var/news/locks" ;

##  =()<$log = "@<_PATH_LOGFILE>@" ;>()=
$log = "/var/log/news/news" ;

##  =()<$mail_badcontrols = ("@<MAIL_BADCONTROLS>@" eq "DO" ? 1 : 0) ;>()=
$mail_badcontrols = ("DONT" eq "DO" ? 1 : 0) ;

##  =()<$mailcmd = "@<_PATH_MAILCMD>@" ;>()=
$mailcmd = "/usr/bin/Mail" ;

##  =()<$most_logs = "@<_PATH_MOST_LOGS>@" ;>()=
$most_logs = "/var/log/news" ;

##  =()<$newactive = "@<_PATH_NEWACTIVE>@" ;>()=
$newactive = "/var/news/etc/active.tmp" ;

##  =()<$newsbin = "@<_PATH_NEWSBIN>@" ;>()=
$newsbin = "/usr/news/bin" ;

##  =()<$newscontrol = "@<_PATH_NEWSCONTROL>@" ;>()=
$newscontrol = "/var/news/run/control" ;

##  =()<$newsfeeds = "@<_PATH_NEWSFEEDS>@" ;>()=
$newsfeeds = "/var/news/etc/newsfeeds" ;

##  =()<$newsgroups = "@<_PATH_NEWSGROUPS>@" ;>()=
$newsgroups = "/var/news/etc/newsgroups" ;

##  =()<$newslib = "@<_PATH_NEWSLIB>@" ;>()=
$newslib = "/var/news/etc" ;

##  =()<$newsmaster = '@<NEWSMASTER>@' ;>()=
$newsmaster = 'usenet' ;

##  =()<$newsuser = "@<NEWSUSER>@" ;>()=
$newsuser = "news" ;

##  =()<$nntpconnect = "@<_PATH_NNTPCONNECT>@" ;>()=
$nntpconnect = "/var/news/run/nntpin" ;

##  =()<$oldactive = "@<_PATH_OLDACTIVE>@" ;>()=
$oldactive = "/var/news/etc/active.old" ;

##  =()<$perl = "@<_PATH_PERL>@" ;>()=
$perl = "/usr/bin/perl" ;

##  =()<$rnews = "@<_PATH_RNEWS>@" ;>()=
$rnews = "/usr/news/bin/rnews" ;

##  =()<$sed = "@<_PATH_SED>@" ;>()=
$sed = "/usr/bin/sed" ;

##  =()<$sort = "@<_PATH_SORT>@" ;>()=
$sort = "/usr/bin/sort" ;

##  =()<$serverpid = "@<_PATH_SERVERPID>@" ;>()=
$serverpid = "/var/news/run/innd.pid" ;

##  =()<$sleeptime = @<INNWATCH_SLEEPTIME>@ ;>()=
$sleeptime = 600 ;

##  =()<$spool = "@<_PATH_SPOOL>@" ;>()=
$spool = "/var/news/spool/articles" ;

##  =()<$overviewdir = "@<_PATH_OVERVIEWDIR>@" ;>()=
$overviewdir = "/var/news/spool/over.view" ;

##  =()<$innwstatus = "@<_PATH_INNWSTATUS>@" ;>()=
$innwstatus = "/var/news/run/innwatch.status" ;

##  =()<($tempsock = "@<_PATH_TEMPSOCK>@") =~ s!.*/(.*)XXXXXX$!$1*! ;>()=
($tempsock = "/var/news/run/ctlinndXXXXXX") =~ s!.*/(.*)XXXXXX$!$1*! ;

##  =()<($tempsockdir = "@<_PATH_TEMPSOCK>@") =~ s!/[^/]*$!! ;>()=
($tempsockdir = "/var/news/run/ctlinndXXXXXX") =~ s!/[^/]*$!! ;

##  =()<$want_pgpverify = ("@<WANT_PGPVERIFY>@" eq "DO" ? 1 : 0) ;>()=
$want_pgpverify = ("DO" eq "DO" ? 1 : 0) ;

##  =()<$watchpid = "@<_PATH_WATCHPID>@" ;>()=
$watchpid = "/var/news/run/innwatch.pid" ;

##  =()<$z = "@<_PATH_COMPRESSEXT>@" ;>()=
$z = ".Z" ;

##  =()<$path_motd = "@<_PATH_MOTD>@" ;>()=
$path_motd = "/var/news/etc/motd.news" ;

##  =()<$path_storagectl = "@<_PATH_STORAGECTL>@" ;>()=
$path_storagectl = "/var/news/etc/storage.ctl" ;

##  =()<$path_overviewctl = "@<_PATH_OVERVIEWCTL>@" ;>()=
$path_overviewctl = "/var/news/etc/overview.ctl" ;

##  =()<$perl_support = ("@<PERL_SUPPORT>@" eq "DO" ? 1 : 0) ;>()=
$perl_support = ("DONT" eq "DO" ? 1 : 0) ;

##  =()<$perl_startup_innd = "@<_PATH_PERL_STARTUP_INND>@" ;>()=
$perl_startup_innd = "/usr/news/bin/control/startup_innd.pl" ;

##  =()<$perl_filter_innd = "@<_PATH_PERL_FILTER_INND>@" ;>()=
$perl_filter_innd = "/usr/news/bin/control/filter_innd.pl" ;

##  =()<$perl_filter_nnrpd = "@<_PATH_PERL_FILTER_NNRPD>@" ;>()=
$perl_filter_nnrpd = "/usr/news/bin/control/filter_nnrpd.pl" ;

##  =()<$tcl_support = ("@<TCL_SUPPORT>@" eq "DO" ? 1 : 0) ;>()=
$tcl_support = ("DONT" eq "DO" ? 1 : 0) ;

#### =()<$path_tcl_startup = "@<_PATH_TCL_STARTUP>@" ;>()=
$path_tcl_startup = "/usr/news/bin/control/startup.tcl" ;

#### =()<$path_tcl_filter = "@<_PATH_TCL_FILTER>@" ;>()=
$path_tcl_filter = "/usr/news/bin/control/filter.tcl" ;

$localgroups = "$newslib/localgroups" ;

if ( ! defined ($ENV{'TMPDIR'}) ) {
    ##  =()<	$ENV{'TMPDIR'} = "@<_PATH_TMP>@" ;>()=
	$ENV{'TMPDIR'} = "/var/tmp" ;
} 
$tmpdir = $ENV{'TMPDIR'} ;

##  =()<$expirectl = "@<_PATH_EXPIRECTL>@" ;>()=
$expirectl = "/var/news/etc/expire.ctl" ;

##  =()<$newshome = "@<_PATH_NEWSHOME>@" ;>()=
$newshome = "/usr/news" ;

##  =()<$archivedir = "@<_PATH_ARCHIVEDIR>@" ;>()=
$archivedir = "/var/news/spool/archive" ;

##  =()<$badnews = "@<_PATH_BADNEWS>@" ;>()=
$badnews = "/var/news/spool/in.coming/bad" ;

#### =()<$spoolnews = "@<_PATH_SPOOLNEWS>@" ;>()=
$spoolnews = "/var/news/spool/in.coming" ;

#### =()<$spooltemp = "@<_PATH_SPOOLTEMP>@" ;>()=
$spooltemp = "/var/tmp" ;

$newslbin = "$newshome/local" ;

##  =()<$umask = @<NEWSUMASK>@ ;>()=
$umask = 02 ;

$ENV{'PATH'} = "${newslbin}:${newsbin}:$ENV{'PATH'}:/bin:/usr/bin:/usr/ucb" ;

1 ;
