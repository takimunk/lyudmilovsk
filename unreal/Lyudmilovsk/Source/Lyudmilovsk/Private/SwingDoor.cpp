#include "SwingDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

ASwingDoor::ASwingDoor()
{
	// the club's doors stood open by default before the shared door core
	bStartOpen = true;
	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	Zone->SetBoxExtent(FVector(120.f, 150.f, 110.f));
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	RootComponent = Zone;
}

void ASwingDoor::PrepareLeaves()
{
	ClosedRotations.Reset();
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		AActor* Leaf = Leaves[i];
		const float Open = OpenAngles.IsValidIndex(i) ? OpenAngles[i] : 90.f;
		FRotator Closed = Leaf ? Leaf->GetActorRotation() : FRotator::ZeroRotator;
		if (bLeavesPlacedOpen)
		{
			Closed.Yaw -= Open;
		}
		ClosedRotations.Add(Closed);
		if (Leaf)
		{
			if (UStaticMeshComponent* Mesh = Leaf->FindComponentByClass<UStaticMeshComponent>())
			{
				Mesh->SetMobility(EComponentMobility::Movable);
			}
		}
	}
	// the leaves' bounds shut and open (and a little between), in actor space
	SweepBox = FBox(ForceInit);
	const FTransform& T = GetActorTransform();
	TArray<UPrimitiveComponent*> Prims;
	GetLeafPrimitives(Prims);
	for (const float O : { 0.f, 0.5f, 1.f })
	{
		ApplyOpenness(O);
		for (UPrimitiveComponent* P : Prims)
		{
			if (P && P->IsRegistered() && P->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				P->UpdateBounds();
				SweepBox += P->Bounds.GetBox().InverseTransformBy(T);
			}
		}
	}
}

void ASwingDoor::ApplyOpenness(float InOpenness)
{
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		if (Leaves[i] && ClosedRotations.IsValidIndex(i))
		{
			const float Open = OpenAngles.IsValidIndex(i) ? OpenAngles[i] : 90.f;
			Leaves[i]->SetActorRotation(ClosedRotations[i] + FRotator(0.f, Open * InOpenness, 0.f));
		}
	}
}

void ASwingDoor::GetLeafPrimitives(TArray<UPrimitiveComponent*>& Out) const
{
	for (AActor* Leaf : Leaves)
	{
		if (!Leaf)
		{
			continue;
		}
		// the leaf and what rides on it (the blast door's wheel)
		TArray<AActor*> Parts;
		Leaf->GetAttachedActors(Parts, true, true);
		Parts.Add(Leaf);
		for (AActor* Part : Parts)
		{
			TArray<UPrimitiveComponent*> Prims;
			Part->GetComponents<UPrimitiveComponent>(Prims);
			Out.Append(Prims);
		}
	}
}

FBox ASwingDoor::GetDoorwayLocalBox() const
{
	// the Zone is centred on the doorway a metre up; its box stands in for the doorway
	const FVector E = Zone->GetUnscaledBoxExtent();
	return FBox(FVector(-E.X, -E.Y, -100.f), FVector(E.X, E.Y, E.Z * 2.f - 100.f));
}

bool ASwingDoor::PlayerCanUse(const APawn* Pawn, const FVector& Eye, const FVector& Dir, float& OutDistance) const
{
	// as before the shared core: anywhere in the Zone
	if (!bPlayerCanUse || !Pawn || !Zone->IsOverlappingActor(Pawn))
	{
		return false;
	}
	OutDistance = FVector::Dist(Eye, Zone->GetComponentLocation());
	return true;
}
