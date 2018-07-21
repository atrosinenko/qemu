#!/bin/bash -e

. emscripten/opts.sh
export STUBDIR=$(pwd)/emscripten/stubs
export CFLAGS="$OPTS"
export CXXFLAGS="$OPTS"

makeargs="$@"

echo "Now this script will download and build some libraries..."
read -p"Press ENTER..." unused

function build_stub()
{
	test -f stub.built && return
	test -d stub || cp -a "$STUBDIR" stub
	pushd stub
	pwd
	$MAKERUNNER make $makeargs
	popd
	touch stub.built
}

function build_libffi()
{
    test -f libffi.built && return
    test -d libffi || git clone --depth 1 --branch emscripten https://github.com/atrosinenko/libffi.git
    pushd libffi
    test -f configure || ./autogen.sh
    $CONFRUNNER ./configure $FFIOPT
    $MAKERUNNER make $makeargs
    popd
    touch libffi.built
}

function build_gettext()
{
    test -f gettext.built && return
    test -d gettext-$GETTEXT_VERSION || wget https://ftp.gnu.org/pub/gnu/gettext/gettext-$GETTEXT_VERSION.tar.gz
    test -d gettext-$GETTEXT_VERSION || tar -axf gettext-$GETTEXT_VERSION.tar.gz
    pushd gettext-$GETTEXT_VERSION/gettext-runtime
    $CONFRUNNER ./configure \
		--disable-java --disable-native-java \
		--disable-largefile \
		--disable-c++ \
		--disable-threads --disable-acl --disable-openmp --disable-curses \
		--without-emacs
    $MAKERUNNER make $makeargs
    popd
    touch gettext.built
}

function build_glib()
{
    test -f glib.built && return
	test -d $GLIB_SRC || wget https://download.gnome.org/sources/glib/$GLIB_VERSION/$GLIB_SRC.tar.xz
	test -d $GLIB_SRC || tar -axf $GLIB_SRC.tar.xz
    pushd $GLIB_SRC

    curdir=$(pwd)
    export ZLIB_CFLAGS="-s USE_ZLIB=1"
    export ZLIB_LIBS="$ZLIB_CFLAGS"
    export LIBFFI_CFLAGS="-I${curdir}/../libffi/emscripten-unknown-linux-gnu/include"
    export LIBFFI_LIBS="-L${curdir}/../libffi/emscripten-unknown-linux-gnu/.libs -lffi"
	export LIBMOUNT_CFLAGS=
	export LIBMOUNT_LIBS=
    export LIBS="-L${curdir}/../gettext-$GETTEXT_VERSION/gettext-runtime/intl/.libs/ -L${curdir}/../stub/"
    
    test -f ./configure || NOCONFIGURE=1 ./autogen.sh
    $CONFRUNNER ./configure \
		--disable-always-build-tests --disable-installed-tests \
		--disable-largefile --disable-selinux --disable-fam --disable-xattr --disable-libelf \
		--disable-libmount \
		--with-pcre=internal
    sed --in-place '/EVENTFD/ d' config.h
    $MAKERUNNER make $makeargs
    popd
    touch glib.built
}

function build_pixman()
{
    test -f pixman.built && return
    test -d $PIXMAN_SRC || wget https://cairographics.org/releases/$PIXMAN_SRC.tar.gz
    test -d $PIXMAN_SRC || tar -axf $PIXMAN_SRC.tar.gz
    pushd $PIXMAN_SRC
    $CONFRUNNER ./configure
    $MAKERUNNER make -C pixman $makeargs
    popd
    touch pixman.built
}

mkdir -p $DIRNAME
cd $DIRNAME
build_stub
build_libffi
build_gettext
build_glib
build_pixman

