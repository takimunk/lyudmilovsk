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
#include <memory>
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
		Murmur,     // starlings: in the evening murmuration over the roost (BirdMurmur.cpp flies it)
		Num
	};
	const char* ActName(EAct A);
	inline bool IsAirborne(EAct A)
	{
		return A == EAct::Takeoff || A == EAct::Fly || A == EAct::Land || A == EAct::Hawk || A == EAct::Soar || A == EAct::Flee
			|| A == EAct::Chase || A == EAct::Mob || A == EAct::Approach || A == EAct::Murmur;
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
		float Sprint = 13.f;             // m/s, flat out (BirdBio): escapes, chases, and the strongest headwind it can beat
		float WindLimit = 12.f;          // m/s mean wind (BirdBio) above which it keeps to shelter and flies low and seldom
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
		// food (BirdEcology.cpp FoodSense): how far off it spots feed in a person's hand or thrown on the ground, sharp-eyed
		// and bold town birds (pigeons, sparrows, crows, gulls) from far, shy woodland birds only near; 0 = never comes to it
		float FoodNotice = 0.f;          // m
		float BegDistance = 3.f;         // m from the person it lands and waits at (bolder: nearer)
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
		// late stragglers (species.json "stragglers"): this many stay past the species' months and leave one by one between
		// these days of the year (the game fills it; only with Options.bMigration, bMigrant false then)
		int Stragglers = 0;
		float LeaveFrom = 0.f, LeaveUntil = 0.f;

		// the daily routine (BirdEcology.cpp, docs/bird_life.md "Daily routines")
		float Stage[NumHabitats] = {};   // pre-roost gathering places (rooks on a field, swallows on wires); none = straight to roost
		float StageMinutes = 0.f;        // gathers this long before it goes to roost
		float WakeSpread = 0.f;          // min: its groups leave the roost one after another over this time
		bool bMurmurates = false;        // starlings: the evening murmuration over the roost, then down into it in waves
		bool bTidal = false;             // feeds on the sand the sea leaves at low tide, keeps off it at high tide
		bool bMixedFlock = false;        // in winter joins the tits' roving flocks
		bool bSleepsAloft = false;       // swifts: the night on the wing, high over home
		bool bDriesWings = false;        // cormorants: after fishing stand on the beach or a post, wings spread
		float Bathe = 0.f;               // 0..1: how often it bathes and preens at the water's edge at midday
		float WinterForage[NumHabitats] = {};  // feeding tastes Nov-Feb (when bWinterForage)
		bool bWinterForage = false;
		float WinterFlock = 1.f;         // flocks this much bigger Nov-Feb
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
		uint8_t Flags = 0;          // 1 guessed tree, 2 hero plant (replaced in the game), 4 a feeder's tray (FFeeder), 8 dropped by the game
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
		float LeaveDay = 1e9f;      // a straggler leaves on this day of the year (migration)
		float Struggle = 0.f;       // s it has been flying without making way against the wind
		// food it spotted (the feed in the player's hand, a handful thrown): it comes and waits or pecks (FWorld::LureThink)
		int Lure = -2;              // -2 none, -1 the player's hand, k >= 0 the thrown pile Piles[k]
		float LureTime = 0.f;       // s since it set off for the food
		float LureCool = 0.f;       // s before it may be drawn to food again (after giving up or a fright)
		bool bSpotChecked = false;  // it sits on a perch checked against the real branch/roof near the camera (FWorld::Perch)
		bool bSpotOk = false;       // the spot it is flying to was checked (it becomes bSpotChecked when it lands there)
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
		float WakeShift = 0.f;      // min after the species' wake time it leaves the roost (WakeSpread)
		int Slot0 = 0;              // first place of its row on a wire
		// after a fright: keep away from where it came from (or from the player, who may follow) until then
		double AvoidUntil = -1.0;
		V3 Avoid;
		float AvoidR = 0.f;
		bool bAvoidPlayer = false;
		int Lured = 0;              // members at the food now (the others follow them: a flock finds food together)
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
		bool bFeed = false;              // feed in the hand (the bag carried, or held out): birds that like it come from FoodNotice
		V3 Palm;
		float HandSpeed = 0.f;           // m/s of the palm
		float ExtraThreat = 0.f;         // added by gameplay (AddPlayerThreat), decays
		bool bClaimedBird = false;       // the player is a bird now: birds treat them as one
		float OfferLike[64];             // how much each species wants the feed in the hand, 0..1 (the feed's table, BirdFeeder.cpp)
		FPlayer() { for (float& L : OfferLike) L = 1.f; }
	};

	/** Site flag of a feeder's tray. */
	constexpr uint8_t SiteFeeder = 4;

	/**
	 * A bird feeder the player put up (BirdFeeder.cpp in the game): a tray on a post that birds whose taste the feed suits
	 * come to, if it stands within their home range and in a kind of place they feed in. They perch on the tray and peck;
	 * eating there near a calm player they slowly learn to trust them.
	 */
	struct FFeeder
	{
		bool bActive = false;
		int Site = -1;                    // its tray (a site with SiteFeeder)
		uint32_t Around = 1u << (int)EHabitat::Garden;   // the kinds of place round it (bits of EHabitat): a species must feed in one of them
		float Like[64] = {};              // how much each species wants the feed in it, 0..1
		float Ready = 0.f;                // 0..1: how far the birds round about have found it (the game ramps it over a couple of hours)
		float Food = 0.f;                 // handfuls in it
		float Eaten = 0.f;                // handfuls pecked since the game last took them off
	};

	/** A handful of feed the player threw on the ground (G): birds that like it and spot it come and peck it up. */
	struct FPile
	{
		bool bActive = false;
		V3 Pos;
		float Food = 0.f;                 // handfuls left
		float Like[64] = {};              // how much each species wants it, 0..1
		double Until = 0.0;               // gone (blown about, trodden in) by then, if not eaten
	};

	/** Something standing birds fly round, not through: the player, a person, a cat (a vertical capsule). */
	struct FBody
	{
		V3 Feet;
		float Height = 1.8f, Radius = 0.35f;
	};

	/** A ground animal birds watch out for: the animal life's cats and dogs (docs/animals.md), filled by the game every frame. */
	struct FBeast
	{
		V3 Pos, Vel;
		bool bCat = false;
		bool bStalking = false;          // a cat creeping up low: hardly noticed until it is close
	};

	enum class EEvent : uint8_t { Notice, Turn, Watch, Call, Approach, Eat, Flee, Alarm, Song, Supplant, ChaseStart, Num };
	const char* EventName(EEvent E);

	struct FEvent
	{
		EEvent Kind;
		int Bird;
		V3 Pos;
	};

	class FMurmuration;
	class FCorvids;
	class FNesting;

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
		/** Murmurations keep within this many metres of the shore over the sea (never deep out). */
		float MurmurOffshore = 250.f;
		/** The crow family's minds and items (BirdCorvid.h); off = they live like the other birds (A/B, lyud.Birds.Corvids 0). */
		bool bCorvids = true;
		/** The breeding year: nests, eggs, chicks, fledglings (BirdNest.h); off = no nests (A/B, lyud.Birds.Nests 0). */
		bool bNests = true;
		/** Flying birds swerve round the player and the people near (AvoidBodies); off = straight through (A/B, lyud.Birds.Avoid 0). */
		bool bAvoid = true;
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
		/** Cats and dogs (cleared and refilled by the game each frame); empty = none. */
		std::vector<FBeast> Beasts;
		/** People near the camera (the game refills it now and then): flying birds steer round them and the player. */
		std::vector<FBody> Bodies;
		/** Thrown feed (index = pile id; an eaten one goes inactive and is reused). */
		std::vector<FPile> Piles;
		/** Throws a handful at P (m) that each species wants by Like (64 entries); returns its id. */
		int AddPile(const V3& P, float Handfuls, const float* Like);
		/** Handfuls of thrown feed the birds ate since the last call (all piles). */
		float TakePileEaten() { const float E = PileEaten; PileEaten = 0.f; return E; }
		/**
		 * The real thing under a perch near the camera (the game: a branch of the drawn tree, a roof, a rail): moves Spot onto
		 * it and returns 1, 0 if there is nothing within reach (the spot is in the air), -1 if it can't tell now (checked
		 * again later). Null = every spot is good (tools/birdsim).
		 */
		std::function<int(const FBird& B, const FSite& Site, V3& Spot)> Perch;
		/** Perches found in the air and moved or given up (Perch): for the report and tests. */
		int PerchMoved = 0, PerchRefused = 0;
		/** Bird-steps a flying bird was inside the player's body (near the camera): for the report and A/B of AvoidBodies. */
		int Intrusions = 0;
		/** How far off a bird of this species spots feed now (m; 0 = it never comes to it). */
		float FoodNoticeDist(const FBird& B, bool bThrown) const;
		FSightFn Sight;
		/** Exact ground height at (X, Y) from the wrapper (a line trace); false = use the grid. */
		std::function<bool(float X, float Y, float& Z)> Probe;
		V3 Camera;                      // for the LOD tiers
		double Time = 0.0;              // s of simulation
		float Hour = 12.f;              // local clock
		float Sunrise = 7.f, Sunset = 17.5f;
		float DayOfYear = 297.f;
		int Year = 2026;                // the calendar year (the nests' plans differ year by year)
		/** The sea's surface now (m): ALyudSea::SeaLevel, mean -43 plus the tide; swimmers float on it, waders follow it. */
		float SeaZ = -43.f;
		float MeanSeaZ = -43.f;
		/** The wind (ULyudWindSubsystem's mean at 10 m, m/s, horizontal) and its gustiness (gust spread / mean),
		 *  filled by the game; flying birds drift with it, and in a gale the small ones keep to shelter. */
		V3 Wind;
		float Gustiness = 0.25f;
		/** How hard the wind is for a species now: 1 = its WindLimit. */
		float WindStress(const FSpecies& S) const;
		/** How much more often a soarer goes up to circle in this wind (0 = never: the albatross in a calm). */
		float SoarWind(const FSpecies& S) const;
		/** The nearest place this bird can sit out a wind it can't fly against (rather downwind, in the lee). */
		int ShelterNear(const FBird& B) const;
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
		/** Stragglers leave on their days (call when DayOfYear changes; Populate does it too). */
		void UpdateMigration();
		/** Metres from the nearest land over the sea (0 on land), and from the nearest sea on land (0 at sea), from the ground grid. */
		float Offshore(float X, float Y) const;
		float Inland(float X, float Y) const;
		/** The starlings' evening murmuration (null when no species murmurates). */
		FMurmuration* Murmuration() const { return Murm.get(); }
		/** The crow family's minds and the things they carry (BirdCorvid.h). */
		FCorvids* Corvids() const { return Corv.get(); }
		/** The breeding year: nests and their birds (BirdNest.h). */
		FNesting* Nesting() const { return Nest.get(); }
		/** Turns the corvids' minds on or off while running (off: their items go, they live like the other birds; A/B). */
		void SetCorvids(bool bOn);
		/** One calm bird on the ground at P that keeps to that spot (tests, tutorials); returns its id. */
		int AddBird(int Species, const V3& P);
		/** Count calm birds feeding within Radius of P, one flock that keeps to that place (returns the first id). */
		int AddFlock(int Species, const V3& P, int Count, float Radius);
		/** Ground height where there is no grid (a test level): flights keep above it. */
		float NoGridZ = -43.f;
		/** The clock jumped (developer mode, a loaded save): every group goes straight to where it would be now. */
		void Resettle();
		/** Sends a bird flying to Spot on Site (-1: a spot of its own); a perch near the camera is checked first (Perch). */
		void FlyTo(FBird& B, int Site, const V3& Spot);
		/** Advances the world by Dt seconds (call every frame with the frame's dt). */
		void Step(float Dt);
		/** Position a bird is drawn at now (between its coarse steps). */
		V3 RenderPos(const FBird& B) const;

		// the player's hand (befriending): the bird coming to it, if any
		int Approacher = -1;
		/** A befriended bird the player called (from the diary): it flies to SummonPalm from wherever it is, without food. */
		int Summoned = -1;
		V3 SummonPalm;
		/** Calls bird Id to the palm; a bird further than FarM is first put Entry (high, out of sight) and flies in from there.
		 *  False if it can't come (gone, claimed, unknown). */
		bool Summon(int Id, const V3& Palm, const V3& Entry, float FarM);
		/** The bird goes back to its flock (the call is over or given up). */
		void CancelSummon();
		/** Takes a bird out of the simulation (the player becomes it); its position is then set by SetClaimedPos. */
		bool Claim(int Id);
		void SetClaimedPos(int Id, const V3& Pos, const V3& Vel);
		void Release(int Id);
		/** Birds within R of P (ids). */
		void Near(const V3& P, float R, std::vector<int>& Out) const;
		/** A sudden scare at P (a gunshot, a dropped tray): radius R. */
		void Scare(const V3& P, float R, float Strength);

		/** The player's feeders (index = feeder id; a removed one stays, inactive, and is reused). */
		std::vector<FFeeder> Feeders;
		/** Puts up a feeder with its tray at Tray (radius m) among the kinds of place Around (KindsAround); returns its id. Fill its Like, Food, Ready. */
		int AddFeeder(const V3& Tray, float Radius, uint32_t Around);
		/** Takes a feeder down: the birds on it leave. */
		void RemoveFeeder(int F);
		/** The feeder whose tray this site is (-1 none). */
		int FeederAt(int Site) const;
		/** The kinds of place within Radius of P birds feed in (bits of EHabitat; Garden alone when none is near). */
		uint32_t KindsAround(const V3& P, float Radius = 30.f) const;

		std::string Report() const;

	private:
		struct FGrid { int NX = 0, NY = 0; float X0 = 0, Y0 = 0, Cell = 8; std::vector<int16_t> Ground, Top; } Grid;
		// spatial hash of birds, rebuilt each step
		std::vector<int> CellHead, CellNext;
		float HashCell = 16.f;
		int HashNX = 0, HashNY = 0;
		float HashX0 = 0.f, HashY0 = 0.f;
		std::vector<uint16_t> OffshoreD, InlandD;   // dm from land / from the sea, per grid cell
		std::shared_ptr<FMurmuration> Murm;
		friend class FMurmuration;
		std::shared_ptr<FCorvids> Corv;
		friend class FCorvids;
		std::shared_ptr<FNesting> Nest;
		friend class FNesting;
		std::vector<int> NearScratch;
		float StepAcc[4] = {};
		double LastThink[4] = {};
		struct FPredator { int Bird = -1; int Prey = -1; float Timer = 0.f; float Mobbed = 0.f; };
		std::vector<FPredator> Predators;
		float PileEaten = 0.f;
		int LuredAtHand = 0;
		int PerchBudget = 0;        // Perch checks left this step (they trace and read tree skeletons in the game)

		void BuildHash();
		void ForNear(const V3& P, float R, const std::function<void(int)>& Fn) const;
		void UpdateTiers();
		void Think(FBird& B, float Dt);
		void Move(FBird& B, float Dt);
		void PerceivePlayer(FBird& B, float Dt);
		void PerceiveBirds(FBird& B, float Dt);
		void PerceiveBeasts(FBird& B, float Dt);
		void GroupThink(FGroup& G, float Dt);
		void SleepAloft(FBird& B, const FGroup& G);
		int MixedFlockSite(const FGroup& G) const;
		void JoinMurmuration(FGroup& G);
		void PredatorThink(FPredator& P, float Dt);
		/** What the group should do now: Plan 0 rest, 1 feed, 2 roost, 3 gather before the roost, 4 murmurate, 5 bathe. */
		void ScheduleOf(const FSpecies& S, const FGroup* G, float& Forage, int& Plan) const;
		float Taste(const FSpecies& S, int Plan, EHabitat Kind) const;
		bool IsWinter() const { return DayOfYear > 305.f || DayOfYear < 60.f; }
		void BuildOffshore();
		V3 WireSpot(const FSite& Site, const FBird& B) const;
		/** A spot on a site for this bird: a free place in its group's row on a wire, anywhere on other sites. */
		V3 SpotFor(FBird& B, int Site);
		/** The site a roosting bird of this group goes to (a murmuration's roost is a whole reedbed). */
		int RoostSpotSite(const FGroup& G, FBird& B);
		bool InRoost(const FGroup& G, int Site) const;
		int ChooseSite(const FGroup& G, int Plan, int Except) const;
		float SiteScore(const FGroup& G, const FSite& Site, int Plan) const;
		V3 SpotOn(const FSite& Site, uint32_t& Rng) const;
		void SendGroup(FGroup& G, int Site);
		bool Avoided(const FGroup& G, const V3& P) const;
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
		/** Spots food it likes (the player's feed, a thrown pile) and sets off for it; LureThink runs it there. */
		void UpdateLure(FBird& B, float Dt);
		bool LureThink(FBird& B, float Dt);
		void EndLure(FBird& B, float Cool);
		/** Where to wait for the hand (a ring round the player at its beg distance, on the ground) or peck at a pile. */
		bool LureSpot(FBird& B, V3& Spot);
		/** How far from the player a bird waits for the hand now (m). */
		float WaitDist(const FBird& B) const;
		const float* LureLike(int Lure) const { return Lure == -1 ? Player.OfferLike : Piles[Lure].Like; }
		V3 LurePos(int Lure) const { return Lure == -1 ? Player.Feet : Piles[Lure].Pos; }
		/** Flying round the player and the people near (cheap: a few capsules, only for birds near the camera). */
		void AvoidBodies(const FBird& B, const FSpecies& S, V3& Want) const;
		/** A perch spot near the camera checked against the real world (Perch); false = take another place. */
		bool CheckPerch(FBird& B, int Site, V3& Spot);
		/** Where the hand is for this bird: the summon's palm for the called bird, the offer's for the others. */
		const V3& PalmFor(const FBird& B) const { return B.Id == Summoned ? SummonPalm : Player.Palm; }
		/** Is there something in the hand for this bird (food held out, or it was called)? */
		bool HandFor(const FBird& B) const { return Player.bOffer || B.Id == Summoned; }
		float FeederScore(const FGroup& G, const FFeeder& F) const;
		/** A bird on a feeder's tray: peck while there is food (and trust a calm player near by), else rest. */
		void AtFeeder(FBird& B, FFeeder& F, float Dt);
		int GroundSpot(FBird& B);
	};
}
