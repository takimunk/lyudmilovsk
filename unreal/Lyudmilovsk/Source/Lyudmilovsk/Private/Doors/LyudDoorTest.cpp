// Door.SelfTest: the doors' checks in the running game, in /Game/Dev/L_DoorTest (unreal/scripts/make_door_test_level.py),
// driven by unreal/scripts/test_doors.py. Each check logs DOORTEST PASS|FAIL <what>.

#include "CharacterStandard.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Doors/LyudDoor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** A stand-in NPC: walks to Goal; with bUseApi it asks the doors on its way like a planner would, else it just walks. */
	struct FWalker
	{
		TWeakObjectPtr<ACharacter> Body;
		FVector Goal = FVector::ZeroVector;
		bool bUseApi = true;
		TWeakObjectPtr<ALyudDoorBase> Door;
	};

	struct FDoorTest
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TPair<float, TFunction<void()>>> Steps;
		int32 Next = 0;
		float Wait = 0.f;
		int32 Fails = 0;
		FVector HeroWalk = FVector::ZeroVector;
		bool bHeroWalk = false;
		bool bReleaseE = false;
		TArray<FWalker> Walkers;
		FTSTicker::FDelegateHandle Tick;
	};
	TSharedPtr<FDoorTest> GDoorTest;

	void Check(bool bOk, const FString& What)
	{
		UE_LOG(LogTemp, Warning, TEXT("DOORTEST %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What);
		if (!bOk && GDoorTest)
		{
			++GDoorTest->Fails;
		}
	}

	ALyudDoorBase* DoorTagged(UWorld* World, FName Tag)
	{
		for (TActorIterator<ALyudDoorBase> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	void PressE(UWorld* World, EInputEvent Event)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), EKeys::E, Event, FPlatformTime::Cycles64()));
		}
	}

	/** Puts the hero at a floor point looking along Yaw (a little down). */
	void PlaceHero(UWorld* World, const FVector& Floor, float Yaw)
	{
		APawn* Hero = UGameplayStatics::GetPlayerPawn(World, 0);
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Hero || !PC)
		{
			return;
		}
		const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Hero->GetRootComponent());
		const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.f;
		Hero->TeleportTo(Floor + FVector(0.f, 0.f, Half + 2.f), FRotator(0.f, Yaw, 0.f));
		PC->SetControlRotation(FRotator(-8.f, Yaw, 0.f));
	}

	/** Does a body of the character standard's size fit through the doorway along its normal? */
	bool StandardBodyFits(UWorld* World, const ALyudDoorBase* Door)
	{
		using namespace LyudmilovskCharacter;
		const float R = MinDoorClearWidth * 0.5f - 0.5f;
		const float Half = MinDoorClearHeight * 0.5f - 0.5f;
		const FVector C = Door->GetPassageCenter() + FVector(0.f, 0.f, Half + 0.5f);
		const FVector N = Door->GetPassageNormal();
		FCollisionQueryParams Params(TEXT("DoorFit"), false);
		Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(World, 0));
		FHitResult Hit;
		const bool bBlocked = World->SweepSingleByChannel(Hit, C - N * 150.f, C + N * 150.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, Half), Params);
		if (bBlocked)
		{
			UE_LOG(LogTemp, Warning, TEXT("DOORTEST   %s: the standard body hits %s at %s"), *Door->GetName(), *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToString());
		}
		return !bBlocked;
	}

	ACharacter* SpawnWalker(UWorld* World, const FVector& Floor, const FVector& Goal, bool bUseApi)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ACharacter* C = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Floor + FVector(0.f, 0.f, 98.f), (Goal - Floor).Rotation(), P);
		if (!C)
		{
			return nullptr;
		}
		C->GetCapsuleComponent()->SetCapsuleSize(LyudmilovskCharacter::PlayerCapsuleRadius, LyudmilovskCharacter::PlayerCapsuleHalfHeight);
		C->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		C->GetCharacterMovement()->MaxWalkSpeed = 150.f;
		C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		FWalker& W = GDoorTest->Walkers.AddDefaulted_GetRef();
		W.Body = C;
		W.Goal = Goal;
		W.bUseApi = bUseApi;
		return C;
	}

	void DriveWalker(UWorld* World, FWalker& W)
	{
		ACharacter* C = W.Body.Get();
		ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(World);
		if (!C || !Doors)
		{
			return;
		}
		const FVector Pos = C->GetActorLocation();
		FVector Target = W.Goal;
		if (W.bUseApi)
		{
			// what a path follower does: a door on the way -> ask for it, wait out of its sweep till it is open
			if (!W.Door.IsValid())
			{
				W.Door = Doors->FindDoorOnSegment(Pos, FVector(W.Goal.X, W.Goal.Y, Pos.Z));
			}
			if (ALyudDoorBase* D = W.Door.Get())
			{
				const bool bPassable = D->RequestPassage(C);
				float Alpha = 0.f;
				if (!D->CrossesSegment(Pos, FVector(W.Goal.X, W.Goal.Y, Pos.Z), Alpha))
				{
					D->ReleasePassage(C);
					W.Door.Reset();
				}
				else if (!bPassable)
				{
					Target = D->GetWaitPoint(Pos, C->GetCapsuleComponent()->GetScaledCapsuleRadius());
				}
			}
		}
		const FVector To = (Target - Pos) * FVector(1.f, 1.f, 0.f);
		if (To.Size() > 15.f)
		{
			C->AddMovementInput(To.GetSafeNormal(), 1.f);
		}
	}
}

static FAutoConsoleCommandWithWorld GDoorSelfTest(TEXT("Door.SelfTest"),
	TEXT("Runs the door checks in L_DoorTest (E opens and closes, the standard body fits, NPCs open doors and shut them behind, locked doors, no shutting on anyone); logs DOORTEST lines"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		using namespace LyudmilovskCharacter;
		GDoorTest = MakeShared<FDoorTest>();
		FDoorTest& T = *GDoorTest;
		T.World = World;
		auto W = [&T]() { return T.World.Get(); };
		auto Door = [W](const TCHAR* Tag) { return DoorTagged(W(), FName(Tag)); };
		auto Hero = [W]() { return UGameplayStatics::GetPlayerPawn(W(), 0); };
		auto Shot = [W](const TCHAR* Name)
		{
			// a screenshot is the viewport's command, not the engine's
			if (UGameViewportClient* V = W()->GetGameViewport())
			{
				V->ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x720 filename=%s"), Name));
			}
		};
		const float StdY = -600.f;
		const float WideY = -200.f;
		const float DoubleY = 300.f;
		const float LockedY = 800.f;

		T.Steps.Add({ 1.f, [Door]()
		{
			const TCHAR* Tags[] = { TEXT("DoorTest_Std"), TEXT("DoorTest_Wide"), TEXT("DoorTest_Double"), TEXT("DoorTest_Locked") };
			for (const TCHAR* Tag : Tags)
			{
				ALyudDoorBase* D = Door(Tag);
				Check(D && D->IsFullyClosed(), FString::Printf(TEXT("%s is there and starts closed"), Tag));
				if (D)
				{
					Check(D->GetClearWidth() >= MinDoorClearWidth && D->GetClearHeight() >= MinDoorClearHeight,
						FString::Printf(TEXT("%s clear %.0f x %.0f >= %.0f x %.0f"), Tag, D->GetClearWidth(), D->GetClearHeight(), MinDoorClearWidth, MinDoorClearHeight));
				}
			}
			const ALyudDoor* Std = Cast<ALyudDoor>(Door(TEXT("DoorTest_Std")));
			const ALyudDoor* Dbl = Cast<ALyudDoor>(Door(TEXT("DoorTest_Double")));
			Check(Std && FMath::IsNearlyEqual(Std->ActualClearWidth, StandardDoorClearWidth) && FMath::IsNearlyEqual(Std->HoleWidth, StandardDoorClearWidth + 2.f * DoorLiningThickness + DoorLeafThickness),
				TEXT("standard door: clear 110, hole = clear + linings + leaf"));
			Check(Dbl && FMath::IsNearlyEqual(Dbl->ActualClearWidth, DoubleDoorClearWidth) && FMath::IsNearlyEqual(Dbl->ActualClearHeight, DoubleDoorClearHeight), TEXT("double door: clear 150 x 230"));
			const ALyudDoor* Small = Cast<ALyudDoor>(Door(TEXT("DoorTest_SmallHole")));
			Check(Small && Small->bBelowStandard, TEXT("a door fitted to a 110 cm hole is flagged below the standard"));
		} });
		T.Steps.Add({ 0.2f, [W, Door]()
		{
			// the shut leaf is a wall for pawns
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("DoorShut"), false);
			Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(W(), 0));
			const FVector C = D->GetPassageCenter() + FVector(0.f, 0.f, 100.f);
			const bool bHit = W()->SweepSingleByChannel(Hit, C - D->GetPassageNormal() * 150.f, C + D->GetPassageNormal() * 150.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(25.f), Params);
			Check(bHit && Hit.GetActor() == D, TEXT("the shut leaf blocks a pawn"));
			PlaceHero(W(), FVector(-170.f, -600.f, 0.f), 0.f);
		} });
		T.Steps.Add({ 1.0f, [&T, W, Door, Shot]()
		{
			Check(ULyudDoorSubsystem::Get(W())->DoorInView() == Door(TEXT("DoorTest_Std")), TEXT("looking at the standard door from 1.7 m: it is the one in view"));
			Shot(TEXT("door_std_closed"));
		} });
		T.Steps.Add({ 0.5f, [&T, W]()
		{
			PressE(W(), IE_Pressed);
			T.bReleaseE = true;
		} });
		// the screenshot's long frame does not count toward the swing: give it time
		T.Steps.Add({ 2.5f, [&T, W, Door, Shot]()
		{
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			Check(D->IsPassable(), FString::Printf(TEXT("E opened it (openness %.2f)"), D->GetOpenness()));
			Check(Door(TEXT("DoorTest_Wide"))->IsFullyClosed(), TEXT("E opened only the door in view"));
			Check(StandardBodyFits(W(), D), TEXT("standard door: a 108 x 210 body fits through, leaf open"));
			Shot(TEXT("door_std_open"));
			T.HeroWalk = FVector(260.f, -600.f, 0.f);
			T.bHeroWalk = true;
		} });
		T.Steps.Add({ 4.f, [&T, W, Door, Hero]()
		{
			T.bHeroWalk = false;
			Check(Hero()->GetActorLocation().X > 200.f, FString::Printf(TEXT("the hero walked through (x %.0f)"), Hero()->GetActorLocation().X));
			// 2 m back: out of the leaf's sweep, the doorway within reach of E
			PlaceHero(W(), FVector(200.f, -600.f, 0.f), 180.f);
		} });
		T.Steps.Add({ 0.5f, [&T, W]()
		{
			PressE(W(), IE_Pressed);
			T.bReleaseE = true;
		} });
		T.Steps.Add({ 1.5f, [W, Door]()
		{
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			Check(D->IsFullyClosed(), FString::Printf(TEXT("E from the other side shut it (openness %.2f)"), D->GetOpenness()));
			// open the others for the fit checks
			Door(TEXT("DoorTest_Wide"))->Open();
			Door(TEXT("DoorTest_Double"))->Open();
		} });
		T.Steps.Add({ 1.5f, [W, Door]()
		{
			Check(StandardBodyFits(W(), Door(TEXT("DoorTest_Wide"))), TEXT("wide door: the standard body fits through"));
			Check(StandardBodyFits(W(), Door(TEXT("DoorTest_Double"))), TEXT("double door: the standard body fits through"));
			Door(TEXT("DoorTest_Wide"))->Close();
			Door(TEXT("DoorTest_Double"))->Close();
			// someone in the sweep: an open door waits before shutting
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			D->Open();
		} });
		T.Steps.Add({ 1.5f, [W, Door]()
		{
			PlaceHero(W(), FVector(60.f, -600.f, 0.f), 180.f);
			Door(TEXT("DoorTest_Std"))->Close();
		} });
		T.Steps.Add({ 1.5f, [W, Door]()
		{
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			Check(D->GetOpenness() > 0.99f, FString::Printf(TEXT("it does not shut on the hero standing in its sweep (openness %.2f)"), D->GetOpenness()));
			PlaceHero(W(), FVector(-300.f, -600.f, 0.f), 0.f);
		} });
		T.Steps.Add({ 1.5f, [Door]()
		{
			ALyudDoorBase* D = Door(TEXT("DoorTest_Std"));
			Check(D->IsFullyClosed(), FString::Printf(TEXT("then shuts once the hero stepped out (openness %.2f)"), D->GetOpenness()));
			Check(Door(TEXT("DoorTest_Wide"))->IsFullyClosed() && Door(TEXT("DoorTest_Double"))->IsFullyClosed(), TEXT("wide and double shut again"));
		} });
		// NPCs: one plans with the door API through the wide door, one just walks into the double door from the swing side
		T.Steps.Add({ 0.2f, [W]()
		{
			SpawnWalker(W(), FVector(-450.f, -200.f, 0.f), FVector(450.f, -200.f, 0.f), true);
			SpawnWalker(W(), FVector(450.f, 300.f, 0.f), FVector(-450.f, 300.f, 0.f), false);
			SpawnWalker(W(), FVector(-450.f, 800.f, 0.f), FVector(450.f, 800.f, 0.f), true);
		} });
		T.Steps.Add({ 2.f, [Door, Shot]()
		{
			Check(Door(TEXT("DoorTest_Wide"))->IsOpen(), TEXT("the wide door opens for the NPC coming to it"));
			Shot(TEXT("door_npc"));
		} });
		T.Steps.Add({ 8.f, [&T, Door]()
		{
			const ACharacter* A = T.Walkers[0].Body.Get();
			const ACharacter* B = T.Walkers[1].Body.Get();
			const ACharacter* L = T.Walkers[2].Body.Get();
			Check(A && A->GetActorLocation().X > 300.f, FString::Printf(TEXT("the NPC using the door API went through the wide door (x %.0f)"), A ? A->GetActorLocation().X : 0.f));
			Check(B && B->GetActorLocation().X < -300.f, FString::Printf(TEXT("the NPC that just walks went through the double door from the swing side (x %.0f)"), B ? B->GetActorLocation().X : 0.f));
			Check(L && L->GetActorLocation().X < 0.f, FString::Printf(TEXT("nobody gets through the locked door (x %.0f)"), L ? L->GetActorLocation().X : 0.f));
			Check(Door(TEXT("DoorTest_Locked"))->IsFullyClosed(), TEXT("the locked door stayed shut"));
		} });
		T.Steps.Add({ 4.f, [Door]()
		{
			Check(Door(TEXT("DoorTest_Wide"))->IsFullyClosed(), FString::Printf(TEXT("the wide door shut behind the NPC (openness %.2f)"), Door(TEXT("DoorTest_Wide"))->GetOpenness()));
			Check(Door(TEXT("DoorTest_Double"))->IsFullyClosed(), FString::Printf(TEXT("the double door shut behind the NPC (openness %.2f)"), Door(TEXT("DoorTest_Double"))->GetOpenness()));
		} });
		T.Steps.Add({ 0.2f, [&T, W]()
		{
			for (FWalker& Wk : T.Walkers)
			{
				if (Wk.Body.IsValid())
				{
					Wk.Body->Destroy();
				}
			}
			T.Walkers.Reset();
			PlaceHero(W(), FVector(-170.f, 800.f, 0.f), 0.f);
		} });
		T.Steps.Add({ 0.5f, [&T, W]()
		{
			PressE(W(), IE_Pressed);
			T.bReleaseE = true;
		} });
		T.Steps.Add({ 1.5f, [&T, W, Door]()
		{
			Check(Door(TEXT("DoorTest_Locked"))->IsFullyClosed(), TEXT("E does not open a locked door"));
			// the state the save/load check expects (test_doors.py): standard open, wide shut, double open, locked locked
			Door(TEXT("DoorTest_Std"))->Open();
			Door(TEXT("DoorTest_Double"))->Open();
			UE_LOG(LogTemp, Warning, TEXT("DOORTEST DONE %d failed"), T.Fails);
		} });

		T.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
		{
			if (!GDoorTest || !GDoorTest->World.IsValid())
			{
				return false;
			}
			FDoorTest& S = *GDoorTest;
			UWorld* World = S.World.Get();
			if (S.bReleaseE)
			{
				PressE(World, IE_Released);
				S.bReleaseE = false;
			}
			if (S.bHeroWalk)
			{
				if (APawn* H = UGameplayStatics::GetPlayerPawn(World, 0))
				{
					H->AddMovementInput((S.HeroWalk - H->GetActorLocation()).GetSafeNormal2D(), 1.f);
				}
			}
			for (FWalker& Wk : S.Walkers)
			{
				DriveWalker(World, Wk);
			}
			// game time, not wall time: a long frame (a screenshot) is clamped and the world falls behind the clock
			S.Wait += World->GetDeltaSeconds();
			if (S.Steps.IsValidIndex(S.Next) && S.Wait >= S.Steps[S.Next].Key)
			{
				S.Wait = 0.f;
				S.Steps[S.Next++].Value();
			}
			return S.Steps.IsValidIndex(S.Next) || S.bHeroWalk || !S.Walkers.IsEmpty();
		}));
	}));

static FAutoConsoleCommandWithWorld GDoorCheckSaved(TEXT("Door.CheckSaved"),
	TEXT("After Door.SelfTest, lyud.Save, changing the doors and lyud.Load: logs DOORTEST lines on whether the doors came back as saved"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		auto Is = [World](const TCHAR* Tag) { return DoorTagged(World, FName(Tag)); };
		const ALyudDoorBase* Std = Is(TEXT("DoorTest_Std"));
		const ALyudDoorBase* Wide = Is(TEXT("DoorTest_Wide"));
		const ALyudDoorBase* Dbl = Is(TEXT("DoorTest_Double"));
		const ALyudDoorBase* Locked = Is(TEXT("DoorTest_Locked"));
		Check(Std && Std->IsPassable(), TEXT("loaded: the standard door is open again, leaf at rest open"));
		Check(Wide && Wide->IsFullyClosed(), TEXT("loaded: the wide door is shut again"));
		Check(Dbl && Dbl->IsPassable(), TEXT("loaded: the double door is open again"));
		Check(Locked && Locked->bLocked && Locked->IsFullyClosed(), TEXT("loaded: the locked door is still locked and shut"));
	}));
