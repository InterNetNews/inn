dnl Find the compiler and linker flags for Kerberos.
dnl $Id$
dnl
dnl Finds the compiler and linker flags for linking with Kerberos libraries.
dnl Provides the --with-krb5, --with-krb5-include, and --with-krb5-lib
dnl configure options to specify non-standard paths to the Kerberos libraries.
dnl Uses krb5-config where available unless reduced dependencies is requested
dnl or --with-krb5-include or --with-krb5-lib are given.
dnl
dnl Provides the macro INN_LIB_KRB5 and sets the substitution variables
dnl KRB5_CPPFLAGS, KRB5_LDFLAGS, and KRB5_LIBS.  Also provides
dnl INN_LIB_KRB5_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl Kerberos libraries, saving the current values first, and
dnl INN_LIB_KRB5_RESTORE to restore those settings to before the last
dnl INN_LIB_KRB5_SWITCH.  HAVE_KRB5 will always be defined if INN_LIB_KRB5 is
dnl used.
dnl
dnl If KRB5_CPPFLAGS, KRB5_LDFLAGS, or KRB5_LIBS are set before calling these
dnl macros, their values will be added to whatever the macros discover.
dnl
dnl Provides the INN_LIB_KRB5_OPTIONAL macro, which should be used if Kerberos
dnl support is optional.  In this case, Kerberos libraries are mandatory if
dnl --with-krb5 is given, and will not be probed for if --without-krb5 is
dnl given.  Otherwise, they'll be probed for but will not be required.
dnl Defines HAVE_KRB5 and sets inn_use_KRB5 to true if the libraries are
dnl found.  The substitution variables will always be set, but they will be
dnl empty unless Kerberos libraries are found and the user did not disable
dnl Kerberos support.
dnl
dnl Sets the Automake conditional KRB5_USES_COM_ERR saying whether we use
dnl com_err, since if we're also linking with AFS libraries, we may have to
dnl change library ordering in that case.
dnl
dnl Depends on INN_KRB5_CONFIG, INN_ENABLE_REDUCED_DEPENDS, and
dnl INN_SET_LDFLAGS.
dnl
dnl Also provides INN_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS, which checks
dnl whether krb5_get_init_creds_opt_free takes one argument or two.  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS if it takes two arguments.
dnl
dnl Also provides INN_INCLUDES_KRB5, which are the headers to include when
dnl probing the Kerberos library properties.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2005, 2006, 2007, 2008, 2009, 2010, 2011
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Ignore Automake conditionals if not using Automake.
m4_define_default([AM_CONDITIONAL], [:])

dnl Headers to include when probing for Kerberos library properties.
AC_DEFUN([INN_INCLUDES_KRB5], [[
#if HAVE_KRB5_H
# include <krb5.h>
#else
# include <krb5/krb5.h>
#endif
]])

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the Kerberos flags.  Used as a wrapper, with
dnl INN_LIB_KRB5_RESTORE, around tests.
AC_DEFUN([INN_LIB_KRB5_SWITCH],
[inn_krb5_save_CPPFLAGS="$CPPFLAGS"
 inn_krb5_save_LDFLAGS="$LDFLAGS"
 inn_krb5_save_LIBS="$LIBS"
 CPPFLAGS="$KRB5_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$KRB5_LDFLAGS $LDFLAGS"
 LIBS="$KRB5_LIBS $LIBS"])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl INN_LIB_KRB5_SWITCH was called).
AC_DEFUN([INN_LIB_KRB5_RESTORE],
[CPPFLAGS="$inn_krb5_save_CPPFLAGS"
 LDFLAGS="$inn_krb5_save_LDFLAGS"
 LIBS="$inn_krb5_save_LIBS"])

dnl Set KRB5_CPPFLAGS and KRB5_LDFLAGS based on inn_krb5_root,
dnl inn_krb5_libdir, and inn_krb5_includedir.
AC_DEFUN([_INN_LIB_KRB5_PATHS],
[AS_IF([test x"$inn_krb5_libdir" != x],
    [KRB5_LDFLAGS="-L$inn_krb5_libdir"],
    [AS_IF([test x"$inn_krb5_root" != x],
        [INN_SET_LDFLAGS([KRB5_LDFLAGS], [$inn_krb5_root])])])
 AS_IF([test x"$inn_krb5_includedir" != x],
    [KRB5_CPPFLAGS="-I$inn_krb5_includedir"],
    [AS_IF([test x"$inn_krb5_root" != x],
        [AS_IF([test x"$inn_krb5_root" != x/usr],
            [KRB5_CPPFLAGS="-I${inn_krb5_root}/include"])])])])

dnl Check for a header using a file existence check rather than using
dnl AC_CHECK_HEADERS.  This is used if there were arguments to configure
dnl specifying the Kerberos header path, since we may have one header in the
dnl default include path and another under our explicitly-configured Kerberos
dnl location.
AC_DEFUN([_INN_LIB_KRB5_CHECK_HEADER],
[AC_MSG_CHECKING([for $1])
 AS_IF([test -f "${inn_krb5_incroot}/$1"],
    [AC_DEFINE_UNQUOTED(AS_TR_CPP([HAVE_$1]), [1],
        [Define to 1 if you have the <$1> header file.])
     AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])])])

dnl Does the appropriate library checks for reduced-dependency Kerberos
dnl linkage.  The single argument, if true, says to fail if Kerberos could not
dnl be found.
AC_DEFUN([_INN_LIB_KRB5_REDUCED],
[INN_LIB_KRB5_SWITCH
 AC_CHECK_LIB([krb5], [krb5_init_context], [KRB5_LIBS="-lkrb5"],
     [AS_IF([test x"$1" = xtrue],
         [AC_MSG_ERROR([cannot find usable Kerberos library])])])
 LIBS="$KRB5_LIBS $LIBS"
 AS_IF([test x"$inn_krb5_incroot" = x],
     [AC_CHECK_HEADERS([krb5.h krb5/krb5.h])],
     [_INN_LIB_KRB5_CHECK_HEADER([krb5.h])
      _INN_LIB_KRB5_CHECK_HEADER([krb5/krb5.h])])
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_LIB([ksvc], [krb5_svc_get_msg],
                 [KRB5_LIBS="$KRB5_LIBS -lksvc"
                  AC_DEFINE([HAVE_KRB5_SVC_GET_MSG], [1])
                  AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [INN_INCLUDES_KRB5])],
                 [AC_CHECK_LIB([com_err], [com_err],
                     [KRB5_LIBS="$KRB5_LIBS -lcom_err"],
                     [AC_MSG_ERROR([cannot find usable com_err library])])
                  AC_CHECK_HEADERS([et/com_err.h])])])])])
 INN_LIB_KRB5_RESTORE])

dnl Does the appropriate library checks for Kerberos linkage when we don't
dnl have krb5-config or reduced dependencies.  The single argument, if true,
dnl says to fail if Kerberos could not be found.
AC_DEFUN([_INN_LIB_KRB5_MANUAL],
[INN_LIB_KRB5_SWITCH
 inn_krb5_extra=
 LIBS=
 AC_SEARCH_LIBS([res_search], [resolv], [],
    [AC_SEARCH_LIBS([__res_search], [resolv])])
 AC_SEARCH_LIBS([gethostbyname], [nsl])
 AC_SEARCH_LIBS([socket], [socket], [],
    [AC_CHECK_LIB([nsl], [socket], [LIBS="-lnsl -lsocket $LIBS"], [],
        [-lsocket])])
 AC_SEARCH_LIBS([crypt], [crypt])
 AC_SEARCH_LIBS([roken_concat], [roken])
 inn_krb5_extra="$LIBS"
 LIBS="$inn_krb5_save_LIBS"
 AC_CHECK_LIB([krb5], [krb5_init_context],
    [KRB5_LIBS="-lkrb5 -lasn1 -lcom_err -lcrypto $inn_krb5_extra"],
    [AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [inn_krb5_extra="-lkrb5support $inn_krb5_extra"],
        [AC_CHECK_LIB([pthreads], [pthread_setspecific],
            [inn_krb5_pthread="-lpthreads"],
            [AC_CHECK_LIB([pthread], [pthread_setspecific],
                [inn_krb5_pthread="-lpthread"])])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [inn_krb5_extra="-lkrb5support $inn_krb5_extra $inn_krb5_pthread"],
            [], [$inn_krb5_pthread $inn_krb5_extra])],
        [$inn_krb5_extra])
     AC_CHECK_LIB([com_err], [error_message],
        [inn_krb5_extra="-lcom_err $inn_krb5_extra"], [], [$inn_krb5_extra])
     AC_CHECK_LIB([ksvc], [krb5_svc_get_msg],
        [inn_krb5_extra="-lksvc $inn_krb5_extra"], [], [$inn_krb5_extra])
     AC_CHECK_LIB([k5crypto], [krb5int_hash_md5],
        [inn_krb5_extra="-lk5crypto $inn_krb5_extra"], [], [$inn_krb5_extra])
     AC_CHECK_LIB([k5profile], [profile_get_values],
        [inn_krb5_extra="-lk5profile $inn_krb5_extra"], [], [$inn_krb5_extra])
     AC_CHECK_LIB([krb5], [krb5_cc_default],
        [KRB5_LIBS="-lkrb5 $inn_krb5_extra"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable Kerberos library])])],
        [$inn_krb5_extra])],
    [-lasn1 -lcom_err -lcrypto $inn_krb5_extra])
 LIBS="$KRB5_LIBS $LIBS"
 AS_IF([test x"$inn_krb5_incroot" = x],
     [AC_CHECK_HEADERS([krb5.h krb5/krb5.h])],
     [_INN_LIB_KRB5_CHECK_HEADER([krb5.h])
      _INN_LIB_KRB5_CHECK_HEADER([krb5/krb5.h])])
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_FUNCS([krb5_svc_get_msg],
                 [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [INN_INCLUDES_KRB5])],
                 [AC_CHECK_HEADERS([et/com_err.h])])])])])
 INN_LIB_KRB5_RESTORE])

dnl Sanity-check the results of krb5-config and be sure we can really link a
dnl Kerberos program.  If that fails, clear KRB5_CPPFLAGS and KRB5_LIBS so
dnl that we know we don't have usable flags and fall back on the manual
dnl check.
AC_DEFUN([_INN_LIB_KRB5_CHECK],
[INN_LIB_KRB5_SWITCH
 AC_CHECK_FUNC([krb5_init_context],
    [INN_LIB_KRB5_RESTORE],
    [INN_LIB_KRB5_RESTORE
     KRB5_CPPFLAGS=
     KRB5_LIBS=
     _INN_LIB_KRB5_PATHS
     _INN_LIB_KRB5_MANUAL([$1])])])

dnl Determine Kerberos compiler and linker flags from krb5-config.  Does the
dnl additional probing we need to do to uncover error handling features, and
dnl falls back on the manual checks.
AC_DEFUN([_INN_LIB_KRB5_CONFIG],
[INN_KRB5_CONFIG([${inn_krb5_root}], [krb5], [KRB5],
    [_INN_LIB_KRB5_CHECK([$1])
     INN_LIB_KRB5_SWITCH
     AS_IF([test x"$inn_krb5_incroot" = x],
         [AC_CHECK_HEADERS([krb5.h krb5/krb5.h])],
         [_INN_LIB_KRB5_CHECK_HEADER([krb5.h])
          _INN_LIB_KRB5_CHECK_HEADER([krb5/krb5.h])])
     AC_CHECK_FUNCS([krb5_get_error_message],
         [AC_CHECK_FUNCS([krb5_free_error_message])],
         [AC_CHECK_FUNCS([krb5_get_error_string], [],
             [AC_CHECK_FUNCS([krb5_get_err_txt], [],
                 [AC_CHECK_FUNCS([krb5_svc_get_msg],
                     [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                         [INN_INCLUDES_KRB5])],
                     [AC_CHECK_HEADERS([et/com_err.h])])])])])
     INN_LIB_KRB5_RESTORE],
    [_INN_LIB_KRB5_PATHS
     _INN_LIB_KRB5_MANUAL([$1])])])

dnl The core of the library checking, shared between INN_LIB_KRB5 and
dnl INN_LIB_KRB5_OPTIONAL.  The single argument, if "true", says to fail if
dnl Kerberos could not be found.  Set up inn_krb5_incroot for later header
dnl checking.
AC_DEFUN([_INN_LIB_KRB5_INTERNAL],
[AC_REQUIRE([INN_ENABLE_REDUCED_DEPENDS])
 inn_krb5_incroot=
 AS_IF([test x"$inn_krb5_includedir" != x],
    [inn_krb5_incroot="$inn_krb5_includedir"],
    [AS_IF([test x"$inn_krb5_root" != x],
        [inn_krb5_incroot="${inn_krb5_root}/include"])])
 AS_IF([test x"$inn_reduced_depends" = xtrue],
    [_INN_LIB_KRB5_PATHS
     _INN_LIB_KRB5_REDUCED([$1])],
    [AS_IF([test x"$inn_krb5_includedir" = x && test x"$inn_krb5_libdir" = x],
        [_INN_LIB_KRB5_CONFIG([$1])],
        [_INN_LIB_KRB5_PATHS
         _INN_LIB_KRB5_MANUAL([$1])])])
 inn_krb5_uses_com_err=false
 AS_CASE([$LIBS], [*-lcom_err*], [inn_krb5_uses_com_err=true])
 AM_CONDITIONAL([KRB5_USES_COM_ERR],
    [test x"$inn_krb5_uses_com_err" = xtrue])])

dnl The main macro for packages with mandatory Kerberos support.
AC_DEFUN([INN_LIB_KRB5],
[inn_krb5_root=
 inn_krb5_libdir=
 inn_krb5_includedir=
 inn_use_KRB5=true
 AC_SUBST([KRB5_CPPFLAGS])
 AC_SUBST([KRB5_LDFLAGS])
 AC_SUBST([KRB5_LIBS])

 AC_ARG_WITH([krb5],
    [AS_HELP_STRING([--with-krb5=DIR],
        [Location of Kerberos headers and libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_krb5_root="$withval"])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_krb5_libdir="$withval"])])
 _INN_LIB_KRB5_INTERNAL([true])
 AC_DEFINE([HAVE_KRB5], 1, [Define to enable Kerberos features.])])

dnl The main macro for packages with optional Kerberos support.
AC_DEFUN([INN_LIB_KRB5_OPTIONAL],
[inn_krb5_root=
 inn_krb5_libdir=
 inn_krb5_includedir=
 inn_use_KRB5=
 AC_SUBST([KRB5_CPPFLAGS])
 AC_SUBST([KRB5_LDFLAGS])
 AC_SUBST([KRB5_LIBS])

 AC_ARG_WITH([krb5],
    [AS_HELP_STRING([--with-krb5@<:@=DIR@:>@],
        [Location of Kerberos headers and libraries])],
    [AS_IF([test x"$withval" = xno],
        [inn_use_KRB5=false],
        [AS_IF([test x"$withval" != xyes], [inn_krb5_root="$withval"])
         inn_use_KRB5=true])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [inn_krb5_libdir="$withval"])])

 AS_IF([test x"$inn_use_KRB5" != xfalse],
     [AS_IF([test x"$inn_use_KRB5" = xtrue],
         [_INN_LIB_KRB5_INTERNAL([true])],
         [_INN_LIB_KRB5_INTERNAL([false])])],
     [AM_CONDITIONAL([KRB5_USES_COM_ERR], [false])])
 AS_IF([test x"$KRB5_LIBS" != x],
    [inn_use_KRB5=true
     AC_DEFINE([HAVE_KRB5], 1, [Define to enable Kerberos features.])])])

dnl Source used by INN_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS.
AC_DEFUN([_INN_FUNC_KRB5_OPT_FREE_ARGS_SOURCE], [INN_INCLUDES_KRB5] [[
int
main(void)
{
    krb5_get_init_creds_opt *opts;
    krb5_context c;
    krb5_get_init_creds_opt_free(c, opts);
}
]])

dnl Check whether krb5_get_init_creds_opt_free takes one argument or two.
dnl Early Heimdal used to take a single argument.  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS if it takes two arguments.
dnl
dnl Should be called with INN_LIB_KRB5_SWITCH active.
AC_DEFUN([INN_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS],
[AC_CACHE_CHECK([if krb5_get_init_creds_opt_free takes two arguments],
    [inn_cv_func_krb5_get_init_creds_opt_free_args],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_INN_FUNC_KRB5_OPT_FREE_ARGS_SOURCE])],
        [inn_cv_func_krb5_get_init_creds_opt_free_args=yes],
        [inn_cv_func_krb5_get_init_creds_opt_free_args=no])])
 AS_IF([test $inn_cv_func_krb5_get_init_creds_opt_free_args = yes],
    [AC_DEFINE([HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS], 1,
        [Define if krb5_get_init_creds_opt_free takes two arguments.])])])
