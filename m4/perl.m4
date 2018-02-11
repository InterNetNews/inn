dnl Probe for Perl properties and, optionally, flags for embedding Perl.
dnl $Id$
dnl
dnl Provides the following macros:
dnl
dnl INN_PROG_PERL
dnl     Checks for a specific Perl version and sets the PERL environment
dnl     variable to the full path, or aborts the configure run if the version
dnl     of Perl is not new enough or couldn't be found.
dnl
dnl INN_PERL_CHECK_MODULE
dnl     Checks for the existence of a Perl module and runs provided code based
dnl     on whether or not it was found.
dnl
dnl INN_LIB_PERL
dnl     Determines the flags required for embedding Perl and sets
dnl     PERL_CPPFLAGS and PERL_LIBS.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2016 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2006, 2009, 2011 Internet Systems Consortium, Inc. ("ISC")
dnl Copyright 1998-2003 The Internet Software Consortium
dnl
dnl Permission to use, copy, modify, and distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
dnl REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
dnl SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
dnl IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl SPDX-License-Identifier: ISC

dnl Check for the path to Perl and ensure it meets our minimum version
dnl requirement (given as the argument).  Honor the $PERL environment
dnl variable, if set.
AC_DEFUN([INN_PROG_PERL],
[AC_ARG_VAR([PERL], [Location of Perl interpreter])
 AS_IF([test x"$PERL" != x],
    [AS_IF([! test -x "$PERL"],
        [AC_MSG_ERROR([Perl binary $PERL not found])])
     AS_IF([! "$PERL" -e 'use $1' >/dev/null 2>&1],
        [AC_MSG_ERROR([Perl $1 or greater is required])])],
    [AC_CACHE_CHECK([for Perl version $1 or later], [ac_cv_path_PERL],
        [AC_PATH_PROGS_FEATURE_CHECK([PERL], [perl],
            [AS_IF(["$ac_path_PERL" -e 'require $1' >/dev/null 2>&1],
                [ac_cv_path_PERL="$ac_path_PERL"
                 ac_path_PERL_found=:])])])
     AS_IF([test x"$ac_cv_path_PERL" = x],
         [AC_MSG_ERROR([Perl $1 or greater is required])])
     PERL="$ac_cv_path_PERL"])])

dnl Check whether a given Perl module can be loaded.  Runs the second argument
dnl if it can, and the third argument if it cannot.
AC_DEFUN([INN_PERL_CHECK_MODULE],
[AS_LITERAL_IF([$1], [], [m4_fatal([$0: requires literal arguments])])dnl
 AS_VAR_PUSHDEF([ac_Module], [inn_cv_perl_module_$1])dnl
 AC_CACHE_CHECK([for Perl module $1], [ac_Module],
    [AS_IF(["$PERL" -e 'use $1' >/dev/null 2>&1],
        [AS_VAR_SET([ac_Module], [yes])],
        [AS_VAR_SET([ac_Module], [no])])])
 AS_VAR_IF([ac_Module], [yes], [$2], [$3])
 AS_VAR_POPDEF([ac_Module])])

dnl Determine the flags used for embedding Perl.
dnl
dnl Some distributions of Linux have Perl linked with gdbm but don't normally
dnl have gdbm installed, so on that platform only strip -lgdbm out of the Perl
dnl libraries.  Leave it in on other platforms where it may be necessary (it
dnl isn't on Linux; Linux shared libraries can manage their own dependencies).
dnl Strip -lc out, which is added on some platforms, is unnecessary, and
dnl breaks compiles with -pthread (which may be added by Python).
AC_DEFUN([INN_LIB_PERL],
[AC_REQUIRE([AC_CANONICAL_HOST])
 AC_SUBST([PERL_CPPFLAGS])
 AC_SUBST([PERL_LIBS])
 AC_MSG_CHECKING([for flags to link with Perl])
 inn_perl_core_path=`"$PERL" -MConfig -e 'print $Config{archlibexp}'`
 inn_perl_core_flags=`"$PERL" -MExtUtils::Embed -e ccopts`
 inn_perl_core_libs=`"$PERL" -MExtUtils::Embed -e ldopts 2>&1 | tail -n 1`
 inn_perl_core_libs=" $inn_perl_core_libs "
 inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/ -lc / /'`
 AS_CASE([$host],
    [*-linux*],
        [inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/ -lgdbm / /'`],
    [*-cygwin*],
        [inn_perl_libname=`"$PERL" -MConfig -e 'print $Config{libperl}'`
         inn_perl_libname=`echo "$inn_perl_libname" | sed 's/^lib//; s/\.a$//'`
         inn_perl_core_libs="${inn_perl_core_libs}-l$inn_perl_libname"])
 inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/^  *//'`
 inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/  *$//'`
 PERL_CPPFLAGS="$inn_perl_core_flags"
 PERL_LIBS="$inn_perl_core_libs"
 AC_MSG_RESULT([$PERL_LIBS])])
