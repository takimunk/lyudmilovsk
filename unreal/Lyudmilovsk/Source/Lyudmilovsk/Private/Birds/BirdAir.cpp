#include "BirdAir.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "Plants/GameCalendar.h"

namespace
{
	TAutoConsoleVariable<float> CVarWindSpeed(TEXT("lyud.Bird.WindSpeed"), 5.f, TEXT("Sea wind for the player bird, m/s."));
	TAutoConsoleVariable<float> CVarWindFrom(TEXT("lyud.Bird.WindFrom"), 247.5f, TEXT("Compass bearing the wind blows from, degrees (247.5 = west-south-west)."));
	TAutoConsoleVariable<float> CVarThermals(TEXT("lyud.Bird.Thermals"), 1.f, TEXT("Thermal strength scale for the player bird (0 = none)."));

	/** Deterministic 0..1 noise from a cell. */
	float Hash(int32 X, int32 Y, int32 K)
	{
		uint32 H = (uint32)X * 73856093u ^ (uint32)Y * 19349663u ^ (uint32)K * 83492791u;
		H ^= H >> 13; H *= 0x5bd1e995u; H ^= H >> 15;
		return (H & 0xffffff) / float(0xffffff);
	}
}

void FBirdAir::Init(UWorld* InWorld)
{
	World = InWorld;
	FBox Box(ForceInit);
	if (InWorld)
	{
		for (TActorIterator<ALandscapeProxy> It(InWorld); It; ++It)
		{
			Box += It->GetComponentsBoundingBox(true);
		}
	}
	if (Box.IsValid)
	{
		WorldBounds = FBox(FVector(Box.Min.X, Box.Min.Y, SeaZ - 500.f), FVector(Box.Max.X, Box.Max.Y, SeaZ + 30000.f));
	}
	CacheAge = 1e9f;
}

float FBirdAir::GroundZ(const FVector2D& XY) const
{
	float Z = SeaZ;
	if (UWorld* W = World.Get())
	{
		FHitResult Hit;
		FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BirdAirGround), false);
		if (W->LineTraceSingleByObjectType(Hit, FVector(XY, 60000.f), FVector(XY, SeaZ - 2000.f), Objects, Params))
		{
			Z = FMath::Max(Z, (float)Hit.ImpactPoint.Z);
		}
	}
	return Z;
}

FBirdAir::FSample FBirdAir::Sample(const FVector& PosCm, float Dt)
{
	Clock += Dt;
	FSample S;
	const float From = FMath::DegreesToRadians(CVarWindFrom.GetValueOnGameThread() + 180.f);
	// compass to world: north = +Y, east = -X (ALyudSky)
	const FVector Dir(-FMath::Sin(From), FMath::Cos(From), 0.f);
	const float Gust = 1.f + 0.22f * FMath::Sin(Clock * 0.37) + 0.12f * FMath::Sin(Clock * 1.13 + 1.7);
	const float Speed = FMath::Max(0.f, CVarWindSpeed.GetValueOnGameThread()) * Gust;
	// over the ground the wind drops off (a bird near the grass is in calm air)
	CacheAge += Dt;
	if (CacheAge > 0.15f || FVector::Dist2D(CachePos, PosCm) > 600.f)
	{
		CacheAge = 0.f;
		CachePos = PosCm;
		const FVector2D P(PosCm);
		const FVector2D D(Dir);
		CacheGround = GroundZ(P);
		const float H1 = GroundZ(P + D * 2500.f), H2 = GroundZ(P + D * 5000.f);
		CacheTop = FMath::Max3(CacheGround, H1, H2);
		CacheSlope = FMath::Max((H1 - CacheGround) / 2500.f, (H2 - CacheGround) / 5000.f);
		if (CacheSlope <= 0.f)
		{
			CacheSlope = FMath::Min((H1 - CacheGround) / 2500.f, 0.f);
		}
	}
	S.GroundZ = CacheGround;
	const float Agl = FMath::Max(0.f, PosCm.Z - CacheGround) / 100.f;
	const float Near = FMath::SmoothStep(0.f, 6.f, Agl);
	S.Wind = Dir * Speed * (0.35f + 0.65f * Near);

	// ridge lift: the wind deflected up by the ground rising ahead of it, fading above the crest
	const float Relief = FMath::Max(8.f, (CacheTop - CacheGround) / 100.f);
	const float OverTop = (PosCm.Z - CacheTop) / 100.f;
	const float Fade = OverTop < 0.f ? 1.f : FMath::Exp(-OverTop / (1.4f * Relief));
	const float Slope = FMath::Clamp(CacheSlope, -0.6f, 1.3f);
	S.Ridge = Speed * Slope * Fade * (Slope < 0.f ? 0.35f : 1.f) * Near;

	// thermals: columns over land by day, ~250 m apart, drifting with the wind
	const float Hour = (float)UGameCalendar::NowIn(World.Get()).GetTimeOfDay().GetTotalHours();
	const float Day = FMath::SmoothStep(9.5f, 11.5f, Hour) * (1.f - FMath::SmoothStep(16.f, 17.5f, Hour)) * CVarThermals.GetValueOnGameThread();
	if (Day > 0.f && CacheGround > SeaZ + 300.f)
	{
		const FVector2D Drift = FVector2D(Dir) * Speed * 40.f * (float)Clock;
		const FVector2D Q = FVector2D(PosCm) - Drift;
		constexpr float Cell = 25000.f;
		const int32 CX = FMath::FloorToInt(Q.X / Cell), CY = FMath::FloorToInt(Q.Y / Cell);
		float Best = 0.f;
		for (int32 Y = CY - 1; Y <= CY + 1; Y++)
		{
			for (int32 X = CX - 1; X <= CX + 1; X++)
			{
				if (Hash(X, Y, 0) < 0.35f) continue;
				const FVector2D C((X + Hash(X, Y, 1)) * Cell, (Y + Hash(X, Y, 2)) * Cell);
				const float R = 3500.f + 2500.f * Hash(X, Y, 3);
				const float W = (0.8f + 1.6f * Hash(X, Y, 4)) * FMath::Exp(-FVector2D::DistSquared(Q, C) / (R * R));
				Best = FMath::Max(Best, W);
			}
		}
		S.Thermal = Best * Day * FMath::SmoothStep(0.f, 15.f, Agl) * (1.f - FMath::SmoothStep(400.f, 700.f, Agl));
	}
	S.Updraft = S.Ridge + S.Thermal;
	return S;
}
