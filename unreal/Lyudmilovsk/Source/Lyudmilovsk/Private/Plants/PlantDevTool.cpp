#include "Plants/PlantDevTool.h"

#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
	TArray<FName> SpeciesNames = PlantSpeciesLibrary::Names();

	Panel = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(24.f, 24.f))
	[
		SNew(SBox).WidthOverride(620.f)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.7f)).Padding(14.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[SNew(STextBlock).Text(LOCTEXT("Title", "Растения: режим разработчика (F7)")).Font(Font(14)).ColorAndOpacity(FLinearColor::White)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Prev", "◀"), [Self]() { if (Self.IsValid()) Self->Step(-1); })]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8, 0)
					[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Text_Lambda([P]()
						{ return P() ? FText::FromString(P()->GetActorNameOrLabel()) : LOCTEXT("None", "нет деревьев"); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Next", "▶"), [Self]() { if (Self.IsValid()) Self->Step(1); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LOCTEXT("InView", "В прицеле"), [Self]() { if (Self.IsValid()) Self->SelectInView(); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[SNew(STextBlock).AutoWrapText(true).Font(Font(10)).ColorAndOpacity(FLinearColor(0.85f, 0.9f, 0.8f)).Text_Lambda([P]()
					{ return P() ? FText::FromString(P()->DescribeState()) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LOCTEXT("Age", "Возраст, лет"), 0.f, 150.f,
					[P]() { return P() ? P()->GetAge() : 0.f; },
					[P](float V) { if (P()) P()->DevAge = V; },
					[P]() { return FText::AsNumber(P() ? FMath::RoundToInt(P()->GetAge() * 10.f) / 10.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LOCTEXT("AgeSpeed", "Рост, лет в секунду"), 0.f, 5.f,
					[Self]() { return Self.IsValid() ? Self->AgeSpeed : 0.f; },
					[Self](float V) { if (Self.IsValid()) Self->AgeSpeed = V; },
					[Self]() { return FText::AsNumber(Self.IsValid() ? FMath::RoundToInt(Self->AgeSpeed * 100.f) / 100.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LOCTEXT("Day", "День года"), 1.f, 365.f,
					[P]() { return P() ? P()->CurrentDay() : 1.f; },
					[P](float V) { if (P()) P()->DevDay = V; },
					[P]() { return P() ? DayText(P()->CurrentDay()) : FText::GetEmpty(); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 3)
				[SliderRow(LOCTEXT("DaySpeed", "Календарь, дней в секунду"), 0.f, 10.f,
					[Self]() { return Self.IsValid() ? Self->DaySpeed : 0.f; },
					[Self](float V) { if (Self.IsValid()) Self->DaySpeed = V; },
					[Self]() { return FText::AsNumber(Self.IsValid() ? FMath::RoundToInt(Self->DaySpeed * 100.f) / 100.f : 0.f); })]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Species", "Вид"), 190.f)]
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
							return S ? S->DisplayName : FText::GetEmpty();
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
					+ SHorizontalBox::Slot().AutoWidth()[Label(LOCTEXT("Seed", "Seed"), 190.f)]
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
					[SNew(STextBlock).Font(Font()).ColorAndOpacity(FLinearColor::White).Text(LOCTEXT("Summary", "Дальний вид (уровень 0: только высота и цвет)"))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Button(LOCTEXT("Live", "Вернуть игровое время"), [Self, P]()
						{
							if (Self.IsValid()) { Self->AgeSpeed = 0.f; Self->DaySpeed = 0.f; }
							if (P()) { P()->DevAge = -1.f; P()->DevDay = -1.f; P()->Refresh(); }
						})]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)[Button(LOCTEXT("Close", "Закрыть"), [Self]() { if (Self.IsValid()) Self->Toggle(); })]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
				[SNew(STextBlock).AutoWrapText(true).Font(Font(9)).ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					.Text(LOCTEXT("Hint", "Пока панель открыта, видна мышь: ходить можно как обычно, обзор мышью работает с зажатой кнопкой. F7 закрывает."))]
			]
		]
	];
	Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 50);
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

void UPlantDevSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	FActorSpawnParameters P;
	P.Name = TEXT("PlantDevTool");
	P.ObjectFlags |= RF_Transient;
	World.SpawnActor<APlantDevTool>(P);
}

#undef LOCTEXT_NAMESPACE
