dnl Use krb5-config to get link paths for Kerberos libraries.
dnl $Id$
dnl
dnl Provides one macro, INN_KRB5_CONFIG, which attempts to get compiler and
dnl linker flags for a library via krb5-config and sets the appropriate shell
dnl variables.  Defines the Autoconf variable PATH_KRB5_CONFIG, which can be
dnl used to find the default path to krb5-config.
dnl
dnl Depends on INN_ENABLE_REDUCED_DEPENDS.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2011, 2012
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Check for krb5-config in the user's path and set PATH_KRB5_CONFIG.  This
dnl is moved into a separate macro so that it can be loaded via AC_REQUIRE,
dnl meaning it will only be run once even if we link with multiple krb5-config
dnl libraries.
AC_DEFUN([_INN_KRB5_CONFIG_PATH],
[AC_ARG_VAR([PATH_KRB5_CONFIG], [Path to krb5-config])
 AC_PATH_PROG([PATH_KRB5_CONFIG], [krb5-config], [],
    [${PATH}:/usr/kerberos/bin])])

dnl Check whether the --deps flag is supported by krb5-config.  Takes the path
dnl to krb5-config to use.  Note that this path is not embedded in the cache
dnl variable, so this macro implicitly assumes that we will always use the
dnl same krb5-config program.
AC_DEFUN([_INN_KRB5_CONFIG_DEPS],
[AC_REQUIRE([_INN_KRB5_CONFIG_PATH])
 AC_CACHE_CHECK([for --deps support in krb5-config],
    [inn_cv_krb5_config_deps],
    [AS_IF(["$1" 2>&1 | grep deps >/dev/null 2>&1],
        [inn_cv_krb5_config_deps=yes],
        [inn_cv_krb5_config_deps=no])])])

dnl Obtain the library flags for a particular library using krb5-config.
dnl Takes the path to the krb5-config program to use, the argument to
dnl krb5-config to use, and the variable prefix under which to store the
dnl library flags.
AC_DEFUN([_INN_KRB5_CONFIG_LIBS],
[AC_REQUIRE([_INN_KRB5_CONFIG_PATH])
 AC_REQUIRE([INN_ENABLE_REDUCED_DEPENDS])
 _INN_KRB5_CONFIG_DEPS([$1])
 AS_IF([test x"$inn_reduced_depends" = xfalse \
        && test x"$inn_cv_krb5_config_deps" = xyes],
    [$3[]_LIBS=`"$1" --deps --libs $2 2>/dev/null`],
    [$3[]_LIBS=`"$1" --libs $2 2>/dev/null`])])

dnl Attempt to find the flags for a library using krb5-config.  Takes the
dnl following arguments (in order):
dnl
dnl 1. The root directory for the library in question, generally from an
dnl    Autoconf --with flag.  Used by preference as the path to krb5-config.
dnl
dnl 2. The argument to krb5-config to retrieve flags for this particular
dnl    library.
dnl
dnl 3. The variable prefix to use when setting CPPFLAGS and LIBS variables
dnl    based on the result of krb5-config.
dnl
dnl 4. Further actions to take if krb5-config was found and supported that
dnl    library type.
dnl
dnl 5. Further actions to take if krb5-config could not be used to get flags
dnl    for that library type.
dnl
dnl Special-case a krb5-config argument of krb5 and run krb5-config without an
dnl argument if that option was requested and not supported.  Old versions of
dnl krb5-config didn't take an argument to specify the library type, but
dnl always returned the flags for libkrb5.
AC_DEFUN([INN_KRB5_CONFIG],
[AC_REQUIRE([_INN_KRB5_CONFIG_PATH])
 inn_krb5_config_$3=
 inn_krb5_config_$3[]_ok=
 AS_IF([test x"$1" != x && test -x "$1/bin/krb5-config"],
    [inn_krb5_config_$3="$1/bin/krb5-config"],
    [inn_krb5_config_$3="$PATH_KRB5_CONFIG"])
 AS_IF([test x"$inn_krb5_config_$3" != x && test -x "$inn_krb5_config_$3"],
    [AC_CACHE_CHECK([for $2 support in krb5-config], [inn_cv_lib_$3[]_config],
         [AS_IF(["$inn_krb5_config_$3" 2>&1 | grep $2 >/dev/null 2>&1],
             [inn_cv_lib_$3[]_config=yes],
             [inn_cv_lib_$3[]_config=no])])
     AS_IF([test "$inn_cv_lib_$3[]_config" = yes],
        [$3[]_CPPFLAGS=`"$inn_krb5_config_$3" --cflags $2 2>/dev/null`
         _INN_KRB5_CONFIG_LIBS([$inn_krb5_config_$3], [$2], [$3])
         inn_krb5_config_$3[]_ok=yes],
        [AS_IF([test x"$2" = xkrb5],
            [$3[]_CPPFLAGS=`"$inn_krb5_config_$3" --cflags 2>/dev/null`
             $3[]_LIBS=`"$inn_krb5_config_$3" --libs $2 2>/dev/null`
             inn_krb5_config_$3[]_ok=yes])])])
 AS_IF([test x"$inn_krb5_config_$3[]_ok" = xyes],
    [$3[]_CPPFLAGS=`echo "$$3[]_CPPFLAGS" | sed 's%-I/usr/include %%'`
     $3[]_CPPFLAGS=`echo "$$3[]_CPPFLAGS" | sed 's%-I/usr/include$%%'`
     $4],
    [$5])])
