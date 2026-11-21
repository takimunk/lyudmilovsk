// The Aton station as the club's front door (docs/front_door.md, backlog item 40), the rules in plain C++ so the game and
// tools/transportsim (run.sh board) run the same ones:
//   - the departures board: the next cabins out of a station on a line, from the running network (cabins boarding there
//     or on their way to it) and the station life's trips about to leave from its hall;
//   - the counter's line in a hall: who stands where, the hero's held spot, stepping up;
//   - the regulars' usual: what somebody has bought at a counter often enough that it is "the usual";
//   - the lost and found: the residents' things that turn up on the shelf, carried, handed back for a small gift;
//   - the travel card: a stamp for every station, all seven make the set.
#pragma once

#include "CablewaySim.h"
#include "StationLife.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace LyudFrontDoor
{
	// --- the departures board ---------------------------------------------------------------------------------------

	enum class EDepKind : uint8_t
	{
		Boarding,   // a cabin stands at the station with its doors for this way: getting on now
		Arriving,   // a cabin on its way that will stop here
		Planned,    // the station life's riders waiting in the hall will call one at this time
		OnCall      // nothing running: how long a cabin would take if called now
	};

	struct FDeparture
	{
		float Hour = 0.f;           // the clock's hour it leaves (or comes in), today's (may pass 24)
		int To = -1;                // the station it goes to (the farthest on this line its riders want)
		int Cabin = -1;             // the network's cabin (-1: not known yet)
		EDepKind Kind = EDepKind::OnCall;
		int People = 0;             // riders aboard or about to get on
	};

	/** The next departures out of station St on line L at clock Hour (the network's own time runs alongside), at most Max
	 *  rows, the soonest first. Life may be null (no station life: only the running cabins, or one "on call" row). */
	void Departures(const LyudTransport::FNetwork& N, const LyudTransport::FStationLife* Life, int St, int L, float Hour, int Max,
		std::vector<FDeparture>& Out);

	/** "07:42" for an hour of the clock (wraps past 24). */
	std::string Clock(float Hour);

	/** The way (Dir) from station A to B on line L: 0 the line's order, 1 back, -1 one of them isn't on it. */
	int DirOn(const LyudTransport::FLine& L, int A, int B);
	/** Where a rider from St to To leaves line L: To itself when it is on L, else the station of L where they change. */
	int LegEnd(const LyudTransport::FNetwork& N, int L, int St, int To);

	// --- the counter's line in a hall -------------------------------------------------------------------------------

	/** The line's spots front first: -1 free, -2 held by the hero, else whoever stands there. Who joins goes to the first
	 *  free spot that has every spot before it taken or held (-1: the line is full, or its front is free and nobody may
	 *  pass it to the back). */
	int JoinSpot(const std::vector<int>& Line);
	/** The first spot whose person should step up into the free spot before them (-1: none). Out: the spot they go to. */
	int StepUp(const std::vector<int>& Line, int& OutTo);

	// --- the regulars' usual ----------------------------------------------------------------------------------------

	/** Counts per counter of what was bought there ("<counter>|<item>" -> times). */
	struct FUsual
	{
		std::map<std::string, int> Bought;
		static constexpr int Times = 3;      // bought this often at a counter: it is the usual there

		void Add(const std::string& Counter, const std::string& Item, int Count = 1);
		/** The item bought most at the counter, at least Times times ("" none); ties: the first by name. */
		std::string Of(const std::string& Counter, int* OutTimes = nullptr) const;
		/** All visits at a counter (how well they know him there). */
		int Visits(const std::string& Counter) const;
		std::string Save() const;
		void Load(const std::string& S);
	};

	// --- the lost and found ------------------------------------------------------------------------------------------

	/** The things that get lost (item ids lost_<kind>). */
	extern const char* const LostKinds[];
	constexpr int NumLostKinds = 6;
	/** What an owner gives back for their thing (item ids of the game's goods). */
	extern const char* const Gifts[];
	constexpr int NumGifts = 6;

	struct FLost
	{
		int Kind = 0;               // into LostKinds
		int Owner = -1;             // into FLostFound::Owners
		int Day = 0;                // the game day it was found
	};

	class FLostFound
	{
	public:
		/** The people who could lose something here (station life riders through the hall: their keys), and when they
		 *  usually come through (hour, for the attendant's hint). */
		std::vector<std::string> Owners;
		std::vector<float> OwnerHour;
		std::vector<FLost> Shelf;           // waiting on the shelf
		std::vector<FLost> Carried;         // the hero took them and hasn't handed them back
		int Returned = 0;
		int Day = -1;
		static constexpr int KeepDays = 5;  // a thing nobody takes goes to the depot's store after this many days
		static constexpr int MaxShelf = 4;

		/** A new game day: today's finds (0..2, seeded by the day) join the shelf; what lay there too long goes. */
		void StartDay(int InDay);
		/** The hero takes the thing at Index of the shelf (false: none). */
		bool Take(int Index, FLost& Out);
		/** A thing he carries that belongs to the owner Key (-1 none). */
		int CarriedFor(const std::string& Key) const;
		/** Hands back Carried[Index]: the gift's item id ("" bad index). */
		std::string GiveBack(int Index);
		/** The owner's index of a key (-1). */
		int OwnerOf(const std::string& Key) const;
		std::string Save() const;
		void Load(const std::string& S);
	};

	/** The owners for station St out of a station life: everyone with a trip from or to it, each once, their earliest
	 *  hour through it. */
	void OwnersAt(const LyudTransport::FStationLife& Life, int St, std::vector<std::string>& OutKeys, std::vector<float>& OutHours);

	// --- the travel card ---------------------------------------------------------------------------------------------

	struct FTravelCard
	{
		bool bHave = false;
		std::vector<std::string> Stamps;    // station ids, in the order punched

		/** A punch at station Id: true if it is a new stamp. */
		bool Punch(const std::string& Id);
		bool Has(const std::string& Id) const;
		std::string Save() const;
		void Load(const std::string& S);
	};

	/** A number 0..1 from a string and a salt (the same everywhere). */
	float Hash01(const std::string& S, uint32_t Salt);
}
