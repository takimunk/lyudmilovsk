# Lyudmilovsk: the town plan

The town runs along the beach and climbs inland in big concrete terraces, the way Japanese hillside towns do: a
promenade over the sand, the lower town on the Academgorodok's level, then four more terraces about 7 m apart. Each
terrace is held by a concrete retaining wall with flats built into it: doors and windows in the wall face, lit at
night when someone is home, and stairs running up along the face. The old flat valley is gone.

**Source of truth:** `tools/city_plan.py` (pure Python) -> `unreal/Lyudmilovsk/Content/City/city_plan.json`.
Every thread reads the JSON; ids are stable (move a plot freely, never reuse an id). Regenerate after any change:

    python3 tools/city_plan.py                    # the JSON
    python3 tools/city_plan.py --plot plan.png    # a top-down picture of the plan (not committed)

The ground itself: `tools/world_map.py` `height()` calls `city_plan.height()`, so the terrain build
(`tools/terrain.py`, `import_terrain.py`) draws the terraces; `keep_flat` keeps them exact (no natural detail).

## Frame

World map metres (`docs/world_map.md`): X west = toward the sea, Y north, Z up, origin at the club's portal.
Unreal cm = `(X*100, portal_face_y + Y*100, Z*100)` with `portal_face_y` = 790 (in the JSON). Every point in the JSON
has `map` (metres) and `ue` (cm). **yaw** = Unreal degrees the front faces: 0 = +X = toward the sea, 90 = north.

The plan is drawn in a *terrace frame* `(s, y)`: `s` = metres inland from the high-water line at that Y
(`world_map.inland`), `y` = map Y. The coast runs almost straight north here, so terraces are bands of `s`.

## The terraces

The town spans map Y 380..1450 (1.07 km along the coast) and fades into the land 45 m past both ends.

| Id | Name | Ground Z (m) | Band (s, north of Y 820) | Band (s, south of Y 700) | What is there |
|---|---|---|---|---|---|
| B | Пляж / Beach | -43..-41.5 | 0..25 | 0..25 | sand, lifeguard towers, tidal pools, the pier's root |
| T0 | Набережная / Embankment | -39.5 | 25..70 | 25..70 | the promenade over a 2 m seawall, kiosks, benches, cable car «amber» |
| T1 | Нижний город / Lower town | -36.2 | 70..330 | 70..560 | Sun Square + fountain, restaurant, market, fair, park, culture house; the Academgorodok and the diner in the south |
| T2 | Вторая терраса | -29.0 | 330..480 | 560..710 | serpentine house, school, clinic, cube houses |
| T3 | Третья терраса | -22.0 | 480..680 | 710..860 | Wave tower, sports centre, cable car «zenit» |
| T4 | Четвёртая терраса | -15.0 | 680..830 | 860..1010 | capsule tower, Unité block |
| T5 | Верхняя терраса | -8.0, then rising 5% after 220 m | 830.. | 1010.. | sanatorium, bridge house, the lookouts; rises into the inland ridge |

Between Y 700 and 820 all four upper walls swing seaward round the Academgorodok (whose ground is T1 up to s ~540):
the **Cascade**, a diagonal run of walls with a grand stair and water steps beside it.

### The walls

| Wall | Holds | Height | Face |
|---|---|---|---|
| SEA (s 25) | T0 over the beach | 2 m | plain concrete seawall, steps down to the sand every 80 m |
| W1 (s 70) | T1 over T0 | 3.3 m | an arcade of small shops and cafés opening onto the promenade (`promenade_units`) |
| W2..W5 | each terrace over the one below | 7 m | flats in the wall: two storeys, a door and window below, two windows above, per 7.5 m bay (`wall_dwellings`) |

A wall is a concrete block `WALL_DEPTH` = 8 m deep whose face is at `wall_s(i, y)`; behind the face the landscape
climbs from the lower terrace to the upper over 6 m, hidden inside the block; the block's top is flush with the
upper terrace. Wall stairs run along the face (two flights and a landing, 2.4 m wide) about every 110 m, offset from
wall to wall so the way up zigzags; grand stairs (8-10 m wide) go straight up on the main axis (Y 905) and on the
Cascade (Y 760). Windows: warm light at night when the flat is occupied (`UPeopleLifeSubsystem::OnWindowLight`).

### Built by

- `tools/city_mesh.py`: the walls bay by bay (faces with recessed doors and windows, door canopies, danchi balconies on
  every third flat, painted steel doors, parapets), the wall stairs, the grand stairs, the seawall with its steps and
  kerb, and the pier on piles. `tools/world_mesh.py` puts them into the world's .glb (`import_world.py`), chunked per
  wall and per 60 m of coast (`City_Walls_W3_07`, ...). It replaced the old promenade strip.
- `ALyudCityWindows` (`Source/.../World/LyudCityWindows.*`), spawned by `ULyudCityWindowsSubsystem` in Lvl_Scene01: the
  lit window panes. The town's life drives them with `ULyudCityWindowsSubsystem::SetWindowLit(WindowId, bLit)` or
  `SetFlatLit(FlatId, bLit)`; every window it has not set keeps its own evening schedule (dusk to a bedtime 21:30..01:30,
  some again before dawn; shops lit 9..21 in the dark). Console: `lyud.CityWindows 0` off, `lyud.CityWindows.All 1`
  every window lit. Nothing per frame: checked every 2 s, instance buffers rebuilt only on a change.

### Lanes and lamps

Along the foot of W2..W5 runs a lane: asphalt from the face out to 7 m (`LANE_W`) with a kerb on its outer edge, so
the flats' doors open onto a street. A concrete street lamp stands on the kerb every fourth bay (~30 m; `lamps` in the
JSON, ids `LAMP-W3-07`); ALyudCityWindows lights their heads whenever it is dark. The landscape's ramp behind each wall
starts 1.1 m behind the face (`RAMP_START`), so no 1 m landscape cell that touches the face lifts the ground in front.

## The axis

The town's main axis is map **Y 905**: the pier and its lighthouse -> the promenade -> **Sun Square** with the
fountain -> grand stairs up W2..W5 -> the cable car's «zenit» station on T3 -> the bridge house and the lookouts
on T5. Sunset over the sea lines up with it in summer.

## Plots

`plots` in the JSON: `id`, `kind` (workplace kind or residential/park/square), `building` (a type key from
`building_types`: the buildings thread's catalog in `blender/city/`), `terrace`, `map` = the plot's middle at ground
level, `front` = the middle of its front edge (the buildings thread's origin), `yaw` (the front faces), `w` (along
the terrace) x `d` (inland) metres, `height_max`, `staff` (hints for the life thread), names ru/en, notes.

| Id | Kind | Building (after) | Terrace |
|---|---|---|---|
| P-PIER | pier | hyperboloid_tower: Shukhov-like lattice lighthouse at the pier head | sea |
| P-KIOSK1..6 | kiosk | bus_stop_kiosk: Soviet sculptural shelters | T0 |
| P-LIFEGUARD1, 2 | lifeguard | lifeguard_tower | beach |
| P-TIDALPOOL | pool | tidal_pool: Siza's Leça pools | beach |
| P-SQUARE | square | open; fountain S-FOUNTAIN, stage S-STAGE | T1 |
| P-RESTAURANT | restaurant | amber_pavilion: the restaurant thread's «Янтарь» | T1 (at W1, over the promenade) |
| P-CAFE | cafe | saucer_pavilion: Kyiv's «flying saucer» | T1 |
| P-MARKET | market | shell_market: Candela's hypar umbrellas; the town's grocery | T1 |
| P-STORE | shop | department_store: glazed floors, mosaic facade | T1 |
| P-FAIR | fair | wave_canopy: Niemeyer's Ibirapuera marquise | T1 |
| P-PARK | park | open: lawns, pond, playground | T1 |
| P-CLUB2 | club | workers_club: Melnikov's Rusakov club; cinema, dances | T1 |
| P-T1H1..5 | residential | cube_houses, twin_cylinder_house, plain_block | T1 |
| P-SERPENTINE | residential | serpentine_slab: Reidy's Pedregulho, 260 m along W3 | T2 |
| P-SCHOOL, P-CLINIC | school, clinic | school, clinic | T2 |
| P-T2H1, P-T2H2 | residential | cube_houses (Blom), commune_slab (Narkomfin) | T2 |
| P-WAVE | residential | wave_tower: Niemeyer's Copan, the landmark from the sea | T3 |
| P-T3SPORT | culture | culture_block: Bo Bardi's SESC Pompéia | T3 |
| P-T3H1..3 | residential | twin_cylinder_house (Melnikov), stacked_boxes (Habitat 67), commune_slab | T3 |
| P-CAPSULE | residential | capsule_tower: Kurokawa's Nakagin | T4 |
| P-UNITE | residential | unite_block: Le Corbusier's Unité | T4 |
| P-T4H1, 2 | residential | plain_block | T4 |
| P-SANATORIUM | hotel | round_sanatorium: Druzhba, Yalta | T5 |
| P-BRIDGEHOUSE | office | stacked_beam_tower: Tbilisi's Ministry of Highways | T5 |
| P-T5H1 | residential | capsule_tower | T5 |
| P-RISER1, 2 | residential | terrace_riser_housing: Ando's Rokko housing, climbing W2 and W4 | T1/T2, T3/T4 |

The architectural references are **proposals** (Dima asked for unusual historical buildings from anywhere:
brutalism, constructivism, Brazil); the buildings thread designs original buildings after them.
Existing: the **Academgorodok** (Laboratory No. 2, Gastronom, library) and the **diner** stay where they are on T1.

## Spots

`spots`: places people gather, sit or watch; the life and holiday threads use them by id: `S-SQUARE`,
`S-FOUNTAIN` (the rim is a bench, 24 seats), `S-STAGE`, `S-PIER-HEAD` (fishing, sunsets, fireworks),
`S-PROMENADE`, `S-BENCH-P01..26` (promenade benches every 40 m), `S-BENCH-01..10` (the grannies' benches by
entrances, the square, the park), `S-BEACH`, `S-BONFIRE`, `S-FAIR`, `S-PARK`, `S-POND`, `S-PLAYGROUND`,
`S-SUNRISE` (dawn lookout on T5, faces inland/east), `S-SUNSET` (the edge of W5 over the town and the sea),
`S-CASCADE` (water steps: the stepped channel beside each Cascade grand stair, `cascade` in the JSON, built by city_mesh, water by the public spaces).

## Links

The club (the hero's computer club on the spur) is reached by the cable car from the Academgorodok station, by the
old stair up the spur's north slope from the town's south end (`L-CLUB-STAIR`, 36 m), and along the beach. Cable car
stations in town: «academ» (T1), «amber» (T0), «zenit» (T3). See `links` in the JSON.

## Who builds what

| Part | Thread |
|---|---|
| This plan, the terrain, the terrace walls with their flats, windows and stairs, the window lights | city plan / terraces |
| The buildings on the plots (`blender/city/`, a `flats.json` per type) | unusual buildings |
| Promenade paving and furniture, park, fountain, square, planting | promenade, park and fountain |
| The restaurant on P-RESTAURANT | restaurant |
| Townsfolk, schedules, homes, jobs (reads this JSON + the buildings' flats) | town life |
| Holidays (venues = spots) | holidays |

## Also changed

- `world_map.forests()`: the valley's dune bank, wet copses and sparse town trees are gone (the terraces stand
  there); the town is planted by its own plants.
- `world_map.keep_flat`: the whole town keeps the exact terrace height.
- The cable car's pylons must be re-resolved on the new ground (`tools/transport.py`), the people's walk grid
  rebuilt (`tools/people_map.py`), the forest floor audit rerun.
