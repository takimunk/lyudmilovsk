// The cable-car stations' halls, v3: the dressing, lamps and people of Content/Transport/halls.json (blender/transport/halls.py).
// docs/transport.md "Halls v3".

#include "Transport/LyudStationHall.h"

#include "Transport/LyudCableway.h"
#include "Transport/LyudTransport.h"
#include "Transport/LyudFrontDoor.h"
#include "Audio/LyudSoundscape.h"
#include "FrontDoor.h"
#include "SLyudFrontDoorWidgets.h"
#include "StationLife.h"
#include "People/CounterService.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Weather/LyudWeather.h"
#include "UI/LyudGameScreen.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudText.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static TAutoConsoleVariable<int32> CVarHallPeople(TEXT("lyud.Transport.HallPeople"), 1,
	TEXT("1: the stations' halls have their staff and waiting passengers near the player (docs/transport.md 'Halls v3'); 0: none."));

namespace
{
	// how near the player must be (cm, to the hall's middle) for its lamps, and for its people (with a margin to let go)
	constexpr float LightsOn = 4500.f;
	constexpr float PeopleOn = 3500.f;
	constexpr float PeopleOff = 5000.f;
	// the hall's people only while the player is about level with it (the tower's stair below, the platforms beside)
	constexpr float LevelBand = 1200.f;

	TSharedPtr<FJsonObject> AllHalls()
	{
		static TSharedPtr<FJsonObject> All;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/halls.json"))))
			{
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), All);
			}
		}
		return All;
	}

	TArray<FLyudShopGood> StockOf(const FString& Name)
	{
		// lobby.json "stock" (the vending machines' presets): the buffet sells from its own list there
		TArray<FLyudShopGood> Out;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/lobby.json"))) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return Out;
		}
		const TSharedPtr<FJsonObject>* Stock = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (Root->TryGetObjectField(TEXT("stock"), Stock) && (*Stock)->TryGetArrayField(Name, List))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FLyudShopGood G;
				G.Item = FName(*O->GetStringField(TEXT("item")));
				G.Price = O->HasField(TEXT("price")) ? (int32)O->GetNumberField(TEXT("price")) : 0;
				G.Count = O->HasField(TEXT("count")) ? (int32)O->GetNumberField(TEXT("count")) : 1;
				Out.Add(G);
			}
		}
		return Out;
	}

	FVector Vec(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		FVector V = FVector::ZeroVector;
		if (O.IsValid() && O->TryGetArrayField(Key, A))
		{
			for (int32 I = 0; I < A->Num() && I < 3; I++)
			{
				V[I] = (*A)[I]->AsNumber();
			}
		}
		return V;
	}

	FString ModelPath(const FString& Model)
	{
		// as PeopleLife.cpp's PeopleModelPath: "Dir/Name" -> /Game/Characters/Humans/Dir/Name/SK_Name
		const FString Name = FPaths::GetCleanFilename(Model);
		return FString::Printf(TEXT("/Game/Characters/Humans/%s/SK_%s.SK_%s"), *Model, *Name, *Name);
	}

	bool ModelExists(const FString& Model)
	{
		return !Model.IsEmpty() && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(ModelPath(Model)));
	}

	UStaticMesh* LoadPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		const FString Name = FPaths::GetCleanFilename(Path);
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

ALyudStationHall::ALyudStationHall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

TSharedPtr<FJsonObject> ALyudStationHall::HallData(const FString& Style)
{
	const TSharedPtr<FJsonObject> All = AllHalls();
	const TSharedPtr<FJsonObject>* H = nullptr;
	return All.IsValid() && All->TryGetObjectField(Style.ToLower(), H) ? *H : nullptr;
}

bool ALyudStationHall::HasDressing(const FString& Style)
{
	const TSharedPtr<FJsonObject> H = HallData(Style);
	FString Path;
	return H.IsValid() && H->TryGetStringField(TEXT("detail"), Path) && LoadPath(Path) != nullptr;
}

void ALyudStationHall::Setup(ALyudCableway* InWay, int32 InStation, const FString& InStyle, const FTransform& C, const TSharedPtr<FJsonObject>& Hall)
{
	Way = InWay;
	Station = InStation;
	Style = InStyle;
	Frame = C;
	Seed = GetTypeHash(InStyle) ^ (uint32)(InStation * 7919);
	// (spawned at C: the root is static and stays put)
	if (!Hall.IsValid())
	{
		return;
	}
	// the dressing: the furniture (no collision, Nanite) and the hidden cores that stop the walkers at counters and tables
	FString Path;
	if (Hall->TryGetStringField(TEXT("detail"), Path))
	{
		if (UStaticMesh* M = LoadPath(Path))
		{
			Detail = NewObject<UStaticMeshComponent>(this, TEXT("HallDetail"));
			Detail->SetupAttachment(RootComponent);
			Detail->SetMobility(EComponentMobility::Static);
			Detail->SetStaticMesh(M);
			Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Detail->bAffectDistanceFieldLighting = false;
			Detail->RegisterComponent();
		}
	}
	if (Hall->TryGetStringField(TEXT("mesh"), Path))
	{
		if (UStaticMesh* M = LoadPath(Path))
		{
			Cores = NewObject<UStaticMeshComponent>(this, TEXT("HallCores"));
			Cores->SetupAttachment(RootComponent);
			Cores->SetMobility(EComponentMobility::Static);
			Cores->SetStaticMesh(M);
			Cores->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Cores->SetCollisionResponseToAllChannels(ECR_Block);
			Cores->SetCastShadow(false);
			Cores->SetHiddenInGame(true);
			Cores->bAffectDistanceFieldLighting = false;
			Cores->RegisterComponent();
		}
	}
	// the lamps: warm, shadowless, off until the player comes near
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	FVector Sum = FVector::ZeroVector;
	if (Hall->TryGetArrayField(TEXT("lamps"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetupAttachment(RootComponent);
			L->SetMobility(EComponentMobility::Movable);
			L->SetRelativeLocation(Vec(O, TEXT("at")));
			L->SetIntensityUnits(ELightUnits::Lumens);
			L->SetIntensity(O->HasField(TEXT("lumens")) ? O->GetNumberField(TEXT("lumens")) : 800.f);
			L->SetAttenuationRadius(O->HasField(TEXT("radius")) ? O->GetNumberField(TEXT("radius")) : 600.f);
			L->SetSourceRadius(6.f);
			const FColor Col = FColor::FromHex(O->HasField(TEXT("colour")) ? O->GetStringField(TEXT("colour")) : TEXT("#FFD9A8"));
			L->SetLightColor(FLinearColor(Col));
			L->SetCastShadows(false);
			L->SetVisibility(false);
			L->RegisterComponent();
			Lights.Add(L);
			Sum += Vec(O, TEXT("at"));
		}
	}
	Middle = C.TransformPosition(Lights.Num() ? FVector(Sum.X / Lights.Num(), Sum.Y / Lights.Num(), 150.f) : FVector(0, 0, 150.f));
	// the passengers' places
	auto AddSlots = [&](const TCHAR* Field, bool bSeat)
	{
		if (!Hall->TryGetArrayField(Field, Arr))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FSlot S;
			S.At = C.TransformPosition(Vec(O, TEXT("at")));
			S.Yaw = C.Rotator().Yaw + O->GetNumberField(TEXT("yaw"));
			if (bSeat)
			{
				S.SeatHeight = O->HasField(TEXT("height")) ? O->GetNumberField(TEXT("height")) : 45.f;
				S.Kind = O->HasField(TEXT("table")) ? 1 : 0;
			}
			else
			{
				const FString K = O->HasField(TEXT("kind")) ? O->GetStringField(TEXT("kind")) : FString();
				S.Kind = K == TEXT("counter") ? 2 : K == TEXT("rail") ? 3 : K == TEXT("window") ? 4 : K == TEXT("diagram") ? 5
					: K == TEXT("read") ? 6 : 7;
			}
			Slots.Add(S);
		}
	};
	AddSlots(TEXT("seats"), true);
	AddSlots(TEXT("stands"), false);
	// the staff's places
	if (Hall->TryGetArrayField(TEXT("staff"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FPerson P;
			P.Role = FName(*O->GetStringField(TEXT("role")));
			P.At = C.TransformPosition(Vec(O, TEXT("at")));
			P.Yaw = C.Rotator().Yaw + O->GetNumberField(TEXT("yaw"));
			P.Pose = (uint8)(P.Role == TEXT("attendant") ? EPersonPose::Look : EPersonPose::Stand);
			StaffPlan.Add(P);
		}
	}
	const TSharedPtr<FJsonObject>* Counter = nullptr;
	if (Hall->TryGetObjectField(TEXT("counter"), Counter))
	{
		CounterAt = C.TransformPosition(Vec(*Counter, TEXT("at")));
		(*Counter)->TryGetStringField(TEXT("shop"), Shop);
	}
	// the models (halls.py: away townsfolk, each with a pool model to fall back on)
	const TSharedPtr<FJsonObject>* Models = nullptr;
	if (Hall->TryGetObjectField(TEXT("models"), Models))
	{
		auto First = [&](const TCHAR* Role, int32 Salt) -> FString
		{
			const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
			if ((*Models)->TryGetArrayField(Role, L) && L->Num())
			{
				const TSharedPtr<FJsonObject> O = (*L)[Salt % L->Num()]->AsObject();
				const FString Body = O->GetStringField(TEXT("body"));
				return ModelExists(Body) ? Body : O->GetStringField(TEXT("mesh"));
			}
			return FString();
		};
		for (FPerson& P : StaffPlan)
		{
			P.Model = First(*P.Role.ToString(), 0);
		}
	}
	for (FPerson& P : StaffPlan)
	{
		if (P.Model.IsEmpty() || !ModelExists(P.Model))
		{
			P.Model = PickModel(P.Role == TEXT("barista") ? 1 : 2);
		}
	}
	// stagger the halls' ticks (they only look at the player's distance until near)
	PrimaryActorTick.TickInterval = 0.25f + 0.03f * (InStation % 5);
}

FString ALyudStationHall::PickModel(int32 Salt) const
{
	// a passenger's body: one of the hall's away townsfolk (their own model when imported), else a pool model
	const TSharedPtr<FJsonObject> Hall = HallData(Style);
	const TSharedPtr<FJsonObject>* Models = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
	if (Hall.IsValid() && Hall->TryGetObjectField(TEXT("models"), Models) && (*Models)->TryGetArrayField(TEXT("passengers"), L) && L->Num())
	{
		const TSharedPtr<FJsonObject> O = (*L)[(uint32)(Salt + Seed) % (uint32)L->Num()]->AsObject();
		const FString Body = O->GetStringField(TEXT("body"));
		if (ModelExists(Body))
		{
			return Body;
		}
		const FString Mesh = O->GetStringField(TEXT("mesh"));
		if (ModelExists(Mesh))
		{
			return Mesh;
		}
	}
	static const TCHAR* Pool[] = { TEXT("Crowd_11"), TEXT("Crowd_12"), TEXT("Crowd_13") };
	return Pool[(uint32)(Salt + Seed) % 3];
}

bool ALyudStationHall::StaffOnDuty() const
{
	// the station life's rota: the attendant opens the hall and locks it at night
	if (const ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this); T && T->GetLife())
	{
		return T->GetLife()->OnShift(Station, "attendant", T->LifeWeekday(), T->LifeHour()) >= 0;
	}
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const float H = Now.GetHour() + Now.GetMinute() / 60.f;
	return H >= 6.f;
}

bool ALyudStationHall::LifeOpen() const
{
	const ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this);
	return !T || !T->GetLife() || T->GetLife()->IsOpen(T->LifeHour());
}

LyudTransport::FHallMood ALyudStationHall::Mood() const
{
	LyudTransport::FHallMood M;
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
	int32 Weekday = 0;
	if (Life)
	{
		M.Hour = FMath::Fmod(Tr->LifeHour(), 24.f);
		Weekday = Tr->LifeWeekday();
		M.bWindy = Life->LastWind() >= Life->WindWarn;
		M.bStopped = Life->IsWindStopped();
		M.bBuffet = Life->OnShift(Station, "barista", Weekday, Tr->LifeHour()) >= 0;
	}
	else
	{
		const FDateTime Now = UGameCalendar::NowIn(GetWorld());
		M.Hour = Now.GetHour() + Now.GetMinute() / 60.f;
		Weekday = ((int32)Now.GetDayOfWeek()) % 7;   // EDayOfWeek: 0 Monday
		M.bBuffet = StaffOnDuty();
	}
	M.bWeekend = Weekday >= 5;
	if (const ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		M.bRain = W->GetState().PrecipRate > 0.1f;
	}
	return M;
}

int32 ALyudStationHall::PickSlot(const FString& Key, int32 Age, bool bOut, const TFunctionRef<bool(const FVector&)>& Unseen, uint32 Salt) const
{
	std::vector<LyudTransport::FHallPlace> Places;
	Places.reserve(Slots.Num());
	for (const FSlot& S : Slots)
	{
		LyudTransport::FHallPlace P;
		P.Kind = (LyudTransport::EHallSpot)S.Kind;
		P.bTaken = S.Who >= 0;
		P.Queue = S.Line > 0 ? S.Line : 0;
		P.bOpen = !P.bTaken && S.Kind != (uint8)LyudTransport::EHallSpot::Queue && Unseen(S.At);   // the line fills by itself
		Places.push_back(P);
	}
	return LyudTransport::HallPick(Places, TCHAR_TO_UTF8(*Key), Age, bOut, Mood(), Salt);
}

static EPersonPose PersonPose(LyudTransport::EHallPose P)
{
	switch (P)
	{
	case LyudTransport::EHallPose::Talk: return EPersonPose::Talk;
	case LyudTransport::EHallPose::Look: return EPersonPose::Look;
	case LyudTransport::EHallPose::Sit: return EPersonPose::Sit;
	case LyudTransport::EHallPose::Eat: return EPersonPose::Eat;
	case LyudTransport::EHallPose::Read: return EPersonPose::Read;
	case LyudTransport::EHallPose::Work: return EPersonPose::Work;
	default: return EPersonPose::Stand;
	}
}

uint8 ALyudStationHall::PoseAt(int32 K, int32 Age, float Roll) const
{
	if (!Slots.IsValidIndex(K))
	{
		return (uint8)EPersonPose::Stand;
	}
	EPersonPose P = PersonPose(LyudTransport::HallPose(Age, (LyudTransport::EHallSpot)Slots[K].Kind, Mood(), Roll));
	// a seat holds a seated pose (Sit, Read, Eat play seated; a word with the neighbour sits as Sit); a standing place never sits
	if (Slots[K].SeatHeight > 0.f && P != EPersonPose::Read && P != EPersonPose::Eat)
	{
		P = EPersonPose::Sit;
	}
	else if (Slots[K].SeatHeight <= 0.f && P == EPersonPose::Sit)
	{
		P = EPersonPose::Stand;
	}
	// in the counter's line (the counter place too) they stand and wait their turn; the barista does the talking
	if (Slots[K].Line >= 0 && P == EPersonPose::Talk)
	{
		P = EPersonPose::Stand;
	}
	return (uint8)P;
}

void ALyudStationHall::TickStaff()
{
	// every 20..45 s each of the staff changes a little: the barista works the counter while anyone waits at it, else wipes,
	// looks about, reads late; the attendant keeps the logbook in a quiet hour, speaks to the hall at a wind stop
	const LyudTransport::FHallMood M = Mood();
	int32 Customers = 0, Waiting = 0;
	for (const FPerson& P : People)
	{
		if (P.Role == TEXT("passenger") && P.Body.IsValid() && P.GoneSince < 0.0)
		{
			Waiting++;
			if (Slots.IsValidIndex(P.Slot) && (Slots[P.Slot].Kind == (uint8)LyudTransport::EHallSpot::Counter
				|| Slots[P.Slot].Kind == (uint8)LyudTransport::EHallSpot::Queue))
			{
				Customers++;
			}
		}
	}
	for (FPerson& P : People)
	{
		if (P.Role == TEXT("passenger") || !P.Body.IsValid())
		{
			continue;
		}
		const bool bBarista = P.Role == TEXT("barista");
		// a queue forming: the barista gets to work at once
		const bool bNow = bBarista && Customers > 0 && P.BasePose != (uint8)EPersonPose::Work && P.BasePose != (uint8)EPersonPose::Talk;
		if (Clock < P.NextRoutine && !bNow)
		{
			continue;
		}
		const float Roll = LyudTransport::LifeHash(TCHAR_TO_UTF8(*P.Role.ToString()), (uint32)(Clock * 10.f) + Seed);
		P.NextRoutine = Clock + 20.f + 25.f * Roll;
		const LyudTransport::EHallPose HP = bBarista ? LyudTransport::BaristaPose(Customers, M, Roll) : LyudTransport::AttendantPose(Waiting, M, Roll);
		P.BasePose = (uint8)PersonPose(HP);
		if (P.TalkLeft <= 0.f)
		{
			P.Pose = P.BasePose;
		}
	}
}

FString ALyudStationHall::LifeModel(const FString& Key, const FString& Mesh, int32& OutPid) const
{
	// their own face: the model the town shows them by (their body, or the stand-in nobody else wears), else the pool's
	OutPid = -1;
	if (UPeopleLifeSubsystem* PeopleLife = UPeopleLifeSubsystem::Get(GetWorld()))
	{
		OutPid = PeopleLife->FindPerson(Key);
		const FString Shown = OutPid >= 0 ? PeopleLife->ShownModel(OutPid) : FString();
		if (ModelExists(Shown))
		{
			return Shown;
		}
	}
	return ModelExists(Mesh) ? Mesh : FString();
}

void ALyudStationHall::Unpresent(FPerson& P)
{
	if (P.Pid >= 0)
	{
		if (UPeopleLifeSubsystem* PeopleLife = UPeopleLifeSubsystem::Get(GetWorld()))
		{
			PeopleLife->SetPresentedExternally(P.Pid, false);
		}
		P.Pid = -1;
	}
}

int32 ALyudStationHall::WantPassengers() const
{
	if (const ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this); T && T->GetLife())
	{
		// the riders come by the station life (SyncLife); a passer-by or two besides (in for the buffet, out of the rain)
		if (!LifeOpen())
		{
			return 0;
		}
		// (the rain brings more in: docs/transport.md "Hall moods")
		const int32 H = (int32)T->LifeHour();
		return FMath::Min(LyudTransport::HallFiller(Mood(), Seed + (uint32)T->LifeWeekday() * 7u + (uint32)H * 131u), Slots.Num() / 2);
	}
	// by the hour: the morning and the evening rush, a quiet afternoon, almost nobody at night
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const int32 H = Now.GetHour();
	int32 N = H < 6 ? 0 : H < 7 ? 2 : H < 10 ? 7 : H < 17 ? 4 : H < 20 ? 7 : H < 23 ? 3 : 1;
	N += (int32)((Seed + Now.GetDay() * 31 + H) % 3) - 1;
	N += Mood().bRain && H >= 6 ? 3 : 0;
	return FMath::Clamp(N, 0, FMath::Min(9, Slots.Num()));
}

bool ALyudStationHall::SpawnPerson(FPerson& P)
{
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANpcBody* B = GetWorld()->SpawnActor<ANpcBody>(P.At, FRotator(0.f, P.Yaw, 0.f), SP);
	if (!B)
	{
		return false;
	}
	if (!B->Setup(P.Model, FString()))
	{
		B->Destroy();
		return false;
	}
	B->Tags.Add(TEXT("StationHall"));
	B->Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	B->Drive(P.At, P.Yaw, (EPersonPose)P.Pose, 0, 0.f, P.SeatHeight, 0.f, true);
	P.Body = B;
	Bodies.Add(B);
	// the counter service knows them (who serves, who is whom): the barista sells through it, owners are found by it
	if (ULyudCounterService* CounterService = ULyudCounterService::Get(this))
	{
		CounterService->RegisterBody(B, P.Pid, P.Role == TEXT("barista"));
	}
	if (P.Pid >= 0)
	{
		if (UPeopleLifeSubsystem* PeopleLife = UPeopleLifeSubsystem::Get(GetWorld()))
		{
			PeopleLife->SetPresentedExternally(P.Pid, true);
		}
	}
	P.BaseYaw = P.Yaw;
	P.BasePose = P.Pose;
	return true;
}

void ALyudStationHall::SpawnPeople()
{
	bPeopleOn = true;
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
	bDutyAtSpawn = StaffOnDuty();
	if (bDutyAtSpawn)
	{
		for (const FPerson& Plan : StaffPlan)
		{
			FPerson P = Plan;
			if (Life)
			{
				// whoever's shift it is (the barista's buffet opens at 6:30)
				P.StaffIndex = Life->OnShift(Station, TCHAR_TO_UTF8(*P.Role.ToString()), Tr->LifeWeekday(), Tr->LifeHour());
				if (P.StaffIndex < 0)
				{
					continue;
				}
				const LyudTransport::FLifeStaff& St = Life->Staff[P.StaffIndex];
				const FString M = LifeModel(UTF8_TO_TCHAR(St.Key.c_str()), UTF8_TO_TCHAR(St.Mesh.c_str()), P.Pid);
				if (!M.IsEmpty())
				{
					P.Model = M;
				}
			}
			if (SpawnPerson(P))
			{
				People.Add(P);
			}
		}
	}
	// the passengers: places by the hall's mood (the hour, the weather; docs/transport.md "Hall moods"), each their own; the
	// line's places behind the counter fill by the line itself
	const int32 Want = WantPassengers();
	FRandomStream R(Seed ^ (uint32)(GetWorld()->GetTimeSeconds() * 10.0));
	for (FSlot& S : Slots)
	{
		S.Who = -1;
	}
	const uint32 Visit = R.GetUnsignedInt();
	for (int32 Made = 0, Tries = 0; Made < Want && Tries < Want + 4; Tries++)
	{
		const FString Key = FString::Printf(TEXT("hall_%d_%u_%d"), Station, Visit, Tries);
		const int32 Age = 18 + (int32)(LyudTransport::LifeHash(TCHAR_TO_UTF8(*Key), 3u) * 60.f);
		const int32 K = PickSlot(Key, Age, false, [](const FVector&) { return true; }, Visit);
		if (K < 0)
		{
			break;
		}
		FPerson P;
		P.Role = TEXT("passenger");
		P.Slot = K;
		P.Age = Age;
		P.At = Slots[K].At;
		P.Yaw = Slots[K].Yaw;
		P.SeatHeight = Slots[K].SeatHeight;
		P.Pose = PoseAt(K, Age, R.FRand());
		P.Model = PickModel(K * 13 + Made);
		if (SpawnPerson(P))
		{
			Slots[K].Who = People.Add(P);
			Made++;
		}
	}
	ChurnTimer = R.FRandRange(35.f, 80.f);
	LifeTimer = 0.f;
}

void ALyudStationHall::ClearPeople()
{
	bPeopleOn = false;
	for (FPerson& P : People)
	{
		Unpresent(P);
	}
	ULyudCounterService* CounterService = ULyudCounterService::Get(this);
	for (ANpcBody* B : Bodies)
	{
		if (IsValid(B))
		{
			if (CounterService)
			{
				CounterService->UnregisterBody(B);
			}
			B->Destroy();
		}
	}
	Bodies.Reset();
	People.Reset();
	for (FSlot& S : Slots)
	{
		S.Who = -1;
	}
	if (Prompted >= 0)
	{
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			UI->SetPrompt(this, FText::GetEmpty());
		}
		Prompted = -1;
	}
}

void ALyudStationHall::Churn(const FVector& Cam, const FVector& Look)
{
	// one passenger goes (out of the player's sight) and another comes to a free place (also unseen), the count drifting
	// toward the hour's
	auto Unseen = [&](const FVector& P)
	{
		const FVector D = P + FVector(0, 0, 100.f) - Cam;
		return FVector::DotProduct(D.GetSafeNormal(), Look) < 0.35f || D.Size() > 2500.f;
	};
	int32 Have = 0;
	for (const FPerson& P : People)
	{
		Have += P.Role == TEXT("passenger") && P.Run < 0 && P.Body.IsValid() ? 1 : 0;
	}
	const int32 Want = WantPassengers();
	FRandomStream R(Seed ^ (uint32)(Clock * 100.f));
	if (Have >= Want || R.FRand() < 0.5f)
	{
		for (int32 Tries = 0; Tries < 6; Tries++)
		{
			const int32 I = R.RandRange(0, People.Num() - 1);
			if (People.IsValidIndex(I) && People[I].Role == TEXT("passenger") && People[I].Run < 0 && People[I].Body.IsValid() && Unseen(People[I].At))
			{
				People[I].Body->Destroy();
				People[I].Body = nullptr;
				if (Slots.IsValidIndex(People[I].Slot))
				{
					Slots[People[I].Slot].Who = -1;
				}
				Have--;
				break;
			}
		}
	}
	if (Have < Want)
	{
		const FString Key = FString::Printf(TEXT("hall_%d_%u"), Station, R.GetUnsignedInt());
		const int32 Age = 18 + (int32)(LyudTransport::LifeHash(TCHAR_TO_UTF8(*Key), 3u) * 60.f);
		const int32 K = PickSlot(Key, Age, false, Unseen, (uint32)Clock);
		if (K >= 0)
		{
			FPerson P;
			P.Role = TEXT("passenger");
			P.Slot = K;
			P.Age = Age;
			P.At = Slots[K].At;
			P.Yaw = Slots[K].Yaw;
			P.SeatHeight = Slots[K].SeatHeight;
			P.Pose = PoseAt(K, Age, R.FRand());
			P.Model = PickModel(K * 17 + (int32)Clock);
			if (SpawnPerson(P))
			{
				Slots[K].Who = People.Add(P);
			}
		}
	}
	// the dead entries (gone people) out of the list now and then, the slots' indices kept right
	if (People.Num() > 24)
	{
		TArray<FPerson> Kept;
		for (FSlot& S : Slots)
		{
			S.Who = -1;
		}
		for (const FPerson& P : People)
		{
			if (P.Body.IsValid())
			{
				const int32 I = Kept.Add(P);
				if (Slots.IsValidIndex(P.Slot))
				{
					Slots[P.Slot].Who = I;
				}
			}
		}
		People = MoveTemp(Kept);
		// and the destroyed bodies out of the kept list (it grew by one for every passenger that ever came)
		Bodies.RemoveAll([](const TObjectPtr<ANpcBody>& B) { return !IsValid(B.Get()); });
	}
}

void ALyudStationHall::SyncLife(const FVector& Cam, const FVector& Look)
{
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
	if (!Life)
	{
		return;
	}
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	if (Life->Day != GreetedDay)
	{
		// a new day: the attendant greets her regulars again (once a day each)
		GreetedDay = Life->Day;
		GreetedRegulars.Reset();
	}
	auto Unseen = [&](const FVector& P)
	{
		const FVector D = P + FVector(0, 0, 100.f) - Cam;
		return FVector::DotProduct(D.GetSafeNormal(), Look) < 0.35f || D.Size() > 2500.f;
	};
	// the staff: the next shift takes over (out of the player's sight)
	for (FPerson& P : People)
	{
		if (P.Role == TEXT("passenger") || !P.Body.IsValid() || P.StaffIndex < 0)
		{
			continue;
		}
		const int32 Want = Life->OnShift(Station, TCHAR_TO_UTF8(*P.Role.ToString()), Tr->LifeWeekday(), Tr->LifeHour());
		if (Want != P.StaffIndex && Unseen(P.At))
		{
			P.Body->Destroy();
			P.Body = nullptr;
			Unpresent(P);
			if (Want >= 0)
			{
				const LyudTransport::FLifeStaff& St = Life->Staff[Want];
				P.StaffIndex = Want;
				P.bGreeted = false;
				const FString M = LifeModel(UTF8_TO_TCHAR(St.Key.c_str()), UTF8_TO_TCHAR(St.Mesh.c_str()), P.Pid);
				if (!M.IsEmpty())
				{
					P.Model = M;
				}
				SpawnPerson(P);
			}
		}
	}
	// the riders: those waiting here before the platform and those walking out from a cabin
	std::vector<int> Here;
	Life->InHall(Station, Here);
	TSet<int32> Want;
	for (const int I : Here)
	{
		Want.Add(I);
	}
	TSet<int32> Have;
	for (FPerson& P : People)
	{
		if (P.Run < 0 || !P.Body.IsValid())
		{
			continue;
		}
		Have.Add(P.Run);
		if (Want.Contains(P.Run))
		{
			P.GoneSince = -1.0;
			continue;
		}
		// gone to the platform, or out into the town: when the player doesn't see (or after a while anyway)
		if (P.GoneSince < 0.0)
		{
			P.GoneSince = Now;
		}
		if (Unseen(P.At) || Now - P.GoneSince > 25.0)
		{
			P.Body->Destroy();
			P.Body = nullptr;
			Unpresent(P);
			if (Slots.IsValidIndex(P.Slot))
			{
				Slots[P.Slot].Who = -1;
			}
		}
	}
	const bool bBuffet = Life->OnShift(Station, "barista", Tr->LifeWeekday(), Tr->LifeHour()) >= 0;
	for (const int I : Here)
	{
		if (Have.Contains(I))
		{
			continue;
		}
		const LyudTransport::FLifeRun& R = Life->Today[I];
		const LyudTransport::FLifeTrip& T = Life->Trips[R.Trip];
		const LyudTransport::FLifeRider& Rd = Life->Riders[T.Rider];
		const bool bOut = R.Phase == LyudTransport::ELifePhase::Out;
		// a place: the counter's line in a rush, else by the mood and who they are (a child to the window, the old to a
		// seat and the paper; walking out: a standing place, a look at the window or the diagram)
		int32 K = -1;
		const uint32 H0 = GetTypeHash(FString(UTF8_TO_TCHAR(Rd.Key.c_str())));
		// the morning rush (and the evening one, and a wind stop's tea): most of them queue for a coffee first (behind whoever
		// is at the counter, never before the hero)
		const LyudTransport::FHallMood M = Mood();
		const bool bQueueHour = Tr->LifeHour() < 10.5f || M.IsRush() || M.bStopped;
		if (!bOut && bBuffet && LineSlots.Num() > 1 && bQueueHour && LyudTransport::LifeHash(Rd.Key, 211u + (uint32)I) < 0.6f)
		{
			TArray<int32> State;
			LineState(State);
			const std::vector<int> V(State.GetData(), State.GetData() + State.Num());
			const int J = LyudFrontDoor::JoinSpot(V);
			if (J >= 0 && Unseen(Slots[LineSlots[J]].At))
			{
				K = LineSlots[J];
			}
		}
		if (K < 0)
		{
			K = PickSlot(UTF8_TO_TCHAR(Rd.Key.c_str()), Rd.Age, bOut, Unseen, (uint32)Life->Day * 17u + (uint32)I);
		}
		if (K < 0)
		{
			continue;       // full, or every free place in sight: next time
		}
		FPerson P;
		P.Role = TEXT("passenger");
		P.Run = I;
		P.Slot = K;
		P.At = Slots[K].At;
		P.Yaw = Slots[K].Yaw;
		P.SeatHeight = Slots[K].SeatHeight;
		P.Age = Rd.Age;
		P.Pose = PoseAt(K, Rd.Age, LyudTransport::LifeHash(Rd.Key, 53u + (uint32)I));
		P.Model = LifeModel(UTF8_TO_TCHAR(Rd.Key.c_str()), UTF8_TO_TCHAR(Rd.Mesh.c_str()), P.Pid);
		if (P.Model.IsEmpty())
		{
			P.Model = PickModel(K * 13 + I);
			P.Pid = -1;
		}
		if (!SpawnPerson(P))
		{
			continue;
		}
		Slots[K].Who = People.Add(P);
		// off a cabin: the attendant looks up at who comes in
		if (bOut)
		{
			for (FPerson& A : People)
			{
				if (A.Role == TEXT("attendant") && A.Body.IsValid() && A.TalkLeft <= 0.f && FVector::Dist(A.At, P.At) < 1500.f)
				{
					A.TalkLeft = 4.f;
					A.Yaw = (P.At - A.At).Rotation().Yaw;
					A.Pose = (uint8)EPersonPose::Look;
					break;
				}
			}
		}
		// a regular: the attendant turns to them for a word (once a day each)
		if (Rd.bRegular && !bOut && !GreetedRegulars.Contains(T.Rider))
		{
			for (FPerson& A : People)
			{
				if (A.Role == TEXT("attendant") && A.Body.IsValid() && A.TalkLeft <= 0.f && FVector::Dist(A.At, P.At) < 1200.f)
				{
					GreetedRegulars.Add(T.Rider);
					A.TalkLeft = 6.f;
					A.Yaw = (P.At - A.At).Rotation().Yaw;
					A.Pose = (uint8)EPersonPose::Talk;
					if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
					{
						Talk->SayLine(A.Body.Get(), FString::Printf(TEXT("line.transport.attendant.regular.%d"), 1 + (int32)(H0 % 4u)));
					}
					break;
				}
			}
		}
	}
}

void ALyudStationHall::TickBarks(float DeltaTime, const FVector& Cam)
{
	// the people waiting say a word now and then while the player is in the hall: the wind, the cabins, the day
	BarkTimer -= DeltaTime;
	if (BarkTimer > 0.f || FVector::Dist(Cam, Middle) > 1500.f)
	{
		return;
	}
	BarkTimer = 35.f + (float)((Seed + (uint32)Clock * 7u) % 40u);
	const FString What = UTF8_TO_TCHAR(LyudTransport::HallBark(Mood()));   // the stop, the wind, the rain, the hour
	TArray<FPerson*> Can;
	for (FPerson& P : People)
	{
		if (P.Role == TEXT("passenger") && P.Body.IsValid() && P.GoneSince < 0.0 && FVector::Dist(P.At, Cam) < 1400.f)
		{
			Can.Add(&P);
		}
	}
	if (Can.Num() == 0)
	{
		return;
	}
	FPerson* P = Can[(int32)((Seed + (uint32)Clock) % (uint32)Can.Num())];
	if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
	{
		Talk->SayLine(P->Body.Get(), FString::Printf(TEXT("line.transport.wait.%s.%d"), *What, 1 + (int32)(((uint32)Clock / 3u) % 4u)));
	}
}

void ALyudStationHall::SetLights(bool bOn)
{
	bLightsOn = bOn;
	for (UPointLightComponent* L : Lights)
	{
		if (L)
		{
			L->SetVisibility(bOn);
		}
	}
}

void ALyudStationHall::TickPrompt(float DeltaTime)
{
	// the barista sells over the counter (T) and pours a regular's usual (E); the attendant shows the route diagram (E), and at
	// the front door gives the travel card first; the punch on the desk (E) stamps the card; the lost and found shelf (E): the
	// attendant hands over what lies there. Each greets the player once a visit.
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ULyudFrontDoor* FrontDoor = ULyudFrontDoor::Get(this);
	int32 Best = -1;
	FPerson* BestP = nullptr;
	FPerson* Attendant = nullptr;
	if (PC && UI && !UI->IsOpen() && Pawn->IsA<AHeroCharacter>() && !Pawn->IsHidden())
	{
		const FVector Eye = Pawn->GetPawnViewLocation();
		const FVector Look = PC->GetControlRotation().Vector();
		const FVector Feet = Pawn->GetActorLocation();
		for (FPerson& P : People)
		{
			if (P.Role == TEXT("passenger") || !P.Body.IsValid())
			{
				continue;
			}
			Attendant = P.Role == TEXT("attendant") ? &P : Attendant;
			const float Dist = FVector::Dist2D(Feet, P.At);
			const FVector To = (P.At + FVector(0, 0, 140.f) - Eye).GetSafeNormal();
			const bool bLooks = FVector::DotProduct(Look, To) > 0.6f;
			if (!P.bGreeted && Dist < 600.f && bLooks && FMath::Abs(Feet.Z - P.At.Z) < 250.f)
			{
				P.bGreeted = true;
				if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
				{
					const int32 N = 1 + (int32)((Seed + (uint32)Clock) % 3);
					// a regular of the buffet (he has a usual here): the barista says so
					const bool bKnown = P.Role == TEXT("barista") && FrontDoor && !CounterKey.IsEmpty() && FrontDoor->UsualAt(CounterKey) != NAME_None;
					Talk->SayLine(P.Body.Get(), bKnown ? FString::Printf(TEXT("line.frontdoor.barista_regular.%d"), 1 + (N % 2))
						: FString::Printf(TEXT("line.transport.%s.hello.%d"), *P.Role.ToString(), N));
				}
				if (P.Role == TEXT("barista"))
				{
					if (ULyudCounterService* CounterService = ULyudCounterService::Get(this))
					{
						CounterService->NoteGreeted(P.Body.Get());
					}
				}
			}
			if (Dist < 330.f && bLooks)
			{
				Best = P.Role == TEXT("barista") ? 0 : 1;
				BestP = &P;
			}
		}
		// the small things win over the people behind them: the punch on the desk, the shelf by it
		auto Aimed = [&](const FVector& At, float Reach)
		{
			return !At.IsZero() && FVector::Dist2D(Feet, At) < Reach && FMath::Abs(Feet.Z - At.Z) < 200.f &&
				FVector::DotProduct(Look, (At - Eye).GetSafeNormal()) > 0.8f;
		};
		if (FrontDoor && Aimed(PunchAt, 170.f))
		{
			Best = 2;
		}
		else if (FrontDoor && bFront && Aimed(ShelfAt + FVector(0, 0, 90.f), 190.f))
		{
			Best = 3;
		}
		// the hall shut for the night: a bell on the attendant's empty desk (between her place and the hall)
		for (const FPerson& Plan : StaffPlan)
		{
			if (Best < 0 && !Attendant && Plan.Role == TEXT("attendant"))
			{
				const FVector Desk = Plan.At + FRotator(0.f, Plan.Yaw, 0.f).Vector() * 75.f;
				if (FVector::Dist2D(Feet, Desk) < 260.f && FMath::Abs(Feet.Z - Plan.At.Z) < 250.f
					&& FVector::DotProduct(Look, (Desk + FVector(0, 0, 90.f) - Eye).GetSafeNormal()) > 0.7f)
				{
					Best = 4;
					BellAt = Desk + FVector(0, 0, 95.f);
				}
			}
		}
	}
	// the barista's prompt names his usual; the attendant's is the card's until he has one
	FName Usual = NAME_None;
	int32 UsualPrice = 0;
	if (Best == 0 && FrontDoor && !CounterKey.IsEmpty())
	{
		Usual = FrontDoor->UsualAt(CounterKey);
		if (Usual != NAME_None)
		{
			bool bSold = false;
			for (const FLyudShopGood& G : StockOf(Shop))
			{
				if (G.Item == Usual)
				{
					UsualPrice = G.Price;
					bSold = true;
				}
			}
			Usual = bSold ? Usual : NAME_None;
		}
	}
	const bool bCardFirst = Best == 1 && bFront && FrontDoor && !FrontDoor->HasCard();
	const int32 Shown = Best == 0 && Usual != NAME_None ? 10 : Best == 1 && bCardFirst ? 11 : Best;
	if (Shown != Prompted && UI)
	{
		Prompted = Shown;
		FText Text = FText::GetEmpty();
		switch (Shown)
		{
		case 0: Text = LyudText::Get(TEXT("ui.shop.prompt")); break;
		case 10:
			Text = FText::FromString(LyudText::Get(TEXT("ui.shop.prompt")).ToString() + TEXT("\n") + LyudText::Get(TEXT("frontdoor.usual.prompt"),
				{ FStringFormatArg(LyudText::Get(FString::Printf(TEXT("item.%s.name"), *Usual.ToString())).ToString()), FStringFormatArg(UsualPrice) }).ToString());
			break;
		case 1: Text = LyudText::Get(TEXT("transport.hall.attendant_prompt")); break;
		case 11: Text = LyudText::Get(TEXT("frontdoor.card.prompt")); break;
		case 2: Text = LyudText::Get(TEXT("frontdoor.punch.prompt")); break;
		case 3: Text = LyudText::Get(TEXT("frontdoor.shelf.prompt")); break;
		case 4: Text = LyudText::Get(TEXT("transport.hall.bell_prompt")); break;
		default: break;
		}
		UI->SetPrompt(this, Text);
	}
	if (!PC || Best < 0)
	{
		return;
	}
	auto Done = [&]()
	{
		Prompted = -1;
		UI->SetPrompt(this, FText::GetEmpty());
	};
	if (Best == 0 && PC->WasInputKeyJustPressed(EKeys::T))
	{
		const TArray<FLyudShopGood> Goods = StockOf(Shop);
		if (Goods.Num())
		{
			FText StationName = FText::GetEmpty();
			if (ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this))
			{
				StationName = T->StationName(Station);
			}
			// the counter service serves it as a scene (the cup on the counter, the change), and the front door counts his usual
			if (ULyudCounterService* CounterService = ULyudCounterService::Get(this))
			{
				CounterService->OpenedAt(CounterKey, BestP->Body.Get(), BestP->Pid);
			}
			UI->OpenShop(LyudText::Get(TEXT("ui.shop.title_station_buffet"), { FStringFormatArg(StationName.ToString()) }), Goods);
			Done();
			// she takes the order: a word and to work at the counter for a moment
			if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
			{
				Talk->SayLine(BestP->Body.Get(), FString::Printf(TEXT("line.transport.barista.order.%d"), 1 + (int32)((uint32)(Clock * 3.f) % 3u)));
			}
			BestP->TalkLeft = 6.f;
			BestP->Pose = (uint8)EPersonPose::Work;
		}
	}
	else if (Best == 0 && Usual != NAME_None && PC->WasInputKeyJustPressed(EKeys::E))
	{
		FrontDoor->BuyUsual(CounterKey, Usual, UsualPrice, BestP->Body.Get(), BestP->Pid);
		if (BestP->Body.IsValid())
		{
			BestP->TalkLeft = 3.f;
			BestP->Yaw = (Pawn->GetActorLocation() - BestP->At).Rotation().Yaw;
			BestP->Pose = (uint8)EPersonPose::Talk;
		}
		Done();
	}
	else if (Best == 1 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		if (bCardFirst)
		{
			FrontDoor->GiveCard(BestP->Body.Get(), BestP->Pid);
		}
		else
		{
			if (ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this))
			{
				T->OpenDiagram(Station);
			}
			if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
			{
				Talk->SayLine(BestP->Body.Get(), FString::Printf(TEXT("line.transport.attendant.help.%d"), 1 + (int32)((uint32)Clock % 3)));
			}
		}
		Done();
	}
	else if (Best == 2 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		FrontDoor->Punch(Station, PunchAt);
		Done();
	}
	else if (Best == 3 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		// the attendant (when on shift) says whose it is; without them he takes it all the same
		FrontDoor->TakeFromShelf(Attendant ? Attendant->Body.Get() : nullptr, Attendant ? Attendant->Pid : -1);
		ShelfShown = -1;
		Done();
	}
	else if (Best == 4 && PC->WasInputKeyJustPressed(EKeys::E) && Clock > BellQuiet)
	{
		// nobody comes: the hall opens in the morning
		BellQuiet = Clock + 2.5f;
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(TEXT("station_chime"), BellAt, 0.5f);
		}
		float Open = 5.75f;
		if (const ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this); T && T->GetLife())
		{
			Open = T->GetLife()->OpenHour;
		}
		if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
		{
			const int32 Min = FMath::RoundToInt(Open * 60.f);
			Screen->Toast(LyudText::Get(TEXT("transport.hall.closed_note"), { FStringFormatArg(FString::Printf(TEXT("%d:%02d"), Min / 60, Min % 60)) }));
		}
	}
}

void ALyudStationHall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const float D = FVector::Dist2D(Cam, Middle);
	const bool bLevel = FMath::Abs(Cam.Z - Middle.Z) < LevelBand;
	// the front door's lamps at dusk: seen from across the forecourt (docs/front_door.md)
	const ULyudFrontDoor* FrontDoor = bFront ? ULyudFrontDoor::Get(this) : nullptr;
	const float LightsReach = FrontDoor && FrontDoor->IsDusk() ? 25000.f : LightsOn;
	const bool bWantLights = D < LightsReach && FMath::Abs(Cam.Z - Middle.Z) < (FrontDoor && FrontDoor->IsDusk() ? 6000.f : LevelBand * 2.f);
	if (bWantLights != bLightsOn)
	{
		SetLights(bWantLights);
	}
	// the line, the punch, the board and the shelf: placed once the player first comes near (the cores are there to test against)
	if (!bFrontBuilt && D < LightsOn)
	{
		BuildFrontDoor();
	}
	if (bFrontBuilt)
	{
		TickFront(DeltaTime, Cam, PC->PlayerCameraManager->GetCameraRotation().Vector());
	}
	const bool bAllowed = CVarHallPeople.GetValueOnGameThread() != 0;
	if (!bPeopleOn && bAllowed && D < PeopleOn && bLevel)
	{
		SpawnPeople();
	}
	else if (bPeopleOn && (!bAllowed || D > PeopleOff || FMath::Abs(Cam.Z - Middle.Z) > LevelBand * 1.5f))
	{
		ClearPeople();
	}
	// near: every frame (the prompt, the greetings); far: four times a second is plenty
	SetActorTickInterval(bPeopleOn ? 0.f : 0.25f + 0.03f * (Station % 5));
	if (!bPeopleOn)
	{
		return;
	}
	// staff come on at six and go at midnight, while the player is about (against the duty the people were made with,
	// not against the bodies standing: a staff body that could not be made would have every frame make them all again)
	const bool bDuty = StaffOnDuty();
	if (bDuty != bDutyAtSpawn && D > 1500.f)
	{
		ClearPeople();
		SpawnPeople();
		return;
	}
	// the station life: riders in and out, the next shift
	LifeTimer -= DeltaTime;
	if (LifeTimer <= 0.f)
	{
		LifeTimer = 1.f;
		SyncLife(Cam, PC->PlayerCameraManager->GetCameraRotation().Vector());
		TickStaff();
	}
	// the bodies hold their pose (the clip's variants change by themselves); a tick keeps their animation's clock going
	for (FPerson& P : People)
	{
		if (P.TalkLeft > 0.f)
		{
			P.TalkLeft -= DeltaTime;
			if (P.TalkLeft <= 0.f)
			{
				P.Yaw = P.BaseYaw;
				P.Pose = P.BasePose;
			}
		}
		if (ANpcBody* B = P.Body.Get(); B && !P.bWalking)
		{
			if (B->WasRecentlyRendered(0.5f))
			{
				B->Drive(P.At, P.Yaw, (EPersonPose)P.Pose, 0, 0.f, P.SeatHeight, DeltaTime, false);
			}
		}
	}
	ChurnTimer -= DeltaTime;
	if (ChurnTimer <= 0.f)
	{
		ChurnTimer = 40.f + (float)((Seed + (uint32)Clock) % 50);
		Churn(Cam, PC->PlayerCameraManager->GetCameraRotation().Vector());
	}
	TickWalks(DeltaTime);
	TickPrompt(DeltaTime);
	TickBarks(DeltaTime, Cam);
}

void ALyudStationHall::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearPeople();
	Super::EndPlay(Reason);
}

// --- the club's front door (docs/front_door.md) -----------------------------------------------------------------------

namespace LyudHallFront
{
	constexpr float LineGap = 85.f;         // cm between people in the counter's line
	constexpr int32 LineMax = 4;            // places behind the counter place
	constexpr float WalkSpeed = 110.f;      // cm/s stepping up or off with the cup
	constexpr float BoardNear = 4000.f;     // the board is drawn within this of the camera
	constexpr float ClackNear = 2500.f;

	FString Paint(FName Lost)
	{
		// the lost things' looks on the shelf (LyudInventory's tints, roughly)
		static const TMap<FName, FString> Paints = {
			{ TEXT("lost_umbrella"), TEXT("#1F3A5F") }, { TEXT("lost_mitten"), TEXT("#B03030") }, { TEXT("lost_scarf"), TEXT("#D9A441") },
			{ TEXT("lost_glasses"), TEXT("#202020") }, { TEXT("lost_keys"), TEXT("#B8A040") }, { TEXT("lost_bear"), TEXT("#8A5A2B") },
		};
		const FString* P = Paints.Find(Lost);
		return P ? *P : FString(TEXT("#888888"));
	}

	FVector Size(FName Lost)
	{
		static const TMap<FName, FVector> Sizes = {
			{ TEXT("lost_umbrella"), FVector(8.f, 34.f, 8.f) }, { TEXT("lost_mitten"), FVector(10.f, 14.f, 5.f) }, { TEXT("lost_scarf"), FVector(16.f, 26.f, 7.f) },
			{ TEXT("lost_glasses"), FVector(6.f, 14.f, 4.f) }, { TEXT("lost_keys"), FVector(6.f, 8.f, 2.f) }, { TEXT("lost_bear"), FVector(14.f, 14.f, 20.f) },
		};
		const FVector* S = Sizes.Find(Lost);
		return S ? *S : FVector(10.f);
	}
}

UStaticMeshComponent* ALyudStationHall::PropBox(const FVector& At, const FRotator& R, const FVector& SizeCm, const FString& Paint, bool bCollide)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Cube);
	C->SetWorldLocationAndRotation(At, R);
	C->SetWorldScale3D(SizeCm / 100.f);
	if (Way)
	{
		C->SetMaterial(0, Way->Mat(FName(*Paint)));
	}
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollide)
	{
		C->SetCollisionResponseToAllChannels(ECR_Block);
	}
	C->SetCastShadow(SizeCm.GetMax() > 40.f);
	C->bAffectDistanceFieldLighting = false;
	C->RegisterComponent();
	Props.Add(C);
	return C;
}

bool ALyudStationHall::Clear(const FVector& Floor, float Radius) const
{
	// room for somebody standing here: nothing of the hall in the way (its cores, walls), floor under the feet
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(HallFrontClear), false, this);
	for (ANpcBody* B : Bodies)
	{
		Q.AddIgnoredActor(B);
	}
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Q.AddIgnoredActor(Pawn);
	}
	if (World->OverlapAnyTestByChannel(Floor + FVector(0, 0, 100.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(Radius, 80.f), Q))
	{
		return false;
	}
	FHitResult H;
	return World->LineTraceSingleByChannel(H, Floor + FVector(0, 0, 40.f), Floor - FVector(0, 0, 60.f), ECC_WorldStatic, Q);
}

void ALyudStationHall::BuildLine()
{
	// the counter's line: the counter place first, then spots behind it, one way that is clear of the furniture and of the
	// hall's other places (away from the counter, or along it)
	LineSlots.Reset();
	int32 Front = -1;
	for (int32 I = 0; I < Slots.Num() && Front < 0; I++)
	{
		Front = Slots[I].Kind == 2 ? I : -1;
	}
	if (Front < 0)
	{
		return;
	}
	Slots[Front].Line = 0;
	LineSlots.Add(Front);
	const FVector Customer = Slots[Front].At;
	FVector Away = Customer - CounterAt;
	Away.Z = 0.f;
	if (CounterAt.IsZero() || Away.Size() < 10.f)
	{
		Away = -FRotator(0.f, Slots[Front].Yaw, 0.f).Vector();
	}
	Away = Away.GetSafeNormal2D();
	const FVector Side = FRotator(0.f, Slots[Front].Yaw + 90.f, 0.f).Vector();
	const FVector Ways[] = { Away, Side, -Side, (Away + Side).GetSafeNormal2D(), (Away - Side).GetSafeNormal2D() };
	auto FarFromPlaces = [&](const FVector& P)
	{
		for (int32 I = 0; I < Slots.Num(); I++)
		{
			if (I != Front && FVector::Dist2D(Slots[I].At, P) < 60.f)
			{
				return false;
			}
		}
		for (const FPerson& S : StaffPlan)
		{
			if (FVector::Dist2D(S.At, P) < 70.f)
			{
				return false;
			}
		}
		return true;
	};
	int32 BestWay = -1, BestCount = 0;
	for (int32 W = 0; W < (int32)UE_ARRAY_COUNT(Ways); W++)
	{
		int32 N = 0;
		while (N < LyudHallFront::LineMax)
		{
			const FVector P = Customer + Ways[W] * LyudHallFront::LineGap * (N + 1);
			if (!FarFromPlaces(P) || !Clear(P, 28.f))
			{
				break;
			}
			N++;
		}
		if (N > BestCount)
		{
			BestCount = N;
			BestWay = W;
		}
	}
	for (int32 N = 0; N < BestCount; N++)
	{
		FSlot S;
		S.At = Customer + Ways[BestWay] * LyudHallFront::LineGap * (N + 1);
		S.Yaw = (-Ways[BestWay]).Rotation().Yaw;
		S.Kind = 8;
		S.Line = N + 1;
		LineSlots.Add(Slots.Add(S));
	}
}

void ALyudStationHall::BuildFrontDoor()
{
	bFrontBuilt = true;
	CounterKey = FString::Printf(TEXT("hall:%d"), Station);
	BuildLine();
	const FPerson* Desk = nullptr;
	for (const FPerson& P : StaffPlan)
	{
		Desk = P.Role == TEXT("attendant") ? &P : Desk;
	}
	ULyudFrontDoor* FrontDoor = ULyudFrontDoor::Get(this);
	bFront = FrontDoor && FrontDoor->FrontStation() == Station;
	if (!Desk)
	{
		return;
	}
	const FRotator Facing(0.f, Desk->Yaw, 0.f);
	const FVector Fwd = Facing.Vector();
	const FVector Right = FRotator(0.f, Desk->Yaw + 90.f, 0.f).Vector();
	// the punch on the attendant's desk: on the desk's top where the cores have one, else at a desk's height
	{
		const FVector Over = Desk->At + Fwd * 55.f + Right * 45.f;
		float Top = Desk->At.Z + 95.f;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(HallPunch), false);
		if (GetWorld()->LineTraceSingleByChannel(H, Over + FVector(0, 0, 150.f), Over + FVector(0, 0, 40.f), ECC_WorldStatic, Q) &&
			H.ImpactPoint.Z - Desk->At.Z > 60.f && H.ImpactPoint.Z - Desk->At.Z < 130.f)
		{
			Top = H.ImpactPoint.Z;
		}
		PunchAt = FVector(Over.X, Over.Y, Top);
		PropBox(PunchAt + FVector(0, 0, 6.f), Facing, FVector(9.f, 13.f, 12.f), TEXT("#2E4A3A"), false);
		PropBox(PunchAt + FVector(0, 0, 13.f) + Fwd * 3.f, Facing + FRotator(-12.f, 0.f, 0.f), FVector(16.f, 2.5f, 2.5f), TEXT("#B4B4B0"), false);
		PropBox(PunchAt + FVector(0, 0, 0.6f) - Right * 14.f, Facing, FVector(6.f, 9.f, 1.2f), TEXT("#E8D9B0"), false);   // a card blank
	}
	if (!bFront)
	{
		return;
	}
	// the departures board over the desk, facing the hall
	if (const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this); Tr && Tr->GetNet())
	{
		const int32 SunLine = Tr->GetNet()->FindLine("sun");
		BoardAt = Desk->At + Fwd * 30.f + FVector(0, 0, 238.f);
		Board = NewObject<UWidgetComponent>(this, TEXT("FlapBoard"));
		Board->SetupAttachment(RootComponent);
		Board->SetMobility(EComponentMobility::Movable);
		Board->SetWidgetSpace(EWidgetSpace::World);
		Board->SetDrawSize(FVector2D(1200.f, 560.f));
		Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Board->SetBlendMode(EWidgetBlendMode::Opaque);
		Board->SetCastShadow(false);
		Board->bAffectDistanceFieldLighting = false;
		Board->RegisterComponent();
		Board->SetWorldLocationAndRotation(BoardAt, Facing);
		Board->SetWorldScale3D(FVector(0.12f));
		FlapBoard = SNew(SLyudFlapBoard).Station(Station).Line(SunLine);
		Board->SetSlateWidget(FlapBoard);
		Board->SetRedrawTime(1.f);
		Board->SetVisibility(false);
		// its frame and the two rods it hangs by
		PropBox(BoardAt - Fwd * 2.f, Facing, FVector(2.f, 150.f, 73.f), TEXT("#2A2A2C"), false);
		for (const float Y : { -60.f, 60.f })
		{
			PropBox(BoardAt - Fwd * 2.f + Right * Y + FVector(0, 0, 55.f), Facing, FVector(1.5f, 1.5f, 40.f), TEXT("#5A5A5E"), false);
		}
	}
	// the lost and found: a little rack beside the desk, where there is room for it
	const FVector Tries[] = { Right * 120.f, -Right * 120.f, Right * 160.f - Fwd * 20.f, -Right * 160.f - Fwd * 20.f, Fwd * 140.f + Right * 120.f };
	for (const FVector& T : Tries)
	{
		const FVector P = Desk->At + T;
		if (Clear(P, 40.f))
		{
			ShelfAt = P;
			break;
		}
	}
	if (ShelfAt.IsZero())
	{
		return;
	}
	const TCHAR* Wood = TEXT("#6B4A2E");
	PropBox(ShelfAt - Fwd * 16.f + FVector(0, 0, 75.f), Facing, FVector(3.f, 92.f, 150.f), Wood, true);
	for (const float Y : { -45.f, 45.f })
	{
		PropBox(ShelfAt + Right * Y + FVector(0, 0, 75.f), Facing, FVector(34.f, 2.5f, 150.f), Wood, true);
	}
	for (const float Z : { 12.f, 72.f, 122.f })
	{
		PropBox(ShelfAt + FVector(0, 0, Z), Facing, FVector(32.f, 88.f, 2.5f), TEXT("#7A5636"), false);
	}
	// its card: «Бюро находок» is painted on the rack's top (a plain plate; the words are the prompt's)
	PropBox(ShelfAt - Fwd * 14.f + FVector(0, 0, 160.f), Facing, FVector(2.f, 70.f, 18.f), TEXT("#E8E0C8"), false);
	ShelfShown = -1;
}

bool ALyudStationHall::HeroHolds(const FVector& At) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Pawn || !Pawn->IsA<AHeroCharacter>() || Pawn->IsHidden())
	{
		return false;
	}
	const FVector H = Pawn->GetActorLocation();
	return FVector::Dist2D(H, At) < 60.f && FMath::Abs(H.Z - At.Z) < 150.f;
}

int32 ALyudStationHall::PersonIndexAt(int32 SlotIndex) const
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return -1;
	}
	const int32 W = Slots[SlotIndex].Who;
	return People.IsValidIndex(W) && People[W].Body.IsValid() && People[W].Slot == SlotIndex ? W : -1;
}

void ALyudStationHall::LineState(TArray<int32>& Out) const
{
	Out.Reset();
	for (const int32 S : LineSlots)
	{
		const int32 W = PersonIndexAt(S);
		Out.Add(W >= 0 ? W : HeroHolds(Slots[S].At) ? -2 : -1);
	}
}

void ALyudStationHall::StartWalk(FPerson& P, const FVector& To, float Yaw, uint8 Pose)
{
	P.WalkFrom = P.bWalking ? FMath::Lerp(P.WalkFrom, P.At, FMath::Clamp(P.WalkT, 0.f, 1.f)) : P.At;
	P.At = To;
	P.Yaw = Yaw;
	P.BaseYaw = Yaw;
	P.WalkPose = Pose;
	P.BasePose = Pose;
	P.WalkT = 0.f;
	P.bWalking = true;
	P.SeatHeight = 0.f;
	P.TalkLeft = 0.f;
}

void ALyudStationHall::TickWalks(float DeltaTime)
{
	for (FPerson& P : People)
	{
		ANpcBody* B = P.Body.Get();
		if (!P.bWalking || !B)
		{
			continue;
		}
		const float Len = FVector::Dist(P.WalkFrom, P.At);
		P.WalkT = Len < 1.f ? 1.f : P.WalkT + DeltaTime * LyudHallFront::WalkSpeed / Len;
		if (P.WalkT >= 1.f)
		{
			P.bWalking = false;
			P.Pose = P.WalkPose;
			P.SeatHeight = Slots.IsValidIndex(P.Slot) ? Slots[P.Slot].SeatHeight : 0.f;
			B->Drive(P.At, P.Yaw, (EPersonPose)P.Pose, 0, 0.f, P.SeatHeight, DeltaTime, false);
			continue;
		}
		const FVector Now = FMath::Lerp(P.WalkFrom, P.At, P.WalkT);
		B->Drive(Now, (P.At - P.WalkFrom).Rotation().Yaw, EPersonPose::Walk, 0, LyudHallFront::WalkSpeed, 0.f, DeltaTime, false);
	}
}

void ALyudStationHall::StepLine(const FVector& Cam, const FVector& Look)
{
	if (LineSlots.Num() == 0)
	{
		return;
	}
	auto Unseen = [&](const FVector& P)
	{
		const FVector D = P + FVector(0, 0, 100.f) - Cam;
		return FVector::DotProduct(D.GetSafeNormal(), Look) < 0.35f || D.Size() > 2500.f;
	};
	// one step up a look: whoever has a free spot before them moves into it (never past the hero's spot)
	TArray<int32> State;
	LineState(State);
	std::vector<int> V(State.GetData(), State.GetData() + State.Num());
	int To = -1;
	const int From = LyudFrontDoor::StepUp(V, To);
	if (From >= 0 && To >= 0 && People.IsValidIndex(State[From]))
	{
		const int32 W = State[From];
		FPerson& P = People[W];
		Slots[LineSlots[From]].Who = -1;
		Slots[LineSlots[To]].Who = W;
		P.Slot = LineSlots[To];
		P.FrontSince = -1.0;
		StartWalk(P, Slots[P.Slot].At, Slots[P.Slot].Yaw, (uint8)EPersonPose::Stand);
	}
	// the front: the barista serves them in a minute or so, then they take their cup to a free table (or a standing place)
	const int32 W = PersonIndexAt(LineSlots[0]);
	if (W < 0 || People[W].bWalking || People[W].bServed || People[W].Role != TEXT("passenger"))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FPerson* Barista = nullptr;
	for (FPerson& S : People)
	{
		Barista = S.Role == TEXT("barista") && S.Body.IsValid() ? &S : Barista;
	}
	FPerson& P = People[W];
	const uint32 H = GetTypeHash(P.Model) ^ (uint32)(W * 2654435761u);
	if (P.FrontSince < 0.0)
	{
		P.FrontSince = Now;
		if (Barista)
		{
			Barista->TalkLeft = 4.f;
			Barista->Yaw = (P.At - Barista->At).Rotation().Yaw;
			Barista->Pose = (uint8)EPersonPose::Talk;
		}
		// a regular at the front: the barista knows what they'll have
		const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
		const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
		if (Barista && Life && P.Run >= 0 && P.Run < (int32)Life->Today.size() && Now >= NextUsualBark && FVector::Dist(Cam, P.At) < 1500.f)
		{
			const LyudTransport::FLifeTrip& T = Life->Trips[Life->Today[P.Run].Trip];
			if (Life->Riders[T.Rider].bRegular)
			{
				NextUsualBark = Now + 90.0;
				if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
				{
					Talk->SayLine(Barista->Body.Get(), FString::Printf(TEXT("line.frontdoor.usual_regular.%d"), 1 + (int32)(H % 3u)));
				}
			}
		}
		return;
	}
	if (Now - P.FrontSince < 45.0 + (double)(H % 40u))
	{
		return;
	}
	int32 Best = -1;
	float BestD = 2000.f;
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		const FSlot& S = Slots[I];
		if (S.Who >= 0 || S.Kind == 2 || S.Kind == 8 || S.Kind == 5 || S.Kind == 6)
		{
			continue;
		}
		const float D = FVector::Dist(S.At, P.At) * (S.Kind == 1 || S.Kind == 7 ? 0.6f : 1.f);   // a table first
		if (D < BestD)
		{
			BestD = D;
			Best = I;
		}
	}
	if (Best < 0)
	{
		// nowhere to sit: out to the platform (when nobody sees them go)
		if (Unseen(P.At) && P.Run < 0)
		{
			P.Body->Destroy();
			P.Body = nullptr;
			Unpresent(P);
			Slots[LineSlots[0]].Who = -1;
		}
		return;
	}
	P.bServed = true;
	Slots[LineSlots[0]].Who = -1;
	Slots[Best].Who = W;
	P.Slot = Best;
	const FSlot& S = Slots[Best];
	StartWalk(P, S.At, S.Yaw, (uint8)(S.Kind == 1 ? EPersonPose::Eat : S.SeatHeight > 0.f ? EPersonPose::Sit : EPersonPose::Stand));
	if (Barista)
	{
		Barista->TalkLeft = 3.f;
		Barista->Pose = (uint8)EPersonPose::Talk;
	}
}

void ALyudStationHall::ShowShelf()
{
	ULyudFrontDoor* FrontDoor = ULyudFrontDoor::Get(this);
	if (!FrontDoor || ShelfAt.IsZero())
	{
		return;
	}
	TArray<FName> Items;
	FrontDoor->ShelfItems(Items);
	uint32 H = (uint32)Items.Num();
	for (const FName& N : Items)
	{
		H = HashCombineFast(H, GetTypeHash(N));
	}
	if ((int32)H == ShelfShown)
	{
		return;
	}
	ShelfShown = (int32)H;
	for (UStaticMeshComponent* C : ShelfThings)
	{
		if (C)
		{
			Props.Remove(C);
			C->DestroyComponent();
		}
	}
	ShelfThings.Reset();
	const FPerson* Desk = nullptr;
	for (const FPerson& P : StaffPlan)
	{
		Desk = P.Role == TEXT("attendant") ? &P : Desk;
	}
	const float DeskYaw = Desk ? Desk->Yaw : 0.f;
	const FVector Right = FRotator(0.f, DeskYaw + 90.f, 0.f).Vector();
	for (int32 I = 0; I < Items.Num() && I < 4; I++)
	{
		// two on the middle board, two on the top one, the long umbrella lying along it
		const FVector Sz = LyudHallFront::Size(Items[I]);
		const float Z = (I < 2 ? 72.f : 122.f) + 1.25f + Sz.Z * 0.5f;
		const FVector At = ShelfAt + Right * ((I % 2) ? 22.f : -20.f) + FVector(0, 0, Z);
		if (UStaticMeshComponent* C = PropBox(At, FRotator(0.f, DeskYaw + (float)(I * 17 % 23) - 11.f, 0.f), Sz, LyudHallFront::Paint(Items[I]), false))
		{
			ShelfThings.Add(C);
		}
	}
}

void ALyudStationHall::TickFront(float DeltaTime, const FVector& Cam, const FVector& Look)
{
	// the board: drawn while near, redrawn every frame while its letters turn, a clack when a row starts turning
	if (Board && FlapBoard.IsValid())
	{
		const bool bNear = FVector::Dist(Cam, BoardAt) < LyudHallFront::BoardNear;
		if (Board->IsVisible() != bNear)
		{
			Board->SetVisibility(bNear);
			Board->SetTickMode(bNear ? ETickMode::Enabled : ETickMode::Disabled);
		}
		if (bNear)
		{
			Board->SetRedrawTime(FlapBoard->IsFlipping() ? 0.f : 1.f);
			if (FlapBoard->Flips() > BoardFlips)
			{
				BoardFlips = FlapBoard->Flips();
				if (FVector::Dist(Cam, BoardAt) < LyudHallFront::ClackNear)
				{
					if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
					{
						Sound->PlayEffect(TEXT("board_flap"), BoardAt, 0.8f);
					}
				}
			}
		}
	}
	FrontClock -= DeltaTime;
	if (FrontClock > 0.f)
	{
		return;
	}
	FrontClock = 0.5f;
	if (bPeopleOn)
	{
		StepLine(Cam, Look);
	}
	if (bFront)
	{
		ShowShelf();
	}
}

FString ALyudStationHall::Report() const
{
	int32 Pass = 0, Staff = 0;
	for (const FPerson& P : People)
	{
		if (P.Body.IsValid())
		{
			(P.Role == TEXT("passenger") ? Pass : Staff)++;
		}
	}
	int32 Riders = 0;
	FString Who;
	for (const FPerson& P : People)
	{
		if (P.Body.IsValid() && (P.Run >= 0 || P.StaffIndex >= 0))
		{
			Riders += P.Run >= 0 ? 1 : 0;
			Who += FString::Printf(TEXT(" %s%s"), *P.Role.ToString().Left(1), P.Pid >= 0 ? TEXT("*") : TEXT(""));
		}
	}
	std::vector<int> Here;
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	if (Tr && Tr->GetLife())
	{
		Tr->GetLife()->InHall(Station, Here);
	}
	TArray<int32> State;
	LineState(State);
	FString LineWords;
	for (const int32 W : State)
	{
		LineWords += W >= 0 ? TEXT("P") : W == -2 ? TEXT("H") : TEXT(".");
	}
	const FString FrontWords = FString::Printf(TEXT("; front door: %s, counter %s, line [%s], punch %s, board %s (%d flips), shelf %s"),
		bFront ? TEXT("yes") : TEXT("no"), CounterKey.IsEmpty() ? TEXT("-") : *CounterKey, *LineWords, PunchAt.IsZero() ? TEXT("no") : TEXT("yes"),
		Board ? TEXT("yes") : TEXT("no"), FlapBoard.IsValid() ? FlapBoard->Flips() : 0, ShelfAt.IsZero() ? TEXT("no") : TEXT("yes"));
	return FString::Printf(TEXT("hall %d %s: dressing %s, cores %s, %d lamps (%s), %d places, staff %d/%d, passengers %d (%d riders of the station life, %d in the hall by it, filler want %d)%s; open %s%s%s"),
		Station, *Style, Detail ? TEXT("yes") : TEXT("no"), Cores ? TEXT("yes") : TEXT("no"), Lights.Num(), bLightsOn ? TEXT("on") : TEXT("off"),
		Slots.Num(), Staff, StaffPlan.Num(), Pass, Riders, (int32)Here.size(), WantPassengers(), *Who, LifeOpen() ? TEXT("yes") : TEXT("no"),
		Tr && Tr->GetLife() ? *FString::Printf(TEXT(", %s"), UTF8_TO_TCHAR(Tr->GetLife()->Report(*Tr->GetNet()).c_str())) : TEXT(""), *FrontWords);
}

static FAutoConsoleCommandWithWorldAndArgs GTransportHall(TEXT("Transport.Hall"),
	TEXT("Transport.Hall [station index]: the stations' halls v3 (dressing, lamps, staff, passengers)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<ALyudStationHall> It(World); It; ++It)
		{
			if (Args.Num() == 0 || FCString::Atoi(*Args[0]) == It->Station)
			{
				UE_LOG(LogTemp, Display, TEXT("TRANSPORTTEST %s"), *It->Report());
			}
		}
	}));
