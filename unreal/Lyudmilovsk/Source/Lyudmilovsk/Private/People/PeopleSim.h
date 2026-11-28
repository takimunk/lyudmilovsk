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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <climits>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
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
		Shovel,     // clearing a path of snow, pushing a scraper along it (the winter's paths)
		Skate,      // skating round the rink's loop (Speed > 0 gliding, 0 standing on the ice: a breath, a chat)
		Walrus,     // in the water of the ice hole to the shoulders (the walruses' swim)
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
		bool bFrontApproach = false; // the approach is the step in front of an outdoor seat: come to it from the front
		float Height = 0.f;         // seat or mattress top over the floor, m
		int Capacity = 1;
		std::string Tag;            // club seats: the station index; flats: the room kind
		std::string Thing;          // a city thing whoever is at the slot holds (UPeopleLifeSubsystem::UpdateThingHolds): a sanatorium guest's chair
		std::vector<V3> Spots;      // a double bed: where each sleeper lies
		std::vector<V3> Inside;     // the way from the place's door (Portal) to Approach; empty outdoors
		/** A slot off the walk grid (on the rink's ice, in the ice hole's water): the way onto it from a point people walk
		 *  to (front: the kerb, the deck) to the slot (back), walked as a line in 3D; out the same way back (LoadSpots). */
		std::vector<V3> Reach;
		int SnowJob = -1;           // a snow place's slot: the stretch of path it stands for (its near end is Pos), -1 none
		// a counter's queue (tools/town/counters.py, docs/counters.md): which line (FWorld::Queues) and the place in it, 0 the
		// one being served; bHeld: the hero stands there (FWorld::SetHeroAt), so it counts as taken
		int Queue = -1;
		int QueueN = -1;
		bool bHeld = false;
		int FrontWho = -1;          // q0: who is being served, since when (sim seconds)
		double FrontSince = 0.0;
		V3 SnowTo;                  // and its far end
		// reservation
		int Owner = -1;             // only this person (or their household) uses it; -1 anybody
		std::string OwnerKey;       // "owner" in town_places.json: that person's key (ReserveOwnSlots looks it up: an allotment bed's keeper)
		std::vector<int> Users;     // who is using or going to it now
		int K = -1;                 // Kind interned (FWorld::Index; -1 not yet: compared by name)
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
		int Island = 0;             // 0 the valley (town, Academgorodok, beach), 1 the club's spur, 2 the sea island (by train),
		                            // 3 the North (on foot by the trail, docs/north.md); -1 cut off
		// a railway halt (kind rail_stop): the timetable's stop id (beach, island) and the way from Portal (on the ground,
		// where walkers arrive) to the platform's deck (the stair's foot and top); its slots (kind board) are the
		// boarding points on the deck. A trail (kind trail): RailWay is its way on foot from Portal (on the valley's walk
		// grid) to the other island's end (TrailTo: the North), WayLen its length, m
		std::string Stop;
		std::vector<V3> RailWay;
		int TrailTo = -1;
		float WayLen = 0.f;
		bool bPrivate = false;      // a home only for whoever people.json gives it (home): never handed out (the ranger's hut, a tent)
		std::vector<int> Slots;
		// flats
		std::string House;
		std::string Stair;          // the stair it is up (its entrance)
		int Floor = 0, Number = 0;
		std::vector<int> Residents; // reservation of the flat (people ids)
		// the town's buildings (town_places.json, docs/town_life.md): a shell has no rooms to walk; whoever is in it is
		// inside (not drawn), at its door. A flat's windows light up while somebody is home and awake after dusk.
		bool bShell = false;
		bool bOpen = false;         // "open": entered by its gate and walked by its paths like a room, but out of doors (an allotment's
		                            // fenced yard): the weather counts there
		std::string Building;       // the plot / building id (T2-05)
		std::vector<std::string> Windows;   // window ids (T2-05/3-02/w1)
		std::vector<std::string> Roles;     // a workplace's job roles (waiter, cook, seller, ...)
		/** A closed loop to go round (the rink: 3 m in from the pond's kerb), anticlockwise seen from above, the first point
		 *  not repeated; LoopS: the distance along it to each point, LoopLen: once round (m). Empty: none. */
		std::vector<V3> Loop;
		std::vector<float> LoopS;
		float LoopLen = 0.f;
		int K = -1;                 // Kind interned
		int Pull = -1;              // its row of weather_pull.json (docs/indoor_weather.md), -1 none
	};

	/** One row of weather_pull.json (tools/town/weather_pull.py, docs/indoor_weather.md): how much the weather of the moment
	 *  draws people to the places of these kinds or ids (a choice's score, as the outdoors' rain penalty is; 1 ~ a strong
	 *  wish). The rain scales with how hard it falls, the cold below 2 C, the heat above 22 C in the dry sun, the storm
	 *  above 12 m/s; dark: after sunset. */
	struct FWeatherPull
	{
		std::vector<std::string> Kinds, Ids;
		float Rain = 0.f, Cold = 0.f, Hot = 0.f, Storm = 0.f, Dark = 0.f;
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
		bool bRound = false;                // a round (with bWander): on to the next spot of the place in its order, whatever the distance
		float WanderNear = 15.f, WanderFar = 220.f;     // how far the next spot is (m)
		float PauseMin = 40.f, PauseMax = 130.f;       // how long they stay at a spot (s)
		bool bPauseSet = false;                         // wander_pause given (else the first stop is 30-90 s)
		bool bAway = false;                 // leaves the world (town, school)
		bool bFree = true;                  // chosen in free time (false: only by a duty: sleep, work, school)
		int Days = 0x7F;                    // the weekdays it can be chosen on (bit 0 Monday; a shop closed on Sundays)
		bool bSnow = false;                 // clears snow off the paths: at a snow place's stretches, one after another
		bool bSunrise = false;              // only around sunrise (see the sun come up: early risers)
		bool bHolidayOff = false;           // a duty skipped on a day off (holiday: "off": school, offices)
		bool bGather = false;               // a gathering (a holiday on the square): only at a place with one on now
		int GatherMode = -1;                // a gathering's activity: the mode it is for (EGatherMode), -1 not one
		bool bStaff = false;                // a workplace duty: at their own workplace (FDuty::Place), at a slot of their role
		float Pace = 1.f;                   // "pace": how fast they go within it against their walk (a jog, children at play: 2)
		bool bRainOk = false;               // "rain": "ok": chosen and kept up in the rain too (outdoors)
		bool bOnTime = false;               // "on_time": 1: a duty of it ends at the duty's hour, however long the way there took (the dogs' walks)
		bool bShelter = false;              // "shelter": 1: only where the weather pulls now (weather_pull.json: under a roof in the rain)
		float WindMin = -1.f, WindMax = 99.f;   // "wind": [min, max] m/s: chosen only in such a wind (a kite: a breeze, not a calm or a gale)
		std::string When;                   // "when": only while this condition is on (FWorld::SetCondition: rink, walrus, banya);
		int WhenId = -1;                    // whoever is at it leaves within a minute after it goes off. Its id (ConditionId)
		float WkFrom = -1.f, WkTo = -1.f;   // "weekend_hours": the hours it can start in on Saturday and Sunday (-1: as on weekdays)
		float DayMore[7] = {};              // "day_more": {"5": 0.3}: liked more on these weekdays (Saturday's banya)
		float LoopMin = 0.f, LoopMax = 0.f; // "loop": [min, max] m/s: round the place's Loop at that speed (skating), pausing at its
		                                    // slots for wander_pause s; 0 none
		float DipMin = 0.f, DipMax = 0.f;   // "dip": [min, max] s at a slot of the first slot kind (the water), then out to one of
		                                    // the others (the deck) for a few minutes, and maybe in once more; 0 none
		float Regulars = 0.f;               // "regulars": only a fixed handful do it (this share of whoever could, more of those with
		                                    // its trait; anybody with a like for it too); 0 everybody
		float TempMin = -99.f, TempMax = 99.f;  // "temp": [min, max] C: chosen only in such weather (the allotment: not in the frost)
		// the days (PeopleDays.cpp, docs/town_life.md "The days"): what the calendar and the day's luck do to it
		bool bPlan = false;                 // "plan": 1: only as today's plan with somebody (FPerson::PlanAct: tea at a friend's, a couple's stroll)
		bool bVisit = false;                // "visit": 1: at the flat of whoever they visit today (FPerson::PlanPlace), a guest at its seats
		bool bOnce = false;                 // "once": 1: once a day each (the pension, the library's books taken back)
		int OnceBit = -1;                   // its bit of FPerson::OnceDone (Index; 32 such activities at most)
		bool bSickOff = false;              // "sick": "off": a duty of it is skipped with a cold (school, work out of town)
		uint8_t TodayMask = 0;              // "today": ["pension"]: only for whoever has one of these today (ETodayBit)
		float TodayMore[8] = {};            // "today_more": {"payday": 0.4}: liked more (or less) by whoever has that today
		float OwnDay = 0.f;                 // "own_day": 0.5: liked this much more on their own weekday for it (everybody has one:
		                                    // FWorld::OwnDayOf, of the activity's days); "own_day_only": 1: on no other day
		bool bOwnDayOnly = false;
		// the sailing school's people (docs/yacht_school.md "People (part F)"): "members": "<group>" only that group's members
		// choose it (FWorld::LoadMembers); "afloat": 1 whoever does it is out on the water (hidden here: the engine shows them
		// in a boat, UPeopleLifeSubsystem::GetSailors)
		std::string Members;
		int MembersG = -1;
		bool bAfloat = false;
		std::vector<int> PlaceIdx;          // the places of its kinds (built after loading; "home" and gatherings aside)
		std::vector<int> SlotK, PlaceK;     // Slots and Places interned
		bool bHome = false;                 // "home" among its places
		int FarIsland = -1;                 // every place of its kinds is on this island, 4 or beyond (the motor ship); -1 otherwise (Index)
		bool bK = false;                    // interned
	};

	/** Fixed hours of a person: work or school. */
	/** What someone does with their bicycle at a stop of their way (docs/bikes.md). */
	enum class EBikeAct : uint8_t
	{
		None,
		Take,       // takes it from where it stands (its place in the entrance), by the bars
		Mount,      // gets on (outside the door)
		Dismount,   // gets off (a few metres short of where they go)
		Park,       // puts it on its stand (by the door they go in at), or back in its place in the entrance
		TakeMount,  // takes it from where it stands outdoors and gets on at once
		Num
	};
	/** How long each takes, s. */
	float BikeActSeconds(EBikeAct A);

	/** What today is for somebody (FWorld::RollToday, PeopleDays.cpp): bits of FPerson::Today. */
	enum ETodayBit : uint8_t
	{
		TodayCold = 1 << 0,         // a cold: stays in, no school, the clinic on its first morning
		TodaySleepy = 1 << 1,       // slept badly: up late, coffee, a nap
		TodayBirthday = 1 << 2,     // their birthday: the cake bought by day, guests in the evening
		TodayPayday = 1 << 3,       // paid today (those who work): the cafe, the shops
		TodayPension = 1 << 4,      // the pension is paid today (the old): the office in the morning, then the market
		TodayGuest = 1 << 5,        // asked to somebody's birthday
	};
	constexpr int NumTodayBits = 6;
	const char* TodayName(int Bit);                     // cold, sleepy, birthday, payday, pension, guest
	uint8_t TodayFromName(const std::string& S);        // the bit, 0 if unknown
	/** Today's plan with somebody (FPerson::PlanKind). */
	enum class EPlan : uint8_t { None, Tea, Birthday, Stroll, Chess, Family, Num };
	const char* PlanName(EPlan P);                      // "", tea, birthday, stroll, chess, family

	struct FDuty
	{
		std::string Activity;
		float From = 9.f, To = 17.f;
		int Days = 0x1F;            // bit 0 Monday .. bit 6 Sunday
		std::string PlaceId;        // their own workplace (the town's: cafe_mar), "" any place of the activity's kinds
		std::string Role;           // their job there (waiter, cook): slots tagged with it, else any staff slot
		int Place = -1;
		int Act = -1;               // Activity's index (Index())
		// tied to the sun (people.json "sun": "set" | "rise"): "hours" are then offsets from today's sunset / sunrise and
		// From/To follow them (FWorld::SyncSunDuties): the lighthouse keeper lights the lamp at dusk all the year round
		int8_t Sun = 0;             // 0 fixed hours, 1 sunset, 2 sunrise
		float Off0 = 0.f, Off1 = 0.f;
		int StayDay = -1;           // "stay_day": only on this day of their stay (0 the day they come; FPerson::StayCycle)
	};

	/** Somebody who belongs to a group that keeps a programme (the sailing school: Content/PeopleData/sailing_members.json,
	 *  FWorld::LoadMembers): while a group of theirs has an activity set (FWorld::SetGroupActivity) they do it as a duty, through
	 *  their role there (a coach rigging when the pupils rig: the role's map turns one activity into another). */
	struct FMembership
	{
		int Group = -1;             // FWorld::GroupNames
		int Role = -1;              // FWorld::RoleNames
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
		float Wake = 7.5f, BedTime = 23.f;  // hours (the usual; today's is Wake + WakeJ, BedTime + BedJ)
		float Spread = 0.f;         // how much their day moves from day to day (h): today's wake and bed are rolled at 14:00
		float WakeJ = 0.f, BedJ = 0.f;
		bool bTown = false;         // a townsperson (townsfolk.json): generated, shown by a model of the shared pool
		std::string Chronotype;     // lark, owl, night, usual (townsfolk.json; debug and the summary)
		std::string HomeId;         // townsfolk.json: their flat (a place id) given by the generator
		bool bExternal = false;     // drawn by somebody else's actor now (the restaurant's waiter on shift)
		int Reserved = -1;          // the slot they hold (one at a time: Reserve)
		double DutyBlockedUntil = -1e18;    // a duty with nowhere to be done: not called to it again till then
		std::vector<FDuty> Duties;
		std::string ClubSeat;       // their usual station in the club ("4"); "" none
		std::vector<std::string> Friends;
		mutable std::vector<int> FriendIds;     // Friends as person ids, looked up when first needed (PickSlot's company)
		mutable bool bFriendIds = false;
		std::vector<std::pair<std::string, float>> Likes;   // activity id -> how much more they like it (a painter and the island)
		// their daily ritual (PeopleManners' ApplyLikes, docs/town_manners.md): this activity draws them round this hour
		int RitualAct = -1;
		float RitualHour = 0.f;
		float RitualMore = 0.f;
		// their household's weekend outing (the same ApplyLikes): on this weekday round this hour the whole household is
		// drawn to this activity at this place (the sea front on Sunday at noon, the cafe on Saturday)
		int OutingAct = -1, OutingDay = -1, OutingPlace = -1;
		int HomeWith = -1;          // one of theirs left the outing for home, to this activity: they go along (Think)
		float OutingHour = 0.f;
		// their standing date with a friend (the same ApplyLikes): one weekday a week, round this hour, the two of them
		// meet at this activity at this place (a bench after work on Thursdays, the cafe, the playground after school)
		// A stay ("stay": {cycle, from, days, arrive, leave}): they are in the town only from the hour Arrive of day From of
		// every Cycle days (day 0: the Monday of FWorld::Week 0) till the hour Leave Days days later; the rest of the time
		// they are gone (the activity stay_gone: away by the railway's platform), needing nothing. Cycle 0: always here.
		int StayCycle = 0, StayFrom = 0, StayDays = 0;
		float StayArrive = 11.f, StayLeave = 10.75f;
		int StayWhenId = -1;        // "stay": {"when": ...}: the condition (FWorld::ConditionId) the stay is kept under, -1 always
		bool bStayOffHere = false;  // "off": "here": while that condition is off they are here all the time (else gone)
		int DateAct = -1, DateDay = -1, DatePlace = -1, DateWith = -1;
		float DateHour = 0.f;
		// today (FWorld::RollToday, at midnight and after a load: worked out from the calendar's day and who they are, so
		// nothing of it is saved): what the day is for them, and the one thing planned with somebody. A plan works as a
		// date does (FWorld::MeetNow 3): round PlanHour they are drawn to PlanAct at PlanPlace, where PlanWith is too
		// (a guest: their host; a host: their first guest; a couple: each other)
		uint8_t Today = 0;          // ETodayBit
		float LateH = 0.f;          // slept badly: up this much later (WakeOf)
		uint32_t OnceDone = 0;      // the once-a-day activities done today (FActivity::OnceBit)
		EPlan PlanKind = EPlan::None;
		bool bPlanHost = false;     // the plan is at their home
		int PlanAct = -1, PlanPlace = -1, PlanWith = -1;
		float PlanHour = 0.f;
		// met somebody they know on the way (FWorld::StreetMeets): standing with them for so many seconds more; and not
		// stopping again before MeetAgainAt. Holding a door for whoever comes behind (Move): so many seconds more
		int MeetWith = -1;
		float MeetFor = 0.f;
		double MeetAgainAt = -1e18;
		float DoorFor = 0.f;
		bool bHeld = false;         // held a door on this way already
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
		// Bike: how they go to this point (0 on foot, 1 leading their bicycle, 2 riding it); BikeAct: what they do with it there
		struct FWay { V3 P; uint8_t Kind = 0; float Ride = 0.f; int8_t ToIsland = -1; int RailFrom = -1, RailTo = -1; uint8_t Bike = 0; EBikeAct BikeAct = EBikeAct::None; };
		std::vector<FWay> Path;
		int PathI = 0;
		int BreathAt = -1;          // the old on a long way on foot: past this point of Path they stand to get their breath back
		float BreathFor = 0.f;      // for so many seconds more
		float Think = 0.f;
		float Recheck = 60.f;       // s till they next look up from what they do
		bool bBreak = false;        // wants a break from a duty (to eat)
		bool bOnBreak = false;      // on it: the duty waits till this activity ends
		bool bRideExternal = false; // the ride is the cable car's (RideHook took it): it ends with EndRide
		bool bTrain = false;        // waiting for or riding the train
		int RailFrom = -1, RailTo = -1;     // the train ride's halts (places)
		float SweepT = -1.f;        // clearing a stretch of snow: how far along 0..1 (-1 not at it)
		float SweepSent = -1.f;     // how far the winter was last told
		double DoorsAt = 0.0;       // waiting: when the doors open (sim seconds)
		double RideUntil = 0.0;     // riding the train: when it opens its doors at the other halt
		// their bicycle (FWorld::Bikes) while they have it or go to take it, -1 none; 1 leading it by the bars, 2 riding it;
		// what they do with it now (a stop on the way: getting on, off, parking) and from when till when (sim s)
		int Bike = -1;
		uint8_t BikeMode = 0;
		EBikeAct BikeAct = EBikeAct::None;
		double BikeActStart = 0.0, BikeActUntil = 0.0;
		uint32_t Rng = 1;
		// off the walk grid on a place's own ground (the rink's ice, the ice hole's water): the place, -1 not; the way out
		// goes back by the nearest of its slots' Reach (Route)
		int OffGrid = -1;
		// round a place's loop (an activity with "loop"): how far along it they are (m; < 0 standing at their slot), how far
		// to go before looking for a slot to stop at (or, stopping, to the slot), their glide (m/s), how far they've come
		// since setting off, stopping at their slot now, and who of theirs they skate beside (-1 nobody)
		float LoopAt = -1.f, LoopLeft = 0.f, LoopGo = 0.f, LoopRun = 0.f;
		bool bLoopStop = false;
		int SkateWith = -1;
		int Dips = 0;               // in the water how many times on this visit (an activity with "dip")
		int PrevActivity = -1;      // what they did before this (the manners: a bag carried from the shop), -1 nothing yet
		double PrevActivityEnd = -1e18;     // and when they left it (sim seconds)
		float HoldFor = 0.f;        // stopped on their way for a word with the hero (FWorld::HoldWalk): s left to stand
		std::string Why;            // a short reason for the last choice (debug)
		// a member of a group with a programme (FMembership, in their order: the first group with one set wins), their
		// place in it (the school's slot: kids_3, coach_head), who they talk as (a dialogue id over Body: Sail_bosun), their
		// staff role for StaffOnShift (coach, bosun: "" none); the group whose programme they are at now (-1 none)
		std::vector<FMembership> Member;
		std::string MemberSlot;
		std::string TalkAs;
		std::string StaffRole;
		int Program = -1;
		int Choices = 0;
	};

	// StreetMeet: two who know each other stop on the way for a word (Other: whom; once, by the one with the lower id);
	// StreetNod: they only greet in passing; DoorHold: Person holds a door for Other, who comes behind
	enum class EEvent : uint8_t { Start, Arrive, Leave, TalkStart, TalkEnd, Ride, Away, Back, StreetMeet, StreetNod, DoorHold, Num };
	/** How people spend a gathering (a holiday's phase): in groups talking, facing a point (a stage, fireworks), seated, about the stalls. */
	enum class EGatherMode : uint8_t { Mingle, Watch, Sit, Fair };
	const char* EventName(EEvent E);
	struct FEvent { EEvent Kind; int Person; int Other; int Place; };

	struct FOptions
	{
		uint32_t Seed = 20261020;
		float ThinkEvery = 2.f;         // s between decisions of an idle person
		int SettleBatch = 0;            // >0: Resettle places the townsfolk this many a Step (the game: no long frame), 0 at once
		float RideSeconds = 150.f;      // the cable car from station to station
		float TrainSeconds = 280.f;     // the train between the halts when there is no timetable (TrainNext unset or -1): hidden
		float WalkCostPerHour = 1.6f;   // how much an hour of walking puts people off a choice
		float Jitter = 0.15f;
		float OwnSpots = 110.f;         // m: how far out of their way people go for a spot of their own (their bench, their cafe); 0 none
		float PullMeters = 160.f;       // m: how far out of their way a weather pull of 1 takes people (the cafe in the rain)
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
		int Week = 0;                   // the calendar's week (Mondays counted from any fixed day): who is here on a stay (FPerson::StayCycle)
		float Sunrise = 7.5f, Sunset = 17.f;
		float DutySunrise = -1.f, DutySunset = -1.f;   // the sun the sun-tied duties were last set by (SyncSunDuties)
		float SunYaw = 225.f;           // where the sun sets/stands, yaw (deg) in the UE frame
		float SunriseYaw = 45.f;        // where it comes up (the early risers face it)
		bool bSunUp = true;
		bool bRain = false;             // raining: people outdoors hurry (the wrapper sets it from the weather)
		// the weather people choose by (the wrapper sets it; docs/town_manners.md): in the rain what they chose to do out of
		// doors is left for a roof and not chosen anew, the frost empties the benches, a warm day draws to the water
		float Rain = 0.f;               // 0 dry .. 1 pouring
		float TempC = 12.f;
		float Wind = 5.f;               // the mean wind, m/s (the wrapper sets it from ULyudWindSubsystem; kites want 3..13)
		float Gloom = 0.f;              // 0 a bright sky .. 1 a dark day (thick cloud, rain): lamps go on earlier (FlatLitNow)
		bool bDayOff = false;           // a public holiday: duties with holiday "off" are skipped (SetDayOff)

		/** A gathering (ULyudFestivalSubsystem's holidays): people free and awake are drawn to a place, or to the places
		 *  of a kind, from From till To (hours, To < From over midnight) by Pull (0..2), more the more social they are. */
		struct FGathering
		{
			std::string Id;
			std::string PlaceOrKind;
			std::vector<int> Places;
			float From = 0.f, To = 24.f, Pull = 1.f;
			EGatherMode Mode = EGatherMode::Mingle;
			bool bFace = false;
			V3 Face;
		};
		std::vector<FGathering> Gatherings;
		void SetGathering(const std::string& Id, const std::string& PlaceOrKind, float From, float To, float Pull, EGatherMode Mode, bool bFace, const V3& Face);
		void ClearGathering(const std::string& Id);
		/** The gathering on now at a place, or null. */
		const FGathering* GatheringAt(int Place) const;

		/** Conditions the game switches (UPeopleLifeSubsystem::SetCondition: rink, rink_busy, walrus, banya): an activity with
		 *  "when" is chosen only while its condition is on, and left within a minute after it goes off. Kept over clock jumps
		 *  (Resettle); not saved (the game sends them again every few seconds). */
		void SetCondition(const std::string& Name, bool bOn);
		bool IsCondition(const std::string& Name) const;
		/** A condition's id (interned on first sight). */
		int ConditionId(const std::string& Name);
		bool ConditionOk(const FActivity& A) const { return A.WhenId < 0 || (A.WhenId < (int)CondOn.size() && CondOn[(size_t)A.WhenId] != 0); }
		std::vector<std::string> CondNames;
		std::vector<uint8_t> CondOn;

		/** Groups that keep a programme (the sailing school's kids, juniors, adults, race_<class>, staff, ...: sailing_members.json).
		 *  SetGroupActivity(Group, Activity) sets what the group does now ("" nothing): its members do it as a duty (before their
		 *  other duties, after sleep), turned by their role's map; when it changes they go on to the next, when it ends they
		 *  leave. A group's id is interned on first sight. */
		bool LoadMembers(const std::string& Text);
		int GroupId(const std::string& Name);
		int FindGroup(const std::string& Name) const;
		void SetGroupActivity(const std::string& Group, const std::string& Activity);
		/** Clears every group's activity (before a new set). */
		void ClearGroupActivities() { std::fill(GroupAct.begin(), GroupAct.end(), -1); }
		/** What P's programme asks of them now (an activity index, -1 none), and from which group. */
		int ProgramNow(const FPerson& P, int* OutGroup = nullptr) const;
		/** Whether P belongs to a group. */
		bool IsMember(const FPerson& P, int Group) const;
		std::vector<std::string> GroupNames;
		std::vector<int> GroupAct;          // per group: the activity now, -1 none
		std::vector<std::string> RoleNames;
		/** Per role: (activity, the role's own activity) pairs, as names (sailing_members.json "roles") and indices (Index). */
		std::vector<std::vector<std::pair<std::string, std::string>>> RoleMapNames;
		std::vector<std::vector<std::pair<int, int>>> RoleMap;
		std::vector<int> MemberIds;         // people with a membership

		/** Extra outdoor and indoor spots in town_plan.json's "spots" format (Content/PeopleData/winter_spots.json, written by
		 *  tools/winter_play/winter_play.py: the rink, the banya, the ice hole), loaded as town_life.py loads town_plan.json's:
		 *  one place a spot, its slots; "indoor" makes it a shell (whoever is in is inside, not drawn, at its door); a closed
		 *  "path" is the place's Loop and every slot is reached from the kerb outside it; slots below the spot (in the water)
		 *  are reached from the nearest slot that isn't. Load reads it when it is there. */
		bool LoadSpots(const std::string& Text);

		/** Adds the town's places (town_places.json) and its people (townsfolk.json); after Load. */
		bool LoadTownPlaces(const std::string& Text);
		bool LoadTownsfolk(const std::string& Text);
		/** After loading: activities' place lists, duties' workplaces. Load and the town loaders call it. */
		void Index();
		/** A place under a roof: indoors and not an open yard (FPlace::bOpen). */
		bool Roofed(int Place) const { return Places[(size_t)Place].bIndoor && !Places[(size_t)Place].bOpen; }
		/** Somebody inside a shell (a town building): not drawn. */
		bool InsideShell(const FPerson& P) const;
		/** Who works at a place now (in their role, "" any), on their shift and there. */
		void StaffOnShift(const std::string& PlaceId, const std::string& Role, std::vector<int>& Out) const;
		/** Window lights: whether each flat with windows is lit (somebody home and awake after dusk); the flats that changed
		 *  since the last call are put in Changed (all of them on the first call). */
		void UpdateLights(std::vector<int>& Changed);
		std::vector<int8_t> FlatLit;    // per place: -1 unknown, 0 dark, 1 lit
		bool FlatLitNow(int Place) const;
		/** The weather's pull to a place now (weather_pull.json; 0 none): added to a choice's score, and taken off the
		 *  metres when choosing between places (PullMeters a unit). */
		float WeatherPull(int Place) const;
		std::vector<FWeatherPull> Pulls;
		/** weather_pull.json: the rows, then each place its row (the first that names its id, else its kind). */
		bool LoadWeatherPull(const std::string& Text);
		void IndexPulls();
		/** Today's wake and bedtime (their usual, moved by the day's roll). */
		float WakeOf(const FPerson& P) const
		{
			const float W = std::fmod(P.Wake + P.WakeJ + P.LateH + 24.f, 24.f);     // (LateH: a bad night, RollToday)
			// the morning after a festival night: up as much later as they stayed up (4 h at most)
			const float Late = StayLate(P);
			return Late > 0.f ? std::fmod(W + Late, 24.f) : W;
		}
		float BedOf(const FPerson& P) const
		{
			const float B = std::fmod(P.BedTime + P.BedJ + 24.f, 24.f);
			return Time < StayUntil && StayLate(P) > 0.f ? StayHourFor(P) : B;
		}
		/** A festival night (SetStayUp): nobody but the small children goes to bed before StayUntil (sim seconds). */
		void SetStayUp(float UntilHour)
		{
			// UntilHour from today's 00:00 (26.5: 02:30 tonight); 0: the night is over (the morning after still starts late)
			if (UntilHour > 0.f)
			{
				StayUntil = StayEnd = Time + ((double)UntilHour - (double)Hour) * 3600.0;
				StayHour = std::fmod(UntilHour, 24.f);
			}
			else
			{
				StayUntil = 0.0;
				if (StayEnd > Time)
				{
					StayHour = std::fmod(StayHour - (float)((StayEnd - Time) / 3600.0) + 48.f, 24.f);
					StayEnd = Time;
				}
			}
		}
		double StayUntil = 0.0, StayEnd = -1e18;
		float StayHour = 0.f;
		float StayHourFor(const FPerson& P) const
		{
			const float S = StayHour;
			return P.Age < 12 ? (NightH(S) > 23.f ? 23.f : S) : S;
		}
		/** hours past their own bedtime the festival keeps them (0: none, or it's long over) */
		float StayLate(const FPerson& P) const
		{
			if (Time > StayEnd + 14.0 * 3600.0 || Time < StayEnd - 20.0 * 3600.0)
			{
				return 0.f;
			}
			const float D = NightH(StayHourFor(P)) - NightH(std::fmod(P.BedTime + P.BedJ + 24.f, 24.f));
			return D > 0.f ? (D > 4.f ? 4.f : D) : 0.f;
		}
		static float NightH(float H) { return H < 12.f ? H + 24.f : H; }
		/** Rolls everybody's day (wake and bed moved by Spread); the step does it at 14:00 each day. */
		void RollDays();
		/** 1: A is P's household outing and it is on now (from half an hour before its hour to an hour and a quarter after);
		 *  2: A is P's date with a friend and it is on now; 0: neither. */
		int MeetNow(const FPerson& P, const FActivity& A) const
		{
			const int Ai = (int)(&A - Activities.data());
			if (P.OutingAct == Ai && Weekday == P.OutingDay && Hour > P.OutingHour - 0.5f && Hour < P.OutingHour + 1.25f)
			{
				return 1;
			}
			if (P.DateAct == Ai && Weekday == P.DateDay && Hour > P.DateHour - 0.5f && Hour < P.DateHour + 1.25f)
			{
				return 2;
			}
			return P.PlanAct == Ai && Hour > P.PlanHour - 0.5f && Hour < P.PlanHour + 1.25f ? 3 : 0;
		}

		// --- the days (PeopleDays.cpp, docs/town_life.md "The days") ---
		/** The calendar's day (Mondays counted as Week counts them). */
		int DayIndex() const { return Week * 7 + Weekday; }
		/** Works out everybody's day when the calendar's day changes (Step, Resettle): colds, bad nights, birthdays, the
		 *  pay and the pension, and the day's plans (tea at a friend's, a birthday's guests, a couple's evening stroll,
		 *  chess on a bench, Sunday at the grandparents'). The same for the same day and seed; bForce: again anyway. */
		void RollToday(bool bForce = false);
		int TodayDay = INT_MIN;           // the day it was last worked out for
		bool bDays = true;                  // false: no days (the tests of what was there before: every day the same)
		/** Their own weekday for an activity with "own_day" (of its days), the same every week. */
		int OwnDayOf(const FPerson& P, const FActivity& A) const;
		/** How much they feel like an activity today (-0.5 .. 0.5; the same all day, another tomorrow). */
		float DayMood(const FPerson& P, const FActivity& A) const;
		/** Whether they'd be at a duty or asleep at this hour of today (their usual hours). */
		bool BusyAt(const FPerson& P, float AtHour, float ForHours) const;
		/** People on their way who know each other and pass within a few metres stop for a word, or greet (every few
		 *  seconds, over those out walking only). */
		void StreetMeets();
		float MeetTimer = 0.f;
		int Meets = 0, Nods = 0, Holds = 0;    // counts for the tests
		/** At a door: somebody bound for the same door a few steps behind has it held for them. */
		void HoldDoor(FPerson& P);
		/** Developer mode and the tests: gives somebody a day (bits of ETodayBit) till the next roll. */
		void SetToday(int Person, uint8_t Bits);
		/** ... and a plan of Kind with With (a visit: P goes to With's). False if it can't be made. */
		bool MakePlan(int Person, int With, EPlan Kind, float AtHour);
		std::string DescribeToday(int Person) const;
	private:
		int PlanActs[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };   // Index: visit_home, host_guests, couple_stroll, chess_bench
		void ClearPlan(FPerson& P);
		int NearestPlaceFor(const FActivity& A, const V3& Near, int Island, int Room) const;
	public:
		bool OutingNow(const FPerson& P, const FActivity& A) const { return MeetNow(P, A) == 1; }
		float PrevHour = -1.f;
		int DayRoll = 0;
		V3 Camera;

		/** Load the town too (town_places.json, townsfolk.json in Dir) when they are there. */
		bool bLoadTown = true;
		/** activities.json, people.json, people_places.json from Dir; the walk grid from AnimalsDir ("" = no grid). */
		bool Load(const std::string& Dir, const std::string& AnimalsDir);
		/** Append: add the rows to those there (weather_pull.json's own activities), else replace them. */
		bool LoadActivities(const std::string& Text, bool bAppend = false);
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
		/** Moves the sun-tied duties' hours to today's Sunrise / Sunset (cheap when they have not changed; Step, Resettle and
		 *  NewGame call it). */
		void SyncSunDuties();
		/** The hero stands at P (Unreal metres; bOn false: nowhere near a counter): a queue spot under him counts as taken, so
		 *  the townsfolk line up behind him (docs/counters.md). */
		void SetHeroAt(const V3& P, bool bOn);
		/** A counter's lines (Slots indices, front first), and how often somebody moved up in one (checks). */
		std::vector<std::vector<int>> Queues;
		std::vector<std::string> QueueIds;   // their ids ("W1-U02/0", "P-MARKET/3")
		int QueueMoves = 0;
		/** Who stands in a line now, front first (-1 a free spot, -2 the hero): checks and the engine's counter scene. */
		void QueueState(int Queue, std::vector<int>& Out) const;
		/** Sends up to Count of the townsfolk who are free and near to stand in a line (developer mode, shots); how many went. */
		int SendToQueue(int Queue, int Count);
		/** The line whose front spot is nearest P within MaxM metres (-1 none). */
		int NearestQueue(const V3& P, float MaxM) const;

		int FindPerson(const std::string& Key) const;
		int FindPlace(const std::string& Id) const;
		int FindActivity(const std::string& Id) const;
		int ActOf(const FDuty& D) const { return D.Act >= 0 ? D.Act : FindActivity(D.Activity); }
		int SleepAct = -1, RestHomeAct = -1;   // Index()
		int GoneAct = -1;               // stay_gone (Index())
		/** Days since the midnight their stay's day 0 began, in [0, StayCycle) (people with a stay only). */
		float StayAt(const FPerson& P) const;
		/** Somebody with a stay who is not in the town now. */
		bool Gone(const FPerson& P) const;
		/** The day of their stay today (0 the day they come), -1 with no stay. */
		int StayDayOf(const FPerson& P) const { return P.StayCycle > 0 ? (int)StayAt(P) : -1; }
		int MindAct = -1, PlayActs[2] = { -1, -1 };     // mind_child; playground, play_run (Index())
		/** The household's child (under 12) out at play whom P could keep an eye on: nobody else of the household does
		 *  already. -1 none. */
		int ChildToMind(const FPerson& P) const;
		/** The box of the slots of the places on the far islands (4 and beyond: the motor ship), with 30 m around; empty
		 *  (bFarBox false) without such places (Index()). */
		bool bFarBox = false;
		float FarX0 = 0.f, FarY0 = 0.f, FarX1 = 0.f, FarY1 = 0.f;
		/** A point aboard the motor ship (in the far islands' box): no walk grid there, the way is straight
		 *  (OutdoorWay). Always false in a world without the ship. */
		bool OffWalkGrid(const V3& P) const
		{
			return bFarBox && P.X >= FarX0 && P.X <= FarX1 && P.Y >= FarY0 && P.Y <= FarY1;
		}
		/** A route between two points (each indoors at a place's slot or outdoors); false = no way. */
		bool Route(const FPerson& P, const V3& From, int FromSlot, int ToSlot, std::vector<FPerson::FWay>& Out) const;
		/** Where they are drawn and which way they face (between steps it is Pos). */
		std::string Report() const;
		std::string Describe(int Id) const;
		/** What each activity would score for them now (debug). */
		std::string Scores(int Id) const;
		/** Forces an activity now (tests, console): goes there and does it. */
		bool Order(int Person, const std::string& Activity, const std::string& PlaceId = std::string());
		/** A friend comes to sit beside the player (docs/friends.md): the free seat nearest Pos (metres) within MaxDist that
		 *  some sitting activity of theirs fits, for Minutes; false if there is none. The seat the player is on is left out
		 *  (nearer than 0.35 m to Pos). */
		bool OrderBeside(int Person, const V3& Pos, float MaxDist, double Minutes);
		/** Somebody on their way on foot stands for Seconds more (a talk with the hero, docs/town_talk.md), then goes on;
		 *  0 lets them go at once. Nothing for whoever isn't walking a way. */
		void HoldWalk(int Person, float Seconds);

		/** Bicycles (docs/bikes.md): a household's bike lives in its entrance (a place on the stair's way in, Home) and
		 *  goes where its rider leaves it. People ride it for an outdoor way of MinBikeWay or more on the valley's ground
		 *  when it stands at home (their way out passes it) or near where they set off. */
		struct FBike
		{
			std::string Id;
			std::string Stair;          // the entrance it lives in ("" none: the player's bikes)
			std::string Owner;          // the household that rides it ("" nobody)
			V3 Home;                    // its place in the entrance, on the stair's way in
			float HomeYaw = 0.f;
			V3 Pos;                     // where it stands now
			float Yaw = 0.f;
			int Rider = -1;             // who has it or goes to take it
			bool bHeld = false;         // the player has it
		};
		std::vector<FBike> Bikes;
		float MinBikeWay = 110.f;       // m outdoors
		float BikeSpeed = 4.4f;         // m/s riding (over 60: 3.6)
		/** A stair's way in: its id, the street door's outside, the lobby inside it (a point people pass). */
		struct FLobby { std::string Stair; V3 Street, Lobby; };
		std::vector<FLobby> Lobbies;
		/** Adds a bike standing at Pos (its home, in Stair's entrance when Stair isn't empty); its index. */
		int AddBike(const std::string& Id, const std::string& Stair, const V3& Pos, float Yaw);
		/** Gives each entrance's bike to a household living up that stair (adults under 77), one bike a household. */
		void AssignBikes();
		/** The player takes a bike (it isn't ridden by its owner while held) or leaves it at Pos. */
		void SetBikeHeld(int Bike, bool bHeld, const V3& Pos, float Yaw);

		/** The cable car (ULyudTransportSubsystem): called when someone gets to the station to ride from From to the station by To
		 *  (both Unreal metres); true if a real cabin will carry them, and then the ride ends only with EndRide (or after an hour,
		 *  should the cable car lose them). Unset or false: the ride takes Opt.RideSeconds, hidden, as before. */
		std::function<bool(int Person, const V3& From, const V3& To)> RideHook;
		/** Their cable-car ride is over: out at the other station, on along their way. */
		void EndRide(int Person);

		/** The sea railway's timetable (ULyudRailwaySubsystem in the game, LyudRail::FRun in tools/peoplesim), in sim seconds:
		 *  Doors = the next moment at or after Time a train going from stop From towards stop To (ids: beach, island) opens
		 *  its doors at From; false if none (no railway: the ride is then TrainSeconds, hidden). Sim seconds can be negative
		 *  (the game's life clock counts from the start date), so no sentinel value. */
		std::function<bool(const std::string& From, const std::string& To, double Time, double& Doors)> TrainNext;
		/** Seconds from the doors opening at From to their opening at To. */
		std::function<double(const std::string& From, const std::string& To)> TrainRide;
		/** The game's own positions of a halt (its stair's foot on the ground and its boarding points on the deck, Unreal
		 *  metres) over the data's guesses; false if there is no such halt. */
		bool SetRailStop(const std::string& Stop, const V3& Foot, const std::vector<V3>& Boarding);

		/** A stretch of path that wants its snow cleared (ULyudWinterSubsystem's patches): an id, its ends (Unreal metres), the snow, cm. */
		struct FSnowJob { int Job = -1; V3 From, To; float Cm = 0.f; };
		/** The winter's paths: up to Max stretches that want clearing, nearest to Near first. Unset: no snow work. */
		std::function<void(const V3& Near, int Max, std::vector<FSnowJob>& Out)> SnowJobs;
		/** Someone clearing a stretch: Progress 0 takes it (false: somebody else has it), then how far 0..1 (1 done), < 0 gives it up. */
		std::function<bool(int Person, int Job, float Progress)> SnowProgress;
		/** Hands the snow place's free slots the stretches that want clearing now, near Near (the wrapper calls it every few seconds). */
		void UpdateSnowSlots(const V3& Near);
		/** Somebody clearing a stretch stops for good or bad (called away, resettled, the life stopped): the winter gets the
		 *  stretch back, or it would stay held and nobody would be given it again. */
		void DropSweep(FPerson& P);

		float Ground_Z(const V3& P) const;
		/** Ground under a point: the island's own height field where it has one (people_places.json ground_patches), else
		 *  the walk grid's; P.Z without either. */
		float GroundAt(const V3& P) const;
		int IslandOf(const V3& P) const;
		/** Seconds of the way from From (on island FromIsland) to slot To leaving at At: walks, rides, waits for the train. */
		float WaySeconds(const FPerson& P, const V3& From, int FromIsland, int ToSlot, double At) const;
		/** A halt of the sea railway (place index) on an island, -1 none. */
		int RailStopOf(int Island) const;
		/** The trail (place index) from the valley to an island reached on foot (the North), -1 none. */
		int TrailOf(int Island) const;

		/** Height fields of ground off the walk grid (the sea island), Unreal metres. */
		struct FPatch { float X0 = 0.f, Y0 = 0.f, Cell = 4.f; int NX = 0, NY = 0; std::vector<float> Z; bool Has(float X, float Y) const; float At(float X, float Y) const; };
		std::vector<FPatch> Patches;
		void AddPatches(const LyudJson::FValue& List);     // a json "ground_patches" list onto Patches

		/** The town's terraces (town_places.json "town"): flat levels joined by stairs. Inside its box people keep to their
		 *  terrace's height and go up and down by the stairs (until the walk grid knows the terraces). */
		struct FTownGround
		{
			bool bValid = false;
			float X0 = 0.f, Y0 = 0.f, X1 = 0.f, Y1 = 0.f;
			std::vector<float> Levels;
			struct FStair { V3 Bottom, Top; int Lo = 0, Hi = 0; };
			std::vector<FStair> Stairs;
			bool Inside(const V3& P) const { return bValid && P.X >= X0 && P.X <= X1 && P.Y >= Y0 && P.Y <= Y1; }
			int LevelOf(float Z) const;
		};
		FTownGround Town;
		/** A way between two points of the town: along the terrace, and over the stairs between terraces. */
		void TownWay(const V3& From, const V3& To, std::vector<FPerson::FWay>& Out) const;

	private:
		int StationOf(int Island) const;
		void ReserveOwnSlots();
		void UpdateNeeds(FPerson& P, float Dt);
		void Think(FPerson& P, bool bSnap = false);
		bool Interrupt(FPerson& P, float Dt);
		void Move(FPerson& P, float Dt);
		/** Their pace now, m/s: a walk, or a run outdoors when late or in the rain (a brisk walk indoors). */
		float Pace(const FPerson& P) const;
		/** On the way to the household's outing, or home with one of theirs: 1, or how much slower or faster to go to keep with their own. */
		float WayTogether(const FPerson& P, bool* bWith = nullptr) const;     // bWith: one of theirs is going along
		/** Their pace within an activity (a walk's next spot): the activity's pace on their walk. */
		float ActPace(const FPerson& P, const FActivity& A) const;
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
		bool AddPeople(const LyudJson::FValue& J, bool bTown);
		/** Their duty's own workplace for an activity (-1 none) and role. */
		const FDuty* DutyFor(const FPerson& P, const FActivity& A) const;
		size_t Indexed = 0;
		/** FindPlace's map: the first PlaceByIdN places by id (filled as it is asked). */
		mutable std::unordered_map<std::string, int> PlaceById;
		mutable size_t PlaceByIdN = 0;
		/** Load is reading its parts: they leave the Index to it. */
		bool bLoading = false;
		std::vector<std::string> KindNames;     // interned kinds: 0 bed, 1 sofa, 2 flat, 3 snow
		int KindId(const std::string& Kind);        // the places Index saw (a place added after it: the slow look over all)
		V3 Where(const FPerson& P) const;
		void Reserve(FPerson& P, int Slot);
		/** The hops between two islands: (kind 2 cable car | 3 train | 5 a trail on foot, the place to set off from, the place to come
		 *  out at, the island). */
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
		/** The lines move: the front served for a minute or two, the rest wait and step up (docs/counters.md). */
		void StepQueues();
		bool QueueOpenFor(int Slot) const;
		/** Per winter patch (FSnowJob::Job): which side its start is on (IslandOf, a path search), -2 not asked yet. A patch never moves. */
		std::vector<int8_t> SnowJobIsland;
		std::vector<int> SettleQueue;   // townsfolk Resettle hasn't placed yet (Opt.SettleBatch)
		size_t SettleI = 0;
	public:
		bool Settling() const { return SettleI < SettleQueue.size(); }
	private:
		void Wander(FPerson& P);
		/** Round the place's loop (an activity with "loop"): a stretch of gliding, then a stop at a free slot ahead, a pause. */
		void Skate(FPerson& P, float Dt);
		/** An activity with "dip": in the water so long, out onto the deck, maybe in again. */
		void Dip(FPerson& P);
		/** Whether P is one of an activity's regulars (its "regulars"; everybody when it has none). */
		bool IsRegular(const FPerson& P, const FActivity& A) const;
		/** The pose at one of an activity's slots (a dip's: in the water at the first kind, standing at the others). */
		EPose PoseAt(const FActivity& A, int Slot) const;
		/** Along a place's loop: the distance to the nearest point of it, and the point and its heading at a distance. */
		float LoopArc(const FPlace& Pl, const V3& At) const;
		V3 LoopPoint(const FPlace& Pl, float S, float* OutYaw = nullptr) const;
		/** Clearing a snow stretch: along it from its near end to its far end at the pusher's pace, then on to the next. */
		void Sweep(FPerson& P, float Dt);
		void OutdoorWay(const V3& From, const V3& To, std::vector<FPerson::FWay>& Out) const;
		void WayToSlot(const V3& From, const FSlot& To, std::vector<FPerson::FWay>& Out) const;
		/** Rewrites a fresh way to go part of it by bike (theirs, if it suits). */
		void PlanBike(FPerson& P);
		/** At a stop of the way with something to do with the bike. */
		void BikeStop(FPerson& P, EBikeAct Act);
		/** Whatever they have in hand is left standing where they are. */
		void DropBike(FPerson& P);
		float Rand(uint32_t& S) const;
		void Emit(EEvent E, const FPerson& P, int Other = -1) { Events.push_back({ E, P.Id, Other, P.Place }); }
	};
}
