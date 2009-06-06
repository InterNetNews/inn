dnl users.m4 -- The usernames built into INN at compile time.
dnl $Id$
dnl
dnl INN allows the user and group INN will run as to be specified, as well as
dnl the user to receive nightly reports and the like.

dnl The settings are all fairly similar, so factor the commonality into this
dnl macro.  Takes the name of what we're looking for, the default, the
dnl variable to set, the help string, and the comment for config.h.
AC_DEFUN([_INN_ARG_USER],
[AC_ARG_WITH([news-$1], [$4], [$3=$with_news_$1], [$3=$2])
AC_SUBST($3)
AC_DEFINE_UNQUOTED($3, "$[$3]", [$5])])

dnl And here they are.
AC_DEFUN([INN_ARG_USERS],
[_INN_ARG_USER([user], [news], [RUNASUSER],
    [AS_HELP_STRING([--with-news-user=USER], [News user name [news]])],
    [The user that INN should run as.])
_INN_ARG_USER([group], [news], [RUNASGROUP],
    [AS_HELP_STRING([--with-news-group=GROUP], [News group name [news]])],
    [The group that INN should run as.])
_INN_ARG_USER([master], [usenet], [NEWSMASTER],
    [AS_HELP_STRING([--with-news-master=USER],
        [News master (address for reports) [usenet]])],
    [The user who gets all INN-related e-mail.])])
