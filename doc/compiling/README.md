# Compiling Luanti

- [Compiling on GNU/Linux](linux.md)
- [Compiling on Windows](windows.md)
- [Compiling on MacOS](macos.md)


## CMake options

General options and their default values:

    BUILD_CLIENT=TRUE          - Build Luanti client
    BUILD_SERVER=FALSE         - Build Luanti server
    BUILD_UNITTESTS=TRUE       - Build unittest sources
    BUILD_BENCHMARKS=FALSE     - Build benchmark sources
    BUILD_DOCUMENTATION=TRUE   - Build doxygen documentation
    CMAKE_BUILD_TYPE=Release   - Type of build (Release vs. Debug)
        Release                - Release build
        Debug                  - Debug build
        SemiDebug              - Partially optimized debug build
        RelWithDebInfo         - Release build with debug information
        MinSizeRel             - Release build with -Os passed to compiler to make executable as small as possible
    PRECOMPILE_HEADERS=FALSE   - Precompile some headers (experimental; requires CMake 3.16 or later)
    PRECOMPILED_HEADERS_PATH=  - Path to a file listing all headers to precompile (default points to src/precompiled_headers.txt)
    USE_SDL2=TRUE              - Build with SDL2; Enables IrrlichtMt device SDL2
    USE_SDL2_STATIC=TRUE       - Links with SDL2::SDL2-static instead of SDL2::SDL2
    ENABLE_CURL=ON             - Build with cURL; Enables use of online mod repo, public serverlist and remote media fetching via http
    ENABLE_CURSES=ON           - Build with (n)curses; Enables a server side terminal (command line option: --terminal)
    ENABLE_GETTEXT=ON          - Build with Gettext; Allows using translations
    ENABLE_LEVELDB=ON          - Build with LevelDB; Enables use of LevelDB map backend
    ENABLE_POSTGRESQL=ON       - Build with libpq; Enables use of PostgreSQL map backend (PostgreSQL 9.5 or greater recommended)
    ENABLE_REDIS=ON            - Build with libhiredis; Enables use of Redis map backend
    ENABLE_SPATIAL=ON          - Build with LibSpatial; Speeds up AreaStores
    ENABLE_OPENSSL=ON          - Build with OpenSSL; Speeds up SHA1 and SHA2 hashing
    ENABLE_SOUND=ON            - Build with OpenAL, libogg & libvorbis; in-game sounds
    ENABLE_LTO=<varies>        - Build with IPO/LTO optimizations (smaller and more efficient than regular build)
    ENABLE_LUAJIT=ON           - Build with LuaJIT (much faster than non-JIT Lua)
    ENABLE_PROMETHEUS=OFF      - Build with Prometheus metrics exporter (listens on tcp/30000 by default)
    ENABLE_SYSTEM_GMP=ON       - Use GMP from system (much faster than bundled mini-gmp)
    ENABLE_SYSTEM_JSONCPP=ON   - Use JsonCPP from system
    RUN_IN_PLACE=FALSE         - Create a portable install (worlds, settings etc. in current directory)
    ENABLE_UPDATE_CHECKER=TRUE - Whether to enable update checks by default
    INSTALL_DEVTEST=FALSE      - Whether the Development Test game should be installed alongside Luanti
    USE_GPROF=FALSE            - Enable profiling using GProf
    BUILD_WITH_TRACY=FALSE     - Fetch and build with the Tracy profiler client
    FETCH_TRACY_GIT_TAG=master - Git tag for fetching Tracy client. Match with your server (gui) version
    VERSION_EXTRA=             - Text to append to version (e.g. VERSION_EXTRA=foobar -> Luanti 5.10.0-foobar)

Library specific options:

    SDL2_DLL                        - Only if building with SDL2 on Windows; path to libSDL2.dll
    SDL2_INCLUDE_DIRS               - Only if building with SDL2; directory where SDL.h is located
    SDL2_LIBRARIES                  - Only if building with SDL2; path to libSDL2.a/libSDL2.so/libSDL2.lib
    CURL_DLL                        - Only if building with cURL on Windows; path to libcurl.dll
    CURL_INCLUDE_DIR                - Only if building with cURL; directory where curl.h is located
    CURL_LIBRARY                    - Only if building with cURL; path to libcurl.a/libcurl.so/libcurl.lib
    EXTRA_DLL                       - Only on Windows; optional paths to additional DLLs that should be packaged
    FREETYPE_INCLUDE_DIR_freetype2  - Directory that contains files such as ftimage.h
    FREETYPE_INCLUDE_DIR_ft2build   - Directory that contains ft2build.h
    FREETYPE_LIBRARY                - Path to libfreetype.a/libfreetype.so/freetype.lib
    FREETYPE_DLL                    - Only on Windows; path to libfreetype-6.dll
    GETTEXT_DLL                     - Only when building with gettext on Windows; paths to libintl + libiconv DLLs
    GETTEXT_INCLUDE_DIR             - Only when building with gettext; directory that contains libintl.h
    GETTEXT_LIBRARY                 - Optional/platform-dependent with gettext; path to libintl.so/libintl.dll.a
    GETTEXT_MSGFMT                  - Only when building with gettext; path to msgfmt/msgfmt.exe
    GMP_INCLUDE_DIR                 - Directory that contains gmp.h
    GMP_LIBRARY                     - Path to libgmp.a/libgmp.so/libgmp.lib
    ICONV_LIBRARY                   - Optional/platform-dependent; path to libiconv.so/libiconv.dylib
    JPEG_DLL                        - Only on Windows; path to libjpeg.dll
    JPEG_INCLUDE_DIR                - Directory that contains jpeg.h
    JPEG_LIBRARY                    - Path to libjpeg.a/libjpeg.so/libjpeg.lib
    JSON_INCLUDE_DIR                - Directory that contains json/allocator.h
    JSON_LIBRARY                    - Path to libjson.a/libjson.so/libjson.lib
    LEVELDB_INCLUDE_DIR             - Only when building with LevelDB; directory that contains db.h
    LEVELDB_LIBRARY                 - Only when building with LevelDB; path to libleveldb.a/libleveldb.so/libleveldb.dll.a
    LEVELDB_DLL                     - Only when building with LevelDB on Windows; path to libleveldb.dll
    PostgreSQL_INCLUDE_DIR          - Only when building with PostgreSQL; directory that contains libpq-fe.h
    PostgreSQL_LIBRARY              - Only when building with PostgreSQL; path to libpq.a/libpq.so/libpq.lib
    REDIS_INCLUDE_DIR               - Only when building with Redis; directory that contains hiredis.h
    REDIS_LIBRARY                   - Only when building with Redis; path to libhiredis.a/libhiredis.so
    SPATIAL_INCLUDE_DIR             - Only when building with LibSpatial; directory that contains spatialindex/SpatialIndex.h
    SPATIAL_LIBRARY                 - Only when building with LibSpatial; path to libspatialindex.so/spatialindex-32.lib
    LUA_INCLUDE_DIR                 - Only if you want to use LuaJIT; directory where luajit.h is located
    LUA_LIBRARY                     - Only if you want to use LuaJIT; path to libluajit.a/libluajit.so
    OGG_DLL                         - Only if building with sound on Windows; path to libogg.dll
    OGG_INCLUDE_DIR                 - Only if building with sound; directory that contains an ogg directory which contains ogg.h
    OGG_LIBRARY                     - Only if building with sound; path to libogg.a/libogg.so/libogg.dll.a
    OPENAL_DLL                      - Only if building with sound on Windows; path to OpenAL32.dll
    OPENAL_INCLUDE_DIR              - Only if building with sound; directory where al.h is located
    OPENAL_LIBRARY                  - Only if building with sound; path to libopenal.a/libopenal.so/OpenAL32.lib
    PNG_DLL                         - Only on Windows; path to libpng.dll
    PNG_INCLUDE_DIR                 - Directory that contains png.h
    PNG_LIBRARY                     - Path to libpng.a/libpng.so/libpng.lib
    PROMETHEUS_PULL_LIBRARY         - Only if building with prometheus; path to prometheus-cpp-pull
    PROMETHEUS_CORE_LIBRARY         - Only if building with prometheus; path to prometheus-cpp-core
    PROMETHEUS_CPP_INCLUDE_DIR      - Only if building with prometheus; directory where prometheus/counter.h is located
    SQLITE3_INCLUDE_DIR             - Directory that contains sqlite3.h
    SQLITE3_LIBRARY                 - Path to libsqlite3.a/libsqlite3.so/sqlite3.lib
    VORBISFILE_LIBRARY              - Only if building with sound; path to libvorbisfile.a/libvorbisfile.so/libvorbisfile.dll.a
    VORBIS_DLL                      - Only if building with sound on Windows; paths to vorbis DLLs
    VORBIS_INCLUDE_DIR              - Only if building with sound; directory that contains a directory vorbis with vorbisenc.h inside
    VORBIS_LIBRARY                  - Only if building with sound; path to libvorbis.a/libvorbis.so/libvorbis.dll.a
    ZLIB_DLL                        - Only on Windows; path to zlib1.dll
    ZLIB_INCLUDE_DIR                - Directory that contains zlib.h
    ZLIB_LIBRARY                    - Path to libz.a/libz.so/zlib.lib
    ZSTD_DLL                        - Only on Windows; path to libzstd.dll
    ZSTD_INCLUDE_DIR                - Directory that contains zstd.h
    ZSTD_LIBRARY                    - Path to libzstd.a/libzstd.so/ztd.lib
