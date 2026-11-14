// Developer-mode QA runs (docs/dev_mode.md "Tests"): every quest played through by its real events, every teleport point
// visited and checked for ground under the feet. They log DEVQA lines for test scripts and end with a DEVQA summary line.

#include "Dev/LyudDev.h"

#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sea/LyudSea.h"
#include "UI/LyudQuests.h"

namespace LyudDevQA
{
	/** The teleport walk: one point at a time, a look a little after landing (the body has settled, streaming caught up). */
	struct FTeleportRun
	{
		TWeakObjectPtr<UWorld> World;
		TArray<LyudDev::FTeleport> Points;
		int32 Next = 0;
		double CheckAt = 0.0;
		double Wait = 2.0;
		bool bShots = false;
		bool bQuit = false;
		int32 Good = 0;
		TArray<FString> Bad;
		FTSTicker::FDelegateHandle Tick;
		FVector Target = FVector::ZeroVector;
		bool bPending = false;
	};
	FTeleportRun GRun;

	/** Is there something solid (or the sea) within Reach below the pawn's feet? Returns the gap in cm (negative: none found). */
	double GapBelow(UWorld* World, APawn* Pawn, double Reach)
	{
		const ACharacter* Char = Cast<ACharacter>(Pawn);
		const double Half = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		const FVector Feet = Pawn->GetActorLocation() - FVector(0.0, 0.0, Half);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudDevQA), false, Pawn);
		double Gap = -1.0;
		if (World->LineTraceSingleByObjectType(Hit, Feet + FVector(0.0, 0.0, 20.0), Feet - FVector(0.0, 0.0, Reach), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Gap = FMath::Max(0.0, Feet.Z - Hit.ImpactPoint.Z);
		}
		if (const ALyudSea* Sea = ALyudSea::Get(World))
		{
			const double Water = Feet.Z - Sea->SeaLevel();
			if (Water > -300.0 && Water < Reach && (Gap < 0.0 || Water < Gap))
			{
				Gap = FMath::Max(0.0, Water);
			}
		}
		return Gap;
	}

	void Finish()
	{
		UE_LOG(LogTemp, Warning, TEXT("DEVQA teleports done: %d of %d landed, %d problems"), GRun.Good, GRun.Points.Num(), GRun.Bad.Num());
		for (const FString& B : GRun.Bad)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEVQA teleports problem %s"), *B);
		}
		const bool bQuit = GRun.bQuit;
		UWorld* World = GRun.World.Get();
		GRun = FTeleportRun(); // the ticker goes away by Step returning false
		if (bQuit && World)
		{
			GEngine->Exec(World, TEXT("quit"));
		}
	}

	bool Step(float)
	{
		UWorld* World = GRun.World.Get();
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		if (!World || !PC || !PC->GetPawn())
		{
			GRun.Bad.Add(TEXT("the world or the player went away"));
			Finish();
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		if (GRun.bPending && Now >= GRun.CheckAt)
		{
			// the point just visited: still where it was put, something under the feet
			const LyudDev::FTeleport& T = GRun.Points[GRun.Next - 1];
			APawn* Pawn = PC->GetPawn();
			const double Moved = FVector::Dist2D(Pawn->GetActorLocation(), GRun.Target);
			const double Gap = GapBelow(World, Pawn, 1000.0);
			FString Verdict = TEXT("ok");
			if (Moved > 300.0)
			{
				Verdict = FString::Printf(TEXT("MOVED %.0f m"), Moved / 100.0);
			}
			else if (Gap < 0.0)
			{
				Verdict = TEXT("NO GROUND (falling)");
			}
			else if (Gap > 200.0)
			{
				Verdict = FString::Printf(TEXT("IN THE AIR %.1f m"), Gap / 100.0);
			}
			UE_LOG(LogTemp, Warning, TEXT("DEVQA tp %s group %s at %.0f %.0f %.0f gap %.0f: %s"), *T.Id.ToString(), *T.Group.ToString(),
				Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z, Gap, *Verdict);
			if (Verdict == TEXT("ok"))
			{
				++GRun.Good;
			}
			else
			{
				GRun.Bad.Add(T.Id.ToString() + TEXT(": ") + Verdict);
			}
			if (GRun.bShots)
			{
				GEngine->Exec(World, TEXT("shot showui"));
			}
			GRun.bPending = false;
		}
		if (GRun.bPending)
		{
			return true;
		}
		if (GRun.Next >= GRun.Points.Num())
		{
			Finish();
			return false;
		}
		const LyudDev::FTeleport& T = GRun.Points[GRun.Next++];
		if (!LyudDev::Teleport(World, T))
		{
			UE_LOG(LogTemp, Warning, TEXT("DEVQA tp %s group %s: COULD NOT JUMP"), *T.Id.ToString(), *T.Group.ToString());
			GRun.Bad.Add(T.Id.ToString() + TEXT(": could not jump"));
			return true;
		}
		GRun.Target = PC->GetPawn()->GetActorLocation();
		GRun.CheckAt = Now + GRun.Wait;
		GRun.bPending = true;
		return true;
	}

	void StartTeleports(UWorld* World, const FString& Group, bool bShots, bool bQuit, double Wait)
	{
		if (GRun.Tick.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("DEVQA teleports: a run is going already"));
			return;
		}
		GRun.World = World;
		GRun.bShots = bShots;
		GRun.bQuit = bQuit;
		GRun.Wait = Wait;
		for (const LyudDev::FTeleport& T : LyudDev::Teleports())
		{
			// bookmarks are one person's own; everything else is the game's
			if (T.Group != TEXT("bookmarks") && (Group.IsEmpty() || Group == TEXT("all") || T.Group == FName(*Group)))
			{
				GRun.Points.Add(T);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("DEVQA teleports start: %d points%s"), GRun.Points.Num(), Group.IsEmpty() ? TEXT("") : *(TEXT(" in ") + Group));
		GRun.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step));
	}

	/** Every quest from scratch to its end; a quest that does not finish is named with the stage it stuck at. */
	void PlayAllQuests(UWorld* World, const FString& Only)
	{
		ULyudQuests* Q = ULyudQuests::Get(World);
		if (!Q)
		{
			return;
		}
		int32 Done = 0, Total = 0;
		TArray<FString> Stuck;
		for (const FLyudQuestDef& Def : ULyudQuests::Defs())
		{
			if (!Only.IsEmpty() && !Def.Id.ToString().StartsWith(Only))
			{
				continue;
			}
			++Total;
			Q->DevForget(Def.Id);
			if (LyudDev::PlayThrough(World, Def.Id))
			{
				++Done;
				UE_LOG(LogTemp, Warning, TEXT("DEVQA quest %s done (%d stages)"), *Def.Id.ToString(), Def.Stages.Num());
			}
			else
			{
				const FLyudQuestProgress* P = Q->FindProgress(Def.Id);
				const FString At = P && Def.Stages.IsValidIndex(P->Stage) ? Def.Stages[P->Stage].Id.ToString() : FString(TEXT("?"));
				Stuck.Add(Def.Id.ToString() + TEXT("/") + At);
				UE_LOG(LogTemp, Warning, TEXT("DEVQA quest %s STUCK at %s"), *Def.Id.ToString(), *At);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("DEVQA quests done: %d of %d played through%s%s"), Done, Total, Stuck.Num() ? TEXT(", stuck: ") : TEXT(""),
			*FString::Join(Stuck, TEXT(" ")));
	}

	FAutoConsoleCommandWithWorldAndArgs GQACmd(TEXT("Dev.QA"),
		TEXT("Dev.QA quests [id prefix] | teleports [group|all] [shots] [wait s] [quit]: plays every quest through by its real events, or visits every teleport point and checks the ground under the feet (DEVQA lines, a summary at the end; quit closes the game after the teleports)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!LyudDev::Enabled() || !World || Args.Num() == 0)
			{
				return;
			}
			if (Args[0] == TEXT("quests"))
			{
				PlayAllQuests(World, Args.Num() > 1 ? Args[1] : FString());
			}
			else if (Args[0] == TEXT("teleports"))
			{
				const FString Group = Args.Num() > 1 && Args[1] != TEXT("shots") && Args[1] != TEXT("quit") && !Args[1].IsNumeric() ? Args[1] : FString();
				const bool bShots = Args.Contains(TEXT("shots"));
				const bool bQuit = Args.Contains(TEXT("quit"));
				double Wait = 2.0;
				for (const FString& A : Args)
				{
					if (A.IsNumeric())
					{
						Wait = FMath::Max(0.5, FCString::Atod(*A));
					}
				}
				StartTeleports(World, Group, bShots, bQuit, Wait);
			}
		}));
}
