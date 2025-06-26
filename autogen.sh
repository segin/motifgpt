#!/bin/sh
set -e
echo "Generating build system..."
mkdir -p m4
aclocal
automake --add-missing --copy
autoconf