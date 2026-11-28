// A race mark, the start line's pin, a training buoy or the man-overboard buoy: one mesh that rides the sea where it
// is put. ULyudSailing calls Advance for the ones near the camera (the actor never ticks).
// docs/yacht_school.md "Engine: the boats".
#include "Sailing/LyudDinghy.h"

#include "LyudDinghyImpl.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Sea/LyudSea.h"
#include "Weather/LyudWind.h"

ALyudSailMark::ALyudSailMark()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	LyudSailKit::NoLumen(Body);
	SetHidden(true);
}

void ALyudSailMark::Setup(int32 InKind)
{
	static const TCHAR* const Assets[4] = { TEXT("Mark"), TEXT("Pin"), TEXT("Buoy"), TEXT("MOB") };
	Kind = FMath::Clamp(InKind, 0, 3);
	if (BuiltKind == Kind)
	{
		return;
	}
	BuiltKind = Kind;
	Body->EmptyOverrideMaterials();
	if (UStaticMesh* M = LyudSailKit::Asset(Assets[Kind]))
	{
		Body->SetStaticMesh(M);
	}
	else
	{
		const LyudSailKit::FBuilt& Built = LyudSailKit::MarkBody(Kind);
		Body->SetStaticMesh(Built.Mesh);
		LyudSailKit::PaintBuilt(Body, Built, FLinearColor(0.95f, 0.32f, 0.04f), FLinearColor::White);
	}
}

void ALyudSailMark::PlaceAt(const FVector2D& InPos)
{
	if (BuiltKind < 0)
	{
		Setup(Kind);
	}
	Pos = InPos;
	bStowed = false;
	const FVector At(Pos.X * 100.0, Pos.Y * 100.0, 0.0);
	const ALyudSea* Sea = ALyudSea::Get(this);
	// far ones sit at the still level; Advance lifts the near ones onto the waves
	SetActorLocation(FVector(At.X, At.Y, Sea ? Sea->SeaLevel() : float(GetActorLocation().Z)));
	SetActorHiddenInGame(false);
}

void ALyudSailMark::Stow()
{
	bStowed = true;
	SetActorHiddenInGame(true);
}

void ALyudSailMark::Advance(float DeltaTime)
{
	if (bStowed || DeltaTime <= 0.f)
	{
		return;
	}
	const ALyudSea* Sea = ALyudSea::Get(this);
	if (!Sea)
	{
		return;
	}
	Clock += DeltaTime;
	const FVector At(Pos.X * 100.0, Pos.Y * 100.0, 0.0);
	const FLyudSeaSample S = Sea->SampleSurface(At);
	// it leans with the water's slope, late and a little more (an anchored float), and nods on its own
	const float WantPitch = FMath::RadiansToDegrees(float(FMath::Atan2(-S.Normal.X, S.Normal.Z))) * 1.3f;
	const float WantRoll = FMath::RadiansToDegrees(float(FMath::Atan2(S.Normal.Y, S.Normal.Z))) * 1.3f;
	LeanPitch = FMath::FInterpTo(LeanPitch, WantPitch, DeltaTime, 3.f);
	LeanRoll = FMath::FInterpTo(LeanRoll, WantRoll, DeltaTime, 3.f);
	const float Nod = 2.5f * FMath::Sin(Clock * 1.7f + float(Pos.X));
	float YawDeg = float(GetActorRotation().Yaw);
	if (Kind == 1 || Kind == 3)
	{
		// the ones with a flag swing to stream it downwind (the mesh's flag flies along +X)
		if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(this))
		{
			const FVector V = Wind->GetWindAt(FVector(At.X, At.Y, S.SurfaceZ), 2.f, true).Velocity;
			if (V.SizeSquared2D() > 0.25)
			{
				const float Want = FMath::RadiansToDegrees(float(FMath::Atan2(V.Y, V.X)));
				YawDeg += FMath::Clamp(FMath::FindDeltaAngleDegrees(YawDeg, Want), -90.f * DeltaTime, 90.f * DeltaTime);
			}
		}
	}
	SetActorLocationAndRotation(FVector(At.X, At.Y, S.SurfaceZ), FRotator(0.f, YawDeg, 0.f));
	// the lean is the water's (a world slope), the actor's yaw is the wind's: turned back into the actor's frame
	const float Y = FMath::DegreesToRadians(YawDeg);
	const float C = FMath::Cos(Y), Sn = FMath::Sin(Y);
	Body->SetRelativeRotation(FRotator(LeanPitch * C - LeanRoll * Sn + Nod, 0.f, LeanRoll * C + LeanPitch * Sn));
}
