#!/bin/bash
set -e

topdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ $# -ne 1 ]; then
	echo "Usage: $0 <build directory>"
	exit 1
fi
builddir=$1
mkdir -p $builddir
builddir="$( cd "$builddir" && pwd )"
libdir=$builddir/libs

source $topdir/common.sh

compiler=x86_64-w64-mingw32-clang

if ! command -v "$compiler" >/dev/null; then
	echo "Unable to find $compiler"
	exit 1
fi
toolchain_file=$topdir/toolchain_${compiler%-*}.cmake
echo "Using $toolchain_file"

find_runtime_dlls ${compiler%-*}

# Get stuff
mkdir -p $libdir

cd $libdir
libhost="http://minetest.kitsunemimi.pw"
download "$libhost/llvm/zlib-$zlib_version-win64.zip"
download "$libhost/llvm/zstd-$zstd_version-win64.zip"
download "$libhost/llvm/libogg-$ogg_version-win64.zip"
download "$libhost/llvm/libvorbis-$vorbis_version-win64.zip"
download "$libhost/llvm/curl-$curl_version-win64.zip"
download "$libhost/ucrt/gettext-$gettext_version-win64.zip"
download "$libhost/llvm/freetype-$freetype_version-win64.zip"
download "$libhost/llvm/sqlite3-$sqlite3_version-win64.zip"
download "$libhost/llvm/luajit-$luajit_version-win64.zip"
download "$libhost/llvm/libleveldb-$leveldb_version-win64.zip"
download "$libhost/llvm/openal-soft-$openal_version-win64.zip"
download "$libhost/llvm/libjpeg-$libjpeg_version-win64.zip"
download "$libhost/llvm/libpng-$libpng_version-win64.zip"
download "$libhost/llvm/sdl2-$sdl2_version-win64.zip"

# Set source dir, downloading Minetest as needed
get_sources

# Build the thing
cd $builddir
[ -d build ] && rm -rf build

cmake_args=(
	-DCMAKE_TOOLCHAIN_FILE=$toolchain_file
	-DCMAKE_INSTALL_PREFIX=/tmp
	-DBUILD_CLIENT=1 -DBUILD_SERVER=0
	-DEXTRA_DLL="$runtime_dlls"

	-DENABLE_SOUND=1
	-DENABLE_CURL=1
	-DENABLE_GETTEXT=1
	-DENABLE_LEVELDB=1
)
add_cmake_libs
cmake -S $sourcedir -B build "${cmake_args[@]}"

cmake --build build -j$(nproc)

[ -z "$NO_PACKAGE" ] && cmake --build build --target package

exit 0
# EOF
