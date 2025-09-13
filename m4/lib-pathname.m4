dnl Determine the library path name.
dnl
dnl Red Hat systems and some other Linux systems use lib64 and either lib or
dnl lib32 rather than just lib in some circumstances. This file provides an
dnl Autoconf macro, INN_SET_LDFLAGS, which given a variable, a prefix, and an
dnl optional suffix, adds -Lprefix/lib, -Lprefix/lib32, or -Lprefix/lib64 to
dnl the variable depending on which directories exist and the size of a long
dnl in the compilation environment. If a suffix is given, a slash and that
dnl suffix will be appended, to allow for adding a subdirectory of the library
dnl directory.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2021-2022, 2025 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2008-2009
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Probe for the alternate library name that we should attempt on this
dnl architecture, given the size of an int, and set inn_lib_arch_name to that
dnl name. Separated out so that it can be AC_REQUIRE'd and not run multiple
dnl times.
dnl
dnl There is an unfortunate abstraction violation here where we assume we know
dnl the cache variable name used by Autoconf. Unfortunately, Autoconf doesn't
dnl provide any other way of getting at that information in shell that I can
dnl see.
AC_DEFUN([_INN_LIB_ARCH_NAME],
[inn_lib_arch_name=lib
 AC_CHECK_SIZEOF([long])
 AS_IF([test "$ac_cv_sizeof_long" -eq 4],
    [inn_lib_arch_name=lib32],
    [AS_IF([test "$ac_cv_sizeof_long" -eq 8],
        [inn_lib_arch_name=lib64])])])

dnl Set VARIABLE to -LPREFIX/lib{,32,64} or -LPREFIX/lib{,32,64}/SUFFIX as
dnl appropriate.
dnl
dnl There is an unfortunate wrinkle here that some systems may have a lib64 in
dnl the PREFIX directory but still have the vast majority of the libraries in
dnl lib. (See https://github.com/InterNetNews/inn/issues/326.) Unfortunately,
dnl other systems may use lib64 for 64-bit libraries and lib for 32-bit
dnl libraries. The right approach is probably to probe for libraries under
dnl each path in sequence and take the first one that works, but that requires
dnl changing all callers. See if we can get away with adding -LPREFIX/lib
dnl after -LPREFIX/lib{32,64} so that the compiler will take the first path
dnl that contains the library we're looking for.
AC_DEFUN([INN_SET_LDFLAGS],
[AC_REQUIRE([_INN_LIB_ARCH_NAME])
 AS_IF([test -d "$2/$inn_lib_arch_name"],
    [AS_IF([test x"$3" = x],
        [$1="[$]$1 -L$2/${inn_lib_arch_name} -L$2/lib"],
        [$1="[$]$1 -L$2/${inn_lib_arch_name}/$3 -L$2/lib/$3"])],
    [AS_IF([test x"$3" = x],
        [$1="[$]$1 -L$2/lib"],
        [$1="[$]$1 -L$2/lib/$3"])])
 $1=`AS_ECHO(["[$]$1"]) | sed -e 's/^ *//'`])
