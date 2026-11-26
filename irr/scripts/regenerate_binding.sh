#!/bin/sh

# Switch to the script's directory
scriptdir=$(dirname "$0")

# Check arguments
if [ $# -eq 0 ]; then
	echo "Usage: regenerate_binding.sh file"
	echo "  file -> use \"\" (to download file) or specifi path to XML file with gl specification you want to use"
	exit 1
fi

if [ -z "$1" ]; then
	echo "No file provided. Downloading gl.xml..."
	wget -O "$scriptdir/gl.xml" https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/refs/heads/main/xml/gl.xml
	xml_file="$scriptdir/gl.xml"
else
	xml_file="$1"
fi

header_template="$scriptdir/mt_opengl.template"
header_file="$scriptdir/../include/mt_opengl.h"
loader_template="$scriptdir/mt_opengl_loader.template"
loader_file="$scriptdir/../src/mt_opengl_loader.cpp"

python3 "$scriptdir/BindingGenerator.py" "$xml_file" "$header_template" "$header_file" "$loader_template" "$loader_file"
