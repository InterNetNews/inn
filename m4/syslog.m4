dnl syslog.m4 -- Options and probes for syslog behavior.
dnl $Id$
dnl
dnl Normally, INN just logs everything to the news facility, but some systems
dnl don't have that so we have to probe and see if we should use local1
dnl instead.  And we also provide an option to change the facility to use.
dnl
dnl INN_LOG_FACILITY sets the output variable SYSLOG_FACILITY and also defines
dnl LOG_INN_SERVER and LOG_INN_PROG in config.h.

dnl Set up the --with-syslog-facility option.  The result is stored in the
dnl inn_syslog_facility variable for later use.
AC_DEFUN([INN_ARG_SYSLOG],
[AC_ARG_WITH([syslog-facility],
    [AC_HELP_STRING([--with-syslog-facility=LOG_FAC],
        [Syslog facility [LOG_NEWS or LOG_LOCAL1]])],
    SYSLOG_FACILITY=$with_syslog_facility,
    SYSLOG_FACILITY=none)])

dnl Determine the facility for syslog messages.  Default to LOG_NEWS for
dnl syslog facility if it's available, but if it's not, fall back on
dnl LOG_LOCAL1.  Honor the existing SYSLOG_FACILITY value if already set by
dnl INN_ARG_SYSLOG.
AC_DEFUN([INN_LOG_FACILITY],
[AC_MSG_CHECKING(log facility for news)
AC_CACHE_VAL(inn_cv_log_facility,
[AC_EGREP_CPP(yes,
[#include <syslog.h>
#ifdef LOG_NEWS
yes
#endif],
              inn_cv_log_facility=LOG_NEWS,
              inn_cv_log_facility=LOG_LOCAL1)])
if test x"$SYSLOG_FACILITY" = xnone ; then
    SYSLOG_FACILITY=$inn_cv_log_facility
fi
AC_MSG_RESULT([$SYSLOG_FACILITY])
AC_DEFINE_UNQUOTED([LOG_INN_SERVER], [$SYSLOG_FACILITY],
    [Syslog facility to use for innd logs.])
AC_DEFINE_UNQUOTED([LOG_INN_PROG], [$SYSLOG_FACILITY],
    [Syslog facility to use for INN program logs.])
AC_SUBST(SYSLOG_FACILITY)])
