// The townsfolk's characters and manners, in plain C++ (no Unreal types) so tools/mannersim can run them on the town's
// people outside the engine (docs/town_manners.md).
//
// The people's life (PeopleSim.h) says where somebody is and what they do: sits on a bench, walks the promenade, looks at
// the sea. This says HOW they do it. Everybody has a persona worked out from who they are (the same every game: warm or
// reserved, fidgety or calm, curious, prepared, and habits: smokes, lives in the phone, reads the paper, feeds the
// birds, takes photographs, knits, carries an umbrella, has a sweet tooth, dozes off, left-handed), and whoever is drawn near the camera has a
// manner now: a small thing done with the hands and the head on top of the pose (arms folded, a phone call, a cigarette,
// a look at the watch, a yawn, an umbrella in the rain, a newspaper on the bench, crumbs for the birds, a rod at the
// water, a bag carried home from the shop, a wave to a friend passing by). Which manners there are and when each fits
// is data: Content/PeopleData/manners.json. People also notice things: the player walking past, running at them, standing
// too close, a lamp in their eyes at night, and each other when they pass.
//
// Cost: personas are made once (Bind). Update is called only for the people who have a body now (a few dozen), and
// between picks it is a couple of comparisons; nothing here allocates after Bind.
#pragma once

#include "PeopleSim.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LyudPeople
{
	/** A small thing somebody does on top of their pose. The game's gesture layer (BodyGestures.h) draws each. */
	enum class EManner : uint8_t
	{
		None,
		// empty hands
		FoldArms,       // arms folded on the chest
		HandsHips,      // hands on the hips
		HandsBack,      // hands clasped behind the back (the old walk like that)
		Stretch,        // arms up, a stretch
		Yawn,           // a hand to the mouth
		ScratchHead,
		CheckWatch,     // the wrist up, a look down at it
		LookSky,        // head back, looking up
		ShadeEyes,      // a hand over the eyes against the low sun
		RubHands,       // cold: hands rubbed together, blown on
		HugSelf,        // cold or wet: arms wrapped round themselves
		Wave,           // a raised hand to somebody
		Nod,            // a nod to somebody
		Point,          // points something out to the one they talk to
		Clap,           // at a show
		Sway,           // to the music at a holiday
		Doze,           // nods off on a bench
		CoverHead,      // caught by the rain: an arm over the head
		// something in the hands (FMannerDef::Prop)
		PhoneCall,
		PhoneScroll,
		Smoke,
		Photo,
		FeedBirds,
		Umbrella,
		Fish,
		Paint,
		Binoculars,
		Newspaper,
		Book,           // the book of whoever reads (the life's read pose): only the thing in the hands
		Knit,
		IceCream,
		Drink,
		CarryBag,
		// empty hands again (new ones go at the end: the numbers stay)
		PointSky,       // an arm up at something in the sky, the head back
		Num
	};
	const char* MannerName(EManner M);
	EManner MannerFromName(const std::string& S);   // Num if unknown
	constexpr int NumManners = (int)EManner::Num;

	/** What a manner puts in the hands. */
	enum class EProp : uint8_t
	{
		None, Phone, Cigarette, Camera, Umbrella, Rod, Brush, Binoculars, Newspaper, Book, Knitting, IceCream, Cup, Bag, Num
	};
	const char* PropName(EProp P);
	EProp PropFromName(const std::string& S);       // Num if unknown

	enum EHabit : uint32_t
	{
		HabitSmoker = 1u << 0,
		HabitPhone = 1u << 1,           // lives in the phone
		HabitReader = 1u << 2,          // a paper on the bench
		HabitFeeder = 1u << 3,          // crumbs for the birds
		HabitPhotographer = 1u << 4,
		HabitKnitter = 1u << 5,
		HabitUmbrella = 1u << 6,        // has one with them when it rains
		HabitSweet = 1u << 7,           // an ice cream on a warm day
		HabitDozer = 1u << 8,           // nods off after lunch
		HabitHandsBack = 1u << 9,       // walks with the hands behind the back
	};
	const char* HabitName(uint32_t Bit);
	uint32_t HabitFromName(const std::string& S);   // 0 if unknown

	/** Who somebody is in manner: the same every game (worked out from their key and what the life knows of them). */
	struct FPersona
	{
		float Warmth = 0.5f;        // greets and waves .. keeps to themselves
		float Nerve = 0.5f;         // fidgets, looks at the watch .. calm
		float Curiosity = 0.5f;     // looks at what goes on .. minds their own business
		float Tidy = 0.5f;          // prepared .. careless
		uint32_t Habits = 0;
		bool bLeftHanded = false;
		bool Has(uint32_t Habit) const { return (Habits & Habit) != 0; }
	};
	FPersona MakePersona(const FPerson& P);
	/** warmth, nerve, curiosity, tidy; 0.5 for anything else. */
	float PersonaTrait(const FPersona& P, const std::string& Name);

	/** The moment, set by the caller before the updates of a frame. */
	struct FMannerNow
	{
		float Clock = 0.f;          // play seconds (the manners run in real time, not on the game's calendar)
		float Rain = 0.f;           // 0 dry .. 1 pouring
		float TempC = 12.f;
		float SunAlt = 30.f;        // degrees over the horizon
		float Cloud = 0.3f;         // 0 clear .. 1 overcast
		float SkyShow = 0.f;        // something to see up there: northern lights, a rainbow, fireworks (0 nothing .. 1)
	};

	/** The player, for what people notice. Metres, the people's frame. */
	struct FHeroNow
	{
		bool bThere = false;        // in the world as a person on foot (not hidden, not a bird, not seated at a computer)
		V3 Pos;
		V3 Vel;
		bool bLamp = false;         // the head lamp is on
		bool bCrouch = false;
		bool bBike = false;         // riding a bicycle
		bool bFishing = false;      // his line is out
		bool bSwim = false;         // in the water
		bool bSeated = false;       // sitting (a bench, a chair)
	};

	/** What somebody just noticed (the game may say a line for it: town_barks.txt on = ...). */
	enum class ENoticeEvent : uint8_t { None, HeroRuns, HeroTooClose, HeroLamp, HeroSneaks, HeroRides, HeroFishes, HeroSwims, HeroSits, Num };
	const char* NoticeEventName(ENoticeEvent E);

	/** A row of manners.json. */
	struct FMannerDef
	{
		EManner Manner = EManner::None;
		EProp Prop = EProp::None;
		uint32_t Poses = 0;             // bit per EPose it goes with
		std::vector<std::string> Doing, NotDoing;   // activities.json ids (empty: any)
		std::vector<std::string> At;    // place kinds (empty: anywhere)
		std::vector<uint8_t> DoingOk;   // per activity, built by Bind (empty: any)
		std::vector<std::string> After; // only for a while after one of these activities (a bag after the shop)
		std::vector<uint8_t> AfterOk;
		float AfterSeconds = 900.f;
		uint32_t Habit = 0;             // only people with this habit
		int MinAge = 0, MaxAge = 200;
		float From = 0.f, To = 24.f;    // game hours
		int8_t Outdoors = -1;           // 1 only outdoors, 0 only indoors, -1 either
		int8_t Rain = -1;               // 1 only in the rain, 0 only out of it (indoors counts as out of it), -1 either
		int8_t SunUp = -1;              // 1 by day, 0 by night
		bool bSunLow = false;           // the sun low and not behind clouds
		bool bClear = false;            // not overcast
		bool bSkyShow = false;          // only while there is something to see in the sky (FMannerNow::SkyShow); likelier the more of it
		float TempBelow = 1e9f, TempAbove = -1e9f;
		int Need = -1;                  // ENeed over NeedOver
		float NeedOver = 0.f;
		int Gather = -1;                // EGatherMode: only at a gathering of this mode
		bool bWaiting = false;          // only waiting for the train
		std::string Trait;              // warmth, nerve, curiosity, tidy: the more of it, the likelier
		float Weight = 1.f;
		float SecMin = 6.f, SecMax = 15.f;
		float Cooldown = 60.f;          // s before the same person does it again
		bool bForce = false;            // while it fits it is what they do (an umbrella in the rain, the rod while fishing)
		bool bReactOnly = false;        // never picked: only started by React (a wave)
	};

	/** Somebody's manner now. */
	struct FMannerState
	{
		EManner Cur = EManner::None;
		int Def = -1;                   // Cur's row, -1 none (None, or a reaction without a row)
		float Start = 0.f, Until = 0.f; // clock seconds
		bool bReact = false;            // started by React: runs out by itself whatever the pose
		float CheckAt = 0.f;            // when it is next looked at whole (once a second)
		// what they look at: -2 nothing in particular, -1 the player, >= 0 a person
		int LookAt = -2;
		float LookUntil = 0.f;
		float LookCool = 0.f;           // no new look before this
		float NearSince = -1.f;         // the player within arm's reach since (clock), -1 not
		float EventCool = 0.f;
		ENoticeEvent Event = ENoticeEvent::None;    // set by Notice, taken by TakeEvent
		bool bTurn = false;             // turns the body toward what they look at (children watching)
		uint32_t Rng = 1;
		float Last[NumManners];         // when each manner last ended
	};

	class FManners
	{
	public:
		std::vector<FMannerDef> Defs;
		std::vector<FPersona> Personas;
		std::vector<FMannerState> States;
		float IdleWeight = 3.f;         // how often people do nothing in particular, against the manners' weights
		std::string LastError;
		std::vector<std::string> Warnings;
		/** Bit per EProp the game can draw; a manner whose prop isn't there is left out. */
		uint32_t PropsReady = ~0u;
		/** Tests and the console: everybody does this one (Num: off). */
		EManner Forced = EManner::Num;

		/** Reads manners.json; false if it holds no manners. */
		bool Load(const std::string& JsonText);
		/** After the world is loaded: a persona and a state for everybody, the rows' activities looked up. */
		void Bind(const FWorld& W);
		/** Habits draw people to what suits them (a feeder to the birds, a reader to a bench with a book): added to their
		 *  likes, once, after Bind. */
		void ApplyLikes(FWorld& W) const;
		bool Bound() const { return !States.empty(); }

		/** Somebody drawn now: keeps or picks their manner. */
		void Update(const FWorld& W, int Person, const FMannerNow& Now);
		/** Their body went: the manner ends (its cooldown starts), looks are forgotten. */
		void Release(int Person, float Clock);
		/** Starts a manner by itself (a wave, a nod, a clap) for Seconds, over whatever they were doing. A manner with
		 *  something in the hands is not interrupted (nobody waves a newspaper), unless bEvenBusy. */
		bool React(int Person, EManner M, float Clock, float Seconds, bool bEvenBusy = false);
		/** What somebody drawn notices of the player: sets what they look at; an event now and then (TakeEvent). */
		void Notice(const FWorld& W, int Person, const FMannerNow& Now, const FHeroNow& Hero);
		/** People drawn who pass each other: a glance; friends wave or nod. Shown: person ids. A couple of times a second. */
		void Passing(const FWorld& W, const int* Shown, int NumShown, const FMannerNow& Now);
		ENoticeEvent TakeEvent(int Person);

		const FMannerState* StateOf(int Person) const { return Person >= 0 && Person < (int)States.size() ? &States[Person] : nullptr; }
		const FPersona* PersonaOf(int Person) const { return Person >= 0 && Person < (int)Personas.size() ? &Personas[Person] : nullptr; }
		const FMannerDef* DefOf(const FMannerState& S) const { return S.Def >= 0 && S.Def < (int)Defs.size() ? &Defs[S.Def] : nullptr; }
		EProp PropOf(const FMannerState& S) const { const FMannerDef* D = DefOf(S); return D ? D->Prop : EProp::None; }
		/** Whether a row fits somebody now (its weight; 0 if not). Public for the tests. */
		float Fits(const FWorld& W, const FPerson& P, const FPersona& Who, const FMannerState& S, const FMannerDef& D, const FMannerNow& Now) const;
		/** One line: who they are (console, tests). */
		std::string Describe(const FWorld& W, int Person) const;

	private:
		void End(FMannerState& S, float Clock);
		void Pick(const FWorld& W, const FPerson& P, FMannerState& S, const FMannerNow& Now);
		static float Rand(uint32_t& S);
		std::vector<int> Cand;          // Pick's scratch
		std::vector<float> CandW;
		int RowOf[NumManners] = {};     // a manner's first row, -1 none
	};
}
