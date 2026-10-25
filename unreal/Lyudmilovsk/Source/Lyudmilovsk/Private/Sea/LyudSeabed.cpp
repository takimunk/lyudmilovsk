#include "Sea/LyudSeabed.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sea/LyudSea.h"
#include "SeaLifeMeshes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSeabed(TEXT("lyud.Seabed"), 1, TEXT("0: no seabed life."));
	TAutoConsoleVariable<float> CVarSeabedRadius(TEXT("lyud.Seabed.Radius"), -1.f, TEXT("Seabed life radius around the camera, m (negative: by lyud.Sea.Quality)."));

	uint32 TileHash(int32 X, int32 Y, uint32 K)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u ^ K * 0xcb1ab31fu;
		H = (H ^ (H >> 15)) * 0x2c1b3c6du;
		H = (H ^ (H >> 12)) * 0x297a2d39u;
		return H ^ (H >> 15);
	}

	struct FRng
	{
		uint32 S;
		float F() { S = S * 1664525u + 1013904223u; return float(S >> 8) / 16777216.f; }
		float R(float A, float B) { return A + (B - A) * F(); }
	};

	/** Smooth 0..1 noise over the ground (patches of boulders, meadows). */
	float Patch(double X, double Y, double Cell, uint32 Seed)
	{
		const double U = X / Cell, V = Y / Cell;
		const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
		const float FX = FMath::SmoothStep(0.f, 1.f, float(U - IX)), FY = FMath::SmoothStep(0.f, 1.f, float(V - IY));
		auto H = [&](int32 A, int32 B) { return float(TileHash(A, B, Seed) >> 8) / 16777216.f; };
		return FMath::Lerp(FMath::Lerp(H(IX, IY), H(IX + 1, IY), FX), FMath::Lerp(H(IX, IY + 1), H(IX + 1, IY + 1), FX), FY);
	}

	/** Per kind: look (tint as base colour), how it is drawn. */
	struct FKindLook
	{
		const TCHAR* Name;
		FLinearColor Tint;
		float Sway;
		float Roughness;
		bool bShadow;
	};
	const FKindLook Looks[] = {
		{ TEXT("Eelgrass"), FLinearColor(0.10f, 0.22f, 0.05f), 1.f, 0.5f, false },
		{ TEXT("Fucus"), FLinearColor(0.20f, 0.17f, 0.05f), 0.6f, 0.45f, false },
		{ TEXT("Furcellaria"), FLinearColor(0.22f, 0.05f, 0.05f), 0.4f, 0.5f, false },
		{ TEXT("Boulder"), FLinearColor(0.2f, 0.19f, 0.18f), 0.f, 0.85f, true },
		{ TEXT("Mussels"), FLinearColor(0.03f, 0.035f, 0.05f), 0.f, 0.3f, false },
		{ TEXT("Shells"), FLinearColor(0.75f, 0.68f, 0.58f), 0.f, 0.6f, false },
		{ TEXT("Flounder"), FLinearColor(0.30f, 0.26f, 0.18f), 0.f, 0.6f, false },
		{ TEXT("Jellyfish"), FLinearColor(0.75f, 0.8f, 0.9f), 0.f, 0.2f, false },
		{ TEXT("Fish"), FLinearColor(0.7f, 0.75f, 0.8f), 0.f, 0.3f, false },
	};
}

ULyudSeabedComponent::ULyudSeabedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickInEditor = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Weed(TEXT("/Game/Sea/M_SeaWeed.M_SeaWeed"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rock(TEXT("/Game/Sea/M_SeaRock.M_SeaRock"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Jelly(TEXT("/Game/Sea/M_SeaJelly.M_SeaJelly"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FishM(TEXT("/Game/Sea/M_SeaFish.M_SeaFish"));
	WeedMaterial = Weed.Succeeded() ? Weed.Object : nullptr;
	RockMaterial = Rock.Succeeded() ? Rock.Object : nullptr;
	JellyMaterial = Jelly.Succeeded() ? Jelly.Object : nullptr;
	FishMaterial = FishM.Succeeded() ? FishM.Object : nullptr;
}

void ULyudSeabedComponent::OnRegister()
{
	Super::OnRegister();
}

void ULyudSeabedComponent::EnsureComponents()
{
	if (Kinds.Num() == NumKinds)
	{
		return;
	}
	AActor* Owner = GetOwner();
	UStaticMesh* Meshes[NumKinds] = { LyudSeaLife::Eelgrass(), LyudSeaLife::Fucus(), LyudSeaLife::Furcellaria(), LyudSeaLife::Boulder(),
		LyudSeaLife::Mussels(), LyudSeaLife::Shells(), LyudSeaLife::Flounder(), LyudSeaLife::Jellyfish(), LyudSeaLife::Fish() };
	UMaterialInterface* Mats[NumKinds] = { WeedMaterial, WeedMaterial, WeedMaterial, RockMaterial, RockMaterial, RockMaterial, FishMaterial, JellyMaterial, FishMaterial };
	for (int32 K = 0; K < NumKinds; K++)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Owner, *FString::Printf(TEXT("Seabed_%s"), Looks[K].Name), RF_Transient);
		C->SetupAttachment(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Meshes[K]);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(Looks[K].bShadow);
		C->SetGenerateOverlapEvents(false);
		C->bAffectDistanceFieldLighting = false;
		C->LightingChannels.bChannel0 = true;
		C->LightingChannels.bChannel1 = true;
		C->SetCullDistances(0, int32(Radius * 1.2f));
		if (Mats[K])
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Mats[K], C);
			M->SetVectorParameterValue(TEXT("Tint"), Looks[K].Tint);
			M->SetScalarParameterValue(TEXT("Sway"), Looks[K].Sway);
			M->SetScalarParameterValue(TEXT("Roughness"), Looks[K].Roughness);
			if (K == Fish)
			{
				M->SetScalarParameterValue(TEXT("Metallic"), 0.35f);
				M->SetScalarParameterValue(TEXT("Wiggle"), 1.f);
			}
			else if (K == Flounder)
			{
				M->SetScalarParameterValue(TEXT("Mottle"), 1.f);
			}
			C->SetMaterial(0, M);
		}
		C->RegisterComponent();
		Kinds.Add(C);
	}
}

float ULyudSeabedComponent::GroundZ(double X, double Y, bool& bOk) const
{
	const ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	bOk = false;
	return Sea ? Sea->GroundZ(X, Y, bOk) : 0.f;
}

void ULyudSeabedComponent::Rebuild(const FVector& Eye)
{
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	if (!Sea)
	{
		return;
	}
	const float SeaZ = Sea->SeaLevel();
	TArray<FTransform> Out[NumKinds];
	Flounders.Reset();
	const int32 TX = FMath::FloorToInt(Eye.X / TileSize), TY = FMath::FloorToInt(Eye.Y / TileSize);
	const int32 Reach = FMath::CeilToInt(AppliedRadius / TileSize);
	const float Dens = Density;
	for (int32 Y = TY - Reach; Y <= TY + Reach; Y++)
	{
		for (int32 X = TX - Reach; X <= TX + Reach; X++)
		{
			const FVector Center((X + 0.5) * TileSize, (Y + 0.5) * TileSize, 0.0);
			if (FVector::Dist2D(Center, Eye) > AppliedRadius + TileSize * 0.71f)
			{
				continue;
			}
			// only where there is water over some of the tile
			float MaxDepth = -1e9f;
			for (int32 K = 0; K < 4; K++)
			{
				MaxDepth = FMath::Max(MaxDepth, Sea->DepthAt(Center + FVector((K & 1 ? 0.5 : -0.5) * TileSize, (K & 2 ? 0.5 : -0.5) * TileSize, 0.0)));
			}
			if (MaxDepth < 10.f)
			{
				continue;
			}
			FRng R{ TileHash(X, Y, 77) };
			auto Place = [&](double PX, double PY, float& OutZ, float& OutDepth) -> bool
			{
				bool bOk;
				OutZ = GroundZ(PX, PY, bOk);
				OutDepth = (SeaZ - OutZ) * 0.01f;
				return bOk && OutDepth > 0.15f;
			};
			const double X0 = X * TileSize, Y0 = Y * TileSize;
			const float Rocky = Patch(Center.X * 0.01, Center.Y * 0.01, 24.0, 5);
			const float Meadow = Patch(Center.X * 0.01, Center.Y * 0.01, 17.0, 9);
			// boulders: fields of them in places, a stray one elsewhere
			const int32 NB = Rocky > 0.6f ? int32(R.R(2.f, 5.f) * Dens) : (R.F() < 0.15f * Dens ? 1 : 0);
			for (int32 B = 0; B < NB; B++)
			{
				const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
				float Z, D;
				const float S = R.R(0.35f, 1.6f);
				if (!Place(PX, PY, Z, D))
				{
					continue;
				}
				const FVector At(PX, PY, Z - 10.f * S);
				Out[Boulder].Add(FTransform(FRotator(R.R(-15.f, 15.f), R.R(0.f, 360.f), R.R(-15.f, 15.f)), At, FVector(S * R.R(0.8f, 1.2f), S * R.R(0.8f, 1.2f), S * R.R(0.6f, 1.f))));
				// weed on its top: wrack in the shallows, the red weed deeper
				const bool bShallow = D < 4.5f;
				const int32 NW = int32(R.R(2.f, 5.f) * Dens);
				for (int32 W = 0; W < NW; W++)
				{
					const float A = R.F() * 2.f * PI, Rr = R.F() * 30.f * S;
					const FVector P = At + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), 52.f * S * (1.f - 0.6f * (Rr / (30.f * S)) * (Rr / (30.f * S))));
					if (P.Z > SeaZ - 20.f) continue;
					Out[bShallow ? Fucus : Furcellaria].Add(FTransform(FRotator(R.R(-12.f, 12.f), R.R(0.f, 360.f), R.R(-12.f, 12.f)), P, FVector(R.R(0.7f, 1.3f))));
				}
				// mussels in the crevice at its foot
				if (R.F() < 0.7f)
				{
					const float A = R.F() * 2.f * PI;
					Out[Mussels].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX + FMath::Cos(A) * 45.f * S, PY + FMath::Sin(A) * 45.f * S, Z - 1.f), FVector(R.R(0.7f, 1.4f))));
				}
			}
			// eelgrass meadows on sand from 1.5 m down to ~7 m
			// (dense, so only near the eye: the water hides it beyond ~15 m anyway)
			if (Meadow > 0.45f && FVector::Dist2D(Center, Eye) < FMath::Min(AppliedRadius, 2000.f) + TileSize * 0.71f)
			{
				const int32 NE = int32((Meadow - 0.45f) * 260.f * Dens);
				for (int32 E = 0; E < NE; E++)
				{
					const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
					float Z, D;
					if (!Place(PX, PY, Z, D) || D < 1.5f || D > 7.5f) continue;
					// shorter where the surf reaches down
					const float H = FMath::Min(R.R(0.8f, 1.25f), D * 0.45f);
					Out[Eelgrass].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX, PY, Z - 2.f), FVector(1.f, 1.f, H)));
				}
			}
			// shells on the sand everywhere but the deepest
			if (R.F() < 0.6f * Dens)
			{
				const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
				float Z, D;
				if (Place(PX, PY, Z, D) && D < 9.f)
				{
					Out[Shells].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX, PY, Z - 0.5f), FVector(R.R(0.8f, 1.3f))));
				}
			}
			// flounders on open sand
			if (Rocky < 0.5f && R.F() < 0.2f * Dens)
			{
				const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
				float Z, D;
				if (Place(PX, PY, Z, D) && D > 1.f)
				{
					FFlounder F;
					F.Rest = F.Pos = F.Target = FVector(PX, PY, Z - 0.8f);
					F.Yaw = R.R(0.f, 360.f);
					F.Scale = R.R(0.7f, 1.3f);
					Flounders.Add(F);
					Out[Flounder].Add(FTransform(FRotator(0.f, F.Yaw, 0.f), F.Pos, FVector(F.Scale)));
				}
			}
		}
	}
	for (int32 K = 0; K < NumKinds; K++)
	{
		if (K == Jelly || K == Fish)
		{
			continue;
		}
		Kinds[K]->ClearInstances();
		if (Out[K].Num())
		{
			Kinds[K]->AddInstances(Out[K], false, true);
		}
	}
}

void ULyudSeabedComponent::UpdateSwimmers(const FVector& Eye, float Dt)
{
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	const FLyudSeaSample Here = Sea->SampleSurface(Eye);
	// only around a camera in or just over the water
	const bool bNear = Here.bIsSea && Here.Depth > 80.f && Eye.Z < Here.SurfaceZ + 600.f;
	UInstancedStaticMeshComponent* J = Kinds[Jelly];
	UInstancedStaticMeshComponent* F = Kinds[Fish];
	if (!bNear)
	{
		if (J->GetInstanceCount()) J->ClearInstances();
		if (F->GetInstanceCount()) F->ClearInstances();
		bSwimmersPlaced = false;
		return;
	}
	const float SeaZ = Sea->SeaLevel();
	const float Box = FMath::Min(AppliedRadius, 1100.f);
	FRng R{ uint32(Clock * 1000.0) + 13u };
	auto Column = [&](const FVector2D& XY, float& Lo, float& Hi) -> bool
	{
		const float D = Sea->DepthAt(FVector(XY, 0.0));
		Lo = SeaZ - D + 40.f;
		Hi = SeaZ - 30.f;
		return D > 80.f;
	};
	if (!bSwimmersPlaced)
	{
		bSwimmersPlaced = true;
		Jellies.SetNum(FMath::Max(Jellyfish, 0));
		for (FSwimmer& S : Jellies)
		{
			float Lo, Hi;
			const FVector2D XY(Eye.X + R.R(-Box, Box), Eye.Y + R.R(-Box, Box));
			S.Pos = FVector(XY, Column(XY, Lo, Hi) ? R.R(Lo, Hi) : SeaZ - 9e5f);
			S.Phase = R.R(0.f, 6.28f);
			S.Scale = R.R(0.6f, 1.4f);
			S.Seed = R.F();
		}
		School.SetNum(FMath::Max(SchoolFish, 0));
		SchoolCenter = SchoolGoal = FVector(Eye.X + 600.f, Eye.Y, FMath::Min(Eye.Z, SeaZ - 150.f));
		for (FSwimmer& S : School)
		{
			S.Pos = SchoolCenter + FVector(R.R(-150.f, 150.f), R.R(-150.f, 150.f), R.R(-40.f, 40.f));
			S.Vel = FVector(30.f, 0.f, 0.f);
			S.Phase = R.R(0.f, 6.28f);
			S.Scale = R.R(1.6f, 2.3f);   // Baltic herring, ~20 cm
			S.Seed = R.F();
		}
	}

	// moon jellies: pulse up a little, sink back, carried by the waves' orbits; recycled ahead of the camera
	TArray<FTransform> JX;
	JX.Reserve(Jellies.Num());
	for (FSwimmer& S : Jellies)
	{
		float Lo, Hi;
		if (FVector::Dist2D(S.Pos, Eye) > Box * 1.3f || S.Pos.Z < SeaZ - 5e5f)
		{
			const FVector2D Away = FVector2D(Eye - S.Pos).GetSafeNormal();
			const FVector2D XY = FVector2D(Eye) + Away * Box * R.R(0.7f, 1.f) + FVector2D(-Away.Y, Away.X) * R.R(-Box, Box) * 0.6f;
			S.Pos = FVector(XY, Column(XY, Lo, Hi) ? R.R(Lo, Hi) : SeaZ - 9e5f);
		}
		S.Phase += Dt * (1.4f + 0.4f * S.Seed);
		const FLyudSeaSample W = Sea->SampleSurface(S.Pos);
		const float Deep = FMath::Exp(-FMath::Max(W.SurfaceZ - S.Pos.Z, 0.f) / 400.f);
		const float Pulse = FMath::Max(0.f, FMath::Sin(S.Phase));
		S.Pos += (W.Velocity * Deep * 0.8f + FVector(0.f, 0.f, 6.f * Pulse - 2.5f)) * Dt;
		if (Column(FVector2D(S.Pos), Lo, Hi))
		{
			S.Pos.Z = FMath::Clamp(S.Pos.Z, Lo, Hi);
		}
		const FRotator Tilt(8.f * FMath::Sin(S.Phase * 0.31f + S.Seed * 6.f), S.Seed * 360.f, 8.f * FMath::Cos(S.Phase * 0.27f));
		// the bell's beat: it narrows and deepens, then relaxes slowly
		const float Beat = FMath::Pow(Pulse, 3.f);
		JX.Add(FTransform(Tilt, S.Pos, FVector(S.Scale * (1.f - 0.14f * Beat), S.Scale * (1.f - 0.14f * Beat), S.Scale * (1.f + 0.2f * Beat))));
	}
	if (J->GetInstanceCount() != JX.Num())
	{
		J->ClearInstances();
		J->AddInstances(JX, false, true);
	}
	else
	{
		J->BatchUpdateInstancesTransforms(0, JX, true, true);
	}

	// the school: a centre wanders between goals near the camera at mid water, the fish keep stations
	// round it on slow loops; the whole school bolts when the camera comes close
	SchoolClock += Dt;
	const float Close = FVector::Dist(SchoolCenter, Eye);
	if (FVector::Dist(SchoolCenter, SchoolGoal) < 150.f || SchoolClock > 12.f || Close < 250.f)
	{
		SchoolClock = 0.f;
		const FVector2D Off = Close < 250.f ? FVector2D(SchoolCenter - Eye).GetSafeNormal() * 900.f : FVector2D(R.R(-1.f, 1.f), R.R(-1.f, 1.f)) * Box * 0.6f;
		const FVector2D XY = FVector2D(Eye) + Off;
		float Lo, Hi;
		if (Column(XY, Lo, Hi))
		{
			SchoolGoal = FVector(XY, FMath::Lerp(Lo, Hi, R.R(0.3f, 0.7f)));
		}
	}
	const float Pace = Close < 400.f ? 260.f : 70.f;
	SchoolCenter += (SchoolGoal - SchoolCenter).GetClampedToMaxSize(Pace * Dt);
	TArray<FTransform> FX;
	FX.Reserve(School.Num());
	for (int32 I = 0; I < School.Num(); I++)
	{
		FSwimmer& S = School[I];
		S.Phase += Dt * (0.25f + 0.1f * S.Seed);
		const float A = S.Phase + I * 0.7f;
		const FVector Station = SchoolCenter + FVector(FMath::Cos(A) * (60.f + 90.f * S.Seed), FMath::Sin(A) * (60.f + 90.f * S.Seed), 25.f * FMath::Sin(A * 1.7f + S.Seed * 6.f));
		const FVector Want = (Station - S.Pos) * 1.5f;
		S.Vel += (Want - S.Vel).GetClampedToMaxSize(400.f * Dt);
		S.Vel = S.Vel.GetClampedToMaxSize(Pace * 2.5f + 40.f);
		S.Pos += S.Vel * Dt;
		const FVector Dir = S.Vel.SizeSquared() > 1.f ? S.Vel.GetSafeNormal() : FVector::ForwardVector;
		FX.Add(FTransform(Dir.Rotation(), S.Pos, FVector(S.Scale)));
	}
	if (F->GetInstanceCount() != FX.Num())
	{
		F->ClearInstances();
		F->AddInstances(FX, false, true);
	}
	else
	{
		F->BatchUpdateInstancesTransforms(0, FX, true, true);
	}

	// flounders: a burst off the sand when the camera comes within reach, then a glide down a few metres on
	UInstancedStaticMeshComponent* Fl = Kinds[Flounder];
	if (Fl->GetInstanceCount() == Flounders.Num())
	{
		for (int32 I = 0; I < Flounders.Num(); I++)
		{
			FFlounder& L = Flounders[I];
			if (L.Fleeing <= 0.f && FVector::Dist(L.Pos, Eye) < 180.f)
			{
				const FVector2D Away = FVector2D(L.Pos - Eye).GetSafeNormal();
				const FVector2D XY = FVector2D(L.Pos) + Away * R.R(250.f, 450.f);
				bool bOk;
				const float Z = GroundZ(XY.X, XY.Y, bOk);
				if (bOk && Z < SeaZ - 50.f)
				{
					L.Target = FVector(XY, Z - 0.8f);
					L.Fleeing = 1.f;
				}
			}
			if (L.Fleeing > 0.f)
			{
				const FVector To = L.Target - L.Pos;
				const float Step = 250.f * Dt;
				const float Lift = 25.f * FMath::Sin(PI * FMath::Clamp(1.f - To.Size2D() / 400.f, 0.f, 1.f));
				if (To.Size() <= Step)
				{
					L.Pos = L.Target;
					L.Fleeing = 0.f;
				}
				else
				{
					L.Pos += To.GetSafeNormal() * Step;
					L.Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
				}
				Fl->UpdateInstanceTransform(I, FTransform(FRotator(0.f, L.Yaw, 0.f), L.Pos + FVector(0.f, 0.f, L.Fleeing > 0.f ? Lift : 0.f), FVector(L.Scale)), true, true);
			}
		}
	}
}

void ULyudSeabedComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	UWorld* World = GetWorld();
	if (!Sea || !World || !World->IsGameWorld())
	{
		return;
	}
	const bool bOn = CVarSeabed.GetValueOnGameThread() != 0 && IsActive();
	EnsureComponents();
	for (UInstancedStaticMeshComponent* C : Kinds)
	{
		C->SetVisibility(bOn);
	}
	if (!bOn || !Sea->IsDepthReady())
	{
		return;
	}
	Clock += DeltaTime;
	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	static IConsoleVariable* Quality = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Sea.Quality"));
	int32 Q = Quality ? Quality->GetInt() : -1;
	if (Q < 0)
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		Q = 0;
#else
		Q = 2;
#endif
	}
	const float Forced = CVarSeabedRadius.GetValueOnGameThread();
	const float Rad = Forced >= 0.f ? Forced * 100.f : (Q == 0 ? 1500.f : Q == 1 ? 2500.f : Radius);
	const FIntPoint Tile(FMath::FloorToInt(Eye.X / TileSize), FMath::FloorToInt(Eye.Y / TileSize));
	if (Tile != BuiltTile || Q != BuiltQuality || Rad != AppliedRadius)
	{
		BuiltTile = Tile;
		BuiltQuality = Q;
		AppliedRadius = Rad;
		for (int32 K = 0; K < Kinds.Num(); K++)
		{
			// small things vanish into the water first
			const bool bSmall = K == Eelgrass || K == Shells || K == Mussels || K == Fish;
			Kinds[K]->SetCullDistances(0, int32(bSmall ? FMath::Min(Rad, 2000.f) : Rad * 1.2f));
		}
		Rebuild(Eye);
	}
	UpdateSwimmers(Eye, DeltaTime);
}

FVector ULyudSeabedComponent::FindNearest(int32 Kind, const FVector& From) const
{
	FVector Best = From;
	double BestD = DBL_MAX;
	if (Kinds.IsValidIndex(Kind) && Kinds[Kind])
	{
		for (int32 I = 0; I < Kinds[Kind]->GetInstanceCount(); I++)
		{
			FTransform T;
			Kinds[Kind]->GetInstanceTransform(I, T, true);
			const double D = FVector::DistSquared(T.GetLocation(), From);
			if (D < BestD)
			{
				BestD = D;
				Best = T.GetLocation();
			}
		}
	}
	return Best;
}

int32 ULyudSeabedComponent::GetInstanceCount() const
{
	int32 N = 0;
	for (const UInstancedStaticMeshComponent* C : Kinds)
	{
		N += C ? C->GetInstanceCount() : 0;
	}
	return N;
}

FString ULyudSeabedComponent::GetDebugReport() const
{
	FString S = TEXT("seabed:");
	for (int32 K = 0; K < Kinds.Num(); K++)
	{
		S += FString::Printf(TEXT(" %s %d"), Looks[K].Name, Kinds[K] ? Kinds[K]->GetInstanceCount() : 0);
	}
	return S;
}
