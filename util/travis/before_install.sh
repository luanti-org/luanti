#!/bin/bash -e

if [ $CC = "clang" ]; then
	export PATH="/usr/bin/:$PATH"
	sudo sh -c 'echo "deb http://ppa.launchpad.net/eudoxos/llvm-3.1/ubuntu precise main" >> /etc/apt/sources.list'
	sudo apt-key adv --keyserver pool.sks-keyservers.net --recv-keys 92DE8183
	sudo apt-get update
	sudo apt-get install llvm-3.1
	sudo apt-get install clang
fi
sudo apt-get install p7zip-full
if [ $ANDROID = "yes" ]; then
	
	printf "Generating swap..."
	# create approx 5 GB swap
	sudo dd if=/dev/zero of=/swap count=10000000 
	sudo mkswap /swap
	sudo swapon -p1 /swap
	echo " Done."
	mount

	# Android builds require this, otherwise they give obscure "no such file or directory" error
	if [ `uname -m` = x86_64 ]; then
		sudo apt-get update
		sudo apt-get install --force-yes libgd2-xpm ia32-libs ia32-libs-multiarch
	fi

	# Download sdk and ndk
	wget https://dl.google.com/android/android-sdk_r24.0.1-linux.tgz -O android-sdk.tgz
	wget https://dl.google.com/android/ndk/android-ndk-r9b-linux-x86_64.tar.bz2 -O android-ndk.tar.bz2
	printf "Extracting sdk and ndk archives..."
	tar xf android-sdk.tgz
	tar xf android-ndk.tar.bz2
	echo " Done."
	sudo mv android-sdk-linux /usr/local/android-sdk
	sudo mv android-ndk-r9b /usr/local/android-ndk
	( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) |\
		/usr/local/android-sdk/tools/android update sdk --no-ui --filter \
		platform-tool,android-10,build-tools-21.1.2
elif [ $WINDOWS = "no" ]; then
	sudo apt-get install libirrlicht-dev cmake libbz2-dev libpng12-dev \
	libjpeg8-dev libxxf86vm-dev libgl1-mesa-dev libsqlite3-dev libhiredis-dev \
	libogg-dev libvorbis-dev libopenal-dev gettext
	# Linking to LevelDB is broken, use a custom build
	wget http://sfan5.pf-control.de/libleveldb-1.18-ubuntu12.04.7z
	sudo 7z x -o/usr libleveldb-1.18-ubuntu12.04.7z
else
	if [ $WINDOWS = "32" ]; then
		wget http://sfan5.pf-control.de/mingw_w64_i686_ubuntu12.04_4.9.1.7z -O mingw.7z
		sed -e "s|%PREFIX%|i686-w64-mingw32|" \
			-e "s|%ROOTPATH%|/usr/i686-w64-mingw32|" \
			< util/travis/toolchain_mingw.cmake.in > util/buildbot/toolchain_mingw.cmake
	elif [ $WINDOWS = "64" ]; then
		wget http://sfan5.pf-control.de/mingw_w64_x86_64_ubuntu12.04_4.9.1.7z -O mingw.7z
		sed -e "s|%PREFIX%|x86_64-w64-mingw32|" \
			-e "s|%ROOTPATH%|/usr/x86_64-w64-mingw32|" \
			< util/travis/toolchain_mingw.cmake.in > util/buildbot/toolchain_mingw64.cmake
	fi
	sudo 7z x -y -o/usr mingw.7z
fi
