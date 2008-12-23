fdidir = /usr/share/hal/fdi/information/20thirdparty
dist_fdi_DATA = 30-orientations-fjbtndrv.fdi
dist_fdi_DATA += 30-keymap-fjbtndrv.fdi

XDG_CONFIG_HOME ?= /etc/xdg
xdgautostartdir = $(XDG_CONFIG_HOME)/autostart
dist_xdgautostart_DATA = fscrotd.desktop
if BUILD_FSCD
dist_xdgautostart_DATA += fscd.desktop
endif

fscdscriptsdir = @scriptdir@
dist_fscdscripts_SCRIPTS = rotate-script.sample

EXTRA_DIST = fscrotd.desktop.in fscd.desktop.in dkms.conf.in

edit = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@VERSION[@]|$(VERSION)|g'

%: %.in Makefile
	$(edit) $< >$@

clean::
	rm -f fscrotd.desktop fscd.desktop dkms.conf

