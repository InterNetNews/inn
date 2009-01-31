dnl cc-flags.m4 -- Checks whether the compiler supports a given flag.
dnl $Id$
dnl
dnl Used to check whether a compiler supports a given flag.  If it does, the
dnl commands in the second argument are run.  If not, the commands in the
dnl third argument are run.

dnl Used to build the result cache name.
AC_DEFUN([_INN_PROG_CC_FLAG_CACHE],
[translit([inn_cv_compiler_c_$1], [-], [_])])

AC_DEFUN([INN_PROG_CC_FLAG],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING([if $CC supports $1])
AC_CACHE_VAL([_INN_PROG_CC_FLAG_CACHE([$1])],
[save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $1"
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([[]], [[int foo = 0;]])],
    [_INN_PROG_CC_FLAG_CACHE([$1])=yes],
    [_INN_PROG_CC_FLAG_CACHE([$1])=no])
CFLAGS=$save_CFLAGS])
AC_MSG_RESULT($_INN_PROG_CC_FLAG_CACHE([$1]))
 if test x"$_INN_PROG_CC_FLAG_CACHE([$1])" = xyes ; then
    ifelse([$2], , :, [$2])
 else
    ifelse([$3], , :, [$3])
 fi])
