#include "Railway/LyudTrainPassenger.h"

#include "RailTicketModel.h"
#include "../City/CityKit.h"
#include "City/SLyudCityPage.h"
#include "Audio/LyudSoundscape.h"
#include "Dev/LyudDev.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Railway/LyudRailway.h"
#include "Railway/LyudTrain.h"
#include "UI/LyudGameScreen.h"
#include "World/LyudPrefetch.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudPassenger, Log, All);

namespace LyudPassengerImpl
{
	const TCHAR* DataFile = TEXT("Railway/passenger.json");
	const TCHAR* BuiltFile = TEXT("Railway/railway_built.json");
	const TCHAR* PropMat = TEXT("/Game/City/M_CityProp.M_CityProp");
	const FName SaveKey(TEXT("TrainPassenger"));
	const FName Ticket(TEXT("rail_ticket"));
	const FName TicketPunched(TEXT("rail_ticket_punched"));
	constexpr float BoxUseCm = 170.f;        // how near the box's front the hero must stand
	constexpr float StandOffCm = 95.f;       // the conductor stops this short of the hero, along the aisle
	constexpr double AskSeconds = 2.6;       // "your ticket?" before the verdict
	constexpr double AnswerSeconds = 4.5;
	constexpr float BoxBackCm = 45.f;        // the box against the platform's railing (as the nameboard)
	constexpr double BoxAlongM = -4.5;       // metres along the line from the halt's middle (between the board and the shelter)

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	FText Key(const std::string& S) { return LyudText::Get(Str(S)); }

	/** The ticket box: a post, a green cabinet with a cream face, the coin slot and the ticket's mouth, a little roof, an amber
	 *  lamp on top (glows from UV2, the material's Glow). Front +X, origin on the deck, metres. */
	void BuildTicketBox(CityKit::FMesh& M)
	{
		using namespace CityKit;
		const FSkin Green = Paint(46, 92, 64, 0.5f);
		const FSkin Cream = Paint(232, 222, 196, 0.6f);
		const FSkin Dark = Paint(30, 30, 30, 0.4f);
		FSkin Brass = Paint(180, 130, 50, 0.3f);
		FSkin Lamp = Paint(255, 190, 90, 0.2f);
		Lamp.Glow = 1.f;
		M.Box(FVector(0, 0, 0.45), FVector(0.06, 0.06, 0.45), Iron(), FRotator::ZeroRotator, true);
		M.Box(FVector(0, 0, 0.02), FVector(0.2, 0.2, 0.02), Iron());
		M.Box(FVector(0, 0, 1.22), FVector(0.17, 0.27, 0.34), Green);
		M.Box(FVector(0.172, 0, 1.28), FVector(0.004, 0.22, 0.22), Cream);
		M.Box(FVector(0.178, 0.12, 1.36), FVector(0.004, 0.03, 0.006), Dark);       // the coin slot
		M.Box(FVector(0.178, -0.08, 1.02), FVector(0.006, 0.07, 0.012), Dark);      // where the ticket comes out
		M.Box(FVector(0.18, 0.0, 1.18), FVector(0.01, 0.025, 0.025), Brass);        // the button
		M.Box(FVector(0.03, 0, 1.6), FVector(0.24, 0.33, 0.025), Green);            // the roof
		M.Sphere(FVector(0.0, 0, 1.69), 0.06, 10, Lamp);
		M.Collide(FVector(0, 0, 0.85), FVector(0.2, 0.3, 0.85));
	}
}

ULyudTrainPassenger::ULyudTrainPassenger() = default;
ULyudTrainPassenger::~ULyudTrainPassenger() = default;

ULyudTrainPassenger* ULyudTrainPassenger::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudTrainPassenger>() : nullptr;
}

void ULyudTrainPassenger::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	bWorld = true;
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(LyudPassengerImpl::SaveKey, this);
	}
}

void ULyudTrainPassenger::Deinitialize()
{
	EndConductor();
	Unsay();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
	{
		Saves->UnregisterSaveObject(LyudPassengerImpl::SaveKey);
	}
	bWorld = false;
	Super::Deinitialize();
}

bool ULyudTrainPassenger::LoadModel()
{
	if (bLoaded)
	{
		return Model.IsValid();
	}
	bLoaded = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / LyudPassengerImpl::DataFile)))
	{
		UE_LOG(LogLyudPassenger, Display, TEXT("Passenger: no %s"), LyudPassengerImpl::DataFile);
		return false;
	}
	TPimplPtr<LyudTicket::FModel> M = MakePimpl<LyudTicket::FModel>();
	if (!M->Load(std::string(TCHAR_TO_UTF8(*Text))))
	{
		UE_LOG(LogLyudPassenger, Warning, TEXT("Passenger: %s"), *LyudPassengerImpl::Str(M->LastError));
		return false;
	}
	Model = MoveTemp(M);
	return true;
}

// --- the ticket boxes on the halts

void ULyudTrainPassenger::BuildBoxes()
{
	UWorld* World = GetWorld();
	ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(World);
	if (!World || !R || !R->IsRunning())
	{
		return;
	}
	bBoxesBuilt = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / LyudPassengerImpl::BuiltFile)))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return;
	}
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, LyudPassengerImpl::PropMat, nullptr, LOAD_NoWarn | LOAD_Quiet);
	CityKit::FMesh Kit;
	LyudPassengerImpl::BuildTicketBox(Kit);
	UStaticMesh* Mesh = Kit.Build(this, TEXT("SM_RailTicketBox"), true);
	const TArray<TSharedPtr<FJsonValue>>* Stops = nullptr;
	if (!Root->TryGetArrayField(TEXT("stops"), Stops) || !Stops)
	{
		return;
	}
	const double Edge = ULyudRailwaySubsystem::PlatformEdge * 100.0, Wd = ULyudRailwaySubsystem::PlatformWidth * 100.0;
	for (const TSharedPtr<FJsonValue>& V : *Stops)
	{
		const TSharedPtr<FJsonObject> St = V->AsObject();
		if (!St.IsValid())
		{
			continue;
		}
		const double Sm = St->GetNumberField(TEXT("s")) + LyudPassengerImpl::BoxAlongM;
		const int32 Side = int32(St->GetNumberField(TEXT("side")));
		const FVector At = R->LineToWorld(Sm, Side * (Edge + Wd - LyudPassengerImpl::BoxBackCm) / 100.0, ULyudRailwaySubsystem::PlatformOverRail);
		const FVector Rail = R->LineToWorld(Sm, 0.0, 0.0);
		const FVector Facing = FVector(Rail.X - At.X, Rail.Y - At.Y, 0.0).GetSafeNormal();
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), At, Facing.Rotation(), SP);
		if (!A)
		{
			continue;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A, TEXT("TicketBox"));
		A->SetRootComponent(C);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(At, Facing.Rotation());
		if (Mat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, C);
			MID->SetScalarParameterValue(TEXT("Glow"), 0.6f);
			C->SetMaterial(0, MID);
		}
		BoxActors.Add(A);
		Boxes.Add({ At + Facing * 20.0 + FVector(0, 0, 120.0), Facing });
	}
	UE_LOG(LogLyudPassenger, Display, TEXT("Passenger: %d ticket boxes on the halts"), Boxes.Num());
}

void ULyudTrainPassenger::TickBoxes(AHeroCharacter* Hero, APlayerController* PC)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ULyudInventory* Inv = ULyudInventory::Get(this);
	int32 Near = -1;
	if (Hero && PC && UI && Inv && !UI->IsOpen() && !Hero->IsSeated())
	{
		const FVector Eye = Hero->GetPawnViewLocation();
		const FVector View = PC->GetControlRotation().Vector();
		for (int32 I = 0; I < Boxes.Num(); ++I)
		{
			const FVector To = Boxes[I].Front - Eye;
			if (FVector::Dist2D(Hero->GetActorLocation(), Boxes[I].Front) < LyudPassengerImpl::BoxUseCm
				&& FVector::DotProduct(To.GetSafeNormal(), View) > 0.55 && FVector::DotProduct(-Boxes[I].Facing, View) > 0.0)
			{
				Near = I;
			}
		}
	}
	const int32 Fare = Model ? Model->Fare : 5;
	if (Near != PromptBox && UI)
	{
		PromptBox = Near;
		const bool bHave = Inv && Inv->Count(LyudPassengerImpl::Ticket) > 0;
		UI->SetPrompt(this, Near >= 0 ? LyudText::Get(bHave ? TEXT("rail.ticket.have") : TEXT("rail.ticket.prompt"), { FStringFormatArg(Fare) }) : FText::GetEmpty());
	}
	if (Near >= 0 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
		if (!Inv->Spend(Fare))
		{
			if (Screen)
			{
				Screen->Toast(LyudText::Get(TEXT("rail.ticket.no_money"), { FStringFormatArg(Fare) }));
			}
			return;
		}
		Inv->Add(LyudPassengerImpl::Ticket, 1);
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(TEXT("rest_coins"), Boxes[Near].Front, 0.7f);
		}
		if (Screen)
		{
			Screen->Toast(LyudText::Get(TEXT("rail.ticket.bought"), { FStringFormatArg(Fare) }));
		}
		PromptBox = -2;      // the prompt is said again (now "you have a ticket")
	}
}

// --- the ride: announcements, the conductor's turn

FText ULyudTrainPassenger::StopName(int32 Stop) const
{
	const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	return R && Stop >= 0 ? LyudText::Get(R->GetStopNameKey(Stop)) : FText::GetEmpty();
}

int32 ULyudTrainPassenger::NextStop(int32 Stop, int32 Dir) const
{
	const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	const int32 N = R ? R->NumStops() : 0;
	const int32 Next = Stop + (Dir >= 0 ? 1 : -1);
	return Stop >= 0 && Next >= 0 && Next < N ? Next : -1;
}

float ULyudTrainPassenger::AlongOf(const FVector& World) const
{
	const ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	const ALyudTrain* T = R && RideTrain >= 0 ? R->GetTrain(RideTrain) : nullptr;
	if (!T)
	{
		return 0.f;
	}
	const double S = T->GetInfo().S;
	return (float)FVector::DotProduct(World - R->LineToWorld(S, 0.0, 0.0), R->LineDir(S));
}

void ULyudTrainPassenger::TickTrain(AHeroCharacter* Hero, float DeltaTime)
{
	UWorld* World = GetWorld();
	ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	if (!World || !R || !R->IsRunning() || !Hero)
	{
		return;
	}
	// which car he is in (none: the ride is over)
	int32 In = -1;
	for (int32 T = 0; T < R->NumTrains() && In < 0; ++T)
	{
		const ALyudTrain* Tr = R->GetTrain(T);
		if (Tr && Tr->IsInside(Hero->GetActorLocation()))
		{
			In = T;
		}
	}
	if (In != RideTrain)
	{
		if (In < 0)
		{
			EndConductor();
		}
		RideTrain = In;
		LastStop = -2;
		bPunchedThisRide = false;
		bCheckedThisRide = false;
		ConductorAt = -1.0;
	}
	if (RideTrain < 0)
	{
		return;
	}
	const FLyudTrainInfo Info = R->GetTrain(RideTrain)->GetInfo();
	if (LastStop == -2)
	{
		LastStop = Info.Stop;
		bLastDoors = Info.bDoorsOpen;
		LastDir = Info.Dir;
	}
	const double Now = World->GetTimeSeconds();
	const FText Title = LyudText::Get(TEXT("rail.announce.title"));
	if (bLastDoors && !Info.bDoorsOpen)
	{
		Say(Title, LyudText::Get(TEXT("rail.announce.doors")), 3.5f);
	}
	if (LastStop >= 0 && Info.Stop < 0)
	{
		// off from a halt: the next one, and the conductor's round of this stretch
		const int32 Next = NextStop(LastStop, Info.Dir);
		if (Next >= 0)
		{
			Say(Title, LyudText::Get(TEXT("rail.announce.next"), { FStringFormatArg(StopName(Next).ToString()) }), 5.f);
		}
		bPunchedThisRide = bPunchedThisRide && bCheckedThisRide;
		bCheckedThisRide = false;
		if (Model && Walk == EWalk::None)
		{
			ConductorAt = Now + Model->StartAfter;
		}
	}
	if (LastStop < 0 && Info.Stop >= 0)
	{
		const bool bEnd = Info.Dir < 0 && NextStop(Info.Stop, Info.Dir) < 0;
		Say(Title, bEnd ? LyudText::Get(TEXT("rail.announce.last"))
			: LyudText::Get(TEXT("rail.announce.arrive"), { FStringFormatArg(StopName(Info.Stop).ToString()) }), 5.f);
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(TEXT("radio_chime"), Hero->GetActorLocation(), 0.5f);
		}
	}
	LastStop = Info.Stop;
	bLastDoors = Info.bDoorsOpen;
	LastDir = Info.Dir;
	if (ConductorAt > 0.0 && Now >= ConductorAt && !bCheckedThisRide)
	{
		ConductorAt = -1.0;
		StartConductor();
	}
	TickConductor(Hero, DeltaTime);
}

bool ULyudTrainPassenger::CallConductor()
{
	if (RideTrain < 0 || !LoadModel())
	{
		return false;
	}
	bCheckedThisRide = false;
	EndConductor();
	StartConductor();
	return Walk != EWalk::None;
}

void ULyudTrainPassenger::StartConductor()
{
	UWorld* World = GetWorld();
	ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	const ALyudTrain* T = R && RideTrain >= 0 ? R->GetTrain(RideTrain) : nullptr;
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	if (!World || !T || !Hero || !Model || T->NumSeats() == 0)
	{
		return;
	}
	// the car's span from its seats, the hero's place in it
	WalkLo = TNumericLimits<float>::Max();
	WalkHi = TNumericLimits<float>::Lowest();
	for (int32 I = 0; I < T->NumSeats(); ++I)
	{
		FTransform Seat;
		if (T->GetSeat(I, Seat))
		{
			const float A = AlongOf(Seat.GetLocation());
			WalkLo = FMath::Min(WalkLo, A);
			WalkHi = FMath::Max(WalkHi, A);
		}
	}
	const float At = AlongOf(Hero->GetActorLocation());
	LyudTicket::FModel::Walk(WalkLo, WalkHi, At, LyudPassengerImpl::StandOffCm, WalkFrom, WalkTo);
	bFromHi = WalkFrom == WalkHi;
	WalkD = WalkFrom;
	Conductor = Model->ConductorOn((long long)(UGameCalendar::NowIn(World).GetDate().GetTicks() / ETimespan::TicksPerDay));
	Walk = EWalk::Coming;
	// the body: a shared crowd look, only once its model is loaded (a body made on the spot stalls the frame)
	const FString ModelName = Conductor >= 0 ? LyudPassengerImpl::Str(Model->Conductors[(size_t)Conductor].Model) : FString();
	const FString Path = FString::Printf(TEXT("/Game/Characters/Humans/%s/SK_%s.SK_%s"), *ModelName, *ModelName, *ModelName);
	if (!ModelName.IsEmpty() && ULyudPrefetch::Ready(this, Path) && ULyudPrefetch::FolderReady(this, TEXT("/Game/Characters/Humans/Anims")))
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Body = World->SpawnActor<ANpcBody>(Hero->GetActorLocation(), FRotator::ZeroRotator, SP);
		if (Body && !Body->Setup(ModelName, FString()))
		{
			Body->Destroy();
			Body = nullptr;
		}
		if (Body)
		{
			Body->Tags.Add(TEXT("NoTalk"));
		}
	}
	UE_LOG(LogLyudPassenger, Display, TEXT("Passenger: conductor %s down the car (%.0f -> %.0f cm)%s"),
		Conductor >= 0 ? *LyudPassengerImpl::Str(Model->Conductors[(size_t)Conductor].Id) : TEXT("?"), WalkFrom, WalkTo, Body ? TEXT("") : TEXT(", no body (a voice)"));
}

void ULyudTrainPassenger::EndConductor()
{
	Walk = EWalk::None;
	if (Body)
	{
		Body->Destroy();
	}
	Body = nullptr;
}

void ULyudTrainPassenger::TickConductor(AHeroCharacter* Hero, float DeltaTime)
{
	UWorld* World = GetWorld();
	ULyudRailwaySubsystem* R = ULyudRailwaySubsystem::Get(this);
	const ALyudTrain* T = R && RideTrain >= 0 ? R->GetTrain(RideTrain) : nullptr;
	if (Walk == EWalk::None || !World || !T || !Model || Conductor < 0)
	{
		return;
	}
	const LyudTicket::FConductor& C = Model->Conductors[(size_t)Conductor];
	const double Now = World->GetTimeSeconds();
	const float Speed = Model->WalkCms;
	EPersonPose Pose = EPersonPose::Stand;
	float Pace = 0.f;
	if (Walk == EWalk::Coming)
	{
		// toward the hero as he is now (he may stand and move)
		const float At = AlongOf(Hero->GetActorLocation());
		WalkTo = FMath::Clamp(bFromHi ? At + LyudPassengerImpl::StandOffCm : At - LyudPassengerImpl::StandOffCm, WalkLo, WalkHi);
		const float Step = Speed * DeltaTime;
		if (FMath::Abs(WalkTo - WalkD) <= Step)
		{
			WalkD = WalkTo;
			Walk = EWalk::Asking;
			PhaseUntil = Now + LyudPassengerImpl::AskSeconds;
			Say(LyudPassengerImpl::Key(C.Name), LyudPassengerImpl::Key(C.Ask), (float)LyudPassengerImpl::AskSeconds + 0.3f);
			if (Body)
			{
				Body->Chatter(2.f);
			}
		}
		else
		{
			WalkD += WalkTo > WalkD ? Step : -Step;
			Pose = EPersonPose::Walk;
			Pace = Speed;
		}
	}
	else if (Walk == EWalk::Asking && Now >= PhaseUntil)
	{
		ULyudInventory* Inv = ULyudInventory::Get(this);
		const int32 Tickets = Inv ? Inv->Count(LyudPassengerImpl::Ticket) : 0;
		const int32 Money = Inv ? Inv->Rubles : 0;
		LyudTicket::FCheck Res = Model->Check(Conductor, Tickets, bPunchedThisRide, Offences, Money);
		ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
		if (Res.Verdict == LyudTicket::EVerdict::Fine && (!Inv || !Inv->Spend(Res.Fine)))
		{
			Res = Model->Check(Conductor, 0, false, Offences, 0);     // the purse came up short: put off at the next halt
		}
		else if (Res.Verdict == LyudTicket::EVerdict::Fine && Screen)
		{
			Screen->Toast(LyudText::Get(TEXT("rail.fine.paid"), { FStringFormatArg(Res.Fine) }));
		}
		if (Res.bTakeTicket && Inv && Inv->Remove(LyudPassengerImpl::Ticket, 1))
		{
			Inv->Add(LyudPassengerImpl::TicketPunched, 1);
			bPunchedThisRide = true;
			++Punched;
			if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
			{
				Sound->PlayEffect(TEXT("ticket_punch"), Hero->GetActorLocation(), 0.8f);
			}
		}
		if (LyudTicket::FModel::IsOffence(Res.Verdict))
		{
			++Offences;
		}
		bCheckedThisRide = true;
		const FText Line = Res.Verdict == LyudTicket::EVerdict::Fine ? LyudText::Get(LyudPassengerImpl::Str(Res.Line), { FStringFormatArg(Res.Fine) })
			: LyudPassengerImpl::Key(Res.Line);
		Say(LyudPassengerImpl::Key(C.Name), Line, (float)LyudPassengerImpl::AnswerSeconds);
		if (Body)
		{
			Body->Chatter(3.f);
		}
		Walk = EWalk::Answer;
		PhaseUntil = Now + LyudPassengerImpl::AnswerSeconds;
		UE_LOG(LogLyudPassenger, Display, TEXT("Passenger: check: verdict %d, fine %d, offences %d, punched %d"), (int32)Res.Verdict, Res.Fine, Offences, Punched);
	}
	else if (Walk == EWalk::Answer && Now >= PhaseUntil)
	{
		Walk = EWalk::Leaving;
	}
	else if (Walk == EWalk::Leaving)
	{
		// back the way he came, gone through the end door
		const float Step = Speed * DeltaTime;
		if (FMath::Abs(WalkFrom - WalkD) <= Step)
		{
			EndConductor();
			return;
		}
		WalkD += WalkFrom > WalkD ? Step : -Step;
		Pose = EPersonPose::Walk;
		Pace = Speed;
	}
	if (Body)
	{
		const double S = T->GetInfo().S;
		const FVector Dir = R->LineDir(S);
		FTransform Seat;
		const double FloorZ = T->GetSeat(0, Seat) ? Seat.GetLocation().Z : Hero->GetActorLocation().Z - 90.0;
		FVector At = R->LineToWorld(S, 0.0, 0.0) + Dir * WalkD;
		At.Z = FloorZ;
		float Yaw = 0.f;
		if (Pose == EPersonPose::Walk)
		{
			const bool bForward = (Walk == EWalk::Coming) == !bFromHi;
			Yaw = (bForward ? Dir : -Dir).Rotation().Yaw;
		}
		else
		{
			Yaw = (Hero->GetActorLocation() - At).GetSafeNormal2D().Rotation().Yaw;
		}
		Body->SetActorHiddenInGame(false);
		Body->Drive(At, Yaw, Pose, 0, Pace, 0.f, DeltaTime, true);
	}
}

// --- the subtitles (the city's screen-style page)

void ULyudTrainPassenger::Say(const FText& Title, const FText& Line, float Seconds)
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
	Page->SetPage(Title, Line, FText::GetEmpty());
	PageUntil = World->GetTimeSeconds() + Seconds;
}

void ULyudTrainPassenger::Unsay()
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

void ULyudTrainPassenger::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !LoadModel())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (!bBoxesBuilt && Now >= NextBoxTry)
	{
		NextBoxTry = Now + 2.0;
		BuildBoxes();
	}
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	TickBoxes(Hero, PC);
	TickTrain(Hero, DeltaTime);
	if (Page && Now >= PageUntil)
	{
		Unsay();
	}
}

FString ULyudTrainPassenger::Report() const
{
	return FString::Printf(TEXT("Passenger: %d ticket boxes, in train %d, conductor %d walking %d at %.0f cm, punched this ride %d, offences %d, punched %d"),
		Boxes.Num(), RideTrain, Conductor, (int32)Walk, WalkD, bPunchedThisRide ? 1 : 0, Offences, Punched);
}

static FAutoConsoleCommandWithWorldAndArgs GRailConductor(TEXT("lyud.Rail.Conductor"),
	TEXT("lyud.Rail.Conductor: the day's conductor walks down the car to the hero now (he must be inside a car)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudTrainPassenger* P = ULyudTrainPassenger::Get(World))
		{
			UE_LOG(LogLyudPassenger, Display, TEXT("lyud.Rail.Conductor: %s"), P->CallConductor() ? TEXT("coming") : TEXT("not in a car"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GRailPassenger(TEXT("lyud.Rail.Passenger"),
	TEXT("lyud.Rail.Passenger: the ticket boxes, the ride, the conductor, offences"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudTrainPassenger* P = ULyudTrainPassenger::Get(World))
		{
			UE_LOG(LogLyudPassenger, Display, TEXT("%s"), *P->Report());
		}
	}));

static LyudDev::FAutoAction GRailDevConductor(TEXT("rail.conductor"), TEXT("transport"), TEXT("dev.action.rail.conductor"), [](UWorld* W)
{
	if (ULyudTrainPassenger* P = ULyudTrainPassenger::Get(W))
	{
		P->CallConductor();
	}
});
