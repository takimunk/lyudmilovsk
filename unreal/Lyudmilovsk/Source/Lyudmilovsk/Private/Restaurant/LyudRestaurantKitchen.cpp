// The restaurant's kitchen (docs/restaurant.md "The kitchen"): the cook at the range (the town life's cook on shift through
// LyudRestaurantStaff, or the stand-in of restaurant.json standin.cook_mesh) with a pan in the right hand that is shaken while anything
// cooks, a stock pot beside it, the order tickets over the pass. A dish that is ready comes out onto the pass's shelf and the chrome
// bell there is rung; the waiter takes it from the shelf. The room's tone and quiet music while open, the kitchen's clatter, the
// sizzle while cooking (ULyudSoundscapeSubsystem's rest_* pools, /Game/Audio/Restaurant).

#include "Restaurant/LyudRestaurant.h"

#include "LyudRestaurantStaff.h"

#include "Audio/LyudSoundscape.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"

void ALyudRestaurant::Effect(FName Pool, const FVector& Where, float Volume) const
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, Where, Volume);
	}
}

AStaticMeshActor* ALyudRestaurant::SpawnProp(FName Mesh, const FTransform& At)
{
	UStaticMesh* M = RestMesh(Mesh);
	if (!M)
	{
		return nullptr;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* A = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), At, SP);
	A->SetMobility(EComponentMobility::Movable);
	A->Tags.Add(TEXT("Restaurant"));
	UStaticMeshComponent* C = A->GetStaticMeshComponent();
	C->SetStaticMesh(M);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->SetCanEverAffectNavigation(false);
	KitchenProps.Add(A);
	return A;
}

void ALyudRestaurant::PutOnPass(FOrder& O)
{
	TSet<int32> Taken;
	for (const FOrder& Other : Orders)
	{
		if (Other.State == FOrder::EState::Ready && Other.Dish.IsValid())
		{
			Taken.Add(Other.Shelf);
		}
	}
	int32 Shelf = -1;
	for (int32 K = 0; K < PassShelf.Num() && Shelf < 0; ++K)
	{
		Shelf = Taken.Contains(K) ? -1 : K;
	}
	if (Shelf < 0)
	{
		// the shelf is full: it waits in the kitchen a little longer
		O.State = FOrder::EState::Cooking;
		O.Due = GetWorld()->GetTimeSeconds() + 3.f;
		return;
	}
	O.Shelf = Shelf;
	O.Dish = SpawnDish(O.Item, FTransform(YawToWorld(PassYaw), ToWorld(PassShelf[Shelf])), 1.f);
	if (ALyudRestDish* D = O.Dish.Get())
	{
		D->OrderId = O.Id;
	}
	BellT = 0.f;
	Effect(TEXT("rest_bell"), ToWorld(BellAt), 0.8f);
	Effect(TEXT("rest_plate"), ToWorld(PassShelf[Shelf]), 0.5f);
	UE_LOG(LogTemp, Display, TEXT("RESTTEST on the pass: %s (shelf %d), the bell"), *O.Item.ToString(), Shelf);
}

void ALyudRestaurant::SetLoops(bool bOpenNow, bool bCooking)
{
	ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this);
	if (!S)
	{
		return;
	}
	if (SoundSpots.Num() == 0)
	{
		// the room's middle, by the bar (the radio), in the kitchen, at the range
		const FVector Spots[] = { FVector(1.5, 0.0, 1.6), FVector(WaiterHome.X, WaiterHome.Y, 2.2),
			FVector(0.5 * (KitchenMin.X + KitchenMax.X), 0.5 * (KitchenMin.Y + KitchenMax.Y), 1.8), FVector(PassStand.X - 2.5, PassStand.Y, 1.0) };
		for (const FVector& P : Spots)
		{
			USceneComponent* C = NewObject<USceneComponent>(this);
			C->SetupAttachment(RootComponent);
			C->SetWorldLocation(ToWorld(P));
			C->RegisterComponent();
			SoundSpots.Add(C);
		}
	}
	auto Loop = [&](TObjectPtr<UAudioComponent>& A, bool bOn, FName Pool, int32 Spot, float Volume)
	{
		if (bOn && !A)
		{
			A = S->AttachLoop(Pool, SoundSpots[Spot], Volume);
		}
		else if (!bOn && A)
		{
			// gone once faded (not left behind as a stopped component)
			A->bAutoDestroy = true;
			A->FadeOut(1.5f, 0.f);
			A = nullptr;
		}
	};
	Loop(RoomLoop, bOpenNow, TEXT("rest_room"), 0, 0.45f);
	Loop(MusicLoop, bOpenNow, TEXT("rest_music"), 1, 0.3f);
	Loop(KitchenLoop, bOpenNow, TEXT("rest_kitchen"), 2, 0.45f);
	Loop(SizzleLoop, bOpenNow && bCooking, TEXT("rest_sizzle"), 3, 0.5f);
}

void ALyudRestaurant::ReleaseCook()
{
	if (CookPerson >= 0)
	{
		LyudRestaurantStaff::SetPresentedExternally(this, CookPerson, false);
	}
	CookPerson = -1;
	if (CookBody)
	{
		CookBody->Destroy();
		CookBody = nullptr;
	}
}

void ALyudRestaurant::TickKitchen(float DeltaTime)
{
	UWorld* World = GetWorld();
	const bool bCooking = Orders.ContainsByPredicate([](const FOrder& O) { return O.State == FOrder::EState::Cooking; });
	SetLoops(bOpen, bCooking);

	// the props once: the pan and its food on the range, the stock pot, the bell and the ticket rail on the pass
	if (KitchenProps.Num() == 0 && bLayout && PassShelf.Num() > 0)
	{
		const float CA = FMath::DegreesToRadians(CookYaw);
		const FVector F(FMath::Cos(CA), FMath::Sin(CA), 0.f), R(FMath::Sin(CA), -FMath::Cos(CA), 0.f);
		const FVector Range = CookAt + F * 0.62f + FVector(0, 0, 0.92f);
		Pan = SpawnProp(TEXT("SM_RT_FryPan"), FTransform(YawToWorld(CookYaw + 180.f), ToWorld(Range + R * 0.08f)));
		PanFood = SpawnProp(TEXT("SM_RT_PanFood"), FTransform(YawToWorld(CookYaw + 180.f), ToWorld(Range + R * 0.08f + FVector(0, 0, 0.005f))));
		SpawnProp(TEXT("SM_RT_StockPot"), FTransform(YawToWorld(CookYaw), ToWorld(Range - R * 0.42f)));
		// the shelf's spots run along the pass: the bell beyond the last, the ticket rail over the middle on the kitchen side
		const FVector First = PassShelf[0], Last = PassShelf.Last();
		const FVector Along = (Last - First).GetSafeNormal();
		BellAt = Last + Along * 0.28f;
		SpawnProp(TEXT("SM_RT_ServiceBell"), FTransform(YawToWorld(PassYaw), ToWorld(BellAt)));
		BellPlunger = SpawnProp(TEXT("SM_RT_ServiceBell_Plunger"), FTransform(YawToWorld(PassYaw), ToWorld(BellAt)));
		const FVector Mid = 0.5f * (First + Last);
		const FVector Kitchenward = FVector(-1.f, 0.f, 0.f);
		SpawnProp(TEXT("SM_RT_TicketRail"), FTransform(YawToWorld(PassYaw + 180.f), ToWorld(FVector(Mid.X, Mid.Y, 0.f) + Kitchenward * 0.35f + FVector(0, 0, 1.92f))));
		SpawnProp(TEXT("SM_RT_PlateStack"), FTransform(YawToWorld(CookYaw), ToWorld(Range - R * 0.95f - FVector(0, 0, 0.0f))));
	}
	// the bell's plunger goes down and up when rung
	if (BellPlunger && BellT >= 0.f)
	{
		BellT += DeltaTime;
		const float Down = BellT < 0.08f ? BellT / 0.08f : FMath::Max(0.f, 1.f - (BellT - 0.08f) / 0.12f);
		BellPlunger->SetActorLocation(ToWorld(BellAt) - FVector(0, 0, 0.5f * Down));
		if (BellT > 0.3f)
		{
			BellT = -1.f;
		}
	}

	// the cook: the town life's on shift, else the stand-in while open
	const int32 Person = StaffCook;
	if ((Person < 0 && !bOpen && !bCooking && !bCookAtDoor) || !bNear)
	{
		// nobody at the range, or nobody near to see them
		ReleaseCook();
		return;
	}
	const FString Mesh = StaffCookMesh.IsEmpty() ? CookStandinMesh : StaffCookMesh;
	if (Person != CookPerson || (CookBody && CookMesh != Mesh))
	{
		ReleaseCook();
		CookPerson = Person;
		if (Person >= 0)
		{
			LyudRestaurantStaff::SetPresentedExternally(this, Person, true);
		}
	}
	if (!CookBody)
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CookBody = World->SpawnActor<ANpcBody>(ToWorld(CookAt), YawToWorld(CookYaw), SP);
		if (!CookBody || !CookBody->Setup(Mesh, FString()))
		{
			if (CookBody)
			{
				CookBody->Destroy();
			}
			CookBody = nullptr;
			return;
		}
		CookBody->Tags.Add(TEXT("Restaurant"));
		CookBody->Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		CookMesh = Mesh;
		UE_LOG(LogTemp, Display, TEXT("RESTTEST cook on: person %d, model %s"), Person, *Mesh);
	}
	CookT += DeltaTime;
	if (bCookAtDoor)
	{
		// at the open back door, facing out to the hero with his fish (the fishing's kitchen point; it buys them on Y)
		const FVector Out = (ToWorld(BackDoorOut) - ToWorld(BackDoorIn)).GetSafeNormal2D();
		CookBody->Drive(ToWorld(BackDoorIn), Out.Rotation().Yaw, EPersonPose::Talk, 0, 0.f, 0.f, DeltaTime, false);
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(CookBody->Mesh->GetAnimInstance()))
		{
			Anim->SetBikeRider(FBikeRiderPose());
		}
		return;
	}
	CookBody->Drive(ToWorld(CookAt), YawToWorld(CookYaw).Yaw, bCooking ? EPersonPose::Work : EPersonPose::Stand, 0, 0.f, 0.f, DeltaTime, false);

	// the pan: on the range, shaken (a toss now and then) while anything cooks; the right hand on its handle
	if (Pan)
	{
		const float CA = FMath::DegreesToRadians(CookYaw);
		const FVector F(FMath::Cos(CA), FMath::Sin(CA), 0.f), R(FMath::Sin(CA), -FMath::Cos(CA), 0.f);
		const FVector Rest = CookAt + F * 0.62f + FVector(0, 0, 0.92f) + R * 0.08f;
		const float Shake = bCooking ? FMath::Sin(CookT * 7.f) * 0.025f * (0.5f + 0.5f * FMath::Sin(CookT * 0.9f)) : 0.f;
		const float Lift = bCooking ? FMath::Max(0.f, FMath::Sin(CookT * 0.9f)) * 0.035f : 0.f;
		const FVector P = Rest + F * Shake + FVector(0, 0, Lift);
		Pan->SetActorLocation(ToWorld(P));
		if (PanFood)
		{
			PanFood->SetActorLocation(ToWorld(P + FVector(0, 0, 0.005f + Lift * 0.4f)));
			PanFood->SetActorHiddenInGame(!bCooking);
		}
		if (ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(CookBody->Mesh->GetAnimInstance()))
		{
			// the handle runs from the pan toward the cook (the pan's +x, turned to him)
			const FTransform CS = CookBody->Mesh->GetComponentTransform();
			const FVector HandleW = ToWorld(P - F * 0.24f + FVector(0, 0, 0.03f));
			const FVector AxisW = (ToWorld(P) - HandleW).GetSafeNormal();
			FBikeRiderPose Pose;
			Pose.Weight = 1.f;
			FBikeRiderPose::FHand& H = Pose.Hands[1];
			H.Weight = 1.f;
			H.Grip = CS.InverseTransformPosition(HandleW);
			H.Axis = CS.InverseTransformVectorNoScale(AxisW);
			H.Radius = 1.2f;
			H.PalmNormal = CS.InverseTransformVectorNoScale(FVector(0, 0, -1.f));
			H.FingerDir = CS.InverseTransformVectorNoScale(FVector::CrossProduct(AxisW, FVector::UpVector).GetSafeNormal());
			H.Close = 1.f;
			Anim->SetBikeRider(Pose);
		}
	}
}
