#!/bin/bash -eu

EMSDK_GIT="https://github.com/emscripten-core/emsdk.git"
EMSDK_VER=6.0.11

LIBZIP_URL="https://github.com/nih-at/libzip/releases/download/v1.11.4/libzip-1.11.4.tar.xz"
LIBZIP_HASH="8a247f57d1e3e6f6d11413b12a6f28a9d388de110adc0ec608d893180ed7097b"
LIBZIP_FILENAME="libzip-1.11.4.tar.xz"
LIBZIP_DIRNAME="libzip-1.11.4"

ZSTD_GIT="https://github.com/facebook/zstd.git"
ZSTD_REV="f8745da6ff1ad1e7bab384bd1f9d742439278e99" # zstd v1.5.7 release

header() {
  echo "####################################################################"
  echo "# $1"
}

# node or browser
TARGET_RUNTIME="${TARGET_RUNTIME:-node}"
ROOT_DIR="$(pwd)"

if [ ! -f "${ROOT_DIR}/util/ci/build_emscripten.sh" ]; then
  echo "Run from the luanti repository root"
  exit 1
fi

case $TARGET_RUNTIME in
  node)
    RUNTIME_DIRNAME="node"
    ;;
  browser)
    RUNTIME_DIRNAME="web"
    ;;
  *)
    echo "Unknown TARGET_RUNTIME: $TARGET_RUNTIME"
    exit 1
esac

# Sources are shared, but each runtime gets its own build directory, since
# CMake caches the compiler and linker flags on the first configure.
DOWNLOAD_DIR="${ROOT_DIR}/build/downloads"
BUILD_DIR="${ROOT_DIR}/build/emscripten/${RUNTIME_DIRNAME}"
INSTALL_DIR="${BUILD_DIR}/install"

mkdir -p "${BUILD_DIR}"
mkdir -p "${DOWNLOAD_DIR}"

cd "${BUILD_DIR}"

###############################################################################
header "DOWNLOAD AND INSTALL EMSDK"
EMSDK_DIR="${DOWNLOAD_DIR}/emsdk"
if [ ! -d "${EMSDK_DIR}" ]; then
  pushd "${DOWNLOAD_DIR}"
  git clone "${EMSDK_GIT}"
  pushd emsdk
  ./emsdk install "${EMSDK_VER}"
  ./emsdk activate "${EMSDK_VER}"

  # Temporary workaround for https://github.com/emscripten-core/emscripten/issues/27932
  CHECK_TYPE_SIZE_CMAKE="upstream/emscripten/cmake/Modules/CheckTypeSize.cmake"
  if ! grep -q 'oformat=wasm' "${CHECK_TYPE_SIZE_CMAKE}"; then
    echo "CheckTypeSize workaround no longer applies, remove it from build script"
    exit 1
  fi
  sed -i 's/oformat=wasm/oformat=bare/g' "${CHECK_TYPE_SIZE_CMAKE}"

  popd
  popd
fi

###############################################################################
header "SETUP EMSCRIPTEN ENVIRONMENT"
source "${EMSDK_DIR}/emsdk_env.sh"

export CFLAGS="-pthread -fwasm-exceptions"
export CXXFLAGS="-pthread -fwasm-exceptions"
export LDFLAGS="-pthread -fwasm-exceptions -sJSPI -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=32"
export LDFLAGS="$LDFLAGS -sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
export LDFLAGS="$LDFLAGS -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2"
export LDFLAGS_NODEJS="-sNODERAWFS=1 -sNODERAWSOCKETS -sEXIT_RUNTIME=1 -sINITIAL_MEMORY=256MB -sENVIRONMENT=node"
export LDFLAGS_BROWSER="-sOFFSCREENCANVAS_SUPPORT=1 -sWASMFS=1 -sINITIAL_MEMORY=1920MB -sENVIRONMENT=web"

case $TARGET_RUNTIME in
  node)
    export LDFLAGS="$LDFLAGS $LDFLAGS_NODEJS"
    ;;
  browser)
    export LDFLAGS="$LDFLAGS $LDFLAGS_BROWSER"
    ;;
esac

USE_PORTS=""
for port in sdl2 zlib libjpeg libpng freetype ogg vorbis sqlite3 ; do
  USE_PORTS="$USE_PORTS --use-port=$port"
done
export CFLAGS="$CFLAGS $USE_PORTS"
export CXXFLAGS="$CXXFLAGS $USE_PORTS"
export LDFLAGS="$LDFLAGS $USE_PORTS"

###############################################################################
header "DOWNLOAD LIBZIP"
LIBZIP_SOURCE="${DOWNLOAD_DIR}/${LIBZIP_DIRNAME}"
if [ ! -d "${LIBZIP_SOURCE}" ]; then
  pushd "${DOWNLOAD_DIR}"
  if [ ! -f "${LIBZIP_FILENAME}" ]; then
    curl -fL -o "${LIBZIP_FILENAME}.tmp" "${LIBZIP_URL}"
    actual_hash=$(sha256sum "${LIBZIP_FILENAME}.tmp")
    actual_hash=${actual_hash%% *}
    if [[ "$actual_hash" != "${LIBZIP_HASH}" ]]; then
      echo "libzip download hash mismatch: $actual_hash != ${LIBZIP_HASH}"
      rm -f "${LIBZIP_FILENAME}.tmp"
      exit 1
    fi
    mv "${LIBZIP_FILENAME}.tmp" "${LIBZIP_FILENAME}"
  fi
  rm -rf "${LIBZIP_DIRNAME}.tmp"
  mkdir "${LIBZIP_DIRNAME}.tmp"
  tar -xJf "${LIBZIP_FILENAME}" -C "${LIBZIP_DIRNAME}.tmp"
  mv "${LIBZIP_DIRNAME}.tmp/${LIBZIP_DIRNAME}" "${LIBZIP_DIRNAME}"
  rmdir "${LIBZIP_DIRNAME}.tmp"
  popd
fi

###############################################################################
header "DOWNLOAD ZSTD"
ZSTD_SOURCE="${DOWNLOAD_DIR}/zstd"
if [ ! -d "${ZSTD_SOURCE}" ]; then
  pushd "${DOWNLOAD_DIR}"
  git init zstd
  pushd zstd
  git remote add origin "${ZSTD_GIT}"
  git fetch --depth=1 origin "${ZSTD_REV}"
  git checkout --detach FETCH_HEAD
  popd
  popd
fi

###############################################################################
header "BUILD LIBZIP"
if [ ! -f libzip-installed ]; then
  mkdir -p build-libzip
  pushd build-libzip
  emcmake cmake -S "${LIBZIP_SOURCE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TOOLS=OFF \
    -DBUILD_REGRESS=OFF \
    -DBUILD_OSSFUZZ=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DOC=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_LZMA=OFF \
    -DENABLE_ZSTD=OFF \
    -DENABLE_OPENSSL=OFF \
    -DENABLE_GNUTLS=OFF \
    -DENABLE_COMMONCRYPTO=OFF \
    -DENABLE_WINDOWS_CRYPTO=OFF
  emmake make -j$(nproc)
  emmake make install
  popd
  touch libzip-installed
fi

###############################################################################
header "BUILD ZSTD"
if [ ! -f zstd-installed ]; then
  mkdir -p build-zstd
  pushd build-zstd
  emcmake cmake \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DZSTD_BUILD_SHARED=OFF \
    -DZSTD_BUILD_STATIC=ON \
    -DZSTD_BUILD_PROGRAMS=OFF \
    "$ZSTD_SOURCE/build/cmake"
  # zstd breaks with make -j
  emmake make
  emmake make install
  popd
  touch zstd-installed
fi

###############################################################################
header "COMPILING DUMMY LIBRARY"
# Emscripten ports has non-standard names for some libraries, so they won't be
# detected automatically by CMake. But it also includes and links these libraries
# automatically, so all we need to do is use dummy entries to satisfy CMake.
DUMMY_LIBRARY="${BUILD_DIR}/dummy.o"
DUMMY_INCLUDE_DIR="${BUILD_DIR}/dummy_dir"
echo > "${BUILD_DIR}/dummy.c"
emcc $CFLAGS -c "${BUILD_DIR}/dummy.c" -o "${DUMMY_LIBRARY}"
mkdir -p "${DUMMY_INCLUDE_DIR}"

###############################################################################
header "CONFIGURE LUANTI"
emcmake cmake -S "${ROOT_DIR}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DRUN_IN_PLACE=TRUE \
  -DBUILD_DOCUMENTATION=FALSE \
  -DENABLE_UPDATE_CHECKER=0 \
  -DPNG_LIBRARY="$DUMMY_LIBRARY" \
  -DSQLITE3_LIBRARY="$DUMMY_LIBRARY" \
  -DVORBISFILE_LIBRARY="$DUMMY_LIBRARY" \
  -DFREETYPE_LIBRARY="$DUMMY_LIBRARY" \
  -DLIBZIP_LIBRARY="$INSTALL_DIR/lib/libzip.a" \
  -DLIBZIP_INCLUDE_DIR="$INSTALL_DIR/include" \
  -DZSTD_LIBRARY="$INSTALL_DIR/lib/libzstd.a" \
  -DZSTD_INCLUDE_DIR="$INSTALL_DIR/include" \
  -DEGL_LIBRARY="$DUMMY_LIBRARY" \
  -DEGL_INCLUDE_DIR="$DUMMY_INCLUDE_DIR" \
  -DENABLE_LTO=FALSE

###############################################################################
header "MAKE LUANTI"
emmake make -j$(nproc)

header "MAKE INSTALL"
emmake make install

# Copy the result into bin/ because we do RUN_IN_PLACE.
pushd "$ROOT_DIR"
mkdir -p bin
pushd bin
cp -f "$BUILD_DIR/bin/luanti.js" .
cp -f "$BUILD_DIR/bin/luanti.wasm" .
popd
popd

header "FINISHED SUCCESSFULLY"

echo "Run unittests with:"
echo "$EMSDK_NODE" --experimental-wasm-jspi bin/luanti.js --run-unittests
