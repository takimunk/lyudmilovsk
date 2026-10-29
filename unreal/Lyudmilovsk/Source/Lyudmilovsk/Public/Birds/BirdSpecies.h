#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BirdSpecies.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;

/** How a species flies (Content/BirdsData/species.json "flight"). */
UENUM(BlueprintType)
enum class EBirdFlightStyle : uint8
{
	Flapping,     // steady beats (crows, starling, cormorant)
	Bounding,     // a burst of beats, then a dip on folded wings (tits, finches, sparrows)
	Undulating,   // beats with short open-wing glides (thrushes)
	Clapping,     // pigeons: fast beats, a clap on takeoff, glides with the wings raised in a V
	Soaring,      // gulls: slow beats, long glides
	Flicker,      // swifts, swallows, martins: flickering beats between long sickle-wing glides
};

UENUM(BlueprintType)
enum class EBirdGait : uint8
{
	Hop,
	Walk,
	Both,
	Cling,        // the swift: never on the ground, clings to walls and eaves
};

/** What a bird's body is doing: ABirdActor plays the clip for it (A_<Id>_<Clip>). */
UENUM(BlueprintType)
enum class EBirdSex : uint8
{
	Male,
	Female,
};

UENUM(BlueprintType)
enum class EBirdState : uint8
{
	Perch,        // standing still, breathing, the odd tail flick (clip Idle)
	Look,         // turning the head about (clip Look; SetLookTarget aims it instead)
	Alert,        // stretched up, sleeked, frozen (clip Alert)
	Hop,          // clip Hop, one hop per stride, rate from velocity
	Walk,         // clip Walk (pigeons nod), rate from velocity
	Peck,         // one peck at the ground (clip Peck)
	Eat,          // pick up, mandibles work, swallow (clip Eat)
	Preen,        // clip Preen
	Call,         // beak opens, throat swells (clip Call)
	Takeoff,      // one shot, ends in Fly
	Fly,          // flapping (clip Flap at WingbeatHz; Bounding/Flicker species mix in Bound/Glide by themselves)
	Glide,        // wings spread (clip Glide)
	Land,         // one shot, ends in Perch
	Sleep,        // head tucked, fluffed up (clip Sleep)
};

/** One row of Content/BirdsData/species.json. */
USTRUCT(BlueprintType)
struct LYUDMILOVSK_API FBirdSpecies
{
	GENERATED_BODY()

	/** CamelCase id, also the asset folder: /Game/Birds/<Id>/SK_<Id>. */
	UPROPERTY(BlueprintReadOnly) FName Id;
	/** Localization key stem: LyudText::Get("bird.<Key>.name"). */
	UPROPERTY(BlueprintReadOnly) FString Key;
	UPROPERTY(BlueprintReadOnly) FString Latin;
	/** Beak tip to tail tip, and wing tip to wing tip, cm (the meshes are built to these). */
	UPROPERTY(BlueprintReadOnly) float LengthCm = 20.f;
	UPROPERTY(BlueprintReadOnly) float WingspanCm = 35.f;
	UPROPERTY(BlueprintReadOnly) float MassG = 50.f;
	UPROPERTY(BlueprintReadOnly) EBirdFlightStyle Flight = EBirdFlightStyle::Flapping;
	/** Wingbeats per second in steady flight: the rate to play clip Flap at (it holds one beat over 1 s). */
	UPROPERTY(BlueprintReadOnly) float WingbeatHz = 10.f;
	/** Travelling flight speed, m/s. */
	UPROPERTY(BlueprintReadOnly) float CruiseMs = 10.f;
	UPROPERTY(BlueprintReadOnly) EBirdGait Gait = EBirdGait::Hop;
	/** Ground covered by one Hop / Walk clip loop, cm (the clips hold one stride over 1 s). */
	UPROPERTY(BlueprintReadOnly) float StrideCm = 4.f;
	UPROPERTY(BlueprintReadOnly) int32 FlockMin = 1;
	UPROPERTY(BlueprintReadOnly) int32 FlockMax = 1;
	/** Typical distance at which an approaching person makes it fly, m (0: never on the ground). */
	UPROPERTY(BlueprintReadOnly) float FleeM = 10.f;
	UPROPERTY(BlueprintReadOnly) bool bPredator = false;
	/** Main plumage colours, darkest-first is not guaranteed (far LODs, map icons, UI swatches). */
	UPROPERTY(BlueprintReadOnly) TArray<FLinearColor> Palette;
	/** Call ids (sound assets /Game/Birds/<Id>/Sounds/S_<Id>_<call>, when there are any). */
	UPROPERTY(BlueprintReadOnly) TArray<FName> Calls;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Diet;
	/** Months (1-12) the species is in the Baltic; migrants leave. */
	UPROPERTY(BlueprintReadOnly) TArray<int32> Months;
	/** Months it wears its winter plumage (gulls without hoods, spangled starlings), and that plumage's colours; empty if it doesn't change. */
	UPROPERTY(BlueprintReadOnly) TArray<int32> WinterMonths;
	UPROPERTY(BlueprintReadOnly) TArray<FLinearColor> WinterPalette;

	bool IsPresentIn(int32 Month) const { return Months.Num() == 0 || Months.Contains(Month); }
	bool InWinterPlumage(int32 Month) const { return WinterMonths.Contains(Month); }

	/** The female looks different (her own material set: MI_<Id>_*_Female, MI_<Id>_*_FemaleWinter). */
	UPROPERTY(BlueprintReadOnly) bool bFemalePlumage = false;
	/** Her length relative to the male's (raptor females are larger, most others a touch smaller). */
	UPROPERTY(BlueprintReadOnly) float FemaleScale = 1.f;
	/** Her outermost tail feathers relative to his (the swallow's streamers). */
	UPROPERTY(BlueprintReadOnly) float FemaleStreamer = 1.f;

	FText DisplayName() const;
	float LengthM() const { return LengthCm * 0.01f; }
};

/**
 * Reads Content/BirdsData/species.json once (Reload() re-reads it) and finds each species' assets:
 *   /Game/Birds/<Id>/SK_<Id>               skeletal mesh, forward +X, up +Z, origin between the feet (perched)
 *   /Game/Birds/<Id>/Anims/A_<Id>_<Clip>   Idle Look Alert Hop Walk Peck Eat Preen Call Takeoff Flap Glide Bound Land Sleep
 *   /Game/Birds/<Id>/SM_<Id>_<Pose>        static poses for instanced far birds: Perch Peck Hop Walk Glide Sleep Flap0..Flap3
 * Bone names are the same in every species: root body neck head jaw tail, humerus/forearm/hand.L|R, thigh/shin/tarsus/toes.L|R,
 * plus feather bones; sockets eye_l / eye_r on head (the bird's own camera).
 */
UCLASS()
class LYUDMILOVSK_API UBirdSpeciesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static bool GetSpecies(FName Id, FBirdSpecies& Out);

	/** Every species id in table order (predators last). */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static TArray<FName> AllSpeciesIds();

	/** The row, or null. Pointers stay valid until Reload(). */
	static const FBirdSpecies* Find(FName Id);
	static const TArray<FBirdSpecies>& All();
	static void Reload();

	UFUNCTION(BlueprintCallable, Category = "Birds")
	static USkeletalMesh* LoadSkeletalMesh(FName Id);
	/** SK_<Id>_Hero (the player's detailed body, on the same skeleton and clips), or null if the species has none. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static USkeletalMesh* LoadHeroMesh(FName Id);
	/** The hero body can be worn from its own eye (ABirdActor::SetFirstPerson): it has the "Skull" material slot. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static bool HasFirstPersonBody(FName Id);
	/** Feral pigeon colour morphs of the hero body: 0 BlueBar (the base) .. Count-1; the name is the material suffix
	 *  ("" for the base). Pick is weighted by how common each is in a city flock. */
	static int32 PigeonMorphCount();
	static FString PigeonMorphName(int32 Morph);
	static int32 PickPigeonMorph(int32 Seed);
	/** Clip is one of the names above (Idle, Flap, ...); null if that species has none. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static UAnimSequence* LoadClip(FName Id, FName Clip);
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static UStaticMesh* LoadPose(FName Id, FName Pose);

	/** Import tooling (unreal/scripts/import_birds.py): screen size per LOD, persisted in the mesh. */
	UFUNCTION(BlueprintCallable, Category = "Birds")
	static void SetLODScreenSizes(USkeletalMesh* Mesh, const TArray<float>& ScreenSizes);

	/** The clip that shows a state (Perch -> Idle, Fly -> Flap ...). */
	static FName ClipForState(EBirdState State);
};
