# The outdoor ground

The whole outdoor map stands on one Unreal **Landscape** (`Ground` in Lvl_Scene01, folder Outside/Ground): a 1 m
grid over 2.65 x 3.15 km (21 x 25 components of 126 quads), heightfield collision that is exactly the drawn
surface, a layered material from seven ground photos. Nothing in Content is committed but the photos; the build
makes it every time.

## Who owns what

- `tools/world_map.py` (map thread): the macro shape `W.height` (the club's hill, the town's lowland, shore,
  beach, the rampart over the corridor), zones and placement, `W.keep_flat` (where the macro height must stay
  exact: apron, forecourt, path, stations, promenade, town lots).
- `tools/terrain.py` (this): samples the macro shape on the landscape grid, routes the dirt tracks, marks the
  forests and the portal hole, writes `Saved/World/terrain_in.*`; `ground_z(x, y)` reads back the built heights
  (`terrain_out.bin`) and is what everything is placed with.
- `Source/Lyudmilovsk/Private/Terrain/TerrainGen.cpp` (plain C++): the natural detail on top of the macro shape
  (rolling land, knolls, hummocks, gullies, low dunes, sand ripples), hydraulic erosion (~600k rain drops),
  worn dirt tracks, the surface layers and the grass masks. Builds outside the engine too, for quick looks.
- `LyudTerrain.cpp` (`ULyudTerrainLibrary::BuildTerrain`): runs it, spawns the landscape, fills the mask asset.
- `unreal/scripts/import_terrain.py`: textures, material, layer infos, the landscape and the portal patch;
  `build_scene01.sh` runs it before `import_world.py`.

## The coastal bluff

Where the map says the sea cliff is (`W.cliff`) or the macro slope passes ~30 degrees, TerrainGen shapes the face
in its own frame (along the bluff / down it): V-shaped gullies up to 8 m deep running straight down, their heads
biting back into the top edge; slump benches in stretches of the coast; landslide scars (the upper face slid
away) with their debris lobe at the foot; fine rain rills; fans of scree and sand where each gully opens onto
the beach. The face is bare clay (the Rock layer) wherever it is steep, grass holds on the gentler benches.
Rock is projected from the sides as well as from above (triplanar, facing from the pixel's own slope) with wavy
strata of pale sand and dark silt, so a 50-60 degree face is not stretched. The 1 m landscape and its collision
take such faces fine; the map keeps W.height free of overhangs.

Over the buried corridor (|X| < 12, Y -470..-30) the detail may raise the ground but never lower it
(`raise_only`); the map keeps the ground at least 5 m over it (the build warns if not).

## Surface layers

Grass, Meadow (dry grass), Dirt (bare patches, steep ground, track edges, gullies), Mud (track centres, wet
hollows), Sand (beach, tidal flat and sea bed; wet and glistening under the tide's wet line, [tides.md](tides.md)), Rock (cliff faces over ~45 degrees), Forest (needles and moss under the pine
woods). Photos: `tools/fal_ground_textures.py` (fal.ai) -> `textures/ground/raw/` (not committed) ->
`blender -b --python tools/make_ground_textures.py` -> `textures/ground/<Layer>_D.jpg` / `_N.jpg` (seamless, even
light). The material (`M_Ground`) mixes each photo at 2.6 m and 11 m tiling, lets stones and grass tips win the
blend between layers, and drifts the brightness slowly so repeats don't show. Tints and roughness: `LOOK` in
import_terrain.py.

## Forest floor

Inside the forest the generator adds mossy hummocks, hollows and windthrow pit-and-mound pairs (`ForestFloorRelief`), and
the mask carries a forest-floor channel (`GetForestFloor`). The litter, ground cover and debris on it:
[forest_floor.md](forest_floor.md).

## Grass mask

`ULyudTerrainMask::Get()` (`/Game/Scene01/Terrain/DA_TerrainMask`, 2 m cells): `GetGrassDensity(WorldLocation)`
0..1 (none on bare patches, tracks, rock, sand), `GetDuneGrassDensity` (dry upper beach), `GetWetness`.
AProceduralGrass can thin its tufts by it.

## The portal hole

The mound over the club drops vertically at the portal face and the forecourt is cut into it with near-vertical
steps behind the wing walls, which a heightfield cannot draw. The landscape has a hole there (`HOLE_X` x `HOLE_Y`
in terrain.py, quads of the visibility layer) and `SM_PortalPatch` (`terrain.portal_patch()`, exact collision)
fills it; its border vertices sit on the landscape's border edges, so there is no seam. If the portal changes
shape, change the hole and the patch's columns with it.

## Checking

`unreal/scripts/make_terrain_preview.sh` builds Lvl_TerrainPreview and traces straight down on a 0.37 m lattice
around the club and a 3.7 m one over the whole map: it reports any spot with no ground (a hole to fall through)
or ground more than 2 cm off `ground_z`. Last run: 0 holes, worst 3 mm. Note: in an editor world landscape
holes are not cut from traces (the editor keeps a hole-less copy for sculpting); the game cuts them, so the check
leaves the landscape out inside the hole. `unreal/scripts/shoot_terrain.py` takes offscreen screenshots
(`-game -RenderOffscreen`, no window).

## Performance

525 landscape components, ~8.3M vertices at LOD 0 with the usual landscape LODs; one draw per component and
up to 3 weightmaps each. The build: ~25 s of Python sampling plus ~7 s of generation in the commandlet.
