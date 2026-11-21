// Children's games in the yards (docs/yard_games.md, interactives backlog item 32): the town's children play together
// after school. Hopscotch on chalked squares, a long skipping rope, hide-and-seek behind the yard's own things, football
// with a ball that rolls, tag. A small group behaviour on top of the people's life: the children whose life has them at
// play near a play area (Content/PeopleData/yard_games.json, tools/yard/yard_areas.py) are offered to it; when enough of
// them are there one of them (the leader, the oldest) starts a game the area fits, gives out the roles, and the game
// runs by its rules until its time is up or too few are left; whoever's life calls them away leaves it.
//
// Plain C++ (no engine): ULyudYardGames drives it in the game (bodies, chalk, rope, ball, the hero), tools/yardsim tests
// it on the real town's afternoon. Unreal metres in the XY plane (world cm / 100), yaw Unreal degrees.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LyudYard
{
	struct FV2
	{
		float X = 0.f, Y = 0.f;
		FV2() = default;
		FV2(float InX, float InY) : X(InX), Y(InY) {}
		FV2 operator+(const FV2& O) const { return FV2(X + O.X, Y + O.Y); }
		FV2 operator-(const FV2& O) const { return FV2(X - O.X, Y - O.Y); }
		FV2 operator*(float S) const { return FV2(X * S, Y * S); }
		FV2& operator+=(const FV2& O) { X += O.X; Y += O.Y; return *this; }
		float Len() const;
		float Dot(const FV2& O) const { return X * O.X + Y * O.Y; }
		FV2 Norm() const;
		/** Unit vector of a yaw (deg). */
		static FV2 Dir(float YawDeg);
		/** The yaw (deg) this vector points along. */
		float Yaw() const;
	};

	enum class EGame : uint8_t { Hopscotch, Rope, Hide, Football, Tag, Num };
	const char* GameName(EGame G);
	EGame GameFromName(const std::string& Name);

	enum class EPhase : uint8_t { Gather, Play, Over };

	/** A child's part in the game now. */
	enum class ERole : uint8_t
	{
		None,
		Wait,       // in the queue (hopscotch, the rope), or for the next round (hide-and-seek)
		Hopper,     // hopscotch: on the squares
		Turner,     // the rope: one of the two turning it
		Jumper,     // the rope: jumping in it
		Seeker,     // hide-and-seek: counts at the wall, then looks
		Hider,      // hide-and-seek: hiding
		Found,      // hide-and-seek: found or home free, waits at the wall for the round's end
		Player,     // football: a field player (Team 0 or 1)
		Keeper,     // football: in goal
		It,         // tag: the one who chases
		Runner,     // tag: running from it
	};
	const char* RoleName(ERole R);

	/** What the body does now (ULyudYardGames turns it into a pose and the hands' goals). */
	enum class EAct : uint8_t
	{
		Stand,      // standing by, watching
		Walk,
		Run,
		Hop,        // hopscotch: a hop on one foot (Lift is the height)
		Jump,       // the rope: a jump with both feet (Lift)
		Turn,       // the rope: the turning hand going round (Phase)
		Count,      // hide-and-seek: face to the wall, hands over the eyes
		Crouch,     // hide-and-seek: bent low behind the cover
		Clap,
		Kick,       // football: a kick (ActT since it began)
		Reach,      // tag: an arm out to touch
		Cheer,      // arms up
		Point,      // found you!
		Look,       // the seeker looking about
	};

	enum class EEvent : uint8_t
	{
		Start,      // a game began (Who: the leader)
		Join,       // somebody joined (Who)
		Leave,      // somebody left (Who): their life called them away, or the hero walked off
		Roles,      // the roles were given out (a round began)
		Found,      // hide-and-seek: Other found Who
		Saved,      // hide-and-seek: Who ran home free to the wall before the seeker
		Tagged,     // tag: Other tagged Who (Who is it now)
		Goal,       // football: Who scored (Other: the team)
		Miss,       // hopscotch: Who stepped on a line / the rope: Who tripped
		Swap,       // the rope: Who takes a turn turning instead of Other
		RoundEnd,   // hide-and-seek: the round is over (Who: the next seeker)
		End,        // the game is over (Other: 0 its time was up, 1 too few left, 2 ended from outside)
	};
	const char* EventName(EEvent E);

	/** A person id that stands for the hero (the player) in a game. */
	constexpr int HeroId = -2;

	struct FCover
	{
		FV2 At;         // where a child crouches
		FV2 Behind;     // the thing they hide behind
	};

	/** One of the yard's own things (the game builds them: tools/yard/yard_areas.py "kit"). */
	struct FKit
	{
		std::string Kind;       // bench_park, bin, sandpit, flowerbed, tree, bush
		std::string Species;    // tree / bush: a PlantSpeciesLibrary species
		FV2 At;
		float Yaw = 0.f, Age = 0.f, R = 0.f;
		int Variant = 0, Seed = 0;
	};

	struct FArea
	{
		std::string Id, Kind;   // S-YARD-01 / yard, playground, park, square, beach
		FV2 C;
		float Z = 0.f;          // the ground (Unreal metres)
		float R = 10.f;         // the clear circle
		float Catch = 25.f;     // children nearer than this are offered to it
		float Yaw = 0.f;
		uint32_t Games = 0;     // 1 << EGame
		std::vector<FCover> Cover;
		FV2 Wall;               // where the seeker counts (facing WallYaw)
		float WallYaw = 0.f;
		FV2 Chalk;              // the hopscotch's first square's near edge; the squares run along ChalkYaw
		float ChalkYaw = 0.f;
		FV2 Rope;               // the rope's middle; the turners stand along RopeYaw
		float RopeYaw = 0.f;
		float FieldHalfL = 12.f, FieldHalfW = 6.f, FieldYaw = 0.f;
		std::vector<std::string> Places;
		std::vector<FKit> Kit;
		bool Fits(EGame G) const { return (Games >> (int)G) & 1u; }
	};

	struct FKid
	{
		int Id = -1;            // the person (HeroId: the hero)
		int Age = 9;
		ERole Role = ERole::None;
		EAct Act = EAct::Stand;
		float ActT = 0.f;       // s since the act began
		FV2 Pos, Goal;
		float Yaw = 0.f;
		float Speed = 0.f;      // m/s now
		float Lift = 0.f;       // m over the ground (a hop, a jump)
		float Phase = 0.f;      // the rope's angle for a turner (rad), the hop's progress
		int Team = 0;
		int Cover = -1;         // hide-and-seek: the hiding place taken
		int Level = 0;          // hopscotch: how far they got
		float Timer = 0.f;      // per role: a kick's cool-down, a tag's freeze, a jumper's count
		float Breath = 1.f;     // tag: 1 fresh .. 0 out of breath (a sprint tires, a jog rests)
		bool bArrived = false;
		bool bPresent = true;   // offered again since the last look (else they leave)
		bool bPending = false;  // hide-and-seek: joins at the next round
		float Fast() const { return 3.0f + 0.1f * (float)(Age < 5 ? 5 : (Age > 14 ? 14 : Age)); }
	};

	struct FBall
	{
		FV2 Pos, Vel;
		float Z = 0.f, VZ = 0.f;
		int LastKick = -1;
		float Spin = 0.f;       // rad rolled (for the drawing)
	};

	struct FEvent
	{
		EEvent Kind = EEvent::Start;
		int Game = -1;          // the game's Uid
		EGame G = EGame::Tag;
		int Area = -1;
		int Who = -1, Other = -1;
	};

	struct FGame
	{
		int Uid = 0;
		EGame Kind = EGame::Tag;
		int Area = -1;
		EPhase Phase = EPhase::Gather;
		float T = 0.f, PhaseT = 0.f, Length = 600.f;
		int Leader = -1;
		std::vector<FKid> Kids;
		FBall Ball;
		int Round = 0;
		int Score[2] = { 0, 0 };
		float Beat = 0.f;           // the rope's angle (rad), hopscotch: the hop's clock
		int Turn = -1;              // hopscotch / rope: whose turn (a person id)
		int Seeker = -1;            // hide-and-seek
		float Count = 0.f;          // hide-and-seek: counting left (s); then the time to seek left
		int FirstFound = -1;
		std::vector<int> Visited;   // hide-and-seek: cover points the seeker has looked behind this round
		int SeekTarget = -1;        // a cover index, or -2: the hero's spot
		int It = -1, LastIt = -1;   // tag
		float Immune = 0.f;
		float Kickoff = 0.f;        // football: till the ball is in play again
		uint32_t Rng = 1;
		bool bHeroSeeks = false;
		bool bForced = false;       // started from outside (developer mode, the bench): nobody is let go till it ends

		FKid* Find(int Id);
		const FKid* Find(int Id) const;
		int NumIn(ERole R) const;
	};

	/** A child who could play now: their life has them at play near the area. */
	struct FCandidate
	{
		int Id = -1;
		int Age = 9;
		FV2 Pos;
	};

	class FYard
	{
	public:
		std::vector<FArea> Areas;
		std::vector<FGame> Games;
		std::vector<FEvent> Events;     // since the caller last cleared them
		std::string LastError;
		int MaxGames = 6;
		uint32_t Seed = 20261121u;

		bool Load(const std::string& Text);
		int FindArea(const std::string& Id) const;

		/** The children who could play at an area now (call every second or so for each area with children near; an
		 *  empty list too, so those who left are let go): starts a game when there are enough, takes newcomers into the
		 *  running one, lets go of whoever is no longer offered. */
		void Offer(int Area, const std::vector<FCandidate>& Kids);
		void Step(float Dt);

		/** Starts a game now (developer mode, tests): false if the area doesn't fit it or there are too few. */
		bool Start(int Area, EGame Kind, const std::vector<FCandidate>& Kids, float Length = -1.f, bool bForced = false);
		void EndGame(int Index, int Why);
		void EndAll();
		int GameAt(int Area) const;
		/** Which game (index) a person is in, -1 none. */
		int InGame(int Id) const;
		/** Whether a child at play at Pos (at it, or still on the way there) is offered to an area: within its catch when at
		 *  it, near its middle when passing; one already in the area's game a good way further (their life's wander). */
		static bool Near(const FArea& A, const FV2& Pos, bool bDoing, bool bInItsGame);
		static int MinPlayers(EGame G);
		static int MaxPlayers(EGame G);

		// --- the hero (hide-and-seek)
		FV2 HeroPos;
		FV2 HeroVel;
		bool bHeroHere = false;         // the hero is about (HeroPos is valid)
		/** Joins the hide-and-seek game Index as a hider (into the next round if the seeker is already looking). */
		bool HeroJoin(int Index);
		/** The hero is the seeker of game Index from its next round (now, if it is between rounds). */
		bool HeroSeek(int Index);
		/** The hero, seeking, says "found you" at HeroPos: the hider found (a person id), -1 nobody near enough. */
		int HeroFind();
		void HeroLeave();
		/** The game the hero is in, -1 none. */
		int HeroGame() const { return InGame(HeroId); }

	private:
		std::vector<float> Cooldown;    // per area: s till a new game may start there
		std::vector<int> LastGame;      // per area: the last game played (not the same twice running)
		int NextUid = 1;

		void Emit(FGame& G, EEvent E, int Who, int Other = -1);
		void Begin(FGame& G);
		void NewRound(FGame& G);
		void StepGame(FGame& G, float Dt);
		void StepHopscotch(FGame& G, const FArea& A, float Dt);
		void StepRope(FGame& G, const FArea& A, float Dt);
		void StepHide(FGame& G, const FArea& A, float Dt);
		void StepFootball(FGame& G, const FArea& A, float Dt);
		void StepTag(FGame& G, const FArea& A, float Dt);
		void Move(FGame& G, const FArea& A, FKid& K, float Speed, float Dt);
		void Leave(FGame& G, int Id);
		bool AddKid(FGame& G, const FCandidate& C);
		FV2 StartSpot(const FGame& G, const FArea& A, const FKid& K, int Index) const;
	};

	// --- the hopscotch's squares (the game draws them, the model hops on them): rows of 0.5 m along ChalkYaw from Chalk,
	// one square or two side by side (rows 3 and 5), a half circle "home" past the last
	constexpr int HopRows = 7;
	constexpr float HopCell = 0.5f;
	inline bool HopPair(int Row) { return Row == 3 || Row == 5; }
	/** The middle of a row (both feet land astride on a pair row's line). */
	FV2 HopRowCentre(const FArea& A, int Row);

	// --- the rope: the turners' hands this far apart along RopeYaw, at this height (m), the rope's sag and its turns a second
	constexpr float RopeHalf = 1.7f;
	constexpr float RopeHands = 0.85f;
	constexpr float RopeHz = 1.05f;
	/** A point along the rope (S 0..1 from one turner's hand to the other's) at angle Beat (rad; pi: the bottom, on the
	 *  ground), relative to the ground under the rope's middle: X/Y Unreal metres in the plane, Z up. */
	void RopePoint(const FArea& A, float Beat, float S, float& OutX, float& OutY, float& OutZ);

	// --- football: the goals' half width (m) at the field's two ends along FieldYaw
	constexpr float GoalHalf = 1.5f;

	/** xorshift32 in [0, 1). */
	float Rand(uint32_t& State);
}
