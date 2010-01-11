#! /bin/sh

dstdir=`pwd`
srcdir=`dirname $0`
[ "$srcdir" ] || srcdir=.

cd $srcdir

case "$1" in
[0-9].[0-9]*)
	sed -i "/^AC_INIT/        s/,[^,]*,/, $1,/" configure.in
	sed -i "/^MODULE_VERSION/ s/( *\".*\" *)/(\"$1\")/" src/linux/fsc_btns.c
	shift
	;;
esac

touch AUTHORS NEWS README ChangeLog
autoreconf -v --install || exit 1
cd $dstdir || exit $?
$srcdir/configure "$@"
