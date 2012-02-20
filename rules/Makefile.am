udevrulesdir = $(slashlibdir)/udev/rules.d

udevrules_DATA = 95-fjbtndrv.rules

EXTRA_DIST = $(udevrules_DATA)

MAINTAINERCLEANFILES = $(udevrules_DATA)

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@VERSION[@]|$(VERSION)|g'

all: $(udevrules_DATA)

%: %.in Makefile
	$(edit) $< >$@

install: install-am
	udevadm control --reload-rules

clean:
	rm -f $(udevrules_DATA)

