dnl modes.m4 -- Setting file and installation modes.
dnl $Id$
dnl
dnl INN defaults to a umask of 002 for historical reasons, but offers an
dnl option to change them.  It also has some programs that are occasionally
dnl installed with special permissions in some situations but not in others
dnl (and not by default).

dnl Let the user specify a new umask and also set the directory and file
dnl permissions at the same time.  Also let the user request inews or rnews
dnl be installed with special permissions.
AC_DEFUN([INN_ARG_MODES],
[NEWSUMASK=02
FILEMODE=0664
DIRMODE=0775
RUNDIRMODE=0770
AC_ARG_WITH([news-umask],
    [AC_HELP_STRING([--with-news-umask=UMASK], [umask for news files [002]])],
    with_news_umask=`echo "$with_news_umask" | sed 's/^0*//'`
    if test "x$with_news_umask" = x22 ; then
        NEWSUMASK=022
        FILEMODE=0644
        DIRMODE=0755
        RUNDIRMODE=0750
    else
        if test "x$with_news_umask" != x2 ; then
            AC_MSG_ERROR([Valid umasks are 002 or 022])
        fi
    fi)
AC_SUBST([NEWSUMASK])
AC_SUBST([FILEMODE])
AC_SUBST([DIRMODE])
AC_SUBST([RUNDIRMODE])
AC_DEFINE_UNQUOTED([ARTFILE_MODE], [$FILEMODE],
    [Mode that incoming articles are created with.])
AC_DEFINE_UNQUOTED([BATCHFILE_MODE], [$FILEMODE],
    [Mode that batch files are created with.])
AC_DEFINE_UNQUOTED([GROUPDIR_MODE], [$DIRMODE],
    [Mode that directories are created with.])
AC_DEFINE_UNQUOTED([NEWSUMASK], [$NEWSUMASK],
    [The umask used by all INN programs.])

dnl inews used to be installed setgid, but may not be secure.  Only do this if
dnl it's explicitly requested at configure time.
INEWSMODE=0550
AC_ARG_ENABLE([setgid-inews],
    [AC_HELP_STRING([--enable-setgid-inews], [Install inews setgid])],
    if test "x$enableval" = xyes ; then
        INEWSMODE=02555
    fi)
AC_SUBST([INEWSMODE])

dnl rnews used to be installed setuid root so that it could be run by the uucp
dnl user for incoming batches, but this isn't necessary unless you're using
dnl UUCP (which most people aren't) and setuid news is all that's needed.
dnl Only do even that if it's explicitly requested at configure time.
RNEWSGRP=$NEWSGRP
RNEWSMODE=0500
AC_ARG_ENABLE([uucp-rnews],
    [AC_HELP_STRING([--enable-uucp-rnews],
        [Install rnews setuid, group uucp])],
    if test "x$enableval" = xyes ; then
        RNEWSGRP=uucp
        RNEWSMODE=04550
    fi)
AC_SUBST([RNEWSGRP])
AC_SUBST([RNEWSMODE])])
