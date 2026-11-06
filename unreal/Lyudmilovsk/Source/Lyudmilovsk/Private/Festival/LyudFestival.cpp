#include "Festival/LyudFestival.h"

#include "FestivalCalendar.h"
#include "../People/PeopleSim.h"
#include "../Transport/LyudMeshKit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/Ephemeris.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Winter/LyudWinter.h"

static TAutoConsoleVariable<int32> CVarFestivalStub(TEXT("lyud.Festival.Stub"), 0,
	TEXT("1: send the people to the festivals by Order (the stand-in for UPeopleLifeSubsystem::SetGathering) even when the people's life has gatherings."));

static TAutoConsoleVariable<int32> CVarFestivals(TEXT("lyud.Festivals"), 1,
	TEXT("The town's holidays (ULyudFestivalSubsystem, Content/PeopleData/festivals.json): 1 on, 0 off."));

namespace
{
	const TCHAR* PaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
	const TCHAR* GlowPath = TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow");

	// the festive palette: flags, bulbs, the fireworks' colours
	const FLinearColor Palette[] = {
		FLinearColor(0.80f, 0.06f, 0.05f),      // red
		FLinearColor(0.95f, 0.70f, 0.05f),      // yellow
		FLinearColor(0.05f, 0.25f, 0.75f),      // blue
		FLinearColor(0.10f, 0.55f, 0.15f),      // green
		FLinearColor(0.90f, 0.88f, 0.82f),      // white
	};
	const FLinearColor FireColors[] = {
		FLinearColor(1.0f, 0.62f, 0.18f), FLinearColor(1.0f, 0.12f, 0.08f), FLinearColor(0.2f, 1.0f, 0.3f),
		FLinearColor(0.25f, 0.45f, 1.0f), FLinearColor(1.0f, 0.95f, 0.85f), FLinearColor(0.75f, 0.3f, 1.0f),
		FLinearColor(1.0f, 0.35f, 0.7f),
	};

	FDateTime ToDateTime(const LyudFest::FDate& D) { return FDateTime(D.Y, D.M, D.D); }
	LyudFest::FDate ToDate(const FDateTime& T) { return LyudFest::FDate{ T.GetYear(), T.GetMonth(), T.GetDay() }; }
	FString DateKey(const LyudFest::FDate& D) { return FString::Printf(TEXT("%04d-%02d-%02d"), D.Y, D.M, D.D); }
	/** Calendar seconds since 2000 (what the shows end by). */
	double CalSeconds(const UWorld* World) { return (UGameCalendar::NowIn(World) - FDateTime(2000, 1, 1)).GetTotalSeconds(); }

	uint32 Hash(const FString& S) { return GetTypeHash(S) | 1u; }
	float Rand01(uint32& S)
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xFFFFFF) / float(0x1000000);
	}

	/** A moving part: no shadows, nothing in Lumen or the distance fields, no collision (docs/perf_checklist.md 2). */
	void Lightweight(UStaticMeshComponent* C)
	{
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bAffectDynamicIndirectLighting = false;
		C->bAffectDistanceFieldLighting = false;
		C->SetVisibleInRayTracing(false);
		C->SetGenerateOverlapEvents(false);
	}

	UPointLightComponent* MakeLight(AActor* Owner, USceneComponent* Parent, const FLinearColor& Color, float Radius)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner);
		L->SetupAttachment(Parent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.f);
		L->SetLightColor(Color);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		L->bAffectsWorld = true;
		L->RegisterComponent();
		Owner->AddInstanceComponent(L);
		return L;
	}

	/** A sphere-ish lathe: an ellipsoid of radius R and height H standing on F's origin. */
	void Ball(FLyudMeshKit& K, FName Mat, const FVector& Centre, float R, float H, int32 Sides = 8)
	{
		TArray<FVector2D> P;
		const int32 N = 6;
		for (int32 I = 0; I <= N; I++)
		{
			const float A = PI * I / N;
			P.Add(FVector2D(FMath::Sin(A) * R, H * 0.5f - FMath::Cos(A) * H * 0.5f));
		}
		TArray<FName> M;
		M.Init(Mat, P.Num() - 1);
		K.Lathe(FTransform(Centre - FVector(0.f, 0.f, H * 0.5f)), P, M, Sides, false, false);
	}

	/** Points along a hanging line from A to B with a sag in the middle. */
	FVector Hang(const FVector& A, const FVector& B, float Sag, float T)
	{
		return A + (B - A) * T - FVector(0.f, 0.f, 4.f * Sag * T * (1.f - T));
	}

	// the people's life gets the gatherings when it has them (the town-life branch: SetGathering, ClearGathering,
	// SetDayOff); before that the festivals still dress the town and put on their shows
	template <class T>
	auto TrySetGathering(T* P, int, FName Id, FName Place, float From, float To, float Pull, FName Mode, const FVector& Face)
		-> decltype(P->SetGathering(Id, Place, From, To, Pull, Mode, Face), bool())
	{
		P->SetGathering(Id, Place, From, To, Pull, Mode, Face);
		return true;
	}
	template <class T>
	bool TrySetGathering(T*, long, FName, FName, float, float, float, FName, const FVector&) { return false; }

	template <class T>
	auto TryClearGathering(T* P, int, FName Id) -> decltype(P->ClearGathering(Id), bool())
	{
		P->ClearGathering(Id);
		return true;
	}
	template <class T>
	bool TryClearGathering(T*, long, FName) { return false; }

	template <class T>
	auto TrySetDayOff(T* P, int, bool bOff) -> decltype(P->SetDayOff(bOff), bool())
	{
		P->SetDayOff(bOff);
		return true;
	}
	template <class T>
	bool TrySetDayOff(T*, long, bool) { return false; }

	/** Kinds to look for when a venue's own kind isn't in the plans yet, best first. */
	TArray<FString> VenueKinds(const FString& Want)
	{
		if (Want == TEXT("square")) return { TEXT("square"), TEXT("town_square") };
		if (Want == TEXT("fair")) return { TEXT("fair"), TEXT("market"), TEXT("square"), TEXT("town_square") };
		if (Want == TEXT("pier")) return { TEXT("pier"), TEXT("beach"), TEXT("promenade"), TEXT("promenade_bench") };
		if (Want == TEXT("beach")) return { TEXT("beach"), TEXT("promenade"), TEXT("promenade_bench") };
		if (Want == TEXT("promenade")) return { TEXT("promenade"), TEXT("embankment"), TEXT("promenade_bench"), TEXT("beach") };
		if (Want == TEXT("park")) return { TEXT("park"), TEXT("square"), TEXT("town_square"), TEXT("bench") };
		if (Want == TEXT("sunrise_point")) return { TEXT("sunrise_point"), TEXT("lookout"), TEXT("beach") };
		return { Want };
	}

	/** A position in a plan's spot: fields in cm (…cm) or in metres. */
	bool SpotPos(const TSharedPtr<FJsonObject>& O, FVector& Out)
	{
		auto Read = [&](const TSharedPtr<FJsonValue>& V, float Scale) -> bool
		{
			if (!V.IsValid())
			{
				return false;
			}
			const TArray<TSharedPtr<FJsonValue>>* A;
			if (V->TryGetArray(A) && A->Num() >= 2)
			{
				Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0) * Scale;
				return true;
			}
			const TSharedPtr<FJsonObject>* Obj;
			if (V->TryGetObject(Obj) && (*Obj)->HasField(TEXT("x")))
			{
				Out = FVector((*Obj)->GetNumberField(TEXT("x")), (*Obj)->GetNumberField(TEXT("y")),
					(*Obj)->HasField(TEXT("z")) ? (*Obj)->GetNumberField(TEXT("z")) : 0.0) * Scale;
				return true;
			}
			return false;
		};
		for (const TCHAR* K : { TEXT("ue_cm"), TEXT("pos_cm"), TEXT("center_cm"), TEXT("at_cm"), TEXT("cm") })
		{
			if (Read(O->TryGetField(K), 1.f)) return true;
		}
		for (const TCHAR* K : { TEXT("ue"), TEXT("ue_m"), TEXT("pos"), TEXT("center"), TEXT("at") })
		{
			if (Read(O->TryGetField(K), 100.f)) return true;
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the subsystem

ULyudFestivalSubsystem::ULyudFestivalSubsystem() = default;
ULyudFestivalSubsystem::~ULyudFestivalSubsystem() = default;

ULyudFestivalSubsystem* ULyudFestivalSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudFestivalSubsystem>() : nullptr;
}

bool ULyudFestivalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudFestivalSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudFestivalSubsystem, STATGROUP_Tickables);
}

void ULyudFestivalSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarFestivals.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("PeopleData/festivals.json");
	Calendar = MakePimpl<LyudFest::FCalendar>();
	if (!FFileHelper::LoadFileToString(Text, *Path) || !Calendar->Load(TCHAR_TO_UTF8(*Text)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Festivals: can't read %s: %s"), *Path, UTF8_TO_TCHAR(Calendar->LastError.c_str()));
		Calendar.Reset();
		return;
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Festivals"), this);
	}
	bRunning = true;
	Timer = 1.5f;       // after the people's life has started
	UE_LOG(LogTemp, Display, TEXT("Festivals: %d in the calendar"), (int32)Calendar->Festivals.size());
}

void ULyudFestivalSubsystem::Deinitialize()
{
	for (auto& It : Decor)
	{
		if (IsValid(It.Value)) It.Value->Destroy();
	}
	for (auto& It : Shows)
	{
		if (IsValid(It.Value)) It.Value->Destroy();
	}
	Decor.Empty();
	Shows.Empty();
	Music.Empty();
	bRunning = false;
	Super::Deinitialize();
}

void ULyudFestivalSubsystem::Tick(float DeltaTime)
{
	Timer -= DeltaTime;
	if (Timer > 0.f)
	{
		return;
	}
	Timer = 2.f;
	Update();
}

void ULyudFestivalSubsystem::LoadFired()
{
	if (!Calendar)
	{
		return;
	}
	Calendar->Fired.clear();
	for (const FString& S : FiredTriggers)
	{
		FString Trigger, Day;
		LyudFest::FDate D;
		if (S.Split(TEXT("|"), &Trigger, &Day) && FCString::Sscanf(*Day, TEXT("%d-%d-%d"), &D.Y, &D.M, &D.D) == 3)
		{
			Calendar->Fire(TCHAR_TO_UTF8(*Trigger), D);
		}
	}
}

void ULyudFestivalSubsystem::Fire(FName Trigger)
{
	if (!Calendar)
	{
		return;
	}
	const FString Key = Trigger.ToString() + TEXT("|") + DateKey(ToDate(UGameCalendar::NowIn(GetWorld())));
	FiredTriggers.AddUnique(Key);
	LoadFired();
	UE_LOG(LogTemp, Display, TEXT("Festivals: fired %s"), *Key);
	Timer = 0.f;
}

void ULyudFestivalSubsystem::CheckFirstSnow(const FDateTime& Now)
{
	// the first snow that lies white on the ground in a winter (October to April) fires first_snow once a winter
	const int32 M = Now.GetMonth();
	if (M > 4 && M < 10)
	{
		return;
	}
	if (ULyudWinterSubsystem::SnowOnGround(GetWorld(), 0.f) < 0.35f)
	{
		return;
	}
	const int32 Winter = M >= 10 ? Now.GetYear() : Now.GetYear() - 1;
	for (const FString& S : FiredTriggers)
	{
		int32 Y = 0, Mo = 0, D = 0;
		if (S.StartsWith(TEXT("first_snow|")) && FCString::Sscanf(*S.Mid(11), TEXT("%d-%d-%d"), &Y, &Mo, &D) == 3 && (Mo >= 10 ? Y : Y - 1) == Winter)
		{
			return;
		}
	}
	Fire(TEXT("first_snow"));
}

float ULyudFestivalSubsystem::Night() const
{
	for (TActorIterator<ALyudSky> It(GetWorld()); It; ++It)
	{
		return FMath::Clamp((3.f - It->SunAltitude()) / 9.f, 0.f, 1.f);
	}
	const float H = UGameCalendar::NowIn(GetWorld()).GetTimeOfDay().GetTotalHours();
	return (H < 7.f || H > 18.f) ? 1.f : 0.f;
}

void ULyudFestivalSubsystem::Update()
{
	if (!Calendar)
	{
		return;
	}
	UWorld* World = GetWorld();
	int32 FiredCount = 0;
	for (const auto& It : Calendar->Fired) FiredCount += (int32)It.second.size();
	if (FiredCount != FiredTriggers.Num())
	{
		LoadFired();     // the save restored them after BeginPlay
	}
	const FDateTime Now = UGameCalendar::NowIn(World);
	CheckFirstSnow(Now);
	const LyudFest::FDate Today = ToDate(Now);
	const float Hour = (float)Now.GetTimeOfDay().GetTotalHours();
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(World);

	// public holiday
	std::string OffId;
	const bool bOff = Calendar->IsDayOff(Today, &OffId);
	if (bOff != bDayOff || !bDayOffSent)
	{
		bDayOff = bOff;
		bDayOffSent = People && TrySetDayOff(People, 0, bOff);
	}

	TMap<FName, TArray<FString>> WantDecor;     // festival.venue -> kinds
	TMap<FName, FLyudVenue> DecorVenue;
	TSet<FName> PartyDecor;
	TSet<FName> WantGather, WantPhases;
	TMap<FName, FString> WantShows;             // key -> show
	TMap<FName, double> ShowEnds;
	TMap<FName, FName> ShowVenue;
	TMap<FString, int32> Claimed;               // venue -> festival
	TArray<FLyudFestivalNow> NewActive;
	const double NowCal = CalSeconds(World);

	for (const LyudFest::FActive& A : Calendar->At(Today, Hour))
	{
		const LyudFest::FFestival& F = Calendar->Festivals[A.Festival];
		const FString FId = UTF8_TO_TCHAR(F.Id.c_str());
		auto Claim = [&](const FString& V) -> bool
		{
			if (const int32* By = Claimed.Find(V))
			{
				return *By == A.Festival;
			}
			Claimed.Add(V, A.Festival);
			return true;
		};
		// decorations
		TArray<FString> DV;
		for (const std::string& V : F.DecorVenues) DV.AddUnique(UTF8_TO_TCHAR(V.c_str()));
		if (DV.Num() == 0)
		{
			for (const LyudFest::FPhase& P : F.Phases) DV.AddUnique(UTF8_TO_TCHAR(P.Venue.c_str()));
		}
		bool bAny = false;
		for (const FString& V : DV)
		{
			if (!Claim(V) || F.Decor.empty())
			{
				continue;
			}
			const FLyudVenue Ve = FindVenue(*V);
			if (!Ve.bValid)
			{
				continue;
			}
			TArray<FString> Kinds;
			for (const std::string& K : F.Decor) Kinds.Add(UTF8_TO_TCHAR(K.c_str()));
			// the stage and the tree belong on the square, stalls on the fair (and wherever the festival has nothing else)
			const FName Key(*(FId + TEXT(".") + V));
			WantDecor.Add(Key, Kinds);
			DecorVenue.Add(Key, Ve);
			bAny = true;
		}
		// the program
		FLyudFestivalNow Entry;
		Entry.Id = *FId;
		Entry.Name = FestivalName(Entry.Id);
		Entry.Hours = A.Rel;
		for (const int32 Pi : A.Phases)
		{
			const LyudFest::FPhase& P = F.Phases[Pi];
			const FString V = UTF8_TO_TCHAR(P.Venue.c_str());
			if (!Claim(V))
			{
				continue;
			}
			const FString PId = UTF8_TO_TCHAR(P.Id.c_str());
			const FName Key(*(FId + TEXT(".") + PId));
			bAny = true;
			if (Entry.Phase.IsNone())
			{
				Entry.Phase = *PId;
				Entry.Venue = *V;
			}
			WantPhases.Add(Key);
			const FLyudVenue Ve = FindVenue(*V);
			// the phase's day's 00:00 in calendar seconds: its end is when the shows stop
			const int32 Day = (int32)FMath::FloorToFloat(A.Rel / 24.f);
			const int32 PhaseDay = P.Day >= 0 ? P.Day : Day;
			const double DayStart = (ToDateTime(A.Start) + FTimespan::FromDays(PhaseDay) - FDateTime(2000, 1, 1)).GetTotalSeconds();
			const double Ends = DayStart + P.To * 3600.0;
			if (Ve.bValid)
			{
				WantGather.Add(Key);
				if (!Gatherings.Contains(Key) && People)
				{
					const FVector Face = FacePoint(Ve, UTF8_TO_TCHAR(P.Face.c_str()), Now);
					bHasGatherings = TrySetGathering(People, 0, Key, *V, FMath::Fmod(P.From, 24.f), FMath::Fmod(P.To, 24.f), P.Pull, FName(UTF8_TO_TCHAR(P.Mode.c_str())), Face);
				}
				if (People && (!bHasGatherings || CVarFestivalStub.GetValueOnGameThread() > 0))
				{
					StubGather(People, Key, Ve, UTF8_TO_TCHAR(P.Mode.c_str()), P.Pull);
				}
			}
			for (const std::string& S : P.Show)
			{
				const FString Show = UTF8_TO_TCHAR(S.c_str());
				if (Show == TEXT("stage_lights"))
				{
					for (const auto& D : WantDecor)
					{
						if (D.Key.ToString().StartsWith(FId + TEXT(".")))
						{
							PartyDecor.Add(D.Key);
						}
					}
					continue;
				}
				const FName ShowKey(*(Key.ToString() + TEXT(".") + Show));
				WantShows.Add(ShowKey, Show);
				ShowEnds.Add(ShowKey, Ends);
				ShowVenue.Add(ShowKey, *V);
			}
		}
		// the shows' "soon" notes: ten minutes before
		for (const LyudFest::FPhase& P : F.Phases)
		{
			for (int32 Day = 0; Day < F.Days; Day++)
			{
				if (P.Day >= 0 && P.Day != Day) continue;
				const float From = Day * 24.f + P.From;
				if (A.Rel >= From - 0.17f && A.Rel < From && !P.Show.empty())
				{
					const FString Show = UTF8_TO_TCHAR(P.Show[0].c_str());
					if (Show == TEXT("fireworks") || Show == TEXT("sky_lanterns") || Show == TEXT("bonfire"))
					{
						const FString Mark = DateKey(Today) + TEXT("|") + FId + TEXT(".") + UTF8_TO_TCHAR(P.Id.c_str());
						if (!Announced.Contains(Mark))
						{
							Announced.Add(Mark);
							if (ULyudGameScreen* Screen = ULyudGameScreen::Get(World))
							{
								Screen->Toast(LyudText::Get(TEXT("festival.soon.") + Show));
							}
						}
					}
				}
			}
		}
		if (bAny)
		{
			NewActive.Add(Entry);
			if (Hour >= 7.f && Hour < 22.f && A.Rel >= 0.f)
			{
				Announce(DateKey(Today), FId);
			}
		}
	}
	Active = NewActive;

	// decorations: up / down
	for (auto It = Decor.CreateIterator(); It; ++It)
	{
		if (!WantDecor.Contains(It.Key()) || !IsValid(It.Value()))
		{
			if (IsValid(It.Value())) It.Value()->Destroy();
			It.RemoveCurrent();
		}
	}
	const float NightNow = Night();
	for (const auto& W : WantDecor)
	{
		if (!Decor.Contains(W.Key))
		{
			FActorSpawnParameters SP;
			SP.ObjectFlags |= RF_Transient;
			const FLyudVenue& Ve = DecorVenue[W.Key];
			if (ALyudFestivalDecor* D = World->SpawnActor<ALyudFestivalDecor>(FVector::ZeroVector, FRotator::ZeroRotator, SP))
			{
				D->Build(Ve, W.Value, Hash(W.Key.ToString()));
				Decor.Add(W.Key, D);
			}
		}
		if (ALyudFestivalDecor* D = Cast<ALyudFestivalDecor>(Decor.FindRef(W.Key)))
		{
			D->SetLights(NightNow, PartyDecor.Contains(W.Key));
		}
	}

	// gatherings
	for (auto It = Gatherings.CreateIterator(); It; ++It)
	{
		if (!WantGather.Contains(*It))
		{
			if (People) TryClearGathering(People, 0, *It);
			It.RemoveCurrent();
		}
	}
	Gatherings.Append(WantGather);
	for (auto It = StubOrdered.CreateIterator(); It; ++It)
	{
		FString Phase, Who;
		It->Split(TEXT("|"), &Phase, &Who);
		if (!WantGather.Contains(FName(*Phase)))
		{
			It.RemoveCurrent();
		}
	}

	// phase events
	for (const FName& K : WantPhases)
	{
		if (!RunningPhases.Contains(K))
		{
			FString FId, PId;
			K.ToString().Split(TEXT("."), &FId, &PId);
			OnPhaseStart.Broadcast(*FId, *PId);
			ULyudQuests::Notify(this, TEXT("festival"), FId);
			UE_LOG(LogTemp, Display, TEXT("Festivals: %s starts"), *K.ToString());
		}
	}
	for (const FName& K : RunningPhases)
	{
		if (!WantPhases.Contains(K))
		{
			FString FId, PId;
			K.ToString().Split(TEXT("."), &FId, &PId);
			OnPhaseEnd.Broadcast(*FId, *PId);
			UE_LOG(LogTemp, Display, TEXT("Festivals: %s ends"), *K.ToString());
		}
	}
	RunningPhases = WantPhases;

	// shows: the self-running ones (fireworks, lanterns, the bonfire) end by their own clock; the music stops with its phase
	for (auto It = Shows.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Value()))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = Music.CreateIterator(); It; ++It)
	{
		if (!WantShows.Contains(It.Key()) || !IsValid(It.Value()))
		{
			if (IsValid(It.Value()))
			{
				It.Value()->FadeOut(4.f, 0.f);
				if (AActor* Holder = It.Value()->GetOwner())
				{
					Holder->SetLifeSpan(5.f);
				}
			}
			It.RemoveCurrent();
		}
	}
	for (const auto& W : WantShows)
	{
		if (Shows.Contains(W.Key) || Music.Contains(W.Key))
		{
			continue;
		}
		const FLyudVenue Ve = FindVenue(ShowVenue[W.Key]);
		if (!Ve.bValid)
		{
			continue;
		}
		if (W.Value == TEXT("music"))
		{
			StartMusic(W.Key, Ve);
		}
		else if (ShowEnds[W.Key] > NowCal)
		{
			StartShow(W.Key, W.Value, Ve, ShowEnds[W.Key]);
		}
	}
}

void ULyudFestivalSubsystem::StubGather(UPeopleLifeSubsystem* People, FName Key, const FLyudVenue& V, const FString& Mode, float Pull)
{
	// Until the people's life has gatherings of its own (SetGathering), the festival sends some of the people who are free
	// now to a place of theirs nearest the venue, with an activity that fits the phase's mode: they talk in groups on the
	// square, look at the show from the beach, sit on the benches. Each person is asked once a phase, by a fixed chance.
	const LyudPeople::FWorld* Sim = People->GetSim();
	if (!Sim)
	{
		return;
	}
	struct FFit { const char* Activity; TArray<FString> Kinds; };
	FFit Fit = { "chat", { TEXT("square"), TEXT("town_square") } };
	if (Mode == TEXT("watch"))
	{
		Fit = { "sun_watch", { TEXT("beach"), TEXT("square"), TEXT("promenade_bench"), TEXT("lookout") } };
	}
	else if (Mode == TEXT("sit"))
	{
		Fit = { "sit_bench", { TEXT("promenade_bench"), TEXT("bench"), TEXT("town_bench") } };
	}
	if (Sim->FindActivity(Fit.Activity) < 0)
	{
		return;
	}
	// the people's places of those kinds nearest the venue (a few, so a crowd spreads over them)
	TArray<TPair<float, FString>> Near;
	for (const LyudPeople::FPlace& Pl : Sim->Places)
	{
		if (Pl.Island == 0 && Fit.Kinds.Contains(FString(UTF8_TO_TCHAR(Pl.Kind.c_str()))))
		{
			const float D = FVector::Dist2D(FVector(Pl.Portal.X, Pl.Portal.Y, 0.f) * 100.f, V.Center);
			Near.Add({ D, UTF8_TO_TCHAR(Pl.Id.c_str()) });
		}
	}
	if (Near.Num() == 0)
	{
		return;
	}
	Near.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) { return A.Key < B.Key; });
	const float Chance = FMath::Clamp(Pull * 0.45f, 0.05f, 0.9f);
	int32 Sent = 0;
	for (int32 Id = 0; Id < (int32)Sim->People.size(); Id++)
	{
		const LyudPeople::FPerson& P = Sim->People[Id];
		const FString Mark = Key.ToString() + TEXT("|") + UTF8_TO_TCHAR(P.Key.c_str());
		if (StubOrdered.Contains(Mark))
		{
			continue;
		}
		// only someone doing something of their own free time (not asleep, at work, at school, away or on a ride)
		const bool bBusy = P.State == LyudPeople::EState::Riding || P.State == LyudPeople::EState::Away ||
			P.State == LyudPeople::EState::Waiting || P.bTrain || P.Island != 0 ||
			(P.Activity >= 0 && !Sim->Activities[P.Activity].bFree);
		if (bBusy)
		{
			continue;
		}
		StubOrdered.Add(Mark);
		uint32 H = Hash(Mark);
		if (Rand01(H) > Chance * (P.bChild ? 0.8f : 1.f))
		{
			continue;
		}
		for (int32 I = 0; I < FMath::Min(3, Near.Num()); I++)
		{
			if (People->Order(Id, UTF8_TO_TCHAR(Fit.Activity), Near[(I + Id) % FMath::Min(3, Near.Num())].Value))
			{
				Sent++;
				break;
			}
		}
	}
	if (Sent > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("Festivals: %d people sent to %s (%s at %s)"), Sent, *Key.ToString(), UTF8_TO_TCHAR(Fit.Activity), *Near[0].Value);
	}
}

void ULyudFestivalSubsystem::Announce(const FString& Day, const FString& Id)
{
	const FString Mark = Day + TEXT("|") + Id;
	if (Announced.Contains(Mark))
	{
		return;
	}
	Announced.Add(Mark);
	// keep the list short: the last few weeks are enough not to repeat a note
	if (Announced.Num() > 120)
	{
		Announced.RemoveAt(0, Announced.Num() - 120);
	}
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(GetWorld()))
	{
		Screen->Toast(LyudText::Get(TEXT("festival.") + Id + TEXT(".today")));
	}
}

FVector ULyudFestivalSubsystem::FacePoint(const FLyudVenue& V, const FString& Face, const FDateTime& Now) const
{
	const FVector Fwd = FRotator(0.f, V.Yaw, 0.f).Vector();
	if (Face == TEXT("stage"))
	{
		return V.Center + Fwd * V.Radius * 0.65f + FVector(0.f, 0.f, 200.f);
	}
	if (Face == TEXT("tree"))
	{
		return V.Center - Fwd * V.Radius * 0.55f + FVector(0.f, 0.f, 300.f);
	}
	if (Face == TEXT("sea"))
	{
		return V.Center + V.SeaDir * 20000.f + FVector(0.f, 0.f, 9000.f);
	}
	if (Face == TEXT("bonfire"))
	{
		return V.Center + V.SeaDir * 600.f + FVector(0.f, 0.f, 100.f);
	}
	if (Face == TEXT("sky"))
	{
		return V.Center + V.SeaDir * 6000.f + FVector(0.f, 0.f, 30000.f);
	}
	if (Face == TEXT("sunrise"))
	{
		// where the sun comes up that morning (the club's latitude; local time UTC+2 like the people's sun times)
		const FDateTime Midnight(Now.GetYear(), Now.GetMonth(), Now.GetDay());
		double Prev = -90.0;
		for (int32 M = 0; M <= 12 * 60; M += 4)
		{
			const FSkyPosition Sun = LyudEphemeris::Sun(Midnight + FTimespan::FromMinutes(M) - FTimespan::FromHours(2), 54.96, 20.40);
			if (Prev < -0.83 && Sun.AltitudeDeg >= -0.83)
			{
				const double A = FMath::DegreesToRadians(Sun.AzimuthDeg);
				return V.Center + FVector(-FMath::Sin(A), FMath::Cos(A), 0.f) * 50000.f + FVector(0.f, 0.f, 500.f);
			}
			Prev = Sun.AltitudeDeg;
		}
	}
	return FVector::ZeroVector;
}

void ULyudFestivalSubsystem::StartMusic(FName Key, const FLyudVenue& V)
{
	// a festival tune (audio/music/festival_<n>, tools/gen_sounds.py), a different one per festival where there are two
	USoundBase* Sound = nullptr;
	const int32 First = (Hash(Key.ToString()) >> 3) & 1;
	for (int32 I = 0; I < 2 && !Sound; I++)
	{
		const FString N = FString::Printf(TEXT("MU_festival_%d"), 1 + (First + I) % 2);
		Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Music/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Sound)
	{
		return;     // no festival music imported yet: the festival is quiet
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	AActor* Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), V.Center + FVector(0.f, 0.f, 300.f), FRotator::ZeroRotator, SP);
	if (!Holder)
	{
		return;
	}
	UAudioComponent* A = NewObject<UAudioComponent>(Holder);
	Holder->SetRootComponent(A);
	A->SetWorldLocation(V.Center + FRotator(0.f, V.Yaw, 0.f).Vector() * V.Radius * 0.6f + FVector(0.f, 0.f, 300.f));
	A->SetSound(Sound);
	A->bOverrideAttenuation = true;
	A->AttenuationOverrides.bAttenuate = true;
	A->AttenuationOverrides.bSpatialize = true;
	A->AttenuationOverrides.AttenuationShapeExtents = FVector(V.Radius * 1.2f, 0.f, 0.f);
	A->AttenuationOverrides.FalloffDistance = 14000.f;
	A->bIsUISound = false;
	A->RegisterComponent();
	A->FadeIn(4.f, 0.8f);
	Music.Add(Key, A);
}

void ULyudFestivalSubsystem::StartShow(FName Key, const FString& Show, const FLyudVenue& V, double EndsAt)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	AActor* Made = nullptr;
	if (Show == TEXT("fireworks"))
	{
		// launched from a barge 120 m out to sea from the venue
		FVector At = V.Center + V.SeaDir * 12000.f;
		float SeaZ = V.Center.Z;
		for (TActorIterator<ALyudSea> It(World); It; ++It)
		{
			SeaZ = It->SeaLevel();
			break;
		}
		At.Z = SeaZ + 150.f;
		if (ALyudFireworks* F = World->SpawnActor<ALyudFireworks>(At, FRotator::ZeroRotator, SP))
		{
			F->Setup(At, V.SeaDir, EndsAt);
			Made = F;
		}
	}
	else if (Show == TEXT("sky_lanterns"))
	{
		const FVector At = Ground(V.Center + V.SeaDir * 400.f);
		if (ALyudSkyLanterns* L = World->SpawnActor<ALyudSkyLanterns>(At, FRotator::ZeroRotator, SP))
		{
			L->Setup(At, V.SeaDir, EndsAt);
			Made = L;
		}
	}
	else if (Show == TEXT("bonfire"))
	{
		const FVector At = Ground(V.Center + V.SeaDir * 600.f);
		if (ALyudBonfire* B = World->SpawnActor<ALyudBonfire>(At, FRotator::ZeroRotator, SP))
		{
			B->Setup(At, Key.ToString().StartsWith(TEXT("maslenitsa.")), EndsAt);
			Made = B;
		}
	}
	if (Made)
	{
		Shows.Add(Key, Made);
		UE_LOG(LogTemp, Display, TEXT("Festivals: show %s at %s"), *Key.ToString(), *Made->GetActorLocation().ToString());
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// venues

FVector ULyudFestivalSubsystem::Ground(const FVector& At) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("FestivalGround"), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, At + FVector(0.f, 0.f, 3000.f), At - FVector(0.f, 0.f, 8000.f), ECC_WorldStatic, Q))
	{
		return Hit.ImpactPoint;
	}
	return At;
}

FVector ULyudFestivalSubsystem::SeaDirAt(const FVector& At) const
{
	const ALyudSea* Sea = nullptr;
	for (TActorIterator<ALyudSea> It(GetWorld()); It; ++It)
	{
		Sea = *It;
		break;
	}
	if (!Sea)
	{
		return FVector(1.f, 0.f, 0.f);     // the map's west (docs/world_map.md)
	}
	const float Z = Sea->HighWaterZ();
	FVector Sum = FVector::ZeroVector;
	for (const float Dist : { 6000.f, 15000.f, 30000.f, 60000.f })
	{
		for (int32 I = 0; I < 16; I++)
		{
			const FVector D = FRotator(0.f, I * 22.5f, 0.f).Vector();
			if (Sea->DepthAtLevel(At + D * Dist, Z) > 100.f)
			{
				Sum += D;
			}
		}
		if (!Sum.IsNearlyZero())
		{
			break;
		}
	}
	return Sum.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Sum.GetSafeNormal2D();
}

bool ULyudFestivalSubsystem::ResolveFromPlan(const FString& File, const FString& Want, FLyudVenue& Out) const
{
	FString Text;
	TSharedPtr<FJsonObject> Doc;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / File)) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
	{
		return false;
	}
	TArray<TSharedPtr<FJsonObject>> Spots;
	const TArray<TSharedPtr<FJsonValue>>* Arr;
	const TSharedPtr<FJsonObject>* Map;
	if (Doc->TryGetArrayField(TEXT("spots"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			if (V->Type == EJson::Object) Spots.Add(V->AsObject());
		}
	}
	else if (Doc->TryGetObjectField(TEXT("spots"), Map))
	{
		for (const auto& It : (*Map)->Values)
		{
			if (It.Value->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> O = It.Value->AsObject();
				if (!O->HasField(TEXT("id"))) O->SetStringField(TEXT("id"), It.Key);
				Spots.Add(O);
			}
		}
	}
	// an id first, then the kinds best first
	TArray<FString> Kinds = VenueKinds(Want);
	for (int32 Pass = 0; Pass <= Kinds.Num(); Pass++)
	{
		for (const TSharedPtr<FJsonObject>& O : Spots)
		{
			FString Id, Kind;
			O->TryGetStringField(TEXT("id"), Id);
			if (!O->TryGetStringField(TEXT("kind"), Kind)) O->TryGetStringField(TEXT("type"), Kind);
			const bool bMatch = Pass == 0 ? Id == Want : Kind == Kinds[Pass - 1];
			FVector P;
			if (!bMatch || !SpotPos(O, P))
			{
				continue;
			}
			Out.Center = P;
			double Yaw = 0.0, R = 0.0;
			if (O->TryGetNumberField(TEXT("yaw"), Yaw)) Out.Yaw = (float)Yaw;
			if (O->TryGetNumberField(TEXT("radius_cm"), R)) Out.Radius = (float)R;
			else if (O->TryGetNumberField(TEXT("radius"), R)) Out.Radius = (float)R * 100.f;
			Out.Radius = FMath::Clamp(Out.Radius, 600.f, 4000.f);
			Out.Source = File + TEXT(":") + Id;
			return true;
		}
	}
	return false;
}

bool ULyudFestivalSubsystem::ResolveFromPeople(const FString& Want, FLyudVenue& Out) const
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(GetWorld());
	const LyudPeople::FWorld* Sim = People ? People->GetSim() : nullptr;
	if (!Sim)
	{
		return false;
	}
	for (const FString& Kind : VenueKinds(Want))
	{
		// the place of that kind nearest the middle of them all (the middle of the promenade, of the beach)
		const std::string K = TCHAR_TO_UTF8(*Kind);
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (const LyudPeople::FPlace& P : Sim->Places)
		{
			if ((P.Kind == K || P.Id == K) && P.Island == 0)
			{
				Sum += FVector(P.Portal.X, P.Portal.Y, P.Portal.Z) * 100.f;
				N++;
			}
		}
		if (N == 0)
		{
			continue;
		}
		const FVector Mid = Sum / N;
		float Best = 1e30f;
		for (const LyudPeople::FPlace& P : Sim->Places)
		{
			if ((P.Kind == K || P.Id == K) && P.Island == 0)
			{
				const FVector At(P.Portal.X * 100.f, P.Portal.Y * 100.f, P.Portal.Z * 100.f);
				if (FVector::DistSquared2D(At, Mid) < Best)
				{
					Best = FVector::DistSquared2D(At, Mid);
					Out.Center = At;
					Out.Yaw = P.Yaw;
					Out.Source = FString(TEXT("place:")) + UTF8_TO_TCHAR(P.Id.c_str());
				}
			}
		}
		Out.Radius = Kind.Contains(TEXT("square")) ? 1400.f : 1000.f;
		return true;
	}
	return false;
}

FLyudVenue ULyudFestivalSubsystem::FindVenue(FName Name)
{
	if (const FLyudVenue* Found = Venues.Find(Name))
	{
		if (Found->bValid)
		{
			return *Found;
		}
	}
	FLyudVenue V;
	V.Name = Name;
	const FString Want = Name.ToString();
	V.bValid = ResolveFromPlan(TEXT("City/city_plan.json"), Want, V) || ResolveFromPlan(TEXT("PeopleData/town_plan.json"), Want, V) ||
		ResolveFromPeople(Want, V);
	if (V.bValid)
	{
		V.Center = Ground(V.Center);
		V.SeaDir = SeaDirAt(V.Center);
		// a place with no yaw of its own faces the sea (the stage's back to it on the square: watchers look inland
		// at the stage, the sea behind them; on the beach and the pier the program faces the sea)
		if (FMath::IsNearlyZero(V.Yaw))
		{
			V.Yaw = V.SeaDir.Rotation().Yaw;
		}
		UE_LOG(LogTemp, Display, TEXT("Festivals: venue %s = %s at %s, radius %.0f m"), *Want, *V.Source, *V.Center.ToString(), V.Radius / 100.f);
		Venues.Add(Name, V);    // a missing venue is looked for again next time (the people's life may start later)
	}
	return V;
}

// ---------------------------------------------------------------------------------------------------------------------
// the API

bool ULyudFestivalSubsystem::IsFestivalToday() const
{
	if (!Calendar)
	{
		return false;
	}
	const LyudFest::FDate Today = ToDate(UGameCalendar::NowIn(GetWorld()));
	for (int32 F = 0; F < (int32)Calendar->Festivals.size(); F++)
	{
		const LyudFest::FFestival& Fe = Calendar->Festivals[F];
		for (int32 D = 0; D < Fe.Days; D++)
		{
			if (Calendar->StartsOn(F, LyudFest::AddDays(Today, -D)))
			{
				return true;
			}
		}
	}
	return false;
}

FName ULyudFestivalSubsystem::ActiveFestival() const
{
	return Active.Num() ? Active[0].Id : NAME_None;
}

FName ULyudFestivalSubsystem::ActivePhase() const
{
	return Active.Num() ? Active[0].Phase : NAME_None;
}

FText ULyudFestivalSubsystem::FestivalName(FName Id)
{
	return LyudText::Get(TEXT("festival.") + Id.ToString() + TEXT(".name"));
}

TArray<FString> ULyudFestivalSubsystem::Upcoming(int32 Days) const
{
	TArray<FString> Out;
	if (!Calendar)
	{
		return Out;
	}
	for (const auto& U : Calendar->Upcoming(ToDate(UGameCalendar::NowIn(GetWorld())), Days))
	{
		const LyudFest::FFestival& F = Calendar->Festivals[U.second];
		if (F.Rule != "weekly")
		{
			Out.Add(DateKey(U.first) + TEXT(" ") + UTF8_TO_TCHAR(F.Id.c_str()));
		}
	}
	return Out;
}

bool ULyudFestivalSubsystem::GoTo(FName Id, FName Phase)
{
	UGameCalendar* Cal = GetWorld() ? GetWorld()->GetSubsystem<UGameCalendar>() : nullptr;
	const int32 F = Calendar ? Calendar->Find(TCHAR_TO_UTF8(*Id.ToString())) : -1;
	if (!Cal || F < 0)
	{
		return false;
	}
	const LyudFest::FFestival& Fe = Calendar->Festivals[F];
	if (Fe.Rule == "trigger")
	{
		Fire(*FString(UTF8_TO_TCHAR(Fe.Trigger.c_str())));
	}
	LyudFest::FDate Start;
	if (!Calendar->Next(F, ToDate(UGameCalendar::NowIn(GetWorld())), Start))
	{
		return false;
	}
	float Hours = Fe.Phases.empty() ? 10.f : Fe.Phases[0].From - 1.f;
	for (const LyudFest::FPhase& P : Fe.Phases)
	{
		if (!Phase.IsNone() && Phase.ToString() == UTF8_TO_TCHAR(P.Id.c_str()))
		{
			Hours = (P.Day > 0 ? P.Day * 24.f : 0.f) + P.From + 0.02f;
		}
	}
	Cal->SetNow(ToDateTime(Start) + FTimespan::FromHours(Hours));
	Timer = 0.f;
	UE_LOG(LogTemp, Display, TEXT("Festivals: calendar moved to %s for %s"), *UGameCalendar::NowIn(GetWorld()).ToString(), *Id.ToString());
	return true;
}

FString ULyudFestivalSubsystem::Report() const
{
	if (!Calendar)
	{
		return TEXT("Festivals: not running");
	}
	FString S = FString::Printf(TEXT("Festivals: %s%s;"), *UGameCalendar::NowIn(GetWorld()).ToString(), bDayOff ? TEXT(" day off") : TEXT(""));
	for (const FLyudFestivalNow& A : Active)
	{
		S += FString::Printf(TEXT(" %s[%s@%s %.1fh]"), *A.Id.ToString(), *A.Phase.ToString(), *A.Venue.ToString(), A.Hours);
	}
	S += FString::Printf(TEXT(" decor %d, shows %d, music %d, gatherings %d"), Decor.Num(), Shows.Num(), Music.Num(), Gatherings.Num());
	for (const auto& V : Venues)
	{
		S += FString::Printf(TEXT("\n  venue %s = %s %s"), *V.Key.ToString(), *V.Value.Source, *V.Value.Center.ToString());
	}
	return S;
}

static FAutoConsoleCommandWithWorldAndArgs GFestivalList(TEXT("lyud.Festival.List"), TEXT("lyud.Festival.List [days]: the festivals of the next days"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFestivalSubsystem* F = ULyudFestivalSubsystem::Get(World))
		{
			for (const FString& S : F->Upcoming(Args.Num() ? FCString::Atoi(*Args[0]) : 365))
			{
				UE_LOG(LogTemp, Display, TEXT("%s"), *S);
			}
		}
	}));

static FAutoConsoleCommandWithWorld GFestivalReport(TEXT("lyud.Festival.Report"), TEXT("lyud.Festival.Report: what is on now"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ULyudFestivalSubsystem* F = ULyudFestivalSubsystem::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *F->Report());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFestivalGo(TEXT("lyud.Festival.Go"), TEXT("lyud.Festival.Go <id> [phase]: moves the calendar to the festival (sea_festival fireworks)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudFestivalSubsystem* F = ULyudFestivalSubsystem::Get(World);
		if (F && Args.Num() > 0 && !F->GoTo(*Args[0], Args.Num() > 1 ? FName(*Args[1]) : NAME_None))
		{
			UE_LOG(LogTemp, Warning, TEXT("Festivals: no festival %s"), *Args[0]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GFestivalFire(TEXT("lyud.Festival.Fire"), TEXT("lyud.Festival.Fire <trigger>: fires a trigger festival (first_snow)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudFestivalSubsystem* F = ULyudFestivalSubsystem::Get(World))
		{
			F->Fire(Args.Num() ? FName(*Args[0]) : FName(TEXT("first_snow")));
		}
	}));

// ---------------------------------------------------------------------------------------------------------------------
// decorations

ALyudFestivalDecor::ALyudFestivalDecor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UMaterialInterface* ALyudFestivalDecor::Mat(FName Name)
{
	if (TObjectPtr<UMaterialInterface>* Found = Mats.Find(Name))
	{
		return *Found;
	}
	const FString S = Name.ToString();
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* M = nullptr;
	auto Index = [&S]() { return FCString::Atoi(*S.Right(1)); };
	if (S.StartsWith(TEXT("glow")))
	{
		UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		M = UMaterialInstanceDynamic::Create(G ? G : Basic, this);
		FLinearColor C(1.f, 0.72f, 0.38f);
		float Base = 6.f, Min = 0.f;
		if (S.StartsWith(TEXT("glow_c")))
		{
			C = FireColors[Index() % UE_ARRAY_COUNT(FireColors)];
			Base = 8.f;
		}
		else if (S.StartsWith(TEXT("glow_paper")))
		{
			C = Index() % 2 ? FLinearColor(1.f, 0.25f, 0.08f) : FLinearColor(1.f, 0.55f, 0.15f);
			Base = 3.f;
			Min = 0.25f;        // paper by day, lit by night
		}
		else if (S == TEXT("glow_star"))
		{
			C = FLinearColor(1.f, 0.85f, 0.4f);
			Base = 25.f;
			Min = 0.1f;
		}
		else if (S == TEXT("glow_candle"))
		{
			C = FLinearColor(1.f, 0.6f, 0.25f);
			Base = 4.f;
		}
		else if (S.StartsWith(TEXT("glow_stage")))
		{
			C = FireColors[Index() % UE_ARRAY_COUNT(FireColors)];
			Base = 30.f;
			StageGlows.Add(M);
		}
		M->SetVectorParameterValue(TEXT("Color"), C);
		M->SetScalarParameterValue(TEXT("Intensity"), Base * Min);
		Glows.Add(M);
		GlowBase.Add(Base);
		GlowBase.Add(Min);
	}
	else
	{
		UMaterialInterface* P = LoadObject<UMaterialInterface>(nullptr, PaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		M = UMaterialInstanceDynamic::Create(P ? P : Basic, this);
		FLinearColor C(0.25f, 0.25f, 0.26f);
		float Rough = 0.6f, Metal = 0.f;
		if (S.StartsWith(TEXT("flag_")))
		{
			C = Palette[Index() % UE_ARRAY_COUNT(Palette)];
			Rough = 0.85f;
		}
		else if (S == TEXT("pole"))
		{
			C = FLinearColor(0.18f, 0.19f, 0.2f);
			Rough = 0.45f;
			Metal = 0.6f;
		}
		else if (S == TEXT("rope"))
		{
			C = FLinearColor(0.05f, 0.05f, 0.05f);
		}
		else if (S == TEXT("wood"))
		{
			C = FLinearColor(0.32f, 0.2f, 0.11f);
			Rough = 0.8f;
		}
		else if (S == TEXT("planks"))
		{
			C = FLinearColor(0.42f, 0.3f, 0.19f);
			Rough = 0.75f;
		}
		else if (S == TEXT("canvas_a"))
		{
			C = FLinearColor(0.75f, 0.08f, 0.06f);
			Rough = 0.9f;
		}
		else if (S == TEXT("canvas_b"))
		{
			C = FLinearColor(0.88f, 0.86f, 0.8f);
			Rough = 0.9f;
		}
		else if (S == TEXT("canvas_c"))
		{
			C = FLinearColor(0.08f, 0.38f, 0.2f);
			Rough = 0.9f;
		}
		else if (S == TEXT("backdrop"))
		{
			C = FLinearColor(0.03f, 0.04f, 0.08f);
			Rough = 0.95f;
		}
		else if (S == TEXT("fir"))
		{
			C = FLinearColor(0.02f, 0.12f, 0.05f);
			Rough = 0.9f;
		}
		else if (S == TEXT("straw"))
		{
			C = FLinearColor(0.62f, 0.5f, 0.24f);
			Rough = 0.95f;
		}
		M->SetVectorParameterValue(TEXT("Color"), C);
		M->SetScalarParameterValue(TEXT("Roughness"), Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), Metal);
	}
	Mats.Add(Name, M);
	return M;
}

FVector ALyudFestivalDecor::GroundAt(const FVector& P) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("FestivalDecor"), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0.f, 0.f, 1500.f), P - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q))
	{
		return Hit.ImpactPoint;
	}
	return P;
}

void ALyudFestivalDecor::Build(const FLyudVenue& V, const TArray<FString>& Kinds, uint32 Seed)
{
	const FVector C = V.Center;
	const float R = V.Radius;
	const FRotator Rot(0.f, V.Yaw, 0.f);
	const FVector Fwd = Rot.Vector(), Left = FRotator(0.f, V.Yaw + 90.f, 0.f).Vector();
	const bool bStage = Kinds.Contains(TEXT("stage")), bTree = Kinds.Contains(TEXT("tree"));
	FLyudMeshKit K, Glow;       // the solid pieces (collision, shadows) and the lights (no shadows)
	uint32 Rng = Seed;

	// the ring of poles the strings hang from (bunting, lanterns, wreaths)
	TArray<FVector> Tops;
	const bool bRing = Kinds.Contains(TEXT("bunting")) || Kinds.Contains(TEXT("lanterns")) || Kinds.Contains(TEXT("wreaths"));
	const int32 NPoles = 6;
	const float PoleH = 520.f;
	if (bRing)
	{
		for (int32 I = 0; I < NPoles; I++)
		{
			const float A = V.Yaw + 30.f + I * 360.f / NPoles;
			const FVector Base = GroundAt(C + FRotator(0.f, A, 0.f).Vector() * R * 0.95f);
			K.Rod(TEXT("pole"), Base - FVector(0.f, 0.f, 30.f), Base + FVector(0.f, 0.f, PoleH), 6.f, 8, true, false);
			K.CollideBox(FTransform(Base + FVector(0.f, 0.f, PoleH * 0.5f)), FVector(7.f, 7.f, PoleH * 0.5f));
			Ball(K, TEXT("pole"), Base + FVector(0.f, 0.f, PoleH + 6.f), 9.f, 12.f);
			Tops.Add(Base + FVector(0.f, 0.f, PoleH - 10.f));
		}
	}
	// the strings: round the ring, and three across it high over the middle (over the fountain)
	TArray<TPair<FVector, FVector>> Strings;
	for (int32 I = 0; I < Tops.Num(); I++)
	{
		Strings.Add({ Tops[I], Tops[(I + 1) % Tops.Num()] });
	}
	for (int32 I = 0; I < Tops.Num() / 2; I++)
	{
		Strings.Add({ Tops[I], Tops[I + Tops.Num() / 2] });
	}
	if (Kinds.Contains(TEXT("bunting")))
	{
		for (int32 Si = 0; Si < Strings.Num(); Si++)
		{
			const FVector A = Strings[Si].Key, B = Strings[Si].Value;
			const float Len = FVector::Dist(A, B), Sag = Len * (Si < Tops.Num() ? 0.06f : 0.09f);
			const int32 N = FMath::Max(4, FMath::CeilToInt(Len / 150.f));
			FVector Prev = A;
			for (int32 I = 1; I <= N; I++)
			{
				const FVector P = Hang(A, B, Sag, (float)I / N);
				K.Rod(TEXT("rope"), Prev, P, 0.6f, 4, false, false);
				Prev = P;
			}
			// a pennant every 42 cm: a triangle hanging from the line, both faces
			const int32 NF = FMath::FloorToInt(Len / 42.f);
			for (int32 I = 1; I < NF; I++)
			{
				const float T0 = (I - 0.36f) / NF, T1 = (I + 0.36f) / NF;
				const FVector P0 = Hang(A, B, Sag, T0), P1 = Hang(A, B, Sag, T1);
				const FVector Tip = (P0 + P1) * 0.5f - FVector(0.f, 0.f, 34.f) + FVector(Rand01(Rng) - 0.5f, Rand01(Rng) - 0.5f, 0.f) * 6.f;
				const FName M(*FString::Printf(TEXT("flag_%d"), (I + Si) % 5));
				K.Tri(M, P0, P1, Tip);
				K.Tri(M, P1, P0, Tip);
			}
		}
	}
	if (Kinds.Contains(TEXT("lanterns")))
	{
		// strings of bulbs a little under the bunting, and a paper lantern every 2.8 m
		for (int32 Si = 0; Si < Strings.Num(); Si++)
		{
			const FVector A = Strings[Si].Key - FVector(0.f, 0.f, 40.f), B = Strings[Si].Value - FVector(0.f, 0.f, 40.f);
			const float Len = FVector::Dist(A, B), Sag = Len * (Si < Tops.Num() ? 0.08f : 0.11f);
			const int32 N = FMath::Max(4, FMath::CeilToInt(Len / 150.f));
			FVector Prev = A;
			for (int32 I = 1; I <= N; I++)
			{
				const FVector P = Hang(A, B, Sag, (float)I / N);
				K.Rod(TEXT("rope"), Prev, P, 0.5f, 4, false, false);
				Prev = P;
			}
			const int32 NB = FMath::FloorToInt(Len / 55.f);
			for (int32 I = 1; I < NB; I++)
			{
				const FVector P = Hang(A, B, Sag, (float)I / NB) - FVector(0.f, 0.f, 5.f);
				if (I % 5 == 0)
				{
					K.Rod(TEXT("rope"), P, P - FVector(0.f, 0.f, 18.f), 0.4f, 4, false, false);
					Ball(Glow, *FString::Printf(TEXT("glow_paper_%d"), (I / 5) % 2), P - FVector(0.f, 0.f, 40.f), 17.f, 38.f, 10);
				}
				else
				{
					Glow.Box(*FString::Printf(TEXT("glow_c%d"), (I + Si) % 5), FTransform(P), FVector(2.6f, 2.6f, 3.5f), false);
				}
			}
		}
	}
	if (Kinds.Contains(TEXT("wreaths")))
	{
		// a fir wreath with a red bow on every pole, facing the middle
		for (const FVector& T : Tops)
		{
			const FVector In = (C - T).GetSafeNormal2D();
			const FVector At = T - FVector(0.f, 0.f, 200.f) + In * 12.f;
			TArray<FVector2D> Prof;
			for (int32 I = 0; I <= 8; I++)
			{
				const float A = 2.f * PI * I / 8;
				Prof.Add(FVector2D(32.f + FMath::Cos(A) * 9.f, FMath::Sin(A) * 9.f));
			}
			TArray<FName> M;
			M.Init(TEXT("fir"), Prof.Num() - 1);
			const FRotator Face = FRotationMatrix::MakeFromZ(In).Rotator();
			K.Lathe(FTransform(Face, At), Prof, M, 12, false, false);
			K.Box(TEXT("flag_0"), FTransform(Face, At - FVector(0.f, 0.f, 32.f) + In * 8.f), FVector(10.f, 4.f, 6.f), false);
		}
	}
	if (Kinds.Contains(TEXT("flags")))
	{
		// tall masts in an arc on the venue's near side, a flag on each
		for (int32 I = 0; I < 6; I++)
		{
			const float A = V.Yaw + 180.f + (I - 2.5f) * 14.f;
			const FVector Base = GroundAt(C + FRotator(0.f, A, 0.f).Vector() * R * 1.05f);
			const float H = 850.f;
			K.Rod(TEXT("pole"), Base - FVector(0.f, 0.f, 40.f), Base + FVector(0.f, 0.f, H), 5.f, 8, true, false);
			K.CollideBox(FTransform(Base + FVector(0.f, 0.f, H * 0.5f)), FVector(6.f, 6.f, H * 0.5f));
			const FVector Along = FRotator(0.f, A + 90.f, 0.f).Vector();
			const FVector T0 = Base + FVector(0.f, 0.f, H - 15.f), T1 = T0 - FVector(0.f, 0.f, 110.f);
			const FName M(*FString::Printf(TEXT("flag_%d"), I % 5));
			// a little wave in it: two panels at an angle
			const FVector Mid0 = T0 + Along * 85.f + FVector(Along.Y, -Along.X, 0.f) * 12.f, Mid1 = T1 + Along * 85.f + FVector(Along.Y, -Along.X, 0.f) * 12.f;
			const FVector E0 = T0 + Along * 170.f, E1 = T1 + Along * 170.f;
			K.Quad(M, T1, Mid1, Mid0, T0);
			K.Quad(M, T0, Mid0, Mid1, T1);
			K.Quad(M, Mid1, E1, E0, Mid0);
			K.Quad(M, Mid0, E0, E1, Mid1);
		}
	}
	if (bStage)
	{
		// a stage at the venue's far side facing the middle: a deck on trestles, steps, a truss with lights, a backdrop
		const FVector S = GroundAt(C + Fwd * R * 0.65f);
		const FRotator SR(0.f, V.Yaw + 180.f, 0.f);       // its front faces the middle
		const FTransform F(SR, S);
		K.BoxIn(TEXT("planks"), F, FVector(0.f, 0.f, 55.f), FVector(500.f, 900.f, 110.f), true);
		K.BoxIn(TEXT("planks"), F, FVector(310.f, 0.f, 18.f), FVector(60.f, 180.f, 36.f), true);
		K.BoxIn(TEXT("planks"), F, FVector(280.f, 0.f, 55.f), FVector(60.f, 180.f, 36.f), true);
		K.BoxIn(TEXT("planks"), F, FVector(250.f, 0.f, 90.f), FVector(60.f, 180.f, 30.f), true);
		const float TH = 560.f;
		for (const float Y : { -440.f, 440.f })
		{
			for (const float X : { -230.f, 230.f })
			{
				K.Rod(TEXT("pole"), F.TransformPosition(FVector(X, Y, 110.f)), F.TransformPosition(FVector(X, Y, 110.f + TH)), 5.f, 6, true, false);
			}
			K.Rod(TEXT("pole"), F.TransformPosition(FVector(-230.f, Y, 110.f + TH)), F.TransformPosition(FVector(230.f, Y, 110.f + TH)), 4.f, 6, true, false);
		}
		for (const float X : { -230.f, 230.f })
		{
			K.Rod(TEXT("pole"), F.TransformPosition(FVector(X, -440.f, 110.f + TH)), F.TransformPosition(FVector(X, 440.f, 110.f + TH)), 4.f, 6, true, false);
		}
		K.BoxIn(TEXT("backdrop"), F, FVector(-235.f, 0.f, 110.f + TH * 0.5f), FVector(4.f, 880.f, TH - 20.f), true);
		for (int32 I = 0; I < 10; I++)
		{
			const FVector L = F.TransformPosition(FVector(225.f, -405.f + I * 90.f, 110.f + TH - 18.f));
			Glow.Box(*FString::Printf(TEXT("glow_stage%d"), I % 7), FTransform(SR, L), FVector(9.f, 9.f, 9.f), false);
		}
		for (const float Y : { -300.f, 300.f })
		{
			UPointLightComponent* L = MakeLight(this, RootComponent, FLinearColor(1.f, 0.6f, 0.3f), 1600.f);
			L->SetWorldLocation(F.TransformPosition(FVector(300.f, Y, 500.f)));
			Lights.Insert(L, 0);
			NumStageLights++;
		}
	}
	if (Kinds.Contains(TEXT("stalls")))
	{
		// fair stalls round the venue, their fronts to the middle: a counter, four posts, a striped canopy
		TArray<float> Angles;
		if (bStage || bTree)
		{
			for (int32 I = 0; I < 5; I++) Angles.Add(V.Yaw + 125.f + I * 27.5f);
		}
		else
		{
			for (int32 I = 0; I < 8; I++) if (I != 4) Angles.Add(V.Yaw + I * 45.f);
		}
		for (int32 Si = 0; Si < Angles.Num(); Si++)
		{
			const FVector Dir = FRotator(0.f, Angles[Si], 0.f).Vector();
			const FVector Base = GroundAt(C + Dir * R * 0.78f);
			const FRotator SR = (-Dir).Rotation();     // facing the middle
			const FTransform F(FRotator(0.f, SR.Yaw, 0.f), Base);
			K.BoxIn(TEXT("planks"), F, FVector(0.f, 0.f, 50.f), FVector(90.f, 240.f, 100.f), true);
			K.BoxIn(TEXT("canvas_b"), F, FVector(2.f, 0.f, 101.f), FVector(96.f, 246.f, 3.f), false);
			for (const float X : { -110.f, 50.f })
			{
				for (const float Y : { -118.f, 118.f })
				{
					K.Rod(TEXT("wood"), F.TransformPosition(FVector(X, Y, 0.f)), F.TransformPosition(FVector(X, Y, X < 0.f ? 250.f : 225.f)), 3.5f, 6, true, false);
				}
			}
			K.BoxIn(TEXT("planks"), F, FVector(-110.f, 0.f, 150.f), FVector(4.f, 236.f, 200.f), true);
			// the canopy: stripes sloping down to the front, with a scalloped edge
			const FName CA = Si % 2 ? TEXT("canvas_a") : TEXT("canvas_c");
			const int32 NS = 8;
			for (int32 I = 0; I < NS; I++)
			{
				const float Y0 = -130.f + 260.f * I / NS, Y1 = -130.f + 260.f * (I + 1) / NS;
				const FName M = I % 2 ? TEXT("canvas_b") : CA;
				const FVector A0 = F.TransformPosition(FVector(-125.f, Y0, 262.f)), A1 = F.TransformPosition(FVector(-125.f, Y1, 262.f));
				const FVector B0 = F.TransformPosition(FVector(75.f, Y0, 228.f)), B1 = F.TransformPosition(FVector(75.f, Y1, 228.f));
				K.Quad(M, A0, B0, B1, A1);
				K.Quad(M, A1, B1, B0, A0);
				const FVector D = F.TransformPosition(FVector(78.f, (Y0 + Y1) * 0.5f, 200.f));
				K.Tri(M, B0, B1, D);
				K.Tri(M, B1, B0, D);
			}
			// a warm bulb under the canopy
			Glow.Box(TEXT("glow_warm"), FTransform(F.TransformPosition(FVector(-10.f, 0.f, 215.f))), FVector(5.f, 5.f, 6.f), false);
		}
	}
	if (bTree)
	{
		// the New Year tree opposite the stage: tiers of fir, a spiral of bulbs, a star
		const FVector T = GroundAt(C - Fwd * R * 0.55f);
		const float H = 950.f;
		K.Rod(TEXT("wood"), T, T + FVector(0.f, 0.f, 120.f), 16.f, 8, true, true);
		const int32 Tiers = 6;
		for (int32 I = 0; I < Tiers; I++)
		{
			const float Z0 = 100.f + I * (H - 150.f) / Tiers, Z1 = Z0 + (H - 100.f) / Tiers * 1.5f;
			const float R0 = 260.f * (1.f - (float)I / (Tiers + 0.5f)), R1 = R0 * 0.18f;
			const TArray<FVector2D> P = { { 0.f, Z0 }, { R0, Z0 + 20.f }, { R1, Z1 }, { 0.f, Z1 + 5.f } };
			const TArray<FName> M = { TEXT("fir"), TEXT("fir"), TEXT("fir") };
			K.Lathe(FTransform(T), P, M, 14, false, false);
		}
		K.CollideBox(FTransform(T + FVector(0.f, 0.f, 200.f)), FVector(120.f, 120.f, 200.f));
		for (int32 I = 0; I < 140; I++)
		{
			const float U = (float)I / 140;
			const float Z = 130.f + U * (H - 180.f);
			const float Rad = 255.f * (1.f - U) * 0.92f + 15.f;
			const float A = U * 2.f * PI * 7.f;
			Glow.Box(*FString::Printf(TEXT("glow_c%d"), I % 5), FTransform(T + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, Z)), FVector(3.5f), false);
		}
		// a five-pointed star facing the middle
		const FVector Top = T + FVector(0.f, 0.f, H + 30.f);
		const FVector Out = (C - T).GetSafeNormal2D(), Side = FVector::CrossProduct(FVector::UpVector, Out);
		for (int32 I = 0; I < 5; I++)
		{
			const float A0 = 2.f * PI * I / 5, A1 = A0 + PI / 5, A2 = A0 - PI / 5;
			auto P = [&](float A, float Rr) { return Top + (Side * FMath::Sin(A) + FVector::UpVector * FMath::Cos(A)) * Rr; };
			Glow.Tri(TEXT("glow_star"), Top, P(A1, 22.f), P(A0, 55.f));
			Glow.Tri(TEXT("glow_star"), Top, P(A0, 55.f), P(A2, 22.f));
			Glow.Tri(TEXT("glow_star"), Top, P(A0, 55.f), P(A1, 22.f));
			Glow.Tri(TEXT("glow_star"), Top, P(A2, 22.f), P(A0, 55.f));
		}
		UPointLightComponent* L = MakeLight(this, RootComponent, FLinearColor(1.f, 0.75f, 0.45f), 1800.f);
		L->SetWorldLocation(T + FVector(0.f, 0.f, 400.f) + Out * 300.f);
		Lights.Add(L);
	}
	if (Kinds.Contains(TEXT("candles")))
	{
		// paper-bag candles in two rows across the venue (along the promenade, the beach's top)
		const FVector Along = FVector::CrossProduct(FVector::UpVector, V.SeaDir).GetSafeNormal();
		const float Len = R * 2.4f;
		for (const float Off : { -260.f, 260.f })
		{
			for (float S = -Len * 0.5f; S <= Len * 0.5f; S += 130.f)
			{
				const FVector P = GroundAt(C + Along * S + V.SeaDir * Off);
				K.Box(TEXT("canvas_b"), FTransform(FRotator(0.f, Rand01(Rng) * 90.f, 0.f), P + FVector(0.f, 0.f, 13.f)), FVector(9.f, 7.f, 13.f), false);
				Glow.Box(TEXT("glow_candle"), FTransform(P + FVector(0.f, 0.f, 8.f)), FVector(2.5f, 2.5f, 6.f), false);
			}
		}
	}
	if (Kinds.Contains(TEXT("birdhouses")))
	{
		// birdhouses on posts for the birds' day: a box, a pitched roof, a round hole
		for (int32 I = 0; I < 6; I++)
		{
			const FVector Dir = FRotator(0.f, V.Yaw + 15.f + I * 60.f, 0.f).Vector();
			const FVector Base = GroundAt(C + Dir * R * 0.6f);
			const FRotator Face = (-Dir).Rotation();
			const FTransform F(FRotator(0.f, Face.Yaw, 0.f), Base);
			K.Rod(TEXT("wood"), Base - FVector(0.f, 0.f, 20.f), Base + FVector(0.f, 0.f, 240.f), 4.f, 6, true, false);
			K.BoxIn(TEXT("planks"), F, FVector(0.f, 0.f, 260.f), FVector(22.f, 22.f, 32.f), false);
			const TArray<FVector2D> Roof = { { -16.f, -15.f }, { 16.f, -15.f }, { 0.f, 4.f } };
			K.Prism(TEXT("wood"), FTransform(FRotator(0.f, Face.Yaw, 90.f), Base + FVector(0.f, 0.f, 291.f)), Roof, -17.f, 17.f, false);
			K.BoxIn(TEXT("backdrop"), F, FVector(11.5f, 0.f, 266.f), FVector(1.f, 6.f, 6.f), false);
		}
	}

	auto Make = [this](FLyudMeshKit& Kit, const TCHAR* Name, bool bSolid)
	{
		if (Kit.IsEmpty())
		{
			return;
		}
		TArray<FName> Slots;
		UStaticMesh* Mesh = Kit.Build(this, Name, Slots);
		UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, Name);
		Comp->SetStaticMesh(Mesh);
		for (int32 I = 0; I < Slots.Num(); I++)
		{
			Comp->SetMaterial(I, Mat(Slots[I]));
		}
		Comp->SetMobility(EComponentMobility::Static);
		Comp->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Comp->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		Comp->SetCastShadow(bSolid);
		Comp->SetCachedMaxDrawDistance(bSolid ? 60000.f : 40000.f);
		Comp->LightingChannels.bChannel1 = true;
		if (!bSolid)
		{
			Comp->bAffectDistanceFieldLighting = false;
		}
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
	};
	// the mesh kit works in world space: the actor stands at the origin
	Make(K, TEXT("FestivalDecor"), true);
	Make(Glow, TEXT("FestivalLights"), false);
	UE_LOG(LogTemp, Display, TEXT("Festivals: decor %s at %s (%d + %d triangles)"), *FString::Join(Kinds, TEXT(",")), *V.Center.ToString(),
		K.NumTriangles(), Glow.NumTriangles());
}

void ALyudFestivalDecor::SetLights(float Night, bool bInParty)
{
	if (FMath::Abs(Night - LightLevel) > 0.02f)
	{
		LightLevel = Night;
		for (int32 I = 0; I < Glows.Num(); I++)
		{
			if (!StageGlows.Contains(Glows[I]))
			{
				const float Base = GlowBase[I * 2], Min = GlowBase[I * 2 + 1];
				Glows[I]->SetScalarParameterValue(TEXT("Intensity"), Base * FMath::Lerp(Min, 1.f, Night));
			}
		}
		for (int32 I = 0; I < Lights.Num(); I++)
		{
			if (!bParty || I >= NumStageLights)
			{
				Lights[I]->SetIntensity(Night * 900.f);
			}
		}
	}
	if (bInParty != bParty)
	{
		bParty = bInParty;
		SetActorTickEnabled(bParty);
		if (!bParty)
		{
			for (UMaterialInstanceDynamic* G : StageGlows) G->SetScalarParameterValue(TEXT("Intensity"), 0.f);
			LightLevel = -1.f;
		}
	}
}

void ALyudFestivalDecor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// the stage's lights run through their colours, chasing along the truss (4 updates a second)
	Clock += DeltaTime;
	for (int32 I = 0; I < StageGlows.Num(); I++)
	{
		const float W = 0.5f + 0.5f * FMath::Sin(Clock * 2.6f + I * 0.9f);
		StageGlows[I]->SetScalarParameterValue(TEXT("Intensity"), 4.f + 40.f * W * W);
		StageGlows[I]->SetVectorParameterValue(TEXT("Color"), FireColors[(I + (int32)(Clock * 0.5f)) % UE_ARRAY_COUNT(FireColors)]);
	}
	for (int32 I = 0; I < NumStageLights; I++)
	{
		const float W = 0.5f + 0.5f * FMath::Sin(Clock * 1.7f + I * 2.1f);
		Lights[I]->SetIntensity(400.f + 2400.f * W * FMath::Max(LightLevel, 0.3f));
		Lights[I]->SetLightColor(FireColors[(I * 3 + (int32)(Clock * 0.5f)) % UE_ARRAY_COUNT(FireColors)]);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// fireworks

ALyudFireworks::ALyudFireworks()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float ALyudFireworks::Rand()
{
	return Rand01(Rng);
}

void ALyudFireworks::Setup(const FVector& InLaunch, const FVector& SeaDir, double InEndsAt)
{
	Launch = InLaunch;
	Sea = SeaDir;
	EndsAt = InEndsAt;
	Rng = Hash(InLaunch.ToString()) ^ (uint32)InEndsAt;
	// one burst: 110 streaks out from the middle on a unit sphere (r 0.62..1 of 100 cm), scaled up as it opens
	FLyudMeshKit B, T;
	const int32 N = 110;
	for (int32 I = 0; I < N; I++)
	{
		const float Y = 1.f - 2.f * (I + 0.5f) / N, Rr = FMath::Sqrt(1.f - Y * Y), Phi = I * 2.39996f;
		const FVector D(FMath::Cos(Phi) * Rr, FMath::Sin(Phi) * Rr, Y);
		B.Rod(TEXT("glow"), D * 62.f, D * 100.f, 0.55f, 3, false, false);
	}
	T.Rod(TEXT("glow"), FVector(0.f, 0.f, -260.f), FVector::ZeroVector, 6.f, 4, false, false);
	TArray<FName> Slots;
	BurstMesh = B.Build(this, TEXT("FireworkBurst"), Slots);
	TrailMesh = T.Build(this, TEXT("FireworkTrail"), Slots);
	UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInstanceDynamic* TrailMat = UMaterialInstanceDynamic::Create(G, this);
	TrailMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.7f, 0.35f));
	TrailMat->SetScalarParameterValue(TEXT("Intensity"), 25.f);
	for (int32 I = 0; I < 14; I++)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(BurstMesh);
		Lightweight(C);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(G, this);
		C->SetMaterial(0, M);
		C->SetupAttachment(RootComponent);
		C->SetVisibility(false);
		C->RegisterComponent();
		Bursts.Add(C);
		BurstMats.Add(M);
		UStaticMeshComponent* Tr = NewObject<UStaticMeshComponent>(this);
		Tr->SetStaticMesh(TrailMesh);
		Lightweight(Tr);
		Tr->SetMaterial(0, TrailMat);
		Tr->SetupAttachment(RootComponent);
		Tr->SetVisibility(false);
		Tr->RegisterComponent();
		Trails.Add(Tr);
	}
	for (int32 I = 0; I < 3; I++)
	{
		Flashes.Add(MakeLight(this, RootComponent, FLinearColor::White, 60000.f));
	}
	NextShell = 0.5f;
}

void ALyudFireworks::Fire(bool bFinale)
{
	int32 Free = -1;
	for (int32 I = 0; I < Bursts.Num(); I++)
	{
		bool bUsed = false;
		for (const FShell& S : Shells) bUsed |= S.Comp == I;
		if (!bUsed)
		{
			Free = I;
			break;
		}
	}
	if (Free < 0)
	{
		return;
	}
	FShell S;
	S.Comp = Free;
	S.T = 0.f;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Sea);
	S.From = Launch + Side * (Rand() - 0.5f) * 6000.f;
	S.Burst = S.From + FVector(0.f, 0.f, 9000.f + Rand() * 7000.f) + Side * (Rand() - 0.5f) * 3000.f + Sea * Rand() * 2500.f;
	S.Rise = 1.4f + Rand() * 0.8f;
	S.Life = 2.4f + Rand() * 1.0f;
	S.Radius = (bFinale ? 3200.f : 2600.f) + Rand() * 2200.f;
	Shells.Add(S);
	const int32 NC = UE_ARRAY_COUNT(FireColors);
	Shells.Last().Color = FireColors[FMath::Min((int32)(Rand() * NC), NC - 1)];
	BurstMats[Free]->SetVectorParameterValue(TEXT("Color"), Shells.Last().Color);
	if (USoundBase* Launch1 = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Festival/S_firework_launch_01.S_firework_launch_01"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Launch1, S.From, 0.6f, 0.9f + Rand() * 0.2f);
	}
}

void ALyudFireworks::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const double Now = CalSeconds(GetWorld());
	const bool bOn = Now < EndsAt;
	const bool bFinale = bOn && EndsAt - Now < 45.0;
	NextShell -= DeltaTime;
	if (bOn && NextShell <= 0.f)
	{
		Fire(bFinale);
		NextShell = bFinale ? 0.18f + Rand() * 0.3f : 1.1f + Rand() * 2.6f;
	}
	FVector Cam = Launch;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager) Cam = PC->PlayerCameraManager->GetCameraLocation();
	}
	float Flash[3] = { 0.f, 0.f, 0.f };
	FVector FlashAt[3];
	FLinearColor FlashCol[3];
	for (int32 Si = Shells.Num() - 1; Si >= 0; Si--)
	{
		FShell& S = Shells[Si];
		S.T += DeltaTime;
		UStaticMeshComponent* Burst = Bursts[S.Comp];
		UStaticMeshComponent* Trail = Trails[S.Comp];
		if (S.T < S.Rise)
		{
			// the shell climbs, slowing near the top
			const float U = S.T / S.Rise;
			const FVector P = FMath::Lerp(S.From, S.Burst, 1.f - (1.f - U) * (1.f - U));
			Trail->SetWorldLocation(P);
			Trail->SetWorldRotation(FRotationMatrix::MakeFromZ(S.Burst - S.From).Rotator());
			Trail->SetVisibility(true);
			continue;
		}
		Trail->SetVisibility(false);
		const float T = S.T - S.Rise;
		if (T > S.Life)
		{
			Burst->SetVisibility(false);
			Shells.RemoveAt(Si);
			continue;
		}
		// opens fast, slows in the air, droops
		const float Open = 1.f - FMath::Exp(-T / 0.32f);
		const float Scale = FMath::Max(0.02f, S.Radius * Open / 100.f);
		Burst->SetWorldLocation(S.Burst - FVector(0.f, 0.f, 120.f * T * T));
		Burst->SetWorldScale3D(FVector(Scale));
		Burst->SetVisibility(true);
		const float Fade = FMath::Square(1.f - T / S.Life);
		BurstMats[S.Comp]->SetScalarParameterValue(TEXT("Intensity"), 60.f * Fade * (T < 0.08f ? 3.f : 1.f));
		// the flash lights the water and the crowd for a moment
		const float F = FMath::Max(0.f, 1.f - T / 0.45f);
		for (int32 L = 0; L < 3; L++)
		{
			if (F > Flash[L])
			{
				Flash[L] = F;
				FlashAt[L] = S.Burst;
				FlashCol[L] = S.Color;
				break;
			}
		}
		// the boom arrives by the distance (343 m/s)
		if (!S.bBoom && T >= FVector::Dist(S.Burst, Cam) / 34300.f)
		{
			S.bBoom = true;
			const int32 Var = 1 + (int32)(Rand() * 3.f) % 3;
			const FString Path = FString::Printf(TEXT("/Game/Audio/Festival/S_firework_boom_%02d.S_firework_boom_%02d"), Var, Var);
			if (USoundBase* Boom = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				const float Vol = FMath::Clamp(1.2f - FVector::Dist(S.Burst, Cam) / 120000.f, 0.25f, 1.f);
				UGameplayStatics::PlaySound2D(this, Boom, Vol, 0.85f + Rand() * 0.3f);
			}
		}
	}
	for (int32 L = 0; L < 3; L++)
	{
		Flashes[L]->SetIntensity(Flash[L] * 120000.f);
		if (Flash[L] > 0.f)
		{
			Flashes[L]->SetWorldLocation(FlashAt[L]);
			Flashes[L]->SetLightColor(FlashCol[L]);
		}
	}
	if (!bOn && Shells.Num() == 0)
	{
		Destroy();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// sky lanterns

ALyudSkyLanterns::ALyudSkyLanterns()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float ALyudSkyLanterns::Rand()
{
	return Rand01(Rng);
}

void ALyudSkyLanterns::Setup(const FVector& InBeach, const FVector& SeaDir, double InEndsAt)
{
	Beach = InBeach;
	Sea = SeaDir;
	EndsAt = InEndsAt;
	Rng = Hash(InBeach.ToString()) ^ 0x5bd1e995u;
	// a paper lantern: a tall tapering shade, open below
	FLyudMeshKit K;
	const TArray<FVector2D> P = { { 20.f, 0.f }, { 26.f, 30.f }, { 28.f, 58.f }, { 0.f, 62.f } };
	const TArray<FName> M = { TEXT("glow"), TEXT("glow"), TEXT("glow") };
	K.Lathe(FTransform::Identity, P, M, 8, false, false);
	TArray<FName> Slots;
	Mesh = K.Build(this, TEXT("SkyLantern"), Slots);
	UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Glow = UMaterialInstanceDynamic::Create(G, this);
	Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.48f, 0.14f));
	Glow->SetScalarParameterValue(TEXT("Intensity"), 9.f);
	for (int32 I = 0; I < 70; I++)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		Lightweight(C);
		C->SetMaterial(0, Glow);
		C->SetupAttachment(RootComponent);
		C->SetVisibility(false);
		C->RegisterComponent();
		Comps.Add(C);
		Lanterns.AddDefaulted();
	}
}

void ALyudSkyLanterns::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const bool bOn = CalSeconds(GetWorld()) < EndsAt;
	NextRelease -= DeltaTime;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Sea);
	if (bOn && NextRelease <= 0.f)
	{
		NextRelease = 2.5f + Rand() * 6.f;
		for (int32 I = 0; I < Lanterns.Num(); I++)
		{
			if (Lanterns[I].T < 0.f)
			{
				FLantern& L = Lanterns[I];
				L.T = 0.f;
				L.P = Beach + Side * (Rand() - 0.5f) * 3000.f + Sea * (Rand() - 0.5f) * 500.f + FVector(0.f, 0.f, 120.f);
				L.V = FVector(0.f, 0.f, 55.f + Rand() * 45.f) + Sea * (40.f + Rand() * 60.f) + Side * (Rand() - 0.5f) * 30.f;
				L.Phase = Rand() * 10.f;
				break;
			}
		}
	}
	int32 Alive = 0;
	for (int32 I = 0; I < Lanterns.Num(); I++)
	{
		FLantern& L = Lanterns[I];
		if (L.T < 0.f)
		{
			continue;
		}
		L.T += DeltaTime;
		// a slow climb that eases off as the air in them cools, a little sway
		const float Lift = FMath::Max(0.25f, 1.f - L.T / 900.f);
		L.P += (L.V * FVector(1.f, 1.f, Lift)) * DeltaTime + FVector(FMath::Sin(L.T * 0.4f + L.Phase), FMath::Cos(L.T * 0.33f + L.Phase), 0.f) * 8.f * DeltaTime;
		if (L.T > 1100.f || L.P.Z - Beach.Z > 60000.f)
		{
			L.T = -1.f;
			Comps[I]->SetVisibility(false);
			continue;
		}
		Alive++;
		Comps[I]->SetWorldLocation(L.P);
		Comps[I]->SetWorldRotation(FRotator(FMath::Sin(L.T * 0.7f + L.Phase) * 4.f, L.Phase * 30.f, FMath::Cos(L.T * 0.6f + L.Phase) * 4.f));
		Comps[I]->SetVisibility(true);
	}
	if (!bOn && Alive == 0)
	{
		Destroy();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the bonfire

ALyudBonfire::ALyudBonfire()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALyudBonfire::Setup(const FVector& At, bool bEffigy, double InEndsAt)
{
	StartsAt = CalSeconds(GetWorld());
	EndsAt = InEndsAt;
	SetActorLocation(At);
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, PaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto PaintMat = [&](const FLinearColor& C)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Paint, this);
		M->SetVectorParameterValue(TEXT("Color"), C);
		M->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
		return M;
	};
	auto Add = [&](UStaticMesh* Mesh, UMaterialInterface* M, bool bLight)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		if (bLight)
		{
			Lightweight(C);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		for (int32 I = 0; I < C->GetNumMaterials(); I++) C->SetMaterial(I, M);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		return C;
	};
	TArray<FName> Slots;
	// the logs: a tipi of ten
	FLyudMeshKit Logs;
	for (int32 I = 0; I < 10; I++)
	{
		const FVector D = FRotator(0.f, I * 36.f, 0.f).Vector();
		Logs.Rod(TEXT("wood"), D * 110.f, D * 12.f + FVector(0.f, 0.f, 170.f), 7.f, 6, true, false);
	}
	Logs.CollideBox(FTransform(FVector(0.f, 0.f, 60.f)), FVector(90.f, 90.f, 60.f));
	Add(Logs.Build(this, TEXT("BonfireLogs"), Slots), PaintMat(FLinearColor(0.12f, 0.07f, 0.04f)), false);
	// the flames: three tongues of different heights, each flickering by its own clock
	FlameMat = UMaterialInstanceDynamic::Create(G, this);
	FlameMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.42f, 0.08f));
	FlameMat->SetScalarParameterValue(TEXT("Intensity"), 22.f);
	for (int32 I = 0; I < 3; I++)
	{
		FLyudMeshKit F;
		const float H = 180.f + I * 70.f, R = 70.f - I * 14.f;
		const TArray<FVector2D> P = { { 0.f, 0.f }, { R, 25.f }, { R * 0.6f, H * 0.55f }, { 0.f, H } };
		const TArray<FName> M = { TEXT("glow"), TEXT("glow"), TEXT("glow") };
		F.Lathe(FTransform(FRotator(0.f, I * 40.f, 0.f), FVector(FMath::Cos(I * 2.1f) * 18.f, FMath::Sin(I * 2.1f) * 18.f, 10.f)), P, M, 7, false, false);
		UStaticMeshComponent* C = Add(F.Build(this, *FString::Printf(TEXT("BonfireFlame%d"), I), Slots), FlameMat, true);
		Flames.Add(C);
	}
	if (bEffigy)
	{
		// the straw doll of Maslenitsa on a pole in the middle of the fire
		FLyudMeshKit E;
		E.Rod(TEXT("wood"), FVector(0.f, 0.f, 0.f), FVector(0.f, 0.f, 420.f), 5.f, 6, true, false);
		E.Rod(TEXT("wood"), FVector(0.f, -110.f, 330.f), FVector(0.f, 110.f, 330.f), 4.f, 6, true, false);
		const TArray<FVector2D> Dress = { { 0.f, 170.f }, { 75.f, 175.f }, { 22.f, 340.f }, { 0.f, 345.f } };
		const TArray<FName> M = { TEXT("straw"), TEXT("straw"), TEXT("straw") };
		E.Lathe(FTransform::Identity, Dress, M, 10, false, false);
		Ball(E, TEXT("straw"), FVector(0.f, 0.f, 375.f), 22.f, 46.f);
		Effigy = Add(E.Build(this, TEXT("Effigy"), Slots), PaintMat(FLinearColor(0.62f, 0.5f, 0.24f)), false);
	}
	Light = MakeLight(this, RootComponent, FLinearColor(1.f, 0.55f, 0.25f), 2500.f);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	if (USoundBase* Crackle = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Festival/S_bonfire_loop_01.S_bonfire_loop_01"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::SpawnSoundAttached(Crackle, RootComponent);
	}
}

void ALyudBonfire::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;
	const double Now = CalSeconds(GetWorld());
	// it dies down over the last ten minutes of its phase
	const float Life = (float)FMath::Clamp((EndsAt - Now) / 600.0, 0.0, 1.0);
	for (int32 I = 0; I < Flames.Num(); I++)
	{
		const float F = 0.82f + 0.18f * FMath::Sin(Clock * (7.f + I * 2.3f) + I) * FMath::Sin(Clock * (3.1f + I) + I * 2.f);
		Flames[I]->SetRelativeScale3D(FVector(0.9f + 0.1f * F, 0.9f + 0.1f * F, F * (0.25f + 0.75f * Life)));
	}
	const float Flick = 0.75f + 0.25f * FMath::Sin(Clock * 11.f) * FMath::Sin(Clock * 4.3f);
	Light->SetIntensity(5000.f * Flick * Life);
	FlameMat->SetScalarParameterValue(TEXT("Intensity"), 22.f * (0.8f + 0.2f * Flick));
	// the doll goes up in a third of the burn
	if (Effigy && Now - StartsAt > (EndsAt - StartsAt) * 0.33)
	{
		Effigy->SetVisibility(false);
		Effigy = nullptr;
	}
	if (Now >= EndsAt)
	{
		Destroy();
	}
}
