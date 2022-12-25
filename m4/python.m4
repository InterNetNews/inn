dnl Probe for Python properties and, optionally, flags for embedding Python.
dnl
dnl Provides the following macros:
dnl
dnl INN_PROG_PYTHON
dnl     Checks for a specific Python version and sets the PYTHON environment
dnl     variable to the full path, or aborts the configure run if the version
dnl     of Python is not new enough or couldn't be found.  Marks PYTHON as a
dnl     substitution variable.
dnl
dnl     The first argument is a Python version related to the 2.x series (if
dnl     empty, it means that Python 2 is not supported).  The second argument
dnl     is a Python version related to at least the 3.x series (if empty,
dnl     it means that Python 3 or later is not supported).
dnl
dnl INN_PYTHON_CHECK_MODULE
dnl     Checks for the existence of a Python module.  Runs the second argument
dnl     if it is present and the third argument if it is not.
dnl
dnl INN_LIB_PYTHON
dnl     Determines the flags required for embedding Python and sets
dnl     PYTHON_CPPFLAGS and PYTHON_LIBS.
dnl
dnl INN_PROG_PYTHON should generally be called before the other two macros.
dnl If it isn't, the PYTHON environment variable must be set in some other
dnl way.  (It cannot be run automatically via dependencies because it takes a
dnl mandatory minimum version argument, which should be provided by the
dnl calling configure script.)
dnl
dnl For Python 3, this macro uses the sysconfig module to find the compiler
dnl and linker flags to use to embed Python.  If the sysconfig module is not
dnl present, it falls back on using the distutils.sysconfig module shipped
dnl with Python 2.2.0 and later until its removal in Python 3.12.0.
dnl
dnl sysconfig.get_paths() in Python 2.7 as packaged in Debian buster returns
dnl an include path in /usr/local/include, suitable for user-built extensions,
dnl not the path in /usr/include required for including Python.h.  Therefore,
dnl always use distutils.sysconfig for Python 2.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2018, 2021-2022 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2009, 2011, 2015, 2018, 2021
dnl     Julien ÉLIE <julien@trigofacile.com>
dnl Copyright 1998-2003 The Internet Software Consortium
dnl
dnl Permission to use, copy, modify, and distribute this software for any
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

dnl Command used to probe the Python version.
AC_DEFUN([_INN_PROG_PYTHON_CMD], [[
import sys
two_okay = False
three_okay = False
if len(sys.argv) > 1 and sys.argv[1]:
    two_tuple = tuple(map(int, sys.argv[1].split(".")))
    if sys.version_info[0] == 2 and sys.version_info >= two_tuple:
        two_okay = True
if len(sys.argv) > 2 and sys.argv[2]:
    three_tuple = tuple(map(int, sys.argv[2].split(".")))
    if sys.version_info[0] > 2 and sys.version_info >= three_tuple:
        three_okay = True
assert(two_okay or three_okay)
]])

dnl Check for the path to Python and ensure it meets our minimum version
dnl requirement.  The first argument specifies the minimum Python 2 version
dnl and the second argument specifies the minimum Python 3 (or later) version.
dnl At least one constraint must be met.  Honor the $PYTHON environment
dnl variable, if set.
AC_DEFUN([INN_PROG_PYTHON],
[AC_ARG_VAR([PYTHON], [Location of Python interpreter])
 AS_IF([test x"$1" != x],
    [inn_py_expected_ver="$1 (2.x series)"],
    [inn_py_expected_ver=""])
 AS_IF([test x"$2" != x],
    [AS_IF([test x"$1" != x],
        [inn_py_expected_ver="$inn_py_expected_ver or "])
     inn_py_expected_ver="${inn_py_expected_ver}$2"])
 AS_IF([test x"$PYTHON" != x],
    [AS_IF([! test -x "$PYTHON"],
        [AC_MSG_ERROR([Python binary $PYTHON not found])])
     AS_IF([! "$PYTHON" -c '_INN_PROG_PYTHON_CMD()' '$1' '$2' >/dev/null 2>&1],
        [AC_MSG_ERROR([Python $inn_py_expected_ver or greater is required])])],
    [AC_CACHE_CHECK([for Python version $inn_py_expected_ver or later],
        [ac_cv_path_PYTHON],
        [AC_PATH_PROGS_FEATURE_CHECK([PYTHON], [python python3 python2],
            [AS_IF(["$ac_path_PYTHON" -c '_INN_PROG_PYTHON_CMD()' \
                    '$1' '$2' >/dev/null 2>&1],
                [ac_cv_path_PYTHON="$ac_path_PYTHON"
                 ac_path_PYTHON_found=:])])])
     AS_IF([test x"$ac_cv_path_PYTHON" = x],
        [AC_MSG_ERROR([Python $inn_py_expected_ver or greater is required])])
     PYTHON="$ac_cv_path_PYTHON"
     AC_SUBST([PYTHON])])])

dnl Check whether a given Python module can be loaded.  Runs the second
dnl argument if it can, and the third argument if it cannot.
AC_DEFUN([INN_PYTHON_CHECK_MODULE],
[AS_LITERAL_IF([$1], [], [m4_fatal([$0: requires literal arguments])])dnl
 AS_VAR_PUSHDEF([ac_Module], [inn_cv_python_module_$1])dnl
 AC_CACHE_CHECK([for Python module $1], [ac_Module],
    [AS_IF(["$PYTHON" -c 'import $1' >/dev/null 2>&1],
        [AS_VAR_SET([ac_Module], [yes])],
        [AS_VAR_SET([ac_Module], [no])])])
 AS_VAR_IF([ac_Module], [yes], [$2], [$3])
 AS_VAR_POPDEF([ac_Module])])

dnl Determine the flags used for embedding Python.
AC_DEFUN([INN_LIB_PYTHON],
[AC_SUBST([PYTHON_CPPFLAGS])
 AC_SUBST([PYTHON_LIBS])
 AC_MSG_CHECKING([for flags to link with Python])
 AS_IF(["$PYTHON" -c 'import sysconfig' >/dev/null 2>&1],
    [py_include=`$PYTHON -c 'import sysconfig; \
        print(sysconfig.get_paths("posix_prefix").get("include", ""))'`
     py_libdir=`$PYTHON -c 'import sysconfig; \
        print(" -L".join(sysconfig.get_config_vars("LIBDIR")))'`
     py_ldlibrary=`$PYTHON -c 'import sysconfig; \
        print(sysconfig.get_config_vars("LDLIBRARY")@<:@0@:>@)'`
     py_linkage=`$PYTHON -c 'import sysconfig;                     \
        print(" ".join(sysconfig.get_config_vars(                  \
            "LIBS", "LIBC", "LIBM", "LOCALMODLIBS", "BASEMODLIBS", \
            "LINKFORSHARED", "LDFLAGS")))'`],
    [py_include=`$PYTHON -c 'import distutils.sysconfig; \
        print(distutils.sysconfig.get_python_inc())'`
     py_libdir=`$PYTHON -c 'import distutils.sysconfig; \
        print(" -L".join(distutils.sysconfig.get_config_vars("LIBDIR")))'`
     py_ldlibrary=`$PYTHON -c 'import distutils.sysconfig; \
        print(distutils.sysconfig.get_config_vars("LDLIBRARY")@<:@0@:>@)'`
     py_linkage=`$PYTHON -c 'import distutils.sysconfig;           \
        print(" ".join(distutils.sysconfig.get_config_vars(        \
            "LIBS", "LIBC", "LIBM", "LOCALMODLIBS", "BASEMODLIBS", \
            "LINKFORSHARED", "LDFLAGS")))'`])
 PYTHON_CPPFLAGS="-I$py_include"
 py_libpython=`AS_ECHO(["$py_ldlibrary"]) \
    | sed -e 's/^lib//' -e 's/\.@<:@a-z@:>@*$//'`
 PYTHON_LIBS="-L$py_libdir -l$py_libpython $py_linkage"
 PYTHON_LIBS=`AS_ECHO(["$PYTHON_LIBS"]) | sed 's/ @<:@ \\t@:>@*/ /g'`
 AC_MSG_RESULT([$PYTHON_LIBS])
 inn_python_save_CPPFLAGS="$CPPFLAGS"
 inn_python_save_LIBS="$LIBS"
 CPPFLAGS="$PYTHON_CPPFLAGS $CPPFLAGS"
 LIBS="$PYTHON_LIBS $LIBS"
 AC_CHECK_HEADER([Python.h], [],
    [AC_MSG_FAILURE([unable to compile with Python.h])])
 AC_CHECK_FUNC([Py_Initialize], [],
    [AC_MSG_FAILURE([unable to link with Python library])])
 CPPFLAGS="$inn_python_save_CPPFLAGS"
 LIBS="$inn_python_save_LIBS"])
