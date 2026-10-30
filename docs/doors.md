# Doors

Every door in the game is one of two classes that share one core, `ALyudDoorBase`
(`Source/Lyudmilovsk/Public/Doors/LyudDoor.h`):

- **`ALyudDoor`**: the standard door, built in code from a few numbers. Use it for every new doorway.
- **`ASwingDoor`**: a door whose leaves are actors modelled with their building (the club's blast door, the hall's
  partition). Keep it for unique doors; it behaves exactly like the standard one.

## What every door does

| | |
|---|---|
| Player | **E** while looking at the door (the shut leaf, the open hole or the open leaf) within 2.2 m opens or shuts it. E is polled by `ULyudDoorSubsystem`, never consumed, and goes to the one door the view meets first. `ASwingDoor` keeps its old rule: anywhere in its `Zone`. |
| NPCs | Any pawn the player doesn't control, and any actor registered with `ULyudDoorSubsystem::RegisterAgent` (animals, extras), gets a door opened when it walks up to the doorway (within 1.6 m, moving toward it). A door that was shut is shut again behind it once the doorway is clear (`bCloseBehindNpcs`). |
| Planners | `FindDoorOnSegment(From, To)` finds the door on a straight move; `RequestPassage(Agent)` opens it and holds it, `IsPassable()` says when the leaves are fully open, `GetWaitPoint(From, Radius)` is where to stand meanwhile (out of the sweep), `ReleasePassage(Agent)` lets it shut. |
| Navmesh | Leaves never affect navigation: a navmesh runs through doorways and the agent opens the door on arrival. |
| Nobody gets shoved | Leaves ignore pawns while they swing and block them only at rest. A door never shuts on a body standing where its leaves sweep: it waits until they step out. |
| Locks | `bLocked`: nobody opens it (E plays `LockedSound`), NPCs treat it as a wall. `SetLocked`. |
| Save | `bOpen`, `bLocked` (and whether an NPC opened it) are saved through `ILyudSaveable`, by actor name, and snap back on load. |
| Events | `OnOpening`, `OnClosing`, `OnOpened`, `OnClosed` (who did it); `OpenSound`, `CloseSound`, `ShutSound`, `LockedSound`. |

## Sizes

Every doorway passes the character standard (`CharacterStandard.h`): clear **108 × 210 cm or more, leaves open**.
The standard sizes (clear, leaves open):

| `Size` | clear | leaves | hole in the wall (with the door's lining) |
|---|---|---|---|
| `Standard` | 110 × 215 | 1 | 120 × 218 |
| `Wide` | 130 × 215 | 1 | 140 × 218 |
| `Double` | 150 × 230 | 2 | 164 × 233 |
| `Custom` | `ClearWidth` × `ClearHeight`, never below 108 × 210 | 1 or 2 | clear + linings + leaves |
| `FitHole` | what `HoleWidth` × `HoleHeight` gives; `bBelowStandard` and a warning if under 108 × 210 | 1 or 2 | given |

How the door is built (actor frame: origin on the floor in the middle of the hole and of the wall, **+X out of the
wall toward the side the leaves swing to**, Y across): a 3 cm lining inside the hole, a 7 cm casing on both faces
(`bBuildFrame`, off when the building models its own frame), the 4 cm leaf in the lining flush with the +X face,
hinged on its edge and swinging out by `OpenAngle` (95°). The open leaf stands across the opening by its thickness:

    hole width  = clear width + 2 × lining + leaves × leaf thickness
    hole height = clear height + lining

Scripts get the same numbers from `tools/character_standard.py`: `door_hole(clear_w, clear_h, leaves, frame)`,
`door_clear(hole_w, hole_h, leaves, frame)`, `door_hole_for("Standard" | "Wide" | "Double")`. Without the door's own
frame a 110 cm hole gives 106 cm: **interior doorways need holes of at least 112 cm** (120 cm for the standard 110 clear
with the frame).

`Hinge` is Left or Right as seen standing on the +X side facing the doorway. `LeafMesh` (stretched to the leaf box:
x thickness, y width, z height), `LeafMaterial`, `FrameMaterial`, `HandleMaterial`, or `LeafColor` / `FrameColor` on the
engine's basic material.

## Placing doors

- By hand: drop `LyudDoor` into the level at the bottom middle of a hole, +X toward the room it swings into, pick the
  size. Cut the hole from the table above.
- From a builder: write the doorways into the layout (`at`, `out`, `hole`, `wall`, `leaves`, `hinge`, `frame`, `open`,
  `locked`, in the builder's frame and metres) and call `unreal/scripts/place_doors.py` `spawn_doors(entries, to_unreal)`
  from the importer; it fits each door to its hole and reports the ones below the standard.
- Doors spawned at run time must get a fixed name (`FActorSpawnParameters::Name`): saves find doors by actor name.

## Console

`lyud.Door.List [name part]`, `lyud.Door.Toggle <name part>`, `lyud.Door.Lock <name part> [1|0]`.

## Test

`unreal/scripts/test_doors.sh` (offscreen, leaves running editors alone) builds `/Game/Dev/L_DoorTest`
(`make_door_test_level.py`, not committed; `REBUILD=1` to rebuild it) and runs `Door.SelfTest`: sizes, the shut leaf
blocking, E opening only the door in view and shutting it from the other side, a 108 × 210 body sweeping through every
open door, the hero walking through, no shutting on the hero in the sweep, NPCs (one using the planner API, one that just
walks into the door from the swing side) going through and the doors shutting behind them, a locked door stopping both
E and an NPC; then save, flip every door, load, `Door.CheckSaved`. Screenshots land in
`Saved/Screenshots/MacEditor/door_*.png`.

`unreal/scripts/test_club_exit.sh` walks the hero out of the club in `Lvl_Scene01` (hall → open blast door → 6 m onto
the forecourt and back, never jumping; `CLUBEXIT_WINTER=1` with 35 cm of forced snow), logs what the feet stand on along
the door's axis, fails on any engine ball or default-material primitive round the hall, and takes four shots into
`Saved/clubexit_shots/`. The forecourt must stay at the club's floor level: `world_map.forecourt_level` holds the
upland's north-slope tail off it (it once stood 0.7 m over the threshold and the head met the portal lintel).
