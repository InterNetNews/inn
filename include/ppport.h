
#ifndef PPPORT_H
#define PPPORT_H 1

/* Perl/Pollution/Portability Version 1.0003 */

/* Copyright (C) 1999, Kenneth Albanowski. This code may be used and
   distributed under the same license as any version of Perl. */
   
/* For the latest version of this code, please contact the author at
   <kjahds@kjahds.com>, or check with the Perl maintainers. */
   
/* If you needed to customize this file for your project, please mention
   your changes. */

/*
   Modified for Perl 5.6.0 by Russ Allbery (use PERL_VERSION instead of
   PERL_PATCHLEVEL).
*/


/*
   In order for a Perl extension module to be as portable as possible
   across differing versions of Perl itself, certain steps need to be taken.
   Including this header is the first major one, then using dTHR is all the
   appropriate places and using a PL_ prefix to refer to global Perl
   variables is the second.
*/


/* If you use one of a few functions that were not present in earlier
   versions of Perl, please add a define before the inclusion of ppport.h
   for a static include, or use the GLOBAL request in a single module to
   produce a global definition that can be referenced from the other
   modules.
   
   Function:            Static define:           Extern define:
   newCONSTSUB()        NEED_newCONSTSUB         NEED_newCONSTSUB_GLOBAL

*/
 

/* To verify whether ppport.h is needed for your module, and whether any
   special defines should be used, ppport.h can be run through Perl to check
   your source code. Simply say:
   
   	perl -x ppport.h *.c *.h *.xs foo/perl.c [etc]
   
   The result will be a list of patches suggesting changes that should at
   least be acceptable, if not necessarily the most efficient solution, or a
   fix for all possible problems. It won't catch where dTHR is needed, and
   doesn't attempt to account for global macro or function definitions,
   nested includes, typemaps, etc.
   
   In order to test for the need of dTHR, please try your module under a
   recent version of Perl that has threading compiled-in.
 
*/ 


/*
#!/usr/bin/perl
@ARGV = ("*.xs") if !@ARGV;
%badmacros = %funcs = %macros = ();
foreach (<DATA>) {
	$funcs{$1} = 1 if /Provide:\s+(\S+)/;
	$macros{$1} = 1 if /^#\s*define\s+([a-zA-Z0-9_]+)/;
	$badmacros{$2}=$1 if /^#\s*define\s+(PL_\S+)\s+(\S+)/;
}
foreach $filename (map(glob($_),@ARGV)) {
	unless (open(IN, "<$filename")) {
		warn "Unable to read from $file: $!\n";
		next;
	}
	print "Scanning $filename...\n";
	$c = ""; while (<IN>) { $c .= $_; } close(IN);
	$need_include = 0; %add_func = (); $changes = 0;
	$has_include = ($c =~ /#.*include.*ppport/m);

	foreach $func (keys %funcs) {
		if ($c =~ /#.*define.*\bNEED_$func(_GLOBAL)?\b/m) {
			if ($c !~ /\b$func\b/m) {
				print "If $func isn't needed, you don't need to request it.\n" if
				$changes += ($c =~ s/^.*#.*define.*\bNEED_$func\b.*\n//m);
			} else {
				print "Uses $func\n";
				$need_include = 1;
			}
		} else {
			if ($c =~ /\b$func\b/m) {
				$add_func{$func} =1 ;
				print "Uses $func\n";
				$need_include = 1;
			}
		}
	}

	if (not $need_include) {
		foreach $macro (keys %macros) {
			if ($c =~ /\b$macro\b/m) {
				print "Uses $macro\n";
				$need_include = 1;
			}
		}
	}

	foreach $badmacro (keys %badmacros) {
		if ($c =~ /\b$badmacro\b/m) {
			$changes += ($c =~ s/\b$badmacro\b/$badmacros{$badmacro}/gm);
			print "Uses $badmacros{$badmacro} (instead of $badmacro)\n";
			$need_include = 1;
		}
	}
	
	if (scalar(keys %add_func) or $need_include != $has_include) {
		if (!$has_include) {
			$inc = join('',map("#define NEED_$_\n", sort keys %add_func)).
			       "#include \"ppport.h\"\n";
			$c = "$inc$c" unless $c =~ s/#.*include.*XSUB.*\n/$&$inc/m;
		} elsif (keys %add_func) {
			$inc = join('',map("#define NEED_$_\n", sort keys %add_func));
			$c = "$inc$c" unless $c =~ s/^.*#.*include.*ppport.*$/$inc$&/m;
		}
		if (!$need_include) {
			print "Doesn't seem to need ppport.h.\n";
			$c =~ s/^.*#.*include.*ppport.*\n//m;
		}
		$changes++;
	}
	
	if ($changes) {
		open(OUT,">/tmp/ppport.h.$$");
		print OUT $c;
		close(OUT);
		open(DIFF, "diff -u $filename /tmp/ppport.h.$$|");
		while (<DIFF>) { s!/tmp/ppport\.h\.$$!$filename.patched!; print STDOUT; }
		close(DIFF);
		unlink("/tmp/ppport.h.$$");
	} else {
		print "Looks OK\n";
	}
}
__DATA__
*/


#if !defined(PERL_VERSION) && !defined(PERL_PATCHLEVEL)
#	ifndef __PATCHLEVEL_H_INCLUDED__
#		include <patchlevel.h>
#	endif
#endif
#ifndef PERL_VERSION
#       define PERL_REVISION (5)
#       ifdef PERL_PATCHLEVEL
#               define PERL_VERSION    PERL_PATCHLEVEL
#       else
#               define PERL_VERSION    PATCHLEVEL
#               define PERL_SUBVERSION SUBVERSION
#       endif
#endif

#ifndef ERRSV
#	define ERRSV perl_get_sv("@",false)
#endif

#if (PERL_REVISION == 5) && ((PERL_VERSION < 4) || ((PERL_VERSION == 4) && (PERL_SUBVERSION <= 4)))
#	define PL_sv_undef	sv_undef
#	define PL_sv_yes	sv_yes
#	define PL_sv_no		sv_no
#	define PL_na		na
#	define PL_stdingv	stdingv
#	define PL_hints		hints
#	define PL_curcop	curcop
#	define PL_curstash	curstash
#	define PL_copline	copline
#endif

#if (PERL_REVISION == 5) && (PERL_VERSION < 5)
#	undef dTHR
#	ifdef WIN32
#		define dTHR extern int Perl___notused
#	else
#		define dTHR extern int errno
#	endif
#endif

#ifndef boolSV
#	define boolSV(b) ((b) ? &PL_sv_yes : &PL_sv_no)
#endif

/* Perl tries to export a bunch of its own functions.  Mutter. */
#undef die
#undef list
#undef warn

#endif /* !PPPORT_H */
