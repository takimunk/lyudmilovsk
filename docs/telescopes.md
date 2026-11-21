# Telescopes

Looking through a telescope is a view, not a line of text: a round eyepiece on the game's own frame, aimed by hand, with
a focus wheel and four eyepieces, showing the sky as it stands that night.

## Where

Every thing whose act has `"screen": "scope" | "scope_coin" | "scope_sun"` (tools/city/content):

| Kind | Eyepieces | Things |
|---|---|---|
| `scope` | ×3, ×12, ×60, ×400 | café «Тарелка» gallery (`tarelka_telescope`), the lighthouse tower (`px_scope`), the library pods (`library_scope`), the corner block's roof (`rcb_telescope`), the Santos loft and other home telescopes (`_home_scope`) |
| `scope_coin` | ×3, ×12, ×60 | every other `what=telescope` (the promenade and bridge house coin telescopes, act `telescope`, 1 rouble) |
| `scope_sun` | ×3, ×12, ×60 | Daisuke Girma's telescope (`girma_telescope`): a dark solar filter, only the sun shows, with its spots |

The act runs as before (price, sound, quest event `do <act>`, the `did` mark), then `ULyudCityThings::Run` hands the thing
to `ULyudTelescope::Begin`; the act's line of the hour is the caption under the eyepiece for the first seconds. The act's
minutes do not pass: the game's clock runs at its own pace while he looks.

## The view

* The picture is the game's own: the view target moves to a camera at the telescope with the eyepiece's field (15°, 4°,
  0.8°, 0.12°). The sky dome, Europa and the moon with its phase, the clouds, the aurora, the sea, the island, the ships,
  the town and the birds are the real ones. No scene capture, no second render.
* Drawn over it (Slate, `SEyepiece`): the eyepiece's black mask, the planets where they really stand, Jupiter's four
  moons, Saturn's ring, Venus' phase, faint field stars (a hash grid, more of them at higher power), the filter's tint,
  the white-out.
* Mouse: the tube, slowly, with inertia, slower at higher power. Wheel or A / D: focus (the camera's depth of field: a
  ship, a bird and the sky each want their own; the sky is sharp at knob 0.85). W / S: eyepiece. E or Esc: step back.
* The sun in the field without a filter: the view whites out, the hero steps back, a line and once a diary note.
* A hint says what is missing when something is in the field but does not count: focus, power, a bright sky, clouds.
* Other screens stay shut while it is up (`ULyudInventoryUI::SetExternalScreen`); nothing ticks unless he is looking.

## The model

`Private/Sky/TelescopeModel.h/.cpp`, namespace `LyudScope`, plain C++ (no engine): the planets (Schlyter's elements with
the Jupiter and Saturn perturbations, about 0.1° over the game's years), the Galilean moons (Meeus), the ring's tilt,
magnitudes and phases, the horizon frame of Lyudmilovsk (54.96 N, 20.40 E), the mount's inertia, the focus, what counts as
a sight (`Target`, `StepWatch`: in the field, sharp, enough power, dark enough, no cloud, held 2 s), the log (`Record`),
the moons' drawing (`MoonsDiagram`), the field stars and the planets' pictures (`Draw`, rasterised once per changed look,
never per frame). The sun and the moon come from `ALyudSky` so the eyepiece and the dome agree.

`tools/telescopesim/run.sh test` (159 checks; reference positions from astropy), `night <y> <m> <d>` (a table of the night:
what is up and when), `preview <dir>` (eyepiece mock pictures as PPM).

The sky in late October 2026: Saturn all evening in the south-east and south (ring 6.9°, nearly edge-on); Jupiter and Mars
rise after 01:00 in the east and stand high before dawn; Venus is at inferior conjunction, lost by the sun, a morning star
from November. The town's words say the same (the café's talk, the Santos note).

## The log

`ULyudSkyLog` (save record "SkyLog"). Sights: `moon_crescent`, `moon_quarter`, `moon_gibbous`, `moon_full`, `venus`,
`venus_phase` (×60), `mars`, `jupiter`, `jupiter_moons` (×12), `saturn`, `saturn_ring` (×60), `sunspots` (the filter),
`aurora`, `noctilucent`. A first sight writes a diary line (`journal.sky.<sight>`), gives the stamp `sky/<sight>` of the
collection «Небо в окуляре» (`sky`, 14 pieces; `noctilucent` is logged but is not a piece: a summer sight) and tells the
quests `observe <sight>`. Jupiter's moons are drawn anew on every night they are looked at: a diary line with the drawing
(`---o-o-O--o--o---`), `observe jupiter_night`, and on the fourth night the stamp `sky/jupiter_four`.

A bird held sharp in the middle of the field for 1.5 s is met as through the binoculars (`ULyudBirdBook::NoteSeen`, how
`telescope`).

## The astronomer

Daisuke Girma (`t_girma_daisuke`) sells fruit at the market in the mornings and draws sunspots at home. At the fruit
stall, 7 to 13 (`buy_market_fruit` topics in talk.json): `market.sky` (what is up tonight; starts the quest),
`market.log` (what else to look at; lines that follow the stamps), `market.moons` (offered with `sky/jupiter_four`).

Quest `jupiter_moons` «Четыре луны Юпитера» (tools/city/content/x_telescope.json): find the moons (`observe
jupiter_night`), three more nights (`observe jupiter_next`: a new night that is not the log's first), show the drawings (`ask market.moons`); 40 roubles and two apples.

## Console and developer mode

`lyud.Scope.Look [coin|sun]` (a telescope where the hero stands, looking where he looks), `lyud.Scope.Aim <moon|venus|mars|
jupiter|saturn|sun>` or `<alt> <az>`, `lyud.Scope.Power <0..3>`, `lyud.Scope.Focus <0..1>`, `lyud.Scope.End`,
`lyud.Scope.Log [clear|all]`, `lyud.Scope.Dof 0|1`, `lyud.Scope.Debug 1` (the state on the log every second). F7, Actions,
group `scope`: `scope.look` (toggle), `scope.best` (aim at a planet that is up and focus).

A shot of Jupiter: `lyud.Birds.Time 4`, `lyud.Scope.Look`, `lyud.Scope.Aim jupiter`, `lyud.Scope.Power 2`,
`lyud.Scope.Focus 0.85`.

## Performance

Nothing runs unless somebody is looking. While looking: one camera, one Slate leaf widget; the planets' textures are
redrawn only when their look changes (a key per picture); the field stars are regenerated when the aim leaves the cell,
into arrays kept between frames; the bird search runs five times a second. The frame is the game's frame with a narrow
field (fewer things in view than on foot).

## Checking it on the Mac

`unreal/scripts/shoot_telescope.sh` (through the UE queue, offscreen, `shoot_telescope.py`): the hero 25 m over where
he starts (`SCOPE_UP`), shots on foot and through the eyepiece of Jupiter at 04:00 (x3, x12, x60, x60 out of focus), the
moon at the night's highest hour (x12, x60), Saturn at 21:00 (x60, x400) and the sea by day (`SCOPE_SEA_AZ`, 270 = +X).
`shot showui`, so the eyepiece's Slate is in them (`HighResShot` leaves it out); Saved/Screenshots/MacEditor.
`SCOPE_BENCH=<seconds>` measures instead: on foot, x3 and x60 at the same place and aim, in rounds, in one process
(`--exclusive`). `SCOPE_SHOTS=a,b` picks shots.

Found on the first Mac run (2026-11-21) and fixed:
* The level's exposure is manual with the physical camera on, so the f-number the depth of field sets changed the
  brightness too (f/1 at x3: the night sky day-bright; f/10 at x60: darker). `CameraISO` now keeps N^2 / ISO the
  level's (`BaseFstop`, `BaseIso` read from the unbound volumes at Begin).
* `StarBrightness` only seeds the dome's material instance: the eyepiece now sets the parameter, so the dome's star
  blobs really go while the field stars are drawn.
* Saturn at x60 (a 4 px ball) was a dot: a picture is drawn when the whole picture (ring included) is over 6 px.

Known limits: the moon is Europa, 14 degrees across, so from x12 its surface fills the field and the phase shows only
at x3; at x60 its 4096 px map is blocky. The tube's horizon at x12 by day is mostly haze.

## Not done yet

The comet (the brief's "if added").
