# Town buildings

The town of Lyudmilovsk (plan: [city_plan.md](city_plan.md)) is built of original buildings in the spirit of
20th-century experimental architecture: Brazilian modernism, constructivism, brutalism, Japanese metabolism, shell
structures. None is a copy of a real building; each module names what inspired it.

## How a building is made

* `blender/city/citykit.py`: the kit. A `Building` collects the shell (`B.m`: exact collision, Nanite), details
  (`B.detail`: no collision), window cards (`B.window`), flats (`B.flat`), street doors (`B.entrance`), places for the
  life simulation (`B.spot`) and real lamps in enterable rooms (`B.lamp`).
* `blender/city/citymats.py`: the material slots (`CT_*`), shared by the Blender preview and Unreal.
* `blender/city/buildings/<type>.py`: one module per type (`INFO`, `build(B)`, optional `VARIANTS` per plot).
  Helpers: `_surrounds.py` (aprons, benches, bike racks, lamp posts at doors), `_furn1.py`, `../furn_int2.py`,
  `../furn_int3.py` (furniture), `_sheet.py` (thin curved shells), `_cons_util.py`.
* `blender/city/build_city.py -- <types|all> [--no-render]`: builds `blender/export/city/<type>[__<plot>].glb` (not
  committed) and `.json` (committed: footprint, entrances, spots, flats, windows, lamps), day and night previews in
  `renders/city/`, and a lint (triangles, stretched UVs, degenerate faces, coplanar faces lifted apart). It runs with
  Blender 5.x or with pip's `bpy`.

Frame of a building: metres, x along the street front (to the right seen from the street), y inland, z up; origin =
the plot's `front` point in `city_plan.json`; Unreal yaw = plot yaw − 90.

## Windows and lights

Every window is one opaque card (`M_City_Window`): dark glass by day, a lit room with a curtain at night. UV 1 of the
card points at one texel of a 64 × 64 texture per building (R light, G lamp warmth, B curtain). `ALyudCityBuilding`
holds the meshes and the window table (`WindowIds`, `WindowKinds`, `WindowFlats`, filled by the import);
`ULyudWindowLights` writes a texel only when a light changes and uploads changed buildings in batches (at most four
times a second), never once per call. Slot row r sits at Blender v = (r + 0.5) / 64; glTF flips v, so the C++ writes
it into texture row 63 - r (`TexelOf`).

* Flats: the town life (`UPeopleLifeSubsystem`, branch town-life) calls `SetDriven(true)` and then
  `SetFlatLit("<plot>/<flat>", bOn)` (by reflection). Only windows it has set are its own; every other flat keeps
  the seeded evening (lights on at dusk when the household is home, off between 21:30 and 01:30, some early risers).
  If the town life goes away (`lyud.People 0`, no subsystem) or calls `SetDriven(false)`, all flats return to it.
  The hour comes from `ALyudSky::ShownTime()`, so windows and sun agree.
* Stair windows: dusk to dawn. Public rooms: open hours (`HOURS` in `import_city.py`).
* Lamps (`B.lamp`, ~1900 in town): buildings own no light components. A shared pool of at most
  `lyud.City.LampBudget` (default 24; with public spaces' lyud.City.StreetLights 12 a shared 36 until GPU numbers) movable point lights without shadows is handed every 0.5 s to the lit lamps
  nearest the camera within 45 m (fading over the last 6 m). A held lamp keeps its light unless another is 4 m nearer,
  a newly lit one comes up over two updates; lights that keep their lamp are not touched. Public lamps
  burn in open hours (0..24 = always), lobby/stair ones dusk to dawn.

Console: `lyud.City 0|1`, `lyud.City.Lights 0|1|2` (auto / all on / all off), `lyud.City.LampBudget N`, `lyud.City.Report`
(windows lit per building, lamps in reach, pool lights on).

## Unreal

```sh
Blender -b -P blender/city/build_city.py -- all --no-render
unreal/scripts/import_city.sh                 # import (/Game/City) then place on the plots of Lvl_Scene01
```

`unreal/scripts/shoot_city_buildings.py` (quits on a Python error) takes offscreen day and night shots of a plot from the street.

## The types

| Type | Name | Style | Flats | Windows | Lamps | Variants | In the spirit of |
|---|---|---|---|---|---|---|---|
| bus_stop_kiosk | Остановка-киоск | late-soviet expressionism | 0 | 2 | 1 | 1 | Soviet concrete bus stops of the 1960s-80s (folded-plate roofs, mosaic panels, wave walls) |
| capsule_cluster | Капсульный кластер | metabolism | 24 | 58 | 17 | 1 |  |
| capsule_library | Библиотека-капсулы | metabolism | 0 | 57 | 21 | 1 |  |
| capsule_tower | Капсульная башня | metabolism | 127 | 157 | 3 | 2 | Japanese Metabolism of the early 1970s: prefabricated steel capsules bolted to concrete cores |
| clinic | Поликлиника | modernism | 0 | 34 | 11 | 1 | 1960s European health centres: white ribbon-window blocks with glazed stair towers |
| cobogo_house | Дом с решёткой-кобого | brazilian modernism | 16 | 82 | 10 | 1 |  |
| cog_club | Клуб «Шестерня» | constructivism | 0 | 98 | 20 | 1 |  |
| commune_slab | Дом-коммуна | constructivism | 16 | 102 | 10 | 2 | Soviet transitional-type communal housing of 1928-30 (Ginzburg, Narkomfin; skip-stop corridors, pilotis, ribbon windows, communal block on a bridge) |
| cube_houses | Кубические дома | experimental / postmodern | 5 | 35 | 3 | 2 | Tilted cube houses of the Netherlands (1980s): cubes on hexagonal pillar cores over a pedestrian street; ours are edge-down prisms in an arc with colour and height variation |
| culture_block | Дом культуры и спорта | brazilian brutalism | 0 | 115 | 13 | 1 | Lina Bo Bardi's SESC Pompeia (Sao Paulo, 1977-86): cave-hole towers, flying bridges, brick workshop shed with saw-tooth roof; massing, positions and details are our own |
| department_store | Универмаг | Soviet modernism | 0 | 54 | 36 | 1 | Soviet Univermag buildings of the 1960s-70s (arcade + glazed ground floor, mosaic/grille upper floor, roof sign) |
| hyperboloid_tower | Гиперболоидная башня-маяк | constructivism | 0 | 8 | 5 | 1 | Russian hyperboloid lattice towers of the 1890s-1920s (straight steel bars between rings) - original proportions and use |
| lifeguard_tower | Спасательная вышка | resort modernism | 0 | 6 | 1 | 1 | concrete beach watch towers of the 1960s-70s Mediterranean and Baltic resorts |
| origami_row | Дом-оригами (ряд складчатых домов) | experimental folded-plate | 7 | 70 | 16 | 1 |  |
| pilotis_curve_slab | Изогнутый дом на V-опорах | brazilian modernism | 20 | 86 | 14 | 1 |  |
| plain_block | Жилой дом (пятиэтажка) | modernism | 20 | 140 | 4 | 5 | late 1950s-70s European social housing slabs with loggias and brick stair towers |
| ribbon_corner_block | Дом с лентой (угловой конструктивистский дом) | constructivism | 48 | 181 | 24 | 1 |  |
| round_sanatorium | Санаторий «Корона» | soviet modernism | 120 | 493 | 9 | 1 | 1980s round resort buildings of the Crimean / Black Sea coast (ring sanatoria on stilts), Soviet late-modernist concrete with painted accents; the sawtooth crown of angled balcony bays and all proportions are our own |
| saucer_pavilion | Кафе «Летающая тарелка» | late-Soviet experimental modernism | 0 | 74 | 20 | 1 | 1970s Kyiv 'flying saucer' pavilion and Soviet space-age café architecture; here a lens-shaped saucer on splayed legs with a spiral glazed core |
| school | Школа | modernism | 0 | 177 | 13 | 1 | mid-century courtyard schools: Scharoun, Aalto, Soviet ribbon-window standard schools |
| serpentine_slab | Змеевидный дом на опорах | brazilian modernism | 210 | 831 | 3 | 1 | Brazilian modernism: Reidy's serpentine Pedregulho, Niemeyer's pilotis, Rio brise-soleil, cobogó screens, azulejo-tiled stair towers |
| shell_chapel_hall | Дом собраний «Своды» | brazilian modernism | 0 | 25 | 15 | 1 |  |
| shell_market | Рынок под оболочками | mexican shell modernism | 0 | 0 | 16 | 1 | 1950s Mexican concrete umbrella shells (inverted hypar umbrellas on one column) - original layout and proportions |
| stacked_beam_tower | Дом-мост (балочная башня) | brutalism | 36 | 192 | 6 | 1 | 1970s Soviet / Georgian 'space city' ideas (Tbilisi linear and bridge houses), Kenzo Tange's Yamanashi cores, Moshe Safdie stacking; massing, proportions and details are our own |
| stacked_boxes | Дом-соты (штабель коробок) | brutalism / prefab megastructure | 104 | 508 | 3 | 1 | 1967 Montreal stacked-box housing (prefab concrete boxes heaped in a terraced pile with roof gardens); here: canyon stair-streets, retaining-wall backdrop, towers and bridges of our own |
| terrace_riser_housing | Террасный дом у лестницы | brutalism / metabolism | 24 | 62 | 3 | 2 | Japanese hillside concrete-terrace housing of the 1970s-80s (stepped gardens, public stairs between terraces) crossed with stacked-box ideas |
| twin_cylinder_house | Дом из двух цилиндров | constructivism | 4 | 174 | 0 | 2 | Konstantin Melnikov's 1920s cylinder-house experiments in Moscow (hexagonal window honeycomb), rationalist avant-garde of the 1920s |
| unite_block | Жилая единица (бруталистский дом на опорах) | brutalism | 196 | 935 | 4 | 1 | early-1950s Marseille housing unit (Le Corbusier): pilotis, loggias with coloured sides, shopping street at mid-height, roof terrace with sculpted forms; original proportions and bay system |
| wave_canopy | Волнистый навес ярмарки | brazilian modernism | 0 | 5 | 39 | 1 | 1950s Brazilian park marquees: a free-form concrete slab on slender columns over a promenade - original plan and section |
| wave_tower | Башня-Волна | brazilian modernism | 142 | 951 | 35 | 1 | 1950s-60s Sao Paulo / Rio residential towers with undulating brise-soleil, azulejo cores, shopping arcades under glazed bases (Niemeyer / Reidy / Artigas era); original design |
| workers_club | Рабочий клуб (кинотеатр) | constructivism | 0 | 34 | 27 | 1 | Soviet workers' clubs 1927-30 (Melnikov's clubs, Golosov's Zuev club: cantilevered auditorium volumes, signage bands, raw concrete and red) |

Flats and windows counted on the first variant of a type.
