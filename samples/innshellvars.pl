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
$active = "/news/etc/active" ;

##  =()<$activetimes = "@<_PATH_ACTIVETIMES>@" ;>()=
$activetimes = "/news/etc/active.times" ;

##  =()<$awk = "@<_PATH_AWK>@" ;>()=
$awk = "/usr/bin/awk" ;

##  =()<$batch = "@<_PATH_BATCHDIR>@" ;>()=
$batch = "/news/spool/out.going" ;

##  =()<$compress = "@<_PATH_COMPRESS>@" ;>()=
$compress = "/usr/bin/compress" ;

##  =()<$controlprogs = "@<_PATH_CONTROLPROGS>@" ;>()=
$controlprogs = "/news/bin/control" ;

##  =()<$ctlfile = "@<_PATH_CONTROLCTL>@" ;>()=
$ctlfile = "/news/etc/control.ctl" ;

##  =()<$cycles = @<LOG_CYCLES>@ ;>()=
$cycles = 3 ;

##  =()<$daily = "@<_PATH_LOCKS>@/LOCK.news.daily" ;>()=
$daily = "/news/locks/LOCK.news.daily" ;

##  =()<$inndf = "@<INNWATCH_DF>@" ;>()=
$inndf = "/bin/df" ;

##  =()<$egrep = "@<_PATH_EGREP>@" ;>()=
$egrep = "/usr/bin/egrep" ;

##  =()<$errlog = "@<_PATH_ERRLOG>@" ;>()=
$errlog = "/news/log/errlog" ;

##  =()<$ctlwatch = "@<_PATH_CTLWATCH>@" ;>()=
$ctlwatch = "/news/etc/innwatch.ctl" ;

##  =()<$have_uustat = ("@<HAVE_UUSTAT>@" eq "DO" ? 1 : 0) ;>()=
$have_uustat = ("DO" eq "DO" ? 1 : 0) ;

##  =()<$history = "@<_PATH_HISTORY>@" ;>()=
$history = "/news/etc/history" ;

##  =()<$incoming = "@<_PATH_SPOOLNEWS>@" ;>()=
$incoming = "/news/spool/in.coming" ;

##  =()<$inews = "@<_PATH_INEWS>@" ;>()=
$inews = "/news/bin/inews" ;

##  =()<$innconfval = "@<_PATH_NEWSBIN>@/innconfval" ;>()=
$innconfval = "/news/bin/innconfval" ;

##  =()<$innd = "@<_PATH_INND>@" ;>()=
$innd = "/news/bin/innd" ;

##  =()<$innddir = "@<_PATH_INNDDIR>@" ;>()=
$innddir = "/news/run" ;

##  =()<$inndstart = "@<_PATH_INNDSTART>@" ;>()=
$inndstart = "/news/bin/inndstart" ;

##  =()<$innwatch = "@<_PATH_NEWSBIN>@/innwatch" ;>()=
$innwatch = "/news/bin/innwatch" ;

##  =()<$locks = "@<_PATH_LOCKS>@" ;>()=
$locks = "/news/locks" ;

##  =()<$log = "@<_PATH_LOGFILE>@" ;>()=
$log = "/news/log/news" ;

##  =()<$mail_badcontrols = ("@<MAIL_BADCONTROLS>@" eq "DO" ? 1 : 0) ;>()=
$mail_badcontrols = ("DONT" eq "DO" ? 1 : 0) ;

##  =()<$mailcmd = "@<_PATH_MAILCMD>@" ;>()=
$mailcmd = "/usr/bin/Mail" ;

##  =()<$most_logs = "@<_PATH_MOST_LOGS>@" ;>()=
$most_logs = "/news/log" ;

##  =()<$newactive = "@<_PATH_NEWACTIVE>@" ;>()=
$newactive = "/news/etc/active.tmp" ;

##  =()<$newsbin = "@<_PATH_NEWSBIN>@" ;>()=
$newsbin = "/news/bin" ;

##  =()<$newscontrol = "@<_PATH_NEWSCONTROL>@" ;>()=
$newscontrol = "/news/run/control" ;

##  =()<$newsfeeds = "@<_PATH_NEWSFEEDS>@" ;>()=
$newsfeeds = "/news/etc/newsfeeds" ;

##  =()<$newsgroups = "@<_PATH_NEWSGROUPS>@" ;>()=
$newsgroups = "/news/etc/newsgroups" ;

##  =()<$newslib = "@<_PATH_NEWSLIB>@" ;>()=
$newslib = "/news/etc" ;

##  =()<$newsmaster = "@<NEWSMASTER>@" ;>()=
$newsmaster = "usenet" ;

##  =()<$newsuser = "@<NEWSUSER>@" ;>()=
$newsuser = "news" ;

##  =()<$nntpconnect = "@<_PATH_NNTPCONNECT>@" ;>()=
$nntpconnect = "/news/run/nntpin" ;

##  =()<$oldactive = "@<_PATH_OLDACTIVE>@" ;>()=
$oldactive = "/news/etc/active.old" ;

##  =()<$perl = "@<_PATH_PERL>@" ;>()=
$perl = "/usr/bin/perl" ;

##  =()<$rnews = "@<_PATH_RNEWS>@" ;>()=
$rnews = "/news/bin/rnews" ;

##  =()<$sed = "@<_PATH_SED>@" ;>()=
$sed = "/bin/sed" ;

##  =()<$sort = "@<_PATH_SORT>@" ;>()=
$sort = "/usr/bin/sort" ;

##  =()<$serverpid = "@<_PATH_SERVERPID>@" ;>()=
$serverpid = "/news/run/innd.pid" ;

##  =()<$sleeptime = @<INNWATCH_SLEEPTIME>@ ;>()=
$sleeptime = 600 ;

##  =()<$spool = "@<_PATH_SPOOL>@" ;>()=
$spool = "/news/spool/articles" ;

##  =()<$overviewdir = "@<_PATH_OVERVIEWDIR>@" ;>()=
$overviewdir = "/news/spool/over.view" ;

##  =()<$innwstatus = "@<_PATH_INNWSTATUS>@" ;>()=
$innwstatus = "/news/run/innwatch.status" ;

##  =()<($tempsock = "@<_PATH_TEMPSOCK>@") =~ s!.*/(.*)XXXXXX$!$1*! ;>()=
($tempsock = "/news/run/ctlinndXXXXXX") =~ s!.*/(.*)XXXXXX$!$1*! ;

##  =()<($tempsockdir = "@<_PATH_TEMPSOCK>@") =~ s!/[^/]*$!! ;>()=
($tempsockdir = "/news/run/ctlinndXXXXXX") =~ s!/[^/]*$!! ;

##  =()<$want_pgpverify = ("@<WANT_PGPVERIFY>@" eq "DO" ? 1 : 0) ;>()=
$want_pgpverify = ("DO" eq "DO" ? 1 : 0) ;

##  =()<$watchpid = "@<_PATH_WATCHPID>@" ;>()=
$watchpid = "/news/run/innwatch.pid" ;

##  =()<$z = "@<_PATH_COMPRESSEXT>@" ;>()=
$z = ".Z" ;

##  =()<$perl_support = ("@<PERL_SUPPORT>@" eq "DO" ? 1 : 0) ;>()=
$perl_support = ("DONT" eq "DO" ? 1 : 0) ;

##  =()<$perl_startup_innd = "@<_PATH_PERL_STARTUP_INND>@" ;>()=
$perl_startup_innd = "/news/bin/control/startup_innd.pl" ;

##  =()<$perl_filter_innd = "@<_PATH_PERL_FILTER_INND>@" ;>()=
$perl_filter_innd = "/news/bin/control/filter_innd.pl" ;

##  =()<$perl_filter_nnrpd = "@<_PATH_PERL_FILTER_NNRPD>@" ;>()=
$perl_filter_nnrpd = "/news/bin/control/filter_nnrpd.pl" ;

##  =()<$tcl_support = ("@<TCL_SUPPORT>@" eq "DO" ? 1 : 0) ;>()=
$tcl_support = ("DONT" eq "DO" ? 1 : 0) ;

#### =()<$path_tcl_startup = "@<_PATH_TCL_STARTUP>@" ;>()=
$path_tcl_startup = "/news/bin/control/startup.tcl" ;

#### =()<$path_tcl_filter = "@<_PATH_TCL_FILTER>@" ;>()=
$path_tcl_filter = "/news/bin/control/filter.tcl" ;

$localgroups = "$newslib/localgroups" ;

if ( ! defined ($ENV{'TMPDIR'}) ) {
    ##  =()<	$ENV{'TMPDIR'} = "@<_PATH_TMP>@" ;>()=
	$ENV{'TMPDIR'} = "/var/tmp" ;
} 
$tmpdir = $ENV{'TMPDIR'} ;

##  =()<$expirectl = "@<_PATH_EXPIRECTL>@" ;>()=
$expirectl = "/news/etc/expire.ctl" ;

##  =()<$newshome = "@<_PATH_NEWSHOME>@" ;>()=
$newshome = "/news" ;

##  =()<$archivedir = "@<_PATH_ARCHIVEDIR>@" ;>()=
$archivedir = "/news/spool/archive" ;

##  =()<$badnews = "@<_PATH_BADNEWS>@" ;>()=
$badnews = "/news/spool/in.coming/bad" ;

#### =()<$spoolnews = "@<_PATH_SPOOLNEWS>@" ;>()=
$spoolnews = "/news/spool/in.coming" ;

#### =()<$spooltemp = "@<_PATH_SPOOLTEMP>@" ;>()=
$spooltemp = "/var/tmp" ;

$newslbin = "$newshome/local" ;

##  =()<$umask = @<NEWSUMASK>@ ;>()=
$umask = 02 ;

$ENV{'PATH'} = "${newslbin}:${newsbin}:$ENV{'PATH'}:/bin:/usr/bin:/usr/ucb" ;

1 ;
