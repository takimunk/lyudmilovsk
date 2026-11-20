#include "Music/SLyudInstrument.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "LyudText.h"
#include "Music/LyudInstruments.h"
#include "Music/TunesModel.h"
#include "Rendering/DrawElements.h"
#include "UI/LyudUIStyle.h"

namespace LyudInstrUI
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
	/** The keyboard's letters for semitones 0..17 from its C (LyudInstruments.cpp: the same keys). */
	const TCHAR* const Letters[18] = { TEXT("A"), TEXT("W"), TEXT("S"), TEXT("E"), TEXT("D"), TEXT("F"), TEXT("T"), TEXT("G"), TEXT("Y"),
		TEXT("H"), TEXT("U"), TEXT("J"), TEXT("K"), TEXT("O"), TEXT("L"), TEXT("P"), TEXT(";"), TEXT("'") };
	const int32 WhiteSemis[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
	const int32 BlackSemis[7] = { 1, 3, 6, 8, 10, 13, 15 };

	int32 WhiteIndexOf(int32 Semi)
	{
		for (int32 I = 0; I < 11; ++I)
		{
			if (WhiteSemis[I] == Semi)
			{
				return I;
			}
		}
		return -1;
	}

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
		FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(FVector2f(600.f, 40.f), FSlateLayoutTransform(At)), S, Font,
			ESlateDrawEffect::None, Color);
	}
}

void SLyudInstrument::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	Owner = InArgs._Owner;
	const ULyudInstruments* I = Owner.Get();
	bGuitar = I && I->IsGuitar();
	Name = LyudText::Get(bGuitar ? TEXT("music.ui.guitar") : TEXT("music.ui.piano"));
	HintFree = LyudText::Get(bGuitar ? TEXT("music.ui.hint_guitar") : TEXT("music.ui.hint_piano"));
	HintTune = LyudText::Get(TEXT("music.ui.hint_tune"));
	HintList = LyudText::Get(TEXT("music.ui.hint_list"));
	ListTitle = LyudText::Get(TEXT("music.ui.list"));
	ListEmpty = LyudText::Get(TEXT("music.ui.list_empty"));
	Perfect = LyudText::Get(TEXT("music.ui.perfect"));
	Good = LyudText::Get(TEXT("music.ui.good"));
	Miss = LyudText::Get(TEXT("music.ui.miss"));
	ByFire = LyudText::Get(TEXT("music.ui.by_fire"));
	if (I)
	{
		for (int32 T = 0; T < I->NumTunes(); ++T)
		{
			Titles.Add(LyudText::Get(TEXT("music.tune.") + I->TuneId(T)));
		}
	}
	for (int32 N = 1; N <= 9; ++N)
	{
		TuneNumbers.Add(FString::FromInt(N));
	}
	for (int32 C = 0; C < 8; ++C)
	{
		ChordLabels.Add(FString::Printf(TEXT("%d  %s"), C + 1, UTF8_TO_TCHAR(LyudTunes::FreeChord(C))));
	}
	for (const TCHAR* L : LyudInstrUI::Letters)
	{
		KeyLetters.Add(L);
	}
	static const TCHAR* const Names[12] = { TEXT("C"), TEXT("C#"), TEXT("D"), TEXT("D#"), TEXT("E"), TEXT("F"), TEXT("F#"), TEXT("G"),
		TEXT("G#"), TEXT("A"), TEXT("A#"), TEXT("B") };
	for (int32 Oct = 0; Oct <= 8; ++Oct)
	{
		NoteNames.Add(FString::Printf(TEXT("%s%d"), Names[0], Oct));
	}
	for (int32 S = 0; S <= 3; ++S)
	{
		FString Stars;
		for (int32 K = 0; K < 3; ++K)
		{
			Stars.AppendChar(K < S ? TEXT('\x2605') : TEXT('\x2606'));
		}
		StarStrings.Add(Stars);
	}
	for (int32 K = 0; K < 4; ++K)
	{
		Count[K] = FString::FromInt(K + 1);
	}
}

int32 SLyudInstrument::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace LyudInstrUI;
	const ULyudInstruments* I = Owner.Get();
	if (!I)
	{
		return Layer;
	}
	const FVector2f View = Geo.GetLocalSize();
	const float W = FMath::Min(View.X - 40.f, 820.f);
	const float KeysH = bGuitar ? 70.f : 150.f;
	const float ChordsH = bGuitar ? 92.f : 0.f;
	const float LaneH = 130.f;
	const float PanelH = 44.f + LaneH + 10.f + ChordsH + KeysH + 34.f;
	const FVector2f Panel((View.X - W) * 0.5f, View.Y - PanelH - 24.f);

	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, PanelH), FSlateLayoutTransform(Panel)), LyudUI::PanelBrush());
	const FSlateFontInfo Big = LyudUI::Font(18, "Bold");
	const FSlateFontInfo Small = LyudUI::Font(12);

	// the heading: the instrument, the tune playing, by the fire
	const LyudTunes::FRhythm* R = I->Rhythm();
	const int32 TuneIx = Titles.IsValidIndex(I->PlayingTune()) ? I->PlayingTune() : -1;
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, 10.f), Name, Big, LyudUI::Text);
	if (TuneIx >= 0)
	{
		Text(Out, Layer + 2, Geo, Panel + FVector2f(200.f, 12.f), Titles[TuneIx], LyudUI::Font(15), LyudUI::Bright);
	}
	else if (I->FinishedStars() >= 0)
	{
		const int32 Done = I->FinishedTuneIndex();
		if (Titles.IsValidIndex(Done))
		{
			Text(Out, Layer + 2, Geo, Panel + FVector2f(200.f, 12.f), Titles[Done], LyudUI::Font(15), LyudUI::Body);
		}
		Text(Out, Layer + 2, Geo, Panel + FVector2f(W - 130.f, 6.f), StarStrings[FMath::Clamp(I->FinishedStars(), 0, 3)], LyudUI::Font(24, "Bold"),
			I->FinishedStars() > 0 ? LyudUI::Text : LyudUI::Dim);
	}
	if (I->IsByBonfire())
	{
		Text(Out, Layer + 2, Geo, Panel + FVector2f(W - 300.f, 14.f), ByFire, Small, LyudUI::Good);
	}

	// the lane over the instrument
	const FVector2f LaneAt = Panel + FVector2f(14.f, 44.f);
	PaintLane(Out, Layer + 1, Geo, LaneAt, FVector2f(W - 28.f, LaneH), *I);
	float Y = 44.f + LaneH + 10.f;
	if (bGuitar)
	{
		PaintStrings(Out, Layer + 1, Geo, Panel + FVector2f(14.f, Y), FVector2f(W - 28.f, ChordsH - 8.f), *I);
		Y += ChordsH;
	}
	PaintKeys(Out, Layer + 1, Geo, Panel + FVector2f(14.f, Y), FVector2f(W - 28.f, KeysH), *I);
	Y += KeysH + 8.f;
	Text(Out, Layer + 2, Geo, Panel + FVector2f(18.f, Y), I->IsListOpen() ? HintList : (R ? HintTune : HintFree), Small, LyudUI::Dim);

	if (I->IsListOpen())
	{
		const float LW = FMath::Min(W, 460.f);
		const float LH = 70.f + 30.f * 10.f;
		PaintList(Out, Layer + 10, Geo, FVector2f((View.X - LW) * 0.5f, FMath::Max(20.f, Panel.Y - LH - 12.f)), FVector2f(LW, LH), *I);
	}
	return Layer + 20;
}

void SLyudInstrument::PaintKeys(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
	const ULyudInstruments& I) const
{
	using namespace LyudInstrUI;
	const bool bFree = I.Rhythm() == nullptr && !I.IsListOpen();
	const float WW = Size.X / 11.f;
	const float BW = WW * 0.6f;
	const float BH = Size.Y * 0.6f;
	const FSlateFontInfo Letter = LyudUI::Font(bGuitar ? 12 : 15, "Bold");
	const FSlateFontInfo Small = LyudUI::Font(10);
	const FLinearColor Ivory = bGuitar ? FLinearColor(0.32f, 0.22f, 0.13f) : FLinearColor(0.93f, 0.89f, 0.8f);
	const FLinearColor Ebony = bGuitar ? FLinearColor(0.12f, 0.08f, 0.05f) : FLinearColor(0.06f, 0.05f, 0.045f);
	const FLinearColor Lit = FLinearColor(1.f, 0.7f, 0.35f);
	const float Fade = bFree ? 1.f : 0.45f;
	for (int32 K = 0; K < 11; ++K)
	{
		const int32 Semi = WhiteSemis[K];
		const bool bDown = I.IsKeyDown(I.BaseMidi() + Semi);
		const FVector2f P = At + FVector2f(K * WW + 1.f, 0.f);
		Box(Out, Layer, Geo, P, FVector2f(WW - 2.f, Size.Y), (bDown ? Lit : Ivory) * FLinearColor(Fade, Fade, Fade, 1.f), true);
		Text(Out, Layer + 3, Geo, P + FVector2f(WW * 0.5f - 6.f, Size.Y - (bGuitar ? 22.f : 28.f)), KeyLetters[Semi], Letter,
			bGuitar ? LyudUI::Bright : FLinearColor(0.2f, 0.15f, 0.1f, Fade));
		if (Semi % 12 == 0)
		{
			const int32 Oct = (I.BaseMidi() + Semi) / 12 - 1;
			if (NoteNames.IsValidIndex(Oct))
			{
				Text(Out, Layer + 3, Geo, P + FVector2f(4.f, BH + 2.f), NoteNames[Oct], Small, bGuitar ? LyudUI::Dim : FLinearColor(0.45f, 0.38f, 0.3f));
			}
		}
	}
	for (const int32 Semi : BlackSemis)
	{
		const int32 Left = WhiteIndexOf(Semi - 1);
		const bool bDown = I.IsKeyDown(I.BaseMidi() + Semi);
		const FVector2f P = At + FVector2f((Left + 1) * WW - BW * 0.5f, 0.f);
		Box(Out, Layer + 1, Geo, P, FVector2f(BW, BH), bDown ? Lit : Ebony, true);
		Text(Out, Layer + 4, Geo, P + FVector2f(BW * 0.5f - 5.f, BH - 22.f), KeyLetters[Semi], LyudUI::Font(11, "Bold"), LyudUI::Body * FLinearColor(1.f, 1.f, 1.f, Fade));
	}
	if (I.IsSustain())
	{
		Box(Out, Layer + 2, Geo, At + FVector2f(0.f, -5.f), FVector2f(Size.X, 3.f), LyudUI::Text);
	}
}

void SLyudInstrument::PaintStrings(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
	const ULyudInstruments& I) const
{
	using namespace LyudInstrUI;
	// six strings across, the low ones thicker, and the chords 1-8 on them
	for (int32 S = 0; S < 6; ++S)
	{
		const float T = 1.f + (5 - S) * 0.45f;
		Box(Out, Layer, Geo, At + FVector2f(0.f, 8.f + S * (Size.Y - 16.f) / 5.f), FVector2f(Size.X, T), FLinearColor(0.75f, 0.68f, 0.55f, 0.55f));
	}
	const bool bFree = I.Rhythm() == nullptr && !I.IsListOpen();
	const float CW = Size.X / 8.f;
	for (int32 C = 0; C < 8; ++C)
	{
		const FVector2f P = At + FVector2f(C * CW + 6.f, Size.Y * 0.5f - 16.f);
		Box(Out, Layer + 1, Geo, P, FVector2f(CW - 12.f, 32.f), FLinearColor(0.09f, 0.07f, 0.05f, bFree ? 0.92f : 0.5f), true);
		Text(Out, Layer + 2, Geo, P + FVector2f(10.f, 6.f), ChordLabels[C], LyudUI::Font(14, "Bold"), bFree ? LyudUI::Text : LyudUI::Faint);
	}
}

void SLyudInstrument::PaintLane(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
	const ULyudInstruments& I) const
{
	using namespace LyudInstrUI;
	Box(Out, Layer, Geo, At, Size, FLinearColor(0.f, 0.f, 0.f, 0.35f), true);
	const float HitX = At.X + 70.f;
	Box(Out, Layer + 2, Geo, FVector2f(HitX - 1.5f, At.Y + 4.f), FVector2f(3.f, Size.Y - 8.f), LyudUI::Text);
	const LyudTunes::FRhythm* R = I.Rhythm();
	if (!R || !R->Tune)
	{
		return;
	}
	const LyudTunes::FTune& T = *R->Tune;
	const double Now = I.MusicClock();
	constexpr float PxPerSecond = 240.f;
	const double Spb = T.SecondsPerBeat();
	// the notes' heights: the tune's lowest to its highest pitch
	int32 Lo = 127, Hi = 0;
	for (const LyudTunes::FNote& N : T.Notes)
	{
		for (const int M : N.Midi)
		{
			Lo = FMath::Min(Lo, M);
			Hi = FMath::Max(Hi, M);
		}
	}
	const float Span = (float)FMath::Max(Hi - Lo, 1);
	const float NoteH = 14.f;
	for (int32 N = FMath::Max(0, R->Next - 6); N < (int32)T.Notes.size(); ++N)
	{
		const LyudTunes::FNote& Note = T.Notes[(size_t)N];
		if (Note.Midi.empty())
		{
			continue;
		}
		const float X = HitX + (float)((R->TimeOf(N) - Now) * PxPerSecond);
		if (X > At.X + Size.X)
		{
			break;
		}
		const float Len = FMath::Max(10.f, (float)(Note.Len * Spb * PxPerSecond) - 4.f);
		if (X + Len < At.X)
		{
			continue;
		}
		const int32 Top = Note.Midi.back();
		const float Y = At.Y + 8.f + (1.f - (Top - Lo) / Span) * (Size.Y - 16.f - NoteH);
		const LyudTunes::EHit J = R->Judged[(size_t)N];
		const FLinearColor C = J == LyudTunes::EHit::Perfect ? LyudUI::Bright : J == LyudTunes::EHit::Good ? LyudUI::Good
			: J == LyudTunes::EHit::Miss ? LyudUI::Bad * FLinearColor(1.f, 1.f, 1.f, 0.6f) : LyudUI::Text;
		const float X0 = FMath::Max(X, At.X);
		const float W = FMath::Min(X + Len, At.X + Size.X) - X0;
		if (W > 1.f)
		{
			Box(Out, Layer + 1, Geo, FVector2f(X0, Y), FVector2f(W, NoteH), C, true);
		}
	}
	// the count-in, then the word for the last stroke
	const double Beat = R->BeatAt(Now);
	const FSlateFontInfo Big = LyudUI::Font(30, "Bold");
	if (Beat < 0.0)
	{
		const int32 K = FMath::Clamp((int32)FMath::CeilToDouble(-Beat), 1, 4) - 1;
		Text(Out, Layer + 3, Geo, FVector2f(At.X + Size.X * 0.5f - 10.f, At.Y + Size.Y * 0.5f - 24.f), Count[K], Big, LyudUI::Bright);
	}
	const double Since = Now - I.LastHitAt();
	if (I.LastHit() > 0 && Since < 0.6)
	{
		const float A = (float)(1.0 - Since / 0.6);
		const FText& Word = I.LastHit() == 1 ? Perfect : (I.LastHit() == 2 ? Good : Miss);
		const FLinearColor C = I.LastHit() == 1 ? LyudUI::Bright : (I.LastHit() == 2 ? LyudUI::Good : LyudUI::Bad);
		Text(Out, Layer + 3, Geo, FVector2f(HitX - 40.f, At.Y + 2.f - (1.f - A) * 10.f), Word, LyudUI::Font(16, "Bold"), C * FLinearColor(1.f, 1.f, 1.f, A));
	}
	// how far through
	const float Through = T.Beats > 0.0 ? FMath::Clamp((float)(Beat / T.Beats), 0.f, 1.f) : 0.f;
	Box(Out, Layer + 1, Geo, FVector2f(At.X, At.Y + Size.Y - 3.f), FVector2f(Size.X * Through, 3.f), LyudUI::Dim);
}

void SLyudInstrument::PaintList(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& At, const FVector2f& Size,
	const ULyudInstruments& I) const
{
	using namespace LyudInstrUI;
	FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(At)), LyudUI::PanelBrush());
	Text(Out, Layer + 2, Geo, At + FVector2f(18.f, 12.f), ListTitle, LyudUI::Font(17, "Bold"), LyudUI::Text);
	int32 Shown = 0;
	const FSlateFontInfo Font = LyudUI::Font(15);
	const FSlateFontInfo Num = LyudUI::Font(13, "Bold");
	// the instrument's tunes he knows, in the order the keys pick them (ULyudInstruments::PlayableTunes)
	for (int32 T = 0; T < I.NumTunes() && Shown < 10; ++T)
	{
		if (!I.CanPlay(T) || !Titles.IsValidIndex(T))
		{
			continue;
		}
		const float Y = 50.f + Shown * 30.f;
		const bool bCursor = Shown == I.ListCursor();
		if (bCursor)
		{
			Box(Out, Layer + 1, Geo, At + FVector2f(10.f, Y - 3.f), FVector2f(Size.X - 20.f, 28.f), FLinearColor(1.f, 0.78f, 0.47f, 0.12f), true);
		}
		if (TuneNumbers.IsValidIndex(Shown))
		{
			Text(Out, Layer + 2, Geo, At + FVector2f(22.f, Y + 1.f), TuneNumbers[Shown], Num, LyudUI::Dim);
		}
		Text(Out, Layer + 2, Geo, At + FVector2f(48.f, Y), Titles[T], Font, bCursor ? LyudUI::Bright : LyudUI::Body);
		++Shown;
	}
	if (Shown == 0)
	{
		Text(Out, Layer + 2, Geo, At + FVector2f(22.f, 54.f), ListEmpty, Font, LyudUI::Dim);
	}
}
