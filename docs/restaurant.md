# The restaurant «Янтарь» on the promenade

A sit-down restaurant in the lower town, over the promenade and the sea. The hero comes in, sits down at a table, opens the menu card,
orders. Bread and water come first, free. Each dish is cooked for its own time, and a waiter carries it from the kitchen pass on a
tray and sets it down at his place. He eats seated, bite by bite: a spoon, chopsticks, the fingers, a fork with a knife in the other
hand, a cup to the lips. The empty plates are cleared away, and the bill is brought when he asks for it. Townspeople eat here too.

## The building

**Ресторан «Янтарь»** ("Amber") is a 1960s–70s Baltic Soviet-modernist seaside pavilion crossed with Niemeyer. A round, fully glazed
drum stands under a thin, wide, slightly upturned concrete disc roof on twelve slender tapering columns. A solid kitchen block clad in
amber ceramic stands behind it, and the whole building sits on a low round podium with steps. The name «ЯНТАРЬ» stands in tall
constructivist letters on the roof's rim over the main door and burns amber at dusk while the restaurant is open. Its relatives are
the Soviet seaside restaurants of the 1960s ("Поплавок", "Чайка") and Niemeyer's pavilions.

- **Frame** (`blender/restaurant/building.py`): metres. The origin is on the hall's floor at the drum's centre. +x faces the sea (the
  terrace side), -y is the main door, z is up. The glTF export turns Blender y into Unreal -Y; `ALyudRestaurant::ToWorld` does the
  conversion.
- **Hall**: a 24-sided drum, radius 10 m, ceiling 4 m. Floor-to-ceiling bronze-framed glazing stands on a low concrete sill. A straight
  chord wall at x = -8.4 holds the **kitchen pass** (a stainless shelf with heat lamps over it) and the kitchen door. Inside are ten
  tables (six round four-seaters along the glass, four square two-seaters inside them), opal pendants over each table, a bar along the
  chord wall, a cloakroom counter by the door, and a central column with a round bench.
- **Kitchen**: a box behind the chord wall (range and hood, worktable, sink, fridge, shelves) with the staff's back door.
- **Doors**: ALyudDoor holes of 1.22 × 2.22 m (clear 1.10 × 2.15, `CharacterStandard`): the main door, the kitchen door and the back
  door.
- **Meshes**: `SM_Rest_{Shell, Inside, Glass, Furniture, Lamps, Sign}`. Material slots are `RS_*` (`restaurant_common.MAT_SPEC`). The
  lamp shades `RS_Opal` and the letters `RS_SignGlow` are emissive slots the game drives.

## Where

On the city plan's plot **P-RESTAURANT** (`Content/City/city_plan.json`, `docs/city_plan.md`): terrace T1 (the lower town, ground
-36.2 m), at the wall W1 over the promenade (T0, -39.5 m), facing the sea (yaw -3.6°). `Content/Restaurant/restaurant.json` names the
plot (`"plot"`) and the podium height (`"podium": 0.6`, the floor over the plot's ground). Everything reads its position from there:
`import_restaurant.py place`, `tools/restaurant/seats.py` and the test. **Moving the restaurant means changing the plot in the city plan
or `"plot"`.**

## Pipeline

```
Blender -b -P blender/restaurant/building.py        -> design/restaurant/game/SM_Rest_*.glb, Content/Restaurant/layout.json
Blender -b -P blender/restaurant/tableware.py       -> SM_RT_* (tableware), SM_RF_<dish>_<k> (bite stages), SM_RF_Load_<dish>_S|B,
                                                       SM_RF_<dish>_Fill; design/restaurant/tableware.json
Blender -b -P blender/restaurant/pack.py            -> design/restaurant/game/restaurant_kit.glb
Blender -b -P blender/restaurant/pack_tableware.py  -> design/restaurant/game/tableware_kit.glb
Blender -b -P blender/restaurant/preview.py         -> design/restaurant/previews/*.png
Blender -b -P blender/transport/lint_mesh.py -- <glb>   (faces on faces: must be 0)
UnrealEditor-Cmd ... -run=pythonscript -script="unreal/scripts/import_restaurant.py import"   (meshes, MI_RS_*, tableware.json)
UnrealEditor-Cmd ... -run=pythonscript -script="unreal/scripts/import_restaurant.py place"    (into Lvl_Scene01; import_world.py too)
python3 tools/restaurant/strings.py      the strings (ru/en/ja) of the UI and the dishes
python3 tools/restaurant/icons.py        the menu icons from one 4x4 Higgsfield grid (design/restaurant/icons_grid.json, 1 credit)
python3 tools/restaurant/seats.py        Content/Restaurant/seats.json for the town life
```

## In the game: `ALyudRestaurant` (Public/Restaurant/LyudRestaurant.h)

| Data | What |
|---|---|
| `Content/Restaurant/restaurant.json` | the plot, the podium, the hours (while the town life has no staff here), `wait_scale`, the bread delay, clearing and table-reset times, the stand-in waiter, the festival hook |
| `Content/Restaurant/menu.json` | the dishes: item id, meshes' name, section, price (₽), cook seconds, bite sizes, free/festival |
| `Content/Restaurant/tableware.json` | per dish: the vessel, where the food sits, the fill, the utensil, the loads, the stages (from Blender) |
| `Content/Restaurant/layout.json` | the building: tables, seats, plate places, the pass, the waiter's spots, the aisle graph, lamps, doors |

**Sitting.** Inside, standing by a free chair of a free table (the hero's four tables; the others are the townspeople's) shows
`[E] Сесть за столик`. E sits him down through the hero's own seat (`AHeroCharacter::SitOn`, the train benches' sit: hips onto the
cushion, the camera at the seated eyes), looking at the table. E (or a step, or Space) gets him up beside or behind the chair, wherever
the standing capsule fits (`SetStandUpOffset`), never into the table.

**The menu.** Looking at the closed menu card on the table, LMB opens it (`SLyudRestMenu`). It is a printed card on the diary's cream
paper with the amber band and the name, the dishes by section in two columns (icon, name, a line about the dish, price), and − / +
for how many. The footer shows the order's sum, the money in the pocket and what is already ordered. «Заказать» pays and orders,
«Попросить счёт» asks for the bill, Esc closes it. The money is taken when ordering.

**Prices.** Starters 45–90 ₽, soups 55–85, mains 70–150 (steak), desserts 30–45, drinks 15–20; bread and water are free.

| Dish | Utensil | Cook (s) |
|---|---|---|
| Оливье, балтийская рыбная тарелка | fork | 40, 50 |
| Борщ | spoon | 60 |
| Рамэн | chopsticks | 80 |
| Пельмени, тальятелле | fork | 75, 80 |
| Хачапури по-аджарски | fingers (tear and dip) | 100 |
| Стейк с картофелем | fork + knife in the left hand | 120 |
| Сырники, медовик | fork | 55, 40 |
| Пломбир | teaspoon | 35 |
| Кофе, морс, вода | cup | 30, 25, — |
| Хлебная корзинка | fingers | — |

**The waiter.** While open, a waiter stands by the bar. The waiter is `UPeopleLifeSubsystem::StaffOnShift("restaurant", "waiter")`,
the town life's staff (t_sokolov_ilya, t_petrauskaite_ruta). Before that branch lands, a stand-in is drawn with
`restaurant.json standin.mesh`. While on shift the restaurant draws their body (`SetPresentedExternally`; the life hides its own). The town life's three calls go through `Private/Restaurant/LyudRestaurantStaff` by name (reflection on their UFUNCTIONs), so the restaurant builds and runs with or without that branch.
Dishes ready at the pass (up to four at a time) go onto a round steel tray held by both hands under its rim (the bike rider's arm IK,
`FBikeRiderPose` on `ULyudBodyAnimInstance`). The waiter walks the hall's aisle graph (layout `walk`, Dijkstra) to the table's serving
spot, turns to it and sets each dish down at its place in an arc, the right hand on the dish's rim. The main plate goes in front of the
seat, a cup to the right, a second plate to the left, the bread and the water toward the table's middle. Empty plates are cleared
`clear_after_s` after they were finished. The bill comes in a folder once everything is served.

**Eating** (`ALyudRestDish`, `ULyudBiteSubsystem`): looking at a dish on his table, LMB eats it. A click is a small bite, holding is a
big one, and keeping it held goes on eating. The food is swapped through its bite-stage meshes `SM_RF_<dish>_0..4`, and soups and
drinks go down (the fill). New in the shared bite mechanic: `ELyudUtensil::Chopsticks` (held like a pen, the pinch hanging under the
tips) and `ELyudUtensil::Fingers` (a torn piece between the thumb and fingers), `FLyudMeal::LoadOffset`, `ScoopPhase()`. The steak's
knife is held in the hero's left hand (`UHeroAnimInstance::Table`, the bike rider's arm IK) and saws while the fork is in the meat.

**Open, lights, night.** The restaurant is open while a waiter is on shift; before the town life has the restaurant's staff, it opens
by `hours` (11–23). The pendants, hall lamps and kitchen light are lit while it is open, and the soffit's ring of downlights and the
sign at night. When closed and empty, the outer doors are locked (never with the hero inside).

**Townspeople** eat here at the tables that aren't the hero's (`tools/restaurant/seats.py` → `Content/Restaurant/seats.json`, the
town plan's `workplaces[].seats` form, `"hero": true` on the hero's four). Whoever of the life sits at a seat with an `eat*` activity
here gets a dish before them (a prop, partly eaten).

**Festival days** (`ULyudFestivalSubsystem::IsFestivalToday`, the holidays thread, called by name when it exists; or
`lyud.Restaurant.Festival 1`) add the festival section to the menu.

**Console**: `lyud.Restaurant.State`, `.Open 1|0|-1`, `.Sit [seat]`, `.Stand`, `.Menu [0]`, `.Order <id>..`, `.Bill`, `.Eat [id]`,
`.Finish [id]`, `.Look x y z`, `.WaitScale x`, `lyud.Restaurant 0|1` (all hidden: `tools/bench_systems.sh`), `lyud.Restaurant.Festival`.

## Checks

`unreal/scripts/test_restaurant.sh` runs offscreen at low priority. `REST_STEPS` takes any of
outside, sit, menu, order, waiter_walk, serve, eat, clear, bill, night; `REST_LANG` is ru, en or ja, and `REST_WAIT` scales the cooking
waits (0.12 by default). It prints RESTTEST lines and writes the shots to `Saved/RestaurantTest`.
