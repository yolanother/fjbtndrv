if BUILD_KERNEL_MODULE
SUBDIRS = linux
endif

bin_PROGRAMS = fjbtndrvd

fjbtndrvd_SOURCES  = fjbtndrv-daemon.c
fjbtndrvd_CPPFLAGS = -ggdb $(DBUS_CFLAGS) $(AM_CPPFLAGS)
fjbtndrvd_LDADD    = $(DBUS_LIBS)

checkxml: fjbtndrv-daemon.xml
	xmllint -valid -noout $<

fjbtndrv-daemon-glue.h: fjbtndrv-daemon.xml
	dbus-binding-tool --mode=glib-server --prefix=fjbtndrv_daemon --output=$@ $<

clean-local:
	rm -f fjbtndrv-daemon-glue.h

