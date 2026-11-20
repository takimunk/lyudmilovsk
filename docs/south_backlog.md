# The Aton coast: not done yet

What [south.md](south.md) leaves open, the nearest first. Everything here is a proposal.

## Looks

- **The brook's water** is the North's river ribbon (`ALyudNorthRiver`) with its material: it inherits that river's open
  points (docs/north_backlog.md: the streaks at a low sun, no foam at the stones). The fall in the ravine is only the
  ribbon on a steeper bed: no white water, no sound of its own.
- **The headland's south side** (the tunnel's portal face, Y -30) is still the old straight terrace edge; only the north
  and east brows wander. The railway's cutting sits in it, so it needs the railway's thread.
- **Words on the boards** are Russian textures by default (`M_South_Text_<id>`, actors tagged `Text:south`); swapping
  them with the game's language is not wired (the North's boards have the same gap).
- **The alley** from the club's door to the station is as it was (poplars and maples in pairs). Lamps, a kiosk, a bus
  shelter of the sea railway's halt at the stair's foot are ideas, not built.
- The **Mayak station**'s surroundings and the cliff walk have a bench and a pine; a real lighthouse ("Маяк") is the
  obvious landmark for the south end and is not built.

## Game

- **Nothing here is used yet**: the coin viewer does not zoom (it could open the binoculars), one cannot drink at the
  spring or fill a bottle, the benches and the hide's bench are not seats (the manifests carry `seats`, nobody reads
  them for the South), the apples are not picked, the boats do not move.
- **People do not walk it.** `tools/people_map.py` and the town's schedules know nothing of the trails: a fisherman by
  the boat shed, a beekeeper in the orchard, walkers on the shoulder, a birdwatcher in the hide at dusk would make it a
  place. The hide looks straight at the starlings' roost: the ornithologist's quests could send the hero there.
- **Quests**: the places are plain map places (found by walking up); no quest uses them.
- **Animals**: `tools/bird_habitats.py` and `tools/animal_map.py` are re-run after the land changed, but no habitat was
  drawn for the new land on purpose (kingfisher and dipper on the brook, bees at the hives, a heron at the mouth).

## Data to refresh after any change of the land here

`python3 tools/south.py --check && python3 tools/south.py`, `tools/transport.py`, `tools/ui_map.py`, `tools/ui_map_ink.py`
(LFS pictures: from the Mac), `tools/bird_habitats.py`, `tools/animal_map.py`, `tools/people_map.py`, then
`unreal/scripts/build_scene01.sh`.
