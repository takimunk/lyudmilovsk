// What ALyudDinghy / ALyudSailLaunch keep behind their pimpl, and the kit the boat part's files share (the classes'
// numbers, the meshes loaded once, the fallback boat built in code, crew bodies). docs/yacht_school.md "Engine: the boats".
// Every component pointer here is also in the actor's Kept array (a UPROPERTY), which is what keeps it alive.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Sailing/LyudDinghy.h"
#include "LyudSailHud.h"
#include "SailModel.h"
#include "SailSound.h"

class ANpcBody;
class AHeroCharacter;
class ALyudSea;
class UAnimSequence;
class UAudioComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class ULyudBodyAnimInstance;
class ULyudSailCamera;
class ULyudSoundscapeSubsystem;
class ULyudWindSubsystem;
class UPeopleLifeSubsystem;
class SLyudSailHud;

namespace LyudSailKit
{
	/** One class's drawing numbers, Unreal cm in the boat frame (X forward, Y STARBOARD, Z up; origin at the waterline
	 *  amidships). Source: blender/sailing/sockets.json (parts, sockets) and blender/humans/sail_grips.json (the body's
	 *  attach point and the hands' rest places), copied by hand: change them there, then here. */
	struct FClassDef
	{
		const TCHAR* Name;             // Optimist, Cadet, Luch: SM_Sail_<Name>_<Part>
		FVector Mast, Boom, Rudder, Tiller, Ext, Board, Sprit, Jib;
		FVector JibAxis;
		bool bDagger;                  // the board slides up (else it swings)
		float BoardRaise;              // cm a daggerboard comes up when "up"
		float BoomLen, MastTop, SailHead;   // cm: the boom, the mast head over the waterline, the sail's head over the boom
		float Loa, Beam, Freeboard;
		float ExtLen;
		FVector HelmSeat;              // the helm's pelvis on the PORT deck (mirror Y for starboard)
		FVector BodyOrigin;            // where a crew body's clips have their origin (the helm's)
		float CrewAhead;               // the second body's origin this far ahead of the helm's (cadet)
		float ClipScale;               // the body scale the class's clips were posed for
		float DeckOverSole;            // cm, for bodies of another scale
		FVector SheetBlock;            // the ratchet block on the hull
		FVector BoomBlock;             // where the sheet leaves the boom, in the boom's frame
		FVector TillerGrip, SheetGrip; // the hands' rest places in the PORT clip's space (cm; +X the body's left, +Y the bow)
		float TrolleyZ;                // the boat's origin over the ground on its trolley
		float HullDraft;
	};
	const FClassDef& ClassDef(int32 BoatClass);

	/** No collision, movable, out of Lumen and the distance fields (the perf checklist's set for things that move). */
	void NoLumen(UPrimitiveComponent* C);
	/** /Game/Sailing/SM_Sail_<Name>: loaded once, rooted; null when it is not imported yet (said once in the log). */
	UStaticMesh* Asset(const FString& Name);
	/** Any static mesh by its full object path, the same way. */
	UStaticMesh* AssetAt(const FString& Path);
	/** A shared, rooted plain colour (BasicShapeMaterial "Color"). */
	UMaterialInstanceDynamic* Paint(const FLinearColor& Tint);
	/** The wake's and the spray's see-through white (the train wake's trick), shared. */
	UMaterialInterface* FoamPaint();
	/** The school's hull colourways (linear). */
	FLinearColor HullTint(int32 Colours);

	/** Meshes built in code (FLyudMeshKit), once each, rooted: the boat that sails before the Mac import, and the small
	 *  things no asset exists for (telltales, the masthead vane, a rope piece, the flag quad). Painted by PaintBuilt. */
	struct FBuilt
	{
		UStaticMesh* Mesh = nullptr;
		TArray<FName> Slots;
	};
	enum class EBuilt : uint8 { Hull, Mast, Boom, Main, Jib, Rudder, Tiller, Ext, Board, Sprit };
	const FBuilt& Fallback(int32 BoatClass, EBuilt What);
	const FBuilt& Ribbon();        // 1 cm along +X, a flat tape
	const FBuilt& Vane();          // an arrow along +X with a tail fin
	const FBuilt& RopePiece();     // 1 cm along +X
	const FBuilt& FlagQuad();      // 90 x 66 cm from the hoist along +X, hanging down
	const FBuilt& LaunchHull();
	const FBuilt& LaunchMotor();
	const FBuilt& MarkBody(int32 Kind);
	const FBuilt& TrolleyFrame();
	void PaintBuilt(UStaticMeshComponent* C, const FBuilt& Built, const FLinearColor& Main, const FLinearColor& Second);

	/** A static mesh component under Parent, registered, kept. */
	UStaticMeshComponent* Part(AActor* Actor, USceneComponent* Parent, const TCHAR* Name, TArray<TObjectPtr<UObject>>& Keep);
	USceneComponent* Frame(AActor* Actor, USceneComponent* Parent, const TCHAR* Name, TArray<TObjectPtr<UObject>>& Keep);
	/** The pooled foam discs of a wake. */
	UInstancedStaticMeshComponent* FoamPool(AActor* Actor, USceneComponent* Parent, int32 Count, TArray<TObjectPtr<UObject>>& Keep);

	/** A clip of the people's shared set (A_Human_<Name>), loaded once, rooted; null when it is not baked yet. */
	UAnimSequence* Clip(const FString& Name);
	/** A townsperson's own body for the boat to hold: the life stops drawing them (SetPresentedExternally) and their
	 *  model is spawned; null when there is no such person or they have no model. InOutId: their people-life id (-1 =
	 *  look it up by the key). */
	ANpcBody* SpawnBody(AActor* Boat, FName PersonKey, int32& InOutId);
	/** Gives the person back to the life and destroys the boat's body. */
	void ReleaseBody(AActor* Boat, TWeakObjectPtr<ANpcBody>& Body, int32& Id);
	/** The life jacket on a body's chest (the clips part's worn prop, else part C's mesh), kept by the body's actor. */
	UStaticMeshComponent* PutJacketOn(ANpcBody* Body);

	/** A puff of foam on the water. */
	struct FPuff
	{
		FVector At = FVector::ZeroVector;
		float Yaw = 0.f, Age = 0.f, Life = 0.f, Size = 0.f, Rise = 0.f;
	};
	/** A ring of puffs drawn by one instanced component: Add as the boat moves, Tick once a frame (one batch). */
	struct FWake
	{
		UInstancedStaticMeshComponent* Ism = nullptr;
		TArray<FPuff> Puffs;
		TArray<FTransform> Xf;
		int32 Next = 0;
		int32 Alive = 0;
		bool bParked = true;
		void Init(UInstancedStaticMeshComponent* InIsm, int32 Count, const FVector& At);
		void Add(const FVector& At, float Yaw, float Size, float Life, float Rise);
		void Tick(float DeltaTime, const FVector& Home);
		void Hide(const FVector& Home);
	};
}

/** One seat's body. */
struct FLyudSailCrew
{
	FName Key;
	float Age = 0.f;
	bool bJacket = true;
	int32 PersonId = -1;
	TWeakObjectPtr<ANpcBody> Body;     // kept by the world (an actor)
	TWeakObjectPtr<UStaticMeshComponent> Jacket;
	bool bTried = false;               // the body was asked for (and maybe refused): not again every frame
	// the clips' state
	enum class EPlay : uint8 { None, Sit, Cross, Fall, Swim, Right, Climb };
	EPlay Play = EPlay::None;
	FName Clip;
	float Left = 0.f;                  // seconds of a one-shot still to go
	float HandWeight = 0.f;
	bool bSailClips = true;            // false: the sailing clips are not baked, a seated stand-in plays
	int32 Side = 0;                    // the side the playing clip is of (+1 starboard, -1 port)
	int32 Level = -1;                  // the sitting pose: 0 helm, 1 half hike, 2 full hike, 3 lull, 4 bailing
	FVector Rel = FVector::ZeroVector; // where the body is in the crew's frame (eased)
	bool bRelSet = false;
	bool bHandsOn = false;
};

struct ALyudDinghy::FImpl
{
	LyudSail::FBoat B;
	LyudSail::FEnv Env;
	LyudSail::FControls Last;
	FLyudSailHudData Hud;
	FRandomStream Rng;

	// ---- the drawn boat ----
	int32 BuiltClass = -1;
	bool bAssets = false;              // the imported meshes (else the code-built boat)
	int32 Colours = 0;
	USceneComponent* Hull = nullptr;   // heels and pitches; everything hangs on it
	USceneComponent* Rig = nullptr;    // boom + sail (+ sprit), turns with the boom
	USceneComponent* CrewFrame = nullptr;
	USceneComponent* HeroSeat = nullptr;
	UStaticMeshComponent* HullMesh = nullptr;
	UStaticMeshComponent* Mast = nullptr;
	UStaticMeshComponent* Boom = nullptr;
	UStaticMeshComponent* Main = nullptr;
	UStaticMeshComponent* Sprit = nullptr;
	UStaticMeshComponent* Jib = nullptr;
	UStaticMeshComponent* Rudder = nullptr;
	UStaticMeshComponent* Tiller = nullptr;
	UStaticMeshComponent* Ext = nullptr;
	UStaticMeshComponent* Board = nullptr;
	UStaticMeshComponent* Far = nullptr;
	UStaticMeshComponent* Trolley = nullptr;
	UStaticMeshComponent* Cover = nullptr;
	UStaticMeshComponent* Rope[2] = { nullptr, nullptr };
	UStaticMeshComponent* Tell[4] = { nullptr, nullptr, nullptr, nullptr };
	UStaticMeshComponent* Vane = nullptr;
	LyudSailKit::FWake Wake;
	bool bRigged = true, bCovered = false;
	// what was last sent to the parts (they are moved only on a change)
	float SentCamber = 99.f, SentFlutter = 99.f;
	float Camber = 0.f;
	float WakeRun = 0.f, SprayRun = 0.f;
	double Clock = 0.0;
	// where it is drawn
	FVector2D DrawPos = FVector2D::ZeroVector;
	float DrawYaw = 0.f;
	float SurfaceZ = 0.f;
	float BedZ = -100000.f;
	bool bSea = false;

	// ---- stepping ----
	float MidClock = 0.f;              // the Mid step's accumulator
	float FarEnvClock = 1e9f;
	uint32 StepEvents = 0;
	bool bAssistSheet = false, bAssistHike = false, bAssistNoCapsize = false;   // the school's (SetAssist)

	// ---- the crew ----
	FLyudSailCrew Crew[2];
	int32 SitSide = 1;                 // the side the crew sits on: +1 starboard, -1 port
	float SideHold = 0.f;
	float CapsizeAlpha = 0.f;          // 0 in the boat .. 1 in the water / on the board
	float CrewOver = 0.f;              // the same for the crew's frame (0 with the hull .. 1 level on the water)
	int32 OverSide = -1;               // capsized: the side her bottom (and the board) faces

	// ---- the hero at the helm ----
	TWeakObjectPtr<AHeroCharacter> Hero;
	TWeakObjectPtr<ULyudSailCamera> Cam;
	TSharedPtr<SLyudSailHud> HudWidget;
	FVector2D Move = FVector2D::ZeroVector;
	uint64 MoveFrame = 0;
	float Tiller01 = 0.f, SheetWant = 0.6f, HikeWant = 0.f, BoardWant = 1.f;
	float SeatSide = 1.f;              // eased -1..1
	bool bLeaving = false;
	bool bTutor = true;
	int32 AdviceNow = 0;
	float AdviceHeld = 0.f;
	double AdviceSaid[16] = {};
	uint64 PromptKey = 0;

	// ---- sound ----
	LyudSail::Sound::FEventState SoundState;
	float LoopGain[LyudSail::Sound::NumLoops] = {};
	float LoopQuiet[LyudSail::Sound::NumLoops] = {};
	float HitWaterIn = -1.f, BailIn = 0.f, ClickRun = 0.f, RunQuiet = 0.f;
	float SheetSeen = 0.5f, BoardSeen = 1.f;

	// LyudDinghyBuild.cpp
	void Build(ALyudDinghy& D, int32 InClass, int32 InNumber, int32 InColours);
	void ApplyLod(ALyudDinghy& D);
	void ShowParked(ALyudDinghy& D);
	void MoveParts(ALyudDinghy& D, float DeltaTime);
	void MoveWake(ALyudDinghy& D, float DeltaTime);
	// LyudDinghy.cpp
	/** Detail: 0 the still level and the depth, 1 the surface under the middle, 2 under bow, stern and both sides. */
	void Sample(ALyudDinghy& D, int32 Detail);
	void Place(ALyudDinghy& D);
	void AfterStep(ALyudDinghy& D, float DeltaTime, bool bStepped);
	LyudSail::FControls HeroControls(ALyudDinghy& D, float DeltaTime);
	void HeroFrame(ALyudDinghy& D, float DeltaTime);
	void FillHud(ALyudDinghy& D, float DeltaTime);
	void Sounds(ALyudDinghy& D, float DeltaTime);
	void StopLoops(ALyudDinghy& D, float Fade);
	void ApplyAssists(ALyudDinghy& D);
	bool MayLeave(ALyudDinghy& D);
	void Say(const FString& Key, const TCHAR* Fallback, float Seconds);
	// LyudDinghyCrew.cpp
	void CrewSet(ALyudDinghy& D, int32 Slot, FName PersonKey, float AgeYears, bool bLifeJacket);
	void CrewDrop(ALyudDinghy& D, int32 Slot, bool bForget);
	void CrewFrameMove(ALyudDinghy& D, float DeltaTime);
	void CrewTick(ALyudDinghy& D, float DeltaTime);
	void CrewHands(ALyudDinghy& D, int32 Slot, ULyudBodyAnimInstance* Anim, float BodyScale);
	/** The helm's tiller hand in the hull's frame (cm): where the extension points. */
	FVector TillerHand(const ALyudDinghy& D) const;
};

struct ALyudSailLaunch::FImpl
{
	LyudSail::FLaunch L;
	LyudSail::FEnv Env;
	FRandomStream Rng;
	USceneComponent* Hull = nullptr;
	UStaticMeshComponent* HullMesh = nullptr;
	UStaticMeshComponent* Motor = nullptr;
	UStaticMeshComponent* Staff = nullptr;
	UStaticMeshComponent* Flag = nullptr;
	LyudSailKit::FWake Wake;
	FName FlagId;
	FName DriverKey;
	int32 DriverId = -1;
	TWeakObjectPtr<ANpcBody> Driver;
	bool bDriverTried = false;
	bool bBuilt = false;
	bool bRunning = false;
	float Turn = 0.f;
	float WakeRun = 0.f;
	float LoopGain[4] = {};
	float LoopQuiet[4] = {};
	float SurfaceZ = 0.f;
	float SlapIn = 1.f;
	float FlagYaw = 180.f;
	float ViewDist = 0.f;              // cm from the player's view, refreshed by Advance
	bool bFlagAsset = false;
	bool bParked = false;
	void Build(ALyudSailLaunch& A);
	void Sample(ALyudSailLaunch& A);
	void Place(ALyudSailLaunch& A, float DeltaTime);
	void Engine(ALyudSailLaunch& A, float DeltaTime);
	void StopEngine(ALyudSailLaunch& A, float Fade);
	void DressFlag(ALyudSailLaunch& A);
	void DriverTick(ALyudSailLaunch& A);
};
