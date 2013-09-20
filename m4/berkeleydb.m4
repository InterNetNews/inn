dnl berkeleydb.m4 -- Various checks for the Berkeley DB libraries.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with Berkeley DB
dnl libraries.  Provides the --with-berkeleydb, --with-berkeleydb-lib,
dnl and --with-berkeleydb-include configure options to specify non-standard
dnl paths to the Berkeley DB libraries.
dnl
dnl Provides the macro INN_LIB_BERKELEYDB and sets the substitution variables
dnl DB_CPPFLAGS, DB_LDFLAGS, and DB_LIBS.  Also provides
dnl INN_LIB_BERKELEYDB_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include
dnl the Berkeley DB libraries, saving the current values first, and
dnl INN_LIB_BERKELEYDB_RESTORE to restore those settings to before the last
dnl INN_LIB_BERKELEYDB_SWITCH.
dnl
dnl If --with-berkeleydb is given, $inn_use_berkeleydb will be set to "true";
dnl and if Berkeley libraries are properly found, USE_BERKELEY_DB will be
dnl defined.
dnl
dnl Depends on INN_SET_LDFLAGS and INN_ENABLE_REDUCED_DEPENDS.
dnl
dnl This file also provides INN_LIB_BERKELEYDB_NDBM, which checks whether
dnl Berkeley DB has ndbm support.  It then defines HAVE_BDB_NDBM if ndbm
dnl compatibility layer for Berkely DB is available.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the Berkeley DB flags.  Used as a wrapper, with
dnl INN_LIB_BERKELEYDB_RESTORE, around tests.
AC_DEFUN([INN_LIB_BERKELEYDB_SWITCH],
[inn_berkeleydb_save_CPPFLAGS="$CPPFLAGS"
 inn_berkeleydb_save_LDFLAGS="$LDFLAGS"
 inn_berkeleydb_save_LIBS="$LIBS"
 CPPFLAGS="$DB_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$DB_LDFLAGS $LDFLAGS"
 LIBS="$DB_LIBS $LIBS"])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_BERKELEYDB_SWITCH was called).
AC_DEFUN([INN_LIB_BERKELEYDB_RESTORE],
[CPPFLAGS="$inn_berkeleydb_save_CPPFLAGS"
 LDFLAGS="$inn_berkeleydb_save_LDFLAGS"
 LIBS="$inn_berkeleydb_save_LIBS"])

dnl Set DB_CPPFLAGS, and DB_LDFLAGS based on inn_berkeleydb_root,
dnl inn_berkeleydb_libdir, and inn_berkeleydb_includedir.
AC_DEFUN([_INN_LIB_BERKELEYDB_PATHS],
[AS_IF([test x"$inn_berkeleydb_libdir" != x],
    [DB_LDFLAGS="-L$inn_berkeleydb_libdir"],
    [AS_IF([test x"$inn_berkeleydb_root" != x],
        [INN_SET_LDFLAGS([DB_LDFLAGS], [$inn_berkeleydb_root])])])
 AS_IF([test x"$inn_berkeleydb_includedir" != x],
    [DB_CPPFLAGS="-I$inn_berkeleydb_includedir"
     inn_berkeleydb_incroot="$inn_berkeleydb_includedir"],
    [AS_IF([test x"$inn_berkeleydb_root" != x],
        [AS_IF([test x"$inn_berkeleydb_root" != x/usr],
            [DB_CPPFLAGS="-I${inn_berkeleydb_root}/include"])
         inn_berkeleydb_incroot="${inn_berkeleydb_root}/include"])])])

dnl Check for a header using a file existence check rather than using
dnl AC_CHECK_HEADERS.  This is used if there were arguments to configure
dnl specifying the Berkeley DB header path, since we may have one header in the
dnl default include path and another under our explicitly-configured Berkeley DB
dnl location.
AC_DEFUN([_INN_LIB_BERKELEYDB_CHECK_HEADER],
[AC_MSG_CHECKING([for $1])
 AS_IF([test -f "${inn_berkeleydb_incroot}/$1"],
    [AC_DEFINE([USE_BERKELEY_DB], [1], [Define if Berkeley DB is available.])
     AC_MSG_RESULT([${inn_berkeleydb_incroot}/$1])],
    [AC_MSG_ERROR([cannot find Berkeley DB in ${inn_berkeleydb_incroot}])])])

dnl The main macro used to set up paths.
AC_DEFUN([INN_LIB_BERKELEYDB],
[AC_REQUIRE([INN_ENABLE_REDUCED_DEPENDS])
 inn_berkeleydb_root=
 inn_berkeleydb_incroot=
 inn_berkeleydb_libdir=
 inn_berkeleydb_includedir=
 inn_use_berkeleydb=
 DB_CPPFLAGS=
 DB_LDFLAGS=
 DB_LIBS=
 AC_SUBST([DB_CPPFLAGS])
 AC_SUBST([DB_LDFLAGS])
 AC_SUBST([DB_LIBS])

 AC_ARG_WITH([berkeleydb],
    [AS_HELP_STRING([--with-berkeleydb@<:@=DIR@:>@],
        [Location of Berkeley DB headers and libraries (for ovdb overview method)])],
    [AS_IF([test x"$withval" = xno],
        [inn_use_berkeleydb=false],
        [AS_IF([test x"$withval" != xyes], [inn_berkeleydb_root="$withval"])
         inn_use_berkeleydb=true])],
    [inn_use_berkeleydb=false])
 AC_ARG_WITH([berkeleydb-include],
    [AS_HELP_STRING([--with-berkeleydb-include=DIR],
        [Location of Berkeley DB headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_berkeleydb_includedir="$withval"])])
 AC_ARG_WITH([berkeleydb-lib],
    [AS_HELP_STRING([--with-berkeleydb-lib=DIR],
        [Location of Berkeley DB libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_berkeleydb_libdir="$withval"])])

 AC_MSG_CHECKING([if Berkeley DB is desired])
 AS_IF([test x"$inn_use_berkeleydb" != xfalse],
    [AC_MSG_RESULT([yes])
     _INN_LIB_BERKELEYDB_PATHS
     AS_IF([test x"$inn_berkeleydb_incroot" = x],
         [AC_CHECK_HEADERS([db.h],
             [AC_DEFINE([USE_BERKELEY_DB], [1],
                 [Define if Berkeley DB is available.])],
             [AC_MSG_ERROR([cannot find Berkeley DB in default path])])],
         [_INN_LIB_BERKELEYDB_CHECK_HEADER([db.h])])
     DB_LIBS="-ldb"],
    [AC_MSG_RESULT([no])])])

dnl Source used by INN_LIB_BERKELEYDB_NDBM.
AC_DEFUN([_INN_LIB_BERKELEYDB_NDBM_SOURCE], [[
#include <stdio.h>
#define DB_DBM_HSEARCH 1
#include <db.h>

int
main(void) {
    DBM *database;
    database = dbm_open("test", 0, 0600);
    dbm_close(database);
    return 0;
}
]])

dnl Check whether Berkeley DB was compiled with ndbm compatibily layer.
dnl If so, set HAVE_BDB_NDBM.
AC_DEFUN([INN_LIB_BERKELEYDB_NDBM],
[INN_LIB_BERKELEYDB_SWITCH
AC_CACHE_CHECK([for working nbdm compatibility layer with Berkeley DB],
    [inn_cv_lib_berkeleydb_ndbm_works],
    [AC_LINK_IFELSE([AC_LANG_SOURCE([_INN_LIB_BERKELEYDB_NDBM_SOURCE])],
        [inn_cv_lib_berkeleydb_ndbm_works=yes],
        [inn_cv_lib_berkeleydb_ndbm_works=no])])
AS_IF([test x"$inn_cv_lib_berkeleydb_ndbm_works" = xyes],
    [AC_DEFINE([HAVE_BDB_NDBM], 1,
        [Define if the Berkeley DB ndbm compatibility layer is available.])])
INN_LIB_BERKELEYDB_RESTORE])
