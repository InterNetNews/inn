dnl Probe for systemd libraries and installation paths.
dnl
dnl Provides the INN_WITH_SYSTEMD_UNITDIR macro, which adds the
dnl --with-systemdsystemunitdir configure flag, sets the systemdsystemunitdir
dnl substitution variable, and provides the HAVE_SYSTEMD Automake conditional
dnl to use to control whether to install unit files.
dnl
dnl Provides the INN_LIB_SYSTEMD_OPTIONAL macro, which sets SYSTEMD_CFLAGS and
dnl SYSTEMD_LIBS substitution variables if libsystemd is available and defines
dnl HAVE_SD_NOTIFY.  Adds sd-daemon.c to LIBOBJS if libsystemd is not available.
dnl pkg-config support for libsystemd-daemon is required for it to be detected.
dnl
dnl Depends on the Autoconf macros that come with pkg-config.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2015, 2021, 2022 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2013-2014
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Determine the systemd system unit directory, along with a configure flag
dnl to override, and sets @systemdsystemunitdir@.  Provides the Automake
dnl HAVE_SYSTEMD Automake conditional.
AC_DEFUN([INN_WITH_SYSTEMD_UNITDIR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])
 AS_IF([test x"$PKG_CONFIG" = x], [PKG_CONFIG=false])
 AC_ARG_WITH([systemdsystemunitdir],
    [AS_HELP_STRING([--with-systemdsystemunitdir=DIR],
        [Directory for systemd service files])],
    [],
    [with_systemdsystemunitdir=`$PKG_CONFIG --variable=systemdsystemunitdir systemd`])
 AS_IF([test x"$with_systemdsystemunitdir" != xno],
    [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
 AM_CONDITIONAL([HAVE_SYSTEMD],
    [test -n "$with_systemdsystemunitdir" \
        && test x"$with_systemdsystemunitdir" != xno])])

dnl Check for libsystemd, libsystemd-daemon or libelogind and define
dnl SYSTEMD_{CFLAGS,LIBS} if it is available.
AC_DEFUN([INN_LIB_SYSTEMD_OPTIONAL],
[PKG_CHECK_EXISTS([libsystemd],
    [PKG_CHECK_MODULES([SYSTEMD], [libsystemd])
     AC_DEFINE([HAVE_SD_NOTIFY], 1, [Define if sd_notify is available.])],
    [PKG_CHECK_EXISTS([libsystemd-daemon],
        [PKG_CHECK_MODULES([SYSTEMD], [libsystemd-daemon])
         AC_DEFINE([HAVE_SD_NOTIFY], 1,
            [Define if sd_notify is available.])],
        [PKG_CHECK_EXISTS([libelogind],
            [PKG_CHECK_MODULES([SYSTEMD], [libelogind])
             AC_DEFINE([HAVE_SD_NOTIFY], 1,
                [Define if sd_notify is available.])],
            [AC_LIBOBJ([sd-daemon])])])])])
