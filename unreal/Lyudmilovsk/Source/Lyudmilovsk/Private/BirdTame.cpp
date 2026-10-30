#include "BirdTame.h"

#include "BirdFeedItem.h"
#include "Birds/BirdBook.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "HeroReachComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "StarlingMurmuration.h"
#include "Audio/LyudSoundscape.h"
#include "UI/LyudGameScreen.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

// the wild birds (bird-life thread) and playing as a bird (bird-flight thread): wired in when their code is in the build
#if __has_include("Birds/BirdLife.h")
#include "Birds/BirdLife.h"
#define LYUD_HAS_BIRDLIFE 1
#else
#define LYUD_HAS_BIRDLIFE 0
#endif
#if __has_include("Birds/BirdFlightSubsystem.h")
#include "Birds/BirdFlightSubsystem.h"
#define LYUD_HAS_BIRDFLIGHT 1
#else
#define LYUD_HAS_BIRDFLIGHT 0
#endif

const FName UBirdTameSubsystem::FeedId(TEXT("bird_feed"));

namespace
{
	/** How long the bird pecks from the palm before the view goes into it, s. */
	constexpr float EatSeconds = 2.4f;

	TAutoConsoleVariable<int32> CVarTame(TEXT("lyud.Tame"), 1,
		TEXT("Befriending birds with feed (hold out the hand, crouch). 0 switches it off: no hand, no offer to the birds."));

	/** The open palm held out with the seeds: an arm's length in front, a little right, below the eyes, toward the bird;
	 *  it dips a little with a look down (not much: a palm pulled in toward the shoes offers nothing). */
	FHeroHoldPose OfferPose()
	{
		FHeroHoldPose P;
		P.Pos = FVector(54.f, 8.f, -30.f);
		P.FingerDir = FVector(1.f, -0.1f, 0.05f);
		P.PalmNormal = FVector(-0.05f, -0.1f, 1.f);
		// the fingers as the animation holds them (relaxed, a little cupped): any fold on top bent them up into a "stop"
		P.Curl = 0.f;
		P.PitchFollow = 0.25f;
		P.BlendTime = 0.8f;
		return P;
	}

	/** The bag carried at the right hip, gripped across its waist, the palm on its broad side. */
	FHeroHoldPose CarryPose()
	{
		FHeroHoldPose P;
		P.Pos = FVector(16.f, 25.f, -72.f);
		P.FingerDir = FVector(1.f, 0.f, -0.35f);
		P.PalmNormal = FVector(0.f, -1.f, 0.f);
		P.Curl = 1.f;
		P.PitchFollow = 0.f;
		P.BlendTime = 0.6f;
		// the bag in the palm's frame (X the fingers, Z out of the palm): its thickness along Z, its width along X
		FReachPrim Box;
		Box.Kind = FReachPrim::EKind::Box;
		Box.Xf = FTransform(FVector(1.5f, 0.f, ABirdFeedBag::HalfDepth + 1.7f));
		Box.Extent = FVector(ABirdFeedBag::HalfWidth, ABirdFeedBag::HalfHeight, ABirdFeedBag::HalfDepth);
		P.Shape.Prims.Add(Box);
		return P;
	}

	/** "HerringGull" -> "herring_gull" (the string tables' bird.<id>.name). */
	FString Snake(const FString& Camel)
	{
		FString Out;
		for (int32 I = 0; I < Camel.Len(); ++I)
		{
			const TCHAR C = Camel[I];
			if (FChar::IsUpper(C) && I > 0)
			{
				Out.AppendChar(TEXT('_'));
			}
			Out.AppendChar(FChar::ToLower(C));
		}
		return Out;
	}

	/** The palm's centre on the skin from the hand's bones (as HandReach measures it), world. */
	bool PalmFromBones(const AHeroCharacter* Hero, FVector& OutPalm)
	{
		const USkeletalMeshComponent* Body = Hero ? Hero->GetMesh() : nullptr;
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r"));
		const FVector Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		OutPalm = Wrist + (Knuckle - Wrist) * 0.6f;
		return true;
	}
}

namespace
{
	/** The right palm's frame from the hand's bones (X the fingers, Z out of the palm the way Normal points, origin the palm's
	 *  middle on the bones), world; false without the bones. */
	bool HandPalm(const USkeletalMeshComponent* Body, const FVector& Normal, const FVector& Finger, FTransform& Out)
	{
		if (!Body || Body->GetBoneIndex(TEXT("hand_r")) == INDEX_NONE || Body->GetBoneIndex(TEXT("middle_01_r")) == INDEX_NONE)
		{
			return false;
		}
		const FVector Wrist = Body->GetBoneLocation(TEXT("hand_r")), Knuckle = Body->GetBoneLocation(TEXT("middle_01_r"));
		const FVector Fd = (Knuckle - Wrist).GetSafeNormal(UE_SMALL_NUMBER, Finger);
		FVector Across = Body->GetBoneLocation(TEXT("thumb_01_r")) - Body->GetBoneLocation(TEXT("pinky_01_r"));
		FVector Pn = FVector::CrossProduct(Across, Fd).GetSafeNormal(UE_SMALL_NUMBER, Normal);
		// out of the palm is the side the thumb tip is on (as HandReach measures it), whatever way the hand is turned now
		const FVector Palm = Wrist + (Knuckle - Wrist) * 0.6f;
		if (Body->GetBoneIndex(TEXT("thumb_03_r")) != INDEX_NONE && FVector::DotProduct(Body->GetBoneLocation(TEXT("thumb_03_r")) - Palm, Pn) < 0.f)
		{
			Pn = -Pn;
		}
		Out = FTransform(FRotationMatrix::MakeFromZX(Pn, Fd).ToQuat(), Palm);
		return true;
	}
}

/** Bottom left: what is in the hand, how much is left and what the keys do with it. */
class SLyudHeldItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudHeldItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot
		[
			SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(36.f, 0.f, 0.f, 36.f)).Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f))
				.Padding(FMargin(10.f, 8.f, 16.f, 8.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(SBox).WidthOverride(48.f).HeightOverride(48.f)
						[
							SAssignNew(Icon, SImage)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(Title, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.7f))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SAssignNew(Hint, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.8f))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
						[
							SAssignNew(Meters, SVerticalBox).Visibility(EVisibility::Collapsed)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SAssignNew(BirdLabel, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.7f))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								Meter(AlertLabel, Alert, FLinearColor(0.9f, 0.3f, 0.2f))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								Meter(TrustLabel, Trust, FLinearColor(0.4f, 0.85f, 0.4f))
							]
						]
					]
				]
			]
		];
	}

	/** The bird the palm is held out to: its name, how alarmed it is and how much it trusts the hero (0..1); Bird empty hides it. */
	void SetBird(const FText& Bird, float InAlert, float InTrust)
	{
		Meters->SetVisibility(Bird.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
		BirdLabel->SetText(Bird);
		AlertLabel->SetText(LyudText::Get(TEXT("ui.tame.alert")));
		TrustLabel->SetText(LyudText::Get(TEXT("ui.tame.trust")));
		Alert->SetPercent(FMath::Clamp(InAlert, 0.f, 1.f));
		Trust->SetPercent(FMath::Clamp(InTrust, 0.f, 1.f));
	}

	void Set(const FSlateBrush* Brush, const FText& InTitle, const FText& InHint)
	{
		Icon->SetImage(Brush);
		Title->SetText(InTitle);
		Hint->SetText(InHint);
	}

private:
	static TSharedRef<SWidget> Meter(TSharedPtr<STextBlock>& Label, TSharedPtr<SProgressBar>& Bar, const FLinearColor& Color)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(80.f)
				[
					SAssignNew(Label, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.8f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(140.f).HeightOverride(7.f)
				[
					SAssignNew(Bar, SProgressBar).FillColorAndOpacity(Color).BarFillType(EProgressBarFillType::LeftToRight)
				]
			];
	}

	TSharedPtr<SVerticalBox> Meters;
	TSharedPtr<STextBlock> BirdLabel, AlertLabel, TrustLabel;
	TSharedPtr<SProgressBar> Alert, Trust;
	TSharedPtr<SImage> Icon;
	TSharedPtr<STextBlock> Title;
	TSharedPtr<STextBlock> Hint;
};

UBirdTameSubsystem* UBirdTameSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UBirdTameSubsystem>() : nullptr;
}

void UBirdTameSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Reset();
}

void UBirdTameSubsystem::Deinitialize()
{
	UnbindBirds();
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Hud)
		{
			Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		}
	}
	Hud.Reset();
	Super::Deinitialize();
}

void UBirdTameSubsystem::Reset()
{
	if (CallId >= 0)
	{
		CancelSummon();
	}
	bWantOffer = false;
	Phase = EPhase::Idle;
	PhaseTime = 0.f;
	Focus = -1;
	TellBirds(false);
	if (Bag)
	{
		Bag->Destroy();
		Bag = nullptr;
	}
	if (Handful)
	{
		Handful->Destroy();
		Handful = nullptr;
	}
}

// ------------------------------------------------------------------ the birds' side

void UBirdTameSubsystem::BindBirds()
{
#if LYUD_HAS_BIRDLIFE
	if (bBound)
	{
		return;
	}
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		Life->OnBirdNotice.AddDynamic(this, &UBirdTameSubsystem::HandleNotice);
		Life->OnBirdTurn.AddDynamic(this, &UBirdTameSubsystem::HandleTurn);
		Life->OnBirdWatch.AddDynamic(this, &UBirdTameSubsystem::HandleWatch);
		Life->OnBirdCall.AddDynamic(this, &UBirdTameSubsystem::HandleCall);
		Life->OnBirdApproach.AddDynamic(this, &UBirdTameSubsystem::HandleApproach);
		Life->OnBirdEat.AddDynamic(this, &UBirdTameSubsystem::HandleEat);
		Life->OnBirdFlee.AddDynamic(this, &UBirdTameSubsystem::HandleFlee);
		bBound = true;
	}
#endif
#if LYUD_HAS_BIRDFLIGHT
	if (UBirdFlightSubsystem* Flight = UBirdFlightSubsystem::Get(this); Flight && !bFlightBound)
	{
		Flight->OnReturnedToHero.AddUObject(this, &UBirdTameSubsystem::HandleReturnedToHero);
		bFlightBound = true;
	}
#endif
}

void UBirdTameSubsystem::UnbindBirds()
{
#if LYUD_HAS_BIRDLIFE
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		Life->OnBirdNotice.RemoveAll(this);
		Life->OnBirdTurn.RemoveAll(this);
		Life->OnBirdWatch.RemoveAll(this);
		Life->OnBirdCall.RemoveAll(this);
		Life->OnBirdApproach.RemoveAll(this);
		Life->OnBirdEat.RemoveAll(this);
		Life->OnBirdFlee.RemoveAll(this);
	}
#endif
	bBound = false;
}

void UBirdTameSubsystem::TellBirds(bool bOn)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
#if LYUD_HAS_BIRDLIFE
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && (bOn || bTold))
	{
		// every frame while the palm is out: the birds read the hand's speed from the successive points
		Life->SetOffer(BirdOfferAt.IsZero() ? OfferAt : BirdOfferAt, FeedId, bOn);
	}
#endif
	if (bOn != bTold)
	{
		for (TActorIterator<AStarlingMurmuration> It(World); It; ++It)
		{
			It->SetFeeding(bOn);
		}
	}
	if (bOn)
	{
		for (TActorIterator<AStarlingMurmuration> It(World); It; ++It)
		{
			It->SetFeedHand(OfferAt);
		}
	}
	bTold = bOn;
}

void UBirdTameSubsystem::LogEvent(const TCHAR* What, int32 BirdId) const
{
#if LYUD_HAS_BIRDLIFE
	// only the birds near the hero (the log is for checking a befriending, not the whole coast)
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	FBirdLifeInfo Info;
	if (Pawn && Life && Life->GetBird(BirdId, Info) && FVector::Dist2D(Info.Location, Pawn->GetActorLocation()) < 3000.f)
	{
		UE_LOG(LogTemp, Log, TEXT("Tame: bird %d %s at %.0f cm, alarm %.2f trust %.2f, offering %d"), BirdId, What,
			FVector::Dist2D(Info.Location, Pawn->GetActorLocation()), Info.Alertness, Info.Trust, IsOffering() ? 1 : 0);
	}
#endif
}

FText UBirdTameSubsystem::BirdName(int32 BirdId) const
{
#if LYUD_HAS_BIRDLIFE
	if (const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		FBirdLifeInfo Info;
		if (Life->GetBird(BirdId, Info))
		{
			return LyudText::Get(FString::Printf(TEXT("bird.%s.name"), *Snake(Info.SpeciesId.ToString())));
		}
	}
#endif
	return LyudText::Get(TEXT("ui.tame.a_bird"));
}

bool UBirdTameSubsystem::IsNearOffer(int32 BirdId) const
{
#if LYUD_HAS_BIRDLIFE
	if (const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		FBirdLifeInfo Info;
		// the events of birds far off (another flock across the bay) are not the player's business
		return Life->GetBird(BirdId, Info) && FVector::Dist(Info.Location, OfferAt) < 2500.f;
	}
#endif
	return false;
}

void UBirdTameSubsystem::Say(const TCHAR* Key, int32 BirdId, float Seconds)
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, BirdId >= 0 ? LyudText::Get(Key, { FStringFormatArg(BirdName(BirdId).ToString()) }) : LyudText::Get(Key));
		SayLeft = Seconds;
	}
}

void UBirdTameSubsystem::HandleNotice(int32 BirdId)
{
	LogEvent(TEXT("notice"), BirdId);
	if (IsOffering() && Focus < 0 && IsNearOffer(BirdId))
	{
		Focus = BirdId;
		Say(TEXT("ui.tame.notice"), BirdId);
	}
}

void UBirdTameSubsystem::HandleTurn(int32 BirdId)
{
	LogEvent(TEXT("turn"), BirdId);
	if (IsOffering() && (Focus < 0 || Focus == BirdId) && IsNearOffer(BirdId))
	{
		Focus = BirdId;
		Say(TEXT("ui.tame.turn"), BirdId);
	}
}

void UBirdTameSubsystem::HandleWatch(int32 BirdId)
{
	LogEvent(TEXT("watch"), BirdId);
	if (IsOffering() && Focus == BirdId)
	{
		Say(TEXT("ui.tame.watch"), BirdId, 4.f);
	}
}

void UBirdTameSubsystem::HandleCall(int32 BirdId)
{
	LogEvent(TEXT("call"), BirdId);
	if (IsOffering() && Focus == BirdId)
	{
		Say(TEXT("ui.tame.call"), BirdId);
	}
}

void UBirdTameSubsystem::HandleApproach(int32 BirdId)
{
#if LYUD_HAS_BIRDLIFE
	if (const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && IsNearOffer(BirdId))
	{
		// a bird coming to the hand is one the hero has met, whether it eats or not
		FBirdLifeInfo Info;
		if (ULyudBirdBook* Book = ULyudBirdBook::Get(this); Book && Life->GetBird(BirdId, Info))
		{
			Book->NoteSeen(this, Info.SpeciesId, Info.bFemale, TEXT("hand"), Info.Location);
		}
	}
#endif
	if (IsOffering() && IsNearOffer(BirdId))
	{
		bApproaching = true;
	}
	LogEvent(TEXT("approach"), BirdId);
	if (IsOffering() && IsNearOffer(BirdId))
	{
		Focus = BirdId;
		Say(TEXT("ui.tame.approach"), BirdId);
	}
}

void UBirdTameSubsystem::HandleEat(int32 BirdId)
{
	LogEvent(TEXT("eat"), BirdId);
	if (Phase == EPhase::Calling)
	{
		if (BirdId == CallId)
		{
			// the called bird is on the palm: a moment with it, then the view goes into it
			Focus = BirdId;
			Phase = EPhase::Eating;
			bCalled = true;
			PhaseTime = 0.f;
			Say(TEXT("ui.tame.summon_arrived"), BirdId);
		}
		return;
	}
	if (!IsOffering() || Phase != EPhase::Idle)
	{
		return;
	}
	Focus = BirdId;
	Phase = EPhase::Eating;
	PhaseTime = 0.f;
	Say(TEXT("ui.tame.eat"), BirdId);
}

void UBirdTameSubsystem::HandleFlee(int32 BirdId)
{
	if (Focus == BirdId)
	{
		bApproaching = false;
	}
	LogEvent(TEXT("flee"), BirdId);
	if (Focus == BirdId && Phase == EPhase::Idle)
	{
		Say(TEXT("ui.tame.flee"), BirdId);
		Focus = -1;
	}
}

void UBirdTameSubsystem::HandleReturnedToHero(APawn* Pawn)
{
	// back in the hero, where he crouched: he stands up with an empty palm
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(Pawn))
	{
		Hero->SetCrouchToggled(false);
		Hero->SetViewZoom(1.f);
		if (UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero))
		{
			Reach->ClearHold();
		}
	}
	Phase = EPhase::Idle;
	bWantOffer = false;
	bCalled = false;
	CallId = -1;
	Focus = -1;
}

void UBirdTameSubsystem::Eaten()
{
	// one handful is gone (a called bird came for the hand, not the feed); the camera goes into the bird
	const int32 Bird = Focus;
	if (ULyudInventory* Inv = ULyudInventory::Get(this); Inv && !bCalled)
	{
		Inv->Remove(FeedId);
	}
#if LYUD_HAS_BIRDLIFE
	if (const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		FBirdLifeInfo Info;
		if (ULyudBirdBook* Book = ULyudBirdBook::Get(this); Book && Life->GetBird(Bird, Info))
		{
			Book->NoteTamed(this, Bird, Info.SpeciesId, Info.bFemale, Info.Location);
		}
	}
#endif
	bWantOffer = false;
	TellBirds(false);
	if (Handful)
	{
		Handful->Destroy();
		Handful = nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("Tame: bird %d befriended"), Bird);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, FText::GetEmpty());
	}
	SayLeft = 0.f;
	bool bBird = false;
#if LYUD_HAS_BIRDFLIGHT
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	bBird = PC && UBirdFlightSubsystem::BecomeBird(PC, Bird);
#endif
	Phase = bBird ? EPhase::Bird : EPhase::Idle;
	if (!bBird)
	{
		Focus = -1;
#if LYUD_HAS_BIRDLIFE
		if (bCalled)
		{
			if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
			{
				Life->CancelSummon();
			}
		}
#endif
		bCalled = false;
		CallId = -1;
	}
}

// ------------------------------------------------------------------ the hero's side

bool UBirdTameSubsystem::IsOffering() const
{
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UHeroReachComponent* Reach = Hero ? Hero->FindComponentByClass<UHeroReachComponent>() : nullptr;
	return bWantOffer && Handful && Reach && Reach->IsHolding() && Reach->HoldWeight() > 0.95f && !Reach->IsBusy();
}

bool UBirdTameSubsystem::SetOffering(bool bOn)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (bOn && (!Inv || Inv->Count(FeedId) < 1))
	{
		Say(TEXT("ui.tame.nofeed"), -1);
		return false;
	}
	if (bOn && Inv->EquippedItem() != FeedId)
	{
		Inv->Equip(FeedId);
	}
	bWantOffer = bOn;
	return true;
}

void UBirdTameSubsystem::DebugGiveFeed(int32 Handfuls)
{
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(FeedId, FMath::Max(Handfuls, 1));
		Inv->Equip(FeedId);
	}
}

void UBirdTameSubsystem::UpdateHand(AHeroCharacter* Hero, float DeltaTime)
{
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Reach || !Inv)
	{
		return;
	}
	if (IsCalling())
	{
		UpdateCall(Hero, DeltaTime);
		return;
	}
	const bool bEquipped = Inv->EquippedItem() == FeedId;
	if (!bEquipped)
	{
		bWantOffer = false;
	}
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// a move of the hand (buying a drink, drinking) takes it over; the feed waits in the pocket meanwhile
	const bool bFree = !Reach->IsBusy() && !Reach->HasOverrideHold();
	const bool bOffer = bEquipped && bWantOffer && bFree;
	const bool bCarry = bEquipped && !bWantOffer && bFree;
	if (bOffer)
	{
		Reach->SetHold(OfferPose());
	}
	else if (bCarry)
	{
		Reach->SetHold(CarryPose());
	}
	else
	{
		Reach->ClearHold();
	}

	// what the hand shows: the bag while carried, the handful while held out
	// (they are fixed on the hand where its palm is when they appear)
	FTransform PalmNow;
	const bool bPalmUp = HandPalm(Hero->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), PalmNow) && PalmNow.GetUnitAxis(EAxis::Z).Z > 0.5f;
	const bool bShowBag = bCarry && Reach->IsHolding() && Reach->HoldWeight() > 0.95f;
	// the seeds as soon as the palm, coming out, is turned up
	const bool bShowSeeds = bOffer && Reach->IsHolding() && Reach->HoldWeight() > 0.5f && (Handful || bPalmUp);
	if (bShowBag && !Bag)
	{
		Bag = World->SpawnActor<ABirdFeedBag>(Reach->HoldPalm(), FRotator::ZeroRotator, Params);
	}
	else if (!bShowBag && Bag)
	{
		Bag->Destroy();
		Bag = nullptr;
	}
	SpillCooldown = FMath::Max(0.f, SpillCooldown - DeltaTime);
	if (bShowSeeds && !Handful && SpillCooldown <= 0.f)
	{
		Handful = World->SpawnActor<ASeedHandful>(Reach->HoldPalm(), FRotator::ZeroRotator, Params);
	}
	else if (!bShowSeeds && Handful && Phase != EPhase::Eating)
	{
		Handful->Destroy();
		Handful = nullptr;
	}
	if (!Handful)
	{
		BirdOfferAt = FVector::ZeroVector;
		OfferAt = FVector::ZeroVector;
	}

	// the bag and the seeds ride on the hand bone (attached once, where the hand's own palm is), so they move with the hand
	// in the same frame: placed from last frame's bones they trailed a fast hand by a frame and vanished from it on a jerk
	const FVector N = Reach->HoldNormal();
	const FVector F = Reach->HoldFinger();
	USkeletalMeshComponent* Body = Hero->GetMesh();
	auto AttachToPalm = [Body, N, F](AActor* Item, const FVector& Offset, bool bAcross)
	{
		FTransform PalmXf;
		if (!Item || Item->GetAttachParentActor() || !HandPalm(Body, N, F, PalmXf))
		{
			return;
		}
		// Offset: the item's middle in the palm's frame (fingers, across, out of the palm). A bag lies across the palm: its
		// thickness (X) out of the palm, its width (Y) along the fingers, its top (Z) across
		const FQuat Rot = bAcross ? FRotationMatrix::MakeFromXY(PalmXf.GetUnitAxis(EAxis::Z), PalmXf.GetUnitAxis(EAxis::X)).ToQuat() : PalmXf.GetRotation();
		Item->SetActorTransform(FTransform(Rot, PalmXf.TransformPosition(Offset)));
		Item->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform, TEXT("hand_r"));
	};
	if (Bag)
	{
		AttachToPalm(Bag, FVector(1.5f, 0.f, ABirdFeedBag::HalfDepth + 1.7f), true);
	}
	// a palm turned over (a jerk of the hand, the arm swung round) spills what is on it; a fresh handful from the bag
	// appears once it is up again (the bag holds plenty: no handful is used up)
	if (Handful && Phase != EPhase::Eating && Handful->GetAttachParentActor() && Handful->GetActorUpVector().Z < 0.25f)
	{
		Handful->Spill();
		Handful = nullptr;
		BirdOfferAt = FVector::ZeroVector;
		SpillCooldown = 0.6f;
	}
	if (Handful)
	{
		AttachToPalm(Handful, FVector(0.f, 0.f, 1.7f), false);
		OfferAt = Handful->GetActorLocation();
		// what the birds are told: the palm smoothed over ~0.1 s, so the frame-to-frame wobble of the bones and uneven frame
		// times (a 3 ms frame after a 100 ms one) don't read as a hand jerked at the bird; a steady move keeps its speed
		BirdOfferAt = BirdOfferAt.IsZero() ? OfferAt : FMath::VInterpTo(BirdOfferAt, OfferAt, DeltaTime, 10.f);
	}

	// a bird coming to the palm: the eyes go to the palm, slowly (a slow turn doesn't frighten it), unless the player looks
	// somewhere himself
	if ((bApproaching || Phase == EPhase::Eating) && Handful)
	{
		if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
		{
			float MouseX = 0.f, MouseY = 0.f;
			PC->GetInputMouseDelta(MouseX, MouseY);
			if (FMath::Abs(MouseX) + FMath::Abs(MouseY) < 0.01f)
			{
				const FRotator Want = (OfferAt + FVector(0.f, 0.f, 6.f) - Hero->EyeLocation()).Rotation();
				const FRotator Now = PC->GetControlRotation();
				PC->SetControlRotation(FMath::RInterpConstantTo(Now, FRotator(FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch), -70.f, 20.f), Want.Yaw, 0.f), DeltaTime, 35.f));
			}
		}
	}
	if (!IsOffering() && Phase != EPhase::Eating)
	{
		bApproaching = false;
	}
	if (Phase != EPhase::Eating)
	{
		Hero->SetViewZoom(1.f);
	}

	if (Phase == EPhase::Eating)
	{
		// the bird pecks the heap down for a moment, looked at closely (the view narrows onto the palm), then the view
		// goes into it
		PhaseTime += DeltaTime;
		Hero->SetViewZoom(0.55f);
		if (Handful)
		{
			Handful->SetLeft(1.f - 0.5f * FMath::Clamp(PhaseTime / EatSeconds, 0.f, 1.f));
		}
		TellBirds(true);
		if (PhaseTime >= EatSeconds)
		{
			Eaten();
		}
		return;
	}
	TellBirds(IsOffering());
	if (!IsOffering() && Phase == EPhase::Idle)
	{
		Focus = -1;
	}
}

void UBirdTameSubsystem::UpdateHud(AHeroCharacter* Hero)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const bool bShow = Hero && Inv && UI && !UI->IsOpen() && !Inv->EquippedItem().IsNone() && Phase != EPhase::Bird;
	if (!Viewport)
	{
		return;
	}
	if (!bShow)
	{
		if (Hud)
		{
			Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
			Hud.Reset();
		}
		return;
	}
	if (!Hud)
	{
		Hud = SNew(SLyudHeldItem);
		Viewport->AddViewportWidgetContent(Hud.ToSharedRef(), 55);
	}
	const FName Id = Inv->EquippedItem();
	const FText Title = LyudText::Get(TEXT("ui.held.item"), { FStringFormatArg(ULyudInventory::ItemName(Id).ToString()), FStringFormatArg(Inv->Count(Id)) });
	// calling a friend holds the empty palm out too (F lowers it)
	const TCHAR* HintKey = bWantOffer || IsCalling() ? TEXT("ui.held.hint_out") : Hero->bIsCrouched ? TEXT("ui.held.hint_crouched") : TEXT("ui.held.hint");
	Hud->Set(UI->IconBrush(Id), Title, LyudText::Get(HintKey));
	// the bird the palm is held out to: how it feels about the hero, so creeping can be judged
	FText Bird;
	float Alert = 0.f, Trust = 0.f;
#if LYUD_HAS_BIRDLIFE
	if (const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this); Life && bWantOffer && Focus >= 0)
	{
		FBirdLifeInfo Info;
		if (Life->GetBird(Focus, Info))
		{
			Bird = BirdName(Focus);
			Alert = Info.Alertness;
			Trust = Info.Trust;
		}
	}
#endif
	Hud->SetBird(Bird, Alert, Trust);
	LogTimer -= World->GetDeltaSeconds();
	if (bWantOffer && LogTimer <= 0.f)
	{
		LogTimer = 1.f;
		if (!Bird.IsEmpty())
		{
			LogEvent(TEXT("focus"), Focus);
		}
	}
}

void UBirdTameSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (CVarTame.GetValueOnGameThread() == 0)
	{
		if (bWantOffer || Bag || Handful || bTold)
		{
			Reset();
			if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
			{
				UHeroReachComponent::Get(Hero)->ClearHold();
			}
		}
		UpdateHud(nullptr);
		return;
	}
	BindBirds();
	AHeroCharacter* Hero = Cast<AHeroCharacter>(PC->GetPawn());
	if (!Hero || Hero->IsHidden())
	{
		// flying as the bird (or sitting at the computer): the hero's hand waits as it was
		UpdateHud(nullptr);
		return;
	}
	if (Phase == EPhase::Bird)
	{
		// back in the hero without the flight's event (the flight code went another way)
		HandleReturnedToHero(Hero);
	}

	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bInUI = (UI && UI->IsOpen()) || (Menu && Menu->IsOpen());
	if (!bInUI && !PC->IsMoveInputIgnored() && PC->WasInputKeyJustPressed(EKeys::F) && Phase == EPhase::Calling)
	{
		CancelSummon(TEXT("ui.tame.summon_stop"));
	}
	else if (!bInUI && !PC->IsMoveInputIgnored() && PC->WasInputKeyJustPressed(EKeys::F) && Phase == EPhase::Idle)
	{
		SetOffering(!bWantOffer);
	}
	UpdateBinoculars(DeltaTime);
	DriveDebugWalk(Hero, DeltaTime);
	UpdateHand(Hero, DeltaTime);
	UpdateHud(Hero);
	if (SayLeft > 0.f)
	{
		SayLeft -= DeltaTime;
		if (SayLeft <= 0.f)
		{
			if (ULyudInventoryUI* UI2 = ULyudInventoryUI::Get(this))
			{
				UI2->SetPrompt(this, FText::GetEmpty());
			}
		}
	}
}

// ------------------------------------------------------------------ calling a befriended bird (the diary)

bool UBirdTameSubsystem::Summon(int32 BirdId)
{
#if LYUD_HAS_BIRDLIFE
	UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	FBirdLifeInfo Info;
	if (!Life || !Hero || Hero->IsHidden() || !Life->GetBird(BirdId, Info))
	{
		Say(TEXT("ui.tame.summon_cant"), -1);
		return false;
	}
	if (Life->IsBirdAway(BirdId))
	{
		Say(TEXT("ui.tame.summon_away"), BirdId);
		return false;
	}
	// under a roof the bird can't get to the hand
	if (const ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this); Sound && Sound->GetOutdoors() < 0.5f)
	{
		Say(TEXT("ui.tame.summon_indoors"), BirdId);
		return false;
	}
	if (Phase != EPhase::Idle)
	{
		return false;
	}
	bWantOffer = false;
	if (Handful)
	{
		Handful->Destroy();
		Handful = nullptr;
	}
	// the palm is empty: the bag carried in it goes back to the pocket (UpdateHand isn't run while calling)
	if (Bag)
	{
		Bag->Destroy();
		Bag = nullptr;
	}
	TellBirds(false);
	CallId = BirdId;
	CallTime = 0.f;
	bCalled = false;
	Phase = EPhase::Calling;
	Focus = BirdId;
	// the bird answers as soon as the hand is out; until then it is told where the hero is
	OfferAt = Hero->EyeLocation() + FRotator(0.f, Hero->GetControlRotation().Yaw, 0.f).Vector() * 54.f - FVector(0.f, 0.f, 30.f);
	if (!Life->SummonBird(BirdId, OfferAt))
	{
		Phase = EPhase::Idle;
		CallId = -1;
		Focus = -1;
		Say(TEXT("ui.tame.summon_cant"), -1);
		return false;
	}
	Say(TEXT("ui.tame.summon"), BirdId, 5.f);
	UE_LOG(LogTemp, Log, TEXT("Tame: calling bird %d (%s) from %.0f m"), BirdId, *Info.SpeciesId.ToString(), FVector::Dist(Info.Location, OfferAt) / 100.f);
	return true;
#else
	return false;
#endif
}

void UBirdTameSubsystem::CancelSummon(const TCHAR* SayKey)
{
#if LYUD_HAS_BIRDLIFE
	if (UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this))
	{
		Life->CancelSummon();
	}
#endif
	const int32 Was = CallId;
	if (Phase == EPhase::Calling || (Phase == EPhase::Eating && bCalled))
	{
		Phase = EPhase::Idle;
	}
	CallId = -1;
	bCalled = false;
	Focus = -1;
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Hero->SetViewZoom(1.f);
		if (UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero))
		{
			Reach->ClearHold();
		}
	}
	if (SayKey)
	{
		Say(SayKey, Was);
	}
}

void UBirdTameSubsystem::UpdateCall(AHeroCharacter* Hero, float DeltaTime)
{
	UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero);
	if (!Reach)
	{
		return;
	}
	// the empty palm held out, as with the seeds; a move of the hand (a drink) gives up the call
	if (Reach->IsBusy() || Reach->HasOverrideHold())
	{
		CancelSummon(TEXT("ui.tame.summon_stop"));
		return;
	}
	Reach->SetHold(OfferPose());
	FTransform PalmNow;
	if (Reach->IsHolding() && Reach->HoldWeight() > 0.5f && HandPalm(Hero->GetMesh(), Reach->HoldNormal(), Reach->HoldFinger(), PalmNow))
	{
		// where the bird's feet go: on the skin of the palm
		OfferAt = PalmNow.TransformPosition(FVector(0.f, 0.f, 1.2f));
	}
#if LYUD_HAS_BIRDLIFE
	UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	if (!Life || !Life->SummonBird(CallId, OfferAt))
	{
		CancelSummon(TEXT("ui.tame.summon_cant"));
		return;
	}
	FBirdLifeInfo Info;
	const bool bNear = Life->GetBird(CallId, Info) && FVector::Dist(Info.Location, OfferAt) < 1500.f;
#else
	const bool bNear = false;
#endif
	// the eyes follow it in the last metres (slowly; the mouse wins)
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()); PC && (bNear || Phase == EPhase::Eating))
	{
		float MouseX = 0.f, MouseY = 0.f;
		PC->GetInputMouseDelta(MouseX, MouseY);
		if (FMath::Abs(MouseX) + FMath::Abs(MouseY) < 0.01f)
		{
			const FRotator Want = (OfferAt + FVector(0.f, 0.f, 6.f) - Hero->EyeLocation()).Rotation();
			const FRotator Now = PC->GetControlRotation();
			PC->SetControlRotation(FMath::RInterpConstantTo(Now, FRotator(FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch), -70.f, 20.f), Want.Yaw, 0.f), DeltaTime, 35.f));
		}
	}
	if (Phase == EPhase::Eating)
	{
		PhaseTime += DeltaTime;
		Hero->SetViewZoom(0.55f);
		if (PhaseTime >= EatSeconds)
		{
			Eaten();
		}
		return;
	}
	Hero->SetViewZoom(1.f);
#if LYUD_HAS_BIRDLIFE
	if (FMath::FloorToInt(CallTime) != FMath::FloorToInt(CallTime + DeltaTime) && Life->GetBird(CallId, Info))
	{
		UE_LOG(LogTemp, Log, TEXT("Tame: called bird %d at %.1f m, %s"), CallId, FVector::Dist(Info.Location, OfferAt) / 100.f, *UEnum::GetValueAsString(Info.Activity));
	}
#endif
	CallTime += DeltaTime;
	if (CallTime > 45.f)
	{
		CancelSummon(TEXT("ui.tame.summon_nobody"));
	}
}

void UBirdTameSubsystem::UpdateBinoculars(float DeltaTime)
{
#if LYUD_HAS_BIRDLIFE
	const ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(this);
	ULyudBirdBook* Book = ULyudBirdBook::Get(this);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Screen || !Life || !Book || !PC || !PC->PlayerCameraManager || Screen->BinocularsAmount() < 0.95f)
	{
		GlassBird = -1;
		GlassTime = 0.f;
		return;
	}
	// a few times a second: the bird nearest the middle of the view (within a couple of degrees, 150 m)
	GlassTime += DeltaTime;
	if ((GlassTimer -= DeltaTime) > 0.f)
	{
		return;
	}
	GlassTimer = 0.2f;
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	int32 Best = -1;
	float BestCos = FMath::Cos(FMath::DegreesToRadians(2.5f));
	for (const int32 Id : Life->FindBirdsNear(Eye, 15000.f))
	{
		FBirdLifeInfo Info;
		if (!Life->GetBird(Id, Info) || Life->IsBirdAway(Id))
		{
			continue;
		}
		const FVector To = Info.Location - Eye;
		const float D = To.Size();
		if (D < 100.f)
		{
			continue;
		}
		const float C = FVector::DotProduct(To / D, Fwd);
		if (C > BestCos)
		{
			BestCos = C;
			Best = Id;
		}
	}
	if (Best != GlassBird)
	{
		GlassBird = Best;
		GlassTime = 0.f;
		return;
	}
	// held on it for a second and a half: the hero has had a good look
	FBirdLifeInfo Info;
	if (GlassBird >= 0 && GlassTime > 1.5f && Life->GetBird(GlassBird, Info))
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdGlass), false, PC->GetPawn());
		if (Info.Actor)
		{
			Q.AddIgnoredActor(Info.Actor);
		}
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Info.Location + FVector(0.f, 0.f, 5.f), ECC_Visibility, Q)
			&& Book->NoteSeen(this, Info.SpeciesId, Info.bFemale, TEXT("binoculars"), Info.Location))
		{
			Say(TEXT("ui.tame.seen"), GlassBird, 3.f);
		}
		GlassTime = -1e6f;
	}
#endif
}

// ------------------------------------------------------------------ scripted approach (offscreen recordings, tests)

void UBirdTameSubsystem::DebugWalkTo(const FVector& Target, float Speed, bool bCrouch, float TurnRate, float StopAt)
{
	bDebugWalk = true;
	DebugTarget = Target;
	DebugSpeed = Speed;
	DebugTurnRate = TurnRate;
	DebugStopAt = StopAt;
	if (AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Hero->SetCrouchToggled(bCrouch);
	}
}

void UBirdTameSubsystem::DebugStop()
{
	bDebugWalk = false;
}

void UBirdTameSubsystem::DriveDebugWalk(AHeroCharacter* Hero, float DeltaTime)
{
	if (!bDebugWalk)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (!PC)
	{
		return;
	}
	const float Left = FVector::Dist2D(Hero->GetActorLocation(), DebugTarget) - DebugStopAt;
	if (Left <= 0.f)
	{
		return;
	}
	// the view turns toward the target (eyes on its ground point, pitched down at it) at the given rate
	const FVector Eye = Hero->EyeLocation();
	const FRotator Want = (DebugTarget + FVector(0.f, 0.f, 25.f) - Eye).Rotation();
	const FRotator Now = PC->GetControlRotation();
	const FRotator Next(FMath::FixedTurn(Now.Pitch, FMath::Clamp(FRotator::NormalizeAxis(Want.Pitch), -35.f, 30.f), DebugTurnRate * DeltaTime),
		FMath::FixedTurn(Now.Yaw, Want.Yaw, DebugTurnRate * DeltaTime), 0.f);
	PC->SetControlRotation(Next);
	const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	const float Max = FMath::Max(Hero->bIsCrouched ? Move->MaxWalkSpeedCrouched : Move->MaxWalkSpeed, 1.f);
	Hero->AddMovementInput((DebugTarget - Hero->GetActorLocation()).GetSafeNormal2D(), FMath::Clamp(DebugSpeed / Max, 0.f, 1.f) * FMath::Clamp(Left / 30.f, 0.2f, 1.f));
}

static FAutoConsoleCommandWithWorldAndArgs GTameGive(TEXT("Tame.Give"), TEXT("Tame.Give [handfuls=5]: bird feed into the pocket and the hand"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBirdTameSubsystem* T = UBirdTameSubsystem::Get(World))
		{
			T->DebugGiveFeed(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 5);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GTameOffer(TEXT("Tame.Offer"), TEXT("Tame.Offer 1|0: hold the palm with feed out (as F), or take it back"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBirdTameSubsystem* T = UBirdTameSubsystem::Get(World))
		{
			T->SetOffering(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GTameWalk(TEXT("Tame.Walk"),
	TEXT("Tame.Walk x y z speed crouch(0|1) turnrate stopat | off: the hero walks to a world point (scripted approach)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UBirdTameSubsystem* T = UBirdTameSubsystem::Get(World);
		if (!T)
		{
			return;
		}
		if (Args.Num() < 3)
		{
			T->DebugStop();
			return;
		}
		auto F = [&Args](int32 I, float Def) { return Args.IsValidIndex(I) ? FCString::Atof(*Args[I]) : Def; };
		T->DebugWalkTo(FVector(F(0, 0.f), F(1, 0.f), F(2, 0.f)), F(3, 80.f), F(4, 1.f) != 0.f, F(5, 60.f), F(6, 90.f));
	}));

// ------------------------------------------------------------------ headless self-test (unreal/scripts/test_tame.py)

namespace
{
	/** Steps run one after another on the game's ticker, each after its delay; each logs TAMETEST PASS|FAIL <what>. */
	struct FTameTest
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TPair<float, TFunction<void()>>> Steps;
		int32 Next = 0;
		float Wait = 0.f;
		int32 Fails = 0;
		float EyeStanding = 0.f;
	};
	TSharedPtr<FTameTest> GTameTest;

	void Check(bool bOk, const FString& What)
	{
		UE_LOG(LogTemp, Warning, TEXT("TAMETEST %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What);
		if (!bOk && GTameTest)
		{
			++GTameTest->Fails;
		}
	}
}

static FAutoConsoleCommandWithWorld GTameSelfTest(TEXT("Tame.SelfTest"),
	TEXT("Runs the befriending checks in the running game (vending machine sells feed, Tab equips it, F offers it, C crouches, lyud.Tame 0 switches off); logs TAMETEST lines"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		GTameTest = MakeShared<FTameTest>();
		FTameTest& T = *GTameTest;
		T.World = World;
		auto Hero = [&T]() { return T.World.IsValid() ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(T.World.Get(), 0)) : nullptr; };
		auto Inv = [&T]() { return ULyudInventory::Get(T.World.Get()); };
		auto Tame = [&T]() { return UBirdTameSubsystem::Get(T.World.Get()); };
		auto Run = [&T](const TCHAR* Cmd) { GEngine->Exec(T.World.Get(), Cmd); };
		T.Steps.Add({ 0.5f, [Inv, Tame, Run]()
		{
			// empty pockets for the test (a new game starts with feed while lyud.StartFeed is on)
			Run(TEXT("lyud.StartFeed 0"));
			Inv()->Reset();
			Run(TEXT("lyud.StartFeed 5"));
			Check(!Tame()->SetOffering(true), TEXT("no feed: the palm stays in"));
			Run(TEXT("Vending.Buy 5"));
		} });
		T.Steps.Add({ 6.f, [Inv]()
		{
			Check(Inv()->Count(UBirdTameSubsystem::FeedId) == 5, FString::Printf(TEXT("bought a bag: 5 handfuls (%d)"), Inv()->Count(UBirdTameSubsystem::FeedId)));
			Check(Inv()->Rubles == 85, FString::Printf(TEXT("paid 15 roubles (%d left)"), Inv()->Rubles));
			Inv()->Equip(UBirdTameSubsystem::FeedId);
			Check(Inv()->EquippedItem() == UBirdTameSubsystem::FeedId, TEXT("equipped from the inventory"));
		} });
		T.Steps.Add({ 1.5f, [Hero, &T]()
		{
			const UHeroReachComponent* R = Hero()->FindComponentByClass<UHeroReachComponent>();
			Check(R && R->IsHolding() && R->HoldWeight() > 0.99f && !R->IsBusy(), TEXT("the bag is carried at the hip (hold, nothing locked)"));
			T.EyeStanding = Hero()->EyeLocation().Z - (Hero()->GetActorLocation().Z - Hero()->GetSimpleCollisionHalfHeight());
			Hero()->SetCrouchToggled(true);
		} });
		T.Steps.Add({ 1.5f, [Hero, Tame, &T]()
		{
			const float Eye = Hero()->EyeLocation().Z - (Hero()->GetActorLocation().Z - Hero()->GetSimpleCollisionHalfHeight());
			Check(Hero()->bIsCrouched && Hero()->GetCrouchAlpha() > 0.99f, TEXT("C crouches"));
			Check(T.EyeStanding - Eye > 30.f, FString::Printf(TEXT("the eyes went down %.0f cm"), T.EyeStanding - Eye));
			Check(Hero()->GetCharacterMovement()->MaxWalkSpeedCrouched <= 110.f, TEXT("crouched speed is a creep"));
			Check(Tame()->SetOffering(true), TEXT("F with feed: the palm goes out"));
		} });
		T.Steps.Add({ 1.5f, [Hero, Tame]()
		{
			UBirdTameSubsystem* S = Tame();
			Check(S->IsOffering(), TEXT("offering once the hand is out"));
			// out in front of the body (the crouch leans the head forward over the shoulders, so not measured from the eyes),
			// below the eyes, palm up
			const FVector Eye = Hero()->EyeLocation();
			const FVector At = S->OfferLocation();
			const FRotator Yaw(0.f, Hero()->GetControlRotation().Yaw, 0.f);
			const FVector Body = Yaw.UnrotateVector(At - Hero()->GetActorLocation());
			const float Below = float(Eye.Z - At.Z);
			const UHeroReachComponent* R = Hero()->FindComponentByClass<UHeroReachComponent>();
			Check(Body.X > 40.f && Body.X < 90.f && Below > 5.f && Below < 70.f && FMath::Abs(Body.Y) < 35.f && R && R->HoldNormal().Z > 0.8f,
				FString::Printf(TEXT("the palm is out in front of the body, below the eyes, up (fwd %.0f right %.0f, %.0f below the eyes, normal z %.2f)"),
					Body.X, Body.Y, Below, R ? R->HoldNormal().Z : 0.f));
			FVector Palm;
			Check(PalmFromBones(Hero(), Palm) && FVector::Dist(Palm, At) < 6.f, FString::Printf(TEXT("the seeds lie on the hand (%.1f cm off the palm)"), FVector::Dist(Palm, At)));
			S->SetOffering(false);
		} });
		T.Steps.Add({ 1.5f, [Hero, Tame, Run]()
		{
			Check(!Tame()->IsOffering(), TEXT("F again: the palm comes back"));
			Run(TEXT("lyud.Tame 0"));
		} });
		T.Steps.Add({ 1.f, [Hero, Run]()
		{
			const UHeroReachComponent* R = Hero()->FindComponentByClass<UHeroReachComponent>();
			Check(R && !R->IsHolding(), TEXT("lyud.Tame 0: the hand is free"));
			Run(TEXT("lyud.Tame 1"));
			Hero()->SetCrouchToggled(false);
		} });
		T.Steps.Add({ 1.f, []()
		{
			UE_LOG(LogTemp, Warning, TEXT("TAMETEST DONE %d failed"), GTameTest ? GTameTest->Fails : -1);
		} });
		T.Wait = T.Steps[0].Key;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
		{
			if (!GTameTest || !GTameTest->World.IsValid())
			{
				return false;
			}
			FTameTest& X = *GTameTest;
			X.Wait -= Dt;
			if (X.Wait > 0.f)
			{
				return true;
			}
			X.Steps[X.Next].Value();
			if (++X.Next >= X.Steps.Num())
			{
				return false;
			}
			X.Wait = X.Steps[X.Next].Key;
			return true;
		}));
	}));
