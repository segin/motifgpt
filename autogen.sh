#!/bin/sh
set -e
echo "Generating build system..."
mkdir -p m4
autoreconf -fi
echo "Now run: ./configure && make"
