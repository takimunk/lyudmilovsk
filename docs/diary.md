# The diary

The game screen is the hero's diary: an A4 hardcover planner (oxblood leatherette, cream dotted paper, four coloured index tabs
on the fore-edge) that he carries in his bag. Everything the inventory, the journal and the map showed is written in it by hand, in
pen and pencil. Keys and pages: docs/ui.md.

## Taking it out

Tab / I, J or M (or `Screen.Open <section>`) runs one move, first person (`ULyudGameScreen`, the phases in `EPhase`):

| Phase | Seconds | What |
|---|---|---|
| TakeOut | 0.55 | the right hand brings the book up from the bag, shut, its spine down in the palm; the eyes go down to a reading angle (pitch −30..−10) |
| Opening | 0.5 | the front board swings open round the spine (quick at first, settling flat), the pages already showing the spread asked for |
| FadeIn | 0.18 | the screen's live pages fade in exactly over the 3D book's paper |
| Reading | | the pages are the screen: click, turn, read |
| FadeOut, Closing, PutAway | 0.14, 0.35, 0.45 | back: the live pages fade into the book's picture of them, the book shuts, goes down, the eyes come back up |

`lyud.Diary.Speed` scales it all. Another section's key turns the pages to it; the open section's key or Esc puts it away. A bird
(no hands) or the hero hidden in a seat: no book, the pages just fade in and out in the middle. Drinking or eating from the pocket
puts the diary away first and then the same hand takes the drink (`ULyudGameScreen::CloseThen`).

The book is `ALyudDiaryBook` (`Public/UI/LyudDiaryBook.h`), built in code (rounded boxes for the boards and page blocks, quads for
the open pages, the tabs). It is placed in the eyes' frame so the open spread takes `lyud.Diary.Height` (0.84) of the screen's
height; its four paper corners are projected to put the screen's pages exactly over them. The hand holds it through
`UHeroReachComponent::SetOverrideHold` (palm under the spine, fingers behind the book, curled round its real shape). The open
pages' material `/Game/UI/Diary/M_DiaryPage` is unlit and undoes the eye's exposure, showing a picture of the screen's pages
(drawn into a render target by `FWidgetRenderer`) so the book and the screen that takes over from it look the same
(`lyud.Diary.PageBright`, `lyud.Diary.PictureGamma`, `lyud.Diary.PictureScale`).

## The pages

`SLyudDiaryScreen` (`Private/UI/SLyudDiary.*`) lays out a spread of two A4 pages (840 × 1188 units, 40 per cm) and scales it onto
the book. Spreads in order: Things, To do, the diary's entries (as many as they take, measured), Birds, Map. The index tabs stick out of
whichever side their section lies on; the dog-eared bottom corners and A / D turn a page, a tab or another key riffles through
several. A turning page (`SLyudDiaryFlip`) is a strip of the two spreads' pictures bent round its spine edge in perspective, shaded
as it faces away, with its shadow on the page under it.

Everything on the paper is handwriting: Caveat (ru, en) or Yomogi (ja) in ink blue and graphite (`LyudDiaryStyle.*`: the colours,
the fonts, the paper, and the hand-drawn marks: underlines, circles, ticks, strikes, highlighter, arrows, each with its own wobble).
What can be clicked is underlined when the mouse is over it; what is chosen is circled or highlighted. Things are pencil sketches
of their icons; done quests are struck through; the followed one has an arrow; the map is in ink with blue-pencil sea and red-pen
marks, and a yellow sticky note says where the hero is.

## Birds

The fifth tab (B, lilac): the hero's bird book (`ULyudBirdBook`, `Public/Birds/BirdBook.h`, saved as "BirdBook"). A card for each
species he has met, in the order he met them, three to a page: its drawing (pencil and watercolour, the female's for a species whose
female looks different when she is the one met), the name and the Latin name, the day and place it was first met and how. Under it,
in green, each befriended bird of that species: male or female, the day it first ate from his hand, how often it has come, and
**call**.

Met (once per species, a line in the diary and the quest event `bird_seen <Species>`):
- the binoculars held on a bird for 1.5 s (within 2.5 degrees of the middle, 150 m, nothing in between);
- a bird coming to the palm with feed (whether it eats or not);
- being a bird of that species.

A friend (per bird, bird life's id; `bird_tamed <Species>`): a bird that ate from the palm.

**Call** puts the diary away and the hero holds out his empty palm (`UBirdTameSubsystem::Summon`). Bird life
(`UBirdLifeSubsystem::SummonBird`, the sim's `FWorld::Summon`) sends that very bird to the palm: it ignores the player's moves,
alarms and hawks on the way; one further than 80 m comes in from out of sight, 45 m behind and 18 m above the hero. It lands, sits a
moment while the view narrows onto it, and the view goes into it, as after a befriending. F gives up the call; nobody comes after
45 s; it can't come under a roof or while it is away for the winter (the note says so). The birdsim test checks a great tit from 25 m
(on the palm in ~7 s) and from 600 m (~9 s).

Drawings: `design/birds_diary/sheets.json` (6 Higgsfield `nano_banana_2_1` sheets, 3 x 2 cells, one style text; every run pinned
there), cut by `tools/ui_diary_birds.py` into `textures/ui/birds/bird_<Species>[_f].png` (the paper made transparent against
its own white fitted across the cell, as some sheets come on faintly tinted paper; each drawing taken as its own blots of pencil
and paint, so a tail or wing tip over the cell's edge stays whole and a neighbour's twig stays out; `--check` also writes a contact
sheet), imported as
`/Game/UI/Diary/Birds/T_Bird_<name>`. A species without a drawing shows `T_Bird_Unknown` (an unfinished outline). New species (the
big birds): add a sheet, generate, rerun.

Console: `Birds.Book`, `Birds.Meet <Species> [tame]`, `Birds.Call [n]`, `Birds.Forget`, `Screen.Open birds`, `Screen.Turn [n]`.
Check: `unreal/scripts/test_bird_diary.sh`.

## Assets

| Asset | Made by | Import |
|---|---|---|
| `textures/ui/diary_paper_left.png`, `_right.png` | `tools/ui_diary_paper.py` (cream paper, 5 mm dot grid, fibres, the spine's shade) | `/Game/UI/Diary/T_PaperLeft`, `T_PaperRight` |
| `textures/ui/map_ink.png` | `tools/ui_map_ink.py` (from `tools/world_map.py` and `tools/ui_map.py`; rerun after the world map changes) | `/Game/UI/Diary/T_MapInk` |
| `textures/ui/birds/bird_<Species>[_f].png` | `tools/ui_diary_birds.py` (cut from the Higgsfield sheets of `design/birds_diary/sheets.json`) | `/Game/UI/Diary/Birds/T_Bird_<name>` |
| `textures/localized/<lang>/sketch_<id>.png` | `tools/ui_diary_icons.py` (a pencil filter over each item's icon; every new item: rerun it for that id) | `/Game/Inventory/Localized/<lang>/T_Sketch_<id>` |
| `fonts/Caveat-*.ttf`, `fonts/Yomogi-Regular.ttf` (OFL, `fonts/README.md`) | Google Fonts | `/Game/UI/Diary/Fonts/FF_*` |
| `audio/sfx/diary/diary_{take,open,close,page,pages}_NN.wav` | `tools/gen_sounds.py diary_take ...` (audio/sounds.json) | `import_audio.py` → `/Game/Audio/Diary` |

`UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_diary_assets.py` imports the pictures and
the fonts and makes `M_DiaryPage`. Without them the diary still works: plain paper, the default font, a blank page on the book.

## Checks

`unreal/scripts/test_ui.py` shoots the diary being taken out, opening, a page turning, every section in three languages, and
putting it away.
