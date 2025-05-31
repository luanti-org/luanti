#!/bin/bash -e

cmake .. \
	-DCMAKE_FIND_FRAMEWORK=LAST \
	-DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
	-DUSE_SDL2_STATIC=TRUE \
	-DSDL2_INCLUDE_DIRS=/opt/homebrew/include/SDL2 \
	-DFREETYPE_LIBRARY=/opt/homebrew/lib/libfreetype.a \
	-DGETTEXT_INCLUDE_DIR=/opt/homebrew/include \
	-DGETTEXT_LIBRARY=/opt/homebrew/lib/libintl.a \
	-DLUA_LIBRARY=/opt/homebrew/lib/libluajit-5.1.a \
	-DOGG_LIBRARY=/opt/homebrew/lib/libogg.a \
	-DVORBIS_LIBRARY=/opt/homebrew/lib/libvorbis.a \
	-DVORBISFILE_LIBRARY=/opt/homebrew/lib/libvorbisfile.a \
	-DZSTD_LIBRARY=/opt/homebrew/lib/libzstd.a \
	-DGMP_LIBRARY=/opt/homebrew/lib/libgmp.a \
	-DENABLE_SYSTEM_JSONCPP=OFF \
	-DENABLE_LEVELDB=OFF \
	-DENABLE_POSTGRESQL=OFF \
	-DENABLE_REDIS=OFF \
	-DJPEG_LIBRARY=/opt/homebrew/lib/libjpeg.a \
	-DPNG_LIBRARY=/opt/homebrew/lib/libpng.a \
	-DCMAKE_EXE_LINKER_FLAGS=-lbz2\
	-GXcode
xcodebuild -project luanti.xcodeproj -scheme luanti -configuration Release build
xcodebuild -project luanti.xcodeproj -scheme luanti -archivePath ./luanti.xcarchive archive
