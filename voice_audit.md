# Voice audit, 2026-11-01 (branch `voice-all`, from main 0871674e)

## What speaks in the game

Every spoken line is a `line.<speaker>.<scene>.<n>` key in `strings_<lang>.json`, played by `LyudText::LoadVoice`:
NPC dialogue (`Content/Dialogue/npc_dialogue.txt` → `NpcDialogue.cpp`), the hero (dialogue replies, `swim_breath`, `europa_planet`,
`cough.1-3`), and the cable car's announcements (`LyudTransport.cpp`, `LyudStationFurnish.cpp`). I cross-checked them:

- every key in `npc_dialogue.txt` has text in all three tables, and every key that the C++ plays exists;
- no spoken text is hard-coded in C++ (the only Cyrillic outside comments is a console-variable help string);
- `ui.tame.*` (`BirdTame::Say`) are on-screen hints, not speech; the quest/journal texts (`quest.*.log|done|desc`, `journal.*`)
  are written diary text, not voiced (see "Optional" below);
- every remote branch and worktree: the only line that isn't on main yet is `line.khuazhev_aslan.*` in `wt/food-signs` (below);
- there is no «ай» or other new hero exclamation on any branch: only the three coughs and two hero reactions, all voiced.

## Totals (main)

164 spoken lines × 3 languages = 492 takes. **130 lines are voiced in all three languages; 34 lines (102 takes) have no voice in any language.**
No line is missing a translation in en or ja, and no take is stale.

| speaker | who | lines | takes missing (ru/en/ja) | characters to send |
|---|---|---|---|---|
| zotov_lev | physicist, 58 | 2 | 2/2/2 | 731 |
| zotova_galina | chemist, 55 | 2 | 2/2/2 | 706 |
| sato_kei | spectroscopist, 34 | 2 | 2/2/2 | 737 |
| sato_mila | doctor, 33 | 2 | 2/2/2 | 694 |
| sato_yuki | girl, second grade (8) | 2 | 2/2/2 | 348 |
| korneev_ivan | pensioner, 74 | 2 | 2/2/2 | 852 |
| korneeva_nina | pensioner, 71 | 2 | 2/2/2 | 312 |
| belov_misha | technician, 29 | 2 | 2/2/2 | 741 |
| orlova_tanya | kiosk seller, 42 | 2 | 2/2/2 | 654 |
| lange_pyotr | fisherman, 47 | 2 | 2/2/2 | 660 |
| kimura_dasha | student, 20 | 2 | 2/2/2 | 603 |
| li_vika | lab assistant, 27 | 2 | 2/2/2 | 690 |
| valiev_rustam | engineer, 45 | 2 | 2/2/2 | 578 |
| nishimura_aya | painter, 39 | 2 | 2/2/2 | 686 |
| sorokin_oleg | ornithologist, 63 | 2 | 2/2/2 | 694 |
| guseva_valentina | corn seller, 67 | 2 | 2/2/2 | 311 |
| khuazheva_susanna | cheese seller, 54 | 2 | 2/2/2 | **skipped** (below) |
| **total to voice** | 16 speakers | 32 | 96 takes | **9 997** (ru 3 620, en 3 897, ja 2 480, incl. direction tags) |

None of these 17 speakers had a voice in `voice.json`: they would all have fallen back to `default` (Daniel, an adult man),
including the women and Yuki.

## Cost (fal, actual prices from api.fal.ai/v1/models/pricing)

**Chosen variant (Dima asked for cheaper, 2026-11-01): no voice design.** Yuki, Nina and Valentina use MiniMax's built-in
system voices (Lovely_Girl, Wise_Woman, Abbess) through speech-2.6-hd: **total ≈ $1.00** for all 96 takes. Their design prompts
stay in `voice.json` under `design_later` in case a built-in voice doesn't fit ($3 each then). Cheaper TTS models exist
(ElevenLabs turbo-v2.5 $0.05, MiniMax turbo $0.06, Chatterbox multilingual $0.025 per 1000 chars) but would save only ~$0.5 and
the new speakers wouldn't match the rest of the cast's quality (turbo-v2.5 doesn't act out direction tags), so they aren't used.

The first estimate, with designed voices:

- TTS: ElevenLabs v3 and MiniMax speech-2.6-hd are both $0.10 per 1000 characters → 9 997 chars = **$1.00**.
- New designed voices (`fal-ai/minimax/voice-design`): $3 each × 3 = **$9.00** (the preview audio is $30/1M chars, under a cent).
- **Total ≈ $10.** Without designing voices for the two grandmothers (giving them premade adult voices instead) it would be ≈ $4.
  A retake of a single bad line costs about $0.01.

## New voice assignments (`localization/voice.json`)

Same scheme as the cast so far: adults get ElevenLabs v3 premade voices with a direction tag; children get a MiniMax voice designed
from a prompt (the kid rule). ElevenLabs has no premade elderly women, so the two grandmothers also get designed MiniMax voices.
I used premade voices the club cast doesn't use wherever there were enough.

| speaker | voice | direction / design |
|---|---|---|
| zotov_lev | George | dry, thoughtful, unhurried, a physicist's quiet irony |
| zotova_galina | Alice | warm, chatty, fondly exasperated with her husband |
| sato_kei | Eric | quiet, precise, softly puzzled |
| sato_mila | Lily | brisk, caring, a doctor's matter-of-fact warmth |
| sato_yuki | **MiniMax built-in Lovely_Girl** | (design prompt kept as design_later) |
| korneev_ivan | Bill | an old man, slow, a little gravelly, contented |
| korneeva_nina | **MiniMax built-in Wise_Woman** | speed 0.95 |
| belov_misha | Liam | easygoing, amused, self-ironic |
| orlova_tanya | Aria | brisk, deadpan, a bit sassy |
| lange_pyotr | Brian | gruff, unhurried, few words |
| kimura_dasha | Charlotte | tired student, wry, self-ironic |
| li_vika | Laura | playful, quick |
| valiev_rustam | Roger | confident, dry, matter-of-fact |
| nishimura_aya | Matilda (also girl_a at the club) | calm, soft, contemplative |
| sorokin_oleg | River | retired scholar, calm, dry humour, slower |
| guseva_valentina | **MiniMax built-in Abbess** | |
| khuazhev_aslan (food branch) | Brian (shared with Pyotr) | hearty, generous, proud of the family cheese |

The design prompts avoid ages (fal's checker rejects them, docs/localization.md). The script records each designed voice's median
pitch; check it before trusting the takes (women ~220 Hz, children 250–300).

## Odd things found

1. **The station hall announcements are silent in the game.** `line.transport.hall_welcome|hall_lift|hall_cabin.1` have WAVs and
   sidecars in all three languages (commit 17dd3cfd) but were never imported: there is no `/Game/Voice/*/V_line_transport_hall_*`
   uasset, so `LoadVoice` returns null. That's 9 assets; no money needed, the import step of the script includes them.
2. **The cheese seller is about to change.** On main she's Susanna Khuazheva (54); `wt/food-signs` (not merged) replaces her with
   Aslan Khuazhev (56, male) with nearly the same lines under `line.khuazhev_aslan.*`. I marked `khuazheva_susanna` as `skip` so
   we don't pay for lines that are about to go. Aslan already has a voice entry, so once food-signs is merged the same command
   voices him (6 takes, 923 characters, ≈ $0.09).
3. Voice reuse: Matilda (Aya / girl_a) and Brian (Pyotr / Aslan) are each shared by two people in different places. There aren't
   enough premade adult voices for 25+ adults; a designed voice is $3 if a pair ever clashes.

## Optional, not in the estimate

The hero's diary/quest text (`quest.*.log|done|desc`, `journal.*`: 19 strings with placeholders) isn't voiced anywhere in the game.
Voicing it as the hero would be ≈ 3 300 characters ≈ $0.35 for three languages, plus code to play it. The `journal.*` strings take
`{0}` bird names, so they'd need one take per filled-in name or would have to stay unvoiced.

## How to generate (after Dima's OK)

```
tools/voice/voice_all.sh --cost   # free: the estimate above, recomputed from the tables
tools/voice/voice_all.sh          # (no voices to design now) voice 96 takes (ru, en, ja) -> import into this checkout's project
```

The script reads `FAL_API_KEY` from `~/projects/asmrx3000/.env` and never writes it anywhere. `--design` writes the new voice_ids
into `voice.json` and keeps each preview with its request in `audio/voice/design/<speaker>.wav|json`; every take gets its
`audio/voice/<lang>/<key>.wav|json` sidecar as before. The import waits for a UE queue slot and runs under
`nice -n 19 taskpolicy -b`, touching only the new keys plus the hall announcements (`IMPORT_ONLY` now takes a comma list).
Steps can be run separately: `STEPS=design`, `STEPS=voice`, `STEPS=import`.
