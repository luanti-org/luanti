#!/bin/bash -eu
cd /tmp
git clone https://github.com/jupp0r/prometheus-cpp
git -C prometheus-cpp checkout b9366fc3b28292e553ce4820c8929287cdf8e04d
git -C prometheus-cpp submodule update --init --depth=1
mkdir prometheus-cpp/build
cd prometheus-cpp/build
cmake .. \
	-DCMAKE_INSTALL_PREFIX=/usr/local \
	-DCMAKE_BUILD_TYPE=Release \
	-DENABLE_TESTING=0
make -j$(nproc)
sudo make install
