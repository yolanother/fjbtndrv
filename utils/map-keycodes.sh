#!/bin/sh

# start with
keycode=240

function in_range() {
	[ $1 -ge $keycode -a $1 -le $[$keycode+14] ]
}

{
  # clear
  xmodmap -pk \
  | grep '\<XF86Launch.\>' \
  | cut -b -8 \
  | while read k; do in_range $k || echo "keycode $k ="; done

  # set
  for key in 1 2 3 4 5 6 7 8 9 0 A B C D E; do
	  echo "keycode $keycode = XF86Launch$key"
	  keycode=$[ $keycode + 1 ]
  done
} | xmodmap -

