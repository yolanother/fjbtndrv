SUBDIRS = linux

DEFS += -DLOCALEDIR=\"$(localedir)\" -DSCRIPTDIR=\"$(scriptdir)\"
export DEFS

bin_PROGRAMS = fscrotd fscd

fscrotd_SOURCES = fscrotd.c wacom.c
fscrotd_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscrotd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS)

fscd_SOURCES = fscd.c wacom.c gui-nogui.c
fscd_CPPFLAGS = $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscd_LDADD = $(X11_LIBS) $(LIBWACOMCFG_LIBS)

if BUILD_XFSCD
bin_PROGRAMS += xfscd
xfscd_SOURCES = fscd.c wacom.c gui-xosd.c
xfscd_CPPFLAGS = $(fscd_CPPFLAGS) $(LIBXOSD_CFLAGS)
xfscd_LDADD = $(fscd_LDADD) $(LIBXOSD_LIBS)
endif

EXTRA_DIST = fscd.h gui.h Xwacom.h
