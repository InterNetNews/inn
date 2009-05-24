dnl python.m4 -- Probe for the details needed to embed Python.
dnl $Id$
dnl
dnl Defines INN_ARG_PYTHON, which sets up the --with-python command line
dnl argument and also sets various flags needed for embedded Python if it is
dnl requested.

AC_DEFUN([INN_ARG_PYTHON],
[AC_ARG_VAR([PYTHON], [Location of Python interpretor])
AC_ARG_WITH([python],
    [AS_HELP_STRING([--with-python], [Embedded Python module support [no]])],
    [case $withval in
    yes) DO_PYTHON=DO
         AC_DEFINE(DO_PYTHON, 1,
             [Define to compile in Python module support.])
         ;;
    no)  DO_PYTHON=DONT ;;
    *)   AC_MSG_ERROR([invalid argument to --with-python]) ;;
    esac],
    DO_PYTHON=DONT)

dnl A better way of doing this rather than grepping through the Makefile would
dnl be to use distutils.sysconfig, but this module isn't available in older
dnl versions of Python.
if test x"$DO_PYTHON" = xDO ; then
    INN_PATH_PROG_ENSURE([PYTHON], [python])
    AC_MSG_CHECKING([for Python linkage])
    py_prefix=`$PYTHON -c 'import sys; print sys.prefix'`
    py_ver=`$PYTHON -c 'import sys; print sys.version[[:3]]'`
    py_libdir="$py_prefix/lib/python$py_ver"
    PYTHON_CPPFLAGS="-I$py_prefix/include/python$py_ver"
    py_linkage=""
    for py_linkpart in LIBS LIBC LIBM LOCALMODLIBS BASEMODLIBS \
                       LINKFORSHARED LDFLAGS ; do
        py_linkage="$py_linkage "`grep "^${py_linkpart}=" \
                                       $py_libdir/config/Makefile \
                                  | sed -e 's/^.*=//'`
    done
    PYTHON_LIBS="-L$py_libdir/config -lpython$py_ver $py_linkage"
    PYTHON_LIBS=`echo $PYTHON_LIBS | sed -e 's/[ \\t]*/ /g'`
    AC_MSG_RESULT([$py_libdir])
else
    PYTHON_CPPFLAGS=
    PYTHON_LIBS=
fi
AC_SUBST([PYTHON_CPPFLAGS])
AC_SUBST([PYTHON_LIBS])])
