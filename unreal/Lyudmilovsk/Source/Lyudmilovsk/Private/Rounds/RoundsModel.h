#pragma once

// The bicycle rounds' rules (docs/rounds.md): plain C++, no engine types, checked by tools/roundsim.
//   paper: the bundle at kiosk 1 from 6:00, a paper thrown to each subscriber's doorstep of the day's terrace before 8:00;
//          where it lands is judged (on the step, near it, missed)
//   post:  the day's parcels from the old postman's door, rung at each flat; whoever is in opens (or a notice is left)

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace LyudRounds
{
	enum class EKind : uint8_t { Paper, Post };
	/** What became of a stop: not yet; a paper in the road or a notice in the box; near the step; on it / opened and handed. */
	enum class EResult : int8_t { Pending = -1, Missed = 0, Good = 1, Perfect = 2 };

	struct FStop
	{
		std::string Id;      // the flat (W2-D005)
		std::string Person;  // who the parcel is for (post), a townsfolk key
		int Terrace = 1;
		double X = 0.0, Y = 0.0, Z = 0.0;  // the door's step, Unreal cm
		double Yaw = 0.0;
	};

	/** A paper on the step: within PerfectCm of the door's point, near it within GoodCm; more than MaxRiseCm above or below
	 *  is another storey's step (missed). */
	constexpr double PerfectCm = 130.0;
	constexpr double GoodCm = 280.0;
	constexpr double MaxRiseCm = 220.0;
	/** A landing farther than this from every waiting door is a stray paper (lost, nobody's). */
	constexpr double ClaimCm = 700.0;

	inline EResult JudgeThrow(const FStop& S, double X, double Y, double Z)
	{
		if (std::fabs(Z - S.Z) > MaxRiseCm)
		{
			return EResult::Missed;
		}
		const double D = std::sqrt((X - S.X) * (X - S.X) + (Y - S.Y) * (Y - S.Y));
		return D <= PerfectCm ? EResult::Perfect : (D <= GoodCm ? EResult::Good : EResult::Missed);
	}

	/** The paper route of a day (days since the game began): the terraces in turn. */
	inline int PaperRouteFor(int Day, int NumRoutes)
	{
		if (NumRoutes <= 0)
		{
			return -1;
		}
		const int R = Day % NumRoutes;
		return R < 0 ? R + NumRoutes : R;
	}

	inline uint32_t Hash(uint32_t X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}

	/** The day's parcels: Count different flats of NumStops (the same every time for a day), in the stops' order (the
	 *  generator lists them terrace by terrace along the town, so the order is a sensible ride). */
	inline std::vector<int> PostPicks(int Day, int NumStops, int Count)
	{
		std::vector<int> Out;
		if (NumStops <= 0 || Count <= 0)
		{
			return Out;
		}
		std::vector<bool> Taken((size_t)NumStops, false);
		for (int I = 0; (int)Out.size() < Count && (int)Out.size() < NumStops && I < Count * 64; ++I)
		{
			const int Pick = (int)(Hash((uint32_t)Day * 7919u + (uint32_t)I * 104729u + 17u) % (uint32_t)NumStops);
			if (!Taken[(size_t)Pick])
			{
				Taken[(size_t)Pick] = true;
				Out.push_back(Pick);
			}
		}
		std::vector<int> Sorted;
		for (int I = 0; I < NumStops; ++I)
		{
			if (Taken[(size_t)I])
			{
				Sorted.push_back(I);
			}
		}
		return Sorted;
	}

	/** Who opens and how (a set of lines, docs/rounds.md): asleep (the bell woke them), warm, reserved, nervy, or "" for the
	 *  usual. Traits 0..1 as the people's manners give them. */
	inline const char* AnswerSet(double Warmth, double Nerve, bool bAsleep)
	{
		if (bAsleep)
		{
			return "sleepy";
		}
		if (Nerve > 0.7)
		{
			return "nervy";
		}
		if (Warmth > 0.66)
		{
			return "warm";
		}
		if (Warmth < 0.34)
		{
			return "reserved";
		}
		return "usual";
	}

	/** Seconds from the bell to the door opening: the nervy are at it at once, the calm take their time, sleepers longest. */
	inline double WaitSeconds(double Nerve, bool bAsleep, int Seed)
	{
		const double Jitter = (Hash((uint32_t)Seed) % 1000u) / 1000.0;
		const double W = bAsleep ? 5.0 + 2.0 * Jitter : 1.2 + 2.6 * (1.0 - Nerve) + 1.0 * Jitter;
		return W;
	}

	/** One round under way. Times are game hours of the day (6.5 = 6:30). */
	struct FRound
	{
		EKind Kind = EKind::Paper;
		int Route = -1;                 // paper: the route; post: unused
		std::vector<int> Stops;         // indices into the route's stops (paper) or the post's flats (post)
		std::vector<EResult> Results;   // per entry of Stops
		int Papers = 0;                 // paper: left in the bag
		int Stray = 0;                  // paper: thrown at nobody's door
		double Started = 0.0, Deadline = 0.0;
		bool bOver = false;

		void Begin(EKind K, int InRoute, const std::vector<int>& InStops, int InPapers, double Now, double InDeadline)
		{
			Kind = K;
			Route = InRoute;
			Stops = InStops;
			Results.assign(Stops.size(), EResult::Pending);
			Papers = InPapers;
			Stray = 0;
			Started = Now;
			Deadline = InDeadline;
			bOver = false;
		}

		int Left() const
		{
			int N = 0;
			for (const EResult R : Results)
			{
				N += R == EResult::Pending ? 1 : 0;
			}
			return N;
		}
		int Count(EResult What) const
		{
			int N = 0;
			for (const EResult R : Results)
			{
				N += R == What ? 1 : 0;
			}
			return N;
		}
		/** Every stop answered, or (paper) no paper left for the ones still waiting. */
		bool AllDone() const { return Left() == 0 || (Kind == EKind::Paper && Papers <= 0); }

		/** Sets a stop's result (the first word counts); false if it was already answered or out of range. */
		bool Set(int Entry, EResult R)
		{
			if (Entry < 0 || Entry >= (int)Results.size() || Results[(size_t)Entry] != EResult::Pending || R == EResult::Pending)
			{
				return false;
			}
			Results[(size_t)Entry] = R;
			return true;
		}

		/** Share of the stops served well (paper: on or near the step; post: handed over). */
		double Hit() const
		{
			if (Results.empty())
			{
				return 0.0;
			}
			return (double)(Count(EResult::Good) + Count(EResult::Perfect)) / (double)Results.size();
		}

		/** The pay, roubles: paper 4 a paper on the step, 3 near it, and 15 more for the whole round by the deadline with
		 *  three in four papers well placed; post 10 a parcel handed, 2 a notice, and 10 more for the whole round in time. */
		int Pay(double FinishedAt) const
		{
			const bool bInTime = FinishedAt <= Deadline && Left() == 0;
			if (Kind == EKind::Paper)
			{
				int P = 4 * Count(EResult::Perfect) + 3 * Count(EResult::Good);
				if (bInTime && Hit() >= 0.75)
				{
					P += 15;
				}
				return P;
			}
			int P = 10 * (Count(EResult::Good) + Count(EResult::Perfect)) + 2 * Count(EResult::Missed);
			if (bInTime)
			{
				P += 10;
			}
			return P;
		}

		/** The stamp it earns (empty: none): a paper round with three in four well placed earns its terrace's piece
		 *  ("rounds/t2"), a post round done to the end "rounds/post". */
		std::string Stamp(int Terrace) const
		{
			if (Kind == EKind::Paper)
			{
				return Left() == 0 && Hit() >= 0.75 ? "rounds/t" + std::to_string(Terrace) : std::string();
			}
			return Left() == 0 ? std::string("rounds/post") : std::string();
		}
	};
}
