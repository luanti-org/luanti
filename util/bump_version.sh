#!/bin/bash -e

prompt_for() {
	local prompt_text=$1
	local pattern=$2
	local default_value=$3
	local tmp=
	while true; do
		read -p "$prompt_text [$default_value]: " tmp
		if [ -z "$tmp" ]; then
			echo "$default_value"; return
		elif echo "$tmp" | grep -qE "^(${pattern})\$"; then
			echo "$tmp"; return
		fi
	done
}

# Reads current versions
# out: VERSION_MAJOR VERSION_MINOR VERSION_PATCH VERSION_IS_DEV CURRENT_VERSION
read_versions() {
	VERSION_MAJOR=$(grep -oE '^set\(VERSION_MAJOR [0-9]+\)$' CMakeLists.txt | tr -dC 0-9)
	VERSION_MINOR=$(grep -oE '^set\(VERSION_MINOR [0-9]+\)$' CMakeLists.txt | tr -dC 0-9)
	VERSION_PATCH=$(grep -oE '^set\(VERSION_PATCH [0-9]+\)$' CMakeLists.txt | tr -dC 0-9)
	VERSION_IS_DEV=$(grep -oE '^set\(DEVELOPMENT_BUILD [A-Z]+\)$' CMakeLists.txt)

	# Make sure they all exist
	[ -n "$VERSION_MAJOR" ]
	[ -n "$VERSION_MINOR" ]
	[ -n "$VERSION_PATCH" ]
	[ -n "$VERSION_IS_DEV" ]

	if echo "$VERSION_IS_DEV" | grep -q ' TRUE'; then
		VERSION_IS_DEV=1
	else
		VERSION_IS_DEV=0
	fi
	CURRENT_VERSION="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"

	echo "Current Luanti version: $CURRENT_VERSION"
}

# Retrieves protocol version from header
# in: $1
read_proto_ver() {
	local ref=$1
	local output=$(git show "$ref":src/network/networkprotocol.cpp 2>/dev/null)
	if [ -z "$output" ]; then
		# Fallback to previous file (for tags < 5.10.0)
		output=$(git show "$ref":src/network/networkprotocol.h)
	fi
	grep -oE 'LATEST_PROTOCOL_VERSION\s+=?\s*[0-9]+' <<<"$output" | tr -dC 0-9
}

## Prompts for new version
# in: VERSION_{MAJOR,MINOR,PATCH} DO_PATCH_REL
# out: NEXT_VERSION NEXT_VERSION_{MAJOR,MINOR,PATCH}
bump_version() {
	NEXT_VERSION_MAJOR=$VERSION_MAJOR
	if [ "$DO_PATCH_REL" -eq 1 ]; then
		NEXT_VERSION_MINOR=$VERSION_MINOR
		NEXT_VERSION_PATCH=$(expr $VERSION_PATCH + 1)
	else
		NEXT_VERSION_MINOR=$(expr $VERSION_MINOR + 1)
		NEXT_VERSION_PATCH=0
	fi

	NEXT_VERSION_MAJOR=$(prompt_for "Set next major" '[0-9]+' $NEXT_VERSION_MAJOR)
	if [ "$NEXT_VERSION_MAJOR" != "$VERSION_MAJOR" ]; then
		NEXT_VERSION_MINOR=0
		NEXT_VERSION_PATCH=0
	fi

	NEXT_VERSION_MINOR=$(prompt_for "Set next minor" '[0-9]+' $NEXT_VERSION_MINOR)
	if [ "$NEXT_VERSION_MINOR" != "$VERSION_MINOR" ]; then
		NEXT_VERSION_PATCH=0
	fi

	NEXT_VERSION_PATCH=$(prompt_for "Set next patch" '[0-9]+' $NEXT_VERSION_PATCH)

	NEXT_VERSION="$NEXT_VERSION_MAJOR.$NEXT_VERSION_MINOR.$NEXT_VERSION_PATCH"

	echo
	echo "New version: $NEXT_VERSION"
}

## Toggles development build
# in: $1
set_dev_build() {
	local is_dev=$1

	# Update CMakeList.txt versions
	if [ "$is_dev" -eq 1 ]; then
		sed -i -re 's/^set\(DEVELOPMENT_BUILD [A-Z]+\)$/set(DEVELOPMENT_BUILD TRUE)/' CMakeLists.txt
	else
		sed -i -re 's/^set\(DEVELOPMENT_BUILD [A-Z]+\)$/set(DEVELOPMENT_BUILD FALSE)/' CMakeLists.txt
	fi

	git add -f CMakeLists.txt android/build.gradle
}

## Writes new version to the right files
# in: NEXT_VERSION NEXT_VERSION_{MAJOR,MINOR,PATCH}
write_new_version() {
	# Update CMakeList.txt versions
	sed -i -re "s/^set\(VERSION_MAJOR [0-9]+\)$/set(VERSION_MAJOR $NEXT_VERSION_MAJOR)/" CMakeLists.txt
	sed -i -re "s/^set\(VERSION_MINOR [0-9]+\)$/set(VERSION_MINOR $NEXT_VERSION_MINOR)/" CMakeLists.txt
	sed -i -re "s/^set\(VERSION_PATCH [0-9]+\)$/set(VERSION_PATCH $NEXT_VERSION_PATCH)/" CMakeLists.txt

	# Update Android versions
	sed -i -re "s/set\(\"versionMajor\", [0-9]+\)/set(\"versionMajor\", $NEXT_VERSION_MAJOR)/" android/build.gradle
	sed -i -re "s/set\(\"versionMinor\", [0-9]+\)/set(\"versionMinor\", $NEXT_VERSION_MINOR)/" android/build.gradle
	sed -i -re "s/set\(\"versionPatch\", [0-9]+\)/set(\"versionPatch\", $NEXT_VERSION_PATCH)/" android/build.gradle

	# Update doc versions
	sed -i -re '1s/[0-9]+\.[0-9]+\.[0-9]+/'"$NEXT_VERSION"'/g' doc/menu_lua_api.md
	sed -i -re '1s/[0-9]+\.[0-9]+\.[0-9]+/'"$NEXT_VERSION"'/g' doc/client_lua_api.md

	git add -f CMakeLists.txt android/build.gradle doc/menu_lua_api.md doc/client_lua_api.md
}

## Create release commit and tag
# in: $1
perform_release() {
	local release_version=$1
	RELEASE_DATE=$(date +%Y-%m-%d)

	sed -i '/\<release/s/\(version\)="[^"]*"/\1="'"$release_version"'"/' misc/*.metainfo.xml
	sed -i 's/\(<release date\)="[^"]*"/\1="'"$RELEASE_DATE"'"/' misc/*.metainfo.xml

	git add -f misc/*.metainfo.xml

	git commit -m "Bump version to $release_version"

	echo "Tagging $release_version"

	git tag -a "$release_version" -m "$release_version"
}

## Create after-release commit
# in: NEXT_VERSION
back_to_devel() {
	echo 'Creating "return back to development" commit'

	git commit -m "Continue with $NEXT_VERSION-dev"
}

#######################
# Start of main logic:
#######################

# Switch to top luanti directory
cd ${0%/*}/..

# Determine old versions
read_versions

# Double-check what we're doing
if [ "$VERSION_IS_DEV" -eq 1 ]; then
	echo "You are on the development branch and about to make a major or minor release."
	DO_PATCH_REL=0
else
	echo "You are on the stable/backport branch and about to make a patch release."
	DO_PATCH_REL=1
fi
if [[ "$(prompt_for "Is this correct?" '[Yy][Ee][Ss]|[Nn][Oo]|' no)" != [Yy][Ee][Ss] ]]; then
	echo "Aborting"
	exit 1
fi

if [ "$DO_PATCH_REL" -eq 0 ]; then
	# On a regular release the version moves from 5.7.0-dev -> 5.7.0 (new tag) -> 5.8.0-dev

	old_proto=$(read_proto_ver origin/stable-5)
	new_proto=$(read_proto_ver HEAD)
	[ -n "$old_proto" ]
	[ -n "$new_proto" ]
	echo "Protocol versions: $old_proto (last release) -> $new_proto (now)"
	if [ "$new_proto" -le "$old_proto" ]; then
		echo "The protocol version has not been increased since last release, refusing to continue."
		exit 1
	fi

	set_dev_build 0

	perform_release "$CURRENT_VERSION"

	bump_version
	set_dev_build 1
	write_new_version

	back_to_devel
else
	# On a patch release the version moves from 5.7.0 -> 5.7.1 (new tag)

	bump_version
	write_new_version

	perform_release "$NEXT_VERSION"
fi
