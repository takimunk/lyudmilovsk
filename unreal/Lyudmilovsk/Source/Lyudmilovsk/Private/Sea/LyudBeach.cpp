#include "Sea/LyudBeach.h"

#include "Audio/LyudSoundscape.h"
#include "BeachModel.h"
#include "City/LyudCityThings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Plants/GameCalendar.h"
#include "Rendering/DrawElements.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "Sea/Tide.h"
#include "Sky/LyudSky.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "UI/LyudUIStyle.h"
#include "Weather/LyudWeather.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudBeach, Log, All);

/** What the beach shows on the screen: a line, the skips' count, the swim's clock, the swing of the throwing arm. */
struct FLyudBeachHud
{
	FText Line;
	FText Count;
	FText Clock;
	/** 0 .. 1 while the throw key is held, under 0 otherwise. */
	float Swing = -1.f;
};

namespace LyudBeachImpl
{
	constexpr int32 MaxStones = 5;
	constexpr int32 MaxFinds = 14;
	constexpr int32 FindCellsEachWay = 6;
	constexpr float ReachCm = 170.f;
	constexpr float CastleReachCm = 260.f;
	constexpr float PostReachCm = 220.f;
	constexpr float HoldToBuild = 0.45f;

	/** The swing of the arm: a bar, the good stretch of it brighter, a marker running along. */
	class SSwing : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSwing) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, TSharedPtr<FLyudBeachHud> InHud) { Hud = InHud; }

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(360.0, 14.0); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle&, bool) const override
		{
			if (!Hud.IsValid() || Hud->Swing < 0.f)
			{
				return Layer;
			}
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			const FVector2D Size = Geometry.GetLocalSize();
			const auto Box = [&](double X0, double X1, double Pad, const FLinearColor& Color, int32 L)
			{
				FSlateDrawElement::MakeBox(Out, L, Geometry.ToPaintGeometry(FVector2D(X1 - X0, Size.Y - Pad * 2.0), FSlateLayoutTransform(FVector2D(X0, Pad))),
					White, ESlateDrawEffect::None, Color);
			};
			Box(0.0, Size.X, 3.0, FLinearColor(0.f, 0.f, 0.f, 0.55f), Layer);
			const double Best = LyudBeach::BestSwing, Width = LyudBeach::SwingWidth;
			Box(Size.X * FMath::Max(0.0, Best - Width * 0.5), Size.X * FMath::Min(1.0, Best + Width * 0.5), 3.0, FLinearColor(0.62f, 0.5f, 0.36f, 0.8f), Layer + 1);
			Box(Size.X * (Best - 0.03), Size.X * (Best + 0.03), 3.0, FLinearColor(1.f, 0.92f, 0.78f, 0.95f), Layer + 2);
			const double At = Size.X * FMath::Clamp(static_cast<double>(Hud->Swing), 0.0, 1.0);
			Box(At - 2.0, At + 2.0, 0.0, FLinearColor::White, Layer + 3);
			return Layer + 3;
		}

	private:
		TSharedPtr<FLyudBeachHud> Hud;
	};

	LyudBeach::FBook ToModel(const ULyudBeachBook& B)
	{
		LyudBeach::FBook M;
		M.BestSkips = B.BestSkips;
		M.Throws = B.Throws;
		M.BestSwim = B.BestSwim;
		M.Swims = B.Swims;
		M.CastlesBuilt = B.CastlesBuilt;
		M.CastlesWashed = B.CastlesWashed;
		M.Shells = B.Shells;
		M.Ambers = B.Ambers;
		M.FoundBits = static_cast<uint32>(B.FoundBits);
		return M;
	}

	void FromModel(ULyudBeachBook& B, const LyudBeach::FBook& M)
	{
		B.BestSkips = M.BestSkips;
		B.Throws = M.Throws;
		B.BestSwim = static_cast<float>(M.BestSwim);
		B.Swims = M.Swims;
		B.CastlesBuilt = M.CastlesBuilt;
		B.CastlesWashed = M.CastlesWashed;
		B.Shells = M.Shells;
		B.Ambers = M.Ambers;
		B.FoundBits = static_cast<int32>(M.FoundBits);
	}

	LyudBeach::FCastle CastleOf(const ULyudBeachBook& B)
	{
		LyudBeach::FCastle C;
		C.bAlive = B.bCastle;
		C.X = B.CastleAt.X / 100.0;
		C.Y = B.CastleAt.Y / 100.0;
		C.Z = B.CastleAt.Z / 100.0;
		C.Work = B.CastleWork;
		C.Worn = B.CastleWorn;
		return C;
	}

	void KeepCastle(ULyudBeachBook& B, const LyudBeach::FCastle& C)
	{
		B.bCastle = C.bAlive;
		B.CastleWork = static_cast<float>(C.Work);
		B.CastleWorn = static_cast<float>(C.Worn);
	}

	int32 DayOf(const FDateTime& When)
	{
		return static_cast<int32>(FMath::FloorToDouble((When - FDateTime(2000, 1, 1)).GetTotalDays()));
	}

	FText Seconds(float S)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(1);
		Options.SetMaximumFractionalDigits(1);
		return FText::AsNumber(S, &Options);
	}

	/** The stone's height over the water (m) and how far along (m) T seconds after the release: LyudBeach::StoneAt on the kept hops. */
	double StoneAt(const TArray<FVector4f>& Hops, double T, double& XOut)
	{
		double T0 = 0.0, X0 = 0.0;
		for (int32 I = 0; I < Hops.Num(); ++I)
		{
			const FVector4f& Hop = Hops[I];
			if (T <= Hop.Y)
			{
				const double U = Hop.Y > T0 ? (T - T0) / (Hop.Y - T0) : 1.0;
				XOut = X0 + (Hop.X - X0) * U;
				return I == 0 ? Hop.Z * (1.0 - U * U) : Hop.Z * 4.0 * U * (1.0 - U);
			}
			T0 = Hop.Y;
			X0 = Hop.X;
		}
		XOut = Hops.Num() ? Hops.Last().X : 0.0;
		return 0.0;
	}

	FAutoConsoleCommandWithWorldAndArgs GInfo(TEXT("lyud.Beach.Info"), TEXT("Logs the beach: the shore under the hero, his stones, the castle, the finds, the swim."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (const ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				UE_LOG(LogLyudBeach, Display, TEXT("BEACH %s"), *Beach->Report());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GStone(TEXT("lyud.Beach.Stone"), TEXT("A flat stone in the pocket: lyud.Beach.Stone [flatness 0..1, default 0.9]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				Beach->GiveStone(Args.Num() ? FCString::Atof(*Args[0]) : 0.9f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GThrow(TEXT("lyud.Beach.Throw"), TEXT("Throws a stone where the hero looks: lyud.Beach.Throw [quality 0..1, default 0.9]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				const bool bOk = Beach->ThrowNow(Args.Num() ? FCString::Atof(*Args[0]) : 0.9f);
				UE_LOG(LogLyudBeach, Display, TEXT("BEACH throw %s"), bOk ? TEXT("ok") : TEXT("refused (no sea, or a stone is in the air)"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GCastle(TEXT("lyud.Beach.Castle"), TEXT("A sandcastle a step in front of the hero: lyud.Beach.Castle [work 0..6, default 6]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				Beach->CastleHere(Args.Num() ? FCString::Atof(*Args[0]) : 6.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GRace(TEXT("lyud.Beach.Race"), TEXT("The whistle for the swim to the nearest buoy (from anywhere)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				UE_LOG(LogLyudBeach, Display, TEXT("BEACH race %s"), Beach->RaceNow() ? TEXT("ready") : TEXT("refused (no post found, or the buoy is aground)"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GStorm(TEXT("lyud.Beach.Storm"), TEXT("Pins how much of a storm the last day was for the waterline's finds: 0..1, -1 back to the weather."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				Beach->SetStorm(Args.Num() ? FCString::Atof(*Args[0]) : -1.f);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReset(TEXT("lyud.Beach.Reset"), TEXT("Empties the beach book: records, stones, the castle, today's picks."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudBeachBook* Book = ULyudBeachBook::Get(World))
			{
				Book->Reset();
			}
		}));

	// developer mode (docs/dev_mode.md)
	LyudDev::FAutoAction GDevStones(TEXT("beach.stones"), TEXT("beach"), TEXT("dev.action.beach.stones"),
		[](UWorld* World)
		{
			if (ULyudBeach* Beach = ULyudBeach::Get(World))
			{
				for (const float Flat : { 0.7f, 0.85f, 0.98f })
				{
					Beach->GiveStone(Flat);
				}
			}
		});
	LyudDev::FAutoAction GDevCastle(TEXT("beach.castle"), TEXT("beach"), TEXT("dev.action.beach.castle"),
		[](UWorld* World) { if (ULyudBeach* Beach = ULyudBeach::Get(World)) { Beach->CastleHere(6.f); } });
	LyudDev::FAutoAction GDevStorm(TEXT("beach.storm"), TEXT("beach"), TEXT("dev.action.beach.storm"),
		[](UWorld* World) { if (ULyudBeach* Beach = ULyudBeach::Get(World)) { Beach->SetStorm(1.f); } });
	LyudDev::FAutoAction GDevRace(TEXT("beach.race"), TEXT("beach"), TEXT("dev.action.beach.race"),
		[](UWorld* World) { if (ULyudBeach* Beach = ULyudBeach::Get(World)) { Beach->RaceNow(); } });
}

// ------------------------------------------------------------------ the book

ULyudBeachBook* ULyudBeachBook::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudBeachBook>() : nullptr;
}

void ULyudBeachBook::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Beach"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]() { Reset(); });
	}
}

void ULyudBeachBook::Reset()
{
	BestSkips = Throws = Swims = CastlesBuilt = CastlesWashed = Shells = Ambers = FoundBits = 0;
	BestSwim = 0.f;
	Stones.Reset();
	bCastle = false;
	CastleWork = CastleWorn = 0.f;
	PickDay = 0;
	Picked.Reset();
}

// ------------------------------------------------------------------ the beach

ULyudBeach* ULyudBeach::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBeach>() : nullptr;
}

void ULyudBeach::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bBegun = true;
}

void ULyudBeach::Deinitialize()
{
	bBegun = false;
	if (Widget.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
			}
		}
		Widget.Reset();
	}
	Hud.Reset();
	Super::Deinitialize();
}

void ULyudBeach::EnsureHud()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (Widget.IsValid() || !Viewport)
	{
		return;
	}
	Hud = MakeShared<FLyudBeachHud>();
	TSharedPtr<FLyudBeachHud> Shown = Hud;
	Widget = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 150.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(LyudUI::Font(40, "Bold")).ColorAndOpacity(LyudUI::Bright)
				.ShadowOffset(FVector2D(1.0, 1.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.Text_Lambda([Shown]() { return Shown->Count; })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f))
			[
				SNew(SBox).WidthOverride(900.f)
				[
					SNew(STextBlock).Font(LyudUI::Font(20)).ColorAndOpacity(LyudUI::Bright).AutoWrapText(true).Justification(ETextJustify::Center)
					.ShadowOffset(FVector2D(1.0, 1.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					.Text_Lambda([Shown]() { return Shown->Line; })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f))
			[
				SNew(LyudBeachImpl::SSwing, Shown)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 90.f, 40.f, 0.f))
		[
			SNew(STextBlock).Font(LyudUI::Font(30, "Bold")).ColorAndOpacity(LyudUI::Text)
			.ShadowOffset(FVector2D(1.0, 1.0)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.Text_Lambda([Shown]() { return Shown->Clock; })
		];
	Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), 40);
}

UStaticMeshComponent* ULyudBeach::AddShape(USceneComponent* Parent, UStaticMesh* Mesh, const FLinearColor& Tint, const FVector& At, const FVector& Scale)
{
	if (!Props || !Parent || !Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Props, NAME_None, RF_Transient);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	// small things that move or come and go: kept out of Lumen's scene and the distance fields (the perf checklist)
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->bVisibleInRayTracing = false;
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(At);
	C->SetRelativeScale3D(Scale);
	C->RegisterComponent();
	if (UMaterialInstanceDynamic* Mid = TintOf(Tint))
	{
		C->SetMaterial(0, Mid);
	}
	C->SetVisibility(false);
	return C;
}

UMaterialInstanceDynamic* ULyudBeach::TintOf(const FLinearColor& Tint)
{
	// one material per colour, shared by the shapes of that colour (made once, kept in Tints)
	for (int32 I = 0; I < Tints.Num() && I < TintColors.Num(); ++I)
	{
		if (Tints[I] && TintColors[I].Equals(Tint, 0.002f))
		{
			return Tints[I];
		}
	}
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Basic)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Basic, this);
	Mid->SetVectorParameterValue(TEXT("Color"), Tint);
	Tints.Add(Mid);
	TintColors.Add(Tint);
	return Mid;
}

void ULyudBeach::MakeProps()
{
	UWorld* World = GetWorld();
	if (Props || !World)
	{
		return;
	}
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Props = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
	if (!Props)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Props, TEXT("Root"), RF_Transient);
	Root->SetMobility(EComponentMobility::Movable);
	Props->SetRootComponent(Root);
	Root->RegisterComponent();

	const FLinearColor Stone(0.23f, 0.23f, 0.24f), Sand(0.46f, 0.38f, 0.25f), Wet(0.2f, 0.16f, 0.1f), Red(0.7f, 0.06f, 0.04f), White(0.85f, 0.85f, 0.8f),
		Wood(0.3f, 0.2f, 0.1f);
	StoneMesh = AddShape(Root, Sphere, Stone, FVector::ZeroVector, FVector(0.07f, 0.06f, 0.018f));
	for (int32 I = 0; I < LyudBeachImpl::MaxFinds; ++I)
	{
		FindMeshes.Add(AddShape(Root, Sphere, White, FVector::ZeroVector, FVector(0.05f)));
	}

	// the castle: every piece hangs on one root, squashed as the water wears it. Index: 0 moat, 1 heap, 2 keep, 3-6 towers,
	// 7-10 their caps, 11-14 walls, 15 the flag's stick, 16 the flag
	CastleRoot = NewObject<USceneComponent>(Props, TEXT("Castle"), RF_Transient);
	CastleRoot->SetMobility(EComponentMobility::Movable);
	CastleRoot->SetupAttachment(Root);
	CastleRoot->RegisterComponent();
	CastleMeshes.Add(AddShape(CastleRoot, Cylinder, Wet, FVector(0.f, 0.f, 0.6f), FVector(1.7f, 1.7f, 0.012f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cone, Sand, FVector(0.f, 0.f, 13.f), FVector(1.0f, 1.0f, 0.26f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cylinder, Sand, FVector(0.f, 0.f, 26.f), FVector(0.34f, 0.34f, 0.3f)));
	for (const FVector2D Corner : { FVector2D(30.0, 30.0), FVector2D(30.0, -30.0), FVector2D(-30.0, -30.0), FVector2D(-30.0, 30.0) })
	{
		CastleMeshes.Add(AddShape(CastleRoot, Cylinder, Sand, FVector(Corner.X, Corner.Y, 13.f), FVector(0.17f, 0.17f, 0.26f)));
	}
	for (const FVector2D Corner : { FVector2D(30.0, 30.0), FVector2D(30.0, -30.0), FVector2D(-30.0, -30.0), FVector2D(-30.0, 30.0) })
	{
		CastleMeshes.Add(AddShape(CastleRoot, Cone, Sand, FVector(Corner.X, Corner.Y, 31.f), FVector(0.2f, 0.2f, 0.1f)));
	}
	CastleMeshes.Add(AddShape(CastleRoot, Cube, Sand, FVector(30.f, 0.f, 8.f), FVector(0.07f, 0.6f, 0.16f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cube, Sand, FVector(-30.f, 0.f, 8.f), FVector(0.07f, 0.6f, 0.16f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cube, Sand, FVector(0.f, 30.f, 8.f), FVector(0.6f, 0.07f, 0.16f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cube, Sand, FVector(0.f, -30.f, 8.f), FVector(0.6f, 0.07f, 0.16f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cylinder, Wood, FVector(0.f, 0.f, 53.f), FVector(0.012f, 0.012f, 0.24f)));
	CastleMeshes.Add(AddShape(CastleRoot, Cube, Red, FVector(5.f, 0.f, 61.f), FVector(0.1f, 0.006f, 0.06f)));

	// the swim's posts and buoys: two shapes each, placed when the lifeguards' towers are known
	for (int32 I = 0; I < 4; ++I)
	{
		PostMeshes.Add(AddShape(Root, Cylinder, Wood, FVector::ZeroVector, FVector(0.08f, 0.08f, 1.3f)));
		PostMeshes.Add(AddShape(Root, Cube, Red, FVector::ZeroVector, FVector(0.04f, 0.4f, 0.25f)));
		BuoyMeshes.Add(AddShape(Root, Sphere, Red, FVector::ZeroVector, FVector(0.7f, 0.7f, 0.55f)));
		BuoyMeshes.Add(AddShape(Root, Cone, White, FVector::ZeroVector, FVector(0.3f, 0.3f, 0.5f)));
	}
}

FVector ULyudBeach::OnGround(const FVector& At) const
{
	const UWorld* World = GetWorld();
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(LyudBeachGround), false);
	if (World && World->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 300.f), At - FVector(0.f, 0.f, 600.f), ECC_WorldStatic, Query))
	{
		return Hit.ImpactPoint;
	}
	return At;
}

bool ULyudBeach::Seaward(ALyudSea& Sea, const FVector& At, FVector& Out) const
{
	// the way the ground falls fastest over 40 m is the way to the sea
	float Lowest = TNumericLimits<float>::Max();
	bool bFound = false;
	for (int32 I = 0; I < 8; ++I)
	{
		const FVector Dir = FRotator(0.f, I * 45.f, 0.f).Vector();
		bool bOk = false;
		const float Z = Sea.GroundZ(At.X + Dir.X * 4000.0, At.Y + Dir.Y * 4000.0, bOk);
		if (bOk && Z < Lowest)
		{
			Lowest = Z;
			Out = Dir;
			bFound = true;
		}
	}
	return bFound;
}

void ULyudBeach::Say(const FText& Text, float Seconds)
{
	EnsureHud();
	if (Hud.IsValid())
	{
		Hud->Line = Text;
		SayLeft = Seconds;
	}
}

void ULyudBeach::Play(FName Sound, const FVector& At)
{
	if (ULyudSoundscapeSubsystem* Soundscape = ULyudSoundscapeSubsystem::Get(this))
	{
		Soundscape->PlayEffect(Sound, At);
	}
}

void ULyudBeach::Stamp(const TCHAR* Piece)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->GiveStamp(FString(TEXT("beach/")) + Piece);
	}
}

float ULyudBeach::StormNow()
{
	if (StormOverride >= 0.f)
	{
		return FMath::Clamp(StormOverride, 0.f, 1.f);
	}
	const UWorld* World = GetWorld();
	const FDateTime Now = UGameCalendar::NowIn(World);
	const int32 Day = LyudBeachImpl::DayOf(Now);
	if (Day != StormDay)
	{
		// once a day: the strongest wind of the day and a half before this morning
		StormDay = Day;
		StormScore = 0.f;
		if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
		{
			const FDateTime Morning = FDateTime(2000, 1, 1) + FTimespan::FromDays(Day) + FTimespan::FromHours(6.0);
			double Wind[13];
			for (int32 I = 0; I < 13; ++I)
			{
				Wind[I] = Weather->GetStateAt(Morning - FTimespan::FromHours(I * 3.0)).WindSpeed;
			}
			StormScore = static_cast<float>(LyudBeach::StormScore(Wind, 13));
		}
		UE_LOG(LogLyudBeach, Display, TEXT("BEACH day %d storm %.2f"), Day, StormScore);
	}
	return StormScore;
}

// ------------------------------------------------------------------ every frame

void ULyudBeach::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!SeaActor.IsValid() && World)
	{
		SeaActor = ALyudSea::Get(World);
	}
	ALyudSea* Sea = SeaActor.Get();
	if (!Hero || !Sea || !Sea->IsDepthReady())
	{
		return;
	}
	if (SayLeft > 0.f && (SayLeft -= DeltaTime) <= 0.f && Hud.IsValid())
	{
		Hud->Line = FText::GetEmpty();
		Hud->Count = FText::GetEmpty();
	}
	SlowClock -= DeltaTime;
	if (SlowClock <= 0.f)
	{
		const float Dt = 0.25f - SlowClock;
		SlowClock = 0.25f;
		SlowTick(*Hero, *Sea, Dt);
	}
	if (bFlying)
	{
		UpdateStone(*Sea, DeltaTime);
	}
	if (RaceState != static_cast<uint8>(LyudBeach::ERace::Idle) || NearPost >= 0)
	{
		UpdateRace(*Hero, *Sea, DeltaTime);
	}
	// nothing else runs away from the water
	if (Shore.bOnSand || bSwinging || bBuilding || bEDown || SearchLeft >= 0.f)
	{
		UpdateInput(*Hero, *Sea, *PC, DeltaTime);
	}
	// the buoys ride the waves while he is near enough to see it
	if (bPostsPlaced && Props)
	{
		const FVector Eye = Hero->GetActorLocation();
		for (int32 I = 0; I < Posts.Num() && I * 2 + 1 < BuoyMeshes.Num(); ++I)
		{
			if (Posts[I].bPlaced && FVector::DistSquared2D(Eye, Posts[I].Buoy) < FMath::Square(25000.f) && BuoyMeshes[I * 2])
			{
				bool bOk = false;
				const float Ground = Sea->GroundZ(Posts[I].Buoy.X, Posts[I].Buoy.Y, bOk);
				const float Z = FMath::Max(Sea->SurfaceZ(Posts[I].Buoy), bOk ? Ground + 25.f : -1.e9f);
				BuoyMeshes[I * 2]->SetWorldLocation(FVector(Posts[I].Buoy.X, Posts[I].Buoy.Y, Z));
				BuoyMeshes[I * 2 + 1]->SetWorldLocation(FVector(Posts[I].Buoy.X, Posts[I].Buoy.Y, Z + 45.f));
			}
		}
	}
}

void ULyudBeach::SlowTick(AHeroCharacter& Hero, ALyudSea& Sea, float Dt)
{
	MakeProps();
	if (!bPostsPlaced)
	{
		PlacePosts(Sea);
	}
	UpdateShore(Hero, Sea);
	UpdateCastle(Sea, Dt);
	if (Shore.bOnSand)
	{
		UpdateFinds(Sea);
	}
	else if (Finds.Num())
	{
		// away from the water: the waterline's things are put away
		Finds.Reset();
		FindsKey = 0;
		NearFind = -1;
		for (UStaticMeshComponent* Mesh : FindMeshes)
		{
			if (Mesh)
			{
				Mesh->SetVisibility(false);
			}
		}
	}
	// what is within reach
	NearFind = -1;
	float Best = FMath::Square(LyudBeachImpl::ReachCm);
	for (int32 I = 0; I < Finds.Num(); ++I)
	{
		const float D = FVector::DistSquared(Finds[I].At, Shore.Feet);
		if (D < Best)
		{
			Best = D;
			NearFind = I;
		}
	}
	NearPost = -1;
	for (int32 I = 0; I < Posts.Num(); ++I)
	{
		if (Posts[I].bPlaced && FVector::DistSquared(Posts[I].At, Shore.Feet) < FMath::Square(LyudBeachImpl::PostReachCm))
		{
			NearPost = I;
		}
	}
	UpdatePrompt();
}

void ULyudBeach::UpdateShore(AHeroCharacter& Hero, ALyudSea& Sea)
{
	Shore = FShore();
	const UCapsuleComponent* Capsule = Hero.GetCapsuleComponent();
	Shore.Feet = Hero.GetActorLocation() - FVector(0.f, 0.f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f);
	Shore.Toward = FRotator(0.f, Hero.GetControlRotation().Yaw, 0.f).Vector();
	Shore.SeaZ = Sea.SeaLevel();
	Shore.WetZ = FMath::Max(Sea.WetLineZ(), Shore.SeaZ);
	const ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this);
	const UCharacterMovementComponent* Move = Hero.GetCharacterMovement();
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if ((Swim && Swim->IsSwimming()) || !Move || !Move->IsMovingOnGround() || Hero.IsSeated() || Hero.IsOnBike() || Hero.IsLeadingBike()
		|| (Inv && !Inv->EquippedItem().IsNone()) || (UI && UI->IsOpen()))
	{
		return;
	}
	// near the sea's level at all
	if (Shore.Feet.Z < Shore.SeaZ - 60.f || Shore.Feet.Z > Shore.WetZ + 300.f)
	{
		return;
	}
	// and the sea itself within 25 m (a pond in the hills at the same height is not the beach)
	bool bSea = false;
	for (int32 I = 0; I < 8 && !bSea; ++I)
	{
		const FVector Dir = FRotator(0.f, I * 45.f, 0.f).Vector();
		bSea = Sea.DepthAt(Shore.Feet + Dir * 1200.f) > 10.f || Sea.DepthAt(Shore.Feet + Dir * 2500.f) > 10.f;
	}
	if (!bSea)
	{
		return;
	}
	Shore.bOnSand = true;
	Shore.bDamp = LyudBeach::CanBuild(Shore.Feet.Z / 100.0, Shore.SeaZ / 100.0, Shore.WetZ / 100.0);
	Shore.bWaterAhead = Sea.DepthAt(Shore.Feet + Shore.Toward * 500.f) > 15.f || Sea.DepthAt(Shore.Feet + Shore.Toward * 900.f) > 15.f;
}

void ULyudBeach::UpdateFinds(ALyudSea& Sea)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book || !Props)
	{
		return;
	}
	const int32 Day = LyudBeachImpl::DayOf(UGameCalendar::NowIn(GetWorld()));
	if (Book->PickDay != Day)
	{
		Book->PickDay = Day;
		Book->Picked.Reset();
	}
	// the shore runs north-south here: a cell of the waterline is a stretch of Y
	const int32 Mid = FMath::FloorToInt32(Shore.Feet.Y / (LyudBeach::FindCellM * 100.0));
	const int32 Level = FMath::RoundToInt32(Shore.SeaZ / 15.f);
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(Mid), GetTypeHash(Level)), HashCombine(GetTypeHash(Day), GetTypeHash(Book->Picked.Num() + 1)));
	if (Key == FindsKey)
	{
		return;
	}
	FindsKey = Key;
	Finds.Reset();
	const float Storm = StormNow();
	// which way the sea is, along X
	bool bOkA = false, bOkB = false;
	const float ZPlus = Sea.GroundZ(Shore.Feet.X + 3000.0, Shore.Feet.Y, bOkA);
	const float ZMinus = Sea.GroundZ(Shore.Feet.X - 3000.0, Shore.Feet.Y, bOkB);
	const double Sign = (bOkA && bOkB && ZMinus < ZPlus) ? -1.0 : 1.0;
	for (int32 Cell = Mid - LyudBeachImpl::FindCellsEachWay; Cell <= Mid + LyudBeachImpl::FindCellsEachWay && Finds.Num() < LyudBeachImpl::MaxFinds; ++Cell)
	{
		LyudBeach::FFind Find;
		if (Book->Picked.Contains(Cell) || !LyudBeach::FindIn(Cell, Day, Storm, Find))
		{
			continue;
		}
		// walk from the land to the sea along this cell's line until the sand drops to where the find lies
		const double Y = Find.Along * 100.0;
		const float Want = Shore.SeaZ + 3.f + static_cast<float>(Find.Across) * (Shore.WetZ - Shore.SeaZ);
		float Before = 0.f;
		bool bHave = false;
		for (double Step = -6000.0; Step <= 6000.0; Step += 150.0)
		{
			bool bOk = false;
			const double X = Shore.Feet.X + Sign * Step;
			const float Z = Sea.GroundZ(X, Y, bOk);
			if (!bOk)
			{
				bHave = false;
				continue;
			}
			if (bHave && Before >= Want && Z < Want)
			{
				const double Back = (Before - Z) > 0.01f ? (Want - Z) / (Before - Z) * 150.0 : 0.0;
				FShownFind Shown;
				Shown.Cell = Cell;
				Shown.Kind = static_cast<int32>(Find.Kind);
				Shown.At = OnGround(FVector(X - Sign * Back, Y, Want)) + FVector(0.f, 0.f, 1.5f);
				// only on sand the water has left, never under it
				if (Shown.At.Z > Shore.SeaZ)
				{
					Finds.Add(Shown);
				}
				break;
			}
			Before = Z;
			bHave = true;
		}
	}
	for (int32 I = 0; I < FindMeshes.Num(); ++I)
	{
		UStaticMeshComponent* Mesh = FindMeshes[I];
		if (!Mesh)
		{
			continue;
		}
		if (I >= Finds.Num())
		{
			Mesh->SetVisibility(false);
			continue;
		}
		const LyudBeach::EFind Kind = static_cast<LyudBeach::EFind>(Finds[I].Kind);
		const bool bAmber = LyudBeach::IsAmber(Kind);
		Mesh->SetWorldLocation(Finds[I].At);
		Mesh->SetWorldRotation(FRotator(0.f, static_cast<float>(Finds[I].Cell * 67 % 360), 0.f));
		Mesh->SetWorldScale3D(bAmber ? FVector(0.035f, 0.028f, 0.02f)
			: Kind == LyudBeach::EFind::Whelk ? FVector(0.08f, 0.045f, 0.04f)
			: Kind == LyudBeach::EFind::Mussel ? FVector(0.075f, 0.035f, 0.015f) : FVector(0.055f, 0.05f, 0.018f));
		Mesh->SetMaterial(0, TintOf(bAmber ? FLinearColor(0.8f, 0.42f, 0.05f) : Kind == LyudBeach::EFind::Mussel ? FLinearColor(0.08f, 0.09f, 0.14f)
			: Kind == LyudBeach::EFind::Tellin ? FLinearColor(0.85f, 0.6f, 0.58f) : FLinearColor(0.85f, 0.8f, 0.7f)));
		Mesh->SetVisibility(true);
	}
}

void ULyudBeach::UpdateCastle(ALyudSea& Sea, float Dt)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	UWorld* World = GetWorld();
	if (!Book || !World)
	{
		return;
	}
	if (Book->bCastle)
	{
		LyudBeach::FCastle Castle = LyudBeachImpl::CastleOf(*Book);
		const FDateTime Now = UGameCalendar::NowIn(World);
		const double Away = (Now - Book->CastleSeen).GetTotalSeconds();
		bool bGone = false;
		if (Away > 120.0)
		{
			// he slept or the game was closed: the tides of the hours between, ten minutes at a time
			double Lon = 20.40, Offset = 2.0;
			if (TActorIterator<ALyudSky> It(World); It)
			{
				Lon = It->LongitudeDeg;
				Offset = It->UtcOffsetHours;
			}
			const double Span = FMath::Min(Away, 4.0 * 86400.0);
			for (double T = 0.0; T < Span && !bGone; T += 600.0)
			{
				const FDateTime Utc = Book->CastleSeen + FTimespan::FromSeconds(T) - FTimespan::FromHours(Offset);
				bGone = LyudBeach::Wash(Castle, Sea.DatumZ() / 100.0 + LyudTide::At(Utc, Lon).LevelM, 0.2, false, 600.0);
			}
		}
		else if (Away > 0.0)
		{
			bool bRain = false;
			if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this))
			{
				bRain = Weather->GetState().PrecipRate > 1.f;
			}
			// on the sand the open sea's waves are a fifth of themselves
			const double Wave = FMath::Clamp(Sea.WaveHeight() / 100.0 * 0.2, 0.05, 0.5);
			bGone = LyudBeach::Wash(Castle, Sea.SeaLevel() / 100.0, Wave, bRain, Away);
		}
		Book->CastleSeen = Now;
		LyudBeachImpl::KeepCastle(*Book, Castle);
		if (bGone)
		{
			Book->CastlesWashed++;
			UE_LOG(LogLyudBeach, Display, TEXT("BEACH castle washed away (work %.1f)"), Book->CastleWork);
			if (Book->CastleWork >= 4.f)
			{
				if (ULyudQuests* Quests = ULyudQuests::Get(this))
				{
					Quests->AddNote(TEXT("journal.beach.castle_washed"));
				}
				Stamp(TEXT("castle_washed"));
				ULyudQuests::Notify(this, TEXT("beach"), TEXT("castle_washed"));
				if (FVector::DistSquared(Book->CastleAt, Shore.Feet) < FMath::Square(1500.f))
				{
					Say(LyudText::Get(TEXT("beach.castle.washed")));
				}
			}
		}
	}
	ShowCastle();
}

void ULyudBeach::ShowCastle()
{
	const ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book || !CastleRoot)
	{
		return;
	}
	const LyudBeach::FCastle Castle = LyudBeachImpl::CastleOf(*Book);
	const int32 Stage = Castle.bAlive ? Castle.Stage() : -1;
	const float Standing = static_cast<float>(Castle.Standing());
	// touched only when what shows has changed (a stage, or a tenth of the wear)
	if (Stage == ShownStage && FMath::Abs(Standing - ShownStanding) < 0.04f)
	{
		return;
	}
	ShownStage = Stage;
	ShownStanding = Standing;
	if (Stage >= 0)
	{
		CastleRoot->SetWorldLocation(Book->CastleAt);
		// the water rounds it off: lower and a little wider
		CastleRoot->SetWorldScale3D(FVector(1.f + (1.f - Standing) * 0.25f, 1.f + (1.f - Standing) * 0.25f, FMath::Max(0.05f, Standing)));
	}
	for (int32 I = 0; I < CastleMeshes.Num(); ++I)
	{
		const int32 From = I == 0 ? 5 : I == 1 ? 0 : I == 2 ? 2 : I <= 10 ? 3 : I <= 14 ? 4 : 6;
		// the first heap shows as soon as the castle is begun; the flag falls at the first wear
		const bool bShow = Stage >= From && (I < 15 || Standing > 0.8f);
		if (CastleMeshes[I])
		{
			CastleMeshes[I]->SetVisibility(bShow);
		}
	}
}

void ULyudBeach::PlacePosts(ALyudSea& Sea)
{
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (!Things || !Props)
	{
		return;
	}
	TArray<FCityThingRef> Masts;
	Things->GetThingsByAct([](const FString& Act) { return Act.StartsWith(TEXT("lg_flagstate")); }, Masts);
	if (!Masts.Num())
	{
		return;     // the town's things are still loading
	}
	bPostsPlaced = true;
	for (int32 I = 0; I < Masts.Num() && I * 2 + 1 < PostMeshes.Num(); ++I)
	{
		FPost Post;
		FVector Dir;
		if (!Seaward(Sea, Masts[I].Location, Dir))
		{
			Posts.Add(Post);
			continue;
		}
		Post.At = OnGround(Masts[I].Location + Dir * 900.f);
		// the buoy: out to where the highest water is two and a half metres deep, and ten metres more
		double Out = 2000.0;
		for (; Out < 16000.0; Out += 250.0)
		{
			bool bOk = false;
			const float Z = Sea.GroundZ(Post.At.X + Dir.X * Out, Post.At.Y + Dir.Y * Out, bOk);
			if (bOk && Sea.DatumZ() - Z > 250.f)
			{
				break;
			}
		}
		Out += 1000.0;
		Post.Buoy = FVector(Post.At.X + Dir.X * Out, Post.At.Y + Dir.Y * Out, Sea.DatumZ());
		Post.bPlaced = true;
		Posts.Add(Post);
		if (PostMeshes[I * 2] && PostMeshes[I * 2 + 1])
		{
			PostMeshes[I * 2]->SetWorldLocation(Post.At + FVector(0.f, 0.f, 65.f));
			PostMeshes[I * 2 + 1]->SetWorldLocation(Post.At + FVector(0.f, 0.f, 120.f));
			PostMeshes[I * 2 + 1]->SetWorldRotation(Dir.Rotation());
			PostMeshes[I * 2]->SetVisibility(true);
			PostMeshes[I * 2 + 1]->SetVisibility(true);
		}
		if (BuoyMeshes[I * 2] && BuoyMeshes[I * 2 + 1])
		{
			BuoyMeshes[I * 2]->SetWorldLocation(Post.Buoy);
			BuoyMeshes[I * 2 + 1]->SetWorldLocation(Post.Buoy + FVector(0.f, 0.f, 45.f));
			BuoyMeshes[I * 2]->SetVisibility(true);
			BuoyMeshes[I * 2 + 1]->SetVisibility(true);
		}
		UE_LOG(LogLyudBeach, Display, TEXT("BEACH post %d at %s, buoy %.0f m out at %s"), I, *Post.At.ToCompactString(), Out / 100.0, *Post.Buoy.ToCompactString());
	}
}

// ------------------------------------------------------------------ the swim

void ULyudBeach::UpdateRace(AHeroCharacter& Hero, ALyudSea& Sea, float Dt)
{
	using namespace LyudBeach;
	if (RaceState == static_cast<uint8>(ERace::Idle) || !Posts.IsValidIndex(RacePost))
	{
		return;
	}
	FRace Race;
	Race.State = static_cast<ERace>(RaceState);
	Race.Seconds = RaceSeconds;
	Race.ReadyFor = RaceReadyFor;
	Race.OutSeconds = RaceOut;
	Race.CourseM = RaceCourse;
	FRaceIn In;
	const ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this);
	const UCharacterMovementComponent* Move = Hero.GetCharacterMovement();
	In.bSwimming = Swim && Swim->IsSwimming();
	In.bAshore = !In.bSwimming && Move && Move->IsMovingOnGround();
	In.ToBuoyM = FVector::Dist2D(Hero.GetActorLocation(), Posts[RacePost].Buoy) / 100.0;
	const ERaceEvent Event = RaceStep(Race, In, Dt);
	RaceState = static_cast<uint8>(Race.State);
	RaceSeconds = static_cast<float>(Race.Seconds);
	RaceReadyFor = static_cast<float>(Race.ReadyFor);
	RaceOut = static_cast<float>(Race.OutSeconds);
	EnsureHud();
	if (Hud.IsValid() && (Race.State == ERace::Out || Race.State == ERace::Back))
	{
		Hud->Clock = LyudBeachImpl::Seconds(RaceSeconds);
	}
	switch (Event)
	{
	case ERaceEvent::Started:
		Say(LyudText::Get(TEXT("beach.race.go")), 4.f);
		break;
	case ERaceEvent::Turned:
		Play(TEXT("fish_splash"), Posts[RacePost].Buoy);
		Say(LyudText::Get(TEXT("beach.race.turn")), 4.f);
		break;
	case ERaceEvent::GaveUp:
		Say(LyudText::Get(TEXT("beach.race.gave_up")));
		if (Hud.IsValid())
		{
			Hud->Clock = FText::GetEmpty();
		}
		RaceState = static_cast<uint8>(ERace::Idle);
		break;
	case ERaceEvent::Finished:
	{
		Play(TEXT("whistle_short"), Posts[RacePost].At);
		ULyudBeachBook* Book = ULyudBeachBook::Get(this);
		const int32 Medal = LyudBeach::Medal(Race.Seconds, Race.CourseM);
		const bool bBeatGuard = Race.Seconds < LifeguardSeconds(Race.CourseM);
		bool bBest = false;
		if (Book)
		{
			FBook Model = LyudBeachImpl::ToModel(*Book);
			bBest = NoteSwim(Model, Race.Seconds).bBest;
			LyudBeachImpl::FromModel(*Book, Model);
		}
		static const TCHAR* Lines[4] = { TEXT("beach.race.done.0"), TEXT("beach.race.done.1"), TEXT("beach.race.done.2"), TEXT("beach.race.done.3") };
		Say(LyudText::Get(bBeatGuard ? TEXT("beach.race.beat_guard") : Lines[Medal], { FStringFormatArg(LyudBeachImpl::Seconds(RaceSeconds).ToString()) }), 9.f);
		Stamp(TEXT("swim"));
		ULyudQuests::Notify(this, TEXT("beach"), TEXT("swim"));
		if (Medal >= 3)
		{
			Stamp(TEXT("swim_gold"));
			ULyudQuests::Notify(this, TEXT("beach"), TEXT("swim_gold"));
		}
		if (bBest)
		{
			if (ULyudQuests* Quests = ULyudQuests::Get(this))
			{
				Quests->AddNote(TEXT("journal.beach.swim"), { LyudBeachImpl::Seconds(RaceSeconds).ToString() });
			}
		}
		UE_LOG(LogLyudBeach, Display, TEXT("BEACH swim %.1f s over 2 x %.0f m, medal %d, out %.1f s"), RaceSeconds, RaceCourse, Medal, RaceOut);
		RaceState = static_cast<uint8>(ERace::Idle);
		break;
	}
	default:
		break;
	}
}

bool ULyudBeach::RaceNow()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ALyudSea* Sea = SeaActor.Get();
	if (!PC || !PC->GetPawn() || !Sea)
	{
		return false;
	}
	int32 Best = NearPost;
	if (Best < 0)
	{
		float BestD = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Posts.Num(); ++I)
		{
			const float D = FVector::DistSquared2D(Posts[I].At, PC->GetPawn()->GetActorLocation());
			if (Posts[I].bPlaced && D < BestD)
			{
				BestD = D;
				Best = I;
			}
		}
	}
	if (!Posts.IsValidIndex(Best))
	{
		return false;
	}
	if (Sea->DepthAt(Posts[Best].Buoy) < 130.f)
	{
		// low water: the buoy lies on the sand
		Say(LyudText::Get(TEXT("beach.race.no_water")));
		return false;
	}
	LyudBeach::FRace Race;
	LyudBeach::RaceReady(Race, FVector::Dist2D(Posts[Best].At, Posts[Best].Buoy) / 100.0);
	RacePost = Best;
	RaceState = static_cast<uint8>(Race.State);
	RaceSeconds = RaceReadyFor = RaceOut = 0.f;
	RaceCourse = static_cast<float>(Race.CourseM);
	Play(TEXT("whistle_short"), Posts[Best].At);
	Say(LyudText::Get(TEXT("beach.race.ready"), { FStringFormatArg(FMath::RoundToInt32(RaceCourse)),
		FStringFormatArg(LyudBeachImpl::Seconds(static_cast<float>(LyudBeach::LifeguardSeconds(RaceCourse))).ToString()) }), 9.f);
	return true;
}

// ------------------------------------------------------------------ stones

void ULyudBeach::GiveStone(float Flatness)
{
	if (ULyudBeachBook* Book = ULyudBeachBook::Get(this))
	{
		if (Book->Stones.Num() < LyudBeachImpl::MaxStones)
		{
			Book->Stones.Add(FMath::Clamp(Flatness, 0.f, 1.f));
		}
	}
}

void ULyudBeach::Search()
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book)
	{
		return;
	}
	if (Book->Stones.Num() >= LyudBeachImpl::MaxStones)
	{
		Say(LyudText::Get(TEXT("beach.search.full")));
		return;
	}
	const int32 Day = LyudBeachImpl::DayOf(UGameCalendar::NowIn(GetWorld()));
	if (Day != TriesDay)
	{
		TriesDay = Day;
		Tries.Reset();
	}
	// a spot is three metres of shingle; seven stones and it is picked over for the day
	const int32 CellX = FMath::FloorToInt32(Shore.Feet.X / 300.f), CellY = FMath::FloorToInt32(Shore.Feet.Y / 300.f);
	int32& Try = Tries.FindOrAdd((static_cast<uint64>(static_cast<uint32>(CellX)) << 32) | static_cast<uint32>(CellY));
	if (Try >= 7)
	{
		Say(LyudText::Get(TEXT("beach.search.empty")));
		return;
	}
	const double Flat = LyudBeach::StoneFlatness(CellX, CellY, Day, Try);
	Try++;
	Play(TEXT("rest_clink"), Shore.Feet);
	if (LyudBeach::IsFlat(Flat))
	{
		Book->Stones.Add(static_cast<float>(Flat));
		Say(LyudText::Get(Flat > 0.9 ? TEXT("beach.search.great") : TEXT("beach.search.flat"), { FStringFormatArg(Book->Stones.Num()) }));
	}
	else
	{
		static const TCHAR* Lines[4] = { TEXT("beach.search.round.1"), TEXT("beach.search.round.2"), TEXT("beach.search.round.3"), TEXT("beach.search.round.4") };
		Say(LyudText::Get(Lines[LyudBeach::Hash(static_cast<uint32>(CellX), static_cast<uint32>(CellY), static_cast<uint32>(Try)) % 4]), 3.f);
	}
}

bool ULyudBeach::ThrowNow(float Quality)
{
	ALyudSea* Sea = SeaActor.Get();
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Sea || !Book || bFlying)
	{
		return false;
	}
	if (!Book->Stones.Num())
	{
		Book->Stones.Add(0.9f);
	}
	Throw(Quality, *Sea);
	return true;
}

void ULyudBeach::Throw(float Quality, ALyudSea& Sea)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book || !Book->Stones.Num() || bFlying)
	{
		return;
	}
	// the flattest stone first
	int32 Pick = 0;
	for (int32 I = 1; I < Book->Stones.Num(); ++I)
	{
		Pick = Book->Stones[I] > Book->Stones[Pick] ? I : Pick;
	}
	LyudBeach::FThrowIn In;
	In.Quality = Quality;
	In.Flatness = Book->Stones[Pick];
	Book->Stones.RemoveAtSwap(Pick);
	// the water where it will land: how deep, and how rough right there (the surface against the still level)
	float Rough = 0.f, Deep = 0.f;
	for (const float Out : { 500.f, 800.f, 1200.f, 1600.f })
	{
		const FVector P = Shore.Feet + Shore.Toward * Out;
		Rough = FMath::Max(Rough, FMath::Abs(Sea.SurfaceZ(P) - Sea.SeaLevel()));
		Deep = FMath::Max(Deep, Sea.DepthAt(P));
	}
	In.WaveM = Rough * 2.f / 100.f;
	In.DepthM = Deep / 100.f;
	In.Seed = static_cast<uint32>(Book->Throws * 7919 + 13);
	const LyudBeach::FThrowOut Flight = LyudBeach::Throw(In);
	FlightHops.Reset();
	for (const LyudBeach::FHop& Hop : Flight.Hops)
	{
		FlightHops.Add(FVector4f(static_cast<float>(Hop.X), static_cast<float>(Hop.T), static_cast<float>(Hop.Height), static_cast<float>(Hop.SpeedOut)));
	}
	FlightSkips = Flight.Skips;
	FlightFrom = Shore.Feet + FVector(0.f, 0.f, 100.f) + Shore.Toward * 40.f;
	FlightDir = Shore.Toward;
	FlightT = 0.f;
	FlightTouch = 0;
	bFlying = true;
	EnsureHud();
	if (Hud.IsValid())
	{
		Hud->Count = FText::GetEmpty();
		Hud->Line = FText::GetEmpty();
	}
	if (StoneMesh)
	{
		StoneMesh->SetWorldLocation(FlightFrom);
		StoneMesh->SetVisibility(true);
	}
	Play(TEXT("swim_stroke"), FlightFrom);
	UE_LOG(LogLyudBeach, Display, TEXT("BEACH throw quality %.2f flat %.2f waves %.2f m depth %.2f m -> %d skips, %.1f m"), In.Quality, In.Flatness, In.WaveM,
		In.DepthM, Flight.Skips, Flight.Distance);
}

void ULyudBeach::UpdateStone(ALyudSea& Sea, float Dt)
{
	FlightT += Dt;
	double Along = 0.0;
	const double Height = LyudBeachImpl::StoneAt(FlightHops, FlightT, Along);
	FVector P = FlightFrom + FlightDir * static_cast<float>(Along * 100.0);
	P.Z = Sea.SurfaceZ(P) + static_cast<float>(Height * 100.0) + 2.f;
	if (StoneMesh)
	{
		StoneMesh->SetWorldLocation(P);
		StoneMesh->SetWorldRotation(FRotator(8.f, FlightT * 1400.f, 0.f));
	}
	// every touch passed since the last frame: a splash and the count
	while (FlightTouch < FlightHops.Num() && FlightT >= FlightHops[FlightTouch].Y)
	{
		FVector At = FlightFrom + FlightDir * (FlightHops[FlightTouch].X * 100.f);
		At.Z = Sea.SurfaceZ(At);
		const bool bLast = FlightTouch == FlightHops.Num() - 1;
		Play(bLast ? TEXT("fish_splash") : TEXT("swim_stroke"), At);
		if (!bLast && Hud.IsValid())
		{
			Hud->Count = FText::AsNumber(FlightTouch + 1);
			SayLeft = FMath::Max(SayLeft, 3.f);
		}
		FlightTouch++;
	}
	if (FlightTouch >= FlightHops.Num())
	{
		bFlying = false;
		if (StoneMesh)
		{
			StoneMesh->SetVisibility(false);
		}
		Finish(FlightSkips);
	}
}

void ULyudBeach::Finish(int32 Skips)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book)
	{
		return;
	}
	LyudBeach::FBook Model = LyudBeachImpl::ToModel(*Book);
	const LyudBeach::FNews News = LyudBeach::NoteThrow(Model, Skips);
	LyudBeachImpl::FromModel(*Book, Model);
	const TCHAR* Key = News.bTownRecord ? TEXT("beach.throw.record") : News.bBest ? TEXT("beach.throw.best")
		: Skips == 0 ? TEXT("beach.throw.none") : Skips < 3 ? TEXT("beach.throw.few") : TEXT("beach.throw.good");
	Say(LyudText::Get(Key, { FStringFormatArg(Skips), FStringFormatArg(LyudBeach::TownRecord) }), 6.f);
	if (Hud.IsValid())
	{
		Hud->Count = Skips > 0 ? FText::AsNumber(Skips) : FText::GetEmpty();
	}
	ULyudQuests* Quests = ULyudQuests::Get(this);
	if (Skips >= 3)
	{
		Stamp(TEXT("skip3"));
		ULyudQuests::Notify(this, TEXT("beach"), TEXT("skip3"));
	}
	if (Skips >= 5)
	{
		Stamp(TEXT("skip5"));
		ULyudQuests::Notify(this, TEXT("beach"), TEXT("skip5"));
	}
	if (Skips >= 8)
	{
		Stamp(TEXT("skip8"));
	}
	if (News.bTownRecord)
	{
		Stamp(TEXT("skip_record"));
		ULyudQuests::Notify(this, TEXT("beach"), TEXT("skip_record"));
		if (Quests)
		{
			Quests->AddNote(TEXT("journal.beach.record"), { FString::FromInt(Skips) });
		}
	}
	else if (News.bBest && Skips >= 5 && Quests)
	{
		Quests->AddNote(TEXT("journal.beach.skips"), { FString::FromInt(Skips) });
	}
}

// ------------------------------------------------------------------ the castle and the finds

bool ULyudBeach::CastleHere(float Work)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	UWorld* World = GetWorld();
	if (!Book || !World)
	{
		return false;
	}
	Book->bCastle = true;
	Book->CastleAt = OnGround(Shore.Feet + Shore.Toward * 110.f);
	Book->CastleWork = FMath::Clamp(Work, 0.f, static_cast<float>(LyudBeach::CastleStages));
	Book->CastleWorn = 0.f;
	Book->CastleBegun = Book->CastleSeen = UGameCalendar::NowIn(World);
	ShownStage = -1;
	ShowCastle();
	return true;
}

void ULyudBeach::BeginCastle(ALyudSea& Sea)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book)
	{
		return;
	}
	const bool bOld = Book->bCastle;
	CastleHere(0.f);
	if (bOld)
	{
		Say(LyudText::Get(TEXT("beach.castle.again")), 4.f);
	}
}

void ULyudBeach::Pick(int32 Index)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book || !Finds.IsValidIndex(Index))
	{
		return;
	}
	const FShownFind Found = Finds[Index];
	const LyudBeach::EFind Kind = static_cast<LyudBeach::EFind>(Found.Kind);
	const FString Id = UTF8_TO_TCHAR(LyudBeach::FindId(Kind));
	const bool bAmber = LyudBeach::IsAmber(Kind);
	Book->Picked.AddUnique(Found.Cell);
	LyudBeach::FBook Model = LyudBeachImpl::ToModel(*Book);
	const LyudBeach::FNews News = LyudBeach::NoteFind(Model, Kind);
	LyudBeachImpl::FromModel(*Book, Model);
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		Inv->Add(bAmber ? FName(TEXT("amber_piece")) : FName(TEXT("seashell")));
	}
	Play(TEXT("rest_clink"), Found.At);
	Say(LyudText::Get(TEXT("beach.picked.") + Id), News.bFirstOfKind ? 6.f : 3.f);
	if (News.bFirstOfKind)
	{
		Stamp(*Id);
		if (ULyudQuests* Quests = ULyudQuests::Get(this))
		{
			Quests->AddNote(TEXT("journal.beach.find.") + Id);
		}
	}
	ULyudQuests::Notify(this, TEXT("beach"), bAmber ? TEXT("amber") : TEXT("shell"));
	if (Book->Ambers >= 3)
	{
		// three pieces: the souvenir seller will look at them (the quest "The amber shore")
		Stamp(TEXT("amber3"));
	}
	UE_LOG(LogLyudBeach, Display, TEXT("BEACH picked %s (cell %d; shells %d, amber %d)"), *Id, Found.Cell, Book->Shells, Book->Ambers);
	FindsKey = 0;     // the waterline is laid out again without it
}

// ------------------------------------------------------------------ the keys

void ULyudBeach::UpdateInput(AHeroCharacter& Hero, ALyudSea& Sea, APlayerController& PC, float Dt)
{
	ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!Book || PC.IsMoveInputIgnored())
	{
		bEDown = bBuilding = bSwinging = false;
		if (Hud.IsValid())
		{
			Hud->Swing = -1.f;
		}
		return;
	}
	const bool bCanAct = Shore.bOnSand;
	const float ToCastle = Book->bCastle ? FVector::Dist(Book->CastleAt, Shore.Feet) : 1.e9f;
	const bool bAtCastle = Book->bCastle && ToCastle < LyudBeachImpl::CastleReachCm && Book->CastleWork < LyudBeach::CastleStages;

	// --- the throw: G held swings the arm, letting go throws
	if (bSwinging)
	{
		SwingHeld += Dt;
		const float Swing = static_cast<float>(LyudBeach::SwingAt(SwingHeld));
		if (Hud.IsValid())
		{
			Hud->Swing = Swing;
		}
		if (!PC.IsInputKeyDown(EKeys::G))
		{
			bSwinging = false;
			if (Hud.IsValid())
			{
				Hud->Swing = -1.f;
			}
			if (bCanAct && Shore.bWaterAhead)
			{
				Throw(static_cast<float>(LyudBeach::ReleaseQuality(Swing)), Sea);
			}
		}
	}
	else if (bCanAct && Shore.bWaterAhead && !bFlying && Book->Stones.Num() > 0 && PC.WasInputKeyJustPressed(EKeys::G))
	{
		bSwinging = true;
		SwingHeld = 0.f;
		EnsureHud();
	}

	// --- E: a find or the swim's post at once; else a tap looks for a stone and a hold builds
	if (SearchLeft >= 0.f)
	{
		// bent over the shingle for a moment
		SearchLeft -= Dt;
		if (SearchLeft < 0.f && bCanAct)
		{
			Search();
		}
		return;
	}
	if (PC.WasInputKeyJustPressed(EKeys::E) && bCanAct)
	{
		if (NearFind >= 0)
		{
			Pick(NearFind);
			return;
		}
		if (NearPost >= 0 && RaceState == static_cast<uint8>(LyudBeach::ERace::Idle))
		{
			RaceNow();
			return;
		}
		bEDown = true;
		EHeld = 0.f;
	}
	if (bEDown)
	{
		const bool bDown = PC.IsInputKeyDown(EKeys::E);
		EHeld += Dt;
		if (bDown && EHeld >= LyudBeachImpl::HoldToBuild && !bBuilding && bCanAct && (bAtCastle || Shore.bDamp))
		{
			bBuilding = true;
			if (!bAtCastle)
			{
				BeginCastle(Sea);
			}
		}
		if (bBuilding && bDown && bCanAct && Book->bCastle && FVector::Dist(Book->CastleAt, Shore.Feet) < LyudBeachImpl::CastleReachCm + 60.f)
		{
			LyudBeach::FCastle Castle = LyudBeachImpl::CastleOf(*Book);
			if (LyudBeach::Build(Castle, Dt))
			{
				const int32 Stage = Castle.Stage();
				Play(TEXT("rest_plate"), Book->CastleAt);
				static const TCHAR* Lines[7] = { TEXT(""), TEXT("beach.castle.stage.1"), TEXT("beach.castle.stage.2"), TEXT("beach.castle.stage.3"),
					TEXT("beach.castle.stage.4"), TEXT("beach.castle.stage.5"), TEXT("beach.castle.stage.6") };
				Say(LyudText::Get(Lines[FMath::Clamp(Stage, 1, 6)]), 4.f);
				if (Stage == 4)
				{
					Stamp(TEXT("castle"));
					ULyudQuests::Notify(this, TEXT("beach"), TEXT("castle"));
				}
				if (Stage == LyudBeach::CastleStages)
				{
					Book->CastlesBuilt++;
					Stamp(TEXT("castle_flag"));
					// when will the tide be here: the level of the coming hours against the sand it stands on
					double Lon = 20.40, Offset = 2.0;
					if (TActorIterator<ALyudSky> It(GetWorld()); It)
					{
						Lon = It->LongitudeDeg;
						Offset = It->UtcOffsetHours;
					}
					const FDateTime Utc = UGameCalendar::NowIn(GetWorld()) - FTimespan::FromHours(Offset);
					const double Datum = Sea.DatumZ() / 100.0;
					const double Hours = LyudBeach::HoursUntilWash(Castle.Z, 0.2, [&](double H) { return Datum + LyudTide::At(Utc + FTimespan::FromHours(H), Lon).LevelM; });
					if (ULyudQuests* Quests = ULyudQuests::Get(this))
					{
						Quests->AddNote(TEXT("journal.beach.castle"));
					}
					Say(Hours < 0.0 ? LyudText::Get(TEXT("beach.castle.safe"))
						: LyudText::Get(TEXT("beach.castle.tide"), { FStringFormatArg(FMath::Max(1, FMath::RoundToInt32(Hours))) }), 8.f);
				}
			}
			LyudBeachImpl::KeepCastle(*Book, Castle);
			Book->CastleSeen = UGameCalendar::NowIn(GetWorld());
			ShowCastle();
		}
		if (!bDown)
		{
			// let go early: that was a tap
			if (!bBuilding && EHeld < LyudBeachImpl::HoldToBuild && bCanAct && !bAtCastle)
			{
				SearchLeft = 0.9f;
			}
			bEDown = false;
			bBuilding = false;
		}
	}
}

void ULyudBeach::UpdatePrompt()
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	const ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	if (!UI || !Book)
	{
		return;
	}
	FText Text;
	if (Shore.bOnSand && !bFlying && !bSwinging && SearchLeft < 0.f)
	{
		const int32 Stones = Book->Stones.Num();
		const bool bAtCastle = Book->bCastle && Book->CastleWork < LyudBeach::CastleStages
			&& FVector::Dist(Book->CastleAt, Shore.Feet) < LyudBeachImpl::CastleReachCm;
		if (NearFind >= 0 && Finds.IsValidIndex(NearFind))
		{
			const FString Id = UTF8_TO_TCHAR(LyudBeach::FindId(static_cast<LyudBeach::EFind>(Finds[NearFind].Kind)));
			Text = LyudText::Get(TEXT("beach.prompt.find"), { FStringFormatArg(LyudText::Get(TEXT("beach.find.") + Id).ToString()) });
		}
		else if (NearPost >= 0 && RaceState == static_cast<uint8>(LyudBeach::ERace::Idle))
		{
			Text = LyudText::Get(TEXT("beach.prompt.post"));
		}
		else if (bAtCastle)
		{
			Text = LyudText::Get(TEXT("beach.prompt.castle"));
		}
		else if (Shore.bDamp)
		{
			Text = Stones > 0 && Shore.bWaterAhead ? LyudText::Get(TEXT("beach.prompt.sand_throw"), { FStringFormatArg(Stones) }) : LyudText::Get(TEXT("beach.prompt.sand"));
		}
		else if (Stones > 0 && Shore.bWaterAhead)
		{
			Text = LyudText::Get(TEXT("beach.prompt.throw"), { FStringFormatArg(Stones) });
		}
	}
	if (!Text.EqualTo(LastPrompt))
	{
		UI->SetPrompt(this, Text);
		LastPrompt = Text;
	}
}

FString ULyudBeach::Report() const
{
	const ULyudBeachBook* Book = ULyudBeachBook::Get(this);
	FString Out = FString::Printf(TEXT("shore: sand %d damp %d water ahead %d, feet z %.0f, sea %.0f, wet line %.0f; storm %.2f; finds shown %d (near %d); posts %d (near %d); race %d %.1f s"),
		Shore.bOnSand ? 1 : 0, Shore.bDamp ? 1 : 0, Shore.bWaterAhead ? 1 : 0, Shore.Feet.Z, Shore.SeaZ, Shore.WetZ, StormOverride >= 0.f ? StormOverride : StormScore,
		Finds.Num(), NearFind, Posts.Num(), NearPost, static_cast<int32>(RaceState), RaceSeconds);
	if (Book)
	{
		Out += FString::Printf(TEXT("; book: best %d skips of %d throws, stones %d, swim best %.1f s of %d, castle %d work %.1f worn %.2f (built %d, washed %d), shells %d amber %d"),
			Book->BestSkips, Book->Throws, Book->Stones.Num(), Book->BestSwim, Book->Swims, Book->bCastle ? 1 : 0, Book->CastleWork, Book->CastleWorn,
			Book->CastlesBuilt, Book->CastlesWashed, Book->Shells, Book->Ambers);
	}
	return Out;
}
