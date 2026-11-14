# North frames, 2026-11-14 (north 80c96ceb, editor build Succeeded, test_north 27 steps / 0 failed)

Offscreen -game, 1920x1080, lyud.Weather.Preset clear, sky hour 13, hero's first-person view, birds off.

| Frame | Spot (map m) | What I see / defects |
|---|---|---|
| a_polar_brow_bench | (-370, 1765) looking N | Brow bench stands fine (seat toward the valley). The river valley itself is hidden behind the brow from eye height. Far volcanoes' snow is vertical white stripes (looks like UV/texture streaking). Trees on the ridge are sparse, thin, many look bare/red-trunked; no "broad masses" of forest here. |
| b_footbridges_south_bank | (-149, 1996) looking N | Bridge deck runs along the view (yaw 0 is right), ramp meets the bank. River sheet shows strong radial streaks (stretched flow texture) instead of water; the gravel bars are one flat brown plain. Gauge post at left OK. Slopes beyond: hundreds of small identical bare deciduous trees, very uniform. |
| c_river_mouth_rowboat | (247, 2121) looking W | Huge empty flat sand plain, no water at the mouth from here; the sea is a thin line far off. The rowboat reads as a dark box (no visible material detail). A pale upright stump/driftwood mid-frame. |
| d_hot_spring | (-296, 3490) looking N, pitch -18 | spring_deck and spring_steps overlap: two railings crossing each other at the pool. The pool is a deep rough dark pit; the water is a tiny patch at its bottom. Trail sign (ru) readable. |
| d2_hot_spring_seated | hero seated (lyud.North.SitSpring 1) | The pool water renders as a small SQUARE teal quad, not round; the seated view looks into the basin wall, the water is below/in front, not around the hero. Seat height vs basin/terrain needs fixing (test passes on numbers only). |
| e_hut_gate_valley | (112, 2508) looking S | Into the sun (huge disc). Hut and hut_interior are still placeholders (meshes not imported: "placeholders for kinds not imported yet: hut, hut_interior"). Bare deciduous trees with autumn leaves (November), fine. |
| f_cape_sea_stacks | (760, 2665) looking W | The spot is down on the wet sand flat (z -42 m), not on a cliff. Sea stack stands in shallow water and reads well; a pale band at its waterline. |
| g_campsite | (118, 3082) looking NE | Good: tents, tarp, woodpile, table, food pole with bag, board all on the ground. Nothing floating seen. |
