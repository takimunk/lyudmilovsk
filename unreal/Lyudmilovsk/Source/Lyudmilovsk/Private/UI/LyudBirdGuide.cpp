#include "UI/LyudBirdGuide.h"

#include "Birds/BirdLife.h"
#include "Birds/BirdSpecies.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "UI/SLyudMapView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	TAutoConsoleVariable<float> CVarGuideRadius(TEXT("lyud.BirdGuide.Radius"), 300.f,
		TEXT("How far round the hero the bird guide (and the map's bird layer) sees the live birds, metres."));

	/** The guide's printing: dark green ink on a warm off-white page. */
	const FLinearColor GuideInk(0.08f, 0.2f, 0.14f);
	const FLinearColor GuideInkSoft(0.25f, 0.33f, 0.28f);
	const FLinearColor GuidePaper(0.93f, 0.91f, 0.84f);

	/** A species' printed colour: its most colourful plumage tone, darkened or lightened enough to read on paper and on the map. */
	FLinearColor PrintColor(const FBirdSpecies& S)
	{
		FLinearColor Best = FLinearColor(0.2f, 0.2f, 0.2f);
		float BestChroma = -1.f;
		for (const FLinearColor& C : S.Palette)
		{
			const FLinearColor Srgb = C.ToFColorSRGB().ReinterpretAsLinear();
			const float Chroma = FMath::Max3(Srgb.R, Srgb.G, Srgb.B) - FMath::Min3(Srgb.R, Srgb.G, Srgb.B);
			if (Chroma > BestChroma)
			{
				BestChroma = Chroma;
				Best = C;
			}
		}
		FLinearColor Hsv = Best.LinearRGBToHSV();
		// no paler than mid-tone (a jackdaw's grey or a gull's cream vanish on the paper)
		Hsv.B = FMath::Clamp(Hsv.B, 0.12f, 0.4f);
		Hsv.G = FMath::Max(Hsv.G, 0.35f);
		return Hsv.HSVToLinearRGB();
	}
}

// ------------------------------------------------------------------ the guide's state

const FName ULyudBirdGuide::ItemId(TEXT("bird_guide"));

ULyudBirdGuide* ULyudBirdGuide::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudBirdGuide>() : nullptr;
}

void ULyudBirdGuide::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("BirdGuide"), this);
	}
}

bool ULyudBirdGuide::IsOwned(const UObject* WorldContext)
{
	const ULyudInventory* Inv = ULyudInventory::Get(WorldContext);
	return Inv && Inv->Count(ItemId) > 0;
}

float ULyudBirdGuide::Radius()
{
	return FMath::Max(20.f, CVarGuideRadius.GetValueOnGameThread());
}

TArray<FLyudGuideBird> ULyudBirdGuide::BirdsNearHero(const UObject* WorldContext, FVector2D* HeroAt)
{
	TArray<FLyudGuideBird> Out;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	const UBirdLifeSubsystem* Life = UBirdLifeSubsystem::Get(WorldContext);
	if (!Pawn)
	{
		return Out;
	}
	// the hero as a bird (bird flight) is the pawn too: the guide sees round wherever he is
	const FVector Feet = Pawn->GetActorLocation();
	const FLyudMapFrame& F = ULyudQuests::Frame();
	if (HeroAt)
	{
		*HeroAt = F.FromWorld(Feet);
	}
	if (!Life || !Life->IsRunning())
	{
		return Out;
	}
	// a cylinder, not a ball: the birds high over the hero count as near
	const float R = Radius() * 100.f;
	for (const int32 Id : Life->FindBirdsNear(Feet, R * 1.5f))
	{
		FBirdLifeInfo B;
		if (!Life->GetBird(Id, B) || FVector::Dist2D(B.Location, Feet) > R || B.Activity == EBirdActivity::Claimed)
		{
			continue;
		}
		FLyudGuideBird G;
		G.Id = Id;
		G.Species = B.SpeciesId;
		G.At = F.FromWorld(B.Location);
		G.Velocity = FVector2D(B.Velocity.X, B.Velocity.Y) / 100.0;
		switch (B.Activity)
		{
		case EBirdActivity::Takeoff: case EBirdActivity::Fly: case EBirdActivity::Land: case EBirdActivity::Hawk:
		case EBirdActivity::Soar: case EBirdActivity::Flee: case EBirdActivity::Chase: case EBirdActivity::Mob: case EBirdActivity::Murmur:
			G.bFlying = true;
			break;
		case EBirdActivity::Swim:
			G.bOnWater = true;
			break;
		case EBirdActivity::Sleep:
			G.bAsleep = true;
			break;
		default:
			break;
		}
		if (const FBirdSpecies* S = UBirdSpeciesLibrary::Find(B.SpeciesId))
		{
			G.Color = PrintColor(*S);
			G.LengthCm = S->LengthCm;
		}
		Out.Add(G);
	}
	return Out;
}

// ------------------------------------------------------------------ the open guide

/** The guide's spread: its map round the hero on the left, the species near him listed on the right. */
class SLyudBirdGuidePage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudBirdGuidePage) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Screen)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Screen = InArgs._Screen;
		static const FSlateRoundedBoxBrush Page(GuidePaper, 8.f, FLinearColor(0.3f, 0.36f, 0.3f), 2.f);
		static const FSlateRoundedBoxBrush Frame(FLinearColor(0.f, 0.f, 0.f, 0.f), 2.f, GuideInk, 2.f);
		Legend = SNew(SVerticalBox);
		RebuildLegend();
		RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
		{
			RebuildLegend();
			return EActiveTimerReturnType::Continue;
		}));
		const int32 R = FMath::RoundToInt(ULyudBirdGuide::Radius());
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[SNew(SImage).Image(LyudUI::ShadeBrush())]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBorder).BorderImage(&Page).Padding(FMargin(30.f, 22.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(LyudUI::Font(30, "Bold")).ColorAndOpacity(GuideInk).Text(LyudText::Get(TEXT("ui.guide.title")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 14)
					[
						SNew(STextBlock).Font(LyudUI::Font(16, "Italic")).ColorAndOpacity(GuideInkSoft)
						.Text(LyudText::Get(TEXT("ui.guide.subtitle"), { FStringFormatArg(R) }))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBorder).BorderImage(&Frame).Padding(2.f)
							[
								SNew(SBox).WidthOverride(700.f).HeightOverride(640.f)[SNew(SLyudMapView).Owner(Screen.Get()).bGuide(true)]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(26, 0, 0, 0)
						[
							SNew(SBox).WidthOverride(340.f).HeightOverride(644.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
								[
									SNew(STextBlock).Font(LyudUI::Font(20, "Bold")).ColorAndOpacity(GuideInk).Text(LyudText::Get(TEXT("ui.guide.near")))
								]
								+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SScrollBox) + SScrollBox::Slot()[Legend.ToSharedRef()]]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(STextBlock).Font(LyudUI::Font(14)).ColorAndOpacity(GuideInkSoft).Text(LyudText::Get(TEXT("ui.guide.hint")))
					]
				]
			]
		];
	}

private:
	TWeakObjectPtr<ULyudGameScreen> Screen;
	TSharedPtr<SVerticalBox> Legend;

	void RebuildLegend()
	{
		struct FRow { FName Species; int32 N = 0; int32 Flying = 0; FLinearColor Color; };
		TArray<FRow> Rows;
		const TArray<FLyudGuideBird> Birds = ULyudBirdGuide::BirdsNearHero(Screen.Get());
		for (const FLyudGuideBird& B : Birds)
		{
			FRow* Row = Rows.FindByPredicate([&B](const FRow& R) { return R.Species == B.Species; });
			if (!Row)
			{
				Row = &Rows.AddDefaulted_GetRef();
				Row->Species = B.Species;
				Row->Color = B.Color;
			}
			++Row->N;
			Row->Flying += B.bFlying ? 1 : 0;
		}
		Rows.Sort([](const FRow& A, const FRow& B) { return A.N != B.N ? A.N > B.N : A.Species.LexicalLess(B.Species); });
		Legend->ClearChildren();
		if (Rows.Num() == 0)
		{
			Legend->AddSlot().AutoHeight()
			[
				SNew(STextBlock).Font(LyudUI::Font(16, "Italic")).ColorAndOpacity(GuideInkSoft).AutoWrapText(true).Text(LyudText::Get(TEXT("ui.guide.none")))
			];
			return;
		}
		for (const FRow& Row : Rows)
		{
			const FBirdSpecies* S = UBirdSpeciesLibrary::Find(Row.Species);
			const FText Name = S ? S->DisplayName() : FText::FromName(Row.Species);
			const FText Latin = S ? FText::FromString(S->Latin) : FText::GetEmpty();
			// the number level with the name, how many of them fly under it, level with the Latin
			// a fixed wrap width: the rows are rebuilt twice a second, and an auto-wrapping text sizes itself a frame late
			constexpr float NameWidth = 200.f;
			const FText Flying = Row.Flying > 0 ? LyudText::Get(TEXT("ui.guide.n_flying"), { FStringFormatArg(Row.Flying) }) : FText::GetEmpty();
			Legend->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
				[
					SNew(SBox).WidthOverride(14.f).HeightOverride(14.f)[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(Row.Color)]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(LyudUI::Font(16, "Bold")).ColorAndOpacity(GuideInk).WrapTextAt(NameWidth).Text(Name)]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(LyudUI::Font(12, "Italic")).ColorAndOpacity(GuideInkSoft).WrapTextAt(NameWidth).Text(Latin)]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 14, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Font(LyudUI::Font(16, "Bold")).ColorAndOpacity(GuideInk).Text(FText::AsNumber(Row.N))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
					[
						SNew(STextBlock).Font(LyudUI::Font(12, "Italic")).ColorAndOpacity(GuideInkSoft).Text(Flying)
					]
				]
			];
		}
		Legend->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
		[
			SNew(STextBlock).Font(LyudUI::Font(14, "Italic")).ColorAndOpacity(GuideInkSoft)
			.Text(LyudText::Get(TEXT("ui.guide.total"), { FStringFormatArg(Birds.Num()), FStringFormatArg(Rows.Num()) }))
		];
	}
};

ULyudBirdGuideScreen* ULyudBirdGuideScreen::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBirdGuideScreen>() : nullptr;
}

void ULyudBirdGuideScreen::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

void ULyudBirdGuideScreen::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	PC->bShowMouseCursor = bLock;
	if (bLock)
	{
		PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ULyudBirdGuideScreen::Open()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	if (IsOpen() || !Viewport || !ULyudBirdGuide::IsOwned(World) || (GameScreen && GameScreen->IsOpen()) || (UI && UI->IsOpen()))
	{
		return;
	}
	Page = SNew(SLyudBirdGuidePage).Screen(ULyudGameScreen::Get(World));
	Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 65);
	OpenedFrame = GFrameCounter;
	Lock(true);
}

void ULyudBirdGuideScreen::Close()
{
	if (!Page)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
	}
	Page.Reset();
	Lock(false);
}

void ULyudBirdGuideScreen::Tick(float)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !World->IsGameWorld())
	{
		return;
	}
	const bool bB = PC->WasInputKeyJustPressed(EKeys::B);
	if (IsOpen())
	{
		const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
		const bool bOther = (Menu && Menu->IsOpen()) || (GameScreen && GameScreen->IsOpen());
		if (OpenedFrame != GFrameCounter && (bB || PC->WasInputKeyJustPressed(EKeys::Escape) || bOther || !ULyudBirdGuide::IsOwned(World)))
		{
			Close();
		}
		return;
	}
	if (bB && !UGameplayStatics::IsGamePaused(World))
	{
		const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
		if (!(Menu && Menu->IsOpen()))
		{
			Open();
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GGuideOpen(TEXT("lyud.BirdGuide.Open"),
	TEXT("lyud.BirdGuide.Open [close]: opens the bird guide (gives it first if the hero has none; checks), or shuts it"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() > 0 && Args[0] == TEXT("close"))
		{
			if (ULyudBirdGuideScreen* S = ULyudBirdGuideScreen::Get(World))
			{
				S->Close();
			}
			return;
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(World); Inv && !ULyudBirdGuide::IsOwned(World))
		{
			Inv->Add(ULyudBirdGuide::ItemId);
		}
		if (ULyudBirdGuideScreen* S = ULyudBirdGuideScreen::Get(World))
		{
			S->Open();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GGuideLayer(TEXT("lyud.BirdGuide.MapLayer"), TEXT("lyud.BirdGuide.MapLayer 0|1: the diary map's bird layer"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudBirdGuide* G = ULyudBirdGuide::Get(World))
		{
			G->bMapLayer = Args.Num() == 0 ? !G->bMapLayer : FCString::Atoi(*Args[0]) != 0;
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GGuideReport(TEXT("lyud.BirdGuide.Report"), TEXT("lyud.BirdGuide.Report: the birds the guide sees round the hero"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		FVector2D At;
		const TArray<FLyudGuideBird> Birds = ULyudBirdGuide::BirdsNearHero(World, &At);
		TMap<FName, int32> N;
		for (const FLyudGuideBird& B : Birds)
		{
			++N.FindOrAdd(B.Species);
		}
		FString S;
		for (const TPair<FName, int32>& P : N)
		{
			S += FString::Printf(TEXT(" %s=%d"), *P.Key.ToString(), P.Value);
		}
		UE_LOG(LogTemp, Display, TEXT("BirdGuide: hero at (%.0f, %.0f) m, radius %.0f m, %d birds:%s"), At.X, At.Y, ULyudBirdGuide::Radius(), Birds.Num(), *S);
	}));
