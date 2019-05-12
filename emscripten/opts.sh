if true
then
	#OPTS="-m32 -O3 -s ALLOW_MEMORY_GROWTH=1"
	#OPTS="-m32 -O3"
	OPTS="-m32 -Os -g2"
	#OPTS="-m32 -O2 -g3 -s ASSERTIONS=1 -s SAFE_HEAP=1 -s ALLOW_MEMORY_GROWTH=1 -s INLINING_LIMIT=1"
	BUILDTYPE=emscripten
	CONFRUNNER=emconfigure
	CMAKERUNNER=emcmake
	MAKERUNNER=emmake
	FFIPORT=emscripten-unknown-linux-gnu
	CROSS_OPT="--host=emscripten-unknown-linux"
	EXTRA_LDOPTS="-s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ERROR_ON_MISSING_LIBRARIES=0"
	export ZLIB_CFLAGS="-s USE_ZLIB=1"
else
	OPTS="-m32 -Og -g3"
	BUILDTYPE=native
	CONFRUNNER=$(pwd)/emscripten/clangconfigure
	CMAKERUNNER=$(pwd)/emscripten/clangconfigure
	MAKERUNNER=$(pwd)/emscripten/clangconfigure
	FFIPORT=i386-pc-linux-gnu/
	CROSS_OPT="--host=i386-pc-linux"
#	EXTRA_LDOPTS="-Wl,--unresolved-symbols=ignore-in-object-files"
fi

DIRNAME="build/${BUILDTYPE}_$(echo $OPTS | sed 's/[ =]/_/g')"

GLIB_VERSION=2.56
GLIB_MINOR=1
GLIB_SRC=glib-$GLIB_VERSION.$GLIB_MINOR
PIXMAN_SRC=pixman-0.38.4
GETTEXT_VERSION=0.20

echo "OPTS = $OPTS"
echo "DIRNAME = $DIRNAME"

