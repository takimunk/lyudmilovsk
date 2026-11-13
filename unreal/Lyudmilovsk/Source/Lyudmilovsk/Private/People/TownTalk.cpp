#include "People/TownTalk.h"

#include "TownBarks.h"
#include "PeopleSim.h"
#include "People/PeopleLife.h"
#include "HeroCharacter.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"

#include "Brushes/SlateColorBrush.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/SLeafWidget.h"

#include <string>
#include <vector>

static TAutoConsoleVariable<int32> CVarTownTalk(TEXT("lyud.Barks"), 1,
	TEXT("The townsfolk's own lines over their heads (ULyudTownTalkSubsystem, docs/town_talk.md): 1 on, 0 off."));
static TAutoConsoleVariable<int32> CVarTownTalkResidents(TEXT("lyud.Barks.Residents"), 0,
	TEXT("1: the story's people (the club's, the Academgorodok's residents) say the town's lines too; 0: only the townsfolk."));
static TAutoConsoleVariable<float> CVarTownTalkEvery(TEXT("lyud.Barks.Every"), 9.f,
	TEXT("Seconds between lines said on one's own near the player, on average (an exchange counts as one)."));
static TAutoConsoleVariable<int32> CVarTownTalkLog(TEXT("lyud.Barks.Log"), 0,
	TEXT("1: log every line put up over a head (BARKTEST ...), for the offscreen tests."));

/** The plain C++ side: the book of lines and the scratch the scans fill (no allocations once warm). */
struct FTownTalkState
{
	LyudBarks::FBook Book;
	LyudBarks::FNow Now;
	LyudBarks::FWho Who;
	LyudBarks::FWho Other;
	std::vector<int> Near;
	/** Per set: how many times it was said this session (Report). */
	std::vector<int> Said;
};

/** What the overlay draws, filled by the subsystem: the rows when a line goes up, where and how strong every frame. */
struct FTownTalkView
{
	static constexpr int32 MaxRows = 4;
	struct FItem
	{
		bool bShow = false;
		FVector2D Pixel = FVector2D::ZeroVector;    // viewport pixels of the point over the head
		float Alpha = 0.f;
		FText Name;
		FText Rows[MaxRows];
		int32 NumRows = 0;
	};
	FVector2D ViewportSize = FVector2D(1, 1);
	FItem Items[4];
};

namespace TownTalkLocal
{
	const FLinearColor BubbleInk(0.97f, 0.95f, 0.91f);
	const FLinearColor BubbleNameInk(1.f, 0.78f, 0.45f);
	const FLinearColor BubbleShade(0.02f, 0.02f, 0.025f, 0.74f);
	/** Rows are wrapped to this width (Slate units at the text's size). */
	constexpr float WrapWidth = 290.f;
	constexpr float WrapWidthCJK = 270.f;

	const FSlateBrush* BubbleBrush()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}
	FSlateFontInfo BubbleFont() { return FCoreStyle::GetDefaultFontStyle("Regular", 13); }
	FSlateFontInfo BubbleNameFont() { return FCoreStyle::GetDefaultFontStyle("Bold", 10); }

	FString ToFString(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	/** Splits a line into rows that fit the bubble: by words, or for Japanese by characters, never starting a row with a
	 *  closing mark (kinsoku). Done once when the line goes up. */
	void Wrap(const FString& Text, bool bCJK, TArray<FText>& Out)
	{
		Out.Reset();
		const FSlateFontInfo Font = BubbleFont();
		TSharedPtr<FSlateFontMeasure> Measure;
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		}
		auto Width = [&Measure, &Font, bCJK](const FString& S) -> float
		{
			return Measure.IsValid() ? (float)Measure->Measure(S, Font).X : (float)S.Len() * (bCJK ? 13.f : 7.5f);
		};
		const float MaxW = bCJK ? WrapWidthCJK : WrapWidth;
		FString Row;
		if (!bCJK)
		{
			TArray<FString> Words;
			Text.ParseIntoArray(Words, TEXT(" "), true);
			for (const FString& W : Words)
			{
				const FString Try = Row.IsEmpty() ? W : Row + TEXT(" ") + W;
				if (!Row.IsEmpty() && Width(Try) > MaxW)
				{
					Out.Add(FText::FromString(Row));
					Row = W;
				}
				else
				{
					Row = Try;
				}
			}
		}
		else
		{
			// 、。，．・：；？！ー…」』）〕］｝〉》 and the small kana
			static const TCHAR* const NoStart = TEXT("\u3001\u3002\uFF0C\uFF0E\u30FB\uFF1A\uFF1B\uFF1F\uFF01\u30FC\u2026\u300D\u300F\uFF09\u3015\uFF3D\uFF5D\u3009\u300B\u3041\u3043\u3045\u3047\u3049\u3063\u3083\u3085\u3087\u308E\u30A1\u30A3\u30A5\u30A7\u30A9\u30C3\u30E3\u30E5\u30E7\u30EE\u30F5\u30F6\uFF5E");
			for (int32 I = 0; I < Text.Len(); ++I)
			{
				const TCHAR C = Text[I];
				FString Try = Row;
				Try.AppendChar(C);
				if (!Row.IsEmpty() && Width(Try) > MaxW && FCString::Strchr(NoStart, C) == nullptr)
				{
					Out.Add(FText::FromString(Row));
					Row.Reset();
				}
				Row.AppendChar(C);
			}
		}
		if (!Row.IsEmpty())
		{
			Out.Add(FText::FromString(Row));
		}
		if (Out.Num() > FTownTalkView::MaxRows)
		{
			Out.SetNum(FTownTalkView::MaxRows);
			Out.Last() = FText::FromString(Out.Last().ToString() + TEXT("\u2026"));
		}
	}

	/** The lines over the heads, drawn where the subsystem projected them. */
	class STownTalkBubbles : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(STownTalkBubbles) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedRef<FTownTalkView> InView)
		{
			Shown = InView;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
			int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (!Shown.IsValid() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
			{
				return Layer;
			}
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo Font = BubbleFont();
			const FSlateFontInfo NameFont = BubbleNameFont();
			const FVector2D Size = Geometry.GetLocalSize();
			const FVector2D Scale = Size / Shown->ViewportSize;
			const FVector2D Pad(11.0, 6.0);
			for (const FTownTalkView::FItem& It : Shown->Items)
			{
				if (!It.bShow || It.Alpha <= 0.01f)
				{
					continue;
				}
				FVector2D Box(0.0, 0.0);
				FVector2D NameSize(0.0, 0.0);
				if (!It.Name.IsEmpty())
				{
					NameSize = Measure->Measure(It.Name, NameFont);
					Box.X = NameSize.X;
					Box.Y = NameSize.Y + 1.0;
				}
				FVector2D RowSize[FTownTalkView::MaxRows];
				for (int32 R = 0; R < It.NumRows; ++R)
				{
					RowSize[R] = Measure->Measure(It.Rows[R], Font);
					Box.X = FMath::Max(Box.X, RowSize[R].X);
					Box.Y += RowSize[R].Y;
				}
				const FVector2D Full = Box + Pad * 2.0;
				const FVector2D Anchor = It.Pixel * Scale;
				// above the head, kept on the screen
				FVector2D TopLeft(Anchor.X - Full.X * 0.5, Anchor.Y - Full.Y - 9.0);
				TopLeft.X = FMath::Clamp(TopLeft.X, 8.0, FMath::Max(8.0, Size.X - Full.X - 8.0));
				TopLeft.Y = FMath::Clamp(TopLeft.Y, 8.0, FMath::Max(8.0, Size.Y - Full.Y - 8.0));
				const FLinearColor Fade(1.f, 1.f, 1.f, It.Alpha);
				FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Full, FSlateLayoutTransform(TopLeft)),
					BubbleBrush(), ESlateDrawEffect::None, BubbleShade * Fade);
				// a small tail under it, toward the speaker
				const double TailX = FMath::Clamp(Anchor.X, TopLeft.X + 10.0, TopLeft.X + Full.X - 10.0);
				FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(8.0, 7.0), FSlateLayoutTransform(FVector2D(TailX - 4.0, TopLeft.Y + Full.Y))),
					BubbleBrush(), ESlateDrawEffect::None, BubbleShade * Fade);
				double Y = TopLeft.Y + Pad.Y;
				if (!It.Name.IsEmpty())
				{
					FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(NameSize, FSlateLayoutTransform(FVector2D(TopLeft.X + Pad.X, Y))),
						It.Name, NameFont, ESlateDrawEffect::None, BubbleNameInk * Fade);
					Y += NameSize.Y + 1.0;
				}
				for (int32 R = 0; R < It.NumRows; ++R)
				{
					FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(RowSize[R], FSlateLayoutTransform(FVector2D(TopLeft.X + Pad.X, Y))),
						It.Rows[R], Font, ESlateDrawEffect::None, BubbleInk * Fade);
					Y += RowSize[R].Y;
				}
			}
			return Layer + 1;
		}

	private:
		TSharedPtr<FTownTalkView> Shown;
	};

	// --- other threads' code, by name (no link to it: it may not be in the game yet) ---------------------------------

	/** The holidays' ULyudFestivalSubsystem when it is in the game. */
	UObject* FestivalSubsystemOf(UWorld* World)
	{
		static TWeakObjectPtr<UClass> Class;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			Class = FindFirstObject<UClass>(TEXT("LyudFestivalSubsystem"), EFindFirstObjectOptions::NativeFirst);
		}
		UClass* C = Class.Get();
		if (!World || !C || !C->IsChildOf(UWorldSubsystem::StaticClass()))
		{
			return nullptr;
		}
		return World->GetSubsystemBase(TSubclassOf<UWorldSubsystem>(C));
	}

	/** Calls a UFUNCTION of no parameters that returns an FName (ActiveFestival, ActivePhase). */
	FName CallName(UObject* Obj, FName Function)
	{
		UFunction* F = Obj ? Obj->FindFunction(Function) : nullptr;
		FNameProperty* Ret = F ? CastField<FNameProperty>(F->GetReturnProperty()) : nullptr;
		if (!Ret || F->NumParms != 1)
		{
			return NAME_None;
		}
		uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Parms, F->ParmsSize);
		Obj->ProcessEvent(F, Parms);
		return Ret->GetPropertyValue_InContainer(Parms);
	}

	/** The restaurant (ALyudRestaurant, review/restaurant) in the level. */
	AActor* FindRestaurant(UWorld* World)
	{
		static TWeakObjectPtr<UClass> Class;
		static bool bLooked = false;
		if (!bLooked)
		{
			bLooked = true;
			Class = FindFirstObject<UClass>(TEXT("LyudRestaurant"), EFindFirstObjectOptions::NativeFirst);
		}
		UClass* C = Class.Get();
		if (!World || !C || !C->IsChildOf(AActor::StaticClass()))
		{
			return nullptr;
		}
		TActorIterator<AActor> It(World, C);
		return It ? *It : nullptr;
	}

	/** What the restaurant shows now, read off its properties by name: its waiter's and cook's bodies (while drawn), the
	 *  dishes on the player's table and on the waiter's tray, the bill's folder. */
	struct FRestNow
	{
		AActor* Waiter = nullptr;
		AActor* Cook = nullptr;
		AActor* Bill = nullptr;
		int32 HeroDishes = 0;
		int32 OnTray = 0;
	};

	AActor* ShownActorProperty(AActor* Rest, const FName& Name)
	{
		const FObjectPropertyBase* P = CastField<FObjectPropertyBase>(Rest->GetClass()->FindPropertyByName(Name));
		AActor* A = P ? Cast<AActor>(P->GetObjectPropertyValue_InContainer(Rest)) : nullptr;
		return IsValid(A) && !A->IsHidden() ? A : nullptr;
	}

	int32 ArrayPropertyNum(AActor* Rest, const FName& Name)
	{
		const FArrayProperty* P = CastField<FArrayProperty>(Rest->GetClass()->FindPropertyByName(Name));
		if (!P)
		{
			return 0;
		}
		FScriptArrayHelper_InContainer Arr(P, Rest);
		return Arr.Num();
	}

	FRestNow ReadRestaurant(AActor* Rest)
	{
		static const FName WaiterName(TEXT("WaiterBody"));
		static const FName CookName(TEXT("CookBody"));
		static const FName BillName(TEXT("BillFolder"));
		static const FName DishesName(TEXT("HeroDishes"));
		static const FName TrayName(TEXT("OnTray"));
		FRestNow R;
		R.Waiter = ShownActorProperty(Rest, WaiterName);
		R.Cook = ShownActorProperty(Rest, CookName);
		R.Bill = ShownActorProperty(Rest, BillName);
		R.HeroDishes = ArrayPropertyNum(Rest, DishesName);
		R.OnTray = ArrayPropertyNum(Rest, TrayName);
		return R;
	}

	FVector PersonAt(const LyudPeople::FPerson& P) { return FVector(P.Pos.X, P.Pos.Y, P.Pos.Z) * 100.0; }
}

// --- the subsystem -------------------------------------------------------------------------------------------------

ULyudTownTalkSubsystem::ULyudTownTalkSubsystem() = default;
ULyudTownTalkSubsystem::~ULyudTownTalkSubsystem() = default;

ULyudTownTalkSubsystem* ULyudTownTalkSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudTownTalkSubsystem>() : nullptr;
}

bool ULyudTownTalkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudTownTalkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudTownTalkSubsystem, STATGROUP_Tickables);
}

void ULyudTownTalkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Start(InWorld);
}

void ULyudTownTalkSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

void ULyudTownTalkSubsystem::Start(UWorld& InWorld)
{
	State = MakePimpl<FTownTalkState>();
	State->Book.Seed((uint32)FPlatformTime::Cycles() | 1u);
	Reload();
	View = MakeShared<FTownTalkView>();
	if (UGameViewportClient* VC = InWorld.GetGameViewport())
	{
		// under the conversation panel and the menus (NpcDialogue's overlay is 5)
		Overlay = SNew(TownTalkLocal::STownTalkBubbles, View.ToSharedRef());
		VC->AddViewportWidgetContent(Overlay.ToSharedRef(), 4);
		Viewport = VC;
	}
	Clock = 0.f;
	NextBarkAt = 6.f;
	NextGreetAt = 2.f;
	bStarted = true;
}

void ULyudTownTalkSubsystem::Stop()
{
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	View.Reset();
	Viewport.Reset();
	ClearAll();
	bStarted = false;
}

bool ULyudTownTalkSubsystem::Reload()
{
	if (!State)
	{
		return false;
	}
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Dialogue/town_barks.txt");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("TownTalk: no %s"), *Path);
		return false;
	}
	const bool bOk = State->Book.Parse(TownTalkLocal::ToStd(Text));
	for (const std::string& W : State->Book.Warnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("TownTalk: town_barks.txt %s"), *TownTalkLocal::ToFString(W));
	}
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("TownTalk: town_barks.txt not read: %s"), *TownTalkLocal::ToFString(State->Book.LastError));
	}
	State->Said.assign(State->Book.Sets.size(), 0);
	Exchange.bActive = false;
	bLoaded = bOk;
	UE_LOG(LogTemp, Display, TEXT("TownTalk: %d sets of lines"), (int32)State->Book.Sets.size());
	return bOk;
}

void ULyudTownTalkSubsystem::ClearAll()
{
	for (FBubble& B : Bubbles)
	{
		B.bActive = false;
		B.Actor.Reset();
		B.Mesh.Reset();
	}
	Exchange.bActive = false;
	if (View.IsValid())
	{
		for (FTownTalkView::FItem& It : View->Items)
		{
			It.bShow = false;
		}
	}
}

void ULyudTownTalkSubsystem::Tick(float DeltaTime)
{
	if (!State || !bLoaded || !View.IsValid())
	{
		return;
	}
	if (CVarTownTalk.GetValueOnGameThread() == 0)
	{
		ClearAll();
		return;
	}
	Clock += DeltaTime;
	FestivalLook -= DeltaTime;
	if (FestivalLook <= 0.f)
	{
		// the holiday on now and its phase (the festivals' subsystem, when it is in the game)
		FestivalLook = 2.f;
		UObject* Fest = TownTalkLocal::FestivalSubsystemOf(GetWorld());
		static const FName ActiveFestivalName(TEXT("ActiveFestival"));
		static const FName ActivePhaseName(TEXT("ActivePhase"));
		const FName Festival = TownTalkLocal::CallName(Fest, ActiveFestivalName);
		const FName Phase = TownTalkLocal::CallName(Fest, ActivePhaseName);
		State->Now.Festival = Festival.IsNone() ? std::string() : TownTalkLocal::ToStd(Festival.ToString());
		State->Now.Phase = Phase.IsNone() ? std::string() : TownTalkLocal::ToStd(Phase.ToString());
	}
	ScanTimer -= DeltaTime;
	if (ScanTimer <= 0.f)
	{
		ScanTimer = 0.5f;
		Scan();
	}
	TickRestaurant(DeltaTime);
	TickExchange();
	UpdateView(DeltaTime);
}

void ULyudTownTalkSubsystem::UpdateNow()
{
	UWorld* World = GetWorld();
	LyudBarks::FNow& N = State->Now;
	const FDateTime T = UGameCalendar::NowIn(World);
	N.Hour = (float)T.GetHour() + (float)T.GetMinute() / 60.f;
	N.Month = T.GetMonth();
	N.Weekday = (int)T.GetDayOfWeek();
	Today = (int32)(T.GetDate().GetTicks() / ETimespan::TicksPerDay);
	float Cloud = 0.3f, Rain = 0.f, Snow = 0.f, Fog = 0.f, TempC = 10.f, Wet = 0.f;
	if (ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
	{
		const FLyudSkyWeather W = Weather->GetSky();
		Cloud = W.Cloud;
		Rain = W.Rain;
		Snow = W.Snow;
		Fog = W.Fog;
		TempC = W.TempC;
		Wet = W.Wet;
	}
	// NpcDialogue's overcast switch (the sky's grey without the weather) counts as clouds too
	static IConsoleVariable* Overcast = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Overcast"));
	if (Overcast && Overcast->GetInt() > 0)
	{
		Cloud = FMath::Max(Cloud, 0.9f);
	}
	if (!Sky.IsValid())
	{
		TActorIterator<ALyudSky> It(World);
		Sky = It ? *It : nullptr;
	}
	const ALyudSky* TheSky = Cast<ALyudSky>(Sky.Get());
	const float Altitude = TheSky ? TheSky->SunAltitude() : (N.Hour > 8.f && N.Hour < 17.f ? 20.f : -10.f);
	N.Sun = LyudBarks::SunState(Altitude, Cloud);
	LyudBarks::WeatherWords(Cloud, Rain, Snow, Fog, TempC, Wet, N.Weather);
}

bool ULyudTownTalkSubsystem::Busy(int32 Person) const
{
	if (Person < 0)
	{
		return false;
	}
	if (Exchange.bActive && (Exchange.Person[0] == Person || Exchange.Person[1] == Person))
	{
		return true;
	}
	for (const FBubble& B : Bubbles)
	{
		if (B.bActive && B.Person == Person)
		{
			return true;
		}
	}
	return false;
}

AActor* ULyudTownTalkSubsystem::ActorOf(int32 Person) const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	FPersonInfo Info;
	if (!Life || !Life->GetPerson(Person, Info))
	{
		return nullptr;
	}
	AActor* A = Info.Actor.Get();
	return IsValid(A) && !A->IsHidden() ? A : nullptr;
}

bool ULyudTownTalkSubsystem::Still(const AActor* Actor, int32 Person) const
{
	// a townsperson's body goes back to the pool when they walk off and may come out as somebody else's: the line
	// stays only over the one who said it
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	if (Person < 0 || !S || Person >= (int32)S->People.size())
	{
		return true;
	}
	return FVector::Dist2D(Actor->GetActorLocation(), TownTalkLocal::PersonAt(S->People[(size_t)Person])) < 300.0;
}

FVector ULyudTownTalkSubsystem::HeadOf(const FBubble& B) const
{
	static const FName HeadBone(TEXT("head"));
	const USkeletalMeshComponent* M = B.Mesh.Get();
	if (M && M->GetBoneIndex(HeadBone) != INDEX_NONE)
	{
		return M->GetBoneLocation(HeadBone) + FVector(0.0, 0.0, 32.0);
	}
	if (M)
	{
		return M->Bounds.Origin + FVector(0.0, 0.0, M->Bounds.BoxExtent.Z + 12.0);
	}
	const AActor* A = B.Actor.Get();
	return A ? A->GetActorLocation() + FVector(0.0, 0.0, 190.0) : FVector::ZeroVector;
}

bool ULyudTownTalkSubsystem::CanSee(const FVector& Cam, const FVector& Head, const AActor* Actor) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TownTalkSight), false);
	Params.AddIgnoredActor(Actor);
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector To = Head - (Head - Cam).GetSafeNormal() * 40.0;
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Cam, To, ECC_Visibility, Params);
}

void ULyudTownTalkSubsystem::CountSaid(int32 Set)
{
	if (Set >= 0 && Set < (int32)State->Said.size())
	{
		State->Said[(size_t)Set]++;
	}
}

bool ULyudTownTalkSubsystem::Show(AActor* Actor, int32 Person, const FString& Key)
{
	if (!IsValid(Actor) || !View.IsValid())
	{
		return false;
	}
	const FString Full = TEXT("line.") + Key;
	const FString Text = LyudText::Get(Full).ToString();
	if (Text.IsEmpty() || Text == Full)
	{
		return false;   // no words for it in any language
	}
	// the speaker's own place on screen if they have one up, else a free one
	int32 Slot = INDEX_NONE;
	for (int32 I = 0; I < MaxBubbles && Slot == INDEX_NONE; ++I)
	{
		Slot = Bubbles[I].bActive && Bubbles[I].Actor.Get() == Actor ? I : INDEX_NONE;
	}
	for (int32 I = 0; I < MaxBubbles && Slot == INDEX_NONE; ++I)
	{
		Slot = !Bubbles[I].bActive ? I : INDEX_NONE;
	}
	if (Slot == INDEX_NONE)
	{
		return false;
	}
	FBubble& B = Bubbles[Slot];
	B.bActive = true;
	B.Actor = Actor;
	B.Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	B.Person = Person;
	B.Key = Key;
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	B.Name = Life && Person >= 0 ? Life->GetDisplayName(Person) : FText::GetEmpty();
	const FString Lang = LyudText::Language();
	const bool bCJK = Lang == TEXT("ja") || Lang == TEXT("zh") || Lang == TEXT("ko");
	TownTalkLocal::Wrap(Text, bCJK, B.Rows);
	// up long enough to read it: about 18 characters a second (Japanese 9), 2.5 to 7 s
	B.Life = FMath::Clamp(2.2f + (bCJK ? 0.11f : 0.055f) * (float)Text.Len(), 2.5f, 7.f);
	B.Age = 0.f;
	B.Alpha = 0.f;
	B.bSeen = true;
	B.SeenCheck = 0.f;
	B.bOwnBody = Person >= 0 && ActorOf(Person) == Actor;
	FTownTalkView::FItem& It = View->Items[Slot];
	It.bShow = false;
	It.Alpha = 0.f;
	It.Name = B.Name;
	It.NumRows = FMath::Min(B.Rows.Num(), FTownTalkView::MaxRows);
	for (int32 R = 0; R < It.NumRows; ++R)
	{
		It.Rows[R] = B.Rows[R];
	}
	// the mouth moves while they say it
	if (ANpcBody* Body = Cast<ANpcBody>(Actor))
	{
		Body->Chatter(B.Life * 0.75f);
	}
	if (LastSpoke.IsValidIndex(Person))
	{
		LastSpoke[Person] = Clock;
	}
	if (CVarTownTalkLog.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Display, TEXT("BARKTEST %s (%d) says %s: %s"), *B.Name.ToString(), Person, *Key, *Text);
	}
	return true;
}

void ULyudTownTalkSubsystem::Scan()
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	// hidden while the sit-down cutscene and ATON OS have him; nobody placed yet after a load
	if (!S || S->Settling() || !PC || !PC->PlayerCameraManager || !Hero || Hero->IsHidden())
	{
		return;
	}
	const int32 Num = (int32)S->People.size();
	if (GreetedDay.Num() != Num)
	{
		GreetedDay.Init(-1, Num);
		LastSpoke.Init(-1000.f, Num);
	}
	UpdateNow();
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Look = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector HeroAt = Hero->GetActorLocation();
	const bool bResidents = CVarTownTalkResidents.GetValueOnGameThread() != 0;
	// who is near enough to be heard (14 m) and up and about (the life's own test, as tools/barksim's)
	std::vector<int>& Near = State->Near;
	Near.clear();
	for (const LyudPeople::FPerson& P : S->People)
	{
		if ((!P.bTown && !bResidents) || P.bExternal || !LyudBarks::CanSpeak(*S, P))
		{
			continue;
		}
		if (FVector::DistSquared(TownTalkLocal::PersonAt(P), Cam) < 1400.0 * 1400.0)
		{
			Near.push_back(P.Id);
		}
	}
	if (Near.empty())
	{
		return;
	}
	auto InView = [&Cam, &Look](const FVector& At, float MinDot)
	{
		return FVector::DotProduct((At + FVector(0.0, 0.0, 150.0) - Cam).GetSafeNormal(), Look) > MinDot;
	};

	// a greeting from somebody the player walks past (once a day each; not everybody greets a boy they don't know);
	// the boy himself only: as a bird the player is no one to say hello to, and the day's greeting is kept for him
	if (Clock >= NextGreetAt && Hero->IsA<AHeroCharacter>())
	{
		for (const int Id : Near)
		{
			const LyudPeople::FPerson& P = S->People[(size_t)Id];
			const FVector At = TownTalkLocal::PersonAt(P);
			if (GreetedDay[Id] == Today || FVector::Dist2D(At, HeroAt) > 350.0 || FMath::Abs(At.Z - HeroAt.Z) > 250.0 || Busy(Id) || !InView(At, 0.f))
			{
				continue;
			}
			GreetedDay[Id] = Today;
			if (FMath::FRand() > 0.65f)
			{
				continue;
			}
			AActor* Actor = ActorOf(Id);
			if (!Actor)
			{
				continue;
			}
			LyudBarks::FillWho(*S, P, State->Who);
			State->Who.bHeroNear = true;
			int SetIx = -1;
			const std::string* Key = State->Book.PickLine(LyudBarks::EKind::Greet, State->Who, State->Now, nullptr, &SetIx);
			if (Key && Show(Actor, Id, TownTalkLocal::ToFString(*Key)))
			{
				CountSaid(SetIx);
				NextGreetAt = Clock + 4.f;
				break;
			}
		}
	}

	if (Clock < NextBarkAt)
	{
		return;
	}
	const float Every = FMath::Max(2.f, CVarTownTalkEvery.GetValueOnGameThread());
	// now and then two of them have an exchange (side by side, or the life's chat pairs)
	if (!Exchange.bActive && FMath::FRand() < 0.45f && StartExchange(Cam, Look))
	{
		NextBarkAt = Clock + Every * FMath::FRandRange(0.8f, 1.3f) + 4.f;
		return;
	}
	// else a line on one's own: somebody in view who hasn't spoken for a minute and a half
	for (int32 Try = 0; Try < 4; ++Try)
	{
		const int Id = Near[(size_t)FMath::RandRange(0, (int32)Near.size() - 1)];
		const LyudPeople::FPerson& P = S->People[(size_t)Id];
		const FVector At = TownTalkLocal::PersonAt(P);
		if (Busy(Id) || Clock - LastSpoke[Id] < 90.f || !InView(At, 0.35f))
		{
			continue;
		}
		AActor* Actor = ActorOf(Id);
		if (!Actor)
		{
			continue;
		}
		LyudBarks::FillWho(*S, P, State->Who);
		State->Who.bHeroNear = FVector::Dist(At, HeroAt) < 1000.0;
		int SetIx = -1;
		const std::string* Key = State->Book.PickLine(LyudBarks::EKind::Say, State->Who, State->Now, nullptr, &SetIx);
		if (Key && Show(Actor, Id, TownTalkLocal::ToFString(*Key)))
		{
			CountSaid(SetIx);
			NextBarkAt = Clock + Every * FMath::FRandRange(0.75f, 1.3f);
			return;
		}
	}
	NextBarkAt = Clock + 1.5f;     // nobody had anything to say: look again soon
}

bool ULyudTownTalkSubsystem::StartExchange(const FVector& Cam, const FVector& Look)
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!S || !Hero)
	{
		return false;
	}
	const FVector HeroAt = Hero->GetActorLocation();
	const std::vector<int>& Near = State->Near;
	for (size_t I = 0; I < Near.size(); ++I)
	{
		const LyudPeople::FPerson& A = S->People[(size_t)Near[I]];
		if (Busy(A.Id) || Clock - LastSpoke[A.Id] < 60.f)
		{
			continue;
		}
		const FVector AtA = TownTalkLocal::PersonAt(A);
		if (FVector::DotProduct((AtA + FVector(0.0, 0.0, 150.0) - Cam).GetSafeNormal(), Look) < 0.35f || FVector::Dist(AtA, Cam) > 1000.0)
		{
			continue;
		}
		for (size_t J = I + 1; J < Near.size(); ++J)
		{
			const LyudPeople::FPerson& B = S->People[(size_t)Near[J]];
			// talking to each other, or seated side by side (a bench, a table, the club's row of computers)
			const bool bPair = A.Partner == B.Id && B.Partner == A.Id;
			const bool bSide = A.Place >= 0 && A.Place == B.Place && LyudPeople::IsSeated(A.Pose) && LyudPeople::IsSeated(B.Pose)
				&& A.State == LyudPeople::EState::Doing && B.State == LyudPeople::EState::Doing && (A.Pos - B.Pos).Len2D() < 2.f;
			if (!(bPair || bSide) || Busy(B.Id) || Clock - LastSpoke[B.Id] < 60.f)
			{
				continue;
			}
			AActor* ActA = ActorOf(A.Id);
			AActor* ActB = ActA ? ActorOf(B.Id) : nullptr;
			if (!ActA || !ActB)
			{
				continue;
			}
			LyudBarks::FillWho(*S, A, State->Who);
			LyudBarks::FillWho(*S, B, State->Other);
			State->Who.bHeroNear = FVector::Dist(AtA, HeroAt) < 1000.0;
			State->Other.bHeroNear = FVector::Dist(TownTalkLocal::PersonAt(B), HeroAt) < 1000.0;
			bool bSwapped = false;
			int SetIx = -1;
			const std::vector<std::string>* Talk = State->Book.PickTalk(State->Who, State->Other, State->Now, bSwapped, &SetIx);
			if (!Talk || SetIx < 0)
			{
				continue;
			}
			const LyudBarks::FSet& Set = State->Book.Sets[(size_t)SetIx];
			Exchange.bActive = true;
			Exchange.Person[0] = bSwapped ? B.Id : A.Id;
			Exchange.Person[1] = bSwapped ? A.Id : B.Id;
			Exchange.Actor[0] = bSwapped ? ActB : ActA;
			Exchange.Actor[1] = bSwapped ? ActA : ActB;
			Exchange.Set = SetIx;
			Exchange.Talk = (int32)(Talk - Set.Talks.data());
			Exchange.Next = 0;
			Exchange.NextAt = Clock;
			CountSaid(SetIx);
			return true;
		}
	}
	return false;
}

void ULyudTownTalkSubsystem::TickExchange()
{
	if (!Exchange.bActive)
	{
		return;
	}
	const std::vector<LyudBarks::FSet>& Sets = State->Book.Sets;
	if (Exchange.Set < 0 || Exchange.Set >= (int32)Sets.size() || Exchange.Talk < 0 || Exchange.Talk >= (int32)Sets[(size_t)Exchange.Set].Talks.size())
	{
		Exchange.bActive = false;
		return;
	}
	const std::vector<std::string>& Lines = Sets[(size_t)Exchange.Set].Talks[(size_t)Exchange.Talk];
	AActor* First = Exchange.Actor[0].Get();
	AActor* Second = Exchange.Actor[1].Get();
	// gone off (the body back to the pool), apart, or somebody else's body now
	if (!IsValid(First) || !IsValid(Second) || First->IsHidden() || Second->IsHidden()
		|| FVector::Dist(First->GetActorLocation(), Second->GetActorLocation()) > 450.0
		|| !Still(First, Exchange.Person[0]) || !Still(Second, Exchange.Person[1]))
	{
		Exchange.bActive = false;
		return;
	}
	if (Clock < Exchange.NextAt)
	{
		return;
	}
	if (Exchange.Next >= (int32)Lines.size())
	{
		Exchange.bActive = false;
		for (const int32 Id : Exchange.Person)
		{
			if (LastSpoke.IsValidIndex(Id))
			{
				LastSpoke[Id] = Clock;
			}
		}
		return;
	}
	const int32 Turn = Exchange.Next % 2;
	AActor* Speaker = Turn == 0 ? First : Second;
	// the line before goes soon after the answer comes up
	for (FBubble& B : Bubbles)
	{
		if (B.bActive && B.Actor.Get() != Speaker && (B.Actor.Get() == First || B.Actor.Get() == Second))
		{
			B.Life = FMath::Min(B.Life, B.Age + 0.6f);
		}
	}
	if (!Show(Speaker, Exchange.Person[Turn], TownTalkLocal::ToFString(Lines[(size_t)Exchange.Next])))
	{
		Exchange.bActive = false;
		return;
	}
	float Read = 2.5f;
	for (const FBubble& B : Bubbles)
	{
		if (B.bActive && B.Actor.Get() == Speaker)
		{
			Read = B.Life;
		}
	}
	// the answer comes once the line has been read
	Exchange.NextAt = Clock + Read * 0.8f + 0.3f;
	Exchange.Next++;
}

void ULyudTownTalkSubsystem::UpdateView(float DeltaTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bHide = !PC || !PC->PlayerCameraManager || !Hero || Hero->IsHidden();
	int32 SizeX = 1, SizeY = 1;
	if (PC)
	{
		PC->GetViewportSize(SizeX, SizeY);
	}
	View->ViewportSize = FVector2D((double)FMath::Max(SizeX, 1), (double)FMath::Max(SizeY, 1));
	const FVector Cam = !bHide ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FVector Look = !bHide ? PC->PlayerCameraManager->GetCameraRotation().Vector() : FVector::ForwardVector;
	for (int32 I = 0; I < MaxBubbles; ++I)
	{
		FBubble& B = Bubbles[I];
		FTownTalkView::FItem& It = View->Items[I];
		if (!B.bActive)
		{
			It.bShow = false;
			continue;
		}
		B.Age += DeltaTime;
		AActor* A = B.Actor.Get();
		if (B.Age >= B.Life || !IsValid(A) || A->IsHidden() || (B.bOwnBody && !Still(A, B.Person)))
		{
			B.bActive = false;
			It.bShow = false;
			continue;
		}
		if (bHide)
		{
			It.bShow = false;
			continue;
		}
		const FVector Head = HeadOf(B);
		const float Dist = (float)FVector::Dist(Head, Cam);
		B.SeenCheck -= DeltaTime;
		if (B.SeenCheck <= 0.f)
		{
			B.SeenCheck = 0.1f;
			B.bSeen = CanSee(Cam, Head, A);
		}
		FVector2D Pixel(0.0, 0.0);
		const bool bOnScreen = FVector::DotProduct(Head - Cam, Look) > 0.0 && PC->ProjectWorldLocationToScreen(Head, Pixel, true);
		// fades in, and out at the end, with the distance (all of it within 9 m, none past 18 m) and behind walls
		const float Fade = FMath::Clamp(B.Age / 0.2f, 0.f, 1.f) * FMath::Clamp((B.Life - B.Age) / 0.35f, 0.f, 1.f)
			* FMath::Clamp((1800.f - Dist) / 900.f, 0.f, 1.f);
		B.Alpha = FMath::FInterpTo(B.Alpha, bOnScreen && B.bSeen ? Fade : 0.f, DeltaTime, 12.f);
		It.bShow = bOnScreen && B.Alpha > 0.01f;
		if (bOnScreen)
		{
			It.Pixel = Pixel;
		}
		It.Alpha = B.Alpha;
	}
}

// --- the restaurant's moments --------------------------------------------------------------------------------------

int32 ULyudTownTalkSubsystem::StaffPerson(FName Role) const
{
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const TArray<int32> On = Life ? Life->StaffOnShift(TEXT("restaurant"), Role) : TArray<int32>();
	return On.Num() > 0 ? On[0] : -1;
}

void ULyudTownTalkSubsystem::TickRestaurant(float DeltaTime)
{
	// the restaurant tells its moments itself (SayEvent): nothing to read off it
	if (bRestPushed)
	{
		return;
	}
	RestaurantLook -= DeltaTime;
	if (!Restaurant.IsValid() && RestaurantLook <= 0.f)
	{
		RestaurantLook = 5.f;
		Restaurant = TownTalkLocal::FindRestaurant(GetWorld());
	}
	AActor* Rest = Restaurant.Get();
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	RestaurantPoll -= DeltaTime;
	if (!Rest || !Hero || RestaurantPoll > 0.f)
	{
		return;
	}
	RestaurantPoll = 0.1f;
	const float HeroDist = (float)FVector::Dist(Hero->GetActorLocation(), Rest->GetActorLocation());
	if (HeroDist > 3000.f)
	{
		bHeroWasSeated = false;
		SitLineAt = WaiterCarryAt = -1.f;
		return;
	}
	const TownTalkLocal::FRestNow R = TownTalkLocal::ReadRestaurant(Rest);
	// the hall is ~10 m round the building's middle: seated within 14 m of it is at one of its tables
	const bool bSeated = Hero->IsSeated() && HeroDist < 1400.f;
	if (bSeated && !bHeroWasSeated)
	{
		// sat down: the waiter has a word once the body is in the chair
		SitLineAt = Clock + 1.6f;
		bAteThisVisit = false;
		BillAnnounced.Reset();
	}
	if (!bSeated && bHeroWasSeated && bAteThisVisit && R.Waiter)
	{
		SayMoment(R.Waiter, TEXT("rest_bye"), StaffPerson(TEXT("waiter")), TEXT("waiter"));
	}
	bHeroWasSeated = bSeated;
	if (SitLineAt > 0.f && Clock >= SitLineAt)
	{
		SitLineAt = -1.f;
		if (bSeated && R.Waiter)
		{
			SayMoment(R.Waiter, TEXT("rest_sit"), StaffPerson(TEXT("waiter")), TEXT("waiter"));
		}
	}
	// a tray taken at the pass (the dishes on it while the table's didn't go: not the empties cleared off): the cook
	// calls it, the waiter answers
	if (R.OnTray > 0 && LastOnTray == 0 && R.HeroDishes >= LastHeroDishes && HeroDist < 2500.f)
	{
		if (R.Cook)
		{
			SayMoment(R.Cook, TEXT("rest_carry"), StaffPerson(TEXT("cook")), TEXT("cook"));
		}
		WaiterCarryAt = Clock + 1.4f;
	}
	if (WaiterCarryAt > 0.f && Clock >= WaiterCarryAt)
	{
		WaiterCarryAt = -1.f;
		if (R.Waiter)
		{
			SayMoment(R.Waiter, TEXT("rest_carry"), StaffPerson(TEXT("waiter")), TEXT("waiter"));
		}
	}
	// a dish set down on the player's table (one word for a whole tray)
	if (R.HeroDishes > LastHeroDishes)
	{
		bAteThisVisit = bAteThisVisit || bSeated;
		if (R.Waiter && Clock - LastServeLine > 25.f)
		{
			LastServeLine = Clock;
			SayMoment(R.Waiter, TEXT("rest_serve"), StaffPerson(TEXT("waiter")), TEXT("waiter"));
		}
	}
	// the bill laid on the player's table
	if (R.Bill && bSeated && R.Bill != BillAnnounced.Get() && FVector::Dist2D(R.Bill->GetActorLocation(), Hero->GetActorLocation()) < 180.0)
	{
		BillAnnounced = R.Bill;
		if (R.Waiter)
		{
			SayMoment(R.Waiter, TEXT("rest_bill"), StaffPerson(TEXT("waiter")), TEXT("waiter"));
		}
	}
	LastOnTray = R.OnTray;
	LastHeroDishes = R.HeroDishes;
}

bool ULyudTownTalkSubsystem::SayMoment(AActor* Speaker, FName Event, int32 PersonId, FName Role)
{
	if (!State || !bLoaded || !IsValid(Speaker) || Speaker->IsHidden() || CVarTownTalk.GetValueOnGameThread() == 0)
	{
		return false;
	}
	UpdateNow();
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	LyudBarks::FWho& W = State->Who;
	if (S && PersonId >= 0 && PersonId < (int32)S->People.size())
	{
		LyudBarks::FillWho(*S, S->People[(size_t)PersonId], W);
	}
	else
	{
		W.Reset();
	}
	if (!Role.IsNone())
	{
		const std::string RoleName = TownTalkLocal::ToStd(Role.ToString());
		LyudBarks::FillWhoRole(W, RoleName.c_str());
	}
	W.bHeroNear = true;
	const std::string EventName = TownTalkLocal::ToStd(Event.ToString());
	int SetIx = -1;
	const std::string* Key = State->Book.PickLine(LyudBarks::EKind::Event, W, State->Now, EventName.c_str(), &SetIx);
	if (!Key || !Show(Speaker, PersonId, TownTalkLocal::ToFString(*Key)))
	{
		return false;
	}
	CountSaid(SetIx);
	return true;
}

bool ULyudTownTalkSubsystem::SayEvent(AActor* Speaker, FName Event, int32 PersonId, FName Role)
{
	// the restaurant calling it itself: its properties needn't be read any more
	if (Event.ToString().StartsWith(TEXT("rest_")))
	{
		bRestPushed = true;
	}
	return SayMoment(Speaker, Event, PersonId, Role);
}

bool ULyudTownTalkSubsystem::SayLine(AActor* Speaker, const FString& Key, int32 PersonId)
{
	FString K = Key;
	K.RemoveFromStart(TEXT("line."));
	if (State)
	{
		State->Book.MarkSaid(TownTalkLocal::ToStd(K));
	}
	return Show(Speaker, PersonId, K);
}

int32 ULyudTownTalkSubsystem::NearestSpeaker(AActor*& OutActor) const
{
	OutActor = nullptr;
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const LyudPeople::FWorld* S = Life ? Life->GetSim() : nullptr;
	const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!S || !Hero)
	{
		return -1;
	}
	int32 Best = -1;
	double BestD = 3000.0 * 3000.0;
	for (const LyudPeople::FPerson& P : S->People)
	{
		const double D = FVector::DistSquared(TownTalkLocal::PersonAt(P), Hero->GetActorLocation());
		if (D < BestD && LyudBarks::CanSpeak(*S, P) && !P.bExternal)
		{
			if (AActor* A = ActorOf(P.Id))
			{
				Best = P.Id;
				BestD = D;
				OutActor = A;
			}
		}
	}
	return Best;
}

FString ULyudTownTalkSubsystem::Report() const
{
	if (!State)
	{
		return TEXT("TownTalk: not running");
	}
	FString Out = FString::Printf(TEXT("TownTalk: %d sets; festival '%s' phase '%s', sun %s, weather"), (int32)State->Book.Sets.size(),
		*TownTalkLocal::ToFString(State->Now.Festival), *TownTalkLocal::ToFString(State->Now.Phase), *TownTalkLocal::ToFString(State->Now.Sun));
	for (const std::string& W : State->Now.Weather)
	{
		Out += TEXT(" ") + TownTalkLocal::ToFString(W);
	}
	for (const FBubble& B : Bubbles)
	{
		if (B.bActive)
		{
			Out += FString::Printf(TEXT("\n  up: %s (%d) %s %.1f/%.1f s"), *B.Name.ToString(), B.Person, *B.Key, B.Age, B.Life);
		}
	}
	if (Exchange.bActive)
	{
		Out += FString::Printf(TEXT("\n  exchange of [%s] between %d and %d, line %d"), *TownTalkLocal::ToFString(State->Book.Sets[(size_t)Exchange.Set].Name),
			Exchange.Person[0], Exchange.Person[1], Exchange.Next);
	}
	Out += TEXT("\n  said:");
	for (size_t I = 0; I < State->Said.size() && I < State->Book.Sets.size(); ++I)
	{
		if (State->Said[I] > 0)
		{
			Out += FString::Printf(TEXT(" %s %d"), *TownTalkLocal::ToFString(State->Book.Sets[I].Name), State->Said[I]);
		}
	}
	return Out;
}

// --- console -------------------------------------------------------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GTownTalkReport(TEXT("lyud.Barks.Report"),
	TEXT("lyud.Barks.Report: the townsfolk's lines on screen now, the moment (holiday, sky) and how often each set was said."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Talk->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GTownTalkSay(TEXT("lyud.Barks.Say"),
	TEXT("lyud.Barks.Say <key>: the townsperson nearest the player says a line (town.granny.3), or the line of a moment (rest_serve)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World);
		AActor* Actor = nullptr;
		const int32 Id = Talk ? Talk->NearestSpeaker(Actor) : -1;
		if (!Talk || Args.Num() < 1 || !Actor)
		{
			UE_LOG(LogTemp, Warning, TEXT("TownTalk: nobody near to say it"));
			return;
		}
		const bool bOk = Args[0].StartsWith(TEXT("rest_")) ? Talk->SayEvent(Actor, FName(*Args[0]), Id) : Talk->SayLine(Actor, Args[0], Id);
		UE_LOG(LogTemp, Warning, TEXT("TownTalk: %d says %s: %s"), Id, *Args[0], bOk ? TEXT("up") : TEXT("no such line"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GTownTalkReload(TEXT("lyud.Barks.Reload"),
	TEXT("lyud.Barks.Reload: reads Content/Dialogue/town_barks.txt again."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(World))
		{
			Talk->Reload();
		}
	}));
