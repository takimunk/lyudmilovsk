#include "LightFlicker.h"

#include "Components/LightComponent.h"
#include "EngineUtils.h"

ALightFlicker::ALightFlicker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALightFlicker::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bFar = It->ActorHasTag(FarTag);
		if (!bFar && !It->ActorHasTag(FlickerTag))
		{
			continue;
		}
		const float Far = bFar ? FMath::Clamp((FarStartY - It->GetActorLocation().Y) / (FarStartY - FarEndY), 0.f, 1.f) : 0.f;
		const float Curve = FMath::Pow(Far, 0.6f);
		TArray<ULightComponent*> Found;
		It->GetComponents(Found);
		for (ULightComponent* Light : Found)
		{
			FFlickeringLight& F = Lights.AddDefaulted_GetRef();
			F.Light = Light;
			F.Far = Curve;
			F.BaseIntensity = Light->Intensity * FMath::Lerp(1.f, FarGlow, Curve);
			F.NextBlink = FMath::FRandRange(0.5f, FMath::Lerp(Interval.Y, FarInterval.Y, Curve));
		}
	}
}

void ALightFlicker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	for (FFlickeringLight& F : Lights)
	{
		ULightComponent* Light = F.Light.Get();
		if (!Light)
		{
			continue;
		}
		if (Clock >= F.NextBlink && F.Dips.Num() == 0)
		{
			// One quick dip, sometimes a double blink.
			const float Start = Clock;
			const float Len = FMath::FRandRange(0.04f, 0.12f) * (1.f + 3.f * F.Far);
			F.Dips.Add(FVector2f(Start, Start + Len));
			if (FMath::FRand() < FMath::Lerp(0.35f, 0.8f, F.Far))
			{
				const float Gap = FMath::FRandRange(0.06f, 0.15f);
				F.Dips.Add(FVector2f(Start + Len + Gap, Start + 2.f * Len + Gap));
			}
		}

		bool bDim = false;
		for (const FVector2f& Dip : F.Dips)
		{
			bDim |= Clock >= Dip.X && Clock < Dip.Y;
		}
		if (F.Dips.Num() > 0 && Clock >= F.Dips.Last().Y)
		{
			F.Dips.Reset();
			F.NextBlink = Clock + FMath::FRandRange(FMath::Lerp(Interval.X, FarInterval.X, F.Far), FMath::Lerp(Interval.Y, FarInterval.Y, F.Far));
		}
		Light->SetIntensity(bDim ? F.BaseIntensity * FMath::Lerp(DimTo, 0.f, F.Far) : F.BaseIntensity);
	}
}
