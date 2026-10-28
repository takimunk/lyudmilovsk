// The town's people, in plain C++ (no Unreal types) so it can also be run and looked at outside the engine
// (tools/peoplesim). UPeopleLifeSubsystem wraps it for the game (docs/people.md).
//
// Every person has a profile (name, age, occupation, household, temper), needs that grow and are met (food, sleep,
// company, fun, fresh air, the mind, the sun), a home (a flat in the Academgorodok, reserved for their household) and
// a day: fixed hours (sleep, school or work) and free time in which they choose what to do from a table of activities
// (Content/PeopleData/activities.json) by their needs, their temper, the hour and how far it is. An activity happens at
// a slot of a place (a bed, a kitchen chair, a club computer, a library chair, a bench); slots are reserved so nobody
// sits on anybody's knees. Ways: outdoors along the animal walk grid (tools/animal_map.py, A*), indoors along the
// polylines of tools/people_map.py (stairs, flats, the library, the club); between the valley and the club's spur by the
// cable car; between the valley and the sea island by the sea railway's train (its timetable through TrainNext/TrainRide:
// Private/Railway/RailRun.h in the game and in tools/peoplesim).
//
// Frame: Unreal world metres (UE cm / 100), Z up, yaw in degrees (0 = +X, 90 = +Y).
#pragma once

#include "../Animals/AnimalSim.h"
#include "MiniJson.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace LyudPeople
{
	using LyudAnimals::V3;

	enum class ENeed : uint8_t
	{
		Hunger,     // food
		Tired,      // sleep
		Social,     // company, talking
		Fun,        // play, games
		Nature,     // fresh air, a walk, green and the sea
		Mind,       // reading, learning
		Sun,        // to be in the sun, to see it go down (docs/story/motifs.md)
		Num
	};
	const char* NeedName(ENeed N);
	ENeed NeedFromName(const std::string& S);   // Num if unknown
	constexpr int NumNeeds = (int)ENeed::Num;

	/** How the body looks while it does something (NpcBody plays the clip). */
	enum class EPose : uint8_t
	{
		Hidden,     // not in the world (riding the cable car, away in town, at school)
		Stand,      // standing, looking round
		Walk,
		Talk,       // standing, talking to someone
		Look,       // standing still, looking at something far (the sun going down)
		Sit,        // sitting (a bench, a sofa, a chair)
		Computer,   // at a computer (each person's own seated clip)
		Eat,        // at a table
		Read,       // a book in the hands, seated
		Sleep,      // lying in bed
		Work,       // at their work place: the club's owner at the booth (ABoothOwner draws him), the clerk at his desk
		Num
	};
	const char* PoseName(EPose P);
	EPose PoseFromName(const std::string& S);
	inline bool IsSeated(EPose P) { return P == EPose::Sit || P == EPose::Computer || P == EPose::Eat || P == EPose::Read; }

	/** Where someone is in what they do. */
	enum class EState : uint8_t
	{
		Idle,       // between activities (decides next think)
		Going,      // walking a route to the activity's slot
		Riding,     // in the cable car or the train (hidden in the sim; the game seats them), comes out at the other station
		Settling,   // sitting down / lying down at the slot (Transit seconds)
		Doing,      // doing the activity at the slot
		Leaving,    // standing up / getting up (Transit seconds), then Idle
		Away,       // gone to town / school / work elsewhere: not in the world
		Waiting,    // on a railway halt's platform at a boarding point, waiting for the train's doors to open
		Num
	};
	const char* StateName(EState S);

	/** A spot to do something at: a bed, a chair, a computer, a place on a bench, a spot to stand. */
	struct FSlot
	{
		std::string Kind;           // bed, sofa, armchair, kitchen_seat, desk_seat, dining_seat, read_seat, club_seat, booth, bench, stand, ...
		int Place = -1;
		V3 Pos;                     // floor point under the seat / the middle of the mattress / where to stand
		float Yaw = 0.f;            // the way the person faces (lying: head to foot)
		V3 Approach;                // where to stand before sitting or lying down
		float Height = 0.f;         // seat or mattress top over the floor, m
		int Capacity = 1;
		std::string Tag;            // club seats: the station index; flats: the room kind
		std::vector<V3> Spots;      // a double bed: where each sleeper lies
		std::vector<V3> Inside;     // the way from the place's door (Portal) to Approach; empty outdoors
		// reservation
		int Owner = -1;             // only this person (or their household) uses it; -1 anybody
		std::vector<int> Users;     // who is using or going to it now
		bool Free(int Person) const;
	};

	/** A place: a flat, the library, the club, a bench, the square, the beach, a station, the way to town. */
	struct FPlace
	{
		std::string Id;
		std::string Kind;           // flat, library, club, bench, square, lookout, beach, promenade, station_lower, station_upper, town,
		                            // lab, shop, rail_stop, island_spot, ...
		V3 Portal;                  // outdoor point by its door (or the spot itself outdoors)
		float Yaw = 0.f;
		bool bIndoor = false;
		int Island = 0;             // 0 the valley (town, Academgorodok, beach), 1 the club's spur, 2 the sea island (by train); -1 cut off
		// a railway halt (kind rail_stop): the timetable's stop id (beach, island) and the way from Portal (on the ground,
		// where walkers arrive) to the platform's deck (the stair's foot and top); its slots (kind board) are the
		// boarding points on the deck
		std::string Stop;
		std::vector<V3> RailWay;
		std::vector<int> Slots;
		// flats
		std::string House;
		int Floor = 0, Number = 0;
		std::vector<int> Residents; // reservation of the flat (people ids)
	};

	/** One row of activities.json: what people can do and what it gives them. */
	struct FActivity
	{
		std::string Id;
		std::vector<std::string> Places;    // place kinds; "home" = their own flat
		std::vector<std::string> Slots;     // slot kinds there
		EPose Pose = EPose::Stand;
		float MinMinutes = 20.f, MaxMinutes = 60.f;
		float From = 0.f, To = 24.f;        // hours it can start in (To < From: over midnight)
		int MinAge = 0, MaxAge = 200;
		float Need[NumNeeds] = {};          // how much of each need it meets per hour (0.6 = most of it in an hour)
		float Cost[NumNeeds] = {};          // how much it adds per hour (a walk makes you hungry)
		float Base = 0.f;                   // how much people like it anyway
		std::string Trait;                  // the temper that draws people to it (social, active, studious, gamer, nature, sun)
		bool bDaylight = false;             // only while the sun is up
		bool bSunset = false;               // only around sunset (see the sun go down)
		bool bTalk = false;                 // looks for somebody to talk to there
		bool bWander = false;               // a walk: goes on round the place's spots
		float WanderNear = 15.f, WanderFar = 220.f;     // how far the next spot is (m)
		float PauseMin = 40.f, PauseMax = 130.f;       // how long they stay at a spot (s)
		bool bPauseSet = false;                         // wander_pause given (else the first stop is 30-90 s)
		bool bAway = false;                 // leaves the world (town, school)
		bool bFree = true;                  // chosen in free time (false: only by a duty: sleep, work, school)
		int Days = 0x7F;                    // the weekdays it can be chosen on (bit 0 Monday; a shop closed on Sundays)
	};

	/** Fixed hours of a person: work or school. */
	struct FDuty
	{
		std::string Activity;
		float From = 9.f, To = 17.f;
		int Days = 0x1F;            // bit 0 Monday .. bit 6 Sunday
	};

	struct FPerson
	{
		int Id = -1;
		std::string Key;            // owner, clerk, streamer, kid_boy, ... (npc.<key>.name in the strings)
		std::string Mesh;           // the model shown now: Owner, Clerk, Streamer, ... (/Game/Characters/Humans/<Mesh>/SK_<Mesh>)
		std::string Body;           // their own body's id (people.json body; = Mesh when absent): the game loads it first and
		                            // falls back to Mesh while it isn't made; also their id in npc_dialogue.txt
		std::string ComputerClip;   // their own seated clip at a computer (Sit_KeyboardMouse, ...)
		std::string Occupation;
		std::string Household;      // people of one household share a flat
		int Age = 30;
		bool bFemale = false;
		bool bChild = false;
		float WalkSpeed = 1.25f;    // m/s
		// temper 0..1
		float Social = 0.5f, Active = 0.5f, Studious = 0.5f, Gamer = 0.5f, NatureLove = 0.5f, SunLove = 0.5f;
		float Wake = 7.5f, BedTime = 23.f;  // hours
		std::vector<FDuty> Duties;
		std::string ClubSeat;       // their usual station in the club ("4"); "" none
		std::vector<std::string> Friends;
		std::vector<std::pair<std::string, float>> Likes;   // activity id -> how much more they like it (a painter and the island)
		std::string OwnSlot;        // "club:9": a slot only they use
		std::string StartActivity;  // what they do when a new game begins
		float StartMinutes[2] = { 60.f, 120.f };

		// home
		int Home = -1;              // flat place
		int BedSlot = -1;           // their bed slot

		// needs 0 fine .. 1 urgent
		float N[NumNeeds] = {};

		// now
		EState State = EState::Idle;
		int Activity = -1;
		int Place = -1, Slot = -1;
		double Until = 0.0;         // sim seconds the activity lasts till
		float Minutes = 0.f;        // how long it will do it once there
		double WanderAt = 0.0;      // a walk: when to go on to the next spot
		double StateStart = 0.0;
		float Transit = 0.f;        // s of sitting down / standing up left
		EPose Pose = EPose::Stand;
		int Partner = -1;           // talking to
		V3 Pos;
		float Yaw = 0.f;
		float Speed = 0.f;
		bool bIndoors = false;
		bool bHurry = false;        // late (for work, school): runs the outdoor part of the way
		int Island = 0;             // which side of the cable car they are on (FPlace::Island)
		// Kind 0 outdoors (the walk grid's ground, or the island's), 1 indoors, 2 the cable car from here (Ride s) to the next,
		// 3 the train from here (a boarding point) to the next (the other halt's boarding point), 4 outdoors along a fixed
		// line in 3D (to a halt, its stair and platform); ToIsland: where a ride comes out; RailFrom/RailTo: the halts' places
		struct FWay { V3 P; uint8_t Kind = 0; float Ride = 0.f; int8_t ToIsland = -1; int RailFrom = -1, RailTo = -1; };
		std::vector<FWay> Path;
		int PathI = 0;
		float Think = 0.f;
		float Recheck = 60.f;       // s till they next look up from what they do
		bool bBreak = false;        // wants a break from a duty (to eat)
		bool bOnBreak = false;      // on it: the duty waits till this activity ends
		bool bRideExternal = false; // the ride is the cable car's (RideHook took it): it ends with EndRide
		bool bTrain = false;        // waiting for or riding the train
		int RailFrom = -1, RailTo = -1;     // the train ride's halts (places)
		double DoorsAt = 0.0;       // waiting: when the doors open (sim seconds)
		double RideUntil = 0.0;     // riding the train: when it opens its doors at the other halt
		uint32_t Rng = 1;
		std::string Why;            // a short reason for the last choice (debug)
		int Choices = 0;
	};

	enum class EEvent : uint8_t { Start, Arrive, Leave, TalkStart, TalkEnd, Ride, Away, Back, Num };
	const char* EventName(EEvent E);
	struct FEvent { EEvent Kind; int Person; int Other; int Place; };

	struct FOptions
	{
		uint32_t Seed = 20261020;
		float ThinkEvery = 2.f;         // s between decisions of an idle person
		float RideSeconds = 150.f;      // the cable car from station to station
		float TrainSeconds = 280.f;     // the train between the halts when there is no timetable (TrainNext unset or -1): hidden
		float WalkCostPerHour = 1.6f;   // how much an hour of walking puts people off a choice
		float Jitter = 0.15f;
	};

	class FWorld
	{
	public:
		FOptions Opt;
		std::vector<FPlace> Places;
		std::vector<FSlot> Slots;
		std::vector<FActivity> Activities;
		std::vector<FPerson> People;
		std::vector<FEvent> Events;     // this step's (cleared by the caller)
		LyudAnimals::FWorld Ground;     // the walk grid (animal_world.bin), for the ways outdoors
		bool bHasGround = false;
		std::string LastError;

		// the clock, set by the caller every step
		double Time = 0.0;              // sim seconds
		float Hour = 16.f;
		int Weekday = 1;                // 0 Monday .. 6 Sunday
		float Sunrise = 7.5f, Sunset = 17.f;
		float SunYaw = 225.f;           // where the sun sets/stands, yaw (deg) in the UE frame
		bool bSunUp = true;
		bool bRain = false;             // raining: people outdoors hurry (the wrapper sets it from the weather)
		V3 Camera;

		/** activities.json, people.json, people_places.json from Dir; the walk grid from AnimalsDir ("" = no grid). */
		bool Load(const std::string& Dir, const std::string& AnimalsDir);
		bool LoadActivities(const std::string& Text);
		bool LoadPeople(const std::string& Text);
		bool LoadPlaces(const std::string& Text);
		/** Gives every household a flat (the saved ones first: SetHome before) and everybody a bed. */
		void AssignHomes();
		/** Moves a household into a flat (by flat id); false if it is not a flat or is full. */
		bool SetHome(const std::string& Household, const std::string& FlatId);
		/** Puts everybody where they would be at Hour, doing what they would be doing. */
		void Resettle();
		/** A new game: everybody where the club scene has them (people.json start), the rest as Resettle. */
		void NewGame();
		void Step(float Dt);

		int FindPerson(const std::string& Key) const;
		int FindPlace(const std::string& Id) const;
		int FindActivity(const std::string& Id) const;
		/** A route between two points (each indoors at a place's slot or outdoors); false = no way. */
		bool Route(const FPerson& P, const V3& From, int FromSlot, int ToSlot, std::vector<FPerson::FWay>& Out) const;
		/** Where they are drawn and which way they face (between steps it is Pos). */
		std::string Report() const;
		std::string Describe(int Id) const;
		/** What each activity would score for them now (debug). */
		std::string Scores(int Id) const;
		/** Forces an activity now (tests, console): goes there and does it. */
		bool Order(int Person, const std::string& Activity, const std::string& PlaceId = std::string());

		/** The cable car (ULyudTransportSubsystem): called when someone gets to the station to ride from From to the station by To
		 *  (both Unreal metres); true if a real cabin will carry them, and then the ride ends only with EndRide (or after an hour,
		 *  should the cable car lose them). Unset or false: the ride takes Opt.RideSeconds, hidden, as before. */
		std::function<bool(int Person, const V3& From, const V3& To)> RideHook;
		/** Their cable-car ride is over: out at the other station, on along their way. */
		void EndRide(int Person);

		/** The sea railway's timetable (ULyudRailwaySubsystem in the game, LyudRail::FRun in tools/peoplesim), in sim seconds:
		 *  the next moment at or after Time a train going from stop From towards stop To (ids: beach, island) opens its doors
		 *  at From; < 0 if none (no railway: the ride is then TrainSeconds, hidden). */
		std::function<double(const std::string& From, const std::string& To, double Time)> TrainNext;
		/** Seconds from the doors opening at From to their opening at To. */
		std::function<double(const std::string& From, const std::string& To)> TrainRide;
		/** The game's own positions of a halt (its stair's foot on the ground and its boarding points on the deck, Unreal
		 *  metres) over the data's guesses; false if there is no such halt. */
		bool SetRailStop(const std::string& Stop, const V3& Foot, const std::vector<V3>& Boarding);

		float Ground_Z(const V3& P) const;
		/** Ground under a point: the island's own height field where it has one (people_places.json ground_patches), else
		 *  the walk grid's; P.Z without either. */
		float GroundAt(const V3& P) const;
		int IslandOf(const V3& P) const;
		/** Seconds of the way from From (on island FromIsland) to slot To leaving at At: walks, rides, waits for the train. */
		float WaySeconds(const FPerson& P, const V3& From, int FromIsland, int ToSlot, double At) const;
		/** A halt of the sea railway (place index) on an island, -1 none. */
		int RailStopOf(int Island) const;

		/** Height fields of ground off the walk grid (the sea island), Unreal metres. */
		struct FPatch { float X0 = 0.f, Y0 = 0.f, Cell = 4.f; int NX = 0, NY = 0; std::vector<float> Z; bool Has(float X, float Y) const; float At(float X, float Y) const; };
		std::vector<FPatch> Patches;

	private:
		int StationOf(int Island) const;
		void ReserveOwnSlots();
		void UpdateNeeds(FPerson& P, float Dt);
		void Think(FPerson& P, bool bSnap = false);
		bool Interrupt(FPerson& P, float Dt);
		void Move(FPerson& P, float Dt);
		/** Their pace now, m/s: a walk, or a run outdoors when late or in the rain (a brisk walk indoors). */
		float Pace(const FPerson& P) const;
		void Arrive(FPerson& P);
		void FinishRide(FPerson& P);
		void Leave(FPerson& P);
		void Finish(FPerson& P);
		bool Start(FPerson& P, int Activity, int Slot, double Minutes, bool bSnap, const char* Why);
		int DutyNow(const FPerson& P, bool& bSleep) const;
		/** Hours of free time left: till bed, or till today's next duty begins. */
		float FreeHoursLeft(const FPerson& P) const;
		bool InHours(float From, float To, float H) const;
		float Score(const FPerson& P, const FActivity& A, int& OutSlot) const;
		int PickSlot(const FPerson& P, const FActivity& A, float& OutMinutes) const;
		bool SlotFits(const FPerson& P, const FActivity& A, int Slot) const;
		float Trait(const FPerson& P, const std::string& T) const;
		V3 Where(const FPerson& P) const;
		void Reserve(FPerson& P, int Slot);
		/** The hops between two islands: (kind 2 cable car | 3 train, the place to set off from, the place to come out at, the island). */
		struct FHop { int Kind; int A; int B; int To; };
		bool Hops(int From, int To, std::vector<FHop>& Out) const;
		/** On the sea island: ends the activity in time for a train back (the walk to the halt and a minute to spare). */
		void CatchTrainBack(FPerson& P);
		void BoardTrain(FPerson& P);
		V3 SpotOf(const FPerson& P, int Slot) const;
		float YawAt(const FPerson& P, int Slot, EPose Pose) const;
		void Unreserve(FPerson& P);
		void TalkPairs();
		float PairTimer = 0.f;
		void Wander(FPerson& P);
		void OutdoorWay(const V3& From, const V3& To, std::vector<FPerson::FWay>& Out) const;
		float Rand(uint32_t& S) const;
		void Emit(EEvent E, const FPerson& P, int Other = -1) { Events.push_back({ E, P.Id, Other, P.Place }); }
	};
}
