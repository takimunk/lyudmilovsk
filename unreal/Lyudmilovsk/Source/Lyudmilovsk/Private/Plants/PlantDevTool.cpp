#include "Plants/PlantDevTool.h"

#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralPlant.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "PlantDevTool"

namespace
{
	FSlateFontInfo Font(int32 Size = 11) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }

	TSharedRef<SWidget> Label(const FText& Text, float Width)
	{
		return SNew(SBox).WidthOverride(Width).VAlign(VAlign_Center)[SNew(STextBlock).Text(Text).Font(Font()).ColorAndOpacity(FLinearColor::White)];
	}

	TSharedRef<SWidget> Button(const FText& Text, TFunction<void()> OnClick)
	{
		return SNew(SButton).IsFocusable(false).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[SNew(STextBlock).Text(Text).Font(Font())];
	}

	/** Label, slider and its value as text. */
	TSharedRef<SWidget> SliderRow(const FText& Name, float Min, float Max, TFunction<float()> Get, TFunction<void(float)> Set, TFunction<FText()> Show)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Label(Name, 190.f)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SSlider).IsFocusable(false).MinValue(Min).MaxValue(Max)
				.Value_Lambda([Get]() { return Get(); })
				.OnValueChanged_Lambda([Set](float V) { Set(V); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(80.f).VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))
				[SNew(STextBlock).Text_Lambda([Show]() { return Show(); }).Font(Font()).ColorAndOpacity(FLinearColor::White)]
			];
	}

	FText DayText(float Day)
	{
		const FDateTime D = FDateTime(2026, 1, 1) + FTimespan::FromDays(FMath::Max(Day, 1.f) - 1.f);
		return FText::FromString(D.ToString(TEXT("%d.%m")));
	}
}

APlantDevTool::APlantDevTool()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	SetActorHiddenInGame(true);
}

void APlantDevTool::BeginPlay()
{
	Super::BeginPlay();
	TryBindInput();
}

bool APlantDevTool::TryBindInput()
{
	if (bBound)
	{
		return true;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return false;
	}
	EnableInput(PC);
	if (InputComponent)
	{
		// E opens doors, F10 the menu, F8 is the editor's eject: F7 is free; never swallow it
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &APlantDevTool::Toggle).bConsumeInput = false;
		bBound = true;
	}
	return bBound;
}

void APlantDevTool::EndPlay(const EEndPlayReason::Type Reason)
{
	Close();
	Super::EndPlay(Reason);
}

TArray<AProceduralPlant*> APlantDevTool::PlantsByDistance() const
{
	TArray<AProceduralPlant*> Out;
	for (TActorIterator<AProceduralPlant> It(GetWorld()); It; ++It)
	{
		Out.Add(*It);
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	Out.Sort([Eye](const AProceduralPlant& A, const AProceduralPlant& B) { return FVector::DistSquared(A.GetActorLocation(), Eye) < FVector::DistSquared(B.GetActorLocation(), Eye); });
	return Out;
}

void APlantDevTool::SelectInView()
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	// the tree whose trunk, a few metres up, is closest to the middle of the view
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Look = PC->PlayerCameraManager->GetCameraRotation().Vector();
	float Best = -1.f;
	for (AProceduralPlant* P : PlantsByDistance())
	{
		const FVector To = P->GetActorLocation() + FVector(0, 0, 300) - Eye;
		const float Score = FVector::DotProduct(To.GetSafeNormal(), Look) - To.Size() / 100000.f;
		if (Score > Best)
		{
			Best = Score;
			Plant = P;
		}
	}
}

void APlantDevTool::Step(int32 Delta)
{
	const TArray<AProceduralPlant*> All = PlantsByDistance();
	if (All.IsEmpty())
	{
		return;
	}
	const int32 At = All.IndexOfByKey(Plant.Get());
	Plant = All[((At == INDEX_NONE ? 0 : At + Delta) % All.Num() + All.Num()) % All.Num()];
}

void APlantDevTool::Toggle()
{
	if (Panel.IsValid())
	{
		Close();
	}
	else
	{
		Open();
	}
}

void APlantDevTool::Open()
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Viewport || !PC)
	{
		return;
	}
	SelectInView();
	TWeakObjectPtr<APlantDevTool> Self(this);
	auto P = [Self]() -> AProceduralPlant* { return Self.IsValid() ? Self->Selected() : nullptr; };
	auto Cal = [Self]() -> UGameCalendar* { return Self.IsValid() && Self->GetWorld() ? Self->GetWorld()->GetSubsystem<UGameCalendar>() : nullptr; };
	TArray<FName> SpeciesNames = PlantSpeciesLibrary::Names();

	Panel = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(24.f, 24.f))
	[
		SNew(SBox).WidthOverride(620.f)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.7f)).Padding(14.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[SNew(STextBlock).Text(LyudText::Get(TEXT("ui.dev.title"))).Font(Font(14)).ColorAndOpacity(FLinearColor::White)]
				// the world's date, time of day and how fast time runs: everything that reads the calendar follows
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 2)
				[SNew(STextBlock).Text(LyudText::Get(TEXT("ui.dev.world"))).Font(Font(12)).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.5f))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.dev.date")), 1.f, 365.f,
					[Cal]() { return Cal() ? (float)Cal()->Now().GetDayOfYear() : 1.f; },
					[Cal](float V)
					{
						if (!Cal()) return;
						const FDateTime Now = Cal()->Now();
						Cal()->SetNow(FDateTime(Now.GetYear(), 1, 1) + FTimespan::FromDays(FMath::FloorToInt(V) - 1) + Now.GetTimeOfDay());
					},
					[Cal]() { return Cal() ? FText::FromString(Cal()->Now().ToString(TEXT("%d.%m.%Y"))) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.dev.hour")), 0.f, 23.99f,
					[Cal]() { return Cal() ? (float)Cal()->Now().GetTimeOfDay().GetTotalHours() : 0.f; },
					[Cal](float V) { if (Cal()) Cal()->SetNow(Cal()->Now().GetDate() + FTimespan::FromHours(V)); },
					[Cal]() { return Cal() ? FText::FromString(Cal()->Now().ToString(TEXT("%H:%M"))) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.dev.speed")), 0.f, 1.f,
					// one to a hundred thousand game seconds per second, on a log scale
					[Cal]() { return Cal() ? FMath::LogX(10.f, FMath::Max(Cal()->TimeScale, 1.f)) / 5.f : 0.f; },
					[Cal](float V) { if (Cal()) Cal()->SetTimeScale(FMath::Pow(10.f, V * 5.f)); },
					[Cal]() { return Cal() ? LyudText::Get(TEXT("ui.dev.speed_value"), { FStringFormatArg(FText::AsNumber(FMath::RoundToInt(Cal()->TimeScale)).ToString()) }) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("ui.dev.speed_real")), [Cal]() { if (Cal()) Cal()->SetTimeScale(1.f); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 2)
				[SNew(STextBlock).Text(LyudText::Get(TEXT("ui.plants.title"))).Font(Font(12)).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.5f))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Prev", "◀"), [Self]() { if (Self.IsValid()) Self->Step(-1); })]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8, 0)
					[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Text_Lambda([P]()
						{ return P() ? FText::FromString(P()->GetActorNameOrLabel()) : LyudText::Get(TEXT("ui.plants.none")); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Next", "▶"), [Self]() { if (Self.IsValid()) Self->Step(1); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.plants.in_view")), [Self]() { if (Self.IsValid()) Self->SelectInView(); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[SNew(STextBlock).AutoWrapText(true).Font(Font(10)).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 0.8f)).Text_Lambda([P]()
					{ return P() ? FText::FromString(P()->DescribeState()) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.plants.age")), 0.f, 500.f,
					[P]() { return P() ? P()->GetAge() : 0.f; },
					[P](float V) { if (P()) P()->DevAge = V; },
					[P]() { return FText::AsNumber(P() ? FMath::RoundToInt(P()->GetAge() * 10.f) / 10.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.plants.age_speed")), 0.f, 5.f,
					[Self]() { return Self.IsValid() ? Self->AgeSpeed : 0.f; },
					[Self](float V) { if (Self.IsValid()) Self->AgeSpeed = V; },
					[Self]() { return FText::AsNumber(Self.IsValid() ? FMath::RoundToInt(Self->AgeSpeed * 100.f) / 100.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.plants.day")), 1.f, 365.f,
					[P]() { return P() ? P()->CurrentDay() : 1.f; },
					[P](float V) { if (P()) P()->DevDay = V; },
					[P]() { return P() ? DayText(P()->CurrentDay()) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LyudText::Get(TEXT("ui.plants.day_speed")), 0.f, 10.f,
					[Self]() { return Self.IsValid() ? Self->DaySpeed : 0.f; },
					[Self](float V) { if (Self.IsValid()) Self->DaySpeed = V; },
					[Self]() { return FText::AsNumber(Self.IsValid() ? FMath::RoundToInt(Self->DaySpeed * 100.f) / 100.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(LyudText::Get(TEXT("ui.plants.species")), 190.f)]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("SpPrev", "◀"), [P, SpeciesNames]()
						{
							if (!P()) return;
							const int32 I = SpeciesNames.IndexOfByKey(P()->Species);
							P()->Species = SpeciesNames[(I - 1 + SpeciesNames.Num()) % SpeciesNames.Num()];
							P()->Refresh();
						})]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8, 0)
					[SNew(STextBlock).Font(Font()).ColorAndOpacity(FLinearColor::White).Text_Lambda([P]()
						{
							const FPlantSpecies* S = P() ? PlantSpeciesLibrary::Find(P()->Species) : nullptr;
							return S ? PlantSpeciesLibrary::DisplayName(*S) : FText::GetEmpty();
						})]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("SpNext", "▶"), [P, SpeciesNames]()
						{
							if (!P()) return;
							const int32 I = SpeciesNames.IndexOfByKey(P()->Species);
							P()->Species = SpeciesNames[(I + 1) % SpeciesNames.Num()];
							P()->Refresh();
						})]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(LyudText::Get(TEXT("ui.plants.seed")), 190.f)]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("SeedMinus", "−"), [P]() { if (P()) { P()->Seed = FMath::Max(0, P()->Seed - 1); P()->Refresh(); } })]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8, 0)
					[SNew(STextBlock).Font(Font()).ColorAndOpacity(FLinearColor::White).Text_Lambda([P]() { return FText::AsNumber(P() ? P()->Seed : 0); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("SeedPlus", "+"), [P]() { if (P()) { P()->Seed++; P()->Refresh(); } })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SCheckBox).IsFocusable(false)
					.IsChecked_Lambda([P]() { return P() && P()->bForceSummary ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([P](ECheckBoxState State) { if (P()) { P()->bForceSummary = State == ECheckBoxState::Checked; P()->Refresh(); } })
					[SNew(STextBlock).Font(Font()).ColorAndOpacity(FLinearColor::White).Text(LyudText::Get(TEXT("ui.plants.summary")))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LyudText::Get(TEXT("ui.plants.live")), [Self, P]()
						{
							if (Self.IsValid()) { Self->AgeSpeed = 0.f; Self->DaySpeed = 0.f; }
							if (P()) { P()->DevAge = -1.f; P()->DevDay = -1.f; P()->Refresh(); }
						})]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LyudText::Get(TEXT("ui.plants.close")), [Self]() { if (Self.IsValid()) Self->Toggle(); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[SNew(STextBlock).AutoWrapText(true).Font(Font(9)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text(LyudText::Get(TEXT("ui.plants.hint")))]
			]
		]
	];
	// over the HUD, the diary and the transit screen (85, 86), under the game menu (100)
	Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 95);
	PC->SetShowMouseCursor(true);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(true);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
}

void APlantDevTool::Close()
{
	if (!Panel.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	}
	Panel.Reset();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void APlantDevTool::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBound)
	{
		TryBindInput();
	}
	AProceduralPlant* P = Plant.Get();
	if (!P)
	{
		return;
	}
	// the time-lapses keep running with the panel closed, until "back to game time"
	if (AgeSpeed > 0.f)
	{
		P->DevAge = FMath::Min((P->DevAge >= 0.f ? P->DevAge : P->GetAge()) + AgeSpeed * DeltaSeconds, 300.f);
	}
	if (DaySpeed > 0.f)
	{
		float Day = (P->DevDay >= 0.f ? P->DevDay : P->CurrentDay()) + DaySpeed * DeltaSeconds;
		if (Day >= 366.f) Day -= 365.f;
		P->DevDay = Day;
	}
#if ENABLE_DRAW_DEBUG
	if (Panel.IsValid())
	{
		// mark the selected tree: a ring round its trunk
		DrawDebugCircle(GetWorld(), P->GetActorLocation() + FVector(0, 0, 20), 120.f, 32, FColor(255, 200, 40), false, -1.f, 0, 3.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	}
#endif
}

namespace
{
	void CollectOfType(const TSharedRef<SWidget>& Widget, FName Type, TArray<TSharedRef<SWidget>>& Out)
	{
		if (Widget->GetType() == Type)
		{
			Out.Add(Widget);
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 I = 0; Children && I < Children->Num(); ++I)
		{
			CollectOfType(Children->GetChildAt(I), Type, Out);
		}
	}

	void LogCalendar(UWorld* World, const TCHAR* What)
	{
		if (const UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEVPANEL %s now %s scale %.1f"), What, *Cal->Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), Cal->TimeScale);
		}
	}
}

// headless checks of the panel: the clicks go through Slate's hit test like the mouse's, so whatever lies over the panel gets them
static FAutoConsoleCommandWithWorldAndArgs GDevPanel(TEXT("Dev.Panel"),
	TEXT("Dev.Panel open | close | log | click <SSlider|SButton> <index> <0..1 across it>: the F7 panel without the keyboard and mouse"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		TActorIterator<APlantDevTool> Tool(World);
		if (!Tool || Args.Num() == 0)
		{
			return;
		}
		const bool bOpen = Tool->GetPanel().IsValid();
		if ((Args[0] == TEXT("open") && !bOpen) || (Args[0] == TEXT("close") && bOpen))
		{
			Tool->Toggle();
		}
		else if (Args[0] == TEXT("log"))
		{
			LogCalendar(World, TEXT("log"));
		}
		else if (Args[0] == TEXT("click") && bOpen && Args.Num() > 2 && FSlateApplication::IsInitialized())
		{
			TArray<TSharedRef<SWidget>> Found;
			CollectOfType(Tool->GetPanel().ToSharedRef(), FName(*Args[1]), Found);
			const int32 Index = FCString::Atoi(*Args[2]);
			if (!Found.IsValidIndex(Index))
			{
				UE_LOG(LogTemp, Warning, TEXT("DEVPANEL no %s %d (%d found)"), *Args[1], Index, Found.Num());
				return;
			}
			const FGeometry& G = Found[Index]->GetTickSpaceGeometry();
			const float Across = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.5f;
			const FVector2D At = G.LocalToAbsolute(G.GetLocalSize() * FVector2D(Across, 0.5f));
			FSlateApplication& Slate = FSlateApplication::Get();
			const FWidgetPath Path = Slate.LocateWindowUnderMouse(At, Slate.GetInteractiveTopLevelWindows(), true);
			const FString Hit = Path.IsValid() ? Path.Widgets.Last().Widget->GetTypeAsString() : TEXT("nothing");
			FString Over;
			bool bReached = false;
			for (int32 I = Path.Widgets.Num() - 1; I >= 0; --I)
			{
				bReached |= Path.Widgets[I].Widget == Found[Index];
				if (I >= Path.Widgets.Num() - 6)
				{
					Over += Path.Widgets[I].Widget->GetTypeAsString() + TEXT(" < ");
				}
			}
			LogCalendar(World, TEXT("before"));
			// the pointer moves there first: a button only clicks when it is hovered as the mouse goes up
			const FPointerEvent Move(Slate.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, At, At - FVector2D(1.f, 1.f), TSet<FKey>(), FKey(), 0.f, FModifierKeysState());
			Slate.ProcessMouseMoveEvent(Move);
			const FPointerEvent Down(Slate.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, At, At, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
			const FPointerEvent Up(Slate.GetUserIndexForMouse(), FSlateApplicationBase::CursorPointerIndex, At, At, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
			Slate.ProcessMouseButtonDownEvent(nullptr, Down);
			Slate.ProcessMouseButtonUpEvent(Up);
			UE_LOG(LogTemp, Warning, TEXT("DEVPANEL click %s %d at %.0f,%.0f hit %s (path %s) target %s"), *Args[1], Index, At.X, At.Y, *Hit, *Over,
				bReached ? TEXT("ok") : TEXT("MISSED"));
			LogCalendar(World, TEXT("after"));
		}
	}));

void UPlantDevSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	FActorSpawnParameters P;
	P.Name = TEXT("PlantDevTool");
	P.ObjectFlags |= RF_Transient;
	World.SpawnActor<APlantDevTool>(P);
}

#undef LOCTEXT_NAMESPACE
