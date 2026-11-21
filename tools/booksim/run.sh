#!/bin/bash
# Offline checks of the library books' layout: bash tools/booksim/run.sh [Content/Books/en/hamlet.txt ...]
# Reading anywhere and the book club (light rule, quotes, stats, the club): bash tools/booksim/run.sh reading [reading.json]
set -e
cd "$(dirname "$0")"
if [ "$1" = "reading" ]; then
	shift
	c++ -std=c++17 -O2 -Wall -Wshadow -I../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private -o /tmp/lyud_readingtest readingtest.cpp
	exec /tmp/lyud_readingtest "$@"
fi
c++ -std=c++17 -O2 -Wall -Wshadow -include cstring -o /tmp/lyud_booktest booktest.cpp
/tmp/lyud_booktest "$@"
