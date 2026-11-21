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
		Follow,     // a befriended dog walking with the player (AnimalFriendSim.cpp)
		SniffHand,  // nose at the player's hand held out to it
		Petted,     // being stroked: leaning into the hand
		RollOver,   // on its back, belly up, under the hand
		Lap,        // a cat curled up on the seated player's lap
		Swim,       // a dog paddling after the player
		Num
	};
	const char* ActName(EAct A);
	inline bool IsMoving(EAct A)
	{
		return A == EAct::Walk || A == EAct::Trot || A == EAct::Run || A == EAct::Sniff || A == EAct::Approach || A == EAct::Stalk
			|| A == EAct::Chase || A == EAct::Flee || A == EAct::Play || A == EAct::Follow || A == EAct::Swim;
	}

	/** Why it is going somewhere / doing it. */
	enum class EGoal : uint8_t { None, Sleep, Eat, Bask, Roam, Visit, Play, Hunt, Person, Rest, Refuge, Follow, Num };
	const char* GoalName(EGoal G);

	/** Sounds and moments the game shows (calls play at the animal). */
	enum class EEvent : uint8_t { Notice, Watch, Approach, Flee, Bark, Growl, Whine, Meow, Purr, Hiss, Pounce, ChaseStart, Climb, Greet, PlayStart,
		BarkHappy,      // a glad bark back at the player who called it (Hail)
		// friendship (AnimalFriendSim.cpp)
		HandSniff,      // it came and sniffs the hand held out
		HandRefuse,     // it will not come to the hand (not yet), or does not want that food
		Stroked,        // a stroke begins: it leans in (a cat purrs)
		Enough,         // a cat that has had enough stroking walks off
		Roll,           // rolls over, belly up
		Ate,            // took the food from the hand (the wrapper takes it out of the pocket)
		Befriended,     // its bond crossed FriendBond: a friend from now on (once)
		WalkStart,      // a dog sets off with the player
		WalkEnd,        // ... was sent home, or left at the end of the day
		WaitDoor,       // it stays outside where the player went in
		Rejoin,         // it catches the player up (out of a door, from afar)
		LapUp,          // a cat jumps onto the seated player's lap
		LapDown,
		// yard cats and the townsfolk (AnimalPetsSim.cpp)
		Tail,           // a cat falls in behind the resident it likes, tail up (Other = -1)
		SeeIn,          // ... and sits down at their door when they have gone in
		BegStall,       // sits down in front of the fish stall, looking up
		Scrap,          // gets a scrap thrown from the stall
		Shooed,         // somebody with a broom wants it out of the way: it trots off and looks back
		// the hero's dog (Maru) and her moments with him (AnimalPetsSim.cpp)
		WaitLate,       // he is not back after dark: she sits down at the door to wait
		Welcome,        // ... and he comes: she runs to meet him
		Gift,           // she lays what she found at his feet (Other = what: FWorld::EGift)
		Comfort,        // thunder, or he sits low and still: she presses against him
		Shelter,        // rain: she lies down under the porch
		Num };
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
		bool bOffer = false;        // food held out on the palm (FWorld::OfferFood)
		V3 Palm;                    // the hand held out (food, or empty for a sniff), world
		bool bSeated = false;       // sitting on a bench
		V3 Lap;                     // seated: the lap (where a cat lies), world
		float SeatYaw = 0.f;        // seated: the way the sitter faces
		bool bSwimming = false;     // in the water
		float WaterZ = 0.f;         // swimming: the surface there
	};

	/** What a befriended (or befriending) animal is doing with the player (AnimalFriendSim.cpp). */
	enum class EFriend : uint8_t
	{
		None,
		ToHand,     // coming to the hand held out
		AtHand,     // sniffing it
		Stroked,
		Rolled,     // belly up
		ToFood,     // coming to the food on the palm
		EatHand,    // eating it
		Heel,       // a dog walking with the player
		Wait,       // ... waiting outside a door, or on the shore
		SwimAfter,  // ... in the water after them
		BySeat,     // ... sitting by their bench
		ToLap,      // a cat coming to the seated player
		OnLap,
		GoHome,     // sent home / off to bed
		Num
	};
	const char* FriendName(EFriend F);

	/** An animal that belongs to somebody (AnimalPetsSim.cpp): what it is doing as a pet, on top of Act. */
	enum class EPet : uint8_t
	{
		None,       // nobody's: the yard cats and the street dogs
		Indoors,    // at home behind the door: not drawn, met by nobody
		Leash,      // a dog out with its owner, on the leash
		Sill,       // a flat's cat on its window sill
		Door,       // ... waiting at the flat's door at feeding time
		Num
	};
	const char* PetName(EPet P);

	/** What a yard cat is up to with the townsfolk (AnimalPetsSim.cpp), on top of Act. */
	enum class ECatDo : uint8_t
	{
		None,
		Tail,       // trotting after the resident it likes, on their way home
		SeeIn,      // sitting at their door after they went in
		Beg,        // going to the open fish stall and begging there
		Scrap,      // eating what the seller threw it
		Shooed,     // sent off by somebody sweeping or mopping: away, then a look back
		Num
	};
	const char* CatDoName(ECatDo D);

	/** What the hero's dog is doing for him (AnimalPetsSim.cpp), on top of Act. */
	enum class EMoment : uint8_t
	{
		None,
		WaitLate,   // at the door after dark, looking the way he went
		Welcome,    // running to meet him
		Fetch,      // off to find something for him
		Bring,      // carrying it back
		Give,       // it lies at his feet; she sits and looks up
		Comfort,    // pressed against him (thunder, or he sits low and still)
		Shelter,    // under the porch in the rain
		Spot,       // going to the place she likes this season
		Num
	};
	const char* MomentName(EMoment M);

	/** A stall the cats beg at (the market's fish). */
	struct FStall
	{
		int Id = -1;
		V3 Pos;                     // where the seller stands
		float Yaw = 0.f;            // the way the counter faces (the cats sit on that side)
		float Front = 1.3f;         // m from the seller to where the cats sit (the buyers' side of the counter)
		bool bOpen = false;
		float Next = 0.f;           // s to the next look for a cat to call over
	};

	/** The weather as a flat's cat minds it (the wrapper sets it). */
	struct FPetWeather
	{
		bool bRain = false;
		float TempC = 15.f;
		float Sun = 0.5f;           // 0 overcast or night .. 1 full sun
		bool bThunder = false;      // a thunderstorm overhead
	};

	/** A hedgehog of the yards and the old orchard (AnimalPetsSim.cpp): out at dusk from April to October. */
	enum class EHog : uint8_t { Away, Shuffle, Sniff, Curl, Num };
	struct FHedgehog
	{
		int Id = -1;
		V3 Home, Pos, Goal;
		float Yaw = 0.f, Speed = 0.f;
		float Range = 16.f;         // m it shuffles from home
		EHog State = EHog::Away;
		float Timer = 0.f;          // s left of the state (Sniff), s since the danger went (Curl)
		float Curl = 0.f;           // 0 open .. 1 a ball (drawing)
		float Phase = 0.f;          // gait phase (drawing)
		int Cat = -1;               // the cat that has come to sniff it
		float CatTime = 0.f;
		float NoCat = 0.f;          // s before another cat bothers
		uint32_t Rng = 1;
	};

	/** A dog's walk on the leash: where its owner is (the wrapper fills it from the town's life every frame). */
	struct FLeash
	{
		int Dog = -1;
		bool bOut = false;
		V3 Owner, Vel, Hand;        // the owner's feet, velocity and the hand that holds the leash
		std::vector<V3> Trail;      // where the owner has just walked (newest last): the dog's way round a corner
		float Sniff = 0.f;          // s left of a sniff stop
		float NextSniff = 6.f;      // s to the next one
		float Greet = 0.f;          // s left of greeting another dog
		int GreetDog = -1;
		float NoGreet = 0.f;
		float Still = 0.f;          // s the owner has stood
	};

	struct FAnimal
	{
		int Id = -1;
		EKind Kind = EKind::Cat;
		uint8_t Breed = 0;          // dogs: 0 laika-like yard dog, 1 shepherd mix, 2 small mutt, 3 Maru (the club's dog); cats: 0
		uint8_t Coat = 0;           // the coat pattern of its breed (AnimalLife picks the material)
		bool bFemale = false;
		bool bYoung = false;        // a kitten or a pup: plays a lot, small
		float Size = 1.f;           // scale of its breed's model
		std::string Name;           // reserved: the name the player gives it (taming, later)
		std::string NameKey;        // a name it already has (a localization key, e.g. animal.maru.name): shown over it

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
		float Bond = 0.f;           // 0..1: grown by the hand and by food; FriendBond and over is a friend (saved)
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
		float HappyBark = 0.f;      // s left of barking back gladly at the player (Hail)
		float NextHappy = 0.f;      // s to its next glad bark
		float CallTimer = 10.f;
		float Cooldown = 0.f;       // s before it starts another play / chase / hunt
		uint32_t Rng = 1;

		// friendship (AnimalFriendSim.cpp): what it is doing with the player, on top of Act
		EFriend Friend = EFriend::None;
		float FriendTime = 0.f;     // s in that state
		float Sniffed = 0.f;        // s left in which it lets itself be stroked (it sniffed the hand, it ate from it)
		float PetFill = 0.f;        // 0..1 how much stroking it has had lately (a cat walks off when full)
		float FoodFill = 0.f;       // 0..1 treats lately (it stops taking them)
		float Lean = 0.f;           // 0..1 leaning into the hand (drawing)
		float OfferLike = 0.f;      // how much it likes the food it is coming for
		V3 WaitPos;                 // where it waits (outside the door, on the shore), where it went into the water
		bool bSaidWait = false;
		float Unreach = 0.f;        // s the player has been out of its reach (a walk): then it catches up out of sight
		float Still = 0.f;          // s at heel with the player standing
		float Repath = 0.f;
		float PurrTimer = 0.f;

		// pets (AnimalPetsSim.cpp)
		EPet Pet = EPet::None;
		int Owner = -1;             // whose it is: the wrapper's id of the resident (or household); -1 nobody's
		V3 DoorPos;                 // the home's door (a dog comes out here, a flat's cat waits here to be fed)
		V3 SillPos;                 // a flat's cat: its window sill (world, the cat's feet)
		float SillYaw = 0.f;        // ... the way out of the window
		float PetTime = 0.f;        // s in the pet state
		int Hog = -1;               // the hedgehog this cat has gone to sniff (FWorld::Hedgehogs), -1 none
		// a yard cat and the townsfolk (AnimalPetsSim.cpp)
		ECatDo Do = ECatDo::None;
		float DoTime = 0.f;         // s in it
		float DoFresh = 0.f;        // Tail: s since the wrapper last said where the person is
		V3 DoPos, DoVel;            // the person followed (feet, velocity); who shooed it
		V3 DoGoal;                  // the person's door (Tail, SeeIn); where it runs off to (Shooed)
		bool bDoThere = false;      // arrived (at the door, at the stall)
		int Favourite = -1;         // the resident this cat likes (the wrapper's id), -1 nobody
		int Stall = -1;             // the stall it begs at
		float NoDo = 0.f;           // s before it follows anybody again
		float NoStall = 0.f;        // s before it begs at a stall again
		// the hero's dog (AnimalPetsSim.cpp)
		EMoment Moment = EMoment::None;
		float MomentTime = 0.f;     // s in it
		V3 MomentPos;               // where she looks for a gift; where she waits
		bool bMomentThere = false;
		int Carry = -1;             // what she carries in her mouth (FWorld::EGift), -1 nothing (drawing)
		float Waited = 0.f;         // s she has sat waiting this evening
		float NoWait = 0.f, NoGift = 0.f, NoComfort = 0.f, NoSpot = 0.f;   // s before the next of each
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
		/** For now (Dima's testing): the club's dog and a cat keep close to the club's door (within ~15 m). */
		bool bClubPair = true;
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
		/** The player speaks to it (E): a dog turns to them and barks back gladly (a cat looks and meows). False if it can't. */
		bool Hail(int Id);
		// --- friendship (AnimalFriendSim.cpp) ---
		/** Bond at which an animal is the player's friend. */
		static constexpr float FriendBond = 0.5f;
		bool IsFriend(int Id) const { return Id >= 0 && Id < (int)Animals.size() && Animals[Id].Bond >= FriendBond; }
		/** The player (crouched, Player.Palm set) holds an empty hand out to it: it comes and sniffs it if it trusts them
		 *  enough (HandSniff, then it lets itself be stroked for a while), else it keeps off (HandRefuse). False: not now. */
		bool OfferHand(int Id);
		/** A stroke (a few seconds; call again for another): true if it lets them (it sniffed the hand, or is a friend). */
		bool Stroke(int Id);
		/** The stroke ends now (the hand went away). */
		void EndStroke(int Id);
		/** Food held out on the palm (Player.bOffer, Player.Palm), Like 0..1 for its kind: it comes and eats (Ate), or
		 *  sniffs and turns away (HandRefuse). False: it won't come now. */
		bool OfferFood(int Id, float Like);
		/** A befriended dog sets off with the player; one at a time. */
		bool StartWalk(int Id);
		/** The walk ends: bHome sends it home (else it stays about where it is). */
		void EndWalk(bool bHome);
		/** The dog walking with the player, -1 none. */
		int Walker = -1;
		/** The cat on the player's lap, -1 none. */
		int LapCat = -1;
		// --- pets and hedgehogs (AnimalPetsSim.cpp) ---
		/** How long a leash is, hand to collar along the ground, m. */
		static constexpr float LeashLen = 2.4f;
		/** The dog becomes Owner's: it lives indoors (Door) and comes out only on the leash (OwnerWalk). */
		bool AdoptDog(int Dog, int Owner, const V3& Door);
		/** The cat becomes a flat's: by day on its window sill, at feeding time at the door, else indoors. */
		bool AdoptCat(int Cat, int Owner, const V3& Sill, float SillYaw, const V3& Door);
		/** Every frame while the owner is out with the dog (bOut), once with bOut false when they are home again. */
		void OwnerWalk(int Dog, bool bOut, const V3& Feet, const V3& Vel, const V3& Hand);
		const FLeash* LeashOf(int Dog) const;
		/** The owner said yes to "may I stroke it?": the dog lets the player's hand come for Seconds. False: not out. */
		bool AllowStroke(int Dog, float Seconds = 40.f);
		/** True: the feeders are filled by a person (PutFood, the lady with her bowls), not by the clock. */
		bool bFeedByPerson = false;
		/** The lady has put her bowls down at that feeder. */
		void PutFood(int Place);
		/** The feeders the lady walks to (place ids), nearest first from P. */
		void FeedersNear(const V3& P, float R, std::vector<int>& Out) const;
		std::vector<FLeash> Leashes;
		std::vector<FHedgehog> Hedgehogs;
		int AddHedgehog(const V3& Home, float Range = 16.f);
		/** The calendar's month 1..12 (the wrapper sets it): hedgehogs sleep from November to March. */
		int Month = 7;
		bool HedgehogsOut() const;
		// --- yard cats and the townsfolk ---
		/** The cat likes that resident: it sees them home when they pass (FavouriteHome). */
		void SetFavourite(int Cat, int Person);
		/** Every frame while the resident the cat likes walks home within its sight. True: it is with them (or sets off). */
		bool FavouriteHome(int Cat, const V3& Feet, const V3& Vel, const V3& Door);
		std::vector<FStall> Stalls;
		int AddStall(const V3& Pos, float Yaw, float Front = 1.3f);
		/** The seller is at the counter (or gone): cats come to beg only at an open stall. */
		void StallOpen(int Stall, bool bOpen);
		/** The seller throws a scrap: the nearest cat begging there gets it. False: nobody is begging. */
		bool ThrowScrap(int Stall);
		/** How many cats sit begging at the stall now. */
		int Begging(int Stall) const;
		/** Somebody sweeping or mopping at P: the cats within R get out of the way (not a fright). Returns how many.
		 *  Not through a wall (a cleaner indoors, a cat under the window) unless bAnyGround (over a counter). */
		int Shoo(const V3& P, float R, bool bAnyGround = false);
		// --- the hero's dog and her moments with him ---
		/** What she brings: a stick in spring, a shell in summer, a cone in autumn, a lost mitten in winter, an old ball now and then. */
		enum EGift { Stick, Cone, Shell, Ball, Mitten, GiftNum };
		/** The dog that is the hero's (Maru; the wrapper sets it). The moments are hers once she is his friend. */
		int HeroDog = -1;
		/** 0 winter, 1 spring, 2 summer, 3 autumn. */
		static int SeasonOf(int Month) { return Month == 12 || Month <= 2 ? 0 : (Month <= 5 ? 1 : (Month <= 8 ? 2 : 3)); }
		int GiftNow() const;
		/** The place she likes this season (a warm hatch in winter, a sunny wall in spring and autumn, a bench or a
		 *  lookout in the summer's breeze), -1: none near her home. */
		int FavouriteSpot(int Dog) const;
		/** What happened, for the diary (the wrapper drains it): "wait", "gift_stick", "comfort", "thunder", "shelter", "spot_winter", ... */
		std::vector<std::string> MomentNotes;
		float HeroStill = 0.f;      // s the hero has sat or crouched without moving
		FPetWeather Weather;
		/** A flat's cat keeps its sill only in the dry, and in the cold only in the sun. */
		bool SillWeather() const { return !Weather.bRain && (Weather.TempC >= 8.f || (Weather.Sun > 0.5f && Weather.TempC >= 0.f)); }
		/** Hours a flat's cat waits at the door (the same as the feeders': 8:00 and 18:30). */
		static bool FeedingTime(float Hour) { return (Hour >= 7.5f && Hour < 8.25f) || (Hour >= 18.f && Hour < 18.75f); }

		/** Hour (local) after which a walking dog goes home to sleep, and before which it won't set off. */
		float BedHour = 22.5f, UpHour = 6.f;
		/** How far from the shore a dog swims after the player, m. */
		float SwimReach = 30.f;
		/** Tests: a walk grid of NX x NY cells of Cell m from (X0, Y0), all grass at height Z; then SetGround. */
		void MakeGrid(int NX, int NY, float X0, float Y0, float Cell, float Z);
		void SetGround(float X, float Y, EGround Kind, float Z);

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
			std::vector<int> CoarsePatch;      // which joined-up patch of open cells it is in (-1 closed): no path between two patches
		} Grid;
		static constexpr int CoarseK = 4;
		/** FindPath's working arrays, kept from one search to the next: a cell's cost and step back count only while its
		 *  stamp is this search's number, so nothing is cleared (three arrays the size of the map were made and zeroed for
		 *  every route, the short ones too). One search at a time: the life runs on one thread. */
		struct FPathScratch
		{
			std::vector<float> Cost;
			std::vector<int> Back;
			std::vector<uint32_t> Set;      // the search that gave the cell its Cost and Back
			std::vector<uint32_t> Shut;     // the search that closed it
			uint32_t Search = 0;
		};
		mutable FPathScratch PathScratch;
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
		// friendship (AnimalFriendSim.cpp)
		void FriendNeeds(FAnimal& A, float Dt);
		bool FriendThink(FAnimal& A, float Dt);
		bool FriendMove(FAnimal& A, float Dt);
		void FriendStep(float Dt);
		void FriendEnd(FAnimal& A, EAct Then, float Len);
		void AddBond(FAnimal& A, float D);
		// pets (AnimalPetsSim.cpp)
		bool PetThink(FAnimal& A, float Dt);
		bool PetMove(FAnimal& A, float Dt);
		void PetStep(float Dt);
		void LeashMove(FAnimal& A, FLeash& L, float Dt);
		void LeashHold(FAnimal& A, FLeash& L);
		void HedgehogStep(float Dt);
		bool CatDoMove(FAnimal& A, float Dt);
		void CatDoEnd(FAnimal& A, float NoDo);
		void StallStep(float Dt);
		void MomentStep(float Dt);
		bool MomentMove(FAnimal& A, float Dt);
		void MomentEnd(FAnimal& A);
		FLeash* LeashFor(int Dog);
		bool Steer(FAnimal& A, const V3& To, float Speed, float Dt, bool bAnyGround = false);
		bool PlayerOnGround() const;
		V3 Route(FAnimal& A, const V3& To, float Dt, bool& bNoWay);
		V3 PlayerLastOk;            // where the player last stood on ground the animals can walk
		bool bPlayerOk = false;
		float ZAt(const FAnimal& A) const;
	};
}
