// Kites (docs/kites.md): the hero's kite flown from a reel in his hand (or, a stunt kite, from two handles), and the kites
// the town flies on windy afternoons. The flight is LyudKite::FKite (Private/Kite/KiteModel.h, plain C++,
// tools/kitesim/run.sh) in the game's real air (ULyudWindSubsystem::GetLocalWind at the kite: gusts, the land's lee and
// speed-up, rising and sinking air, turbulence).
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
class APlayerController;
namespace LyudKite { struct FKite; struct FTricks; struct FSnag; }

/**
 * One kite as drawn: the sail (a diamond with its cross spars, a box kite's two cells, a delta wing with its keel, a stunt
 * kite's swept wing, a dragon's round face), a tail that trails in the air (the diamond's nine bows, the dragon's twelve
 * metres of ribbon), the line from the hand in sagging pieces (two lines for the stunt kite), and for the hero the reel or
 * the handles in his hands with the hum of the line. Built in code from shared meshes (made once per shape) and shared
 * colours; everything moves every frame, so all of it is out of Lumen and distance fields, and nothing is rebuilt: the parts
 * are moved, the line's and the tail's pieces re-placed in one batch each.
 */
UCLASS(NotBlueprintable)
class LYUDMILOVSK_API ALyudKite : public AActor
{
	GENERATED_BODY()

public:
	ALyudKite();

	/** Shape: 0 diamond, 1 box, 2 delta, 3 stunt, 4 dragon (LyudKite::EShape). Colours picks one of the colourways (a
	 *  townsperson's kite: their own by their id). */
	void Build(int32 Shape, int32 Colours, bool bWithReel);
	int32 GetShape() const { return Shape; }

	/**
	 * Places it for this frame: the kite (cm, its nose and sail normal), the line through Points (cm, hand first; empty: no
	 * line), the wind (m/s). Points2: a stunt kite's second line (null: none drawn).
	 */
	void Show(const FVector& KitePos, const FVector& Forward, const FVector& Normal, const TArray<FVector>& Points, const FVector& Wind, float DeltaTime,
		const TArray<FVector>* Points2 = nullptr);
	/** Only the reel (the kite wound on it, nothing in the air). */
	void ShowWound();
	void SetHum(float Hum, float Air, float Gain);
	/** The reel's own frame (attached to the hand by the subsystem); a stunt kite's right handle. */
	UStaticMeshComponent* GetReel() const { return Reel; }
	/** A stunt kite's left handle (null for the others). */
	UStaticMeshComponent* GetLeftHandle() const { return LeftHandle; }
	/** Winds the reel's drum round (it turns as the line goes out or comes in), m of line. */
	void TurnReel(float Metres);

	static constexpr int32 LinePieces = 16;
	static constexpr int32 TailPieces = 9;
	static constexpr int32 DragonTailPieces = 40;
	/** Where a stunt kite's two lines tie on: this far either side of its middle, cm. */
	static constexpr float StuntBridle = 38.f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Sail;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Reel;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftHandle;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Line;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Tail;
	UPROPERTY(Transient) TObjectPtr<ULyudKiteHumSynth> Hum;
	TArray<FTransform> LineXf, TailXf;
	TArray<FVector> TailPts;
	int32 Shape = 0;
	/** The tail: how many pieces, how long each (cm), and how far behind the bridle it ties on (cm). */
	int32 TailN = 0;
	float TailSeg = 16.f;
	float TailFoot = 40.f;
	float ReelAngle = 0.f;
	bool bTailSet = false;
	bool bHumOn = false;
};

/**
 * The kites in play.
 *
 * The hero: a kite taken in the hand (Tab, "take in the hand") comes out on its reel. A single-line kite (diamond, box,
 * delta, dragon): the left button tosses it up and, held, pulls the line in (a pull makes its own air in a lull); the right
 * button held lets the line run out; the wheel does the same a metre at a click; Q winds it all in and puts it away. A
 * stunt kite has two lines: the left button pulls the left one, the right the right one (the nose turns that way), both
 * together are a pull. Any kite can be stood on the sand and launched from a distance, or held up by somebody of the town
 * who is out with the kites (E) while the hero backs off into the wind and pulls. A calm lets it fall, a gale makes it
 * spin, a storm on a held line snaps it, and flown into a tree or a lamp post it is caught: it hangs there with its line
 * to the ground, and can be got down by tugging the line from the right side, best in a gust (and so can a child's).
 *
 * The town: whoever is flying a kite now (town life's fly_kite, fly_kite_delta, fly_kite_stunt, fly_kite_dragon, chosen
 * on windy afternoons) and drawn near the camera gets one on a line from their hand, worked by LyudKite::AutoReel or, a
 * stunt kite, LyudKite::AutoSteer.
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
	/** The hero's tricks so far with the kite in his hand (console lyud.Kite.Tricks). */
	FString TricksReport() const;

	/** For the bench and the shots: N kites flown round the camera on lines from the ground (0 = none). Shape: one kind
	 *  (LyudKite::EShape as a number), or -1 for a mix of all five. */
	void SpawnTestKites(int32 N, int32 Shape = -1);
	/** Checks without a mouse: hold the line (0), let out (1) or pull (-1). */
	void ForceReel(int32 Dir) { ForcedReel = Dir; bForced = true; }
	void ReleaseReel() { bForced = false; ForcedReel = 0; }
	/** A stunt kite's lines without a mouse: -1 the left one .. 1 the right; a pull on both (0 or 1). */
	void ForceSteer(float Steer) { ForcedSteer = FMath::Clamp(Steer, -1.f, 1.f); bSteerForced = true; }
	void ForcePull(bool bPull) { bPullForced = bPull; bSteerForced = true; }
	void ReleaseSteer() { bSteerForced = false; bPullForced = false; ForcedSteer = 0.f; }
	/** 0 toss it up, 1 stand it on the sand, 2 a long launch (held up 20 m downwind as a helper would; pull to let go). */
	void ForceLaunch(int32 Mode = 0) { LaunchQueued = Mode + 1; }
	/** The hero's flying kite is caught where it is; with none up, a caught kite is hung 10 m ahead for the rescue. */
	void SnagNow();
	/** E on a caught kite's line without a keyboard: take it, then a tug each time. */
	void ForceTug() { bTugQueued = true; }

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
		int32 Shape = 0;            // LyudKite::EShape
		FVector Hand = FVector::ZeroVector;     // cm
		FVector Hand2 = FVector::ZeroVector;    // a stunt kite's other line: the left hand, cm
		FVector Wind = FVector::ZeroVector;     // the air at the kite, m/s (rising or sinking in Z)
		float Turbulence = 0.f;     // rms m/s there
		float WindIn = 0.f;         // s to the next look at the air (the town's kites look ten times a second)
		float Steer = 0.f;          // a stunt kite's lines this frame
		float Pull = 0.f;
		float Want = 25.f;          // m of line a child lets out
		float GroundZ = 0.f;        // cm under the kite
		bool bWater = false;
		float GroundIn = 0.f;
		float RelaunchIn = 0.f;
		float Lain = 0.f;           // s a stunt kite has lain on the sand
		bool bSeen = false;
		bool bCaught = false;       // hangs in a tree (it is in Caught)
		// a stunt flier shows the hero how it is done: 0 no, 1 a loop to the right, 2 back to the left
		int32 Demo = 0;
		float DemoFrom = 0.f;
		float HeroNear = 0.f;       // s the hero has stood by
		bool bDemoDone = false;
	};

	/** A kite caught in a tree or on a post: it hangs there with its line to the ground until somebody gets it down. */
	struct FCaught
	{
		TSharedPtr<LyudKite::FKite> Sim;
		TSharedPtr<LyudKite::FSnag> Snag;
		TWeakObjectPtr<ALyudKite> Actor;
		int32 Person = -1;          // whose it is: a townsperson, or -1 the hero's (Item) or nobody's
		FName Item;                 // the hero's own: what goes back into the pocket
		FVector Forward = FVector::ForwardVector, Normal = FVector::UpVector;   // how it hangs
		FVector Foot = FVector::ZeroVector;     // where its line's end lies on the ground, cm
		FVector Wind = FVector::ZeroVector;     // m/s there
		float Gust = 1.f;
		float AirIn = 0.f;
		float GoneIn = 600.f;
		float Shake = 0.f;          // a tug just shook it
		float LookIn = 0.f;
	};

	void TickHero(float DeltaTime);
	void TakeOut(FName Item);
	void PutAway();
	void Lose(const TCHAR* Why);
	void Say(const TCHAR* Key, float Seconds = 3.f);
	void Stamp(const TCHAR* Piece);
	/** A quest event "kite <What>" (docs/kites.md lists them). */
	void Event(const TCHAR* What);
	/**
	 * One step of a flight: the air at the kite, the ground under it, a sweep for what it hits; then drawn. Reel, and the
	 * flight's Steer and Pull, are the flyer's hands. Tricks (the hero's): what the step finished comes back as a
	 * LyudKite::ETrick number (0 none).
	 */
	int32 Fly(FFlight& F, float Reel, float DeltaTime, const AActor* Ignore, LyudKite::FTricks* Tricks = nullptr);
	/** A flight nobody steers by hand (the town's, the bench's): launched, reeled or steered as a practised flyer would. */
	void FlyItself(FFlight& F, float DeltaTime, const AActor* Ignore, bool bHoldWound);
	void TickTown(float DeltaTime);
	void TickTests(float DeltaTime);
	/** The caught kites: drawn hanging, tugged by the hero, fluttering down, given back. */
	void TickRescue(float DeltaTime);
	void AddCaught(const TSharedPtr<LyudKite::FKite>& Sim, ALyudKite* Actor, int32 Person, FName Item, const FVector& Hand, const FVector& Wind);
	void RemoveCaught(int32 Index, bool bDestroyActor);
	void FinishRescue(int32 Index);
	/** Somebody of the town within reach who is out with the kites (flying one or watching), or -1. */
	int32 FindHelper(const FVector& Near) const;
	void LookUpActivities();
	/** A townsperson says a moment's line (town barks' on = <Event>); false when no line is written for it yet. */
	bool Bark(int32 Person, const TCHAR* Event);
	/** The prompt while a kite is out: what the keys do now, and under it what the air is like where the hero stands. */
	void RefreshPrompt(const AHeroCharacter* H);
	FText WindLine(const AHeroCharacter* H) const;
	void ShowPrompt(const FText& Text);
	ALyudKite* MakeActor(int32 Shape, int32 Colours, bool bReel);
	void Park(ALyudKite* Kite);
	float SoundGain() const;

	FFlight Hero;
	TWeakObjectPtr<ALyudKite> HeroKite;
	TWeakObjectPtr<AHeroCharacter> HeldBy;
	FName HeroItem;
	TSharedPtr<LyudKite::FTricks> Tricks;
	bool bAttached = false;
	bool bWaitUp = false;
	bool bWindingAway = false;
	/** Stood on the sand by the hero and not yet pulled up: the line follows him as he backs off. */
	bool bLaid = false;
	/** Who of the town holds the hero's kite up for a long launch (-1 nobody), and that the launch now flying was theirs. */
	int32 Helper = -1;
	bool bHelperLaunch = false;
	int32 NearHelper = -1;
	float HelperScanIn = 0.f;
	float HoldTime = 0.f;
	float WheelReel = 0.f;
	float SayLeft = 0.f;
	float UpFor = 0.f;
	float CalmFor = 0.f;
	float ClickIn = 0.f;
	float PromptIn = 0.f;
	FString LastPrompt;
	int32 PrevState = 0;
	bool bWasSpinning = false;
	/** These stamps were asked for with this kite out (asked once, not every frame it is up). */
	bool bStampHigh = false;
	bool bStampBox = false;
	bool bStampCalm = false;
	bool bStampDragon = false;
	int32 ForcedReel = 0;
	bool bForced = false;
	float ForcedSteer = 0.f;
	bool bSteerForced = false;
	bool bPullForced = false;
	int32 LaunchQueued = 0;
	bool bTugQueued = false;

	TArray<FFlight> Town;
	float TownScanIn = 0.f;
	/** Town life's activities: fly_kite, fly_kite_delta, fly_kite_stunt, fly_kite_dragon, and watch_kites (-2 not looked up, -1 none). */
	int32 KiteActs[4] = { -2, -2, -2, -2 };
	int32 WatchAct = -2;

	TArray<FFlight> Tests;

	TArray<FCaught> Caught;
	/** The caught kite whose line the hero holds (-1 none), and the one he stands by. */
	int32 HeldLine = -1;
	float TugIn = 0.f;
	bool bRescuePrompt = false;

	UPROPERTY(Transient) TArray<TObjectPtr<ALyudKite>> Pool;
	UPROPERTY(Transient) TArray<TObjectPtr<ALyudKite>> Made;
};
