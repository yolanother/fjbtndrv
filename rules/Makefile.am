udevrulesdir = $(slashlibdir)/udev/rules.d

udevrules_DATA = 95-fjbproxy.rules

EXTRA_DIST = $(udevrules_DATA)

edit = sed \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@VERSION[@]|$(VERSION)|g'

all: $(udevrules_DATA)

%: %.in Makefile
	$(edit) $< >$@

clean:
	rm -f $(udevrules_DATA)
