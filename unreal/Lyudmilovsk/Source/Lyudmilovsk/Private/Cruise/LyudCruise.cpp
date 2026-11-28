#include "Cruise/LyudCruise.h"

#include "CruiseModel.h"
#include "City/LyudCityThings.h"
#include "City/SLyudCityPage.h"
#include "Dev/LyudDev.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSwim.h"
#include "UI/LyudQuests.h"
#include "World/LyudCityBuilding.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <string>
#include <vector>

DEFINE_LOG_CATEGORY_STATIC(LogLyudCruise, Log, All);

static_assert((int)ELyudCruisePhase::Laid == (int)LyudCruise::EPhase::Laid, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Port == (int)LyudCruise::EPhase::Port, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Boarding == (int)LyudCruise::EPhase::Boarding, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Underway == (int)LyudCruise::EPhase::Underway, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Circling == (int)LyudCruise::EPhase::Circling, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Anchored == (int)LyudCruise::EPhase::Anchored, "ELyudCruisePhase follows LyudCruise::EPhase");
static_assert((int)ELyudCruisePhase::Landing == (int)LyudCruise::EPhase::Landing, "ELyudCruisePhase follows LyudCruise::EPhase");

static TAutoConsoleVariable<int32> CVarCruise(TEXT("lyud.Cruise"), 1,
	TEXT("The motor ship's cruise (ULyudCruise): 1 on in Lvl_Scene01, 0 off; 2 also in any other game level (tests). Read at the level's start."));
static TAutoConsoleVariable<float> CVarCruiseClock(TEXT("lyud.Cruise.Clock"), 0.f,
	TEXT("Seconds added to the cruise's clock (tests: see the voyage without moving the calendar; the people and the sky do not follow)."));

static FAutoConsoleCommandWithWorld CmdCruiseReport(TEXT("lyud.Cruise.Report"), TEXT("Where the motor ship is, what the programme says, the hero's ticket."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudCruise* C = ULyudCruise::Get(World))
		{
			UE_LOG(LogLyudCruise, Warning, TEXT("%s"), *C->Report());
		}
	}));
static FAutoConsoleCommandWithWorld CmdCruiseBoard(TEXT("lyud.Cruise.Board"), TEXT("A ticket, the clock at the next boarding's Friday 17:30, the hero at the pier's launch gate."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudCruise* C = ULyudCruise::Get(World))
		{
			C->DevBoardNow();
		}
	}));
static FAutoConsoleCommandWithWorld CmdCruiseLanding(TEXT("lyud.Cruise.Landing"), TEXT("The landing at the Far Light: the clock at Saturday 16:40 of the voyage, the hero on the rock's jetty."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudCruise* C = ULyudCruise::Get(World))
		{
			C->DevLanding();
		}
	}));
static FAutoConsoleCommandWithWorld CmdCruiseNext(TEXT("lyud.Cruise.Next"), TEXT("The calendar on to five seconds before the next event of the programme."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ULyudCruise* C = ULyudCruise::Get(World);
		if (C && !C->DevNextEvent())
		{
			UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE no next event"));
		}
	}));
static FAutoConsoleCommandWithWorldAndArgs CmdCruiseJump(TEXT("lyud.Cruise.Jump"),
	TEXT("lyud.Cruise.Jump <event id>: the calendar to five seconds before that event (of this voyage, else of the next one)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCruise* C = ULyudCruise::Get(World);
		if (C && (Args.Num() < 1 || !C->DevJumpTo(FName(*Args[0]))))
		{
			UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE jump: no such event (lyud.Cruise.Report lists them)"));
		}
	}));
static FAutoConsoleCommandWithWorld CmdCruiseAshore(TEXT("lyud.Cruise.Ashore"), TEXT("Puts the hero ashore as at the voyage's end."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudCruise* C = ULyudCruise::Get(World))
		{
			C->DevAshore();
		}
	}));
static FAutoConsoleCommandWithWorld CmdCruiseBridgePass(TEXT("lyud.Cruise.BridgePass"), TEXT("Opens the wheelhouse to the hero for today (the stay cruise_bridge)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudCruise* C = ULyudCruise::Get(World))
		{
			C->GiveBridgePass();
		}
	}));

// developer mode (docs/dev_mode.md): the group "cruise"; the teleports are Content/Dev/teleports/ship.json (tools/ship/cruise.py)
static LyudDev::FAutoAction GDevCruiseBoard(TEXT("cruise.board_now"), TEXT("cruise"), TEXT("dev.action.cruise.board_now"),
	[](UWorld* W) { if (ULyudCruise* C = ULyudCruise::Get(W)) { C->DevBoardNow(); } });
static LyudDev::FAutoAction GDevCruiseLanding(TEXT("cruise.landing_now"), TEXT("cruise"), TEXT("dev.action.cruise.landing_now"),
	[](UWorld* W) { if (ULyudCruise* C = ULyudCruise::Get(W)) { C->DevLanding(); } });
static LyudDev::FAutoAction GDevCruiseNext(TEXT("cruise.next_event"), TEXT("cruise"), TEXT("dev.action.cruise.next_event"),
	[](UWorld* W) { if (ULyudCruise* C = ULyudCruise::Get(W)) { C->DevNextEvent(); } });
static LyudDev::FAutoAction GDevCruiseAshore(TEXT("cruise.ashore"), TEXT("cruise"), TEXT("dev.action.cruise.ashore"),
	[](UWorld* W) { if (ULyudCruise* C = ULyudCruise::Get(W)) { C->DevAshore(); } });
static LyudDev::FAutoAction GDevCruiseReport(TEXT("cruise.report"), TEXT("cruise"), TEXT("dev.action.cruise.report"),
	[](UWorld* W) { if (const ULyudCruise* C = ULyudCruise::Get(W)) { UE_LOG(LogLyudCruise, Warning, TEXT("%s"), *C->Report()); } });
static LyudDev::FAutoAction GDevCruiseCabin(TEXT("cruise.my_cabin"), TEXT("cruise"), TEXT("dev.action.cruise.my_cabin"),
	[](UWorld* W)
	{
		const ULyudCruise* C = ULyudCruise::Get(W);
		ULyudCityThings* Things = ULyudCityThings::Get(W);
		if (C && Things)
		{
			// his cabin's door; the standard cabin's when he is not aboard
			const int32 Cabin = C->HeroCabin() > 0 ? C->HeroCabin() : 7;
			Things->GoToThing(FString::Printf(TEXT("P-SHIP/cabin%d_door"), Cabin));
		}
	});
static LyudDev::FAutoAction GDevCruiseBox(TEXT("cruise.ticket_box"), TEXT("cruise"), TEXT("dev.action.cruise.ticket_box"),
	[](UWorld* W) { if (ULyudCityThings* Things = ULyudCityThings::Get(W)) { Things->GoToThing(TEXT("P-PIER/ship_ticket_box")); } });
static LyudDev::FAutoAction GDevCruiseTicketHold(TEXT("cruise.ticket_hold"), TEXT("cruise"), TEXT("dev.action.cruise.ticket_hold"),
	[](UWorld* W) { if (ULyudInventory* Inv = ULyudInventory::Get(W)) { Inv->Add(TEXT("cruise_ticket_hold"), 1); } });
static LyudDev::FAutoAction GDevCruiseTicketLux(TEXT("cruise.ticket_lux"), TEXT("cruise"), TEXT("dev.action.cruise.ticket_lux"),
	[](UWorld* W) { if (ULyudInventory* Inv = ULyudInventory::Get(W)) { Inv->Add(TEXT("cruise_ticket_lux"), 1); } });

namespace LyudCruiseImpl
{
	const FName SaveKey(TEXT("Cruise"));
	const TCHAR* const LaunchGate = TEXT("P-PIER/launch_gate");
	/** Where a man fished out of the sea is set down: the aft mooring deck. */
	const TCHAR* const AftThing = TEXT("P-SHIP/capstan");
	/** The landing at the Far Light: the launch's two ends, and how far round the rock's chart point is "on the rock" (cm;
	 *  the rock is 130 m at its longest from that point, the ship's side is 326 m from it). */
	const TCHAR* const GangwayThing = TEXT("P-SHIP/gangway");
	const TCHAR* const JettyThing = TEXT("P-FARLIGHT/jetty_gate");
	const FName RockPlot(TEXT("P-FARLIGHT"));
	constexpr double RockCm = 17000.0;
	/** The real rock is shown while the seascape's copy would stand within this of it (m), or a hero who is no passenger
	 *  is within RockVisitCm of it (a developer's visit). */
	constexpr double RockMatchM = 1.0;
	constexpr double RockVisitCm = 120000.0;
	const TCHAR* const ClassNames[3] = { TEXT("hold"), TEXT("std"), TEXT("lux") };
	/** Seconds between the looks at the hero and the flags. */
	constexpr float PollEvery = 0.25f;
	/** All flags are sent again this often (the city's model is replaced when its data comes). */
	constexpr float ResendEvery = 2.f;
	/** A clock step of this much or less is the voyage going on (a sleep, an act's minutes): every event passed is fired. */
	constexpr double FlowSec = 3.0 * 3600.0 + 120.0;
	/** An event that began this long ago or less is still announced. */
	constexpr double AnnounceSec = 900.0;
	/** Round the stage this far (cm) is "by the ship": the stage is far out at sea, nothing else is there. */
	constexpr double NearCm = 40000.0;

	static FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	static bool ReadVector(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Field, A) || A->Num() < 3)
		{
			return false;
		}
		Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return true;
	}

	static const TCHAR* PhaseName(ELyudCruisePhase P)
	{
		switch (P)
		{
		case ELyudCruisePhase::Laid: return TEXT("laid up");
		case ELyudCruisePhase::Port: return TEXT("on the roads");
		case ELyudCruisePhase::Boarding: return TEXT("boarding");
		case ELyudCruisePhase::Underway: return TEXT("under way");
		case ELyudCruisePhase::Circling: return TEXT("circling");
		case ELyudCruisePhase::Anchored: return TEXT("at anchor");
		default: return TEXT("landing");
		}
	}
}

struct ULyudCruise::FWork
{
	LyudCruise::FModel Model;

	// names made once (no FName or string is built while the cruise runs)
	TArray<FName> FlagNames;
	TArray<FName> EventIds;
	TArray<FString> EventTexts;
	TArray<FString> AnnounceKeys;
	TArray<FName> TicketItems;

	// the programme as last seen
	bool bSeenOn = false;
	long long SeenVoyage = -1;
	double SeenTv = 0.0;
	uint64 OnBits = 0;

	// the flags as last sent
	std::vector<uint8_t> Bits;
	std::vector<uint8_t> Sent;
	float ResendClock = 0.f;
	int32 SentWait = -1;
	bool bHasTicket = false;
	int32 ThingsSeen = -1;

	// the hero by the ship
	float GraceFor = 0.f;
	float OverFor = 0.f;
	float AwayFor = 0.f;
	float AtShipFor = 0.f;
	bool bArrivalSeen = false;

	// the landing at the Far Light: the rock's building (found lazily), whether it is shown, the bosun's warnings said
	TWeakObjectPtr<ALyudCityBuilding> Rock;
	bool bRockApplied = false;
	bool bRockShown = false;
	double RockMismatchM = 1e9;
	float RockSwimFor = 0.f;
	int32 WarnSaid = 0;

	// announcements waiting their turn
	struct FLine
	{
		FText Speaker;
		FText Line;
		float Seconds = 7.f;
	};
	TArray<FLine> Queue;

	// a move under way: 0 none, 1 ashore, 2 out of the sea
	int32 MoveKind = 0;
	/** 0 waiting for the hero to be free, 1 the city's lift has him, 2 our own fade. */
	int32 MoveStage = 0;
	int32 MoveTries = 0;
	FString MoveThing;
	FVector MoveFallback = FVector::ZeroVector;
	float MoveYaw = 0.f;
	bool bMoveGround = false;
	FString MoveSpeaker;
	FString MoveLine;
	double MoveSince = 0.0;
	double MoveNext = 0.0;

	FDelegateHandle LoadHandle;
};

ULyudCruise::ULyudCruise() = default;
ULyudCruise::~ULyudCruise() = default;

ULyudCruise* ULyudCruise::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCruise>() : nullptr;
}

bool ULyudCruise::IsHeroAboard(const UObject* WorldContext)
{
	const ULyudCruise* C = Get(WorldContext);
	return C && C->IsHeroAboard();
}

bool ULyudCruise::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudCruise::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudCruise, STATGROUP_Tickables);
}

void ULyudCruise::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const int32 Mode = CVarCruise.GetValueOnGameThread();
	const bool bScene = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01"));
	if (Mode == 0 || (!bScene && Mode < 2))
	{
		return;
	}
	bStarted = Start();
	if (!bStarted)
	{
		Work.Reset();
		return;
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(LyudCruiseImpl::SaveKey, this);
		Work->LoadHandle = Saves->OnAfterLoad.AddUObject(this, &ULyudCruise::OnSaveLoaded);
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(&InWorld))
	{
		Things->AddActHook(ULyudCityThings::FActHook::CreateUObject(this, &ULyudCruise::OnCityAct));
	}
	Step(NowSeconds());
}

void ULyudCruise::Deinitialize()
{
	if (bStarted)
	{
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
		{
			Saves->UnregisterSaveObject(LyudCruiseImpl::SaveKey);
			if (Work.IsValid())
			{
				Saves->OnAfterLoad.Remove(Work->LoadHandle);
			}
		}
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->RemoveActHooks(this);
		}
	}
	Unsay();
	bStarted = false;
	Super::Deinitialize();
}

bool ULyudCruise::Start()
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Cruise/cruise.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE off: no %s (python3 tools/ship/cruise.py)"), *Path);
		return false;
	}
	Work = MakePimpl<FWork>();
	LyudCruise::FModel& Model = Work->Model;
	if (!Model.Load(std::string(TCHAR_TO_UTF8(*Text))))
	{
		UE_LOG(LogLyudCruise, Error, TEXT("CRUISE off: %s"), *LyudCruiseImpl::Str(Model.LastError));
		return false;
	}
	if (Model.Events.size() > 64)
	{
		UE_LOG(LogLyudCruise, Error, TEXT("CRUISE off: %d events (64 at most)"), (int32)Model.Events.size());
		return false;
	}

	// what the model does not need: the stage, the roads, the named points, the hull's outline
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogLyudCruise, Error, TEXT("CRUISE off: cruise.json does not parse"));
		return false;
	}
	const FLyudMapFrame& Frame = ULyudQuests::Frame();
	const TSharedPtr<FJsonObject>* StageObj = nullptr;
	FVector StageAt(2600000.0, 400790.0, -4300.0);
	double StageYaw = 0.0;
	if (Root->TryGetObjectField(TEXT("stage"), StageObj))
	{
		LyudCruiseImpl::ReadVector(*StageObj, TEXT("world_cm"), StageAt);
		(*StageObj)->TryGetNumberField(TEXT("yaw_deg"), StageYaw);
	}
	Stage = FTransform(FRotator(0.0, StageYaw, 0.0), StageAt);

	double SeaZ = -4300.0;
	const TSharedPtr<FJsonObject>* RoadsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("roads"), RoadsObj))
	{
		(*RoadsObj)->TryGetNumberField(TEXT("sea_z_cm"), SeaZ);
	}
	{
		// the route's beginning: where the ship lies between voyages (the stern at the point, the bow along the heading)
		LyudCruise::FVec2 At;
		double Heading = 0.0;
		Model.PathAt(0.0, At, Heading);
		const FVector A = Frame.ToWorld(FVector2D(At.X, At.Y), SeaZ);
		const FVector B = Frame.ToWorld(FVector2D(At.X + FMath::Cos(Heading) * 10.0, At.Y + FMath::Sin(Heading) * 10.0), SeaZ);
		Roads = FTransform(FRotator(0.0, (B - A).Rotation().Yaw, 0.0), A);
	}

	const TSharedPtr<FJsonObject>* HullObj = nullptr;
	if (Root->TryGetObjectField(TEXT("hull"), HullObj))
	{
		double Number = 0.0;
		if ((*HullObj)->TryGetNumberField(TEXT("length_cm"), Number))
		{
			HullLengthCm = (float)Number;
		}
		if ((*HullObj)->TryGetNumberField(TEXT("half_beam_cm"), Number))
		{
			HullHalfBeamCm = (float)Number;
		}
		const TArray<TSharedPtr<FJsonValue>>* Plan = nullptr;
		if ((*HullObj)->TryGetArrayField(TEXT("plan_cm"), Plan))
		{
			for (const TSharedPtr<FJsonValue>& V : *Plan)
			{
				const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
				if (V.IsValid() && V->TryGetArray(Pair) && Pair->Num() >= 2)
				{
					HullPlan.Add(FVector2D((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber()));
				}
			}
		}
	}

	const TSharedPtr<FJsonObject>* PointsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("points"), PointsObj))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*PointsObj)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
			if (P.Value.IsValid() && P.Value->TryGetArray(A) && A->Num() >= 3)
			{
				Points.Add(FName(*P.Key), Stage.TransformPosition(FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber())));
			}
		}
	}

	const TSharedPtr<FJsonObject>* PierObj = nullptr;
	FVector2D PierMap(311.3, 905.0);
	if (Root->TryGetObjectField(TEXT("pier_fallback"), PierObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if ((*PierObj)->TryGetArrayField(TEXT("map"), A) && A->Num() >= 2)
		{
			PierMap = FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
		}
		double Yaw = 180.0;
		if ((*PierObj)->TryGetNumberField(TEXT("yaw_deg"), Yaw))
		{
			PierFallbackYaw = (float)Yaw;
		}
	}
	PierFallback = Frame.ToWorld(PierMap, 0.0);
	// the landing's jetty (world cm, the capsule's middle; cruise.py computes it from the rock's export)
	JettyFallback = Stage.TransformPosition(FVector(HullLengthCm * 0.5, 26800.0, 480.0));
	const TSharedPtr<FJsonObject>* AshoreObj = nullptr;
	if (Root->TryGetObjectField(TEXT("ashore"), AshoreObj))
	{
		FVector Jetty = FVector::ZeroVector;
		if (LyudCruiseImpl::ReadVector(*AshoreObj, TEXT("jetty_world_cm"), Jetty))
		{
			JettyFallback = Jetty;
		}
		double JettyYaw = 0.0;
		if ((*AshoreObj)->TryGetNumberField(TEXT("jetty_yaw_deg"), JettyYaw))
		{
			JettyFallbackYaw = (float)JettyYaw;
		}
	}

	// the names
	for (const std::string& F : Model.FlagNames)
	{
		Work->FlagNames.Add(FName(*LyudCruiseImpl::Str(F)));
	}
	TSet<FName> JumpsMade;
	for (const LyudCruise::FEvent& E : Model.Events)
	{
		const FString Id = LyudCruiseImpl::Str(E.Id);
		const FName IdName(*Id);
		Work->EventIds.Add(IdName);
		Work->EventTexts.Add(Id);
		Work->AnnounceKeys.Add(LyudCruiseImpl::Str(E.Announce));
		if (!JumpsMade.Contains(IdName))
		{
			// a developer's jump to every event of the programme (the name keys come with the strings of tools/ship/cruise.py)
			JumpsMade.Add(IdName);
			LyudDev::FAction Jump;
			Jump.Id = FName(*(TEXT("cruise.jump_") + Id));
			Jump.Group = TEXT("cruise");
			Jump.Name = TEXT("dev.action.cruise.jump_") + Id;
			Jump.Run = [IdName](UWorld* W)
			{
				if (ULyudCruise* C = ULyudCruise::Get(W))
				{
					C->DevJumpTo(IdName);
				}
			};
			LyudDev::AddAction(Jump);
		}
	}
	for (const LyudCruise::FTicket& K : Model.Tickets)
	{
		Work->TicketItems.Add(FName(*LyudCruiseImpl::Str(K.Item)));
	}
	Work->Bits.assign(Model.FlagNames.size(), 0);
	Work->Sent.assign(Model.FlagNames.size(), 0);
	Work->Queue.Reserve(4);
	UE_LOG(LogLyudCruise, Log, TEXT("CRUISE on: %d events, %d islands, route %.0f km, stage %s yaw %.0f"), (int32)Model.Events.size(),
		(int32)Model.Islands.size(), Model.Length / 1000.0, *StageAt.ToCompactString(), StageYaw);
	return true;
}

// --- the clock

double ULyudCruise::NowSeconds() const
{
	const UWorld* World = GetWorld();
	const FDateTime Now = World ? UGameCalendar::NowIn(World) : FDateTime(2026, 6, 5, 16);
	return (Now - FDateTime(2026, 1, 1)).GetTotalSeconds() + CVarCruiseClock.GetValueOnGameThread();
}

void ULyudCruise::SetClock(double Seconds)
{
	UWorld* World = GetWorld();
	if (UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr)
	{
		Cal->SetNow(FDateTime(2026, 1, 1) + FTimespan::FromSeconds(Seconds - CVarCruiseClock.GetValueOnGameThread()));
	}
}

void ULyudCruise::Tick(float DeltaTime)
{
	if (!bStarted || !Work.IsValid())
	{
		return;
	}
	Step(NowSeconds());
	TickMove();
	TickSubtitle();
	PollClock += DeltaTime;
	if (PollClock >= LyudCruiseImpl::PollEvery)
	{
		const float Passed = PollClock;
		PollClock = 0.f;
		Poll(Passed);
	}
}

void ULyudCruise::Step(double Now)
{
	const LyudCruise::FModel& Model = Work->Model;
	const LyudCruise::FState St = Model.At(Now);
	Seen.Phase = (ELyudCruisePhase)St.Phase;
	Seen.bHeroAboard = bAboard;
	Seen.Day = St.Day;
	Seen.ChartPosM = FVector2D(St.Pos.X, St.Pos.Y);
	Seen.HeadingRad = (float)St.Heading;
	Seen.SpeedMS = (float)St.Speed;
	Seen.TurnRateRad = (float)St.TurnRate;
	Seen.VoyageSeconds = St.Tv;
	Work->RockMismatchM = Model.AshoreMismatch(St);

	const bool bOn = St.Phase != LyudCruise::EPhase::Laid && St.Phase != LyudCruise::EPhase::Port;
	uint64 Bits = 0;
	if (bOn)
	{
		// the voyage going on (a frame, a sleep, an act's minutes): every event whose beginning was passed, in its order;
		// anything else (the level's start, a load, a long jump): the events on now
		const bool bFlow = Work->bSeenOn && Work->SeenVoyage == St.Voyage && St.Tv >= Work->SeenTv && St.Tv - Work->SeenTv <= LyudCruiseImpl::FlowSec;
		const int32 Num = (int32)Model.Events.size();
		for (int32 I = 0; I < Num; ++I)
		{
			const LyudCruise::FEvent& E = Model.Events[(size_t)I];
			const bool bNow = St.Tv >= E.T0 && St.Tv < E.T1;
			if (bNow)
			{
				Bits |= 1ull << I;
			}
			if (bFlow ? (E.T0 > Work->SeenTv && E.T0 <= St.Tv) : bNow)
			{
				FireEvent(I, St.Tv - E.T0 < LyudCruiseImpl::AnnounceSec);
			}
		}
	}
	Work->OnBits = Bits;
	Work->bSeenOn = bOn;
	Work->SeenVoyage = St.Voyage;
	Work->SeenTv = St.Tv;
}

void ULyudCruise::FireEvent(int32 Index, bool bAnnounce)
{
	const FName Id = Work->EventIds[Index];
	UE_LOG(LogLyudCruise, Log, TEXT("CRUISE event %s%s"), *Work->EventTexts[Index], bAboard ? TEXT(" (hero aboard)") : TEXT(""));
	OnCruiseEvent.Broadcast(Id);
	if (!bAboard)
	{
		return;
	}
	// the quests follow the voyage by this (W9's ship_voyage: cruise_event aboard | <event id> | ashore)
	ULyudQuests::Notify(this, TEXT("cruise_event"), Work->EventTexts[Index]);
	if (Work->EventTexts[Index] == TEXT("bridge_tour"))
	{
		GiveBridgePass();
	}
	if (bAnnounce && !Work->AnnounceKeys[Index].IsEmpty())
	{
		// the ship's radio; when several come at once (a jump) the last two are heard
		if (Work->Queue.Num() >= 2)
		{
			Work->Queue.RemoveAt(0, 1, EAllowShrinking::No);
		}
		FWork::FLine& L = Work->Queue.AddDefaulted_GetRef();
		L.Speaker = LyudText::Get(TEXT("cruise.speaker.radio"));
		L.Line = LyudText::Get(Work->AnnounceKeys[Index]);
		L.Seconds = FMath::Clamp(L.Line.ToString().Len() * 0.07f, 6.f, 14.f);
	}
}

// --- what others ask

bool ULyudCruise::IsEventOn(FName EventId) const
{
	if (!bStarted || !Work.IsValid() || Work->OnBits == 0)
	{
		return false;
	}
	for (int32 I = 0; I < Work->EventIds.Num(); ++I)
	{
		if ((Work->OnBits >> I & 1ull) && Work->EventIds[I] == EventId)
		{
			return true;
		}
	}
	return false;
}

double ULyudCruise::SecondsToNextEvent(FName* OutId) const
{
	if (!bStarted || !Work.IsValid())
	{
		return -1.0;
	}
	double Seconds = -1.0;
	const int Index = Work->Model.NextEvent(NowSeconds(), Seconds);
	if (Index < 0)
	{
		return -1.0;
	}
	if (OutId)
	{
		*OutId = Work->EventIds[Index];
	}
	return Seconds;
}

bool ULyudCruise::GetShipPoint(FName Name, FVector& OutWorld) const
{
	if (const FVector* P = Points.Find(Name))
	{
		OutWorld = *P;
		return true;
	}
	if (const FVector* P = ThingPoints.Find(Name))
	{
		OutWorld = *P;
		return true;
	}
	return false;
}

FName ULyudCruise::HeroClass() const
{
	static const FName Names[3] = { FName(TEXT("hold")), FName(TEXT("std")), FName(TEXT("lux")) };
	return bAboard ? Names[FMath::Clamp(HeroClassIndex, 0, 2)] : FName();
}

bool ULyudCruise::IsInsideHull(const FVector& World, float MarginCm) const
{
	const FVector L = ToShipFrame(World);
	if (L.X < -MarginCm || L.X > HullLengthCm + MarginCm)
	{
		return false;
	}
	double Half = HullHalfBeamCm;
	if (HullPlan.Num() >= 2)
	{
		// the outline: half the beam at every station, straight between them
		const double X = FMath::Clamp(L.X, HullPlan[0].X, HullPlan.Last().X);
		Half = HullPlan.Last().Y;
		for (int32 I = 1; I < HullPlan.Num(); ++I)
		{
			if (X <= HullPlan[I].X)
			{
				const double Span = FMath::Max(1.0, HullPlan[I].X - HullPlan[I - 1].X);
				Half = FMath::Lerp(HullPlan[I - 1].Y, HullPlan[I].Y, (X - HullPlan[I - 1].X) / Span);
				break;
			}
		}
	}
	return FMath::Abs(L.Y) <= Half + MarginCm;
}

LyudCruise::FHero ULyudCruise::HeroState() const
{
	LyudCruise::FHero H;
	H.bAboard = bAboard;
	H.Class = HeroClassIndex;
	H.Cabin = HeroCabinNo;
	H.Voyage = HeroVoyage;
	H.Cruises = CruisesSailed;
	return H;
}

void ULyudCruise::TakeHeroState(const LyudCruise::FHero& H)
{
	bAboard = H.bAboard;
	HeroClassIndex = H.Class;
	HeroCabinNo = H.Cabin;
	HeroVoyage = H.Voyage;
	CruisesSailed = H.Cruises;
	Seen.bHeroAboard = bAboard;
}

FString ULyudCruise::Report() const
{
	if (!bStarted || !Work.IsValid())
	{
		return TEXT("CRUISE off (lyud.Cruise, Content/Cruise/cruise.json)");
	}
	const double Now = NowSeconds();
	const LyudCruise::FDate D = LyudCruise::DateOf(Now);
	const double Clock = FMath::Fmod(Now, 86400.0);
	FString Out = FString::Printf(TEXT("CRUISE %04d-%02d-%02d %02d:%02d: %s, day %d, tv %.0f s, chart %.0f %.0f m, heading %.0f deg, %.2f m/s, turn %.4f rad/s\n"),
		D.Year, D.Month, D.Day, (int32)(Clock / 3600.0), (int32)(Clock / 60.0) % 60, LyudCruiseImpl::PhaseName(Seen.Phase), Seen.Day, Seen.VoyageSeconds,
		Seen.ChartPosM.X, Seen.ChartPosM.Y, FMath::RadiansToDegrees(Seen.HeadingRad), Seen.SpeedMS, Seen.TurnRateRad);
	Out += TEXT("  on now:");
	for (int32 I = 0; I < Work->EventIds.Num(); ++I)
	{
		if (Work->OnBits >> I & 1ull)
		{
			Out += TEXT(" ") + Work->EventTexts[I];
		}
	}
	FName Next;
	const double ToNext = SecondsToNextEvent(&Next);
	Out += ToNext >= 0.0 ? FString::Printf(TEXT("\n  next: %s in %.0f min\n"), *Next.ToString(), ToNext / 60.0) : FString(TEXT("\n  next: none\n"));
	Out += bAboard
		? FString::Printf(TEXT("  hero ABOARD: class %s, cabin %d, voyage %lld; cruises sailed %d\n"), LyudCruiseImpl::ClassNames[FMath::Clamp(HeroClassIndex, 0, 2)], HeroCabinNo, (long long)HeroVoyage, CruisesSailed)
		: FString::Printf(TEXT("  hero ashore%s; cruises sailed %d\n"), Work->bHasTicket ? TEXT(", a ticket in the pocket") : TEXT(""), CruisesSailed);
	Out += TEXT("  flags on:");
	for (int32 I = 0; I < Work->FlagNames.Num(); ++I)
	{
		if (Work->Sent[(size_t)I])
		{
			Out += TEXT(" ") + Work->FlagNames[I].ToString();
		}
	}
	Out += FString::Printf(TEXT("\n  stage %s, roads %s yaw %.0f, %d points, %d things of P-SHIP\n  events:"), *Stage.GetLocation().ToCompactString(),
		*Roads.GetLocation().ToCompactString(), Roads.Rotator().Yaw, Points.Num(), ThingPoints.Num());
	for (const FString& E : Work->EventTexts)
	{
		Out += TEXT(" ") + E;
	}
	return Out;
}

// --- a few times a second

void ULyudCruise::Poll(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = NowSeconds();
	const LyudCruise::FModel& Model = Work->Model;
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	bool bTicket = false;
	if (Inv)
	{
		for (const FName& Item : Work->TicketItems)
		{
			bTicket = bTicket || Inv->Count(Item) > 0;
		}
	}
	Work->bHasTicket = bTicket;

	// the things of the ship by their ids, once the city's data has come (and again when it is replaced)
	if (Things && Things->NumThings() != Work->ThingsSeen)
	{
		Work->ThingsSeen = Things->NumThings();
		ThingPoints.Reset();
		TArray<FCityThingRef> Refs;
		Things->GetThingsOfPlot(TEXT("P-SHIP"), FName(), Refs);
		for (const FCityThingRef& R : Refs)
		{
			int32 Slash = INDEX_NONE;
			ThingPoints.Add(FName(R.Id.FindLastChar(TEXT('/'), Slash) ? *R.Id.Mid(Slash + 1) : *R.Id), R.Location);
		}
		Work->ResendClock = LyudCruiseImpl::ResendEvery;
	}

	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	Work->GraceFor += DeltaTime;
	// the first seconds of a level or a load are left alone: the hero may not be where the save puts him yet
	if (Hero && Work->MoveKind == 0 && Work->GraceFor > 5.f)
	{
		const FVector At = Hero->GetActorLocation();
		const double FeetZ = At.Z - (Hero->GetCapsuleComponent() ? Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f);
		const FVector L = ToShipFrame(FVector(At.X, At.Y, FeetZ));
		const bool bNear = FVector::DistSquared2D(At, Stage.TransformPosition(FVector(HullLengthCm * 0.5f, 0.f, 0.f))) < FMath::Square(LyudCruiseImpl::NearCm);
		const bool bAtShip = bNear && L.X > -300.0 && L.X < HullLengthCm + 300.0 && FMath::Abs(L.Y) < HullHalfBeamCm + 300.0 && L.Z > -200.0 && L.Z < 1800.0;
		const ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this);
		const bool bSwims = Swim && Swim->IsSwimming();
		const bool bInSea = bNear && (L.Z < -30.0 || (bSwims && !IsInsideHull(At, 0.f)));
		Work->OverFor = bInSea ? Work->OverFor + DeltaTime : 0.f;
		Work->AwayFor = bAboard && !bNear ? Work->AwayFor + DeltaTime : 0.f;
		Work->AtShipFor = bAtShip ? Work->AtShipFor + DeltaTime : 0.f;
		if (!bAtShip)
		{
			Work->bArrivalSeen = false;
		}
		const LyudCruise::FHero H = HeroState();
		// the landing at the Far Light: on the rock he is neither overboard nor gone ashore
		const bool bOnRock = Model.AshoreStop >= 0 && FVector::DistSquared2D(At, RockWorld()) < FMath::Square(LyudCruiseImpl::RockCm) && !bAtShip;
		bHeroOnRock = bOnRock;
		const int32 Due = bAboard && bOnRock && Work->SeenVoyage == H.Voyage ? Model.AshoreWarnings(Work->SeenTv) : 0;
		if (Due > Work->WarnSaid)
		{
			static const TCHAR* const WarnKeys[3] = { TEXT("cruise.say.ashore_warn.1"), TEXT("cruise.say.ashore_warn.2"), TEXT("cruise.say.ashore_warn.3") };
			Say(LyudText::Get(TEXT("cruise.speaker.bosun")), LyudText::Get(WarnKeys[FMath::Clamp(Due, 1, 3) - 1]), 8.f);
		}
		Work->WarnSaid = Due;
		Work->RockSwimFor = bOnRock && bSwims ? Work->RockSwimFor + DeltaTime : 0.f;
		if (bOnRock && !(bAboard && Model.MustGoAshore(Now, H)))
		{
			Work->OverFor = 0.f;
			Work->AwayFor = 0.f;
			if (bAboard && !Model.MayBeAshore(Now, H))
			{
				// the last launch has left (18:20), or the clock moved while he stood there: the bosun fetches him
				FVector Gangway = Stage.TransformPosition(FVector(3140.0, 240.0, 380.0));
				GetShipPoint(TEXT("vestibule"), Gangway);
				BeginMove(3, LyudCruiseImpl::GangwayThing, Gangway, (float)Stage.Rotator().Yaw + 180.f, false, TEXT("cruise.speaker.bosun"), TEXT("cruise.say.ashore_fetch"));
				UE_LOG(LogLyudCruise, Log, TEXT("CRUISE fetched from the rock"));
			}
			else if (Work->RockSwimFor >= 2.5f)
			{
				// he swam off the rock: back onto the jetty
				Work->RockSwimFor = 0.f;
				BeginMove(4, LyudCruiseImpl::JettyThing, JettyFallback, JettyFallbackYaw, false, TEXT("cruise.speaker.bosun"), TEXT("cruise.say.ashore_swim"));
			}
		}
		else if (bAboard && Model.MustGoAshore(Now, H))
		{
			if (bNear)
			{
				// Sunday 20:00, or he slept through it, or the save is of another week: the last launch
				const bool bJustOver = Work->SeenVoyage == H.Voyage && Work->SeenTv < Model.TotalSec + Model.LandingSec + 3600.0;
				BeginMove(1, LyudCruiseImpl::LaunchGate, PierFallback, PierFallbackYaw, true, TEXT("cruise.speaker.bosun"), bJustOver ? TEXT("cruise.say.ashore") : TEXT("cruise.say.ashore_over"));
			}
			else
			{
				FinishAshore(Now);
			}
		}
		else if (Work->OverFor >= 2.f)
		{
			// man overboard: two seconds in the water and he is on the aft deck again, wet and told off
			Work->OverFor = 0.f;
			FVector Aft = Stage.TransformPosition(FVector(450.0, -150.0, 280.0));
			GetShipPoint(TEXT("aft_deck"), Aft);
			BeginMove(2, LyudCruiseImpl::AftThing, Aft, (float)Stage.Rotator().Yaw, false, TEXT("cruise.speaker.bosun"), TEXT("cruise.say.overboard"));
			UE_LOG(LogLyudCruise, Log, TEXT("CRUISE man overboard"));
			if (bAboard)
			{
				ULyudQuests::Notify(this, TEXT("cruise_event"), TEXT("overboard"));
			}
		}
		else if (bAboard && Work->AwayFor > 3.f)
		{
			// he left by the gangway (the landing; or before she sailed), or a developer took him away
			const bool bEarly = Seen.Phase == ELyudCruisePhase::Boarding && Work->SeenVoyage == H.Voyage;
			FinishAshore(Now);
			if (bEarly)
			{
				Say(LyudText::Get(TEXT("cruise.speaker.bosun")), LyudText::Get(TEXT("cruise.say.ashore_early")), 7.f);
			}
		}
		else if (!bAboard && bAtShip && Work->AtShipFor > 0.5f && !Work->bArrivalSeen)
		{
			// he came up the gangway (or a developer put him aboard): with a ticket he is a passenger from here
			Work->bArrivalSeen = true;
			const bool bVoyage = Seen.Phase == ELyudCruisePhase::Boarding || Seen.Phase == ELyudCruisePhase::Underway
				|| Seen.Phase == ELyudCruisePhase::Circling || Seen.Phase == ELyudCruisePhase::Anchored;
			if (bVoyage && bTicket)
			{
				Board(Now);
			}
			else if (bVoyage)
			{
				Say(LyudText::Get(TEXT("cruise.speaker.purser")), LyudText::Get(TEXT("cruise.say.stowaway")), 7.f);
			}
		}
	}

	if (!Hero)
	{
		bHeroOnRock = false;
	}
	Work->ResendClock += DeltaTime;
	const bool bAll = Work->ResendClock >= LyudCruiseImpl::ResendEvery;
	// the real rock or the seascape's copy (ALyudCruiseSeascape asks IsRealRockShown): the real one while the copy would stand
	// on it (the ship lies at the landing stop), or a hero who is no passenger is near it. Applied only when it changes.
	{
		const bool bVisit = Hero && !bAboard && FVector::DistSquared2D(Hero->GetActorLocation(), RockWorld()) < FMath::Square(LyudCruiseImpl::RockVisitCm);
		const bool bShow = Work->Model.AshoreStop >= 0 && (Work->RockMismatchM < LyudCruiseImpl::RockMatchM || bVisit || bHeroOnRock);
		if (!Work->Rock.IsValid() && (bAll || !Work->bRockApplied))
		{
			for (TActorIterator<ALyudCityBuilding> It(World); It; ++It)
			{
				if (It->PlotId == LyudCruiseImpl::RockPlot)
				{
					Work->Rock = *It;
					Work->bRockApplied = false;
					break;
				}
			}
		}
		if (ALyudCityBuilding* Rock = Work->Rock.Get())
		{
			if (!Work->bRockApplied || Work->bRockShown != bShow)
			{
				Rock->SetActorHiddenInGame(!bShow);
				Rock->SetActorEnableCollision(bShow);
				Work->bRockApplied = true;
				UE_LOG(LogLyudCruise, Log, TEXT("CRUISE the Far Light: the real rock %s (the copy is %.2f m off it)"), bShow ? TEXT("shown") : TEXT("hidden"), Work->RockMismatchM);
			}
		}
		Work->bRockShown = bShow;
	}
	if (bAll)
	{
		Work->ResendClock = 0.f;
		// the captain's invitation (W9's quest ship_captain, stage wheel) opens the wheelhouse too
		if (bAboard && Things && !Things->IsOnStay(TEXT("cruise_bridge")))
		{
			static const FName QuestCaptain(TEXT("ship_captain"));
			static const FName StageWheel(TEXT("wheel"));
			const ULyudQuests* Quests = ULyudQuests::Get(this);
			if (Quests && Quests->StageId(QuestCaptain) == StageWheel)
			{
				GiveBridgePass();
			}
		}
	}
	PushFlags(Now, bAll);
}

void ULyudCruise::PushFlags(double Now, bool bAll)
{
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (!Things)
	{
		return;
	}
	Work->Model.Flags(Now, HeroState(), Work->bHasTicket, Work->Bits);
	const int32 Num = Work->FlagNames.Num();
	for (int32 I = 0; I < Num; ++I)
	{
		const uint8_t On = Work->Bits[(size_t)I];
		if (bAll || On != Work->Sent[(size_t)I])
		{
			Things->SetFlag(Work->FlagNames[I], On != 0);
			Work->Sent[(size_t)I] = On;
		}
	}
	// the acts that last "until the next event" (deck chairs, berths): the minutes to it
	const int32 Wait = bAboard ? Work->Model.WaitMinutes(Now) : 0;
	if (bAll || Wait != Work->SentWait)
	{
		static const FName WaitName(TEXT("cruise_event"));
		Things->SetWaitMinutes(WaitName, Wait);
		Work->SentWait = Wait;
	}
	if (bAll)
	{
		// the people aboard (W10): their stays open with the season
		static const FName SeasonName(TEXT("cruise_season"));
		UWorld* World = GetWorld();
		if (UPeopleLifeSubsystem* Life = World ? World->GetSubsystem<UPeopleLifeSubsystem>() : nullptr)
		{
			Life->SetCondition(SeasonName, Work->Bits[0] != 0);
		}
	}
}

void ULyudCruise::Board(double Now)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const LyudCruise::FModel& Model = Work->Model;
	// the dearest ticket in the pocket is the one handed in
	const LyudCruise::FTicket* Ticket = nullptr;
	FName TicketItem;
	for (int32 I = 0; I < Work->TicketItems.Num(); ++I)
	{
		const LyudCruise::FTicket& K = Model.Tickets[(size_t)I];
		if (Inv && Inv->Count(Work->TicketItems[I]) > 0 && (!Ticket || K.ClassIndex > Ticket->ClassIndex))
		{
			Ticket = &K;
			TicketItem = Work->TicketItems[I];
		}
	}
	if (!Inv || !Ticket)
	{
		return;
	}
	Inv->Remove(TicketItem, 1);
	if (Inv->Count(TEXT("cabin_key")) == 0)
	{
		Inv->Add(TEXT("cabin_key"), 1);
	}
	LyudCruise::FHero H = HeroState();
	Model.Board(H, Now, *Ticket);
	TakeHeroState(H);
	Work->bHasTicket = false;
	Work->AwayFor = 0.f;
	const int32 ClassIndex = FMath::Clamp(HeroClassIndex, 0, 2);
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		// the stays of the city's rules: "cruise" (everything aboard that is for passengers), to Sunday night; the cabins go by the flags cruise_class_*
		const int32 Days = FMath::Clamp(4 - FMath::Max(1, Seen.Day), 1, 3);
		Things->StartStay(TEXT("cruise"), 0);
		Things->StartStay(TEXT("cruise"), Days);
	}
	const bool bLate = Seen.Phase != ELyudCruisePhase::Boarding;
	const FString LineKey = bLate ? FString(TEXT("cruise.say.board.late")) : FString(TEXT("cruise.say.board.")) + LyudCruiseImpl::ClassNames[ClassIndex];
	Say(LyudText::Get(TEXT("cruise.speaker.purser")), LyudText::Get(LineKey, { FStringFormatArg(HeroCabinNo) }), 9.f);
	UE_LOG(LogLyudCruise, Log, TEXT("CRUISE hero aboard: class %s, cabin %d, voyage %lld"), LyudCruiseImpl::ClassNames[ClassIndex], HeroCabinNo, (long long)HeroVoyage);
	ULyudQuests::Notify(this, TEXT("cruise_event"), TEXT("aboard"));
	PushFlags(Now, true);
}

void ULyudCruise::FinishAshore(double Now)
{
	if (!bAboard)
	{
		return;
	}
	LyudCruise::FHero H = HeroState();
	const bool bSailed = Work->Model.Ashore(H, Now);
	TakeHeroState(H);
	Work->AwayFor = 0.f;
	Work->bArrivalSeen = true;
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		const int32 Keys = Inv->Count(TEXT("cabin_key"));
		if (Keys > 0)
		{
			Inv->Remove(TEXT("cabin_key"), Keys);
		}
	}
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->StartStay(TEXT("cruise"), 0);
		Things->StartStay(TEXT("cruise_bridge"), 0);
	}
	UE_LOG(LogLyudCruise, Log, TEXT("CRUISE hero ashore%s; cruises sailed %d"), bSailed ? TEXT(" (the voyage counts)") : TEXT(""), CruisesSailed);
	ULyudQuests::Notify(this, TEXT("cruise_event"), TEXT("ashore"));
	PushFlags(Now, true);
}

void ULyudCruise::GiveBridgePass()
{
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (Things && !Things->IsOnStay(TEXT("cruise_bridge")))
	{
		Things->StartStay(TEXT("cruise_bridge"), 1);
		UE_LOG(LogLyudCruise, Log, TEXT("CRUISE bridge pass for today"));
	}
}

bool ULyudCruise::OnCityAct(const FCityActInfo& Info, FText& Line)
{
	// what an act aboard uses up (the city's acts ask for an item but do not take it): the postcard posted, the bubliki thrown
	(void)Line;
	if (Info.Plot == TEXT("P-SHIP") && !Info.bFromConsole)
	{
		if (ULyudInventory* Inv = ULyudInventory::Get(this))
		{
			if (Info.Act.StartsWith(TEXT("ship_post_box")))
			{
				Inv->Remove(TEXT("ship_postcard"), 1);
			}
			else if (Info.Act.StartsWith(TEXT("ship_gull_feed")))
			{
				Inv->Remove(TEXT("bubliki"), 1);
			}
		}
	}
	return false;
}

void ULyudCruise::OnSaveLoaded(ULyudSaveGame* Save)
{
	if (!Work.IsValid())
	{
		return;
	}
	// the record "Cruise" is in the fields; everything seen is of another time: look afresh (no events fired for the gap)
	(void)Save;
	Work->bSeenOn = false;
	Work->GraceFor = 0.f;
	Work->OverFor = 0.f;
	Work->AwayFor = 0.f;
	Work->AtShipFor = 0.f;
	Work->bArrivalSeen = bAboard;
	Work->MoveKind = 0;
	Work->Queue.Reset();
	Work->ResendClock = LyudCruiseImpl::ResendEvery;
	Seen.bHeroAboard = bAboard;
	Unsay();
}

// --- a move with a fade: ashore at the voyage's end, out of the sea

void ULyudCruise::BeginMove(int32 Kind, const TCHAR* Thing, const FVector& Fallback, float FallbackYaw, bool bFallbackGround, const TCHAR* SpeakerKey, const TCHAR* LineKey)
{
	const UWorld* World = GetWorld();
	Work->MoveKind = Kind;
	Work->MoveStage = 0;
	Work->MoveTries = 0;
	Work->MoveThing = Thing;
	Work->MoveFallback = Fallback;
	Work->MoveYaw = FallbackYaw;
	Work->bMoveGround = bFallbackGround;
	Work->MoveSpeaker = SpeakerKey;
	Work->MoveLine = LineKey;
	Work->MoveSince = World ? World->GetTimeSeconds() : 0.0;
	Work->MoveNext = Work->MoveSince;
}

void ULyudCruise::TickMove()
{
	if (Work->MoveKind == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	// the hero is needed only to begin (stage 0); once a fade to black is out it is always finished, hero or no hero
	if (!PC || (!Hero && Work->MoveStage == 0))
	{
		return;
	}
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	const double Time = World->GetTimeSeconds();
	if (Work->MoveStage == 0)
	{
		if (Time < Work->MoveNext)
		{
			return;
		}
		// asleep in his berth he is left to wake; a page, a game, a talk get twenty seconds
		if (Things && (Things->IsLyingOnThing() || Things->IsChoosingSleep() || Things->IsUsingLift()))
		{
			return;
		}
		if (Things && Things->HasModeUp() && Time - Work->MoveSince < 20.0)
		{
			return;
		}
		if (Hero->IsSeated())
		{
			Hero->StandUp();
			Work->MoveNext = Time + 0.3;
			return;
		}
		if (Things && Work->MoveTries < 3 && Things->NumThings() > 0)
		{
			++Work->MoveTries;
			if (Things->GoToThing(Work->MoveThing))
			{
				Work->MoveStage = 1;
				Work->MoveNext = Time + 0.5;
			}
			else
			{
				Work->MoveNext = Time + 0.6;
			}
			return;
		}
		// the city has no such thing (or no room by it): our own fade to the fallback place
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.4f, FLinearColor::Black, false, true);
		}
		Work->MoveStage = 2;
		Work->MoveNext = Time + 0.5;
		return;
	}
	if (Time < Work->MoveNext || (Work->MoveStage == 1 && Things && Things->IsUsingLift()))
	{
		return;
	}
	if (Work->MoveStage == 2)
	{
		LyudDev::TeleportWorld(World, Work->MoveFallback, Work->MoveYaw, Work->bMoveGround);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.6f, FLinearColor::Black, false, false);
		}
	}
	// there
	const int32 Kind = Work->MoveKind;
	Work->MoveKind = 0;
	Work->OverFor = 0.f;
	Work->GraceFor = 4.f;
	if (Kind == 1)
	{
		FinishAshore(NowSeconds());
	}
	Say(LyudText::Get(Work->MoveSpeaker), LyudText::Get(Work->MoveLine), 9.f);
}

// --- the subtitles (the city's screen-style page, as the train's announcements)

void ULyudCruise::Say(const FText& Speaker, const FText& Line, float Seconds)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 60);
		Page->SetScreen(true);
	}
	Page->SetPage(Speaker, Line, FText::GetEmpty());
	PageUntil = World->GetTimeSeconds() + Seconds;
}

void ULyudCruise::Unsay()
{
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ULyudCruise::TickSubtitle()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (Page && World->GetTimeSeconds() >= PageUntil)
	{
		Unsay();
	}
	if (!Page && Work->Queue.Num() > 0 && Work->MoveKind == 0)
	{
		const FWork::FLine L = Work->Queue[0];
		Work->Queue.RemoveAt(0, 1, EAllowShrinking::No);
		Say(L.Speaker, L.Line, L.Seconds);
	}
}

// --- developer mode

void ULyudCruise::DevBoardNow()
{
	if (!bStarted || !Work.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (bAboard)
	{
		UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE board: he is aboard already"));
		return;
	}
	if (Inv && !Work->bHasTicket && Inv->Count(TEXT("cruise_ticket")) == 0)
	{
		Inv->Add(TEXT("cruise_ticket"), 1);
	}
	// Friday 17:30 of the next voyage that sails (of this one while its boarding has half an hour left)
	const LyudCruise::FModel& Model = Work->Model;
	const double Now = NowSeconds();
	const double Half = 1800.0;
	const double Boarding = Model.NextBoarding(Now - (Model.BoardSec() - Half));
	if (Boarding < 0.0)
	{
		UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE board: no voyage ahead"));
		return;
	}
	const double When = Boarding + Model.BoardSec() - Half;
	if (When > Now)
	{
		SetClock(When);
	}
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (!Things || !Things->GoToThing(LyudCruiseImpl::LaunchGate))
	{
		LyudDev::TeleportWorld(World, PierFallback, PierFallbackYaw, true);
	}
	UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE board: a ticket, Friday 17:30, at the launch gate"));
}

bool ULyudCruise::DevNextEvent()
{
	if (!bStarted || !Work.IsValid())
	{
		return false;
	}
	const double Now = NowSeconds();
	double Seconds = -1.0;
	const int Index = Work->Model.NextEvent(Now, Seconds);
	if (Index < 0)
	{
		return false;
	}
	SetClock(Now + FMath::Max(0.0, Seconds - 5.0));
	UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE next: %s in 5 s (the clock moved %.0f min)"), *Work->EventTexts[Index], Seconds / 60.0);
	return true;
}

bool ULyudCruise::DevJumpTo(FName EventId)
{
	if (!bStarted || !Work.IsValid())
	{
		return false;
	}
	const int32 Index = Work->EventIds.IndexOfByKey(EventId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const double Now = NowSeconds();
	const double When = Work->Model.EventTime(Now, std::string(TCHAR_TO_UTF8(*Work->EventTexts[Index])));
	if (When < 0.0)
	{
		return false;
	}
	SetClock(When - 5.0);
	UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE jump: %s in 5 s (the clock moved %.1f h)"), *Work->EventTexts[Index], (When - 5.0 - Now) / 3600.0);
	return true;
}

FVector ULyudCruise::RockWorld() const
{
	// the island's chart point: the model's offset from the stage ship's midship point (x to the bow, y to starboard)
	const LyudCruise::FVec2 Off = Work.IsValid() ? Work->Model.AshoreOffset : LyudCruise::FVec2();
	return Stage.TransformPosition(FVector(HullLengthCm * 0.5 + Off.X * 100.0, Off.Y * 100.0, 0.0));
}

bool ULyudCruise::IsRealRockShown() const
{
	return Work.IsValid() && Work->bRockShown;
}

void ULyudCruise::DevLanding()
{
	if (!bStarted || !Work.IsValid() || Work->Model.AshoreStop < 0)
	{
		return;
	}
	const LyudCruise::FModel& Model = Work->Model;
	const double Now = NowSeconds();
	const LyudCruise::FState St = Model.At(Now);
	// ten minutes into the landing of the voyage on now (while it is still ahead or on), else of the next one that sails
	double Sail = -1.0;
	if (St.bRuns && St.Tv < Model.AshoreLastLaunch() - 1200.0)
	{
		Sail = Model.SailTime(St.Voyage);
	}
	else
	{
		const double Boarding = Model.NextBoarding(Now + 1.0);
		Sail = Boarding < 0.0 ? -1.0 : Boarding + Model.BoardSec();
	}
	if (Sail < 0.0)
	{
		UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE landing: no voyage ahead"));
		return;
	}
	const double When = Sail + Model.AshoreT0 + Model.AshoreLeadSec + 300.0;
	if (When > Now)
	{
		SetClock(When);
	}
	ULyudCityThings* Things = ULyudCityThings::Get(this);
	if (!Things || !Things->GoToThing(LyudCruiseImpl::JettyThing))
	{
		LyudDev::TeleportWorld(GetWorld(), JettyFallback, JettyFallbackYaw, false);
	}
	UE_LOG(LogLyudCruise, Warning, TEXT("CRUISE landing: Saturday 16:40 at the Far Light's jetty%s"), bAboard ? TEXT("") : TEXT(" (he is no passenger: board first for the bosun's warnings and the fetch)"));
}

void ULyudCruise::DevAshore()
{
	if (!bStarted || !Work.IsValid() || Work->MoveKind != 0)
	{
		return;
	}
	BeginMove(1, LyudCruiseImpl::LaunchGate, PierFallback, PierFallbackYaw, true, TEXT("cruise.speaker.bosun"), TEXT("cruise.say.ashore"));
}
