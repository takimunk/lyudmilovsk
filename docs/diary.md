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
the book. Spreads in order: Things, To do, the diary's entries (as many as they take, measured), Map. The index tabs stick out of
whichever side their section lies on; the dog-eared bottom corners and A / D turn a page, a tab or another key riffles through
several. A turning page (`SLyudDiaryFlip`) is a strip of the two spreads' pictures bent round its spine edge in perspective, shaded
as it faces away, with its shadow on the page under it.

Everything on the paper is handwriting: Caveat (ru, en) or Yomogi (ja) in ink blue and graphite (`LyudDiaryStyle.*`: the colours,
the fonts, the paper, and the hand-drawn marks: underlines, circles, ticks, strikes, highlighter, arrows, each with its own wobble).
What can be clicked is underlined when the mouse is over it; what is chosen is circled or highlighted. Things are pencil sketches
of their icons; done quests are struck through; the followed one has an arrow; the map is in ink with blue-pencil sea and red-pen
marks, and a yellow sticky note says where the hero is.

## Assets

| Asset | Made by | Import |
|---|---|---|
| `textures/ui/diary_paper_left.png`, `_right.png` | `tools/ui_diary_paper.py` (cream paper, 5 mm dot grid, fibres, the spine's shade) | `/Game/UI/Diary/T_PaperLeft`, `T_PaperRight` |
| `textures/ui/map_ink.png` | `tools/ui_map_ink.py` (from `tools/world_map.py` and `tools/ui_map.py`; rerun after the world map changes) | `/Game/UI/Diary/T_MapInk` |
| `textures/localized/<lang>/sketch_<id>.png` | `tools/ui_diary_icons.py` (a pencil filter over each item's icon; every new item: rerun it for that id) | `/Game/Inventory/Localized/<lang>/T_Sketch_<id>` |
| `fonts/Caveat-*.ttf`, `fonts/Yomogi-Regular.ttf` (OFL, `fonts/README.md`) | Google Fonts | `/Game/UI/Diary/Fonts/FF_*` |
| `audio/sfx/diary/diary_{take,open,close,page,pages}_NN.wav` | `tools/gen_sounds.py diary_take ...` (audio/sounds.json) | `import_audio.py` → `/Game/Audio/Diary` |

`UnrealEditor-Cmd Lyudmilovsk.uproject -run=pythonscript -script=unreal/scripts/import_diary_assets.py` imports the pictures and
the fonts and makes `M_DiaryPage`. Without them the diary still works: plain paper, the default font, a blank page on the book.

## Checks

`unreal/scripts/test_ui.py` shoots the diary being taken out, opening, a page turning, every section in three languages, and
putting it away.
