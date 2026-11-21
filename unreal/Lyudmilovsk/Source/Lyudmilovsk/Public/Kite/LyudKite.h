// Kites (docs/kites.md): the hero's kite flown from a reel in his hand, and the kites the town's children fly on windy
// afternoons. The flight is LyudKite::FKite (Private/Kite/KiteModel.h, plain C++, tools/kitesim/run.sh) in the game's
// real wind (ULyudWindSubsystem::GetWindAt at the kite: gusts and the height's log profile).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudKite.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class ULyudKiteHumSynth;
class AHeroCharacter;
namespace LyudKite { struct FKite; }

/**
 * One kite as drawn: the sail (a diamond with its cross spars, or a box kite's two cells), a ribbon tail that trails in the
 * air, the line from the hand in sagging pieces, and for the hero the reel in his hand with the hum of the line. Built in
 * code from shared meshes (made once per shape) and shared colours; everything moves every frame, so all of it is out of
 * Lumen and distance fields, and nothing is rebuilt: the parts are moved, the line's pieces re-placed in one batch.
 */
UCLASS(NotBlueprintable)
class LYUDMILOVSK_API ALyudKite : public AActor
{
	GENERATED_BODY()

public:
	ALyudKite();

	/** 0 diamond, 1 box; Colours picks one of the colourways (a child's kite: its own by their id). */
	void Build(int32 Shape, int32 Colours, bool bWithReel);
	int32 GetShape() const { return Shape; }

	/** Places it for this frame: the kite (cm, its nose and sail normal), the line through Points (cm, hand first), the wind (m/s). */
	void Show(const FVector& KitePos, const FVector& Forward, const FVector& Normal, const TArray<FVector>& Points, const FVector& Wind, float DeltaTime);
	/** Only the reel (the kite wound on it, nothing in the air). */
	void ShowWound();
	void SetHum(float Hum, float Air, float Gain);
	/** The reel's own frame (attached to the hand by the subsystem). */
	UStaticMeshComponent* GetReel() const { return Reel; }
	/** Winds the reel's drum round (it turns as the line goes out or comes in), m of line. */
	void TurnReel(float Metres);

	static constexpr int32 LinePieces = 16;
	static constexpr int32 TailPieces = 9;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Sail;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Reel;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Line;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Tail;
	UPROPERTY(Transient) TObjectPtr<ULyudKiteHumSynth> Hum;
	TArray<FTransform> LineXf, TailXf;
	TArray<FVector> TailPts;
	int32 Shape = 0;
	float ReelAngle = 0.f;
	bool bTailSet = false;
	bool bHumOn = false;
};

/**
 * The kites in play.
 *
 * The hero: a kite taken in the hand (Tab, "take in the hand") comes out on its reel. The left button tosses it up and,
 * held, pulls the line in (a pull makes its own air in a lull); the right button held lets the line run out; the wheel
 * does the same a metre at a click; Q winds it all in and puts it away. A calm lets it fall, a gale makes it spin (reel
 * it in: lower down the air is weaker), a storm on a held line snaps it, and flown into a tree or a lamp post it is
 * caught and lost. The line hums when it is taut.
 *
 * The children: whoever of the town is flying a kite now (town life's fly_kite, chosen on windy afternoons) and drawn
 * near the camera gets one on a line from their hand, worked by LyudKite::AutoReel.
 */
UCLASS()
class LYUDMILOVSK_API ULyudKiteFlying : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyudKiteFlying();
	virtual ~ULyudKiteFlying() override;

	static ULyudKiteFlying* Get(const UObject* WorldContext);
	static bool IsKite(FName Item);
	/** A kite is out in the hand: the mouse buttons, the wheel and Q are the kite's. */
	static bool HasHand(const UObject* WorldContext);
	bool IsOut() const { return HeroKite.IsValid(); }
	FString DebugState() const;

	/** For the bench and the shots: N kites flown round the camera on lines from the ground (0 = none). */
	void SpawnTestKites(int32 N);
	/** Checks without a mouse: hold the line (0), let out (1) or pull (-1); toss it up. */
	void ForceReel(int32 Dir) { ForcedReel = Dir; bForced = true; }
	void ReleaseReel() { bForced = false; ForcedReel = 0; }
	void ForceLaunch() { bLaunchQueued = true; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudKiteFlying, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	/** A kite in the air and what flies it. */
	struct FFlight
	{
		TSharedPtr<LyudKite::FKite> Sim;   // shared: its deleter is made where the type is whole (LyudKite.cpp)
		TWeakObjectPtr<ALyudKite> Actor;
		int32 Person = -1;          // a townsperson's id; -1 the hero's or a test kite
		FVector Hand = FVector::ZeroVector;     // cm
		float Want = 25.f;          // m of line a child lets out
		float GroundZ = 0.f;        // cm under the kite
		bool bWater = false;
		float GroundIn = 0.f;
		float RelaunchIn = 0.f;
		bool bSeen = false;
	};

	void TickHero(float DeltaTime);
	void TakeOut(FName Item);
	void PutAway();
	void Lose(const TCHAR* Why);
	void Say(const TCHAR* Key, float Seconds = 3.f);
	void Stamp(const TCHAR* Piece);
	/** One step of a flight: the wind at the kite, the ground under it, a sweep for what it hits; then drawn. */
	void Fly(FFlight& F, float Reel, float DeltaTime, const AActor* Ignore);
	void TickTown(float DeltaTime);
	void TickTests(float DeltaTime);
	ALyudKite* MakeActor(int32 Shape, int32 Colours, bool bReel);
	void Park(ALyudKite* Kite);
	float SoundGain() const;

	FFlight Hero;
	TWeakObjectPtr<ALyudKite> HeroKite;
	TWeakObjectPtr<AHeroCharacter> HeldBy;
	FName HeroItem;
	bool bAttached = false;
	bool bWaitUp = false;
	bool bWindingAway = false;
	float HoldTime = 0.f;
	float WheelReel = 0.f;
	float SayLeft = 0.f;
	float UpFor = 0.f;
	float ClickIn = 0.f;
	int32 PrevState = 0;
	bool bWasSpinning = false;
	/** The "high" and "box" stamps were asked for with this kite out (asked once, not every frame it is up). */
	bool bStampHigh = false;
	bool bStampBox = false;
	int32 ForcedReel = 0;
	bool bForced = false;
	bool bLaunchQueued = false;

	TArray<FFlight> Town;
	float TownScanIn = 0.f;
	int32 FlyKiteAct = -2;

	TArray<FFlight> Tests;

	UPROPERTY(Transient) TArray<TObjectPtr<ALyudKite>> Pool;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudKite>> Made;
};
