#!/bin/sh

export LANG=C

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

test "$ACTION" = "rotated" || exit 0
xsetwacom="`which xsetwacom`" || exit 0
xinput="`which xinput`" || exit 0

case "$ORIENTATION" in
	n*) rotate=0 ;;
	r*) rotate=1 ;;
	l*) rotate=2 ;;
	i*) rotate=3 ;;
	*)  exit 1 ;;
esac

devname="`find_stylus`"
if [ "$devname" ]; then
	$xsetwacom set "$devname" rotate "$rotate"
	$xsetwacom set "$devname" xyDefault
else
	echo "Wacom Stylus not found." >&2
fi

exit 0
