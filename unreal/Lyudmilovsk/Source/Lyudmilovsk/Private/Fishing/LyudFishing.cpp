#include "Fishing/LyudFishing.h"

#include "../Sea/FishModel.h"
#include "Audio/LyudSoundscape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FishingModel.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudEcosystem.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudUnderwater.h"
#include "UI/LyudGameScreen.h"

namespace LyudF = LyudFishing;

const FName ULyudFishing::RodId(TEXT("fishing_rod"));
const FName ULyudFishingState::DinerFishId(TEXT("diner_fish"));

namespace
{
	TAutoConsoleVariable<int32> CVarFishing(TEXT("lyud.Fishing"), 1, TEXT("The fishing rod's mouse buttons (0: the rod comes out but does nothing)."));

	float FishEase(float T) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f)); }

	/** The right palm's frame from the hand's bones (as ULyudHandUse's): X the fingers, Z out of the palm, world. */
	bool FishBonePalm(const USkeletalMeshComponent* Body, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		const FVector Across = Body->GetBoneLocation(TEXT("thumb_01_r")) - Body->GetBoneLocation(TEXT("pinky_01_r"));
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		if (FVector::DotProduct(Pn, Normal) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Wrist + (Knuckle - Wrist) * 0.6f);
		return true;
	}

	/**
	 * The rod in the palm: a hammer grip on the reel seat, the reel's foot between the middle and ring fingers. The rod runs
	 * along the palm's Y (across the palm, thumb side forward), the reel hangs on the fingers' side (palm -X), the rod's axis a
	 * finger's thickness out of the palm.
	 */
	FTransform RodInPalm()
	{
		return FTransform(FRotationMatrix::MakeFromXZ(FVector(0.f, 1.f, 0.f), FVector(-1.f, 0.f, 0.f)).ToQuat(), FVector(0.f, 0.f, 1.7f));
	}

	/** The hand holding the rod at Pos (look frame: forward, right, up from the eyes), the rod raised Elev degrees and turned Yaw. */
	FHeroHoldPose RodPose(const FVector& Pos, float Elev, float Yaw)
	{
		const float E = FMath::DegreesToRadians(Elev), Y = FMath::DegreesToRadians(Yaw);
		const FVector R(FMath::Cos(E) * FMath::Cos(Y), FMath::Cos(E) * FMath::Sin(Y), FMath::Sin(E));
		const FVector N0(0.f, -1.f, 0.f);
		const FVector N = (N0 - R * FVector::DotProduct(N0, R)).GetSafeNormal();
		FHeroHoldPose P;
		P.Pos = Pos;
		P.FingerDir = FVector::CrossProduct(R, N).GetSafeNormal();
		P.PalmNormal = N;
		P.Curl = 0.9f;
		P.PitchFollow = 0.5f;
		P.BlendTime = 0.35f;
		// the fingers close on the reel seat
		P.Shape.AddCapsule(FVector(0.f, 0.f, 1.7f), FVector(0.f, 1.f, 0.f), 5.f, 1.1f);
		return P;
	}

	/** The species' mean group (a school is one weight in the web for many fish on the hook). */
	float SchoolFactor(int32 Species)
	{
		const LyudFish::FSpecies& Sp = LyudFish::Species(Species);
		return FMath::Sqrt(0.5f * float(Sp.GroupMin + Sp.GroupMax));
	}

	const TCHAR* FishStateName(LyudF::EState S)
	{
		switch (S)
		{
		case LyudF::EState::Idle: return TEXT("idle");
		case LyudF::EState::Charging: return TEXT("charging");
		case LyudF::EState::Swing: return TEXT("swing");
		case LyudF::EState::Flying: return TEXT("flying");
		case LyudF::EState::Waiting: return TEXT("waiting");
		case LyudF::EState::Nibbling: return TEXT("nibbling");
		case LyudF::EState::Taking: return TEXT("taking");
		case LyudF::EState::Fighting: return TEXT("fighting");
		case LyudF::EState::Landing: return TEXT("landing");
		case LyudF::EState::Winding: return TEXT("winding");
		}
		return TEXT("?");
	}

	FName FishItem(int32 Index)
	{
		return FName(ANSI_TO_TCHAR(LyudF::ItemOf(LyudFishing::Catchable(Index).Id).c_str()));
	}
	FText FishName(FName Species)
	{
		return LyudText::Get(FString::Printf(TEXT("fish.%s.name"), *Species.ToString().ToLower()));
	}
}

// ------------------------------------------------------------------ the saved state

ULyudFishingState* ULyudFishingState::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudFishingState>() : nullptr;
}

void ULyudFishingState::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Fishing"), this);
	}
	Reset();
}

void ULyudFishingState::Reset()
{
	Bait = TEXT("bait_worm");
	bBaited = false;
	Log.Reset();
	BagGrams.Reset();
	Total = 0;
	Cooked = 0;
}

int32 ULyudFishingState::FishHeld(const UObject* WorldContext, int32* OutRoubles) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	int32 N = 0, Roubles = 0;
	for (int32 C = 0; Inv && C < LyudF::NumCatchable(); C++)
	{
		const int32 Held = Inv->Count(FishItem(C));
		if (Held <= 0) continue;
		N += Held;
		// each priced at the mean weight of those held (a fish given or found: the species' usual weight)
		const FName Species(ANSI_TO_TCHAR(LyudF::Catchable(C).Id));
		const float* G = BagGrams.Find(Species);
		const float Usual = LyudF::Catchable(C).K * FMath::Cube(LyudF::Catchable(C).Length * 100.f) / 100.f;
		const float Each = G && *G > 0.f ? *G / Held : Usual;
		Roubles += Held * LyudF::PriceOf(C, Each);
	}
	if (OutRoubles) *OutRoubles = Roubles;
	return N;
}

int32 ULyudFishingState::SellAll(const UObject* WorldContext)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	int32 Roubles = 0;
	if (!Inv || FishHeld(WorldContext, &Roubles) == 0)
	{
		return 0;
	}
	for (int32 C = 0; C < LyudF::NumCatchable(); C++)
	{
		const FName Item = FishItem(C);
		const int32 Held = Inv->Count(Item);
		// the ones nobody buys (sand gobies, sticklebacks) stay in the pocket
		if (Held > 0 && LyudF::Catchable(C).PerKilo > 0)
		{
			Inv->Remove(Item, Held);
			BagGrams.Remove(FName(ANSI_TO_TCHAR(LyudF::Catchable(C).Id)));
		}
	}
	Inv->Earn(Roubles);
	UE_LOG(LogTemp, Display, TEXT("FISHING sold for %d roubles"), Roubles);
	return Roubles;
}

FName ULyudFishingState::BiggestHeld(const UObject* WorldContext, float* OutGrams) const
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	FName Best;
	float BestGrams = 0.f;
	for (int32 C = 0; Inv && C < LyudF::NumCatchable(); C++)
	{
		const int32 Held = Inv->Count(FishItem(C));
		if (Held <= 0) continue;
		const float* G = BagGrams.Find(FName(ANSI_TO_TCHAR(LyudF::Catchable(C).Id)));
		const float Usual = LyudF::Catchable(C).K * FMath::Cube(LyudF::Catchable(C).Length * 100.f) / 100.f;
		const float Each = G && *G > 0.f ? *G / Held : Usual;
		if (Best.IsNone() || Each > BestGrams)
		{
			Best = FishItem(C);
			BestGrams = Each;
		}
	}
	if (OutGrams) *OutGrams = BestGrams;
	return Best;
}

float ULyudFishingState::TakeOne(const UObject* WorldContext, FName Item)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	const int32 Held = Inv ? Inv->Count(Item) : 0;
	if (Held <= 0)
	{
		return 0.f;
	}
	float Grams = 0.f;
	for (int32 C = 0; C < LyudF::NumCatchable(); C++)
	{
		if (FishItem(C) != Item) continue;
		const FName Species(ANSI_TO_TCHAR(LyudF::Catchable(C).Id));
		const float Usual = LyudF::Catchable(C).K * FMath::Cube(LyudF::Catchable(C).Length * 100.f) / 100.f;
		if (float* G = BagGrams.Find(Species); G && *G > 0.f)
		{
			Grams = *G / Held;
			*G -= Grams;
			if (Held == 1) BagGrams.Remove(Species);
		}
		else
		{
			Grams = Usual;
		}
		break;
	}
	Inv->Remove(Item, 1);
	return Grams;
}

bool ULyudFishingState::Cook(const UObject* WorldContext, FName Item, int32 Fee)
{
	ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(WorldContext);
	if (!Inv || !UI || Item.IsNone() || Inv->Count(Item) <= 0 || !Inv->Spend(Fee))
	{
		return false;
	}
	const float Grams = TakeOne(WorldContext, Item);
	// the plate goes the way of anything ordered at the diner: into the pocket for a moment, then ALyudDiner takes it back out
	// and the waitress brings it to the counter in front of the hero
	Inv->Add(DinerFishId);
	UI->OnShopBought.Broadcast(DinerFishId, 1);
	Cooked++;
	UE_LOG(LogTemp, Display, TEXT("FISHING cooked %s (%.0f g) for %d roubles"), *Item.ToString(), Grams, Fee);
	return true;
}

FLyudCatchRecord* ULyudFishingState::Find(FName Species)
{
	return Log.FindByPredicate([Species](const FLyudCatchRecord& R) { return R.Species == Species; });
}

bool ULyudFishingState::Record(FName Species, float Grams, float Cm, const FDateTime& When)
{
	Total++;
	FLyudCatchRecord* R = Find(Species);
	if (!R)
	{
		R = &Log.AddDefaulted_GetRef();
		R->Species = Species;
		R->First = When;
		R->Count = 1;
		R->BiggestGrams = Grams;
		R->BiggestCm = Cm;
		return true;
	}
	R->Count++;
	if (Grams > R->BiggestGrams)
	{
		R->BiggestGrams = Grams;
		R->BiggestCm = Cm;
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ the subsystem

ULyudFishing::ULyudFishing() = default;
ULyudFishing::~ULyudFishing() = default;

ULyudFishing* ULyudFishing::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudFishing>() : nullptr;
}

bool ULyudFishing::HasHand(const UObject* WorldContext)
{
	const ULyudFishing* F = Get(WorldContext);
	return F && F->IsOut();
}

bool ULyudFishing::IsLineOut() const
{
	return Session && Session->State() != LyudF::EState::Idle && Session->State() != LyudF::EState::Charging;
}

void ULyudFishing::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Session = MakePimpl<LyudF::FSession>(uint32(FPlatformTime::Cycles() | 1u));
	// every level start is a new game unless a load restores the state right after (as the inventory)
	if (ULyudFishingState* State = ULyudFishingState::Get(&InWorld))
	{
		State->Reset();
	}
}

void ULyudFishing::Deinitialize()
{
	for (ALyudFishingRod* R : { Rod.Get(), Parked.Get() })
	{
		if (R) R->Destroy();
	}
	Rod.Reset();
	Parked.Reset();
	Super::Deinitialize();
}

FTransform ULyudFishing::LookFrame() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Hero)
	{
		return FTransform::Identity;
	}
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	const float Yaw = PC ? PC->GetControlRotation().Yaw : Hero->GetActorRotation().Yaw;
	return FTransform(FRotator(0.f, Yaw, 0.f), Hero->EyeLocation());
}

void ULyudFishing::Sound(FName Pool, const FVector& Where, float Volume)
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, Where, Volume);
	}
}

void ULyudFishing::Say(const FText& Text, float Seconds)
{
	Said = Text;
	SayLeft = Seconds;
	SayCount++;
}

void ULyudFishing::SetBait(FName Item)
{
	ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!State || State->Bait == Item)
	{
		return;
	}
	// the old bait comes off the hook back into its box
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (Inv && State->bBaited && !State->Bait.IsNone())
	{
		const int32 Old = LyudF::FindBait(TCHAR_TO_ANSI(*State->Bait.ToString()));
		if (Old >= 0 && !LyudFishing::Bait(Old).bLure)
		{
			Inv->Add(State->Bait);
		}
	}
	State->Bait = Item;
	State->bBaited = false;
	if (Session)
	{
		Session->SetBait(Item.IsNone() ? -1 : LyudF::FindBait(TCHAR_TO_ANSI(*Item.ToString())));
	}
	UE_LOG(LogTemp, Display, TEXT("FISHING bait %s"), *Item.ToString());
}

void ULyudFishing::CycleBait()
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!Inv || !State)
	{
		return;
	}
	TArray<FName> Held;
	for (int32 B = 0; B < LyudF::NumBaits(); B++)
	{
		const FName Id(ANSI_TO_TCHAR(LyudFishing::Bait(B).Item));
		if (Inv->Count(Id) > 0 || (State->Bait == Id && State->bBaited))
		{
			Held.Add(Id);
		}
	}
	if (Held.Num() == 0)
	{
		return;
	}
	const int32 At = Held.IndexOfByKey(State->Bait);
	SetBait(Held[(At + 1) % Held.Num()]);
}

void ULyudFishing::SyncBait()
{
	// the hook baited from the pocket when it is bare and there is bait (a spoon is tied on once and stays)
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	if (!Inv || !State || !Session)
	{
		return;
	}
	const int32 B = State->Bait.IsNone() ? -1 : LyudF::FindBait(TCHAR_TO_ANSI(*State->Bait.ToString()));
	if (Session->GetBait() != B)
	{
		Session->SetBait(B);
	}
	if (B < 0)
	{
		return;
	}
	if (LyudFishing::Bait(B).bLure)
	{
		State->bBaited = Inv->Count(State->Bait) > 0;
		if (State->bBaited && !Session->IsBaited()) Session->Rebait();
		if (!State->bBaited && Session->IsBaited()) Session->Unbait();
		return;
	}
	if (!Session->IsBaited() && State->bBaited)
	{
		Session->Rebait();
	}
	if (!Session->IsBaited() && Inv->Count(State->Bait) > 0)
	{
		Inv->Remove(State->Bait);
		Session->Rebait();
		State->bBaited = true;
	}
}

void ULyudFishing::UpdateWeights()
{
	const ULyudEcosystem* Eco = ULyudEcosystem::Get(this);
	std::vector<float> W(LyudF::NumCatchable(), 0.f);
	if (Eco)
	{
		for (const TPair<FName, float>& It : Eco->GetBiteWeights(EndPos))
		{
			const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*It.Key.ToString()));
			const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*It.Key.ToString()));
			if (C >= 0 && S >= 0)
			{
				W[C] = It.Value * SchoolFactor(S);
			}
		}
	}
	Session->SetBiteWeights(W);
}

bool ULyudFishing::IsWater(const FVector& Where) const
{
	const ALyudSea* Sea = ALyudSea::Get(this);
	return Sea && Sea->DepthAt(Where) > 25.f;
}

FVector ULyudFishing::OnWater(const FVector2D& XY, float Below) const
{
	const ALyudSea* Sea = ALyudSea::Get(this);
	const FVector P(XY.X, XY.Y, 0.f);
	const float Z = Sea ? Sea->SurfaceZ(P) : Origin.Z;
	return FVector(XY.X, XY.Y, Z - Below);
}

void ULyudFishing::TakeOut()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Look = LookFrame();
	// the rod put away last time comes back out; only the first take-out in a world builds one
	ALyudFishingRod* R = Parked.Get();
	Parked.Reset();
	if (R)
	{
		R->SetActorLocationAndRotation(Look.TransformPosition(FVector(30.f, 20.f, -60.f)), Look.Rotator());
	}
	else
	{
		R = World->SpawnActor<ALyudFishingRod>(Look.TransformPosition(FVector(30.f, 20.f, -60.f)), Look.Rotator(), Params);
	}
	if (!R)
	{
		return;
	}
	R->SetActorHiddenInGame(true);
	Rod = R;
	bAttached = false;
	bHidden = true;
	HoldTime = 0.f;
	ShowLeft = -1.f;
	Session->Reset();
	if (ULyudFishingState* State = ULyudFishingState::Get(this))
	{
		Session->SetBait(State->Bait.IsNone() ? -1 : LyudF::FindBait(TCHAR_TO_ANSI(*State->Bait.ToString())));
		if (State->bBaited) Session->Rebait();
	}
	PosePos = FVector(30.f, 22.f, -50.f);
	PoseElev = 20.f;
	PoseYaw = -4.f;
	UE_LOG(LogTemp, Display, TEXT("FISHING rod out"));
}

void ULyudFishing::PutAway()
{
	if (Session)
	{
		Session->Reset();
	}
	if (ALyudFishingRod* R = Rod.Get())
	{
		R->Park();
		Parked = R;
	}
	Rod.Reset();
	bAttached = false;
	bFlying = false;
	PromptKey = ~0ull;
	ShowLeft = -1.f;
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		UHeroReachComponent::Get(Hero)->ClearOverrideHold();
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	LastPrompt = FText::GetEmpty();
	UE_LOG(LogTemp, Display, TEXT("FISHING rod away"));
}

FString ULyudFishing::DebugState() const
{
	if (!Session)
	{
		return TEXT("no session");
	}
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	const LyudF::FFish& F = Session->Fish();
	return FString::Printf(TEXT("out %d state %s t %.1f dist %.1f tension %.2f dip %.2f stamina %.2f bait %s baited %d fish %s %.0f g bites/min %.2f total %d"),
		IsOut() ? 1 : 0, FishStateName(Session->State()), Session->Time(), Session->Distance(), Session->Tension(), Session->FloatDip(), Session->Stamina(),
		State ? *State->Bait.ToString() : TEXT("-"), Session->IsBaited() ? 1 : 0, F.Species >= 0 ? ANSI_TO_TCHAR(LyudF::Catchable(F.Species).Id) : TEXT("-"), F.Grams,
		Session->BitesPerMinute(), State ? State->Total : 0);
}

void ULyudFishing::ForceBite()
{
	if (!Session) return;
	UpdateWeights();
	// the likeliest fish here
	int32 Best = LyudF::FindCatchable("RoundGoby");
	float BestW = -1.f;
	const ULyudEcosystem* Eco = ULyudEcosystem::Get(this);
	if (Eco)
	{
		for (const TPair<FName, float>& It : Eco->GetBiteWeights(EndPos))
		{
			const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*It.Key.ToString()));
			if (C >= 0 && It.Value * LyudF::BaitLiking(C, Session->GetBait()) > BestW)
			{
				BestW = It.Value * LyudF::BaitLiking(C, Session->GetBait());
				Best = C;
			}
		}
	}
	Session->DebugBite(Best);
	HandleEvents();
}

void ULyudFishing::ForceHook(FName Species)
{
	ForcedHook = Species;
}

FHeroHoldPose ULyudFishing::PoseNow(float DeltaTime)
{
	// targets per state; the hand follows them quickly (the swing exactly)
	FVector& Pos = PosePos;
	float& Elev = PoseElev;
	float& Yaw = PoseYaw;
	FVector TPos(40.f, 20.f, -28.f);
	float TElev = 32.f, TYaw = -4.f, Rate = 9.f;
	const LyudF::EState S = Session->State();
	const float T = Session->Time();
	switch (S)
	{
	case LyudF::EState::Idle:
		if (ShowLeft > 0.f)
		{
			// the fish lifted to the eyes on the line
			TPos = FVector(30.f, 16.f, -12.f);
			TElev = 52.f;
			TYaw = -12.f;
		}
		break;
	case LyudF::EState::Charging:
	{
		// the rod goes back over the shoulder as the swing builds
		const float C = FishEase(Session->Charge());
		TPos = FMath::Lerp(FVector(38.f, 20.f, -26.f), FVector(12.f, 26.f, 2.f), C);
		TElev = FMath::Lerp(40.f, 105.f, C);
		Rate = 14.f;
		break;
	}
	case LyudF::EState::Swing:
	{
		// and comes forward hard, stopping low toward the water
		const float W = FMath::Clamp(T / Session->Tune.SwingTime, 0.f, 1.f);
		const float E = W * W * (3.f - 2.f * W);
		TPos = FMath::Lerp(FVector(12.f, 26.f, 2.f), FVector(54.f, 18.f, -22.f), E);
		TElev = FMath::Lerp(105.f, 12.f, E);
		Rate = 40.f;
		break;
	}
	case LyudF::EState::Flying:
		TPos = FVector(52.f, 18.f, -22.f);
		TElev = FMath::Lerp(12.f, 24.f, FishEase(T / 0.8f));
		break;
	case LyudF::EState::Waiting:
	case LyudF::EState::Nibbling:
	case LyudF::EState::Taking:
		TPos = FVector(42.f, 20.f, -28.f);
		TElev = 24.f;
		break;
	case LyudF::EState::Fighting:
	{
		// up and back against the fish, the tip following it left and right; it shakes on a run
		const float Te = Session->Tension();
		TPos = FVector(36.f - 8.f * Te, 22.f, -18.f + 6.f * Te);
		TElev = 50.f + 18.f * Te;
		TYaw = -4.f + 14.f * Session->Side();
		if (Session->IsRunning())
		{
			const float Now = GetWorld()->GetTimeSeconds();
			TPos += FVector(0.f, 0.6f * FMath::Sin(Now * 31.f), 0.8f * FMath::Sin(Now * 23.f)) * Te;
		}
		Rate = 12.f;
		break;
	}
	case LyudF::EState::Landing:
		TPos = FVector(26.f, 18.f, -4.f);
		TElev = 72.f;
		Rate = 8.f;
		break;
	case LyudF::EState::Winding:
		TPos = FVector(40.f, 20.f, -26.f);
		TElev = 34.f;
		break;
	}
	// the strike: a jerk up and back
	TElev += 30.f * Strike;
	TPos += FVector(-8.f, 0.f, 10.f) * Strike;
	const float A = 1.f - FMath::Exp(-Rate * DeltaTime);
	Pos = FMath::Lerp(Pos, TPos, A);
	Elev = FMath::Lerp(Elev, TElev, A);
	Yaw = FMath::Lerp(Yaw, TYaw, A);
	return RodPose(Pos, Elev, Yaw);
}

void ULyudFishing::HandleEvents()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudFishingState* State = ULyudFishingState::Get(this);
	ALyudFishingRod* R = Rod.Get();
	for (LyudF::EEvent E : Session->Events())
	{
		switch (E)
		{
		case LyudF::EEvent::CastStart:
			if (R) Sound(TEXT("rod_swish"), R->Tip(), 0.8f);
			break;
		case LyudF::EEvent::Cast:
		{
			// the float flies from the tip along the look, landing Throw metres out (a little further when cast upward)
			const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
			const FRotator Look = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
			Dir = FRotator(0.f, Look.Yaw, 0.f).Vector();
			const float Up = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch), -10.f, 35.f);
			const float Throw = Session->CastDistance() * (1.f + Up / 90.f) * 100.f;
			FlyFrom = R ? R->Tip() : LookFrame().GetLocation();
			const AActor* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
			Origin = Hero ? Hero->GetActorLocation() : FlyFrom;
			const FVector2D To = FVector2D(Origin) + FVector2D(Dir) * Throw;
			FlyTo = OnWater(To, 0.f);
			if (!IsWater(FlyTo))
			{
				// no sea under it: down to the ground there (the trace in Fly finds it)
				FlyTo.Z = FlyFrom.Z - 400.f;
			}
			FlyDur = 0.45f + Throw / 2400.f;
			FlyTime = 0.f;
			FlyPos = FlyFrom;
			bFlying = true;
			UE_LOG(LogTemp, Display, TEXT("FISHING cast %.1f m"), Throw * 0.01f);
			break;
		}
		case LyudF::EEvent::Landed:
			Sound(TEXT("float_plop"), EndPos, 0.7f);
			UpdateWeights();
			UE_LOG(LogTemp, Display, TEXT("FISHING landed %.1f m at %s bites/min %.2f"), Session->Distance(), *EndPos.ToCompactString(), Session->BitesPerMinute());
			break;
		case LyudF::EEvent::Ashore:
			Say(LyudText::Get(TEXT("ui.fish.ashore")), 2.5f);
			UE_LOG(LogTemp, Display, TEXT("FISHING ashore"));
			break;
		case LyudF::EEvent::Nibble:
			UE_LOG(LogTemp, Display, TEXT("FISHING nibble"));
			if (R && Session->BitingFish() >= 0)
			{
				// its look loads in the background while it nibbles, so the strike shows it without a hitch
				R->PrefetchFish(FName(ANSI_TO_TCHAR(LyudF::Catchable(Session->BitingFish()).Id)));
			}
			break;
		case LyudF::EEvent::Take:
			if (R && Session->BitingFish() >= 0)
			{
				R->PrefetchFish(FName(ANSI_TO_TCHAR(LyudF::Catchable(Session->BitingFish()).Id)));
			}
			Sound(TEXT("float_plop"), EndPos, 0.35f);
			UE_LOG(LogTemp, Display, TEXT("FISHING take"));
			break;
		case LyudF::EEvent::Hooked:
		{
			Strike = 1.f;
			const LyudF::FFish& F = Session->Fish();
			const FName Species(ANSI_TO_TCHAR(LyudF::Catchable(F.Species).Id));
			if (R) R->SetFish(Species, F.Length);
			// the one the diver would see there is this one
			if (ULyudUnderwaterSubsystem* Under = ULyudUnderwaterSubsystem::Get(this))
			{
				Under->TakeFish(Species, EndPos, 1500.f);
			}
			Sound(TEXT("fish_splash"), EndPos, 0.6f);
			UE_LOG(LogTemp, Display, TEXT("FISHING hooked %s %.0f g %.1f cm"), *Species.ToString(), F.Grams, F.Length * 100.f);
			break;
		}
		case LyudF::EEvent::Missed:
			Strike = 1.f;
			UE_LOG(LogTemp, Display, TEXT("FISHING missed"));
			break;
		case LyudF::EEvent::Gone:
			Say(LyudText::Get(TEXT("ui.fish.gone")), 2.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING gone"));
			break;
		case LyudF::EEvent::BaitStolen:
			if (State) State->bBaited = false;
			Say(LyudText::Get(TEXT("ui.fish.bait_stolen")), 2.5f);
			UE_LOG(LogTemp, Display, TEXT("FISHING bait stolen"));
			break;
		case LyudF::EEvent::Run:
			if (R && Session->State() == LyudF::EState::Fighting) Sound(TEXT("reel_drag"), R->GetActorLocation(), 0.5f);
			break;
		case LyudF::EEvent::Snapped:
		{
			if (R) Sound(TEXT("line_snap"), R->Tip(), 0.9f);
			// a spoon goes with the line
			if (State && Inv && !State->Bait.IsNone())
			{
				const int32 B = LyudF::FindBait(TCHAR_TO_ANSI(*State->Bait.ToString()));
				if (B >= 0 && LyudFishing::Bait(B).bLure) Inv->Remove(State->Bait);
				State->bBaited = false;
			}
			if (R) R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
			Say(LyudText::Get(TEXT("ui.fish.snapped")), 3.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING snapped"));
			break;
		}
		case LyudF::EEvent::Escaped:
			if (State) State->bBaited = Session->IsBaited();
			if (R) R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
			Say(LyudText::Get(TEXT("ui.fish.escaped")), 3.f);
			UE_LOG(LogTemp, Display, TEXT("FISHING escaped"));
			break;
		case LyudF::EEvent::Landing:
			Sound(TEXT("fish_splash"), EndPos, 0.8f);
			break;
		case LyudF::EEvent::Caught:
			Caught();
			break;
		case LyudF::EEvent::Retrieved:
			UE_LOG(LogTemp, Display, TEXT("FISHING retrieved"));
			break;
		}
	}
}

void ULyudFishing::Caught()
{
	const LyudF::FFish& F = Session->Fish();
	if (F.Species < 0)
	{
		return;
	}
	const FName Species(ANSI_TO_TCHAR(LyudF::Catchable(F.Species).Id));
	ULyudFishingState* State = ULyudFishingState::Get(this);
	if (State)
	{
		State->bBaited = Session->IsBaited();
	}
	ShowSpecies = Species;
	ShowGrams = F.Grams;
	ShowCm = F.Length * 100.f;
	ShowLeft = 2.4f;
	// out of the web and its spot
	if (ULyudEcosystem* Eco = ULyudEcosystem::Get(this))
	{
		Eco->ReportCatch(Species, EndPos, F.Grams);
	}
	const bool bNew = State && State->Record(Species, F.Grams, ShowCm, UGameCalendar::NowIn(GetWorld()));
	const bool bFirst = State && State->Find(Species) && State->Find(Species)->Count == 1;
	const TArray<FStringFormatArg> Args = { FStringFormatArg(FishName(Species).ToString()), FStringFormatArg(FMath::RoundToInt(F.Grams)), FStringFormatArg(FMath::RoundToInt(ShowCm)) };
	Say(LyudText::Get(bFirst ? TEXT("ui.fish.caught_first") : (bNew ? TEXT("ui.fish.caught_record") : TEXT("ui.fish.caught")), Args), 4.f);
	Sound(TEXT("fish_flap"), EndPos, 0.8f);
	UE_LOG(LogTemp, Display, TEXT("FISHING caught %s %.0f g %.1f cm first %d record %d"), *Species.ToString(), F.Grams, ShowCm, bFirst ? 1 : 0, bNew ? 1 : 0);
}

void ULyudFishing::Fly(float DeltaTime)
{
	ALyudFishingRod* R = Rod.Get();
	if (!bFlying || !R)
	{
		return;
	}
	FlyTime += DeltaTime;
	const float S = FMath::Clamp(FlyTime / FlyDur, 0.f, 1.f);
	const float D = FVector::Dist2D(FlyFrom, FlyTo);
	FVector P = FMath::Lerp(FlyFrom, FlyTo, S);
	P.Z += D * 0.22f * 4.f * S * (1.f - S);
	// it hits whatever is in the way (a pier, rocks, the beach)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FishingCast), false, R);
	Q.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	bool bDone = false, bWater = false;
	FVector At = P;
	if (GetWorld()->LineTraceSingleByChannel(Hit, FlyPos, P, ECC_Visibility, Q))
	{
		At = Hit.ImpactPoint;
		bDone = true;
		// the shallow sea's bed under the surface is still the water
		const FVector Surf = OnWater(FVector2D(At), 0.f);
		bWater = IsWater(At) && At.Z <= Surf.Z + 5.f;
		if (bWater) At = Surf;
	}
	else if (S >= 1.f)
	{
		bDone = true;
		bWater = IsWater(P);
		At = bWater ? OnWater(FVector2D(P), 0.f) : P;
	}
	else if (IsWater(P) && P.Z <= OnWater(FVector2D(P), 0.f).Z && S > 0.5f)
	{
		bDone = true;
		bWater = true;
		At = OnWater(FVector2D(P), 0.f);
	}
	FlyPos = bDone ? At : P;
	EndPos = FlyPos;
	if (bDone)
	{
		bFlying = false;
		Session->SetLanded(bWater, FVector::Dist2D(Origin, At) * 0.01f);
		if (bWater)
		{
			// the line lies along the way it went
			Dir = FVector(FVector2D(At - Origin).GetSafeNormal(), 0.f);
			if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
		}
		LastDist = Session->Distance();
	}
}

void ULyudFishing::Prompt()
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	// built only when what it says changes (a new state, bait, words said, the tension by 5 %), not every frame
	const LyudF::EState St = Session->State();
	const bool bRunHint = St == LyudF::EState::Fighting && Session->IsRunning() && Session->Tension() > 0.55f;
	const uint64 Key = uint64(St) | (uint64(SayLeft > 0.f) << 8) | (uint64(Session->GetBait() + 1) << 9) | (uint64(Session->IsBaited()) << 17)
		| (uint64(bRunHint) << 18) | (uint64(St == LyudF::EState::Fighting && !bRunHint ? FMath::RoundToInt(Session->Tension() * 20.f) : 0) << 19)
		| (uint64(SayCount) << 32);
	if (Key == PromptKey)
	{
		return;
	}
	PromptKey = Key;
	FText Text;
	const ULyudFishingState* State = ULyudFishingState::Get(this);
	const FText BaitName = State && !State->Bait.IsNone() && Session->IsBaited() ? ULyudInventory::ItemName(State->Bait) : LyudText::Get(TEXT("ui.fish.no_bait"));
	const bool bLure = Session->GetBait() >= 0 && LyudFishing::Bait(Session->GetBait()).bLure;
	if (SayLeft > 0.f)
	{
		Text = Said;
	}
	else
	{
		switch (Session->State())
		{
		case LyudF::EState::Idle: Text = LyudText::Get(TEXT("ui.fish.hint_idle"), { FStringFormatArg(BaitName.ToString()) }); break;
		case LyudF::EState::Charging: Text = LyudText::Get(TEXT("ui.fish.hint_charge")); break;
		case LyudF::EState::Waiting:
		case LyudF::EState::Nibbling: Text = LyudText::Get(bLure ? TEXT("ui.fish.hint_lure") : TEXT("ui.fish.hint_wait")); break;
		case LyudF::EState::Taking: Text = LyudText::Get(TEXT("ui.fish.hint_take")); break;
		case LyudF::EState::Fighting:
			Text = Session->IsRunning() && Session->Tension() > 0.55f ? LyudText::Get(TEXT("ui.fish.hint_run"))
				: LyudText::Get(TEXT("ui.fish.hint_fight"), { FStringFormatArg(FMath::RoundToInt(Session->Tension() * 20.f) * 5) });
			break;
		default: break;
		}
	}
	if (!Text.EqualTo(LastPrompt))
	{
		UI->SetPrompt(this, Text);
		LastPrompt = Text;
	}
}

void ULyudFishing::Draw(float DeltaTime)
{
	ALyudFishingRod* R = Rod.Get();
	if (!R)
	{
		return;
	}
	const LyudF::EState S = Session->State();
	const FVector Tip = R->Tip();
	const bool bLure = Session->GetBait() >= 0 && LyudFishing::Bait(Session->GetBait()).bLure;
	const ALyudSea* Sea = ALyudSea::Get(this);
	// the float or fish lies along the cast's way, the fish off to its side as it swims
	const FVector2D Base(Origin);
	auto Along = [&](float Side) -> FVector2D
	{
		const FVector D = FRotator(0.f, Side * 25.f, 0.f).RotateVector(Dir);
		return Base + FVector2D(D) * Session->Distance() * 100.f;
	};
	bool bFloat = false, bSpoon = false, bFish = false;
	FRotator FishRot = FRotator::ZeroRotator;
	float Wriggle = 0.f, Lean = 0.f;
	const float Now = GetWorld()->GetTimeSeconds();
	switch (S)
	{
	case LyudF::EState::Idle:
	case LyudF::EState::Charging:
	case LyudF::EState::Swing:
		if (ShowLeft > 0.f)
		{
			// the catch hangs from the tip, head up, flapping
			EndPos = Tip - FVector(0.f, 0.f, 75.f);
			bFish = true;
			Wriggle = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Now * 3.f));
			FishRot = FRotator(88.f, LookFrame().Rotator().Yaw + 90.f + 25.f * FMath::Sin(Now * 2.1f), 0.f);
		}
		else
		{
			// the float (or the spoon) dangles a little under the tip
			EndPos = Tip - FVector(0.f, 0.f, 35.f);
			bFloat = !bLure;
			bSpoon = bLure;
			Lean = 0.f;
		}
		break;
	case LyudF::EState::Flying:
		EndPos = FlyPos;
		bFloat = !bLure;
		bSpoon = bLure;
		Lean = 0.6f;
		break;
	case LyudF::EState::Waiting:
	case LyudF::EState::Nibbling:
	case LyudF::EState::Taking:
	case LyudF::EState::Winding:
	{
		const FVector2D XY = Along(0.f);
		if (bLure)
		{
			// the spoon a little under, rising as it is wound fast
			EndPos = OnWater(XY, 60.f);
			bSpoon = S != LyudF::EState::Winding || Session->Distance() > 0.5f;
		}
		else
		{
			// riding the waves, ducking at a nibble and under on a take
			EndPos = OnWater(XY, Session->FloatDip() * 7.f - 0.4f * FMath::Sin(Now * 2.3f));
			bFloat = true;
			Lean = S == LyudF::EState::Winding ? 0.8f : 0.f;
		}
		break;
	}
	case LyudF::EState::Fighting:
	{
		const FVector2D XY = Along(Session->Side());
		EndPos = OnWater(XY, 25.f + 45.f * Session->Stamina());
		bFish = true;
		Wriggle = Session->IsRunning() ? 1.f : 0.45f;
		// heading away from the rod and toward its side
		const FVector Away = FVector(XY - FVector2D(Tip), 0.f).GetSafeNormal();
		FishRot = FRotator(0.f, Away.Rotation().Yaw + 35.f * Session->Side(), 0.f);
		break;
	}
	case LyudF::EState::Landing:
	{
		// out of the water up to the tip
		const float W = FishEase(Session->Time() / 0.9f);
		const FVector From = OnWater(Along(Session->Side()), 10.f);
		EndPos = FMath::Lerp(From, Tip - FVector(0.f, 0.f, 75.f), W);
		bFish = true;
		Wriggle = 1.f;
		FishRot = FRotator(88.f * W, LookFrame().Rotator().Yaw + 90.f, 0.f);
		break;
	}
	}
	const float WaterZ = Sea ? Sea->SurfaceZ(EndPos) : -1e6f;
	// the rod bends toward the line with its tension
	const float Bend = 0.04f + 1.2f * Session->Tension() + (bFish && S != LyudF::EState::Fighting ? 0.18f : 0.f);
	R->SetBend(Bend, EndPos - Tip);
	{
		const float Slack = (S == LyudF::EState::Waiting || S == LyudF::EState::Nibbling || S == LyudF::EState::Taking) && !bLure ? 0.35f : FMath::Clamp(1.f - Session->Tension() * 2.5f, 0.f, 1.f) * 0.5f;
		R->SetLine(EndPos + (bFloat ? FVector(0.f, 0.f, 7.5f) : FVector::ZeroVector), Slack, WaterZ);
	}
	R->SetFloat(bFloat, EndPos, Lean);
	R->SetSpoon(bSpoon, EndPos, Tip - EndPos, Now * 18.f);
	R->PlaceFish(bFish, EndPos, FishRot, Wriggle, DeltaTime);
	// the reel: wound in, or the clutch giving line
	const float DistNow = Session->Distance();
	const float Moved = DistNow - LastDist;
	LastDist = DistNow;
	R->TurnReel(-Moved / 0.6f);
	// the ratchet ticks while it is wound
	ReelClickIn -= DeltaTime;
	if (Moved < -0.005f && ReelClickIn <= 0.f)
	{
		Sound(TEXT("reel_click"), R->GetActorLocation(), 0.35f);
		ReelClickIn = 0.45f;
	}
}

void ULyudFishing::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventory* Inv = ULyudInventory::Get(World);
	if (!Hero || !PC || !Inv || !Session)
	{
		return;
	}
	// a bait taken in the hand goes on the rig (the rod comes back into the hand); a rod in the hand comes out
	const FName Equipped = Inv->EquippedItem();
	if (!Equipped.IsNone() && LyudF::FindBait(TCHAR_TO_ANSI(*Equipped.ToString())) >= 0)
	{
		if (Inv->Count(RodId) > 0)
		{
			SetBait(Equipped);
			Inv->Equip(RodId);
		}
		else
		{
			Inv->Equip(FName());
			if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World)) UI->SetPrompt(this, LyudText::Get(TEXT("ui.fish.need_rod")));
			SayLeft = 0.f;
			LastPrompt = LyudText::Get(TEXT("ui.fish.need_rod"));
		}
	}
	if (Inv->EquippedItem() == RodId && !Rod.IsValid())
	{
		TakeOut();
	}
	else if (Inv->EquippedItem() != RodId && Rod.IsValid())
	{
		PutAway();
	}
	ALyudFishingRod* R = Rod.Get();
	if (!R)
	{
		return;
	}
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	// the diary takes the hand (it holds the book itself); a reach move, a screen, a bird's eyes: the rod is hidden and waits
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bScreen = Screen && Screen->IsOpen();
	const bool bBusy = bScreen || Reach->IsBusy() || (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || Hero->IsHidden();
	if (bBusy)
	{
		if (!bHidden)
		{
			R->SetActorHiddenInGame(true);
			bHidden = true;
		}
		if (!bScreen && !Reach->IsBusy())
		{
			Reach->ClearOverrideHold();
		}
		HoldTime = 0.f;
		bWaitUp = true;
		return;
	}
	SayLeft = FMath::Max(0.f, SayLeft - DeltaTime);
	if (ShowLeft > 0.f)
	{
		ShowLeft -= DeltaTime;
		if (ShowLeft <= 0.f)
		{
			// into the bag
			const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*ShowSpecies.ToString()));
			if (C >= 0) Inv->Add(FishItem(C));
			if (ULyudFishingState* State = ULyudFishingState::Get(World)) State->BagGrams.FindOrAdd(ShowSpecies) += ShowGrams;
			Sound(TEXT("fish_bag"), Hero->GetActorLocation(), 0.6f);
			R->PlaceFish(false, FVector::ZeroVector, FRotator::ZeroRotator, 0.f, 0.f);
		}
	}

	// the buttons (a press only after the rod is up, and only once they were let go after a screen)
	bool bDown = ShowLeft <= 0.f && HoldTime > 0.35f && CVarFishing.GetValueOnGameThread() != 0 && !PC->IsMoveInputIgnored()
		&& (bForced || bClickDown || PC->IsInputKeyDown(EKeys::LeftMouseButton));
	if (bClickQueued)
	{
		bClickQueued = false;
		bClickDown = true;
	}
	else
	{
		bClickDown = false;
	}
	if (bWaitUp)
	{
		bWaitUp = bDown;
		bDown = false;
	}
	LyudF::FInput In;
	In.bButton = bDown;
	In.bReelIn = !PC->IsMoveInputIgnored() && PC->IsInputKeyDown(EKeys::RightMouseButton);
	// Q: wound in first, then away
	if (PC->WasInputKeyJustPressed(EKeys::Q) && !PC->IsMoveInputIgnored())
	{
		if (!IsLineOut() && ShowLeft <= 0.f)
		{
			Inv->Equip(FName());
			PutAway();
			return;
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::X) && !PC->IsMoveInputIgnored() && Session->State() == LyudF::EState::Idle)
	{
		CycleBait();
	}
	if (Session->State() == LyudF::EState::Idle)
	{
		SyncBait();
	}
	if (!ForcedHook.IsNone())
	{
		const int32 C = LyudF::FindCatchable(TCHAR_TO_ANSI(*ForcedHook.ToString()));
		ForcedHook = FName();
		if (C >= 0)
		{
			if (Session->State() == LyudF::EState::Idle)
			{
				Origin = Hero->GetActorLocation();
				Dir = FVector(FVector2D(LookFrame().GetRotation().GetForwardVector()).GetSafeNormal(), 0.f);
			}
			Session->DebugHook(C);
			HandleEvents();
		}
	}
	// the weights where the float lies, now and then
	WeightsIn -= DeltaTime;
	if (WeightsIn <= 0.f && (Session->State() == LyudF::EState::Waiting || Session->State() == LyudF::EState::Nibbling))
	{
		UpdateWeights();
		WeightsIn = 3.f;
	}
	Session->Step(DeltaTime, In);
	Strike = FMath::Max(0.f, Strike - DeltaTime * 5.f);
	HandleEvents();
	Fly(DeltaTime);
	// walked off too far from the line (or the fish): it is wound in
	if (IsLineOut() && !bFlying && FVector::Dist2D(Hero->GetActorLocation(), Origin) > 3000.f)
	{
		Session->Reset();
	}

	// the hand: exactly in the look's frame each frame; the rod rides on the hand bone once it is up
	Reach->SetOverrideHold(PoseNow(DeltaTime));
	HoldTime += DeltaTime;
	if (!bAttached && Reach->HoldWeight() > 0.6f)
	{
		FTransform Palm;
		if (FishBonePalm(Hero->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), Palm))
		{
			R->SetActorTransform(RodInPalm() * Palm);
			R->AttachToComponent(Hero->GetMesh(), FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
			bAttached = true;
		}
	}
	if (bAttached && bHidden)
	{
		R->SetActorHiddenInGame(false);
		bHidden = false;
	}
	if (bAttached)
	{
		Draw(DeltaTime);
	}
	Prompt();
}

// ------------------------------------------------------------------ console

static FAutoConsoleCommandWithWorld GFishGive(TEXT("lyud.Fish.Give"), TEXT("lyud.Fish.Give: a rod, worms, maggots and a spoon in the pocket, the rod in the hand"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			if (Inv->Count(ULyudFishing::RodId) < 1) Inv->Add(ULyudFishing::RodId);
			Inv->Add(TEXT("bait_worm"), 10);
			Inv->Add(TEXT("bait_maggot"), 10);
			Inv->Add(TEXT("lure_spoon"), 1);
			Inv->Equip(ULyudFishing::RodId);
		}
	}));

static FAutoConsoleCommandWithWorld GFishState(TEXT("lyud.Fish.State"), TEXT("lyud.Fish.State: the rod's state in the log (FISHING state ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudFishing* F = ULyudFishing::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("FISHING state %s"), *F->DebugState());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishPress(TEXT("lyud.Fish.Press"), TEXT("lyud.Fish.Press 1|0: holds the left mouse button down for the rod, or lets it go"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->Press(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}
	}));

static FAutoConsoleCommandWithWorld GFishClick(TEXT("lyud.Fish.Click"), TEXT("lyud.Fish.Click: one click of the left mouse button for the rod (a strike)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->Click();
		}
	}));

static FAutoConsoleCommandWithWorld GFishBite(TEXT("lyud.Fish.Bite"), TEXT("lyud.Fish.Bite: the likeliest fish here takes the float now"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->ForceBite();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishHook(TEXT("lyud.Fish.Hook"), TEXT("lyud.Fish.Hook <Species>: that fish on the line now (Cod, Perch, Salmon ...)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->ForceHook(FName(Args.Num() ? *Args[0] : TEXT("Perch")));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFishBait(TEXT("lyud.Fish.Bait"), TEXT("lyud.Fish.Bait <item>: fish with that bait (bait_worm, bait_maggot, lure_spoon)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFishing* F = ULyudFishing::Get(World))
		{
			F->SetBait(FName(Args.Num() ? *Args[0] : TEXT("bait_worm")));
		}
	}));

static FAutoConsoleCommandWithWorld GFishLog(TEXT("lyud.Fish.Log"), TEXT("lyud.Fish.Log: the angler's log (FISHING log ...)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudFishingState* S = ULyudFishingState::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("FISHING log total %d"), S->Total);
			for (const FLyudCatchRecord& R : S->Log)
			{
				UE_LOG(LogTemp, Display, TEXT("FISHING log %s x%d biggest %.0f g %.1f cm first %s"), *R.Species.ToString(), R.Count, R.BiggestGrams, R.BiggestCm, *R.First.ToString());
			}
		}
	}));
