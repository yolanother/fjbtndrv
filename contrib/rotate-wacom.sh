#!/bin/sh

test "$ACTION" = "rotated" || exit 0

case "$ORIENTATION" in
	n*) val=0 ;;
	r*) val=1 ;;
	l*) val=2 ;;
	i*) val=3 ;;
	*)  exit 1 ;;
esac

# xinput installed?
xinput --version || exit 0

export LC_ALL=C

xinput list --short \
| sed 's/^[^[:alnum:]]* \(.*\) *id=.*/\1/p; d' \
| grep -vi 'virtual' \
| while read dev; do
	if xinput list-props "$dev" | grep -q 'Wacom Rotation'; then
		xinput set-prop "$dev" 'Wacom Rotation' $val
	fi
done

exit 0
