#!/bin/bash
# Offline checks of the library books' layout: bash tools/booksim/run.sh [Content/Books/en/hamlet.txt ...]
set -e
cd "$(dirname "$0")"
c++ -std=c++17 -O2 -Wall -Wshadow -include cstring -o /tmp/lyud_booktest booktest.cpp
/tmp/lyud_booktest "$@"
