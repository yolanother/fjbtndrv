AC_INIT([fjbtndrv], [git], [http://fjbtndrv.sourceforge.net])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC




dnl ***************************************************************************
dnl Internationalization
dnl ***************************************************************************
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=fjbtndrv
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])

AM_GLIB_GNU_GETTEXT



LT_INIT



AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--enable-debug],
    [Enable debug output (Default: disabled)]),
  [enable_debug=$enableval], [enable_debug=no])
if test x$enable_debug = xyes; then
  AC_DEFINE(DEBUG, [], [debug output])
fi
AM_CONDITIONAL(DEBUG, [test x$enable_debug = xyes])





PKG_CHECK_MODULES(XRANDR,xrandr)

PKG_CHECK_MODULES(XTST,xtst)

PKG_CHECK_MODULES(X11,x11)

PKG_CHECK_MODULES(XI,x11
xi)

PKG_CHECK_MODULES(GLIB,glib-2.0)

PKG_CHECK_MODULES(GIO,gobject-2.0
xi
glib-2.0
gio-2.0)



AC_ARG_ENABLE([kernel-module],
  AS_HELP_STRING([--disable-kernel-module],
    [Disable building the kernel module (Default: enabled)]),
  [enable_kernel_module=$enableval], [enable_kernel_module=yes])
AM_CONDITIONAL(BUILD_KERNEL_MODULE, [test x$enable_kernel_module = xyes])



AC_ARG_ENABLE([xosd],
  AS_HELP_STRING([--disable-xosd],
    [Disable OSD support (Default: auto)]),
  [enable_xosd=$enableval], [enable_xosd=auto])
if test x$enable_xosd != xno; then
  AM_PATH_LIBXOSD([has_xosd=yes], [has_xosd=no])
  if test x$enable_xosd = xyes -a x$has_xosd != xyes; then
    AC_MSG_ERROR([xosd library not found])
  fi
  enable_xosd=$has_xosd
fi
if test x$enable_xosd = xyes; then
  AC_DEFINE(ENABLE_XOSD, [], [enable osd support])
fi
AM_CONDITIONAL(HAVE_XOSD, [test x$enable_xosd = xyes])




AC_OUTPUT([
Makefile
src/Makefile
contrib/Makefile
po/Makefile.in
])



echo "

  F J B T N D R V

  Copyright (C) 2006-2011  Robert Gerlach

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  See file COPYING for more details.


        prefix:         ${prefix}
        debug:          ${enable_debug}
        osd support:    ${enable_xosd}

  Type 'make' to build and then 'sudo make install' to install fjbtndrv tools.
"
