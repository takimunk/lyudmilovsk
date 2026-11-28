#!/bin/sh
# Runs Blender headless at the lowest priority (the Mac is shared with play): blender/diner/run.sh -P blender/diner/building.py [-- args]
exec nice -n 19 taskpolicy -b /Applications/Blender.app/Contents/MacOS/Blender -b "$@"
