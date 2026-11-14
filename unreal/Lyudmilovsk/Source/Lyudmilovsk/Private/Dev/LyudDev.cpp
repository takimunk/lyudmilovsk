#include "Dev/LyudDev.h"

#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HeroCharacter.h"
#include "Bikes/HeroBike.h"
#include "Birds/BirdPawn.h"
#include "World/LyudLadder.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Sea/LyudSea.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UI/LyudQuests.h"

namespace LyudDev
{
	namespace
	{
		/** What a stage's "dev" block in quests.json asks for. */
		struct FStageDev
		{
			FName Tp;
			TArray<TPair<FName, int32>> Give;
			FString Time;
			TArray<FString> Exec;
		};

		struct FRegistry
		{
			TMap<FName, FAction> Actions;
			/** Code and data points by id (bookmarks and map places are added when listed). */
			TMap<FName, FTeleport> Code;
			TMap<FName, FTeleport> Files;
			TMap<FName, FTeleport> Bookmarks;
			TMap<FString, TArray<FRun>> Setups;
			TMap<FString, FStageDev> StageDev;
			bool bFilesRead = false;
			bool bBookmarksRead = false;
		};

		FRegistry& Reg()
		{
			static FRegistry R;
			return R;
		}

		FString StageKey(FName Quest, FName Stage) { return Quest.ToString() + TEXT("/") + Stage.ToString(); }

		TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
		{
			FString Text;
			TSharedPtr<FJsonObject> Root;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				return nullptr;
			}
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
			{
				UE_LOG(LogTemp, Warning, TEXT("DEV %s does not parse"), *Path);
				return nullptr;
			}
			return Root;
		}

		FTeleport ParseTeleport(const TSharedPtr<FJsonObject>& O, const FString& Source)
		{
			FTeleport T;
			T.Id = *O->GetStringField(TEXT("id"));
			FString Text;
			T.Group = O->TryGetStringField(TEXT("group"), Text) ? FName(*Text) : FName(*FPaths::GetBaseFilename(Source));
			O->TryGetStringField(TEXT("name"), T.Name);
			if (O->TryGetStringField(TEXT("place"), Text))
			{
				T.Place = *Text;
			}
			O->TryGetNumberField(TEXT("x"), T.Map.X);
			O->TryGetNumberField(TEXT("y"), T.Map.Y);
			double Number = 0.0;
			if (O->TryGetNumberField(TEXT("z"), Number))
			{
				T.Z = Number;
			}
			if (O->TryGetNumberField(TEXT("yaw"), Number))
			{
				T.Yaw = static_cast<float>(Number);
			}
			if (O->TryGetNumberField(TEXT("pitch"), Number))
			{
				T.Pitch = static_cast<float>(Number);
			}
			O->TryGetStringField(TEXT("time"), T.Time);
			T.Source = Source;
			return T;
		}

		FString BookmarksPath() { return FPaths::ProjectSavedDir() / TEXT("Dev") / TEXT("bookmarks.json"); }

		void ReadCityPlan(TMap<FName, FTeleport>& Out);   // below: the city plan's plots, squares and shops
		void ReadFiles()
		{
			FRegistry& R = Reg();
			if (R.bFilesRead)
			{
				return;
			}
			R.bFilesRead = true;
			R.Files.Reset();
			R.StageDev.Reset();
			// Content/Dev/teleports/*.json: one file per feature
			const FString Dir = FPaths::ProjectContentDir() / TEXT("Dev") / TEXT("teleports");
			TArray<FString> Names;
			IFileManager::Get().FindFiles(Names, *(Dir / TEXT("*.json")), true, false);
			Names.Sort();
			for (const FString& FileName : Names)
			{
				const TSharedPtr<FJsonObject> Root = ReadJsonFile(Dir / FileName);
				const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
				if (!Root || !Root->TryGetArrayField(TEXT("teleports"), List))
				{
					continue;
				}
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					if (O && O->HasField(TEXT("id")))
					{
						const FTeleport T = ParseTeleport(O, FileName);
						R.Files.Add(T.Id, T);
					}
				}
			}
			ReadCityPlan(R.Files);
			// the stages' "dev" blocks in quests.json
			if (const TSharedPtr<FJsonObject> Root = ReadJsonFile(FPaths::ProjectContentDir() / TEXT("UI") / TEXT("quests.json")))
			{
				const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
				Root->TryGetArrayField(TEXT("quests"), Quests);
				for (const TSharedPtr<FJsonValue>& QV : Quests ? *Quests : TArray<TSharedPtr<FJsonValue>>())
				{
					const TSharedPtr<FJsonObject> Q = QV->AsObject();
					const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
					if (!Q || !Q->TryGetArrayField(TEXT("stages"), Stages))
					{
						continue;
					}
					for (const TSharedPtr<FJsonValue>& SV : *Stages)
					{
						const TSharedPtr<FJsonObject> S = SV->AsObject();
						const TSharedPtr<FJsonObject>* DevBlock = nullptr;
						if (!S || !S->TryGetObjectField(TEXT("dev"), DevBlock))
						{
							continue;
						}
						FStageDev D;
						FString Text;
						if ((*DevBlock)->TryGetStringField(TEXT("tp"), Text))
						{
							D.Tp = *Text;
						}
						(*DevBlock)->TryGetStringField(TEXT("time"), D.Time);
						const TSharedPtr<FJsonObject>* Give = nullptr;
						if ((*DevBlock)->TryGetObjectField(TEXT("give"), Give))
						{
							for (const TPair<FString, TSharedPtr<FJsonValue>>& It : (*Give)->Values)
							{
								D.Give.Add({ FName(*It.Key), static_cast<int32>(It.Value->AsNumber()) });
							}
						}
						const TArray<TSharedPtr<FJsonValue>>* Exec = nullptr;
						if ((*DevBlock)->TryGetArrayField(TEXT("exec"), Exec))
						{
							for (const TSharedPtr<FJsonValue>& E : *Exec)
							{
								D.Exec.Add(E->AsString());
							}
						}
						R.StageDev.Add(StageKey(*Q->GetStringField(TEXT("id")), *S->GetStringField(TEXT("id"))), MoveTemp(D));
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("DEV read %d teleport files (%d points), %d quest dev blocks"), Names.Num(), R.Files.Num(), R.StageDev.Num());
		}

		/** A city_plan.json point [x, y, z] in map metres. */
		bool PlanPoint(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FVector& Out)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (!O->TryGetArrayField(Field, A) || A->Num() < 3)
			{
				return false;
			}
			Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
			return true;
		}

		/** The city's plots (at their front, facing the building), squares and parks, and the promenade's shops (at the door). */
		void ReadCityPlan(TMap<FName, FTeleport>& Out)
		{
			const TSharedPtr<FJsonObject> Root = ReadJsonFile(FPaths::ProjectContentDir() / TEXT("City") / TEXT("city_plan.json"));
			if (!Root)
			{
				return;
			}
			const FString NameField = LyudText::Language() == TEXT("ru") ? TEXT("name_ru") : TEXT("name_en");
			auto Add = [&Out, &NameField](const TSharedPtr<FJsonObject>& O, FName Group, const FVector& Feet, float Yaw)
			{
				FTeleport T;
				T.Id = *O->GetStringField(TEXT("id"));
				T.Group = Group;
				if (!O->TryGetStringField(*NameField, T.Label))
				{
					O->TryGetStringField(TEXT("name_en"), T.Label);
				}
				FString Kind;
				if (O->TryGetStringField(TEXT("kind"), Kind) && T.Label.IsEmpty())
				{
					T.Label = Kind;
				}
				T.Map = FVector2D(Feet.X, Feet.Y);
				T.Z = Feet.Z * 100.0 + 5.0;
				T.Yaw = Yaw;
				T.Source = TEXT("city_plan.json");
				Out.Add(T.Id, T);
			};
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Root->TryGetArrayField(TEXT("plots"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FVector Front, Middle;
					if (O && PlanPoint(O, TEXT("front"), Front) && PlanPoint(O, TEXT("map"), Middle))
					{
						// a few metres out from the front, looking at the building
						const FVector2D In = FVector2D(Middle - Front).GetSafeNormal();
						const FVector At = Front - FVector(In * 4.0, 0.0);
						Add(O, TEXT("city_plots"), At, FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X)));
					}
				}
			}
			if (Root->TryGetArrayField(TEXT("spots"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FVector At;
					double Yaw = 0.0;
					// the benches are too many to list
					if (O && O->GetStringField(TEXT("kind")) != TEXT("bench") && PlanPoint(O, TEXT("map"), At))
					{
						O->TryGetNumberField(TEXT("yaw"), Yaw);
						Add(O, TEXT("city_spots"), At, static_cast<float>(Yaw));
					}
				}
			}
			if (Root->TryGetArrayField(TEXT("promenade_units"), List))
			{
				for (const TSharedPtr<FJsonValue>& V : *List)
				{
					const TSharedPtr<FJsonObject> O = V->AsObject();
					FVector Door;
					double Yaw = 0.0;
					if (O && PlanPoint(O, TEXT("door"), Door))
					{
						O->TryGetNumberField(TEXT("yaw"), Yaw);
						Add(O, TEXT("city_shops"), Door, static_cast<float>(Yaw));
					}
				}
			}
		}

		void ReadBookmarks()
		{
			FRegistry& R = Reg();
			if (R.bBookmarksRead)
			{
				return;
			}
			R.bBookmarksRead = true;
			const TSharedPtr<FJsonObject> Root = ReadJsonFile(BookmarksPath());
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (!Root || !Root->TryGetArrayField(TEXT("teleports"), List))
			{
				return;
			}
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (O && O->HasField(TEXT("id")))
				{
					FTeleport T = ParseTeleport(O, TEXT("bookmark"));
					T.Group = TEXT("bookmarks");
					R.Bookmarks.Add(T.Id, T);
				}
			}
		}

		void WriteBookmarks()
		{
			TArray<TSharedPtr<FJsonValue>> List;
			for (const TPair<FName, FTeleport>& It : Reg().Bookmarks)
			{
				const FTeleport& T = It.Value;
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetStringField(TEXT("id"), T.Id.ToString());
				O->SetStringField(TEXT("name"), T.Name);
				O->SetNumberField(TEXT("x"), T.Map.X);
				O->SetNumberField(TEXT("y"), T.Map.Y);
				if (T.Z.IsSet())
				{
					O->SetNumberField(TEXT("z"), T.Z.GetValue());
				}
				O->SetNumberField(TEXT("yaw"), T.Yaw);
				O->SetNumberField(TEXT("pitch"), T.Pitch);
				List.Add(MakeShared<FJsonValueObject>(O));
			}
			TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetArrayField(TEXT("teleports"), List);
			FString Text;
			FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
			FFileHelper::SaveStringToFile(Text, *BookmarksPath());
		}

		APlayerController* PlayerOf(UWorld* World)
		{
			return World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		}

		/** The top of whatever is solid at X, Y between Top and Bottom (the sea's surface where that is higher), world cm;
		 *  false: nothing there. */
		bool GroundAt(UWorld* World, const FVector2D& XY, double& OutZ, double Top = 200000.0, double Bottom = -100000.0)
		{
			FHitResult Hit;
			FCollisionObjectQueryParams Objects(ECC_WorldStatic);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(LyudDevGround), false);
			ALyudSea* Sea = ALyudSea::Get(World);
			if (Sea)
			{
				Params.AddIgnoredActor(Sea);
				if (AActor* Vol = Sea->GetWaterVolume())
				{
					Params.AddIgnoredActor(Vol);
				}
			}
			if (APlayerController* PC = PlayerOf(World); PC && PC->GetPawn())
			{
				Params.AddIgnoredActor(PC->GetPawn());
			}
			const bool bHit = World->LineTraceSingleByObjectType(Hit, FVector(XY, Top), FVector(XY, Bottom), Objects, Params);
			if (!bHit && !Sea)
			{
				return false;
			}
			OutZ = bHit ? Hit.ImpactPoint.Z : -100000.0;
			if (Sea)
			{
				OutZ = FMath::Max(OutZ, static_cast<double>(Sea->SeaLevel()));
			}
			return true;
		}

		void SetClock(UWorld* World, const FString& HHMM)
		{
			FString H, M;
			UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
			if (!Cal || !HHMM.Split(TEXT(":"), &H, &M))
			{
				return;
			}
			Cal->SetNow(Cal->Now().GetDate() + FTimespan(FCString::Atoi(*H), FCString::Atoi(*M), 0));
		}

		/** The map frame's metres of a world point, and back (Content/UI/map.json's frame). */
		FTeleport FromMapPlace(const FLyudMapPlace& P)
		{
			FTeleport T;
			T.Id = P.Id;
			T.Group = TEXT("map");
			T.Place = P.Id;
			T.Map = P.At;
			T.Source = TEXT("map");
			return T;
		}
	}

	FText NameOf(const FString& Key, FName Id)
	{
		if (!Key.IsEmpty())
		{
			const FText T = LyudText::Get(Key);
			if (T.ToString() != Key)
			{
				return T;
			}
		}
		return FText::FromName(Id);
	}

	bool Enabled()
	{
#if UE_BUILD_SHIPPING
		static const bool bDev = FParse::Param(FCommandLine::Get(), TEXT("dev"));
		return bDev;
#else
		return true;
#endif
	}

	void AddAction(const FAction& Action) { Reg().Actions.Add(Action.Id, Action); }
	void AddTeleport(const FTeleport& Teleport) { Reg().Code.Add(Teleport.Id, Teleport); }
	void AddQuestSetup(FName Quest, FName Stage, FRun Run) { Reg().Setups.FindOrAdd(StageKey(Quest, Stage)).Add(MoveTemp(Run)); }

	TArray<const FAction*> Actions()
	{
		TArray<const FAction*> Out;
		for (const TPair<FName, FAction>& It : Reg().Actions)
		{
			Out.Add(&It.Value);
		}
		Out.Sort([](const FAction& A, const FAction& B) { return A.Group != B.Group ? A.Group.LexicalLess(B.Group) : A.Id.LexicalLess(B.Id); });
		return Out;
	}

	const FAction* FindAction(FName Id) { return Reg().Actions.Find(Id); }

	TArray<FTeleport> Teleports()
	{
		ReadFiles();
		ReadBookmarks();
		FRegistry& R = Reg();
		TMap<FName, FTeleport> All;
		for (const FLyudMapPlace& P : ULyudQuests::Places())
		{
			All.Add(P.Id, FromMapPlace(P));
		}
		All.Append(R.Files);
		All.Append(R.Code);
		All.Append(R.Bookmarks);
		TArray<FTeleport> Out;
		All.GenerateValueArray(Out);
		Out.Sort([](const FTeleport& A, const FTeleport& B) { return A.Group != B.Group ? A.Group.LexicalLess(B.Group) : A.Id.LexicalLess(B.Id); });
		return Out;
	}

	bool FindTeleport(FName Id, FTeleport& Out)
	{
		ReadFiles();
		ReadBookmarks();
		FRegistry& R = Reg();
		for (const TMap<FName, FTeleport>* M : { &R.Bookmarks, &R.Code, &R.Files })
		{
			if (const FTeleport* T = M->Find(Id))
			{
				Out = *T;
				return true;
			}
		}
		if (const FLyudMapPlace* P = ULyudQuests::FindPlace(Id))
		{
			Out = FromMapPlace(*P);
			return true;
		}
		return false;
	}

	bool Do(UWorld* World, FName Id)
	{
		if (!Enabled())
		{
			return false;
		}
		const FAction* A = FindAction(Id);
		if (!A || !A->Run || !World)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEV do %s: no such action"), *Id.ToString());
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("DEV do %s"), *Id.ToString());
		A->Run(World);
		return true;
	}

	bool TeleportWorld(UWorld* World, const FVector& At, float Yaw, bool bGround, float Pitch)
	{
		if (!Enabled())
		{
			return false;
		}
		APlayerController* PC = PlayerOf(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			return false;
		}
		// whatever holds the body in place every frame lets go first, or the jump is undone on the next frame
		if (AHeroCharacter* Hero = Cast<AHeroCharacter>(Pawn))
		{
			if (Hero->IsSeated())
			{
				Hero->StandUp();
			}
			if (UHeroBikeComponent* Bike = Hero->FindBike())
			{
				Bike->DropNow();
			}
			if (ULyudLadderClimber* Climber = Hero->FindComponentByClass<ULyudLadderClimber>(); Climber && Climber->IsOnLadder())
			{
				Climber->LetGo();
			}
		}
		// At.Z is where the feet go (the capsule's middle is half its height above)
		FVector P = At;
		ACharacter* Char = Cast<ACharacter>(Pawn);
		const double Half = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 60.0;
		if (bGround)
		{
			double Z = 0.0;
			if (!GroundAt(World, FVector2D(At), Z))
			{
				Z = 10000.0;
			}
			P.Z = Z + Half + 5.0;
		}
		else
		{
			P.Z = At.Z + Half + 2.0;
		}
		if (ABirdPawn* BirdPawn = Cast<ABirdPawn>(Pawn))
		{
			BirdPawn->DevTeleport(P);
		}
		else
		{
			Pawn->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Pawn->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
		if (Char && Char->GetCharacterMovement())
		{
			Char->GetCharacterMovement()->StopMovementImmediately();
		}
		UE_LOG(LogTemp, Warning, TEXT("DEV tp at %.0f %.0f %.0f yaw %.0f"), P.X, P.Y, P.Z, Yaw);
		return true;
	}

	bool Teleport(UWorld* World, const FTeleport& To)
	{
		if (!World)
		{
			return false;
		}
		bool bDone = false;
		if (To.Where)
		{
			FTransform At;
			if (!To.Where(World, At))
			{
				UE_LOG(LogTemp, Warning, TEXT("DEV tp %s: not there now"), *To.Id.ToString());
				return false;
			}
			const FRotator R = At.Rotator();
			bDone = TeleportWorld(World, At.GetLocation(), R.Yaw, false, R.Pitch);
		}
		else
		{
			FVector2D M = To.Map;
			if (!To.Place.IsNone())
			{
				if (const FLyudMapPlace* P = ULyudQuests::FindPlace(To.Place))
				{
					M = P->At;
				}
			}
			FVector W = ULyudQuests::Frame().ToWorld(M, To.Z.Get(0.0));
			if (To.Z.IsSet())
			{
				// a given floor height may be a little off (the city plan's terraces vs the built ground): stand on the first
				// solid thing from a metre and a half above it down to 20 m below (a ceiling is higher than that, so indoors
				// still lands on the floor, not the roof)
				double Floor = 0.0;
				if (GroundAt(World, FVector2D(W), Floor, W.Z + 150.0, W.Z - 2000.0) && Floor <= W.Z + 150.0 && Floor >= W.Z - 2000.0)
				{
					W.Z = Floor;
				}
			}
			bDone = TeleportWorld(World, W, To.Yaw, !To.Z.IsSet(), To.Pitch);
		}
		if (bDone && !To.Time.IsEmpty())
		{
			SetClock(World, To.Time);
		}
		UE_LOG(LogTemp, Warning, TEXT("DEV tp %s %s"), *To.Id.ToString(), bDone ? TEXT("ok") : TEXT("FAILED"));
		return bDone;
	}

	bool TeleportTo(UWorld* World, FName Id)
	{
		FTeleport T;
		if (!FindTeleport(Id, T))
		{
			UE_LOG(LogTemp, Warning, TEXT("DEV tp %s: no such point"), *Id.ToString());
			return false;
		}
		return Teleport(World, T);
	}

	bool JumpToStage(UWorld* World, FName Quest, int32 Stage)
	{
		if (!Enabled())
		{
			return false;
		}
		ULyudQuests* Q = ULyudQuests::Get(World);
		const FLyudQuestDef* Def = ULyudQuests::FindDef(Quest);
		if (!Q || !Def || !Def->Stages.IsValidIndex(Stage))
		{
			UE_LOG(LogTemp, Warning, TEXT("DEV quest %s stage %d: no such stage"), *Quest.ToString(), Stage);
			return false;
		}
		ReadFiles();
		const FName StageId = Def->Stages[Stage].Id;
		Q->DevJump(Quest, Stage);
		const FString Key = StageKey(Quest, StageId);
		if (const FStageDev* D = Reg().StageDev.Find(Key))
		{
			if (ULyudInventory* Inv = ULyudInventory::Get(World))
			{
				for (const TPair<FName, int32>& G : D->Give)
				{
					const int32 Missing = G.Value - Inv->Count(G.Key);
					if (Missing > 0)
					{
						Inv->Add(G.Key, Missing);
					}
				}
			}
			if (!D->Tp.IsNone())
			{
				TeleportTo(World, D->Tp);
			}
			if (!D->Time.IsEmpty())
			{
				SetClock(World, D->Time);
			}
			for (const FString& Cmd : D->Exec)
			{
				GEngine->Exec(World, *Cmd);
			}
		}
		if (const TArray<FRun>* Runs = Reg().Setups.Find(Key))
		{
			for (const FRun& Run : *Runs)
			{
				Run(World);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("DEV quest %s stage %d (%s)"), *Quest.ToString(), Stage, *StageId.ToString());
		return true;
	}

	bool CompleteObjective(UWorld* World, FName Quest, int32 Objective)
	{
		if (!Enabled())
		{
			return false;
		}
		ULyudQuests* Q = ULyudQuests::Get(World);
		if (!Q)
		{
			return false;
		}
		const TArray<TPair<const FLyudQuestObjective*, int32>> Objs = Q->CurrentObjectives(Quest);
		if (!Objs.IsValidIndex(Objective))
		{
			return false;
		}
		const FLyudQuestObjective& O = *Objs[Objective].Key;
		// the first of alternative events, the target with any '*' made concrete enough to match
		FString Event = O.Event.ToString();
		Event.Split(TEXT("|"), &Event, nullptr);
		FString Target = O.Target;
		Target.RemoveFromEnd(TEXT("*"));
		const int32 Left = FMath::Max(1, O.Need - Objs[Objective].Value);
		UE_LOG(LogTemp, Warning, TEXT("DEV objective %s.%s: %s %s x%d"), *Quest.ToString(), *O.Id.ToString(), *Event, *Target, Left);
		if (Event == TEXT("have") || Event == TEXT("get"))
		{
			// a real item into the pocket: the inventory reports get (and have stays true)
			if (ULyudInventory* Inv = ULyudInventory::Get(World); Inv && ULyudInventory::FindDef(*Target))
			{
				Inv->Add(*Target, Left);
				return true;
			}
		}
		ULyudQuests::Notify(World, *Event, Target, Left);
		return true;
	}

	bool PlayThrough(UWorld* World, FName Quest)
	{
		if (!Enabled())
		{
			return false;
		}
		ULyudQuests* Q = ULyudQuests::Get(World);
		const FLyudQuestDef* Def = ULyudQuests::FindDef(Quest);
		if (!Q || !Def)
		{
			return false;
		}
		if (Q->State(Quest) != ELyudQuestState::Active)
		{
			JumpToStage(World, Quest, 0);
		}
		// each round completes the current stage's objectives; a stage that does not move on is stuck
		for (int32 Round = 0; Round < Def->Stages.Num() + 2 && Q->State(Quest) == ELyudQuestState::Active; ++Round)
		{
			const FLyudQuestProgress* P = Q->FindProgress(Quest);
			const int32 Before = P ? P->Stage : -1;
			UE_LOG(LogTemp, Warning, TEXT("DEV play %s stage %s"), *Quest.ToString(), Def->Stages.IsValidIndex(Before) ? *Def->Stages[Before].Id.ToString() : TEXT("?"));
			const int32 Count = Q->CurrentObjectives(Quest).Num();
			for (int32 O = 0; O < Count && Q->State(Quest) == ELyudQuestState::Active; ++O)
			{
				const FLyudQuestProgress* Now = Q->FindProgress(Quest);
				if (!Now || Now->Stage != Before)
				{
					break;
				}
				if (!Now->Counts.IsValidIndex(O) || Now->Counts[O] < Def->Stages[Before].Objectives[O].Need)
				{
					CompleteObjective(World, Quest, O);
				}
			}
			const FLyudQuestProgress* After = Q->FindProgress(Quest);
			if (Q->State(Quest) == ELyudQuestState::Active && After && After->Stage == Before)
			{
				UE_LOG(LogTemp, Warning, TEXT("DEV play %s STUCK at stage %s"), *Quest.ToString(), *Def->Stages[Before].Id.ToString());
				return false;
			}
		}
		const bool bDone = Q->State(Quest) == ELyudQuestState::Done;
		UE_LOG(LogTemp, Warning, TEXT("DEV play %s %s"), *Quest.ToString(), bDone ? TEXT("done") : TEXT("STUCK"));
		return bDone;
	}

	FString Where(UWorld* World, bool bCopy)
	{
		APlayerController* PC = PlayerOf(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			return FString();
		}
		// the feet, as Dev.TpAt and the teleport files take it
		const ACharacter* Char = Cast<ACharacter>(Pawn);
		const double Half = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		const FVector P = Pawn->GetActorLocation() - FVector(0.0, 0.0, Half);
		const FRotator R = PC->GetControlRotation();
		const FVector2D M = ULyudQuests::Frame().FromWorld(P);
		const FString Line = FString::Printf(TEXT("Dev.TpAt %.0f %.0f %.0f %.0f   (map x %.1f y %.1f)"), P.X, P.Y, P.Z, R.Yaw, M.X, M.Y);
		if (bCopy)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Line);
			UE_LOG(LogTemp, Warning, TEXT("DEV where %s"), *Line);
		}
		return Line;
	}

	FName AddBookmark(UWorld* World, const FString& Name)
	{
		APlayerController* PC = PlayerOf(World);
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			return NAME_None;
		}
		ReadBookmarks();
		FRegistry& R = Reg();
		FString Id = Name;
		for (int32 N = R.Bookmarks.Num() + 1; Id.IsEmpty() || (Name.IsEmpty() && R.Bookmarks.Contains(*Id)); ++N)
		{
			Id = FString::Printf(TEXT("bm_%d"), N);
		}
		const FVector P = Pawn->GetActorLocation();
		const ACharacter* Char = Cast<ACharacter>(Pawn);
		const double Half = Char && Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		FTeleport T;
		T.Id = *Id;
		T.Group = TEXT("bookmarks");
		T.Name = Id;
		T.Map = ULyudQuests::Frame().FromWorld(P);
		// the feet, so the jump back drops the capsule onto the same floor (indoors too)
		T.Z = P.Z - Half + 5.0;
		T.Yaw = PC->GetControlRotation().Yaw;
		T.Source = TEXT("bookmark");
		R.Bookmarks.Add(T.Id, T);
		WriteBookmarks();
		UE_LOG(LogTemp, Warning, TEXT("DEV bookmark %s"), *Id);
		return T.Id;
	}

	void RemoveBookmark(FName Id)
	{
		ReadBookmarks();
		if (Reg().Bookmarks.Remove(Id) > 0)
		{
			WriteBookmarks();
		}
	}

	void Reload()
	{
		Reg().bFilesRead = false;
		Reg().bBookmarksRead = false;
		ULyudQuests::ReloadData();
	}

	FAutoAction::FAutoAction(const TCHAR* Id, const TCHAR* Group, const TCHAR* NameKey, FRun Run, FIsOn IsOn)
	{
		AddAction(FAction{ FName(Id), FName(Group), FString(NameKey), MoveTemp(Run), MoveTemp(IsOn) });
	}

	FAutoTeleport::FAutoTeleport(const TCHAR* Id, const TCHAR* Group, const TCHAR* NameKey, FWhere WhereFn)
	{
		FTeleport T;
		T.Id = Id;
		T.Group = Group;
		T.Name = NameKey;
		T.Where = MoveTemp(WhereFn);
		T.Source = TEXT("code");
		AddTeleport(T);
	}

	FAutoQuestSetup::FAutoQuestSetup(const TCHAR* Quest, const TCHAR* Stage, FRun Run)
	{
		AddQuestSetup(Quest, Stage, MoveTemp(Run));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Built-in actions: movement, time, the pocket, birds, the quests and the map. Features add their own the same way.

// a named namespace, not an anonymous one: unity builds put this file next to others with their own HeroOf, Exec...
namespace LyudDevBuiltins
{
	using namespace LyudDev;

	AHeroCharacter* HeroOf(UWorld* World)
	{
		APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
		return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	}

	void Exec(UWorld* World, const TCHAR* Cmd)
	{
		if (GEngine && World)
		{
			GEngine->Exec(World, Cmd);
		}
	}

	/** Free flight through walls: W/S along the view (up and down with it), Space up, C down; the hero's own speed ×6. */
	struct FFly
	{
		TWeakObjectPtr<AHeroCharacter> Hero;
		FTSTicker::FDelegateHandle Tick;
	};
	FFly GFly;

	bool IsFlying(UWorld* World)
	{
		return GFly.Hero.IsValid() && GFly.Hero->GetWorld() == World;
	}

	void SetFly(UWorld* World, bool bOn)
	{
		if (!bOn || IsFlying(World))
		{
			if (AHeroCharacter* H = GFly.Hero.Get())
			{
				H->SetActorEnableCollision(true);
				H->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
			GFly.Hero.Reset();
			if (GFly.Tick.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(GFly.Tick);
				GFly.Tick.Reset();
			}
			return;
		}
		AHeroCharacter* Hero = HeroOf(World);
		// a seat or a bike holds the body itself
		if (!Hero || Hero->IsSeated() || Hero->IsOnBike())
		{
			return;
		}
		GFly.Hero = Hero;
		Hero->SetActorEnableCollision(false);
		Hero->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Hero->GetCharacterMovement()->MaxFlySpeed = Hero->RunSpeed * 6.f;
		Hero->GetCharacterMovement()->BrakingDecelerationFlying = 4000.f;
		GFly.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			AHeroCharacter* H = GFly.Hero.Get();
			APlayerController* PC = H ? Cast<APlayerController>(H->GetController()) : nullptr;
			if (!H || !PC)
			{
				// the hero lost his controller (became a bird): give him his body back as it was
				if (H)
				{
					H->SetActorEnableCollision(true);
					H->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				}
				GFly.Hero.Reset();
				GFly.Tick.Reset();
				return false;
			}
			H->SetActorEnableCollision(false);
			if (H->GetCharacterMovement()->MovementMode != MOVE_Flying)
			{
				H->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
			}
			// the hero's own input moves him flat; the view's pitch and Space / C add the height
			const float Ahead = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
			const float Up = (PC->IsInputKeyDown(EKeys::SpaceBar) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::C) ? 1.f : 0.f);
			const float Pitch = FMath::DegreesToRadians(PC->GetControlRotation().Pitch);
			H->AddMovementInput(FVector::UpVector, FMath::Clamp(Ahead * FMath::Sin(Pitch) + Up, -1.f, 1.f));
			return true;
		}));
	}

	/** The hero runs ×4 (walk, run and crouch alike); his own speeds are kept to put back. */
	struct FFast
	{
		TWeakObjectPtr<AHeroCharacter> Hero;
		FVector Speeds = FVector::ZeroVector;
	};
	FFast GFast;

	bool IsFast(UWorld* World)
	{
		AHeroCharacter* H = HeroOf(World);
		return H && GFast.Hero.Get() == H;
	}

	void SetFast(UWorld* World, bool bOn)
	{
		AHeroCharacter* H = HeroOf(World);
		if (!H || bOn == IsFast(World))
		{
			return;
		}
		if (bOn)
		{
			GFast.Hero = H;
			GFast.Speeds = FVector(H->WalkSpeed, H->RunSpeed, H->CrouchSpeed);
			H->WalkSpeed *= 4.f;
			H->RunSpeed *= 4.f;
			H->CrouchSpeed *= 4.f;
		}
		else
		{
			H->WalkSpeed = GFast.Speeds.X;
			H->RunSpeed = GFast.Speeds.Y;
			H->CrouchSpeed = GFast.Speeds.Z;
			GFast.Hero.Reset();
		}
	}

	UGameCalendar* CalOf(UWorld* World) { return World ? World->GetSubsystem<UGameCalendar>() : nullptr; }
	void At(UWorld* World, int32 H, int32 M)
	{
		if (UGameCalendar* Cal = CalOf(World))
		{
			Cal->SetNow(Cal->Now().GetDate() + FTimespan(H, M, 0));
		}
	}

	bool GStats = false;

	/** A line at the top of the screen for screenshots and bug reports: where, when, frame time, the quest step. Takes no input. */
	struct FOverlay
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UGameViewportClient> Viewport;
		TSharedPtr<SWidget> Widget;
		FText Line;
		double NextAt = 0.0;
		float FrameMs = 0.f;
	};
	FOverlay GOverlay;

	bool IsOverlay(UWorld* World) { return GOverlay.Widget.IsValid() && GOverlay.World.Get() == World; }

	FText OverlayLine()
	{
		// rebuilt four times a second, not every frame
		const float Ms = static_cast<float>(FApp::GetDeltaTime() * 1000.0);
		GOverlay.FrameMs = GOverlay.FrameMs > 0.f ? FMath::Lerp(GOverlay.FrameMs, Ms, 0.1f) : Ms;
		const double Now = FPlatformTime::Seconds();
		UWorld* World = GOverlay.World.Get();
		if (Now < GOverlay.NextAt || !World)
		{
			return GOverlay.Line;
		}
		GOverlay.NextAt = Now + 0.25;
		FString Quest;
		if (const ULyudQuests* Q = ULyudQuests::Get(World))
		{
			const FName Shown = Q->ShownQuest();
			const FLyudQuestProgress* P = Q->FindProgress(Shown);
			const FLyudQuestDef* Def = ULyudQuests::FindDef(Shown);
			if (P && Def && Def->Stages.IsValidIndex(P->Stage))
			{
				Quest = FString::Printf(TEXT("  ·  %s/%s"), *Shown.ToString(), *Def->Stages[P->Stage].Id.ToString());
			}
		}
		const UGameCalendar* Cal = World->GetSubsystem<UGameCalendar>();
		const FString When = Cal ? Cal->Now().ToString(TEXT("%d.%m.%Y %H:%M")) : FString();
		GOverlay.Line = FText::FromString(FString::Printf(TEXT("%s  ·  %s  ·  %.1f ms (%.0f fps)%s"), *Where(World, false), *When,
			GOverlay.FrameMs, GOverlay.FrameMs > 0.f ? 1000.f / GOverlay.FrameMs : 0.f, *Quest));
		return GOverlay.Line;
	}

	void SetOverlay(UWorld* World, bool bOn)
	{
		if (GOverlay.Widget.IsValid())
		{
			if (UGameViewportClient* V = GOverlay.Viewport.Get())
			{
				V->RemoveViewportWidgetContent(GOverlay.Widget.ToSharedRef());
			}
			GOverlay.Widget.Reset();
		}
		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		if (!bOn || !Viewport)
		{
			return;
		}
		GOverlay.World = World;
		GOverlay.Viewport = Viewport;
		GOverlay.NextAt = 0.0;
		GOverlay.Widget = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 6.f)).Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)).Padding(FMargin(10.f, 3.f))
			[SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor::White).Text_Static(&OverlayLine)]
		];
		// over the HUD (40), under the dev panel (95)
		Viewport->AddViewportWidgetContent(GOverlay.Widget.ToSharedRef(), 90);
	}

	FAutoTeleport GWakeTp(TEXT("club_wake"), TEXT("club"), TEXT("dev.tp.club_wake"), [](UWorld* W, FTransform& Out)
	{
		TActorIterator<APlayerStart> Start(W);
		if (!Start)
		{
			return false;
		}
		Out = Start->GetActorTransform();
		// the start's capsule middle down to its feet
		const double StartHalf = Start->GetCapsuleComponent() ? Start->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0;
		Out.AddToTranslation(FVector(0.0, 0.0, -StartHalf));
		return true;
	});

	FAutoAction GFlyAction(TEXT("move.fly"), TEXT("move"), TEXT("dev.action.move.fly"),
		[](UWorld* W) { SetFly(W, !IsFlying(W)); }, [](UWorld* W) { return IsFlying(W); });
	FAutoAction GFastAction(TEXT("move.fast"), TEXT("move"), TEXT("dev.action.move.fast"),
		[](UWorld* W) { SetFast(W, !IsFast(W)); }, [](UWorld* W) { return IsFast(W); });
	FAutoAction GReturnAction(TEXT("move.bird_return"), TEXT("move"), TEXT("dev.action.move.bird_return"),
		[](UWorld* W) { Exec(W, TEXT("Bird.Return now")); });
	FAutoAction GBookmarkAction(TEXT("move.bookmark"), TEXT("move"), TEXT("dev.action.move.bookmark"),
		[](UWorld* W) { AddBookmark(W, FString()); });
	FAutoAction GWhereAction(TEXT("move.where"), TEXT("move"), TEXT("dev.action.move.where"),
		[](UWorld* W) { Where(W, true); });

	FAutoAction GDawn(TEXT("time.dawn"), TEXT("time"), TEXT("dev.action.time.dawn"), [](UWorld* W) { At(W, 5, 30); });
	FAutoAction GNoon(TEXT("time.noon"), TEXT("time"), TEXT("dev.action.time.noon"), [](UWorld* W) { At(W, 12, 0); });
	FAutoAction GEvening(TEXT("time.evening"), TEXT("time"), TEXT("dev.action.time.evening"), [](UWorld* W) { At(W, 19, 0); });
	FAutoAction GNight(TEXT("time.night"), TEXT("time"), TEXT("dev.action.time.night"), [](UWorld* W) { At(W, 23, 30); });
	FAutoAction GPlusDay(TEXT("time.plus_day"), TEXT("time"), TEXT("dev.action.time.plus_day"),
		[](UWorld* W) { if (UGameCalendar* Cal = CalOf(W)) Cal->SetNow(Cal->Now() + FTimespan::FromDays(1.0)); });
	/** The speed time ran at before Freeze, to give back (a save while frozen keeps 0: unfreeze before saving). */
	float GFrozenScale = 1.f;
	FAutoAction GFreeze(TEXT("time.freeze"), TEXT("time"), TEXT("dev.action.time.freeze"),
		[](UWorld* W)
		{
			if (UGameCalendar* Cal = CalOf(W))
			{
				if (Cal->TimeScale > 0.f)
				{
					GFrozenScale = Cal->TimeScale;
					Cal->SetTimeScale(0.f);
				}
				else
				{
					Cal->SetTimeScale(GFrozenScale);
				}
			}
		},
		[](UWorld* W) { const UGameCalendar* Cal = CalOf(W); return Cal && Cal->TimeScale <= 0.f; });
	FAutoAction GSlow(TEXT("time.slowmo"), TEXT("time"), TEXT("dev.action.time.slowmo"),
		[](UWorld* W) { UGameplayStatics::SetGlobalTimeDilation(W, UGameplayStatics::GetGlobalTimeDilation(W) < 0.9f ? 1.f : 0.25f); },
		[](UWorld* W) { return UGameplayStatics::GetGlobalTimeDilation(W) < 0.9f; });

	FAutoAction GMoney(TEXT("pocket.money"), TEXT("pocket"), TEXT("dev.action.pocket.money"),
		[](UWorld* W) { if (ULyudInventory* I = ULyudInventory::Get(W)) I->Earn(1000); });
	FAutoAction GFeed(TEXT("pocket.feed"), TEXT("pocket"), TEXT("dev.action.pocket.feed"),
		[](UWorld* W) { Exec(W, TEXT("Tame.Give 20")); });
	FAutoAction GAll(TEXT("pocket.all"), TEXT("pocket"), TEXT("dev.action.pocket.all"),
		[](UWorld* W)
		{
			if (ULyudInventory* I = ULyudInventory::Get(W))
			{
				for (const FLyudItemDef& D : ULyudInventory::AllDefs())
				{
					if (I->Count(D.Id) == 0)
					{
						I->Add(D.Id, 1);
					}
				}
			}
		});
	FAutoAction GNewPocket(TEXT("pocket.reset"), TEXT("pocket"), TEXT("dev.action.pocket.reset"),
		[](UWorld* W) { if (ULyudInventory* I = ULyudInventory::Get(W)) I->Reset(); });

	FAutoAction GMapAll(TEXT("quests.map_all"), TEXT("quests"), TEXT("dev.action.quests.map_all"),
		[](UWorld* W)
		{
			if (ULyudQuests* Q = ULyudQuests::Get(W))
			{
				for (const FLyudMapPlace& P : ULyudQuests::Places())
				{
					Q->Found.AddUnique(P.Id);
				}
				Q->OnChanged.Broadcast();
			}
		});
	FAutoAction GQuestsReset(TEXT("quests.reset"), TEXT("quests"), TEXT("dev.action.quests.reset"),
		[](UWorld* W) { Exec(W, TEXT("Quest.Reset")); });
	FAutoAction GReload(TEXT("quests.reload"), TEXT("quests"), TEXT("dev.action.quests.reload"),
		[](UWorld*) { Reload(); });

	FAutoAction GSave(TEXT("game.save"), TEXT("game"), TEXT("dev.action.game.save"), [](UWorld* W) { Exec(W, TEXT("lyud.Save Dev")); });
	FAutoAction GLoad(TEXT("game.load"), TEXT("game"), TEXT("dev.action.game.load"), [](UWorld* W) { Exec(W, TEXT("lyud.Load 0")); });
	FAutoAction GStatsAction(TEXT("game.stats"), TEXT("game"), TEXT("dev.action.game.stats"),
		[](UWorld* W) { GStats = !GStats; Exec(W, TEXT("stat fps")); Exec(W, TEXT("stat unit")); }, [](UWorld*) { return GStats; });
	FAutoAction GOverlayAction(TEXT("game.overlay"), TEXT("game"), TEXT("dev.action.game.overlay"),
		[](UWorld* W) { SetOverlay(W, !IsOverlay(W)); }, [](UWorld* W) { return IsOverlay(W); });
	FAutoAction GBreath(TEXT("game.breath"), TEXT("game"), TEXT("dev.action.game.breath"), [](UWorld* W) { Exec(W, TEXT("lyud.Swim.Free")); });

	FAutoAction GHatch(TEXT("europa.hatch"), TEXT("europa"), TEXT("dev.action.europa.hatch"), [](UWorld* W) { Exec(W, TEXT("lyud.Europa.GoHatch")); });
	FAutoAction GEuropa(TEXT("europa.go"), TEXT("europa"), TEXT("dev.action.europa.go"), [](UWorld* W) { Exec(W, TEXT("lyud.Europa.GoEuropa")); });

	// ---- console: everything the panel does, for tests and for typing ------------------------------------------------

	FAutoConsoleCommandWithWorldAndArgs GTpCmd(TEXT("Dev.Tp"), TEXT("Dev.Tp <id>: to a teleport point (Dev.TpList lists them; map.json places, Content/Dev/teleports/*.json, code, bookmarks)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0)
			{
				TeleportTo(World, *Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GTpAtCmd(TEXT("Dev.TpAt"), TEXT("Dev.TpAt <x> <y> [z] [yaw]: to a world point (cm); without z onto the ground. Dev.TpMap <x> <y> [yaw] for map metres."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() >= 2)
			{
				const bool bZ = Args.Num() >= 3;
				TeleportWorld(World, FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), bZ ? FCString::Atod(*Args[2]) : 0.0),
					Args.Num() >= 4 ? FCString::Atof(*Args[3]) : 0.f, !bZ);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GTpMapCmd(TEXT("Dev.TpMap"), TEXT("Dev.TpMap <x> <y> [yaw]: to a map point (metres, docs/world_map.md frame), onto the ground"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() >= 2)
			{
				const FVector W = ULyudQuests::Frame().ToWorld(FVector2D(FCString::Atod(*Args[0]), FCString::Atod(*Args[1])));
				TeleportWorld(World, W, Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 0.f, true);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GTpListCmd(TEXT("Dev.TpList"), TEXT("Dev.TpList [group]: logs the teleport points (DEVTP lines)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			for (const FTeleport& T : Teleports())
			{
				if (Args.Num() == 0 || T.Group == FName(*Args[0]))
				{
					UE_LOG(LogTemp, Warning, TEXT("DEVTP %s group %s from %s map %.1f %.1f%s"), *T.Id.ToString(), *T.Group.ToString(), *T.Source, T.Map.X, T.Map.Y,
						T.Where ? TEXT(" (moving)") : TEXT(""));
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GDoCmd(TEXT("Dev.Do"), TEXT("Dev.Do <action id>: runs a developer action (Dev.List lists them)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0)
			{
				Do(World, *Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GListCmd(TEXT("Dev.List"), TEXT("Dev.List: logs the developer actions (DEVACTION lines)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			for (const FAction* A : Actions())
			{
				UE_LOG(LogTemp, Warning, TEXT("DEVACTION %s group %s%s"), *A->Id.ToString(), *A->Group.ToString(),
					A->IsOn ? (A->IsOn(World) ? TEXT(" on") : TEXT(" off")) : TEXT(""));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GQuestCmd(TEXT("Dev.Quest"), TEXT("Dev.Quest <quest> <stage id or index> | <quest> done <objective index> | <quest> play | <quest> forget: jump to a stage, complete an objective, play to the end, forget"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				return;
			}
			const FName Quest(*Args[0]);
			if (Args[1] == TEXT("forget"))
			{
				if (ULyudQuests* Q = ULyudQuests::Get(World))
				{
					Q->DevForget(Quest);
				}
				return;
			}
			if (Args[1] == TEXT("play"))
			{
				PlayThrough(World, Quest);
				return;
			}
			if (Args[1] == TEXT("done"))
			{
				CompleteObjective(World, Quest, Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0);
				return;
			}
			const FLyudQuestDef* Def = ULyudQuests::FindDef(Quest);
			if (!Def)
			{
				return;
			}
			int32 Stage = Def->Stages.IndexOfByPredicate([&Args](const FLyudQuestStage& S) { return S.Id == FName(*Args[1]); });
			if (Stage == INDEX_NONE && Args[1].IsNumeric())
			{
				Stage = FCString::Atoi(*Args[1]);
			}
			JumpToStage(World, Quest, Stage);
		}));

	FAutoConsoleCommandWithWorldAndArgs GWhereCmd(TEXT("Dev.Where"), TEXT("Dev.Where: logs (and copies) the player's place as a Dev.TpAt line with map metres"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { Where(World, true); }));

	FAutoConsoleCommandWithWorldAndArgs GBookmarkCmd(TEXT("Dev.Bookmark"), TEXT("Dev.Bookmark [name] | remove <name>: saves the player's place as a teleport point (Saved/Dev/bookmarks.json)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() >= 2 && Args[0] == TEXT("remove"))
			{
				RemoveBookmark(*Args[1]);
			}
			else
			{
				AddBookmark(World, Args.Num() > 0 ? Args[0] : FString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReloadCmd(TEXT("Dev.Reload"), TEXT("Dev.Reload: re-reads Content/Dev/teleports/*.json, quests.json and map.json"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*) { Reload(); }));
}
