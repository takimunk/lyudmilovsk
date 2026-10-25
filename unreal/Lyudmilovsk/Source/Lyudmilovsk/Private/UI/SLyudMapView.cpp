#include "UI/SLyudMapView.h"

#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"

namespace
{
	const float MaxZoom = 4.f;   // pixels per metre

	void Circle(TArray<FVector2D>& Out, const FVector2D& C, float R, int32 N = 20)
	{
		Out.Reset();
		for (int32 I = 0; I <= N; ++I)
		{
			const float A = 2.f * PI * I / N;
			Out.Add(C + R * FVector2D(FMath::Cos(A), FMath::Sin(A)));
		}
	}

	FSlateFontInfo PlaceFont(FName Kind)
	{
		if (Kind == TEXT("town"))
		{
			return LyudUI::Font(20, "Bold");
		}
		if (Kind == TEXT("water") || Kind == TEXT("land"))
		{
			return LyudUI::Font(16, "Italic");
		}
		return LyudUI::Font(13);
	}

	FLinearColor PlaceColor(FName Kind)
	{
		if (Kind == TEXT("water"))
		{
			return FLinearColor(0.16f, 0.28f, 0.42f);
		}
		if (Kind == TEXT("land"))
		{
			return FLinearColor(0.3f, 0.36f, 0.22f);
		}
		return FLinearColor(0.16f, 0.11f, 0.07f);
	}
}

void SLyudMapView::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	SetClipping(EWidgetClipping::ClipToBounds);
	MapBrush = MakeShared<FSlateBrush>();
	if (ULyudGameScreen* O = Owner.Get())
	{
		if (UTexture2D* Tex = O->MapTexture())
		{
			MapBrush->SetResourceObject(Tex);
			MapBrush->ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
		}
	}
	CenterOnPlayer();
}

SLyudMapView::~SLyudMapView() = default;

float SLyudMapView::MinZoom(const FVector2D& Size) const
{
	const FLyudMapFrame& F = ULyudQuests::Frame();
	return FMath::Min(Size.X / (F.XMax - F.XMin), Size.Y / (F.YMax - F.YMin));
}

void SLyudMapView::CenterOn(const FVector2D& At, float InZoom)
{
	Center = At;
	if (InZoom > 0.f)
	{
		Zoom = InZoom;
	}
}

void SLyudMapView::CenterOnPlayer()
{
	FVector2D At, Facing;
	if (Owner.IsValid() && Owner->PlayerOnMap(At, Facing))
	{
		CenterOn(At, 1.f);
	}
	else
	{
		CenterOn(FVector2D(-150.0, 450.0), 0.4f);
	}
}

FVector2D SLyudMapView::ToScreen(const FVector2D& M, const FVector2D& Size) const
{
	// west (+X) to the left, north (+Y) up
	return FVector2D(Size.X * 0.5 + (Center.X - M.X) * Zoom, Size.Y * 0.5 + (Center.Y - M.Y) * Zoom);
}

FVector2D SLyudMapView::ToMap(const FVector2D& S, const FVector2D& Size) const
{
	return FVector2D(Center.X - (S.X - Size.X * 0.5) / Zoom, Center.Y - (S.Y - Size.Y * 0.5) / Zoom);
}

int32 SLyudMapView::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = G.GetLocalSize();
	const_cast<SLyudMapView*>(this)->LastSize = Size;
	const FLyudMapFrame& F = ULyudQuests::Frame();
	const ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	auto Box = [&G](const FVector2D& Pos, const FVector2D& Sz) { return G.ToPaintGeometry(FVector2f(Sz), FSlateLayoutTransform(FVector2f(Pos))); };

	// the sea beyond the edges, then the picture
	static const FSlateColorBrush Sea(FLinearColor(0.42f, 0.56f, 0.62f));
	FSlateDrawElement::MakeBox(Out, LayerId, Box(FVector2D::ZeroVector, Size), &Sea);
	const FVector2D TopLeft = ToScreen(FVector2D(F.XMax, F.YMax), Size);
	const FVector2D BottomRight = ToScreen(FVector2D(F.XMin, F.YMin), Size);
	if (MapBrush->GetResourceObject())
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Box(TopLeft, BottomRight - TopLeft), MapBrush.Get());
	}

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto Label = [&](const FString& S, const FVector2D& At, const FSlateFontInfo& Font, const FLinearColor& Color, int32 Layer)
	{
		const FVector2D TS = Measure->Measure(S, Font);
		const FVector2D P = At - FVector2D(TS.X * 0.5, TS.Y * 0.5);
		// a soft paper halo so the name reads over forest and water
		for (const FVector2D& O : { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) })
		{
			FSlateDrawElement::MakeText(Out, Layer, Box(P + O * 1.2, TS), S, Font, ESlateDrawEffect::None, FLinearColor(0.93f, 0.89f, 0.78f, 0.75f));
		}
		FSlateDrawElement::MakeText(Out, Layer + 1, Box(P, TS), S, Font, ESlateDrawEffect::None, Color);
	};

	// places
	TArray<FVector2D> Pts;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		const FVector2D S = ToScreen(P.At, Size);
		if (S.X < -200 || S.Y < -50 || S.X > Size.X + 200 || S.Y > Size.Y + 50)
		{
			continue;
		}
		const bool bFound = Quests && Quests->IsFound(P.Id);
		const bool bArea = P.Kind == TEXT("town") || P.Kind == TEXT("water") || P.Kind == TEXT("land");
		if (!bFound)
		{
			if (!bArea)
			{
				Circle(Pts, S, 3.f, 10);
				FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.2f, 0.15f, 0.1f, 0.6f), true, 1.5f);
			}
			continue;
		}
		FVector2D At = S;
		if (!bArea)
		{
			Circle(Pts, S, 3.5f, 10);
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.16f, 0.1f, 0.06f), true, 3.f);
			At.Y -= 14.f;
		}
		Label(ULyudQuests::PlaceName(P.Id).ToString(), At, PlaceFont(P.Kind), PlaceColor(P.Kind), LayerId + 3);
	}

	// the next steps of the active quests
	if (Quests)
	{
		const FName Shown = Quests->ShownQuest();
		const float Pulse = 0.5f + 0.5f * FMath::Sin(FPlatformTime::Seconds() * 4.0);
		for (const FLyudQuestProgress& Q : Quests->Progress)
		{
			for (const TPair<const FLyudQuestObjective*, int32>& O : Quests->CurrentObjectives(Q.Quest))
			{
				const FLyudMapPlace* Place = O.Key->Place.IsNone() || O.Value >= O.Key->Need ? nullptr : ULyudQuests::FindPlace(O.Key->Place);
				if (!Place)
				{
					continue;
				}
				const bool bMain = Q.Quest == Shown;
				const FVector2D S = ToScreen(Place->At, Size);
				const float R = bMain ? 9.f + 2.f * Pulse : 7.f;
				Pts = { S + FVector2D(0, -R), S + FVector2D(R, 0), S + FVector2D(0, R), S + FVector2D(-R, 0), S + FVector2D(0, -R) };
				FSlateDrawElement::MakeLines(Out, LayerId + 5, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.1f, 0.05f, 0.f), true, 5.f);
				FSlateDrawElement::MakeLines(Out, LayerId + 6, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None,
					bMain ? FLinearColor(1.f, 0.62f, 0.12f) : FLinearColor(0.85f, 0.7f, 0.4f), true, 2.5f);
			}
		}
	}

	// the player
	FVector2D At, Facing;
	if (Owner.IsValid() && Owner->PlayerOnMap(At, Facing))
	{
		const FVector2D S = ToScreen(At, Size);
		const FVector2D D(-Facing.X, -Facing.Y);   // to the screen: west left, north up
		const FVector2D Side(-D.Y, D.X);
		const FVector2D Tip = S + D * 13.f, L = S - D * 8.f + Side * 8.f, R = S - D * 8.f - Side * 8.f, Back = S - D * 4.f;
		Pts = { Tip, L, Back, R, Tip };
		FSlateDrawElement::MakeLines(Out, LayerId + 7, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f), true, 6.f);
		FSlateDrawElement::MakeLines(Out, LayerId + 8, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.95f, 0.15f, 0.08f), true, 3.f);
		Pts = { S - D * 3.f, Tip - D * 3.f };
		FSlateDrawElement::MakeLines(Out, LayerId + 8, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.95f, 0.15f, 0.08f), true, 4.f);
	}

	// scale bar: a round length 70..180 px long
	float Metres = 10.f;
	for (const float M : { 10.f, 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f })
	{
		Metres = M;
		if (M * Zoom >= 70.f)
		{
			break;
		}
	}
	const FVector2D Bar0(24.f, Size.Y - 28.f), Bar1 = Bar0 + FVector2D(Metres * Zoom, 0.f);
	Pts = { Bar0 + FVector2D(0, -6), Bar0, Bar1, Bar1 + FVector2D(0, -6) };
	FSlateDrawElement::MakeLines(Out, LayerId + 9, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.12f, 0.08f, 0.05f), false, 2.5f);
	Label(LyudText::Get(TEXT("ui.map.scale"), { FStringFormatArg(FMath::RoundToInt(Metres)) }).ToString(), (Bar0 + Bar1) * 0.5 + FVector2D(0, -14), LyudUI::Font(12),
		FLinearColor(0.12f, 0.08f, 0.05f), LayerId + 9);

	// north
	const FVector2D N(Size.X - 34.f, 40.f);
	Pts = { N + FVector2D(0, 14), N + FVector2D(0, -14), N + FVector2D(-6, -6), N + FVector2D(0, -14), N + FVector2D(6, -6) };
	FSlateDrawElement::MakeLines(Out, LayerId + 9, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, FLinearColor(0.12f, 0.08f, 0.05f), false, 2.5f);
	Label(LyudText::Get(TEXT("ui.map.north")).ToString(), N + FVector2D(0, 26), LyudUI::Font(13, "Bold"), FLinearColor(0.12f, 0.08f, 0.05f), LayerId + 9);
	return LayerId + 11;
}

FReply SLyudMapView::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D Cursor = MyGeometry.AbsoluteToLocal(E.GetScreenSpacePosition());
	const FVector2D Before = ToMap(Cursor, Size);
	Zoom = FMath::Clamp(Zoom * FMath::Pow(1.25f, E.GetWheelDelta()), MinZoom(Size), MaxZoom);
	// keep the point under the cursor where it was
	Center += Before - ToMap(Cursor, Size);
	return FReply::Handled();
}

FReply SLyudMapView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (E.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Unhandled();
}

FReply SLyudMapView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (bDragging && E.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SLyudMapView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	const FVector2D Delta = E.GetCursorDelta() / MyGeometry.Scale;
	Center += FVector2D(Delta.X, Delta.Y) / Zoom;
	const FLyudMapFrame& F = ULyudQuests::Frame();
	Center.X = FMath::Clamp(Center.X, F.XMin, F.XMax);
	Center.Y = FMath::Clamp(Center.Y, F.YMin, F.YMax);
	return FReply::Handled();
}
