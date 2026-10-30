#include "Birds/BirdFeeder.h"

#include "BirdSim.h"
#include "Birds/BirdLife.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/LyudGameScreen.h"

// ------------------------------------------------------------------ the feeds' table

namespace LyudBirdFeeds
{
	namespace
	{
		struct FFeed
		{
			float Default = 0.f;
			TMap<FName, float> Like;
		};

		const TMap<FName, FFeed>& Table()
		{
			static TMap<FName, FFeed> Feeds;
			static bool bRead = false;
			if (bRead)
			{
				return Feeds;
			}
			bRead = true;
			FString Text;
			const FString Path = FPaths::ProjectContentDir() / TEXT("BirdsData/feeds.json");
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
			{
				UE_LOG(LogTemp, Warning, TEXT("Feeds: can't read %s"), *Path);
				return Feeds;
			}
			const TSharedPtr<FJsonObject>* List = nullptr;
			if (!Root->TryGetObjectField(TEXT("feeds"), List))
			{
				return Feeds;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*List)->Values)
			{
				const TSharedPtr<FJsonObject> O = It.Value->AsObject();
				if (!O)
				{
					continue;
				}
				FFeed F;
				O->TryGetNumberField(TEXT("default"), F.Default);
				const TSharedPtr<FJsonObject>* Like = nullptr;
				if (O->TryGetObjectField(TEXT("like"), Like))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*Like)->Values)
					{
						F.Like.Add(FName(*L.Key), (float)L.Value->AsNumber());
					}
				}
				Feeds.Add(FName(*It.Key), MoveTemp(F));
			}
			return Feeds;
		}
	}

	bool IsFeed(FName Id)
	{
		return Table().Contains(Id);
	}

	float Like(FName Feed, FName Species, float Unknown)
	{
		const FFeed* F = Table().Find(Feed);
		if (!F)
		{
			return Unknown;
		}
		const float* L = F->Like.Find(Species);
		return FMath::Clamp(L ? *L : F->Default, 0.f, 1.f);
	}

	void LikeTable(FName Feed, float Out[64], float Unknown)
	{
		static FName Cached;
		static float Values[64];
		static bool bCached = false;
		const std::vector<LyudBirds::FSpecies>& Species = LyudBirds::SpeciesTable();
		if (!bCached || Cached != Feed || Unknown != Values[63])
		{
			for (int32 I = 0; I < 63; ++I)
			{
				Values[I] = I < (int32)Species.size() ? Like(Feed, FName(UTF8_TO_TCHAR(Species[I].Id.c_str())), Unknown) : Unknown;
			}
			Values[63] = Unknown;           // (the last slot remembers what an unknown feed was given)
			Cached = Feed;
			bCached = true;
		}
		FMemory::Memcpy(Out, Values, sizeof(Values));
		if ((int32)Species.size() > 63)
		{
			Out[63] = Like(Feed, FName(UTF8_TO_TCHAR(Species[63].Id.c_str())), Unknown);
		}
	}
}

namespace
{
	TAutoConsoleVariable<float> CVarFeederFindHours(TEXT("lyud.Feeder.FindHours"), 1.5f,
		TEXT("Game hours after a feed goes into a feeder before the first birds round about find it (all of them have an hour later)."));

	UMaterialInstanceDynamic* Paint(UPrimitiveComponent* C, UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base || !C)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		for (int32 I = 0; I < C->GetNumMaterials(); ++I)
		{
			C->SetMaterial(I, MID);
		}
		return MID;
	}

	/** The colour of a feed's grains on the tray (sunflower seeds dark, rowan red, mealworms brown...). */
	FLinearColor GrainColor(FName Feed, int32 I)
	{
		const FString F = Feed.ToString();
		if (F == TEXT("feed_rowan"))
		{
			return I % 4 ? FLinearColor(0.55f, 0.08f, 0.03f) : FLinearColor(0.75f, 0.35f, 0.05f);
		}
		if (F == TEXT("feed_mealworm"))
		{
			return FLinearColor(0.42f, 0.27f, 0.12f);
		}
		if (F == TEXT("feed_niger"))
		{
			return FLinearColor(0.03f, 0.03f, 0.03f);
		}
		if (F == TEXT("feed_grain"))
		{
			return I % 3 ? FLinearColor(0.62f, 0.45f, 0.18f) : FLinearColor(0.7f, 0.65f, 0.22f);
		}
		if (F == TEXT("feed_peanuts"))
		{
			return FLinearColor(0.78f, 0.6f, 0.36f);
		}
		if (F == TEXT("feed_suet"))
		{
			return FLinearColor(0.86f, 0.8f, 0.62f);
		}
		return I % 3 ? FLinearColor(0.03f, 0.028f, 0.026f) : FLinearColor(0.72f, 0.55f, 0.25f);
	}

	/** Grain size (cm): a suet cake and peanuts are big lumps, niger tiny. */
	FVector GrainSize(FName Feed)
	{
		const FString F = Feed.ToString();
		if (F == TEXT("feed_suet"))
		{
			return FVector(3.f, 3.f, 1.6f);
		}
		if (F == TEXT("feed_peanuts"))
		{
			return FVector(1.4f, 0.9f, 0.9f);
		}
		if (F == TEXT("feed_rowan"))
		{
			return FVector(0.8f);
		}
		if (F == TEXT("feed_mealworm"))
		{
			return FVector(2.2f, 0.4f, 0.4f);
		}
		if (F == TEXT("feed_niger"))
		{
			return FVector(0.4f, 0.2f, 0.15f);
		}
		return FVector(1.f, 0.5f, 0.3f);
	}
}

// ------------------------------------------------------------------ the feeder

ABirdFeeder::ABirdFeeder()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Part = [this, Root, Cube](const FName Name, const FVector& Loc, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator, bool bSolid = false)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetStaticMesh(Cube);
		C->SetRelativeLocationAndRotation(Loc, Rot);
		C->SetRelativeScale3D(Size / 100.f);
		if (bSolid)
		{
			// the hero walks round it; the birds' ground probes (visibility traces) and the camera see through it
			C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			C->SetCollisionResponseToAllChannels(ECR_Ignore);
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		return C;
	};
	const float T = TrayTop, H = TrayHalf;
	// a 7 cm post sunk a little into the ground, two braces under the tray, the tray (a 2 cm board) with a 2.5 cm rim
	Post = Part(TEXT("Post"), FVector(0.f, 0.f, (T - 2.f) / 2.f - 5.f), FVector(7.f, 7.f, T - 2.f + 10.f), FRotator::ZeroRotator, true);
	Tray = Part(TEXT("Tray"), FVector(0.f, 0.f, T - 1.f), FVector(2.f * H, 2.f * H, 2.f), FRotator::ZeroRotator, true);
	int32 N = 0;
	auto W = [&](const FVector& Loc, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator)
	{
		Wood.Add(Part(FName(*FString::Printf(TEXT("Wood%d"), N++)), Loc, Size, Rot));
	};
	W(FVector(0.f, 0.f, T - 12.f), FVector(4.f, 30.f, 3.f), FRotator(0.f, 0.f, 0.f));
	W(FVector(0.f, 9.f, T - 8.f), FVector(3.f, 3.f, 16.f), FRotator(0.f, 0.f, -40.f));
	W(FVector(0.f, -9.f, T - 8.f), FVector(3.f, 3.f, 16.f), FRotator(0.f, 0.f, 40.f));
	for (int32 S = -1; S <= 1; S += 2)
	{
		W(FVector(S * (H - 0.75f), 0.f, T + 1.25f), FVector(1.5f, 2.f * H, 2.5f));
		W(FVector(0.f, S * (H - 0.75f), T + 1.25f), FVector(2.f * H - 3.f, 1.5f, 2.5f));
	}
	// corner posts and the roof: two planks meeting over the middle, a ridge board
	constexpr float PostH = 30.f;
	for (int32 X = -1; X <= 1; X += 2)
	{
		for (int32 Y = -1; Y <= 1; Y += 2)
		{
			W(FVector(X * (H - 2.f), Y * (H - 2.f), T + PostH / 2.f), FVector(3.f, 3.f, PostH));
		}
	}
	constexpr float Pitch = 32.f;
	const float Half = (H + 6.f) / FMath::Cos(FMath::DegreesToRadians(Pitch));
	const float Rise = (H + 6.f) * FMath::Tan(FMath::DegreesToRadians(Pitch));
	for (int32 S = -1; S <= 1; S += 2)
	{
		Roof.Add(Part(FName(*FString::Printf(TEXT("Roof%d"), S > 0 ? 1 : 0)), FVector(0.f, S * (H + 6.f) / 2.f, T + PostH + Rise / 2.f + 1.f),
			FVector(2.f * H + 10.f, Half, 1.6f), FRotator(0.f, 0.f, S * Pitch)));      // (roll +: +Y side down)
	}
	Roof.Add(Part(TEXT("Ridge"), FVector(0.f, 0.f, T + PostH + Rise + 1.8f), FVector(2.f * H + 12.f, 3.f, 2.f)));
	Heap = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Heap"));
	Heap->SetupAttachment(Root);
	Heap->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Heap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Heap->SetRelativeLocation(FVector(0.f, 0.f, T));
}

void ABirdFeeder::BeginPlay()
{
	Super::BeginPlay();
	// weathered pine, the roof a shade darker
	Paint(Post, this, FLinearColor(0.36f, 0.27f, 0.17f));
	Paint(Tray, this, FLinearColor(0.5f, 0.38f, 0.24f));
	for (UStaticMeshComponent* C : Wood)
	{
		Paint(C, this, FLinearColor(0.46f, 0.35f, 0.22f));
	}
	for (UStaticMeshComponent* C : Roof)
	{
		Paint(C, this, FLinearColor(0.3f, 0.22f, 0.14f));
	}
}

void ABirdFeeder::ShowFeed(FName Feed, float Handfuls)
{
	// a heap that grows with the handfuls: 40 grains a handful (bigger lumps fewer), the newest on top
	const FVector Size = GrainSize(Feed);
	const float PerHandful = FMath::Clamp(40.f / FMath::Max(Size.X * Size.Y, 0.3f), 2.f, 60.f);
	const int32 Grains = FMath::CeilToInt(FMath::Max(Handfuls, 0.f) * PerHandful);
	if (Feed == Shown && Grains == ShownGrains)
	{
		return;
	}
	Shown = Feed;
	ShownGrains = Grains;
	Heap->ClearInstances();
	if (Grains <= 0)
	{
		return;
	}
	Paint(Heap, this, GrainColor(Feed, 1));
	FRandomStream Rand(2026);
	const float Radius = FMath::Min(TrayHalf - 4.f, 6.f + 2.2f * FMath::Sqrt((float)Grains));
	for (int32 I = 0; I < Grains; ++I)
	{
		const float R = Radius * FMath::Sqrt(Rand.FRand());
		const float A = Rand.FRand() * 2.f * PI;
		const float Top = 0.04f * Radius * (1.f - FMath::Square(R / Radius)) * FMath::Min(Handfuls, 3.f);
		const FVector P(R * FMath::Cos(A), R * FMath::Sin(A), Top * FMath::Sqrt(Rand.FRand()) + Size.Z * 0.5f);
		const FRotator Rot(Rand.FRandRange(-20.f, 20.f), Rand.FRand() * 360.f, Rand.FRandRange(-20.f, 20.f));
		Heap->AddInstance(FTransform(Rot, P, Size * 0.01f * Rand.FRandRange(0.85f, 1.15f)));
	}
}

// ------------------------------------------------------------------ the feeders

const FName UBirdFeederSubsystem::FeederItem(TEXT("feeder"));

UBirdFeederSubsystem* UBirdFeederSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UBirdFeederSubsystem>() : nullptr;
}

void UBirdFeederSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Feeders"), this);
	}
}

void UBirdFeederSubsystem::Deinitialize()
{
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(GetWorld()))
	{
		Saves->UnregisterSaveObject(TEXT("Feeders"));
	}
	Super::Deinitialize();
}

void UBirdFeederSubsystem::OnGameSaving_Implementation()
{
	// (States is kept up to date as the birds eat: nothing to copy)
}

void UBirdFeederSubsystem::OnGameLoaded_Implementation()
{
	SpawnAll();
}

void UBirdFeederSubsystem::SpawnAll()
{
	for (ABirdFeeder* A : Actors)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	Actors.Reset();
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	P.ObjectFlags |= RF_Transient;
	for (const FBirdFeederState& S : States)
	{
		ABirdFeeder* A = World ? World->SpawnActor<ABirdFeeder>(S.Location, FRotator(0.f, S.Yaw, 0.f), P) : nullptr;
		if (A)
		{
			A->ShowFeed(S.Feed, S.Handfuls);
		}
		Actors.Add(A);
	}
	// put them up in the bird life again: take the old trays down first, or they stay there with the old feed
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && Life->IsRunning() && SimStart == Life->GetStartCount())
	{
		for (int32 Id : SimIds)
		{
			Life->RemoveFeeder(Id);
		}
	}
	SimIds.Reset();
	SimStart = -1;
}

void UBirdFeederSubsystem::Register()
{
	UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	if (!Life || !Life->IsRunning())
	{
		SimStart = -1;
		return;
	}
	if (SimStart != Life->GetStartCount() || SimIds.Num() != States.Num())
	{
		if (SimStart == Life->GetStartCount())
		{
			for (int32 Id : SimIds)
			{
				Life->RemoveFeeder(Id);
			}
		}
		SimIds.Reset();
		for (int32 I = 0; I < States.Num(); ++I)
		{
			SimIds.Add(Actors.IsValidIndex(I) && Actors[I] ? Life->AddFeeder(Actors[I]->TrayLocation(), ABirdFeeder::TrayHalf - 5.f) : -1);
		}
		SimStart = Life->GetStartCount();
	}
}

float UBirdFeederSubsystem::Ready(const FBirdFeederState& S) const
{
	if (S.Feed.IsNone() || S.Handfuls <= 0.f)
	{
		return 0.f;
	}
	const double Hours = (UGameCalendar::NowIn(GetWorld()) - S.FilledAt).GetTotalHours();
	return FMath::Clamp((float)(Hours - CVarFeederFindHours.GetValueOnGameThread()), 0.f, 1.f);
}

void UBirdFeederSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (Actors.Num() != States.Num())
	{
		SpawnAll();
	}
	Register();
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && SimIds.Num() == States.Num())
	{
		for (int32 I = 0; I < States.Num(); ++I)
		{
			FBirdFeederState& S = States[I];
			S.Handfuls = FMath::Max(0.f, S.Handfuls - Life->TakeFeederEaten(SimIds[I]));
			Life->SetFeeder(SimIds[I], S.Feed, S.Handfuls, Ready(S));
			if (Actors[I])
			{
				Actors[I]->ShowFeed(S.Feed, S.Handfuls);
			}
		}
	}
	UpdatePrompt(World->GetFirstPlayerController());
}

int32 UBirdFeederSubsystem::FeederInReach() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!Hero || !PC || Hero->IsHidden())
	{
		return -1;
	}
	const FVector Eye = Hero->EyeLocation();
	const FVector Look = PC->GetControlRotation().Vector();
	int32 Best = -1;
	float BestD = 230.f;
	for (int32 I = 0; I < Actors.Num(); ++I)
	{
		if (!Actors[I])
		{
			continue;
		}
		const FVector Tray = Actors[I]->TrayLocation();
		const FVector Foot = Actors[I]->GetActorLocation();
		const float D = FVector::Dist2D(Eye, Foot);
		// at arm's length and looking at it (anywhere from its foot to its roof)
		const FVector Mid = (Tray + Foot) * 0.5f;
		if (D < BestD && FVector::DotProduct(Look, (Mid - Eye).GetSafeNormal()) > 0.55f)
		{
			BestD = D;
			Best = I;
		}
	}
	return Best;
}

void UBirdFeederSubsystem::UpdatePrompt(APlayerController* PC)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudGameMenuSubsystem* Menu = GetWorld()->GetSubsystem<ULyudGameMenuSubsystem>();
	const int32 At = PC && UI && Inv && !UI->IsOpen() && !(Menu && Menu->IsOpen()) && !PC->IsMoveInputIgnored() ? FeederInReach() : -1;
	if (At < 0)
	{
		if (bPrompt && UI)
		{
			UI->SetPrompt(this, FText::GetEmpty());
		}
		bPrompt = false;
		return;
	}
	const FBirdFeederState& S = States[At];
	const FName Held = Inv->EquippedItem();
	const FLyudItemDef* Def = ULyudInventory::FindDef(Held);
	const bool bFeed = Def && Def->Kind == ELyudItemKind::Feed;
	const bool bHas = !S.Feed.IsNone() && S.Handfuls > 0.01f;
	const FStringFormatArg In(bHas ? ULyudInventory::ItemName(S.Feed).ToString() : FString());
	const FStringFormatArg Left(FMath::CeilToInt(S.Handfuls));
	FText Text;
	if (bFeed)
	{
		const FStringFormatArg What(ULyudInventory::ItemName(Held).ToString());
		Text = S.Handfuls >= Capacity - 0.01f && S.Feed == Held ? LyudText::Get(TEXT("ui.feeder.full"), { In, Left })
			: bHas ? LyudText::Get(TEXT("ui.feeder.fill"), { In, Left, What }) : LyudText::Get(TEXT("ui.feeder.fill_empty"), { What });
	}
	else
	{
		Text = bHas ? LyudText::Get(TEXT("ui.feeder.take"), { In, Left }) : LyudText::Get(TEXT("ui.feeder.take_empty"));
	}
	UI->SetPrompt(this, Text);
	bPrompt = true;
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		if (bFeed)
		{
			Fill(At, Held, 1);
		}
		else
		{
			TakeBack(At);
		}
	}
}

bool UBirdFeederSubsystem::PlaceFromPocket()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
	auto Note = [Screen](const TCHAR* Key) { if (Screen) { Screen->Toast(LyudText::Get(Key)); } };
	if (!Inv || !Hero || !PC || Inv->Count(FeederItem) < 1)
	{
		return false;
	}
	// on the ground a metre and a bit ahead, level enough, not in the water, not against another feeder or a wall
	const float Yaw = PC->GetControlRotation().Yaw;
	const FVector Ahead = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetSimpleCollisionHalfHeight());
	const FVector Spot = Feet + Ahead * 120.f;
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FeederPlace), false, Hero);
	FHitResult Hit;
	const bool bGround = World->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 150.f), Spot - FVector(0.f, 0.f, 200.f), ECC_Visibility, Q);
	FHitResult Wall;
	const bool bBlocked = World->SweepSingleByChannel(Wall, Hero->GetActorLocation(), Spot + FVector(0.f, 0.f, 80.f), FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(30.f), Q);
	bool bNear = false;
	for (const FBirdFeederState& S : States)
	{
		bNear |= FVector::Dist2D(S.Location, Hit.ImpactPoint) < 150.f;
	}
	const bool bWater = Hit.GetActor() && Hit.GetActor()->GetName().Contains(TEXT("Sea"));
	if (!bGround || bBlocked || bNear || bWater || Hit.ImpactNormal.Z < 0.8f)
	{
		Note(TEXT("ui.feeder.cant_place"));
		return false;
	}
	PlaceAt(Hit.ImpactPoint, Yaw + 180.f);
	Inv->Remove(FeederItem);
	Note(TEXT("ui.feeder.placed"));
	return true;
}

int32 UBirdFeederSubsystem::PlaceAt(const FVector& Location, float Yaw)
{
	FBirdFeederState S;
	S.Location = Location;
	S.Yaw = Yaw;
	S.FilledAt = UGameCalendar::NowIn(GetWorld());
	States.Add(S);
	SpawnAll();
	UE_LOG(LogTemp, Display, TEXT("Feeder: %d put up at %s"), States.Num() - 1, *Location.ToString());
	return States.Num() - 1;
}

int32 UBirdFeederSubsystem::Fill(int32 Index, FName Feed, int32 Count)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !States.IsValidIndex(Index))
	{
		return 0;
	}
	FBirdFeederState& S = States[Index];
	if (S.Feed != Feed || S.Handfuls <= 0.01f)
	{
		// another feed (or none left): the old one is thrown out, and the birds must find the new one
		S.Feed = Feed;
		S.Handfuls = 0.f;
		S.FilledAt = UGameCalendar::NowIn(GetWorld());
	}
	const int32 N = FMath::Min3(Count, Inv->Count(Feed), FMath::FloorToInt(Capacity - S.Handfuls + 0.01f));
	if (N <= 0)
	{
		return 0;
	}
	Inv->Remove(Feed, N);
	S.Handfuls += N;
	if (Actors.IsValidIndex(Index) && Actors[Index])
	{
		Actors[Index]->ShowFeed(S.Feed, S.Handfuls);
	}
	UE_LOG(LogTemp, Display, TEXT("Feeder: %d gets %d of %s (%.1f)"), Index, N, *Feed.ToString(), S.Handfuls);
	return N;
}

void UBirdFeederSubsystem::TakeBack(int32 Index)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || !States.IsValidIndex(Index))
	{
		return;
	}
	const FBirdFeederState S = States[Index];
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && SimIds.IsValidIndex(Index) && SimStart == Life->GetStartCount())
	{
		Life->RemoveFeeder(SimIds[Index]);
		SimIds.RemoveAt(Index);
	}
	if (Actors.IsValidIndex(Index))
	{
		if (Actors[Index])
		{
			Actors[Index]->Destroy();
		}
		Actors.RemoveAt(Index);
	}
	States.RemoveAt(Index);
	Inv->Add(FeederItem);
	if (!S.Feed.IsNone() && S.Handfuls >= 1.f)
	{
		Inv->Add(S.Feed, FMath::FloorToInt(S.Handfuls));
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	bPrompt = false;
}

TArray<int32> UBirdFeederSubsystem::BirdsOn(int32 Index) const
{
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	return Life && SimIds.IsValidIndex(Index) ? Life->BirdsOnFeeder(SimIds[Index]) : TArray<int32>();
}

void UBirdFeederSubsystem::Age(int32 Index, float Hours)
{
	if (States.IsValidIndex(Index))
	{
		States[Index].FilledAt = UGameCalendar::NowIn(GetWorld()) - FTimespan::FromHours(Hours);
	}
}

// ------------------------------------------------------------------ console

static FAutoConsoleCommandWithWorldAndArgs GFeederPlaceCmd(TEXT("lyud.Feeder.Place"),
	TEXT("Sets a feeder up in front of the hero (gives one if the pocket has none)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		ULyudInventory* Inv = ULyudInventory::Get(World);
		UBirdFeederSubsystem* F = UBirdFeederSubsystem::Get(World);
		if (Inv && F)
		{
			if (Inv->Count(UBirdFeederSubsystem::FeederItem) < 1)
			{
				Inv->Add(UBirdFeederSubsystem::FeederItem);
			}
			UE_LOG(LogTemp, Display, TEXT("FEEDER place %s"), F->PlaceFromPocket() ? TEXT("ok") : TEXT("refused"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFeederFillCmd(TEXT("lyud.Feeder.Fill"),
	TEXT("lyud.Feeder.Fill <feed> [handfuls] [feeder]: pours feed (given to the hero if missing) into a feeder (default the newest)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudInventory* Inv = ULyudInventory::Get(World);
		UBirdFeederSubsystem* F = UBirdFeederSubsystem::Get(World);
		if (!Inv || !F || F->Num() == 0)
		{
			return;
		}
		const FName Feed(Args.Num() > 0 ? *Args[0] : TEXT("bird_feed"));
		const int32 N = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 5;
		const int32 Index = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : F->Num() - 1;
		if (Inv->Count(Feed) < N)
		{
			Inv->Add(Feed, N - Inv->Count(Feed));
		}
		UE_LOG(LogTemp, Display, TEXT("FEEDER fill %d %s: %d"), Index, *Feed.ToString(), F->Fill(Index, Feed, N));
	}));

static FAutoConsoleCommandWithWorldAndArgs GFeederAgeCmd(TEXT("lyud.Feeder.Age"),
	TEXT("lyud.Feeder.Age <hours> [feeder]: as if the feed went into the feeder that many game hours ago."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBirdFeederSubsystem* F = UBirdFeederSubsystem::Get(World); F && F->Num() > 0)
		{
			F->Age(Args.Num() > 1 ? FCString::Atoi(*Args[1]) : F->Num() - 1, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 3.f);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFeederReportCmd(TEXT("lyud.Feeder.Report"),
	TEXT("Logs every feeder: its feed, handfuls, and the birds on it (FEEDER lines)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		const UBirdFeederSubsystem* F = UBirdFeederSubsystem::Get(World);
		const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(World);
		if (!F)
		{
			return;
		}
		for (int32 I = 0; I < F->Num(); ++I)
		{
			const FBirdFeederState& S = F->State(I);
			FString Birds;
			for (int32 Id : F->BirdsOn(I))
			{
				FBirdLifeInfo Info;
				if (Life && Life->GetBird(Id, Info))
				{
					Birds += FString::Printf(TEXT(" %s(%d, trust %.2f)"), *Info.SpeciesId.ToString(), Id, Info.Trust);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("FEEDER %d at %s: %s x%.2f, filled %.2f h ago; on it:%s"), I, *S.Location.ToString(), *S.Feed.ToString(),
				S.Handfuls, (UGameCalendar::NowIn(World) - S.FilledAt).GetTotalHours(), *Birds);
		}
	}));

static TWeakObjectPtr<ACameraActor> GFeederCamera;
static FAutoConsoleCommandWithWorldAndArgs GFeederCamCmd(TEXT("lyud.Feeder.Cam"),
	TEXT("lyud.Feeder.Cam [feeder] [cm away] [cm up] [deg round] | off: views a feeder's tray from a camera beside it (headless checks)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UBirdFeederSubsystem* F = UBirdFeederSubsystem::Get(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!F || !PC)
		{
			return;
		}
		if (Args.Num() > 0 && Args[0] == TEXT("off"))
		{
			PC->SetViewTarget(PC->GetPawn());
			return;
		}
		const ABirdFeeder* A = F->Actor(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : F->Num() - 1);
		if (!A)
		{
			return;
		}
		const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 130.f;
		const float Up = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 35.f;
		const float Round = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 40.f;
		const FVector Tray = A->TrayLocation() + FVector(0.f, 0.f, 4.f);
		const FVector At = Tray + FRotator(0.f, A->GetActorRotation().Yaw + Round, 0.f).Vector() * Dist + FVector(0.f, 0.f, Up);
		if (!GFeederCamera.IsValid())
		{
			FActorSpawnParameters P;
			P.ObjectFlags |= RF_Transient;
			GFeederCamera = World->SpawnActor<ACameraActor>(At, (Tray - At).Rotation(), P);
			if (GFeederCamera.IsValid())
			{
				GFeederCamera->GetCameraComponent()->SetFieldOfView(50.f);
				GFeederCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
			}
		}
		if (GFeederCamera.IsValid())
		{
			GFeederCamera->SetActorLocationAndRotation(At, (Tray - At).Rotation());
			PC->SetViewTarget(GFeederCamera.Get());
		}
	}));
