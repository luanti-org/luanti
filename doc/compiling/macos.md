# Compiling on MacOS

## Requirements

- [Homebrew](https://brew.sh/)
- [Git](https://git-scm.com/downloads)

Install dependencies with homebrew:

```
brew install cmake freetype gettext gmp hiredis jpeg-turbo jsoncpp leveldb libogg libpng libvorbis luajit zstd gettext
```

## Download

Download source (this is the URL to the latest of source repository, which might not work at all times) using Git:

```bash
git clone --depth 1 https://github.com/luanti-org/luanti luanti
cd luanti
```

## Building for personal usage

### Build

```bash
mkdir build
cd build

cmake .. \
    -DCMAKE_FIND_FRAMEWORK=LAST \
    -DCMAKE_INSTALL_PREFIX=../build/macos/ \
    -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE

make -j$(sysctl -n hw.logicalcpu)
make install

# Apple Silicon (M1/M2) Macs w/ MacOS >= BigSur signature for local run
codesign --force --deep -s - --entitlements ../misc/macos/entitlements/debug.entitlements macos/luanti.app
```

If you are using LuaJIT with `MAP_JIT` support use `debug_map_jit.entitlements`.

### Run

```
open ./build/macos/luanti.app
```

## Building for distribution

### Generate Xcode project

```bash
mkdir build
cd build

cmake .. \
    -DCMAKE_FIND_FRAMEWORK=LAST \
    -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
    -DFREETYPE_LIBRARY=/path/to/lib/dir/libfreetype.a \
    -DGETTEXT_INCLUDE_DIR=/path/to/include/dir \
    -DGETTEXT_LIBRARY=/path/to/lib/dir/libintl.a \
    -DLUA_LIBRARY=/path/to/lib/dir/libluajit-5.1.a \
    -DOGG_LIBRARY=/path/to/lib/dir/libogg.a \
    -DVORBIS_LIBRARY=/path/to/lib/dir/libvorbis.a \
    -DVORBISFILE_LIBRARY=/path/to/lib/dir/libvorbisfile.a \
    -DZSTD_LIBRARY=/path/to/lib/dir/libzstd.a \
    -DGMP_LIBRARY=/path/to/lib/dir/libgmp.a \
    -DJSON_LIBRARY=/path/to/lib/dir/libjsoncpp.a \
    -DENABLE_LEVELDB=OFF \
    -DENABLE_POSTGRESQL=OFF \
    -DENABLE_REDIS=OFF \
    -DJPEG_LIBRARY=/path/to/lib/dir/libjpeg.a \
    -DPNG_LIBRARY=/path/to/lib/dir/libpng.a \
    -DCMAKE_EXE_LINKER_FLAGS=-lbz2\
    -GXcode
```

If you are using LuaJIT with `MAP_JIT` support add `-DXCODE_CODE_SIGN_ENTITLEMENTS=../misc/macos/entitlements/release_map_jit.entitlements`.

*WARNING:* You have to regenerate Xcode project if you switch commit, branch or etc.

### Build and Run

* Open generated Xcode project
* Select scheme `luanti`
* Configure signing Team etc.
* Run Build command
* Open application from `build/build/Debug/` directory or run it from Xcode

### Archive and Run

* Open generated Xcode project
* Select scheme `luanti`
* Configure signing Team etc.
* Run Build command
* Open application archive in finder, go into it, copy application and test it.

