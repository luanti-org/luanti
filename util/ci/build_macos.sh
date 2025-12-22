#!/bin/bash -e

sudo xcode-select -s /Applications/Xcode_${xcodever}.app/Contents/Developer
sdkroot="$(realpath $(xcrun --sdk macosx --show-sdk-path)/../MacOSX${osver}.sdk)"
export CMAKE_PREFIX_PATH=${DEPS_DIR}
export SDKROOT="$sdkroot"

THIS_DIR=$(pwd)

# common args
cmake_args=(
	-DCMAKE_OSX_DEPLOYMENT_TARGET=$osver
	-DCMAKE_FIND_FRAMEWORK=LAST
	-DCMAKE_OSX_ARCHITECTURES=${arch}
	-DCMAKE_INSTALL_PREFIX=${THIS_DIR}/macos/
	-DCMAKE_BUILD_TYPE=${build_type}
	-DCMAKE_FIND_FRAMEWORK=LAST
	-DRUN_IN_PLACE=FALSE
	-DENABLE_GETTEXT=TRUE
	-DJPEG_LIBRARY=${DEPS_DIR}/lib/libjpeg.a
	-DJPEG_INCLUDE_DIR=${DEPS_DIR}/include
	-DPNG_LIBRARY=${DEPS_DIR}/lib/libpng.a
	-DPNG_PNG_INCLUDE_DIR=${DEPS_DIR}/include
	-DFREETYPE_LIBRARY=${DEPS_DIR}/lib/libfreetype.a
	-DFREETYPE_INCLUDE_DIRS=${DEPS_DIR}/include/freetype2
	-DGETTEXT_INCLUDE_DIR=${DEPS_DIR}/include
	-DGETTEXT_LIBRARY=${DEPS_DIR}/lib/libintl.a
	-DLUA_LIBRARY=${DEPS_DIR}/lib/libluajit-5.1.a
	-DLUA_INCLUDE_DIR=${DEPS_DIR}/include/luajit-2.1
	-DOGG_LIBRARY=${DEPS_DIR}/lib/libogg.a
	-DOGG_INCLUDE_DIR=${DEPS_DIR}/include
	-DVORBIS_LIBRARY=${DEPS_DIR}/lib/libvorbis.a
	-DVORBISFILE_LIBRARY=${DEPS_DIR}/lib/libvorbisfile.a
	-DVORBIS_INCLUDE_DIR=${DEPS_DIR}/include
	-DOPENAL_LIBRARY=${DEPS_DIR}/lib/libopenal.a
	-DOPENAL_INCLUDE_DIR=${DEPS_DIR}/include/AL
	-DZSTD_LIBRARY=${DEPS_DIR}/lib/libzstd.a
	-DZSTD_INCLUDE_DIR=${DEPS_DIR}/include
	-DGMP_LIBRARY=${DEPS_DIR}/lib/libgmp.a
	-DGMP_INCLUDE_DIR=${DEPS_DIR}/include
	-DJSON_LIBRARY=${DEPS_DIR}/lib/libjsoncpp.a
	-DJSON_INCLUDE_DIR=${DEPS_DIR}/include
	-DENABLE_LEVELDB=OFF
	-DENABLE_POSTGRESQL=OFF
	-DENABLE_REDIS=OFF
	-DCMAKE_EXE_LINKER_FLAGS=-lbz2
)
if [ "$USE_XCODE" == "yes" ]; then
	cmake_args+=(-GXcode)
fi

cmake .. "${cmake_args[@]}"

if [ "$USE_XCODE" == "yes" ]; then
	xcodebuild -project luanti.xcodeproj -scheme luanti -configuration Release build
	xcodebuild -project luanti.xcodeproj -scheme luanti -archivePath ./luanti.xcarchive archive
else
	cmake --build . -j$(sysctl -n hw.logicalcpu)
	make install
fi

