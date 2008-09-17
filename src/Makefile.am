SUBDIRS = linux

DEFS += -DLOCALEDIR=\"$(localedir)\" -DSCRIPTDIR=\"$(scriptdir)\"
export DEFS

bin_PROGRAMS = fscd xfscd

AM_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS) $(LIBXOSD_CFLAGS)

fscd_SOURCES = fscd-base.c fscd-nogui.c
fscd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS)

if BUILD_XFSCD
xfscd_SOURCES = fscd-base.c fscd-xosd.c
xfscd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS) $(LIBXOSD_LIBS)
endif

EXTRA_DIST = fscd-base.h fscd-gui.h Xwacom.h
