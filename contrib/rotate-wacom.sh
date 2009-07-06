#!/bin/sh

export LANG=C

xsetwacom="`which xsetwacom`" || exit 0
xinput="`which xinput`" || exit 0


usage () {
	echo "usage: $0 [-v] <normal|left|right|inverted>"
	exit 1
}

info () {
	[ "$verbose" ] || return
	echo "$@" >&2
}

find_stylus () {
	$xinput --list --short \
	| cut -d\" -f2 \
	| while read name; do
		if $xinput --list "$name" | grep -q 'Type is Wacom Stylus'; then
			echo "$name"
			break
		fi
	done
}


rotate=
verbose=

if [ $# = 0 ]; then
	set -- "${0##*-}"
fi

while [ "$1" ]; do
	case "$1" in
		-v) verbose=y;;
		n|normal) rotate=0;; # normal
		r|right) rotate=1;; # right
		l|left) rotate=2;; # left
		i|invert|inverted) rotate=3;; # inverted
		*) usage;;
	esac
	shift
done

test "$rotate" || usage

devname="`find_stylus`"
info "device: $devname"

if [ "$devname" ]; then
	info "exec: xsetwacom set \"$devname\" rotate \"$rotate\""
	$xsetwacom set "$devname" rotate "$rotate"
	$xsetwacom set "$devname" xyDefault
else
	echo "Wacom Stylus not found." >&2
fi

exit 0
