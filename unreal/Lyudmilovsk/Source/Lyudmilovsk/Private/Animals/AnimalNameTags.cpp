#include "Animals/AnimalNameTags.h"

#include "AnimalSim.h"
#include "Animals/AnimalActor.h"
#include "Animals/AnimalLife.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HeroSeat.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

struct FAnimalTagView
{
	struct FLabel
	{
		int32 Id = -1;
		FVector2D Pixel;
		FText Name;
		bool bTarget = false;
		float Alpha = 1.f;
	};
	FVector2D ViewportSize = FVector2D(1, 1);
	TArray<FLabel> Labels;
};

namespace
{
	// the people's labels' colours (NpcDialogue.cpp)
	const FLinearColor Warm(1.f, 0.78f, 0.45f);
	const FLinearColor Paper(0.96f, 0.94f, 0.9f);
	const FLinearColor Shade(0.f, 0.f, 0.f, 0.62f);

	const FSlateBrush* Flat()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	class SAnimalTags : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAnimalTags) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedRef<FAnimalTagView> InView) { View = InView; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 15);
			const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
			const FText Hint = LyudText::Get(TEXT("ui.animal.call"));
			const FVector2D Scale = Geometry.GetLocalSize() / View->ViewportSize;
			for (const FAnimalTagView::FLabel& L : View->Labels)
			{
				const FVector2D NameSize = Measure->Measure(L.Name, NameFont);
				const FVector2D HintSize = L.bTarget ? Measure->Measure(Hint, HintFont) : FVector2D::ZeroVector;
				const FVector2D Box(FMath::Max(NameSize.X, HintSize.X), NameSize.Y + HintSize.Y);
				const FVector2D Pad(10, 5);
				const FVector2D Anchor = L.Pixel * Scale;
				const FVector2D TopLeft(Anchor.X - Box.X * 0.5f - Pad.X, Anchor.Y - Box.Y - Pad.Y * 2);
				FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Box + Pad * 2, FSlateLayoutTransform(TopLeft)),
					Flat(), ESlateDrawEffect::None, Shade * FLinearColor(1, 1, 1, L.Alpha * (L.bTarget ? 1.f : 0.7f)));
				float Y = TopLeft.Y + Pad.Y;
				FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(FVector2D(Anchor.X - NameSize.X * 0.5f, Y))),
					L.Name, NameFont, ESlateDrawEffect::None, Warm * FLinearColor(1, 1, 1, L.Alpha));
				if (L.bTarget)
				{
					Y += NameSize.Y;
					FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(HintSize, FSlateLayoutTransform(FVector2D(Anchor.X - HintSize.X * 0.5f, Y))),
						Hint, HintFont, ESlateDrawEffect::None, Paper * FLinearColor(1, 1, 1, L.Alpha));
				}
			}
			return Layer + 1;
		}

	private:
		TSharedPtr<FAnimalTagView> View;
	};
}

AAnimalNameTags::AAnimalNameTags()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // after the camera and the bodies moved
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

int32 AAnimalNameTags::NumLabels() const
{
	return View.IsValid() ? View->Labels.Num() : 0;
}

void AAnimalNameTags::BeginPlay()
{
	Super::BeginPlay();
	View = MakeShared<FAnimalTagView>();
	if (GEngine && GEngine->GameViewport)
	{
		Overlay = SNew(SAnimalTags, View.ToSharedRef()).Visibility(EVisibility::HitTestInvisible);
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 4);
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PC);
		if (InputComponent)
		{
			// several actors listen for E (doors, people, the hero's seat); none may swallow it
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAnimalNameTags::Call).bConsumeInput = false;
		}
	}
}

void AAnimalNameTags::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::EndPlay(Reason);
}

bool AAnimalNameTags::PlayerBusy() const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || Pawn->IsHidden())     // the sit-down cutscene, ATON OS
	{
		return true;
	}
	for (TActorIterator<AHeroSeat> It(GetWorld()); It; ++It)
	{
		if (It->IsPlayerInRange())     // E there means "sit down"
		{
			return true;
		}
	}
	return false;
}

void AAnimalNameTags::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CallCooldown = FMath::Max(0.f, CallCooldown - DeltaSeconds);
	View->Labels.Reset();
	Target = -1;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this);
	if (!PC || !PC->PlayerCameraManager || !Life || !Life->GetSim())
	{
		return;
	}
	int32 SizeX = 1, SizeY = 1;
	PC->GetViewportSize(SizeX, SizeY);
	View->ViewportSize = FVector2D(FMath::Max(SizeX, 1), FMath::Max(SizeY, 1));
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const bool bBusy = PlayerBusy();
	float BestDot = 0.8f;
	for (const LyudAnimals::FAnimal& A : Life->GetSim()->Animals)
	{
		if (A.NameKey.empty() && A.Name.empty())
		{
			continue;
		}
		FAnimalInfo Info;
		if (!Life->GetAnimal(A.Id, Info) || !Info.Actor)
		{
			continue;      // not drawn: too far
		}
		// over her head
		const AAnimalActor* Body = Cast<AAnimalActor>(Info.Actor);
		FVector Head = Info.Location + FVector(0, 0, 70.f);
		if (Body && Body->Body)
		{
			const FName HeadBone(TEXT("head"));
			if (Body->Body->GetBoneIndex(HeadBone) != INDEX_NONE)
			{
				Head = Body->Body->GetBoneLocation(HeadBone) + FVector(0, 0, 28.f);
			}
		}
		const float Dist = (float)FVector::Dist(Head, Cam);
		if (Dist > LabelRange || FVector::DotProduct(Head - Cam, Dir) <= 0.f)
		{
			continue;
		}
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AnimalTagSight), false, Info.Actor);
		if (const APawn* Pawn = PC->GetPawn())
		{
			Q.AddIgnoredActor(Pawn);
		}
		if (GetWorld()->LineTraceSingleByChannel(Hit, Cam, Head, ECC_Visibility, Q))
		{
			continue;      // behind a wall
		}
		FVector2D Pixel;
		if (!PC->ProjectWorldLocationToScreen(Head, Pixel, true))
		{
			continue;
		}
		FAnimalTagView::FLabel& L = View->Labels.AddDefaulted_GetRef();
		L.Id = A.Id;
		L.Pixel = Pixel;
		L.Name = FText::FromString(Info.Name);
		L.Alpha = FMath::Clamp((LabelRange - Dist) / 500.f, 0.f, 1.f);
		const float Dot = (float)FVector::DotProduct((Head - Cam).GetSafeNormal(), Dir);
		if (!bBusy && Dist < CallRange && Dot > BestDot)
		{
			BestDot = Dot;
			Target = A.Id;
		}
	}
	for (FAnimalTagView::FLabel& L : View->Labels)
	{
		L.bTarget = L.Id == Target;
		L.Alpha = L.bTarget ? 1.f : L.Alpha;
	}
}

void AAnimalNameTags::Call()
{
	if (Target < 0 || CallCooldown > 0.f || PlayerBusy())
	{
		return;
	}
	if (UAnimalLifeSubsystem* Life = UAnimalLifeSubsystem::Get(this))
	{
		if (Life->Hail(Target))
		{
			CallCooldown = 1.2f;
			UE_LOG(LogTemp, Log, TEXT("AnimalLife: the player called %d"), Target);
		}
	}
}
