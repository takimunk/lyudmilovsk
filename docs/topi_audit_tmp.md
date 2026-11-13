# Audit: branch `topi` (Topi dunes, the river Ural, the hero's mind)

Scope: `git diff origin/main...HEAD -- unreal/Lyudmilovsk/Source` in `/home/user/rd_topi` (13 files) plus the code it
calls (save subsystem, soundscape, hero, bike, grass, dev teleport). Read-only, reasoned from the code: nothing was
compiled or run. Paths are relative to `Source/Lyudmilovsk`; line numbers are on the branch.

Summary: the ground itself is sound (chunks are built once on workers, uploaded within a ms budget, never rebuilt;
the "living" dunes are material WPO; idle chunks are freed; the mind component early-outs outside a 150 m zone).
The real problems are in the state machines meeting other systems (the bike, the save order) and two frame-cost items
(shadow cache, grass).

---

### T1 S1: riding a bike into the Ural breaks the dive and the wake-up (and the turn-back does nothing on a bike)
Confidence: high (code path read end to end; not run).

- `Private/Topi/LyudTopiMind.cpp:448-455` (`UpdateRiver` starts the dive), `:468-495` (`BeginDissolve`), `:623-634`
  (`Wake`), `:331-338` and `:353-397` (`BeginTurn`), `:409` (`UpdateTurn` forces the walk).
- `Private/Bikes/HeroBike.cpp:158-163` (mounted: `MOVE_None`, capsule `NoCollision`), `:536-548` (`PlaceHero` sets the
  hero's location to the bike **every frame**), `Private/HeroCharacter.cpp:251-256` (`OnMove` hands the input to the
  bike without asking `IsMoveInputIgnored`).

What happens:
1. **The river.** At night the hero rides down the path into the water. Feet depth passes 110 cm, `BeginDissolve` runs:
   it sets `MOVE_Flying`, gravity 0, actor collision off, and locks input with `SetIgnoreMoveInput`. None of that
   stops the bike: `OnMove` still feeds `Bike->SetMoveInput`, and `PlaceHero` overwrites the `SetActorLocation` of
   `UpdateDissolve` each frame. At 8.5 s `Wake` teleports the hero to the PlayerStart, sets `MOVE_Walking`, skips the
   calendar to 07:30 next day, increments `TimesDissolved`, and later fires `ural_dissolved`. On the next frame
   `PlaceHero` puts him back on the bike in the river. Result: white-out, a day skipped, the quest event fired, and
   the hero still in Topi on the bike, with `MOVE_Walking` and a no-collision capsule while mounted (until he
   dismounts). He never wakes at the computer.
2. **Leading a bike by the bars** (`IsLeading`): the hero is teleported to the club and the bike follows him there.
3. **Turning back.** On a bike `BeginTurn` only turns the camera and plays the line. `AddMovementInput(..., true)` is
   ignored in `MOVE_None`, the bike keeps taking W/A/S/D, and `SetSpeedScale` does not slow it. By bike the player
   crosses the daytime dunes freely; the mechanic is bypassed.

Fix: get him off the bike before either sequence starts. `UHeroBikeComponent::Dismount()` and `LetGo()` are public
(`Public/Bikes/HeroBike.h:48,50`), `AHeroCharacter::FindBike()` is public (`Public/HeroCharacter.h:91`).

```cpp
// LyudTopiMind.cpp: #include "Bikes/HeroBike.h"
namespace
{
	/** True when he is on his own feet. A rider is asked off (the bike's own dismount, a moment); a led bike is let go. */
	bool TopiOnFoot(AHeroCharacter& Hero)
	{
		if (UHeroBikeComponent* Bike = Hero.FindBike())
		{
			if (Bike->IsOnBike())
			{
				Bike->Dismount();      // no-op unless riding; call again next frame until he stands
				return false;
			}
			Bike->LetGo();
		}
		if (Hero.IsSeated())
		{
			Hero.StandUp();
		}
		return true;
	}
}

// UpdateRiver, replacing lines 448-455
	if (Depth > TopiChestDepthCm && !bTurning && TopiOnFoot(Hero))
	{
		APlayerController* PC = nullptr;
		if (PlayerHero(&PC) == &Hero && PC)
		{
			BeginDissolve(Hero, *PC);
		}
	}

// UpdatePressure, replacing lines 331-338
	if (!bCalm && !bTurning && !bDissolving && Grace <= 0.f && (Pressure >= 0.999f || HighFor >= 4.f) && TopiOnFoot(Hero))
	{
		APlayerController* PC = nullptr;
		if (PlayerHero(&PC) == &Hero && PC)
		{
			BeginTurn(Hero, *PC);
		}
	}

// ForceTurnBack (line 347) and StartDissolve (line 462): add `&& TopiOnFoot(*Hero)` to the condition
```

---

### T2 S1: `TimesDissolved` is never restored from a save (the mind registers after the save is applied)
Confidence: medium-high (the order follows from `UWorld::Tick`: the timer manager ticks before
`FTickableGameObject::TickObjects`; engine source was not available to re-read).

- `Private/Topi/LyudTopi.cpp:1011-1030`: `ULyudTopiSubsystem::Tick` spawns `ALyudTopi` on the world's **first tick**
  (a tickable subsystem).
- `Private/Topi/LyudTopiMind.cpp:213-216`: `ULyudTopiMind::BeginPlay` (at that spawn) calls
  `RegisterSaveObject("TopiMind", this)`.
- `Private/LyudSave.cpp:229`: after a load, `ApplyPending` is a `SetTimerForNextTick` set at world BeginPlay, so it
  fires in the first frame's timer tick, **before** the tickable subsystem runs. `:275-282`: it reads only the
  objects registered at that moment.

What happens: load a save, `ApplyPending` runs, "TopiMind" is not registered yet, the record is skipped. Topi spawns
later in the same frame with `TimesDissolved = 0`, and the next save writes 0. The count is lost on every load.

Fix: spawn Topi in `OnWorldBeginPlay` (level actors, the sky included, already exist; the new actor then begins play
with the rest, before the first tick). Keep the tick as a fallback.

```cpp
// LyudTopi.h, ULyudTopiSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
private:
	void SpawnTopi(UWorld& World);

// LyudTopi.cpp
void ULyudTopiSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld() && !Topi.IsValid())
	{
		SpawnTopi(InWorld);
	}
}

void ULyudTopiSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && !Topi.IsValid() && !bLooked)
	{
		SpawnTopi(*World);
	}
}

void ULyudTopiSubsystem::SpawnTopi(UWorld& World)
{
	// only a world with the open sky has Topi
	TActorIterator<ALyudSky> It(&World);
	if (!It)
	{
		return;                 // OnWorldBeginPlay found no sky: the first tick looks once more
	}
	bLooked = true;
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Topi = World.SpawnActor<ALyudTopi>(ALyudTopi::StaticClass(), FTransform::Identity, P);
}
// and in Tick's fallback set bLooked = true after the call so it looks only once
```

---

### T3 S2: the shadow-casting dune chunks use a WPO material: the virtual shadow map cache is invalidated every frame
Confidence: medium (what the code does is verified; the cost is the documented VSM behaviour for non-Nanite
primitives whose material has World Position Offset. Confirm in game with
`r.Shadow.Virtual.Cache.DrawInvalidatingBounds 1` or `stat VirtualShadowMapCache` near the dunes).

- `Private/Topi/LyudTopi.cpp:648`: `Mesh->SetCastShadow(Data->Level <= 2)`: every chunk out to about 1.9 km casts.
- `:655-658`: all chunks share `SandMID`. M_TopiSand writes World Position Offset
  (`unreal/scripts/make_topi_materials.py:72`, `tools/topi/topi_hlsl.py:67-72`: the heave, zero near the eye, but the
  material is still a WPO material for every chunk).
- No `ShadowCacheInvalidationBehavior` is set, so it stays `Auto`: a primitive with WPO invalidates the cached pages
  under it each frame. That is the whole near dune field (about a hundred 9k-triangle procedural meshes) redrawn
  into the sun's shadow maps every frame while the player is in or near Topi.

Fix (the same line `Private/Plants/ProceduralVine.cpp:51` already uses). The heave is far and slow, shadows from the
unheaved shape are right:

```cpp
// LyudTopi.cpp FinishBuilt, after SetCastShadow (line 648)
		Mesh->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
```

---

### T4 S2: marram grass: four `AProceduralGrass::Scatter` in one frame per chunk, and patches grown 5x farther than they can draw
Confidence: high for the mechanics, medium for the milliseconds (estimated).

- `Private/Topi/LyudTopi.cpp:670-684`: one finest chunk a frame gets grass; `:687-718` `GrowGrass` spawns up to
  **four** 64 m patches in that one call. Each patch's `BeginPlay` runs `Scatter`
  (`Private/Plants/ProceduralGrass.cpp:152-308`): three `TActorIterator`s over every plant, fallen fruit and grass
  patch in the world, about 1,100 line traces 120 m long against the chunk's trimesh, then `AddInstances` of a few
  thousand tufts and a per-instance `SetCustomDataValue` loop. Four of those in a frame is a visible hitch, repeated
  for ~45 frames on arriving at the edge and again for each new row of chunks while walking along it.
- Grass is asked for every shown level-0 chunk, which is everything within ~490 m of the eye, but the tufts are culled
  at 35 m / 90 m (`Public/Plants/ProceduralGrass.h:56-57`). In the grass band that is ~180 patch actors (360 ISM
  components, on the order of 600k instances, all ticking every 2 s) of which only the nearest dozen can draw.

Fix: one quarter a frame, only near the eye, and let go of the far ones.

```cpp
// FState::FChunk: replace `bool bGrassAsked` with
		uint8 GrassNext = 0;            // the next quarter to grow (4: done)

// FinishBuilt, replacing the block at lines 670-684
	if (bGrass && CVarTopiGrass.GetValueOnGameThread() != 0)
	{
		const double Time = GetWorld()->GetTimeSeconds();
		const double Size = double(FinestChunkM);
		const double Reach = 220.0;     // m: the tufts are culled at 90 m
		for (const uint64 Key : State->Shown)
		{
			FState::FChunk& Chunk = State->Chunks.FindChecked(Key);
			if (Chunk.Level != 0)
			{
				continue;
			}
			const double DX = FMath::Max3(Chunk.IX * Size - State->LastEye.X, 0.0, State->LastEye.X - (Chunk.IX + 1) * Size);
			const double DY = FMath::Max3(Chunk.IY * Size - State->LastEye.Y, 0.0, State->LastEye.Y - (Chunk.IY + 1) * Size);
			const double D2 = DX * DX + DY * DY;
			if (Chunk.GrassNext < 4 && Chunk.bShown && Time - Chunk.ShownAt > 2.0 && D2 < Reach * Reach)
			{
				GrowGrass(Key, Chunk.GrassNext++);
				break;                  // one patch (one Scatter) a frame
			}
			if (Chunk.GrassPatches.Num() > 0 && D2 > 4.0 * Reach * Reach)
			{
				for (const TWeakObjectPtr<AProceduralGrass>& Patch : Chunk.GrassPatches)
				{
					if (Patch.IsValid())
					{
						Patch->Destroy();
					}
				}
				Chunk.GrassPatches.Reset();
				Chunk.GrassNext = 0;
				break;
			}
		}
	}

// GrowGrass(uint64 Key, int32 Q): the same body without the `for (Q...)` loop
// (`if (Chunk.Grass[Q] < 0.04f) return;` in place of `continue`)
```

---

### T5 S2 (build): `LoadMesh` in LyudTopiProps.cpp's anonymous namespace collides with `LyudTransportKit::LoadMesh` in a unity build
Confidence: medium (depends on the two files landing in one unity blob; `Private/Topi/` is immediately followed by
`Private/Transport/` and the four Topi files total 90 KB, so they normally do. A machine using adaptive non-unity for
locally changed files will not see it; a clean build will).

- `Private/Topi/LyudTopiProps.cpp:46`: `UStaticMesh* LoadMesh(const FString& Name)` in a global anonymous namespace.
- `Private/Transport/TransportKit.h:147`: `inline UStaticMesh* LoadMesh(const FString& Name)` in
  `namespace LyudTransportKit`; `Private/Transport/LyudCableway.cpp:34` (also LyudLobby, LyudStationBuild,
  LyudStationFurnish) has `using namespace LyudTransportKit;` at file scope and calls `LoadMesh(...)` unqualified
  (`LyudCableway.cpp:573`). In the same translation unit both are visible with the same signature: "call to
  'LoadMesh' is ambiguous".

Fix: rename the Topi one (definition at line 46 and its four uses at lines 200-206):

```cpp
	UStaticMesh* LoadTopiMesh(const FString& Name)
```

---

### T6 S3: `DropLoop` leaks the audio component when the loop is not playing at that moment
Confidence: high for the mechanism; whether it triggers depends on the assets (a non-looping `topi_heart` /
`ural_hum` wave, or a loop the mixer stopped while silent).

- `Private/Topi/LyudTopiMind.cpp:820-829`: `bAutoDestroy = true; FadeOut(1.2f, 0.f); Loop = nullptr;`. `FadeOut` on a
  component that is not playing does nothing and no completion ever comes, so the component (spawned by `AttachLoop`
  with `bAutoDestroy=false`, attached to the hero's root) is never destroyed. Called from `GoQuiet` (`:748-749`) and
  `UpdateSound` (`:888`, `:902`), so once per approach to Topi.
- Related: `:881` and `:895` only spawn a loop when the pointer is invalid, so a loop that has stopped by itself is
  never restarted (silent heartbeat).

Fix: this branch's base has no `ReleaseLoop`; current main has
`static void ULyudSoundscapeSubsystem::ReleaseLoop(UAudioComponent* Loop, float FadeSeconds)`, which destroys a loop
that is not playing. After merging main:

```cpp
void ULyudTopiMind::DropLoop(TObjectPtr<UAudioComponent>& Loop)
{
	ULyudSoundscapeSubsystem::ReleaseLoop(Loop.Get(), 1.2f);
	Loop = nullptr;
}

// UpdateSound: treat a stopped loop as gone, so it is released and spawned again
	if (HeartLoop && !HeartLoop->IsPlaying()) { DropLoop(HeartLoop); }
	if (HumLoop && !HumLoop->IsPlaying()) { DropLoop(HumLoop); }
```

(Until the merge, the same body inline: `if (IsValid(L) && L->IsPlaying()) { L->bAutoDestroy = true; L->FadeOut(1.2f, 0.f); }
else if (IsValid(L)) { L->Stop(); L->DestroyComponent(); }`.)

---

### T7 S3: a sequence is not aborted when something else moves the hero (dev teleport, a quest's dev jump)
Confidence: high.

- `Private/Topi/LyudTopiMind.cpp:249-253` aborts only when the pawn changes. `Private/Dev/LyudDev.cpp:406-439`
  (`TeleportWorld`) keeps the pawn.
- Teleport during the dive: `UpdateDissolve` (`:513-519`) goes on at the new place with actor collision off, sinking
  the hero through the ground toward the river's level at 45 cm/s for up to 8.5 s, then `Wake` still skips the day,
  increments `TimesDissolved` and fires `ural_dissolved`. Teleport during the turn: he is force-walked along `TurnDir`
  for up to 5 s wherever he now is.

Fix:

```cpp
// LyudTopiMind.h
	FVector SequenceAt = FVector::ZeroVector;

// BeginTurn and BeginDissolve: SequenceAt = Hero.GetActorLocation();

// TickComponent, right after the pawn-changed check (line 253)
	if ((bTurning || (bDissolving && !bWoken)) && Hero)
	{
		if (FVector::DistSquared(Hero->GetActorLocation(), SequenceAt) > FMath::Square(3000.0))
		{
			UE_LOG(LogTemp, Log, TEXT("LyudTopi: sequence interrupted (the hero was moved)"));
			AbortSequences();
		}
		SequenceAt = Hero->GetActorLocation();
	}
```

---

### T8 S3: synchronous asset loads in play paths
Confidence: high that they are synchronous; the hitch size depends on what else is streaming (a `LoadObject` of a
package not in memory flushes the async loading queue).

- `Private/Topi/LyudTopiMind.cpp:838-839`: both music tracks are `LoadObject`ed on the first frame the hero is within
  150 m of the sand.
- `:914` (`Say`): `LyudText::LoadVoice` is a `LoadObject` at the instant the turn-back starts and at the wake line.

Fix: ask for them in `BeginPlay` and keep the handle, so the later `LoadObject` calls find them in memory.

```cpp
// LyudTopiMind.h:  #include "Engine/StreamableManager.h"   and   TSharedPtr<FStreamableHandle> Preload;
// LyudTopiMind.cpp: #include "Engine/AssetManager.h"
// BeginPlay, at the end
	{
		TArray<FSoftObjectPath> Paths = { FSoftObjectPath(TopiMusicPath), FSoftObjectPath(UralMusicPath) };
		const FString Lang = LyudText::Language();
		for (const TCHAR* Line : { TEXT("turn_1"), TEXT("turn_2"), TEXT("turn_3"), TEXT("turn_4"), TEXT("wake_1") })
		{
			const FString Name = FString::Printf(TEXT("V_line_hero_topi_%s"), Line);
			Paths.Add(FSoftObjectPath(FString::Printf(TEXT("/Game/Voice/%s/%s.%s"), *Lang, *Name, *Name)));
		}
		Preload = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
// EndPlay: Preload.Reset();
```

---

## Checked and fine

- **Dune meshes**: each chunk is built once on a worker (`BuildTopiChunk`), uploaded once inside `lyud.Topi.BudgetMs`,
  never rebuilt; no per-frame `MarkRenderStateDirty` / `SetMaterial` / MID creation. The four MIDs are created once;
  parameters are written only when the value changes. Collision only on level 0, cooked async
  (`bUseAsyncCooking`). `DropChunk` clears the sections and destroys the component; the chunk map is bounded
  (`SpareChunks`); a chunk being built is never dropped, so no double build. Workers hold a thread-safe
  `TSharedPtr` to the model and stop on `EndPlay`. `bAffectDistanceFieldLighting=false`; the meshes are static.
- **Far from Topi**: `UpdateChunks` re-resolves only every 4 m of camera travel (every frame only while chunks are
  still being built); `ULyudTopiMind` does one `Near` test and returns; `ULyudTopiProps` stops ticking after placing;
  the river, glow, motes and three lights are hidden by day; `GoQuiet` stops the music, drops both loops, releases the
  music hold and zeroes the post process weight.
- **Memory, bounded but large**: about 250-300 chunks deep inside Topi, roughly 0.7 MB each as a procedural mesh.
- **State machines**: Load and New Game are `OpenLevel`, so a sequence cannot survive them; `EndPlay` calls
  `AbortSequences` (input, camera fade, gravity, movement mode, collision). A save made mid-dive reloads with the hero
  in the water at night and the dive simply starts again. Becoming a bird changes the pawn, which aborts and restores;
  as a bird nothing presses. Input ignore counters are balanced through `bInputLocked` / `LockedPC`; both fades end
  (the hold is cleared by the 1 to 0 fade or by `StopCameraFade`). `SpeedScale` is put back on every exit path.
- **Sea**: the river bed (about -35 m) is above sea level (-43 m), so `ULyudSwimComponent` does not engage in the Ural.
- **Timers and delegates**: no timers or async lambdas capture `this`; the language delegate is removed in `EndPlay`;
  the subtitle widget holds a weak pointer and is removed in `EndPlay`.
- **Clang**: no local shadows a member or an outer local, no loop with an unconditional exit, in the four Topi files
  (apart from T5). `TopiModel.cpp`: table and ridge indices are clamped; no division by zero on the path.
- **Soundscape change** (`SetMusicHold`): weak holders, pruned in `UpdateMusic`; safe across worlds.
