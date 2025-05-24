CORE_GIT=https://github.com/luanti-org/luanti
CORE_BRANCH=master
CORE_NAME=minetest

ogg_version=1.3.5
openal_version=1.24.2
vorbis_version=1.3.7
curl_version=8.12.1
gettext_version=0.20.2
freetype_version=2.13.3
sqlite3_version=3.46.1
luajit_version=20250113
leveldb_version=1.23
zlib_version=1.3.1
zstd_version=1.5.7
libjpeg_version=3.0.1
libpng_version=1.6.47
sdl2_version=2.32.2

download () {
	local url=$1
	local filename=$2
	[ -z "$filename" ] && filename=${url##*/}
	local foldername=${filename%%[.-]*}
	local extract=${3:-unzip}

	[ -d "./$foldername" ] && return 0
	wget "$url" -c -O "./$filename"
	sha256sum -w -c <(grep -F "$filename" "$topdir/sha256sums.txt")
	if [ "$extract" = "unzip" ]; then
		unzip -o "$filename" -d "$foldername"
	elif [ "$extract" = "unzip_nofolder" ]; then
		unzip -o "$filename"
	else
		return 1
	fi
}

# sets $sourcedir
get_sources () {
	if [ -n "$EXISTING_MINETEST_DIR" ]; then
		sourcedir="$( cd "$EXISTING_MINETEST_DIR" && pwd )"
		return
	fi
	cd $builddir
	sourcedir=$PWD/$CORE_NAME
	[ -d $CORE_NAME ] && { pushd $CORE_NAME; git pull --ff-only; popd; } || \
		git clone -b $CORE_BRANCH $CORE_GIT $CORE_NAME
}

# sets $runtime_dlls
find_runtime_dlls () {
	local triple=$1
	# Try to find runtime DLLs in various paths (fun)
	local tmp=$(dirname "$(command -v $compiler)")/..
	runtime_dlls=
	for name in lib{c++,unwind,winpthread-}'*'.dll; do
		for dir in $tmp/$triple/{bin,lib}; do
			[ -d "$dir" ] || continue
			local file=$(echo $dir/$name)
			[ -f "$file" ] && { runtime_dlls+="$file;"; break; }
		done
	done
	if [ -z "$runtime_dlls" ]; then
		echo "The compiler runtime DLLs could not be found, they might be missing in the final package."
	else
		echo "Found DLLs: $runtime_dlls"
	fi
}

_dlls () {
	for f in "$@"; do
		if [ ! -e "$f" ]; then
			echo "Could not find $f" >&2
		elif [[ -f "$f" && "$f" == *.dll ]]; then
			printf '%s;' "$f"
		fi
	done
}

add_cmake_libs () {
	cmake_args+=(
		-DPNG_LIBRARY=$libdir/libpng/lib/libpng16.dll.a
		-DPNG_PNG_INCLUDE_DIR=$libdir/libpng/include
		-DPNG_DLL="$(_dlls $libdir/libpng/bin/*)"

		-DJPEG_LIBRARY=$libdir/libjpeg/lib/libjpeg.dll.a
		-DJPEG_INCLUDE_DIR=$libdir/libjpeg/include
		-DJPEG_DLL="$(_dlls $libdir/libjpeg/bin/libjpeg*)"

		-DSDL2_DIR=$libdir/sdl2/lib/cmake/SDL2
		-DSDL2_DLL="$(_dlls $libdir/sdl2/bin/*)"

		-DZLIB_INCLUDE_DIR=$libdir/zlib/include
		-DZLIB_LIBRARY=$libdir/zlib/lib/libz.dll.a
		-DZLIB_DLL=$libdir/zlib/bin/zlib1.dll

		-DZSTD_INCLUDE_DIR=$libdir/zstd/include
		-DZSTD_LIBRARY=$libdir/zstd/lib/libzstd.dll.a
		-DZSTD_DLL=$libdir/zstd/bin/libzstd.dll

		-DLUA_INCLUDE_DIR=$libdir/luajit/include
		-DLUA_LIBRARY=$libdir/luajit/libluajit.a

		-DOGG_INCLUDE_DIR=$libdir/libogg/include
		-DOGG_LIBRARY=$libdir/libogg/lib/libogg.dll.a
		-DOGG_DLL="$(_dlls $libdir/libogg/bin/*)"

		-DVORBIS_INCLUDE_DIR=$libdir/libvorbis/include
		-DVORBIS_LIBRARY=$libdir/libvorbis/lib/libvorbis.dll.a
		-DVORBIS_DLL="$(_dlls $libdir/libvorbis/bin/libvorbis{,file}[-.]*)"
		-DVORBISFILE_LIBRARY=$libdir/libvorbis/lib/libvorbisfile.dll.a

		-DOPENAL_INCLUDE_DIR=$libdir/openal/include/AL
		-DOPENAL_LIBRARY=$libdir/openal/lib/libOpenAL32.dll.a
		-DOPENAL_DLL=$libdir/openal/bin/OpenAL32.dll

		-DCURL_DLL="$(_dlls $libdir/curl/bin/libcurl*)"
		-DCURL_INCLUDE_DIR=$libdir/curl/include
		-DCURL_LIBRARY=$libdir/curl/lib/libcurl.dll.a

		-DGETTEXT_MSGFMT=`command -v msgfmt`
		-DGETTEXT_DLL="$(_dlls $libdir/gettext/bin/lib{intl,iconv}*)"
		-DGETTEXT_INCLUDE_DIR=$libdir/gettext/include
		-DGETTEXT_LIBRARY=$libdir/gettext/lib/libintl.dll.a

		-DFREETYPE_INCLUDE_DIR_freetype2=$libdir/freetype/include/freetype2
		-DFREETYPE_INCLUDE_DIR_ft2build=$libdir/freetype/include/freetype2
		-DFREETYPE_LIBRARY=$libdir/freetype/lib/libfreetype.dll.a
		-DFREETYPE_DLL="$(_dlls $libdir/freetype/bin/libfreetype*)"

		-DSQLITE3_INCLUDE_DIR=$libdir/sqlite3/include
		-DSQLITE3_LIBRARY=$libdir/sqlite3/lib/libsqlite3.dll.a
		-DSQLITE3_DLL="$(_dlls $libdir/sqlite3/bin/libsqlite*)"

		-DLEVELDB_INCLUDE_DIR=$libdir/libleveldb/include
		-DLEVELDB_LIBRARY=$libdir/libleveldb/lib/libleveldb.dll.a
		-DLEVELDB_DLL=$libdir/libleveldb/bin/libleveldb.dll
	)
}
