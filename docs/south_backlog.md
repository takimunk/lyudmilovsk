# The Aton coast: not done yet

What [south.md](south.md) leaves open, the nearest first. Everything here is a proposal.

## Looks

- **The brook's water** is the North's river ribbon (`ALyudNorthRiver`) with its material: it inherits that river's open
  points (docs/north_backlog.md: the streaks at a low sun, no foam at the stones). The fall in the ravine is only the
  ribbon on a steeper bed: no white water, no sound of its own.
- **The headland's south side** (the tunnel's portal face, Y -30) is still the old straight terrace edge; only the north
  and east brows wander. The railway's cutting sits in it, so it needs the railway's thread.
- **Words on the boards** follow the game's language only when the level starts (`LocalizeSouthBoards`); a change in
  the menu shows after a reload.
- **Lamps do not shine**: `park_lamp` (manifest `lamps`) is a mesh only; night lights through the town's street-light
  system are the next step. The lighthouse shines (branch `lighthouse`, docs/lighthouse.md).
- The lichen on the boulders is a pale cloth tile with hard edges (the atlas has no moss); the flower bowl's flowers are
  simple low-poly tufts, not procedural plants.

## Game

- **Used so far**: the seats and the coin viewer (south.md "Game side"). Not yet: one cannot drink at the spring or fill
  a bottle, the apples are not picked, the boats do not move, the info boards cannot be read up close as a page.
- **People** walk the shoulder trail, sit at the belvedere, mend nets at the shed, keep the hives, watch the starlings
  from the hide and play at the brook (south.md "People", `tools/town/south_people.py`). Not yet: the cliff walk, the sea
  trail and the spring trail have nobody of their own; the hero's seats are not claimed against the townsfolk; the
  ornithologist's quests could send the hero to the hide at dusk.
- **Quests**: the places are plain map places (found by walking up); no quest uses them.
- **Animals**: `tools/bird_habitats.py` and `tools/animal_map.py` are re-run after the land changed, but no habitat was
  drawn for the new land on purpose (kingfisher and dipper on the brook, bees at the hives, a heron at the mouth).

## Data to refresh after any change of the land here

`python3 tools/south.py --check && python3 tools/south.py`, `tools/transport.py`, `tools/ui_map.py`, `tools/ui_map_ink.py`
(LFS pictures: from the Mac), `tools/bird_habitats.py`, `tools/animal_map.py`, `tools/people_map.py`, then
`unreal/scripts/build_scene01.sh`.
