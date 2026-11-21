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

	/** A number 0..1 from a string and a salt (the same in the tool, the game and the tests). */
	float LifeHash(const std::string& S, uint32_t Salt);
}
