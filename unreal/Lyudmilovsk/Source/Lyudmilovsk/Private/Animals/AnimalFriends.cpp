#include "Animals/AnimalFriends.h"

#include "AnimalSim.h"
#include "Animals/AnimalActor.h"
#include "Animals/AnimalLife.h"
#include "Animals/LyudPets.h"
#include "BirdTame.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

using LyudAnimals::EAct;
using LyudAnimals::EFriend;
using LyudAnimals::EKind;
using LyudAnimals::FAnimal;

static TAutoConsoleVariable<int32> CVarFriends(TEXT("lyud.Pets"), 1,
	TEXT("0: the player cannot befriend the cats and dogs (no hand, no treats, no walks)."));

// --- the diary's record ---------------------------------------------------------------------------------

ULyudAnimalFriends* ULyudAnimalFriends::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudAnimalFriends>() : nullptr;
}

void ULyudAnimalFriends::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("AnimalFriends"), this);
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { OnChanged.Broadcast(); });
		// this outlives the level: a new game starts with nobody, not the last game's friends
		Saves->OnNewGame.AddWeakLambda(this, [this]() { Reset(); });
	}
}

void ULyudAnimalFriends::Reset()
{
	Friends.Reset();
	OnChanged.Broadcast();
}

const FLyudAnimalFriend* ULyudAnimalFriends::Find(int32 AnimalId) const
{
	return Friends.FindByPredicate([AnimalId](const FLyudAnimalFriend& F) { return F.AnimalId == AnimalId; });
}

FLyudAnimalFriend* ULyudAnimalFriends::FindMutable(int32 AnimalId)
{
	return Friends.FindByPredicate([AnimalId](const FLyudAnimalFriend& F) { return F.AnimalId == AnimalId; });
}

bool ULyudAnimalFriends::NoteFriend(const UObject* WorldContext, const FLyudAnimalFriend& Who, const FVector& Where)
{
	if (Who.AnimalId < 0 || Find(Who.AnimalId))
	{
		return false;
	}
	FLyudAnimalFriend& F = Friends.Add_GetRef(Who);
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	F.Met = UGameCalendar::NowIn(World);
	// the nearest place the hero knows (within 400 m): "by the club"
	const ULyudQuests* Q = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULyudQuests>() : nullptr;
	const FVector2D At = ULyudQuests::Frame().FromWorld(Where);
	double BestD = 400.0 * 400.0;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		const double D = FVector2D::DistSquared(P.At, At);
		if (Q && (Q->IsFound(P.Id) || P.bKnown) && P.Kind != TEXT("land") && D < BestD)
		{
			BestD = D;
			F.Place = P.Id;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("AnimalFriends: animal %d (%s) is a friend (%s)"), F.AnimalId, *F.Breed.ToString(), *F.How.ToString());
	OnChanged.Broadcast();
	return true;
}

void ULyudAnimalFriends::SetName(int32 AnimalId, const FString& Name)
{
	if (FLyudAnimalFriend* F = FindMutable(AnimalId))
	{
		F->Name = Name;
		OnChanged.Broadcast();
	}
}

void ULyudAnimalFriends::Count(int32 AnimalId, FName What)
{
	if (FLyudAnimalFriend* F = FindMutable(AnimalId))
	{
		static const FName Strokes(TEXT("Strokes")), Treats(TEXT("Treats")), Walks(TEXT("Walks"));
		(What == Strokes ? F->Strokes : (What == Treats ? F->Treats : F->Walks))++;
		// (no OnChanged: the page is rebuilt when the diary is next taken out; a stroke must not rebuild it every time)
	}
}

FText ULyudAnimalFriends::DisplayName(const FLyudAnimalFriend& F)
{
	if (!F.Name.IsEmpty())
	{
		return FText::FromString(F.Name);
	}
	if (!F.NameKey.IsEmpty())
	{
		return LyudText::Get(F.NameKey);
	}
	return LyudText::Get(F.bCat ? (F.bFemale ? TEXT("animal.kind.cat_f") : TEXT("animal.kind.cat_m")) : (F.bFemale ? TEXT("animal.kind.dog_f") : TEXT("animal.kind.dog_m")));
}

const FSlateBrush* ULyudAnimalFriends::Portrait(FName Breed)
{
	// rooted with raw pointers like the bird drawings (ULyudBirdBook::Drawing): five breeds at most
	struct FDrawing { TSharedPtr<FSlateBrush> Brush; UTexture2D* Tex = nullptr; };
	static TMap<FName, FDrawing> Cache;
	if (const FDrawing* D = Cache.Find(Breed))
	{
		return D->Brush.Get();
	}
	FDrawing& D = Cache.Add(Breed);
	const FString Asset = TEXT("T_Friend_") + Breed.ToString();
	if (UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Diary/Friends/%s.%s"), *Asset, *Asset), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		T->AddToRoot();
		D.Tex = T;
		D.Brush = MakeShared<FSlateBrush>();
		D.Brush->SetResourceObject(T);
		D.Brush->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
	}
	return D.Brush.Get();
}

// --- what they like to eat -------------------------------------------------------------------------------

namespace
{
	/** How much a cat (X) and a dog (Y) want an item, 0..1; an item not here is not offered. Fish caught on the rod (fish_<species>)
	 *  is the cats' best. Sweets are in the table so that they can be held out and left: a dog does not eat chocolate. */
	const TMap<FName, FVector2f>& TreatTable()
	{
		struct FRow { const TCHAR* Id; float Cat; float Dog; };
		static const FRow Rows[] = {
			{ TEXT("smoked_fish"), 1.f, 0.6f },
			{ TEXT("dried_fish"), 0.9f, 0.5f },
			{ TEXT("diner_fish"), 1.f, 0.6f },
			{ TEXT("rs_baltic_fish"), 1.f, 0.6f },
			{ TEXT("rs_fried_flounder"), 1.f, 0.6f },
			{ TEXT("rs_sushi_set"), 0.8f, 0.4f },
			{ TEXT("milk_pyramid"), 0.7f, 0.3f },
			{ TEXT("kefir"), 0.5f, 0.3f },
			{ TEXT("condensed_milk"), 0.4f, 0.3f },
			{ TEXT("cheese_braid"), 0.5f, 0.7f },
			{ TEXT("cheese_round_small"), 0.5f, 0.7f },
			{ TEXT("cheese_round_big"), 0.5f, 0.7f },
			{ TEXT("sandwich"), 0.5f, 1.f },
			{ TEXT("cheburek"), 0.4f, 1.f },
			{ TEXT("pelmeni_portion"), 0.5f, 1.f },
			{ TEXT("rs_pelmeni"), 0.5f, 1.f },
			{ TEXT("rs_steak"), 0.8f, 1.f },
			{ TEXT("rs_shashlik"), 0.8f, 1.f },
			{ TEXT("diner_burger"), 0.5f, 1.f },
			{ TEXT("rs_golubtsy"), 0.3f, 0.8f },
			{ TEXT("diner_eggs"), 0.4f, 0.7f },
			{ TEXT("pie"), 0.1f, 0.7f },
			{ TEXT("diner_pie"), 0.1f, 0.7f },
			{ TEXT("rs_fest_kulebyaka"), 0.6f, 0.8f },
			{ TEXT("bread_loaf"), 0.05f, 0.35f },
			{ TEXT("bread_rye"), 0.05f, 0.35f },
			{ TEXT("bubliki"), 0.05f, 0.3f },
			{ TEXT("cookies"), 0.05f, 0.3f },
			{ TEXT("vatrushka"), 0.2f, 0.4f },
			{ TEXT("donuts"), 0.05f, 0.2f },
			{ TEXT("apple"), 0.f, 0.15f },
			{ TEXT("chips"), 0.05f, 0.2f },
			{ TEXT("chocolate"), 0.f, 0.05f },
			{ TEXT("candy"), 0.f, 0.05f },
			{ TEXT("gingerbread"), 0.f, 0.1f },
		};
		static TMap<FName, FVector2f> Table;
		if (Table.IsEmpty())
		{
			for (const FRow& R : Rows)
			{
				Table.Add(FName(R.Id), FVector2f(R.Cat, R.Dog));
			}
		}
		return Table;
	}

	float TreatLike(FName Item, bool bCat)
	{
		if (const FVector2f* L = TreatTable().Find(Item))
		{
			return bCat ? L->X : L->Y;
		}
		// the catch: fish_<species> (not the fish book)
		TCHAR Buf[NAME_SIZE];
		Item.GetPlainNameString(Buf);
		if (FCString::Strncmp(Buf, TEXT("fish_"), 5) == 0 && FCString::Strcmp(Buf, TEXT("fish_book")) != 0)
		{
			return bCat ? 1.f : 0.5f;
		}
		return -1.f;
	}

	FVector SimToUE(const LyudAnimals::V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }

	const FSlateBrush* FlatBrush()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** The little box a friend's name is typed in. */
	class SAnimalNamePopup : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAnimalNamePopup) {}
			SLATE_ARGUMENT(FText, Title)
			SLATE_ARGUMENT(FText, Hint)
			SLATE_ARGUMENT(FText, Initial)
			SLATE_EVENT(FOnTextCommitted, OnDone)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			// the name tags' colours (AnimalNameTags.cpp)
			const FLinearColor Warm(1.f, 0.78f, 0.45f), Paper(0.96f, 0.94f, 0.9f), Shade(0.f, 0.f, 0.f, 0.78f);
			ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder).BorderImage(FlatBrush()).BorderBackgroundColor(Shade).Padding(FMargin(28.f, 20.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Args._Title).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(Warm)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f)
					[
						SAssignNew(Box, SEditableTextBox).Text(Args._Initial).Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
							.MinDesiredWidth(340.f).SelectAllTextWhenFocused(true).ClearKeyboardFocusOnCommit(false).OnTextCommitted(Args._OnDone)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Args._Hint).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)).ColorAndOpacity(Paper)
					]
				]
			];
		}

		TSharedPtr<SEditableTextBox> Box;
	};
}

// --- the hero's side -----------------------------------------------------------------------------------

UAnimalFriendSubsystem* UAnimalFriendSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UAnimalFriendSubsystem>() : nullptr;
}

void UAnimalFriendSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bHooked = false;
}

void UAnimalFriendSubsystem::Deinitialize()
{
	CloseNaming(false, FString());
	if (FoodActor)
	{
		FoodActor->Destroy();
		FoodActor = nullptr;
	}
	Super::Deinitialize();
}

FString UAnimalFriendSubsystem::QuestTarget(int32 Id) const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || Id < 0 || Id >= (int32)Sim->Animals.size())
	{
		return FString();
	}
	const FAnimal& A = Sim->Animals[Id];
	// her own name for the one the story knows (animal.maru.name -> Maru), else the kind
	if (A.NameKey == "animal.maru.name")
	{
		return TEXT("Maru");
	}
	return A.Kind == EKind::Cat ? TEXT("Cat") : TEXT("Dog");
}

FVector UAnimalFriendSubsystem::HeadOf(int32 Id) const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || Id < 0 || Id >= (int32)Sim->Animals.size())
	{
		return FVector::ZeroVector;
	}
	const FAnimal& A = Sim->Animals[Id];
	if (const AAnimalActor* Body = Life->BodyOf(Id))
	{
		static const FName HeadBone(TEXT("head"));
		if (Body->Body && Body->Body->GetBoneIndex(HeadBone) != INDEX_NONE)
		{
			return Body->Body->GetBoneLocation(HeadBone);
		}
	}
	const bool bCat = A.Kind == EKind::Cat;
	return SimToUE(A.Pos) + FVector(FMath::Cos(A.Yaw), FMath::Sin(A.Yaw), 0.0) * (bCat ? 20.0 : 38.0) + FVector(0.0, 0.0, bCat ? 22.0 : 48.0);
}

bool UAnimalFriendSubsystem::InSight(const AHeroCharacter& Hero, int32 Id) const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AnimalFriendSight), false, &Hero);
	if (const AAnimalActor* Body = Life ? Life->BodyOf(Id) : nullptr)
	{
		Q.AddIgnoredActor(Body);
	}
	return !GetWorld()->LineTraceSingleByChannel(Hit, Hero.EyeLocation(), HeadOf(Id), ECC_Visibility, Q);
}

int32 UAnimalFriendSubsystem::Nearest(float Radius, bool bFriendDog) const
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Sim || !Pawn)
	{
		return -1;
	}
	int32 Best = -1;
	double BestD = (double)Radius * Radius;
	for (const FAnimal& A : Sim->Animals)
	{
		if (bFriendDog && (A.Kind != EKind::Dog || !Sim->IsFriend(A.Id)))
		{
			continue;
		}
		const double D = FVector::DistSquared(SimToUE(A.Pos), Pawn->GetActorLocation());
		if (D < BestD)
		{
			BestD = D;
			Best = A.Id;
		}
	}
	return Best;
}

void UAnimalFriendSubsystem::PickTarget(const AHeroCharacter& Hero, const APlayerController& PC)
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld& Sim = *Life->GetSim();
	// the hand is busy with one: it stays the target; the cat on the lap is the one within reach
	if (Hand != EHand::None && HandAnimal >= 0)
	{
		Target = HandAnimal;
		return;
	}
	if (Hero.IsSeated())
	{
		Target = Sim.LapCat >= 0 && Sim.Animals[Sim.LapCat].Friend == EFriend::OnLap ? Sim.LapCat : -1;
		return;
	}
	const FVector Eye = Hero.EyeLocation();
	const FVector Dir = PC.GetControlRotation().Vector();
	const FVector Feet = Hero.GetActorLocation() - FVector(0.0, 0.0, Hero.GetSimpleCollisionHalfHeight());
	Target = -1;
	float BestScore = -1e9f;
	for (const FAnimal& A : Sim.Animals)
	{
		const FVector P = SimToUE(A.Pos);
		const float D = (float)FVector::Dist2D(P, Feet);
		if (D > 600.f || FMath::Abs(P.Z - Feet.Z) > 220.0 || A.Up > 0.f)
		{
			continue;
		}
		const FVector To = (P + FVector(0.0, 0.0, A.Kind == EKind::Cat ? 18.0 : 40.0) - Eye).GetSafeNormal();
		const float Dot = (float)FVector::DotProduct(To, Dir);
		if (Dot < 0.6f && !(D < 120.f && Dot > 0.f))      // looked at, or right by the feet in front
		{
			continue;
		}
		const float Score = Dot * 2.f - D / 600.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Target = A.Id;
		}
	}
}

void UAnimalFriendSubsystem::UpdateTreat(bool bCat)
{
	Treat = NAME_None;
	TreatLikes = -1.f;
	bTreatForCat = bCat;
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv)
	{
		return;
	}
	for (const TPair<FName, int32>& It : Inv->Counts)
	{
		if (It.Value > 0)
		{
			const float L = TreatLike(It.Key, bCat);
			if (L > TreatLikes)
			{
				TreatLikes = L;
				Treat = It.Key;
			}
		}
	}
	if (TreatLikes < 0.f)
	{
		Treat = NAME_None;
	}
}

void UAnimalFriendSubsystem::PickActions(const AHeroCharacter& Hero)
{
	EAction = EKeyAction::None;
	FAction = EKeyAction::None;
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld& Sim = *Life->GetSim();
	if (Target < 0 || Target >= (int32)Sim.Animals.size())
	{
		return;
	}
	const FAnimal& A = Sim.Animals[Target];
	const bool bCat = A.Kind == EKind::Cat;
	const bool bFriend = Sim.IsFriend(Target);
	const bool bGone = A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch || A.Act == EAct::Chase || A.Act == EAct::Hiss;
	const FVector Feet = Hero.GetActorLocation() - FVector(0.0, 0.0, Hero.GetSimpleCollisionHalfHeight());
	const float D = (float)FVector::Dist2D(SimToUE(A.Pos), Feet);
	if (bGone)
	{
		return;
	}
	if (Hero.IsSeated())
	{
		FAction = A.Friend == EFriend::OnLap ? EKeyAction::LapStroke : EKeyAction::None;
		return;
	}
	const bool bWalking = Sim.Walker == Target;
	if (Hand == EHand::None)
	{
		if (Hero.bIsCrouched)
		{
			if (D <= 255.f)
			{
				const bool bKnows = bFriend || A.Sniffed > 0.f;
				const bool bEnough = bCat && A.PetFill >= 1.f;
				const bool bFree = A.Friend == EFriend::None || A.Friend == EFriend::Heel || A.Friend == EFriend::Wait || A.Friend == EFriend::BySeat;
				if (bKnows && D <= 165.f && bFree)
				{
					EAction = bEnough ? EKeyAction::None : EKeyAction::Stroke;
				}
				else if (A.Friend == EFriend::None)
				{
					EAction = EKeyAction::Hand;
				}
			}
		}
		else if (!bCat && bFriend && D <= 450.f)
		{
			const bool bNight = Sim.Hour >= Sim.BedHour || Sim.Hour < Sim.UpHour;
			EAction = bWalking ? EKeyAction::Home : (bNight ? EKeyAction::None : EKeyAction::Walk);
		}
		else if (D <= 300.f && A.Friend == EFriend::None && (!bFriend || bCat))
		{
			EAction = EKeyAction::CrouchHint;
		}
		// a treat: the best thing in the pocket for its kind (looked for once a second), unless bird feed is in the hand (F is the birds')
		const UBirdTameSubsystem* Tame = UBirdTameSubsystem::Get(this);
		const bool bBirdFeed = Tame && !Tame->HeldFeed().IsNone();
		if (!bBirdFeed && D <= 580.f && !Treat.IsNone() && bTreatForCat == bCat && A.Friend != EFriend::SwimAfter
			&& A.Friend != EFriend::Stroked && A.Friend != EFriend::Rolled && A.Friend != EFriend::EatHand && A.Friend != EFriend::ToFood)
		{
			FAction = EKeyAction::Treat;
		}
	}
}

void UAnimalFriendSubsystem::Say(const TCHAR* Key, float Seconds)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, LyudText::Get(Key));
		bPromptUp = true;
		SayLeft = Seconds;
		ShownTarget = -2;       // the action line is built again after it
	}
}

void UAnimalFriendSubsystem::UpdatePrompt()
{
	if (SayLeft > 0.f)
	{
		return;
	}
	const FName ShowTreat = FAction == EKeyAction::Treat ? Treat : FName();
	if (ShownTarget == Target && ShownE == EAction && ShownF == FAction && ShownTreat == ShowTreat)
	{
		return;
	}
	ShownTarget = Target;
	ShownE = EAction;
	ShownF = FAction;
	ShownTreat = ShowTreat;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI)
	{
		return;
	}
	FString Line;
	auto Add = [&Line](const FText& T)
	{
		if (!Line.IsEmpty())
		{
			Line += TEXT("     ");
		}
		Line += T.ToString();
	};
	switch (EAction)
	{
	case EKeyAction::Hand:
	{
		const ULyudPets* Pets = ULyudPets::Get(this);
		Add(LyudText::Get(Pets && Pets->NeedsAsking(Target) ? TEXT("ui.pets.ask") : TEXT("ui.friend.hand")));
		break;
	}
	case EKeyAction::Stroke: Add(LyudText::Get(TEXT("ui.friend.stroke"))); break;
	case EKeyAction::Walk: Add(LyudText::Get(TEXT("ui.friend.walk"))); break;
	case EKeyAction::Home: Add(LyudText::Get(TEXT("ui.friend.home"))); break;
	case EKeyAction::CrouchHint: Add(LyudText::Get(TEXT("ui.friend.crouch"))); break;
	default: break;
	}
	switch (FAction)
	{
	case EKeyAction::Treat: Add(LyudText::Get(TEXT("ui.friend.treat"), { FStringFormatArg(ULyudInventory::ItemName(Treat).ToString()) })); break;
	case EKeyAction::LapStroke: Add(LyudText::Get(TEXT("ui.friend.lap_stroke"))); break;
	default: break;
	}
	if (Line.IsEmpty() && !bPromptUp)
	{
		return;
	}
	UI->SetPrompt(this, FText::FromString(Line));
	bPromptUp = !Line.IsEmpty();
}

bool UAnimalFriendSubsystem::StartHand(EHand Mode, int32 Id)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || !Hero || Id < 0 || Id >= (int32)Sim->Animals.size() || Hand != EHand::None)
	{
		return false;
	}
	if (UHeroReachComponent* Reach = Hero->FindReach())
	{
		if (Reach->IsBusy() || Reach->HasOverrideHold())
		{
			return false;       // the hand is taking or holding something else
		}
	}
	const FAnimal& A = Sim->Animals[Id];
	const bool bCat = A.Kind == EKind::Cat;
	// where the hand will be, told to the animals before they are asked (they come to the palm)
	const FVector Feet = Hero->GetActorLocation() - FVector(0.0, 0.0, Hero->GetSimpleCollisionHalfHeight());
	const FVector To = (SimToUE(A.Pos) - Feet).GetSafeNormal2D();
	const double Height = Hero->bIsCrouched ? (bCat ? 26.0 : 42.0) : (bCat ? 50.0 : 66.0);
	Life->SetPlayerHand(true, Feet + To * 52.0 + FVector(0.0, 0.0, Height), Mode == EHand::Food);
	bool bOk = false;
	switch (Mode)
	{
	case EHand::Offer:
	{
		// somebody's dog on a leash: the owner is asked first ("may I stroke it?", ULyudPets) and answers aloud
		ULyudPets* Pets = ULyudPets::Get(this);
		const bool bRefused = Pets && Pets->NeedsAsking(Id) && !Pets->AskToStroke(Id);
		bOk = !bRefused && Life->OfferHand(Id);
		HandLeft = bOk ? 18.f : 2.4f;
		if (!bOk && !bRefused)
		{
			Say(TEXT("ui.friend.not_yet"));
		}
		break;
	}
	case EHand::Stroke:
	case EHand::Lap:
		bOk = Life->StrokeAnimal(Id);
		HandLeft = Mode == EHand::Lap ? 3.2f : 11.f;
		if (!bOk)
		{
			Life->SetPlayerHand(false);
			return false;
		}
		break;
	case EHand::Food:
		bOk = Life->OfferFood(Id, TreatLikes);
		HandLeft = bOk ? 22.f : 2.4f;
		FoodItem = Treat;
		if (!bOk)
		{
			Say(TEXT("ui.friend.not_yet"));
		}
		break;
	default:
		return false;
	}
	Hand = Mode;
	HandAnimal = Id;
	HandTime = 0.f;
	bHandHeld = bOk;
	if (Mode == EHand::Food)
	{
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			FoodActor = UI->SpawnHeld(FoodItem, Hero->EyeLocation());
			if (FoodActor)
			{
				FoodActor->SetActorEnableCollision(false);
				FoodActor->SetActorScale3D(FVector(0.6));
			}
		}
	}
	return true;
}

void UAnimalFriendSubsystem::EndHand()
{
	if (Hand == EHand::None)
	{
		return;
	}
	if (UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this))
	{
		if (Hand == EHand::Stroke)
		{
			Life->EndStroke(HandAnimal);
		}
		Life->SetPlayerHand(false);
	}
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UHeroReachComponent* Reach = Hero->FindReach())
		{
			Reach->ClearOverrideHold();
		}
	}
	if (FoodActor)
	{
		FoodActor->Destroy();
		FoodActor = nullptr;
	}
	Hand = EHand::None;
	HandAnimal = -1;
	FoodItem = NAME_None;
}

void UAnimalFriendSubsystem::TickHand(AHeroCharacter& Hero, const APlayerController& PC, float DeltaTime)
{
	if (Hand == EHand::None)
	{
		return;
	}
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld& Sim = *Life->GetSim();
	UHeroReachComponent* Reach = Hero.FindReach();
	if (HandAnimal < 0 || HandAnimal >= (int32)Sim.Animals.size() || !Reach)
	{
		EndHand();
		return;
	}
	const FAnimal& A = Sim.Animals[HandAnimal];
	const bool bCat = A.Kind == EKind::Cat;
	HandTime += DeltaTime;
	HandLeft -= DeltaTime;
	// over: the time is up, the animal has gone on to something else, the hero got up from a stroke or walked off
	bool bOver = HandLeft <= 0.f || Reach->IsBusy();
	if (bHandHeld && HandTime > 0.4f)
	{
		switch (Hand)
		{
		case EHand::Offer: bOver |= A.Friend != EFriend::ToHand && A.Friend != EFriend::AtHand; break;
		case EHand::Stroke: bOver |= (A.Friend != EFriend::Stroked && A.Friend != EFriend::Rolled) || !Hero.bIsCrouched; break;
		case EHand::Food: bOver |= A.Friend != EFriend::ToFood && A.Friend != EFriend::AtHand && A.Friend != EFriend::EatHand; break;
		case EHand::Lap: bOver |= A.Friend != EFriend::OnLap || !Hero.IsSeated(); break;
		default: break;
		}
	}
	if (bOver)
	{
		EndHand();
		return;
	}
	// where the palm goes this frame (world)
	const FVector Feet = Hero.GetActorLocation() - FVector(0.0, 0.0, Hero.GetSimpleCollisionHalfHeight());
	const FVector At = SimToUE(A.Pos);
	const FVector To = (At - Feet).GetSafeNormal2D();
	const FVector AFwd(FMath::Cos(A.Yaw), FMath::Sin(A.Yaw), 0.0);
	FVector Palm, Finger = To, Normal = FVector::DownVector;
	float Curl = 0.3f;
	bool bTellSim = true;
	switch (Hand)
	{
	case EHand::Stroke:
	{
		// on its head, sliding back along the neck and forward again (a rolled-over one: on the belly)
		const float Slide = (bCat ? 7.f : 11.f) * (1.f + FMath::Sin(HandTime * 3.1f - 1.57f));
		const FVector Head = HeadOf(HandAnimal);
		Palm = A.Friend == EFriend::Rolled ? At + FVector(0.0, 0.0, bCat ? 16.0 : 30.0) + AFwd * (Slide - 8.f)
			: Head + FVector(0.0, 0.0, bCat ? 5.0 : 7.0) - AFwd * Slide;
		Curl = 0.22f;
		bTellSim = false;
		break;
	}
	case EHand::Lap:
	{
		const float Slide = 6.f * FMath::Sin(HandTime * 3.1f);
		Palm = At + FVector(0.0, 0.0, 13.0) + AFwd * Slide;
		Finger = AFwd;
		Curl = 0.22f;
		bTellSim = false;
		break;
	}
	case EHand::Food:
		Palm = Feet + To * 52.0 + FVector(0.0, 0.0, Hero.bIsCrouched ? (bCat ? 26.0 : 42.0) : (bCat ? 50.0 : 66.0));
		Normal = FVector::UpVector;
		Curl = 0.15f;
		break;
	default:
		// held out to be sniffed: the back of the fingers first, low
		Palm = Feet + To * 52.0 + FVector(0.0, 0.0, bCat ? 26.0 : 42.0);
		Normal = (FVector::DownVector + To * 0.35).GetSafeNormal();
		Finger = (To + FVector(0.0, 0.0, -0.25)).GetSafeNormal();
		Curl = 0.45f;
		break;
	}
	if (bTellSim)
	{
		Life->SetPlayerHand(true, Palm, Hand == EHand::Food);
	}
	// the hold is placed in the look's frame (the eyes, the look's yaw): the world point is put into it every frame
	const FTransform Frame(FRotator(0.0, PC.GetControlRotation().Yaw, 0.0), Hero.EyeLocation());
	FHeroHoldPose Pose;
	Pose.Pos = Frame.InverseTransformPositionNoScale(Palm);
	Pose.FingerDir = Frame.InverseTransformVectorNoScale(Finger);
	Pose.PalmNormal = Frame.InverseTransformVectorNoScale(Normal);
	Pose.Curl = Curl;
	Pose.PitchFollow = 0.f;
	Pose.BlendTime = 0.45f;
	Reach->SetOverrideHold(Pose);
	if (FoodActor)
	{
		FoodActor->SetActorLocation(Reach->HoldPalm() + Reach->HoldNormal() * 2.5);
		FoodActor->SetActorHiddenInGame(Reach->HoldWeight() < 0.6f);
	}
}

void UAnimalFriendSubsystem::TickWalk(const AHeroCharacter& Hero)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld& Sim = *Life->GetSim();
	if (Sim.Walker < 0 || Sim.Walker >= (int32)Sim.Animals.size())
	{
		return;
	}
	const FAnimal& A = Sim.Animals[Sim.Walker];
	const float D = (float)FVector::Dist2D(SimToUE(A.Pos), Hero.GetActorLocation());
	if (A.Friend != EFriend::Heel || D > 900.f)
	{
		return;
	}
	// the places they came to together (a quest's "walk her to ...")
	if (const ULyudQuests* Q = ULyudQuests::Get(this))
	{
		for (const FName Place : Q->InsidePlaces())
		{
			if (!WalkReached.Contains(Place))
			{
				WalkReached.Add(Place);
				ULyudQuests::Notify(this, TEXT("animal_walk_reach"), Place.ToString());
			}
		}
	}
	// somebody near notices the dog
	if (RemarkCooldown <= 0.f)
	{
		RemarkCooldown = 12.f;
		if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
		{
			AActor* Speaker = nullptr;
			const int32 Person = Talk->NearestSpeaker(Speaker);
			if (Speaker && FVector::Dist2D(Speaker->GetActorLocation(), Hero.GetActorLocation()) < 650.0 && Talk->SayEvent(Speaker, TEXT("dog_walk"), Person))
			{
				RemarkCooldown = 150.f;
			}
		}
	}
}

void UAnimalFriendSubsystem::Tick(float DeltaTime)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (SayLeft > 0.f)
	{
		SayLeft -= DeltaTime;
	}
	if (!Sim || !Hero || !PC || CVarFriends.GetValueOnGameThread() == 0 || Hero->IsHidden())
	{
		EndHand();
		Target = -1;
		EAction = FAction = EKeyAction::None;
		UpdatePrompt();
		return;
	}
	if (!bHooked)
	{
		bHooked = true;
		Life->OnAnimalEvent.AddUniqueDynamic(this, &UAnimalFriendSubsystem::OnAnimalEvent);
	}
	RemarkCooldown = FMath::Max(0.f, RemarkCooldown - DeltaTime);
	const ULyudGameMenuSubsystem* Menu = GetWorld()->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bInUI = (UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || IsNaming();
	if (bInUI)
	{
		EndHand();
		Target = -1;
		EAction = FAction = EKeyAction::None;
		UpdatePrompt();
		return;
	}
	PickTarget(*Hero, *PC);
	TreatTimer -= DeltaTime;
	if (Target >= 0 && Target < (int32)Sim->Animals.size())
	{
		const bool bCat = Sim->Animals[Target].Kind == EKind::Cat;
		if (TreatTimer <= 0.f || bCat != bTreatForCat)
		{
			TreatTimer = 1.f;
			UpdateTreat(bCat);
		}
	}
	PickActions(*Hero);
	if (PC->WasInputKeyJustPressed(EKeys::E) && TakesE() && InSight(*Hero, Target))
	{
		switch (EAction)
		{
		case EKeyAction::Hand: StartHand(EHand::Offer, Target); break;
		case EKeyAction::Stroke: StartHand(EHand::Stroke, Target); break;
		case EKeyAction::Walk:
			Life->Hail(Target);
			if (!Life->StartWalk(Target))
			{
				Say(TEXT("ui.friend.not_now"));
			}
			break;
		case EKeyAction::Home: Life->EndWalk(true); break;
		default: break;
		}
	}
	else if (PC->WasInputKeyJustPressed(EKeys::F) && FAction != EKeyAction::None)
	{
		if (FAction == EKeyAction::LapStroke)
		{
			StartHand(EHand::Lap, Target);
		}
		else if (InSight(*Hero, Target))
		{
			StartHand(EHand::Food, Target);
		}
	}
	TickHand(*Hero, *PC, DeltaTime);
	SlowTimer -= DeltaTime;
	if (SlowTimer <= 0.f)
	{
		SlowTimer = 1.f;
		TickWalk(*Hero);
	}
	UpdatePrompt();
}

void UAnimalFriendSubsystem::OnAnimalEvent(int32 AnimalId, FName Event)
{
	static const FName NSniff(TEXT("HandSniff")), NRefuse(TEXT("HandRefuse")), NStroked(TEXT("Stroked")), NEnough(TEXT("Enough")), NAte(TEXT("Ate")),
		NFriend(TEXT("Befriended")), NWalk(TEXT("WalkStart")), NWalkEnd(TEXT("WalkEnd")), NWait(TEXT("WaitDoor")), NLap(TEXT("LapUp"));
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || AnimalId < 0 || AnimalId >= (int32)Sim->Animals.size())
	{
		return;
	}
	const FAnimal& A = Sim->Animals[AnimalId];
	const bool bCat = A.Kind == EKind::Cat;
	ULyudAnimalFriends* Book = ULyudAnimalFriends::Get(this);
	const FString QTarget = QuestTarget(AnimalId);
	if (Event == NSniff)
	{
		ULyudQuests::Notify(this, TEXT("animal_sniffed"), QTarget);
	}
	else if (Event == NRefuse)
	{
		if (AnimalId == HandAnimal && Hand == EHand::Food && bHandHeld)
		{
			Say(bCat ? TEXT("ui.friend.wont_eat_cat") : TEXT("ui.friend.wont_eat_dog"));
		}
	}
	else if (Event == NStroked)
	{
		if (Book)
		{
			Book->Count(AnimalId, TEXT("Strokes"));
		}
		ULyudQuests::Notify(this, TEXT("animal_stroked"), QTarget);
	}
	else if (Event == NEnough)
	{
		Say(TEXT("ui.friend.enough"));
	}
	else if (Event == NAte)
	{
		// the treat it was given leaves the pocket now
		if (AnimalId == HandAnimal && !FoodItem.IsNone())
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				Inv->Remove(FoodItem, 1);
			}
			TreatTimer = 0.f;
		}
		if (Book)
		{
			Book->Count(AnimalId, TEXT("Treats"));
		}
		ULyudQuests::Notify(this, TEXT("animal_fed"), QTarget);
	}
	else if (Event == NFriend)
	{
		FAnimalInfo Info;
		Life->GetAnimal(AnimalId, Info);
		FLyudAnimalFriend F;
		F.AnimalId = AnimalId;
		F.bCat = bCat;
		F.Breed = Info.Breed;
		F.bFemale = A.bFemale;
		F.NameKey = Info.NameKey;
		F.Name = UTF8_TO_TCHAR(A.Name.c_str());
		F.How = A.Friend == EFriend::EatHand || (AnimalId == HandAnimal && Hand == EHand::Food) ? TEXT("food") : TEXT("hand");
		if (Book && Book->NoteFriend(this, F, Info.Location))
		{
			const bool bNamed = !Info.Name.IsEmpty();
			const TCHAR* Kind = bCat ? (A.bFemale ? TEXT("@animal.kind.cat_f") : TEXT("@animal.kind.cat_m")) : (A.bFemale ? TEXT("@animal.kind.dog_f") : TEXT("@animal.kind.dog_m"));
			const FString Who = bNamed ? (Info.NameKey.IsEmpty() ? Info.Name : FString(TEXT("@")) + Info.NameKey) : FString(Kind);
			if (ULyudQuests* Q = ULyudQuests::Get(this))
			{
				if (bNamed)
				{
					Q->AddNote(TEXT("journal.friend.new_named"), { Who });
				}
				else
				{
					Q->AddNote(bCat ? (A.bFemale ? TEXT("journal.friend.new_cat_f") : TEXT("journal.friend.new_cat_m")) : (A.bFemale ? TEXT("journal.friend.new_dog_f") : TEXT("journal.friend.new_dog_m")));
				}
			}
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(bNamed ? TEXT("ui.friend.toast_named") : TEXT("ui.friend.toast"),
					{ FStringFormatArg(bNamed ? Info.Name : LyudText::Get(FString(Kind).RightChop(1)).ToString()) }));
			}
		}
		ULyudQuests::Notify(this, TEXT("animal_friend"), QTarget);
	}
	else if (Event == NWalk)
	{
		WalkReached.Reset();
		if (const ULyudQuests* Q = ULyudQuests::Get(this))
		{
			WalkReached = Q->InsidePlaces();        // where they set off from is not a place they came to
		}
		if (Book)
		{
			Book->Count(AnimalId, TEXT("Walks"));
		}
		ULyudQuests::Notify(this, TEXT("animal_walk"), QTarget);
	}
	else if (Event == NWalkEnd)
	{
		ULyudQuests::Notify(this, TEXT("animal_home"), QTarget);
		const bool bNight = Sim->Hour >= Sim->BedHour || Sim->Hour < Sim->UpHour;
		Say(bNight ? TEXT("ui.friend.bed") : TEXT("ui.friend.sent_home"), 3.5f);
	}
	else if (Event == NWait)
	{
		Say(TEXT("ui.friend.waits"), 3.5f);
	}
	else if (Event == NLap)
	{
		ULyudQuests::Notify(this, TEXT("animal_lap"), QTarget);
	}
}

bool UAnimalFriendSubsystem::Do(FName What, int32 Id)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim)
	{
		return false;
	}
	if (What == TEXT("crouch") || What == TEXT("stand"))
	{
		// (headless checks: C is a key)
		AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (Hero)
		{
			Hero->SetCrouchToggled(What == TEXT("crouch"));
		}
		return Hero != nullptr;
	}
	if (Id < 0)
	{
		Id = Target >= 0 ? Target : Nearest(800.f);
	}
	if (Id < 0 || Id >= (int32)Sim->Animals.size())
	{
		return false;
	}
	if (What == TEXT("hand"))
	{
		return StartHand(EHand::Offer, Id);
	}
	if (What == TEXT("stroke"))
	{
		return StartHand(Sim->Animals[Id].Friend == EFriend::OnLap ? EHand::Lap : EHand::Stroke, Id);
	}
	if (What == TEXT("feed"))
	{
		UpdateTreat(Sim->Animals[Id].Kind == EKind::Cat);
		return !Treat.IsNone() && StartHand(EHand::Food, Id);
	}
	if (What == TEXT("walk"))
	{
		return Life->StartWalk(Id);
	}
	if (What == TEXT("home"))
	{
		Life->EndWalk(true);
		return true;
	}
	return false;
}

void UAnimalFriendSubsystem::SetName(int32 Id, const FString& InName)
{
	FString Name = InName.TrimStartAndEnd().Left(18);
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	const LyudAnimals::FWorld* Sim = Life ? Life->GetSim() : nullptr;
	if (!Sim || Id < 0 || Id >= (int32)Sim->Animals.size() || !Sim->Animals[Id].NameKey.empty())
	{
		return;     // one that has a name of its own keeps it
	}
	Life->SetAnimalName(Id, Name);
	if (ULyudAnimalFriends* Book = ULyudAnimalFriends::Get(this))
	{
		Book->SetName(Id, Name);
	}
	if (!Name.IsEmpty())
	{
		if (ULyudQuests* Q = ULyudQuests::Get(this))
		{
			Q->AddNote(TEXT("journal.friend.named"), { Name });
		}
		ULyudQuests::Notify(this, TEXT("animal_named"), QuestTarget(Id));
	}
}

void UAnimalFriendSubsystem::BeginNaming(int32 Id)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const ULyudAnimalFriends* Book = ULyudAnimalFriends::Get(this);
	const FLyudAnimalFriend* F = Book ? Book->Find(Id) : nullptr;
	if (!Viewport || !PC || !F || !F->NameKey.IsEmpty() || NamePopup.IsValid())
	{
		return;
	}
	NamingId = Id;
	TWeakObjectPtr<UAnimalFriendSubsystem> Weak(this);
	TSharedRef<SAnimalNamePopup> Popup = SNew(SAnimalNamePopup)
		.Title(LyudText::Get(F->bCat ? TEXT("ui.friend.name_title_cat") : TEXT("ui.friend.name_title_dog")))
		.Hint(LyudText::Get(TEXT("ui.friend.name_hint")))
		.Initial(FText::FromString(F->Name))
		.OnDone_Lambda([Weak](const FText& Text, ETextCommit::Type How)
		{
			if (UAnimalFriendSubsystem* Self = Weak.Get())
			{
				if (How == ETextCommit::OnEnter || How == ETextCommit::OnCleared)
				{
					Self->CloseNaming(How == ETextCommit::OnEnter, Text.ToString());
				}
			}
		});
	NamePopup = Popup;
	Viewport->AddViewportWidgetContent(Popup, 80);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->Lock(true);
		UI->SetExternalScreen(this);
	}
	// the keys go to the box alone while it is up (the game's own keys are polled: M, J, Tab must not fire while typing)
	PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(Popup->Box).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	FSlateApplication::Get().SetKeyboardFocus(Popup->Box, EFocusCause::SetDirectly);
}

void UAnimalFriendSubsystem::CloseNaming(bool bKeep, const FString& Name)
{
	if (!NamePopup.IsValid())
	{
		return;
	}
	const int32 Id = NamingId;
	NamingId = -1;
	// (taken off the viewport on the next tick: this runs inside the box's own commit)
	TSharedPtr<SWidget> Popup = NamePopup;
	NamePopup.Reset();
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Popup.ToSharedRef());
	}
	if (ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(this) : nullptr)
	{
		UI->SetExternalScreen(nullptr);
		UI->Lock(false);
	}
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
	if (bKeep && Id >= 0)
	{
		SetName(Id, Name);
	}
}

// --- console and developer mode -------------------------------------------------------------------------

namespace
{
	int32 FindMaru(const LyudAnimals::FWorld& Sim)
	{
		for (const FAnimal& A : Sim.Animals)
		{
			if (A.NameKey == "animal.maru.name")
			{
				return A.Id;
			}
		}
		return -1;
	}

	void FriendsDo(const TArray<FString>& Args, UWorld* World)
	{
		UAnimalFriendSubsystem* Friends = UAnimalFriendSubsystem::Get(World);
		if (!Friends || Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("lyud.Pets.Do hand|stroke|feed|walk|home [id]"));
			return;
		}
		const int32 Id = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1;
		const bool bOk = Friends->Do(FName(*Args[0]), Id);
		UE_LOG(LogTemp, Display, TEXT("AnimalFriends: %s %d: %s"), *Args[0], Id, bOk ? TEXT("ok") : TEXT("no"));
	}

	void FriendsBefriend(const TArray<FString>& Args, UWorld* World)
	{
		UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(World);
		UAnimalFriendSubsystem* Friends = UAnimalFriendSubsystem::Get(World);
		if (!Life || !Life->GetSim() || !Friends)
		{
			return;
		}
		const int32 Id = Args.Num() > 0 ? (Args[0] == TEXT("maru") ? FindMaru(*Life->GetSim()) : FCString::Atoi(*Args[0])) : Friends->Nearest(3000.f);
		Life->Befriend(Id);
		UE_LOG(LogTemp, Display, TEXT("AnimalFriends: befriended %d: %s"), Id, *Life->Describe(Id));
	}

	void FriendsName(const TArray<FString>& Args, UWorld* World)
	{
		UAnimalFriendSubsystem* Friends = UAnimalFriendSubsystem::Get(World);
		if (Friends && Args.Num() == 1)
		{
			Friends->BeginNaming(FCString::Atoi(*Args[0]));
		}
		else if (Friends && Args.Num() >= 2)
		{
			Friends->SetName(FCString::Atoi(*Args[0]), Args[1]);
		}
	}

	void FriendsReport(const TArray<FString>&, UWorld* World)
	{
		const ULyudAnimalFriends* Book = ULyudAnimalFriends::Get(World);
		const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(World);
		const UAnimalFriendSubsystem* Friends = UAnimalFriendSubsystem::Get(World);
		UE_LOG(LogTemp, Display, TEXT("AnimalFriends: %d friends, target %d, walker %d, lap cat %d"), Book ? Book->Friends.Num() : 0,
			Friends ? Friends->GetTarget() : -1, Life ? Life->Walker() : -1, Life ? Life->LapCat() : -1);
		if (Book)
		{
			for (const FLyudAnimalFriend& F : Book->Friends)
			{
				UE_LOG(LogTemp, Display, TEXT("  %d %s \"%s\" met %s at %s (%s), strokes %d treats %d walks %d: %s"), F.AnimalId, *F.Breed.ToString(),
					*ULyudAnimalFriends::DisplayName(F).ToString(), *F.Met.ToString(), *F.Place.ToString(), *F.How.ToString(), F.Strokes, F.Treats, F.Walks,
					Life ? *Life->Describe(F.AnimalId) : TEXT(""));
			}
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GFriendsDo(TEXT("lyud.Pets.Do"), TEXT("lyud.Pets.Do hand|stroke|feed|walk|home [id]: what E and F do with an animal (default: the target, else the nearest); crouch|stand: the hero's C."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FriendsDo));
static FAutoConsoleCommandWithWorldAndArgs GFriendsBefriend(TEXT("lyud.Pets.Befriend"), TEXT("lyud.Pets.Befriend [id|maru]: makes an animal a friend at once (default: the nearest)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FriendsBefriend));
static FAutoConsoleCommandWithWorldAndArgs GFriendsName(TEXT("lyud.Pets.Name"), TEXT("lyud.Pets.Name <id> [name]: names a friend (without a name: opens the box to type it)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FriendsName));
static FAutoConsoleCommandWithWorldAndArgs GFriendsReport(TEXT("lyud.Pets.Report"), TEXT("lyud.Pets.Report: the friends' book and what the hand is doing."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FriendsReport));

static LyudDev::FAutoAction GDevFriendMaru(TEXT("pets.maru"), TEXT("pets"), TEXT("dev.action.pets.maru"), [](UWorld* W)
{
	FriendsBefriend({ TEXT("maru") }, W);
});
static LyudDev::FAutoAction GDevFriendNear(TEXT("pets.near"), TEXT("pets"), TEXT("dev.action.pets.near"), [](UWorld* W)
{
	FriendsBefriend({}, W);
});
static LyudDev::FAutoAction GDevFriendTreats(TEXT("pets.treats"), TEXT("pets"), TEXT("dev.action.pets.treats"), [](UWorld* W)
{
	if (ULyudInventory* Inv = ULyudInventory::Get(W))
	{
		Inv->Add(TEXT("smoked_fish"), 3);
		Inv->Add(TEXT("sandwich"), 3);
		Inv->Add(TEXT("chocolate"), 1);
	}
});
static LyudDev::FAutoAction GDevFriendWalk(TEXT("pets.walk"), TEXT("pets"), TEXT("dev.action.pets.walk"), [](UWorld* W)
{
	UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(W);
	if (Life && Life->GetSim())
	{
		const int32 Maru = FindMaru(*Life->GetSim());
		Life->Befriend(Maru);
		if (Life->Walker() == Maru)
		{
			Life->EndWalk(true);
		}
		else
		{
			Life->StartWalk(Maru);
		}
	}
}, [](UWorld* W)
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(W);
	return Life && Life->Walker() >= 0;
});
static LyudDev::FAutoTeleport GDevTpMaru(TEXT("pets_maru"), TEXT("pets"), TEXT("dev.tp.pets_maru"), [](UWorld* W, FTransform& At)
{
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(W);
	const int32 Maru = Life && Life->GetSim() ? FindMaru(*Life->GetSim()) : -1;
	FAnimalInfo Info;
	if (Maru < 0 || !Life->GetAnimal(Maru, Info))
	{
		return false;
	}
	// two metres in front of her, facing her
	const FVector Fwd = FRotator(0.0, Info.Yaw, 0.0).Vector();
	At = FTransform((-Fwd).Rotation(), Info.Location + Fwd * 200.0);
	return true;
});
