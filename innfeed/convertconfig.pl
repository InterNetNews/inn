#!/usr/bin/perl
# 
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Sun, 19 Jan 1997 21:19:24 +0100
# Project:      INN (innfeed)
# File:         convertconfig.pl
# RCSId:        $Id$
# Description:  Read in a old version of innfeed.conf on the command line 
#		or on stdin, and write a new version on stdout.
# 

require 'ctime.pl' ;


@keyorder = (
'news-spool',
'pid-file',
'debug-level',
'use-mmap',
'log-file',
'stdio-fdmax',
'backlog-directory',
'backlog-rotate-period',
'backlog-ckpt-period',
'backlog-newfile-period',
'dns-retry',
'dns-expire',
'close-period',
'gen-html',
'status-file',
'connection-stats',
'host-queue-highwater',
'stats-period',
'stats-reset',
'max-reconnect-time',
'initial-reconnect-time',
'article-timeout',
'response-timeout',
'initial-connections',
'max-connections',
'max-queue-size',
'streaming',
'no-check-high',
'no-check-low',
'port-number',
'backlog-limit',
'backlog-factor',
'backlog-limit-highwater'
);

%procDefaults = (
'news-spool', '/var/news/spool/articles',
'pid-file', 'innfeed.pid',
'debug-level', '0',
'use-mmap', 'false',
'log-file', 'innfeed.log',
'stdio-fdmax', '0',
'backlog-directory', '/var/news/spool/innfeed',
'backlog-rotate-period', '60',
'backlog-ckpt-period', '30',
'backlog-newfile-period', '600',
'dns-retry', '900',
'dns-expire', '86400',
'close-period', '3600',
'gen-html', 'false',
'status-file', 'innfeed.status',
'connection-stats', 'false',
'host-queue-highwater', '10',
'stats-period', '600',
'stats-reset', '43200',
'max-reconnect-time', '3600',
'initial-reconnect-time', '30',
'article-timeout', '600',
'response-timeout', '300',
'initial-connections', '1',
'max-connections', '5',
'max-queue-size', '25',
'streaming', 'true',
'no-check-high', '195.0',
'no-check-low', '90.0',
'port-number', '119',
'backlog-limit', '0',
'backlog-factor', '1.10',
'backlog-limit-highwater', '0',
		 );

%defaultKeys = ('article-timeout', 1,
		'response-timeout', 1,
		'initial-connections', 1,
		'max-connections', 1,
		'max-queue-size', 1,
		'streaming', 1,
		'no-check-high', 1,
		'no-check-low', 1,
		'port-number', 1,
		'backlog-limit', 1) ;

@defaultOrder = ('article-timeout', 
		'response-timeout', 
		'initial-connections', 
		'max-connections', 
		'max-queue-size', 
		'streaming', 
		'no-check-high', 
		'no-check-low', 
		'port-number', 
		'backlog-limit') ;

%formats = () ;

foreach $key (keys %procDefaults) {
    $max = length ($key) if length ($key) > $max ;
    if ($procDefaults{$key} =~ /^true$/i || $procDefaults{$key} =~ /^false$/i){
	$formats{$key} = "%s" ;
    } elsif ($procDefaults{$key} =~ /^\d+$/) {
	$formats{$key} = "%d" ;
    } elsif ($procDefaults{$key} =~ /^\d+\.\d*$/) {
	$formats{$key} = "%.4f" ;
    } else {
	$formats{$key} = "%s" ;
    }
}


while (<>) {
    next if /^\s*$/ ;
    next if /^#/ ;

    chop ;
    @F = split (':') ;

    if ($F[0] eq "default") {
	$procDefaults{'article-timeout'} = $F[2] ;
	$procDefaults{'response-timeout'} = $F[3] ;
	$procDefaults{'initial-connections'} = $F[4] ;
	$procDefaults{'max-connections'} = $F[5] ;
	$procDefaults{'max-queue-size'} = $F[6] ;
	$procDefaults{'streaming'} = $F[7] ;
	$procDefaults{'no-check-low'} = $F[8] * 10.0 ;
	$procDefaults{'no-check-high'} = $F[9] * 10.0 ;
	$procDefaults{'port-number'} = $F[10] ;

	printf "## This file was automatically generated created by $0\n" ;
	printf "## On %s##\n\n", &ctime(time) ;

	foreach $key (@keyorder) {
	    next if $defaultKeys{$key} ;

	    die "No format for $key\n" unless $formats{$key} ;
	    $format = "%${max}s:\t" . $formats{$key} . "\n" ;
	    printf $format, $key, $procDefaults{$key} ;
	}

	printf "\n\n## Defaults merged from:\n##\t$_\n\n" ;
	foreach $key (@defaultOrder) {
	    die "No format for $key\n" unless $formats{$key} ;
	    $format ="%${max}s:\t" . $formats{$key} . "\n" ;
	    printf $format, $key, $procDefaults{$key} ;
	}
	print "\n\n\n" ;
	$gotDefault = 1 ;
    } elsif (@F == 0) {
	die "Badly formed line: $0\n" ;
    } else {
	if (!$gotDefault) {
	    $gotDefault = 1 ;	# warn only one time.
	    warn "No default line was present.\n" ;
	}

	print "## Peer created from:\n" ;
	print "##\t$_\n\n" ;
	printf "peer %s {\n", $F[0] ;

	printf "\tip-name: $F[1]\n" if $F[1] && $F[0] ne $F[1] ;
	printf "\tarticle-timeout: %d\n", $F[2] if $F[2] ;
	printf "\tresponse-timeout: %d\n", $F[3] if $F[3] ;
	printf "\tinitial-connections: %d\n", $F[4] if ($F[4] ne "") ;
	printf "\tmax-connections: %d\n", $F[5] if ($F[5] ne "") ;
	printf "\tmax-queue-size: %d\n", $F[6] if ($F[6] ne "") ;
	printf "\tstreaming: %s\n", $F[7] if ($F[7] ne "") ;
	printf "\tno-check-high: %0.2f\n", $F[9] * 10.0 if ($F[9] ne "") ;
	printf "\tno-check-low: %0.2f\n", $F[8] * 10.0 if ($F[8] ne "") ;
	printf "\tport-number: %d\n", $F[10] if ($F[10] ne "") ;

	print "}\n\n\n" ;
    }
}
	
	
