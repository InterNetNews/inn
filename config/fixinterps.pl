#!/usr/bin/perl
# 
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Fri, 08 Nov 1996 14:48:44 +0100
# Project:      INN
# File:         fixinterps.pl
# RCSId:        $Id$
# Description:	Fix interpreter scripts based on config.data values (as
#               substituted into innshellvars.pl).
#
#               The normal subst must have been run on innshellvars.pl
#               beforehand. This doesn't get subst'd as it's done 
#               before installation of innshellvars.pl (and hence must 
#               use the on in the samples directory)
# 

require 'getopts.pl' ;

$0 =~ s!.*/!! ;

$usage = "$0 [ -v -n -q ][ -d dir ][ -f innshellvars.pl ][ -t topdir ] file...\n" ;

&Getopts ("vnqd:f:h") || die $usage ;

die $usage if $opt_h ;

$topdir = ($opt_t || "..") ;
if ( ! -d $topdir ) {
    warn "No such directory: $topdir\n" ;
    $die = 1 ;
}

$config = ($opt_f || "$topdir/samples/innshellvars.pl") ;
if ( ! -f $config ) {
    warn "No such file: $config\n" ;
    $die = 1 ;
}

$dir = ( $opt_d || "." ) ;
if ( ! -d $dir ) {
    warn "No such directory: $dir\n" ;
    $die = 1 ;
}

$verbose = $opt_v ;
$quiet = $opt_q ;

exit (1) if $die ;

do "$config" ;

%interps = ( 'perl', $inn'perl,  #'
	    ) ;

chdir ($dir) ;

foreach ( @ARGV ) {
    &fixInterp ($_) ;
}



# fix up the FILE script.
sub fixInterp {
    local ($file) = @_ ;

    if ( ! -f $file ) {
	warn "$file: no such file\n" ;
	return ;
    }

    open (INFILE, "<$file") || die "open($file): $!" ;

    chop ($line = <INFILE>) ;

    ($interp = $line) =~ s/^#!\s*(\S+).*/$1/ ;
    ($base = $interp) =~ s!^.*/!! ;

    if ( ! defined ($interps{$base}) ) {
	close (INFILE) ;
	warn "$file: no substitution defined for $base\n" unless $quiet ;
	return ;
    } elsif ( $interps{$base} eq $interp ) {
	close (INFILE) ;
	warn "$file: no change\n" if $verbose ;
	return ;
    }
	
    $line =~ s/#!(\s*\S+)(.*)/#!$interps{$base}$2/ ;

    ($dev,$ino,$mode,$a,$uid,$gid,$a,$a,$atime,$mtime) = 
	stat INFILE ;
    $mode = 0755 unless $dev ;
    $mtime = time unless $dev ;

    rename ($file,"$file.bak") ;

    if ( !open (OUTFILE,">$file") ) {
	close (INFILE) ;
	warn "open($file): $!" ;
	return ;
    }

    print OUTFILE "$line\n" ;
    while ( <INFILE> ) {
	print OUTFILE $_ ;
    }
    close (OUTFILE) ;
		
    chmod $mode, $file ;
    chown ($uid, $gid, $file) if ($> == 0) ;
    utime ($atime,$mtime,$file) || die "utime ($atime,$mtime,$file)" ;

    warn "$file: to $interps{$base}\n" if $verbose ;
    warn "$file: uid is different\n" if ($> != 0 && $uid != $> && !$quiet) ;

    close (INFILE) ;
}
	

