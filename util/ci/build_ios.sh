#!/bin/bash -e

sudo xcode-select -s /Applications/Xcode_${xcodever}.app/Contents/Developer
sdkroot="$(realpath $(xcrun --sdk iphonesimulator --show-sdk-path)/../iPhoneSimulator${osver}.sdk)"
export CMAKE_PREFIX_PATH=${DEPS_DIR}
export SDKROOT="$sdkroot"

cmake .. -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=$osver -DCMAKE_FIND_FRAMEWORK=LAST -DCMAKE_OSX_ARCHITECTURES=arm64 \
					-DCMAKE_INSTALL_PREFIX=../build/ios/ -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE -DCMAKE_BUILD_TYPE=Release \
					-DCMAKE_PREFIX_PATH=${DEPS_DIR} \
					-DENABLE_OPENGL=OFF \
					-DENABLE_OPENGL3=OFF \
					-DENABLE_GLES2=ON \
					-DUSE_SDL2_STATIC=ON \
					-DSDL2_DIR=${DEPS_DIR}/lib/cmake/SDL2 \
					-DOPENGLES2_LIBRARY=${DEPS_DIR}/lib/libGLESv2_static.a \
					-DOPENGLES2_INCLUDE_DIR=${DEPS_DIR}/include/ANGLE \
					-DCURL_LIBRARY=${DEPS_DIR}/lib/libcurl.a \
					-DCURL_INCLUDE_DIR=${DEPS_DIR}/include \
					-DFREETYPE_LIBRARY=${DEPS_DIR}/lib/libfreetype.a \
					-DFREETYPE_INCLUDE_DIRS=${DEPS_DIR}/include/freetype2 \
					-DGETTEXT_INCLUDE_DIR=${DEPS_DIR}/include \
					-DGETTEXT_LIBRARY=${DEPS_DIR}/lib/libintl.a \
					-DLUA_LIBRARY=${DEPS_DIR}/lib/libluajit-5.1.a \
					-DLUA_INCLUDE_DIR=${DEPS_DIR}/include/luajit-2.1 \
					-DOGG_LIBRARY=${DEPS_DIR}/lib/libogg.a \
					-DOGG_INCLUDE_DIR=${DEPS_DIR}/include \
					-DVORBIS_LIBRARY=${DEPS_DIR}/lib/libvorbis.a \
					-DVORBISFILE_LIBRARY=${DEPS_DIR}/lib/libvorbisfile.a \
					-DVORBIS_INCLUDE_DIR=${DEPS_DIR}/include \
					-DZSTD_LIBRARY=${DEPS_DIR}/lib/libzstd.a \
					-DZSTD_INCLUDE_DIR=${DEPS_DIR}/include \
					-DGMP_LIBRARY=${DEPS_DIR}/lib/libgmp.a \
					-DGMP_INCLUDE_DIR=${DEPS_DIR}/include \
					-DJSON_LIBRARY=${DEPS_DIR}/lib/libjsoncpp.a \
					-DJSON_INCLUDE_DIR=${DEPS_DIR}/include \
					-DENABLE_LEVELDB=OFF \
					-DENABLE_POSTGRESQL=OFF \
					-DENABLE_REDIS=OFF \
					-DJPEG_LIBRARY=${DEPS_DIR}/lib/libjpeg.a \
					-DJPEG_INCLUDE_DIR=${DEPS_DIR}/include \
					-DPNG_LIBRARY=${DEPS_DIR}/lib/libpng.a \
					-DPNG_PNG_INCLUDE_DIR=${DEPS_DIR}/include \
					-DCMAKE_EXE_LINKER_FLAGS="-lbz2 ${DEPS_DIR}/lib/libANGLE_static.a ${DEPS_DIR}/lib/libEGL_static.a" \
					-DXCODE_CODE_SIGN_ENTITLEMENTS=${REPDIR}/misc/ios/entitlements/release.entitlements \
					-GXcode
xcodebuild 	-project luanti.xcodeproj \
						-target luanti \
						-configuration Release \
						-sdk iphonesimulator \
						build
