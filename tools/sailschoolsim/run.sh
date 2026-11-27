#!/bin/sh
# Builds and runs the offline checks of the yacht school's rules (tools/sailschoolsim/schooltest.cpp on
# Private/Sailing/SailSchoolModel.h, docs/yacht_school.md part B) on the real Content/City/sailing.json
# (python3 tools/sailing/sailing.py writes it). Strict warnings as on the Mac.
#   tools/sailschoolsim/run.sh                  the checks (exit 1 on a failure)
#   tools/sailschoolsim/run.sh week 2026 5 11   the timetable of the seven days from a date
#   tools/sailschoolsim/run.sh season 2026      a season's race days resolved: the cups' tables
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/Saved/SailSchoolSim"
clang++ -std=c++17 -O2 $EXTRA_CXXFLAGS -Wall -Wextra -Wshadow -Wunreachable-code-loop-increment \
    -o "$ROOT/Saved/SailSchoolSim/schooltest" "$ROOT/tools/sailschoolsim/schooltest.cpp"
cd "$ROOT" && exec "$ROOT/Saved/SailSchoolSim/schooltest" "$@"
