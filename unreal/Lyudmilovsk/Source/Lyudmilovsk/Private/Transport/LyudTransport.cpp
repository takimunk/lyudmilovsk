// The cable-car metro in the game (LyudTransport.h): the network running, the cabins and the people on it, the
// player's calls, rides and announcements.
#include "Transport/LyudTransport.h"

#include "Audio/LyudSoundscape.h"
#include "CablewaySim.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "../People/PeopleSim.h"
#include "SLyudTransitWidgets.h"
#include "Transport/LyudCableway.h"
#include "TransportStd.h"
#include "UI/LyudGameScreen.h"
#include "Weather/LyudWind.h"

namespace LT = LyudTransport;

static TAutoConsoleVariable<int32> CVarTransport(TEXT("lyud.Transport"), 1,
	TEXT("The cable-car metro (ULyudTransportSubsystem): 1 on in Lvl_Scene01, 0 off; 2 also in any other game level (tests)."));
static TAutoConsoleVariable<float> CVarTransportSpeedup(TEXT("lyud.Transport.Speedup"), 1.f,
	TEXT("Runs the cable car's simulation this many times faster (tests)."));

namespace
{
	const float PanelReach = 220.f;

	FString Key(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	FLinearColor LineColor(const LT::FLine& L)
	{
		return FLinearColor(FColor::FromHex(Key(L.Color)));
	}

	ULyudTransportSubsystem* ForConsole()
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.World() && (C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE))
			{
				if (ULyudTransportSubsystem* T = C.World()->GetSubsystem<ULyudTransportSubsystem>())
				{
					return T;
				}
			}
		}
		return nullptr;
	}

	FAutoConsoleCommand CmdReport(TEXT("Transport.Report"), TEXT("Prints every cabin of the cable car and the trips."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (ULyudTransportSubsystem* T = ForConsole())
			{
				UE_LOG(LogTemp, Display, TEXT("Transport:\n%s"), *T->Report());
			}
		}));
	FAutoConsoleCommand CmdRide(TEXT("Transport.Ride"), TEXT("Transport.Ride <from> <to>: puts the player on the platform of station <from> and calls a cabin to <to>."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (ULyudTransportSubsystem* T = ForConsole(); T && Args.Num() >= 2)
			{
				T->TestRide(FName(*Args[0]), FName(*Args[1]));
			}
		}));
	FAutoConsoleCommand CmdGoto(TEXT("Transport.Goto"), TEXT("Transport.Goto <station> [vestibule|gallery|platform|outside]: puts the player there (tests, looking round)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ULyudTransportSubsystem* T = ForConsole();
			APlayerController* PC = T && T->GetWorld() ? T->GetWorld()->GetFirstPlayerController() : nullptr;
			if (!T || !PC || !PC->GetPawn() || !Args.Num() || !T->GetCableway())
			{
				return;
			}
			const int32 S = T->FindStation(FName(*Args[0]));
			if (!T->GetCableway()->Built.IsValidIndex(S))
			{
				return;
			}
			const FLyudBuiltStation& B = T->GetCableway()->Built[S];
			const FString Where = Args.Num() > 1 ? Args[1] : TEXT("vestibule");
			// "vestibule": 6 m into the vestibule from its connector, facing the gallery; "gallery": its middle facing the
			// tower; "outside": by the street entrance looking in; "platform": by the call panel
			FTransform At = B.Room.GetLocation().IsZero() ? FTransform(FRotator(0.f, 0.f, 0.f), FVector(-600.f, 0.f, 100.f)) * B.Connector : B.Room;
			if (Where == TEXT("gallery"))
			{
				At = FTransform(FRotator::ZeroRotator, FVector(-120.f, 0.f, 100.f)) * B.Vestibule;
			}
			else if (Where == TEXT("outside"))
			{
				const FVector In = (B.Connector.GetLocation() - B.Entrance).GetSafeNormal2D();
				At = FTransform(FRotator(-6.f, In.Rotation().Yaw, 0.f), B.Entrance - In * 500.f + FVector(0, 0, 180.f));
			}
			else if (Where == TEXT("platform") && B.Panels.Num())
			{
				At = FTransform(FRotator(0.f, 180.f, 0.f), FVector(-250.f, 0.f, 100.f)) * B.Panels.Last();
			}
			else
			{
				T->GetCableway()->View(S, Where, At);
			}
			PC->GetPawn()->SetActorLocation(At.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(At.Rotator());
		}));
	FAutoConsoleCommand CmdProbe(TEXT("Transport.Probe"), TEXT("Transport.Probe [station]: traces down at the stations' landings, platforms, galleries (STATIONPROBE lines)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ULyudTransportSubsystem* T = ForConsole();
			if (!T || !T->GetCableway())
			{
				return;
			}
			int32 Fails = 0;
			for (int32 S = 0; S < T->GetCableway()->Built.Num(); S++)
			{
				if (!Args.Num() || T->FindStation(FName(*Args[0])) == S)
				{
					Fails += T->GetCableway()->Probe(S);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("STATIONPROBE done, %d fails"), Fails);
		}));
	FAutoConsoleCommand CmdCall(TEXT("Transport.Call"), TEXT("Transport.Call <from> <to> [who]: a trip for a person id (default 900) without a body."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ULyudTransportSubsystem* T = ForConsole();
			if (T && T->GetNet() && Args.Num() >= 2)
			{
				const int32 A = T->FindStation(FName(*Args[0])), B = T->FindStation(FName(*Args[1]));
				const int32 Who = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 900;
				UE_LOG(LogTemp, Display, TEXT("Transport.Call %s -> %s: rider %d"), *Args[0], *Args[1], T->GetNet()->Travel(Who, A, B));
			}
		}));
	FAutoConsoleCommand CmdDiagram(TEXT("Transport.Diagram"), TEXT("Transport.Diagram [station|close]: opens the route diagram (from a station: to pick where to go), or closes it."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (ULyudTransportSubsystem* T = ForConsole())
			{
				if (Args.Num() && Args[0] == TEXT("close"))
				{
					T->CloseDiagram();
				}
				else
				{
					T->OpenDiagram(Args.Num() ? T->FindStation(FName(*Args[0])) : -1);
				}
			}
		}));
}

ULyudTransportSubsystem::ULyudTransportSubsystem() = default;
ULyudTransportSubsystem::~ULyudTransportSubsystem() = default;

ULyudTransportSubsystem* ULyudTransportSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudTransportSubsystem>() : nullptr;
}

bool ULyudTransportSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudTransportSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudTransportSubsystem, STATGROUP_Tickables);
}

void ULyudTransportSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const int32 Mode = CVarTransport.GetValueOnGameThread();
	const bool bScene = InWorld.GetMapName().Contains(TEXT("Lvl_Scene01"));
	if (Mode == 0 || (!bScene && Mode < 2))
	{
		return;
	}
	Start(InWorld);
}

bool ULyudTransportSubsystem::Start(UWorld& World)
{
	FString Text;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/network_built.json"));
#if WITH_EDITOR
	// network_built.json is made per checkout from its terrain (not committed): make it now if this one has none
	if (!FPaths::FileExists(Path))
	{
		const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../../tools/transport.py")));
		int32 Code = -1;
		FString Out, Err;
		FPlatformProcess::ExecProcess(TEXT("/usr/bin/python3"), *FString::Printf(TEXT("\"%s\""), *Script), &Code, &Out, &Err);
		UE_LOG(LogTemp, Warning, TEXT("Transport: no network_built.json in this checkout: ran tools/transport.py (exit %d)%s%s"), Code,
			Err.IsEmpty() ? TEXT("") : TEXT(": "), *Err.Left(400));
	}
#endif
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("Transport: OFF: no %s. Run python3 tools/transport.py (build_scene01.sh does) and play again."), *Path);
		return false;
	}
	TPimplPtr<LT::FNetwork> N = MakePimpl<LT::FNetwork>();
	if (!N->Load(TCHAR_TO_UTF8(*Text)))
	{
		UE_LOG(LogTemp, Error, TEXT("Transport: OFF: %s"), UTF8_TO_TCHAR(N->LastError.c_str()));
		return false;
	}
	Net = MoveTemp(N);
	LyudTransit::SetActive(this);
	// a level built before the network had world_mesh's placeholder cable car: hide it
	for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
	{
		const UStaticMesh* M = It->GetStaticMeshComponent() ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		if (M && M->GetName().Contains(TEXT("World_CableCar")))
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
		}
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Cableway = World.SpawnActor<ALyudCableway>(ALyudCableway::StaticClass(), FTransform::Identity, SP);
	Cableway->Build(this, *Net);
	for (int32 I = 0; I < SimNum(Net->Cabins); I++)
	{
		const LT::FCabin& C = Net->Cabins[I];
		ALyudCabin* A = World.SpawnActor<ALyudCabin>(ALyudCabin::StaticClass(), FTransform::Identity, SP);
		A->Setup(Cableway, I, C.Number, LyudText::Get(Key(C.Name)), LineColor(Net->Lines[C.Line]));
		Cabins.Add(A);
	}
	Tick(0.f);
	UE_LOG(LogTemp, Display, TEXT("Transport: %d stations, %d lines, %d cabins\n%s"), SimNum(Net->Stations), SimNum(Net->Lines), SimNum(Net->Cabins), *Report());
	return true;
}

void ULyudTransportSubsystem::Deinitialize()
{
	CloseDiagram();
	if (LyudTransit::Active() == this)
	{
		LyudTransit::SetActive(nullptr);
	}
	if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this); People && People->GetSim())
	{
		People->GetSim()->RideHook = nullptr;
	}
	Net.Reset();
	Super::Deinitialize();
}

FVector ULyudTransportSubsystem::MapToWorld(float X, float Y, float Z) const
{
	const float Face = Net ? Net->PortalFaceY : 790.f;
	return FVector(X * 100.f, Face + Y * 100.f, Z * 100.f);
}

FVector2D ULyudTransportSubsystem::WorldToMap(const FVector& P) const
{
	const float Face = Net ? Net->PortalFaceY : 790.f;
	return FVector2D(P.X / 100.0, (P.Y - Face) / 100.0);
}

// --- queries -----------------------------------------------------------------------------------------------

int32 ULyudTransportSubsystem::FindStation(FName Id) const
{
	return Net ? Net->FindStation(TCHAR_TO_UTF8(*Id.ToString())) : -1;
}

FText ULyudTransportSubsystem::StationName(int32 Station) const
{
	return Net && SimValid(Net->Stations, Station) ? LyudText::Get(Key(Net->Stations[Station].Name)) : FText::GetEmpty();
}

TArray<FLyudTransitStation> ULyudTransportSubsystem::GetStations() const
{
	TArray<FLyudTransitStation> Out;
	if (!Net)
	{
		return Out;
	}
	for (int32 S = 0; S < SimNum(Net->Stations); S++)
	{
		const LT::FStation& St = Net->Stations[S];
		FLyudTransitStation& O = Out.AddDefaulted_GetRef();
		O.Id = FName(Key(St.Id));
		O.NameKey = Key(St.Name);
		O.MapAt = FVector2D(St.X, St.Y);
		O.Schema = FVector2D(St.SchemaX, St.SchemaY);
		for (const LT::FLevel& Lv : St.Levels)
		{
			O.Lines.Add(FName(Key(Net->Lines[Lv.Line].Id)));
		}
		O.Entrance = Cableway && Cableway->Built.IsValidIndex(S) ? Cableway->Built[S].Entrance : MapToWorld(St.CoreX, St.CoreY, St.CoreGround);
	}
	return Out;
}

TArray<FLyudTransitLine> ULyudTransportSubsystem::GetLines() const
{
	TArray<FLyudTransitLine> Out;
	if (!Net)
	{
		return Out;
	}
	for (const LT::FLine& L : Net->Lines)
	{
		FLyudTransitLine& O = Out.AddDefaulted_GetRef();
		O.Id = FName(Key(L.Id));
		O.NameKey = Key(L.Name);
		O.Number = L.Number;
		O.Color = LineColor(L);
		O.Depot = L.Depot;
		for (int32 S : L.Stations)
		{
			O.Stations.Add(S);
		}
		// the way out only (the first half of the loop, up to the turn at the last station)
		const float Half = L.Stops.empty() ? L.Length : L.Length * 0.5f;
		for (int32 I = 0; I < SimNum(L.Loop); I++)
		{
			if (L.LoopS[I] > Half + 30.f)
			{
				break;
			}
			if (I % 3 == 0)
			{
				O.Path.Add(FVector2D(L.Loop[I].X, L.Loop[I].Y));
			}
		}
	}
	return Out;
}

TArray<FLyudTransitCabin> ULyudTransportSubsystem::GetCabins() const
{
	TArray<FLyudTransitCabin> Out;
	if (!Net)
	{
		return Out;
	}
	for (int32 I = 0; I < SimNum(Net->Cabins); I++)
	{
		const LT::FCabin& C = Net->Cabins[I];
		FLyudTransitCabin& O = Out.AddDefaulted_GetRef();
		O.Id = FName(Key(C.Id));
		O.Number = C.Number;
		O.NameKey = Key(C.Name);
		O.Line = C.Line;
		const LT::V3 P = Net->CabinAt(I);
		O.MapAt = FVector2D(P.X, P.Y);
		O.State = FName(LT::CabinStateName(C.State));
		O.Riders = SimNum(C.Riders);
		O.NextStation = Net->NextStation(I);
		O.Destination = Net->Destination(I);
	}
	return Out;
}

bool ULyudTransportSubsystem::CallCabin(int32 From, int32 To)
{
	if (!Net)
	{
		return false;
	}
	const int32 R = Net->Travel(LT::PlayerId, From, To);
	UE_LOG(LogTemp, Display, TEXT("Transport: the player calls a cabin %s -> %s (rider %d)"), *StationName(From).ToString(), *StationName(To).ToString(), R);
	return R >= 0;
}

float ULyudTransportSubsystem::WaitAt(int32 Station, int32 Line, bool& bCalled) const
{
	bCalled = false;
	return Net ? Net->Eta(Station, Line, -1, bCalled) : -1.f;
}

int32 ULyudTransportSubsystem::PlayerStation() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn && Cableway ? Cableway->StationAt(Pawn->GetActorLocation()) : -1;
}

bool ULyudTransportSubsystem::PlayerTrip(int32& From, int32& To) const
{
	From = To = -1;
	const int32 R = Net ? Net->FindRider(LT::PlayerId) : -1;
	if (R < 0)
	{
		return false;
	}
	const LT::FRider& Rd = Net->Riders[R];
	From = Rd.Legs.front().From;
	To = Rd.Legs.back().To;
	return true;
}

FString ULyudTransportSubsystem::Report() const
{
	return Net ? FString(UTF8_TO_TCHAR(Net->Report().c_str())) : FString(TEXT("off"));
}

// --- the diagram -------------------------------------------------------------------------------------------

void ULyudTransportSubsystem::OpenDiagram(int32 FromStation)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !Net)
	{
		return;
	}
	CloseDiagram();
	ScreenStation = FromStation;
	Screen = SNew(SLyudTransitScreen).From(FromStation);
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 86);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
	{
		UI->Lock(true);
		UI->SetPrompt(this, FText::GetEmpty());
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Screen);
	}
}

void ULyudTransportSubsystem::CloseDiagram()
{
	if (!Screen.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->Lock(false);
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
	Screen.Reset();
	PromptCooldown = 0.3f;
}

// --- people ------------------------------------------------------------------------------------------------

bool ULyudTransportSubsystem::BeginRide(int32 Person, const FVector& From, const FVector& To)
{
	if (!Net)
	{
		return false;
	}
	auto Nearest = [this](const FVector& P)
	{
		const FVector2D M = WorldToMap(P);
		int32 Best = -1;
		float BestD = 120.f;     // metres: the people's map has the stations' doors near their old spots
		for (int32 S = 0; S < SimNum(Net->Stations); S++)
		{
			const float D = FVector2D::Distance(M, FVector2D(Net->Stations[S].X, Net->Stations[S].Y));
			if (D < BestD)
			{
				BestD = D;
				Best = S;
			}
		}
		return Best;
	};
	const int32 A = Nearest(From), B = Nearest(To);
	if (A < 0 || B < 0 || A == B)
	{
		return false;
	}
	const int32 R = Net->Travel(Person, A, B);
	if (R < 0)
	{
		return false;
	}
	PeopleRides.Add(Person, R);
	UE_LOG(LogTemp, Display, TEXT("Transport: person %d rides %s -> %s (rider %d)"), Person, *StationName(A).ToString(), *StationName(B).ToString(), R);
	return true;
}

bool ULyudTransportSubsystem::RiderPose(int32 Person, FVector& Location, float& Yaw, bool& bSeated) const
{
	const int32* R = PeopleRides.Find(Person);
	if (!R || !Net || !SimValid(Net->Riders, *R) || !Cableway)
	{
		return false;
	}
	const LT::FRider& Rd = Net->Riders[*R];
	if (Rd.State == LT::ERiderState::Riding && Cabins.IsValidIndex(Rd.Cabin) && Cabins[Rd.Cabin])
	{
		const FTransform T = Cabins[Rd.Cabin]->Seat(Rd.Seat % 4);
		Location = T.GetLocation();
		Yaw = T.Rotator().Yaw;
		bSeated = true;
		return true;
	}
	if (Rd.State == LT::ERiderState::Waiting)
	{
		const LT::FLeg& G = Rd.Legs[Rd.LegI];
		const FTransform T = Cableway->WaitSpot(G.From, G.Line, *R % 6);
		Location = T.GetLocation();
		Yaw = T.Rotator().Yaw;
		bSeated = false;
		return true;
	}
	return false;
}

// --- running -----------------------------------------------------------------------------------------------

void ULyudTransportSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Net)
	{
		return;
	}
	UWorld* World = GetWorld();
	// the people's rides wait for real cabins (installed once their life runs)
	if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this); People && People->GetSim() && !People->GetSim()->RideHook)
	{
		TWeakObjectPtr<ULyudTransportSubsystem> Self(this);
		People->GetSim()->RideHook = [Self](int Person, const LyudPeople::V3& From, const LyudPeople::V3& To)
		{
			ULyudTransportSubsystem* T = Self.Get();
			return T && T->BeginRide(Person, FVector(From.X, From.Y, From.Z) * 100.f, FVector(To.X, To.Y, To.Z) * 100.f);
		};
	}
	const float Dt = FMath::Min(DeltaTime, 0.2f) * FMath::Max(0.f, CVarTransportSpeedup.GetValueOnGameThread());
	if (Dt > 0.f)
	{
		Net->Step(Dt);
		// the ropes sag and sway, the cabins swing: in the wind over the viewer
		FVector Wind = FVector::ZeroVector;
		if (const ULyudWindSubsystem* WS = ULyudWindSubsystem::Get(this))
		{
			FVector V;
			FRotator R;
			if (const APlayerController* P = World ? World->GetFirstPlayerController() : nullptr)
			{
				P->GetPlayerViewPoint(V, R);
				Wind = WS->GetWindAt(V, 25.f).Velocity;
			}
		}
		Net->StepMotion(FMath::Min(DeltaTime, 0.05f), LT::V3(Wind.X, Wind.Y, 0.f));
	}
	HandleEvents();

	FVector View = FVector::ZeroVector;
	FRotator ViewRot;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		PC->GetPlayerViewPoint(View, ViewRot);
	}
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	for (int32 I = 0; I < Cabins.Num() && I < SimNum(Net->Cabins); I++)
	{
		ALyudCabin* A = Cabins[I];
		const LT::FCabin& C = Net->Cabins[I];
		if (!A)
		{
			continue;
		}
		if (C.State == LT::ECabinState::Depot)
		{
			// parked in a row on the depot's storage rail
			const LT::FLine& Ln = Net->Lines[C.Line];
			const FLyudBuiltStation* B = Cableway && Cableway->Built.IsValidIndex(Ln.Depot) ? &Cableway->Built[Ln.Depot] : nullptr;
			const int32 K = B ? B->DeckLines.Find(C.Line) : INDEX_NONE;
			if (B && K != INDEX_NONE)
			{
				const int32 End = SimValid(Net->Stations[Ln.Depot].Ends, C.Line) ? Net->Stations[Ln.Depot].Ends[C.Line] : -1;
				const float Dir = End == 1 ? -1.f : 1.f;
				const FVector Grip = B->Decks[K].TransformPosition(FVector(Dir * (650.f + 200.f + 560.f * C.DepotSlot), 0.f, ALyudCabin::GripOverFloor()));
				A->Drive(Grip, B->Decks[K].GetRotation().Vector(), 0.f, 0.f);
			}
		}
		else
		{
			LT::V3 D;
			const LT::V3 P = Net->CabinAt(I, &D);
			const FTransform Before = A->GetActorTransform();
			A->Drive(MapToWorld(P.X, P.Y, P.Z), FVector(D.X, D.Y, D.Z), C.Door, DeltaTime,
				FMath::RadiansToDegrees(C.Pitch), FMath::RadiansToDegrees(C.Roll));
			// the player aboard goes with it (unless the character's own moving base already carries them)
			if (I == RidingCabin)
			{
				Carry(Before, A->GetActorTransform(), A);
			}
		}
		// the motor's hum near the player while it moves
		const float Dist = FVector::Dist(A->GetActorLocation(), View);
		const bool bHum = C.State == LT::ECabinState::Moving && Dist < 15000.f;
		if (bHum && !A->Hum && Sound)
		{
			A->Hum = Sound->AttachLoop(TEXT("cabin_motor"), A->GetRootComponent(), 0.6f);
		}
		else if (!bHum && A->Hum)
		{
			A->Hum->Stop();
			A->Hum->DestroyComponent();
			A->Hum = nullptr;
		}
		if (A->Hum)
		{
			A->Hum->SetPitchMultiplier(0.8f + 0.04f * C.Speed);
		}
		A->Lamp->SetVisibility(Dist < 8000.f);
		// the box cabin's shell (an old kit) gets in the view from inside: the player aboard sees the boxes only; the
		// hollow cabin is seen from inside as it is
		if (!A->bRoom && A->Shell->GetStaticMesh() && A->Shell->IsVisible() == (I == RidingCabin))
		{
			A->Shell->SetVisibility(I != RidingCabin);
		}
	}
	RiderIgnoresWay(RidingCabin >= 0 && RidingCabin < SimNum(Net->Cabins) && Net->Cabins[RidingCabin].State == LT::ECabinState::Moving);
	if (Cableway)
	{
		Cableway->UpdateBoards(*Net, View, DeltaTime);
		Cableway->UpdateRopes(*Net, View);
	}
	UpdatePlayer(DeltaTime);
}

void ULyudTransportSubsystem::RiderIgnoresWay(bool bIgnore)
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Char || !Cableway || bIgnore == bRiderIgnoresWay)
	{
		return;
	}
	bRiderIgnoresWay = bIgnore;
	Char->GetCapsuleComponent()->IgnoreActorWhenMoving(Cableway, bIgnore);
}

void ULyudTransportSubsystem::Carry(const FTransform& Before, const FTransform& After, AActor* Cabin)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Char || Before.Equals(After, 0.01f))
	{
		return;
	}
	const UPrimitiveComponent* Base = Char->GetMovementBase();
	if (Base && Base->GetOwner() == Cabin)
	{
		return;
	}
	const FVector P = After.TransformPosition(Before.InverseTransformPosition(Char->GetActorLocation()));
	const float Turn = After.Rotator().Yaw - Before.Rotator().Yaw;
	Char->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
	if (FMath::Abs(Turn) > 0.001f)
	{
		Char->AddActorWorldRotation(FRotator(0.f, Turn, 0.f));
		PC->SetControlRotation(PC->GetControlRotation() + FRotator(0.f, Turn, 0.f));
	}
}

void ULyudTransportSubsystem::HandleEvents()
{
	const std::vector<LT::FEvent> Events = Net->Events;
	Net->Events.clear();
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	for (const LT::FEvent& E : Events)
	{
		static const TCHAR* Names[] = { TEXT("Launch"), TEXT("Arrive"), TEXT("Depart"), TEXT("Board"), TEXT("Alight"), TEXT("Done"), TEXT("Park") };
		const int32 Who = SimValid(Net->Riders, E.Rider) ? Net->Riders[E.Rider].Who : -1;
		OnTransportEvent.Broadcast(FName(Names[(int)E.Kind]), E.Cabin, E.Station, Who);
		ALyudCabin* A = Cabins.IsValidIndex(E.Cabin) ? Cabins[E.Cabin].Get() : nullptr;
		if (A && Sound)
		{
			if (E.Kind == LT::EEvent::Arrive)
			{
				Sound->PlayEffect(TEXT("cabin_door"), A->DoorPoint() + FVector(0, 0, 120.f), 0.8f);
				Sound->PlayEffect(TEXT("station_chime"), A->DoorPoint() + FVector(0, 0, 300.f), 0.5f);
			}
			else if (E.Kind == LT::EEvent::Depart)
			{
				Sound->PlayEffect(TEXT("cabin_door"), A->DoorPoint() + FVector(0, 0, 120.f), 0.7f);
			}
		}
		if (E.Cabin == RidingCabin && RidingCabin >= 0)
		{
			if (E.Kind == LT::EEvent::Depart)
			{
				const int32 Next = Net->NextStation(E.Cabin);
				if (Next >= 0)
				{
					Announce(E.Cabin, FName(*FString::Printf(TEXT("next_%s"), *Key(Net->Stations[Next].Id))), Next);
				}
			}
			else if (E.Kind == LT::EEvent::Arrive && SimValid(Net->Stations, E.Station))
			{
				Announce(E.Cabin, FName(*FString::Printf(TEXT("arrive_%s"), *Key(Net->Stations[E.Station].Id))), E.Station);
			}
		}
		// a person's trip is over: on along their way from the station
		if (E.Kind == LT::EEvent::Done && Who >= 0 && PeopleRides.Contains(Who))
		{
			PeopleRides.Remove(Who);
			if (People && People->GetSim())
			{
				People->GetSim()->EndRide(Who);
			}
		}
	}
}

void ULyudTransportSubsystem::Announce(int32 Cabin, FName What, int32 Station)
{
	// the voice (line.transport.<what>) and the same words as a note on the screen
	const FString LineKey = FString::Printf(TEXT("line.transport.%s.1"), *What.ToString());
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this); Sound && Cabins.IsValidIndex(Cabin) && Cabins[Cabin])
	{
		Sound->PlayVoiceAt(LyudText::LoadVoice(LineKey), Cabins[Cabin]->GetActorLocation() + FVector(0, 0, 180.f), 0.9f);
	}
	if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
	{
		GS->Toast(LyudText::Get(LineKey));
	}
}

void ULyudTransportSubsystem::UpdatePlayer(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	if (!Pawn || !Net)
	{
		return;
	}
	const FVector P = Pawn->GetActorLocation();
	// in a cabin?
	int32 In = -1;
	for (int32 I = 0; I < Cabins.Num(); I++)
	{
		LT::FCabin& C = Net->Cabins[I];
		const bool bIn = Cabins[I] && Cabins[I]->Contains(P - FVector(0, 0, 90.f));
		C.bPlayerAboard = bIn;
		// standing in the doorway holds the doors
		C.bHold = Cabins[I] && !bIn && C.State == LT::ECabinState::Dwelling && FVector::Dist2D(Cabins[I]->DoorPoint(), P) < 110.f;
		if (bIn)
		{
			In = I;
		}
	}
	if (In != RidingCabin)
	{
		if (RidingCabin >= 0)
		{
			Net->PlayerAlight(RidingCabin);
		}
		if (In >= 0)
		{
			int32 From = -1, To = -1;
			PlayerTrip(From, To);
			const int32 R = Net->FindRider(LT::PlayerId);
			const int32 LegTo = R >= 0 ? Net->Riders[R].Legs[Net->Riders[R].LegI].To : To;
			Net->PlayerBoard(In, LegTo);
			const LT::FCabin& C = Net->Cabins[In];
			const int32 Dest = Net->Destination(In);
			if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
			{
				GS->Toast(LyudText::Get(TEXT("transport.cabin.welcome"), { FStringFormatArg(LyudText::Get(Key(C.Name)).ToString()),
					FStringFormatArg(Dest >= 0 ? StationName(Dest).ToString() : FString()) }));
			}
		}
		RidingCabin = In;
	}
	else if (In >= 0 && Net->FindRider(LT::PlayerId) < 0 && Net->Cabins[In].AtStop >= 0 && Net->Cabins[In].State != LT::ECabinState::Moving
		&& Net->Time > Net->Cabins[In].DwellUntil - 1.0)
	{
		// still aboard after their stop: they ride on to the next
		Net->PlayerBoard(In, -1);
	}

	// prompts: a station's call panel, the club's board
	PromptCooldown = FMath::Max(0.f, PromptCooldown - DeltaTime);
	if (Screen.IsValid() || !UI)
	{
		return;
	}
	FText Prompt;
	int32 PanelStation = -1;
	bool bClubBoard = false;
	if (Cableway)
	{
		const int32 S = Cableway->StationAt(P);
		if (S >= 0 && Cableway->PanelNear(S, P, PanelReach) >= 0)
		{
			PanelStation = S;
			Prompt = LyudText::Get(TEXT("transport.prompt.call"));
		}
		else if (const int32 V = Cableway->VestibuleBoardNear(P, 250.f); V >= 0)
		{
			PanelStation = V;
			Prompt = LyudText::Get(TEXT("transport.prompt.call"));
		}
		else if (Cableway->ClubBoardNear(P, 350.f))
		{
			bClubBoard = true;
			Prompt = LyudText::Get(TEXT("transport.prompt.board"));
		}
	}
	if (RidingCabin >= 0 && Prompt.IsEmpty())
	{
		const LT::FCabin& C = Net->Cabins[RidingCabin];
		const int32 Dest = Net->Destination(RidingCabin);
		Prompt = LyudText::Get(TEXT("transport.prompt.ride"), { FStringFormatArg(LyudText::Get(Key(C.Name)).ToString()),
			FStringFormatArg(Dest >= 0 ? StationName(Dest).ToString() : LyudText::Get(TEXT("transport.cabin.idle")).ToString()) });
	}
	UI->SetPrompt(this, Prompt);
	if ((PanelStation >= 0 || bClubBoard) && PromptCooldown <= 0.f && PC->WasInputKeyJustPressed(EKeys::E) && !UI->IsOpen())
	{
		OpenDiagram(PanelStation);
	}
}

bool ULyudTransportSubsystem::TestRide(FName From, FName To)
{
	const int32 A = FindStation(From), B = FindStation(To);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Net || !Cableway || A < 0 || B < 0 || !Pawn)
	{
		return false;
	}
	// on the platform of the first leg's line, at the panel
	std::vector<LT::FLeg> Legs;
	const int32 R = Net->Travel(LT::PlayerId, A, B);
	if (R < 0)
	{
		return false;
	}
	const LT::FLeg& G = Net->Riders[R].Legs[0];
	const FTransform Spot = Cableway->WaitSpot(A, G.Line, 0);
	Pawn->SetActorLocation(Spot.GetLocation() + FVector(0, 0, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(Spot.Rotator());
	UE_LOG(LogTemp, Display, TEXT("Transport.Ride: the player on %s's platform, a cabin called to %s (rider %d)"), *From.ToString(), *To.ToString(), R);
	return true;
}
