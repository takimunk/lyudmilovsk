// The cable-car metro's working, in plain C++ (no Unreal types) so it runs and is tested outside the engine too
// (tools/transportsim). ULyudTransportSubsystem wraps it for the game (docs/transport.md).
//
// The network comes from Content/Transport/network_built.json (tools/transport.py): stations, lines, and for each
// line one closed loop of rope the cabins go round (out on one track, round the end station's wheel, back on the
// other). A cabin's place is the distance S along its loop. Cabins only run when somebody needs them: a trip
// (Travel) puts a rider at a stop, the dispatcher sends the cabin that gets there soonest (one parked nearby, one
// already going that way with room, or one out of the depot), the cabin stops where its riders get on or off and,
// with nobody left to carry, waits a while at its last stop and goes back to the depot. Empty cabins run faster.
// A trip over two lines changes at the shared station.
//
// Frame: the map's metres (tools/world_map.py: X west, Y north, Z up). Times in seconds.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudTransport
{
	struct V3
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
		V3() = default;
		V3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& O) const { return V3(X + O.X, Y + O.Y, Z + O.Z); }
		V3 operator-(const V3& O) const { return V3(X - O.X, Y - O.Y, Z - O.Z); }
		V3 operator*(float K) const { return V3(X * K, Y * K, Z * K); }
	};

	struct FLevel
	{
		int Line = -1;
		float Z = 0.f;              // the deck of this line's platform
	};

	/** A sheave where a station's ropes bend (its vestibule's ends, the convergence pylons): map metres. */
	struct FSheave
	{
		float X = 0.f, Y = 0.f, Z = 0.f, Yaw = 0.f, Turn = 0.f;
		int Side = 0;
		bool bConverge = false;
	};

	/** A station's rope on one line (network_built "geo"): the tracks G apart along its vestibule's edges. */
	struct FStationGeo
	{
		bool bValid = false;
		float CX = 0.f, CY = 0.f, Yaw = 0.f;            // the line's middle at the station (map) and its axis
		float Gap = 0.f, Edge = 0.f, Length = 0.f, StopX = 0.f, Rope = 0.f;
		bool bWheel = false;
		V3 Wheel;
		std::vector<FSheave> Sheaves;
	};

	struct FStation
	{
		std::string Id, Name, Style;
		float X = 0.f, Y = 0.f;
		float Ground = 0.f;         // ground at the station's centre
		float Deck = 0.f;           // the lowest deck
		float Lift = 0.f;           // from the ground to the top deck
		bool bOnGround = false;     // stands right on the ground (Aton): no tower, no lift
		std::vector<FLevel> Levels;
		std::vector<float> AxisYaw; // per line (index = line), degrees in the map frame; 0 if not on it
		std::vector<int> Ends;      // per line: +1 its first stop, -1 its last, 0 a through station (or not on it)
		float CoreX = 0.f, CoreY = 0.f, CoreGround = 0.f;   // the stair and lift (map), the ground under them
		// the vestibule's connector (map; docs/transport.md): where its floor meets the gallery, +X of its frame toward the tower
		bool bHasVest = false;
		float VestX = 0.f, VestY = 0.f, VestZ = 0.f, VestYaw = 0.f, VestGroundMin = 0.f;
		int SchemaX = 0, SchemaY = 0; bool bHasSchema = false;
		std::vector<FStationGeo> Geo;   // per line (index = line)
	};

	struct FStop
	{
		int Station = -1;
		int Dir = 0;                // 0 the way out (first stop to last), 1 back
		float S = 0.f;              // along the loop
		bool bEnd = false;          // round an end station's wheel
	};

	struct FPylon
	{
		float X = 0.f, Y = 0.f, Ground = 0.f, Top = 0.f, Height = 0.f, Yaw = 0.f, Turn = 0.f;
		bool bCorner = false;
	};

	struct FLine
	{
		std::string Id, Name, Color;
		int Number = 0;
		std::vector<int> Stations;  // stops in order (station indices)
		int Depot = -1;             // station index
		int DepotStop = -1;         // index into Stops
		std::vector<V3> Loop;       // rope points
		std::vector<float> LoopS;   // their S
		float Length = 0.f;
		std::vector<FStop> Stops;   // by S
		std::vector<FPylon> Pylons;
		/** Free spans of rope between supports, [S0, S1] along the loop (they sag under cabins and sway). */
		std::vector<std::pair<float, float>> Spans;

		/** Point on the rope at S (wrapped) and the way it goes there (unit, map frame). */
		V3 At(float S, V3* OutDir = nullptr) const;
		/** S - From forward round the loop, 0..Length. */
		float Ahead(float From, float To) const;
		int StopOf(int Station, int Dir) const;      // -1 if none
	};

	enum class ECabinState : uint8_t { Depot, Moving, Dwelling, Idle };
	const char* CabinStateName(ECabinState S);

	struct FCabin
	{
		std::string Id, Name;
		int Number = 0;
		int Line = -1;
		ECabinState State = ECabinState::Depot;
		float S = 0.f;
		float Speed = 0.f;
		float Door = 0.f;           // 0 shut .. 1 open
		int AtStop = -1;            // the stop it stands at (Dwelling, Idle)
		std::vector<int> Targets;   // stops it has to stop at
		std::vector<int> Riders;    // on board (rider indices)
		int Reserved = 0;           // riders it is on its way to pick up
		double DwellUntil = 0.0;
		double IdleSince = 0.0;
		bool bToDepot = false;
		bool bHold = false;         // somebody in the doorway (the game sets it): the doors stay open
		bool bShutReq = false;      // the player shut the doors by hand (the game sets it): an idle cabin stays shut, a
		                            // dwelling one may go once its dwell is over; cleared when it moves
		bool bPlayerAboard = false; // the game sets it from the cabin's inside volume
		// the swing of the cabin under its hanger (radians): pitch along the rope, roll across it
		float Pitch = 0.f, PitchV = 0.f, Roll = 0.f, RollV = 0.f;
		float LastSpeed = 0.f;
		float Acc = 0.f;            // its acceleration along the rope, changed no faster than FParams::Jerk (no jolts)
		float Yaw = 0.f, YawRate = 0.f;  // heading on the rope and how fast it turns (the swing outward in a bend)
		bool bYawSet = false;
		float Odometer = 0.f;
		float EmptyMetres = 0.f;    // run without anybody aboard (tests: no running for nothing)
		int DepotSlot = 0;
	};

	/** A free span of rope moving: its first two modes up and down (loaded by the cabins on it), the first sideways
	 *  (the wind), each a damped spring toward the shape the loads ask for. */
	struct FSpanMotion
	{
		int Line = -1;
		float S0 = 0.f, S1 = 0.f;
		float Qv[2] = { 0.f, 0.f }, Vv[2] = { 0.f, 0.f };
		float Qh = 0.f, Vh = 0.f;
	};

	enum class ERiderState : uint8_t { Waiting, Riding, Done, Cancelled };

	struct FLeg
	{
		int Line = -1;
		int From = -1, To = -1;     // stations
		int BoardStop = -1, AlightStop = -1;
	};

	struct FRider
	{
		int Who = -1;               // a person's id (UPeopleLifeSubsystem), or PlayerId
		ERiderState State = ERiderState::Waiting;
		std::vector<FLeg> Legs;
		int LegI = 0;
		int Cabin = -1;             // riding in, or assigned to pick it up
		int Seat = -1;
		double Since = 0.0;         // when it started waiting
		double Asked = 0.0;
		int Station() const;        // where it waits / is
	};

	enum class EEvent : uint8_t { Launch, Arrive, Depart, Board, Alight, Done, Park, Num };
	const char* EventName(EEvent E);
	struct FEvent { EEvent Kind; int Cabin; int Stop; int Rider; int Station; };

	constexpr int PlayerId = -100;

	struct FParams
	{
		float Speed = 6.f, SpeedEmpty = 11.f, SpeedStation = 1.2f, SpeedThrough = 3.f;
		float Accel = 0.9f;
		float Jerk = 0.9f;          // m/s^3: how fast the acceleration itself may change
		float Lateral = 0.6f;       // m/s^2: the most sideways pull in a bend
		float Dwell = 14.f, IdleReturn = 90.f, Headway = 28.f, PlatformLen = 18.f, DoorTime = 1.6f;
		int Capacity = 4;
	};

	class FNetwork
	{
	public:
		FParams P;
		std::vector<FStation> Stations;
		std::vector<FLine> Lines;
		std::vector<FCabin> Cabins;
		std::vector<FRider> Riders;
		std::vector<FEvent> Events;     // since the caller last cleared them
		double Time = 0.0;
		float PortalFaceY = 0.f;        // the map's frame in Unreal (cm): Y = PortalFaceY + map Y * 100
		bool bHasFrame = false;
		V3 ClubBoard; float ClubBoardYaw = 0.f; bool bHasClubBoard = false;
		std::string LastError;

		bool Load(const std::string& JsonText);
		/** Puts the cabins where a new day starts: one parked at each line's first stop, the rest in the depot. */
		void Reset();
		void Step(float Dt);

		int FindStation(const std::string& Id) const;
		int FindLine(const std::string& Id) const;
		int FindCabin(const std::string& Id) const;

		/** A trip from station A to station B for Who; returns the rider index (-1 if there is no way). Another trip of
		 *  the same Who (still waiting) is cancelled first. */
		int Travel(int Who, int From, int To);
		void Cancel(int Rider);
		int FindRider(int Who) const;       // the active trip of Who, -1 if none
		/** The player got into a cabin standing at a stop without a trip: rides to Station (or the line's next stop). */
		int PlayerBoard(int Cabin, int ToStation = -1);
		/** The player got out (at a stop): their trip ends where the cabin stands. */
		void PlayerAlight(int Cabin);

		/** Seconds till the next cabin that will stop at station St for line L heading Dir (-1 either way) gets there;
		 *  if none is coming, how long one would take if called now. bCalled says which. */
		float Eta(int St, int L, int Dir, bool& bCalled) const;
		/** The station the cabin heads for next (-1 none) and the trip's last station of its riders (-1). */
		int NextStation(int Cabin) const;
		int Destination(int Cabin) const;
		/** World point of a cabin's grip on the rope (the rope's sag under it and its sway included) and the way it faces. */
		V3 CabinAt(int Cabin, V3* OutDir = nullptr) const;

		// --- the ropes' and cabins' motion (looks only: the network's timing doesn't depend on it) ---
		std::vector<FSpanMotion> SpanMotion;
		/** Moves the spans and swings the cabins; Wind: the air's velocity over the line, m/s in the map frame. */
		void StepMotion(float Dt, const V3& Wind);
		/** How far the moving rope is off its resting shape at S of line L, metres (map frame). */
		V3 RopeOffset(int Line, float S) const;
		std::string Report() const;

	private:
		void Dispatch();
		/** Nobody else of the line moving within the headway of S (cabins in the depot and on sidings don't count). */
		bool Clear(int Cabin, float S) const;
		void Assign(FRider& R, int RiderI);
		float CostTo(const FCabin& C, int Line, int Stop) const;
		bool Plan(int From, int To, std::vector<FLeg>& Out) const;
		void StartLeg(FRider& R);
		void ArriveAt(int CabinI, int Stop);
		void Leave(FCabin& C, int CabinI);
		int NextTarget(const FCabin& C) const;
		void Emit(EEvent E, int Cabin, int Stop, int Rider) { Events.push_back({ E, Cabin, Stop, Rider, Stop >= 0 && Cabin >= 0 ? Lines[Cabins[Cabin].Line].Stops[Stop].Station : -1 }); }
	};
}
