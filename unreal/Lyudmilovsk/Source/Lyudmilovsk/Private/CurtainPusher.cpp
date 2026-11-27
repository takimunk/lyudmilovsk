#include "CurtainPusher.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialParameterCollectionInstance.h"

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
	// every character of the world is walked only 5 times a second (per curtain, every frame before), keeping those
	// within reach of the range by the next sweep; the frames between look at those few
	SweepIn -= DeltaSeconds;
	if (SweepIn <= 0.f)
	{
		SweepIn = 0.2f;
		Nearby.Reset();
		const float Reach = Range + 500.f;   // a running person covers ~150 cm in 0.2 s: plenty of margin
		for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Here) < Reach)
			{
				Nearby.Add(*It);
			}
		}
	}
	for (const TWeakObjectPtr<ACharacter>& Weak : Nearby)
	{
		const ACharacter* Who = Weak.Get();
		if (!Who)
		{
			continue;
		}
		const FVector P = Who->GetActorLocation();
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
	// nobody in range: the far value goes in once, then the collection is left alone
	const bool bFar = BestDist[0] >= Range;
	if (bFar && bWroteFar)
	{
		return;
	}
	UMaterialParameterCollectionInstance* Instance = CollectionInstance.Get();
	if (!Instance || Instance->GetCollection() != Collection)
	{
		Instance = GetWorld()->GetParameterCollectionInstance(Collection);
		CollectionInstance = Instance;
	}
	if (!Instance)
	{
		return;
	}
	static const FName Pusher0(TEXT("Pusher0"));
	static const FName Pusher1(TEXT("Pusher1"));
	Instance->SetVectorParameterValue(Pusher0, FLinearColor(Best[0]));
	Instance->SetVectorParameterValue(Pusher1, FLinearColor(Best[1]));
	bWroteFar = bFar;
}
