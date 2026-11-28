// The helmsman's HUD and view: drawn from ALyudDinghy::HudData() only (the boat fills it once a frame).
// docs/yacht_school.md "Engine: the boats".
#include "LyudSailHud.h"

#include "Camera/CameraTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Sailing/LyudDinghy.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudUIStyle.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSailHudMode(TEXT("lyud.Sail.Hud"), 2,
		TEXT("lyud.Sail.Hud 0|1|2: the helmsman's HUD. 0 off; 1 the wind rose, the speed and the words; 2 the heel, sheet and board gauges too."));

	FLinearColor SailHudFade(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	FVector2D SailHudMeasure(const FText& T, const FSlateFontInfo& Font)
	{
		const FVector2D Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Font);
		return Measured;
	}

	/** The words with a soft shadow under them. Align: 0 from At, 0.5 centred on it, 1 ending at it. */
	void SailHudText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& At, const FText& T, const FSlateFontInfo& Font,
		const FLinearColor& Col, float Align = 0.f)
	{
		if (T.IsEmpty() || Col.A <= 0.f)
		{
			return;
		}
		FVector2D P = At;
		if (Align > 0.f)
		{
			P.X -= SailHudMeasure(T, Font).X * Align;
		}
		const FVector2f Size(2400.f, 120.f);
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(P + FVector2D(1.5, 1.5)))), T, Font, ESlateDrawEffect::None,
			FLinearColor(0.f, 0.f, 0.f, Col.A * 0.7f));
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(P))), T, Font, ESlateDrawEffect::None, Col);
	}

	void SailHudBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& At, const FVector2D& Size, const FLinearColor& Col)
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(At))), FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None, Col);
	}

	void SailHudLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& A, const FVector2D& B, const FLinearColor& Col, float Thick)
	{
		const TArray<FVector2D> P = { A, B };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, Thick);
	}

	/** On the rose the bow is up and angles run clockwise (to starboard): the screen's way of an angle off the bow. */
	FVector2D SailHudWay(float Rad)
	{
		return FVector2D(FMath::Sin(Rad), -FMath::Cos(Rad));
	}

	void SailHudArc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, float From, float To, const FLinearColor& Col, float Thick)
	{
		TArray<FVector2D> P;
		const int32 N = FMath::Max(3, FMath::CeilToInt(FMath::Abs(To - From) * 10.f));
		P.Reserve(N + 1);
		for (int32 I = 0; I <= N; ++I)
		{
			P.Add(C + SailHudWay(FMath::Lerp(From, To, float(I) / float(N))) * R);
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, Thick);
	}

	/** A hatched wedge between two radii (one polyline going in and out across it). */
	void SailHudWedge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R0, float R1, float From, float To, const FLinearColor& Col)
	{
		TArray<FVector2D> P;
		const int32 N = FMath::Max(4, FMath::CeilToInt(FMath::Abs(To - From) * 14.f));
		P.Reserve(2 * N + 2);
		for (int32 I = 0; I <= N; ++I)
		{
			const FVector2D W = SailHudWay(FMath::Lerp(From, To, float(I) / float(N)));
			const bool bOut = (I % 2) == 0;
			P.Add(C + W * (bOut ? R0 : R1));
			P.Add(C + W * (bOut ? R1 : R0));
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), P, ESlateDrawEffect::None, Col, true, 3.f);
	}

	/** A bar with a dark bed: From..To of its width filled (0..1 each). */
	void SailHudBar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& At, const FVector2D& Size, float From, float To, const FLinearColor& Col)
	{
		SailHudBox(Out, Layer, Geo, At, Size, FLinearColor(0.f, 0.f, 0.f, 0.4f));
		const float A = FMath::Clamp(FMath::Min(From, To), 0.f, 1.f), B = FMath::Clamp(FMath::Max(From, To), 0.f, 1.f);
		SailHudBox(Out, Layer + 1, Geo, At + FVector2D(Size.X * A, 0.0), FVector2D(Size.X * (B - A), Size.Y), Col);
	}

	FText SailHudNumber(float Value, int32 Decimals, const FText& Unit)
	{
		// no "%.*f": the format checker of FString::Printf does not take a star precision
		const FString Number = Decimals <= 0 ? FString::Printf(TEXT("%.0f"), Value) : FString::Printf(TEXT("%.1f"), Value);
		return FText::FromString(Number + TEXT(" ") + Unit.ToString());
	}
}

int32 LyudSailHud::Mode()
{
	return FMath::Clamp(CVarSailHudMode.GetValueOnGameThread(), 0, 2);
}

FText LyudSailHud::Word(const FString& Key, const TCHAR* Fallback)
{
	// LyudText gives the key back when no table has it: then the English draft, so nothing reads "ui.sail.hud..."
	const FText T = LyudText::Get(Key);
	if (Fallback && (T.IsEmpty() || T.ToString() == Key))
	{
		return FText::FromString(Fallback);
	}
	return T;
}

void SLyudSailHud::Construct(const FArguments&, ALyudDinghy* InBoat)
{
	Boat = InBoat;
	SetVisibility(EVisibility::HitTestInvisible);
	WordHeel = LyudSailHud::Word(TEXT("ui.sail.hud.heel"), TEXT("Heel"));
	WordSheet = LyudSailHud::Word(TEXT("ui.sail.hud.sheet"), TEXT("Sheet"));
	WordBoard = LyudSailHud::Word(TEXT("ui.sail.hud.board"), TEXT("Board"));
	WordWater = LyudSailHud::Word(TEXT("ui.sail.hud.water"), TEXT("Water aboard"));
	WordKn = LyudSailHud::Word(TEXT("ui.sail.hud.kn"), TEXT("kn"));
	WordMs = LyudSailHud::Word(TEXT("ui.sail.hud.ms"), TEXT("m/s"));
	WordM = LyudSailHud::Word(TEXT("ui.sail.hud.m"), TEXT("m"));
	WordShallow = LyudSailHud::Word(TEXT("ui.sail.hud.shallow"), TEXT("Shallow water"));
	WordCapsized = LyudSailHud::Word(TEXT("ui.sail.hud.capsized"), TEXT("Capsized: hold Space to stand on the board and right her"));
	WordPlaning = LyudSailHud::Word(TEXT("ui.sail.hud.planing"), TEXT("Planing"));
	WordWind = LyudSailHud::Word(TEXT("ui.sail.hud.wind"), TEXT("Wind"));
	WordAuto = LyudSailHud::Word(TEXT("ui.sail.hud.auto"), TEXT("auto"));
}

TSharedPtr<SLyudSailHud> SLyudSailHud::AddTo(ALyudDinghy* InBoat)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	TSharedRef<SLyudSailHud> Hud = SNew(SLyudSailHud, InBoat);
	GEngine->GameViewport->AddViewportWidgetContent(Hud, 40);
	return Hud;
}

void SLyudSailHud::Remove(const TSharedPtr<SLyudSailHud>& Hud)
{
	if (Hud.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
}

int32 SLyudSailHud::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const ALyudDinghy* Dinghy = Boat.Get();
	const FLyudSailHudData* Data = Dinghy ? Dinghy->HudData() : nullptr;
	const int32 Mode = LyudSailHud::Mode();
	if (!Data || Data->bHidden || Mode <= 0)
	{
		return Layer;
	}
	const FLyudSailHudData& H = *Data;
	const FVector2D Size = FVector2D(Geo.GetLocalSize());
	const FSlateFontInfo Big = LyudUI::Font(30, "Bold");
	const FSlateFontInfo Head = LyudUI::Font(20, "Bold");
	const FSlateFontInfo Body = LyudUI::Font(16);
	const FSlateFontInfo Small = LyudUI::Font(13);
	// 1 = sailing, swamped, aground; the capsize set are 4..6 of LyudSail::EState (Capsized, Turtled, Righting)
	const bool bOver = H.State >= 4 && H.State <= 6;

	// ---- the wind rose, bow up: the no-go zone round the wind, the wind's arrow, the apparent wind's tick, the mark
	const float R = 84.f;
	const FVector2D C(Size.X - 150.f, Size.Y - 190.f);
	{
		SailHudArc(Out, Layer, Geo, C, R, 0.f, 2.f * PI, SailHudFade(LyudUI::Dim, 0.8f), 1.5f);
		// the boat: a leaf, bow up
		const TArray<FVector2D> Hull = { C + FVector2D(0.0, -26.0), C + FVector2D(9.0, -6.0), C + FVector2D(8.0, 20.0), C + FVector2D(-8.0, 20.0), C + FVector2D(-9.0, -6.0),
			C + FVector2D(0.0, -26.0) };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Hull, ESlateDrawEffect::None, LyudUI::Bright, true, 1.5f);
		SailHudLine(Out, Layer, Geo, C + FVector2D(0.0, -R), C + FVector2D(0.0, -R - 9.0), LyudUI::Bright, 2.f);
		if (H.TwsMs > 0.3f)
		{
			// the wind comes from Twa off the bow; she cannot point closer to it than the beat angle
			const bool bInside = FMath::Abs(H.TwaRad) < H.BeatRad;
			SailHudWedge(Out, Layer, Geo, C, R * 0.42f, R - 3.f, H.TwaRad - H.BeatRad, H.TwaRad + H.BeatRad, SailHudFade(LyudUI::Bad, bInside ? 0.75f : 0.32f));
			SailHudLine(Out, Layer, Geo, C + SailHudWay(H.TwaRad - H.BeatRad) * (R * 0.42f), C + SailHudWay(H.TwaRad - H.BeatRad) * R, SailHudFade(LyudUI::Bad, 0.7f), 1.5f);
			SailHudLine(Out, Layer, Geo, C + SailHudWay(H.TwaRad + H.BeatRad) * (R * 0.42f), C + SailHudWay(H.TwaRad + H.BeatRad) * R, SailHudFade(LyudUI::Bad, 0.7f), 1.5f);
			// the arrow flies with the wind: from outside the ring in toward the boat
			const FVector2D W = SailHudWay(H.TwaRad);
			const FVector2D Side(-W.Y, W.X);
			const FVector2D Tip = C + W * (R * 0.5f), Tail = C + W * (R + 22.f);
			SailHudLine(Out, Layer + 2, Geo, Tail, Tip, LyudUI::Bright, 3.f);
			SailHudLine(Out, Layer + 2, Geo, Tip, Tip + W * 14.0 + Side * 8.0, LyudUI::Bright, 3.f);
			SailHudLine(Out, Layer + 2, Geo, Tip, Tip + W * 14.0 - Side * 8.0, LyudUI::Bright, 3.f);
			// what she feels: the apparent wind, a tick on the ring
			const FVector2D A = SailHudWay(H.AwaRad);
			SailHudLine(Out, Layer + 2, Geo, C + A * (R - 8.f), C + A * (R + 8.f), LyudUI::Text, 2.5f);
		}
		SailHudText(Out, Layer + 2, Geo, C + FVector2D(0.0, R + 30.0), FText::FromString(WordWind.ToString() + FString::Printf(TEXT(" %.1f "), H.TwsMs) + WordMs.ToString()), Small,
			LyudUI::Body, 0.5f);
		if (H.bHasMark)
		{
			// the next mark: a diamond on the ring where it bears, its distance beside the rose
			const FVector2D M = C + SailHudWay(H.MarkBearingRad) * (R + 2.f);
			const TArray<FVector2D> Diamond = { M + FVector2D(0.0, -9.0), M + FVector2D(8.0, 0.0), M + FVector2D(0.0, 9.0), M + FVector2D(-8.0, 0.0), M + FVector2D(0.0, -9.0) };
			FSlateDrawElement::MakeLines(Out, Layer + 3, Geo.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, LyudUI::Good, true, 3.f);
			SailHudLine(Out, Layer + 1, Geo, C + SailHudWay(H.MarkBearingRad) * 30.f, C + SailHudWay(H.MarkBearingRad) * (R - 8.f), SailHudFade(LyudUI::Good, 0.5f), 1.5f);
			SailHudText(Out, Layer + 2, Geo, C + FVector2D(0.0, -R - 42.0), SailHudNumber(H.MarkDistM, 0, WordM), Head, LyudUI::Good, 0.5f);
		}
	}

	// ---- the point of sail and the speed, left of the rose
	{
		const FVector2D At(C.X - R - 40.f, C.Y - 46.f);
		SailHudText(Out, Layer + 2, Geo, At, H.PointName, Head, H.bByTheLee ? LyudUI::Bad : LyudUI::Text, 1.f);
		SailHudText(Out, Layer + 2, Geo, At + FVector2D(0.0, 26.0), SailHudNumber(FMath::Abs(H.SpeedMs) * 1.94384f, 1, WordKn), Big, LyudUI::Bright, 1.f);
		SailHudText(Out, Layer + 2, Geo, At + FVector2D(0.0, 66.0), SailHudNumber(FMath::Abs(H.SpeedMs), 1, WordMs), Small, LyudUI::Body, 1.f);
		if (H.bPlaning)
		{
			SailHudText(Out, Layer + 2, Geo, At + FVector2D(0.0, 86.0), WordPlaning, Small, LyudUI::Good, 1.f);
		}
	}

	// ---- the gauges: heel, sheet (with where it should be), board, water aboard
	if (Mode >= 2 && !bOver)
	{
		const FVector2D BarSize(190.f, 7.f);
		FVector2D At(C.X - R - 40.f - BarSize.X, C.Y + 62.f);
		{
			// the heel: from the middle to the side she leans, greener the flatter
			const float Lean = FMath::Clamp(H.HeelDeg / 45.f, -1.f, 1.f);
			const float Much = FMath::Clamp((FMath::Abs(H.HeelDeg) - 12.f) / 18.f, 0.f, 1.f);
			SailHudText(Out, Layer + 2, Geo, At + FVector2D(-10.0, -8.0), FText::FromString(WordHeel.ToString() + FString::Printf(TEXT(" %.0f\u00B0"), FMath::Abs(H.HeelDeg))), Small,
				LyudUI::Body, 1.f);
			SailHudBar(Out, Layer, Geo, At, BarSize, 0.5f, 0.5f + 0.5f * Lean, FMath::Lerp(LyudUI::Good, LyudUI::Bad, Much));
			SailHudBox(Out, Layer + 2, Geo, At + FVector2D(BarSize.X * 0.5 - 1.0, -3.0), FVector2D(2.0, BarSize.Y + 6.0), LyudUI::Bright);
		}
		At.Y += 24.f;
		{
			// the sheet: 0 hard in .. 1 right out; the mark is where the sail would draw best; red while it flaps
			const FText Label = H.bEasySheet ? FText::FromString(WordSheet.ToString() + TEXT(" (") + WordAuto.ToString() + TEXT(")")) : WordSheet;
			SailHudText(Out, Layer + 2, Geo, At + FVector2D(-10.0, -8.0), Label, Small, LyudUI::Body, 1.f);
			SailHudBar(Out, Layer, Geo, At, BarSize, 0.f, H.Sheet, H.Luff > 0.35f ? LyudUI::Bad : LyudUI::Text);
			SailHudBox(Out, Layer + 2, Geo, At + FVector2D(BarSize.X * FMath::Clamp(H.IdealSheet, 0.f, 1.f) - 1.5, -4.0), FVector2D(3.0, BarSize.Y + 8.0), LyudUI::Good);
		}
		At.Y += 24.f;
		{
			SailHudText(Out, Layer + 2, Geo, At + FVector2D(-10.0, -8.0), WordBoard, Small, LyudUI::Body, 1.f);
			SailHudBar(Out, Layer, Geo, At, BarSize, 0.f, H.Board, H.bShallow ? LyudUI::Bad : LyudUI::Dim);
		}
		if (H.Water > 0.04f)
		{
			At.Y += 24.f;
			SailHudText(Out, Layer + 2, Geo, At + FVector2D(-10.0, -8.0), WordWater, Small, LyudUI::Body, 1.f);
			SailHudBar(Out, Layer, Geo, At, BarSize, 0.f, H.Water, FLinearColor(0.35f, 0.6f, 0.85f, 0.9f));
		}
	}

	// ---- the words in the middle: the tutor's hint, shallow water, the capsize
	float MidY = Size.Y * 0.22f;
	if (H.HintLeft > 0.f && !H.Hint.IsEmpty())
	{
		SailHudText(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5, MidY), H.Hint, LyudUI::Font(24, "Bold"), SailHudFade(LyudUI::Bright, FMath::Clamp(H.HintLeft / 0.6f, 0.f, 1.f)), 0.5f);
		MidY += 40.f;
	}
	if (H.bShallow && !bOver)
	{
		const float Blink = 0.65f + 0.35f * FMath::Sin(float(FPlatformTime::Seconds()) * 6.f);
		SailHudText(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5, MidY), FText::FromString(WordShallow.ToString() + FString::Printf(TEXT(": %.1f "), FMath::Max(0.f, H.DepthM)) + WordM.ToString()),
			Head, SailHudFade(LyudUI::Bad, Blink), 0.5f);
		MidY += 34.f;
	}
	if (bOver)
	{
		SailHudText(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5, MidY), WordCapsized, Head, LyudUI::Bright, 0.5f);
		const FVector2D BarSize(260.f, 8.f);
		SailHudBar(Out, Layer, Geo, FVector2D((Size.X - BarSize.X) * 0.5, MidY + 36.0), BarSize, 0.f, H.Righting, LyudUI::Good);
	}

	// ---- the school's task: the lesson's or the race's lines, top left; the countdown, big in its last ten seconds
	if (!H.Title.IsEmpty() || H.Lines.Num() > 0)
	{
		FVector2D At(48.f, 120.f);
		float Wide = SailHudMeasure(H.Title, Head).X;
		for (const FText& Line : H.Lines)
		{
			Wide = FMath::Max(Wide, float(SailHudMeasure(Line, Body).X));
		}
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(Wide + 28.f, 40.f + 24.f * H.Lines.Num()), FSlateLayoutTransform(FVector2f(At - FVector2D(14.0, 8.0)))),
			LyudUI::HudBrush(), ESlateDrawEffect::None, FLinearColor::White);
		SailHudText(Out, Layer + 1, Geo, At, H.Title, Head, LyudUI::Text);
		At.Y += 30.f;
		for (const FText& Line : H.Lines)
		{
			SailHudText(Out, Layer + 1, Geo, At, Line, Body, LyudUI::Body);
			At.Y += 24.f;
		}
	}
	if (H.Countdown > 0.f)
	{
		const int32 Whole = FMath::CeilToInt(H.Countdown);
		if (H.Countdown <= 10.f)
		{
			// each second swells as it comes
			const float Beat = H.Countdown - FMath::FloorToFloat(H.Countdown);
			SailHudText(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5, Size.Y * 0.08), FText::AsNumber(Whole), LyudUI::Font(FMath::RoundToInt(64.f + 14.f * Beat), "Bold"),
				Whole <= 3 ? LyudUI::Bad : LyudUI::Bright, 0.5f);
		}
		else
		{
			SailHudText(Out, Layer + 2, Geo, FVector2D(Size.X * 0.5, 44.0), FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60)), LyudUI::Font(28, "Bold"),
				LyudUI::Text, 0.5f);
		}
	}

	// ---- the keys, for a while after boarding: one to a line, bottom left
	if (H.KeysLeft > 0.f && !H.Keys.IsEmpty())
	{
		TArray<FString> Items;
		H.Keys.ToString().ParseIntoArray(Items, TEXT("   "), true);
		const float A = FMath::Clamp(H.KeysLeft / 2.f, 0.f, 1.f);
		FVector2D At(48.f, Size.Y - 60.f - 22.f * Items.Num());
		for (const FString& Item : Items)
		{
			SailHudText(Out, Layer + 1, Geo, At, FText::FromString(Item.TrimStartAndEnd()), Body, SailHudFade(LyudUI::Body, 0.9f * A));
			At.Y += 22.f;
		}
	}

	// ---- lyud.Sail.Debug
	if (!H.Debug.IsEmpty())
	{
		TArray<FString> Items;
		H.Debug.ParseIntoArray(Items, TEXT("  "), true);
		FVector2D At(48.f, 40.f);
		FString Row;
		for (int32 I = 0; I < Items.Num(); ++I)
		{
			Row += Items[I] + TEXT("   ");
			if (Row.Len() > 110 || I == Items.Num() - 1)
			{
				SailHudText(Out, Layer + 1, Geo, At, FText::FromString(Row), Small, FLinearColor(0.8f, 1.f, 0.8f, 1.f));
				At.Y += 18.f;
				Row.Reset();
			}
		}
	}
	return Layer + 5;
}

// ------------------------------------------------------------------ the view

bool ULyudSailCamera::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	// eased: the hull snatches at every wave, the head does not
	Roll = FMath::FInterpTo(Roll, WantRoll, DeltaTime, 3.5f);
	Pitch = FMath::FInterpTo(Pitch, WantPitch, DeltaTime, 3.5f);
	Drop = FMath::FInterpTo(Drop, WantDrop, DeltaTime, 3.f);
	InOutPOV.Rotation.Roll += Roll;
	InOutPOV.Rotation.Pitch = FMath::Clamp(InOutPOV.Rotation.Pitch + double(Pitch), -89.0, 89.0);
	InOutPOV.Location.Z -= Drop;
	return false;
}
