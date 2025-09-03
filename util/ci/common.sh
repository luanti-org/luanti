#!/bin/bash -e

# Linux build only
install_linux_deps() {
	local graphics=1
	if [[ "$1" == "--headless" ]]; then
		graphics=
		shift
	fi
	local pkgs=(
		cmake gettext postgresql
		libsqlite3-dev libhiredis-dev libogg-dev libgmp-dev libpq-dev
		libleveldb-dev libcurl4-openssl-dev libzstd-dev libssl-dev
	)
	[ -n "$graphics" ] && pkgs+=(
		libpng-dev libjpeg-dev libgl1-mesa-dev libsdl2-dev libfreetype-dev
		libogg-dev libvorbis-dev libopenal-dev
	)

	sudo apt-get update
	sudo apt-get install -y --no-install-recommends "${pkgs[@]}" "$@"

	# set up Postgres for unit tests
	if [ -n "$MINETEST_POSTGRESQL_CONNECT_STRING" ]; then
		sudo systemctl start postgresql.service
		sudo -u postgres psql <<<"
			CREATE USER minetest WITH PASSWORD 'minetest';
			CREATE DATABASE minetest;
			\c minetest
			GRANT ALL ON SCHEMA public TO minetest;
		"
	fi
}

# macOS build only
install_macos_deps() {
	# Uninstall the bundled cmake, it is outdated, and brew does not want to install the newest version with this one present since they are from different taps.
	brew uninstall cmake || :

	local pkgs=(
		cmake gettext freetype gmp jpeg-turbo jsoncpp leveldb
		libogg libpng libvorbis luajit zstd sdl2
	)
	export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
	export HOMEBREW_NO_INSTALL_CLEANUP=1
	# contrary to how it may look --auto-update makes brew do *less*
	brew update --auto-update
	brew install --display-times "${pkgs[@]}"
	brew unlink $(brew ls --formula)
	brew link "${pkgs[@]}"
}
