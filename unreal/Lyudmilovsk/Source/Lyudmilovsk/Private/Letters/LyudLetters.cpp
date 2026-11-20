#include "Letters/LyudLetters.h"

#include "Letters/LettersModel.h"
#include "People/MiniJson.h"
#include "Audio/LyudSoundscape.h"
#include "Birds/BirdBook.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/LyudDoor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "Fishing/LyudFishing.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudGoodsItem.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/LyudFriends.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_LETTERS_DEV 1
#else
#define LYUD_LETTERS_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudLetters, Log, All);

namespace LyudLettersImpl
{
	/** How far the hero reaches a box (cm, from the eyes) and how near the middle of the view it must be (cos of the angle). */
	constexpr double Reach = 240.0;
	constexpr double ConeCos = 0.968;   // 14.5 degrees
	/** Boxes are placed when the hero comes this near (cm): the ground and the buildings round them are there by then. */
	constexpr double PlaceRadius = 12000.0;
	/** A shelf card is set up when the hero is this near its home (cm). */
	constexpr double ShelfRadius = 2500.0;
	/** A thank-you in the street: the townsperson within this of the hero (cm). */
	constexpr double ThanksRadius = 700.0;
	/** Entries shown at once in a chooser (1..9 pick among them). */
	constexpr int32 PageSize = 9;
	/** A letter's page holds about this many characters (a Japanese one counts double). */
	constexpr int32 PageChars = 520;

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string Std(const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		return std::string(Conv.Get(), (size_t)Conv.Length());
	}

	UMaterialInterface* BasicShapeMaterial()
	{
		static UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return Basic;
	}

	/** The name key of a letter's sender: a key already, or a person's key (npc.<key>.name). */
	FString FromKey(const std::string& From)
	{
		const FString F = Str(From);
		return F.Contains(TEXT(".")) ? F : FString::Printf(TEXT("npc.%s.name"), *F);
	}

	int32 TextCost(const FString& S)
	{
		int32 N = 0;
		for (const TCHAR C : S)
		{
			N += C >= 0x3000 ? 2 : 1;
		}
		return N;
	}
}

// --- the box -----------------------------------------------------------------------------------------------------------

ALyudPostbox::ALyudPostbox()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* ALyudPostbox::Part(UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const FRotator& Rot)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->bAffectDistanceFieldLighting = false;
	C->SetCullDistance(9000.f);
	if (UMaterialInstanceDynamic* MID = ALyudGoodsItem::SharedMaterial(LyudLettersImpl::BasicShapeMaterial(), Color))
	{
		C->SetMaterial(0, MID);
	}
	C->RegisterComponent();
	return C;
}

void ALyudPostbox::Build(bool bMailbox, bool bOnWall)
{
	bIsMailbox = bMailbox;
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// the engine's cube and cylinder are 100 cm, centred
	const FLinearColor Slot(0.02f, 0.02f, 0.025f);
	const FLinearColor Plate(0.85f, 0.83f, 0.76f);
	if (!bMailbox)
	{
		// the town's street box: a blue cabinet on a short dark foot, a rounded hood, the slot under the hood, a cream plate
		const FLinearColor Blue(0.05f, 0.16f, 0.42f);
		const FLinearColor Foot(0.06f, 0.06f, 0.07f);
		Part(Cube, FVector(0, 0, 18), FVector(0.30f, 0.30f, 0.36f), Foot);
		Part(Cube, FVector(0, 0, 80), FVector(0.40f, 0.46f, 0.90f), Blue);
		Part(Cyl, FVector(0, 0, 125), FVector(0.40f, 0.46f, 0.16f), Blue, FRotator(0, 0, 90));
		Part(Cube, FVector(20.5f, 0, 110), FVector(0.02f, 0.26f, 0.025f), Slot);
		Part(Cube, FVector(20.5f, 0, 82), FVector(0.01f, 0.24f, 0.16f), Plate);
		Part(Cube, FVector(20.6f, 0, 60), FVector(0.01f, 0.06f, 0.06f), Foot);
		return;
	}
	// the club's mailbox: a small dark green box with a lid over the slot and a cream name card
	const FLinearColor Green(0.06f, 0.18f, 0.10f);
	const FLinearColor Metal(0.32f, 0.32f, 0.30f);
	const double Back = bOnWall ? 0.0 : -12.0;
	const double Z = bOnWall ? 0.0 : 125.0;
	if (!bOnWall)
	{
		Part(Cube, FVector(Back, 0, 55), FVector(0.06f, 0.06f, 1.10f), Metal);
	}
	Part(Cube, FVector(Back + 12, 0, Z), FVector(0.22f, 0.36f, 0.44f), Green);
	Part(Cube, FVector(Back + 24, 0, Z + 12), FVector(0.03f, 0.30f, 0.05f), Metal);
	Part(Cube, FVector(Back + 23.3f, 0, Z + 7), FVector(0.01f, 0.24f, 0.012f), Slot);
	Part(Cube, FVector(Back + 23.3f, 0, Z - 8), FVector(0.01f, 0.14f, 0.07f), Plate);
}

FVector ALyudPostbox::UsePoint() const
{
	// the slot: the street box's at 1.1 m over its foot, the mailbox's on its front
	const FTransform T = GetActorTransform();
	if (!bIsMailbox)
	{
		return T.TransformPosition(FVector(20.0, 0.0, 105.0));
	}
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	return Parts.Num() > 0 ? Parts.Last()->GetComponentLocation() : T.GetLocation();
}

// --- the subsystem -------------------------------------------------------------------------------------------------------

ULyudLetters::ULyudLetters() = default;
ULyudLetters::~ULyudLetters() = default;

ULyudLetters* ULyudLetters::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudLetters>() : nullptr;
}

void ULyudLetters::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudLetters::FModel>();
	Load();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Letters"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudLetters::StartOver);
	}
}

void ULyudLetters::Deinitialize()
{
	EndMode();
	SetPrompt(nullptr);
	Super::Deinitialize();
}

void ULyudLetters::Load()
{
	FString Text;
	const FString File = FPaths::ProjectContentDir() / TEXT("Letters/letters.json");
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		UE_LOG(LogLyudLetters, Warning, TEXT("Letters: %s not found: no post"), *File);
		return;
	}
	const std::string Json = LyudLettersImpl::Std(Text);
	std::string Error;
	if (!Model->Data.Load(Json, Error))
	{
		UE_LOG(LogLyudLetters, Warning, TEXT("Letters: %s"), *LyudLettersImpl::Str(Error));
		return;
	}
	// what only the engine side uses: the boxes and the street's thank-you lines
	LyudJson::FValue Root;
	if (LyudJson::Parse(Json, Root, Error))
	{
		for (const LyudJson::FValue& B : Root["postboxes"].Items)
		{
			FBoxSlot S;
			S.Id = LyudLettersImpl::Str(B["id"].S());
			S.Anchor = FVector(B["ue"][(size_t)0].F(), B["ue"][(size_t)1].F(), B["ue"][(size_t)2].F());
			S.Yaw = B["yaw"].F();
			Boxes.Add(S);
		}
		const LyudJson::FValue& M = Root["mailbox"];
		FBoxSlot S;
		S.Id = TEXT("mailbox");
		S.bMailbox = true;
		S.Anchor = ULyudQuests::Frame().ToWorld(FVector2D(M["map"][(size_t)0].F(), M["map"][(size_t)1].F()), 0.0);
		S.Yaw = M["yaw"].F(90.f);
		Boxes.Add(S);
		for (const LyudJson::FValue& L : Root["thanks_lines"].Items)
		{
			ThanksLines.Add(LyudLettersImpl::Str(L.S()));
		}
	}
	ShelfCards.SetNum(Model->Data.Correspondents.size());
	bLoaded = true;
	UE_LOG(LogLyudLetters, Display, TEXT("Letters: %d boxes, %d correspondents, %d letters of the pen friend"), Boxes.Num(),
		(int32)Model->Data.Correspondents.size(), (int32)Model->Data.Pen.size());
	if (bSavedPending)
	{
		bSavedPending = false;
		ApplySaved();
	}
}

void ULyudLetters::StartOver()
{
	EndMode();
	if (Model)
	{
		Model->Reset();
	}
	Saved.Reset();
	for (TObjectPtr<ALyudGoodsItem>& Card : ShelfCards)
	{
		if (Card)
		{
			Card->Destroy();
		}
		Card = nullptr;
	}
}

void ULyudLetters::OnGameSaving_Implementation()
{
	if (!bLoaded || !Model)
	{
		return;
	}
	std::map<std::string, std::string> Out;
	Model->Save(Out);
	Saved.Reset();
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(LyudLettersImpl::Str(KV.first), LyudLettersImpl::Str(KV.second));
	}
}

void ULyudLetters::OnGameLoaded_Implementation()
{
	EndMode();
	if (!bLoaded)
	{
		bSavedPending = true;
		return;
	}
	ApplySaved();
}

void ULyudLetters::ApplySaved()
{
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[LyudLettersImpl::Std(KV.Key)] = LyudLettersImpl::Std(KV.Value);
	}
	Model->LoadState(In);
	// the shelves are set up again from the counts of this save
	for (TObjectPtr<ALyudGoodsItem>& Card : ShelfCards)
	{
		if (Card)
		{
			Card->Destroy();
		}
		Card = nullptr;
	}
}

LyudLetters::FTime ULyudLetters::Now() const
{
	const FDateTime N = UGameCalendar::NowIn(GetWorld());
	return LyudLetters::FTime{ LyudLetters::DaysFromCivil(N.GetYear(), N.GetMonth(), N.GetDay()), N.GetHour() * 60 + N.GetMinute() };
}

void ULyudLetters::FillFacts(LyudLetters::FFacts& Out) const
{
	if (const ULyudBirdBook* Book = ULyudBirdBook::Get(this))
	{
		for (const FLyudBirdSeen& S : Book->Seen)
		{
			Out.Birds.push_back(LyudLettersImpl::Std(FString::Printf(TEXT("bird.%s.name"), *ULyudBirdBook::Snake(S.Species.ToString()))));
		}
		TSet<FName> Species;
		for (const FLyudBirdFriend& F : Book->Friends)
		{
			if (!Species.Contains(F.Species))
			{
				Species.Add(F.Species);
				Out.Friends.push_back(LyudLettersImpl::Std(FString::Printf(TEXT("bird.%s.name"), *ULyudBirdBook::Snake(F.Species.ToString()))));
			}
		}
	}
	if (const ULyudFishingState* Fishing = ULyudFishingState::Get(this))
	{
		for (const FLyudCatchRecord& R : Fishing->Log)
		{
			if (R.Count > 0)
			{
				Out.Fish.push_back(LyudLettersImpl::Std(FString::Printf(TEXT("fish.%s.name"), *R.Species.ToString().ToLower())));
			}
		}
	}
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		for (const FName& Place : Quests->Found)
		{
			Out.Places.push_back(LyudLettersImpl::Std(FString::Printf(TEXT("place.%s.name"), *Place.ToString())));
		}
		for (const FLyudQuestProgress& P : Quests->Progress)
		{
			if (P.State == ELyudQuestState::Done)
			{
				Out.QuestsDone.insert(LyudLettersImpl::Std(P.Quest.ToString()));
			}
		}
	}
	if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(this))
	{
		for (const FString& U : Fest->Upcoming(Model->Data.Post.FestivalNotice + 1))
		{
			// "YYYY-MM-DD id"
			FString Date, Id;
			if (U.Split(TEXT(" "), &Date, &Id))
			{
				TArray<FString> Parts;
				Date.ParseIntoArray(Parts, TEXT("-"));
				if (Parts.Num() == 3)
				{
					Out.Festivals.push_back({ LyudLetters::DaysFromCivil(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2])),
						LyudLettersImpl::Std(Id) });
				}
			}
		}
	}
}

void ULyudLetters::StepPost()
{
	LyudLetters::FFacts Facts;
	FillFacts(Facts);
	const LyudLetters::FEvents E = Model->Step(Now(), Facts);
	for (const std::string& To : E.Delivered)
	{
		UE_LOG(LogLyudLetters, Display, TEXT("Letters: a card was carried to %s"), *LyudLettersImpl::Str(To));
		// the friendship grows (docs/friends.md: a card of his on their shelf)
		if (ULyudFriends* Friends = ULyudFriends::Get(this))
		{
			Friends->OnCardDelivered(LyudLettersImpl::Str(To));
		}
	}
	if (!E.Arrived.empty())
	{
		for (const int32 I : E.Arrived)
		{
			UE_LOG(LogLyudLetters, Display, TEXT("Letters: in the mailbox: %s"), *LyudLettersImpl::Str(Model->Letters[(size_t)I].Id));
		}
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			const int32 N = Model->NumUnread();
			Screen->Toast(N > 1 ? LyudText::Get(TEXT("letters.mail.new_n"), { FStringFormatArg(N) }) : LyudText::Get(TEXT("letters.mail.new")));
		}
	}
}

// --- ticking ---------------------------------------------------------------------------------------------------------------

void ULyudLetters::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !bLoaded || !Model)
	{
		return;
	}
	StepClock -= DeltaTime;
	if (StepClock <= 0.f)
	{
		StepClock = 1.f;
		StepPost();
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (Hero)
	{
		PlaceClock -= DeltaTime;
		if (PlaceClock <= 0.f)
		{
			PlaceClock = 2.f;
			PlaceNear(Hero->GetActorLocation());
			PlaceShelfCards(Hero->GetActorLocation());
		}
		PeopleClock -= DeltaTime;
		if (PeopleClock <= 0.f)
		{
			PeopleClock = 1.f;
			TickPeople(Hero);
		}
	}
	if (Mode != EMode::None)
	{
		SetPrompt(nullptr);
		TickMode(Hero, PC);
		return;
	}
	if (World->GetTimeSeconds() < RefusedUntil)
	{
		return;
	}
	PickClock -= DeltaTime;
	if (PickClock <= 0.f)
	{
		PickClock = 0.12f;
		const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
		const ULyudCityThings* City = ULyudCityThings::Get(World);
		const bool bCalm = Hero && PC && UI && UI->CanOpen(true) && !Hero->IsOnBike() && !Hero->IsLeadingBike() && !Hero->IsSeated()
			&& !(City && City->HasModeUp());
		SetPrompt(bCalm ? PickInView(Hero, PC) : nullptr);
	}
	ALyudPostbox* Box = OfferedWeak.Get();
	if (Box && PC && ANpcTalkDirector::UsePressed(PC))
	{
		Use(Box);
	}
}

void ULyudLetters::PlaceNear(const FVector& Hero)
{
	for (FBoxSlot& S : Boxes)
	{
		if (S.bTried || FVector::DistSquared2D(S.Anchor, Hero) > FMath::Square(LyudLettersImpl::PlaceRadius))
		{
			continue;
		}
		S.bTried = true;
		const bool bOk = S.bMailbox ? PlaceMailbox(S) : PlaceStreetBox(S);
		UE_LOG(LogLyudLetters, Display, TEXT("Letters: box %s %s"), *S.Id, bOk ? TEXT("placed") : TEXT("found no spot"));
	}
}

bool ULyudLetters::PlaceStreetBox(FBoxSlot& Slot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// offsets in the anchor's frame (forward along its yaw, to its left), the first that stands on believable ground with
	// room for the box and nothing between it and the anchor
	static const FVector2D Offsets[] = { { 110, 170 }, { 110, -170 }, { 170, 120 }, { 170, -120 }, { 210, 0 }, { 110, 260 }, { 110, -260 },
		{ 260, 160 }, { 260, -160 }, { -60, 200 }, { -60, -200 } };
	const FRotator Frame(0.f, Slot.Yaw, 0.f);
	const FVector Fwd = Frame.Vector();
	const FVector Left = FRotator(0.f, Slot.Yaw - 90.f, 0.f).Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LettersPlace), false);
	for (const FVector2D& O : Offsets)
	{
		const FVector Top = Slot.Anchor + Fwd * O.X + Left * O.Y + FVector(0, 0, 180);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 500), ECC_Visibility, Q) || Hit.ImpactNormal.Z < 0.85f)
		{
			continue;
		}
		const FVector Foot = Hit.ImpactPoint;
		if (Foot.Z < Slot.Anchor.Z - 260.0 || Foot.Z > Slot.Anchor.Z + 60.0)
		{
			continue;
		}
		const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(26, 30, 62));
		if (World->OverlapAnyTestByChannel(Foot + FVector(0, 0, 72), FRotator(0.f, Slot.Yaw, 0.f).Quaternion(), ECC_Pawn, Shape, Q))
		{
			continue;
		}
		FHitResult Wall;
		if (World->LineTraceSingleByChannel(Wall, Foot + FVector(0, 0, 100), Slot.Anchor + FVector(0, 0, 20), ECC_Visibility, Q)
			&& Wall.Distance < FVector::Dist(Foot + FVector(0, 0, 100), Slot.Anchor) - 80.f)
		{
			continue;     // round a corner or inside the kiosk
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALyudPostbox* Box = World->SpawnActor<ALyudPostbox>(ALyudPostbox::StaticClass(), Foot, FRotator(0.f, Slot.Yaw, 0.f), P);
		if (!Box)
		{
			return false;
		}
		Box->Build(false, false);
		Slot.Actor = Box;
		return true;
	}
	return false;
}

bool ULyudLetters::PlaceMailbox(FBoxSlot& Slot)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LettersMailbox), false);
	const FVector Out = FRotator(0.f, Slot.Yaw, 0.f).Vector();
	// the forecourt's floor in front of the portal face, then the face itself
	FHitResult Floor;
	const FVector Over = Slot.Anchor + Out * 80.0 + FVector(0, 0, 400);
	if (!World->LineTraceSingleByChannel(Floor, Over, Over - FVector(0, 0, 1200), ECC_Visibility, Q))
	{
		return false;
	}
	const double FloorZ = Floor.ImpactPoint.Z;
	FHitResult Wall;
	const FVector From = FVector(Slot.Anchor.X, Slot.Anchor.Y, FloorZ + 135.0) + Out * 160.0;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudPostbox* Box = nullptr;
	if (World->LineTraceSingleByChannel(Wall, From, From - Out * 400.0, ECC_Visibility, Q) && FMath::Abs(Wall.ImpactNormal.Z) < 0.3f)
	{
		const FVector N = FVector(Wall.ImpactNormal.X, Wall.ImpactNormal.Y, 0.0).GetSafeNormal();
		Box = World->SpawnActor<ALyudPostbox>(ALyudPostbox::StaticClass(), Wall.ImpactPoint + N * 0.5, N.Rotation(), P);
		if (Box)
		{
			Box->Build(true, true);
		}
	}
	else
	{
		Box = World->SpawnActor<ALyudPostbox>(ALyudPostbox::StaticClass(), FVector(Slot.Anchor.X, Slot.Anchor.Y, FloorZ) + Out * 60.0,
			FRotator(0.f, Slot.Yaw, 0.f), P);
		if (Box)
		{
			Box->Build(true, false);
		}
	}
	Slot.Actor = Box;
	return Box != nullptr;
}

ALyudPostbox* ULyudLetters::PickInView(const AHeroCharacter* Hero, const APlayerController* PC) const
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !PC)
	{
		return nullptr;
	}
	if (const ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(World); Doors && Doors->DoorInView())
	{
		return nullptr;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const FVector Dir = View.Vector();
	ALyudPostbox* Best = nullptr;
	double BestCos = LyudLettersImpl::ConeCos;
	for (const FBoxSlot& S : Boxes)
	{
		ALyudPostbox* Box = S.Actor.Get();
		if (!Box)
		{
			continue;
		}
		const FVector At = Box->UsePoint();
		const FVector To = At - Eye;
		const double Dist = To.Size();
		if (Dist > LyudLettersImpl::Reach || Dist < 1.0)
		{
			continue;
		}
		const double Cos = FVector::DotProduct(To / Dist, Dir);
		if (Cos <= BestCos)
		{
			continue;
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LettersSight), false, Hero);
		Q.AddIgnoredActor(Box);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Eye, At, ECC_Visibility, Q) && Hit.Distance < Dist - 30.0)
		{
			continue;
		}
		Best = Box;
		BestCos = Cos;
	}
	return Best;
}

void ULyudLetters::SetPrompt(ALyudPostbox* Box)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (Box == OfferedWeak.Get() && (!Box || !Box->IsMailbox()))
	{
		return;
	}
	OfferedWeak = Box;
	if (!UI)
	{
		return;
	}
	if (!Box)
	{
		UI->SetPrompt(this, FText::GetEmpty());
		return;
	}
	const int32 Unread = Model ? Model->NumUnread() : 0;
	UI->SetPrompt(this, !Box->IsMailbox() ? LyudText::Get(TEXT("letters.postbox.prompt"))
		: (Unread > 0 ? LyudText::Get(TEXT("letters.mailbox.prompt_new"), { FStringFormatArg(Unread) }) : LyudText::Get(TEXT("letters.mailbox.prompt"))));
}

void ULyudLetters::Refuse(const FText& Why)
{
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!World || !UI)
	{
		return;
	}
	OfferedWeak = nullptr;
	RefusedUntil = World->GetTimeSeconds() + 2.5;
	UI->SetPrompt(this, Why);
	UE_LOG(LogLyudLetters, Display, TEXT("Letters: refused: %s"), *Why.ToString());
}

// --- the people round the hero ------------------------------------------------------------------------------------------

void ULyudLetters::TickPeople(const AHeroCharacter* Hero)
{
	const FVector At = Hero->GetActorLocation();
	ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
	// the open homes: being in one makes its people known (their address in the chooser)
	for (size_t I = 0; I < Model->Data.Correspondents.size(); ++I)
	{
		const LyudLetters::FCorrespondent& C = Model->Data.Correspondents[I];
		const FVector Near(C.Near[0], C.Near[1], C.Near[2]);
		if (FVector::DistSquared2D(Near, At) < FMath::Square(C.NearRadius) && FMath::Abs(Near.Z - At.Z) < 400.0 && Model->Meet(C.Id))
		{
			if (Screen)
			{
				Screen->Toast(LyudText::Get(TEXT("letters.know"), { FStringFormatArg(LyudText::Get(LyudLettersImpl::Str(C.Name)).ToString()) }));
			}
		}
		// his card on their shelf, seen for the first time
		ALyudGoodsItem* Card = I < (size_t)ShelfCards.Num() ? ShelfCards[(int32)I].Get() : nullptr;
		if (Card && FVector::DistSquared(Card->GetActorLocation(), At) < FMath::Square(260.0) && !C.Shelfnote.empty())
		{
			const auto G = Model->Got.find(C.Id);
			const std::string Key = "shelfnote:" + C.Id + ":" + std::to_string(G == Model->Got.end() ? 0 : G->second);
			if (Model->Done.insert(Key).second && Screen)
			{
				Screen->Toast(LyudText::Get(LyudLettersImpl::Str(C.Shelfnote)));
			}
		}
	}
	// a recipient of a recent card says thank you as the hero passes
	const std::vector<std::string> Want = Model->WantsToThank(Now().Day);
	if (Want.empty() || ThanksLines.Num() == 0)
	{
		return;
	}
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	if (!Talk || !Life)
	{
		return;
	}
	AActor* Speaker = nullptr;
	const int32 Id = Talk->NearestSpeaker(Speaker);
	FPersonInfo Info;
	if (Id < 0 || !Speaker || FVector::DistSquared(Speaker->GetActorLocation(), At) > FMath::Square(LyudLettersImpl::ThanksRadius) || !Life->GetPerson(Id, Info))
	{
		return;
	}
	const std::string Key = LyudLettersImpl::Std(Info.Key);
	for (const std::string& To : Want)
	{
		const LyudLetters::FCorrespondent* C = Model->Data.FindCorrespondent(To);
		if (To != Key && !(C && C->Person == Key))
		{
			continue;
		}
		const FString Line = ThanksLines[LyudLetters::FModel::Roll(To, (int32)Model->Cards.size()) % ThanksLines.Num()];
		if (Talk->SayLine(Speaker, Line, Id))
		{
			Model->NoteThanked(To);
			UE_LOG(LogLyudLetters, Display, TEXT("Letters: %s thanks the hero (%s)"), *Info.Key, *Line);
		}
		return;
	}
}

void ULyudLetters::PlaceShelfCards(const FVector& Hero)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (size_t I = 0; I < Model->Data.Correspondents.size() && I < (size_t)ShelfCards.Num(); ++I)
	{
		const LyudLetters::FCorrespondent& C = Model->Data.Correspondents[I];
		const auto G = Model->Got.find(C.Id);
		if (!C.bHasShelf || G == Model->Got.end() || G->second <= 0 || ShelfCards[(int32)I])
		{
			continue;
		}
		const FVector Shelf(C.Shelf[0], C.Shelf[1], C.Shelf[2]);
		if (FVector::DistSquared(Shelf, Hero) > FMath::Square(LyudLettersImpl::ShelfRadius))
		{
			continue;
		}
		// on the board just under the shelf's look point, a little toward the room, leaning back against the books
		const FVector Fwd = FRotator(0.f, C.ShelfYaw, 0.f).Vector();
		const FVector Top = Shelf + Fwd * 9.0 + FVector(0, 0, 20);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(LettersShelf), false);
		if (!World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 70), ECC_Visibility, Q) || Hit.ImpactNormal.Z < 0.8f)
		{
			continue;
		}
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALyudGoodsItem* Card = World->SpawnActor<ALyudGoodsItem>(ALyudGoodsItem::StaticClass(), Hit.ImpactPoint + FVector(0, 0, 7.8),
			FRotator(-9.f, C.ShelfYaw, 0.f), P);
		if (!Card)
		{
			continue;
		}
		Card->Init(TEXT("postcard"));
		Card->SetActorEnableCollision(false);
		ALyudGoodsItem::SetShelfStock(Card);
		ShelfCards[(int32)I] = Card;
	}
}

// --- words ----------------------------------------------------------------------------------------------------------------

FText ULyudLetters::Fill(const FString& Key, const TArray<FString>& Args) const
{
	TArray<FStringFormatArg> A;
	for (const FString& Arg : Args)
	{
		A.Add(FStringFormatArg(Arg.StartsWith(TEXT("@")) ? LyudText::Get(Arg.Mid(1)).ToString() : Arg));
	}
	return LyudText::Get(Key, A);
}

FText ULyudLetters::FromName(const FString& From) const
{
	return LyudText::Get(LyudLettersImpl::FromKey(LyudLettersImpl::Std(From)));
}

bool ULyudLetters::PostTownLetter(const FString& Id, const FString& From, const FString& Body, const TArray<FString>& Args)
{
	if (!bLoaded || !Model)
	{
		return false;
	}
	std::vector<std::string> A;
	for (const FString& S : Args)
	{
		A.push_back(LyudLettersImpl::Std(S));
	}
	return Model->AddLetter(Now(), LyudLettersImpl::Std(Id), LyudLettersImpl::Std(From), "town", LyudLettersImpl::Std(Body), A);
}

FText ULyudLetters::HomeAddress(const FString& Home) const
{
	if (!Model || Home.IsEmpty())
	{
		return FText::GetEmpty();
	}
	const std::vector<std::string> Parts = Model->AddressOf(LyudLettersImpl::Std(Home));
	if (Parts.size() < 2)
	{
		return FText::GetEmpty();
	}
	const FString Street = LyudText::Get(TEXT("letters.street.") + LyudLettersImpl::Str(Parts[0])).ToString();
	return Parts.size() >= 3
		? LyudText::Get(TEXT("letters.addr.flat"), { FStringFormatArg(Street), FStringFormatArg(LyudLettersImpl::Str(Parts[1])), FStringFormatArg(LyudLettersImpl::Str(Parts[2])) })
		: LyudText::Get(TEXT("letters.addr.house"), { FStringFormatArg(Street), FStringFormatArg(LyudLettersImpl::Str(Parts[1])) });
}

FText ULyudLetters::AddressOf(const FString& AddresseeId, int32 PersonId) const
{
	FString Home;
	if (const LyudLetters::FCorrespondent* C = Model->Data.FindCorrespondent(LyudLettersImpl::Std(AddresseeId)))
	{
		Home = LyudLettersImpl::Str(C->Home);
	}
	else if (const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
	{
		FPersonInfo Info;
		if (Life->GetPerson(PersonId, Info))
		{
			Home = Info.Home;
		}
	}
	const std::vector<std::string> Parts = Model->AddressOf(LyudLettersImpl::Std(Home));
	if (Parts.size() >= 2)
	{
		const FString Street = LyudText::Get(TEXT("letters.street.") + LyudLettersImpl::Str(Parts[0])).ToString();
		return Parts.size() >= 3
			? LyudText::Get(TEXT("letters.addr.flat"), { FStringFormatArg(Street), FStringFormatArg(LyudLettersImpl::Str(Parts[1])), FStringFormatArg(LyudLettersImpl::Str(Parts[2])) })
			: LyudText::Get(TEXT("letters.addr.house"), { FStringFormatArg(Street), FStringFormatArg(LyudLettersImpl::Str(Parts[1])) });
	}
	// the Academgorodok's flats (Novikova1-2-17): the flat's number after the last dash
	FString Left, Flat;
	if (Home.Split(TEXT("-"), &Left, &Flat, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && !Flat.IsEmpty())
	{
		return LyudText::Get(TEXT("letters.addr.academ_flat"), { FStringFormatArg(Flat) });
	}
	return LyudText::Get(TEXT("letters.addr.academ"));
}

void ULyudLetters::DiaryLetter(int32 Letter)
{
	ULyudQuests* Quests = ULyudQuests::Get(this);
	if (!Quests || Letter < 0 || Letter >= (int32)Model->Letters.size())
	{
		return;
	}
	const LyudLetters::FLetter& L = Model->Letters[(size_t)Letter];
	const auto Args = [](const std::vector<std::string>& In)
	{
		TArray<FString> Out;
		for (const std::string& S : In)
		{
			Out.Add(LyudLettersImpl::Str(S));
		}
		return Out;
	};
	Quests->AddNote(TEXT("letters.diary.letter"), { TEXT("@") + LyudLettersImpl::FromKey(L.From) });
	if (!L.Ack.empty())
	{
		Quests->AddNote(LyudLettersImpl::Str(L.Ack), Args(L.AckArgs));
	}
	Quests->AddNote(LyudLettersImpl::Str(L.Body), Args(L.Args));
}

// --- the choosers -------------------------------------------------------------------------------------------------------

void ULyudLetters::Use(ALyudPostbox* Box)
{
	if (!Box)
	{
		return;
	}
	if (Box->IsMailbox())
	{
		OpenMailbox();
		return;
	}
	OpenMenu();
}

void ULyudLetters::OpenPage()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !MoveBlockedBy.IsValid())
	{
		// A and D choose: the hero stands while the page is up (looking about is free)
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
	ModeFrame = GFrameCounter;
}

void ULyudLetters::ClosePage()
{
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ULyudLetters::EndMode()
{
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
	ClosePage();
	Mode = EMode::None;
	Sel = 0;
	NumEntries = 0;
	ReadQueue.Reset();
	ReadPages.Reset();
	ReadAt = 0;
	ReadPage = 0;
}

void ULyudLetters::OpenMenu()
{
	MenuActions.Reset();
	if (Model->PenWaiting())
	{
		MenuActions.Add(TEXT("pen"));
	}
	MenuActions.Add(TEXT("card"));
	MenuActions.Add(TEXT("leave"));
	Mode = EMode::Menu;
	Sel = 0;
	OpenPage();
	ShowMode();
}

void ULyudLetters::BuildAddressees(TArray<FAddressee>& Out) const
{
	Out.Reset();
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	TSet<FString> Persons;
	for (const LyudLetters::FCorrespondent& C : Model->Data.Correspondents)
	{
		if (!C.Person.empty())
		{
			Persons.Add(LyudLettersImpl::Str(C.Person));
		}
		if (!bKnowAll && !Model->Known.count(C.Id))
		{
			continue;
		}
		FAddressee A;
		A.Id = LyudLettersImpl::Str(C.Id);
		A.Name = LyudText::Get(LyudLettersImpl::Str(C.Name));
		A.Address = AddressOf(A.Id, -1);
		Out.Add(A);
	}
	if (Life)
	{
		const int32 N = Life->NumPeople();
		int32 TownShown = 0;
		for (int32 Id = 0; Id < N; ++Id)
		{
			FPersonInfo Info;
			if (!Life->GetPerson(Id, Info) || Persons.Contains(Info.Key))
			{
				continue;
			}
			const bool bTown = Life->IsTownsperson(Id);
			const bool bKnown = bTown ? Life->GetMet(Id) > 0 : StoryKnown.Contains(Info.Key.ToLower());
			const bool bDevAll = bKnowAll && (!bTown || TownShown < 30);
			if (!bKnown && !bDevAll)
			{
				continue;
			}
			TownShown += bTown ? 1 : 0;
			FAddressee A;
			A.Id = Info.Key;
			A.PersonId = Id;
			A.Name = Life->GetDisplayName(Id);
			A.Address = AddressOf(A.Id, Id);
			A.Group = LyudLettersImpl::Str(LyudLetters::AgeGroup(Info.Age));
			Out.Add(A);
		}
	}
	Out.Sort([](const FAddressee& L, const FAddressee& R) { return L.Name.CompareTo(R.Name) < 0; });
}

void ULyudLetters::BeginCard()
{
	// the story's people he has talked with: the targets of the talks of every quest begun
	StoryKnown.Reset();
	if (const ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		for (const FLyudQuestDef& Def : ULyudQuests::Defs())
		{
			if (Quests->State(Def.Id) == ELyudQuestState::Inactive)
			{
				continue;
			}
			for (const FLyudQuestStage& S : Def.Stages)
			{
				for (const FLyudQuestObjective& O : S.Objectives)
				{
					if (O.Event == TEXT("talk"))
					{
						StoryKnown.Add(O.Target.ToLower());
					}
				}
			}
		}
	}
	BuildAddressees(Addressees);
	if (Addressees.Num() == 0)
	{
		EndMode();
		Refuse(LyudText::Get(TEXT("letters.refuse.nobody")));
		return;
	}
	CardTo = CardPicture = CardOpening = CardWish = -1;
	Mode = EMode::CardTo;
	Sel = 0;
	ShowMode();
}

void ULyudLetters::ShowList(const FText& Title, const TArray<FText>& Entries, const FText& Keys, const FText& Head)
{
	if (!Page)
	{
		return;
	}
	NumEntries = Entries.Num();
	Sel = NumEntries > 0 ? FMath::Clamp(Sel, 0, NumEntries - 1) : 0;
	const int32 First = (Sel / LyudLettersImpl::PageSize) * LyudLettersImpl::PageSize;
	FString Body = Head.IsEmpty() ? FString() : Head.ToString() + TEXT("\n\n");
	for (int32 I = First; I < NumEntries && I < First + LyudLettersImpl::PageSize; ++I)
	{
		Body += FString::Printf(TEXT("%s%d. %s\n"), I == Sel ? TEXT("▸ ") : TEXT("   "), I - First + 1, *Entries[I].ToString());
	}
	const int32 After = NumEntries - (First + LyudLettersImpl::PageSize);
	if (After > 0)
	{
		Body += LyudText::Get(TEXT("letters.more"), { FStringFormatArg(After) }).ToString();
	}
	Page->SetPage(Title, FText::FromString(Body.TrimEnd()), Keys);
}

void ULyudLetters::ShowMode()
{
	if (!Page)
	{
		return;
	}
	const FText Choose = LyudText::Get(TEXT("letters.keys.choose"));
	TArray<FText> E;
	const auto Phrases = [this](const std::vector<std::string>& Ids)
	{
		TArray<FText> Out;
		for (const std::string& Id : Ids)
		{
			Out.Add(LyudText::Get(TEXT("letters.phrase.") + LyudLettersImpl::Str(Id)));
		}
		return Out;
	};
	switch (Mode)
	{
	case EMode::Menu:
	{
		const ULyudInventory* Inv = ULyudInventory::Get(this);
		const int32 Cards = Inv ? Inv->Count(TEXT("postcard")) : 0;
		for (const FName& A : MenuActions)
		{
			E.Add(A == TEXT("pen") ? LyudText::Get(TEXT("letters.menu.pen"))
				: (A == TEXT("card") ? LyudText::Get(TEXT("letters.menu.card"), { FStringFormatArg(Cards) }) : LyudText::Get(TEXT("letters.menu.leave"))));
		}
		ShowList(LyudText::Get(TEXT("letters.postbox.title")), E, Choose, LyudText::Get(TEXT("letters.postbox.collect")));
		break;
	}
	case EMode::CardTo:
		for (const FAddressee& A : Addressees)
		{
			E.Add(FText::FromString(A.Name.ToString() + TEXT(" — ") + A.Address.ToString()));
		}
		ShowList(LyudText::Get(TEXT("letters.step.to")), E, Choose);
		break;
	case EMode::CardPicture:
		for (const std::string& Id : Model->Data.Pictures)
		{
			E.Add(LyudText::Get(TEXT("letters.pic.") + LyudLettersImpl::Str(Id)));
		}
		ShowList(LyudText::Get(TEXT("letters.step.picture")), E, Choose);
		break;
	case EMode::CardOpening:
		ShowList(LyudText::Get(TEXT("letters.step.opening")), Phrases(Model->Data.Openings), Choose);
		break;
	case EMode::CardWish:
		ShowList(LyudText::Get(TEXT("letters.step.wish")), Phrases(Model->Data.Wishes), Choose);
		break;
	case EMode::CardPreview:
	{
		NumEntries = 0;
		const FAddressee& To = Addressees[CardTo];
		const FText Body = LyudText::Get(TEXT("letters.card.preview"), {
			FStringFormatArg(LyudText::Get(TEXT("letters.pic.") + LyudLettersImpl::Str(Model->Data.Pictures[(size_t)CardPicture])).ToString()),
			FStringFormatArg(LyudText::Get(TEXT("letters.phrase.") + LyudLettersImpl::Str(Model->Data.Openings[(size_t)CardOpening])).ToString()),
			FStringFormatArg(LyudText::Get(TEXT("letters.phrase.") + LyudLettersImpl::Str(Model->Data.Wishes[(size_t)CardWish])).ToString()),
			FStringFormatArg(To.Name.ToString()), FStringFormatArg(To.Address.ToString()) });
		Page->SetPage(LyudText::Get(TEXT("letters.step.preview")), Body, LyudText::Get(TEXT("letters.keys.post")));
		break;
	}
	case EMode::Answer:
		for (int32 I = 0; I < AnswerKinds.Num(); ++I)
		{
			const auto It = Model->Data.PenAnswers.find(LyudLettersImpl::Std(AnswerKinds[I]));
			const FString K = It != Model->Data.PenAnswers.end() ? LyudLettersImpl::Str(It->second) : FString();
			E.Add(Fill(K, { TEXT("@") + AnswerFacts[I] }));
		}
		ShowList(LyudText::Get(TEXT("letters.step.answer")), E, Choose);
		break;
	case EMode::AnswerPreview:
	{
		NumEntries = 0;
		const auto It = Model->Data.PenAnswers.find(LyudLettersImpl::Std(AnswerKinds[AnswerSel]));
		const FString K = It != Model->Data.PenAnswers.end() ? LyudLettersImpl::Str(It->second) : FString();
		const FText Sentence = Fill(K, { TEXT("@") + AnswerFacts[AnswerSel] });
		Page->SetPage(LyudText::Get(TEXT("letters.step.answer_preview")), LyudText::Get(TEXT("letters.answer.preview"), { FStringFormatArg(Sentence.ToString()) }),
			LyudText::Get(TEXT("letters.keys.post")));
		break;
	}
	case EMode::Reread:
		for (const int32 I : RereadList)
		{
			const LyudLetters::FLetter& L = Model->Letters[(size_t)I];
			int32 Y = 0, M = 0, D = 0;
			LyudLetters::CivilFromDays(L.Arrive, Y, M, D);
			E.Add(FText::FromString(FString::Printf(TEXT("%s, %02d.%02d"), *FromName(LyudLettersImpl::Str(L.From)).ToString(), D, M)));
		}
		ShowList(LyudText::Get(TEXT("letters.step.reread")), E, Choose);
		break;
	case EMode::Reading:
		ShowLetterPage();
		break;
	default:
		break;
	}
}

void ULyudLetters::Choose(int32 Entry)
{
	if (Entry < 0 || (NumEntries > 0 && Entry >= NumEntries))
	{
		return;
	}
	Sel = Entry;
	switch (Mode)
	{
	case EMode::Menu:
	{
		const FName A = MenuActions.IsValidIndex(Entry) ? MenuActions[Entry] : FName(TEXT("leave"));
		if (A == TEXT("card"))
		{
			const ULyudInventory* Inv = ULyudInventory::Get(this);
			if (!Inv || Inv->Count(TEXT("postcard")) <= 0)
			{
				EndMode();
				Refuse(LyudText::Get(TEXT("letters.refuse.no_cards")));
				return;
			}
			BeginCard();
		}
		else if (A == TEXT("pen"))
		{
			LyudLetters::FFacts Facts;
			FillFacts(Facts);
			AnswerKinds.Reset();
			AnswerFacts.Reset();
			for (const LyudLetters::FAnswer& O : Model->PenOptions(Facts))
			{
				AnswerKinds.Add(LyudLettersImpl::Str(O.Kind));
				AnswerFacts.Add(LyudLettersImpl::Str(O.Fact));
			}
			Mode = EMode::Answer;
			Sel = 0;
			ShowMode();
		}
		else
		{
			EndMode();
		}
		return;
	}
	case EMode::CardTo:
		CardTo = Entry;
		Mode = EMode::CardPicture;
		Sel = FMath::Max(CardPicture, 0);
		break;
	case EMode::CardPicture:
		CardPicture = Entry;
		Mode = EMode::CardOpening;
		Sel = FMath::Max(CardOpening, 0);
		break;
	case EMode::CardOpening:
		CardOpening = Entry;
		Mode = EMode::CardWish;
		Sel = FMath::Max(CardWish, 0);
		break;
	case EMode::CardWish:
		CardWish = Entry;
		Mode = EMode::CardPreview;
		break;
	case EMode::CardPreview:
		PostTheCard();
		return;
	case EMode::Answer:
		AnswerSel = Entry;
		Mode = EMode::AnswerPreview;
		break;
	case EMode::AnswerPreview:
		PostTheAnswer();
		return;
	case EMode::Reread:
		if (RereadList.IsValidIndex(Entry))
		{
			ReadQueue = { RereadList[Entry] };
			ReadAt = 0;
			ReadPage = 0;
			ReadPages.Reset();
			Mode = EMode::Reading;
		}
		break;
	default:
		return;
	}
	ShowMode();
}

void ULyudLetters::Back()
{
	switch (Mode)
	{
	case EMode::CardTo:
	case EMode::Answer:
		Mode = EMode::Menu;
		Sel = 0;
		break;
	case EMode::CardPicture:
		Mode = EMode::CardTo;
		Sel = FMath::Max(CardTo, 0);
		break;
	case EMode::CardOpening:
		Mode = EMode::CardPicture;
		Sel = FMath::Max(CardPicture, 0);
		break;
	case EMode::CardWish:
		Mode = EMode::CardOpening;
		Sel = FMath::Max(CardOpening, 0);
		break;
	case EMode::CardPreview:
		Mode = EMode::CardWish;
		Sel = FMath::Max(CardWish, 0);
		break;
	case EMode::AnswerPreview:
		Mode = EMode::Answer;
		Sel = FMath::Max(AnswerSel, 0);
		break;
	default:
		EndMode();
		return;
	}
	ShowMode();
}

void ULyudLetters::PostTheCard()
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Addressees.IsValidIndex(CardTo) || CardPicture < 0 || CardOpening < 0 || CardWish < 0)
	{
		EndMode();
		return;
	}
	if (!Inv || !Inv->Remove(TEXT("postcard"), 1))
	{
		EndMode();
		Refuse(LyudText::Get(TEXT("letters.refuse.no_cards")));
		return;
	}
	const FAddressee& To = Addressees[CardTo];
	const LyudLetters::FTime T = Now();
	const std::string Pic = Model->Data.Pictures[(size_t)CardPicture];
	const std::string Open = Model->Data.Openings[(size_t)CardOpening];
	const std::string Wish = Model->Data.Wishes[(size_t)CardWish];
	if (!Model->PostCard(T, LyudLettersImpl::Std(To.Id), Pic, Open, Wish, LyudLettersImpl::Std(To.Group)))
	{
		Inv->Add(TEXT("postcard"), 1);
		EndMode();
		return;
	}
	const LyudLetters::FCorrespondent* C = Model->Data.FindCorrespondent(LyudLettersImpl::Std(To.Id));
	const FString NameKey = C ? LyudLettersImpl::Str(C->Name) : FString::Printf(TEXT("npc.%s.name"), *To.Id);
	if (ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		Quests->AddNote(TEXT("letters.diary.card"), { TEXT("@letters.pic.") + LyudLettersImpl::Str(Pic), TEXT("@") + NameKey,
			TEXT("@letters.phrase.") + LyudLettersImpl::Str(Open), TEXT("@letters.phrase.") + LyudLettersImpl::Str(Wish) });
	}
	ULyudQuests::Notify(this, TEXT("do"), TEXT("post_card"));
	ULyudQuests::Notify(this, TEXT("letter_sent"), To.Id);
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		if (const ALyudPostbox* Box = OfferedWeak.Get())
		{
			Sound->PlayEffect(TEXT("diary_page"), Box->UsePoint());
		}
	}
	const int32 Day = Model->DeliveryDay(T);
	const FText When = Day == T.Day + 1 ? LyudText::Get(TEXT("letters.when.tomorrow")) : LyudText::Get(FString::Printf(TEXT("letters.when.%d"), LyudLetters::Weekday(Day)));
	UE_LOG(LogLyudLetters, Display, TEXT("Letters: a card to %s (%s), carried on day %d"), *To.Id, *LyudLettersImpl::Str(Pic), Day);
	EndMode();
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(TEXT("letters.posted"), { FStringFormatArg(When.ToString()) }));
	}
}

void ULyudLetters::PostTheAnswer()
{
	if (!AnswerKinds.IsValidIndex(AnswerSel))
	{
		EndMode();
		return;
	}
	const LyudLetters::FAnswer A{ LyudLettersImpl::Std(AnswerKinds[AnswerSel]), LyudLettersImpl::Std(AnswerFacts[AnswerSel]) };
	if (!Model->PostPenAnswer(Now(), A))
	{
		EndMode();
		return;
	}
	const auto It = Model->Data.PenAnswers.find(A.Kind);
	const FText Sentence = Fill(It != Model->Data.PenAnswers.end() ? LyudLettersImpl::Str(It->second) : FString(), { TEXT("@") + AnswerFacts[AnswerSel] });
	if (ULyudQuests* Quests = ULyudQuests::Get(this))
	{
		Quests->AddNote(TEXT("letters.diary.answer"), { Sentence.ToString() });
	}
	ULyudQuests::Notify(this, TEXT("do"), TEXT("pen_answer"));
	UE_LOG(LogLyudLetters, Display, TEXT("Letters: answered Tonya (%s %s)"), *AnswerKinds[AnswerSel], *AnswerFacts[AnswerSel]);
	EndMode();
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(TEXT("letters.posted_pen")));
	}
}

void ULyudLetters::OpenMailbox()
{
	ReadQueue.Reset();
	for (size_t I = 0; I < Model->Letters.size(); ++I)
	{
		if (Model->Letters[I].bArrived && !Model->Letters[I].bRead)
		{
			ReadQueue.Add((int32)I);
		}
	}
	ReadQueue.Sort([this](int32 L, int32 R) { return Model->Letters[(size_t)L].Arrive < Model->Letters[(size_t)R].Arrive; });
	if (ReadQueue.Num() > 0)
	{
		Mode = EMode::Reading;
		ReadAt = 0;
		ReadPage = 0;
		ReadPages.Reset();
		OpenPage();
		ShowMode();
		return;
	}
	RereadList.Reset();
	for (int32 I = (int32)Model->Letters.size() - 1; I >= 0 && RereadList.Num() < 45; --I)
	{
		if (Model->Letters[(size_t)I].bRead)
		{
			RereadList.Add(I);
		}
	}
	if (RereadList.Num() == 0)
	{
		Refuse(LyudText::Get(TEXT("letters.refuse.empty")));
		return;
	}
	Mode = EMode::Reread;
	Sel = 0;
	OpenPage();
	ShowMode();
}

void ULyudLetters::ShowLetterPage()
{
	if (!Page || !ReadQueue.IsValidIndex(ReadAt))
	{
		EndMode();
		return;
	}
	const int32 Index = ReadQueue[ReadAt];
	const LyudLetters::FLetter& L = Model->Letters[(size_t)Index];
	if (ReadPages.Num() == 0)
	{
		// the letter's words, the acknowledgement first, cut into pages by paragraphs
		const auto Args = [](const std::vector<std::string>& In)
		{
			TArray<FString> Out;
			for (const std::string& S : In)
			{
				Out.Add(LyudLettersImpl::Str(S));
			}
			return Out;
		};
		FString All = Fill(LyudLettersImpl::Str(L.Body), Args(L.Args)).ToString();
		if (!L.Ack.empty())
		{
			All = Fill(LyudLettersImpl::Str(L.Ack), Args(L.AckArgs)).ToString() + TEXT("\n\n") + All;
		}
		TArray<FString> Paras;
		All.ParseIntoArray(Paras, TEXT("\n\n"));
		FString Cur;
		for (const FString& P : Paras)
		{
			if (!Cur.IsEmpty() && LyudLettersImpl::TextCost(Cur) + LyudLettersImpl::TextCost(P) > LyudLettersImpl::PageChars)
			{
				ReadPages.Add(FText::FromString(Cur));
				Cur.Reset();
			}
			Cur += Cur.IsEmpty() ? P : TEXT("\n\n") + P;
		}
		if (!Cur.IsEmpty())
		{
			ReadPages.Add(FText::FromString(Cur));
		}
		ReadPage = 0;
		// read from its first page on: into the diary once
		if (!L.bRead)
		{
			Model->MarkRead(Index, Now().Day);
			DiaryLetter(Index);
		}
	}
	const FText Name = FromName(LyudLettersImpl::Str(L.From));
	const FText Title = L.Kind == "pen"
		? LyudText::Get(TEXT("letters.letter.from_place"), { FStringFormatArg(Name.ToString()), FStringFormatArg(LyudText::Get(TEXT("letters.pen.place")).ToString()) })
		: LyudText::Get(TEXT("letters.letter.from"), { FStringFormatArg(Name.ToString()) });
	const bool bMore = ReadPage + 1 < ReadPages.Num() || ReadAt + 1 < ReadQueue.Num();
	Page->SetPage(Title, ReadPages.IsValidIndex(ReadPage) ? ReadPages[ReadPage] : FText::GetEmpty(), LyudText::Get(bMore ? TEXT("letters.keys.next") : TEXT("letters.keys.close")));
}

void ULyudLetters::FinishLetter()
{
	const bool bWaits = Model->PenWaiting();
	if (++ReadAt < ReadQueue.Num())
	{
		ReadPages.Reset();
		ReadPage = 0;
		ShowLetterPage();
		return;
	}
	EndMode();
	if (bWaits)
	{
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			Screen->Toast(LyudText::Get(TEXT("letters.hint.answer")));
		}
	}
}

void ULyudLetters::TickMode(AHeroCharacter* Hero, APlayerController* PC)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !Hero || !PC || !Page || (UI && UI->IsOpen()))
	{
		EndMode();
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || GFrameCounter == ModeFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::S)
		|| PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		if (Mode == EMode::Reading || Mode == EMode::Reread)
		{
			EndMode();
		}
		else
		{
			Back();
		}
		return;
	}
	const bool bE = PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Enter);
	if (Mode == EMode::Reading)
	{
		if (bE)
		{
			if (++ReadPage < ReadPages.Num())
			{
				ShowLetterPage();
				if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
				{
					Sound->PlayEffect(TEXT("diary_page"), Hero->GetActorLocation());
				}
			}
			else
			{
				FinishLetter();
			}
		}
		return;
	}
	if (Mode == EMode::CardPreview || Mode == EMode::AnswerPreview)
	{
		if (bE)
		{
			Choose(0);
		}
		return;
	}
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const int32 First = (Sel / LyudLettersImpl::PageSize) * LyudLettersImpl::PageSize;
	for (int32 I = 0; I < (int32)UE_ARRAY_COUNT(Digits); ++I)
	{
		if (First + I < NumEntries && PC->WasInputKeyJustPressed(Digits[I]))
		{
			Choose(First + I);
			return;
		}
	}
	const int32 Delta = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::Down)) ? 1
		: ((PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::Up)) ? -1 : 0);
	if (Delta != 0 && NumEntries > 0)
	{
		Sel = (Sel + Delta + NumEntries) % NumEntries;
		ShowMode();
		return;
	}
	if (bE)
	{
		Choose(Sel);
	}
}

// --- console and developer mode ------------------------------------------------------------------------------------------

FString ULyudLetters::Report() const
{
	if (!Model || !bLoaded)
	{
		return TEXT("Letters: no data");
	}
	int32 Placed = 0;
	for (const FBoxSlot& S : Boxes)
	{
		Placed += S.Actor.IsValid() ? 1 : 0;
	}
	int32 OnWay = 0;
	for (const LyudLetters::FCard& C : Model->Cards)
	{
		OnWay += C.bDelivered ? 0 : 1;
	}
	return FString::Printf(TEXT("Letters: day %d; boxes %d of %d placed; cards %d sent, %d on their way; letters %d (%d unread, %d on their way); "
		"pen friend: %d sent, %d answered%s; known homes %d"),
		Now().Day, Placed, Boxes.Num(), (int32)Model->Cards.size(), OnWay, (int32)Model->Letters.size(), Model->NumUnread(), Model->NumOnTheWay(),
		Model->PenSent, Model->PenAnswered, Model->PenWaiting() ? TEXT(" (waits for an answer)") : TEXT(""), (int32)Model->Known.size());
}

void ULyudLetters::DevDeliverAll()
{
	if (!Model)
	{
		return;
	}
	const LyudLetters::FTime T = Now();
	Model->DevDeliverAll(T);
	StepPost();     // cards carried, replies written
	Model->DevDeliverAll(T);
	StepPost();     // and those arrive too
}

void ULyudLetters::DevPenNext()
{
	if (!Model)
	{
		return;
	}
	Model->DevPenNext(Now());
	StepPost();
	DevDeliverAll();
}

void ULyudLetters::DevKnowAll()
{
	bKnowAll = true;
	for (const LyudLetters::FCorrespondent& C : Model->Data.Correspondents)
	{
		Model->Meet(C.Id);
	}
}

void ULyudLetters::OpenMailAnywhere()
{
	if (Mode == EMode::None && bLoaded)
	{
		OpenMailbox();
	}
}

bool ULyudLetters::DevPost(const FString& To, const FString& Picture)
{
	if (!Model || To.IsEmpty())
	{
		return false;
	}
	FString Group = TEXT("adult");
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const int32 Id = Life ? Life->FindPerson(To) : -1;
	if (!Model->Data.FindCorrespondent(LyudLettersImpl::Std(To)) && Id < 0)
	{
		return false;
	}
	FPersonInfo Info;
	if (Id >= 0 && Life->GetPerson(Id, Info))
	{
		Group = LyudLettersImpl::Str(LyudLetters::AgeGroup(Info.Age));
	}
	const std::string Pic = Picture.IsEmpty() ? Model->Data.Pictures[0] : LyudLettersImpl::Std(Picture);
	return Model->PostCard(Now(), LyudLettersImpl::Std(To), Pic, Model->Data.Openings[0], Model->Data.Wishes[0], LyudLettersImpl::Std(Group));
}

bool ULyudLetters::MailboxStand(FTransform& Out) const
{
	for (const FBoxSlot& S : Boxes)
	{
		if (const ALyudPostbox* Box = S.bMailbox ? S.Actor.Get() : nullptr)
		{
			const FVector Fwd = Box->GetActorForwardVector().GetSafeNormal2D();
			const FVector Feet = Box->GetActorLocation() + Fwd * 150.0;
			Out = FTransform(FRotator(0.f, (-Fwd).Rotation().Yaw, 0.f), FVector(Feet.X, Feet.Y, Box->UsePoint().Z - 135.0));
			return true;
		}
	}
	return false;
}

bool ULyudLetters::NearestPostboxStand(const FVector& From, FTransform& Out) const
{
	const ALyudPostbox* Best = nullptr;
	for (const FBoxSlot& S : Boxes)
	{
		const ALyudPostbox* Box = S.bMailbox ? nullptr : S.Actor.Get();
		if (Box && (!Best || FVector::DistSquared(Box->GetActorLocation(), From) < FVector::DistSquared(Best->GetActorLocation(), From)))
		{
			Best = Box;
		}
	}
	if (!Best)
	{
		return false;
	}
	const FVector Fwd = Best->GetActorForwardVector().GetSafeNormal2D();
	Out = FTransform(FRotator(0.f, (-Fwd).Rotation().Yaw, 0.f), Best->GetActorLocation() + Fwd * 140.0);
	return true;
}

namespace LyudLettersConsole
{
	ULyudLetters* Of(UWorld* World) { return World ? World->GetSubsystem<ULyudLetters>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs GReport(TEXT("lyud.Letters.Report"), TEXT("The post: cards, letters, the pen friend, the boxes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudLetters* L = Of(World)) { UE_LOG(LogLyudLetters, Display, TEXT("%s"), *L->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GDeliver(TEXT("lyud.Letters.Deliver"), TEXT("Every card and letter on its way arrives now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudLetters* L = Of(World)) { L->DevDeliverAll(); UE_LOG(LogLyudLetters, Display, TEXT("%s"), *L->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GPen(TEXT("lyud.Letters.PenNext"), TEXT("Tonya's next letter now (answering for the hero if needed)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudLetters* L = Of(World)) { L->DevPenNext(); UE_LOG(LogLyudLetters, Display, TEXT("%s"), *L->Report()); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GMail(TEXT("lyud.Letters.Mail"), TEXT("Opens the club's mailbox wherever the hero is."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudLetters* L = Of(World)) { L->OpenMailAnywhere(); }
		}));
	FAutoConsoleCommandWithWorldAndArgs GPost(TEXT("lyud.Letters.Post"), TEXT("lyud.Letters.Post <person key | h_id> [picture]: a card at once."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudLetters* L = Of(World);
			const bool bOk = L && Args.Num() > 0 && L->DevPost(Args[0], Args.Num() > 1 ? Args[1] : FString());
			UE_LOG(LogLyudLetters, Display, TEXT("Letters: post %s: %s"), Args.Num() > 0 ? *Args[0] : TEXT("?"), bOk ? TEXT("sent") : TEXT("refused"));
		}));
	FAutoConsoleCommandWithWorldAndArgs GKnow(TEXT("lyud.Letters.KnowAll"), TEXT("Every correspondent and some townsfolk in the chooser."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudLetters* L = Of(World)) { L->DevKnowAll(); }
		}));
}

#if LYUD_LETTERS_DEV
namespace LyudLettersDev
{
	using namespace LyudDev;
	FAutoAction GDeliverAction(TEXT("letters.deliver"), TEXT("letters"), TEXT("dev.action.letters.deliver"),
		[](UWorld* W) { if (ULyudLetters* L = LyudLettersConsole::Of(W)) { L->DevDeliverAll(); } });
	FAutoAction GPenAction(TEXT("letters.pen_next"), TEXT("letters"), TEXT("dev.action.letters.pen_next"),
		[](UWorld* W) { if (ULyudLetters* L = LyudLettersConsole::Of(W)) { L->DevPenNext(); } });
	FAutoAction GCardsAction(TEXT("letters.cards"), TEXT("letters"), TEXT("dev.action.letters.cards"),
		[](UWorld* W) { if (ULyudInventory* Inv = ULyudInventory::Get(W)) { Inv->Add(TEXT("postcard"), 5); } });
	FAutoAction GKnowAction(TEXT("letters.know_all"), TEXT("letters"), TEXT("dev.action.letters.know_all"),
		[](UWorld* W) { if (ULyudLetters* L = LyudLettersConsole::Of(W)) { L->DevKnowAll(); } });
	FAutoTeleport GMailboxTp(TEXT("letters_club_mailbox"), TEXT("letters"), TEXT("dev.tp.letters_club_mailbox"), [](UWorld* W, FTransform& Out)
	{
		const ULyudLetters* L = LyudLettersConsole::Of(W);
		return L && L->MailboxStand(Out);
	});
	FAutoTeleport GPostboxTp(TEXT("letters_postbox_kiosk1"), TEXT("letters"), TEXT("dev.tp.letters_postbox_kiosk1"), [](UWorld* W, FTransform& Out)
	{
		// the kiosk by the first stop: its box once placed, else the kiosk's front (the box is placed as the hero arrives)
		const ULyudLetters* L = LyudLettersConsole::Of(W);
		const FVector Kiosk(9276.3, 47573.8, -3845.0);
		if (L && L->NearestPostboxStand(Kiosk, Out) && FVector::Dist2D(Out.GetLocation(), Kiosk) < 800.0)
		{
			return true;
		}
		Out = FTransform(FRotator(0.f, 178.f, 0.f), Kiosk + FVector(250.0, 0.0, -105.0));
		return true;
	});
}
#endif
