#include "UI/LyudDiaryStyle.h"

#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

namespace LyudDiary
{
	namespace
	{
		UTexture2D* LoadTex(const TCHAR* Path)
		{
			return LoadObject<UTexture2D>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		UFontFace* LoadFace(const TCHAR* Name)
		{
			return LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("/Game/UI/Diary/Fonts/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		struct FHandFonts
		{
			TSharedPtr<const FCompositeFont> Font;
			TArray<TStrongObjectPtr<UFontFace>> Faces;
			bool bTried = false;
		};

		/** One composite font per script family: Caveat for Latin and Cyrillic with Yomogi for kana and kanji; Yomogi for all of Japanese. */
		const FHandFonts& Fonts(bool bJapanese)
		{
			static FHandFonts Latin, Japanese;
			FHandFonts& F = bJapanese ? Japanese : Latin;
			if (F.bTried)
			{
				return F;
			}
			F.bTried = true;
			UFontFace* Regular = LoadFace(TEXT("FF_Caveat_Regular"));
			UFontFace* Bold = LoadFace(TEXT("FF_Caveat_Bold"));
			UFontFace* Yomogi = LoadFace(TEXT("FF_Yomogi_Regular"));
			if (!Regular || !Yomogi)
			{
				UE_LOG(LogTemp, Warning, TEXT("Diary: the handwriting fonts are missing (unreal/scripts/import_diary_assets.py); the default font stands in"));
				return F;
			}
			for (UFontFace* Face : { Regular, Bold, Yomogi })
			{
				if (Face)
				{
					F.Faces.Emplace(Face);
				}
			}
			TSharedRef<FCompositeFont> C = MakeShared<FCompositeFont>();
			auto Add = [](FTypeface& T, FName Name, UFontFace* Face)
			{
				FTypefaceEntry& E = T.Fonts.AddDefaulted_GetRef();
				E.Name = Name;
				E.Font = FFontData(Face);
			};
			if (bJapanese)
			{
				Add(C->DefaultTypeface, TEXT("Regular"), Yomogi);
				Add(C->DefaultTypeface, TEXT("Bold"), Yomogi);
			}
			else
			{
				Add(C->DefaultTypeface, TEXT("Regular"), Regular);
				Add(C->DefaultTypeface, TEXT("Bold"), Bold ? Bold : Regular);
				// a Japanese word in a Russian line (a name) still reads as handwriting; Caveat has no arrows, Yomogi does
				FCompositeSubFont& Sub = C->SubTypefaces.AddDefaulted_GetRef();
				Sub.CharacterRanges.Add(FInt32Range(0x2190, 0x21FF + 1));
				Sub.CharacterRanges.Add(FInt32Range(0x3000, 0x30FF + 1));
				Sub.CharacterRanges.Add(FInt32Range(0x4E00, 0x9FFF + 1));
				Sub.CharacterRanges.Add(FInt32Range(0xFF00, 0xFFEF + 1));
				Sub.ScalingFactor = 0.85f;
				Add(Sub.Typeface, TEXT("Regular"), Yomogi);
			}
			F.Font = C;
			return F;
		}

		struct FBrushes
		{
			FSlateBrush PaperL, PaperR, Sticky;
			TStrongObjectPtr<UTexture2D> TexL, TexR, Map;
			bool bTried = false;
			bool bMapTried = false;
		};

		FBrushes& Brushes()
		{
			static FBrushes B;
			if (!B.bTried)
			{
				B.bTried = true;
				auto Make = [](FSlateBrush& Brush, TStrongObjectPtr<UTexture2D>& Keep, const TCHAR* Path, const FLinearColor& Tint)
				{
					if (UTexture2D* T = LoadTex(Path))
					{
						Keep.Reset(T);
						Brush.SetResourceObject(T);
						Brush.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
						Brush.DrawAs = ESlateBrushDrawType::Image;
						Brush.TintColor = Tint;
					}
					else
					{
						Brush.DrawAs = ESlateBrushDrawType::Box;
						Brush.TintColor = Paper * Tint;
					}
				};
				Make(B.PaperL, B.TexL, TEXT("/Game/UI/Diary/T_PaperLeft.T_PaperLeft"), FLinearColor::White);
				Make(B.PaperR, B.TexR, TEXT("/Game/UI/Diary/T_PaperRight.T_PaperRight"), FLinearColor::White);
				// the sticky note: plain yellow (the page's paper would bring its spine shadow along)
				B.Sticky.DrawAs = ESlateBrushDrawType::Image;
				B.Sticky.TintColor = Sticky;
			}
			return B;
		}
	}

	FLinearColor TabColor(ESection S)
	{
		switch (S)
		{
		case ESection::Things: return FLinearColor(0.84f, 0.55f, 0.32f);    // orange card
		case ESection::Quests: return FLinearColor(0.42f, 0.62f, 0.78f);    // blue
		case ESection::Diary: return FLinearColor(0.58f, 0.74f, 0.46f);     // green
		case ESection::Birds: return FLinearColor(0.72f, 0.6f, 0.8f);      // lilac
		case ESection::Map: return FLinearColor(0.86f, 0.76f, 0.38f);       // yellow
		default: return FLinearColor::Gray;
		}
	}

	FSlateFontInfo Hand(float Size, bool bBold)
	{
		const bool bJa = LyudText::Language() == TEXT("ja");
		const FHandFonts& F = Fonts(bJa);
		if (!F.Font)
		{
			return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", FMath::RoundToInt(Size * 0.75f));
		}
		// Caveat is small on its body; Yomogi's kanji fill the em
		return FSlateFontInfo(F.Font, Size * (bJa ? 0.8f : 1.f), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}

	const FSlateBrush* PaperBrush(bool bRight)
	{
		FBrushes& B = Brushes();
		return bRight ? &B.PaperR : &B.PaperL;
	}

	const FSlateBrush* StickyBrush()
	{
		return &Brushes().Sticky;
	}

	UTexture2D* MapInk()
	{
		FBrushes& B = Brushes();
		if (!B.bMapTried)
		{
			B.bMapTried = true;
			B.Map.Reset(LoadTex(TEXT("/Game/UI/Diary/T_MapInk.T_MapInk")));
		}
		return B.Map.Get();
	}

	const FSlateBrush* SketchBrush(FName Item)
	{
		struct FSketch { TSharedPtr<FSlateBrush> Brush; TStrongObjectPtr<UTexture2D> Tex; };
		static TMap<FName, FSketch> Cache;
		static FString CacheLang;
		if (CacheLang != LyudText::Language())
		{
			Cache.Reset();
			CacheLang = LyudText::Language();
		}
		if (FSketch* S = Cache.Find(Item))
		{
			return S->Brush.Get();
		}
		FSketch& S = Cache.Add(Item);
		if (UTexture2D* T = LyudText::LoadTexture(TEXT("Inventory"), FString(TEXT("T_Sketch_")) + Item.ToString()))
		{
			S.Tex.Reset(T);
			S.Brush = MakeShared<FSlateBrush>();
			S.Brush->SetResourceObject(T);
			S.Brush->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		}
		return S.Brush.Get();
	}

	float Hash(uint32 Seed, int32 I)
	{
		uint32 N = Seed * 374761393u + uint32(I) * 668265263u;
		N = (N ^ (N >> 13)) * 1274126177u;
		N ^= N >> 16;
		return (N & 0xFFFF) / 65535.f;
	}

	void DrawStroke(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const TArray<FVector2D>& Points, const FLinearColor& Color,
		float Thickness, uint32 Seed, float Wobble)
	{
		if (Points.Num() < 2)
		{
			return;
		}
		// subdivide every ~12 units and push each point off the line by a smooth, seeded amount; the pen presses harder
		// in the middle of a stroke than at its ends
		TArray<FVector2D> P;
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			const FVector2D A = Points[I], B = Points[I + 1];
			const int32 N = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 12.f));
			for (int32 K = 0; K < N; ++K)
			{
				P.Add(FMath::Lerp(A, B, float(K) / N));
			}
		}
		P.Add(Points.Last());
		const float Phase = Hash(Seed, 0) * 6.28f;
		for (int32 I = 0; I < P.Num(); ++I)
		{
			const FVector2D Prev = P[FMath::Max(I - 1, 0)], Next = P[FMath::Min(I + 1, P.Num() - 1)];
			FVector2D T = (Next - Prev).GetSafeNormal();
			const FVector2D N(-T.Y, T.X);
			const float S = FMath::Sin(I * 0.45f + Phase) * 0.6f + (Hash(Seed, I + 1) - 0.5f) * 0.8f;
			P[I] += N * S * Wobble;
		}
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, Thickness);
	}

	void DrawMark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, EMark Kind, const FVector2D& Pos, const FVector2D& Size,
		const FLinearColor& Color, float Thickness, uint32 Seed)
	{
		const float X0 = Pos.X, Y0 = Pos.Y, X1 = Pos.X + Size.X, Y1 = Pos.Y + Size.Y;
		auto J = [Seed](int32 I, float Amp) { return (Hash(Seed, 100 + I) - 0.5f) * 2.f * Amp; };
		switch (Kind)
		{
		case EMark::Underline:
		case EMark::DoubleUnderline:
		{
			const float Y = Y1 + 2.f;
			DrawStroke(Out, Layer, G, { FVector2D(X0 - 4 + J(1, 4), Y + J(2, 2)), FVector2D(X1 + 6 + J(3, 6), Y + 1.5f + J(4, 3)) }, Color, Thickness, Seed);
			if (Kind == EMark::DoubleUnderline)
			{
				DrawStroke(Out, Layer, G, { FVector2D(X0 + 8 + J(5, 6), Y + 7 + J(6, 2)), FVector2D(X1 - 4 + J(7, 8), Y + 6 + J(8, 3)) }, Color, Thickness * 0.8f, Seed + 7);
			}
			break;
		}
		case EMark::Strike:
		{
			const float Y = (Y0 + Y1) * 0.5f + 2.f;
			DrawStroke(Out, Layer, G, { FVector2D(X0 - 6, Y + J(1, 3)), FVector2D(X1 + 6, Y - 2 + J(2, 3)) }, Color, Thickness, Seed, 0.8f);
			break;
		}
		case EMark::Circle:
		{
			// an ellipse round the box drawn in one go, a little more than once round, the end overshooting the start
			const FVector2D C((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f);
			const FVector2D R(Size.X * 0.5f + 14.f, Size.Y * 0.5f + 10.f);
			TArray<FVector2D> P;
			const float A0 = -2.2f + J(1, 0.4f);
			const int32 N = 40;
			for (int32 I = 0; I <= N; ++I)
			{
				const float T = float(I) / N;
				const float A = A0 + T * (6.2832f + 0.5f);
				const float Grow = 1.f + 0.06f * T + J(2, 0.03f);
				P.Add(C + FVector2D(FMath::Cos(A) * R.X * Grow, FMath::Sin(A) * R.Y * Grow));
			}
			DrawStroke(Out, Layer, G, P, Color, Thickness, Seed, 1.0f);
			break;
		}
		case EMark::Box:
		{
			// four strokes, each starting a little past the last one's end
			const FVector2D C[4] = { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
			for (int32 I = 0; I < 4; ++I)
			{
				const FVector2D A = C[I], B = C[(I + 1) % 4];
				const FVector2D D = (B - A).GetSafeNormal();
				DrawStroke(Out, Layer, G, { A - D * (3.f + J(I, 2.f)) + FVector2D(J(I + 10, 1.5f), J(I + 20, 1.5f)), B + D * (2.f + J(I + 30, 2.f)) }, Color, Thickness, Seed + I, 0.9f);
			}
			break;
		}
		case EMark::Highlight:
		{
			// two broad passes of a highlighter, a bit off the line
			for (int32 K = 0; K < 2; ++K)
			{
				const float Y = Y0 + Size.Y * (0.38f + 0.3f * K) + J(K, 2.f);
				FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(),
					{ FVector2D(X0 - 6 + J(K + 4, 4), Y), FVector2D(X1 + 6 + J(K + 6, 4), Y + J(K + 8, 3)) }, ESlateDrawEffect::None, Color, true, Size.Y * 0.55f);
			}
			break;
		}
		case EMark::Tick:
		{
			DrawStroke(Out, Layer, G, { FVector2D(X0 + Size.X * 0.12f, Y0 + Size.Y * 0.52f), FVector2D(X0 + Size.X * 0.42f, Y1 - Size.Y * 0.08f),
				FVector2D(X1 + Size.X * 0.15f, Y0 - Size.Y * 0.18f) }, Color, Thickness, Seed, 0.6f);
			break;
		}
		case EMark::Cross:
		{
			DrawStroke(Out, Layer, G, { FVector2D(X0, Y0), FVector2D(X1, Y1) }, Color, Thickness, Seed, 0.6f);
			DrawStroke(Out, Layer, G, { FVector2D(X1, Y0 + J(1, 2)), FVector2D(X0 + J(2, 2), Y1) }, Color, Thickness, Seed + 3, 0.6f);
			break;
		}
		case EMark::Arrow:
		{
			const float Y = (Y0 + Y1) * 0.5f;
			DrawStroke(Out, Layer, G, { FVector2D(X0, Y + J(1, 2)), FVector2D(X1, Y) }, Color, Thickness, Seed, 0.8f);
			DrawStroke(Out, Layer, G, { FVector2D(X1 - Size.Y * 0.45f, Y0 + 2), FVector2D(X1, Y), FVector2D(X1 - Size.Y * 0.45f, Y1 - 2) }, Color, Thickness, Seed + 1, 0.5f);
			break;
		}
		case EMark::Bracket:
		{
			DrawStroke(Out, Layer, G, { FVector2D(X0 + 8, Y0), FVector2D(X0, Y0 + 6), FVector2D(X0, Y1 - 6), FVector2D(X0 + 8, Y1) }, Color, Thickness, Seed, 0.7f);
			break;
		}
		}
	}
}
