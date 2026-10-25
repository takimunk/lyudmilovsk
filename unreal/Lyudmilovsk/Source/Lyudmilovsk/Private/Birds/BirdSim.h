// The wild birds' life on the outdoor map, in plain C++ (no Unreal types) so it can also be run and
// looked at outside the engine (tools/birdsim). UBirdLifeSubsystem wraps it for the game.
//
// Every bird is a small struct: where it is, what it is doing, how alert it is and how much it trusts
// the player. Birds live in groups (a flock, a pair, a single robin) that choose where to go from the
// habitat sites of the map by their species' taste, the time of day, how crowded a site is, who else
// is there and where they were scared lately. Each bird perceives the player (sight with a field of
// view, hearing), its flock mates' alarm, other species near it (who is bigger), predators, and
// food held out in a hand.
//
// Frame: Unreal world metres (UE cm / 100), Z up.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LyudBirds
{
	struct V3
	{
		float X = 0.f, Y = 0.f, Z = 0.f;
		V3() = default;
		V3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
		V3 operator+(const V3& B) const { return { X + B.X, Y + B.Y, Z + B.Z }; }
		V3 operator-(const V3& B) const { return { X - B.X, Y - B.Y, Z - B.Z }; }
		V3 operator*(float S) const { return { X * S, Y * S, Z * S }; }
		V3& operator+=(const V3& B) { X += B.X; Y += B.Y; Z += B.Z; return *this; }
		V3& operator-=(const V3& B) { X -= B.X; Y -= B.Y; Z -= B.Z; return *this; }
		V3& operator*=(float S) { X *= S; Y *= S; Z *= S; return *this; }
		float Dot(const V3& B) const { return X * B.X + Y * B.Y + Z * B.Z; }
		float Len() const;
		float Len2D() const;
		V3 Norm() const;
		V3 Flat() const { return { X, Y, 0.f }; }
	};

	/** Habitat kinds; tools/bird_habitats.py KINDS has the same order. */
	enum class EHabitat : uint8_t { Sea, Beach, Cliff, Meadow, Reed, Street, Roof, Tower, Wire, Garden, Broadleaf, Conifer, ForestFloor, Num };
	constexpr int NumHabitats = (int)EHabitat::Num;
	const char* HabitatName(EHabitat H);
	/** Birds stand on the ground or water here (walk, hop, peck, swim) rather than sit on a perch. */
	bool IsGroundHabitat(EHabitat H);

	enum class EFlight : uint8_t { Flap, FlapGlide, Bounding, Aerial, Soar };

	/** What a bird is doing. The animation state for the species' actor is derived from it (BirdLife.cpp). */
	enum class EAct : uint8_t
	{
		Sleep,      // on the roost at night
		Perch,      // sitting on a perch, looking round
		Preen,      // sitting, cleaning its feathers
		Forage,     // on the ground or water: walking or hopping between spots
		Peck,       // on the ground or water: taking food
		Swim,       // sitting on the water
		Watch,      // frozen, head on the danger (the player, a predator)
		Takeoff,
		Fly,        // flying somewhere
		Land,       // the last metres to a spot
		Hawk,       // swifts, swallows, martins: hunting insects on the wing
		Soar,       // gulls: circling on the wind
		Flee,       // escaping
		Chase,      // a gull or crow after another bird's food, or a hawk after prey
		Mob,        // small birds round a perched predator, calling
		Approach,   // coming to the food in the player's hand
		Eat,        // eating from the player's hand
		Claimed,    // the player is this bird (bird flight); not simulated
		Num
	};
	const char* ActName(EAct A);
	inline bool IsAirborne(EAct A)
	{
		return A == EAct::Takeoff || A == EAct::Fly || A == EAct::Land || A == EAct::Hawk || A == EAct::Soar || A == EAct::Flee
			|| A == EAct::Chase || A == EAct::Mob || A == EAct::Approach;
	}

	/** One species' ecology and temper. Size and flight style match the species thread's table (species.json). */
	struct FSpecies
	{
		std::string Id;
		float Length = 0.2f;             // m, beak to tail
		float Mass = 0.05f;              // kg
		int Population = 20;             // birds on the map
		int FlockMin = 1, FlockMax = 1;
		float HomeRange = 300.f;         // m: how far a group goes from its home to feed
		float Forage[NumHabitats] = {};  // taste for feeding grounds, 0..1
		float Perch[NumHabitats] = {};   // ... for resting places
		float Roost[NumHabitats] = {};   // ... for the night
		bool bCommunalRoost = false;     // every group of the species sleeps at the same place
		EFlight Flight = EFlight::Flap;
		float Cruise = 10.f;             // m/s
		float Wingbeat = 0.f;            // Hz, 0 = from the mass (the species table sets it)
		float FlightClearance = 12.f;    // m over the tallest thing under it
		float HawkAlt = 15.f;            // aerial feeders: height over the ground they hunt at
		bool bHops = true;               // hops (sparrow) or walks (pigeon, gull)
		bool bClings = false;            // swifts: never perch, cling to walls, eaves and the cliff face
		float Gait = 0.6f;               // m/s on the ground
		// the player
		float FleeDistance = 10.f;       // m: flushes when a person walks at it closer than this
		float SightRange = 60.f;         // m
		float Fov = 300.f;               // deg
		float Tameness = 0.3f;           // 0..1: how fast it learns to trust
		float Curiosity = 0.3f;          // 0..1: comes closer to look
		// other birds
		int Dominance = 30;              // the bigger number wins a spot
		bool bKlepto = false;            // steals food (gulls, crows)
		bool bMobs = false;              // mobs predators
		bool bPredator = false;          // hunts small birds (sparrowhawk, peregrine)
		float PreyValue = 0.f;           // 0..1 how much hawks want it
		// the day (minutes from sunrise / from sunset)
		float WakeMinutes = 0.f;
		float RoostMinutes = 0.f;
		float MiddayRest = 0.4f;         // 0..1 how much of the midday it rests
		float CallRate = 1.f;            // contact calls a minute when calm
		float DawnSong = 0.f;            // extra song at dawn, calls a minute
		uint32_t Color = 0x404040;       // placeholder colour, sRGB
		bool bMigrant = false;           // away now (the game sets it from species.json months); only with Options.bMigration
	};

	const std::vector<FSpecies>& SpeciesTable();
	/** The same table, for the game to fill in sizes and months from Content/BirdsData/species.json before Populate. */
	std::vector<FSpecies>& MutableSpeciesTable();
	int FindSpecies(const std::string& Id);

	struct FSite
	{
		EHabitat Kind = EHabitat::Meadow;
		V3 A, B;                    // a point (A == B) or a segment (ridge, rope, ledge)
		float Radius = 0.f;         // patch round it to walk in (ground and water)
		int Capacity = 8;
		uint8_t Flags = 0;          // 1 guessed tree, 2 hero plant (replaced in the game)
		// live
		int Occupants = 0;
		float Disturbed = 0.f;      // game seconds until the birds trust this spot again
		int TopDominance = 0;       // the strongest species there now
		V3 Center() const { return (A + B) * 0.5f; }
	};

	struct FBird
	{
		int Id = -1;
		int Species = 0;
		int Group = -1;
		EAct Act = EAct::Perch;
		float ActTime = 0.f;        // s in this act
		float ActLen = 1.f;         // planned length of this act
		V3 Pos, Vel;
		float Yaw = 0.f;            // body heading, radians (0 = +X)
		V3 Look;                    // where the head looks (world point); valid when bLook
		bool bLook = false;
		int Site = -1;              // where it is or is going
		V3 Spot;                    // the exact spot on the site
		V3 FlyFrom;
		float Alert = 0.f;          // 0..1, 1 = flushes
		float Trust = 0.f;          // 0..1, toward the player
		float Hunger = 0.5f;        // 0..1
		float Boldness = 1.f;       // 0.6..1.4 personal
		float Phase = 0.f;          // wing-beat / gait phase
		float CallTimer = 5.f;
		float ThinkTimer = 0.f;
		float Seen = 0.f;           // how long it has watched the player
		float Unseen = 0.f;         // s since it last perceived the player
		float Habituation = 0.f;    // 0..1 used to the player standing still near it
		float Wary = 0.f;           // s it stays watchful after a fright (alert fades slowly)
		float Delay = 0.f;          // reaction delay before a flush
		int Target = -1;            // a bird it chases / mobs / flees from (-1 = the player)
		uint8_t Tier = 3;           // LOD: 0 near .. 3 far
		bool bNoticed = false, bTurned = false, bWatching = false, bAte = false;
		bool bGone = false;         // away (migrated); not drawn
		bool bFemale = false;       // half of each species (its own seed), for the female plumage and size
		uint32_t Rng = 1;
	};

	struct FGroup
	{
		int Species = 0;
		std::vector<int> Members;
		V3 Home;                    // centre of its home range
		int Roost = -1;             // its roost site
		int Site = -1;              // where the group is
		int Plan = 0;               // 0 rest, 1 forage, 2 roost
		float StayTimer = 0.f;      // s until it thinks of moving on
		float FlushedAt = -1e9f;
		bool bMoving = false;
		bool bStayPut = false;      // a test bird: keeps to its spot (AddBird)
		// after a fright: keep away from where it came from (or from the player, who may follow) until then
		double AvoidUntil = -1.0;
		V3 Avoid;
		float AvoidR = 0.f;
		bool bAvoidPlayer = false;
	};

	/** The player, as birds perceive them (filled by the wrapper ~every frame). */
	struct FPlayer
	{
		bool bPresent = false;
		V3 Eye, Feet, Vel;
		V3 Facing { 1.f, 0.f, 0.f };     // camera forward
		float TurnRate = 0.f;            // camera angular speed, deg/s
		bool bCrouched = false;
		bool bOffer = false;             // food held out
		V3 Palm;
		float HandSpeed = 0.f;           // m/s of the palm
		float ExtraThreat = 0.f;         // added by gameplay (AddPlayerThreat), decays
		bool bClaimedBird = false;       // the player is a bird now: birds treat them as one
	};

	enum class EEvent : uint8_t { Notice, Turn, Watch, Call, Approach, Eat, Flee, Alarm, Song, Supplant, ChaseStart, Num };
	const char* EventName(EEvent E);

	struct FEvent
	{
		EEvent Kind;
		int Bird;
		V3 Pos;
	};

	struct FOptions
	{
		uint32_t Seed = 20261024;
		float PopulationScale = 1.f;
		bool bMigration = false;
		bool bPredators = true;
		/** Where the player spends most time (the club's door): this share of the small birds' groups live within FocusRadius of it. */
		V3 Focus;
		float FocusShare = 0.f, FocusRadius = 350.f;
		/** Distances (m) of the LOD tiers to the camera: near (full perception every frame), mid, far. */
		float NearDist = 40.f, MidDist = 160.f, FarDist = 500.f;
		/** Think intervals per tier, s. */
		float ThinkNear = 0.f, ThinkMid = 0.12f, ThinkFar = 0.5f, ThinkDormant = 2.f;
		/** Movement steps per tier (render interpolates), s. */
		float StepFar = 0.1f, StepDormant = 0.5f;
	};

	/** Line-of-sight test from the wrapper (true = clear); null = always clear. */
	using FSightFn = std::function<bool(const V3& From, const V3& To)>;

	class FWorld
	{
	public:
		FOptions Opt;
		std::vector<FSite> Sites;
		std::vector<FBird> Birds;
		std::vector<FGroup> Groups;
		std::vector<FEvent> Events;     // this step's events (cleared by the caller)
		FPlayer Player;
		FSightFn Sight;
		/** Exact ground height at (X, Y) from the wrapper (a line trace); false = use the grid. */
		std::function<bool(float X, float Y, float& Z)> Probe;
		V3 Camera;                      // for the LOD tiers
		double Time = 0.0;              // s of simulation
		float Hour = 12.f;              // local clock
		float Sunrise = 7.f, Sunset = 17.5f;
		float DayOfYear = 297.f;
		int SpeciesCount[64] = {};
		int TierCount[4] = {};
		std::string LastError;

		/** Reads bird_habitats.json and bird_world.bin from Dir. */
		bool LoadHabitats(const std::string& Dir);
		/** Sites with this flag are dropped (the game replaces guessed trees with real ones before Populate). */
		void RemoveSites(uint8_t Flag);
		void AddSite(const FSite& S) { Sites.push_back(S); }
		/** Ground and obstacle-top heights (m) on the grid; sea level outside it. */
		float GroundZ(float X, float Y) const;
		float TopZ(float X, float Y) const;
		/** Creates the birds and their groups (after the sites are final), each where it would be at Hour. */
		void Populate();
		/** One calm bird on the ground at P that keeps to that spot (tests, tutorials); returns its id. */
		int AddBird(int Species, const V3& P);
		/** Ground height where there is no grid (a test level): flights keep above it. */
		float NoGridZ = -43.f;
		/** The clock jumped (developer mode, a loaded save): every group goes straight to where it would be now. */
		void Resettle();
		/** Advances the world by Dt seconds (call every frame with the frame's dt). */
		void Step(float Dt);
		/** Position a bird is drawn at now (between its coarse steps). */
		V3 RenderPos(const FBird& B) const;

		// the player's hand (befriending): the bird coming to it, if any
		int Approacher = -1;
		/** Takes a bird out of the simulation (the player becomes it); its position is then set by SetClaimedPos. */
		bool Claim(int Id);
		void SetClaimedPos(int Id, const V3& Pos, const V3& Vel);
		void Release(int Id);
		/** Birds within R of P (ids). */
		void Near(const V3& P, float R, std::vector<int>& Out) const;
		/** A sudden scare at P (a gunshot, a dropped tray): radius R. */
		void Scare(const V3& P, float R, float Strength);

		std::string Report() const;

	private:
		struct FGrid { int NX = 0, NY = 0; float X0 = 0, Y0 = 0, Cell = 8; std::vector<int16_t> Ground, Top; } Grid;
		// spatial hash of birds, rebuilt each step
		std::vector<int> CellHead, CellNext;
		float HashCell = 16.f;
		int HashNX = 0, HashNY = 0;
		float HashX0 = 0.f, HashY0 = 0.f;
		std::vector<int> NearScratch;
		float StepAcc[4] = {};
		double LastThink[4] = {};
		struct FPredator { int Bird = -1; int Prey = -1; float Timer = 0.f; float Mobbed = 0.f; };
		std::vector<FPredator> Predators;

		void BuildHash();
		void ForNear(const V3& P, float R, const std::function<void(int)>& Fn) const;
		void UpdateTiers();
		void Think(FBird& B, float Dt);
		void Move(FBird& B, float Dt);
		void PerceivePlayer(FBird& B, float Dt);
		void PerceiveBirds(FBird& B, float Dt);
		void GroupThink(FGroup& G, float Dt);
		void PredatorThink(FPredator& P, float Dt);
		void ScheduleOf(const FSpecies& S, float& Forage, int& Plan) const;
		int ChooseSite(const FGroup& G, int Plan, int Except) const;
		float SiteScore(const FGroup& G, const FSite& Site, int Plan) const;
		V3 SpotOn(const FSite& Site, uint32_t& Rng) const;
		void SendGroup(FGroup& G, int Site);
		bool Avoided(const FGroup& G, const V3& P) const;
		void FlyTo(FBird& B, int Site, const V3& Spot);
		void SetAct(FBird& B, EAct A, float Len);
		void Flush(FBird& B, const V3& From, int Cause);
		void Settle(FBird& B);
		void Cling(FBird& B, const FSite& Site);
		void Emit(EEvent E, const FBird& B) { Events.push_back({ E, B.Id, B.Pos }); }
		void Alarm(const FBird& From, float Radius, float Strength);
		float Rand(uint32_t& S) const;
		float Danger(const FBird& B, float Dist, bool bSeen) const;
		void PlaceAtStart(FBird& B, FGroup& G);
		void UpdateApproach(FBird& B, float Dt);
		int GroundSpot(FBird& B);
	};
}
