# The North: what is left to finish

The North (everything north of Y 1450, docs/north.md) went into main on 2026-11-14 building and passing
`test_north` (27 steps), but the first in-game frames show it is not finished. The frames are on the branch
`north-shots`, `shots/north-2026-11-14/` (with a README). This list is the owner's "merge, but record what must be
finished".

## Looks wrong in the game

1. **River water**: the water sheet draws as stretched radial streaks (the flow texture's UVs along the ribbon,
   `north.json` `river.water`, the river mesh and its material). Wanted: calm clear water with a slow flow and soft
   edges on the gravel.
2. **Gravel bars**: at eye level the plain between the channels is one flat brown surface. The braids, bars and
   islands of `tools/north_river.py` do not read: too little relief, one texture, no wet edges, no cobbles. The
   driftwood, boulders, willows and sedge are too few to break it up.
3. **River mouth**: from about (247, 2121) it is a wide empty sand flat with no water in sight; the rowboat reads as a
   dark box. Check the outlet through the beach berm against the tide, the water sheet's reach, and the boat's material.
4. **Hot spring**: the basin is a deep dark pit with a small square of water at the bottom (the water quad is not the
   pool's shape or size); `spring_deck` and `spring_steps` overlap with crossing railings; seated, the hero faces the
   basin wall. Rework the basin (shallower, a lip at the water line), make the water a disc of the pool's radius, and
   choose either the deck or the steps on the entry side.
5. **Ranger's hut**: `hut` and `hut_interior` were never imported: a grey placeholder box stands there. Build or
   import the hut model and its interior (an enterable building is rebuilt by us in Blender, per the asset rule).
6. **Woods**: the ridge forest round the Polar brow is thin and sparse, not broad masses; the valley's sides carry
   hundreds of small uniform bare trees. Raise the cover and vary age and size in `north.wood` / `WOOD_DENSITY`, mass
   the trees in groves with open grass between, and check the mixes in the season the game starts in (late October:
   bare birch and larch are right, but pines and spruces should carry the ridge).
7. **Far volcanoes**: the snow shows vertical white stripes (the snow mask's UVs on the cone, `tools/north_far.py`).
8. **Polar brow view**: the river valley cannot be seen from the bench at eye height; move the bench to the edge or
   open the view.
9. **Sea stacks**: the shapes are blocky prisms; the cape viewpoint near (760, 2665) is down on the sand flat, not on
   a cliff. Rework the meshes and check each stack against the real seabed and tide.

## Not verified yet

- **Performance**: no benchmark of the North against main (frame time and memory on the trail from the Polar station
  to the camp). Run it alone, per the perf checklist.
- **Tall herbs**: meadowsweet, cow parsnip and sedge patches (up to ~580k triangles in a 40 x 40 m patch at full
  detail) have not been looked at or measured in the engine.
- **Birds**: `AddRealTrees` replaces the data's tree sites with real crowns, so the tree birds' share in the North may
  be higher in game than in the offline sim; perch heights on bridges, driftwood, posts and the hut ridge unchecked.
- **People**: the North's people say their place remarks only with `lyud.Barks.Residents 1` (the talk subsystem takes
  townsfolk only); a camper soaking does not reserve a seat in `ALyudNorthSpring`; no rest between repeated soaks.
- **Residents' models** for the five people of the North: `looks.py`, `build_residents.py --only ranger,
  melnikova_anya,ito_ren,savin_gleb,okada_kiyo`, `check_export`, `import_residents.sh --only` the same keys.
- **Voice**: the ranger's lines are unvoiced (a paid generation: needs the owner's go-ahead).
- **Winter**: the North keeps snow in winter by design (audit N4 not done); not looked at in the engine.
- `fire_ring` uses `Builder.stone`, whose UVs wrap across atlas tiles on big stones: check its larger stones.
