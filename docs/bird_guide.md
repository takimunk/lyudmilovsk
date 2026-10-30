# The bird guide

«Птицы Людмиловска», an ornithological guide: a bottle-green cloth-bound book sold at the Academgorodok library's desk. Its
observation map shows the live birds of the bird life (docs/bird_life.md), but only within `lyud.BirdGuide.Radius` (300 m) of the
hero: what is near him, as it moves, flies off and comes back, never the whole map. Once it is in the pocket, the diary's map (M) gets
the same birds as a layer.

## Buying it

Olya (`girl_b`), the librarian, sells it over the desk while she is at work (`work_library`: Wednesday to Saturday, 10 to 15, at her
staff seat). Stand by her (3.2 m), look at her: "T — trade", T opens the shop (`ULyudInventoryUI::OpenShop`), 60 roubles.

The sellers are data: `Content/PeopleData/counters.json` (`ULyudCounterTradeSubsystem`, `Public/People/CounterTrade.h`), a list of
`{person, activity, title, reach_cm, goods: [{item, price, count}]}`. A seller sells only while doing that activity at its place
(state `doing`). Other shops (the grocery, the kiosk) add a row. `lyud.Trade.Open <person>` opens a counter from anywhere (checks).

## Reading it

B (or "Open the guide" on the diary's Things page, which puts the diary away first) opens the guide (`ULyudBirdGuideScreen`,
`Private/UI/LyudBirdGuide.cpp`): the printed map held round the hero (`SLyudMapView` with `bGuide`), the land beyond the radius faded
out, each bird a mark in its species' colour (its most colourful plumage tone): a dot when it sits (a ripple under it on the water,
pale asleep), a small "v" pointing the way it flies with a short wake. Pointing at one names it and what it does. The wheel zooms; the
map stays on the hero. On the right: the species near, with how many (and how many flying), refreshed twice a second. B or Esc shuts
it; like a shop it does not pause the game.

## On the diary's map

`ULyudBirdGuide` (game instance, saved as "BirdGuide") keeps `bMapLayer`. With the guide in the pocket and the layer on, the diary's
map draws the same birds in pencil-toned colours and the radius as a dashed pencil ring; the sticky note has a "birds on the map:
hide / show" line. `lyud.BirdGuide.MapLayer 0|1` sets it from the console.

## Checks

`lyud.BirdGuide.Open` (gives the guide if missing, opens it), `lyud.BirdGuide.Report` (the birds the guide sees, by species).
`unreal/scripts/test_bird_guide.py` shoots the guide and the diary map with the layer.
