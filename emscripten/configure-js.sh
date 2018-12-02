#!/bin/sh

top=$(pwd)
. $top/emscripten/opts.sh
build=$top/$DIRNAME

export PATH="$top/emscripten/bin:$PATH"

EXTRA_CFLAGS="-DNOTHREAD -I$build/$GLIB_SRC/glib/ -I$build/$GLIB_SRC/ -I$build/$PIXMAN_SRC/pixman -I$build/binaryen/src $OPTS -s ERROR_ON_UNDEFINED_SYMBOLS=0"
EXTRA_LDFLAGS="-L$build/$GLIB_SRC/glib/.libs/ -L$build/$GLIB_SRC/gthread/.libs/ -L$build/stub -L$build/$PIXMAN_SRC/pixman/.libs -L$build/binaryen/lib/ -lbinaryen $OPTS -s ERROR_ON_UNDEFINED_SYMBOLS=0"

export CFLAGS="$EXTRA_CFLAGS"

mkdir -p $build/qemu
cd $build/qemu

emconfigure $top/configure \
    --extra-cflags="$EXTRA_CFLAGS" \
    --extra-ldflags="$EXTRA_LDFLAGS" \
    $*


# Generate helper wrappers
make config-host.h

for _tgt in *-softmmu
do
  tgt=$(basename -s -softmmu $_tgt)
  make -C $tgt-softmmu config-target.h target
  generic_tgt=$(basename $_tgt/target/*)
  echo "Generating helper wrappers for $tgt/$generic_tgt..."
  (
    echo "#define NEED_CPU_H"
    echo "#include \"config-target.h\""
    echo "#include \"config-host.h\""
    echo "#include \"qemu/osdep.h\""
    echo "#include \"exec/helper-head.h\""
    echo "#include \"target/$generic_tgt/cpu.h\""
    echo "#include \"target/$generic_tgt/helper.h\""
    echo "#include \"accel/tcg/tcg-runtime.h\""
  ) | cpp \
        -I $tgt-softmmu -I . \
        -I $top/include -I $top -I $top/tcg -I $top/tcg/tci \
        -I $build/$GLIB_SRC/glib/ -I$build/$GLIB_SRC/ \
        | sed -r 's/(DEF_HELPER)/\n\1/g' \
        | $top/emscripten/gen_helper_wrappers.py > $tgt-softmmu/wrappers.h
done

