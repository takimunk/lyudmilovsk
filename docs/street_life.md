# The street's everyday life

The town's streets and yards through the warm half of the year and the turns of the seasons: the kvass barrel and the
ice-cream cart that come out on a hot afternoon, the morning street (the yard keeper's broom at dawn, the milk round,
the shutters going up before the shops open), yard life (the washing on the lines on a fine wash-day, the grandmothers
on the benches by the entrances, dominoes at the yards' tables in the evening) and the seasons (the autumn's leaves raked
into piles, the spring subbotnik with its green paint and whitewashed trunks). No new areas: all of it happens on the
promenade, the square and the nine yards that are there already (docs/yard.md, docs/town_life.md).

It is the people's life switched by a few conditions. The game works the conditions out from the date, the hour and the
weather; an activity or a duty with `"when"` is only done while its condition is on, and the street's things (the barrel,
the cart, the washing, the tables, the piles, the white bands) are shown while theirs holds.

## Files

| What | Where |
|---|---|
| The town add-on (places, activities, people, things, the city things' places, teleports) | `tools/town/street_life.py` (`--check`; first in `town_life.py`'s chain) |
| The conditions from the date and the weather, plain C++ | `Source/Lyudmilovsk/Private/People/StreetLifeModel.h` (`LyudStreet::PlanDay`, `Conditions`) |
| Game: the conditions to the people, the things shown and hidden | `Public/People/LyudStreetLife.h`, `Private/People/LyudStreetLife.cpp` (`ULyudStreetLife`, `ALyudStreetThings`) |
| The things' meshes (CityKit, built in code) | `Private/City/CityKitStreet.cpp`: `kvass_barrel`, `icecream_cart`, `laundry_line_posts`, `laundry_washing` (3), `domino_table`, `trunk_white`, `leaf_pile` |
| Where the things stand | `Content/City/street_props.json` (generated) |
| The stalls' shops, the hero's acts, the collection | `tools/city/content/x_street.json`, `strings/x_street.json` (then `tools/city/interiors.py`) |
| The yard keeper's broom and rake | `blender/humans/props.py` (`broom`, `rake`; EProp `Broom`, `Rake`, held as `Shaft`) |
| Manners at the new activities | `Content/PeopleData/manners.json` |
| Barks | `tools/barks/lines/18_street_life.txt` (keys `town.street_*`) |
| Diary | `tools/daylog/content.py`, topic `street` |
| Developer mode's words | `Content/Localization/street/strings_<lang>.json` |
| Tests | `tools/streetsim/run.sh` (the model), `tools/peoplesim/run.sh street` (the town) |

## The conditions

`ULyudStreetLife` (a world subsystem, Game and PIE) looks every five game seconds. Once a morning it plans the day from
the weather's forecast (`ULyudWeatherSubsystem::GetStateAt`), then it works out the conditions and hands only the changed
ones to `UPeopleLifeSubsystem::SetCondition`.

| condition | when |
|---|---|
| `street_hot` | May to September, a day whose afternoon (12, 14, 16 h) will be over 22 C and dry at 14 h: 10:00-19:00, packed up while a shower (0.5 mm/h) lasts |
| `street_sweep` | March to November, not while it snows: the dawn sweep (winter's snow is shovelled, docs/people.md) |
| `laundry_hang` | a wash-day (Monday, Thursday, Saturday, April to October) with a fine morning (dry at 9, under 0.75 cloud, 8 C and over, no rain to 11): 9:00-9:30 |
| `laundry_out` | from 9:20 until the washing is in (the things only) |
| `laundry_take` | the first quarter hour with rain within 45 minutes in the forecast, else 18:00 (17:00 on a hot day), for 24 minutes |
| `yard_grannies` | April to October, 10:00-12:30 and 16:00-20:00, dry, 10 C and over |
| `yard_dominoes` | May to September, 17:30-21:30, dry, 14 C and over |
| `leaf_raking`, `leaf_piles` | 20 September to 15 November: raking 8:00-11:30 (not in a shower or snow), the piles lie 8:30-18:00 |
| `subbotnik` | the third Saturday of April, 9:00-13:00 |
| `trunks_white` | from ten on the subbotnik morning (its first hour done) to the end of October |
| `prom_evening`, `prom_dances`, `prom_band`, `hall_dances` | the promenade's summer evenings, the dances on the square, the Sunday brass band and the hall's own dances' months (docs/promenade.md) |

Nothing is saved: it all follows from the date and the weather. Moving the clock or the weather by hand: `lyud.Street.Replan`.

## The hot day

The barrel stands at the promenade's west end by the steps (`P-STREET-KVASS`, kind `kvass_stall`), the cart on the
square's sea side (`P-STREET-ICECART`, `icecream_stall`). Each has a vendor resident with a duty there from 10 to 19
(Egle Morales at the barrel, Astrid Moreau at the cart) that only holds while `street_hot` is on: on a cool day they stay
at home. A queue of four slots runs from each counter (`"queue"`, docs/counters.md), so a line forms and fills from the
front. Residents with a liking come for kvass or ice cream on a hot afternoon (`buy_kvass`, `buy_icecream`: 11:30-18:40,
20 C and over); afterwards they walk about with a paper cup or an ice cream in hand for ten minutes.

The hero buys at the counter while the vendor stands there (`needs_staff`; the stall's city thing is hidden while it is not
out, `ULyudCityThings::SetPlotHidden`): kvass 6, a can of kvass 15 (`kvass_can`), plombir 28, an eskimo 22, fruit ice 12
(`fruit_ice`).

## The morning street

- The yard keeper (Tyler Kovalenko) sweeps the promenade every day from 5:30 to 7:24 with a birch broom, a round of spots
  along it (`sweep_street` at `S-SWEEP-ROUND`).
- The milkwoman (Nour Hoffmann) goes round the yards' doors from 6:00 to 7:30, Monday to Saturday, with her bag
  (`deliver_milk` at `S-MILK-ROUND`).
- Whoever opens a shop on a weekday morning (an opening between 6:00 and 10:30) puts up the shutters in front of it a
  quarter of an hour before (`open_shutters` at `S-FRONT-<shop>`, first in their duty list).

## Yard life

Every yard (`yard_01` ... `yard_09`) has a line with posts and a laundress (`S-YARD-xx-LAUNDRY`). On a fine wash-day she
hangs the washing out at nine; it hangs on the line (three variants of sheets, shirts and towels) until she takes it in
before the forecast's rain or in the evening. The grandmothers (most women of 62 and over, a few men) sit on the benches by
the entrances, knitting or reading the paper, and remark on the hero when he passes. In the evening the men of 45 and over
play dominoes at a table in each yard (`S-YARD-xx-DOMINO`, four seats). The hero can sit in (`street_dominoes`).

## The seasons

Autumn: the yard keeper rakes the leaves round the yards from 8 to 11, Monday to Saturday (`rake_leaves` at
`S-LEAF-ROUND`), and the piles lie in the yards through the day. The hero can take his rake for a while (`street_rake`,
five roubles from the keeper, once a day).

Spring: on the subbotnik morning the residents come out in their yards (`S-YARD-xx-WORK`): they paint the benches green
(`subbotnik_paint`), whitewash the trunks (`subbotnik_whitewash`) and sweep last year's leaves (`subbotnik_sweep`). The
trunks stay white to the end of October. The hero can whitewash a trunk too (`street_whitewash`, once a day).

## In the game

The collection «Улица летом» (`street`): a mug of kvass, an ice cream at the cart, a trunk whitewashed, a pile of leaves,
a game of dominoes in a yard. The diary writes a line on such a day (topic `street`).

Developer mode, group «Улица»: the actions `street.hot`, `street.sweep`, `street.laundry`, `street.laundry_take`,
`street.grannies`, `street.dominoes`, `street.leaves`, `street.subbotnik` (each holds its condition on, again off), `street.auto`
(all back to the weather and the date), `street.report` (the day's plan to the log); the teleports `street.kvass`,
`street.icecart`, `street.sweep`, `street.yard`, `street.dominoes`. Console: `lyud.Street.Force <cond> 1|0|-1`,
`lyud.Street.Auto`, `lyud.Street.Report`, `lyud.Street.Replan`.

## Tests

- `tools/streetsim/run.sh`: the model. A hot July day and a cool one, a wash-day with rain at 14 (taken in before it), a
  dry wash-day (taken in at 18), a winter morning, the autumn's leaves, the subbotnik's date over many years, a year of
  conditions (one subbotnik, no barrel in winter, the washing only on wash-days).
- `tools/peoplesim/run.sh street`: the town. Vendors at their stalls all a hot day and none on a cool one, buyers and a line
  filled from the front, the dawn sweep, the milk round and the shutters, no sweeping in winter, the washing hung in all
  nine yards and taken in before rain, the grandmothers and the dominoes only while their conditions hold, the autumn's
  raking, the subbotnik (dozens at once), no slot overbooked.
- `python3 tools/town/street_life.py --check` and the other add-ons' `--check`.

## Owed

- The broom and the rake meshes: `Blender -b -P blender/humans/props.py`, then `PROPS=broom,rake` with
  `unreal/scripts/import_props.py`. Until then the sweep and the raking use the mop's manner with the mop.
- The icons `icon_kvass_can`, `icon_fruit_ice`.
- A compile, a look at the things in place (the ground trace puts each on the ground), a bench pass.
