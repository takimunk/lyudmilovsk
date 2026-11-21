// The cable car's stations living by the town's day (docs/transport.md "Station life"), in plain C++ so the game
// (ULyudStationLife) and tools/transportsim (run.sh town) run the same rules: the townsfolk's trips of
// Content/Transport/station_life.json (tools/station_life.py) become real trips of the network (FNetwork::Travel), each
// rider first waits a few minutes in the hall they came into and walks out of the other one; the halls' staff by their
// rota; the wind stop.
#pragma once

#include "CablewaySim.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LyudTransport
{
	enum class ELifeCond : uint8_t { None, Warm, FestivalEnd };
	enum class ELifePhase : uint8_t { Pending, InHall, Riding, Out, Done, Skipped };

	struct FLifeRider
	{
		std::string Key, Group, Mesh;
		std::string With;           // a child along with a parent: the day's chance is drawn by the parent's key
		int Age = 35;
		bool bRegular = false;
	};

	struct FLifeTrip
	{
		int Rider = -1;             // index into Riders
		int From = -1, To = -1;     // stations
		float At = 0.f;             // hour they walk into the From hall (FestivalEnd: hours after the evening's end)
		uint8_t Days = 0x7F;        // bit per weekday, 0 Monday
		ELifeCond Cond = ELifeCond::None;
		float Chance = 1.f;
	};

	struct FLifeStaff
	{
		std::string Key, Role, Shift, Mesh;
		int Station = -1;
		float From = 0.f, To = 0.f; // hours (To < From: past midnight)
		uint8_t Days = 0;
	};

	/** One of today's trips running. */
	struct FLifeRun
	{
		int Trip = -1;
		float At = 0.f;             // hour into the From hall today
		float HallMinutes = 4.f;    // minutes in the hall before the platform
		ELifePhase Phase = ELifePhase::Pending;
		int NetRider = -1;          // the network's trip (Riding)
		double OutUntil = 0.0;      // network time they leave the To hall
		double RideStart = 0.0;     // network time the network's trip was asked for
		double Wait = 0.0;          // seconds from asking to boarding
	};

	class FStationLife
	{
	public:
		/** Network ids of the riders: Base + rider index (the people's life ids are small, the player's -100). */
		static constexpr int Base = 200000;
		float OpenHour = 5.75f, CloseHour = 24.25f;
		float WindStop = 17.f, WindGo = 12.f;      // m/s over the line: the stop above, going again below (and 5 min)
		float WindWarn = 11.f;                      // m/s: people in the halls start to worry
		std::vector<FLifeRider> Riders;
		std::vector<FLifeTrip> Trips;
		std::vector<FLifeStaff> Staff;
		std::vector<FLifeRun> Today;
		int Day = -1;
		std::string LastError;
		// stats (tests, the report)
		int Started = 0, Finished = 0, Cut = 0, Missed = 0;
		double MaxWait = 0.0, SumWait = 0.0;
		int WindStops = 0;

		bool Load(const std::string& JsonText, const FNetwork& N);
		/** A new day (Weekday 0 Monday; bWarm: a beach day): today's trips, by their chance seeded by the day. */
		void StartDay(int InDay, int InWeekday, bool bWarm);
		/** A festival's evening ended at Hour: its crowd goes home. */
		void FestivalEnded(float Hour);
		/** Runs today's trips at Hour (today's, may pass 24) against the network. */
		void Step(FNetwork& N, float Hour);
		/** The wind over the line (m/s): the stop and the going again, with a 5 minute calm needed. */
		void SetWind(FNetwork& N, float Speed);
		bool IsWindStopped() const { return bWindStopped; }
		float LastWind() const { return Wind; }

		/** Who is in station S's hall now: runs (indices into Today) waiting there or walking out. */
		void InHall(int Station, std::vector<int>& Out) const;
		/** The staff member of station S in Role ("attendant", "barista") on shift at Hour of Weekday, -1 none. */
		int OnShift(int Station, const std::string& Role, int Weekday, float Hour) const;
		bool IsOpen(float Hour) const;
		std::string Report(const FNetwork& N) const;

	private:
		bool bWindStopped = false;
		float Wind = 0.f;
		double CalmSince = -1.0;
		int Weekday = 0;
	};

	/** The hall's mood (docs/transport.md "Hall moods"): what the hour, the day and the weather make of the waiting people. */
	struct FHallMood
	{
		float Hour = 12.f;
		bool bWeekend = false;
		bool bRain = false;         // it rains or snows (PrecipRate over 0.1 mm/h)
		bool bWindy = false;        // over WindWarn: people worry
		bool bStopped = false;      // the wind stop
		bool bBuffet = true;        // the barista is on shift
		bool IsRush() const { return (Hour >= 7.f && Hour < 9.5f) || (Hour >= 16.5f && Hour < 19.f); }
		bool IsLunch() const { return Hour >= 11.5f && Hour < 14.5f; }
	};

	/** A hall's spots (ALyudStationHall's slots): as halls.json's seats and stands, plus the queue behind the counter. */
	enum class EHallSpot : uint8_t { Seat, TableSeat, Counter, Rail, Window, Diagram, Read, StandTable, Queue };
	/** What a waiting person does there (the hall maps it to EPersonPose). */
	enum class EHallPose : uint8_t { Stand, Talk, Look, Sit, Eat, Read, Work };

	/** How much a person of Age (walking out: bOut) wants spot Kind in Mood; 0: never. Roll 0..1 is theirs. */
	float HallSpotScore(int Age, bool bOut, EHallSpot Kind, const FHallMood& Mood, float Roll);
	/** What they do at that spot (Roll 0..1, theirs). */
	EHallPose HallPose(int Age, EHallSpot Kind, const FHallMood& Mood, float Roll);
	/** A place as the picker sees it: its kind, its turn in the queue (the counter 0, then 1, 2 behind it), whether
	 *  someone stands there (bTaken) and whether it can be taken now (bOpen: free and out of the player's sight). */
	struct FHallPlace
	{
		EHallSpot Kind = EHallSpot::Seat;
		int Queue = 0;
		bool bTaken = false;
		bool bOpen = true;
	};
	/** The place a person (Key, Age; walking out: bOut) takes: the best score among the open ones, a queue spot only behind
	 *  a taken counter and the spots before it; -1: none. */
	int HallPick(const std::vector<FHallPlace>& Places, const std::string& Key, int Age, bool bOut, const FHallMood& Mood, uint32_t Salt);

	/** Passers-by in for the buffet or out of the weather, besides the riders (Salt: the hall and the hour). */
	int HallFiller(const FHallMood& Mood, uint32_t Salt);
	/** The barista between customers: working while someone waits at the counter, else by the hour (Roll 0..1). */
	EHallPose BaristaPose(int Customers, const FHallMood& Mood, float Roll);
	/** The attendant's moment (every so often, Roll 0..1): the logbook in a quiet hour, a word to the hall at a stop. */
	EHallPose AttendantPose(int Waiting, const FHallMood& Mood, float Roll);
	/** The bark category of the waiting people's words (line.transport.wait.<it>.N). */
	const char* HallBark(const FHallMood& Mood);

	/** A ride's weather and hour as the people aboard feel it (docs/transport.md "Cabin life"). */
	struct FCabinMood
	{
		float Hour = 12.f;
		bool bFog = false;          // visibility under 1.5 km: the bay gone, the next pylon a shadow
		bool bRain = false;         // drops running down the glass
		bool IsDark() const { return Hour < 6.5f || Hour >= 20.5f; }
	};
	/** What a rider does on the way: sits, reads, or stands at the side glass (a child's nose to it, an adult at the view). */
	enum class ECabinAct : uint8_t { Sit, Read, Window };
	/** Their act for this ride (Roll 0..1: theirs and the ride's); Aboard: how many ride with them. */
	ECabinAct CabinAct(int Age, int Aboard, const FCabinMood& Mood, float Roll);
	/** At most two at the glass (one each side): the acts of the riders aboard (ages; rolls) with the extras sat down. */
	void CabinActs(const std::vector<int>& Ages, const std::vector<float>& Rolls, const FCabinMood& Mood, std::vector<ECabinAct>& Out);
	/** The words aboard: line.transport.ride.<it>.N (a child at the glass: "kid"). */
	const char* CabinBark(const FCabinMood& Mood, bool bKidAtGlass, float Roll);

	/** A number 0..1 from a string and a salt (the same in the tool, the game and the tests). */
	float LifeHash(const std::string& S, uint32_t Salt);
}
