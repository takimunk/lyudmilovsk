#include "Books/SLyudBook.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

static_assert(sizeof(TCHAR) == sizeof(char16_t), "the book text is drawn straight from UTF-16");

namespace LyudBookDraw
{
	const FLinearColor Shade(0.f, 0.f, 0.f, 0.78f);
	const FLinearColor Paper(0.95f, 0.92f, 0.85f);
	const FLinearColor PaperEdge(0.78f, 0.73f, 0.63f);
	const FLinearColor Gutter(0.f, 0.f, 0.f, 0.10f);
	const FLinearColor Ink(0.13f, 0.11f, 0.09f);
	const FLinearColor InkSoft(0.42f, 0.37f, 0.30f);
	const FLinearColor Hint(0.86f, 0.80f, 0.70f, 0.85f);

	const FSlateBrush* White()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** Glyph widths of one font, measured once on the game thread for the scripts the books use; the rest of CJK takes '漢'. */
	struct FWidths
	{
		std::vector<float> Table;
		float Cjk = 16.f;
		float Other = 8.f;
		float LineH = 20.f;
		float Em = 16.f;

		float Of(char16_t C) const
		{
			const float W = Table[(size_t)C];
			if (W >= 0.f)
			{
				return W;
			}
			return LyudBooks::IsCJK(C) ? Cjk : Other;
		}
	};

	TSharedPtr<const FWidths, ESPMode::ThreadSafe> MeasureFont(const FSlateFontInfo& Font)
	{
		// one table per face and size for the whole run (the reader opens many books at a few sizes)
		// (keyed without a string: this is asked for every page drawn, every frame)
		static TMap<TPair<FName, int32>, TSharedPtr<const FWidths, ESPMode::ThreadSafe>> Cache;
		const TPair<FName, int32> Key(Font.TypefaceFontName, (int32)Font.Size);
		if (const TSharedPtr<const FWidths, ESPMode::ThreadSafe>* Hit = Cache.Find(Key))
		{
			return *Hit;
		}
		TSharedPtr<FWidths, ESPMode::ThreadSafe> W = MakeShared<FWidths, ESPMode::ThreadSafe>();
		W->Table.assign(65536, -1.f);
		const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		auto One = [&M, &Font](TCHAR C) { const TCHAR S[2] = { C, 0 }; return (float)M->Measure(S, Font).X; };
		const uint32 Ranges[][2] = { { 0x20, 0x24F }, { 0x370, 0x3FF }, { 0x400, 0x52F }, { 0x2000, 0x206F }, { 0x2100, 0x215F },
									 { 0x3000, 0x30FF }, { 0xFF00, 0xFFEF } };
		for (const auto& R : Ranges)
		{
			for (uint32 C = R[0]; C <= R[1]; ++C)
			{
				W->Table[C] = One((TCHAR)C);
			}
		}
		W->Table[0x0A] = 0.f;
		W->Table[0x0D] = 0.f;
		W->Cjk = One(TEXT('漢'));
		W->Other = One(TEXT('n'));
		W->Em = One(TEXT('M'));
		W->LineH = FMath::Max((float)M->GetMaxCharacterHeight(Font), (float)M->Measure(TEXT("漢Ay"), Font).Y) * 1.28f;
		Cache.Add(Key, W);
		return W;
	}
}

// ------------------------------------------------------------------------------------------------------------- the reader

float SLyudBookReader::FontSizeAt(int32 Index)
{
	static const float Sizes[] = { 12.f, 14.f, 16.f, 18.f, 21.f, 24.f };
	return Sizes[FMath::Clamp(Index, 0, (int32)UE_ARRAY_COUNT(Sizes) - 1)];
}

void SLyudBookReader::Construct(const FArguments& InArgs)
{
	Book = InArgs._Book;
	OnClose = InArgs._OnClose;
	WantOffset = FMath::Max(0, InArgs._Bookmark);
	SetCanTick(true);
	LoadingText = LyudText::Get(TEXT("ui.books.loading")).ToString();
	MissingText = LyudText::Get(TEXT("ui.books.missing")).ToString();
	KeysText = LyudText::Get(TEXT("ui.books.keys")).ToString();
	ContentsTitle = LyudText::Get(TEXT("ui.books.contents")).ToString();
	CreditsEnd = LyudText::Get(TEXT("ui.books.the_end")).ToString();
	CreditsLicense = LyudText::Get(TEXT("ui.books.credit.license"), { FStringFormatArg(Book.License) }).ToString();
	CreditsSource = LyudText::Get(TEXT("ui.books.credit.source"), { FStringFormatArg(Book.Source) }).ToString();
	const FString Path = ULyudBooks::PathOf(Book);
	// the file is read and turned into UTF-16 on a worker (a long novel is a few MB)
	LoadTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Path]
	{
		FLoaded R;
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path))
		{
			return R;
		}
		std::u16string U = LyudBooks::FromUtf8((const char*)Bytes.GetData(), (size_t)Bytes.Num());
		R.Text = FString((int32)U.size(), (const TCHAR*)U.data());
		R.U16 = MakeShared<std::u16string, ESPMode::ThreadSafe>(MoveTemp(U));
		R.bOk = true;
		return R;
	});
}

SLyudBookReader::~SLyudBookReader()
{
	// a task still running holds its own copies: nothing of ours to wait for
}

int32 SLyudBookReader::NumPages() const
{
	return Layout.IsValid() ? (int32)Layout->Pages.size() + 1 : 0;
}

int32 SLyudBookReader::Offset() const
{
	if (!Layout.IsValid() || Layout->Pages.empty() || PageIndex < 0)
	{
		return WantOffset;
	}
	const int32 P = FMath::Clamp(PageIndex, 0, (int32)Layout->Pages.size() - 1);
	return Layout->Pages[(size_t)P].Start;
}

bool SLyudBookReader::AtEnd() const
{
	return Layout.IsValid() && PageIndex + PagesPerSpread() >= NumPages();
}

int32 SLyudBookReader::Turn(int32 Spreads)
{
	if (!Layout.IsValid() || Spreads == 0 || bContents)
	{
		return 0;
	}
	const int32 Per = PagesPerSpread();
	const int32 LastStart = FMath::Max(0, ((NumPages() - 1) / Per) * Per);
	const int32 Was = PageIndex;
	PageIndex = FMath::Clamp(PageIndex + Spreads * Per, 0, LastStart);
	if (PageIndex <= Was)
	{
		return 0;
	}
	int32 Chars = 0;
	for (int32 P = FMath::Max(Was + Per, PageIndex); P < PageIndex + Per && P < (int32)Layout->Pages.size(); ++P)
	{
		Chars += Layout->Pages[(size_t)P].End - Layout->Pages[(size_t)P].Start;
	}
	return Chars;
}

void SLyudBookReader::GoToOffset(int32 InOffset)
{
	WantOffset = InOffset;
	if (Layout.IsValid())
	{
		const int32 Per = PagesPerSpread();
		PageIndex = (Layout->PageOf(InOffset) / Per) * Per;
	}
}

void SLyudBookReader::StepFont(int32 Step)
{
	const int32 Next = FMath::Clamp(FontIndex + Step, 0, 5);
	if (Next != FontIndex)
	{
		WantOffset = Offset();
		FontIndex = Next;
		// Tick sees the font differ from the layout's and lays the book out again at the bookmark
	}
}

void SLyudBookReader::ToggleContents()
{
	if (Layout.IsValid() && !Layout->Headings.empty())
	{
		bContents = !bContents;
		if (bContents)
		{
			// the chapter he is in near the top of the list
			int32 Cur = 0;
			for (int32 I = 0; I < (int32)Layout->Headings.size(); ++I)
			{
				if (Layout->Headings[(size_t)I].Page <= PageIndex + PagesPerSpread() - 1)
				{
					Cur = I;
				}
			}
			ContentsTop = FMath::Max(0, Cur - 3);
		}
	}
}

void SLyudBookReader::ScrollContents(int32 RowsBy)
{
	if (Layout.IsValid())
	{
		ContentsTop = FMath::Clamp(ContentsTop + RowsBy, 0, FMath::Max(0, (int32)Layout->Headings.size() - ContentsRows()));
	}
}

int32 SLyudBookReader::ContentsRows() const
{
	return FMath::Max(4, (int32)(Spread.PageSize.Y * 0.78f / (Spread.LineH * 1.15f)));
}

void SLyudBookReader::ComputeSpread()
{
	const FVector2f S = Allotted;
	BodyFont = LyudUI::Font((int32)FontSizeAt(FontIndex), "Regular");
	HeadFont = LyudUI::Font((int32)FontSizeAt(FontIndex) + 3, "Bold");
	SmallFont = LyudUI::Font(FMath::Max(10, (int32)FontSizeAt(FontIndex) - 3), "Regular");
	const TSharedPtr<const LyudBookDraw::FWidths, ESPMode::ThreadSafe> W = LyudBookDraw::MeasureFont(BodyFont);
	Spread.bTwo = S.X >= S.Y * 1.15f;
	float PageH = S.Y * 0.86f;
	float PageW = PageH * 0.70f;
	const float Total = Spread.bTwo ? PageW * 2.f : PageW;
	if (Total > S.X * 0.96f)
	{
		const float K = S.X * 0.96f / Total;
		PageH *= K;
		PageW *= K;
	}
	Spread.PageSize = FVector2f(PageW, PageH);
	Spread.Origin = FVector2f((S.X - (Spread.bTwo ? PageW * 2.f : PageW)) * 0.5f, (S.Y - PageH) * 0.42f);
	Spread.TextOffset = FVector2f(PageW * 0.10f, PageH * 0.085f);
	Spread.TextSize = FVector2f(PageW * 0.80f, PageH * 0.81f);
	Spread.LineH = W->LineH;
	Spread.Rows = FMath::Max(6, (int32)(Spread.TextSize.Y / Spread.LineH));
}

void SLyudBookReader::StartLayout()
{
	if (!U16.IsValid())
	{
		return;
	}
	ComputeSpread();
	LyudBooks::FParams P;
	// a little slack: the layout adds glyph advances, the drawing kerns them
	P.Width = Spread.TextSize.X * 0.985f;
	P.Rows = Spread.Rows;
	const TSharedPtr<const LyudBookDraw::FWidths, ESPMode::ThreadSafe> BodyW = LyudBookDraw::MeasureFont(BodyFont);
	const TSharedPtr<const LyudBookDraw::FWidths, ESPMode::ThreadSafe> HeadW = LyudBookDraw::MeasureFont(HeadFont);
	P.Indent = BodyW->Em * 1.4f;
	P.Width16 = [BodyW](char16_t C) { return BodyW->Of(C); };
	P.HeadWidth16 = [HeadW](char16_t C) { return HeadW->Of(C); };
	const TSharedPtr<std::u16string, ESPMode::ThreadSafe> Src = U16;
	LayoutTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Src, P]() -> TSharedPtr<LyudBooks::FLayout, ESPMode::ThreadSafe>
	{
		return MakeShared<LyudBooks::FLayout, ESPMode::ThreadSafe>(LyudBooks::Layout(*Src, P));
	});
	bLayingOut = true;
	LaidFor = Allotted;
	LaidFontIndex = FontIndex;
}

void SLyudBookReader::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Allotted = FVector2f(AllottedGeometry.GetLocalSize());
	if (bLoading && LoadTask.IsCompleted())
	{
		bLoading = false;
		FLoaded R = LoadTask.GetResult();
		if (!R.bOk)
		{
			bFailed = true;
			return;
		}
		Text = MoveTemp(R.Text);
		U16 = R.U16;
		StartLayout();
		return;
	}
	if (bLayingOut)
	{
		if (!LayoutTask.IsCompleted())
		{
			return;
		}
		bLayingOut = false;
		Layout = LayoutTask.GetResult();
		FolioFor = -2;  // the running heads and folios come from the new layout
		ComputeSpread();
		const int32 Per = PagesPerSpread();
		PageIndex = (Layout->PageOf(WantOffset) / Per) * Per;
		return;
	}
	UpdateFolio();
	if (!bLoading && !bFailed && U16.IsValid() && Allotted.X > 10.f
		&& (LaidFontIndex != FontIndex || FMath::Abs(LaidFor.X - Allotted.X) > 2.f || FMath::Abs(LaidFor.Y - Allotted.Y) > 2.f))
	{
		WantOffset = Offset();
		StartLayout();
	}
}

float SLyudBookReader::LineWidth(int32 Start, int32 Len, bool bHead) const
{
	const TSharedPtr<const LyudBookDraw::FWidths, ESPMode::ThreadSafe> W = LyudBookDraw::MeasureFont(bHead ? HeadFont : BodyFont);
	float X = 0.f;
	const TCHAR* S = *Text;
	for (int32 I = Start; I < Start + Len && I < Text.Len(); ++I)
	{
		X += W->Of((char16_t)S[I]);
	}
	return X;
}

void SLyudBookReader::UpdateFolio()
{
	if (!Layout.IsValid() || FolioFor == PageIndex + (Spread.bTwo ? 10000000 : 0))
	{
		return;
	}
	FolioFor = PageIndex + (Spread.bTwo ? 10000000 : 0);
	const int32 Total = NumPages();
	Folio[0] = PageIndex < Total ? FString::FromInt(PageIndex + 1) : FString();
	Folio[1] = PageIndex + 1 < Total ? FString::FromInt(PageIndex + 2) : FString();
	HeadLeft = Book.TitleOwn();
	HeadRight = RunningHead(PageIndex + PagesPerSpread() - 1);
	if (!Spread.bTwo && HeadRight.IsEmpty())
	{
		HeadRight = HeadLeft;
	}
}

FString SLyudBookReader::RunningHead(int32 Page) const
{
	FString Head;
	if (Layout.IsValid())
	{
		for (const LyudBooks::FHeading& H : Layout->Headings)
		{
			if (H.Page > Page)
			{
				break;
			}
			Head = Text.Mid(H.Start, FMath::Min(H.Len, 60));
		}
	}
	return Head;
}

void SLyudBookReader::PaintPage(int32 Page, const FGeometry& G, FVector2f PageOrigin, FSlateWindowElementList& Out, int32 Layer, bool bLeft) const
{
	using namespace LyudBookDraw;
	const FVector2f TextOrigin = PageOrigin + Spread.TextOffset;
	if (Page >= (int32)Layout->Pages.size())
	{
		if (Page == (int32)Layout->Pages.size())
		{
			PaintCredits(G, TextOrigin, Out, Layer);
		}
		return;
	}
	const LyudBooks::FPage& Pg = Layout->Pages[(size_t)Page];
	const float Indent = MeasureFont(BodyFont)->Em * 1.4f;
	for (int32 K = 0; K < Pg.NumLines; ++K)
	{
		const LyudBooks::FLine& Ln = Layout->Lines[(size_t)(Pg.FirstLine + K)];
		if (Ln.Len <= 0)
		{
			continue;
		}
		const bool bHead = Ln.Kind == LyudBooks::ELineKind::Heading;
		float X = Ln.Kind == LyudBooks::ELineKind::First ? Indent : 0.f;
		if (bHead)
		{
			X = FMath::Max(0.f, (Spread.TextSize.X - LineWidth(Ln.Start, Ln.Len, true)) * 0.5f);
		}
		const FVector2f At = TextOrigin + FVector2f(X, Ln.Row * Spread.LineH);
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2f(Spread.TextSize.X, Spread.LineH), FSlateLayoutTransform(At)), Text,
			Ln.Start, Ln.Start + Ln.Len, bHead ? HeadFont : BodyFont, ESlateDrawEffect::None, Ink);
	}
}

void SLyudBookReader::PaintCredits(const FGeometry& G, FVector2f TextOrigin, FSlateWindowElementList& Out, int32 Layer) const
{
	using namespace LyudBookDraw;
	float Y = Spread.TextSize.Y * 0.22f;
	auto Line = [&](const FString& S, const FSlateFontInfo& F, const FLinearColor& C, bool bCenter)
	{
		const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FString Fit = S;
		while (Fit.Len() > 4 && M->Measure(Fit, F).X > Spread.TextSize.X)
		{
			Fit = Fit.LeftChop(5) + TEXT("…");
		}
		const float X = bCenter ? FMath::Max(0.f, (Spread.TextSize.X - (float)M->Measure(Fit, F).X) * 0.5f) : 0.f;
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2f(Spread.TextSize.X, Spread.LineH * 1.4f),
			FSlateLayoutTransform(TextOrigin + FVector2f(X, Y))), Fit, F, ESlateDrawEffect::None, C);
		Y += Spread.LineH * 1.25f;
	};
	Line(CreditsEnd, HeadFont, Ink, true);
	Y += Spread.LineH;
	Line(Book.TitleOwn(), BodyFont, Ink, true);
	Y += Spread.LineH * 0.5f;
	for (const FString& C : Book.Credits)
	{
		Line(C, SmallFont, InkSoft, true);
	}
	Y += Spread.LineH * 0.5f;
	Line(CreditsLicense, SmallFont, InkSoft, true);
	Line(CreditsSource, SmallFont, InkSoft, true);
}

void SLyudBookReader::PaintContents(const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) const
{
	using namespace LyudBookDraw;
	const FVector2f PanelOrigin = Spread.Origin + FVector2f(Spread.bTwo ? Spread.PageSize.X : 0.f, 0.f);
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(Spread.PageSize, FSlateLayoutTransform(PanelOrigin)), White(), ESlateDrawEffect::None, Paper);
	const FVector2f TextOrigin = PanelOrigin + Spread.TextOffset;
	FSlateDrawElement::MakeText(Out, Layer + 1, G.ToPaintGeometry(FVector2f(Spread.TextSize.X, Spread.LineH * 1.5f), FSlateLayoutTransform(TextOrigin)),
		ContentsTitle, HeadFont, ESlateDrawEffect::None, Ink);
	const float RowH = Spread.LineH * 1.15f;
	const int32 RowsShown = ContentsRows();
	const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (int32 R = 0; R < RowsShown; ++R)
	{
		const int32 I = ContentsTop + R;
		if (I >= (int32)Layout->Headings.size())
		{
			break;
		}
		const LyudBooks::FHeading& H = Layout->Headings[(size_t)I];
		const bool bHere = H.Page <= PageIndex + PagesPerSpread() - 1
			&& (I + 1 >= (int32)Layout->Headings.size() || Layout->Headings[(size_t)I + 1].Page > PageIndex);
		const float Y = Spread.LineH * 2.f + R * RowH;
		const FString Num = FString::FromInt(H.Page + 1);
		const float NumW = (float)M->Measure(Num, BodyFont).X;
		FSlateDrawElement::MakeText(Out, Layer + 1, G.ToPaintGeometry(FVector2f(Spread.TextSize.X - NumW - 12.f, RowH), FSlateLayoutTransform(TextOrigin + FVector2f(0.f, Y))),
			Text, H.Start, H.Start + FMath::Min(H.Len, 48), BodyFont, ESlateDrawEffect::None, bHere ? Ink : InkSoft);
		FSlateDrawElement::MakeText(Out, Layer + 1, G.ToPaintGeometry(FVector2f(NumW + 2.f, RowH), FSlateLayoutTransform(TextOrigin + FVector2f(Spread.TextSize.X - NumW, Y))),
			Num, BodyFont, ESlateDrawEffect::None, InkSoft);
	}
}

int32 SLyudBookReader::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace LyudBookDraw;
	const FVector2f Size(G.GetLocalSize());
	FSlateDrawElement::MakeBox(Out, LayerId, G.ToPaintGeometry(Size, FSlateLayoutTransform()), White(), ESlateDrawEffect::None, Shade);
	const int32 Per = PagesPerSpread();
	// the paper: one or two pages, a shade along the gutter
	for (int32 K = 0; K < Per; ++K)
	{
		const FVector2f O = Spread.Origin + FVector2f(K * Spread.PageSize.X, 0.f);
		FSlateDrawElement::MakeBox(Out, LayerId + 1, G.ToPaintGeometry(Spread.PageSize + FVector2f(2.f, 2.f), FSlateLayoutTransform(O - FVector2f(1.f, 1.f))),
			White(), ESlateDrawEffect::None, PaperEdge);
		FSlateDrawElement::MakeBox(Out, LayerId + 2, G.ToPaintGeometry(Spread.PageSize, FSlateLayoutTransform(O)), White(), ESlateDrawEffect::None, Paper);
	}
	if (Per == 2)
	{
		const float GW = Spread.PageSize.X * 0.05f;
		FSlateDrawElement::MakeBox(Out, LayerId + 3, G.ToPaintGeometry(FVector2f(GW * 2.f, Spread.PageSize.Y),
			FSlateLayoutTransform(Spread.Origin + FVector2f(Spread.PageSize.X - GW, 0.f))), White(), ESlateDrawEffect::None, Gutter);
	}
	const int32 TextLayer = LayerId + 4;
	if (!Layout.IsValid())
	{
		const FString& Msg = bFailed ? MissingText : LoadingText;
		FSlateDrawElement::MakeText(Out, TextLayer, G.ToPaintGeometry(Spread.TextSize, FSlateLayoutTransform(Spread.Origin + Spread.TextOffset)),
			Msg, BodyFont, ESlateDrawEffect::None, InkSoft);
		return TextLayer;
	}
	for (int32 K = 0; K < Per; ++K)
	{
		const int32 P = PageIndex + K;
		const FVector2f O = Spread.Origin + FVector2f(K * Spread.PageSize.X, 0.f);
		PaintPage(P, G, O, Out, TextLayer, K == 0);
		if (P < NumPages())
		{
			// the running head (the book on the left page, the chapter on the right) and the folio
			const FString& Head = (K == 0 && Per == 2) ? HeadLeft : HeadRight;
			if (!Head.IsEmpty() && P < (int32)Layout->Pages.size())
			{
				FSlateDrawElement::MakeText(Out, TextLayer, G.ToPaintGeometry(FVector2f(Spread.TextSize.X, Spread.LineH),
					FSlateLayoutTransform(O + FVector2f(Spread.TextOffset.X, Spread.PageSize.Y * 0.035f))), Head, SmallFont, ESlateDrawEffect::None, InkSoft);
			}
			FSlateDrawElement::MakeText(Out, TextLayer, G.ToPaintGeometry(FVector2f(Spread.PageSize.X, Spread.LineH),
				FSlateLayoutTransform(O + FVector2f(Spread.PageSize.X * 0.5f - 8.f, Spread.PageSize.Y * 0.935f))), K == 0 ? Folio[0] : Folio[1], SmallFont,
				ESlateDrawEffect::None, InkSoft);
		}
	}
	if (bContents)
	{
		PaintContents(G, Out, TextLayer + 1);
	}
	FSlateDrawElement::MakeText(Out, TextLayer, G.ToPaintGeometry(FVector2f(Size.X, 30.f), FSlateLayoutTransform(FVector2f(Spread.Origin.X,
		Spread.Origin.Y + Spread.PageSize.Y + 12.f))), KeysText, SmallFont, ESlateDrawEffect::None, Hint);
	return TextLayer + 2;
}

FReply SLyudBookReader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!Layout.IsValid())
	{
		return FReply::Handled();
	}
	const FVector2f P(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (bContents)
	{
		const FVector2f PanelOrigin = Spread.Origin + FVector2f(Spread.bTwo ? Spread.PageSize.X : 0.f, 0.f) + Spread.TextOffset;
		const float RowH = Spread.LineH * 1.15f;
		const float Y = P.Y - PanelOrigin.Y - Spread.LineH * 2.f;
		const int32 Row = Y >= 0.f ? (int32)(Y / RowH) : -1;
		const int32 I = ContentsTop + Row;
		if (Row >= 0 && Row < ContentsRows() && I < (int32)Layout->Headings.size() && P.X >= PanelOrigin.X - 10.f && P.X <= PanelOrigin.X + Spread.TextSize.X + 10.f)
		{
			GoToOffset(Layout->Headings[(size_t)I].Start);
		}
		bContents = false;
		return FReply::Handled();
	}
	const float Mid = Spread.Origin.X + (Spread.bTwo ? Spread.PageSize.X : Spread.PageSize.X * 0.5f);
	AskedTurns += P.X >= Mid ? 1 : -1;
	return FReply::Handled();
}

FReply SLyudBookReader::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bContents)
	{
		ScrollContents(MouseEvent.GetWheelDelta() > 0.f ? -2 : 2);
	}
	else
	{
		AskedTurns += MouseEvent.GetWheelDelta() > 0.f ? -1 : 1;
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------------------------------------- the catalogue

void SLyudBookCatalog::Construct(const FArguments& InArgs)
{
	Books = InArgs._Books;
	bAtDesk = InArgs._AtDesk;
	OnRead = InArgs._OnRead;
	OnClose = InArgs._OnClose;
	LangFilter = bAtDesk ? LyudText::Language() : FString();
	const FSlateFontInfo Big = LyudUI::Font(22, "Bold");
	const FSlateFontInfo Normal = LyudUI::Font(15);
	TWeakPtr<SLyudBookCatalog> WeakSelf = SharedThis(this);
	TSharedRef<SHorizontalBox> Langs = SNew(SHorizontalBox);
	if (bAtDesk)
	{
		Langs->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[LangButton(FString(), LyudText::Get(TEXT("ui.books.filter.all")))];
		Langs->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[LangButton(TEXT("ru"), LyudText::Get(TEXT("ui.books.lang.ru")))];
		Langs->AddSlot().AutoWidth().Padding(0, 0, 8, 0)[LangButton(TEXT("en"), LyudText::Get(TEXT("ui.books.lang.en")))];
		Langs->AddSlot().AutoWidth().Padding(0, 0, 18, 0)[LangButton(TEXT("ja"), LyudText::Get(TEXT("ui.books.lang.ja")))];
	}
	Langs->AddSlot().FillWidth(1.f)
	[
		SNew(SEditableTextBox)
		.Font(Normal)
		.HintText(LyudText::Get(TEXT("ui.books.search")))
		.OnTextChanged_Lambda([WeakSelf](const FText& T)
		{
			if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin())
			{
				S->Filter = T.ToString();
				S->Refill();
			}
		})
	];
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(LyudUI::ShadeBrush())]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(980.f).HeightOverride(720.f)
			[
				SNew(SBorder).BorderImage(LyudUI::PanelBrush()).Padding(26.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Big).ColorAndOpacity(LyudUI::Bright)
							.Text(LyudText::Get(bAtDesk ? TEXT("ui.books.catalog.title") : TEXT("ui.books.mine.title")))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton).ButtonStyle(&LyudUI::Button()).ContentPadding(FMargin(14.f, 6.f))
							.OnClicked_Lambda([WeakSelf] { if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin()) { S->OnClose.ExecuteIfBound(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Font(Normal).ColorAndOpacity(LyudUI::Text).Text(LyudText::Get(TEXT("ui.books.close")))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)
					[
						SAssignNew(Status, STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Dim)
						.Text_Lambda([WeakSelf] { TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin(); return S ? S->StatusText() : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[Langs]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(List, SListView<TSharedPtr<FString>>)
						.ListItemsSource(&Rows)
						.SelectionMode(ESelectionMode::None)
						.OnGenerateRow(this, &SLyudBookCatalog::MakeRow)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
					[
						SNew(STextBlock).Font(LyudUI::Font(12)).ColorAndOpacity(LyudUI::Faint).AutoWrapText(true)
						.Text(LyudText::Get(TEXT("ui.books.catalog.note")))
					]
				]
			]
		]
	];
	Refill();
}

TSharedRef<SWidget> SLyudBookCatalog::LangButton(const FString& Lang, const FText& Label)
{
	TWeakPtr<SLyudBookCatalog> WeakSelf = SharedThis(this);
	return SNew(SButton).ButtonStyle(&LyudUI::FlatButton()).ContentPadding(FMargin(10.f, 4.f))
		.OnClicked_Lambda([WeakSelf, Lang]
		{
			if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin())
			{
				S->LangFilter = Lang;
				S->Refill();
			}
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Font(LyudUI::Font(15, "Bold")).Text(Label)
			.ColorAndOpacity_Lambda([WeakSelf, Lang]
			{
				TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin();
				return FSlateColor(S && S->LangFilter == Lang ? LyudUI::Bright : LyudUI::Dim);
			})
		];
}

FText SLyudBookCatalog::StatusText() const
{
	const ULyudBooks* B = Books.Get();
	if (!B)
	{
		return FText::GetEmpty();
	}
	return LyudText::Get(TEXT("ui.books.on_loan"), { FStringFormatArg(B->Borrowed().Num()), FStringFormatArg(ULyudBooks::MaxBorrowed) });
}

void SLyudBookCatalog::Refill()
{
	Rows.Reset();
	ULyudBooks* B = Books.Get();
	if (B)
	{
		const FString Needle = Filter.TrimStartAndEnd();
		auto Match = [&Needle](const FLyudBookInfo& Bk)
		{
			if (Needle.IsEmpty())
			{
				return true;
			}
			for (const TPair<FString, FString>& T : Bk.Title)
			{
				if (T.Value.Contains(Needle))
				{
					return true;
				}
			}
			for (const TPair<FString, FString>& A : Bk.Author)
			{
				if (A.Value.Contains(Needle))
				{
					return true;
				}
			}
			return false;
		};
		// his own books first, then the shelves
		for (const FString& Key : B->Borrowed())
		{
			if (const FLyudBookInfo* Bk = B->Find(Key); Bk && Match(*Bk))
			{
				Rows.Add(MakeShared<FString>(Key));
			}
		}
		if (bAtDesk)
		{
			for (const FLyudBookInfo& Bk : B->Catalog())
			{
				if (!B->IsBorrowed(Bk.Key) && (LangFilter.IsEmpty() || Bk.Lang == LangFilter) && Match(Bk))
				{
					Rows.Add(MakeShared<FString>(Bk.Key));
				}
			}
		}
	}
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SLyudBookCatalog::MakeRow(TSharedPtr<FString> Key, const TSharedRef<STableViewBase>& Owner)
{
	ULyudBooks* B = Books.Get();
	const FLyudBookInfo* Bk = B && Key.IsValid() ? B->Find(*Key) : nullptr;
	if (!Bk)
	{
		return SNew(STableRow<TSharedPtr<FString>>, Owner)[SNullWidget::NullWidget];
	}
	const FString BookKey = *Key;
	const bool bMine = B->IsBorrowed(BookKey);
	TWeakPtr<SLyudBookCatalog> WeakSelf = SharedThis(this);
	auto ActionButton = [&](const TCHAR* Label, TFunction<void()> Do, bool bEnabled)
	{
		return SNew(SButton).ButtonStyle(&LyudUI::Button()).ContentPadding(FMargin(12.f, 4.f)).IsEnabled(bEnabled)
			.OnClicked_Lambda([Do] { Do(); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(LyudUI::Font(14)).ColorAndOpacity(LyudUI::Text).Text(LyudText::Get(Label))
			];
	};
	TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
	if (bMine)
	{
		Actions->AddSlot().AutoWidth().Padding(4, 0)[ActionButton(TEXT("ui.books.read"), [WeakSelf, BookKey]
		{
			if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin()) { S->OnRead.ExecuteIfBound(BookKey); }
		}, true)];
		if (bAtDesk)
		{
			Actions->AddSlot().AutoWidth().Padding(4, 0)[ActionButton(TEXT("ui.books.return"), [WeakSelf, BookKey]
			{
				if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin(); S && S->Books.IsValid()) { S->Books->Return(BookKey); S->Refill(); }
			}, true)];
		}
	}
	else
	{
		Actions->AddSlot().AutoWidth().Padding(4, 0)[ActionButton(TEXT("ui.books.take"), [WeakSelf, BookKey]
		{
			if (TSharedPtr<SLyudBookCatalog> S = WeakSelf.Pin(); S && S->Books.IsValid()) { S->Books->Borrow(BookKey); S->Refill(); }
		}, B->Borrowed().Num() < ULyudBooks::MaxBorrowed)];
	}
	const FString LangKey = FString::Printf(TEXT("ui.books.lang_short.%s"), *Bk->Lang);
	FString Sub = Bk->AuthorNow();
	if (Bk->Year != 0)
	{
		Sub += FString::Printf(TEXT(", %d"), FMath::Abs(Bk->Year));
	}
	const bool bOtherLang = Bk->Lang != LyudText::Language();
	const FText Title = FText::FromString(bOtherLang && Bk->TitleOwn() != Bk->TitleNow() ? Bk->TitleNow() + TEXT("  ·  ") + Bk->TitleOwn() : Bk->TitleNow());
	return SNew(STableRow<TSharedPtr<FString>>, Owner).Padding(FMargin(4.f, 3.f))
	[
		SNew(SBorder).BorderImage(LyudUI::CellBrush(bMine)).Padding(FMargin(12.f, 7.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
			[
				SNew(SBox).WidthOverride(40.f)
				[
					SNew(STextBlock).Font(LyudUI::Font(13, "Bold")).ColorAndOpacity(LyudUI::Dim).Text(LyudText::Get(LangKey))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(LyudUI::Font(16, "Bold")).ColorAndOpacity(LyudUI::Bright).Text(Title)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(LyudUI::Font(13)).ColorAndOpacity(LyudUI::Body).Text(FText::FromString(Sub))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
			[
				SNew(STextBlock).Font(LyudUI::Font(12)).ColorAndOpacity(LyudUI::Good)
				.Text(B->IsFinished(BookKey) ? LyudText::Get(TEXT("ui.books.finished")) : FText::GetEmpty())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Actions]
		]
	];
}
