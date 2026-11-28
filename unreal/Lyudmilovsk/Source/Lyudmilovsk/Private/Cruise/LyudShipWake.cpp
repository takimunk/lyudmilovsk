#include "Cruise/LyudShipWake.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "../Transport/LyudMeshKit.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sea/LyudSea.h"
#include "SeascapeMath.h"
#include "Weather/LyudWind.h"

namespace
{
constexpr int32 ShipMaxDrops = 300;
constexpr int32 ShipMaxFoam = 900;
constexpr float ShipGravity = 980.f;
}

ULyudShipWake::ULyudShipWake()
{
	PrimaryComponentTick.bCanEverTick = false;
	Rnd.Initialize(1962);
}

float ULyudShipWake::HalfBeamAt(float X) const
{
	// full amidships, a fine entrance over the forward third, a rounded run aft
	const float T = FMath::Clamp(X / LengthCm, 0.f, 1.f);
	const float Fore = FMath::Pow(FMath::Clamp((1.f - T) / 0.32f, 0.f, 1.f), 0.65f);
	const float Aft = FMath::Pow(FMath::Clamp(T / 0.12f + 0.55f, 0.f, 1.f), 0.8f);
	return HalfBeamCm * FMath::Min(Fore, Aft);
}

void ULyudShipWake::EnsureParts()
{
	if (DropIsm)
	{
		return;
	}
	AActor* Owner = GetOwner();
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// see-through white: the cable car's glass master (Color, Opacity), as the train's wake
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Make = [&](UStaticMesh* M, const TCHAR* Name, const FLinearColor& Col, float Opacity)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UInstancedStaticMeshComponent::StaticClass(), Name));
		I->SetStaticMesh(M);
		I->SetMobility(EComponentMobility::Movable);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCastShadow(false);
		I->SetUsingAbsoluteLocation(true);
		I->SetUsingAbsoluteRotation(true);
		I->SetUsingAbsoluteScale(true);
		I->LightingChannels.bChannel1 = true;
		// moved every frame: out of the Lumen scene, or Lumen recaptures it every frame
		I->bAffectDynamicIndirectLighting = false;
		I->bAffectDistanceFieldLighting = false;
		if (Paint)
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Paint, Owner);
			D->SetVectorParameterValue(TEXT("Color"), Col);
			D->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			I->SetMaterial(0, D);
		}
		I->SetupAttachment(this);
		I->RegisterComponent();
		I->SetWorldTransform(FTransform::Identity);
		Owner->AddInstanceComponent(I);
		return I;
	};
	DropIsm = Make(Sphere, TEXT("ShipWakeDrops"), FLinearColor(0.85f, 0.9f, 0.9f), 0.55f);
	// the train wake's lace (LyudTrainWake.cpp): irregular flecks scattered over a ring or a disc, both faces
	auto Lace = [&](FName Name, int32 Count, float RIn, float ROut, bool bRing, int32 Seed) -> UStaticMesh*
	{
		FRandomStream R(Seed);
		FLyudMeshKit K;
		int32 Made = 0;
		for (int32 Tries = 0; Made < Count && Tries < Count * 8; Tries++)
		{
			const float A = R.FRandRange(0.f, 2.f * PI);
			float Rad;
			if (bRing)
			{
				const float Keep = 0.55f + 0.45f * FMath::Sin(A * 3.f + 1.3f) * FMath::Sin(A * 5.f + 0.4f);
				if (R.FRand() > Keep)
				{
					continue;
				}
				Rad = FMath::Lerp(RIn, ROut, 0.5f + 0.5f * (R.FRand() - R.FRand()));
			}
			else
			{
				Rad = ROut * FMath::Sqrt(R.FRand()) * (0.5f + 0.5f * R.FRand());
			}
			const FVector C(Rad * FMath::Cos(A), Rad * FMath::Sin(A), 0.f);
			const float S = bRing ? R.FRandRange(1.5f, 4.5f) : R.FRandRange(0.9f, 2.2f) * (1.f - 0.5f * Rad / ROut);
			const int32 N = 5;
			TArray<FVector> P;
			const float A0 = R.FRandRange(0.f, 2.f * PI);
			for (int32 I = 0; I < N; I++)
			{
				const float B = A0 + 2.f * PI * I / N;
				const float L = S * R.FRandRange(0.6f, 1.3f);
				FVector D(FMath::Cos(B) * L, FMath::Sin(B) * L * 0.6f, 0.f);
				P.Add(C + (bRing ? FQuat(FVector::UpVector, A + PI / 2).RotateVector(D) : D));
			}
			for (int32 I = 1; I + 1 < N; I++)
			{
				K.Tri(TEXT("foam"), P[0], P[I], P[I + 1]);
				K.Tri(TEXT("foam"), P[0] + FVector(0, 0, 0.1f), P[I + 1] + FVector(0, 0, 0.1f), P[I] + FVector(0, 0, 0.1f));
			}
			Made++;
		}
		TArray<FName> Slots;
		return K.Build(Owner, MakeUniqueObjectName(Owner, UStaticMesh::StaticClass(), Name), Slots);
	};
	RingMesh = Lace(TEXT("ShipWakeRing"), 90, 34.f, 50.f, true, 17);
	PatchMesh = Lace(TEXT("ShipWakePatch"), 230, 0.f, 50.f, false, 19);
	FoamIsm = Make(RingMesh ? RingMesh.Get() : Disc, TEXT("ShipWakeFoam"), FLinearColor(0.9f, 0.93f, 0.92f), 0.5f);
	TrailIsm = Make(PatchMesh ? PatchMesh.Get() : Disc, TEXT("ShipWakeTrail"), FLinearColor(0.88f, 0.92f, 0.9f), 0.42f);
}

void ULyudShipWake::Feed(const FTransform& Hull, const FVector& HullVelocity, const FVector& WaterVelocity, float DeltaSeconds)
{
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const FVector Rel = HullVelocity - WaterVelocity;
	const float SpeedMS = float(Rel.Size2D()) * 0.01f;
	if (SpeedMS < LyudSeascape::WakeMinSpeed && Foam.Num() == 0 && Drops.Num() == 0 && FoamUsed == 0 && TrailUsed == 0 && DropUsed == 0)
	{
		// at anchor with nothing left on the water: nothing to do (and nothing is built before the first way on)
		Strength = 0.f;
		return;
	}
	EnsureParts();
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	ALyudSea* S = Sea.Get();
	FVector Cam = Hull.GetLocation();
	if (const APlayerCameraManager* PC = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Cam = PC->GetCameraLocation();
	}
	const FVector Fwd = Hull.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const FVector Stb = Hull.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	const float Level = S ? S->SeaLevel() : float(Hull.GetLocation().Z);
	const FVector Mid = Hull.TransformPosition(FVector(LengthCm * 0.5f, 0.f, 0.f));
	const bool bNear = FVector::Dist(Mid, Cam) < NearCm;
	// a head sea throws spray over the bow: the waves run with the sea-state wind
	float HeadSea = 0.f;
	const float HsM = S ? S->WaveHeight() * 0.01f : 0.f;
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
	{
		const FVector W = Wind->GetSeaStateWind().GetSafeNormal2D();
		HeadSea = FMath::Clamp(float(-FVector::DotProduct(W, Fwd)), 0.f, 1.f);
	}
	const LyudSeascape::FWakeRates Rates = LyudSeascape::WakeRates(SpeedMS, HsM, HeadSea);
	Strength = Rates.Strength;
	auto At = [&](float X, float Y) { const FVector P = Hull.TransformPosition(FVector(X, Y, 0.f)); return FVector(P.X, P.Y, Level + 2.f); };
	if (bNear && Rates.Strength > 0.f)
	{
		const float Big = Amount * (0.6f + 0.4f * Rates.Strength);
		// the bow wave: rings from the stem and the shoulders, pushed out along the two arms of the V
		BowDebt += Dt * Rates.BowRings * Amount;
		while (BowDebt >= 1.f && Foam.Num() < ShipMaxFoam)
		{
			BowDebt -= 1.f;
			const float Sd = Rnd.FRand() < 0.5f ? -1.f : 1.f;
			const float X = LengthCm * Rnd.FRandRange(0.80f, 0.995f);
			FFoam F;
			F.P = At(X, Sd * (HalfBeamAt(X) + Rnd.FRandRange(10.f, 60.f)));
			F.V = WaterVelocity + Stb * Sd * Rnd.FRandRange(60.f, 125.f) * (0.5f + 0.5f * Rates.Strength) + Fwd * SpeedMS * 100.f * Rnd.FRandRange(0.05f, 0.2f);
			F.Age = 0.f;
			F.Life = Rnd.FRandRange(6.f, 9.f);
			F.Size = Rnd.FRandRange(0.8f, 1.6f) * Big;
			F.Yaw = Rnd.FRandRange(0.f, 360.f);
			Foam.Add(F);
		}
		BowDebt = FMath::Min(BowDebt, 3.f);
		// streaks sliding along both sides
		SideDebt += Dt * Rates.SideStreaks * Amount;
		while (SideDebt >= 1.f && Foam.Num() < ShipMaxFoam)
		{
			SideDebt -= 1.f;
			const float Sd = Rnd.FRand() < 0.5f ? -1.f : 1.f;
			const float X = LengthCm * Rnd.FRandRange(0.08f, 0.8f);
			FFoam F;
			F.Kind = 1;
			F.P = At(X, Sd * (HalfBeamAt(X) + Rnd.FRandRange(15.f, 50.f)));
			F.V = WaterVelocity + Stb * Sd * Rnd.FRandRange(18.f, 45.f);
			F.Yaw = Fwd.Rotation().Yaw + Rnd.FRandRange(-4.f, 4.f) + Sd * 3.f;
			F.Age = 0.f;
			F.Life = Rnd.FRandRange(8.f, 12.f);
			F.Size = Rnd.FRandRange(0.9f, 1.5f) * Big;
			Foam.Add(F);
		}
		SideDebt = FMath::Min(SideDebt, 3.f);
		// the screw's wash: a band of patches under the counter, left astern as a white road
		SternDebt += Dt * Rates.SternPatches * Amount;
		while (SternDebt >= 1.f && Foam.Num() < ShipMaxFoam)
		{
			SternDebt -= 1.f;
			FFoam F;
			F.Kind = 2;
			F.P = At(-Rnd.FRandRange(40.f, 260.f), Rnd.FRandRange(-0.7f, 0.7f) * HalfBeamCm);
			F.V = WaterVelocity + Stb * Rnd.FRandRange(-14.f, 14.f);
			F.Yaw = Rnd.FRandRange(0.f, 360.f);
			F.Age = 0.f;
			F.Life = Rates.SternLife * Rnd.FRandRange(0.75f, 1.f);
			F.Size = Rnd.FRandRange(1.6f, 2.6f) * Big;
			Foam.Add(F);
		}
		SternDebt = FMath::Min(SternDebt, 3.f);
		// spray at the stem
		SprayDebt += Dt * Rates.Spray * Amount;
		while (SprayDebt >= 1.f && Drops.Num() < ShipMaxDrops)
		{
			SprayDebt -= 1.f;
			const float Sd = Rnd.FRand() < 0.5f ? -1.f : 1.f;
			FDrop D;
			D.P = At(LengthCm * Rnd.FRandRange(0.93f, 1.f), Sd * Rnd.FRandRange(20.f, 120.f)) + FVector(0, 0, 20.f);
			D.V = HullVelocity + Stb * Sd * Rnd.FRandRange(120.f, 380.f) + Fwd * Rnd.FRandRange(-80.f, 160.f) + FVector(0, 0, Rnd.FRandRange(250.f, 620.f));
			D.Life = 3.f;
			D.Size = Rnd.FRandRange(0.03f, 0.08f);
			Drops.Add(D);
		}
		SprayDebt = FMath::Min(SprayDebt, 5.f);
	}
	// droplets fly and fall; on the water they become small foam going with the water
	for (int32 I = Drops.Num() - 1; I >= 0; I--)
	{
		FDrop& D = Drops[I];
		D.V.Z -= ShipGravity * Dt;
		D.V *= 1.f - 0.6f * Dt;
		D.P += D.V * Dt;
		D.Life -= Dt;
		if (D.P.Z < Level || D.Life <= 0.f)
		{
			if (D.P.Z < Level && Foam.Num() < ShipMaxFoam && Rnd.FRand() < 0.25f)
			{
				FFoam F;
				F.P = FVector(D.P.X, D.P.Y, Level);
				F.V = WaterVelocity;
				F.Age = 0.f;
				F.Life = Rnd.FRandRange(1.5f, 3.f);
				F.Size = Rnd.FRandRange(0.15f, 0.35f);
				F.Yaw = Rnd.FRandRange(0.f, 360.f);
				Foam.Add(F);
			}
			Drops.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
	for (int32 I = Foam.Num() - 1; I >= 0; I--)
	{
		FFoam& F = Foam[I];
		F.Age += Dt;
		// the spread dies away, the water's own way stays
		F.V = WaterVelocity + (F.V - WaterVelocity) * (1.f - 0.25f * Dt);
		F.P += F.V * Dt;
		if (F.Age >= F.Life)
		{
			Foam.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
	// draw
	DropTs.Reset();
	for (const FDrop& D : Drops)
	{
		DropTs.Add(FTransform(FQuat::Identity, D.P, FVector(D.Size)));
	}
	FoamTs.Reset();
	TrailTs.Reset();
	for (int32 I = 0; I < Foam.Num(); I++)
	{
		const FFoam& F = Foam[I];
		const float T = F.Age / F.Life;
		float Z = Level + 2.f;
		if (S && (I + GFrameNumber) % 6 == 0)
		{
			Z = S->SurfaceZ(F.P) + 2.f;
			Foam[I].P.Z = Z;
		}
		else if (F.P.Z > Level - 500.f)
		{
			Z = F.P.Z;
		}
		const FVector Where(F.P.X, F.P.Y, Z);
		if (F.Kind == 0)
		{
			// a ring spreads and at its very end shrinks to nothing, so it never pops
			const float R = F.Size * 2.f * (0.6f + 2.4f * T) * (T > 0.85f ? (1.f - T) / 0.15f : 1.f);
			FoamTs.Add(FTransform(FRotator(0.f, F.Yaw + 25.f * T, 0.f), Where, FVector(R, R, 1.f)));
		}
		else if (F.Kind == 1)
		{
			// a streak: long along the side, widening as it ages, gone at the end
			const float Len = F.Size * 2.f * (1.f + 0.4f * T);
			const float Wid = F.Size * 0.7f * (1.f + 1.6f * T) * (1.f - T * T);
			TrailTs.Add(FTransform(FRotator(0.f, F.Yaw, 0.f), Where, FVector(Len, Wid, 1.f)));
		}
		else
		{
			// the wash: a patch that boils up, spreads and thins out over its life
			const float In = FMath::Min(T / 0.04f, 1.f);
			const float R = F.Size * (1.f + 1.5f * T) * In * (T > 0.7f ? (1.f - T) / 0.3f : 1.f);
			TrailTs.Add(FTransform(FRotator(0.f, F.Yaw + 15.f * T, 0.f), Where, FVector(R, R, 1.f)));
		}
	}
	// the train wake's pools: they grow to the most ever shown and never shrink; slots past the live ones are parked at
	// the hull with a zero scale; nothing at all is sent while idle
	const FVector HideAt = Mid;
	auto Push = [&HideAt](UInstancedStaticMeshComponent* Ism, TArray<FTransform>& Ts, int32& Used)
	{
		if (!Ism)
		{
			return;
		}
		const int32 Now = Ts.Num();
		const int32 Span = FMath::Max(Now, Used);
		Used = Now;
		if (Span == 0)
		{
			return;
		}
		const int32 Have = Ism->GetInstanceCount();
		if (Have < Now)
		{
			TArray<FTransform> Add;
			Add.Reserve(Now - Have);
			for (int32 I = Have; I < Now; I++)
			{
				Add.Add(Ts[I]);
			}
			Ism->AddInstances(Add, false, true);
		}
		const FTransform Hidden(FQuat::Identity, HideAt, FVector::ZeroVector);
		const int32 Pool = FMath::Max(Have, Now);
		while (Ts.Num() < Pool)
		{
			Ts.Add(Hidden);
		}
		Ism->BatchUpdateInstancesTransforms(0, Ts, true, false, true);
	};
	Push(DropIsm, DropTs, DropUsed);
	Push(FoamIsm, FoamTs, FoamUsed);
	Push(TrailIsm, TrailTs, TrailUsed);
}
