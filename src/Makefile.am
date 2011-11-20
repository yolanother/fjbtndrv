if BUILD_KERNEL_MODULE
SUBDIRS = linux
endif

bin_PROGRAMS = fjbtndrvd

fjbtndrvd_SOURCES  = fjbtndrv-daemon.c
fjbtndrvd_CPPFLAGS = -ggdb $(DBUS_CFLAGS) $(AM_CPPFLAGS)
fjbtndrvd_LDADD    = $(DBUS_LIBS)

