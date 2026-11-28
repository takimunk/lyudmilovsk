// Wind tell-tales: the windsocks and pennants that show the local wind (see Public/Weather/LyudWindsock.h, docs/wind.md).
#include "Weather/LyudWindsock.h"

#include "Camera/PlayerCameraManager.h"
#include "City/LyudCityThings.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Festival/LyudFestival.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sea/LyudSea.h"
#include "../Transport/LyudMeshKit.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarWindTells(TEXT("lyud.Wind.Tells"), 1, TEXT("lyud.Wind.Tells 0|1: the windsocks and pennants that show the local wind"));

	constexpr float TellNearCm = 25000.f;        // moved every frame within 250 m of the camera
	constexpr float TellFarCm = 60000.f;         // out to 600 m four times a second; beyond that hidden
	constexpr float TellFarStepS = 0.25f;
	constexpr float TellSampleStepS = 0.1f;      // the near ones ask the wind ten times a second and ease between the answers

	// The sock: five bands of 48 cm, 50 cm across at the mouth and half that at the tail.
	constexpr int32 SockBands = 5;
	constexpr float SockBandCm = 48.f;
	constexpr float SockMouthRadiusCm = 25.f;
	constexpr float SockTaper = 0.871f;      // each band's far end against its near one: 0.5 ^ (1 / 5)
	// The pennant: six pieces of 30 cm, 28 cm deep at the staff and 6 cm at the tip.
	constexpr int32 PennantPieces = 6;
	constexpr float PennantPieceCm = 30.f;
	constexpr float PennantHalfCm = 14.f;
	constexpr float PennantTaper = 0.774f;   // (3 / 14) ^ (1 / 6)

	const FLinearColor TellPoleTint(0.74f, 0.74f, 0.70f);
	const FLinearColor TellWhite(0.92f, 0.92f, 0.88f);
	/** The sock's two bands; the pennant's cloth (A) and its tip (B). */
	struct FTellColours { FLinearColor A, B; };
	const FTellColours SockColours[] = {
		{ FLinearColor(0.80f, 0.08f, 0.06f), TellWhite },
		{ FLinearColor(0.95f, 0.40f, 0.05f), TellWhite },
	};
	const FTellColours PennantColours[] = {
		{ FLinearColor(0.80f, 0.08f, 0.06f), TellWhite },
		{ FLinearColor(0.08f, 0.26f, 0.72f), TellWhite },
		{ FLinearColor(0.95f, 0.78f, 0.12f), FLinearColor(0.08f, 0.26f, 0.72f) },
		{ TellWhite, FLinearColor(0.80f, 0.08f, 0.06f) },
		{ FLinearColor(0.10f, 0.52f, 0.22f), TellWhite },
	};
	constexpr int32 NumSockColours = UE_ARRAY_COUNT(SockColours);
	constexpr int32 NumPennantColours = UE_ARRAY_COUNT(PennantColours);

	void TellNoLumen(UPrimitiveComponent* C)
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetMobility(EComponentMobility::Movable);
		// it moves every frame: out of the Lumen scene and distance fields (the perf checklist)
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetCanEverAffectNavigation(false);
	}

	/** Shared colours: one rooted material instance per colour for every tell-tale (never freed: a handful), as the kites'. */
	UMaterialInstanceDynamic* TellPaint(const FLinearColor& Tint)
	{
		static TMap<FLinearColor, UMaterialInstanceDynamic*> Cache;
		if (UMaterialInstanceDynamic** Found = Cache.Find(Tint))
		{
			return *Found;
		}
		static UMaterialInterface* Basic = nullptr;
		if (!Basic)
		{
			Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (!Basic)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basic, GetTransientPackage());
		M->SetVectorParameterValue(TEXT("Color"), Tint);
		M->AddToRoot();
		Cache.Add(Tint, M);
		return M;
	}

	struct FTellBuilt
	{
		UStaticMesh* Mesh = nullptr;
		TArray<FName> Slots;
	};

	FTellBuilt TellKeep(const FLyudMeshKit& K, const TCHAR* Name)
	{
		FTellBuilt B;
		B.Mesh = K.Build(GetTransientPackage(), FName(Name), B.Slots);
		if (B.Mesh)
		{
			B.Mesh->AddToRoot();
		}
		return B;
	}

	/** Both sides of a piece of cloth (the material is one-sided, and a sock is seen from inside through its mouth). */
	void TellCloth(FLyudMeshKit& K, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		K.Quad(TEXT("Cloth"), A, B, C, D);
		K.Quad(TEXT("Cloth"), D, C, B, A);
	}

	// Frames (cm): X along the piece, away from where it is tied on; every piece is this one mesh, scaled.

	/** One band of the sock: an open tube 100 long, radius 100 at the near end narrowing by SockTaper. */
	const FTellBuilt& TellSockMesh()
	{
		static FTellBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			constexpr int32 Sides = 12;
			for (int32 J = 0; J < Sides; ++J)
			{
				const float A0 = 2.f * UE_PI * J / Sides, A1 = 2.f * UE_PI * (J + 1) / Sides;
				const FVector N0(0.f, FMath::Cos(A0), FMath::Sin(A0)), N1(0.f, FMath::Cos(A1), FMath::Sin(A1));
				TellCloth(K, N0 * 100.f, N1 * 100.f, FVector(100.f, 0.f, 0.f) + N1 * (100.f * SockTaper), FVector(100.f, 0.f, 0.f) + N0 * (100.f * SockTaper));
			}
			B = TellKeep(K, TEXT("SM_WindsockBand"));
		}
		return B;
	}

	/** One piece of the pennant: an upright sheet 100 long, 100 either side of the middle at the near end narrowing by PennantTaper. */
	const FTellBuilt& TellPennantMesh()
	{
		static FTellBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			TellCloth(K, FVector(0.f, 0.f, -100.f), FVector(100.f, 0.f, -100.f * PennantTaper), FVector(100.f, 0.f, 100.f * PennantTaper), FVector(0.f, 0.f, 100.f));
			B = TellKeep(K, TEXT("SM_WindPennantPiece"));
		}
		return B;
	}

	/** The staff: a metre of it, 7 cm thick, stretched to the height. */
	const FTellBuilt& TellPoleMesh()
	{
		static FTellBuilt B;
		if (!B.Mesh)
		{
			FLyudMeshKit K;
			K.Rod(TEXT("Pole"), FVector::ZeroVector, FVector(0.f, 0.f, 100.f), 3.5f, 8, true, false);
			B = TellKeep(K, TEXT("SM_WindTellPole"));
		}
		return B;
	}

	bool TellCameraAt(const UWorld* World, FVector& Out, FRotator* OutRot = nullptr)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && PC->PlayerCameraManager)
		{
			Out = PC->PlayerCameraManager->GetCameraLocation();
			if (OutRot)
			{
				*OutRot = PC->PlayerCameraManager->GetCameraRotation();
			}
			return true;
		}
		return false;
	}

	// Developer mode (F7, Actions): the tell-tales on and off, and a pair stood in front of the camera to look at.
	LyudDev::FAutoAction GDevWindTells(TEXT("wind.tells"), TEXT("wind"), TEXT("dev.action.wind.tells"),
		[](UWorld* W)
		{
			if (IConsoleVariable* C = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Wind.Tells")))
			{
				C->Set(CVarWindTells.GetValueOnGameThread() != 0 ? 0 : 1, ECVF_SetByConsole);
			}
		},
		[](UWorld* W) { return CVarWindTells.GetValueOnGameThread() != 0; });
	LyudDev::FAutoAction GDevWindSockHere(TEXT("wind.sock_here"), TEXT("wind"), TEXT("dev.action.wind.sock_here"),
		[](UWorld* W)
		{
			FVector Cam;
			FRotator Rot;
			if (!TellCameraAt(W, Cam, &Rot))
			{
				return;
			}
			const FVector Ahead = FRotator(0.f, Rot.Yaw, 0.f).Vector();
			const FVector Right = FRotator(0.f, Rot.Yaw + 90.f, 0.f).Vector();
			// eight metres ahead, on whatever the camera's owner stands on (about its eye height under the camera)
			const FVector Base = Cam + Ahead * 800.0 - FVector(0.0, 0.0, 160.0);
			ULyudWindTells::Spawn(W, Base - Right * 200.0, ELyudWindTell::Windsock, 600.f, 0);
			ULyudWindTells::Spawn(W, Base + Right * 200.0, ELyudWindTell::Pennant, 400.f, 1);
		});
}

// ------------------------------------------------------------------ ALyudWindTell

ALyudWindTell::ALyudWindTell()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

FVector ALyudWindTell::GetTop() const
{
	return GetActorTransform().TransformPosition(FVector(0.0, 0.0, PoleHeight));
}

float ALyudWindTell::GetShownFromDegrees() const
{
	// it points downwind: the bearing the air comes from is the opposite one (north +Y, east -X)
	const float Deg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Cos(Yaw), -FMath::Sin(Yaw)));
	return Deg < 0.f ? Deg + 360.f : Deg;
}

void ALyudWindTell::Build(ELyudWindTell InKind, float PoleHeightCm, int32 Colours)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Kind = InKind;
	PoleHeight = FMath::Max(PoleHeightCm, 0.f);
	const bool bSock = Kind == ELyudWindTell::Windsock;
	const int32 NumColours = bSock ? NumSockColours : NumPennantColours;
	const FTellColours* const Ways = bSock ? SockColours : PennantColours;
	const FTellColours& Col = Ways[((Colours % NumColours) + NumColours) % NumColours];

	if (PoleHeight > 1.f && TellPoleMesh().Mesh)
	{
		Pole = NewObject<UStaticMeshComponent>(this, TEXT("Pole"), RF_Transient);
		Pole->SetupAttachment(Root);
		TellNoLumen(Pole);
		Pole->SetStaticMesh(TellPoleMesh().Mesh);
		Pole->SetRelativeScale3D(FVector(1.0, 1.0, PoleHeight / 100.0));
		if (UMaterialInstanceDynamic* M = TellPaint(TellPoleTint))
		{
			Pole->SetMaterial(0, M);
		}
		Pole->SetCastShadow(true);
		Pole->RegisterComponent();
	}

	const FTellBuilt& Body = bSock ? TellSockMesh() : TellPennantMesh();
	const int32 Num = bSock ? SockBands : PennantPieces;
	Pieces.Reserve(Num);
	float Across = bSock ? SockMouthRadiusCm : PennantHalfCm;
	for (int32 I = 0; I < Num && Body.Mesh; ++I)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, FName(*FString::Printf(TEXT("Cloth%d"), I)), RF_Transient);
		C->SetupAttachment(Root);
		TellNoLumen(C);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetStaticMesh(Body.Mesh);
		// the one mesh is stretched to this piece's length and girth; the next piece starts where this one's taper ends
		C->SetRelativeScale3D(FVector((bSock ? SockBandCm : PennantPieceCm) / 100.0, Across / 100.0, Across / 100.0));
		Across *= bSock ? SockTaper : PennantTaper;
		// the sock's bands alternate; the pennant is one colour with its last piece the tip's
		const FLinearColor Tint = bSock ? ((I & 1) ? Col.B : Col.A) : (I == Num - 1 ? Col.B : Col.A);
		if (UMaterialInstanceDynamic* M = TellPaint(Tint))
		{
			C->SetMaterial(0, M);
		}
		C->SetCastShadow(bSock);
		C->RegisterComponent();
		Pieces.Add(C);
	}
	// laid out once, hanging, so that it is whole before the first wind is asked
	Step(nullptr, 0.f, false);
}

void ALyudWindTell::Step(const ULyudWindSubsystem* Wind, float Dt, bool bSample)
{
	if (!bBuilt)
	{
		return;
	}
	const bool bSock = Kind == ELyudWindTell::Windsock;
	const FVector Top = GetTop();
	SinceSample += Dt;
	if (bSample)
	{
		FVector Air = FVector::ZeroVector;
		float Rough = 0.f;
		if (Wind)
		{
			const FLyudLocalWind L = Wind->GetLocalWind(Top);
			Air = L.Velocity;
			Rough = L.Turbulence;
		}
		// on something that moves (a mast head) it shows the apparent wind; a jump (a teleport, a respawn) is not motion
		if (bHasLastTop && SinceSample > 1e-3f)
		{
			const FVector Own = (Top - LastTop) * (0.01 / SinceSample);
			if (Own.SizeSquared() < FMath::Square(60.0))
			{
				Air -= Own;
			}
		}
		LastTop = Top;
		bHasLastTop = true;
		SinceSample = 0.f;
		WindSpeed = float(Air.Size2D());
		if (WindSpeed > 0.2f)
		{
			WindYaw = float(FMath::Atan2(Air.Y, Air.X));
		}
		WindRough = Rough;
		WindRise = float(Air.Z);
	}

	if (Dt > 0.f)
	{
		// the cloth fills and empties over a moment, so a gust lifts it and a lull lets it sag
		ShownSpeed = FMath::FInterpTo(ShownSpeed, WindSpeed, Dt, bSock ? 1.8f : 3.5f);
		// It weathercocks: the pull that turns it downwind grows with the square of the wind, and it is underdamped, so
		// it swings past and back rather than snapping round. In light air it barely turns at all.
		const float Stiff = FMath::Clamp((bSock ? 0.5f : 0.9f) * ShownSpeed * ShownSpeed, 0.15f, 60.f);
		const float Err = FMath::UnwindRadians(WindYaw - Yaw);
		if (Dt <= 0.05f)
		{
			YawRate += (Stiff * Err - 2.f * 0.45f * FMath::Sqrt(Stiff) * YawRate) * Dt;
			Yaw += YawRate * Dt;
		}
		else
		{
			// a coarse step (far from the camera, or a hitch): no swinging, it just comes round
			Yaw += Err * (1.f - FMath::Exp(-Dt * FMath::Sqrt(Stiff)));
			YawRate = 0.f;
		}
		Yaw = FMath::UnwindRadians(Yaw);
		Flutter = FMath::Fmod(Flutter + Dt * (3.f + 1.1f * ShownSpeed), 2.f * UE_PI * 64.f);
	}

	const FVector Down(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
	const FVector Side(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.f);
	// how rough the air is against its speed: what makes the tail whip
	const float Rough01 = FMath::Clamp(WindRough / FMath::Max(ShownSpeed, 1.f), 0.f, 0.6f);
	// rising air (the cliff's brow) cocks it up, sinking air down
	const float RiseTilt = FMath::Clamp(FMath::Atan2(WindRise, FMath::Max(ShownSpeed, 1.f)), -0.45f, 0.45f);
	const int32 Num = Pieces.Num();
	const float Len = bSock ? SockBandCm : PennantPieceCm;
	// clear of the staff
	FVector P = Top + Down * (PoleHeight > 1.f ? 8.0 : 0.0);
	for (int32 I = 0; I < Num; ++I)
	{
		// The sock stands out band by band, about 1.5 m/s (3 knots) a band: limp under 2 m/s, straight by 8. Its
		// first band is held half out by the hoop of its mouth. The pennant is lighter: its tip is out by 5 m/s.
		const float Lift = bSock ? FMath::SmoothStep(1.55f * I + 0.4f, 1.55f * I + 2.0f, ShownSpeed)
			: FMath::SmoothStep(0.3f + 0.5f * I, 1.6f + 0.7f * I, ShownSpeed);
		const float HangMax = bSock ? (I == 0 ? 0.96f : 1.47f) : 1.50f;   // radians under the horizontal with no wind
		const float Hang = FMath::Lerp(HangMax, 0.035f, Lift);
		const float Amp = (0.03f + 0.5f * Rough01) * float(I + 1) / float(Num) * (0.25f + 0.75f * Lift) * (bSock ? 1.f : 1.8f);
		const float YawW = Amp * FMath::Sin(Flutter - 0.9f * I);
		const float PitchW = 0.6f * Amp * FMath::Sin(Flutter * 1.31f - 1.1f * I + 1.3f);
		const float Th = FMath::Clamp(Hang - RiseTilt * Lift + PitchW, -0.6f, 1.52f);
		const FVector Out = Down * FMath::Cos(YawW) + Side * FMath::Sin(YawW);
		const FVector Dir = Out * FMath::Cos(Th) - FVector::UpVector * FMath::Sin(Th);
		if (UStaticMeshComponent* C = Pieces[I])
		{
			C->SetWorldLocationAndRotation(P, FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat());
		}
		P += Dir * Len;
	}
}

// ------------------------------------------------------------------ ULyudWindTells

ULyudWindTells* ULyudWindTells::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudWindTells>() : nullptr;
}

bool ULyudWindTells::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudWindTells::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudWindTells, STATGROUP_Tickables);
}

void ULyudWindTells::Deinitialize()
{
	Tells.Reset();
	Super::Deinitialize();
}

ALyudWindTell* ULyudWindTells::Spawn(const UObject* WorldContext, const FVector& FootLocation, ELyudWindTell Kind, float PoleHeightCm, int32 Colours)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ALyudWindTell* T = World->SpawnActor<ALyudWindTell>(ALyudWindTell::StaticClass(), FTransform(FootLocation), P);
	if (T)
	{
		T->Build(Kind, PoleHeightCm, Colours);
		if (ULyudWindTells* Sys = World->GetSubsystem<ULyudWindTells>())
		{
			Sys->Tells.Add(T);
			T->SetActorHiddenInGame(CVarWindTells.GetValueOnGameThread() == 0);
		}
	}
	return T;
}

void ULyudWindTells::Remove(ALyudWindTell* Tell)
{
	if (!IsValid(Tell))
	{
		return;
	}
	if (ULyudWindTells* Sys = Get(Tell))
	{
		Sys->Tells.RemoveSingleSwap(Tell);
	}
	Tell->Destroy();
}

bool ULyudWindTells::GroundUnder(const FVector& At, float UpCm, float DownCm, FVector& Out) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudWindTellGround), false);
	// the sea's surface and its water volume are not ground
	if (const ALyudSea* Sea = ALyudSea::Get(World))
	{
		Params.AddIgnoredActor(Sea);
		if (const AActor* Volume = Sea->GetWaterVolume())
		{
			Params.AddIgnoredActor(Volume);
		}
	}
	if (World->LineTraceSingleByObjectType(Hit, At + FVector(0.0, 0.0, UpCm), At - FVector(0.0, 0.0, DownCm), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Out = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool ULyudWindTells::PlaceWorldTells()
{
	const ULyudCityThings* City = ULyudCityThings::Get(this);
	if (!City || City->NumThings() <= 0)
	{
		return false;
	}
	int32 Socks = 0;
	// The beach: a sock by each lifeguard tower's flag mast (the towers stand at the top of the sand). Three metres to
	// seaward of the mast (+X is west, the sea), clear of the tower's deck, its foot on the sand.
	TArray<FCityThingRef> Masts;
	City->GetThingsByAct([](const FString& Act) { return Act == TEXT("lg_flagstate") || Act.StartsWith(TEXT("lg_flagstate@")); }, Masts);
	for (const FCityThingRef& M : Masts)
	{
		const FVector Beside = M.Location + FVector(300.0, 0.0, 0.0);
		FVector Foot;
		if (!GroundUnder(Beside, 300.f, 1200.f, Foot))
		{
			Foot = Beside - FVector(0.0, 0.0, 150.0);
		}
		Socks += Spawn(this, Foot, ELyudWindTell::Windsock, 700.f, 0) ? 1 : 0;
	}
	// The pier's head (the city plan's spot S-PIER-HEAD, through the festivals' venue lookup, which puts it on the deck):
	// a sock two and a half metres further out, if there is deck there, else on the spot itself.
	if (ULyudFestivalSubsystem* Festival = ULyudFestivalSubsystem::Get(this))
	{
		const FLyudVenue Head = Festival->FindVenue(FName(TEXT("S-PIER-HEAD")));
		if (Head.bValid && Head.SpotId == TEXT("S-PIER-HEAD"))
		{
			const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this);
			const float Water = Wind ? Wind->GetWaterZ() : -4300.f;
			FVector Foot;
			if (!GroundUnder(Head.Center + Head.SeaDir.GetSafeNormal2D() * 250.0, 400.f, 600.f, Foot) || Foot.Z < Water + 50.f)
			{
				Foot = Head.Center;
			}
			Socks += Spawn(this, Foot, ELyudWindTell::Windsock, 500.f, 1) ? 1 : 0;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("LyudWindTells: %d windsocks stood (%d flag masts on the beach, the pier's head)"), Socks, Masts.Num());
	return true;
}

void ULyudWindTells::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (!bPlaced)
	{
		// once the town's things are read (a few seconds into play); given up after a minute in a level with no town
		PlaceClock += DeltaTime;
		if (PlaceClock >= PlaceNext)
		{
			PlaceNext = PlaceClock + 2.f;
			bPlaced = PlaceWorldTells() || PlaceClock > 60.f;
		}
	}
	const int32 Show = CVarWindTells.GetValueOnGameThread() != 0 ? 1 : 0;
	const bool bShowChanged = Show != ShownState;
	ShownState = Show;
	if (!Show)
	{
		if (bShowChanged)
		{
			for (ALyudWindTell* T : Tells)
			{
				if (IsValid(T))
				{
					T->SetActorHiddenInGame(true);
				}
			}
		}
		return;
	}
	FVector Cam = FVector::ZeroVector;
	const bool bCam = TellCameraAt(World, Cam);
	const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(World);
	FarClock += DeltaTime;
	const float FarDt = FarClock;
	const bool bFarStep = FarClock >= TellFarStepS;
	if (bFarStep)
	{
		FarClock = 0.f;
	}
	SampleClock += DeltaTime;
	const bool bSample = SampleClock >= TellSampleStepS;
	if (bSample)
	{
		SampleClock = 0.f;
	}
	for (int32 I = Tells.Num() - 1; I >= 0; --I)
	{
		ALyudWindTell* T = Tells[I];
		if (!IsValid(T))
		{
			Tells.RemoveAtSwap(I);
			continue;
		}
		const double D2 = bCam ? FVector::DistSquared(Cam, T->GetActorLocation()) : FMath::Square(double(TellNearCm) + 1.0);
		if (D2 > FMath::Square(double(TellFarCm)))
		{
			T->SetActorHiddenInGame(true);
			continue;
		}
		T->SetActorHiddenInGame(false);
		if (D2 <= FMath::Square(double(TellNearCm)))
		{
			T->Step(Wind, DeltaTime, bSample);
		}
		else if (bFarStep)
		{
			T->Step(Wind, FarDt, true);
		}
	}
}

FString ULyudWindTells::DebugReport() const
{
	FString Out = FString::Printf(TEXT("%d tell-tales"), Tells.Num());
	for (const ALyudWindTell* T : Tells)
	{
		if (IsValid(T))
		{
			const FVector Top = T->GetTop();
			Out += FString::Printf(TEXT("; %s at (%.0f, %.0f, %.0f) shows %.1f m/s from %.0f deg%s"),
				T->GetKind() == ELyudWindTell::Windsock ? TEXT("sock") : TEXT("pennant"), Top.X, Top.Y, Top.Z,
				T->GetShownSpeed(), T->GetShownFromDegrees(), T->IsHidden() ? TEXT(" (hidden)") : TEXT(""));
		}
	}
	return Out;
}
