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

AC_INIT([INN], [2.5.0])
AC_PREREQ(2.59)
AC_REVISION($Revision$)
AC_CONFIG_AUX_DIR([support])

dnl Lots of our macros are stored in separate files for ease of maintenance.
m4_include([m4/aux-libs.m4])
m4_include([m4/berkeleydb.m4])
m4_include([m4/cc-c-o.m4])
m4_include([m4/compress.m4])
m4_include([m4/inet-ntoa.m4])
m4_include([m4/int32.m4])
m4_include([m4/iov-max.m4])
m4_include([m4/libtool.m4])
m4_include([m4/krb5.m4])
m4_include([m4/large-fpos.m4])
m4_include([m4/mmap.m4])
m4_include([m4/modes.m4])
m4_include([m4/openssl.m4])
m4_include([m4/paths.m4])
m4_include([m4/perl.m4])
m4_include([m4/prog-ensure.m4])
m4_include([m4/python.m4])
m4_include([m4/sasl.m4])
m4_include([m4/sendmail.m4])
m4_include([m4/sendfd.m4])
m4_include([m4/snprintf.m4])
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

dnl A few tests need to happen before any of the libtool tests in order to
dnl avoid error messages.  We therefore lift them up to the top of the file.
AC_PROG_CC
AC_AIX
AC_ISC_POSIX
INN_PROG_CC_C_O
AC_CANONICAL_HOST

dnl Check to see if the user wants to use libtool.  We only invoke the libtool
dnl setup macros if they do.  Keep this call together with the libtool setup
dnl so that the arguments to configure will be together in configure --help.
inn_use_libtool=no
AC_ARG_ENABLE(libtool,
    [  --enable-libtool        Use libtool for lib generation [default=no]],
    if test "$enableval" = yes ; then
        inn_use_libtool=yes
    fi)
if test x"$inn_use_libtool" = xyes ; then
    AC_PROG_LIBTOOL
    EXTLIB='la'
    EXTOBJ='lo'
    LIBTOOL='$(top)/libtool'
    LIBTOOLCC='$(top)/libtool --mode=compile'
    LIBTOOLLD='$(top)/libtool --mode=link'
    CCOUTPUT='-c -o $@ $<'
else
    EXTLIB='a'
    EXTOBJ='o'
    LIBTOOL=''
    LIBTOOLCC=''
    LIBTOOLLD=''
    if test x"$compiler_c_o" = xyes ; then
        CCOUTPUT='-c -o $@ $<'
    else
        CCOUTPUT='-c $< && if test x"$(@F)" != x"$@" ; then mv $(@F) $@ ; fi'
    fi
    AC_SUBST(LIBTOOL)
fi
AC_SUBST(EXTLIB)
AC_SUBST(EXTOBJ)
AC_SUBST(LIBTOOLCC)
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

dnl innbind by default only allows ports 119 and 433 below 1024; if the user
dnl wants to use some other port as well, they must use this option.
AC_ARG_WITH([innd-port],
    [AC_HELP_STRING([--with-innd-port=PORT],
        [Additional low-numbered port for innbind])],
    [AC_DEFINE_UNQUOTED([INND_PORT], [$with_innd_port],
        [Additional permitted low-numbered port for innbind.])])

dnl Whether IPv6 support should be enabled (disabled by default).  Eventually
dnl this will be automatically detected and always turned on, but it's not
dnl quite mature enough yet.
AC_ARG_ENABLE([ipv6],
    [AC_HELP_STRING([--enable-ipv6], [Enable IPv6 support])],
    [if test "x$enableval" = xyes ; then
        AC_DEFINE([HAVE_INET6], 1, [Define to enable IPv6 support.])
    fi])

dnl Whether to use a different (less tested) history algorithm.  This will
dnl eventually be a runtime option rather than a compile-time option as soon
dnl as the #ifdefs can be untangled and put into the history API.  The output
dnl variable is needed for now to support news.daily.
DO_DBZ_TAGGED_HASH=DONT
inn_enable_tagged_hash=no
AC_ARG_ENABLE([tagged-hash],
    [AC_HELP_STRING([--enable-tagged-hash],
        [Use tagged hash table for history])],
    [if test "x$enableval" = xyes ; then
        DO_DBZ_TAGGED_HASH=DO
        inn_enable_tagged_hash=yes
        AC_DEFINE([DO_TAGGED_HASH], 1,
            [Define to use tagged hash for the history file.])
    fi])
AC_SUBST([DO_DBZ_TAGGED_HASH])

dnl Whether to enable the keyword generation code in innd.  Use of this code
dnl requires a regular expression library, which is checked for later on.
inn_enable_keywords=0
AC_ARG_ENABLE([keywords],
    [AC_HELP_STRING([--enable-keywords],
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
    [AC_HELP_STRING([--enable-largefiles],
        [Support for files larger than 2GB [default=no]])],
    if test x"$enableval" = xyes ; then
        inn_enable_largefiles=yes
    fi)
if test x"$inn_enable_tagged_hash" = xyes \
   && test x"$inn_enable_largefiles" = xyes ; then
    AC_MSG_ERROR([--enable-tagged-hash conflicts with --enable-largefiles.])
fi

dnl Checks for embedded interpretors.
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

dnl When building with largefile support on Linux, we need to compile with
dnl _GNU_SOURCE to get the right definitions of pread and pwrite.  (Well,
dnl _XOPEN_SOURCE would also work, but then stuff that we use disappears.)
*linux*)
    if test x"$inn_enable_largefiles" = xyes ; then
        AC_DEFINE([_GNU_SOURCE], 1,
            [Define if compiling with large file support on Linux to get the
             correct prototypes for pread and pwrite.])
    fi
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
  
dnl Checks for pathnames.
AC_ARG_VAR([AWK], [Location of awk])
AC_ARG_VAR([EGREP], [Location of egrep])
AC_ARG_VAR([SED], [Location of sed])
AC_ARG_VAR([SORT], [Location of sort])
INN_PATH_PROG_ENSURE([AWK], [awk])
INN_PATH_PROG_ENSURE([EGREP], [egrep])
INN_PATH_PROG_ENSURE([SED], [sed])
INN_PATH_PROG_ENSURE([SORT], [sort])
INN_PATH_COMPRESS
INN_PATH_SENDMAIL

dnl Look for PGP 5.0's pgpv, then pgp, then pgpgpg (not sure why anyone would
dnl have pgpgpg and not gpgv, but it doesn't hurt).  Separately look for
dnl GnuPG (which we prefer).
DO_PGPVERIFY=true
AC_ARG_VAR([GPGV], [Location of GnuPG gpgv program])
AC_ARG_VAR([PGP], [Location of PGP verification program])
AC_PATH_PROGS([GPGV], [gpgv])
AC_PATH_PROGS([PGP], [pgpv pgp pgpgpg])
if test -z "$PATH_PGP" && test -z "$PATH_GPGV" ; then
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
    [AC_DEFINE([HAVE_SETPROCTITLE], 1,
        [Define if you have the setproctitle function.])],
    [AC_LIBOBJ([setproctitle])
     AC_CHECK_FUNCS([pstat])])

dnl The rat's nest of networking libraries.  The common cases are not to
dnl need any extra libraries, or to need -lsocket -lnsl.  We need to avoid
dnl linking with libnsl unless we need it, though, since on some OSes where
dnl it isn't necessary it will totally break networking.  Unisys also
dnl includes gethostbyname in libsocket but needs libnsl for socket().
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], ,
    [AC_CHECK_LIB([nsl], [socket], LIBS="$LIBS -lsocket -lnsl", , -lsocket)])

dnl Check for inet_aton.  We have our own, but on Solaris the version in
dnl libresolv is more lenient in ways that Solaris's internal DNS resolution
dnl code requires, so if we use our own *and* link with libresolv (which may
dnl be forced by Perl) DNS resolution fails.
AC_SEARCH_LIBS([inet_aton], [resolv])

dnl Search for various additional libraries used by portions of INN.
INN_SEARCH_AUX_LIBS([crypt], [crypt], [CRYPT_LIBS])
INN_SEARCH_AUX_LIBS([getspnam], [shadow], [SHADOW_LIBS])
INN_SEARCH_AUX_LIBS([dbm_open], [ndbm dbm], [DBM_LIBS],
    [AC_DEFINE([HAVE_DBM], 1, [Define if you have a dbm library.])])

dnl IRIX has a PAM library with the right symbols but no header files suitable
dnl for use with it, so we have to check the header files first and then only
dnl if one is found do we check for the library.
inn_check_pam=1
AC_CHECK_HEADERS([pam/pam_appl.h], ,
    [AC_CHECK_HEADER([security/pam_appl.h], , [inn_check_pam=0])])
if test x"$inn_check_pam" = x1; then
    INN_SEARCH_AUX_LIBS([pam_start], [pam], [PAM_LIBS],
        [AC_DEFINE([HAVE_PAM], 1, [Define if you have PAM.])])
fi

dnl If keyword generation support was requested, check for the appropriate
dnl libraries.
if test x"$inn_enable_keywords" = x1 ; then
    INN_SEARCH_AUX_LIBS([regexec], [regex], [REGEX_LIBS], ,
        [AC_MSG_ERROR([no usable regular expression library found])])
fi

dnl Handle optional libraries and probing for their locations and component
dnl libraries if needed.
INN_LIB_BERKELEYDB
INN_LIB_ZLIB
INN_LIB_OPENSSL
INN_LIB_SASL
INN_LIB_KERBEROS

dnl If configuring with large file support, determine the right flags to
dnl use based on the platform.
if test x"$inn_enable_largefiles" = xyes ; then
    AC_SYS_LARGEFILE
    AC_DEFINE([DO_LARGEFILES], [1], [Define to use large files.])
fi

dnl Start by checking for standard C headers.  AC_HEADER_STDC will set
dnl STDC_HEADERS if stdlib.h, stdarg.h, string.h, and float.h all exist, if
dnl memchr (and probably the other mem functions) is in string.h, if free (and
dnl probably malloc and friends) are in stdlib.h, and if ctype.h will work on
dnl high-bit characters.
AC_HEADER_STDC

dnl Only if that wasn't set do we need to go hunting for other headers to
dnl include on non-ANSI systems and check for functions that all ANSI C
dnl systems should have.
if test x"$ac_cv_header_stdc" = xno ; then
    AC_CHECK_HEADERS(memory.h stdlib.h strings.h)
    AC_CHECK_FUNCS(memcpy strchr)
fi

dnl Special checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_HEADER_SYS_WAIT

dnl Generic checks for header files.
AC_CHECK_HEADERS([crypt.h inttypes.h limits.h pam/pam_appl.h stddef.h \
                  stdint.h string.h sys/bitypes.h sys/filio.h sys/loadavg.h \
                  sys/time.h unistd.h])

dnl Some Linux systems have db1/ndbm.h instead of ndbm.h.  Others have
dnl gdbm-ndbm.h.
AC_CHECK_HEADERS([ndbm.h db1/ndbm.h gdbm-ndbm.h], [break])

dnl Check for whether various symbols are declared.
AC_CHECK_DECLS([pread, pwrite, snprintf, vsnprintf])
AC_CHECK_DECLS([h_errno], , , [#include <netdb.h>])
AC_CHECK_DECLS([inet_aton, inet_ntoa], , ,
[#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>])
AC_CHECK_DECLS([altzone], , , [#include <time.h>])

dnl Checks for compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
INN_C_C99_VAMACROS
INN_C_GNU_VAMACROS

dnl Checks for structures.
AC_STRUCT_ST_BLKSIZE
AC_STRUCT_TM
AC_STRUCT_TIMEZONE
AC_CHECK_MEMBERS([struct tm.tm_gmtoff])
AC_CHECK_MEMBERS([struct sockaddr.sa_len], , ,
[#include <sys/types.h>
#include <sys/socket.h>])

dnl Checks for types.
INN_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_CHECK_TYPES([long long])
AC_CHECK_TYPE([ptrdiff_t], ,
    [AC_DEFINE([ptrdiff_t], [long],
        [Define to long if <sys/types.h> does not define.])])
AC_CHECK_TYPE([ssize_t], ,
    [AC_DEFINE([ssize_t], [int],
        [Define to int if <sys/types.h> does not define.])])
AC_CHECK_TYPE([sig_atomic_t], ,
    [AC_DEFINE([sig_atomic_t], [int],
        [Define to int if <signal.h> does not define.])],
[#include <sys/types.h>
#include <signal.h>])
AC_CHECK_TYPE([socklen_t], ,
    [AC_DEFINE([socklen_t], [int],
        [Define to int if <sys/socket.h> does not define.])],
[#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_TYPES([union semun], , ,
[#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>])

dnl See if struct sockaddr_storage is available, and if so, how its members
dnl are named.
AC_CHECK_TYPES([struct sockaddr_storage],
    [AC_CHECK_MEMBERS([struct sockaddr_storage.ss_family], , ,
        [#include <sys/types.h>
        #include <sys/socket.h>])], ,
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
INN_FUNC_INET_NTOA
INN_FUNC_SNPRINTF

dnl Check for various other functions.
AC_CHECK_FUNCS(atexit getloadavg getrusage getspnam setbuffer sigaction \
               setgroups setrlimit setsid socketpair strncasecmp strtoul \
               sysconf)

dnl Find a way to get the file descriptor limit.
AC_CHECK_FUNCS([getrlimit getdtablesize ulimit], [break])

dnl Probe for the appropriate stat functions.
AC_CHECK_FUNCS([statvfs], ,
    [AC_CHECK_FUNCS([statfs])
     AC_CHECK_HEADERS([sys/vfs.h sys/param.h sys/mount.h])])

dnl If we can't find any of the following, we have replacements for them.
AC_REPLACE_FUNCS(getaddrinfo getnameinfo getpagesize hstrerror inet_aton \
                 inet_ntop mkstemp pread pwrite seteuid strcasecmp strerror \
                 strlcat strlcpy strspn setenv symlink)

dnl Probe for fseeko and ftello, which take off_t instead of int.
if test x"$inn_enable_largefiles" = xyes ; then
    AC_FUNC_FSEEKO
    if test x"$ac_cv_func_fseeko" != xyes ; then
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
AC_CONFIG_FILES([Makefile.global include/paths.h])
AC_CONFIG_FILES([scripts/inncheck scripts/innshellvars])
AC_CONFIG_FILES([scripts/innshellvars.pl scripts/innshellvars.tcl])
AC_CONFIG_FILES([scripts/news.daily])
AC_CONFIG_FILES([support/fixconfig], [chmod +x support/fixconfig])
AC_CONFIG_FILES([support/fixscript], [chmod +x support/fixscript])

dnl Include some additional files at the beginning of config.h.
AH_TOP(
[#include "inn/defines.h"
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
        include/paths.h
        innfeed/innfeed.h

EOM

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
