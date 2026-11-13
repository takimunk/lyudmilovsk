#!/bin/sh
# Builds and runs the plant model outside the engine (tools/plantsim/plantsim.cpp): the species presets, PlantModel.cpp
# (skeleton, seasons) and the herb builder, compiled with clang++ against a small stand-in for Unreal's core types
# (tools/plantsim/shim). It counts what a species costs: the triangles of each forest-bake LOD as
# PlantMeshes::PrepareTreeBake would build them, a herb's two LODs as an AProceduralGrass herb patch builds them, its
# height and reach. From the repo root.
#   tools/plantsim/run.sh                 the North's species (docs/north.md "Plants") with a few old ones to compare
#   tools/plantsim/run.sh all             every species of the library (also: does each build without NaNs)
#   tools/plantsim/run.sh <Name> ...      those species
#   tools/plantsim/run.sh render <out.png> <Name>:<age>:<day>[:<seed>] ...
#                                         a picture of each (tools/plantsim/render.cpp), side by side at one scale
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/unreal/Lyudmilovsk/Source/Lyudmilovsk"
OUT="$ROOT/Saved/PlantSim"
mkdir -p "$OUT/inc/Engine" "$OUT/inc/PhysicsEngine"
for H in PlantSpecies.generated.h PlantModel.generated.h GrassSpecies.generated.h Engine/StaticMesh.h Engine/World.h \
	Engine/Texture2D.h MeshDescription.h PhysicsEngine/BodySetup.h StaticMeshAttributes.h StaticMeshResources.h; do
	cp "$ROOT/tools/plantsim/shim/Empty.h" "$OUT/inc/$H"
done
# the herb builder without its meshes and textures (Unreal objects): PlantHerb.cpp down to its marker line
awk '/^\/\/ --- meshes and textures \(Unreal objects\)/ { exit } { print }' "$SRC/Private/Plants/PlantHerb.cpp" > "$OUT/PlantHerbModel.cpp"
clang++ -std=c++20 -O2 -Wall -Wshadow -Wunreachable-code-loop-increment -Wno-unused-function -Wno-unused-variable \
	-I "$OUT/inc" -I "$ROOT/tools/plantsim/shim" -I "$SRC/Public" -I "$SRC/Private" \
	-o "$OUT/plantsim" "$ROOT/tools/plantsim/plantsim.cpp" "$ROOT/tools/plantsim/render.cpp" "$SRC/Private/Plants/PlantModel.cpp" \
	"$SRC/Private/Plants/PlantSpecies.cpp" "$SRC/Private/Plants/PlantSpeciesHerbs.cpp" "$SRC/Private/Plants/PlantSpeciesFloor.cpp" \
	"$SRC/Private/Plants/PlantSpeciesNorth.cpp" "$SRC/Private/Plants/PlantAlga.cpp" "$OUT/PlantHerbModel.cpp"
exec "$OUT/plantsim" "$@"
