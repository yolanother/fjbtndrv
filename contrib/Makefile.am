udevrulesdir = /etc/udev/rules.d
dist_udevrules_DATA = 65-fsc_btns.rules

fdidir = /usr/share/hal/fdi/information/20thirdparty
dist_fdi_DATA = 30-orientations-fjbtndrv.fdi
dist_fdi_DATA += 30-keymap-fjbtndrv.fdi

if HAVE_FUJITSU_LAPTOP_MODULE
else
dist_fdi_DATA += 10-laptop-panel-fjtbl.fdi
halscriptsdir = /usr/lib/hal/scripts
dist_halscripts_SCRIPTS = \
  hal-system-lcd-get-brightness-fjtbl \
  hal-system-lcd-set-brightness-fjtbl
endif

XDG_CONFIG_HOME ?= /etc/xdg
xdgautostartdir = $(XDG_CONFIG_HOME)/autostart
dist_xdgautostart_DATA = fscd.desktop

fscdscriptsdir = @scriptdir@
dist_fscdscripts_SCRIPTS = fscd-rotate.sample

EXTRA_DIST = fscd.desktop.in

%: %.in Makefile
	sed -e 's|@bindir[@]|$(bindir)|g' $< >$@
