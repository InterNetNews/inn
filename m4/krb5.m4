dnl krb5.m4 -- Find the Kerberos v5 libraries.
dnl $Id$
dnl
dnl Defines the macro INN_LIB_KERBEROS, which probes for the Kerberos v5
dnl libraries and defines the output variables KRB5_CPPFLAGS, KRB5_LDFLAGS,
dnl and KRB5_LIBS to the appropriate preprocessor and linker flags.
dnl
dnl Also defines KRB5_AUTH to auth_krb5 if Kerberos was desired and found and
dnl probe for krb5_init_ets (which may or may not be required).

AC_DEFUN([INN_LIB_KERBEROS],
[KRB5_CPPFLAGS=
KRB5_LDFLAGS=
KRB5_LIBS=
KRB5_AUTH=
AC_ARG_WITH([kerberos],
    [AS_HELP_STRING([--with-kerberos@<:@=PATH@:>@],
        [Enable Kerberos v5 (for auth_krb5)])],
    KRB5_DIR=$with_kerberos,
    KRB5_DIR=no)
AC_MSG_CHECKING([if Kerberos is desired])
if test x"$KRB5_DIR" = xno ; then
    AC_MSG_RESULT([no])
else
    AC_MSG_RESULT([yes])
    AC_MSG_CHECKING([for Kerberos location])
    if test x"$KRB5_DIR" = xyes ; then
        for dir in $prefix /usr/local /usr ; do
            if test -f "$dir/include/krb5.h" ; then
                KRB5_DIR=$dir
                break
            fi
        done
    fi
    if test x"$KRB5_DIR" = xyes ; then
        AC_MSG_ERROR([cannot find Kerberos])
    else
        AC_MSG_RESULT([$KRB5_DIR])
        if test x"$KRB5_DIR" != x/usr ; then
            KRB5_CPPFLAGS="-I$KRB5_DIR/include"
            KRB5_LDFLAGS="-L$KRB5_DIR/lib"
        fi
        inn_save_LDFLAGS=$LDFLAGS
        LDFLAGS="$KRB5_LDFLAGS $LDFLAGS"
        AC_CHECK_LIB([krb5], [krb5_init_context],
            [KRB5_LIBS="-lkrb5 -lasn1 -lroken -lcrypto -lcom_err"],
            [KRB5_EXTRA="-lk5crypto -lcom_err"
             AC_CHECK_LIB([krb5support], [krb5int_getspecific],
                 [KRB5_EXTRA="$KRB5_EXTRA -lkrb5support"],
                 [AC_SEARCH_LIBS([pthread_setspecific], [pthreads pthread])
                  AC_CHECK_LIB([krb5support], [krb5int_setspecific],
                      [KRB5_EXTRA="$KRB5_EXTRA -lkrb5support"])])
             AC_CHECK_LIB([krb5], [krb5_cc_default],
                 [KRB5_LIBS="-lkrb5 $KRB5_EXTRA"],
                 [AC_MSG_ERROR([cannot find usable Kerberos v5 library])],
                 [$KRB5_EXTRA])],
            [-lasn1 -lroken -lcrypto -lcom_err])
        LDFLAGS=$inn_save_LDFLAGS
        KRB5_AUTH=auth_krb5
        AC_CHECK_HEADERS([et/com_err.h])
    fi
fi
AC_SUBST([KRB5_CPPFLAGS])
AC_SUBST([KRB5_LDFLAGS])
AC_SUBST([KRB5_LIBS])
AC_SUBST([KRB5_AUTH])])
