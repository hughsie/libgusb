#!/bin/sh
set -e

mkdir -p build && cd build
rm * -rf
meson .. $@
ninja -v
ninja test -v
DESTDIR=/tmp/install-ninja ninja install
cd ..
