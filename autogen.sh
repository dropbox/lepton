#!/bin/sh
git submodule init
git submodule update || ( rm -rf -- "`dirname $0`/dependencies/xz" && git submodule update )

exec autoreconf -fi
