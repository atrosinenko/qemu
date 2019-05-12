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
    $build/binaryen/lib/libbinaryen.so \
    -o $base.html \
    -s USE_SDL=2 \
    -s USE_ZLIB=1 \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -s INVOKE_RUN=0 \
    -s EXPORTED_FUNCTIONS='["_main","_helper_ret_ldub_mmu","_helper_le_lduw_mmu","_helper_le_ldul_mmu","_helper_le_ldq_mmu","_helper_be_lduw_mmu","_helper_be_ldul_mmu","_helper_be_ldq_mmu","_helper_ret_stb_mmu","_helper_le_stw_mmu","_helper_le_stl_mmu","_helper_le_stq_mmu","_helper_be_stw_mmu","_helper_be_stl_mmu","_helper_be_stq_mmu","_call_helper"]' \
    -s EXTRA_EXPORTED_RUNTIME_METHODS='["addFunction"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    --shell-file $top/shell.html \
    $ARGS
  ls -lh $base*
}
cd $build/qemu

make $makeargs || exit 1
for bin in *-softmmu/qemu-system-*
do
  if echo $bin | grep -vqF "."
  then
    link_bin $bin
  fi
done
