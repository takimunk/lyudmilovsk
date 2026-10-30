#include "Sky/LyudSunActivity.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SunActivityModel.h"

namespace
{
	TAutoConsoleVariable<float> CVarSunActivity(TEXT("lyud.Sun.Activity"), 1.f,
		TEXT("The red dwarf's activity: births of active regions, prominences and flares x this (0 = a quiet star, 2 = a busy one)."));
	TAutoConsoleVariable<float> CVarSunTimeScale(TEXT("lyud.Sun.TimeScale"), 20.f,
		TEXT("The activity clock (prominences, eruptions, flares) over the game clock: real hours of a star's life pass in game minutes."));
	TAutoConsoleVariable<float> CVarSunForceFlare(TEXT("lyud.Sun.ForceFlare"), 0.f,
		TEXT("Set > 0: a flare starts now at this peak 0..1 on the most visible strong region (set 0, then again, for another)."));
	TAutoConsoleVariable<int32> CVarSunDebug(TEXT("lyud.Sun.Debug"), 0, TEXT("1: print the star's activity on screen."));

	FLinearColor V4(float X, float Y, float Z, float W) { return FLinearColor(X, Y, Z, W); }

	/** The material's parameter names, made once. */
	struct FNames
	{
		FName Region[LyudSunActivity::MaxRegions];
		FName PromA[LyudSunActivity::MaxProminences], PromB[LyudSunActivity::MaxProminences], PromC[LyudSunActivity::MaxProminences];
		FName Flare = TEXT("SunFlare");
		FNames()
		{
			for (int32 I = 0; I < LyudSunActivity::MaxRegions; ++I)
			{
				Region[I] = FName(*FString::Printf(TEXT("SunRegion%d"), I));
			}
			for (int32 I = 0; I < LyudSunActivity::MaxProminences; ++I)
			{
				PromA[I] = FName(*FString::Printf(TEXT("SunPromA%d"), I));
				PromB[I] = FName(*FString::Printf(TEXT("SunPromB%d"), I));
				PromC[I] = FName(*FString::Printf(TEXT("SunPromC%d"), I));
			}
		}
	};
}

ULyudSunActivity* ULyudSunActivity::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudSunActivity>() : nullptr;
}

void ULyudSunActivity::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = MakeShared<LyudSunActivity::FState>();
	LastForceCVar = CVarSunForceFlare.GetValueOnGameThread();
}

void ULyudSunActivity::ForceFlare(float Peak)
{
	bRequestForce = true;
	RequestedPeak = FMath::Clamp(Peak, 0.f, 1.f);
}

float ULyudSunActivity::Update(UMaterialInstanceDynamic* SkyMID, const FDateTime& Local, double RotationDays, float NoonSpin)
{
	using namespace LyudSunActivity;
	if (!State)
	{
		State = MakeShared<FState>();
	}
	FSettings Set;
	Set.RotationDays = RotationDays;
	Set.Activity = FMath::Max(CVarSunActivity.GetValueOnGameThread(), 0.f);
	Set.TimeScale = FMath::Max(CVarSunTimeScale.GetValueOnGameThread(), 0.f);
	const double GameHours = (Local - FDateTime(2026, 1, 1)).GetTotalHours();
	const double Ah = ActivityHours(GameHours, Set);

	const float Force = CVarSunForceFlare.GetValueOnGameThread();
	if (Force != LastForceCVar)
	{
		LastForceCVar = Force;
		if (Force > 0.f)
		{
			ForceFlare(Force);
		}
	}
	if (bRequestForce)
	{
		bRequestForce = false;
		ForcedStartActivityHours = Ah;
		ForcedPeak = RequestedPeak;
	}
	FForcedFlare Forced;
	Forced.StartActivityHours = ForcedStartActivityHours;
	Forced.Peak = ForcedPeak;

	FState& S = *State;
	Evaluate(GameHours, Set, S, &Forced);
	FlareLight = S.FlareLight;
	LimbProms = S.LimbProms;

	if (SkyMID)
	{
		static const FNames N;
		for (int32 I = 0; I < MaxRegions; ++I)
		{
			const FRegion& R = S.Regions[I];
			SkyMID->SetVectorParameterValue(N.Region[I], I < S.NumRegions ? V4(R.Lat, R.Lon, R.Strength, R.Seed) : V4(0.f, 0.f, 0.f, 0.f));
		}
		for (int32 I = 0; I < MaxProminences; ++I)
		{
			const FProminence& P = S.Proms[I];
			if (I < S.NumProms)
			{
				SkyMID->SetVectorParameterValue(N.PromA[I], V4(P.A[0], P.A[1], P.A[2], P.Height));
				SkyMID->SetVectorParameterValue(N.PromB[I], V4(P.T[0], P.T[1], P.T[2], P.HalfWidth));
				SkyMID->SetVectorParameterValue(N.PromC[I], V4(P.Bright, float(uint8(P.Kind)), P.Lift, P.Seed));
			}
			else
			{
				SkyMID->SetVectorParameterValue(N.PromC[I], V4(0.f, 0.f, 0.f, 0.f));
			}
		}
		// the flare in the rare sky's own format (rare_hlsl.py FLARE_HLSL): latitude, longitude from the noon face,
		// strength now, peak
		const FFlare& F = S.Flare;
		const float Lon = FMath::UnwindRadians(F.Lon - NoonSpin);
		SkyMID->SetVectorParameterValue(N.Flare, V4(F.Lat, Lon, F.Strength, FMath::Max(F.Peak, 0.05f)));
	}

	DebugLine = FString::Printf(TEXT("SUNACT regions %d (sent %d) | prominences %d alive, %d on the limb, %d erupting, %d sent | flare %.2f (peak %.2f, %.1f min, face %.2f) light %.2f | spin %.1f deg"),
		S.AliveRegions, S.NumRegions, S.AliveProms, S.LimbProms, S.Eruptions, S.NumProms, S.Flare.Strength, S.Flare.Peak,
		S.Flare.AgeMin, S.Flare.Face, S.FlareLight, FMath::RadiansToDegrees(S.Spin));
	if (CVarSunDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x5A11), 0.f, FColor::Orange, DebugLine);
	}
	return FlareLight;
}
