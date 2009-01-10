dnl perl.m4 -- Probe for the details needed to embed Perl.
dnl $Id$
dnl
dnl Defines INN_ARG_PERL, which sets up the --with-perl command line argument
dnl and also sets various flags needed for embedded Perl if it is requested
dnl and ensures that Perl is the appropriate version.

dnl Check for a required version of Perl.
AC_DEFUN([_INN_PERL_VERSION],
[AC_CACHE_CHECK([for Perl version], [inn_cv_perl_version],
    [if $PERL -e 'require $1;' > /dev/null 2>&1 ; then
        inn_cv_perl_version=`$PERL -e 'print [$]@:>@'`
    else
        AC_MSG_ERROR([Perl $1 or greater is required])
    fi])])

dnl Check for Perl modules used by scripts shipped with INN.
AC_DEFUN([INN_PERL_MODULE],
[AC_MSG_CHECKING([for $1])
    if $PERL -e 'require $1;' > /dev/null 2>&1 ; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_WARN([$1 Perl module is required by $2])
        inn_perl_module_warning="$inn_perl_module_warning $1 (for $2)"
    fi])

dnl Check to see if Perl embedding was requested.  Regardless of whether it
dnl was or not, determine the path to Perl.  If it was requested, make sure
dnl that we have the right version and then set PERL_CPPFLAGS and PERL_LIBS as
dnl appropriate for embedded Perl.
AC_DEFUN([INN_ARG_PERL],
[AC_ARG_VAR([PERL], [Location of Perl interpretor])
AC_ARG_WITH([perl],
    [AC_HELP_STRING([--with-perl], [Embedded Perl script support [no]])],
    [case $withval in
    yes) DO_PERL=DO
         AC_DEFINE(DO_PERL, 1, [Define to compile in Perl script support.])
         ;;
    no)  DO_PERL=DONT ;;
    *)   AC_MSG_ERROR([invalid argument to --with-perl]) ;;
    esac],
    DO_PERL=DONT)

dnl Embedded Perl requires 5.004.  controlchan requires 5.004_03.  Other
dnl things may work with 5.003, but make 5.004_03 the minimum level; anyone
dnl should really have at least that these days.
dnl We also check for useful Perl modules.
INN_PATH_PROG_ENSURE([PERL], [perl])
_INN_PERL_VERSION(5.004_03)
INN_PERL_MODULE([Encode], [controlchan])
INN_PERL_MODULE([MIME::Parser], [controlchan])

dnl Libraries and flags for embedded Perl.  Some distributions of Linux have
dnl Perl linked with gdbm but don't normally have gdbm installed, so on that
dnl platform only strip -lgdbm out of the Perl libraries.  Leave it in on
dnl other platforms where it may be necessary (it isn't on Linux; Linux
dnl shared libraries can manage their own dependencies).  Strip -lc out, which
dnl is added on some platforms, is unnecessary, and breaks compiles with
dnl -pthread (which may be added by Python).
dnl
dnl If we aren't compiling with large-file support, strip out the large file
dnl flags from inn_perl_core_flags; otherwise, innd/cc.c and lib/qio.c
dnl disagree over the size of an off_t.  Since none of our calls into Perl
dnl use variables of type off_t, this should be harmless; in any event, it's
dnl going to be better than the innd/cc.c breakage.
dnl
dnl Also check to see if the complier supports -Wno-extra and, if so, add it
dnl to PERL_WARNING.  This has to be conditional since -Wno-extra is only
dnl supported in gcc 4.0 and later.
if test x"$DO_PERL" = xDO ; then
    AC_MSG_CHECKING([for Perl linkage])
    inn_perl_core_path=`$PERL -MConfig -e 'print $Config{archlibexp}'`
    inn_perl_core_flags=`$PERL -MExtUtils::Embed -e ccopts`
    inn_perl_core_libs=`$PERL -MExtUtils::Embed -e ldopts 2>&1 | tail -n 1`
    inn_perl_core_libs=" $inn_perl_core_libs "
    inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/ -lc / /'`
    for i in $LIBS ; do
        inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed "s/ $i / /"`
    done
    case $host in
    *-linux*)
        inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/ -lgdbm / /'`
        ;;
    *-cygwin*)
        inn_perl_libname=`$PERL -MConfig -e 'print $Config{libperl}'`
        inn_perl_libname=`echo "$inn_perl_libname" | sed 's/^lib//; s/\.a$//'`
        inn_perl_core_libs="${inn_perl_core_libs}-l$inn_perl_libname"
        ;;
    esac
    inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/^  *//'`
    inn_perl_core_libs=`echo "$inn_perl_core_libs" | sed 's/  *$//'`
    inn_perl_core_flags=" $inn_perl_core_flags "
    if test x"$inn_enable_largefiles" != xyes ; then
        for f in -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES ; do
            inn_perl_core_flags=`echo "$inn_perl_core_flags" | sed "s/ $f / /"`
        done
    fi
    inn_perl_core_flags=`echo "$inn_perl_core_flags" | sed 's/^  *//'`
    inn_perl_core_flags=`echo "$inn_perl_core_flags" | sed 's/  *$//'`
    PERL_CPPFLAGS="$inn_perl_core_flags"
    PERL_LIBS="$inn_perl_core_libs"
    AC_MSG_RESULT([$inn_perl_core_path])
    INN_PROG_CC_FLAG([-Wno-extra], [PERL_WARNINGS=-Wno-extra],
        [PERL_WARNINGS=''])
else
    PERL_CPPFLAGS=''
    PERL_LIBS=''
    PERL_WARNINGS=''
fi
AC_SUBST([PERL_CPPFLAGS])
AC_SUBST([PERL_LIBS])
AC_SUBST([PERL_WARNINGS])])
