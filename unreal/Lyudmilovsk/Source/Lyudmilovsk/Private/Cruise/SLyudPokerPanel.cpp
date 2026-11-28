#include "Cruise/SLyudPokerPanel.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Cruise/LyudShipPoker.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UI/LyudUIStyle.h"

// Every helper here is called with its namespace (never through a using-directive): the names are plain ones, and in a unity
// build this file shares its translation unit with the other panels.
namespace LyudPokerUI
{
	const FSlateBrush* White()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}
	const FSlateBrush* Round()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 4.f);
		return &Brush;
	}

	const FLinearColor Ivory(0.93f, 0.89f, 0.8f);
	const FLinearColor RedSuit(0.7f, 0.1f, 0.08f);
	const FLinearColor BlackSuit(0.07f, 0.06f, 0.06f);
	const FLinearColor CardBack(0.33f, 0.09f, 0.08f);
	const FLinearColor CardBackLine(0.62f, 0.3f, 0.2f);

	/** The smallest size a long line is set at before it is clipped. */
	constexpr int32 MinFitSize = 8;
	constexpr int32 FitSlots = 128;

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const FLinearColor& Color, bool bRound = false)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(At)), bRound ? Round() : White(),
			ESlateDrawEffect::None, Color);
	}

	/** Is the point (U right, V down, both -1..1) inside the suit's shape? Suits: 0 spades, 1 hearts, 2 diamonds, 3 clubs. */
	bool InSuit(int32 Suit, float U, float V)
	{
		const float A = FMath::Abs(U);
		auto Heart = [](float X, float Y)
		{
			const float DX = X - 0.45f;
			const float DY = Y + 0.4f;
			if (DX * DX + DY * DY <= 0.27f)
			{
				return true;
			}
			return Y >= -0.3f && Y <= 0.95f && X <= 0.93f * (1.f - (Y + 0.3f) / 1.25f);
		};
		const float Stem = 0.1f + 0.3f * FMath::Max(0.f, (V - 0.55f) / 0.45f);
		switch (Suit)
		{
		case 0: return Heart(A, -V - 0.1f) || (V >= 0.1f && A <= Stem);
		case 1: return Heart(A, V);
		case 2: return A / 0.8f + FMath::Abs(V) <= 1.f;
		default:
		{
			const float TopY = V + 0.48f;
			const float SideX = A - 0.52f;
			const float SideY = V - 0.1f;
			return U * U + TopY * TopY <= 0.19f || SideX * SideX + SideY * SideY <= 0.19f || (V >= -0.3f && A <= FMath::Max(Stem, V < 0.3f ? 0.3f : 0.f));
		}
		}
	}
}

void SLyudPokerPanel::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	Owner = InArgs._Owner;
	for (int32 R = 2; R <= 10; ++R)
	{
		Ranks.Add(FString::FromInt(R));
	}
	Ranks.Add(TEXT("J"));
	Ranks.Add(TEXT("Q"));
	Ranks.Add(TEXT("K"));
	Ranks.Add(TEXT("A"));
	Fitted.Reserve(LyudPokerUI::FitSlots);
}

int32 SLyudPokerPanel::FitSize(const FString& S, int32 Size, const char* Face, float MaxWidth) const
{
	if (S.IsEmpty() || MaxWidth <= 0.f || !FSlateApplication::IsInitialized())
	{
		return Size;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	// a little kept in hand: the measure is taken at scale 1, the panel is painted at the screen's
	const float Room = MaxWidth * 0.98f;
	int32 Use = Size;
	float Wide = (float)Measure->Measure(S, LyudUI::Font(Use, Face)).X;
	for (int32 Try = 0; Try < 3 && Wide > Room && Use > LyudPokerUI::MinFitSize; ++Try)
	{
		Use = FMath::Max(LyudPokerUI::MinFitSize, FMath::Min(Use - 1, FMath::FloorToInt((float)Use * Room / Wide)));
		Wide = (float)Measure->Measure(S, LyudUI::Font(Use, Face)).X;
	}
	return Wide > MaxWidth ? -Use : Use;
}

void SLyudPokerPanel::FitText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FString& S, int32 Size,
	const char* Face, const FLinearColor& Color, float MaxWidth) const
{
	// the place in the list is taken even by an empty line: the order of the lines is the same for the same view
	const int32 Slot = FitAt++;
	int32 Use = Size;
	if (Fitted.IsValidIndex(Slot))
	{
		Use = Fitted[Slot];
	}
	else
	{
		Use = FitSize(S, Size, Face, MaxWidth);
		if (Slot == Fitted.Num())
		{
			Fitted.Add(Use);
		}
	}
	if (S.IsEmpty())
	{
		return;
	}
	const bool bClip = Use < 0;
	const int32 Points = FMath::Clamp(FMath::Abs(Use), LyudPokerUI::MinFitSize, FMath::Max(Size, LyudPokerUI::MinFitSize));
	// a smaller line keeps near the foot of its row
	const FVector2f Where = At + FVector2f(0.f, (float)(Size - Points) * 0.7f);
	if (bClip)
	{
		Out.PushClip(FSlateClippingZone(Geo.MakeChild(FVector2f(FMath::Max(MaxWidth, 1.f), 40.f), FSlateLayoutTransform(Where)).ToPaintGeometry()));
	}
	FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(FVector2f(900.f, 40.f), FSlateLayoutTransform(Where)), S, LyudUI::Font(Points, Face),
		ESlateDrawEffect::None, Color);
	if (bClip)
	{
		Out.PopClip();
	}
}

void SLyudPokerPanel::FitText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FText& S, int32 Size,
	const char* Face, const FLinearColor& Color, float MaxWidth) const
{
	// FText::ToString hands out the text's own string: nothing is copied
	FitText(Out, Layer, Geo, At, S.ToString(), Size, Face, Color, MaxWidth);
}

void SLyudPokerPanel::PaintSuit(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Centre, float Size, int32 Suit,
	const FLinearColor& Color) const
{
	// the shape in rows of plain boxes (a run of points inside it per row): no glyph, no brush, nothing to load
	const int32 Rows = FMath::Clamp(FMath::RoundToInt(Size * 0.5f), 6, 18);
	const int32 Cols = Rows * 2;
	const float RowH = Size / (float)Rows;
	const float ColW = Size / (float)Cols;
	for (int32 R = 0; R < Rows; ++R)
	{
		const float V = (((float)R + 0.5f) / (float)Rows) * 2.f - 1.f;
		int32 From = -1;
		for (int32 C = 0; C <= Cols; ++C)
		{
			const bool bIn = C < Cols && LyudPokerUI::InSuit(Suit, (((float)C + 0.5f) / (float)Cols) * 2.f - 1.f, V);
			if (bIn && From < 0)
			{
				From = C;
			}
			else if (!bIn && From >= 0)
			{
				LyudPokerUI::Box(Out, Layer, Geo, Centre + FVector2f(-Size * 0.5f + (float)From * ColW, -Size * 0.5f + (float)R * RowH),
					FVector2f((float)(C - From) * ColW, RowH + 0.5f), Color);
				From = -1;
			}
		}
	}
}

void SLyudPokerPanel::PaintCard(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size, int32 Card,
	bool bFace, float Fade) const
{
	const FLinearColor Tint(Fade, Fade, Fade, 1.f);
	const bool bBig = Size.X >= 50.f;
	if (!bFace)
	{
		// the back: a dark red card with a frame and a lattice of small squares
		const FLinearColor BackColor = LyudPokerUI::CardBack * Tint;
		const FLinearColor LineColor = LyudPokerUI::CardBackLine * Tint;
		LyudPokerUI::Box(Out, Layer, Geo, At, Size, BackColor, true);
		const float In = bBig ? 6.f : 3.f;
		const float Step = bBig ? 12.f : 6.f;
		const float Dot = bBig ? 5.f : 2.5f;
		LyudPokerUI::Box(Out, Layer + 1, Geo, At + FVector2f(In, In), FVector2f(Size.X - 2.f * In, 1.f), LineColor);
		LyudPokerUI::Box(Out, Layer + 1, Geo, At + FVector2f(In, Size.Y - In - 1.f), FVector2f(Size.X - 2.f * In, 1.f), LineColor);
		LyudPokerUI::Box(Out, Layer + 1, Geo, At + FVector2f(In, In), FVector2f(1.f, Size.Y - 2.f * In), LineColor);
		LyudPokerUI::Box(Out, Layer + 1, Geo, At + FVector2f(Size.X - In - 1.f, In), FVector2f(1.f, Size.Y - 2.f * In), LineColor);
		int32 Row = 0;
		for (float Y = In + Step * 0.5f; Y + Dot < Size.Y - In; Y += Step, ++Row)
		{
			for (float X = In + Step * (Row % 2 ? 1.f : 0.5f); X + Dot < Size.X - In; X += Step)
			{
				LyudPokerUI::Box(Out, Layer + 1, Geo, At + FVector2f(X, Y), FVector2f(Dot, Dot), LineColor);
			}
		}
		return;
	}
	const int32 Rank = FMath::Clamp(Card / 4, 0, 12);
	const int32 Suit = FMath::Clamp(Card % 4, 0, 3);
	const FLinearColor Ink = (Suit == 1 || Suit == 2 ? LyudPokerUI::RedSuit : LyudPokerUI::BlackSuit) * Tint;
	LyudPokerUI::Box(Out, Layer, Geo, At, Size, LyudPokerUI::Ivory * Tint, true);
	if (Ranks.IsValidIndex(Rank))
	{
		// "10" and the letters always fit their corner: no fitting, straight to the list
		FSlateDrawElement::MakeText(Out, Layer + 2,
			Geo.ToPaintGeometry(FVector2f(60.f, 40.f), FSlateLayoutTransform(At + (bBig ? FVector2f(7.f, 3.f) : FVector2f(3.f, 0.f)))), Ranks[Rank],
			LyudUI::Font(bBig ? 20 : 10, "Bold"), ESlateDrawEffect::None, Ink);
	}
	if (bBig)
	{
		PaintSuit(Out, Layer + 1, Geo, At + FVector2f(Size.X - 15.f, 17.f), 14.f, Suit, Ink);
		PaintSuit(Out, Layer + 1, Geo, At + FVector2f(Size.X * 0.5f, Size.Y * 0.62f), 34.f, Suit, Ink);
	}
	else
	{
		PaintSuit(Out, Layer + 1, Geo, At + FVector2f(Size.X * 0.5f, Size.Y - 13.f), 14.f, Suit, Ink);
	}
}

int32 SLyudPokerPanel::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const ULyudShipPoker* Poker = Owner.Get();
	if (!Poker)
	{
		return Layer;
	}
	const FLyudPokerView& V = Poker->View();
	const FVector2f View = Geo.GetLocalSize();
	const float W = FMath::Max(FMath::Min(View.X - 40.f, 900.f), 320.f);
	const float PanelH = 424.f;
	const FVector2f Panel((View.X - W) * 0.5f, View.Y - PanelH - 24.f);
	// the lines are fitted again only when the view was rebuilt or the panel's width changed
	if (V.Serial != FitSerial || W != FitWidth)
	{
		FitSerial = V.Serial;
		FitWidth = W;
		Fitted.Reset();
	}
	FitAt = 0;
	const float Full = W - 36.f;
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, PanelH), FSlateLayoutTransform(Panel)), LyudUI::PanelBrush());

	// where the head line's parts stand (at the full width: the pot 380, the hand's number 560, his stack 690)
	const float PotX = W * 0.5f - 70.f;
	const float StackX = W - 210.f;
	const float HandNoX = FMath::Max(StackX - 130.f, PotX + 60.f);
	const bool bTable = V.Stage == 1;
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 10.f), V.Title, 18, "Bold", LyudUI::Text, bTable ? PotX - 28.f : Full);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, PanelH - 46.f), V.Keys, 12, "Regular", LyudUI::Dim, Full);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, PanelH - 26.f), V.Keys2, 12, "Regular", LyudUI::Faint, Full);

	if (V.bHelp && V.Help.Num() > 0)
	{
		// the rules, over the table
		const float HelpH = 24.f + 22.f * (float)V.Help.Num();
		const FVector2f HelpAt(Panel.X, FMath::Max(12.f, Panel.Y - HelpH - 10.f));
		FSlateDrawElement::MakeBox(Out, Layer + 10, Geo.ToPaintGeometry(FVector2f(W, HelpH), FSlateLayoutTransform(HelpAt)), LyudUI::PanelBrush());
		for (int32 I = 0; I < V.Help.Num(); ++I)
		{
			FitText(Out, Layer + 12, Geo, HelpAt + FVector2f(18.f, 12.f + 22.f * (float)I), V.Help[I], 13, "Regular", I == 0 ? LyudUI::Text : LyudUI::Body, Full);
		}
	}

	if (!bTable)
	{
		// choosing the table, or the evening's count: plain lines
		for (int32 I = 0; I < V.Lines.Num(); ++I)
		{
			FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 54.f + 26.f * (float)I), V.Lines[I], I == 0 ? 15 : 14, I == 0 ? "Bold" : "Regular",
				I == 0 ? LyudUI::Bright : LyudUI::Body, Full);
		}
		FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 54.f + 26.f * (float)V.Lines.Num() + 10.f), V.Banner, 14, "Regular",
			V.bBannerBad ? LyudUI::Bad : LyudUI::Good, Full);
		return Layer + 20;
	}

	FitText(Out, Layer + 2, Geo, Panel + FVector2f(HandNoX, 15.f), V.HandNo, 12, "Regular", LyudUI::Dim, StackX - HandNoX - 8.f);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(PotX, 11.f), V.Pot, 16, "Bold", LyudUI::Bright, HandNoX - PotX - 8.f);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(StackX, 13.f), V.Stack, 14, "Regular", LyudUI::Text, 192.f);

	// the regulars across the top
	const int32 NumShown = FMath::Min(V.Seats.Num(), 4);
	const float ColW = (W - 28.f) / 4.f;
	for (int32 I = 0; I < NumShown; ++I)
	{
		const FLyudPokerSeatView& S = V.Seats[I];
		const FVector2f At = Panel + FVector2f(14.f + (float)I * ColW + 3.f, 44.f);
		FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(ColW - 6.f, 120.f), FSlateLayoutTransform(At)), LyudUI::CellBrush(S.bTurn));
		FitText(Out, Layer + 3, Geo, At + FVector2f(10.f, 5.f), S.Name, 13, "Bold", S.bDim ? LyudUI::Faint : (S.bTurn || S.bWon ? LyudUI::Bright : LyudUI::Text),
			ColW - 40.f);
		FitText(Out, Layer + 3, Geo, At + FVector2f(10.f, 25.f), S.Stack, 12, "Regular", S.bDim ? LyudUI::Faint : LyudUI::Body, 82.f);
		FitText(Out, Layer + 3, Geo, At + FVector2f(96.f, 25.f), S.Note, 12, "Regular", S.bDim ? LyudUI::Faint : LyudUI::Dim, ColW - 108.f);
		if (S.bDealer)
		{
			LyudPokerUI::Box(Out, Layer + 3, Geo, At + FVector2f(ColW - 24.f, 9.f), FVector2f(9.f, 9.f), LyudUI::Text, true);
		}
		const float Fade = S.bDim ? 0.45f : 1.f;
		for (int32 C = 0; C < FMath::Min(S.NumCards, 5); ++C)
		{
			PaintCard(Out, Layer + 3, Geo, At + FVector2f(10.f + (float)C * 34.f, 46.f), FVector2f(30.f, 44.f), S.Cards[C], S.bFaces, Fade);
		}
		FitText(Out, Layer + 3, Geo, At + FVector2f(10.f, 96.f), S.Hand, 11, "Bold", S.bWon ? LyudUI::Good : LyudUI::Body, ColW - 22.f);
	}

	// what gives a man away, and what just happened
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 170.f), V.Tell, 14, "Italic", LyudUI::Bright, Full);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 194.f), V.Banner, 13, "Bold", V.bBannerBad ? LyudUI::Bad : LyudUI::Good, Full);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 213.f), V.Banner2, 13, "Regular", V.bBannerBad ? LyudUI::Bad : LyudUI::Good, Full);

	// the hero's cards
	const float LogX = FMath::Min(438.f, W * 0.5f);
	const FVector2f HandAt = Panel + FVector2f(18.f, 250.f);
	for (int32 C = 0; C < FMath::Min(V.Hero.NumCards, 5); ++C)
	{
		const bool bMarked = ((V.Discard >> C) & 1) != 0;
		const FVector2f At = HandAt + FVector2f((float)C * 80.f, bMarked ? -12.f : 0.f);
		if (C == V.Cursor)
		{
			LyudPokerUI::Box(Out, Layer + 1, Geo, At - FVector2f(4.f, 4.f), FVector2f(80.f, 108.f), LyudUI::Text, true);
		}
		PaintCard(Out, Layer + 2, Geo, At, FVector2f(72.f, 100.f), V.Hero.Cards[C], true, V.Hero.bDim ? 0.45f : (bMarked ? 0.7f : 1.f));
	}
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 356.f), V.Hero.Hand, 13, "Bold", V.Hero.bWon ? LyudUI::Good : LyudUI::Text, 166.f);
	FitText(Out, Layer + 2, Geo, Panel + FVector2f(190.f, 357.f), V.Hero.Note, 12, "Regular", V.Hero.bTurn ? LyudUI::Bright : LyudUI::Dim,
		(V.Hero.bDealer ? 350.f : LogX) - 190.f - 8.f);
	if (V.Hero.bDealer)
	{
		LyudPokerUI::Box(Out, Layer + 2, Geo, Panel + FVector2f(350.f, 361.f), FVector2f(9.f, 9.f), LyudUI::Text, true);
		FitText(Out, Layer + 2, Geo, Panel + FVector2f(364.f, 357.f), V.DealerWord, 12, "Regular", LyudUI::Dim, LogX - 364.f - 6.f);
	}

	// the log, the newest line last and brightest
	const int32 First = FMath::Max(0, V.Log.Num() - 7);
	for (int32 I = First; I < V.Log.Num(); ++I)
	{
		FitText(Out, Layer + 2, Geo, Panel + FVector2f(LogX, 240.f + 17.f * (float)(I - First)), V.Log[I], 11, "Regular",
			I == V.Log.Num() - 1 ? LyudUI::Bright : (I >= V.Log.Num() - 3 ? LyudUI::Body : LyudUI::Dim), W - LogX - 18.f);
	}
	return Layer + 20;
}
