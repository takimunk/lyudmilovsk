# The East: the old dunes between the town and Topi

Source of truth: `tools/east.py` (`python3 tools/east.py` draws `Saved/East/east.png`). It replaces the dead level
plain under square forest tiles that lay behind the town, and the straight ramp on the map's east edge.

- **Relief** (`east.lift`, called from `world_map.valley` for X < -700, Y < 1450): behind the upper terrace's plots a
  wooded scarp (`scarp_x(y)`, ~195 m behind W5's back, wavy), then fossil dunes on a plateau at -7..-2 m: long
  ridges lying north to south, up to ~13 m, with flat heath slacks between them. A pass on the town's main axis
  (`pass_y(x)`, Y ~905) keeps the dunes back from the track. Fades into the old ramp over Y 1370..1450, where the
  North (`tools/north.py`) takes the land over.
- **Woods** (`east.wood`, the cover the forest tiles are thinned by; contract with the North thread:
  `world_map.forest_cover` -> `Content/World/forest_cover.fcv` -> `AProceduralForest`): pines on the ridges, heath in
  the slacks, glades with lobed edges everywhere south of Y 1450 (the upland's forest too), a front toward the town
  with lone trees ahead of it, the pass open, and the trees thinning out from X -1100 to nothing at X -1200 where
  Topi's sand begins. Tiles in `world_map.forests`: old forest X -1160..-700, dune pines and juniper X -1300..-1040
  (mixed where they overlap).
- **Track**: `terrain.TRACKS` has `town_east -> topi`, from the back of the upper terrace (-730, 905) to the start of
  Topi's path (-1170, 905); the router takes the pass by itself.
- **Topi's side**: after any change here rerun `python3 tools/topi.py` and `bash tools/topi/run.sh test`. Where the
  path climbs the great dune it has a saddle (TopiModel.cpp), ~8 m over the ground instead of ~20.

Not done yet: the landscape and the forests have to be rebuilt in Unreal (terrain build + `build_scene01.sh`) to see
it in the game; the cover mask needs the North branch's `AProceduralForest` change.
