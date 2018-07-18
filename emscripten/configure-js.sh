#!/bin/sh

top=$(pwd)
. $top/emscripten/opts.sh
build=$top/$DIRNAME

export PATH="$top/emscripten/bin:$PATH"

EXTRA_CFLAGS="-DNOTHREAD -I$build/$GLIB_SRC/glib/ -I$build/$GLIB_SRC/ -I$build/$PIXMAN_SRC/pixman $OPTS"
EXTRA_LDFLAGS="-L$build/$GLIB_SRC/glib/.libs/ -L$build/$GLIB_SRC/gthread/.libs/ -L$build/stub -L$build/$PIXMAN_SRC/pixman/.libs $OPTS"

export CFLAGS="$EXTRA_CFLAGS"

mkdir -p $build/qemu
cd $build/qemu

exec emconfigure $top/configure \
	--extra-cflags="$EXTRA_CFLAGS" \
	--extra-ldflags="$EXTRA_LDFLAGS" \
	$*
