// The town's cats and dogs, in plain C++ (no Unreal types) so it can also be run and looked at outside the
// engine (tools/animalsim). UAnimalLifeSubsystem wraps it for the game (docs/animals.md).
//
// Every animal is a small struct with needs that grow and are met (hunger, sleep, play, company, novelty),
// a temper of its own (bold, friendly to people, sociable, a hunter, playful, lively), a home and a range,
// and a short memory of who it knows. It decides what to do from its needs, the hour and what it perceives:
// the player, the other animals (dog-dog, cat-cat, dog-cat) and the birds on the ground. Cats live by the
// houses (doorsteps, cellar windows, the lady who feeds them), mostly round the Academgorodok and in the town,
// keep to themselves and hunt; dogs live in packs with a den and roam the streets, the bins, the shop's back
// door and the beach.
//
// Frame: Unreal world metres (UE cm / 100), Z up, yaw 0 = +X.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LyudAnimals
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

	enum class EKind : uint8_t { Cat, Dog };

	/** Ground kinds of the walk grid (tools/animal_map.py; the low 3 bits of a cell, bit 0x08 = cover). */
	enum class EGround : uint8_t { Blocked, Grass, Paved, Yard, Sand, Forest, Num };

	/** Places; tools/animal_map.py PLACES has the same order. */
	enum class EPlace : uint8_t
	{
		Doorstep, ShopDoor, Bins, WarmHatch, Bench, SunnyWall, Den, Square, Beach, ClubDoor, Station, Feeder, Basement, Lookout,
		Refuge,     // a tree (or a wall) a cat goes up when a dog is after it: added by the game from the real trees
		Num
	};
	const char* PlaceName(EPlace P);

	/** What an animal is doing; its body's animation follows (AnimalLife.cpp). */
	enum class EAct : uint8_t
	{
		Sleep,      // asleep: a cat curled up, a dog on its side or curled
		Rest,       // lying awake (a cat's loaf, a dog with its head up)
		Sit,
		Groom,      // a cat washing; a dog scratching
		Stand,      // standing, looking round
		Walk,
		Trot,
		Run,
		Sniff,      // nose to the ground, slowly on
		Eat,
		Watch,      // still, head on something (the player, an animal, a bird)
		Approach,   // going up to someone (the player, an animal) slowly
		Greet,      // nose to nose, sniffing the other (dogs: round each other)
		Beg,        // sitting in front of the player looking up (a dog), meowing at them (a cat)
		Rub,        // a cat rubbing against the player's legs
		PlayBow,    // a dog's invitation: front down, rear up
		Play,       // chasing and wrestling (dogs, kittens)
		Stalk,      // a cat creeping up on a bird, low
		Crouch,     // ... frozen before the pounce, rear wiggling
		Pounce,
		Chase,      // after a bird, or a dog after a cat
		Flee,
		Climb,      // a cat up (or down) a tree
		Perch,      // a cat up a tree, or on a wall
		Hiss,       // a cat's standoff: back arched, side on
		Bark,       // a dog barking (at a cat up a tree, a stranger dog, the player)
		Follow,     // reserved: a dog walking with the player (taming comes later)
		Num
	};
	const char* ActName(EAct A);
	inline bool IsMoving(EAct A)
	{
		return A == EAct::Walk || A == EAct::Trot || A == EAct::Run || A == EAct::Sniff || A == EAct::Approach || A == EAct::Stalk
			|| A == EAct::Chase || A == EAct::Flee || A == EAct::Play || A == EAct::Follow;
	}

	/** Why it is going somewhere / doing it. */
	enum class EGoal : uint8_t { None, Sleep, Eat, Bask, Roam, Visit, Play, Hunt, Person, Rest, Refuge, Follow, Num };
	const char* GoalName(EGoal G);

	/** Sounds and moments the game shows (calls play at the animal). */
	enum class EEvent : uint8_t { Notice, Watch, Approach, Flee, Bark, Growl, Whine, Meow, Purr, Hiss, Pounce, ChaseStart, Climb, Greet, PlayStart, Num };
	const char* EventName(EEvent E);

	struct FEvent
	{
		EEvent Kind;
		int Animal;
		int Other;      // the animal it is about, -1 = the player or nobody, -2 = a bird
		V3 Pos;
	};

	struct FPlace
	{
		EPlace Kind = EPlace::Doorstep;
		V3 Pos;
		float Yaw = 0.f;
		int Capacity = 2;
		float Sun = 0.f;        // 0..1 how sunny (south walls)
		float Warm = 0.f;       // 0..1 warm (heating hatch, cellar window)
		float Food = 0.f;       // 0..1 how much food there is when it is put out / found
		float Height = 0.f;     // refuges: how high up the perch is, m
		std::string Tag;        // "academ", "town", "gastronom", "club", ...
		// live
		float FoodLeft = 0.f;   // portions there now
		int Occupants = 0;
	};

	/** A bird near an animal, from the bird life (filled by the wrapper). */
	struct FPrey
	{
		int Id = -1;
		V3 Pos;
		float Size = 0.2f;      // m beak to tail
		bool bGround = true;    // on the ground (a cat can stalk it); false = perched or flying
	};

	/** The player as the animals perceive them (filled by the wrapper every frame). */
	struct FPlayer
	{
		bool bPresent = false;
		V3 Eye, Feet, Vel;
		V3 Facing { 1.f, 0.f, 0.f };
		bool bCrouched = false;
		float ExtraThreat = 0.f;    // a shout, a dropped thing (decays)
		bool bOffer = false;        // food held out (befriending, later)
		V3 Palm;
	};

	struct FAnimal
	{
		int Id = -1;
		EKind Kind = EKind::Cat;
		uint8_t Breed = 0;          // dogs: 0 laika-like yard dog, 1 shepherd mix, 2 small mutt; cats: 0
		uint8_t Coat = 0;           // the coat pattern of its breed (AnimalLife picks the material)
		bool bFemale = false;
		bool bYoung = false;        // a kitten or a pup: plays a lot, small
		float Size = 1.f;           // scale of its breed's model
		std::string Name;           // reserved: the name the player gives it (taming, later)

		V3 Pos, Vel;
		float Yaw = 0.f;
		float YawRate = 0.f;
		float Speed = 0.f;          // m/s it is going at now (along Yaw)
		EAct Act = EAct::Rest;
		float ActTime = 0.f, ActLen = 5.f;
		EGoal Goal = EGoal::None;
		int GoalPlace = -1;
		int Target = -1;            // the animal it is about (play, chase, greet, flee from); -1 the player
		int TargetBird = -1;        // the bird it is stalking / chasing
		V3 GoalPos;
		V3 Look;                    // where the head looks (world point); valid when bLook
		bool bLook = false;
		float Up = 0.f;             // m above the ground (a cat up a tree)
		float UpFrom = 0.f, UpTo = 0.f;   // a climb: from and to (m above the ground)
		int Refuge = -1;            // the tree it is up (or going to)
		bool bChaser = false;       // play: chasing (true) or being chased
		V3 Center;                  // play: the spot they play round; flee: where the danger was
		int Meals = 0;              // portions eaten today (debug)

		// needs, 0 fine .. 1 urgent
		float Hunger = 0.3f, Tired = 0.3f, Playful = 0.3f, Lonely = 0.2f, Curious = 0.4f;
		// how it feels now
		float Fear = 0.f;           // 0..1 frightened
		float Alert = 0.f;          // 0..1 on edge
		float Excite = 0.f;         // 0..1 worked up (a chase, play): tail and ears
		// temper, 0..1 (fixed per animal)
		float Bold = 0.5f, Friendly = 0.5f, Social = 0.5f, Hunter = 0.5f, Playfulness = 0.5f, Lively = 0.5f;

		int Home = -1;              // its home place (doorstep, cellar, den)
		V3 HomePos;
		float Range = 80.f;         // m it goes from home
		int Pack = -1;              // dogs: its pack
		bool bLeader = false;

		// the player (befriending later): trust grows when the player is calm near it, bond is reserved for taming
		float Trust = 0.f;          // 0..1
		float Bond = 0.f;           // reserved
		bool bNoticed = false;
		float Seen = 0.f, Unseen = 0.f;
		float Habituation = 0.f;

		// moving
		std::vector<V3> Path;
		int PathI = 0;
		float Stuck = 0.f;
		float BestD = 1e9f, NoProgress = 0.f;   // headway to the next waypoint
		int Replans = 0;
		float Phase = 0.f;          // gait phase (drawing)

		uint8_t Tier = 3;
		float ThinkTimer = 0.f;
		float BarkTimer = 0.f;
		float CallTimer = 10.f;
		float Cooldown = 0.f;       // s before it starts another play / chase / hunt
		uint32_t Rng = 1;
	};

	struct FPack
	{
		std::vector<int> Members;
		int Den = -1;               // its den place
		V3 Home;
		float Range = 400.f;
		int Leader = -1;
		// the pack's errand: everybody follows the leader there
		int Place = -1;
		float Timer = 0.f;
	};

	struct FOptions
	{
		uint32_t Seed = 20261025;
		int Cats = 42;
		int Dogs = 19;
		float PopulationScale = 1.f;
		/** LOD tiers by distance to the camera (m) and think intervals (s). */
		float NearDist = 50.f, MidDist = 160.f, FarDist = 450.f;
		float ThinkNear = 0.15f, ThinkMid = 0.5f, ThinkFar = 2.f, ThinkDormant = 5.f;
		float StepFar = 0.25f, StepDormant = 1.f;
	};

	/** Line of sight from the wrapper (true = clear); null = always clear. */
	using FSightFn = std::function<bool(const V3& From, const V3& To)>;

	class FWorld
	{
	public:
		FOptions Opt;
		std::vector<FPlace> Places;
		std::vector<FAnimal> Animals;
		std::vector<FPack> Packs;
		std::vector<FEvent> Events;     // this step's events (cleared by the caller)
		std::vector<FPrey> Prey;        // birds near the animals (the wrapper fills it a few times a second)
		/** Birds a cat pounced at or a dog ran into this step: the wrapper scares them (bird ids). */
		std::vector<int> ScaredBirds;
		FPlayer Player;
		FSightFn Sight;
		/** Exact ground height at (X, Y) from the wrapper (a line trace); false = use the grid. */
		std::function<bool(float X, float Y, float& Z)> Probe;
		V3 Camera;
		double Time = 0.0;              // s of simulation
		float Hour = 16.f;              // local clock
		float Sunrise = 7.5f, Sunset = 17.f;
		int TierCount[4] = {};
		std::string LastError;
		/** Ground height where there is no grid (a test level). */
		float NoGridZ = 0.f;

		/** Reads animal_world.bin and animal_places.json from Dir. */
		bool Load(const std::string& Dir);
		void AddPlace(const FPlace& P) { Places.push_back(P); }
		/** A tree (or a wall top) a cat can escape up; Height = perch over the ground (m). */
		void AddRefuge(const V3& Base, float Height);
		/** Creates the animals (after the places are final), each where it would be at Hour. */
		void Populate();
		/** One animal at P (tests): it lives there (its home is P). Returns its id. */
		int AddAnimal(EKind Kind, int Breed, int Coat, const V3& P, bool bYoung = false);
		/** The clock jumped: everybody goes to where they would be now. */
		void Resettle();
		void Step(float Dt);
		/** Where the animal is drawn now (between coarse steps). */
		V3 RenderPos(const FAnimal& A) const;

		bool Walkable(float X, float Y) const;
		EGround GroundAt(float X, float Y) const;
		float GroundZ(float X, float Y) const;
		void Near(const V3& P, float R, std::vector<int>& Out) const;
		/** A walking route (waypoints, the last one To) for that kind of animal; false = no way there. */
		bool FindPath(const V3& From, const V3& To, EKind Kind, std::vector<V3>& Out) const;
		/** How well two animals know each other, 0 strangers .. 1 packmates, mother and kittens. */
		float Familiarity(int A, int B) const { return Fam(A, B); }
		void SetFamiliarity(int A, int B, float V) { AddFam(A, B, V - Fam(A, B)); }
		/** A loud noise at P: animals within R start (dogs bark, cats flee). */
		void Scare(const V3& P, float R, float Strength);
		std::string Report() const;
		/** One line about an animal (debug). */
		std::string Describe(int Id) const;

	private:
		struct FGrid
		{
			int NX = 0, NY = 0;
			float X0 = 0, Y0 = 0, Cell = 2;
			std::vector<int16_t> Z;
			std::vector<uint8_t> Kind;
			// coarse grid for paths: CoarseK fine cells a side
			int CNX = 0, CNY = 0;
			std::vector<uint8_t> CoarseWalk;   // walkable share 0..16 of each coarse cell
			std::vector<uint8_t> CoarseKind;   // its commonest ground
		} Grid;
		static constexpr int CoarseK = 4;
		std::vector<int> CellHead, CellNext;
		float HashCell = 12.f;
		int HashNX = 0, HashNY = 0;
		float HashX0 = 0.f, HashY0 = 0.f;
		float StepAcc[4] = {};
		std::vector<float> Familiar;    // N x N: 0 strangers .. 1 packmates / old neighbours
		float LastHour = -1.f;
		std::vector<int> Scratch;

		float Fam(int A, int B) const;
		void AddFam(int A, int B, float D);
		void BuildCoarse();
		void BuildHash();
		void ForNear(const V3& P, float R, const std::function<void(int)>& Fn) const;
		void UpdateTiers();
		void UpdatePlaces(float Dt);
		void UpdateNeeds(FAnimal& A, float Dt);
		void Think(FAnimal& A, float Dt);
		void Perceive(FAnimal& A, float Dt);
		void PerceivePlayer(FAnimal& A, float Dt);
		bool PerceiveAnimals(FAnimal& A, float Dt);
		bool PerceivePrey(FAnimal& A, float Dt);
		void Choose(FAnimal& A);
		void PackThink(FPack& P, float Dt);
		void Move(FAnimal& A, float Dt);
		void Arrive(FAnimal& A);
		void SetAct(FAnimal& A, EAct Act, float Len);
		void GoTo(FAnimal& A, const V3& To, EAct Gait, EGoal Goal, int Place = -1);
		bool LineWalkable(const V3& A, const V3& B) const;
		V3 FreeSpot(const V3& Near, float R, uint32_t& Rng) const;
		int PickPlace(const FAnimal& A, EGoal Goal) const;
		float PlaceScore(const FAnimal& A, const FPlace& P, EGoal Goal) const;
		void StartFlee(FAnimal& A, const V3& From, int Cause);
		void GiveWay(FAnimal& A, const V3& From);
		void StartChase(FAnimal& A, int Target);
		void Emit(EEvent E, const FAnimal& A, int Other = -1) { Events.push_back({ E, A.Id, Other, A.Pos }); }
		float Rand(uint32_t& S) const;
		float Activity(const FAnimal& A) const;
		float GaitSpeed(const FAnimal& A, EAct Gait) const;
		void PlaceAtStart(FAnimal& A);
		int NearestRefuge(const V3& P, float MaxD, const V3& AwayFrom) const;
		float ZAt(const FAnimal& A) const;
	};
}
