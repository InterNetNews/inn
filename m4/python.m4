dnl python.m4 -- Probe for the details needed to embed Python.
dnl $Id$
dnl
dnl Defines INN_ARG_PYTHON, which sets up the --with-python command line
dnl argument and also sets various flags needed for embedded Python if it is
dnl requested.
dnl
dnl We use the distutils.sysconfig module shipped with Python 2.2.0 and later
dnl to find the compiler and linker flags to use to embed Python.

AC_DEFUN([INN_ARG_PYTHON],
[AC_ARG_VAR([PYTHON], [Location of Python interpreter])
 AC_ARG_WITH([python],
    [AS_HELP_STRING([--with-python], [Embedded Python module support [no]])],
    [AS_CASE([$withval],
     [yes], [DO_PYTHON=DO
             AC_DEFINE([DO_PYTHON], [1],
                [Define to compile in Python module support.])],
     [no], [DO_PYTHON=DONT],
     [AC_MSG_ERROR([invalid argument to --with-python])])],
    [DO_PYTHON=DONT])
 AS_IF([test x"$DO_PYTHON" = xDO],
    [INN_PATH_PROG_ENSURE([PYTHON], [python])
     AC_MSG_CHECKING([for Python linkage])
     py_include=`$PYTHON -c 'import distutils.sysconfig; \
         print(distutils.sysconfig.get_python_inc())'`
     PYTHON_CPPFLAGS="-I$py_include"
     py_ver=`$PYTHON -c 'import sys; print(sys.version[[:3]])'`
     py_libdir=`$PYTHON -c 'import distutils.sysconfig; \
         print(distutils.sysconfig.get_python_lib(False, True))'`
     py_linkage=`$PYTHON -c 'import distutils.sysconfig; \
         print(" ".join(distutils.sysconfig.get_config_vars("LIBS", \
             "LIBC", "LIBM", "LOCALMODLIBS", "BASEMODLIBS", \
             "LINKFORSHARED", "LDFLAGS")))'`
     py_configdir=`$PYTHON -c 'import distutils.sysconfig; \
         print(distutils.sysconfig.get_config_var("LIBPL"))'`
     PYTHON_LIBS="-L$py_configdir -lpython$py_ver $py_linkage"
     PYTHON_LIBS=`echo $PYTHON_LIBS | sed -e 's/[ \\t]*/ /g'`
     AC_MSG_RESULT([$py_libdir])],
    [PYTHON_CPPFLAGS=
     PYTHON_LIBS=])
 AC_SUBST([PYTHON_CPPFLAGS])
 AC_SUBST([PYTHON_LIBS])])
