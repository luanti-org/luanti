#!/bin/bash
set -e
topdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ -z "$1" ]; then
	echo "Usage: $0 <dest path>"
	exit 1
fi

# key points:
# * Clang + LLD + libc++ instead of GCC + binutils + stdc++
# * Mingw-w64 with UCRT enabled and winpthreads support
# why are we avoiding GCC? -> Thread Local Storage (TLS) is totally broken
date=20250305
name=llvm-mingw-${date}-ucrt-ubuntu-20.04-x86_64.tar.xz
wget "https://github.com/mstorsjo/llvm-mingw/releases/download/$date/$name" -O "$name"
sha256sum -w -c <(grep -F "$name" "$topdir/sha256sums.txt")
tar -xaf "$name" -C "$1" --strip-components=1
rm -f "$name"
