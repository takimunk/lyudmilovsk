// The restaurant's waiter (docs/restaurant.md "The waiter"): a resident on their shift (UPeopleLifeSubsystem::StaffOnShift) or, before
// the town life has the restaurant's staff, the stand-in of restaurant.json. While on shift the restaurant draws them (the life hides
// its own body: SetPresentedExternally): standing by the bar, walking the hall's aisle graph (layout.json walk) to the kitchen pass, a
// round tray on both hands with the dishes on it (the arms' IK of the bike rider: LyudBikeRider::ApplyArms), to the table, setting each
// dish down at its place with the right hand while the left keeps the tray; later clearing the empty plates and bringing the bill.

#include "Restaurant/LyudRestaurant.h"

#include "LyudRestaurantStaff.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "People/LyudBodyAnim.h"
#include "People/PeopleLife.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"

namespace LyudRestWaiter
{
	constexpr float TrayUp = 104.f;        // the tray's bottom over the floor (a body of the clips' size)
	constexpr float TrayFwd = 34.f;        // out in front of the body
	constexpr float PickSeconds = 1.4f;
	constexpr float SetSeconds = 0.9f;
	constexpr float DropSeconds = 1.2f;


	/** lyud.Restaurant 0: everything of the restaurant hidden (the benchmark's A/B). */
	bool Hidden()
	{
		static IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Restaurant"));
		return Var && Var->GetInt() == 0;
	}

}

FName ALyudRestaurant::NearestNode(const FVector2D& P) const
{
	FName Best;
	double BestD = TNumericLimits<double>::Max();
	for (const TPair<FName, FVector2D>& N : Nodes)
	{
		const double D = FVector2D::DistSquared(N.Value, P);
		if (D < BestD)
		{
			BestD = D;
			Best = N.Key;
		}
	}
	return Best;
}

void ALyudRestaurant::PlanPath(const FVector& ToLocalM)
{
	// Dijkstra over the hall's aisle graph (a few dozen nodes) from the node nearest the waiter to the one nearest the goal
	Path.Reset();
	const FVector From = ToLocal(WaiterPos);
	const FName A = NearestNode(FVector2D(From.X, From.Y));
	const FName B = NearestNode(FVector2D(ToLocalM.X, ToLocalM.Y));
	TArray<FName> Route;
	if (!A.IsNone() && !B.IsNone())
	{
		TMap<FName, double> Dist;
		TMap<FName, FName> Prev;
		TSet<FName> Done;
		Dist.Add(A, 0.0);
		while (true)
		{
			FName U;
			double Du = TNumericLimits<double>::Max();
			for (const TPair<FName, double>& D : Dist)
			{
				if (!Done.Contains(D.Key) && D.Value < Du)
				{
					Du = D.Value;
					U = D.Key;
				}
			}
			if (U.IsNone() || U == B)
			{
				break;
			}
			Done.Add(U);
			if (const TArray<FName>* Next = Edges.Find(U))
			{
				for (const FName& V : *Next)
				{
					const double W = Du + FVector2D::Distance(Nodes.FindRef(U), Nodes.FindRef(V));
					if (!Dist.Contains(V) || W < Dist[V])
					{
						Dist.Add(V, W);
						Prev.Add(V, U);
					}
				}
			}
		}
		if (Dist.Contains(B))
		{
			for (FName N = B; !N.IsNone(); N = Prev.Contains(N) ? Prev[N] : FName())
			{
				Route.Insert(N, 0);
				if (N == A)
				{
					break;
				}
			}
		}
	}
	for (const FName& N : Route)
	{
		const FVector2D P = Nodes.FindRef(N);
		Path.Add(ToWorld(FVector(P.X, P.Y, 0.0)));
	}
	Path.Add(ToWorld(FVector(ToLocalM.X, ToLocalM.Y, 0.0)));
}

bool ALyudRestaurant::WalkPath(float DeltaTime)
{
	// along the path at a waiter's pace, turning toward the next point; true once there
	float Step = WaiterSpeed * DeltaTime;
	while (Path.Num() > 0 && Step > 0.f)
	{
		const FVector To = Path[0];
		const FVector D = To - WaiterPos;
		const float L = (float)D.Size2D();
		if (L > 1.f)
		{
			const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			WaiterYaw = FMath::FixedTurn(WaiterYaw, WantYaw, 400.f * DeltaTime);
		}
		if (L <= Step)
		{
			WaiterPos = FVector(To.X, To.Y, WaiterPos.Z);
			Step -= L;
			Path.RemoveAt(0);
		}
		else
		{
			WaiterPos += FVector(D.X, D.Y, 0.0) / L * Step;
			Step = 0.f;
		}
	}
	return Path.Num() == 0;
}

FTransform ALyudRestaurant::TrayWorld() const
{
	const float S = WaiterBody ? WaiterBody->GetBodyScale() : 1.f;
	const FRotator Y(0.f, WaiterYaw, 0.f);
	return FTransform(Y, WaiterPos + Y.Vector() * LyudRestWaiter::TrayFwd * S + FVector(0.f, 0.f, LyudRestWaiter::TrayUp * S));
}

bool ALyudRestaurant::EnsureWaiter()
{
	// who is on shift (RefreshStaff, once a second): the town life's waiter, else the stand-in while open or finishing a job
	const bool bBusy = Job != EJob::None || (Waiter != EWaiter::Off && Waiter != EWaiter::Idle) || PendingCount(false) > 0;
	// a shift that ends with work in hand: the same person (the same body) finishes it, still drawn here, before going home
	const bool bKeep = bBusy && WaiterBody && StaffWaiter != WaiterPerson;
	const int32 Person = bKeep ? WaiterPerson : StaffWaiter;
	if (Person < 0 && !bOpen && !bBusy)
	{
		return false;
	}
	// by reference: this runs every frame while open (all three are members that outlive the call)
	const FString& Mesh = bKeep ? WaiterMesh : (StaffWaiterMesh.IsEmpty() ? WaiterStandinMesh : StaffWaiterMesh);
	if (Person != WaiterPerson)
	{
		if (WaiterPerson >= 0)
		{
			LyudRestaurantStaff::SetPresentedExternally(this, WaiterPerson, false);
		}
		WaiterPerson = Person;
		if (Person >= 0)
		{
			LyudRestaurantStaff::SetPresentedExternally(this, Person, true);
		}
	}
	if (WaiterBody && WaiterMesh != Mesh && Waiter == EWaiter::Idle)
	{
		WaiterBody->Destroy();
		WaiterBody = nullptr;
	}
	if (!WaiterBody)
	{
		if (GetWorld()->GetTimeSeconds() < WaiterRetryAt)
		{
			// the model was missing a moment ago: not a spawn and a failed load every frame
			return false;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WaiterPos = ToWorld(WaiterHome);
		WaiterYaw = YawToWorld(HomeYaw).Yaw;
		WaiterBody = GetWorld()->SpawnActor<ANpcBody>(WaiterPos, FRotator(0.f, WaiterYaw, 0.f), SP);
		if (!WaiterBody || !WaiterBody->Setup(Mesh, FString()))
		{
			UE_LOG(LogTemp, Warning, TEXT("RESTTEST waiter: no model %s"), *Mesh);
			if (WaiterBody)
			{
				WaiterBody->Destroy();
			}
			WaiterBody = nullptr;
			WaiterRetryAt = GetWorld()->GetTimeSeconds() + 10.f;
			return false;
		}
		WaiterBody->Tags.Add(TEXT("Restaurant"));
		// posed only when seen (bNear also skips the hands' IK)
		WaiterBody->Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		WaiterMesh = Mesh;
		UE_LOG(LogTemp, Display, TEXT("RESTTEST waiter on: person %d, model %s"), Person, *Mesh);
	}
	if (!Tray)
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Tray = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), TrayWorld(), SP);
		Tray->SetMobility(EComponentMobility::Movable);
		Tray->Tags.Add(TEXT("Restaurant"));
		UStaticMeshComponent* C = Tray->GetStaticMeshComponent();
		C->SetStaticMesh(RestMesh(TEXT("SM_RT_Tray")));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		Tray->SetActorHiddenInGame(true);
	}
	if (Waiter == EWaiter::Off)
	{
		Waiter = EWaiter::Idle;
	}
	return true;
}

void ALyudRestaurant::ReleaseWaiter()
{
	if (WaiterPerson >= 0)
	{
		if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
		{
			LyudRestaurantStaff::SetPresentedExternally(this, WaiterPerson, false);
		}
	}
	WaiterPerson = -1;
	if (WaiterBody)
	{
		WaiterBody->Destroy();
		WaiterBody = nullptr;
	}
	if (Tray)
	{
		Tray->Destroy();
		Tray = nullptr;
	}
	for (ALyudRestDish* D : OnTray)
	{
		if (D)
		{
			D->Destroy();
		}
	}
	OnTray.Reset();
	OnTrayOrders.Reset();
	Waiter = EWaiter::Off;
	Job = EJob::None;
}

void ALyudRestaurant::StartJob()
{
	// the bill once everything is on the table, then what is ready at the pass, then clearing the empty plates
	const bool bAllServed = !Orders.ContainsByPredicate([](const FOrder& O) { return !O.bGuest && O.State != FOrder::EState::Served; });
	const bool bReady = Orders.ContainsByPredicate([](const FOrder& O) { return O.State == FOrder::EState::Ready; });
	const float ClearAfter = FMath::Max(3.f, ClearAfterS * WaitScale);
	const bool bEmpty = HeroDishes.ContainsByPredicate([ClearAfter](const ALyudRestDish* D)
	{
		return D && D->IsEmpty() && !D->IsEating() && D->SecondsEmpty() > ClearAfter;
	});
	OnTrayOrders.Reset();
	if (bBillAsked && !bBillServed && bAllServed && HeroTableIx >= 0)
	{
		Job = EJob::Bill;
		JobTable = HeroTableIx;
		PlanPath(PassStand);
		Waiter = EWaiter::ToPass;
	}
	else if (bReady)
	{
		Job = EJob::Serve;
		// up to four dishes of one table at a time (the first ready one's)
		JobTable = -1;
		for (int32 I = 0; I < Orders.Num() && OnTrayOrders.Num() < 4; ++I)
		{
			const FOrder& O = Orders[I];
			const int32 T = Seats.IsValidIndex(O.Seat) ? Seats[O.Seat].Table : -1;
			if (O.State == FOrder::EState::Ready && T >= 0 && (JobTable < 0 || T == JobTable))
			{
				JobTable = T;
				OnTrayOrders.Add(O.Id);
			}
		}
		PlanPath(PassStand);
		Waiter = EWaiter::ToPass;
	}
	else if (bEmpty && HeroTableIx >= 0)
	{
		Job = EJob::Clear;
		JobTable = HeroTableIx;
		PlanPath(Tables[HeroTableIx].ServeAt);
		Waiter = EWaiter::ToTable;
	}
	else
	{
		Job = EJob::None;
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("RESTTEST waiter job %s (%d on the tray)"), Job == EJob::Bill ? TEXT("bill") : Job == EJob::Serve ? TEXT("serve") : TEXT("clear"),
		OnTrayOrders.Num());
}

void ALyudRestaurant::PoseWaiter(float DeltaTime, bool bWalking)
{
	if (!WaiterBody)
	{
		return;
	}
	WaiterBody->SetActorHiddenInGame(LyudRestWaiter::Hidden() || !bNear);
	if (!bNear)
	{
		// far: the body only keeps its place (no clip blending, no hands)
		WaiterBody->SetActorLocation(WaiterPos);
		if (Tray)
		{
			Tray->SetActorHiddenInGame(true);
		}
		return;
	}
	WaiterBody->Drive(WaiterPos, WaiterYaw, bWalking ? EPersonPose::Walk : EPersonPose::Stand, 0, bWalking ? WaiterSpeed : 0.f, 0.f, DeltaTime, false);
	const bool bTray = OnTray.Num() > 0 || Waiter == EWaiter::Pick || Waiter == EWaiter::SetDown || Waiter == EWaiter::Collect
		|| (Job != EJob::None && Waiter != EWaiter::Idle && Waiter != EWaiter::ToHome) || BillFolder;
	const bool bCarry = bTray && Job != EJob::None && Waiter != EWaiter::Idle;
	ULyudBodyAnimInstance* Anim = Cast<ULyudBodyAnimInstance>(WaiterBody->Mesh->GetAnimInstance());
	if (Tray)
	{
		Tray->SetActorHiddenInGame(!bCarry);
		Tray->SetActorTransform(TrayWorld());
	}
	// the dishes ride on the tray, in a ring
	const FTransform T = TrayWorld();
	for (int32 I = 0; I < OnTray.Num(); ++I)
	{
		if (ALyudRestDish* D = OnTray[I])
		{
			const float A = I * 2.f * PI / FMath::Max(OnTray.Num(), 1) + 0.6f;
			const float R = OnTray.Num() > 1 ? 8.5f : 0.f;
			const FVector P = T.TransformPosition(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 1.2f - D->BottomZ()));
			if (!(Waiter == EWaiter::SetDown && I == 0))
			{
				D->SetActorLocationAndRotation(P, T.GetRotation());
			}
		}
	}
	if (!Anim)
	{
		return;
	}
	if (!bCarry)
	{
		Anim->SetBikeRider(FBikeRiderPose());
		return;
	}
	// both hands under the tray's rim at its sides, palms up, the fingers curling round the rim; while setting a dish down the right
	// hand holds that dish's rim instead
	const FTransform CS = WaiterBody->Mesh->GetComponentTransform();
	const FVector Fwd = T.GetRotation().GetForwardVector(), Right = T.GetRotation().GetRightVector();
	const float TrayR = 19.f;
	FBikeRiderPose Pose;
	Pose.Weight = 1.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float S = Side == 0 ? -1.f : 1.f;     // left, right
		FBikeRiderPose::FHand& H = Pose.Hands[Side];
		FVector Grip = T.GetLocation() + Right * S * TrayR - Fwd * 3.f;
		FVector Axis = Fwd;
		FVector Palm = FVector::UpVector;
		FVector Finger = (Right * -S * 0.6f + Fwd * 0.5f).GetSafeNormal();
		float Radius = 0.9f;
		if (Side == 1 && Waiter == EWaiter::SetDown && OnTray.Num() > 0 && OnTray[0])
		{
			const ALyudRestDish* D = OnTray[0];
			const FVector C = D->GetActorLocation() + FVector(0.f, 0.f, D->BottomZ());
			const FVector Toward = (WaiterPos - C).GetSafeNormal2D();
			Grip = C + Toward * D->Radius() + FVector(0.f, 0.f, 1.f);
			Axis = FVector::CrossProduct(Toward, FVector::UpVector).GetSafeNormal();
			Palm = -Toward;
			Finger = (-Toward * 0.4f + FVector(0, 0, -0.9f)).GetSafeNormal();
			Radius = 0.7f;
		}
		H.Weight = 1.f;
		H.Grip = CS.InverseTransformPosition(Grip);
		H.Axis = CS.InverseTransformVectorNoScale(Axis);
		H.PalmNormal = CS.InverseTransformVectorNoScale(Palm);
		H.FingerDir = CS.InverseTransformVectorNoScale(Finger);
		H.Radius = Radius;
		H.Close = 0.9f;
	}
	Anim->SetBikeRider(Pose);
}

void ALyudRestaurant::TickWaiter(float DeltaTime)
{
	// closed: the job in hand and what is paid for still get finished, then the waiter goes off
	const bool bNeeded = bOpen || Job != EJob::None || PendingCount(false) > 0 || (Waiter != EWaiter::Off && Waiter != EWaiter::Idle);
	if (!bNeeded)
	{
		if (Waiter != EWaiter::Off)
		{
			UE_LOG(LogTemp, Display, TEXT("RESTTEST waiter off shift"));
			ReleaseWaiter();
		}
		return;
	}
	if (!EnsureWaiter())
	{
		return;
	}
	WaiterT += DeltaTime;
	// no state lasts for ever: past its limit the walk ends where it was going and each step finishes at once
	if (Waiter != LastWaiter)
	{
		LastWaiter = Waiter;
		StateAge = 0.f;
	}
	StateAge += DeltaTime;
	const bool bWalk = Waiter == EWaiter::ToPass || Waiter == EWaiter::ToTable || Waiter == EWaiter::ToPassBack || Waiter == EWaiter::ToHome;
	const float Limit = bWalk ? 40.f : Waiter == EWaiter::Idle ? 1e9f : 15.f;
	if (StateAge > Limit)
	{
		if (Path.Num() > 0)
		{
			WaiterPos = FVector(Path.Last().X, Path.Last().Y, WaiterPos.Z);
			Path.Reset();
		}
		WaiterT = FMath::Max(WaiterT, 100.f);
		if (StateAge - DeltaTime <= Limit)
		{
			UE_LOG(LogTemp, Warning, TEXT("RESTTEST waiter timeout in state %d, hurried on"), (int32)Waiter);
		}
	}
	bool bWalking = false;
	switch (Waiter)
	{
	case EWaiter::Idle:
	{
		// home by the bar, turned to the hall; a job when there is one
		const FVector Home = ToWorld(WaiterHome);
		if (FVector::Dist2D(Home, WaiterPos) > 5.f)
		{
			if (Path.Num() == 0)
			{
				PlanPath(WaiterHome);
			}
			bWalking = !WalkPath(DeltaTime);
		}
		else
		{
			WaiterYaw = FMath::FixedTurn(WaiterYaw, YawToWorld(HomeYaw).Yaw, 200.f * DeltaTime);
		}
		if (WaiterT > 0.5f)
		{
			WaiterT = 0.f;
			StartJob();
		}
		break;
	}
	case EWaiter::ToPass:
		bWalking = !WalkPath(DeltaTime);
		if (!bWalking)
		{
			Waiter = EWaiter::Pick;
			WaiterT = 0.f;
		}
		break;
	case EWaiter::Pick:
		WaiterYaw = FMath::FixedTurn(WaiterYaw, YawToWorld(PassYaw).Yaw, 300.f * DeltaTime);
		if (WaiterT >= LyudRestWaiter::PickSeconds)
		{
			if (Job == EJob::Bill && (!bBillAsked || HeroTableIx < 0))
			{
				// the bill was withdrawn on the way (a new order) or the table was laid again: nothing to bring
				Job = EJob::None;
				Waiter = EWaiter::ToHome;
				PlanPath(WaiterHome);
				break;
			}
			if (Job == EJob::Bill)
			{
				// the bill in its folder on the tray
				FActorSpawnParameters SP;
				SP.ObjectFlags |= RF_Transient;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				BillFolder = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), TrayWorld(), SP);
				BillFolder->SetMobility(EComponentMobility::Movable);
				BillFolder->Tags.Add(TEXT("Restaurant"));
				UStaticMeshComponent* C = BillFolder->GetStaticMeshComponent();
				C->SetStaticMesh(RestMesh(TEXT("SM_RT_MenuCard")));
				C->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.f));
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->bAffectDynamicIndirectLighting = false;
				C->bAffectDistanceFieldLighting = false;
			}
			else
			{
				// off the pass's shelf onto the tray (made there now if it never stood on the shelf)
				for (int32 OrderId : OnTrayOrders)
				{
					FOrder* Found = FindOrder(OrderId);
					if (!Found)
					{
						continue;
					}
					FOrder& O = *Found;
					O.State = FOrder::EState::Carried;
					ALyudRestDish* D = O.Dish.Get();
					O.Dish = nullptr;
					if (!D)
					{
						D = SpawnDish(O.Item, TrayWorld(), 1.f);
					}
					if (D)
					{
						D->OrderId = O.Id;
						OnTray.Add(D);
					}
				}
				Effect(TEXT("rest_tray"), TrayWorld().GetLocation(), 0.6f);
				UE_LOG(LogTemp, Display, TEXT("RESTTEST waiter picked %d dishes at the pass"), OnTray.Num());
			}
			if (Tables.IsValidIndex(JobTable))
			{
				PlanPath(Tables[JobTable].ServeAt);
				Waiter = EWaiter::ToTable;
			}
			else
			{
				Waiter = EWaiter::ToPassBack;
				PlanPath(PassStand);
			}
		}
		break;
	case EWaiter::ToTable:
		bWalking = !WalkPath(DeltaTime);
		if (BillFolder)
		{
			BillFolder->SetActorTransform(FTransform(TrayWorld().GetRotation(), TrayWorld().GetLocation() + FVector(0, 0, 1.5f)));
		}
		if (!bWalking)
		{
			WaiterT = 0.f;
			SetIndex = 0;
			Waiter = Job == EJob::Clear ? EWaiter::Collect : EWaiter::SetDown;
			if (OnTray.Num() > 0 && OnTray[0])
			{
				SetFrom = OnTray[0]->GetActorTransform();
			}
		}
		break;
	case EWaiter::SetDown:
	{
		// facing the table, each dish from the tray to its place in an arc
		if (Tables.IsValidIndex(JobTable))
		{
			const FVector To = ToWorld(Tables[JobTable].At) - WaiterPos;
			WaiterYaw = FMath::FixedTurn(WaiterYaw, FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)), 300.f * DeltaTime);
		}
		if (Job == EJob::Bill)
		{
			if (!BillFolder || HeroTableIx < 0 || !bBillAsked)
			{
				// the folder went while it was carried (a new order withdrew the bill, or the table was laid again): no state
				// lasts for ever, the waiter goes home and takes the next job
				if (BillFolder)
				{
					BillFolder->Destroy();
					BillFolder = nullptr;
				}
				Job = EJob::None;
				Waiter = EWaiter::ToHome;
				PlanPath(WaiterHome);
				break;
			}
			if (WaiterT >= LyudRestWaiter::SetSeconds)
			{
				const FTable& Tb = Tables[HeroTableIx];
				const int32 Seat = HeroSeatIx >= 0 ? HeroSeatIx : Tb.Seats[0];
				FTransform At = PlaceTransform(Seat, 2, *FindDish(Menu()[0].Id));
				At.AddToTranslation(FVector(0, 0, 0.3f));
				BillFolder->SetActorTransform(At);
				bBillServed = true;
				Effect(TEXT("rest_coins"), At.GetLocation(), 0.6f);
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					Screen->Toast(LyudText::Get(TEXT("ui.rest.bill_toast"), { FStringFormatArg(Tab) }));
				}
				UE_LOG(LogTemp, Display, TEXT("RESTTEST bill served (tab %d)"), Tab);
				Waiter = EWaiter::ToHome;
				Job = EJob::None;
				PlanPath(WaiterHome);
			}
			break;
		}
		if (OnTray.Num() == 0)
		{
			Waiter = EWaiter::ToHome;
			Job = EJob::None;
			PlanPath(WaiterHome);
			break;
		}
		ALyudRestDish* D = OnTray[0];
		FOrder* Ord = FindOrder(D->OrderId);
		const int32 Seat = Ord && Seats.IsValidIndex(Ord->Seat) ? Ord->Seat : Tables[Tables.IsValidIndex(JobTable) ? JobTable : 0].Seats[0];
		const bool bGuest = Ord && Ord->bGuest;
		// the slot: the main plate first, a cup to the right, a second plate left, bread and water to the middle
		const FLyudRestDishDef& Def = D->GetDef();
		int32 Slot = Def.IsCup() ? 1 : 0;
		if (Def.bFree)
		{
			Slot = Def.IsCup() ? 4 : 3;
		}
		for (int32 Tries = 0; Tries < 6; ++Tries)
		{
			const FVector Want = PlaceTransform(Seat, Slot, Def).GetLocation();
			const bool bTaken = HeroDishes.ContainsByPredicate([&Want](const ALyudRestDish* O) { return O && FVector::Dist2D(O->RestTransform().GetLocation(), Want) < 9.f; })
				|| Props.ContainsByPredicate([&Want](const FProp& Pr) { return Pr.Dish.IsValid() && FVector::Dist2D(Pr.Dish->RestTransform().GetLocation(), Want) < 9.f; });
			if (!bTaken)
			{
				break;
			}
			Slot = Slot == 0 ? 2 : Slot == 1 ? 5 : Slot + 1;
		}
		FTransform Target = PlaceTransform(Seat, Slot, Def);
		Target.AddToTranslation(FVector(0.f, 0.f, -D->BottomZ() + D->SaucerZ()));
		const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(WaiterT / LyudRestWaiter::SetSeconds, 0.f, 1.f));
		FVector P = FMath::Lerp(SetFrom.GetLocation(), Target.GetLocation(), A);
		P.Z += FMath::Sin(A * PI) * 6.f;
		D->SetActorLocationAndRotation(P, FQuat::Slerp(SetFrom.GetRotation(), Target.GetRotation(), A));
		if (WaiterT >= LyudRestWaiter::SetSeconds)
		{
			D->SetRest(Target);
			OnTray.RemoveAt(0);
			if (bGuest)
			{
				// a townsperson's: theirs to eat (LyudRestaurantGuests.cpp); gone already, it goes back to the kitchen
				FProp* Pr = Props.FindByPredicate([Seat](const FProp& Pp) { return Pp.Seat == Seat; });
				TWeakObjectPtr<ALyudRestDish>* Into = Pr ? (Def.IsCup() ? &Pr->Cup : &Pr->Dish) : nullptr;
				if (Into && !Into->IsValid())
				{
					*Into = D;
				}
				else
				{
					D->Destroy();
				}
			}
			else
			{
				D->SetEdible(true);
				HeroDishes.Add(D);
			}
			if (FOrder* Done = FindOrder(D->OrderId))
			{
				Done->State = FOrder::EState::Served;
			}
			Effect(Def.IsCup() ? TEXT("rest_glass") : TEXT("rest_plate"), Target.GetLocation(), 0.7f);
			if (!Def.bFree && !bGuest)
			{
				if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
				{
					// the waiter's name when the town life knows them
					const FText Who = WaiterPerson >= 0 ? LyudRestaurantStaff::DisplayName(this, WaiterPerson) : FText::GetEmpty();
					Screen->Toast(Who.IsEmpty()
						? LyudText::Get(TEXT("ui.rest.served"), { FStringFormatArg(ULyudInventory::ItemName(Def.Id).ToString()) })
						: LyudText::Get(TEXT("ui.rest.served_by"), { FStringFormatArg(ULyudInventory::ItemName(Def.Id).ToString()), FStringFormatArg(Who.ToString()) }));
				}
			}
			UE_LOG(LogTemp, Display, TEXT("RESTTEST served %s at seat %d slot %d"), *Def.Id.ToString(), Seat, Slot);
			++SetIndex;
			WaiterT = 0.f;
			if (OnTray.Num() > 0 && OnTray[0])
			{
				SetFrom = OnTray[0]->GetActorTransform();
			}
		}
		break;
	}
	case EWaiter::Collect:
	{
		// the empty plates onto the tray, one at a time
		ALyudRestDish* Empty = nullptr;
		for (ALyudRestDish* D : HeroDishes)
		{
			if (D && D->IsEmpty() && !D->IsEating())
			{
				Empty = D;
				break;
			}
		}
		if (WaiterT >= LyudRestWaiter::SetSeconds * 0.8f)
		{
			WaiterT = 0.f;
			if (Empty)
			{
				HeroDishes.Remove(Empty);
				Empty->SetEdible(false);
				OnTray.Add(Empty);
				UE_LOG(LogTemp, Display, TEXT("RESTTEST cleared %s"), *Empty->GetDef().Id.ToString());
			}
			else
			{
				Waiter = EWaiter::ToPassBack;
				PlanPath(PassStand);
			}
		}
		break;
	}
	case EWaiter::ToPassBack:
		bWalking = !WalkPath(DeltaTime);
		if (!bWalking)
		{
			Waiter = EWaiter::Drop;
			WaiterT = 0.f;
		}
		break;
	case EWaiter::Drop:
		WaiterYaw = FMath::FixedTurn(WaiterYaw, YawToWorld(PassYaw).Yaw, 300.f * DeltaTime);
		if (WaiterT >= LyudRestWaiter::DropSeconds)
		{
			for (ALyudRestDish* D : OnTray)
			{
				if (D)
				{
					D->Destroy();
				}
			}
			OnTray.Reset();
			Job = EJob::None;
			Waiter = EWaiter::ToHome;
			PlanPath(WaiterHome);
		}
		break;
	case EWaiter::ToHome:
		bWalking = !WalkPath(DeltaTime);
		if (!bWalking)
		{
			Waiter = EWaiter::Idle;
			WaiterT = 0.f;
		}
		break;
	default:
		break;
	}
	PoseWaiter(DeltaTime, bWalking);
}
