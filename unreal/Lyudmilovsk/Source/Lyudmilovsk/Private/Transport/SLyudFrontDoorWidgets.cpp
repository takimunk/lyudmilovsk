#include "SLyudFrontDoorWidgets.h"

#include "CablewaySim.h"
#include "FrontDoor.h"
#include "StationLife.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateBrush.h"
#include "Transport/LyudTransport.h"
#include "UI/LyudUIStyle.h"
#include "SLyudTransitWidgets.h"

namespace LyudFlapImpl
{
	// letters a flap shows on its way to the right one
	const TCHAR* const Scramble = TEXT("АБВГДЕЖЗИКЛМНОПРСТУФ0123456789:ABCDEFGHKMNPRSTUVXYZ");

	FString StationWord(const LyudTransport::FNetwork& N, int32 S)
	{
		return S >= 0 && S < (int32)N.Stations.size() ? LyudText::Get(UTF8_TO_TCHAR(N.Stations[(size_t)S].Name.c_str())).ToString() : FString();
	}

	/** Text padded or cut to exactly W cells. */
	FString Fit(const FString& S, int32 W, bool bRight = false)
	{
		FString T = S.Left(W);
		const FString Pad = FString::ChrN(W - T.Len(), TEXT(' '));
		return bRight ? Pad + T : T + Pad;
	}

	void Box(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& C, float Radius = 0.f)
	{
		const FSlateRoundedBoxBrush Brush(C, Radius);
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos))), &Brush, ESlateDrawEffect::None, C);
	}

	void Text(FSlateWindowElementList& Out, const FGeometry& G, int32 Layer, const FString& S, const FVector2D& Centre, const FSlateFontInfo& F, const FLinearColor& C,
		float Align = 0.5f)
	{
		const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D TS = M->Measure(S, F);
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2f(TS), FSlateLayoutTransform(FVector2f(Centre - FVector2D(TS.X * Align, TS.Y * 0.5)))),
			S, F, ESlateDrawEffect::None, C);
	}
}

// --- the departures board --------------------------------------------------------------------------------------------

void SLyudFlapBoard::Construct(const FArguments& InArgs)
{
	Station = InArgs._Station;
	Line = InArgs._Line;
	Rebuild();
	// the letters start where they should be (no clatter when the hall comes into being)
	for (int32 R = 0; R < Rows; R++)
	{
		for (int32 C = 0; C < Cols; C++)
		{
			Cells[R][C].Shown = Cells[R][C].Target;
			Cells[R][C].Steps = 0;
		}
	}
	FlipCount = 0;
	bFlipping = false;
	RegisterActiveTimer(0.06f, FWidgetActiveTimerDelegate::CreateSP(this, &SLyudFlapBoard::Step));
}

void SLyudFlapBoard::SetRow(int32 Row, const FString& Text)
{
	const FString T = LyudFlapImpl::Fit(Text, Cols);
	bool bChanged = false;
	for (int32 C = 0; C < Cols; C++)
	{
		FCell& Cell = Cells[Row][C];
		if (Cell.Target != T[C])
		{
			Cell.Target = T[C];
			// each letter goes round a few others first; the left ones settle first, like a row of real flaps
			Cell.Steps = 2 + (int32)((uint32)(Row * 31 + C * 7 + (int32)Spin) % 4u) + C / 6;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		FlipCount++;
		bFlipping = true;
	}
}

void SLyudFlapBoard::Rebuild()
{
	using namespace LyudFrontDoor;
	ULyudTransportSubsystem* T = LyudTransit::Active();
	const LyudTransport::FNetwork* N = T ? T->GetNet() : nullptr;
	if (!N || Station < 0 || Line < 0 || Line >= (int32)N->Lines.size())
	{
		return;
	}
	std::vector<FDeparture> Deps;
	Departures(*N, T->GetLife(), Station, Line, T->LifeHour(), Rows, Deps);
	Spin++;
	for (int32 R = 0; R < Rows; R++)
	{
		if (R >= (int32)Deps.size())
		{
			SetRow(R, FString());
			continue;
		}
		const FDeparture& D = Deps[(size_t)R];
		const TCHAR* StatusKey = D.Kind == EDepKind::Boarding ? TEXT("frontdoor.board.boarding")
			: D.Kind == EDepKind::Arriving ? TEXT("frontdoor.board.arriving")
			: D.Kind == EDepKind::Planned ? TEXT("frontdoor.board.planned") : TEXT("frontdoor.board.on_call");
		const FString Cabin = D.Cabin >= 0 && D.Cabin < (int32)N->Cabins.size()
			? LyudText::Get(TEXT("frontdoor.board.cabin"), { FStringFormatArg(N->Cabins[(size_t)D.Cabin].Number) }).ToString() : FString(TEXT("—"));
		// 05 time | 1 | 12 where | 1 | 4 cabin | 1 | 6 what
		const FString Row = LyudFlapImpl::Fit(UTF8_TO_TCHAR(Clock(D.Hour).c_str()), 5) + TEXT(" ")
			+ LyudFlapImpl::Fit(LyudFlapImpl::StationWord(*N, D.To), 12) + TEXT(" ")
			+ LyudFlapImpl::Fit(Cabin, 4) + TEXT(" ")
			+ LyudFlapImpl::Fit(LyudText::Get(StatusKey).ToString(), 7);
		SetRow(R, Row);
	}
}

EActiveTimerReturnType SLyudFlapBoard::Step(double, float Dt)
{
	Refresh -= Dt;
	if (Refresh <= 0.f)
	{
		Refresh = 4.f;
		Rebuild();
	}
	if (bFlipping)
	{
		bool bAny = false;
		const int32 NS = FCString::Strlen(LyudFlapImpl::Scramble);
		for (int32 R = 0; R < Rows; R++)
		{
			for (int32 C = 0; C < Cols; C++)
			{
				FCell& Cell = Cells[R][C];
				if (Cell.Steps <= 0)
				{
					continue;
				}
				Cell.Steps--;
				Cell.Shown = Cell.Steps == 0 ? Cell.Target : LyudFlapImpl::Scramble[(uint32)(R * 13 + C * 5 + Cell.Steps * 3 + (int32)Spin) % (uint32)NS];
				bAny = true;
			}
		}
		bFlipping = bAny;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return EActiveTimerReturnType::Continue;
}

FString SLyudFlapBoard::Describe() const
{
	FString Out;
	for (int32 R = 0; R < Rows; R++)
	{
		FString Row;
		for (int32 C = 0; C < Cols; C++)
		{
			Row.AppendChar(Cells[R][C].Target);
		}
		Out += Row.TrimEnd() + TEXT(" | ");
	}
	return Out;
}

int32 SLyudFlapBoard::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	const float U = Size.Y / 560.f;
	// a black board in a brass frame, the line's name over the rows, the rows' flaps in dark tiles with a hairline across
	LyudFlapImpl::Box(Out, G, LayerId, FVector2D::ZeroVector, Size, FLinearColor(0.62f, 0.46f, 0.2f), 12.f * U);
	LyudFlapImpl::Box(Out, G, LayerId + 1, FVector2D(10.f, 10.f) * U, Size - FVector2D(20.f, 20.f) * U, FLinearColor(0.02f, 0.02f, 0.022f), 6.f * U);
	FString Title = LyudText::Get(TEXT("frontdoor.board.title")).ToString();
	if (ULyudTransportSubsystem* T = LyudTransit::Active())
	{
		if (const LyudTransport::FNetwork* N = T->GetNet())
		{
			if (Line >= 0 && Line < (int32)N->Lines.size())
			{
				Title = LyudText::Get(TEXT("frontdoor.board.title_line"), { FStringFormatArg(LyudText::Get(UTF8_TO_TCHAR(N->Lines[(size_t)Line].Name.c_str())).ToString()) }).ToString();
			}
		}
	}
	const FLinearColor Ivory(0.95f, 0.9f, 0.78f), Flap(0.075f, 0.07f, 0.068f), Letter(0.97f, 0.93f, 0.8f), Hair(0.f, 0.f, 0.f);
	LyudFlapImpl::Text(Out, G, LayerId + 2, Title, FVector2D(40.f * U, 52.f * U), LyudUI::Font(FMath::RoundToInt(36 * U), "Bold"), Ivory, 0.f);
	const float CellW = (Size.X - 60.f * U) / Cols, CellH = 82.f * U, Top = 100.f * U;
	const FSlateFontInfo F = LyudUI::Font(FMath::RoundToInt(40 * U), "Bold");
	for (int32 R = 0; R < Rows; R++)
	{
		const float Y = Top + R * (CellH + 6.f * U);
		for (int32 C = 0; C < Cols; C++)
		{
			const FVector2D At(30.f * U + C * CellW, Y);
			const FVector2D Cell(CellW - 3.f * U, CellH);
			LyudFlapImpl::Box(Out, G, LayerId + 2, At, Cell, Flap, 3.f * U);
			LyudFlapImpl::Box(Out, G, LayerId + 4, At + FVector2D(0.f, CellH * 0.5f - 1.f * U), FVector2D(Cell.X, 2.f * U), Hair);
			const TCHAR Ch = Cells[R][C].Shown;
			if (Ch != TEXT(' '))
			{
				LyudFlapImpl::Text(Out, G, LayerId + 3, FString::Chr(Ch), At + Cell * 0.5f, F, Cells[R][C].Steps > 0 ? Letter * 0.75f : Letter);
			}
		}
	}
	return LayerId + 5;
}

// --- the club's sign ------------------------------------------------------------------------------------------------

int32 SLyudClubSign::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	const float U = Size.Y / 360.f;
	// the club's own lettering (Cyrillic geometry in every language, like the restaurant's): a dark plate, a bulb row along
	// its edges, the name in tall letters; lit at dusk the letters glow and the bulbs shine
	const FLinearColor Plate(0.05f, 0.045f, 0.04f), Brass(0.55f, 0.42f, 0.2f);
	const FLinearColor Glow = bLit ? FLinearColor(1.f, 0.78f, 0.42f) * 3.f : FLinearColor(0.42f, 0.33f, 0.18f);
	LyudFlapImpl::Box(Out, G, LayerId, FVector2D::ZeroVector, Size, Brass, 24.f * U);
	LyudFlapImpl::Box(Out, G, LayerId + 1, FVector2D(14.f, 14.f) * U, Size - FVector2D(28.f, 28.f) * U, Plate, 16.f * U);
	const int32 Bulbs = 22;
	for (int32 I = 0; I < Bulbs; I++)
	{
		const float X = 40.f * U + (Size.X - 80.f * U) * I / (Bulbs - 1);
		for (const float Y : { 30.f * U, Size.Y - 30.f * U })
		{
			LyudFlapImpl::Box(Out, G, LayerId + 2, FVector2D(X - 7.f * U, Y - 7.f * U), FVector2D(14.f, 14.f) * U, bLit ? FLinearColor(1.f, 0.85f, 0.55f) * 4.f : Brass * 0.6f, 7.f * U);
		}
	}
	LyudFlapImpl::Text(Out, G, LayerId + 2, LyudText::Get(TEXT("frontdoor.sign")).ToString(), Size * 0.5f, LyudUI::Font(FMath::RoundToInt(200 * U), "Bold"), Glow);
	return LayerId + 3;
}
