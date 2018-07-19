if true
then
	#OPTS="-m32 -O3 -s ALLOW_MEMORY_GROWTH=1"
	#OPTS="-m32 -O3"
	OPTS="-m32 -Os -g2"
	#OPTS="-m32 -O2 -g3 -s ASSERTIONS=1 -s SAFE_HEAP=1 -s ALLOW_MEMORY_GROWTH=1 -s INLINING_LIMIT=1"
	BUILDTYPE=emscripten
	CONFRUNNER=emconfigure
	MAKERUNNER=emmake
	FFIOPT="--host=emscripten-unknown-linux"
else
	OPTS="-m32 -g"
	BUILDTYPE=native
	CONFRUNNER=$(pwd)/emscripten/clangconfigure
	MAKERUNNER=$(pwd)/emscripten/clangconfigure
	FFIOPT=
fi

DIRNAME="build/${BUILDTYPE}_$(echo $OPTS | sed 's/[ =]/_/g')"

GLIB_VERSION=2.56
GLIB_MINOR=1
GLIB_SRC=glib-$GLIB_VERSION.$GLIB_MINOR
PIXMAN_SRC=pixman-0.34.0
GETTEXT_VERSION=0.19.8

echo "OPTS = $OPTS"
echo "DIRNAME = $DIRNAME"

