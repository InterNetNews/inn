#!/usr/local/bin/perl
#
# autoinsert.pl - File substitution mechanism generalized, but originally for INN
# written by dave@jetcafe.org (Dave Hayes). Send bugs there.
#
require 5.000; # Perl 4 has no POD

=head1 SYNOPSIS
	autoinsert.pl target_file

=head1 DESCRIPTION
	autoinsert.pl uses lines embedded inside comments to define
	contiguous lines as "automatically inserted sections", and
	what source file and subsets of lines thereof, should be
	inserted into those sections.

	In many programming languages, declarations, intialization,
	documentation, and variable cleanup, interface to external
	files, etc. cannot be located together for semantic or syntax
	reasons.  When a change must be made, it requires making changes
	in many locations.

	autoinsert.pl permits all of the items to be located together,
	and "split out" into the files where they are used.  This can
	make maintenance and development much easier.

=head1 OPERATION AND SYNTAX

	autoinsert.pl scans the named target file looking for sections noted
	for automatic replacement, by having a line with the keyword:
		    BEGIN_AUTO_INSERTED_SECTION
	as the SECOND whitespace-separated string on the line. (The first 
        whitespace-separated string is generally a comment lead-in sequence,
        and it may be preceded by whitespace.)

	Example:
		/* BEGIN_AUTO_INSERTED_SECTION from ../config.h DEFAULT  */

	The third whitespace-separated string must be the four characters "from"
	in lower case.  It is an error if this string is not "from."

	The fourth whitespace-separated string must be an absolute path (beginning
        with '/') or a relative path from the directory containing the file 
	being processed.  This is the	"source file."

	The fifth whitespace-separated string must be a "line selector tag."
	All lines in the source file which begin with this sequence of characters
        will be inserted into the section, with the line selector tag itself
	plus one trailing whitespace character removed.

        It is recommended that selector tags begin with '||', followed
        by an alphanumeric label which notes the destination of the
        lines.  

	All characters after the fifth whitespace-separated string are ignored
	by autoinsert.pl.  (They generally include a comment end sequence.)

        The end of an automatically inserted section is determined by a line
        with the keyword:
              END_AUTO_INSERTED_SECTION
        as the second whitespace-separated string.  Other whitespace
        separated strings on that line are not checked.  If there is no
	matching end for a begin, an error is reported which includes the
	line number of the begin.

        When processing, the previous contents of the section are removed.
        The lines with the "begin" and "end" keywords as well as all lines 
	outside those sections, are preserved unaltered in the target file. (Of 
	course, the source file remains unchanged.)

	There may be 0 or more auto-insert sections in a file.  They do not
	have to reference the same source file.  They will generally have
	different line selector tags.

=cut

#########
# Revision history
# 
# !$Id$
# !$Log$
# !Revision 1.3  1998/03/04 12:55:04  mibsoft
# !Improved error handling in autoinsert.pl
# !
# !Revision 1.2  1998/02/27 04:24:54  mibsoft
# !More robust error checking in the autoinsert.pl utility.
# !parameters to END_AUTO_INSERTED_SECTION must match the
# !begin line.
# !
# !From: dave@jetcafe.org
# !
#
###########

### Switches and args
sub usage { die "usage: autoinsert.pl [-replace] <target file>\n"; }
$replace = 0;
while ($ARGV[0] =~ /^\-/) {
    my $switch = shift @ARGV;
    SWITCH: for ($switch) {
        /^-replace/           && do { $replace = 1; last; };
        usage();
    }
}
$infile = shift @ARGV;
usage() if (!defined($infile) || $infile eq "");

### Get the basename
if ($infile =~ /^(.*)\/([^\/]*)$/) {
    $basename = $1;
    $infile = $2;
    chdir($basename);
} else {
    $basename = "";
    # this directory
}

### Now figure out where our files are
if ($infile eq "-") {
    *INFILE = *STDIN;
} else {
    open(INFILE,"<$infile") || die "Can't open target file '$infile': $!\n";
}

if ($replace) {
    if ($infile eq "-") {
	*OUTFILE = *STDOUT;
	$replace = 0;        # Duh. Bad user.
    } else {
	open(OUTFILE,">$infile.tmp") || die "Can't open replacement file '$infile.tmp': $!\n";
    }
} else {
    *OUTFILE = *STDOUT;
}

### Do the substitution thang
$state = 0; $line = 0; 
while (<INFILE>) {
    $line++;

    # check for beginning token
    if (/^\S+\s+BEGIN_AUTO_INSERTED_SECTION\s+(\S+)\s+(\S+)\s+(\S+)\s/o) {
	if ($state != 0) {
	    warn "Error (BEGIN on line $state): Mismatched auto insert sections\n";
	    next;
	}
	$from = $1;
	$file = $2;
	$tag = $3;
	$sub = "";
	
	# Yes, it's an error, but we -could- just ignore the line
	if ($from ne "from") {
	    warn "Error (line $line): no 'from' token.\n";
	    print OUTFILE;
	    next;
	}
	
	if (! -e $file) {
	    warn "Error (line $line): file '$file' does not exist.\n";
	    print OUTFILE;
	    next;
	}
	
	# Load our substitutions
	print OUTFILE;
	if (!open(SUBS,"<$file")) {
	    warn "Error (line $line): can't open '$file': $!.\n";
	    next;
	}
	while ($foo = <SUBS>) {
	    if ($foo =~ /^(\S+)\s+(.*)/) {
		$sub .= $2."\n" if ($tag eq $1);
	    }
	}
	close(SUBS);
	
	$state = $line;   # Remember where this line was
	
	# Clean these up so we can match against them later
	$from =~ s/([^\w\d])/\\\1/go;
	$file =~ s/([^\w\d])/\\\1/go;
	$tag =~ s/([^\w\d])/\\\1/go;
	next;
    } 

    # check for end token
    if (/^\S+\s+END_AUTO_INSERTED_SECTION\s+/o) {
	if ($state == 0) {
	    warn "Error (line $line): No BEGIN for this END token\n";
	    next;
	}
	if (/^\S+\s+END_AUTO_INSERTED_SECTION\s+$from\s+$file\s+$tag/) {
	    $state = 0; 
	    print OUTFILE $sub;
	    print OUTFILE;
	    next;
	} else {
	    warn "Error (Line $line, begin on line $state): END token does not match BEGIN\n";
	    $state = 0;
	}
    }

    print OUTFILE if ($state == 0);
}
close(INFILE);

# Warn on mismatched tokens 
warn "Error (BEGIN on line $state): Mismatched auto insert sections\n" if 
($state != 0);

# Replace the files
if ($replace) {
    close(OUTFILE);
    unlink("$infile") || warn "Couldn't unlink $infile: $!\n";
    if (link("$infile.tmp",$infile)) {
	unlink("$infile.tmp") || warn "Couldn't unlink $infile.tmp: $!\n";
    } else {
	warn "Link failed: $!. Substituted file in $infile.tmp.\n";
    }
}

# We're done
exit 0;


	
    
	    
	
	



