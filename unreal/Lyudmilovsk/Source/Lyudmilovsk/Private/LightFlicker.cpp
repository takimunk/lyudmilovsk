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
		if (!It->ActorHasTag(FlickerTag))
		{
			continue;
		}
		TArray<ULightComponent*> Found;
		It->GetComponents(Found);
		for (ULightComponent* Light : Found)
		{
			FFlickeringLight& F = Lights.AddDefaulted_GetRef();
			F.Light = Light;
			F.BaseIntensity = Light->Intensity;
			F.NextBlink = FMath::FRandRange(0.5f, Interval.Y);
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
			const float Len = FMath::FRandRange(0.04f, 0.12f);
			F.Dips.Add(FVector2f(Start, Start + Len));
			if (FMath::FRand() < 0.35f)
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
			F.NextBlink = Clock + FMath::FRandRange(Interval.X, Interval.Y);
		}
		Light->SetIntensity(bDim ? F.BaseIntensity * DimTo : F.BaseIntensity);
	}
}
