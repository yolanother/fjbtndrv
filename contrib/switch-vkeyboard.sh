#!/bin/bash
# Sample script for fscd.
# Copy or link this script to rotate-normal and rotate-tablet.
#
# Copyright (C) 2008-2012 Robert Gerlach <khnz@users.sourceforge.net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2.
################################################################################

oskb_bin=
[ "$oskb_bin" ] || oskb_bin="`type -p cellwriter`"
[ "$oskb_bin" ] || oskb_bin="`type -p onboard`"
[ "$oskb_bin" ] || oskb_bin="`type -p xvkbd`"
[ "$oskb_bin" ] || exit 0

case "$MODE" in

  tablet)
    if test "$ACTION" = "rotated"; then
      "$oskb_bin" &
    fi
    ;;

  normal)
    if test "$ACTION" = "rotating"; then
      pid=$( pgrep -u "$USER" "`basename "$oskb_bin"`" )
      if [ "$pid" ]; then
        kill -TERM $pid
      fi
    fi
    ;;

esac

exit 0
