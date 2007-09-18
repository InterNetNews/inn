dnl paths.m4 -- Configure various paths used by INN.
dnl $Id$
dnl
dnl INN has quite a few more configurable paths than autoconf supports by
dnl default.  The regular --*dir options are honored where appropriate, but
dnl for the rest that are non-standard, add --with-*-dir options.
dnl
dnl Also set the output variables PATH_CONFIG and PATH_TMP with fully expanded
dnl versions of tmpdir and sysconfdir, suitable for use in C code.

dnl This is the generic macro for those arguments; it takes the name of the
dnl directory, the path relative to $prefix if none given to configure, the
dnl variable to set, and the help string.
AC_DEFUN([_INN_ARG_DIR],
[AC_ARG_WITH([$1-dir], [$4], [$3=$with_$1_dir], [$3=$2])
AC_SUBST($3)])

dnl And here are all the paths.
AC_DEFUN([INN_ARG_PATHS],
[_INN_ARG_DIR([control], ['${bindir}/control'], [CONTROLDIR],
    [AC_HELP_STRING([--with-control-dir=PATH],
        [Path for control programs [PREFIX/bin/control]])])
_INN_ARG_DIR([db], ['${prefix}/db'], [DBDIR],
    [AC_HELP_STRING([--with-db-dir=PATH],
        [Path for news database files [PREFIX/db]])])
_INN_ARG_DIR([doc], ['${prefix}/doc'], [docdir],
    [AC_HELP_STRING([--with-doc-dir=PATH],
        [Path for news documentation [PREFIX/doc]])])
_INN_ARG_DIR([filter], ['${bindir}/filter'], [FILTERDIR],
    [AC_HELP_STRING([--with-filter-dir=PATH],
        [Path for embedded filters [PREFIX/bin/filter]])])
_INN_ARG_DIR([libperl], ['${libdir}/perl'], [LIBPERLDIR],
    [AC_HELP_STRING([--with-libperl-dir=PATH],
        [Path for Perl modules [PREFIX/lib/perl]])])
_INN_ARG_DIR([log], ['${prefix}/log'], [LOGDIR],
    [AC_HELP_STRING([--with-log-dir=PATH],
        [Path for news logs [PREFIX/log]])])
_INN_ARG_DIR([run], ['${prefix}/run'], [RUNDIR],
    [AC_HELP_STRING([--with-run-dir=PATH],
        [Path for news PID/runtime files [PREFIX/run]])])
_INN_ARG_DIR([spool], ['${prefix}/spool'], [SPOOLDIR],
    [AC_HELP_STRING([--with-spool-dir=PATH],
        [Path for news storage [PREFIX/spool]])])
_INN_ARG_DIR([tmp], ['${prefix}/tmp'], [tmpdir],
    [AC_HELP_STRING([--with-tmp-dir=PATH],
        [Path for temporary files [PREFIX/tmp]])])

dnl Some additional paths used by inn/paths.h.
eval PATH_CONFIG="$sysconfdir"
eval PATH_TMP="$tmpdir"
AC_SUBST([PATH_CONFIG])
AC_SUBST([PATH_TMP])])
