if BUILD_KERNEL_MODULE
SUBDIRS = linux
endif

DEFS += -DLOCALEDIR=\"$(localedir)\" -DSCRIPTDIR=\"$(scriptdir)\"
export DEFS

bin_PROGRAMS = fscrotd

fscrotd_SOURCES = fscrotd.c wacom.c
fscrotd_CPPFLAGS = $(HAL_CFLAGS) $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscrotd_LDADD = $(HAL_LIBS) $(X11_LIBS) $(LIBWACOMCFG_LIBS)

if BUILD_FSCD
bin_PROGRAMS += fscd

fscd_SOURCES = fscd.c wacom.c
fscd_CPPFLAGS = $(X11_CFLAGS) $(LIBWACOMCFG_CFLAGS)
fscd_LDADD = $(X11_LIBS) $(LIBWACOMCFG_LIBS)

if HAVE_XOSD
fscd_SOURCES += gui-xosd.c
fscd_CPPFLAGS += $(LIBXOSD_CFLAGS)
fscd_LDADD += $(LIBXOSD_LIBS)
else
fscd_SOURCES += gui-nogui.c
endif

endif # BUILD_FSCD

EXTRA_DIST = fscd.h gui.h
