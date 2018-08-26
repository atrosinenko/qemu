#!/bin/bash

top=$(pwd)
. $top/emscripten/opts.sh
build=$top/$DIRNAME

makeargs=-j3
ARGS="$@"

function link_bin() {
  echo "Linking $1..."
  base=$(basename $1)
  ln -sf $1 $base.bc
  time emcc $base.bc $OPTS \
    $build/stub/*.so \
    $build/$GLIB_SRC/glib/.libs/libglib-2.0.so \
    $build/$PIXMAN_SRC/pixman/.libs/libpixman-1.so \
    -o $base.html \
    -s USE_SDL=2 \
    -s USE_ZLIB=1 \
    -s INVOKE_RUN=0 \
    -s ALLOW_MEMORY_GROWTH=1 \
    --shell-file $top/shell.html \
    $ARGS
  ls -lh $base*
}
#    -s INVOKE_RUN=0 \

cd $build/qemu

make $makeargs || exit 1
for bin in *-softmmu/qemu-system-*
do
  if echo $bin | grep -vqF "."
  then
    link_bin $bin
  fi
done
