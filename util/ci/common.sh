#!/bin/bash -e

# Linux build only
install_linux_deps() {
	local pkgs=(
		cmake gettext postgresql
		libpng-dev libjpeg-dev libgl1-mesa-dev libsdl2-dev libfreetype-dev
		libsqlite3-dev libhiredis-dev libogg-dev libgmp-dev libvorbis-dev
		libopenal-dev libpq-dev libleveldb-dev libcurl4-openssl-dev libzstd-dev
		libssl-dev
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
install_macos_brew_deps() {
	local pkgs=(
		cmake gettext freetype gmp jpeg-turbo jsoncpp leveldb
		libogg libpng libvorbis luajit zstd
	)
	export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
	export HOMEBREW_NO_INSTALL_CLEANUP=1
	# contrary to how it may look --auto-update makes brew do *less*
	brew update --auto-update
	brew install --display-times "${pkgs[@]}"
	brew unlink $(brew ls --formula)
	brew link "${pkgs[@]}"
}

install_macos_precompiled_deps() {
  osver=$1
	arch=$2

  local pkgs=(
    cmake wget
  )
  export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1
  export HOMEBREW_NO_INSTALL_CLEANUP=1
  # contrary to how it may look --auto-update makes brew do *less*
  brew update --auto-update
  brew install --display-times "${pkgs[@]}"
  brew unlink $(brew ls --formula)
  brew link "${pkgs[@]}"

  wget macos${osver}_${arch}_deps.tar.gz https://github.com/luanti-org/luanti_macos_deps/releases/download/latest/macos${osver}_${arch}_deps.tar.gz || echo "Ignore stupid error number 4: $?"
  tar -xf macos${osver}_${arch}_deps.tar.gz
}
