# Review B: Sky, Sea, Europa, Weather, Winter, Audio

Reviewer: B. Scope: `git diff` of Private|Public/{Sky,Sea,Europa,Weather,Winter,Audio} (23 files, +359/-100), checked against
audit/skysea.md and audit/weather.md "Fixes applied". Read-only, nothing built. Engine checks were against UE_5.8 headers
(AActor::IsHidden, UBoxComponent::GetUnscaledBoxExtent, USynthComponent has no `Rate` member, so there is no shadowing).

Result: **no BLOCKERs**. I found nothing that should fail to compile and no new crash path. Each change matches its stated intent.
Below are the remaining behaviour notes, all SHOULD or NIT.

## Verified OK (no action)
- LyudSky.cpp UpdateSky: the space-view SkyK, Shafts and moon visibility are folded in before the guarded setters, and the
  removed tail block did nothing else. `TurnLight`: WorldDirection is normalised (GetSafeNormal), so the |cross| > 1.745e-4
  (0.01 deg) test is valid, and the dot < 0 branch catches flips. The Mie relative test is fine.
- LyudSunActivity: `State` is a TSharedPtr and is null-checked in GetDebugLine. No other user of the old `DebugLine` member
  remains. LyudSky.cpp:827 `*...->GetDebugLine()` on a temporary is the same as before (by-value return).
- LyudRareSky: `StringCast<TCHAR>` plus `FString != const TCHAR*` compiles. The compare is case-insensitive, but the phase
  names (quiet/growth/breakup/recovery/none) differ by more than case. `Now` is a member, so this does save allocations.
- LyudSea StepDepthMap: the resumable row/col loop is correct. On timeout `Done = Budget` and the for-increment still advances
  past the cell just written. At row end DepthCol resets and DepthRow++. Every call does at least 32 cells, so the map always
  progresses. `Proxies` has no remaining users. The DepthCol reset sits in the one start block (bDepthStarted is never
  cleared). Default 12 ms as the lead set.
- LyudSea KeepSwimmerHeadUp: the TSet<TWeakObjectPtr> iterator RemoveCurrent is correct.
- LyudUnderwater: the hysteresis works (bActive is set false on leave, so re-entry uses the strict test). Release() at the top
  now also runs for a hidden-only Life. AwayTime is reset on near. The MPC gate is safe: only M_LyudSeaAlga and
  M_LyudSuspended read MPC_LyudUnderwater (M_LyudSea does not; checked the uassets and make_underwater_materials.py). The
  collect zone (depth > 20 cm, < level + 15 m) is a superset of where the seabed can show (depth > 25 cm, < level + 11 m),
  and both use the same WaterDepthAtM, so weed never sways frozen in view.
- LyudSeabed: the hide/free logic is right. Clear() resets bBuilt, and bAwayHidden is cleared with it. The per-frame
  SetVisibility(bOn && !bAwayHidden) agrees with the explicit toggles.
- LyudEuropa Deinitialize: it matches SetOnEuropa's restore (same cvar, same ECVF_SetByCode).
- LyudRainFog roof map: the Swap into the scratch arrays is fine. With SetNumUninitialized, every cell is written by the slide
  loop (copied or reset to NoRoof/dirty), and `Old` is only read when !bFar, so the first (empty-scratch) call is safe.
  bFilling ends on the frame DirtyLeft reaches 0, and that frame's upload sets bRoofUploaded. Drop, Splash and Wet visibility
  is set before the MID writes in the same call, so a part that just became visible gets fresh params. The decal is attached
  to Root at zero offset, so removing its SetWorldLocation is safe.
- Winter ReadWeather: Drift touches only RoseHistory and TransportSum, which are zeroed just before. With SnowDepthCm <= 0.5
  the replay would end zeroed anyway, so skipping it is exact.
- KeepOffDecals gate: the snow decal shows only when `LevelCm > 0.2` (LyudWinter.cpp:1194), which matches.
- ALyudSeaIce::Drive: CX, CY and HalfT are moved above the early-out and used consistently. The SetBoxExtent skip is fine
  (new tiles default to 32, so the first lay still sets the extent).
- SnowNear: BuildMesh has no early return before the NearCollection load, and Drive calls BuildMesh first. The member
  `LandOrigin` has no shadowing local left. The UMaterialParameterCollection forward declaration is in LyudWinter.h:13.
  The Walkers prune runs in Recentre, outside any Walkers iteration.
- Audio: Grain, Ping, Strike and Cricket pans are never rewritten after birth (grep), so the precomputed gains are exact. No
  positional aggregate inits of the changed structs exist. Muted sleep is safe: Left/Right are zeroed every buffer, and
  Master multiplies the final mix unsmoothed (AmbienceDSP.cpp:660/669), so output is exactly 0 before the cut. Bed is only
  touched on the audio thread (the SynthCommand lambdas). The soundscape's RMS readout is debug-only, and test_audio silences
  via au.LinearGainScalarForFinalOutut, not Master.

## Findings

1. **SHOULD (minor gameplay or test visibility). Hidden-but-alive sea life and seabed for 30 s.**
   - LyudUnderwater.cpp:418-431: while away, Sim keeps its fish, so `GetFishCount` and `FindNearestFish` (BlueprintCallable)
     return frozen fish for up to 30 s after the player leaves. Before, they returned 0 or `From` at once.
   - LyudSeabed.cpp:537-552: the same applies to `ULyudSeabedComponent::FindNearest` over the hidden ISMs.
   - No C++ caller outside Sea/ was found, but Python tests or BP that probe them right after leaving the water would see a
     change.
   - Fix if it matters: have those queries return empty when `Life->IsHidden()` or `bAwayHidden`.

2. **NIT. Sea-ice collision follows cover changes more slowly.** LyudWinter.cpp:1652.
   - Walkability depends on `IP.Cover` (IceAt), which changes with the weather. Before, a 1.5 m walk re-laid it. Now it waits
     for a 3 m tile-centre crossing, a 2 cm level change or a 1 cm half-thickness change.
   - The tide moving 2 cm usually re-lays it soon anyway.
   - Optional: also re-lay when `|Cover - LaidCover| > ~0.02`.

3. **NIT. Unmute rebuilds FAmbience on the audio render thread.** LyudAmbienceSynth.cpp:78-82.
   - `MakePimpl` plus `Init` (vector reserves, delay lines) allocate on the render thread once per unmute: leaving Europa,
     `lyud.Sound 1`, or the volume slider coming back from 0.
   - Output is silent or fading in at that point, so it is unlikely to be audible. Note it only for the "no audio-thread
     allocation" rule.

4. **NIT. Rain blinks off for about 3 frames on a teleport or fast jump.** LyudRainFog.cpp:474-478.
   - `bRoofUploaded = false` while the fresh map fills at 1024 cells per frame (64x64 map), as intended.
   - It also triggers whenever more than half the cells go dirty mid-rain. A seated cable-car or train ride slides far less
     than that per frame, so in practice only real jumps trigger it.

5. **NIT. KeepOffDecals now skips the snowless months.** LyudWinter.cpp:492.
   - The sweep also used to keep the rain WetDecal off movers spawned after winter.
   - With the gate, an NPC, animal or plant spawned in a snowless period after winter keeps bReceivesDecals = true. That is
     the same as a world where winter never ran (the old baseline), and people, animals and bodies already switch decals
     off at creation. Plants are the only remaining case, but their materials take the wet decal fine.

6. **NIT. Depth map takes longer at load.** LyudSea.cpp:388-396.
   - At 12 ms, the full 1024x~788 map takes on the order of 10 s of frames (estimate: 12 ms / ~8 µs per trace ≈ 1500 cells
     per frame).
   - The seabed, fish, pools and tide flats wait for it (OpenSeaDepth until then).
   - Tests that settle at the sea quickly (test_underwater, test_swim, test_europa hatch, test_cosmic pools) may need
     `lyud.Sea.DepthMapMs 0` or a wait on `IsDepthReady`. This is the lead's chosen trade-off; I flag it only for the test
     run.
