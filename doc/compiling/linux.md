# Compiling on GNU/Linux

## Dependencies

| Dependency | Version | Commentary |
| ---------- | ------- | ---------- |
| GCC        | 7.5+    | or Clang 7.0.1+ |
| CMake      | 3.5+    |            |
| libjpeg    | -       |            |
| libpng     | -       |            |
| SDL        | 2.x     |            |
| Freetype   | 2.0+    |            |
| SQLite3    | 3+      |            |
| Zlib       | -       |            |
| Zstd       | 1.0+    |            |
| LuaJIT     | 2.0+    | Bundled Lua 5.1 is used if not present |
| GMP        | 5.0.0+  | Bundled mini-GMP is used if not present |
| JsonCPP    | 1.0.0+  | Bundled JsonCPP is used if not present |
| Curl       | 7.56.0+ | Optional   |
| gettext    | -       | Optional   |
| OpenSSL    | 3.0+    | Optional (only libcrypto used) |

For Debian/Ubuntu users:

    sudo apt install g++ make libc6-dev cmake libpng-dev libjpeg-dev libgl1-mesa-dev libsqlite3-dev libogg-dev libvorbis-dev libopenal-dev libcurl4-gnutls-dev libfreetype6-dev zlib1g-dev libgmp-dev libjsoncpp-dev libzstd-dev libluajit-5.1-dev gettext libsdl2-dev

For Fedora users:

    sudo dnf install make automake gcc gcc-c++ kernel-devel cmake libcurl-devel openal-soft-devel libpng-devel libjpeg-devel libvorbis-devel libogg-devel freetype-devel mesa-libGL-devel zlib-devel jsoncpp-devel gmp-devel sqlite-devel luajit-devel leveldb-devel ncurses-devel spatialindex-devel libzstd-devel gettext SDL2-devel

For openSUSE users:

	sudo zypper install gcc gcc-c++ cmake libjpeg8-devel libpng16-devel openal-soft-devel libcurl-devel sqlite3-devel luajit-devel libzstd-devel Mesa-libGL-devel libvorbis-devel freetype2-devel SDL2-devel

For Arch users:

    sudo pacman -S --needed base-devel libcurl-gnutls cmake libpng libjpeg-turbo sqlite libogg libvorbis openal freetype2 jsoncpp gmp luajit leveldb ncurses zstd gettext sdl2

For Alpine users:

    sudo apk add build-base cmake libpng-dev jpeg-dev mesa-dev sqlite-dev libogg-dev libvorbis-dev openal-soft-dev curl-dev freetype-dev zlib-dev gmp-dev jsoncpp-dev luajit-dev zstd-dev gettext sdl2-dev

For Void users:

    sudo xbps-install cmake libpng-devel jpeg-devel mesa sqlite-devel libogg-devel libvorbis-devel libopenal-devel libcurl-devel freetype-devel zlib-devel gmp-devel jsoncpp-devel LuaJIT-devel zstd libzstd-devel gettext SDL2-devel

## Download

You can install Git for easily keeping your copy up to date.
If you don’t want Git, read below on how to get the source without Git.
This is an example for installing Git on Debian/Ubuntu:

    sudo apt install git

For Fedora users:

    sudo dnf install git

For Arch users:

	sudo pacman -S git

For Alpine users:

	sudo apk add git

For Void users:

    sudo xbps-install git

Download source (this is the URL to the latest of source repository, which might not work at all times) using Git:

    git clone --depth 1 https://github.com/luanti-org/luanti
    cd luanti

Download source, without using Git:

    wget https://github.com/luanti-org/luanti/archive/master.tar.gz
    tar xf master.tar.gz
    cd luanti-master

## Build

Build a version that runs directly from the source directory:

    cmake . -DRUN_IN_PLACE=TRUE
    make -j$(nproc)

Run it:

    ./bin/luanti

Run unit tests:

    ./bin/luanti --run-unittests

- Use `cmake . -LH` to see all CMake options and their current state.
- If you want to install it system-wide (or are making a distribution package),
  you will want to use `-DRUN_IN_PLACE=FALSE`.
- You can build a bare server by specifying `-DBUILD_SERVER=TRUE`.
- You can disable the client build by specifying `-DBUILD_CLIENT=FALSE`.
- You can select between Release and Debug build by `-DCMAKE_BUILD_TYPE=<Debug or Release>`.
  - Debug build is slower, but gives much more useful output in a debugger.
