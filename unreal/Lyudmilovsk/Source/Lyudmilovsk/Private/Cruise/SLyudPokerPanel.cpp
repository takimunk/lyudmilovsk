#include "Cruise/SLyudPokerPanel.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Cruise/LyudShipPoker.h"
#include "Rendering/DrawElements.h"
#include "UI/LyudUIStyle.h"

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
	const FLinearColor Back(0.33f, 0.09f, 0.08f);
	const FLinearColor BackLine(0.62f, 0.3f, 0.2f);

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
		const FLinearColor& Color, bool bRound = false)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(At)), bRound ? Round() : White(),
			ESlateDrawEffect::None, Color);
	}

	template <typename TextT>
	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const TextT& S,
		const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(FVector2f(900.f, 40.f), FSlateLayoutTransform(At)), S, Font,
			ESlateDrawEffect::None, Color);
	}

	/** Is the point (U right, V down, both -1..1) inside the suit's shape? Suits: 0 spades, 1 hearts, 2 diamonds, 3 clubs. */
	bool Inside(int32 Suit, float U, float V)
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
}

void SLyudPokerPanel::PaintSuit(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Centre, float Size, int32 Suit,
	const FLinearColor& Color) const
{
	using namespace LyudPokerUI;
	// the shape in rows of plain boxes (a run of points inside it per row): no glyph, no brush, nothing to load
	const int32 Rows = FMath::Clamp(FMath::RoundToInt(Size * 0.5f), 6, 18);
	const int32 Cols = Rows * 2;
	const float RowH = Size / Rows;
	const float ColW = Size / Cols;
	for (int32 R = 0; R < Rows; ++R)
	{
		const float V = ((R + 0.5f) / Rows) * 2.f - 1.f;
		int32 From = -1;
		for (int32 C = 0; C <= Cols; ++C)
		{
			const bool bIn = C < Cols && Inside(Suit, ((C + 0.5f) / Cols) * 2.f - 1.f, V);
			if (bIn && From < 0)
			{
				From = C;
			}
			else if (!bIn && From >= 0)
			{
				Box(Out, Layer, Geo, Centre + FVector2f(-Size * 0.5f + From * ColW, -Size * 0.5f + R * RowH), FVector2f((C - From) * ColW, RowH + 0.5f), Color);
				From = -1;
			}
		}
	}
}

void SLyudPokerPanel::PaintCard(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size, int32 Card,
	bool bFace, float Fade) const
{
	using namespace LyudPokerUI;
	const FLinearColor Tint(Fade, Fade, Fade, 1.f);
	const bool bBig = Size.X >= 50.f;
	if (!bFace)
	{
		// the back: a dark red card with a frame and a lattice of small squares
		Box(Out, Layer, Geo, At, Size, Back * Tint, true);
		const float In = bBig ? 6.f : 3.f;
		const float Step = bBig ? 12.f : 6.f;
		const float Dot = bBig ? 5.f : 2.5f;
		Box(Out, Layer + 1, Geo, At + FVector2f(In, In), FVector2f(Size.X - 2.f * In, 1.f), BackLine * Tint);
		Box(Out, Layer + 1, Geo, At + FVector2f(In, Size.Y - In - 1.f), FVector2f(Size.X - 2.f * In, 1.f), BackLine * Tint);
		Box(Out, Layer + 1, Geo, At + FVector2f(In, In), FVector2f(1.f, Size.Y - 2.f * In), BackLine * Tint);
		Box(Out, Layer + 1, Geo, At + FVector2f(Size.X - In - 1.f, In), FVector2f(1.f, Size.Y - 2.f * In), BackLine * Tint);
		int32 Row = 0;
		for (float Y = In + Step * 0.5f; Y + Dot < Size.Y - In; Y += Step, ++Row)
		{
			for (float X = In + Step * (Row % 2 ? 1.f : 0.5f); X + Dot < Size.X - In; X += Step)
			{
				Box(Out, Layer + 1, Geo, At + FVector2f(X, Y), FVector2f(Dot, Dot), BackLine * Tint);
			}
		}
		return;
	}
	const int32 Rank = FMath::Clamp(Card / 4, 0, 12);
	const int32 Suit = Card % 4;
	const FLinearColor Ink = (Suit == 1 || Suit == 2 ? RedSuit : BlackSuit) * Tint;
	Box(Out, Layer, Geo, At, Size, Ivory * Tint, true);
	if (Ranks.IsValidIndex(Rank))
	{
		Text(Out, Layer + 2, Geo, At + (bBig ? FVector2f(7.f, 3.f) : FVector2f(3.f, 0.f)), Ranks[Rank], LyudUI::Font(bBig ? 20 : 10, "Bold"), Ink);
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
	using namespace LyudPokerUI;
	const ULyudShipPoker* Poker = Owner.Get();
	if (!Poker)
	{
		return Layer;
	}
	const FLyudPokerView& V = Poker->View();
	const FVector2f View = Geo.GetLocalSize();
	const float W = FMath::Min(View.X - 40.f, 900.f);
	const float PanelH = 424.f;
	const FVector2f Panel((View.X - W) * 0.5f, View.Y - PanelH - 24.f);
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, PanelH), FSlateLayoutTransform(Panel)), LyudUI::PanelBrush());

	const FSlateFontInfo Big = LyudUI::Font(18, "Bold");
	const FSlateFontInfo Mid = LyudUI::Font(14);
	const FSlateFontInfo Small = LyudUI::Font(12);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 10.f), V.Title, Big, LyudUI::Text);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, PanelH - 46.f), V.Keys, Small, LyudUI::Dim);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, PanelH - 26.f), V.Keys2, Small, LyudUI::Faint);

	if (V.bHelp && V.Help.Num() > 0)
	{
		// the rules, over the table
		const float HelpH = 24.f + 22.f * V.Help.Num();
		const FVector2f HelpAt(Panel.X, FMath::Max(12.f, Panel.Y - HelpH - 10.f));
		FSlateDrawElement::MakeBox(Out, Layer + 10, Geo.ToPaintGeometry(FVector2f(W, HelpH), FSlateLayoutTransform(HelpAt)), LyudUI::PanelBrush());
		for (int32 I = 0; I < V.Help.Num(); ++I)
		{
			Text(Out, Layer + 12, Geo, HelpAt + FVector2f(18.f, 12.f + 22.f * I), V.Help[I], LyudUI::Font(13), I == 0 ? LyudUI::Text : LyudUI::Body);
		}
	}

	if (V.Stage != 1)
	{
		// choosing the table, or the evening's count: plain lines
		for (int32 I = 0; I < V.Lines.Num(); ++I)
		{
			Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 54.f + 26.f * I), V.Lines[I], I == 0 ? LyudUI::Font(15, "Bold") : Mid,
				I == 0 ? LyudUI::Bright : LyudUI::Body);
		}
		Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 54.f + 26.f * V.Lines.Num() + 10.f), V.Banner, Mid, V.bBannerBad ? LyudUI::Bad : LyudUI::Good);
		return Layer + 20;
	}

	Text(Out, Layer + 2, Geo, Panel + FVector2f(560.f, 15.f), V.HandNo, Small, LyudUI::Dim);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(W * 0.5f - 70.f, 11.f), V.Pot, LyudUI::Font(16, "Bold"), LyudUI::Bright);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(W - 210.f, 13.f), V.Stack, Mid, LyudUI::Text);

	// the regulars across the top
	const int32 Shown = FMath::Min(V.Seats.Num(), 4);
	const float ColW = (W - 28.f) / 4.f;
	for (int32 I = 0; I < Shown; ++I)
	{
		const FLyudPokerSeatView& S = V.Seats[I];
		const FVector2f At = Panel + FVector2f(14.f + I * ColW + 3.f, 44.f);
		FSlateDrawElement::MakeBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(ColW - 6.f, 120.f), FSlateLayoutTransform(At)), LyudUI::CellBrush(S.bTurn));
		Text(Out, Layer + 3, Geo, At + FVector2f(10.f, 5.f), S.Name, LyudUI::Font(13, "Bold"), S.bDim ? LyudUI::Faint : (S.bTurn || S.bWon ? LyudUI::Bright : LyudUI::Text));
		Text(Out, Layer + 3, Geo, At + FVector2f(10.f, 25.f), S.Stack, Small, S.bDim ? LyudUI::Faint : LyudUI::Body);
		Text(Out, Layer + 3, Geo, At + FVector2f(96.f, 25.f), S.Note, Small, S.bDim ? LyudUI::Faint : LyudUI::Dim);
		if (S.bDealer)
		{
			Box(Out, Layer + 3, Geo, At + FVector2f(ColW - 24.f, 9.f), FVector2f(9.f, 9.f), LyudUI::Text, true);
		}
		const float Fade = S.bDim ? 0.45f : 1.f;
		for (int32 C = 0; C < S.NumCards; ++C)
		{
			PaintCard(Out, Layer + 3, Geo, At + FVector2f(10.f + C * 34.f, 46.f), FVector2f(30.f, 44.f), S.Cards[C], S.bFaces, Fade);
		}
		Text(Out, Layer + 3, Geo, At + FVector2f(10.f, 96.f), S.Hand, LyudUI::Font(11, "Bold"), S.bWon ? LyudUI::Good : LyudUI::Body);
	}

	// what gives a man away, and what just happened
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 170.f), V.Tell, LyudUI::Font(14, "Italic"), LyudUI::Bright);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 194.f), V.Banner, LyudUI::Font(13, "Bold"), V.bBannerBad ? LyudUI::Bad : LyudUI::Good);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 213.f), V.Banner2, LyudUI::Font(13), V.bBannerBad ? LyudUI::Bad : LyudUI::Good);

	// the hero's cards
	const FVector2f HandAt = Panel + FVector2f(18.f, 250.f);
	for (int32 C = 0; C < V.Hero.NumCards; ++C)
	{
		const bool bMarked = (V.Discard >> C) & 1;
		const FVector2f At = HandAt + FVector2f(C * 80.f, bMarked ? -12.f : 0.f);
		if (C == V.Cursor)
		{
			Box(Out, Layer + 1, Geo, At - FVector2f(4.f, 4.f), FVector2f(80.f, 108.f), LyudUI::Text, true);
		}
		PaintCard(Out, Layer + 2, Geo, At, FVector2f(72.f, 100.f), V.Hero.Cards[C], true, V.Hero.bDim ? 0.45f : (bMarked ? 0.7f : 1.f));
	}
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 356.f), V.Hero.Hand, LyudUI::Font(13, "Bold"), V.Hero.bWon ? LyudUI::Good : LyudUI::Text);
	Text(Out, Layer + 2, Geo, Panel + FVector2f(190.f, 357.f), V.Hero.Note, Small, V.Hero.bTurn ? LyudUI::Bright : LyudUI::Dim);
	if (V.Hero.bDealer)
	{
		Box(Out, Layer + 2, Geo, Panel + FVector2f(350.f, 361.f), FVector2f(9.f, 9.f), LyudUI::Text, true);
		Text(Out, Layer + 2, Geo, Panel + FVector2f(364.f, 357.f), V.DealerWord, Small, LyudUI::Dim);
	}

	// the log, the newest line last and brightest
	const float LogX = FMath::Min(438.f, W * 0.5f);
	const int32 First = FMath::Max(0, V.Log.Num() - 7);
	for (int32 I = First; I < V.Log.Num(); ++I)
	{
		Text(Out, Layer + 2, Geo, Panel + FVector2f(LogX, 240.f + 17.f * (I - First)), V.Log[I], LyudUI::Font(11),
			I == V.Log.Num() - 1 ? LyudUI::Bright : (I >= V.Log.Num() - 3 ? LyudUI::Body : LyudUI::Dim));
	}
	return Layer + 20;
}
