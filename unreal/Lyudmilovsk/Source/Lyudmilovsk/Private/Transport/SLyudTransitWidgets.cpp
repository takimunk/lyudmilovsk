// The cable-car metro's boards and route diagram (SLyudTransitWidgets.h).
#include "SLyudTransitWidgets.h"

#include "CablewaySim.h"
#include "TransportStd.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Transport/LyudTransport.h"
#include "UI/LyudUIStyle.h"
#include "Plants/GameCalendar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace LyudTransit
{
	static TWeakObjectPtr<ULyudTransportSubsystem> GActive;

	ULyudTransportSubsystem* Active()
	{
		return GActive.Get();
	}

	void SetActive(ULyudTransportSubsystem* T)
	{
		GActive = T;
	}

	FText WaitText(float Seconds)
	{
		if (Seconds < 0.f)
		{
			return FText::FromString(TEXT("—"));
		}
		if (Seconds < 15.f)
		{
			return LyudText::Get(TEXT("transport.wait.arriving"));
		}
		if (Seconds < 90.f)
		{
			return LyudText::Get(TEXT("transport.wait.sec"), { FStringFormatArg(FMath::RoundToInt(Seconds / 10.f) * 10) });
		}
		return LyudText::Get(TEXT("transport.wait.min"), { FStringFormatArg(FMath::RoundToInt(Seconds / 60.f)) });
	}
}

namespace
{
	using LyudTransit::Active;

	const LyudTransport::FNetwork* Net()
	{
		ULyudTransportSubsystem* T = Active();
		return T ? T->GetNet() : nullptr;
	}

	FLinearColor LineColor(const LyudTransport::FLine& L)
	{
		return FLinearColor(FColor::FromHex(FString(UTF8_TO_TCHAR(L.Color.c_str()))));
	}

	FString Key(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	FString StationName(int32 S)
	{
		const LyudTransport::FNetwork* N = Net();
		return N && SimValid(N->Stations, S) ? LyudText::Get(Key(N->Stations[S].Name)).ToString() : FString();
	}

	struct FPainter
	{
		const FGeometry& G;
		FSlateWindowElementList& Out;
		TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		FPainter(const FGeometry& InG, FSlateWindowElementList& InOut) : G(InG), Out(InOut) {}

		FPaintGeometry Box(const FVector2D& Pos, const FVector2D& Size) const
		{
			return G.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Pos)));
		}
		void Rect(int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& C, float Radius = 0.f) const
		{
			const FSlateRoundedBoxBrush Brush(C, Radius);
			FSlateDrawElement::MakeBox(Out, Layer, Box(Pos, Size), &Brush, ESlateDrawEffect::None, C);
		}
		FVector2D TextSize(const FString& S, const FSlateFontInfo& F) const { return Measure->Measure(S, F); }
		/** Text with its anchor: Align 0 left, 0.5 centre, 1 right (x); y is the middle. */
		void Text(int32 Layer, const FString& S, const FVector2D& At, const FSlateFontInfo& F, const FLinearColor& C, float Align = 0.f) const
		{
			const FVector2D TS = TextSize(S, F);
			FSlateDrawElement::MakeText(Out, Layer, Box(At - FVector2D(TS.X * Align, TS.Y * 0.5), TS), S, F, ESlateDrawEffect::None, C);
		}
		void Lines(int32 Layer, const TArray<FVector2D>& Pts, const FLinearColor& C, float W) const
		{
			FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, C, true, W);
		}
		void Disc(int32 Layer, const FVector2D& C, float R, const FLinearColor& Fill) const
		{
			Rect(Layer, C - FVector2D(R, R), FVector2D(2 * R, 2 * R), Fill, R);
		}
		void Ring(int32 Layer, const FVector2D& C, float R, const FLinearColor& Col, float W) const
		{
			TArray<FVector2D> P;
			for (int32 I = 0; I <= 32; I++)
			{
				const float A = 2.f * PI * I / 32;
				P.Add(C + R * FVector2D(FMath::Cos(A), FMath::Sin(A)));
			}
			Lines(Layer, P, Col, W);
		}
		/** A line's badge: a disc in its colour with its number. */
		void Badge(int32 Layer, const FVector2D& C, float R, const LyudTransport::FLine& L) const
		{
			Disc(Layer, C, R, LineColor(L));
			Text(Layer + 1, FString::FromInt(L.Number), C, LyudUI::Font(FMath::RoundToInt(R * 1.1f), "Bold"), FLinearColor::White, 0.5f);
		}
	};
}

// --- diagram -----------------------------------------------------------------------------------------------

void SLyudTransitDiagram::Construct(const FArguments& InArgs)
{
	bLive = InArgs._bLive;
	bBoard = InArgs._bBoard;
	From = InArgs._From;
	OnPick = InArgs._OnPick;
	if (bLive)
	{
		RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SLyudTransitDiagram::Repaint));
	}
}

EActiveTimerReturnType SLyudTransitDiagram::Repaint(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

FVector2D SLyudTransitDiagram::ToScreen(const FVector2D& S, const FVector2D& Size) const
{
	// the schema's grid: 0..10 across, 0..10 down, inside margins (the title on top, the legend at the bottom)
	const FVector2D Min(Size.X * 0.08, Size.Y * 0.2), Max(Size.X * 0.84, Size.Y * 0.86);
	return FVector2D(FMath::Lerp(Min.X, Max.X, S.X / 10.0), FMath::Lerp(Min.Y, Max.Y, S.Y / 10.0));
}

int32 SLyudTransitDiagram::StationUnder(const FVector2D& Local, const FVector2D& Size) const
{
	const LyudTransport::FNetwork* N = Net();
	if (!N)
	{
		return -1;
	}
	for (int32 S = 0; S < SimNum(N->Stations); S++)
	{
		const FVector2D P = ToScreen(FVector2D(N->Stations[S].SchemaX, N->Stations[S].SchemaY), Size);
		if (FVector2D::Distance(P, Local) < Size.Y * 0.045)
		{
			return S;
		}
	}
	return -1;
}

int32 SLyudTransitDiagram::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	FPainter P(G, Out);
	const float U = Size.Y / 900.f;     // design unit
	// the ground: cream enamel on the board, warm paper in the screen
	const FLinearColor Paper = bBoard ? FLinearColor(0.93f, 0.89f, 0.78f) : FLinearColor(0.9f, 0.86f, 0.76f);
	const FLinearColor Ink(0.12f, 0.08f, 0.05f);
	P.Rect(LayerId, FVector2D::ZeroVector, Size, Paper, 18.f * U);
	P.Rect(LayerId + 1, FVector2D(0, 0), FVector2D(Size.X, 10.f * U), FLinearColor(0.86f, 0.42f, 0.08f));
	P.Rect(LayerId + 1, FVector2D(0, Size.Y - 10.f * U), FVector2D(Size.X, 10.f * U), FLinearColor(0.86f, 0.42f, 0.08f));
	P.Text(LayerId + 2, LyudText::Get(TEXT("transport.diagram.title")).ToString(), FVector2D(Size.X * 0.5, 62.f * U), LyudUI::Font(FMath::RoundToInt(44 * U), "Bold"), Ink, 0.5f);
	P.Text(LayerId + 2, LyudText::Get(TEXT("transport.diagram.subtitle")).ToString(), FVector2D(Size.X * 0.5, 112.f * U), LyudUI::Font(FMath::RoundToInt(22 * U), "Italic"),
		Ink * 0.8f, 0.5f);
	const LyudTransport::FNetwork* N = Net();
	if (!N)
	{
		return LayerId + 3;
	}
	auto At = [&](int32 S) { return ToScreen(FVector2D(N->Stations[S].SchemaX, N->Stations[S].SchemaY), Size); };

	// the lines: a thick band in the line's colour, each line a little off the other where they share a station
	for (int32 L = 0; L < SimNum(N->Lines); L++)
	{
		const LyudTransport::FLine& Ln = N->Lines[L];
		TArray<FVector2D> Pts;
		for (int32 S : Ln.Stations)
		{
			Pts.Add(At(S));
		}
		P.Lines(LayerId + 3, Pts, Ink * 0.25f + FLinearColor(0, 0, 0, 0.75f), 22.f * U);
		P.Lines(LayerId + 4, Pts, LineColor(Ln), 16.f * U);
		// the depot: a little hatched square past the end station
		if (Ln.Depot >= 0)
		{
			const FVector2D D = At(Ln.Depot);
			int32 Home = 0;
			for (const LyudTransport::FCabin& C : N->Cabins)
			{
				Home += C.Line == L && C.State == LyudTransport::ECabinState::Depot;
			}
			const FString S = LyudText::Get(TEXT("transport.diagram.depot")).ToString() + (bLive ? FString::Printf(TEXT(" · %d"), Home) : FString());
			// above and right of the station, clear of its name
			P.Rect(LayerId + 4, D + FVector2D(22.f, -52.f) * U, FVector2D(20.f, 20.f) * U, LineColor(Ln) * 0.6f, 3.f * U);
			P.Text(LayerId + 5, S, D + FVector2D(50.f, -42.f) * U, LyudUI::Font(FMath::RoundToInt(17 * U), "Italic"), Ink * 0.8f);
		}
	}
	// live cabins: a dot between the stations they run between
	if (bLive)
	{
		for (const LyudTransport::FCabin& C : N->Cabins)
		{
			if (C.State != LyudTransport::ECabinState::Moving && C.State != LyudTransport::ECabinState::Dwelling)
			{
				continue;
			}
			const LyudTransport::FLine& Ln = N->Lines[C.Line];
			// the stops before and after it round the loop
			int32 Prev = -1, Next = -1;
			float BestP = 1e30f, BestN = 1e30f;
			for (int32 K = 0; K < SimNum(Ln.Stops); K++)
			{
				const float Back = Ln.Ahead(Ln.Stops[K].S, C.S), Fwd = Ln.Ahead(C.S, Ln.Stops[K].S);
				if (Back < BestP)
				{
					BestP = Back;
					Prev = K;
				}
				if (Fwd < BestN && Fwd > 0.01f)
				{
					BestN = Fwd;
					Next = K;
				}
			}
			if (Prev < 0 || Next < 0)
			{
				continue;
			}
			const float F = BestP / FMath::Max(1.f, BestP + BestN);
			const FVector2D A = At(Ln.Stops[Prev].Station), B = At(Ln.Stops[Next].Station);
			FVector2D D = (B - A).GetSafeNormal();
			const FVector2D Side(-D.Y, D.X);
			const FVector2D Dot = FMath::Lerp(A, B, F) + Side * 14.f * U;
			P.Disc(LayerId + 7, Dot, 9.f * U, Ink);
			P.Disc(LayerId + 8, Dot, 6.f * U, SimNum(C.Riders) ? FLinearColor(1.f, 0.85f, 0.4f) : FLinearColor(0.95f, 0.93f, 0.88f));
		}
	}
	// the stations: white discs ringed in ink (a change: a double ring), the names beside them
	const int32 Here = From >= 0 ? From : (bBoard ? N->FindStation("aton") : -1);
	for (int32 S = 0; S < SimNum(N->Stations); S++)
	{
		const LyudTransport::FStation& St = N->Stations[S];
		const FVector2D C = At(S);
		const bool bChange = SimNum(St.Levels) > 1;
		const float R = (bChange ? 20.f : 15.f) * U;
		P.Disc(LayerId + 9, C, R + 4.f * U, Ink);
		P.Disc(LayerId + 10, C, R, FLinearColor(0.98f, 0.97f, 0.93f));
		if (bChange)
		{
			P.Ring(LayerId + 11, C, R * 0.55f, Ink, 3.f * U);
		}
		if (S == Hover)
		{
			P.Ring(LayerId + 11, C, R + 10.f * U, FLinearColor(0.86f, 0.42f, 0.08f), 4.f * U);
		}
		// names to the right (to the left at the diagram's right edge), under the station where a line runs across it
		bool bAcross = false;
		for (const LyudTransport::FLine& Ln : N->Lines)
		{
			for (size_t K = 0; K < Ln.Stations.size(); K++)
			{
				if (Ln.Stations[K] != S)
				{
					continue;
				}
				for (const int32 O : { K > 0 ? Ln.Stations[K - 1] : -1, K + 1 < Ln.Stations.size() ? Ln.Stations[K + 1] : -1 })
				{
					bAcross |= O >= 0 && N->Stations[O].SchemaY == St.SchemaY;
				}
			}
		}
		const bool bLeft = !bAcross && St.SchemaX >= 8;
		const FVector2D NameAt = bAcross ? C + FVector2D(0.f, R + 26.f * U) : C + FVector2D((bLeft ? -1.f : 1.f) * (R + 14.f * U), 0.f);
		const float NameAlign = bAcross ? 0.5f : (bLeft ? 1.f : 0.f);
		P.Text(LayerId + 12, StationName(S), NameAt, LyudUI::Font(FMath::RoundToInt(27 * U), S == Hover ? "Bold" : "Regular"), Ink, NameAlign);
		if (S == Here)
		{
			// you are here: a red arrowhead under the station
			const FVector2D T = bAcross ? C - FVector2D(0.f, R + 70.f * U) : C + FVector2D(0.f, R + 16.f * U);
			TArray<FVector2D> Tri = { T, T + FVector2D(-12.f, 20.f) * U, T + FVector2D(12.f, 20.f) * U, T };
			P.Lines(LayerId + 12, Tri, FLinearColor(0.85f, 0.1f, 0.05f), 5.f * U);
			const FString HereS = LyudText::Get(bBoard ? TEXT("transport.diagram.here_club") : TEXT("transport.diagram.here")).ToString();
			P.Text(LayerId + 12, HereS, T + FVector2D(0.f, 40.f * U), LyudUI::Font(FMath::RoundToInt(19 * U), "Bold"), FLinearColor(0.75f, 0.08f, 0.04f), 0.5f);
		}
		if (S == Hover && Active())
		{
			// the wait at the hovered station
			for (int32 L = 0, Row = 0; L < SimNum(N->Lines); L++)
			{
				if (!std::any_of(St.Levels.begin(), St.Levels.end(), [L](const LyudTransport::FLevel& Lv) { return Lv.Line == L; }))
				{
					continue;
				}
				bool bCalled = false;
				const float Wait = N->Eta(S, L, -1, bCalled);
				const FString W = LyudText::Get(bCalled ? TEXT("transport.wait.called") : TEXT("transport.wait.on_call"),
					{ FStringFormatArg(LyudTransit::WaitText(Wait).ToString()) }).ToString();
				P.Text(LayerId + 12, W, NameAt + FVector2D(0.f, (34.f + 28.f * Row++) * U), LyudUI::Font(FMath::RoundToInt(19 * U), "Italic"), LineColor(N->Lines[L]) * 0.8f,
					NameAlign);
			}
		}
	}
	// the legend: the lines' badges and names
	float X = Size.X * 0.06f;
	for (int32 L = 0; L < SimNum(N->Lines); L++)
	{
		const FVector2D C(X + 22.f * U, Size.Y - 58.f * U);
		P.Badge(LayerId + 13, C, 20.f * U, N->Lines[L]);
		const FString Name = LyudText::Get(Key(N->Lines[L].Name)).ToString();
		P.Text(LayerId + 13, Name, C + FVector2D(32.f * U, 0.f), LyudUI::Font(FMath::RoundToInt(22 * U)), Ink);
		X += 64.f * U + P.TextSize(Name, LyudUI::Font(FMath::RoundToInt(22 * U))).X + 40.f * U;
	}
	if (bLive)
	{
		// the dots
		const FVector2D C(X + 20.f * U, Size.Y - 58.f * U);
		P.Disc(LayerId + 13, C, 9.f * U, Ink);
		P.Disc(LayerId + 14, C, 6.f * U, FLinearColor(1.f, 0.85f, 0.4f));
		P.Text(LayerId + 14, LyudText::Get(TEXT("transport.diagram.cabins")).ToString(), C + FVector2D(20.f * U, 0.f), LyudUI::Font(FMath::RoundToInt(20 * U), "Italic"), Ink);
	}
	return LayerId + 15;
}

FReply SLyudTransitDiagram::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	Hover = StationUnder(MyGeometry.AbsoluteToLocal(E.GetScreenSpacePosition()), MyGeometry.GetLocalSize());
	return FReply::Unhandled();
}

FReply SLyudTransitDiagram::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	const int32 S = StationUnder(MyGeometry.AbsoluteToLocal(E.GetScreenSpacePosition()), MyGeometry.GetLocalSize());
	if (S >= 0 && E.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnPick.ExecuteIfBound(S);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// --- station board -----------------------------------------------------------------------------------------

void SLyudStationBoard::Construct(const FArguments& InArgs)
{
	Station = InArgs._Station;
	Line = InArgs._Line;
	RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SLyudStationBoard::Repaint));
}

EActiveTimerReturnType SLyudStationBoard::Repaint(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SLyudStationBoard::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	FPainter P(G, Out);
	const float U = Size.Y / 360.f;
	// a dark board with amber letters, an ivory frame
	P.Rect(LayerId, FVector2D::ZeroVector, Size, FLinearColor(0.8f, 0.76f, 0.66f), 14.f * U);
	P.Rect(LayerId + 1, FVector2D(10, 10) * U, Size - FVector2D(20, 20) * U, FLinearColor(0.035f, 0.03f, 0.025f), 8.f * U);
	const LyudTransport::FNetwork* N = Net();
	if (!N || !SimValid(N->Stations, Station) || !SimValid(N->Lines, Line))
	{
		return LayerId + 2;
	}
	const LyudTransport::FLine& Ln = N->Lines[Line];
	const FLinearColor Amber(1.f, 0.72f, 0.3f);
	P.Badge(LayerId + 2, FVector2D(60.f, 62.f) * U, 30.f * U, Ln);
	P.Text(LayerId + 2, StationName(Station), FVector2D(108.f * U, 62.f * U), LyudUI::Font(FMath::RoundToInt(44 * U), "Bold"), FLinearColor(0.98f, 0.93f, 0.82f));
	P.Text(LayerId + 2, LyudText::Get(Key(Ln.Name)).ToString(), FVector2D(Size.X - 34.f * U, 62.f * U), LyudUI::Font(FMath::RoundToInt(24 * U), "Italic"), LineColor(Ln), 1.f);
	// a row for each way: towards the line's last stop (dir 0) and its first (dir 1)
	int32 Row = 0;
	for (int32 Dir = 0; Dir < 2; Dir++)
	{
		const int32 Term = Dir == 0 ? Ln.Stations.back() : Ln.Stations[0];
		if (Term == Station)
		{
			continue;
		}
		bool bCalled = false;
		const float Wait = N->Eta(Station, Line, Dir, bCalled);
		const float Y = (150.f + 90.f * Row++) * U;
		P.Rect(LayerId + 2, FVector2D(30.f * U, Y - 34.f * U), FVector2D(Size.X - 60.f * U, 68.f * U), FLinearColor(0.07f, 0.06f, 0.05f), 4.f * U);
		P.Text(LayerId + 3, LyudText::Get(TEXT("transport.board.to"), { FStringFormatArg(StationName(Term)) }).ToString(), FVector2D(50.f * U, Y),
			LyudUI::Font(FMath::RoundToInt(34 * U)), Amber);
		const FString W = bCalled ? LyudTransit::WaitText(Wait).ToString()
			: LyudText::Get(TEXT("transport.wait.on_call"), { FStringFormatArg(LyudTransit::WaitText(Wait).ToString()) }).ToString();
		P.Text(LayerId + 3, W, FVector2D(Size.X - 50.f * U, Y), LyudUI::Font(FMath::RoundToInt(34 * U), bCalled ? "Bold" : "Italic"), bCalled ? Amber : Amber * 0.7f, 1.f);
	}
	return LayerId + 4;
}

// --- station sign ------------------------------------------------------------------------------------------

int32 SLyudStationSign::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	FPainter P(G, Out);
	const float U = Size.Y / 260.f;
	const LyudTransport::FNetwork* N = Net();
	if (!N || !SimValid(N->Stations, Station))
	{
		return LayerId;
	}
	const LyudTransport::FStation& St = N->Stations[Station];
	// a dark enamel plate in an amber frame, the line badges on the left, the name in ivory capitals in the middle of the
	// free width; the amber rule runs under the letters, never through them
	const FLinearColor Frame(0.86f, 0.42f, 0.08f), Plate(0.07f, 0.08f, 0.10f), Ivory(0.93f, 0.89f, 0.80f);
	P.Rect(LayerId, FVector2D(0, 0), Size, Frame, 26.f * U);
	P.Rect(LayerId + 1, FVector2D(10.f * U, 10.f * U), Size - FVector2D(20.f * U, 20.f * U), Plate, 18.f * U);
	float X = 40.f * U;
	for (const LyudTransport::FLevel& Lv : St.Levels)
	{
		P.Badge(LayerId + 2, FVector2D(X + 50.f * U, Size.Y * 0.5f), 50.f * U, N->Lines[Lv.Line]);
		X += 116.f * U;
	}
	const FString Name = StationName(Station).ToUpper();
	FSlateFontInfo Font = LyudUI::Font(FMath::RoundToInt(110 * U), "Bold");
	const float Free = Size.X - X - 40.f * U;
	const FVector2D TS = P.TextSize(Name, Font);
	if (TS.X > Free && TS.X > 1.f)
	{
		Font = LyudUI::Font(FMath::Max(8, FMath::FloorToInt(110 * U * Free / TS.X)), "Bold");
	}
	const float Cx = X + Free * 0.5f;
	P.Text(LayerId + 3, Name, FVector2D(Cx, Size.Y * 0.46f), Font, Ivory, 0.5f);
	const float W = FMath::Min(Free, P.TextSize(Name, Font).X);
	P.Rect(LayerId + 3, FVector2D(Cx - W * 0.5f, Size.Y * 0.80f), FVector2D(W, 8.f * U), Frame);
	return LayerId + 4;
}

// --- cabin board and plate ---------------------------------------------------------------------------------

void SLyudCabinBoard::Construct(const FArguments& InArgs)
{
	Cabin = InArgs._Cabin;
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SLyudCabinBoard::Repaint));
}

EActiveTimerReturnType SLyudCabinBoard::Repaint(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SLyudCabinBoard::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	FPainter P(G, Out);
	const float U = Size.Y / 150.f;
	P.Rect(LayerId, FVector2D::ZeroVector, Size, FLinearColor(0.03f, 0.025f, 0.02f), 6.f * U);
	const LyudTransport::FNetwork* N = Net();
	if (!N || !SimValid(N->Cabins, Cabin))
	{
		return LayerId + 1;
	}
	const LyudTransport::FCabin& C = N->Cabins[Cabin];
	const LyudTransport::FLine& Ln = N->Lines[C.Line];
	P.Rect(LayerId + 1, FVector2D::ZeroVector, FVector2D(14.f * U, Size.Y), LineColor(Ln));
	const FLinearColor Amber(1.f, 0.72f, 0.3f);
	FString A, B;
	const int32 Dest = N->Destination(Cabin), Next = N->NextStation(Cabin);
	switch (C.State)
	{
	case LyudTransport::ECabinState::Dwelling:
		A = Dest >= 0 ? LyudText::Get(TEXT("transport.cabin.to"), { FStringFormatArg(StationName(Dest)) }).ToString() : LyudText::Get(TEXT("transport.cabin.idle")).ToString();
		B = LyudText::Get(C.Door > 0.5f ? TEXT("transport.cabin.doors_open") : TEXT("transport.cabin.doors")).ToString();
		break;
	case LyudTransport::ECabinState::Moving:
		A = Dest >= 0 ? LyudText::Get(TEXT("transport.cabin.to"), { FStringFormatArg(StationName(Dest)) }).ToString() : FString();
		B = Next >= 0 ? LyudText::Get(TEXT("transport.cabin.next"), { FStringFormatArg(StationName(Next)) }).ToString() : FString();
		break;
	case LyudTransport::ECabinState::Idle:
		A = LyudText::Get(TEXT("transport.cabin.idle")).ToString();
		B = LyudText::Get(TEXT("transport.cabin.how")).ToString();
		break;
	default:
		A = LyudText::Get(TEXT("transport.cabin.depot")).ToString();
		break;
	}
	P.Text(LayerId + 2, A, FVector2D(32.f * U, 48.f * U), LyudUI::Font(FMath::RoundToInt(34 * U), "Bold"), Amber);
	P.Text(LayerId + 2, B, FVector2D(32.f * U, 106.f * U), LyudUI::Font(FMath::RoundToInt(26 * U)), Amber * 0.8f);
	return LayerId + 3;
}

int32 SLyudCabinPlate::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPainter P(G, Out);
	const FVector2D Size = G.GetLocalSize();
	const float U = Size.Y / 160.f;
	const FLinearColor Ink(0.32f, 0.2f, 0.12f);
	// the number in big stencil figures, the name beside it in italics, a dot of the line's colour
	P.Text(LayerId, FString::Printf(TEXT("%02d"), Number), FVector2D(10.f * U, 80.f * U), LyudUI::Font(FMath::RoundToInt(92 * U), "Bold"), Ink);
	P.Disc(LayerId, FVector2D(190.f * U, 80.f * U), 12.f * U, Color);
	P.Text(LayerId, Name.ToString(), FVector2D(214.f * U, 84.f * U), LyudUI::Font(FMath::RoundToInt(50 * U), "Italic"), Ink);
	return LayerId + 1;
}

// --- full screen -------------------------------------------------------------------------------------------

void SLyudTransitScreen::Construct(const FArguments& InArgs)
{
	From = InArgs._From;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			// a click anywhere round the map closes it (Esc in the editor's play leaves the game)
			SNew(SBorder).BorderImage(LyudUI::ShadeBrush())
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&)
			{
				if (ULyudTransportSubsystem* T = Active())
				{
					T->CloseDiagram();
				}
				return FReply::Handled();
			})
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(40.f)
		[
			SNew(SBorder).BorderImage(LyudUI::PanelBrush()).Padding(14.f)
			.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f))
					.OnClicked_Lambda([]()
					{
						if (ULyudTransportSubsystem* T = Active())
						{
							T->CloseDiagram();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Font(LyudUI::Font(22)).ColorAndOpacity(LyudUI::Text).Text(FText::FromString(TEXT("\u2715")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(1280.f).HeightOverride(720.f)
					[
						SAssignNew(Diagram, SLyudTransitDiagram).bLive(true).bBoard(false).From(From)
							.OnPick(FOnTransitPick::CreateSP(this, &SLyudTransitScreen::OnPick))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 12.f, 8.f, 4.f)
				[
					SNew(STextBlock).Font(LyudUI::Font(16)).ColorAndOpacity(LyudUI::Text).Text(this, &SLyudTransitScreen::Footer).AutoWrapText(true)
				]
			]
		]
	];
}

void SLyudTransitScreen::OnPick(int32 Station)
{
	if (From < 0 || Station == From)
	{
		return;
	}
	if (ULyudTransportSubsystem* T = Active())
	{
		if (T->CallCabin(From, Station))
		{
			Picked = Station;
		}
	}
}

FText SLyudTransitScreen::Footer() const
{
	ULyudTransportSubsystem* T = Active();
	const FText Close = LyudText::Get(TEXT("transport.diagram.close"));
	if (From < 0 || !T)
	{
		return FText::Format(FText::FromString(TEXT("{0}   {1}")), LyudText::Get(TEXT("transport.diagram.look")), Close);
	}
	if (Picked >= 0)
	{
		const LyudTransport::FNetwork* N = T->GetNet();
		const int32 R = N ? N->FindRider(LyudTransport::PlayerId) : -1;
		float Wait = -1.f;
		if (R >= 0)
		{
			const LyudTransport::FLeg& Leg = N->Riders[R].Legs[N->Riders[R].LegI];
			bool bCalled = false;
			Wait = N->Eta(Leg.From, Leg.Line, N->Lines[Leg.Line].Stops[Leg.BoardStop].Dir, bCalled);
		}
		return FText::Format(FText::FromString(TEXT("{0}   {1}")),
			LyudText::Get(TEXT("transport.diagram.called"), { FStringFormatArg(StationName(Picked)), FStringFormatArg(LyudTransit::WaitText(Wait).ToString()) }), Close);
	}
	return FText::Format(FText::FromString(TEXT("{0}   {1}")), LyudText::Get(TEXT("transport.diagram.pick")), Close);
}

FReply SLyudTransitScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey K = InKeyEvent.GetKey();
	if (K == EKeys::Escape || K == EKeys::E || K == EKeys::Tab || K == EKeys::M)
	{
		if (ULyudTransportSubsystem* T = Active())
		{
			T->CloseDiagram();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// --- label and clock ---------------------------------------------------------------------------------------

int32 SLyudLabel::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPainter P(G, Out);
	const FVector2D S = G.GetLocalSize();
	const float U = S.Y / 140.f;
	P.Rect(LayerId, FVector2D::ZeroVector, S, FLinearColor(0.035f, 0.03f, 0.025f), 10.f * U);
	P.Rect(LayerId + 1, FVector2D(0, S.Y - 14.f * U), FVector2D(S.X, 14.f * U), FLinearColor(0.86f, 0.42f, 0.08f));
	P.Text(LayerId + 2, LyudText::Get(Key).ToString(), FVector2D(S.X * 0.5, S.Y * 0.45), LyudUI::Font(FMath::RoundToInt(56 * U), "Bold"),
		FLinearColor(1.f, 0.78f, 0.4f), 0.5f);
	return LayerId + 3;
}

void SLyudClock::Construct(const FArguments& InArgs)
{
	Context = InArgs._WorldContext;
	RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SLyudClock::Repaint));
}

EActiveTimerReturnType SLyudClock::Repaint(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SLyudClock::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPainter P(G, Out);
	const FVector2D S = G.GetLocalSize();
	const FVector2D C = S * 0.5;
	const float R = FMath::Min(S.X, S.Y) * 0.5f;
	const FLinearColor Ink(0.1f, 0.07f, 0.05f);
	P.Disc(LayerId, C, R, FLinearColor(0.86f, 0.42f, 0.08f));
	P.Disc(LayerId + 1, C, R * 0.9f, FLinearColor(0.95f, 0.93f, 0.86f));
	for (int32 I = 0; I < 12; I++)
	{
		const float A = 2.f * PI * I / 12.f;
		const FVector2D D(FMath::Sin(A), -FMath::Cos(A));
		P.Lines(LayerId + 2, { C + D * R * 0.72f, C + D * R * (I % 3 ? 0.8f : 0.86f) }, Ink, I % 3 ? 3.f : 7.f);
	}
	double Hours = 12.0;
	if (UObject* W = Context.Get())
	{
		Hours = UGameCalendar::NowIn(W->GetWorld()).GetTimeOfDay().GetTotalHours();
	}
	const float Hr = 2.f * PI * FMath::Fmod((float)Hours, 12.f) / 12.f, Mn = 2.f * PI * FMath::Fmod((float)Hours * 60.f, 60.f) / 60.f;
	P.Lines(LayerId + 3, { C, C + FVector2D(FMath::Sin(Hr), -FMath::Cos(Hr)) * R * 0.45f }, Ink, 8.f);
	P.Lines(LayerId + 3, { C, C + FVector2D(FMath::Sin(Mn), -FMath::Cos(Mn)) * R * 0.7f }, Ink, 5.f);
	P.Disc(LayerId + 4, C, R * 0.05f, FLinearColor(0.86f, 0.42f, 0.08f));
	return LayerId + 5;
}
