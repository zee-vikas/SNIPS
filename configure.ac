AC_PREREQ(2.63)
AC_INIT([snips], [0.13])

dnl The following macro will cause all the support files to be put in
dnl the config/ subdirectory.
dnl Unfortunately for a probable autoconf bug that is not yet fixed
dnl even int he development version as of today (2009-02-03), this
dnl requires the install-sh file to be there, even if we're not going
dnl to use it.
AC_CONFIG_AUX_DIR(config)

AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADER([include/config.h])

AC_SUBST([PACKAGE], [AC_PACKAGE_NAME])

BUILDSYS_INIT

dnl For now assume we always want shared libraries, if we want to
dnl support static linking we have also to change the definition of
dnl SNIPS_LIB as used for linking
BUILDSYS_SHARED_LIB
AC_SUBST([SNIPS_LIB], ['$(top_srcdir)/lib/libsnips.so'])

AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug], [Turn on extended debugging code])],,
	      [enable_debug=no])

AS_IF([test "x$enable_debug" = "xyes"],
      [AC_DEFINE([DEBUG], [1], [Enable extended debugging code])])

save_LIBS=$LIBS
LIBS=
AC_SEARCH_LIBS([wgetch], [ncurses curses])
CURSES_LIBS=$LIBS
LIBS=$save_LIBS
AC_SUBST([CURSES_LIBS])

AC_ARG_WITH([hostname],
            [AS_HELP_STRING([--with-hostname=name],
                            [Set the default hostname for the system (default: localhost)])],
            [], [with_hostname=localhost])
AC_SUBST([HOSTNAME], [$with_hostname])

AC_ARG_WITH([operator-address],
            [AS_HELP_STRING([--with-operator-address=email],
                            [Set the default operator address for the system (default: operator@localhost.localdomain)])],
            [], [with_operator_address=operator@localhost.localdomain])
AC_SUBST([OPSMAIL], [$with_operator_address])

AC_ARG_WITH([admin-address],
            [AS_HELP_STRING([--with-admin-address=email],
                            [Set the default admin address for the system (default: admin@localhost.localdomain)])],
            [], [with_admin_address=admin@localhost.localdomain])
AC_SUBST([ADMINMAIL], [$with_admin_address])

dnl Search for tools and options as needed by snips
AC_CANONICAL_HOST
AS_CASE([$host],
	[*-linux-gnu], [PSFLAGS="axw"],
	[AC_MSG_ERROR([$host systems not supported, please add support to configure.ac])])
AC_SUBST([PSFLAGS])

dnl sendpage is optional
AC_PATH_PROG([SENDPAGE], [sendpage])

AC_PATH_PROG([MAIL], [mail])
AS_IF([test "x$MAIL" = "x"],
      [AC_MSG_ERROR([Unable to find mail])])

AC_CONFIG_SUBDIRS([cmu-snmp/snmp])
AC_CONFIG_FILES([buildsys.mk misc.defs.mk])
AC_OUTPUT
