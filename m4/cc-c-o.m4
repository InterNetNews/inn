dnl cc-c-o.m4 -- Checks whether -o can be used with -c.
dnl $Id$
dnl
dnl Used to check whether -o can be provided with -c with the chosen compiler.
dnl We need this if we're not using libtool so that object files can be built
dnl in subdirectories.  This macro is stolen shamelessly from the libtool
dnl macros.  Note that we don't use the Autoconf version because it both
dnl checks too much (checking cc as well as the chosen compiler) and it sets a
dnl compiler #define rather than setting a variable we can use.
dnl
dnl $compiler_c_o is set to yes if the compiler supports this and no if not.

AC_DEFUN([INN_PROG_CC_C_O],
[AC_REQUIRE([AC_OBJEXT])
AC_MSG_CHECKING([if $CC supports -c -o file.$ac_objext])
AC_CACHE_VAL([inn_cv_compiler_c_o],
[rm -f -r conftest 2>/dev/null
mkdir conftest
cd conftest
echo "int some_variable = 0;" > conftest.$ac_ext
mkdir out
# According to Tom Tromey, Ian Lance Taylor reported there are C compilers
# that will create temporary files in the current directory regardless of
# the output directory.  Thus, making CWD read-only will cause this test
# to fail, enabling locking or at least warning the user not to do parallel
# builds.
chmod -w .
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -o out/conftest2.$ac_objext"
compiler_c_o=no
if { (eval $ac_compile) 2> out/conftest.err; } \
    && test -s out/conftest2.$ac_objext; then
  # The compiler can only warn and ignore the option if not recognized
  # So say no if there are warnings
  if test -s out/conftest.err; then
    inn_cv_compiler_c_o=no
  else
    inn_cv_compiler_c_o=yes
  fi
else
  # Append any errors to the config.log.
  cat out/conftest.err 1>&AC_FD_CC
  inn_cv_compiler_c_o=no
fi
CFLAGS="$save_CFLAGS"
chmod u+w .
rm -f conftest* out/*
rmdir out
cd ..
rmdir conftest
rm -f -r conftest 2>/dev/null])
compiler_c_o=$inn_cv_compiler_c_o
AC_MSG_RESULT([$compiler_c_o])])
