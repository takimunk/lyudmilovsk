# The town's post: postcards and letters

Item 11 of docs/interactives_backlog.md. The hero buys postcards at the kiosks (the `postcard` item already sold on the
kiosks' racks and the hall's foyer, with its icon), writes them at a street postbox, and gets letters in the club's own
mailbox. Every letter read is a diary page.

## How it plays

- **Writing a card.** At a blue street postbox (by each kiosk's postcard rack, on the square, by the sanatorium's
  brochure rack and in the hall's foyer) E opens the chooser: whom to (people the hero has met, plus the open homes'
  owners once he has been near their home), the picture (8), an opening and a wish (6 + 6 warm phrases), then a preview.
  E posts it: one postcard leaves the pocket, a diary line is written, and a toast says on which day it will be carried.
- **Delivery by the calendar.** Boxes are emptied at 17:00 on post days (Monday to Saturday); the card is delivered at
  11:00 on the next post day. A card posted on Saturday evening arrives on Tuesday, not on Sunday.
- **What the card does.** The recipient puts it on the shelf of their open home (a postcard stands on the shelf board),
  thanks the hero in the street within 4 days when he passes by (a bark, `line.letters.thanks.N`), and replies: a
  correspondent of the open homes answers every card (2 replies each, then an invitation); a townsperson replies with a
  chance by age (55%/70%/85%), the same every playthrough (a hash, not a dice).
- **The club's mailbox** (dark green, on the club's wall by the door) is filled at 10:00 on post days: replies, thanks
  after quests (13 quests, from the people who gave them), invitations to holidays (2 days before), the council's notices.
- **Tonya.** Tonya Lotsmanova, 14, daughter of the keeper of the White Cape lighthouse far north, birdwatcher. She first
  writes 2 days into the game if the hero has met a bird (else on day 5). Each of her 8 letters asks something: a bird,
  a place, a fish, a bird friend. At any postbox the hero answers from what he really did (birds seen in the bird book,
  places found, fish caught, birds befriended), or "nothing special yet"; her next letter acknowledges his answer by
  name. Mail takes 3 post days each way. After 10 days without an answer she sends a short nudge.
- **Reading.** Letters open page by page on the city page (A/D turn, E next letter, Esc close). A first read writes a
  diary entry; old letters can be reread from the mailbox's menu.

## Files

| What | Where |
| --- | --- |
| Rules, engine-free | `Source/.../Private/Letters/LettersModel.{h,cpp}` (namespace `LyudLetters`) |
| Engine side | `Public/Letters/LyudLetters.h`, `Private/Letters/LyudLetters.cpp` (`ULyudLetters`, `ALyudPostbox`) |
| Words and content | `tools/letters/content.py` (all ru/en/ja, Russian first) |
| Builder | `tools/letters/build_letters.py` → `Content/Letters/letters.json`, `Content/Localization/letters/strings_*.json` |
| Offline tests | `tools/lettersim/run.sh` (tests), `run.sh story [ru|en|ja]` (a whole autumn of mail, printed) |

Regenerate with `python3 tools/letters/build_letters.py`; never edit the json by hand. `--check` fails on stale output.
Both rows are in `tools/run_all_suites.sh` (`lettersim.*`, `gen.letters`).

The postal addresses come from the city plan (terrace = street, plots numbered along it; wall dwellings take the wall's
street and their door number). The model is saved as the flat map "Letters".

## One E, one thing

The postbox is offered only in a tight cone (14.5°, 2.4 m) with a clear line of sight and no door in view. While it is
offered or a chooser is up, `ULyudCityThings` picks nothing; while a city thing has its mode up, the postbox is not
offered. Keys are polled, never consumed.

## Console and developer mode

`lyud.Letters.Report`, `lyud.Letters.Deliver` (everything on its way arrives now), `lyud.Letters.PenNext`,
`lyud.Letters.Mail` (open the mailbox anywhere), `lyud.Letters.Post <key|h_id> [picture]`, `lyud.Letters.KnowAll`.
F7 group "Post": deliver now, Tonya's next letter, 5 postcards, know everybody; teleports to the club's mailbox and to
the postbox by kiosk 1.

## Tests (tools/lettersim)

The calendar (post days, Sundays skipped, month and year edges); delivery and reply days; the words of every key in the
three languages; a 75-day autumn (all 8 of Tonya's letters, 12 cards delivered, nothing arrives on a Sunday, the
correspondent's replies, quest thanks, invitations before festivals); save and load give the same state; street thanks;
the nudge; answer options in order; a late start without birds; a month's jump of the clock.

## Known gaps

- The engine side was written in the cloud and has not been compiled yet: the first Mac build may need small fixes.
- The boxes are placed at run time by traces round the kiosks; their spots want one look in the game.
- The boxes and the card on the shelf are built from the engine's basic shapes; no new item, so no new icon.
