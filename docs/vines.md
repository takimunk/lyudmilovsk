# Climbing vines

Virginia creeper (*Parthenocissus quinquefolia*, девичий виноград пятилисточковый), the vine on old houses
all along the Baltic coast, grows on the courtyard (west) face of Novikova 1 in the Academgorodok.

## How it grows

`AProceduralVine` (`Source/Lyudmilovsk/{Public,Private}/Plants/ProceduralVine.*`) stands at the foot of a wall,
its +X out of it. Every shoot is a chain of 12 cm steps over whatever static collision is there:

* each step is traced onto the surface (the adhesive pads hold anything opaque; translucent surfaces,
  glass, are never held);
* a face in the way (an inner corner, a sill, a cornice's underside) is climbed onto;
* an outer corner is wrapped round;
* where the wall stops but goes on a little further (a window or door hole, up to 2.6 m) the shoot slides
  along the edge, the way that climbs more, instead of crossing the hole;
* where there is nothing at all (a roof edge, the top of a window) it hangs free for up to 0.7 m;
* it does not creep along the ground, nor run over a ledge or roof for more than 1.2 m.

Shoots turn up toward the light (side shoots less), wander, branch (side shoots on side shoots, up to
order 5; a bud breaks a season or so later; one in five turns down to fill the wall below) and steer clear of
wall another shoot already holds (20 cm cells), giving up when boxed in, so the plant spreads until the wall
is full. Stems thicken by the pipe model (the stem they carry) and with age.

It is a pure function of (species, seed, age at the calendar's start), the date and the walls. Growth happens
in the growing season only (`GrowthAt`: late May to August), leaves follow the species' seasons through
`PlantModel::Season` (bronze in spring, crimson then scarlet from late September, bare by early November,
only the stems in winter), blue-black berries hang on the side shoots from September into January.

The growth is simulated in time order and only as far as the age needs, at most `TraceBudgetMs` (2 ms) a frame,
so a loaded wall costs no hitch; it appears once caught up (~0.1–0.2 s of traces in all per plant, a few seconds
of frames). `MaxNodes` (9000 steps) caps a plant's cost.

Species presets: `VineSpecies.cpp` (`FVineSpecies`: the growth rules plus an ordinary `FPlantSpecies` for leaves,
bark, berries, seasons and colours).

## Where they are

`UVinePlantings` (`Plants/VinePlantings.*`) plants them when play begins, by finding the house mesh
(`SM_Novikova1`) and tracing to its wall from a point given in the house's own frame, so no level edit is
needed and the vines follow the house wherever the world build puts it. They carry the tag `Academ`
(`lyud.Academ 0` hides them with the house). Now: the two end bays of the courtyard face, 5 m clear of its doors.

## Console

* `Vine.ShowAt <age> <day>`: hold every vine at an age and day of the year (no arguments: the calendar).
* `Vine.Report`: each vine's state (steps, shoots, leaves, berries, wall covered, wraps / slides / hangs,
  simulation time and traces).
* `lyud.Vines 0/1`: hide or show them (A/B benchmarks).

## Checks (offscreen, no window)

```sh
VINE_CMDS="lyud.SkyHourOffset -3.5" unreal/scripts/shoot_vine.sh       # seasons, ages, close and far
VINE_ROUNDS=4 VINE_SHOTS=ab_on,ab_off,ab_close_on,ab_close_off unreal/scripts/shoot_vine.sh   # frame time A/B
```

Shots land in `unreal/Lyudmilovsk/Saved/VineShots/<name>.png`.
