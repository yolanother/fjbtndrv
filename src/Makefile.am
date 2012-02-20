if BUILD_KERNEL_MODULE
SUBDIRS = linux
endif

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"

if DEBUG
AM_CFLAGS = \
	 -Wall \
	 -g
endif

bin_PROGRAMS = fjbdaemon
sbin_PROGRAMS = fjbproxy

fjbproxy_SOURCES = \
	fjbtndrv.h \
	fjbproxy.c

fjbproxy_CFLAGS = \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

fjbproxy_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

fjbdaemon_SOURCES = \
	fjbtndrv.h \
	fjbtndrv-display.h \
	fjbtndrv-display.c \
	fjbtndrv-device.h \
	fjbtndrv-device.c \
	fjbtndrv-backlight.h \
	fjbtndrv-backlight.c \
	fjbtndrv-osd.h \
	fjbtndrv-osd.c \
	fjbdaemon.c

fjbdaemon_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(X11_LIBS) \
	$(XI_LIBS) \
	$(XTST_LIBS) \
	$(XRANDR_LIBS) \
	$(LIBXOSD_LIBS)

