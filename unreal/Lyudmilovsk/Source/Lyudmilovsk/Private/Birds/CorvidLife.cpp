#include "Birds/CorvidLife.h"

#include "BirdCorvid.h"
#include "BirdSim.h"
#include "Birds/BirdActor.h"
#include "Birds/BirdLife.h"
#include "Birds/BirdSpecies.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

using namespace LyudBirds;

static TAutoConsoleVariable<int32> CVarCorvidItems(TEXT("lyud.Corvids.Items"), 1,
	TEXT("Draw the corvids' items (acorns, nuts, shells, finds, sticks) on the ground and in their bills (0 hides them, for A/B)."));
static TAutoConsoleVariable<float> CVarCorvidItemsDistance(TEXT("lyud.Corvids.ItemDistance"), 60.f,
	TEXT("How far (m) the corvids' small items are drawn (a dead fish twice as far)."));

namespace
{
	FVector ToUE(const V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	V3 FromUE(const FVector& V) { return V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }

	/** A placeholder's size (cm) per item kind, until /Game/Birds/Items is imported. */
	const FVector PlaceholderSize[NumItems] = {
		{ 2.5f, 1.6f, 1.6f }, { 1.8f, 1.6f, 1.6f }, { 3.5f, 3.2f, 3.2f }, { 5.f, 3.f, 3.f }, { 5.f, 2.5f, 2.f }, { 6.f, 4.f, 1.5f },
		{ 20.f, 4.f, 3.f }, { 12.f, 12.f, 0.6f }, { 2.6f, 2.6f, 0.6f }, { 4.f, 3.f, 1.5f }, { 2.f, 2.f, 0.4f }, { 2.f, 2.f, 0.4f },
		{ 2.f, 2.f, 0.3f }, { 5.f, 2.f, 0.4f }, { 2.5f, 2.f, 1.f }, { 30.f, 1.f, 1.f },
	};

	/** The species' name in the player's language (bird.<key>.name). */
	FText SpeciesName(const FWorld& W, int32 Bird)
	{
		const FName Id(SpeciesTable()[W.Birds[Bird].Species].Id.c_str());
		const FBirdSpecies* R = UBirdSpeciesLibrary::Find(Id);
		return R ? LyudText::Get(FString::Printf(TEXT("bird.%s.name"), *R->Key)) : FText::FromName(Id);
	}
}

// --- the subsystem ------------------------------------------------------------------------------------

UCorvidLifeSubsystem* UCorvidLifeSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCorvidLifeSubsystem>() : nullptr;
}

bool UCorvidLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

TStatId UCorvidLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCorvidLifeSubsystem, STATGROUP_Tickables);
}

void UCorvidLifeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (ULyudSaveSubsystem* Save = ULyudSaveSubsystem::Get(&InWorld))
	{
		Save->RegisterSaveObject(TEXT("Corvids"), this);
		Save->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncFeelings(false); });
		Save->OnBeforeSave.AddWeakLambda(this, [this](ULyudSaveGame*) { SyncFeelings(true); });
	}
}

void UCorvidLifeSubsystem::Deinitialize()
{
	if (ULyudInventoryUI* UI = GetWorld() ? ULyudInventoryUI::Get(GetWorld()) : nullptr)
	{
		UI->SetPrompt(this, FText());
	}
	Super::Deinitialize();
}

FWorld* UCorvidLifeSubsystem::Sim() const
{
	const UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(this);
	return Birds && Birds->IsRunning() ? Birds->GetSim() : nullptr;
}

FCorvids* UCorvidLifeSubsystem::Corvids() const
{
	const FWorld* W = Sim();
	return W ? W->Corvids() : nullptr;
}

void UCorvidLifeSubsystem::Tick(float DeltaTime)
{
	FWorld* W = Sim();
	FCorvids* C = W ? W->Corvids() : nullptr;
	const UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(this);
	if (!C || !Birds)
	{
		if (Renderer)
		{
			Renderer->Destroy();
			Renderer = nullptr;
		}
		SimStart = -1;
		return;
	}
	if (SimStart != Birds->GetStartCount())
	{
		// the bird life (re)started: a fresh world, the saved feelings go back on
		SimStart = Birds->GetStartCount();
		W->Step(0.f);          // (the minds are made on a step)
		SyncFeelings(false);
		if (!Renderer)
		{
			FActorSpawnParameters P;
			P.ObjectFlags |= RF_Transient;
			Renderer = GetWorld()->SpawnActor<ACorvidItemsRenderer>(P);
			Renderer->Setup();
		}
	}
	Dispatch();
	UpdatePlayer();
	if (Renderer)
	{
		if (CVarCorvidItems.GetValueOnGameThread() != 0)
		{
			Renderer->Draw(*W, ToUE(W->Camera));
		}
		else
		{
			Renderer->SetActorHiddenInGame(true);
		}
	}
	SaveTimer -= DeltaTime;
	if (SaveTimer <= 0.f)
	{
		SaveTimer = 2.f;
		SyncFeelings(true);
	}
}

void UCorvidLifeSubsystem::SyncFeelings(bool bToSave)
{
	FCorvids* C = Corvids();
	if (!C)
	{
		return;
	}
	if (bToSave)
	{
		SavedBond.Reset();
		SavedGrudge.Reset();
		for (int32 Id = 0; Id < (int32)C->Minds.size(); Id++)
		{
			const FCorvid& M = C->Minds[Id];
			if (M.Kind >= 0 && M.Bond > 0.01f)
			{
				SavedBond.Add(Id, M.Bond);
			}
			if (M.Kind >= 0 && M.Grudge > 0.01f)
			{
				SavedGrudge.Add(Id, M.Grudge);
			}
		}
		return;
	}
	for (const TPair<int32, float>& It : SavedBond)
	{
		if (FCorvid* M = C->Mind(It.Key))
		{
			M->Bond = It.Value;
		}
	}
	for (const TPair<int32, float>& It : SavedGrudge)
	{
		if (FCorvid* M = C->Mind(It.Key))
		{
			M->Grudge = It.Value;
		}
	}
}

/** The corvids' events: the feed a magpie snatched leaves the pocket, presents and scolding get a word on the HUD. */
void UCorvidLifeSubsystem::Dispatch()
{
	FWorld& W = *Sim();
	FCorvids& C = *W.Corvids();
	const std::vector<FCorvidEvent> Events = C.Events;       // (handlers may call back in)
	C.Events.clear();
	const double Now = GetWorld()->GetTimeSeconds();
	for (const FCorvidEvent& E : Events)
	{
		if ((E.Pos - W.Camera).Len() > 120.f)
		{
			continue;
		}
		const FName What(CorvidEventName(E.Kind));
		OnCorvid.Broadcast(E.Bird, What, E.Item);
		if (E.Bird < 0)
		{
			continue;
		}
		switch (E.Kind)
		{
		case ECorvidEvent::Snatch:
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				const FName Feed = Inv->EquippedItem();
				const FLyudItemDef* Def = ULyudInventory::FindDef(Feed);
				if (Def && Def->Kind == ELyudItemKind::Feed)
				{
					Inv->Remove(Feed, 1);
				}
			}
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(TEXT("corvid.snatch"), { SpeciesName(W, E.Bird).ToString() }));
			}
			ULyudQuests::Notify(this, TEXT("corvid_snatch"), FString(SpeciesTable()[W.Birds[E.Bird].Species].Id.c_str()));
			break;
		case ECorvidEvent::Gift:
			if (Now - LastGiftToast > 20.0)
			{
				LastGiftToast = Now;
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					Screen->Toast(LyudText::Get(TEXT("corvid.gift"), { SpeciesName(W, E.Bird).ToString() }));
				}
			}
			ULyudQuests::Notify(this, TEXT("corvid_gift"), FString(SpeciesTable()[W.Birds[E.Bird].Species].Id.c_str()));
			break;
		case ECorvidEvent::Scold:
			if (Now - LastScoldToast > 300.0)
			{
				LastScoldToast = Now;
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					Screen->Toast(LyudText::Get(TEXT("corvid.scold"), { SpeciesName(W, E.Bird).ToString() }));
				}
			}
			break;
		default:
			break;
		}
	}
}

/** E picks up the find in front of the hero (a present, an acorn, a shell, a store they watched being dug in); G throws feed. */
void UCorvidLifeSubsystem::UpdatePlayer()
{
	FWorld& W = *Sim();
	FCorvids& C = *W.Corvids();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* Hero = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	int32 Best = -1;
	if (Hero && PC && UI && !UI->IsOpen() && !W.Player.bClaimedBird)
	{
		Best = Pickable();
		if (Best >= 0 && PC->WasInputKeyJustPressed(EKeys::E))
		{
			Take(Best);
			Best = -1;
		}
		if (PC->WasInputKeyJustPressed(EKeys::G))
		{
			ThrowFeed(true);
		}
	}
	if (UI && Best != Prompted)
	{
		Prompted = Best;
		if (Best < 0)
		{
			UI->SetPrompt(this, FText());
		}
		else
		{
			const FItem& It = C.Items[Best];
			const FText Name = ULyudInventory::ItemName(FindItemId((int32)It.Kind));
			UI->SetPrompt(this, LyudText::Get(It.State == EItemState::Cached ? TEXT("corvid.prompt.dig") : TEXT("corvid.prompt.take"), { Name.ToString() }));
		}
	}
}

/** The find in front of the hero within reach (looked at), -1 if none. */
int32 UCorvidLifeSubsystem::Pickable() const
{
	const FWorld* W = Sim();
	const FCorvids* C = W ? W->Corvids() : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ACharacter* Hero = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!C || !PC || !Hero)
	{
		return -1;
	}
	const FVector Loc = Hero->GetActorLocation();
	const float Half = Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const V3 Feet = FromUE(Loc - FVector(0.0, 0.0, Half));
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	std::vector<int> Near;
	C->PickableNear(*W, Feet, 1.8f, Near);
	int32 Best = -1;
	float BestDot = 0.8f;
	for (int I : Near)
	{
		if (FindItemId((int32)C->Items[I].Kind).IsNone())
		{
			continue;
		}
		const float Dot = (float)FVector::DotProduct(View.Vector(), (ToUE(C->Items[I].Pos) - Eye).GetSafeNormal());
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = I;
		}
	}
	return Best;
}

FName UCorvidLifeSubsystem::TakeNearest()
{
	const int32 I = Pickable();
	if (I < 0)
	{
		return FName();
	}
	const FName Id = FindItemId((int32)Corvids()->Items[I].Kind);
	Take(I);
	return Id;
}

TArray<int32> UCorvidLifeSubsystem::GetCarriers(FVector Location, float Radius) const
{
	TArray<int32> Out;
	const FWorld* W = Sim();
	const FCorvids* C = W ? W->Corvids() : nullptr;
	if (!C)
	{
		return Out;
	}
	for (int32 Id = 0; Id < (int32)C->Minds.size(); Id++)
	{
		if (C->Minds[Id].Kind >= 0 && C->Minds[Id].Held >= 0 && FVector::Dist(ToUE(W->Birds[Id].Pos), Location) < Radius)
		{
			Out.Add(Id);
		}
	}
	Out.Sort([W, &Location](int32 A, int32 B) { return FVector::DistSquared(ToUE(W->Birds[A].Pos), Location) < FVector::DistSquared(ToUE(W->Birds[B].Pos), Location); });
	return Out;
}

FString UCorvidLifeSubsystem::DescribeCorvid(int32 BirdId) const
{
	const FWorld* W = Sim();
	const FCorvids* C = W ? W->Corvids() : nullptr;
	const FCorvid* M = C ? C->Mind(BirdId) : nullptr;
	if (!M)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s step %d holds %s bond %.2f grudge %.2f"), UTF8_TO_TCHAR(TaskName(M->Task)), M->Step,
		M->Held >= 0 ? UTF8_TO_TCHAR(ItemName(C->Items[M->Held].Kind)) : TEXT("-"), M->Bond, M->Grudge);
}

void UCorvidLifeSubsystem::Take(int32 Item)
{
	FWorld& W = *Sim();
	FCorvids& C = *W.Corvids();
	const FItem It = C.Items[Item];
	const FName Id = FindItemId((int32)It.Kind);
	const bool bGift = (It.Flags & ItemGift) != 0;
	if (Id.IsNone() || !C.PlayerTake(W, Item))
	{
		return;
	}
	ULyudQuests::Notify(this, bGift ? TEXT("corvid_gift_taken") : (It.State == EItemState::Cached ? TEXT("corvid_store_dug") : TEXT("corvid_find")), Id.ToString());
	// the hand reaches down for it and puts it in the pocket (a stand-in actor of its mesh); without a hand, straight in
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UHeroReachComponent* Reach = Hero ? UHeroReachComponent::Get(Hero) : nullptr;
	UStaticMesh* Mesh = Renderer ? Renderer->MeshOf((int32)It.Kind) : nullptr;
	if (Reach && !Reach->IsBusy() && Mesh)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(ToUE(It.Pos), FRotator(0.f, FMath::RadiansToDegrees(It.Yaw), 0.f), P);
		if (A)
		{
			A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->GetStaticMeshComponent()->LightingChannels.bChannel1 = true;
			FReachItem R;
			R.Actor = A;
			R.Center = A->GetActorLocation();
			R.Radius = 2.f;
			TWeakObjectPtr<UCorvidLifeSubsystem> Self(this);
			if (Reach->Take(R, [Self, Id]()
				{
					if (Self.IsValid())
					{
						if (ULyudInventory* Inv = ULyudInventory::Get(Self.Get()))
						{
							Inv->Add(Id, 1);
						}
					}
				}))
			{
				return;
			}
			A->Destroy();
		}
	}
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(Id, 1);
	}
}

bool UCorvidLifeSubsystem::ThrowFeed(bool bFromInventory)
{
	FWorld* W = Sim();
	FCorvids* C = W ? W->Corvids() : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!C || !PC)
	{
		return false;
	}
	if (bFromInventory)
	{
		ULyudInventory* Inv = ULyudInventory::Get(this);
		const FName Feed = Inv ? Inv->EquippedItem() : FName();
		const FLyudItemDef* Def = ULyudInventory::FindDef(Feed);
		if (!Def || Def->Kind != ELyudItemKind::Feed || !Inv->Remove(Feed, 1))
		{
			return false;
		}
	}
	// a few metres ahead on the ground
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const FVector Ahead = Eye + FRotator(0.f, View.Yaw, 0.f).Vector() * 350.0;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CorvidThrow), false, PC->GetPawn());
	FVector At = Ahead - FVector(0.0, 0.0, 170.0);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Ahead + FVector(0.0, 0.0, 200.0), Ahead - FVector(0.0, 0.0, 1000.0), ECC_Visibility, Q))
	{
		At = Hit.ImpactPoint;
	}
	C->PlayerDrop(*W, EItem::FeedPile, FromUE(At));
	if (UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(this))
	{
		Birds->AddPlayerThreat(0.25f);       // the throw itself makes them look up
	}
	ULyudQuests::Notify(this, TEXT("corvid_feed_thrown"));
	return true;
}

int32 UCorvidLifeSubsystem::SpawnItem(FName Kind, FVector Location)
{
	FWorld* W = Sim();
	FCorvids* C = W ? W->Corvids() : nullptr;
	if (!C)
	{
		return -1;
	}
	for (int32 K = 0; K < NumItems; K++)
	{
		if (Kind == FName(ItemName((EItem)K)))
		{
			return (EItem)K == EItem::Fish ? C->WashUp(*W, FromUE(Location)) : C->Spawn(*W, (EItem)K, FromUE(Location));
		}
	}
	return -1;
}

FName UCorvidLifeSubsystem::FindItemId(int32 Kind)
{
	switch ((EItem)Kind)
	{
	case EItem::Acorn: return TEXT("find_acorn");
	case EItem::Hazelnut: return TEXT("find_hazelnut");
	case EItem::Walnut: return TEXT("find_walnut");
	case EItem::PineCone: return TEXT("find_pinecone");
	case EItem::Mussel: return TEXT("find_mussel");
	case EItem::BottleCap: return TEXT("find_bottlecap");
	case EItem::Foil: return TEXT("find_foil");
	case EItem::Ring: return TEXT("find_ring");
	case EItem::Button: return TEXT("find_button");
	case EItem::Coin: return TEXT("find_coin");
	case EItem::Key: return TEXT("find_key");
	case EItem::SeaGlass: return TEXT("find_seaglass");
	default: return FName();
	}
}

void UCorvidLifeSubsystem::SetFeelings(FVector Location, float Radius, float Bond, float Grudge)
{
	FWorld* W = Sim();
	FCorvids* C = W ? W->Corvids() : nullptr;
	if (!C)
	{
		return;
	}
	for (int32 Id = 0; Id < (int32)C->Minds.size(); Id++)
	{
		if (C->Minds[Id].Kind >= 0 && (ToUE(W->Birds[Id].Pos) - Location).Size() < Radius)
		{
			C->Minds[Id].Bond = Bond;
			C->Minds[Id].Grudge = Grudge;
			C->Minds[Id].Cooldown = 0.f;
		}
	}
}

FString UCorvidLifeSubsystem::Report() const
{
	const FWorld* W = Sim();
	const FCorvids* C = W ? W->Corvids() : nullptr;
	if (!C)
	{
		return TEXT("corvids: off");
	}
	return FString(UTF8_TO_TCHAR(C->Report(*W).c_str())) + FString::Printf(TEXT("; items drawn %d"), Renderer ? Renderer->DrawnLastFrame : 0);
}

// --- console --------------------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GCorvidReportCmd(TEXT("lyud.Corvids.Report"), TEXT("What the corvids hold, hide and do."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UCorvidLifeSubsystem* S = UCorvidLifeSubsystem::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("CORVIDS %s"), *S->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCorvidSpawnCmd(TEXT("lyud.Corvids.Spawn"),
	TEXT("lyud.Corvids.Spawn <Acorn|Walnut|Mussel|Crust|Fish|BottleCap|Foil|Ring|Coin|Key|SeaGlass|Stick|...> [count]: on the ground in front of the player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UCorvidLifeSubsystem* S = UCorvidLifeSubsystem::Get(World);
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		if (!S || !PC || Args.Num() < 1)
		{
			return;
		}
		FVector Eye;
		FRotator View;
		PC->GetPlayerViewPoint(Eye, View);
		const int32 N = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		for (int32 I = 0; I < FMath::Max(1, N); I++)
		{
			const FVector Ahead = Eye + FRotator(0.f, View.Yaw, 0.f).Vector() * (250.0 + I * 40.0) + FRotator(0.f, View.Yaw + 90.f, 0.f).Vector() * ((I % 3) - 1) * 40.0;
			FHitResult Hit;
			FVector At = Ahead - FVector(0.0, 0.0, 170.0);
			if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0.0, 0.0, 200.0), Ahead - FVector(0.0, 0.0, 1000.0), ECC_Visibility,
				FCollisionQueryParams(SCENE_QUERY_STAT(CorvidSpawn), false, PC->GetPawn())))
			{
				At = Hit.ImpactPoint;
			}
			S->SpawnItem(FName(*Args[0]), At);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCorvidFeelCmd(TEXT("lyud.Corvids.Feel"),
	TEXT("lyud.Corvids.Feel <bond 0..1> <grudge 0..1> [radius m, 80]: how the corvids near the player feel about them (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UCorvidLifeSubsystem* S = UCorvidLifeSubsystem::Get(World);
		const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		if (S && Pawn && Args.Num() >= 2)
		{
			S->SetFeelings(Pawn->GetActorLocation(), (Args.Num() > 2 ? FCString::Atof(*Args[2]) : 80.f) * 100.f, FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCorvidTakeCmd(TEXT("lyud.Corvids.Take"), TEXT("Picks up the find in front of the player, as E does (tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UCorvidLifeSubsystem* S = UCorvidLifeSubsystem::Get(World))
		{
			const FName Id = S->TakeNearest();
			const ULyudInventory* Inv = ULyudInventory::Get(World);
			UE_LOG(LogTemp, Display, TEXT("CORVIDS take: %s (now %d in the pocket)"), Id.IsNone() ? TEXT("nothing in reach") : *Id.ToString(), Inv && !Id.IsNone() ? Inv->Count(Id) : 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCorvidThrowCmd(TEXT("lyud.Corvids.Throw"), TEXT("Throws a handful of feed in front of the player (no feed needed; tests)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (UCorvidLifeSubsystem* S = UCorvidLifeSubsystem::Get(World))
		{
			S->ThrowFeed(false);
		}
	}));

// --- the items ------------------------------------------------------------------------------------------

ACorvidItemsRenderer::ACorvidItemsRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMesh* ACorvidItemsRenderer::MeshOf(int32 Kind) const
{
	return Kinds.IsValidIndex(Kind) && Kinds[Kind] ? Kinds[Kind]->GetStaticMesh() : nullptr;
}

void ACorvidItemsRenderer::Setup()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Kinds.SetNum(NumItems);
	Xf.SetNum(NumItems);
	Last.SetNum(NumItems);
	PlaceholderScale.SetNum(NumItems);
	for (int32 K = 0; K < NumItems; K++)
	{
		const FString Name = FString::Printf(TEXT("SM_CorvidItem_%s"), UTF8_TO_TCHAR(ItemName((EItem)K)));
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Birds/Items/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		PlaceholderScale[K] = Mesh ? FVector::OneVector : PlaceholderSize[K] / 100.0;
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ISM_%s"), *Name));
		ISM->SetupAttachment(RootComponent);
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->bDisableCollision = true;
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(K == (int32)EItem::Fish || K == (int32)EItem::Stick || K == (int32)EItem::PineCone || K == (int32)EItem::Walnut);
		ISM->LightingChannels.bChannel1 = true;       // the outdoor sun
		// small, and some move every frame (in a bill, falling): out of Lumen (docs/benchmark.md)
		ISM->bAffectDynamicIndirectLighting = false;
		ISM->bAffectDistanceFieldLighting = false;
		ISM->SetStaticMesh(Mesh ? Mesh : Sphere);
		ISM->RegisterComponent();
		Kinds[K] = ISM;
	}
}

void ACorvidItemsRenderer::Draw(const FWorld& World, const FVector& Camera)
{
	SetActorHiddenInGame(false);
	const FCorvids* C = World.Corvids();
	if (!C)
	{
		return;
	}
	const float Far = FMath::Max(5.f, CVarCorvidItemsDistance.GetValueOnGameThread()) * 100.f;
	for (TArray<FTransform>& A : Xf)
	{
		A.Reset();
	}
	const UBirdLifeSubsystem* Birds = UBirdLifeSubsystem::Get(this);
	for (const FItem& It : C->Items)
	{
		if (It.State != EItemState::Ground && It.State != EItemState::Carried && It.State != EItemState::Falling)
		{
			continue;
		}
		FVector P = ToUE(It.Pos);
		if (FVector::DistSquared(P, Camera) > FMath::Square(It.Kind == EItem::Fish ? Far * 2.f : Far))
		{
			continue;
		}
		FRotator R(0.f, FMath::RadiansToDegrees(It.Yaw), 0.f);
		FVector Scale = PlaceholderScale[(int32)It.Kind];
		if (It.State == EItemState::Carried && It.Holder >= 0 && Birds)
		{
			// in the bill of a near body: just past the jaw bone, along the bill
			FBirdLifeInfo Info;
			if (Birds->GetBird(It.Holder, Info))
			{
				if (const ABirdActor* Body = Cast<ABirdActor>(Info.Actor))
				{
					const FVector Jaw = Body->GetBoneWorld(TEXT("jaw"));
					const FVector Head = Body->GetBoneWorld(TEXT("head"));
					if (!Jaw.IsZero() && !Head.IsZero())
					{
						const FVector Bill = (Jaw - Head).GetSafeNormal();
						const float Len = SpeciesTable()[World.Birds[It.Holder].Species].Length * 100.f;
						P = Jaw + Bill * (Len * 0.07f);
						R = Bill.Rotation() + FRotator(0.f, 90.f, 0.f);    // held across the bill
					}
				}
			}
		}
		else if (It.State == EItemState::Falling)
		{
			R.Pitch = (float)FMath::Fmod(World.Time * 400.0 + It.Id * 37, 360.0);    // tumbling down
		}
		if (It.Kind == EItem::FeedPile || It.Kind == EItem::Fish)
		{
			Scale *= FMath::Clamp(FMath::Sqrt(FMath::Max(It.Food, 0.f)) * (It.Kind == EItem::Fish ? 1.f : 1.1f), 0.25f, 1.f);   // eaten down
		}
		if (It.Kind == EItem::FeedPile && It.State == EItemState::Carried)
		{
			Scale *= 0.35f;          // a beakful
		}
		Xf[(int32)It.Kind].Add(FTransform(R, P, Scale));
	}
	static const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	int32 Drawn = 0;
	for (int32 K = 0; K < NumItems; K++)
	{
		TArray<FTransform>& Want = Xf[K];
		TArray<FTransform>& Had = Last[K];
		Drawn += Want.Num();
		// nothing moved, came or went: leave the component alone (no proxy update, docs/benchmark.md)
		bool bSame = Want.Num() == Had.Num();
		for (int32 I = 0; bSame && I < Want.Num(); I++)
		{
			bSame = Want[I].Equals(Had[I], 0.01);
		}
		if (bSame)
		{
			continue;
		}
		Had = Want;
		UInstancedStaticMeshComponent* ISM = Kinds[K];
		const int32 Have = ISM->GetInstanceCount();
		if (Want.Num() > Have)
		{
			TArray<FTransform> Extra;
			Extra.Init(Hidden, Want.Num() - Have + 8);
			ISM->AddInstances(Extra, false);
		}
		TArray<FTransform> All = Want;
		All.SetNum(ISM->GetInstanceCount());
		for (int32 I = Want.Num(); I < All.Num(); I++)
		{
			All[I] = Hidden;
		}
		ISM->BatchUpdateInstancesTransforms(0, All, true, false, true);
	}
	DrawnLastFrame = Drawn;
}
