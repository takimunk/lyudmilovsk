// A regatta day on the water: the courses laid at the warning signal, the launch as the committee boat, each race
// closed into the log, the wait between races, the town at the rail, the fanfare and the prizes.
// docs/yacht_school.md "Engine: the school".
#include "Sailing/LyudSailing.h"

#include "LyudSailingState.h"

#include "Audio/LyudSoundscape.h"
#include "Festival/LyudFestival.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "People/PeopleLife.h"
#include "Sailing/LyudDinghy.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailRace, Log, All);

using namespace LyudSail;

bool ULyudSailing::StartRace()
{
	FLyudSailingState& S = *M;
	const FSchoolParams& P = S.P;
	FFleetBrain& Brain = S.Brain;
	FLyudSailRaceRun& R = S.Race;
	if (R.Wanted == 0)
	{
		R.Event = RaceDayOn(P, S.Date).Event;
		R.Wanted = FMath::Max(1, S.Now.Session.Races);
	}
	// who is out: the classes of the boats the session brought to the water
	int32 Boats[SchoolBoats] = { 0, 0, 0 };
	for (size_t I = 1; I < Brain.Units.size(); I++)
	{
		if (Brain.Units[I].Boat)
		{
			Boats[FMath::Clamp((int32)Brain.Units[I].Boat->Class, 0, SchoolBoats - 1)]++;
		}
	}
	const bool bHero = R.bHeroIn && S.Hero.bBoatOut && !S.Hero.bLeftElsewhere;
	if (bHero)
	{
		Boats[P.HeroClass]++;
	}
	int MainMask = 0;
	for (int C = SchoolCadet; C < SchoolBoats; C++)
	{
		if (Boats[C] > 0)
		{
			MainMask |= 1 << C;
		}
	}
	const int LeadClass = Boats[SchoolLuch] > 0 ? SchoolLuch : (Boats[SchoolCadet] > 0 ? SchoolCadet : SchoolOptimist);
	ERaceCourse Kind = ERaceCourse::WindwardLeeward;
	if (!S.DevCourse.empty())
	{
		Kind = RaceCourseKind(S.DevCourse);
	}
	else if (R.Event >= 0 && R.Event < (int32)P.Events.size() && !P.Events[(size_t)R.Event].Course.empty())
	{
		Kind = RaceCourseKind(P.Events[(size_t)R.Event].Course);
	}
	if (Kind == ERaceCourse::KidsTriangle || Kind == ERaceCourse::Figure8 || Kind == ERaceCourse::StartLine)
	{
		// the big boats do not race round the children's buoys
		Kind = ERaceCourse::Triangle;
	}
	const int EventNo = FMath::Max(0, R.Event);
	FRaceCourse Main, Kids;
	if (MainMask != 0)
	{
		Main = RaceSetCourse(P, Kind, LeadClass, S.WindTo, S.W.Wind, Boats[SchoolCadet] + Boats[SchoolLuch], RaceSeed(S.Date, EventNo, R.Sailed, LeadClass), S.W.TideM);
		if (!Main.bOk)
		{
			MainMask = 0;
		}
	}
	if (Boats[SchoolOptimist] > 0)
	{
		Kids = RaceSetCourse(P, ERaceCourse::KidsTriangle, SchoolOptimist, S.WindTo, S.W.Wind, Boats[SchoolOptimist], RaceSeed(S.Date, EventNo, R.Sailed, SchoolOptimist), S.W.TideM);
	}
	if (MainMask == 0 && !Kids.bOk)
	{
		UE_LOG(LogLyudSailRace, Display, TEXT("SAIL race: no course can be laid in today's water"));
		R.Stage = 3;
		return false;
	}
	Brain.SetRace(Main, MainMask, Kids.bOk ? &Kids : nullptr, MainMask != 0 ? LeadClass : SchoolOptimist);
	// the hero's boat is a unit of the fleet for good: she is in the race only when he entered it
	FFleetUnit& Mine = Brain.Units[0];
	if (!bHero && Mine.Group >= 0)
	{
		Brain.Groups[Mine.Group].Entries = FMath::Max(0, Brain.Groups[Mine.Group].Entries - 1);
		Mine.Group = -1;
	}
	int32 N = LayCourse(0, Main, false);
	N += LayCourse(N, Kids, MainMask != 0);
	StowMarks(N, 16);
	R.Stage = 1;
	S.Hero.bStartSaid = false;
	if (S.NewsDay != S.Day)
	{
		S.NewsDay = S.Day;
		S.bNewsRecall = false;
		for (int C = 0; C < SchoolBoats; C++)
		{
			S.NewsCapsizes[C] = 0;
			S.NewsBoats[C] = 0;
		}
	}
	for (int C = 0; C < SchoolBoats; C++)
	{
		S.NewsBoats[C] = FMath::Max(S.NewsBoats[C], Boats[C]);
	}
	UE_LOG(LogLyudSailRace, Display, TEXT("SAIL race %d of %d: main course %s (%.0f m, classes %d), children's %s, hero %s"), R.Sailed + 1, R.Wanted, Main.bOk ? TEXT("laid") : TEXT("none"),
		Main.LengthM, MainMask, Kids.bOk ? TEXT("laid") : TEXT("none"), bHero ? TEXT("in") : TEXT("out"));
	return true;
}

void ULyudSailing::TickRace(float Dt)
{
	FLyudSailingState& S = *M;
	FLyudSailRaceRun& R = S.Race;
	if (R.Stage == 2 && !S.bHomeCalled)
	{
		R.BetweenS -= Dt;
		if (R.BetweenS <= 0.0)
		{
			// room for another before the water's time is up? (the limit and a way home)
			const double LeftS = (S.Now.Session.WaterTo - S.Hour) * 3600.0;
			if (LeftS < S.P.WarningS + 0.6 * S.P.TimeLimitS[SchoolLuch] || !StartRace())
			{
				R.Stage = 3;
			}
		}
	}
	else if (R.Stage == 1 && S.Brain.bRaceRunning && S.Brain.RaceT >= 0.0 && !S.Hero.bStartSaid)
	{
		S.Hero.bStartSaid = true;
		if (R.bHeroIn && S.Hero.bAboard)
		{
			Say(TEXT("sail.race.started"));
		}
	}
}

void ULyudSailing::RaceClosed(bool bAbandoned)
{
	FLyudSailingState& S = *M;
	FLyudSailRaceRun& R = S.Race;
	if (S.WaterKey == 0 || !S.bWaterRace || R.Stage != 1)
	{
		return;
	}
	if (!bAbandoned)
	{
		FSchoolEvents Ev;
		for (int C = 0; C < SchoolBoats; C++)
		{
			const std::vector<FRaceFinish>& Res = S.Brain.ResultsOf(C);
			if (Res.empty())
			{
				continue;
			}
			if (R.Event >= 0)
			{
				Ev.Add(SchoolRecordRace(S.P, S.Journal, S.Date, R.Event, C, Res, S.W.Wind));
			}
			if (!Res.empty())
			{
				UE_LOG(LogLyudSailRace, Display, TEXT("SAIL race %d, %s: %d boats, first %s in %.0f s"), R.Sailed + 1, UTF8_TO_TCHAR(SchoolBoatId(C)), (int32)Res.size(),
					*LyudSailingImpl::Str(Res[0].Slot), Res[0].TimeS);
			}
		}
		R.Sailed++;
		Emit(Ev);
	}
	else if (R.bHeroIn && S.Hero.bAboard)
	{
		Say(TEXT("sail.race.abandoned"));
	}
	if (S.Brain.Mode == EFleetMode::Recall || S.bHomeCalled)
	{
		// called in by the weather or the hour: the day's racing is over
		R.Stage = 3;
	}
	else if (R.Sailed >= R.Wanted)
	{
		R.Stage = 3;
		S.Brain.EndRace(true);
	}
	else
	{
		R.Stage = 2;
		R.BetweenS = S.P.BetweenRacesS;
		S.Brain.EndRace(false);
	}
	S.DevCourse.clear();
}

void ULyudSailing::RaceHud()
{
	FLyudSailingState& S = *M;
	ALyudDinghy* Boat = HeroDinghy();
	if (!Boat || S.Brain.Units.empty())
	{
		return;
	}
	const FFleetBrain& Brain = S.Brain;
	const FFleetUnit& U = Brain.Units[0];
	TArray<FText>& Lines = S.Hero.HudLines;
	Lines.Reset();
	float Countdown = -1.f;
	FVector2D Mark = FVector2D::ZeroVector;
	if (U.Group >= 0 && S.Race.Stage == 1)
	{
		const FFleetRaceGroup& G = Brain.Groups[U.Group];
		Mark = LyudSailingImpl::V2(Brain.NextMarkOf(0));
		if (!U.Tracker.bStarted)
		{
			Lines.Add(LyudText::Get(U.Tracker.bOcs ? TEXT("sail.race.ocs") : TEXT("ui.sail.task.start")));
			if (!Brain.bHeld && Brain.RaceT < 0.0)
			{
				Countdown = float(-Brain.RaceT);
			}
		}
		else if (U.Tracker.Status == ERaceStatus::Finished)
		{
			Lines.Add(LyudText::Get(TEXT("sail.race.finish"), { FStringFormatArg(FMath::Max(1, U.Place)) }));
			Mark = LyudSailingImpl::V2(Brain.Home);
		}
		else
		{
			Lines.Add(LyudText::Get(TEXT("ui.sail.task.pos"), { FStringFormatArg(Brain.PlaceNow(0)), FStringFormatArg(FMath::Max(1, G.Entries)) }));
			const bool bMark = U.Tracker.Next >= 0 && U.Tracker.Next < (int)G.Course.Order.size();
			const FString MarkName = bMark ? LyudSailingImpl::Str(G.Course.Marks[(size_t)G.Course.Order[(size_t)U.Tracker.Next]].Id) : FString(TEXT("F"));
			Lines.Add(LyudText::Get(TEXT("ui.sail.task.mark"), { FStringFormatArg(MarkName) }));
		}
	}
	else if (S.Race.Stage == 2)
	{
		Lines.Add(LyudText::Get(TEXT("ui.sail.task.start")));
		Countdown = float(FMath::Max(0.0, S.Race.BetweenS));
	}
	else if (S.Race.Stage == 3)
	{
		Lines.Add(LyudText::Get(TEXT("sail.recall.call")));
		Mark = LyudSailingImpl::V2(Brain.Home);
	}
	Boat->SetHudTask(LyudText::Get(TEXT("ui.sail.task.race")), Lines, Countdown, Mark);
}

void ULyudSailing::RaceWatchers()
{
	FLyudSailingState& S = *M;
	FLyudSailRaceRun& R = S.Race;
	UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	// the town at the rail while the boats race (on the opening, the cup and the closing the festival itself leads them)
	std::string Where;
	const double Pull = SchoolSpectatorPull(S.P, S.Date, S.Hour, S.bDayOff, &Where);
	bool bWant = Pull > 0.0 && S.Now.bOn && S.Now.Kind == "race" && S.Now.bAfloat;
	if (bWant)
	{
		const int32 Event = RaceDayOn(S.P, S.Date).Event;
		const std::string Festival = Event >= 0 ? S.P.Events[(size_t)Event].Festival : std::string();
		if (const ULyudFestivalSubsystem* Fest = Festival.empty() ? nullptr : ULyudFestivalSubsystem::Get(this))
		{
			const FName FestId = LyudSailingImpl::Name(Festival);
			for (const FLyudFestivalNow& F : Fest->ActiveFestivals())
			{
				bWant = bWant && F.Id != FestId;
			}
		}
	}
	if (People && bWant != R.bGathering)
	{
		R.bGathering = bWant;
		if (bWant)
		{
			const FSchoolArea* Area = S.P.Area("race");
			const FVector Face = LyudSailingImpl::CmOf(Area ? SchoolCentroid(*Area) : S.Brain.CorridorEnd, S.P.SeaZ * 100.0);
			People->SetGathering(TEXT("sail_watch"), TEXT("sail_spectators"), float(S.Hour), float(S.Now.Session.WaterTo), float(Pull), EPeopleGatherMode::Watch, Face);
		}
		else
		{
			People->ClearGathering(TEXT("sail_watch"));
		}
	}
	// the prize-giving at the podium: the fanfare once, and the hero's prize when he is on it
	if (S.Now.bOn && S.Now.Phase == "prizes" && !R.bPrizes)
	{
		R.bPrizes = true;
		const FSchoolPlace* Podium = S.P.Place("podium");
		if (ULyudSoundscapeSubsystem* Scape = Podium ? ULyudSoundscapeSubsystem::Get(this) : nullptr)
		{
			Scape->PlayEffect(TEXT("sail_fanfare"), LyudSailingImpl::CmOf(Podium->At) + FVector(0.0, 0.0, 150.0), 1.f);
		}
		int32 Place = 0;
		for (const FSchoolDayTable& Tab : S.Journal.Days)
		{
			if (Tab.Day != S.Day || Tab.Class != S.P.HeroClass)
			{
				continue;
			}
			for (size_t I = 0; I < Tab.Points.size(); I++)
			{
				if (Tab.Points[I].first == "hero")
				{
					Place = (int32)I + 1;
				}
			}
		}
		R.HeroPlace = Place;
		const int32 Event = RaceDayOn(S.P, S.Date).Event;
		if (R.bHeroIn && Place >= 1 && Place <= 3 && Event >= 0)
		{
			// the entry fees come back to the podium: three, two and one of them (at least the hire's price)
			const int32 Prize = (4 - Place) * FMath::Max(S.P.HireFee, S.P.Events[(size_t)Event].EntryFee);
			if (ULyudInventory* Inv = ULyudInventory::Get(this))
			{
				Inv->Earn(Prize);
				SayWith(TEXT("sail.race.prizes"), FString::FromInt(Prize));
			}
		}
	}
}
