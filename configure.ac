dnl Process this file with Autoconf to produce a configure script.
dnl $Id$
dnl
dnl Please try to follow GNU conventions and Autoconf manual conventions as
dnl much as possible in this file so that any macros we develop can be easily
dnl contributed to the macro archive and possibly rolled into future versions
dnl of Autoconf so that we can stop supporting them.  This means, for example,
dnl that code excerpts should probably follow the GNU coding standards rather
dnl than INN's.
dnl
dnl If a check is any way non-trivial, please package it up in a macro with
dnl AC_DEFUN and put that macro into a separate file under m4.  Please also
dnl put any long code blocks into a separate macro rather than in-line in the
dnl test macro; this will make quoting issues much easier.  See the existing
dnl tests for details on how to do this.
dnl
dnl Try to do as much with AC_DEFINE and as little with AC_SUBST as is
dnl reasonable; obviously, makefile things like library paths and so forth and
dnl paths to programs have to use AC_SUBST, but any compile-time parameters
dnl are easier to handle with AC_DEFINE.  (And AC_SUBST is slower.)
dnl
dnl And remember:  If you don't have any alternative available if your check
dnl for something fails, and there's no purpose served in aborting configure
dnl instead of the compile if what you're checking for is missing, don't
dnl bother checking for it.  Compile-time errors often produce a lot more
dnl useful information for someone debugging a problem than configure-time
dnl errors.

AC_INIT([INN], [2.6.0], [inn-workers@lists.isc.org],
    [inn], [https://www.isc.org/downloads/other/])
AC_PREREQ([2.64])
AC_REVISION([$Revision$])
AC_CONFIG_AUX_DIR([support])

dnl Lots of our macros are stored in separate files for ease of maintenance.
m4_include([m4/aux-libs.m4])
m4_include([m4/berkeleydb.m4])
m4_include([m4/cc-c-o.m4])
m4_include([m4/cc-flags.m4])
m4_include([m4/compress.m4])
m4_include([m4/inet-ntoa.m4])
m4_include([m4/iov-max.m4])
m4_include([m4/krb5-config.m4])
m4_include([m4/krb5.m4])
m4_include([m4/large-fpos.m4])
m4_include([m4/lib-depends.m4])
m4_include([m4/lib-helper.m4])
m4_include([m4/lib-pathname.m4])
m4_include([m4/libtool.m4])
m4_include([m4/lt~obsolete.m4])
m4_include([m4/ltoptions.m4])
m4_include([m4/ltsugar.m4])
m4_include([m4/ltversion.m4])
m4_include([m4/mmap.m4])
m4_include([m4/modes.m4])
m4_include([m4/openssl.m4])
m4_include([m4/pam-const.m4])
m4_include([m4/paths.m4])
m4_include([m4/perl.m4])
m4_include([m4/prog-ensure.m4])
m4_include([m4/python.m4])
m4_include([m4/sasl.m4])
m4_include([m4/sendfd.m4])
m4_include([m4/sendmail.m4])
m4_include([m4/snprintf.m4])
m4_include([m4/socket-unix.m4])
m4_include([m4/socket.m4])
m4_include([m4/syslog.m4])
m4_include([m4/users.m4])
m4_include([m4/vamacros.m4])
m4_include([m4/zlib.m4])

dnl By default, INN installs into its own independent tree.  Support for FHS
dnl is not yet implemented; once it is, this will need to become conditional
dnl on that support being disabled.
AC_PREFIX_DEFAULT([/usr/local/news])

dnl Make sure $prefix is set so that we can use it internally.
test x"$prefix" = xNONE && prefix="$ac_default_prefix"

dnl A few tests need to happen before any of the libtool tests.  We
dnl therefore lift them up to the top of the file.  (This is probably no
dnl longer needed now that we're running LT_INIT unconditionally, but
dnl we'll leave it alone until we switch to Automake.)
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror], [cposix])
INN_PROG_CC_C_O
AC_CANONICAL_HOST

dnl Configure libtool and set the variables that we used to use when we
dnl supported builds without libtool.
LT_INIT
EXTLIB='la'
EXTOBJ='lo'
LIBTOOL='$(top)/libtool'
LIBTOOLCC='$(top)/libtool --mode=compile'
LIBTOOLINST='$(top)/libtool --mode=install'
LIBTOOLLD='$(top)/libtool --mode=link'
CCOUTPUT='-c -o $@ $<'
AC_SUBST(EXTLIB)
AC_SUBST(EXTOBJ)
AC_SUBST(LIBTOOLCC)
AC_SUBST(LIBTOOLINST)
AC_SUBST(LIBTOOLLD)
AC_SUBST(CCOUTPUT)

dnl Command-line options.  Many of these macros just do the command-line
dnl component of a set of macros that have additional parts called later.
INN_ARG_PATHS
INN_ARG_SYSLOG
INN_ARG_USERS
INN_ARG_MODES
INN_ARG_COMPRESS
INN_ARG_SENDMAIL

dnl innbind by default only allows ports 119, 433 and 563 below 1024; if the user
dnl wants to use some other port as well, they must use this option.
AC_ARG_WITH([innd-port],
    [AS_HELP_STRING([--with-innd-port=PORT],
        [Additional low-numbered port for innbind])],
    [AC_DEFINE_UNQUOTED([INND_PORT], [$with_innd_port],
        [Additional permitted low-numbered port for innbind.])])

dnl Whether IPv6 support should be enabled (disabled by default).  Eventually
dnl this will be automatically detected and always turned on, but it's not
dnl quite mature enough yet.
AC_ARG_ENABLE([ipv6],
    [AS_HELP_STRING([--enable-ipv6], [Enable IPv6 support])],
    [if test "x$enableval" = xyes ; then
        AC_DEFINE([HAVE_INET6], [1], [Define to enable IPv6 support.])
    fi])

dnl Whether to use a different (less tested) history algorithm.  This will
dnl eventually be a runtime option rather than a compile-time option as soon
dnl as the #ifdefs can be untangled and put into the history API.  The output
dnl variable is needed for now to support news.daily.
DO_DBZ_TAGGED_HASH=DONT
inn_enable_tagged_hash=no
AC_ARG_ENABLE([tagged-hash],
    [AS_HELP_STRING([--enable-tagged-hash],
        [Use tagged hash table for history])],
    [if test "x$enableval" = xyes ; then
        DO_DBZ_TAGGED_HASH=DO
        inn_enable_tagged_hash=yes
        AC_DEFINE([DO_TAGGED_HASH], [1],
            [Define to use tagged hash for the history file.])
    fi])
AC_SUBST([DO_DBZ_TAGGED_HASH])

dnl Whether to enable the keyword generation code in innd.  Use of this code
dnl requires a regular expression library, which is checked for later on.
inn_enable_keywords=0
AC_ARG_ENABLE([keywords],
    [AS_HELP_STRING([--enable-keywords],
        [Automatic innd keyword generation support])],
    if test x"$enableval" = xyes ; then
        inn_enable_keywords=1
    fi)
AC_DEFINE_UNQUOTED([DO_KEYWORDS], [$inn_enable_keywords],
    [Define to 1 to compile in support for keyword generation code.])

dnl Whether to use the OS flags to enable large file support.  Ideally this
dnl should just always be turned on if possible and the various parts of INN
dnl that read off_t's from disk should adjust somehow to the size, but INN
dnl isn't there yet.  Currently tagged hash doesn't work with large file
dnl support due to assumptions about the size of off_t.
inn_enable_largefiles=no
AC_ARG_ENABLE([largefiles],
    [AS_HELP_STRING([--enable-largefiles],
        [Support for files larger than 2GB @<:@default=no@:>@])],
    if test x"$enableval" = xyes ; then
        inn_enable_largefiles=yes
    fi)
if test x"$inn_enable_tagged_hash" = xyes \
   && test x"$inn_enable_largefiles" = xyes ; then
    AC_MSG_ERROR([--enable-tagged-hash conflicts with --enable-largefiles.])
fi

dnl Checks for embedded interpreters.
INN_ARG_PERL
INN_ARG_PYTHON

dnl Set some configuration file defaults from the machine hostname.
HOSTNAME=`hostname 2> /dev/null || uname -n`
AC_SUBST(HOSTNAME)

dnl Checks for programs.
AC_PROG_GCC_TRADITIONAL
AC_PROG_LEX
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_RANLIB
AC_PROG_YACC

dnl On MacOS X Server, -traditional-cpp is needed for gcc for compiling as
dnl well as preprocessing according to Miro Jurisic <meeroh@meeroh.org>.
case "$CPP" in
*-traditional-cpp*)
    CFLAGS="-traditional-cpp $CFLAGS"
    ;;
esac

case "$host" in

dnl When building on Linux, we need to compile with _GNU_SOURCE to get the
dnl right definitions of pread and pwrite and to get some other functions
dnl (like asprintf) that we use.
*linux*)
    AC_DEFINE([_GNU_SOURCE], [1],
        [Define if compiling on Linux to get prototypes for some functions.])
    ;;

dnl HP-UX's native compiler needs a special flag to turn on ANSI, and needs
dnl -g on link as well as compile for debugging to work.
*hpux*)
    if test x"$GCC" != xyes ; then
        CFLAGS="$CFLAGS -Ae"
        case "$CFLAGS" in
        *-g*) LDFLAGS="$LDFLAGS -g" ;;
        esac
    fi
    ;;

dnl OSX needs -multiply_defined suppress.
*darwin*)
    LDFLAGS="$LDFLAGS -multiply_defined suppress"
    ;;

dnl From Boyd Gerber <gerberb@zenez.com>, needed in some cases to compile
dnl the bison-generated parser for innfeed.conf.
*UnixWare*|*unixware*|*-sco3*)
    if test x"$GCC" != xyes ; then
        CFLAGS="$CFLAGS -Kalloca"
    fi
esac
  
dnl Checks for pathnames.  If AC_PROG_* does not manage to find a working
dnl program, INN_PATH_PROG_ENSURE will output an error.
AC_ARG_VAR([AWK], [Location of awk])
AC_ARG_VAR([EGREP], [Location of egrep])
AC_ARG_VAR([SED], [Location of sed])
AC_ARG_VAR([SORT], [Location of sort])
AC_ARG_VAR([SSLBIN], [Location of openssl])
AC_ARG_VAR([UUSTAT], [Location of uustat])
AC_ARG_VAR([UUX], [Location of uux])
AC_PROG_AWK
AC_PROG_EGREP
AC_PROG_SED
INN_PATH_PROG_ENSURE([AWK], [awk])
INN_PATH_PROG_ENSURE([EGREP], [egrep])
INN_PATH_PROG_ENSURE([SED], [sed])
INN_PATH_PROG_ENSURE([SORT], [sort])
AC_PATH_PROGS([SSLBIN], [openssl], [openssl])
AC_PATH_PROGS([UUSTAT], [uustat])
AC_PATH_PROGS([UUX], [uux], [uux])
INN_PATH_COMPRESS
INN_PATH_SENDMAIL

dnl Look for PGP 5.0's pgpv, then pgp, then pgpgpg (not sure why anyone would
dnl have pgpgpg and not gpgv, but it doesn't hurt).  Separately look for
dnl GnuPG (which we prefer).
DO_PGPVERIFY=true
AC_ARG_VAR([GPGV], [Location of GnuPG gpgv program])
AC_ARG_VAR([PGP], [Location of PGP verification program])
AC_PATH_PROGS([GPGV], [gpgv2 gpgv])
AC_PATH_PROGS([PGP], [pgpv pgp pgpgpg])
if test -z "$PGP" && test -z "$GPGV" ; then
    DO_PGPVERIFY=false
fi
AC_SUBST([DO_PGPVERIFY])

dnl Look for a program that takes an ftp URL as a command line argument and
dnl retrieves the file to the current directory.  Shame we can't also use
dnl lynx -source; it only writes to stdout.  ncftp as of version 3 doesn't
dnl support this any more (it comes with ncftpget instead), but if someone
dnl has ncftp and not ncftpget they have an earlier version.
AC_PATH_PROGS([PATH_GETFTP], [wget ncftpget ncftp], [$bindir/simpleftp])

dnl Checks for libraries.  Use AC_SEARCH_LIBS where possible to avoid
dnl adding libraries when the function is found in libc, and use
dnl INN_SEARCH_AUX_LIBS for libraries that are only used by particular
dnl subsections of INN.

dnl Check for setproctitle in libc first, then libutil if not found there.
dnl We have a replacement function if we can't find it, and then we also need
dnl to check for pstat.
AC_SEARCH_LIBS([setproctitle], [util],
    [AC_DEFINE([HAVE_SETPROCTITLE], [1],
        [Define if you have the setproctitle function.])],
    [AC_LIBOBJ([setproctitle])
     AC_CHECK_FUNCS([pstat])])

dnl The rat's nest of networking libraries.  The common cases are not to
dnl need any extra libraries, or to need -lsocket -lnsl.  We need to avoid
dnl linking with libnsl unless we need it, though, since on some OSes where
dnl it isn't necessary it will totally break networking.  Unisys also
dnl includes gethostbyname in libsocket but needs libnsl for socket().
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], [],
    [AC_CHECK_LIB([nsl], [socket], [LIBS="$LIBS -lsocket -lnsl"], [],
        [-lsocket])])

dnl Check for inet_aton.  We have our own, but on Solaris the version in
dnl libresolv is more lenient in ways that Solaris's internal DNS resolution
dnl code requires, so if we use our own *and* link with libresolv (which may
dnl be forced by Perl) DNS resolution fails.
AC_SEARCH_LIBS([inet_aton], [resolv])

dnl Search for various additional libraries used by portions of INN.
INN_SEARCH_AUX_LIBS([crypt], [crypt], [CRYPT_LIBS])
INN_SEARCH_AUX_LIBS([getspnam], [shadow], [SHADOW_LIBS])

dnl IRIX has a PAM library with the right symbols but no header files suitable
dnl for use with it, so we have to check the header files first and then only
dnl if one is found do we check for the library.
inn_check_pam=1
AC_CHECK_HEADERS([security/pam_appl.h], [],
    [AC_CHECK_HEADERS([pam/pam_appl.h], [], [inn_check_pam=0])])
if test x"$inn_check_pam" = x1; then
    INN_SEARCH_AUX_LIBS([pam_start], [pam], [PAM_LIBS],
        [AC_DEFINE([HAVE_PAM], [1], [Define if you have PAM.])])
fi
INN_HEADER_PAM_CONST

dnl If keyword generation support was requested, check for the appropriate
dnl libraries.
if test x"$inn_enable_keywords" = x1 ; then
    INN_SEARCH_AUX_LIBS([regexec], [regex], [REGEX_LIBS], [],
        [AC_MSG_ERROR([no usable regular expression library found])])
fi

dnl Handle optional libraries and probing for their locations and component
dnl libraries if needed.
INN_LIB_BERKELEYDB
INN_LIB_ZLIB
INN_LIB_KRB5_OPTIONAL
INN_LIB_OPENSSL_OPTIONAL
INN_LIB_SASL_OPTIONAL

dnl If Kerberos is found, define KRB5_AUTH to auth_krb5 so as to build
dnl that program.
AS_IF([test x"$KRB5_LIBS" != x],
    [KRB5_AUTH=auth_krb5])
AC_SUBST([KRB5_AUTH])

dnl The dbm libraries are a special case.  If we're building with Berkeley DB,
dnl just use the ndbm support provided by it.
if test x"$DB_LIBS" != x ; then
    INN_LIB_BERKELEYDB_NDBM
fi
if test x"$inn_cv_lib_berkeleydb_ndbm_works" != xyes ; then
    INN_SEARCH_AUX_LIBS([dbm_open], [ndbm dbm gdbm "gdbm_compat -lgdbm"], [DBM_LIBS],
        [AC_DEFINE([HAVE_DBM], [1], [Define if you have a dbm library.])])
    DBM_CPPFLAGS=
else
    DBM_CPPFLAGS="$DB_CPPFLAGS"
    DBM_LIBS="$DB_LDFLAGS $DB_LIBS"
    AC_SUBST([DBM_LIBS])
fi
AC_SUBST([DBM_CPPFLAGS])

dnl If configuring with large file support, determine the right flags to
dnl use based on the platform.
if test x"$inn_enable_largefiles" = xyes ; then
    AC_SYS_LARGEFILE
    AC_DEFINE([DO_LARGEFILES], [1], [Define to use large files.])
fi

dnl Special checks for header files.
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_HEADER_SYS_WAIT

dnl Generic checks for header files.
AC_CHECK_HEADERS([crypt.h inttypes.h limits.h stddef.h \
                  stdint.h string.h sys/bitypes.h sys/filio.h sys/loadavg.h \
                  sys/select.h sys/time.h unistd.h])

dnl Some Linux systems have db1/ndbm.h instead of ndbm.h.  Others have
dnl gdbm-ndbm.h.
AC_CHECK_HEADERS([ndbm.h db1/ndbm.h gdbm-ndbm.h], [break])

dnl Check for whether various symbols are declared.
AC_CHECK_DECLS([pread, pwrite, snprintf, setproctitle, strlcat,
                strlcpy, vsnprintf])
AC_CHECK_DECLS([h_errno], [], [], [#include <netdb.h>])
AC_CHECK_DECLS([inet_aton, inet_ntoa], [], [],
    [#include <sys/types.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>])
AC_CHECK_DECLS([altzone], [], [], [#include <time.h>])

dnl Checks for compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
INN_C_C99_VAMACROS
INN_C_GNU_VAMACROS

dnl Checks for structures.
AC_STRUCT_TIMEZONE
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_CHECK_MEMBERS([struct tm.tm_gmtoff])
AC_CHECK_MEMBERS([struct sockaddr.sa_len], [], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])

dnl Checks for types.
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_LONG_LONG_INT
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_FUNC_GETGROUPS
AC_TYPE_UID_T
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_TYPES([sig_atomic_t], [], [],
    [#include <sys/types.h>
     #include <signal.h>])
AC_CHECK_TYPES([socklen_t], [], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])
AC_CHECK_TYPES([union semun], [], [],
    [#include <sys/types.h>
     #include <sys/ipc.h>
     #include <sys/sem.h>])

dnl See if struct sockaddr_storage is available, and if so, how its members
dnl are named.
AC_CHECK_TYPES([struct sockaddr_storage],
    [AC_CHECK_MEMBERS([struct sockaddr_storage.ss_family], [], [],
        [#include <sys/types.h>
         #include <sys/socket.h>])], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])

dnl Checks for macros.
INN_MACRO_IN6_ARE_ADDR_EQUAL
INN_MACRO_IOV_MAX
INN_MACRO_SA_LEN
INN_MACRO_SUN_LEN

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
INN_FUNC_GETADDRINFO_ADDRCONFIG
INN_FUNC_INET_NTOA
INN_FUNC_SNPRINTF

dnl Check for various other functions.
AC_CHECK_FUNCS(atexit getloadavg getrusage getspnam setbuffer sigaction \
               setgroups setrlimit setsid socketpair strncasecmp strtoul \
               sysconf)

dnl Find a way to get the file descriptor limit.
AC_CHECK_FUNCS([getrlimit getdtablesize ulimit], [break])

dnl Probe for the appropriate stat functions.
AC_CHECK_FUNCS([statvfs], [],
    [AC_CHECK_FUNCS([statfs])
     AC_CHECK_HEADERS([sys/vfs.h sys/param.h])
     AC_CHECK_HEADERS(sys/mount.h, [], [],
         [#ifdef HAVE_SYS_PARAM_H
          # include <sys/param.h>
          #endif
         ])
    ])

dnl If we can't find any of the following, we have replacements for them.
AC_REPLACE_FUNCS(asprintf getaddrinfo getnameinfo getpagesize \
                 inet_aton inet_ntop mkstemp pread pwrite seteuid strcasecmp \
                 strlcat strlcpy strspn setenv symlink)

dnl Probe for fseeko and ftello, which take off_t instead of int.
if test x"$inn_enable_largefiles" = xyes ; then
    AC_FUNC_FSEEKO
    if test x"$ac_cv_sys_largefile_source" = xunknown ; then
        INN_TYPE_FPOS_T_LARGE
        AC_CHECK_DECLS([fseeko, ftello])
        AC_LIBOBJ([fseeko])
        AC_LIBOBJ([ftello])
    fi
else
    AC_CHECK_DECLS([fseeko])
fi

dnl Probe for mmap properties.
INN_FUNC_MMAP(
    [AC_CHECK_FUNCS([madvise])
     INN_FUNC_MMAP_SEES_WRITES
     INN_FUNC_MMAP_NEEDS_MSYNC
     INN_FUNC_MSYNC_ARGS])

dnl Probes for system characteristics.
INN_SYS_STREAMS_SENDFD
INN_SYS_UNIX_SOCKETS
INN_LOG_FACILITY

dnl Clean up our LIBS, just for grins.
LIBS=`echo "$LIBS" | sed 's/^  *//' | sed 's/   */ /g' | sed 's/  *$//'`

dnl Configure our output files.  Try to keep this as limited as possible,
dnl since it directly affects how long it takes configure to complete.
AC_CONFIG_FILES([Makefile.global include/inn/paths.h])
AC_CONFIG_FILES([scripts/innshellvars scripts/innshellvars.tcl])
AC_CONFIG_FILES([perl/INN/Config.pm scripts/innshellvars.pl])
AC_CONFIG_FILES([support/fixconfig], [chmod +x support/fixconfig])
AC_CONFIG_FILES([support/fixscript], [chmod +x support/fixscript])

dnl Include some additional files at the beginning of config.h.
AH_TOP([#include "inn/defines.h"
        #include "inn/options.h"])

dnl Most configuration information goes here.
AC_CONFIG_HEADER([include/config.h])

dnl Generate the configure output.
AC_OUTPUT

dnl Print out some additional information on what to check.
cat <<EOM

Please check the following files before running make, to ensure that 
everything was set correctly.

        Makefile.global
        include/config.h
        include/inn/options.h
        include/inn/paths.h
        innfeed/innfeed.h

EOM

dnl Write a warning for missing Perl modules at the end of the output.
dnl $inn_perl_module_warning already has a leading space.
if test x"$inn_perl_module_warning" != x ; then
    echo "Missing Perl modules: $inn_perl_module_warning."
    echo "You should install them for these programs to work fine."
fi

dnl Finally, double-check the configured temporary directory.  Some people
dnl point this at the system temporary directories or at other world-writeable
dnl directories, which can be a local security hole.
if $PERL -e "exit((stat('$PATH_TMP'))[[2]] & 2)" > /dev/null ; then
    :
else
    cat <<EOM

WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  The temporary directory you have configured is world-writeable.  It is
  currently set to $tmpdir.

  This directory is used by INN for temporary files and should only be
  writeable by the news user.  INN does not go to great lengths to prevent
  symlink attacks and the like because it assumes this directory is not
  world-writeable.  By configuring INN in this fashion, you may be
  introducing a locally exploitable security hole.

  It is STRONGLY recommended that you use a temporary directory reserved for
  INN's exclusive use, one which is not world-writeable.  You can do this
  either with --with-tmp-dir or by setting --prefix to something other than
  /usr or /.

EOM
fi
