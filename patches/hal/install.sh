#!/bin/sh
set -e

if ! [ -d /usr/lib/hal -a -d /usr/share/hal ]; then
  echo "hal not installed?"
  exit 1
fi


info () {
  echo " * $*" >&2
}

backup () {
  local src="$1"
  local dst="${src}__backup_fjbtndrv"

  if [ ! -e "$dst" ]; then
    info "backup $src"
    cp -a "$src" "$dst"
  fi
}

install_bin=`type -p install`
_install() {
  local src="$1"
  local dst="$2"

  if [ -f "$src" -a -f "$dst" ]; then
    info "overwrite $src"
    cat "$src" > "$dst"
  elif [ -x "$src" ]; then
    info "install $src (binary)"
    $install_bin --owner="root" --group="root" --mode="755" "$src" "$dst"
  else
    info "install $src"
    $install_bin --owner="root" --group="root" --mode="644" "$src" "$dst"
  fi
}

install () {
  eval local dst="\${$#}"

  while [ $# -gt 1 ]; do
    _install "$1" "$dst"
    shift
  done
}


/etc/dbus-1/event.d/*hal stop
sleep 1

install hal-system-lcd-[gs]et-brightness-fjtbl /usr/lib/hal/scripts/
#install 10-keyboard-policy.fdi /usr/share/hal/fdi/policy/10osvendor/10-keyboard-policy.fdi
install 20-fjtblpc-lcd.fdi /usr/share/hal/fdi/information/20thirdparty/

if [ -x /usr/lib/hal/hald-addon-input ]; then
  echo " * hal >= 0.5.10 (addon-input)"
  aipt="/usr/lib/hal/hald-addon-input"
elif [ -x /usr/lib/hal/hald-addon-keyboard ]; then
  echo " * hal <= 0.5.9 (addon-keyboard)"
  aipt="/usr/lib/hal/hald-addon-keyboard"
else
  echo " * input addon not found"
  exit 1
fi

if grep -q '\<fn\>' $aipt &&
   grep -q '\<display-toggle\>' $aipt &&
   grep -q 'ButtonRepeat' $aipt
then
  echo " * input addon is up to date"
else
  echo "building hald-addon-input"
  hal_user=`getent passwd haldaemon | awk -F: '{ print $3 }'`
  hal_group=`getent group haldaemon | awk -F: '{ print $3 }'`
  echo "USER:$hal_user GROUP:$hal_group"
  gcc -DHAL_USER=$hal_user -DHAL_GROUP=$hal_group \
      -I/usr/include/dbus-1.0 \
      -o ${aipt##*/} addon-input.c \
      -lhal -ldbus-1

  backup $aipt
  install ${aipt##*/} $aipt
fi

sleep 1
/etc/dbus-1/event.d/*hal start

