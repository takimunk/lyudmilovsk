// The sea railway's timetable and motion, plain C++ (no Unreal types): where a train is at any moment of the game
// calendar. Shared by the game (ALyudTrain, the level crossing, the people's rides) and tools/railsim (run outside the
// engine). docs/railway.md.
//
// A run: a train leaves the tunnel's dark end (s = 0), stops at each stop for Dwell seconds, goes on out to sea to the
// line's end (Length), lies over there (hidden in the haze), comes back the same way stopping at every stop, and lies
// over in the tunnel until its next departure. Departures every Period seconds from First (seconds of the calendar day);
// a train that would need longer than the period is given the time (the period stretches). Its speed: VMax on the bank,
// VLand by the tunnel, the crossing and the beach, VIsland on the island, accelerating and braking smoothly.
#pragma once

#include <string>
#include <vector>

namespace LyudRail
{
struct FStop
{
	std::string Id;
	double S = 0.0;          // metres along the line from the tunnel's dark end
};

struct FRunParams
{
	double Length = 26000.0;   // the line's far end, m
	double HiddenS = 24000.0;  // past this the train is gone into the haze (not drawn)
	double PortalS = 283.0;    // the tunnel's mouth
	double SlowUntilS = 420.0; // VLand up to here (tunnel, crossing, beach halt)
	double IslandS = 2564.0, IslandHalf = 160.0;   // VIsland within IslandHalf of IslandS
	double VMax = 15.0, VLand = 7.0, VIsland = 9.0;
	double Accel = 0.45, Decel = 0.6;
	double Dwell = 45.0;       // seconds at a stop
	double Layover = 240.0;    // at each end, at least
	double Period = 3600.0;    // seconds between departures
	double First = 0.0;        // the first departure, seconds after midnight
	int Trains = 1;
	std::vector<FStop> Stops;  // in order along the line
};

struct FTrainState
{
	double S = 0.0;          // where the train's middle is, m along the line
	double V = 0.0;          // m/s, signed: + outbound (to the sea), - inbound
	int Dir = 1;             // +1 outbound, -1 inbound (where it heads or last headed)
	int Stop = -1;           // index of the stop it stands at (doors may open), else -1
	bool bDoorsOpen = false;
	bool bVisible = true;    // false in the tunnel's dark and past HiddenS
	bool bInService = true;  // false while it lies over at either end
	double Phase = 0.0;      // seconds since this run's departure
};

/** Reads the run's parameters from Content/Railway/railway_built.json's text (tools/railway.py). */
bool LoadParams(const std::string& BuiltJson, FRunParams& Out, std::string& Error);

class FRun
{
public:
	void Build(const FRunParams& InParams);
	const FRunParams& Params() const { return P; }
	/** The whole round trip's length in seconds (<= the period unless the period was stretched). */
	double CycleSeconds() const { return Cycle; }
	double OneWaySeconds() const { return OutT.empty() ? 0.0 : OutT.back(); }

	/** Train Index's state at Time (seconds of the game calendar since its epoch; any origin that counts whole days). */
	FTrainState At(double Time, int Index = 0) const;

	/** The next time (>= Time) a train stands at a stop with its doors open heading Dir (+1 out, -1 in): the moment its
	 *  doors open. -1 if it never does. OutTrain gets which train. */
	double NextDoorsOpen(int Stop, int Dir, double Time, int* OutTrain = nullptr) const;
	/** Seconds from the doors opening at stop A (heading Dir) to the doors opening at stop B (further along Dir). */
	double RideSeconds(int StopA, int StopB, int Dir) const;
	/** Seconds until the train next passes S (its middle) at or after Time, either way; for the crossing's warning. */
	double NextPass(double S, double Time, int* OutDir = nullptr) const;
	int FindStop(const std::string& Id) const;

private:
	FRunParams P;
	// one way, sampled every DT: time -> s (outbound; inbound mirrors it over the reversed stops)
	std::vector<double> OutT, OutS, OutV;
	std::vector<double> InT, InS, InV;
	std::vector<double> OutStopOpen, OutStopClose, InStopOpen, InStopClose;   // per stop
	double Cycle = 0.0;
	double InStart = 0.0;      // seconds after departure the inbound run starts
	static void Simulate(const FRunParams& P, int Dir, std::vector<double>& T, std::vector<double>& S, std::vector<double>& V,
		std::vector<double>& Open, std::vector<double>& Close);
	static void Sample(const std::vector<double>& T, const std::vector<double>& S, const std::vector<double>& V, double t,
		double& OutS, double& OutV);
};
}
