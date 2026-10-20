#include "CurtainPusher.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMaterialLibrary.h"

ACurtainPusher::ACurtainPusher()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACurtainPusher::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Collection)
	{
		return;
	}
	// The two closest characters within range push the strips; far away = no push.
	const FVector Here = GetActorLocation();
	FVector Best[2] = {FVector(0, 0, -100000.f), FVector(0, 0, -100000.f)};
	float BestDist[2] = {Range, Range};
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		const FVector P = It->GetActorLocation();
		const float D = FVector::Dist2D(P, Here);
		if (D < BestDist[0])
		{
			Best[1] = Best[0];
			BestDist[1] = BestDist[0];
			Best[0] = P;
			BestDist[0] = D;
		}
		else if (D < BestDist[1])
		{
			Best[1] = P;
			BestDist[1] = D;
		}
	}
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Pusher0"), FLinearColor(Best[0]));
	UKismetMaterialLibrary::SetVectorParameterValue(this, Collection, TEXT("Pusher1"), FLinearColor(Best[1]));
}
