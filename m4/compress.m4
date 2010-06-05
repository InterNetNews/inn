dnl compress.m4 -- Log compression handling.
dnl $Id$
dnl
dnl By default, INN compresses logs with gzip, but some people may want to use
dnl compress instead, and others may want to use bzip2, or even not compress
dnl at all their logs.  INN also needs to locate gzip regardless, since it's
dnl used for compressed rnews batches, and needs to know how to uncompress .Z
dnl files.
dnl
dnl There are two macros defined here.  The first, INN_ARG_COMPRESS, sets the
dnl command-line option that lets the user specify what compression program to
dnl use for logs.  The second, INN_PATH_COMPRESS, hunts down the appropriate
dnl compression programs.

dnl Choose the log compression method; the argument should not be a full path,
dnl just the name of the compression type.
AC_DEFUN([INN_ARG_COMPRESS],
[AC_ARG_WITH([log-compress],
    [AS_HELP_STRING([--with-log-compress=METHOD],
        [Log compression method [gzip]])],
    LOG_COMPRESS=$with_log_compress,
    LOG_COMPRESS=gzip)
case "$LOG_COMPRESS" in
bzip2)    LOG_COMPRESSEXT=".bz2" ;;
cat)      LOG_COMPRESSEXT=""     ;;
compress) LOG_COMPRESSEXT=".Z"   ;;
gzip)     LOG_COMPRESSEXT=".gz"  ;;
*)        AC_MSG_ERROR([unknown log compression type $LOG_COMPRESS]) ;;
esac
AC_SUBST([LOG_COMPRESS])
AC_SUBST([LOG_COMPRESSEXT])])

dnl Track down the appropriate compression programs.  We always look for gzip
dnl and compress (although we may not use the result of looking for compress
dnl in the future; right now, the only user is innshellvars), and we also look
dnl for some other compression program if another was requested.  We also need
dnl to find a program to use to uncompress .Z files, preferring gzip if found.
dnl
dnl Only the program for log compression has to be found; for other purposes,
dnl use the bare program name if it can't be found in the path.
AC_DEFUN([INN_PATH_COMPRESS],
[AC_ARG_VAR([BZIP2], [Location of bzip2 program])
AC_ARG_VAR([CAT], [Location of cat program])
AC_ARG_VAR([COMPRESS], [Location of compress program])
AC_ARG_VAR([GZIP], [Location of gzip program])
AC_PATH_PROG([BZIP2], [bzip2], [bzip2])
AC_PATH_PROG([CAT], [cat], [cat])
AC_PATH_PROG([COMPRESS], [compress], [compress])
AC_PATH_PROG([GZIP], [gzip], [gzip])
case "$LOG_COMPRESS" in
bzip2)
    if test x"$BZIP2" = xbzip2 ; then
        AC_MSG_ERROR([bzip2 not found but specified for log compression])
    fi
    LOG_COMPRESS=$BZIP2
    ;;
cat)
    if test x"$CAT" = xcat ; then
        AC_MSG_ERROR([cat not found but specified for log compression])
    fi
    LOG_COMPRESS=$CAT
    ;;
compress)
    if test x"$COMPRESS" = xcompress ; then
        AC_MSG_ERROR([compress not found but specified for log compression])
    fi
    LOG_COMPRESS=$COMPRESS
    ;;
gzip)
    if test x"$GZIP" = xgzip ; then
        AC_MSG_ERROR([gzip not found but specified for log compression])
    fi
    LOG_COMPRESS=$GZIP
    ;;
*)
    INN_PATH_PROG_ENSURE([LOG_COMPRESS], [$LOG_COMPRESS])
    ;;
esac
if test x"$GZIP" != xgzip || test x"$COMPRESS" = xcompress ; then
    UNCOMPRESS="$GZIP -d"
else
    UNCOMPRESS="$COMPRESS -d"
fi
AC_SUBST([UNCOMPRESS])])
