// The townspeople eating at «Янтарь» (docs/restaurant.md "Townspeople"): whoever of the people's life sits at one of the seats that are
// not the hero's while eating out (the town life's eat_out at Content/Restaurant/seats.json) opens a menu card in both hands, orders after a
// while (a main, sometimes a drink), is served by the same waiter off the same pass, and eats it with a fork from the plate to the mouth,
// bite by bite. When they get up (the life moves them on) they pay (coins on the saucer) and the table is cleared. With nobody serving
// (closed) a partly eaten dish just stands before them.

#include "Restaurant/LyudRestaurant.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"

namespace LyudRestGuests
{
	constexpr float ReadSeconds = 14.f;      // with the menu card before ordering
	constexpr float BitePeriod = 7.f;        // one bite every so often
	constexpr float DrawCm = 2500.f;         // the hands are only posed near the camera

	AStaticMeshActor* SpawnStatic(UWorld* World, UStaticMesh* M, const FTransform& At)
	{
		if (!M)
		{
			return nullptr;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), At, SP);
		A->SetMobility(EComponentMobility::Movable);
		A->Tags.Add(TEXT("Restaurant"));
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetStaticMesh(M);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		return A;
	}
}

void ALyudRestaurant::GuestLeft(FProp& P)
{
	if (ALyudRestDish* D = P.Dish.Get())
	{
		Effect(TEXT("rest_coins"), D->GetActorLocation(), 0.4f);
		D->Destroy();
	}
	if (AStaticMeshActor* C = P.Card.Get())
	{
		C->Destroy();
	}
	if (AStaticMeshActor* F = P.Fork.Get())
	{
		F->Destroy();
	}
	// what they ordered and hasn't been carried yet is not needed any more
	for (int32 I = Orders.Num() - 1; I >= 0; --I)
	{
		FOrder& O = Orders[I];
		if (O.bGuest && O.Seat == P.Seat && O.State != FOrder::EState::Carried)
		{
			if (ALyudRestDish* D = O.Dish.Get())
			{
				D->Destroy();
			}
			Orders.RemoveAt(I);
		}
	}
}

void ALyudRestaurant::TickProps()
{
	// who of the life is eating here now, at which seat (not the hero's)
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	TSet<int32> Seen;
	const float Now = GetWorld()->GetTimeSeconds();
	if (People && People->IsRunning())
	{
		const int32 N = People->NumPeople();
		for (int32 Id = 0; Id < N; ++Id)
		{
			FPersonInfo P;
			// eating out (the town life's eat_out, the old eat_restaurant) and in the hall, seated by now
			if (!People->GetPerson(Id, P) || !P.Activity.StartsWith(TEXT("eat")) || (P.State != TEXT("doing") && P.State != TEXT("settling")))
			{
				continue;
			}
			const FVector LP = ToLocal(P.Location);
			if (FVector2D(LP.X, LP.Y).Size() > HallR || FMath::Abs(LP.Z) > 1.5f)
			{
				continue;
			}
			int32 Best = -1;
			float BestD = 90.f;
			for (int32 S = 0; S < Seats.Num(); ++S)
			{
				const float D = (float)FVector::Dist2D(ToWorld(Seats[S].At), P.Location);
				if (!Tables[Seats[S].Table].bHero && D < BestD)
				{
					BestD = D;
					Best = S;
				}
			}
			if (Best < 0)
			{
				continue;
			}
			Seen.Add(Best);
			if (Props.ContainsByPredicate([Best](const FProp& Pr) { return Pr.Seat == Best; }))
			{
				continue;
			}
			FProp Pr;
			Pr.Seat = Best;
			Pr.Person = Id;
			Pr.SatAt = Now;
			Pr.BiteT = (Id % 7) * 0.9f;
			if (!bOpen || Waiter == EWaiter::Off)
			{
				// nobody to serve: a dish of theirs is just there, partly eaten
				TArray<const FLyudRestDishDef*> Mains;
				for (const FLyudRestDishDef& D : Menu())
				{
					if (!D.bFree && !D.IsCup() && D.Stages > 0 && (D.Section == TEXT("mains") || D.Section == TEXT("soups")))
					{
						Mains.Add(&D);
					}
				}
				if (Mains.Num() > 0)
				{
					const FLyudRestDishDef* Pick = Mains[(Id * 7 + Best) % Mains.Num()];
					Pr.Dish = SpawnDish(Pick->Id, PlaceTransform(Best, 0, *Pick), 0.35f + 0.1f * (Id % 6));
				}
				Pr.bOrdered = true;
			}
			else
			{
				// the menu card in their hands first
				Pr.Card = LyudRestGuests::SpawnStatic(GetWorld(), RestMesh(TEXT("SM_RT_MenuCard")), FTransform(ToWorld(Seats[Best].Place)));
			}
			UE_LOG(LogTemp, Display, TEXT("RESTTEST guest %d sits at seat %d (table %s)"), Id, Best, *Tables[Seats[Best].Table].Id.ToString());
			Props.Add(Pr);
		}
	}
	for (int32 I = Props.Num() - 1; I >= 0; --I)
	{
		FProp& Pr = Props[I];
		if (!Seen.Contains(Pr.Seat))
		{
			UE_LOG(LogTemp, Display, TEXT("RESTTEST guest %d leaves seat %d"), Pr.Person, Pr.Seat);
			GuestLeft(Pr);
			Props.RemoveAt(I);
			continue;
		}
		// done reading: the order (a main by their id, every other one a drink too), cooked like the hero's
		if (!Pr.bOrdered && Now - Pr.SatAt > LyudRestGuests::ReadSeconds * FMath::Max(WaitScale, 0.2f))
		{
			Pr.bOrdered = true;
			TArray<const FLyudRestDishDef*> Mains, Drinks;
			for (const FLyudRestDishDef& D : Menu())
			{
				if (!IsOnMenuToday(this, D) || D.bFree)
				{
					continue;
				}
				(D.IsCup() ? Drinks : Mains).Add(&D);
			}
			auto Add = [&](const FLyudRestDishDef* D) { AddOrder(D->Id, Pr.Seat, Now + D->Cook * WaitScale, true); };
			if (Mains.Num() > 0)
			{
				Add(Mains[(Pr.Person * 5 + Pr.Seat) % Mains.Num()]);
			}
			if (Drinks.Num() > 0 && (Pr.Person % 2) == 0)
			{
				Add(Drinks[Pr.Person % Drinks.Num()]);
			}
			if (AStaticMeshActor* C = Pr.Card.Get())
			{
				C->Destroy();
			}
			UE_LOG(LogTemp, Display, TEXT("RESTTEST guest %d orders"), Pr.Person);
		}
	}
}

void ALyudRestaurant::TickGuests(float DeltaTime)
{
	// the hands: the card held up to read, then the fork between the plate and the mouth (the bike rider's arm IK on their body)
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	FVector Cam = FVector::ZeroVector;
	FRotator CamRot;
	if (PC)
	{
		PC->GetPlayerViewPoint(Cam, CamRot);
	}
	if (!People)
	{
		return;
	}
	for (FProp& Pr : Props)
	{
		FPersonInfo P;
		if (!People->GetPerson(Pr.Person, P))
		{
			continue;
		}
		ANpcBody* Body = Cast<ANpcBody>(P.Actor);
		ULyudBodyAnimInstance* Anim = Body ? Cast<ULyudBodyAnimInstance>(Body->Mesh->GetAnimInstance()) : nullptr;
		if (!Anim || FVector::Dist(Body->GetActorLocation(), Cam) > LyudRestGuests::DrawCm)
		{
			continue;
		}
		const FSeat& S = Seats[Pr.Seat];
		const float A = FMath::DegreesToRadians(S.Yaw);
		// the building's frame -> the world: forward to the table, the sitter's right
		const FVector Fwd = (ToWorld(S.At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f)) - ToWorld(S.At)).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
		const FVector Head = Body->Mesh->GetBoneLocation(TEXT("head"));
		const FTransform CS = Body->Mesh->GetComponentTransform();
		FBikeRiderPose Pose;
		Pose.Weight = 1.f;
		auto Hand = [&](int32 Side, const FVector& Grip, const FVector& Axis, const FVector& Palm, const FVector& Finger, float Radius, float W)
		{
			FBikeRiderPose::FHand& H = Pose.Hands[Side];
			H.Weight = W;
			H.Grip = CS.InverseTransformPosition(Grip);
			H.Axis = CS.InverseTransformVectorNoScale(Axis);
			H.PalmNormal = CS.InverseTransformVectorNoScale(Palm);
			H.FingerDir = CS.InverseTransformVectorNoScale(Finger);
			H.Radius = Radius;
			H.Close = 0.9f;
		};
		if (AStaticMeshActor* Card = Pr.Card.Get())
		{
			// open in front of the chest, tilted toward the face, a hand at each side
			const FVector C = Head + Fwd * 30.f - FVector(0, 0, 34.f);
			const FVector Up = (FVector::UpVector * 0.7f - Fwd * 0.7f).GetSafeNormal();
			Card->SetActorLocationAndRotation(C - Up * 16.f, FRotationMatrix::MakeFromZX(Fwd * -0.7f + FVector::UpVector * 0.7f, Right).ToQuat());
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float Sg = Side == 0 ? -1.f : 1.f;
				Hand(Side, C + Right * Sg * 12.f, Up, -Right * Sg, Up, 0.6f, 1.f);
			}
			Anim->SetBikeRider(Pose);
			continue;
		}
		ALyudRestDish* Dish = Pr.Dish.Get();
		if (!Dish || Dish->IsEmpty())
		{
			if (AStaticMeshActor* F = Pr.Fork.Get())
			{
				F->Destroy();
			}
			Anim->SetBikeRider(FBikeRiderPose());
			continue;
		}
		AStaticMeshActor* Fork = Pr.Fork.Get();
		if (!Fork)
		{
			const FName U = Dish->GetDef().Utensil;
			const FName M = Dish->GetDef().IsCup() ? NAME_None : U == TEXT("spoon") || U == TEXT("teaspoon") ? FName(TEXT("SM_RT_Spoon"))
				: U == TEXT("chopsticks") ? FName(TEXT("SM_RT_Chopsticks")) : FName(TEXT("SM_RT_Fork"));
			Fork = M.IsNone() ? nullptr : LyudRestGuests::SpawnStatic(GetWorld(), RestMesh(M), Dish->GetActorTransform());
			Pr.Fork = Fork;
			if (!Fork)
			{
				continue;
			}
		}
		// a bite: down to the plate (0..0.25), scoop (..0.35), up to the mouth (..0.55), there (..0.62), back down to rest (..0.8)
		Pr.BiteT += DeltaTime;
		const float T = FMath::Fmod(Pr.BiteT, LyudRestGuests::BitePeriod) / LyudRestGuests::BitePeriod;
		const FVector Plate = Dish->GetActorLocation() + FVector(0, 0, 5.f);
		const FVector Mouth = Head + Fwd * 9.f - FVector(0, 0, 6.f);
		const FVector Rest = Plate + FVector(0, 0, 8.f) + Right * 10.f - Fwd * 8.f;
		auto Smooth = [](float X, float A0, float A1) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp((X - A0) / (A1 - A0), 0.f, 1.f)); };
		FVector Tip;
		if (T < 0.25f) { Tip = FMath::Lerp(Rest, Plate, Smooth(T, 0.f, 0.25f)); }
		else if (T < 0.35f) { Tip = Plate + Fwd * 2.f * Smooth(T, 0.25f, 0.35f); }
		else if (T < 0.55f) { Tip = FMath::Lerp(Plate, Mouth, Smooth(T, 0.35f, 0.55f)); }
		else if (T < 0.62f) { Tip = Mouth; }
		else { Tip = FMath::Lerp(Mouth, Rest, Smooth(T, 0.62f, 0.8f)); }
		if (T >= 0.55f && T - DeltaTime / LyudRestGuests::BitePeriod < 0.55f)
		{
			Dish->SetLeft(Dish->GetLeft() - Dish->GetDef().SmallBite);
		}
		// the fork points from the hand at the tip: down onto the plate, back at the mouth near it
		const float AtMouth = Smooth(T, 0.4f, 0.55f) * (1.f - Smooth(T, 0.62f, 0.75f));
		const FVector Dir = FMath::Lerp((Fwd * 0.6f - FVector(0, 0, 0.8f) - Right * 0.2f).GetSafeNormal(), (-Fwd * 0.7f + FVector(0, 0, 0.3f) - Right * 0.5f).GetSafeNormal(), AtMouth).GetSafeNormal();
		const FVector Grip = Tip - Dir * 13.f;
		Fork->SetActorLocationAndRotation(Grip - Dir * 2.f, FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat());
		Hand(1, Grip - Dir * 3.f, Dir, -FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal(), FVector::UpVector, 0.7f, 1.f);
		Anim->SetBikeRider(Pose);
	}
}
