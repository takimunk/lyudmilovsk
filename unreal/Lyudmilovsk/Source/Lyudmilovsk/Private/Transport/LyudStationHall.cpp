// The cable-car stations' halls, v3: the dressing, lamps and people of Content/Transport/halls.json (blender/transport/halls.py).
// docs/transport.md "Halls v3".

#include "Transport/LyudStationHall.h"

#include "Transport/LyudCableway.h"
#include "Transport/LyudTransport.h"
#include "StationLife.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudText.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
		const int32 H = (int32)T->LifeHour();
		return H < 7 || H >= 23 ? 0 : (int32)((Seed + (uint32)T->LifeWeekday() * 7u + (uint32)H) % 3u);
	}
	// by the hour: the morning and the evening rush, a quiet afternoon, almost nobody at night
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const int32 H = Now.GetHour();
	int32 N = H < 6 ? 0 : H < 7 ? 2 : H < 10 ? 7 : H < 17 ? 4 : H < 20 ? 7 : H < 23 ? 3 : 1;
	N += (int32)((Seed + Now.GetDay() * 31 + H) % 3) - 1;
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
	// the passengers: free places picked by this visit's luck, seated ones reading or eating now and then
	const int32 Want = WantPassengers();
	FRandomStream R(Seed ^ (uint32)(GetWorld()->GetTimeSeconds() * 10.0));
	TArray<int32> Order;
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		Slots[I].Who = -1;
		Order.Add(I);
	}
	for (int32 I = Order.Num() - 1; I > 0; I--)
	{
		Order.Swap(I, R.RandRange(0, I));
	}
	int32 Made = 0;
	for (int32 K = 0; K < Order.Num() && Made < Want; K++)
	{
		FSlot& S = Slots[Order[K]];
		if (S.Kind == 2 && !StaffOnDuty())
		{
			continue;          // nobody queues at a shut buffet
		}
		FPerson P;
		P.Role = TEXT("passenger");
		P.Slot = Order[K];
		P.At = S.At;
		P.Yaw = S.Yaw;
		P.SeatHeight = S.SeatHeight;
		const float Roll = R.FRand();
		P.Pose = (uint8)(S.SeatHeight > 0.f ? (S.Kind == 1 ? (Roll < 0.5f ? EPersonPose::Eat : EPersonPose::Read)
			: (Roll < 0.3f ? EPersonPose::Read : EPersonPose::Sit))
			: (S.Kind == 6 || S.Kind == 5 ? EPersonPose::Look : (Roll < 0.25f ? EPersonPose::Talk : EPersonPose::Stand)));
		P.Model = PickModel(Order[K] * 13 + Made);
		if (SpawnPerson(P))
		{
			S.Who = People.Add(P);
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
	for (ANpcBody* B : Bodies)
	{
		if (IsValid(B))
		{
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
		for (int32 Tries = 0; Tries < 8; Tries++)
		{
			const int32 K = R.RandRange(0, Slots.Num() - 1);
			if (!Slots.IsValidIndex(K) || Slots[K].Who >= 0 || !Unseen(Slots[K].At) || (Slots[K].Kind == 2 && !StaffOnDuty()))
			{
				continue;
			}
			FPerson P;
			P.Role = TEXT("passenger");
			P.Slot = K;
			P.At = Slots[K].At;
			P.Yaw = Slots[K].Yaw;
			P.SeatHeight = Slots[K].SeatHeight;
			P.Pose = (uint8)(P.SeatHeight > 0.f ? (R.FRand() < 0.35f ? EPersonPose::Read : EPersonPose::Sit) : EPersonPose::Stand);
			P.Model = PickModel(K * 17 + (int32)Clock);
			if (SpawnPerson(P))
			{
				Slots[K].Who = People.Add(P);
			}
			break;
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
		// a place: the one their name picks first, else the next free one (walking out: a standing place by the door side)
		int32 K = -1;
		const uint32 H0 = GetTypeHash(FString(UTF8_TO_TCHAR(Rd.Key.c_str())));
		for (int32 J = 0; J < Slots.Num() && K < 0; J++)
		{
			const int32 C = (int32)((H0 + (uint32)J * 7u) % (uint32)Slots.Num());
			const FSlot& S = Slots[C];
			if (S.Who >= 0 || (S.Kind == 2 && !bBuffet) || (bOut && S.SeatHeight > 0.f) || !Unseen(S.At))
			{
				continue;
			}
			K = C;
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
		const float Roll = LyudTransport::LifeHash(Rd.Key, 53u + (uint32)I);
		P.Pose = (uint8)(P.SeatHeight > 0.f ? (Slots[K].Kind == 1 ? (Roll < 0.5f ? EPersonPose::Eat : EPersonPose::Read)
			: (Roll < 0.35f ? EPersonPose::Read : EPersonPose::Sit))
			: (Slots[K].Kind == 6 || Slots[K].Kind == 5 ? EPersonPose::Look : (Roll < 0.2f ? EPersonPose::Talk : EPersonPose::Stand)));
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
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
	const TCHAR* What = TEXT("any");
	if (Life && Life->IsWindStopped())
	{
		What = TEXT("stopped");
	}
	else if (Life && Life->LastWind() >= Life->WindWarn)
	{
		What = TEXT("wind");
	}
	else if (Tr && Tr->LifeHour() < 9.5f)
	{
		What = TEXT("morning");
	}
	else if (Tr && Tr->LifeHour() > 20.5f)
	{
		What = TEXT("evening");
	}
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
		Talk->SayLine(P->Body.Get(), FString::Printf(TEXT("line.transport.wait.%s.%d"), What, 1 + (int32)(((uint32)Clock / 3u) % 4u)));
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
	// the barista sells over the counter (T); the attendant shows the route diagram (E); each greets the player once a visit
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	int32 Best = -1;
	FPerson* BestP = nullptr;
	if (PC && UI && !UI->IsOpen() && Pawn->IsA<AHeroCharacter>() && !Pawn->IsHidden())
	{
		const FVector Eye = Pawn->GetPawnViewLocation();
		const FVector Look = PC->GetControlRotation().Vector();
		for (FPerson& P : People)
		{
			if (P.Role == TEXT("passenger") || !P.Body.IsValid())
			{
				continue;
			}
			const float Dist = FVector::Dist2D(Pawn->GetActorLocation(), P.At);
			const FVector To = (P.At + FVector(0, 0, 140.f) - Eye).GetSafeNormal();
			const bool bLooks = FVector::DotProduct(Look, To) > 0.6f;
			if (!P.bGreeted && Dist < 600.f && bLooks && FMath::Abs(Pawn->GetActorLocation().Z - P.At.Z) < 250.f)
			{
				P.bGreeted = true;
				if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
				{
					const int32 N = 1 + (int32)((Seed + (uint32)Clock) % 3);
					Talk->SayLine(P.Body.Get(), FString::Printf(TEXT("line.transport.%s.hello.%d"), *P.Role.ToString(), N));
				}
			}
			if (Dist < 330.f && bLooks)
			{
				Best = P.Role == TEXT("barista") ? 0 : 1;
				BestP = &P;
			}
		}
	}
	if (Best != Prompted && UI)
	{
		Prompted = Best;
		UI->SetPrompt(this, Best == 0 ? LyudText::Get(TEXT("ui.shop.prompt")) : Best == 1 ? LyudText::Get(TEXT("transport.hall.attendant_prompt")) : FText::GetEmpty());
	}
	if (!PC || Best < 0)
	{
		return;
	}
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
			UI->OpenShop(LyudText::Get(TEXT("ui.shop.title_station_buffet"), { FStringFormatArg(StationName.ToString()) }), Goods);
			Prompted = -1;
			UI->SetPrompt(this, FText::GetEmpty());
		}
	}
	else if (Best == 1 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		if (ULyudTransportSubsystem* T = ULyudTransportSubsystem::Get(this))
		{
			T->OpenDiagram(Station);
		}
		if (ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this))
		{
			Talk->SayLine(BestP->Body.Get(), FString::Printf(TEXT("line.transport.attendant.help.%d"), 1 + (int32)((uint32)Clock % 3)));
		}
		Prompted = -1;
		UI->SetPrompt(this, FText::GetEmpty());
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
	const bool bWantLights = D < LightsOn && FMath::Abs(Cam.Z - Middle.Z) < LevelBand * 2.f;
	if (bWantLights != bLightsOn)
	{
		SetLights(bWantLights);
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
		if (ANpcBody* B = P.Body.Get())
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
	TickPrompt(DeltaTime);
	TickBarks(DeltaTime, Cam);
}

void ALyudStationHall::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearPeople();
	Super::EndPlay(Reason);
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
	return FString::Printf(TEXT("hall %d %s: dressing %s, cores %s, %d lamps (%s), %d places, staff %d/%d, passengers %d (%d riders of the station life, %d in the hall by it, filler want %d)%s; open %s%s"),
		Station, *Style, Detail ? TEXT("yes") : TEXT("no"), Cores ? TEXT("yes") : TEXT("no"), Lights.Num(), bLightsOn ? TEXT("on") : TEXT("off"),
		Slots.Num(), Staff, StaffPlan.Num(), Pass, Riders, (int32)Here.size(), WantPassengers(), *Who, LifeOpen() ? TEXT("yes") : TEXT("no"),
		Tr && Tr->GetLife() ? *FString::Printf(TEXT(", %s"), UTF8_TO_TCHAR(Tr->GetLife()->Report(*Tr->GetNet()).c_str())) : TEXT(""));
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
