#! /bin/sh

dstdir=`pwd`
srcdir=`dirname $0`
[ "$srcdir" ] || srcdir=.

cd $srcdir
touch AUTHORS NEWS README ChangeLog
autoreconf -v --install || exit 1
cd $dstdir || exit $?
$srcdir/configure "$@"
