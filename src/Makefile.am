SUBDIRS = linux

DEFS += -DLOCALEDIR=\"$(localedir)\" -DSCRIPTDIR=\"$(scriptdir)\"
export DEFS

bin_PROGRAMS = fscrotd fscd

fscrotd_SOURCES = fscrotd.c fscd-wacom.c
fscrotd_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscrotd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS)

fscd_SOURCES = fscd-base.c fscd-wacom.c fscd-nogui.c
fscd_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS)

if BUILD_XFSCD
bin_PROGRAMS += xfscd
xfscd_SOURCES = fscd-base.c fscd-wacom.c fscd-xosd.c
xfscd_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS) $(LIBXOSD_CFLAGS)
xfscd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS) $(LIBXOSD_LIBS)
endif

EXTRA_DIST = fscd-base.h fscd-gui.h Xwacom.h
