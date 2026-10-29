#include "Railway/LyudTrainWake.h"

#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "../Transport/LyudMeshKit.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sea/LyudSea.h"
#include "Sound/SoundBase.h"

namespace
{
constexpr int32 MaxDrops = 420;
constexpr int32 MaxFoam = 340;
constexpr float Gravity = 980.f;
}

ULyudTrainWake::ULyudTrainWake()
{
	PrimaryComponentTick.bCanEverTick = false;
	Rnd.Initialize(1958);
}

void ULyudTrainWake::EnsureParts()
{
	if (DropIsm)
	{
		return;
	}
	AActor* Owner = GetOwner();
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// see-through white: the cable car's glass master (Color, Opacity) reads as spray and thin foam on the water
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Make = [&](UStaticMesh* M, const TCHAR* Name, const FLinearColor& Col, float Opacity)
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(Owner, Name);
		I->SetStaticMesh(M);
		I->SetMobility(EComponentMobility::Movable);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		I->SetCastShadow(false);
		I->SetUsingAbsoluteLocation(true);
		I->SetUsingAbsoluteRotation(true);
		I->SetUsingAbsoluteScale(true);
		I->LightingChannels.bChannel1 = true;
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
	DropIsm = Make(Sphere, TEXT("WakeDrops"), FLinearColor(0.85f, 0.9f, 0.9f), 0.55f);
	// a ring of foam: a flat annulus 50 cm across its outside, both faces (the kit builds it once)
	{
		FLyudMeshKit K;
		K.Lathe(FTransform::Identity, { FVector2D(36.0, 0.0), FVector2D(50.0, 0.0) }, { TEXT("foam") }, 28, false, false);
		K.Lathe(FTransform::Identity, { FVector2D(50.0, 0.2), FVector2D(36.0, 0.2) }, { TEXT("foam") }, 28, false, false);
		TArray<FName> Slots;
		RingMesh = K.Build(Owner, TEXT("WakeRing"), Slots);
	}
	FoamIsm = Make(RingMesh ? RingMesh.Get() : Disc, TEXT("WakeFoam"), FLinearColor(0.86f, 0.9f, 0.88f), 0.45f);
	TrailIsm = Make(Disc, TEXT("WakeTrail"), FLinearColor(0.8f, 0.85f, 0.84f), 0.22f);
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Railway/S_splash_01.S_splash_01"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Hiss = NewObject<UAudioComponent>(Owner, TEXT("WakeHiss"));
		Hiss->SetSound(S);
		Hiss->bAutoActivate = false;
		Hiss->SetupAttachment(this);
		Hiss->RegisterComponent();
		Owner->AddInstanceComponent(Hiss);
	}
}

void ULyudTrainWake::Feed(const TArray<FVector>& Wheels, const FVector& Velocity, float DeltaSeconds)
{
	EnsureParts();
	if (!Sea.IsValid())
	{
		Sea = ALyudSea::Get(this);
	}
	ALyudSea* S = Sea.Get();
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const float Speed = Velocity.Size2D();
	FVector Cam = FVector::ZeroVector;
	if (const APlayerCameraManager* PC = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Cam = PC->GetCameraLocation();
	}
	// how wet the wheels are: the water's height over each rail head, from the real waves and tide
	float WetSum = 0.f;
	TArray<TPair<FVector, float>> WetWheels;
	if (S && Wheels.Num())
	{
		for (const FVector& W : Wheels)
		{
			const FLyudSeaSample Sm = S->SampleSurface(W);
			const float Over = Sm.SurfaceZ - W.Z;
			if (Sm.bIsSea && Sm.Depth > 0.f && Over > -6.f)
			{
				const float K = FMath::Clamp((Over + 6.f) / 30.f, 0.f, 1.f);
				WetSum += K;
				WetWheels.Add({ FVector(W.X, W.Y, Sm.SurfaceZ), K });
			}
		}
		Wet = WetSum / Wheels.Num();
	}
	else
	{
		Wet = 0.f;
	}
	const bool bNear = Wheels.Num() && FVector::Dist(Wheels[0], Cam) < NearCm;
	// spray: each wet wheel throws droplets out to both sides and up, more the faster it goes
	if (bNear && WetWheels.Num() && Speed > 80.f)
	{
		EmitDebt += Dt * WetWheels.Num() * FMath::Clamp(Speed / 1500.f, 0.f, 1.f) * 70.f;
		const FVector Fwd = Velocity.GetSafeNormal2D();
		const FVector Side(-Fwd.Y, Fwd.X, 0.f);
		while (EmitDebt >= 1.f && Drops.Num() < MaxDrops)
		{
			EmitDebt -= 1.f;
			const TPair<FVector, float>& W = WetWheels[Rnd.RandHelper(WetWheels.Num())];
			const float Sd = Rnd.FRand() < 0.5f ? -1.f : 1.f;
			const float Out = Rnd.FRandRange(150.f, 420.f) * (0.4f + 0.6f * W.Value) * FMath::Clamp(Speed / 1200.f, 0.3f, 1.2f);
			FDrop D;
			D.P = W.Key + Side * Sd * Rnd.FRandRange(50.f, 80.f) + FVector(0, 0, 4.f);
			D.V = Side * Sd * Out + Fwd * Speed * Rnd.FRandRange(0.05f, 0.25f) + FVector(0, 0, Rnd.FRandRange(150.f, 430.f) * (0.5f + 0.5f * W.Value));
			D.Life = 3.f;
			D.Size = Rnd.FRandRange(0.025f, 0.07f);
			Drops.Add(D);
		}
		EmitDebt = FMath::Min(EmitDebt, 5.f);
		// the bank's flanks shove a V of foam outward from the front wheels
		FoamDebt += Dt * FMath::Clamp(Speed / 1500.f, 0.f, 1.f) * 14.f * FMath::Min(1.f, float(WetWheels.Num()));
		while (FoamDebt >= 1.f && Foam.Num() < MaxFoam)
		{
			FoamDebt -= 1.f;
			const FVector Front = WetWheels[0].Key;
			const float Sd = Rnd.FRand() < 0.5f ? -1.f : 1.f;
			FFoam F;
			F.P = Front + Side * Sd * Rnd.FRandRange(260.f, 320.f) - Fwd * Rnd.FRandRange(0.f, 200.f);
			F.V = Side * Sd * Rnd.FRandRange(50.f, 110.f) + Fwd * Speed * 0.04f;
			F.Age = 0.f;
			F.Life = Rnd.FRandRange(4.f, 7.f);
			F.Size = Rnd.FRandRange(0.5f, 1.0f);
			Foam.Add(F);
		}
		FoamDebt = FMath::Min(FoamDebt, 3.f);
		// the trail: a streak along each flank every 1.6 m run behind the last wet wheel, widening and drifting off
		TrailAt = FMath::Min(TrailAt, double(Speed) * 2.0);
		TrailAt -= Speed * Dt;
		while (TrailAt <= 0.0 && Foam.Num() < MaxFoam - 2)
		{
			TrailAt += 160.0;
			const FVector Back = WetWheels.Last().Key;
			for (int32 Sd = -1; Sd <= 1; Sd += 2)
			{
				FFoam F;
				F.Kind = 1;
				F.Yaw = Fwd.Rotation().Yaw + Rnd.FRandRange(-4.f, 4.f) + Sd * 3.f;
				F.P = Back + Side * Sd * Rnd.FRandRange(240.f, 300.f) - Fwd * Rnd.FRandRange(50.f, 250.f);
				F.V = Side * Sd * Rnd.FRandRange(25.f, 55.f);
				F.Age = 0.f;
				F.Life = Rnd.FRandRange(9.f, 14.f);
				F.Size = Rnd.FRandRange(0.8f, 1.2f);
				Foam.Add(F);
			}
		}
	}
	// droplets fly and fall; on the water they become small foam
	for (int32 I = Drops.Num() - 1; I >= 0; I--)
	{
		FDrop& D = Drops[I];
		D.V.Z -= Gravity * Dt;
		D.V *= 1.f - 0.6f * Dt;
		D.P += D.V * Dt;
		D.Life -= Dt;
		const float Water = S ? S->SeaLevel() : D.P.Z - 1.f;
		if (D.P.Z < Water || D.Life <= 0.f)
		{
			if (D.P.Z < Water && Foam.Num() < MaxFoam && Rnd.FRand() < 0.25f)
			{
				FFoam F;
				F.P = FVector(D.P.X, D.P.Y, Water);
				F.V = FVector(D.V.X, D.V.Y, 0.f) * 0.1f;
				F.Age = 0.f;
				F.Life = Rnd.FRandRange(1.5f, 3.f);
				F.Size = Rnd.FRandRange(0.15f, 0.35f);
				Foam.Add(F);
			}
			Drops.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
	for (int32 I = Foam.Num() - 1; I >= 0; I--)
	{
		FFoam& F = Foam[I];
		F.Age += Dt;
		F.V *= 1.f - 0.25f * Dt;
		F.P += F.V * Dt;
		if (F.Age >= F.Life)
		{
			Foam.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
	// draw: droplets as small spheres, foam as flat discs riding the surface, growing and thinning out
	TArray<FTransform> DT;
	DT.Reserve(Drops.Num());
	for (const FDrop& D : Drops)
	{
		DT.Add(FTransform(FQuat::Identity, D.P, FVector(D.Size)));
	}
	TArray<FTransform> FT, TT;
	FT.Reserve(Foam.Num());
	const float Level = S ? S->SeaLevel() : 0.f;
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
		const FVector At(F.P.X, F.P.Y, Z);
		if (F.Kind == 0)
		{
			// a ring spreads out and its band thins away (the mesh's band is fixed: the ring grows, then shrinks to nothing at
			// its very end so it never pops)
			const float R = F.Size * 2.f * (0.6f + 2.4f * T) * (T > 0.85f ? (1.f - T) / 0.15f : 1.f);
			FT.Add(FTransform(FQuat::Identity, At, FVector(R, R, 1.f)));
		}
		else
		{
			// a streak: long along the way, widening as it ages, gone at the end
			const float Len = F.Size * 3.2f * (1.f + 0.4f * T);
			const float Wid = F.Size * 0.45f * (1.f + 2.2f * T) * (1.f - T * T);
			TT.Add(FTransform(FRotator(0.f, F.Yaw, 0.f), At, FVector(Len, Wid, 0.004f)));
		}
	}
	auto Push = [](UInstancedStaticMeshComponent* Ism, const TArray<FTransform>& Ts)
	{
		if (!Ism)
		{
			return;
		}
		const int32 Have = Ism->GetInstanceCount();
		if (Have < Ts.Num())
		{
			TArray<FTransform> Add;
			for (int32 I = Have; I < Ts.Num(); I++)
			{
				Add.Add(Ts[I]);
			}
			Ism->AddInstances(Add, false, true);
		}
		else if (Have > Ts.Num())
		{
			TArray<int32> Rem;
			for (int32 I = Ts.Num(); I < Have; I++)
			{
				Rem.Add(I);
			}
			Ism->RemoveInstances(Rem);
		}
		if (Ts.Num())
		{
			Ism->BatchUpdateInstancesTransforms(0, Ts, true, true, true);
		}
	};
	Push(DropIsm, DT);
	Push(FoamIsm, FT);
	Push(TrailIsm, TT);
	if (Hiss)
	{
		const float Vol = bNear ? FMath::Clamp(Wet * Speed / 900.f, 0.f, 1.2f) : 0.f;
		if (Vol > 0.02f)
		{
			if (!Hiss->IsPlaying())
			{
				Hiss->Play();
			}
			Hiss->SetVolumeMultiplier(Vol);
			Hiss->SetWorldLocation(Wheels.Num() ? Wheels[0] : GetComponentLocation());
		}
		else if (Hiss->IsPlaying())
		{
			Hiss->Stop();
		}
	}
}
