if BUILD_KERNEL_MODULE
SUBDIRS = linux
endif

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(X11_CFLAGS) \
	$(XI_CFLAGS) \
	$(XTST_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(LIBXOSD_CFLAGS) 


if DEBUG
AM_CFLAGS = \
	 -Wall \
	 -g
endif

bin_PROGRAMS = fjbproxy \
	fjbdaemon

fjbproxy_SOURCES = \
	fjbtndrv.h \
	fjbtndrv-proxy.h \
	fjbtndrv-proxy.c \
	fjbproxy.c

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

