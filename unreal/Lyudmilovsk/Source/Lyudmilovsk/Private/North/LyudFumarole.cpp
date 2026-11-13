#include "North/LyudFumarole.h"

#include "../Transport/LyudMeshKit.h"
#include "Audio/LyudSoundscape.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "North/LyudNorthFx.h"

namespace
{
	const TCHAR* FumPaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
	const TCHAR* FumMudPath = TEXT("/Game/North/Materials/M_North_Mud.M_North_Mud");
	const TCHAR* FumWaterPath = TEXT("/Game/North/Materials/M_North_Water.M_North_Water");

	/** Small things a few hundred metres off are not worth drawing. */
	constexpr float FumDrawDistance = 40000.f;

	/** A dome of radius 100 cm on its base (a bubble, scaled). */
	void FumDome(FLyudMeshKit& K, FName Mat)
	{
		TArray<FVector2D> P;
		for (int32 I = 0; I <= 5; I++)
		{
			const float A = 0.5f * PI * I / 5.f;
			P.Add(FVector2D(100.f * FMath::Cos(A), 100.f * FMath::Sin(A)));
		}
		TArray<FName> M;
		M.Init(Mat, P.Num() - 1);
		K.Lathe(FTransform::Identity, P, M, 10, false, false);
	}
}

ALyudFumarole::ALyudFumarole()
{
	PrimaryActorTick.TickInterval = 0.04f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float ALyudFumarole::PoolRadius() const
{
	switch (Kind)
	{
	case ELyudFumaroleKind::Vent: return 60.f + 50.f * Size;
	case ELyudFumaroleKind::Steam: return 150.f * Size;
	case ELyudFumaroleKind::MudPot: return 50.f * Size;          // Size: the pool's diameter in metres
	default: return 81.25f * Size;                               // the spring: 2.6 m at 3.2
	}
}

void ALyudFumarole::BeginPlay()
{
	// the import put it on terrain.ground_z; the built ground decides (a trace, a few metres at most)
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FumaroleGround), false, this);
		FHitResult Hit;
		const FVector At = GetActorLocation();
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 300.0), At - FVector(0.0, 0.0, 1000.0), ECC_Visibility, Q)
			&& FMath::Abs(Hit.ImpactPoint.Z - At.Z) < 400.0)
		{
			SetActorLocation(FVector(At.X, At.Y, Hit.ImpactPoint.Z));
		}
	}
	Rng = GetTypeHash(FumaroleId) | 1u;
	Build();
	Super::BeginPlay();
}

void ALyudFumarole::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLoop();
	Super::EndPlay(EndPlayReason);
}

UMaterialInstanceDynamic* ALyudFumarole::MakeMat(const TCHAR* BasePath, const TCHAR* FallbackPath, const FLinearColor& Color, float Roughness)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, BasePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base && FallbackPath)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, FallbackPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Mats.Add(M);
	return M;
}

UStaticMeshComponent* ALyudFumarole::AddPart(UStaticMesh* Mesh, UMaterialInstanceDynamic* Mat, bool bMoving)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(RootComponent);
	if (bMoving)
	{
		LyudNorthFx::Lightweight(C);
	}
	else
	{
		// lies still on the ground: no collision (the hero may step in; quests ask IsHotGround), a cheap shadow-less part
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
	}
	C->SetLightingChannels(true, true, false);
	C->SetCachedMaxDrawDistance(FumDrawDistance);
	C->LDMaxDrawDistance = FumDrawDistance;
	if (Mat)
	{
		for (int32 I = 0; I < C->GetNumMaterials(); I++)
		{
			C->SetMaterial(I, Mat);
		}
	}
	C->RegisterComponent();
	return C;
}

void ALyudFumarole::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const float R = PoolRadius();
	TArray<FName> Slots;
	auto Assign = [this, &Slots](UStaticMeshComponent* C, const TMap<FName, UMaterialInstanceDynamic*>& BySlot)
	{
		for (int32 I = 0; I < Slots.Num(); I++)
		{
			if (UMaterialInstanceDynamic* const* M = BySlot.Find(Slots[I]))
			{
				C->SetMaterial(I, *M);
			}
		}
		Parts.Add(C);
	};
	switch (Kind)
	{
	case ELyudFumaroleKind::Vent:
	{
		// a low crusted mound, its lip pale with sulphur, a dark throat
		FLyudMeshKit K;
		const float H = 10.f + 14.f * Size;
		const TArray<FVector2D> P = { { R, -6.f }, { R * 0.62f, H * 0.55f }, { R * 0.3f, H }, { R * 0.16f, H * 0.8f }, { R * 0.1f, -40.f } };
		const TArray<FName> M = { TEXT("clay"), TEXT("crust"), TEXT("crust"), TEXT("throat") };
		K.Lathe(FTransform::Identity, P, M, 14, false, false);
		UStaticMeshComponent* C = AddPart(K.Build(this, TEXT("NorthVent"), Slots), nullptr, false);
		Assign(C, { { TEXT("clay"), MakeMat(FumPaintPath, nullptr, FLinearColor(0.52f, 0.47f, 0.38f), 0.95f) },
			{ TEXT("crust"), MakeMat(FumPaintPath, nullptr, FLinearColor(0.78f, 0.68f, 0.28f), 0.9f) },
			{ TEXT("throat"), MakeMat(FumPaintPath, nullptr, FLinearColor(0.025f, 0.022f, 0.02f), 0.8f) } });
		SurfaceZ = H * 0.8f;
		break;
	}
	case ELyudFumaroleKind::MudPot:
	{
		// a dried rim round a pool of grey mud a few centimetres over the ground, and the bubbles
		FLyudMeshKit K;
		const TArray<FVector2D> P = { { R + 45.f, -4.f }, { R + 12.f, 9.f }, { R, 6.f }, { R * 0.97f, 3.f } };
		const TArray<FName> M = { TEXT("rim"), TEXT("rim"), TEXT("rim") };
		K.Lathe(FTransform::Identity, P, M, 18, false, false);
		const TArray<FVector2D> D = { { R * 0.98f, 4.f }, { 0.f, 4.f } };
		const TArray<FName> DM = { TEXT("mud") };
		K.Lathe(FTransform::Identity, D, DM, 18, false, false);
		UMaterialInstanceDynamic* Mud = MakeMat(FumMudPath, FumPaintPath, FLinearColor(0.15f, 0.145f, 0.135f), 0.22f);
		UStaticMeshComponent* C = AddPart(K.Build(this, TEXT("NorthMudPot"), Slots), nullptr, false);
		Assign(C, { { TEXT("rim"), MakeMat(FumPaintPath, nullptr, FLinearColor(0.42f, 0.4f, 0.37f), 0.95f) }, { TEXT("mud"), Mud } });
		SurfaceZ = 4.f;
		FLyudMeshKit B;
		FumDome(B, TEXT("mud"));
		TArray<FName> BSlots;
		UStaticMesh* Dome = B.Build(this, TEXT("NorthBubble"), BSlots);
		const int32 N = Size > 2.2f ? 3 : 2;
		for (int32 I = 0; I < N; I++)
		{
			UStaticMeshComponent* Bc = AddPart(Dome, Mud, true);
			Bc->SetVisibility(false);
			BubbleParts.Add(Bc);
			LyudNorthModel::FBubble& Bub = Bubbles.AddDefaulted_GetRef();
			LyudNorthModel::NewBubble(Bub, Rng);
		}
		break;
	}
	case ELyudFumaroleKind::Spring:
	{
		// a ring of stones round still green water
		FLyudMeshKit K;
		const TArray<FVector2D> P = { { R + 70.f, -4.f }, { R + 24.f, 16.f }, { R + 4.f, 12.f }, { R - 6.f, 4.f } };
		const TArray<FName> M = { TEXT("stone"), TEXT("stone"), TEXT("stone") };
		K.Lathe(FTransform::Identity, P, M, 24, false, false);
		const TArray<FVector2D> W = { { R, 7.f }, { 0.f, 7.f } };
		const TArray<FName> WM = { TEXT("water") };
		K.Lathe(FTransform::Identity, W, WM, 24, false, false);
		UStaticMeshComponent* C = AddPart(K.Build(this, TEXT("NorthSpring"), Slots), nullptr, false);
		UMaterialInstanceDynamic* Water = MakeMat(FumWaterPath, FumPaintPath, FLinearColor(0.05f, 0.2f, 0.17f), 0.04f);
		if (Water)
		{
			Water->SetScalarParameterValue(TEXT("Opacity"), 0.72f);
		}
		Assign(C, { { TEXT("stone"), MakeMat(FumPaintPath, nullptr, FLinearColor(0.24f, 0.22f, 0.2f), 0.9f) }, { TEXT("water"), Water } });
		SurfaceZ = 7.f;
		break;
	}
	default:
		// steaming ground: the terrain's sulphur layer is the ground; nothing to build
		SurfaceZ = 0.f;
		break;
	}
}

void ALyudFumarole::SetAwake(bool bInAwake, bool bInNear)
{
	const bool bWasAwake = bAwake;
	Super::SetAwake(bInAwake, bInNear);
	if (bNear)
	{
		StartLoop();
	}
	else
	{
		StopLoop();
	}
	if (bWasAwake && !bAwake)
	{
		for (UStaticMeshComponent* C : BubbleParts)
		{
			C->SetVisibility(false);
		}
	}
}

void ALyudFumarole::StartLoop()
{
	if (Loop)
	{
		return;
	}
	ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	switch (Kind)
	{
	case ELyudFumaroleKind::Vent: Loop = S->AttachLoop(TEXT("north_vent"), RootComponent, 0.45f + 0.25f * Size); break;
	case ELyudFumaroleKind::Steam: Loop = S->AttachLoop(TEXT("north_vent"), RootComponent, 0.1f); break;
	case ELyudFumaroleKind::MudPot: Loop = S->AttachLoop(TEXT("north_mudpot"), RootComponent, 0.6f); break;
	default: Loop = S->AttachLoop(TEXT("north_spring"), RootComponent, 0.5f); break;
	}
	if (Loop && Kind == ELyudFumaroleKind::Steam)
	{
		// a breath of the vents' hiss, softer and lower
		Loop->SetPitchMultiplier(0.75f);
	}
}

void ALyudFumarole::StopLoop()
{
	if (Loop)
	{
		// AttachLoop spawns it with bAutoDestroy off: on, or the faded component stays behind
		Loop->bAutoDestroy = true;
		Loop->FadeOut(1.5f, 0.f);
		Loop = nullptr;
	}
}

void ALyudFumarole::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	EmitSteam(DeltaTime);
	if (BubbleParts.Num())
	{
		StepBubbles(DeltaTime);
	}
}

void ALyudFumarole::EmitSteam(float DeltaTime)
{
	ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this);
	ALyudNorthSteam* Fx = N ? N->Steam() : nullptr;
	if (!Fx)
	{
		return;
	}
	const float Dens = Fx->GetDensity();
	float Rate = 0.f;
	switch (Kind)
	{
	case ELyudFumaroleKind::Vent: Rate = LyudNorthModel::VentRate(Size, Dens); break;
	case ELyudFumaroleKind::Steam: Rate = (1.f + 0.6f * Size) * Dens; break;
	case ELyudFumaroleKind::MudPot: Rate = 0.5f * Dens; break;
	default: Rate = 1.6f * Dens; break;
	}
	// a little thinner from afar (beyond the sound's reach)
	EmitDebt += DeltaTime * Rate * (bNear ? 1.f : 0.6f);
	const FVector Base = GetActorLocation() + FVector(0.0, 0.0, SurfaceZ);
	const float R = PoolRadius();
	while (EmitDebt >= 1.f)
	{
		EmitDebt -= 1.f;
		Emitted++;
		const float U = LyudNorthModel::Rand01(Rng), V = LyudNorthModel::Rand01(Rng), Wv = LyudNorthModel::Rand01(Rng);
		const float A = U * 2.f * PI, Rr = FMath::Sqrt(V);
		const FVector Disc(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f);
		switch (Kind)
		{
		case ELyudFumaroleKind::Vent:
			Fx->Emit(Base + Disc * 12.f, FVector(Disc.X * 40.f, Disc.Y * 40.f, LyudNorthModel::VentSpeed(Size) * (0.85f + 0.3f * Wv)),
				25.f + 15.f * Size, (160.f + 120.f * Size) * (0.8f + 0.4f * V), 0.5f, 3.5f + Wv, 25.f, LyudNorthModel::VentDrag(), 0.f, 1.f);
			break;
		case ELyudFumaroleKind::Steam:
			Fx->Emit(Base + Disc * R, FVector(0.f, 0.f, 30.f + 40.f * Wv), 40.f, (180.f + 100.f * Wv) * FMath::Sqrt(Size), 0.22f,
				4.f + 2.f * V, 12.f, 1.f, 0.f, 0.8f);
			break;
		case ELyudFumaroleKind::MudPot:
			Fx->Emit(Base + Disc * R * 0.8f, FVector(0.f, 0.f, 25.f + 20.f * Wv), 30.f, 140.f, 0.14f, 3.5f, 8.f, 1.f, 0.f, 0.8f);
			break;
		default:
			Fx->Emit(Base + Disc * R * 0.9f, FVector(0.f, 0.f, 20.f + 25.f * Wv), 50.f, 200.f + 60.f * V, 0.12f, 5.f, 10.f, 1.2f, 0.f, 0.8f);
			break;
		}
	}
}

void ALyudFumarole::StepBubbles(float DeltaTime)
{
	const float R = PoolRadius();
	const float Biggest = 8.f + 6.f * Size;
	ALyudNorthSteam* Fx = nullptr;
	for (int32 I = 0; I < Bubbles.Num() && I < BubbleParts.Num(); I++)
	{
		LyudNorthModel::FBubble& B = Bubbles[I];
		const float PrevU = B.U, PrevV = B.V;
		const bool bPop = LyudNorthModel::StepBubble(B, DeltaTime, Rng);
		UStaticMeshComponent* C = BubbleParts[I];
		const float S = LyudNorthModel::BubbleScale(B);
		if (S > 0.01f)
		{
			const float Rad = Biggest * S / 100.f;
			C->SetRelativeLocation(FVector(B.U * R, B.V * R, SurfaceZ - 1.f));
			C->SetRelativeScale3D(FVector(Rad, Rad, Rad * 0.7f));
			if (!C->IsVisible())
			{
				C->SetVisibility(true);
			}
		}
		else if (C->IsVisible())
		{
			C->SetVisibility(false);
		}
		if (bPop)
		{
			// the plop: a wisp where it burst (the loop carries the sound)
			if (!Fx)
			{
				ULyudNorthSubsystem* N = ULyudNorthSubsystem::Get(this);
				Fx = N ? N->Steam() : nullptr;
			}
			if (Fx)
			{
				Fx->Emit(GetActorLocation() + FVector(PrevU * R, PrevV * R, SurfaceZ + 5.f), FVector(0.f, 0.f, 40.f), 12.f, 70.f, 0.25f, 1.6f,
					10.f, 1.5f, 0.f, 0.6f);
			}
		}
	}
}
