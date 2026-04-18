#!/bin/bash
# msys2-bundledlls - Copy DLLs linked against a Windows executable for bundling with a distribution

if [ "$#" -ne 2 ];
then
	echo "Usage: ./bundledlls <executable> <directory>"
	exit
fi

mkdir -p $2

# Get list of dynamic libraries, filter by ones that are found in the current MSYS2 prefix
# and strip off everything but the full path. E.g.:
#   `lua51.dll => /ucrt64/bin/lua51.dll (0xdeadbeef)` => `/ucrt64/bin/lua51.dll`
list=$(ldd $1 | grep $MINGW_PREFIX | sed 's/.* => //' | sed 's/ \(.*\)//')

for dll in $list;
do
	echo $dll
	cp $dll $2/
done
