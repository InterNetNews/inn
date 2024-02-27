dnl Check whether the compiler supports particular flags.
dnl
dnl Provides INN_PROG_CC_FLAG and INN_PROG_LD_FLAG, which checks whether a
dnl compiler supports a given flag for either compilation or linking,
dnl respectively.  If it does, the commands in the second argument are run.
dnl If not, the commands in the third argument are run.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2016-2024 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2006, 2009, 2016
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl
dnl Permission to use, copy, modify, and/or distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
dnl REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
dnl SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
dnl IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl SPDX-License-Identifier: ISC

dnl Used to build the result cache name.
AC_DEFUN([_INN_PROG_CC_FLAG_CACHE],
[translit([inn_cv_compiler_c_$1], [-=+,], [____])])
AC_DEFUN([_INN_PROG_LD_FLAG_CACHE],
[translit([inn_cv_linker_c_$1], [-=+,], [____])])

dnl Check whether a given flag is supported by the compiler when compiling a C
dnl source file.
AC_DEFUN([INN_PROG_CC_FLAG],
[AC_REQUIRE([AC_PROG_CC])
 AC_MSG_CHECKING([if $CC supports $1])
 AC_CACHE_VAL([_INN_PROG_CC_FLAG_CACHE([$1])],
    [save_CFLAGS=$CFLAGS
     AS_CASE([$1],
        [-Wno-*], [CFLAGS="$CFLAGS `AS_ECHO(["$1"]) | sed 's/-Wno-/-W/'`"],
        [CFLAGS="$CFLAGS $1"])
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [int foo = 0;])],
        [_INN_PROG_CC_FLAG_CACHE([$1])=yes],
        [_INN_PROG_CC_FLAG_CACHE([$1])=no])
     CFLAGS=$save_CFLAGS])
 AC_MSG_RESULT([$_INN_PROG_CC_FLAG_CACHE([$1])])
 AS_IF([test x"$_INN_PROG_CC_FLAG_CACHE([$1])" = xyes], [$2], [$3])])

dnl Check whether a given flag is supported by the compiler when linking an
dnl executable.
AC_DEFUN([INN_PROG_LD_FLAG],
[AC_REQUIRE([AC_PROG_CC])
 AC_MSG_CHECKING([if $CC supports $1 for linking])
 AC_CACHE_VAL([_INN_PROG_LD_FLAG_CACHE([$1])],
    [save_LDFLAGS=$LDFLAGS
     LDFLAGS="$LDFLAGS $1"
     AC_LINK_IFELSE([AC_LANG_PROGRAM([], [int foo = 0;])],
        [_INN_PROG_LD_FLAG_CACHE([$1])=yes],
        [_INN_PROG_LD_FLAG_CACHE([$1])=no])
     LDFLAGS=$save_LDFLAGS])
 AC_MSG_RESULT([$_INN_PROG_LD_FLAG_CACHE([$1])])
 AS_IF([test x"$_INN_PROG_LD_FLAG_CACHE([$1])" = xyes], [$2], [$3])])
