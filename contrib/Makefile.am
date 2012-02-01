EXTRA_DIST = fscrotd.desktop.in fscd.desktop.in dkms.conf.in

fscdscriptsdir = @scriptdir@
dist_fscdscripts_SCRIPTS = rotate-wacom.sh switch-vkeyboard.sh

all: dkms.conf

edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@sbindir[@]|$(sbindir)|g' \
	-e 's|@scriptdir[@]|$(scriptdir)|g' \
	-e 's|@VERSION[@]|$(VERSION)|g'

%: %.in Makefile
	$(edit) $< >$@

clean:
	rm -f dkms.conf
