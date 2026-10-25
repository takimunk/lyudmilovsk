#include "BirdHud.h"

#include "Birds/BirdPawn.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Bird.Debug"), 0, TEXT("1: the player bird's flight numbers on screen."));
	TAutoConsoleVariable<float> CVarHints(TEXT("lyud.Bird.HintSeconds"), 12.f, TEXT("How long the bird's controls stay on screen after becoming a bird."));

	void Ring(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, float From, float To, const FLinearColor& Col, float Thick)
	{
		TArray<FVector2D> P;
		const int32 N = FMath::Max(4, FMath::CeilToInt(48 * (To - From)));
		for (int32 I = 0; I <= N; I++)
		{
			const float A = 2.f * PI * FMath::Lerp(From, To, (float)I / N) - PI * 0.5f;
			P.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, Thick);
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& At, const FText& T, const FSlateFontInfo& Font, const FLinearColor& Col)
	{
		// a soft shadow, then the words
		const FVector2f Size(2000.f, 60.f);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(At + FVector2D(1.5, 1.5)))), T, Font, ESlateDrawEffect::None, FLinearColor(0, 0, 0, Col.A * 0.7f));
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(At))), T, Font, ESlateDrawEffect::None, Col);
	}
}

void SBirdHud::Construct(const FArguments&, ABirdPawn* InPawn)
{
	Pawn = InPawn;
	SetVisibility(EVisibility::HitTestInvisible);
	for (const TCHAR* K : { TEXT("bird.hint.look"), TEXT("bird.hint.beat"), TEXT("bird.hint.tuck"), TEXT("bird.hint.flare"), TEXT("bird.hint.magnet"), TEXT("bird.hint.actions"), TEXT("bird.hint.return") })
	{
		Hints.Add(LyudText::Get(K));
	}
	Returning = LyudText::Get(TEXT("bird.hud.returning"));
	AirPods = LyudText::Get(TEXT("bird.hint.airpods"));
	EnergyLabel = LyudText::Get(TEXT("bird.hud.energy"));
}

TSharedPtr<SBirdHud> SBirdHud::AddTo(ABirdPawn* InPawn)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	TSharedRef<SBirdHud> Hud = SNew(SBirdHud, InPawn);
	GEngine->GameViewport->AddViewportWidgetContent(Hud, 40);
	return Hud;
}

void SBirdHud::Remove(const TSharedPtr<SBirdHud>& Hud)
{
	if (Hud.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
}

int32 SBirdHud::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const ABirdPawn* P = Pawn.Get();
	if (!P)
	{
		return Layer;
	}
	const FVector2D Size = FVector2D(Geo.GetLocalSize());
	const FSlateFontInfo Title = FCoreStyle::GetDefaultFontStyle("Bold", 30);
	const FSlateFontInfo Body = FCoreStyle::GetDefaultFontStyle("Regular", 22);
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 15);

	// the controls, fading out after a while
	const float Hold = CVarHints.GetValueOnGameThread();
	const float Alpha = 1.f - FMath::Clamp((P->TimeAsBird() - Hold) / 3.f, 0.f, 1.f);
	if (Alpha > 0.f && P->GetMode() != EBirdMode::Leaving)
	{
		FVector2D At(48.f, Size.Y - 56.f - 34.f * (Hints.Num() + 2));
		Text(Out, Layer, Geo, At, LyudText::Get(TEXT("bird.hint.title")), Title, FLinearColor(1.f, 0.92f, 0.75f, Alpha));
		At.Y += 48.f;
		for (const FText& T : Hints)
		{
			Text(Out, Layer, Geo, At, T, Body, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha));
			At.Y += 34.f;
		}
		if (P->IsHeadTracked())
		{
			Text(Out, Layer, Geo, At, AirPods, Body, FLinearColor(0.7f, 0.95f, 1.f, 0.88f * Alpha));
		}
	}

	// R held: a ring round the centre fills
	const float Ret = P->ReturnProgress();
	if (Ret > 0.01f)
	{
		const FVector2D C = Size * 0.5f;
		Ring(Out, Layer, Geo, C, 34.f, 0.f, 1.f, FLinearColor(1, 1, 1, 0.18f), 2.f);
		Ring(Out, Layer + 1, Geo, C, 34.f, 0.f, FMath::Clamp(Ret, 0.f, 1.f), FLinearColor(1.f, 0.92f, 0.75f, 0.9f), 3.f);
		Text(Out, Layer + 2, Geo, C + FVector2D(-70.f, 46.f), Returning, Body, FLinearColor(1, 1, 1, 0.85f));
	}

	// where a flare will put the feet
	FVector Spot;
	const APlayerController* PC = Cast<APlayerController>(P->GetController());
	if (PC && P->HasLandingSpot(Spot) && GEngine && GEngine->GameViewport)
	{
		FVector2D Px;
		FVector2D Vp;
		GEngine->GameViewport->GetViewportSize(Vp);
		if (PC->ProjectWorldLocationToScreen(Spot, Px, true) && Vp.X > 0)
		{
			const FVector2D C = Px * (Size / Vp);
			Ring(Out, Layer, Geo, C, 14.f, 0.f, 1.f, FLinearColor(1.f, 0.95f, 0.8f, 0.55f), 1.5f);
			if (P->IsPulledToPerch())
			{
				// the pull: a faint thread from the bird to the perch it is drawn to
				TArray<FVector2D> Line = { FVector2D(Size.X * 0.5f, Size.Y * 0.62f), C };
				FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Line, ESlateDrawEffect::None, FLinearColor(1.f, 0.95f, 0.8f, 0.35f), true, 1.5f);
				Ring(Out, Layer, Geo, C, 22.f, 0.f, 1.f, FLinearColor(1.f, 0.95f, 0.8f, 0.3f), 1.f);
			}
		}
	}

	if (P->GetMode() != EBirdMode::Leaving && P->GetMode() != EBirdMode::Arriving)
	{
		// strength: a thin bar at the bottom, warm when the wings tire
		const float E = P->GetEnergy();
		const FVector2D BarSize(240.f, 6.f);
		const FVector2D At((Size.X - BarSize.X) * 0.5f, Size.Y - 64.f);
		const FLinearColor Col = FMath::Lerp(FLinearColor(1.f, 0.45f, 0.2f, 0.9f), FLinearColor(1.f, 1.f, 1.f, 0.75f), FMath::Clamp(E / 0.4f, 0.f, 1.f));
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(BarSize), FSlateLayoutTransform(FVector2f(At))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.35f));
		FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(BarSize.X * E, BarSize.Y), FSlateLayoutTransform(FVector2f(At))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, Col);
		Text(Out, Layer + 2, Geo, At + FVector2D(-70.f, -10.f), EnergyLabel, Small, FLinearColor(1, 1, 1, 0.7f));
	}
	// a note in the middle: what just happened (a fight, the sea, a cat...)
	const FString& Note = P->GetNote();
	if (!Note.IsEmpty() && P->GetNoteTime() > 0.f)
	{
		const float A = FMath::Clamp(P->GetNoteTime() / 0.6f, 0.f, 1.f);
		const FText T = LyudText::Get(Note);
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Body);
		Text(Out, Layer + 2, Geo, FVector2D((Size.X - TextSize.X) * 0.5f, Size.Y * 0.28f), T, Body, FLinearColor(1.f, 0.95f, 0.8f, A));
	}
	if (P->IsInDanger())
	{
		const FText T = LyudText::Get(TEXT("bird.note.danger"));
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Body);
		Text(Out, Layer + 2, Geo, FVector2D((Size.X - TextSize.X) * 0.5f, Size.Y * 0.2f), T, Body, FLinearColor(1.f, 0.45f, 0.35f, 0.95f));
	}
	if (CVarDebug.GetValueOnGameThread())
	{
		Text(Out, Layer, Geo, FVector2D(40.f, 40.f), FText::FromString(P->DebugLine()), Small, FLinearColor(0.8f, 1.f, 0.8f, 1.f));
	}
	return Layer + 3;
}
