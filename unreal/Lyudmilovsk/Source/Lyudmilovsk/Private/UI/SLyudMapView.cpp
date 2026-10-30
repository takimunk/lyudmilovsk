#include "UI/SLyudMapView.h"

#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudDiaryStyle.h"
#include "UI/LyudUIStyle.h"
#include "Transport/LyudTransport.h"
#include "Birds/BirdSpecies.h"
#include "UI/LyudBirdGuide.h"

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

	FSlateFontInfo PlaceFont(FName Kind, bool bInk)
	{
		if (bInk)
		{
			return LyudDiary::Hand(Kind == TEXT("town") ? 40.f : Kind == TEXT("water") || Kind == TEXT("land") ? 34.f : 27.f, Kind == TEXT("town"));
		}
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

	FLinearColor PlaceColor(FName Kind, bool bInk)
	{
		if (bInk)
		{
			return Kind == TEXT("water") ? FLinearColor(0.2f, 0.34f, 0.56f) : Kind == TEXT("land") ? FLinearColor(0.3f, 0.36f, 0.24f) : LyudDiary::Ink;
		}
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
	bInk = InArgs._bInk;
	bGuide = InArgs._bGuide;
	SetClipping(EWidgetClipping::ClipToBounds);
	// the birds move: paint every frame
	ForceVolatile(true);
	MapBrush = MakeShared<FSlateBrush>();
	if (ULyudGameScreen* O = Owner.Get())
	{
		if (UTexture2D* Tex = bInk && LyudDiary::MapInk() ? LyudDiary::MapInk() : O->MapTexture())
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
	if (bGuide)
	{
		// the guide's page is held round the hero: the radius fills it at first, the wheel brings it closer
		SLyudMapView* Self = const_cast<SLyudMapView*>(this);
		FVector2D Here, Facing;
		if (Owner.IsValid() && Owner->PlayerOnMap(Here, Facing))
		{
			Self->Center = Here;
		}
		if (!bGuideZoomSet && Size.X > 1.0)
		{
			Self->Zoom = 0.47f * FMath::Min(Size.X, Size.Y) / ULyudBirdGuide::Radius();
			Self->bGuideZoomSet = true;
		}
	}
	const FLyudMapFrame& F = ULyudQuests::Frame();
	// the screen fades the diary in and out: everything drawn here goes with it
	const FLinearColor Fade = InWidgetStyle.GetColorAndOpacityTint();
	const ULyudQuests* Quests = ULyudQuests::Get(Owner.Get());
	auto Box = [&G](const FVector2D& Pos, const FVector2D& Sz) { return G.ToPaintGeometry(FVector2f(Sz), FSlateLayoutTransform(FVector2f(Pos))); };

	// the sea beyond the edges (in the diary: the page's paper), then the picture
	static const FSlateColorBrush Sea(FLinearColor(0.42f, 0.56f, 0.62f));
	if (!bInk)
	{
		FSlateDrawElement::MakeBox(Out, LayerId, Box(FVector2D::ZeroVector, Size), &Sea, ESlateDrawEffect::None, Fade * Sea.TintColor.GetSpecifiedColor());
	}
	const FLinearColor InkC = bInk ? LyudDiary::Ink : FLinearColor(0.12f, 0.08f, 0.05f);
	const FLinearColor Halo = bInk ? FLinearColor(0.95f, 0.93f, 0.87f, 0.85f) : FLinearColor(0.93f, 0.89f, 0.78f, 0.75f);
	const FVector2D TopLeft = ToScreen(FVector2D(F.XMax, F.YMax), Size);
	const FVector2D BottomRight = ToScreen(FVector2D(F.XMin, F.YMin), Size);
	if (MapBrush->GetResourceObject())
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Box(TopLeft, BottomRight - TopLeft), MapBrush.Get(), ESlateDrawEffect::None, Fade * MapBrush->TintColor.GetSpecifiedColor());
	}

	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto Label = [&](const FString& S, const FVector2D& At, const FSlateFontInfo& Font, const FLinearColor& Color, int32 Layer)
	{
		const FVector2D TS = Measure->Measure(S, Font);
		const FVector2D P = At - FVector2D(TS.X * 0.5, TS.Y * 0.5);
		// a soft paper halo so the name reads over forest and water
		for (const FVector2D& O : { FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1) })
		{
			FSlateDrawElement::MakeText(Out, Layer, Box(P + O * (bInk ? 2.0 : 1.2), TS), S, Font, ESlateDrawEffect::None, Fade * Halo);
		}
		FSlateDrawElement::MakeText(Out, Layer + 1, Box(P, TS), S, Font, ESlateDrawEffect::None, Fade * Color);
	};

	// places (the guide leaves out the names right by the hero: his arrow is what matters there)
	TArray<FVector2D> Pts;
	FVector2D HeroAt, HeroFacing;
	const bool bHideNearHero = bGuide && Owner.IsValid() && Owner->PlayerOnMap(HeroAt, HeroFacing);
	const FVector2D HeroS = bHideNearHero ? ToScreen(HeroAt, Size) : FVector2D::ZeroVector;
	for (const FLyudMapPlace& P : ULyudQuests::Places())
	{
		const FVector2D S = ToScreen(P.At, Size);
		if (S.X < -200 || S.Y < -50 || S.X > Size.X + 200 || S.Y > Size.Y + 50)
		{
			continue;
		}
		if (bHideNearHero && FVector2D::DistSquared(S, HeroS) < 70.0 * 70.0)
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
				FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.2f, 0.15f, 0.1f, 0.6f), true, 1.5f);
			}
			continue;
		}
		FVector2D At = S;
		if (!bArea)
		{
			Circle(Pts, S, 3.5f, 10);
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.16f, 0.1f, 0.06f), true, 3.f);
			At.Y -= 14.f;
		}
		if (bInk && !bArea)
		{
			At.Y -= 8.f;
		}
		Label(ULyudQuests::PlaceName(P.Id).ToString(), At, PlaceFont(P.Kind, bInk), PlaceColor(P.Kind, bInk), LayerId + 3);
	}

	// the cable car (ULyudTransportSubsystem): the lines in their colours, the stations, the cabins on the move
	if (const ULyudTransportSubsystem* Transport = ULyudTransportSubsystem::Get(Owner.Get()); Transport && Transport->IsRunning())
	{
		const TArray<FLyudTransitLine> Lines = Transport->GetLines();
		for (const FLyudTransitLine& L : Lines)
		{
			TArray<FVector2D> Way;
			for (const FVector2D& M : L.Path)
			{
				Way.Add(ToScreen(M, Size));
			}
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Way, ESlateDrawEffect::None, Fade * FLinearColor(0.1f, 0.06f, 0.03f, 0.7f), true, 5.f);
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Way, ESlateDrawEffect::None, Fade * L.Color, true, 3.f);
		}
		for (const FLyudTransitCabin& C : Transport->GetCabins())
		{
			if (C.State == TEXT("depot") || !Lines.IsValidIndex(C.Line))
			{
				continue;
			}
			Circle(Pts, ToScreen(C.MapAt, Size), 2.5f, 8);
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.1f, 0.06f, 0.03f), true, 3.f);
		}
		for (const FLyudTransitStation& St : Transport->GetStations())
		{
			const FVector2D S = ToScreen(St.MapAt, Size);
			Circle(Pts, S, 5.f, 14);
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.1f, 0.06f, 0.03f), true, 3.5f);
			if (Zoom > 0.25f)
			{
				Label(LyudText::Get(St.NameKey).ToString(), S + FVector2D(0.f, bInk ? 26.f : 16.f), bInk ? LyudDiary::Hand(24.f, true) : LyudUI::Font(12, "Bold"),
					bInk ? LyudDiary::Red : FLinearColor(0.35f, 0.16f, 0.04f), LayerId + 3);
			}
		}
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
				if (bInk)
				{
					// circled in red pen, twice round for the followed quest
					const float R = bMain ? 22.f : 16.f;
					const uint32 Seed = GetTypeHash(Q.Quest) | 1u;
					LyudDiary::DrawMark(Out, LayerId + 6, G, LyudDiary::EMark::Circle, S - FVector2D(R, R * 0.7f), FVector2D(2.f * R, 1.4f * R), Fade * LyudDiary::Red, bMain ? 3.2f : 2.4f, Seed);
					if (bMain)
					{
						LyudDiary::DrawMark(Out, LayerId + 6, G, LyudDiary::EMark::Circle, S - FVector2D(R + 4.f, R * 0.7f + 3.f), FVector2D(2.f * R + 8.f, 1.4f * R + 6.f),
							Fade * LyudDiary::Red * FLinearColor(1, 1, 1, 0.6f + 0.4f * Pulse), 2.f, Seed + 5);
					}
					continue;
				}
				const float R = bMain ? 9.f + 2.f * Pulse : 7.f;
				Pts = { S + FVector2D(0, -R), S + FVector2D(R, 0), S + FVector2D(0, R), S + FVector2D(-R, 0), S + FVector2D(0, -R) };
				FSlateDrawElement::MakeLines(Out, LayerId + 5, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.1f, 0.05f, 0.f), true, 5.f);
				FSlateDrawElement::MakeLines(Out, LayerId + 6, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None,
					Fade * (bMain ? FLinearColor(1.f, 0.62f, 0.12f) : FLinearColor(0.85f, 0.7f, 0.4f)), true, 2.5f);
			}
		}
	}

	if (ShowsBirds())
	{
		DrawBirds(Out, LayerId + 4, G, Size, Fade);
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
		if (bInk)
		{
			// a red pen arrow, drawn bigger on the page
			for (FVector2D& Pt : Pts)
			{
				Pt = S + (Pt - S) * 1.6;
			}
			LyudDiary::DrawStroke(Out, LayerId + 8, G, Pts, Fade * LyudDiary::Red, 3.4f, 4242, 0.5f);
		}
		else
		{
			FSlateDrawElement::MakeLines(Out, LayerId + 7, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.f, 0.f, 0.f, 0.8f), true, 6.f);
			FSlateDrawElement::MakeLines(Out, LayerId + 8, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.95f, 0.15f, 0.08f), true, 3.f);
		}
		if (!bInk)
		{
			Pts = { S - D * 3.f, Tip - D * 3.f };
			FSlateDrawElement::MakeLines(Out, LayerId + 8, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.95f, 0.15f, 0.08f), true, 4.f);
		}
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
	const FVector2D Bar0(bInk ? 40.f : 24.f, Size.Y - (bInk ? 120.f : 28.f)), Bar1 = Bar0 + FVector2D(Metres * Zoom, 0.f);
	Pts = { Bar0 + FVector2D(0, -6), Bar0, Bar1, Bar1 + FVector2D(0, -6) };
	if (bInk)
	{
		LyudDiary::DrawStroke(Out, LayerId + 9, G, Pts, Fade * InkC, 2.6f, 77, 0.5f);
	}
	else
	{
		FSlateDrawElement::MakeLines(Out, LayerId + 9, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * InkC, false, 2.5f);
	}
	Label(LyudText::Get(TEXT("ui.map.scale"), { FStringFormatArg(FMath::RoundToInt(Metres)) }).ToString(), (Bar0 + Bar1) * 0.5 + FVector2D(0, bInk ? -22 : -14),
		bInk ? LyudDiary::Hand(24.f) : LyudUI::Font(12), InkC, LayerId + 9);

	// north
	const float NS = bInk ? 2.f : 1.f;
	const FVector2D N(Size.X - 34.f * NS, 40.f * NS);
	Pts = { N + FVector2D(0, 14) * NS, N + FVector2D(0, -14) * NS, N + FVector2D(-6, -6) * NS, N + FVector2D(0, -14) * NS, N + FVector2D(6, -6) * NS };
	if (bInk)
	{
		LyudDiary::DrawStroke(Out, LayerId + 9, G, Pts, Fade * InkC, 2.6f, 78, 0.4f);
	}
	else
	{
		FSlateDrawElement::MakeLines(Out, LayerId + 9, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * InkC, false, 2.5f);
	}
	Label(LyudText::Get(TEXT("ui.map.north")).ToString(), N + FVector2D(0, 26) * NS, bInk ? LyudDiary::Hand(28.f, true) : LyudUI::Font(13, "Bold"), InkC, LayerId + 9);
	return LayerId + 11;
}

FReply SLyudMapView::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D Cursor = MyGeometry.AbsoluteToLocal(E.GetScreenSpacePosition());
	const FVector2D Before = ToMap(Cursor, Size);
	Zoom = FMath::Clamp(Zoom * FMath::Pow(1.25f, E.GetWheelDelta()), bGuide ? 0.3f * FMath::Min(Size.X, Size.Y) / ULyudBirdGuide::Radius() : MinZoom(Size), MaxZoom);
	// keep the point under the cursor where it was (the guide stays on the hero)
	if (!bGuide)
	{
		Center += Before - ToMap(Cursor, Size);
	}
	if (OnMoved)
	{
		OnMoved(Center, Zoom);
	}
	return FReply::Handled();
}

FReply SLyudMapView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& E)
{
	if (E.GetEffectingButton() == EKeys::LeftMouseButton && !bGuide)
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
	Mouse = MyGeometry.AbsoluteToLocal(E.GetScreenSpacePosition());
	if (!bDragging)
	{
		return FReply::Unhandled();
	}
	const FVector2D Delta = E.GetCursorDelta() / MyGeometry.Scale;
	Center += FVector2D(Delta.X, Delta.Y) / Zoom;
	const FLyudMapFrame& F = ULyudQuests::Frame();
	Center.X = FMath::Clamp(Center.X, F.XMin, F.XMax);
	Center.Y = FMath::Clamp(Center.Y, F.YMin, F.YMax);
	if (OnMoved)
	{
		OnMoved(Center, Zoom);
	}
	return FReply::Handled();
}

void SLyudMapView::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	Mouse = FVector2D(-1.0, -1.0);
}

bool SLyudMapView::ShowsBirds() const
{
	if (bGuide)
	{
		return true;
	}
	const ULyudBirdGuide* Guide = ULyudBirdGuide::Get(Owner.Get());
	return Guide && Guide->ShowsOnMap(Owner.Get());
}

namespace
{
	/** Fills everything outside a circle (the land the guide does not see) with a colour: a ring of quads out past the corners. */
	void FillOutside(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Size, const FVector2D& C, float R, const FLinearColor& Color)
	{
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
		const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
		const FColor Col = Color.ToFColor(true);
		const float Far = R + 2.f * Size.Size();
		constexpr int32 N = 72;
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Index;
		for (int32 K = 0; K < N; ++K)
		{
			const float A = 2.f * PI * K / N;
			const FVector2f Dir(FMath::Cos(A), FMath::Sin(A));
			const FVector2f Center(C);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Center + Dir * R, FVector2f(0.5f, 0.5f), Col));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Center + Dir * Far, FVector2f(0.5f, 0.5f), Col));
		}
		for (int32 K = 0; K < N; ++K)
		{
			const SlateIndex I0 = 2 * K, I1 = 2 * K + 1, J0 = 2 * ((K + 1) % N), J1 = J0 + 1;
			Index.Append({ I0, I1, J1, I0, J1, J0 });
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Index, nullptr, 0, 0);
	}

	/** Words for what a bird is doing, as the guide names it. */
	const TCHAR* DoingKey(const FLyudGuideBird& B)
	{
		return B.bFlying ? TEXT("ui.guide.flying") : B.bOnWater ? TEXT("ui.guide.swimming") : B.bAsleep ? TEXT("ui.guide.asleep") : TEXT("ui.guide.perched");
	}
}

void SLyudMapView::DrawBirds(FSlateWindowElementList& Out, int32 LayerId, const FGeometry& G, const FVector2D& Size, const FLinearColor& Fade) const
{
	FVector2D Hero;
	const TArray<FLyudGuideBird> Birds = ULyudBirdGuide::BirdsNearHero(Owner.Get(), &Hero);
	const FVector2D HeroS = ToScreen(Hero, Size);
	const float Rpx = ULyudBirdGuide::Radius() * Zoom;
	TArray<FVector2D> Pts;

	// the edge of what the guide sees
	if (bGuide)
	{
		FillOutside(Out, LayerId, G, Size, HeroS, Rpx, Fade * FLinearColor(0.93f, 0.91f, 0.84f, 0.86f));
		Circle(Pts, HeroS, Rpx, 96);
		FSlateDrawElement::MakeLines(Out, LayerId + 1, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.08f, 0.2f, 0.14f, 0.9f), true, 2.f);
	}
	else
	{
		// a dashed pencil ring
		constexpr int32 Dashes = 48;
		for (int32 K = 0; K < Dashes; ++K)
		{
			const float A0 = 2.f * PI * K / Dashes, A1 = A0 + PI / Dashes;
			Pts = { HeroS + Rpx * FVector2D(FMath::Cos(A0), FMath::Sin(A0)), HeroS + Rpx * FVector2D(FMath::Cos((A0 + A1) * 0.5f), FMath::Sin((A0 + A1) * 0.5f)),
				HeroS + Rpx * FVector2D(FMath::Cos(A1), FMath::Sin(A1)) };
			FSlateDrawElement::MakeLines(Out, LayerId + 1, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * LyudDiary::PencilFaint, true, 2.f);
		}
	}

	// the birds: a mark each, flying ones with a stroke along their way
	const FLyudGuideBird* Hover = nullptr;
	double HoverD = 16.0 * 16.0;
	for (const FLyudGuideBird& B : Birds)
	{
		const FVector2D S = ToScreen(B.At, Size);
		if (S.X < -20 || S.Y < -20 || S.X > Size.X + 20 || S.Y > Size.Y + 20)
		{
			continue;
		}
		const float R = FMath::Clamp(1.6f + B.LengthCm / 14.f, 2.2f, 6.f) * (bInk ? 1.5f : 1.f);
		FLinearColor C = B.Color;
		if (bInk)
		{
			// pencilled: the bird's colour taken down towards the graphite
			C = FMath::Lerp(C, FLinearColor(LyudDiary::Pencil), 0.35f);
		}
		C.A = B.bAsleep ? 0.55f : 1.f;
		if (B.bFlying && B.Velocity.SizeSquared() > 0.01)
		{
			// a little "v" pointed the way it flies, and a short wake behind it
			const FVector2D D = FVector2D(-B.Velocity.X, -B.Velocity.Y).GetSafeNormal();
			const FVector2D Side(-D.Y, D.X);
			const float W = R * 1.7f;
			Pts = { S - D * W * 0.4f + Side * W, S + D * W * 0.35f, S - D * W * 0.4f - Side * W };
			FSlateDrawElement::MakeLines(Out, LayerId + 3, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * C, true, bInk ? 2.6f : 2.2f);
			Pts = { S - D * W * 0.6f, S - D * (W * 0.6f + FMath::Clamp((float)B.Velocity.Size() * 0.8f, 4.f, 16.f)) };
			FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * C * FLinearColor(1, 1, 1, 0.45f), true, 1.5f);
		}
		else
		{
			// a filled dot (a ring as thick as its radius); on the water a ripple under it
			Circle(Pts, S, R * 0.5f, 10);
			FSlateDrawElement::MakeLines(Out, LayerId + 3, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * C, true, R);
			if (B.bOnWater)
			{
				Pts = { S + FVector2D(-R * 1.4f, R * 1.3f), S + FVector2D(-R * 0.5f, R * 0.9f), S + FVector2D(R * 0.5f, R * 1.3f), S + FVector2D(R * 1.4f, R * 0.9f) };
				FSlateDrawElement::MakeLines(Out, LayerId + 2, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.2f, 0.34f, 0.56f, 0.8f), true, 1.4f);
			}
		}
		if (bGuide && Mouse.X >= 0.0)
		{
			const double D2 = FVector2D::DistSquared(S, Mouse);
			if (D2 < HoverD)
			{
				HoverD = D2;
				Hover = &B;
			}
		}
	}

	// the guide names the bird under the mouse
	if (Hover)
	{
		const FBirdSpecies* Sp = UBirdSpeciesLibrary::Find(Hover->Species);
		const FString Name = (Sp ? Sp->DisplayName().ToString() : Hover->Species.ToString()) + TEXT(" · ") + LyudText::Get(DoingKey(*Hover)).ToString();
		const FVector2D S = ToScreen(Hover->At, Size);
		Circle(Pts, S, 10.f, 16);
		FSlateDrawElement::MakeLines(Out, LayerId + 5, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Fade * FLinearColor(0.08f, 0.2f, 0.14f), true, 1.5f);
		const FSlateFontInfo Font = LyudUI::Font(14, "Bold");
		const FVector2D TS = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Name, Font);
		const FVector2D P(FMath::Clamp(S.X - TS.X * 0.5, 4.0, Size.X - TS.X - 4.0), S.Y - 18.0 - TS.Y);
		static const FSlateColorBrush Tag(FLinearColor::White);
		FSlateDrawElement::MakeBox(Out, LayerId + 5, G.ToPaintGeometry(FVector2f(TS + FVector2D(10, 4)), FSlateLayoutTransform(FVector2f(P - FVector2D(5, 2)))), &Tag,
			ESlateDrawEffect::None, Fade * FLinearColor(0.97f, 0.95f, 0.88f, 0.92f));
		FSlateDrawElement::MakeText(Out, LayerId + 6, G.ToPaintGeometry(FVector2f(TS), FSlateLayoutTransform(FVector2f(P))), Name, Font, ESlateDrawEffect::None,
			Fade * FLinearColor(0.08f, 0.2f, 0.14f));
	}
}
