dnl Probe for Python properties and, optionally, flags for embedding Python.
dnl $Id$
dnl
dnl Provides the following macros:
dnl
dnl INN_PROG_PYTHON
dnl     Checks for a specific Python version and sets the PYTHON environment
dnl     variable to the full path, or aborts the configure run if the version
dnl     of Python is not new enough or couldn't be found.
dnl     The first argument is a Python version related to the 2.x series (if
dnl     empty, it means that Python 2 is not supported).  The second argument
dnl     is a Python version related to at least the 3.x series (if empty,
dnl     it means that Python 3 or later is not supported).
dnl
dnl INN_PYTHON_CHECK_MODULE
dnl     Checks for the existence of a Python module and runs provided code
dnl     based on whether or not it was found.
dnl
dnl INN_LIB_PYTHON
dnl     Determines the flags required for embedding Python and sets
dnl     PYTHON_CPPFLAGS and PYTHON_LIBS.
dnl
dnl Defines INN_ARG_PYTHON, which sets up the --with-python command line
dnl argument and also sets various flags needed for embedded Python if it is
dnl requested.
dnl
dnl We use the distutils.sysconfig module shipped with Python 2.2.0 and later
dnl to find the compiler and linker flags to use to embed Python.
dnl We also select libpython in the main library location (a shared library
dnl is present there in Python 2.3.0 and later).

dnl Check for the path to Python and ensure it meets our minimum version
dnl requirement (given as the argument).  Honor the $PYTHON environment
dnl variable, if set.
AC_DEFUN([INN_PROG_PYTHON],
[AC_ARG_VAR([PYTHON], [Location of Python interpreter])
 AS_IF([test x"$1" != x], [py_expected_ver="$1 (in the 2.x series)"],
     [py_expected_ver=""])
 AS_IF([test x"$2" != x],
     [AS_IF([test x"$1" != x], [py_expected_ver="$py_expected_ver or "])
      py_expected_ver="${py_expected_ver}$2"])
 AS_IF([test x"$PYTHON" != x],
    [AS_IF([! test -x "$PYTHON"],
        [AC_MSG_ERROR([Python binary $PYTHON not found])])
     AS_IF([! "$PYTHON" -c 'import sys; assert((False if "$2" == "" else (sys.version_info.major > 2 and sys.version_info >= tuple(int(i) for i in "$2".split(".")))) if "$1" == "" else ((sys.version_info.major == 2 and sys.version_info >= tuple(int(i) for i in "$1".split("."))) if "$2" == "" else ((sys.version_info.major == 2 and sys.version_info >= tuple(int(i) for i in "$1".split("."))) or sys.version_info >= tuple(int(i) for i in "$2".split(".")))))' >/dev/null 2>&1],
        [AC_MSG_ERROR([Python $py_expected_ver or greater is required])])],
    [AC_CACHE_CHECK([for Python version $py_expected_ver or later], [ac_cv_path_PYTHON],
        [AC_PATH_PROGS_FEATURE_CHECK([PYTHON], [python],
            [AS_IF(["$ac_path_PYTHON" -c 'import sys; assert((False if "$2" == "" else (sys.version_info.major > 2 and sys.version_info >= tuple(int(i) for i in "$2".split(".")))) if "$1" == "" else ((sys.version_info.major == 2 and sys.version_info >= tuple(int(i) for i in "$1".split("."))) if "$2" == "" else ((sys.version_info.major == 2 and sys.version_info >= tuple(int(i) for i in "$1".split("."))) or sys.version_info >= tuple(int(i) for i in "$2".split(".")))))' >/dev/null 2>&1],
                [ac_cv_path_PYTHON="$ac_path_PYTHON"
                 ac_path_PYTHON_found=:])])])
     AS_IF([test x"$ac_cv_path_PYTHON" = x],
         [AC_MSG_ERROR([Python $py_expected_ver or greater is required])])
     PYTHON="$ac_cv_path_PYTHON"
     AC_SUBST([PYTHON])])])

dnl Check whether a given Python module can be loaded.  Runs the second argument
dnl if it can, and the third argument if it cannot.
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
 py_include=`$PYTHON -c 'import distutils.sysconfig; \
     print(distutils.sysconfig.get_python_inc())'`
 PYTHON_CPPFLAGS="-I$py_include"
 py_libdir=`$PYTHON -c 'import distutils.sysconfig; \
     print(" -L".join(distutils.sysconfig.get_config_vars("LIBDIR")))'`
 py_ldlibrary=`$PYTHON -c 'import distutils.sysconfig; \
     print(distutils.sysconfig.get_config_vars("LDLIBRARY")@<:@0@:>@)'`
 py_linkage=`$PYTHON -c 'import distutils.sysconfig; \
     print(" ".join(distutils.sysconfig.get_config_vars("LIBS", \
         "LIBC", "LIBM", "LOCALMODLIBS", "BASEMODLIBS", \
         "LINKFORSHARED", "LDFLAGS")))'`
 py_libpython=`echo $py_ldlibrary | sed "s/^lib//" | sed "s/\.@<:@a-z@:>@*$//"`
 PYTHON_LIBS="-L$py_libdir -l$py_libpython $py_linkage"
 PYTHON_LIBS=`echo $PYTHON_LIBS | sed -e 's/[ \\t]*/ /g'`
 AC_MSG_RESULT([$PYTHON_LIBS])])
