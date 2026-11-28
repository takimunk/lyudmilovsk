// The yacht school's fleet in the world: the pooled dinghies, the coach's launch and the marks; who goes afloat when
// (the people's life says who has come to the slip), the helmsmen of the fleet's brain, the three levels of detail, the
// boats' places ashore by the season and the hour. docs/yacht_school.md "Engine: the school".
#include "Sailing/LyudSailing.h"

#include "LyudSailingState.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "People/PeopleLife.h"
#include "Sailing/LyudDinghy.h"
#include "Sea/LyudSea.h"

#include <cmath>
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailFleet, Log, All);

using namespace LyudSail;

namespace LyudSailingFleetImpl
{
	TAutoConsoleVariable<int32> CVarNearBoats(TEXT("lyud.Sail.NearBoats"), 6, TEXT("How many dinghies at most are simulated and drawn in full at once (the hero's own counts)."));

	constexpr int32 MaxUnits = 32;
	constexpr int32 MarkCount = 24;
	/** FLyudSailHull::Shown of a boat lying on the beach where she landed. */
	constexpr int32 OnBeach = 100;

	ELyudDinghyLod LodOf(int Lod)
	{
		return Lod == FleetLodNear ? ELyudDinghyLod::Near : (Lod == FleetLodMid ? ELyudDinghyLod::Mid : ELyudDinghyLod::Far);
	}

	double AgeOf(const FLyudSailingState& S, const std::string& Slot, const std::string& Group)
	{
		const FName Wanted = LyudSailingImpl::Name(Slot);
		for (const FLyudSailorNow& N : S.Sailors)
		{
			if (N.Slot == Wanted && N.AgeYears > 0.f)
			{
				return N.AgeYears;
			}
		}
		return Group == "kids" ? 9.0 : (Group == "juniors" ? 14.0 : 30.0);
	}

	FName PersonOf(const FLyudSailingState& S, const std::string& Slot)
	{
		if (Slot.empty())
		{
			return NAME_None;
		}
		const FName Wanted = LyudSailingImpl::Name(Slot);
		for (const FLyudSailorNow& N : S.Sailors)
		{
			if (N.Slot == Wanted)
			{
				return N.Person;
			}
		}
		for (const FSchoolSailor& Sailor : S.P.Sailors)
		{
			if (Sailor.Slot == Slot && !Sailor.Person.empty())
			{
				return LyudSailingImpl::Name(Sailor.Person);
			}
		}
		return NAME_None;
	}

	const FSchoolSessionState* StateOf(const FLyudSailingState& S, long long Key)
	{
		for (const FSchoolSessionState& St : S.States)
		{
			if (St.Key == Key)
			{
				return &St;
			}
		}
		return nullptr;
	}

	/** How far the season has gone, 0 the opening .. 1 the closing. */
	double SeasonShare(const FLyudSailingState& S)
	{
		const long long A = SchoolDayNumber(SeasonOpening(S.P, S.Date.Y)), B = SchoolDayNumber(SeasonClosing(S.P, S.Date.Y));
		return B > A ? SchoolClamp(double(S.Day - A) / double(B - A), 0.0, 1.0) : 0.0;
	}
}

// ------------------------------------------------------------------ the pool

bool ULyudSailing::MakeFleet()
{
	if (bFleetMade)
	{
		return true;
	}
	UWorld* World = GetWorld();
	FLyudSailingState& S = *M;
	if (!World || !S.bFacts)
	{
		return false;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;
	S.Hulls.clear();
	Dinghies.Reset();
	for (size_t I = 0; I < S.P.BoatSlots.size(); I++)
	{
		const FSchoolBoatSlot& Slot = S.P.BoatSlots[I];
		if (Slot.Class < 0 || Slot.Class >= SchoolBoats)
		{
			continue;
		}
		ALyudDinghy* Boat = World->SpawnActor<ALyudDinghy>(ALyudDinghy::StaticClass(), FTransform::Identity, SP);
		if (!Boat)
		{
			continue;
		}
		// the sail number is the slot's own ("luch_9" -> 9)
		int32 Number = (int32)S.Hulls.size() + 1;
		const size_t Dash = Slot.Id.rfind('_');
		if (Dash != std::string::npos)
		{
			Number = FMath::Max(1, FCString::Atoi(*LyudSailingImpl::Str(Slot.Id.substr(Dash + 1))));
		}
		Boat->Setup(Slot.Class, Number, (int32)S.Hulls.size(), SchoolHashStr(Slot.Id));
		FLyudSailHull Hull;
		Hull.Slot = (int32)I;
		Hull.BoatClass = Slot.Class;
		if (Slot.Id == S.P.HeroBoat)
		{
			S.HeroHull = (int32)S.Hulls.size();
		}
		S.Hulls.push_back(Hull);
		Dinghies.Add(Boat);
	}
	if (S.HeroHull < 0)
	{
		// the data names no boat of the fleet for him: the last one of his class
		for (size_t I = 0; I < S.Hulls.size(); I++)
		{
			if (S.Hulls[I].BoatClass == S.P.HeroClass)
			{
				S.HeroHull = (int32)I;
			}
		}
	}
	LaunchBoat = World->SpawnActor<ALyudSailLaunch>(ALyudSailLaunch::StaticClass(), FTransform::Identity, SP);
	MarkPool.Reset();
	for (int32 I = 0; I < LyudSailingFleetImpl::MarkCount; I++)
	{
		MarkPool.Add(World->SpawnActor<ALyudSailMark>(ALyudSailMark::StaticClass(), FTransform::Identity, SP));
	}
	// unit 0 is the hero's for good; the sailors of a session follow it while it is on the water
	S.Brain.Units.clear();
	S.Brain.Units.reserve(LyudSailingFleetImpl::MaxUnits);
	S.UnitHull.clear();
	S.UnitCrew.clear();
	FFleetUnit HeroUnit;
	HeroUnit.bHero = true;
	HeroUnit.Sailor = -1;
	HeroUnit.Boat = S.HeroHull >= 0 ? &Dinghies[S.HeroHull]->Boat() : nullptr;
	S.Brain.Units.push_back(HeroUnit);
	S.UnitHull.push_back(S.HeroHull);
	S.UnitCrew.push_back(std::string());
	S.Brain.Launch = LaunchBoat ? &LaunchBoat->Launch() : nullptr;
	if (S.Lp.bOk)
	{
		S.Brain.SetLaunchPoint(S.Lp.At);
	}
	bFleetMade = true;
	S.bLaunchOut = true;
	LaunchOut(false);
	ParkHulls(true);
	UE_LOG(LogLyudSailFleet, Display, TEXT("SAIL fleet made: %d dinghies, the launch, %d marks"), Dinghies.Num(), MarkPool.Num());
	return true;
}

void ULyudSailing::StowFleet()
{
	if (!bFleetMade || !M)
	{
		return;
	}
	FLyudSailingState& S = *M;
	for (size_t I = 0; I < S.Hulls.size(); I++)
	{
		FLyudSailHull& Hull = S.Hulls[I];
		Hull.bAfloat = false;
		Hull.Unit = -1;
		Hull.Shown = -1;
		if (ALyudDinghy* Boat = Dinghies[(int32)I])
		{
			Boat->SetCrew(0, NAME_None, 0.f);
			Boat->SetCrew(1, NAME_None, 0.f);
			Boat->SetActorHiddenInGame(true);
		}
	}
	if (S.Brain.Units.size() > 1)
	{
		S.Brain.Units.resize(1);
		S.UnitHull.resize(1);
		S.UnitCrew.resize(1);
	}
	if (!S.Brain.Units.empty())
	{
		S.Brain.Units[0].Stage = EFleetStage::Ashore;
	}
	S.Brain.Mode = EFleetMode::Idle;
	S.Brain.bRecall = false;
	S.Brain.bRaceRunning = false;
	S.Brain.bHeld = false;
	S.Brain.Towing = -1;
	S.Brain.ForceTow = -1;
	S.Brain.Task = FSchoolLaunchTask();
	S.WaterKey = 0;
	S.bWaterWatched = false;
	S.bWaterRace = false;
	S.bDevWater = false;
	StowMarks(0, LyudSailingFleetImpl::MarkCount);
	LaunchOut(false);
	if (LaunchBoat)
	{
		LaunchBoat->SetActorHiddenInGame(true);
	}
}

float ULyudSailing::GroundZ(const FSchoolV2& At) const
{
	if (const ALyudSea* Sea = M->Sea.Get())
	{
		return Sea->SeaLevel() - Sea->DepthAt(FVector(At.X * 100.0, At.Y * 100.0, 0.0));
	}
	return float((M->P.SeaZ + M->W.TideM - SchoolDepthAt(M->P, At, M->W.TideM)) * 100.0);
}

FTransform ULyudSailing::BeachTransform(const FSchoolV2& At) const
{
	// bow up the beach
	const float YawDeg = FMath::RadiansToDegrees(float(std::atan2(-M->P.LaunchDir.Y, -M->P.LaunchDir.X)));
	return FTransform(FRotator(0.f, YawDeg, 0.f), LyudSailingImpl::CmOf(At, GroundZ(At)));
}

void ULyudSailing::LaunchOut(bool bOut)
{
	FLyudSailingState& S = *M;
	if (!LaunchBoat || S.bLaunchOut == bOut)
	{
		return;
	}
	S.bLaunchOut = bOut;
	if (bOut)
	{
		const float Yaw = float(std::atan2(S.P.LaunchDir.Y, S.P.LaunchDir.X));
		LaunchBoat->SetActorHiddenInGame(false);
		LaunchBoat->PlaceAt(LyudSailingImpl::V2(S.Brain.Home), Yaw);
		S.Brain.LaunchTarget = S.Brain.Home;
		S.Brain.LaunchCruise = 0.0;
		const std::string Coach = S.Now.bOn && !S.Now.Coach.empty() ? S.Now.Coach : std::string("coach_head");
		LaunchBoat->SetDriver(LyudSailingFleetImpl::PersonOf(S, Coach));
		return;
	}
	// back on its mooring by the slip's foot (the data's launch slot), engine off, the coach given back to the town
	LaunchBoat->SetDriver(NAME_None);
	LaunchBoat->SetFlag(NAME_None);
	S.LaunchFlags[0] = S.LaunchFlags[1] = NAME_None;
	FVector At = LyudSailingImpl::CmOf(S.Brain.Home, GroundZ(S.Brain.Home));
	float YawDeg = FMath::RadiansToDegrees(float(std::atan2(S.P.LaunchDir.Y, S.P.LaunchDir.X)));
	for (const FSchoolBoatSlot& Slot : S.P.BoatSlots)
	{
		if (Slot.Class < 0)
		{
			const bool bLaidUp = S.Season == ESeasonState::LaidUp && (Slot.Hall.X != 0.0 || Slot.Hall.Y != 0.0);
			const FSchoolPoint& Pt = bLaidUp ? Slot.Hall : Slot.At;
			At = LyudSailingImpl::CmOf(Pt);
			YawDeg = float(Pt.Yaw);
			break;
		}
	}
	LaunchBoat->ParkAt(FTransform(FRotator(0.f, YawDeg, 0.f), At));
	LaunchBoat->SetActorHiddenInGame(!bSiteShown);
}

// ------------------------------------------------------------------ ashore

void ULyudSailing::ParkHulls(bool bForce)
{
	if (!bFleetMade)
	{
		return;
	}
	FLyudSailingState& S = *M;
	const FSchoolSessionState* St = S.Now.bOn ? LyudSailingFleetImpl::StateOf(S, S.Now.Key) : nullptr;
	const bool bGoing = S.Now.bOn && S.Now.Session.bAfloat && St && St->Go == ESchoolGo::Sail && St->RecallHour < 0.0
		&& (S.Now.Phase == "rig" || S.Now.Phase == "launch" || S.Now.Phase == "water" || S.Now.Phase == "return");
	const bool bBeachHeld = S.WaterKey != 0 || (S.Now.bOn && S.Now.Phase == "return");
	const bool bOpen = S.Season == ESeasonState::Open;
	const FSchoolDaylight Light = SchoolDaylight(S.P, S.Date);
	const bool bHireHours = bOpen && S.Hour >= S.P.HireFrom - 0.5 && S.Hour <= Light.Sunset - S.P.HireBeforeSunset && !S.W.bThunder;
	for (size_t I = 0; I < S.Hulls.size(); I++)
	{
		FLyudSailHull& Hull = S.Hulls[I];
		ALyudDinghy* Boat = Dinghies[(int32)I];
		if (!Boat || Hull.bAfloat)
		{
			continue;
		}
		if (Hull.Shown == LyudSailingFleetImpl::OnBeach && bBeachHeld && !bForce)
		{
			Boat->SetActorHiddenInGame(false);
			continue;
		}
		const FSchoolBoatSlot& Slot = S.P.BoatSlots[(size_t)Hull.Slot];
		const bool bHall = S.Season == ESeasonState::LaidUp && (Slot.Hall.X != 0.0 || Slot.Hall.Y != 0.0);
		// rigged when her class is about to go afloat (the hire boat through the hire's hours); under her cover in the hall,
		// while the school lays up, and at night
		const bool bHers = (int32)I == S.HeroHull;
		const bool bRigged = !bHall && bOpen && ((bGoing && (S.Now.Session.Classes >> Hull.BoatClass & 1)) || (bHers && bHireHours));
		const bool bCovered = bHall || S.Season == ESeasonState::LayUp || (bOpen && !bRigged && !S.W.bDaylight);
		const int32 Want = (bHall ? 1 : 2) + (bRigged ? 4 : 0) + (bCovered ? 8 : 0);
		if (Hull.Shown != Want || bForce)
		{
			Hull.Shown = Want;
			const FSchoolPoint& Pt = bHall ? Slot.Hall : Slot.At;
			Boat->ParkAt(FTransform(FRotator(0.f, float(Pt.Yaw), 0.f), LyudSailingImpl::CmOf(Pt)), bRigged, bCovered);
		}
		Boat->SetActorHiddenInGame(!bSiteShown);
	}
	if (LaunchBoat && !S.bLaunchOut)
	{
		LaunchBoat->SetActorHiddenInGame(!bSiteShown);
	}
}

// ------------------------------------------------------------------ marks

void ULyudSailing::PlaceMark(int32 Index, int32 MarkKind, const FSchoolV2& At)
{
	if (!MarkPool.IsValidIndex(Index) || !MarkPool[Index])
	{
		return;
	}
	MarkPool[Index]->Setup(MarkKind);
	MarkPool[Index]->PlaceAt(LyudSailingImpl::V2(At));
	M->MarksOut |= 1u << Index;
}

void ULyudSailing::StowMarks(int32 First, int32 Last)
{
	for (int32 I = FMath::Max(0, First); I < Last && I < MarkPool.Num(); I++)
	{
		if ((M->MarksOut >> I & 1u) && MarkPool[I])
		{
			MarkPool[I]->Stow();
		}
		M->MarksOut &= ~(1u << I);
	}
}

int32 ULyudSailing::LayCourse(int32 First, const FRaceCourse& Course, bool bBoatEnd)
{
	if (!Course.bOk)
	{
		return 0;
	}
	// a regatta's marks are the big orange ones; a drill's and the children's the small buoys
	const bool bSmall = Course.Kind == ERaceCourse::KidsTriangle || Course.Kind == ERaceCourse::Figure8 || Course.Kind == ERaceCourse::StartLine;
	int32 N = 0;
	for (const FRaceMark& Mark : Course.Marks)
	{
		PlaceMark(First + N++, bSmall ? 2 : 0, Mark.At);
	}
	if (Course.bLine)
	{
		PlaceMark(First + N++, 1, Course.LinePin);
		if (bBoatEnd)
		{
			PlaceMark(First + N++, 2, Course.LineBoat);
		}
	}
	return N;
}

// ------------------------------------------------------------------ a session on the water

void ULyudSailing::BeginWater(bool bRace)
{
	FLyudSailingState& S = *M;
	const auto Decided = S.Decisions.find(S.Now.Key);
	const FSchoolGo Go = Decided != S.Decisions.end() ? Decided->second : SchoolDecide(S.P, S.Date, S.W, S.Now.Session.Classes);
	FFleetBrain& Brain = S.Brain;
	Brain.Units.resize(1);
	S.UnitHull.resize(1);
	S.UnitCrew.resize(1);
	for (FLyudSailHull& Hull : S.Hulls)
	{
		if (!Hull.bAfloat)
		{
			Hull.Unit = -1;
		}
	}
	if (S.HeroHull >= 0)
	{
		S.Hulls[(size_t)S.HeroHull].Unit = 0;
	}
	const double Share = LyudSailingFleetImpl::SeasonShare(S);
	const uint32_t Seed = SchoolHash((uint32_t)S.Now.Key, 0xB0A7u);
	const int32 Cap = S.DevFleet >= 0 ? S.DevFleet : LyudSailingFleetImpl::MaxUnits - 1;
	for (size_t K = 0; K < S.P.Sailors.size() && (int32)Brain.Units.size() <= Cap && (int32)Brain.Units.size() < LyudSailingFleetImpl::MaxUnits; K++)
	{
		const FSchoolSailor& Sailor = S.P.Sailors[K];
		const int C = Sailor.Class;
		const bool bIn = C >= 0 && C < SchoolBoats && Go.ClassOk[C] && (S.Now.Session.Classes >> C & 1) && (bRace || Sailor.Group == S.Now.Group || S.Now.Group == "race");
		if (!bIn)
		{
			continue;
		}
		int32 Mine = -1;
		for (size_t I = 0; I < S.Hulls.size() && Mine < 0; I++)
		{
			if (S.Hulls[I].BoatClass == C && S.Hulls[I].Unit < 0 && !S.Hulls[I].bAfloat && (int32)I != S.HeroHull)
			{
				Mine = (int32)I;
			}
		}
		if (Mine < 0 || !Dinghies[Mine])
		{
			continue;
		}
		const double Age = LyudSailingFleetImpl::AgeOf(S, Sailor.Slot, Sailor.Group);
		const auto Known = S.Learned.find(Sailor.Slot);
		const double Skill = FleetSkill(Sailor, Age, Share, Known != S.Learned.end() ? Known->second : 0.0, 0x51C0u);
		FFleetUnit U;
		U.Boat = &Dinghies[Mine]->Boat();
		U.Sailor = (int)K;
		HelmInit(U.Helm, Skill, SchoolHash(Seed, (uint32_t)K));
		S.Hulls[(size_t)Mine].Unit = (int32)Brain.Units.size();
		Brain.Units.push_back(U);
		S.UnitHull.push_back(Mine);
		S.UnitCrew.push_back(Sailor.Crew);
	}
	// they land in a row across the corridor where the trolleys stop today
	const int32 N = (int32)Brain.Units.size();
	const FFleetUnit Mine0 = Brain.Units[0];
	Brain.SetLaunchPoint(S.Lp.At);
	const FSchoolV2 Across = S.P.LaunchDir.Left();
	const double Gap = FMath::Min(2.6, 26.0 / FMath::Max(1, N - 2));
	for (int32 I = 1; I < N; I++)
	{
		Brain.Units[(size_t)I].Beach = S.Lp.At + Across * ((double(I - 1) - 0.5 * (N - 2)) * Gap) + S.P.LaunchDir * -1.5;
	}
	Brain.Units[0].Beach = Mine0.Stage == EFleetStage::Ashore ? S.Lp.At + Across * (-0.5 * (N - 2) * Gap - 4.0) : Mine0.Beach;
	Brain.GeneralRecalls = 0;
	Brain.Task = FSchoolLaunchTask();
	S.WaterKey = S.Now.Key;
	S.bWaterRace = bRace;
	S.bWaterWatched = true;
	S.bHomeCalled = false;
	S.LaunchNext = 1;
	S.LaunchIn = 0.0;
	S.WaterS = 0.0;
	S.DrillSet.clear();
	LaunchOut(true);
	if (LaunchBoat)
	{
		LaunchBoat->SetDriver(LyudSailingFleetImpl::PersonOf(S, S.Now.Coach));
	}
	if (bRace)
	{
		S.Race.Stage = 0;
		if (!StartRace())
		{
			// no course can be laid in today's water: the day is sailed as a practice round the area
			Brain.SetDrill("free", S.Now.Area);
		}
	}
	else
	{
		Brain.SetDrill(S.Now.Drill, S.Now.Area);
		S.DrillSet = S.Now.Drill;
		StowMarks(LayCourse(0, Brain.DrillCourse, true), 16);
	}
	UE_LOG(LogLyudSailFleet, Display, TEXT("SAIL water: %s %s, %d boats, drill %s in %s"), *LyudSailingImpl::Str(S.Now.Group), bRace ? TEXT("race") : TEXT("lesson"), N - 1,
		*LyudSailingImpl::Str(S.Now.Drill), *LyudSailingImpl::Str(S.Now.Area));
}

bool ULyudSailing::LaunchNextUnit()
{
	FLyudSailingState& S = *M;
	const int32 I = S.LaunchNext;
	if (I < 1 || I >= (int32)S.Brain.Units.size())
	{
		return false;
	}
	FFleetUnit& U = S.Brain.Units[(size_t)I];
	const FSchoolSailor& Sailor = S.P.Sailors[(size_t)U.Sailor];
	// the life brings them to the slip; a boat does not leave without her helmsman (but does not wait for ever)
	const FName Wanted = LyudSailingImpl::Name(Sailor.Slot);
	FName Person = NAME_None;
	bool bHere = true;
	for (const FLyudSailorNow& Now : S.Sailors)
	{
		if (Now.Slot == Wanted)
		{
			Person = Now.Person;
			bHere = Now.bAfloat;
			break;
		}
	}
	if (!bHere && S.WaterS < 240.0 && !S.bDevWater)
	{
		return false;
	}
	if (Person.IsNone())
	{
		Person = LyudSailingFleetImpl::PersonOf(S, Sailor.Slot);
	}
	const int32 HullAt = S.UnitHull[(size_t)I];
	ALyudDinghy* Boat = Dinghies[HullAt];
	const FSchoolV2 At = S.Brain.PushOff(S.Lp.At) + S.P.LaunchDir.Left() * (((I % 3) - 1) * 3.0);
	const float Age = float(LyudSailingFleetImpl::AgeOf(S, Sailor.Slot, Sailor.Group));
	Boat->SetActorHiddenInGame(false);
	Boat->PlaceAt(LyudSailingImpl::V2(At), float(S.Brain.PushOffYaw()), int32(EState::Sailing), 1.f);
	Boat->SetCrew(0, Person, Age);
	if (!S.UnitCrew[(size_t)I].empty())
	{
		Boat->SetCrew(1, LyudSailingFleetImpl::PersonOf(S, S.UnitCrew[(size_t)I]), Age);
	}
	Boat->SetLod(ELyudDinghyLod::Mid);
	U.Lod = FleetLodMid;
	FLyudSailHull& Hull = S.Hulls[(size_t)HullAt];
	Hull.bAfloat = true;
	Hull.Shown = -1;
	Hull.FarDt = 0.f;
	S.Brain.LaunchUnit(I);
	S.LaunchNext++;
	return true;
}

void ULyudSailing::UnitLanded(int32 Unit)
{
	FLyudSailingState& S = *M;
	if (Unit == 0)
	{
		// only the launch's tow lands the hero's boat
		HeroAshore(true);
		return;
	}
	if (Unit < 0 || Unit >= (int32)S.UnitHull.size())
	{
		return;
	}
	const int32 HullAt = S.UnitHull[(size_t)Unit];
	FLyudSailHull& Hull = S.Hulls[(size_t)HullAt];
	ALyudDinghy* Boat = Dinghies[HullAt];
	const FFleetUnit& U = S.Brain.Units[(size_t)Unit];
	// up the beach where she touched, sails still up; the crew are the town's again
	const FSchoolV2 At = U.Boat ? FSchoolV2(U.Boat->X, U.Boat->Y) : U.Beach;
	const bool bByHerPlace = (At - U.Beach).Len() < 25.0;
	Boat->ParkAt(BeachTransform(bByHerPlace ? At : U.Beach), true, false);
	Hull.bAfloat = false;
	Hull.Shown = LyudSailingFleetImpl::OnBeach;
}

void ULyudSailing::EndWater()
{
	FLyudSailingState& S = *M;
	FFleetBrain& Brain = S.Brain;
	for (size_t I = 1; I < Brain.Units.size(); I++)
	{
		const FFleetUnit& U = Brain.Units[I];
		if (U.Sailor >= 0)
		{
			double& Learned = S.Learned[S.P.Sailors[(size_t)U.Sailor].Slot];
			Learned = SchoolClamp(Learned + FleetSkillGain(U.AfloatS / 3600.0, U.Capsizes), 0.0, 0.12);
		}
		FLyudSailHull& Hull = S.Hulls[(size_t)S.UnitHull[I]];
		Hull.Unit = -1;
		if (Hull.bAfloat)
		{
			Hull.bAfloat = false;
			Hull.Shown = -1;
		}
	}
	Brain.Units.resize(1);
	S.UnitHull.resize(1);
	S.UnitCrew.resize(1);
	Brain.Mode = EFleetMode::Idle;
	Brain.bRecall = false;
	Brain.bRaceRunning = false;
	Brain.bHeld = false;
	Brain.Task = FSchoolLaunchTask();
	if (Brain.Towing > 0)
	{
		Brain.Towing = -1;
	}
	S.StatDone = S.WaterKey;
	S.WaterKey = 0;
	S.bWaterRace = false;
	S.bWaterWatched = false;
	S.DrillSet.clear();
	if (S.bDevWater)
	{
		// the developer's session is over: the timetable's hour comes back with the next look at the clock
		S.bDevWater = false;
		S.DevCourse.clear();
		for (size_t I = S.States.size(); I-- > 0;)
		{
			if (S.States[I].Key == S.DevNow.Key)
			{
				S.States.erase(S.States.begin() + (std::ptrdiff_t)I);
			}
		}
		S.Decisions.erase(S.DevNow.Key);
		S.Now = FSchoolNow();
		FactsIn = 0.f;
	}
	StowMarks(0, 16);
	S.LaunchFlags[0] = S.LaunchFlags[1] = NAME_None;
	if (LaunchBoat)
	{
		LaunchBoat->SetFlag(NAME_None);
	}
	ParkHulls(false);
	UE_LOG(LogLyudSailFleet, Display, TEXT("SAIL water: everybody ashore"));
}

void ULyudSailing::WaterControl(float Dt)
{
	FLyudSailingState& S = *M;
	const FSchoolSessionState* St = S.Now.bOn ? LyudSailingFleetImpl::StateOf(S, S.Now.Key) : nullptr;
	const bool bSail = S.Now.bOn && S.Now.bAfloat && St && St->Go == ESchoolGo::Sail && St->RecallHour < 0.0 && S.Lp.bOk;
	if (S.WaterKey == 0)
	{
		if (bSail && S.DevFleet != 0 && S.Hour < S.Now.Session.WaterTo - 0.25 && S.Brain.AllAshore()
			&& !(S.Now.Kind == "race" && !S.bDevWater && (S.Race.Stage == 3 || S.ResolvedDay == S.Day)))
		{
			BeginWater(S.Now.Kind == "race");
		}
		return;
	}
	S.WaterS += Dt;
	S.SailorsIn -= Dt;
	if (S.SailorsIn <= 0.0)
	{
		S.SailorsIn = 2.0;
		S.Sailors.Reset();
		if (const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
		{
			People->GetSailors(S.Sailors);
		}
	}
	const bool bStill = bSail && S.Now.Key == S.WaterKey && S.Hour < S.Now.Session.WaterTo - 0.1 && !(S.bWaterRace && S.Race.Stage == 3);
	if (!bStill && !S.bHomeCalled)
	{
		S.bHomeCalled = true;
		// the weather's recall is the three blasts and the flag; the hour's end is one long whistle
		const bool bWeather = St && St->RecallHour >= 0.0;
		S.Brain.Recall(bWeather);
		if (LaunchBoat)
		{
			if (bWeather)
			{
				LaunchBoat->SetFlag(TEXT("flag_first_sub"));
			}
			else
			{
				LaunchBoat->Sound(1);
			}
		}
	}
	if (bStill && !S.bWaterRace && S.DrillSet != S.Now.Drill)
	{
		S.DrillSet = S.Now.Drill;
		S.Brain.SetDrill(S.Now.Drill, S.Now.Area);
		StowMarks(LayCourse(0, S.Brain.DrillCourse, true), 16);
	}
	S.LaunchIn -= Dt;
	if (bStill && S.LaunchNext < (int32)S.Brain.Units.size() && S.LaunchIn <= 0.0)
	{
		S.LaunchIn = LaunchNextUnit() ? 7.0 : 1.0;
	}
	if (S.bWaterRace)
	{
		TickRace(Dt);
	}
	if (S.bHomeCalled && S.Brain.AllAshore())
	{
		EndWater();
	}
}

void ULyudSailing::ResolveUnwatched()
{
	FLyudSailingState& S = *M;
	if (S.WaterKey != 0)
	{
		// he walked away from a session in full swing: the boats are taken off the water and the rest goes by numbers
		for (size_t I = 1; I < S.Brain.Units.size(); I++)
		{
			FLyudSailHull& Hull = S.Hulls[(size_t)S.UnitHull[I]];
			if (Hull.bAfloat)
			{
				Hull.bAfloat = false;
				Hull.Shown = -1;
				if (ALyudDinghy* Boat = Dinghies[S.UnitHull[I]])
				{
					Boat->SetCrew(0, NAME_None, 0.f);
					Boat->SetCrew(1, NAME_None, 0.f);
				}
			}
		}
		const bool bWasRace = S.bWaterRace;
		EndWater();
		if (bWasRace)
		{
			S.Race.Stage = S.Race.Stage == 3 ? 3 : 0;
		}
		LaunchOut(false);
	}
	// a lesson nobody watched, once it is over: what it taught
	if (S.Now.bOn && S.Now.Kind != "race" && S.Now.Session.bAfloat && S.StatDone != S.Now.Key && S.Hour >= S.Now.Session.WaterTo)
	{
		const FSchoolSessionState* St = LyudSailingFleetImpl::StateOf(S, S.Now.Key);
		if (St && St->Go == ESchoolGo::Sail)
		{
			const double Hours = FMath::Max(0.0, (St->RecallHour >= 0.0 ? St->RecallHour : S.Now.Session.WaterTo) - S.Now.Session.WaterFrom);
			const double Share = LyudSailingFleetImpl::SeasonShare(S);
			for (const FSchoolSailor& Sailor : S.P.Sailors)
			{
				if (Sailor.Group != S.Now.Group)
				{
					continue;
				}
				double& Learned = S.Learned[Sailor.Slot];
				const double Skill = FleetSkill(Sailor, LyudSailingFleetImpl::AgeOf(S, Sailor.Slot, Sailor.Group), Share, Learned, 0x51C0u);
				const FFleetSessionStat Stat = FleetResolveSession(S.P, (uint32_t)S.Now.Key, Sailor, Skill, S.W.Wind, Hours);
				Learned = SchoolClamp(Learned + FleetSkillGain(Hours, Stat.Capsizes), 0.0, 0.12);
			}
		}
		S.StatDone = S.Now.Key;
	}
}

// ------------------------------------------------------------------ the tick

void ULyudSailing::FleetLod(const FVector& Eye)
{
	FLyudSailingState& S = *M;
	int Lod[LyudSailingFleetImpl::MaxUnits];
	double Dist[LyudSailingFleetImpl::MaxUnits];
	bool Pinned[LyudSailingFleetImpl::MaxUnits];
	const int32 N = FMath::Min((int32)S.Brain.Units.size(), LyudSailingFleetImpl::MaxUnits);
	const FSchoolV2 EyeM = LyudSailingImpl::MetresOf(Eye);
	for (int32 I = 0; I < N; I++)
	{
		const FFleetUnit& U = S.Brain.Units[(size_t)I];
		const int32 HullAt = S.UnitHull[(size_t)I];
		const bool bOut = HullAt >= 0 && S.Hulls[(size_t)HullAt].bAfloat && U.Boat;
		Dist[I] = bOut ? (FSchoolV2(U.Boat->X, U.Boat->Y) - EyeM).Len() : 1e9;
		Pinned[I] = I == 0 && bOut;
		Lod[I] = !bOut ? FleetLodFar : (Pinned[I] ? FleetLodNear : FleetLodOf(Dist[I], U.Lod));
	}
	FleetLodBudget(Lod, Dist, Pinned, N, LyudSailingFleetImpl::CVarNearBoats.GetValueOnGameThread());
	for (int32 I = 0; I < N; I++)
	{
		FFleetUnit& U = S.Brain.Units[(size_t)I];
		U.Lod = Lod[I];
		const int32 HullAt = S.UnitHull[(size_t)I];
		if (HullAt < 0 || !S.Hulls[(size_t)HullAt].bAfloat)
		{
			continue;
		}
		ALyudDinghy* Boat = Dinghies[HullAt];
		const ELyudDinghyLod Want = LyudSailingFleetImpl::LodOf(Lod[I]);
		if (Boat && Boat->GetLod() != Want && !(I == 0 && Boat->IsHeroAboard()))
		{
			Boat->SetLod(Want);
		}
	}
}

void ULyudSailing::FleetEvents()
{
	FLyudSailingState& S = *M;
	const FFleetBrain& Brain = S.Brain;
	// the events are copied out first and the brain's list emptied: what they set off (a landing, a closed race) may add to
	// it, and those are heard at the next call (the tick calls this before the brain thinks and after)
	FFleetEvent Events[48];
	const int32 Count = FMath::Clamp(Brain.EventCount, 0, 48);
	for (int32 K = 0; K < Count; K++)
	{
		Events[K] = Brain.Events[K];
	}
	S.Brain.EventCount = 0;
	auto ShowFlag = [this, &S]()
	{
		if (LaunchBoat)
		{
			LaunchBoat->SetFlag(!S.LaunchFlags[1].IsNone() ? S.LaunchFlags[1] : S.LaunchFlags[0]);
		}
	};
	for (int32 K = 0; K < Count; K++)
	{
		const FFleetEvent& E = Events[K];
		switch (E.Kind)
		{
		case EFleetSignal::WhistleShort: if (LaunchBoat) { LaunchBoat->Sound(0); } break;
		case EFleetSignal::WhistleLong: if (LaunchBoat) { LaunchBoat->Sound(1); } break;
		case EFleetSignal::WhistleRecall: if (LaunchBoat) { LaunchBoat->Sound(2); } break;
		case EFleetSignal::HornShort: if (LaunchBoat) { LaunchBoat->Sound(3); } break;
		case EFleetSignal::HornLong: if (LaunchBoat) { LaunchBoat->Sound(4); } break;
		case EFleetSignal::FlagUp:
		case EFleetSignal::FlagDown:
		{
			if (E.Other < 0 || E.Other >= (int)Brain.Sequence.size())
			{
				break;
			}
			const FName Flag = LyudSailingImpl::Name(Brain.Sequence[(size_t)E.Other].Flag);
			if (E.Kind == EFleetSignal::FlagUp)
			{
				S.LaunchFlags[S.LaunchFlags[0].IsNone() ? 0 : 1] = Flag;
			}
			else if (S.LaunchFlags[1] == Flag)
			{
				S.LaunchFlags[1] = NAME_None;
			}
			else if (S.LaunchFlags[0] == Flag)
			{
				S.LaunchFlags[0] = S.LaunchFlags[1];
				S.LaunchFlags[1] = NAME_None;
			}
			ShowFlag();
			break;
		}
		case EFleetSignal::FlagX:
			if (LaunchBoat)
			{
				if (E.Value > 0.5)
				{
					LaunchBoat->SetFlag(TEXT("flag_x"));
				}
				else
				{
					ShowFlag();
				}
			}
			break;
		case EFleetSignal::Capsize:
			if (E.Unit > 0 && E.Unit < (int)Brain.Units.size() && Brain.Units[(size_t)E.Unit].Boat && S.bWaterRace)
			{
				S.NewsCapsizes[FMath::Clamp((int32)Brain.Units[(size_t)E.Unit].Boat->Class, 0, SchoolBoats - 1)]++;
			}
			break;
		case EFleetSignal::Landed:
			UnitLanded(E.Unit);
			break;
		case EFleetSignal::Ocs:
			if (E.Unit == 0 && S.Race.bHeroIn)
			{
				Say(TEXT("sail.race.ocs"));
			}
			break;
		case EFleetSignal::Finished:
			if (E.Unit == 0 && S.Race.bHeroIn)
			{
				SayWith(TEXT("sail.race.finish"), FString::FromInt(FMath::Max(1, FMath::RoundToInt32(E.Value))));
			}
			break;
		case EFleetSignal::GeneralRecall:
			S.bNewsRecall = true;
			S.Race.bRecalled = true;
			if (S.Race.bHeroIn && S.Hero.bAboard)
			{
				Say(TEXT("sail.race.general_recall"));
			}
			break;
		case EFleetSignal::RaceOver:
			RaceClosed(false);
			break;
		case EFleetSignal::RaceAbandoned:
			RaceClosed(true);
			break;
		default:
			break;
		}
	}
}

void ULyudSailing::TickFleet(const FVector& Eye, float Dt, bool bNear)
{
	FLyudSailingState& S = *M;
	if (!bNear)
	{
		if (bFleetMade)
		{
			ResolveUnwatched();
		}
		else if (S.Now.bOn)
		{
			// the fleet was never made (he has not been near this session): still, the lessons teach
			if (S.Now.Kind != "race" && S.Now.Session.bAfloat && S.StatDone != S.Now.Key && S.Hour >= S.Now.Session.WaterTo)
			{
				ResolveUnwatched();
			}
		}
		return;
	}
	if (!MakeFleet())
	{
		return;
	}
	WaterControl(Dt);
	S.ParkIn -= Dt;
	if (S.ParkIn <= 0.0)
	{
		S.ParkIn = 2.0;
		ParkHulls(false);
		if (S.Lp.bOk && S.WaterKey == 0 && (S.Lp.At - S.LpBrain).Len() > 1.0)
		{
			// the tide moves the trolleys' stop
			S.LpBrain = S.Lp.At;
			const FFleetUnit Mine = S.Brain.Units[0];
			S.Brain.SetLaunchPoint(S.Lp.At);
			if (Mine.Stage != EFleetStage::Ashore)
			{
				S.Brain.Units[0].Beach = Mine.Beach;
			}
		}
	}
	bool bAny = S.bLaunchOut;
	for (const FLyudSailHull& Hull : S.Hulls)
	{
		bAny = bAny || Hull.bAfloat;
	}
	if (!bAny)
	{
		return;
	}
	S.LodIn -= Dt;
	if (S.LodIn <= 0.0)
	{
		S.LodIn = 0.25;
		FleetLod(Eye);
	}
	FFleetBrain& Brain = S.Brain;
	const int32 N = (int32)Brain.Units.size();
	for (int32 I = 0; I < N; I++)
	{
		const int32 HullAt = S.UnitHull[(size_t)I];
		if (HullAt >= 0 && S.Hulls[(size_t)HullAt].bAfloat && Dinghies[HullAt])
		{
			Brain.Units[(size_t)I].Env = Dinghies[HullAt]->Env();
		}
	}
	// what the brain was told between two ticks (a recall, an abandoned race) has its signals too
	FleetEvents();
	Brain.Think(Dt);
	const FVector2D Hitch = LaunchBoat ? LaunchBoat->HitchM() : FVector2D::ZeroVector;
	for (int32 I = 0; I < N; I++)
	{
		const int32 HullAt = S.UnitHull[(size_t)I];
		if (HullAt < 0 || !S.Hulls[(size_t)HullAt].bAfloat || !Dinghies[HullAt])
		{
			continue;
		}
		FLyudSailHull& Hull = S.Hulls[(size_t)HullAt];
		ALyudDinghy* Boat = Dinghies[HullAt];
		const FFleetUnit& U = Brain.Units[(size_t)I];
		if (U.Stage == EFleetStage::Towed)
		{
			Boat->AdvanceTowed(Hitch, 6.f, Dt);
		}
		else if (I == 0)
		{
			// the hero's: his own hands when he is aboard, else she lies to her painter
			Boat->Advance(FControls(), Dt);
		}
		else if (U.Lod == FleetLodFar)
		{
			// far boats move on their polar ten times a second, each in its own frame
			Hull.FarDt += Dt;
			if (Hull.FarDt >= 0.1f)
			{
				Boat->AdvanceFar(float(U.Helm.CourseYaw), float(U.Helm.SpeedScale), Hull.FarDt);
				Hull.FarDt = 0.f;
			}
		}
		else
		{
			Boat->Advance(U.Controls, Dt);
		}
	}
	if (LaunchBoat && S.bLaunchOut)
	{
		LaunchBoat->Advance(LyudSailingImpl::V2(Brain.LaunchTarget), float(Brain.LaunchCruise), float(Brain.LaunchRadius), Dt);
		// nothing left to do on the water: back to the mooring
		const FVector2D At = LaunchBoat->PosM();
		const bool bLead = S.Hero.bLesson && S.Hero.bAboard && Brain.Mode == EFleetMode::Drill;
		if (S.WaterKey == 0 && Brain.ForceTow < 0 && Brain.Towing < 0 && !bLead && (FSchoolV2(At.X, At.Y) - Brain.Home).Len() < 14.0)
		{
			S.LaunchIdleS += Dt;
			if (S.LaunchIdleS > 20.0)
			{
				LaunchOut(false);
			}
		}
		else
		{
			S.LaunchIdleS = 0.0;
		}
	}
	Brain.After(Dt);
	FleetEvents();
	// the marks near the eye ride the waves
	if (S.MarksOut != 0)
	{
		const FVector2D EyeM(Eye.X * 0.01, Eye.Y * 0.01);
		for (int32 I = 0; I < MarkPool.Num(); I++)
		{
			if ((S.MarksOut >> I & 1u) && MarkPool[I] && FVector2D::DistSquared(MarkPool[I]->PosM(), EyeM) < FMath::Square(180.0))
			{
				MarkPool[I]->Advance(Dt);
			}
		}
	}
}
