dnl aux-libs.m4 -- Search for libraries and store them a variable.
dnl $Id$
dnl
dnl Search for a particular library, and if found, add that library to the
dnl specified variable (the third argument) and run the commands given in the
dnl fourth argument, if any.  This is for libraries we don't want to pollute
dnl LIBS with.  Takes the same arguments as AC_SEARCH_LIBS otherwise.
AC_DEFUN([INN_SEARCH_AUX_LIBS],
[inn_save_LIBS=$LIBS
LIBS=${$3}
AC_SEARCH_LIBS($1, $2,
               [$3=$LIBS
                $4], $5, $6)
LIBS=$inn_save_LIBS
AC_SUBST($3)])
