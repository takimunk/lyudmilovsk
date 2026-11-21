# Counters with real service

Backlog part 2, item 27. Buying at a counter is a small scene with a person behind the counter, and the town's counters
have lines that residents wait in.

## The scene at the counter (ULyudCounterService)

`Public/People/CounterService.h`, `Private/People/CounterService.cpp`, a world subsystem.

- **The shop opens** (`ULyudInventoryUI::IsShopOpen` goes true, from a city shop, a counter trade or a market stall). The
  nearest drawn person on a staff duty within 3.5 m of the hero is the one serving him. They nod (`ReactManner nod`) and
  then keep looking at him (`ANpcBody::LookAtPoint`, every 0.5 s) while he chooses.
- **Hello.** `ULyudCityThings::Greet` already says `counter_hello` when he steps up and tells the service
  (`NoteGreeted`). A shop opened any other way gets the hello from the service. The same person does not say hello again
  within 5 minutes. The `counter_hello` sets in `tools/barks/lines/09_counters.txt` choose a line in this order of weight:
  - a friend's line with his nickname (`bond = friend close`, weight 6);
  - `bond = friendly` (4);
  - `met = known`, for a resident who has greeted him on many days (3);
  - lines for the hour: morning 4–9, noon 12–15 and evening 18–4 (2 each);
  - lines for festival days (2);
  - the plain lines.
- **The handover.** When the shop shuts, `ULyudFoodSubsystem` hands over what he bought. Each item is now put on the
  counter: halfway between him and the server, at most 55 cm from his feet, about a metre up, with items side by side
  (`ULyudCounterService::HandoverPoint`). His hand then reaches for it (`UHeroReachComponent::Take`). If nobody served
  him, the old point in front of his eyes is used.
- **Money.** A toast counts it: «Итого 7 руб. С 10 руб. — сдача 3 руб.» or «Итого 5 руб., без сдачи.».
  - The note is the smallest one of the time that covers the total and fits his purse: 1, 3, 5, 10, 25, 50 or 100.
  - If one note is exactly the price, or none fits, he counts the money out and gets no change.
  - The rules are in `Private/People/CounterModel.h` and are checked by `tools/countersim/run.sh`.
- **After paying.** The server points at the counter and says `counter_give`. A friend says it with his nickname.

## The lines (the people's life)

- `tools/town/counters.py` is a post-pass on `Content/PeopleData/town_places.json`. It is idempotent, `--check` tests it,
  and `town_life.py` runs it at its end.
  - Every kiosk, shop and cafe visit slot that is within 2.5 m of a staff slot becomes the front of a line, q0.
  - The line's spots go back from the counter 0.85 m apart: 4 spots at kiosks and shops, 3 at cafes and the market.
  - Each spot carries `"queue"` and `"q"`. There are 39 lines with 134 spots.
- **The market (P-MARKET) fix.** Its sellers used to stand at random points of the hall, and its shoppers stood by the
  door, so `LyudBarks::AtCounter` (4 m) never paired them. Now each seller stands at a stall's seller spot
  (`shell_market.json` stallN) and the shoppers line up at that stall's counter spot (counterN).
  - The butcher gets the meat stalls (3 and 15).
  - The fishmongers get the fish stalls (6 and 11).
  - The other sellers take the remaining stalls in order.
- **Engine-free rules** (`Private/People/PeopleSim.cpp`):
  - A line fills from the front: a spot is open only when every spot before it is taken (`QueueOpenFor`).
  - `FWorld::StepQueues` does the rest:
    - the front is served for 60–90 s and then leaves;
    - the people behind step up as soon as the spot ahead frees, even while they are still walking to their own spot;
    - while waiting, nobody's activity ends, for up to 10 minutes.
- **The hero holds a spot.** The service tells `FWorld::SetHeroAt` four times a second where he stands, when he is on foot,
  not on a bike, and slower than 1.5 m/s. A spot under him counts as taken: residents line up behind him and never step
  in front.
  - If he joins behind others, a toast says how many are ahead.
  - Near him (15 m), the server calls «Следующий!» (`queue_next`) when the front changes, and a newcomer at the back
    asks «Кто последний?» (`queue_join`). Both are rate-limited.

## Checks

- `tools/peoplesim/run.sh town`:
  - lines form and move;
  - few person-minutes are spent behind an empty spot (under 30);
  - market shoppers stand at a stall with its seller (1328 person-minutes a day, it was 0);
  - the hero's spot at a kiosk is never taken;
  - a crowd sent to his line stands behind him.
- `tools/countersim/run.sh`: the note and the change, the place in a line, and `counters.py --check`.
- The words: `Content/Localization/counters/` (`ui.counter.*`, dev keys) and the barks
  (`tools/barks/build_barks.py`, events `counter_give`, `queue_next` and `queue_join`).

## Developer mode

- Console: `lyud.Counter status` logs the lines within 50 m. `lyud.Counter crowd [n]` sends n residents (3 by default) to
  the nearest line within 40 m.
- F7 → Actions → «Прилавки и очереди»: crowd, status.
- Teleports `city.w1_u07` (a bakery) and `city.p_market`. For a shot of a queue: teleport there, step up to the counter,
  run `lyud.Counter crowd 3`, and wait half a minute.
