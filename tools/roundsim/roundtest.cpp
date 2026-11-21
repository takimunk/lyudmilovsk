// Offline checks of the bicycle rounds (docs/rounds.md): the rules of Private/Rounds/RoundsModel.h, and a ride through every
// route of Content/City/rounds.json (as dump.py writes it) by a careful, a sloppy and a lazy paper boy, and every day's post.
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Rounds/RoundsModel.h"

using namespace LyudRounds;

static int Checks = 0, Failed = 0;
static void Check(bool bOk, const std::string& What)
{
	++Checks;
	if (!bOk)
	{
		++Failed;
		std::printf("  FAIL %s\n", What.c_str());
	}
}

struct FRouteData
{
	std::string Id;
	int Terrace = 0;
	std::vector<FStop> Stops;
};

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::printf("usage: roundtest <rounds.txt>\n");
		return 2;
	}
	std::ifstream In(Argv[1]);
	std::string Line;
	double BundleX = 0, BundleY = 0, BundleZ = 0, PostmanX = 0, PostmanY = 0;
	double PFrom = 0, PUntil = 0, PDead = 0, QFrom = 0, QUntil = 0, QDead = 0;
	int Spare = 0, Parcels = 0;
	std::vector<FRouteData> Routes;
	std::vector<FStop> Post;
	while (std::getline(In, Line))
	{
		std::istringstream L(Line);
		std::string K;
		L >> K;
		if (K == "bundle")
		{
			L >> BundleX >> BundleY >> BundleZ;
		}
		else if (K == "paperwin")
		{
			L >> PFrom >> PUntil >> PDead >> Spare;
		}
		else if (K == "postwin")
		{
			L >> QFrom >> QUntil >> QDead >> Parcels;
		}
		else if (K == "postman")
		{
			double Z = 0;
			L >> PostmanX >> PostmanY >> Z;
		}
		else if (K == "route")
		{
			FRouteData R;
			L >> R.Id >> R.Terrace;
			Routes.push_back(R);
		}
		else if (K == "stop" || K == "post")
		{
			FStop S;
			L >> S.Id >> S.Terrace >> S.X >> S.Y >> S.Z >> S.Person;
			if (K == "stop" && !Routes.empty())
			{
				Routes.back().Stops.push_back(S);
			}
			else if (K == "post")
			{
				Post.push_back(S);
			}
		}
	}

	// --- the rules
	FStop Door;
	Door.X = 1000.0;
	Door.Y = 2000.0;
	Door.Z = -3620.0;
	Check(JudgeThrow(Door, 1050.0, 2050.0, -3620.0) == EResult::Perfect, "a paper half a metre off is on the step");
	Check(JudgeThrow(Door, 1200.0, 2000.0, -3600.0) == EResult::Good, "two metres off is near it");
	Check(JudgeThrow(Door, 1400.0, 2000.0, -3620.0) == EResult::Missed, "four metres off is in the road");
	Check(JudgeThrow(Door, 1000.0, 2000.0, -3300.0) == EResult::Missed, "on the storey above is not his step");
	Check(PaperRouteFor(0, 4) == 0 && PaperRouteFor(5, 4) == 1 && PaperRouteFor(-1, 4) == 3 && PaperRouteFor(3, 0) == -1, "the terraces in turn");
	for (int Day = 0; Day < 60; ++Day)
	{
		const std::vector<int> P = PostPicks(Day, 39, 5);
		const std::set<int> U(P.begin(), P.end());
		bool bSorted = true;
		for (size_t I = 1; I < P.size(); ++I)
		{
			bSorted = bSorted && P[I - 1] < P[I];
		}
		Check(P.size() == 5 && U.size() == 5 && bSorted && P == PostPicks(Day, 39, 5), "day " + std::to_string(Day) + ": five different parcels, in order, the same every time");
	}
	Check(PostPicks(1, 3, 5).size() == 3 && PostPicks(1, 0, 5).empty(), "no more parcels than flats");
	Check(PostPicks(1, 39, 5) != PostPicks(2, 39, 5), "another day, other parcels");
	Check(std::string(AnswerSet(0.5, 0.2, true)) == "sleepy" && std::string(AnswerSet(0.9, 0.8, false)) == "nervy" &&
		std::string(AnswerSet(0.9, 0.2, false)) == "warm" && std::string(AnswerSet(0.1, 0.2, false)) == "reserved" &&
		std::string(AnswerSet(0.5, 0.5, false)) == "usual", "who opens how");
	for (int S = 0; S < 50; ++S)
	{
		const double W = WaitSeconds(0.5, false, S), Wn = WaitSeconds(0.95, false, S), Ws = WaitSeconds(0.5, true, S);
		Check(W >= 1.0 && W <= 5.0 && Wn < W && Ws >= 5.0 && Ws <= 7.5, "the wait at the door " + std::to_string(S));
	}

	// a round's bookkeeping
	{
		FRound R;
		R.Begin(EKind::Paper, 0, {0, 1, 2, 3}, 6, 6.2, 8.0);
		Check(R.Left() == 4 && !R.AllDone(), "a fresh round");
		Check(R.Set(0, EResult::Perfect) && !R.Set(0, EResult::Missed), "the first word counts");
		R.Set(1, EResult::Perfect);
		R.Set(2, EResult::Good);
		R.Set(3, EResult::Perfect);
		Check(R.AllDone() && R.Pay(7.0) == 4 * 3 + 3 + 15 && R.Pay(8.5) == 15, "paper pay: in time and late");
		Check(R.Stamp(2) == "rounds/t2", "a good paper round earns its terrace");
		FRound Bad;
		Bad.Begin(EKind::Paper, 0, {0, 1, 2, 3}, 4, 6.2, 8.0);
		Bad.Set(0, EResult::Missed);
		Bad.Set(1, EResult::Missed);
		Bad.Set(2, EResult::Good);
		Bad.Set(3, EResult::Perfect);
		Check(Bad.Pay(7.0) == 7 && Bad.Stamp(1).empty(), "half in the road: no bonus, no stamp");
		FRound Q;
		Q.Begin(EKind::Post, -1, {3, 9, 12}, 0, 9.5, 14.0);
		Check(!Q.AllDone(), "post: the bag full (no papers to run out of)");
		Q.Set(0, EResult::Perfect);
		Q.Set(1, EResult::Missed);
		Check(Q.Stamp(0).empty() && Q.Pay(10.0) == 12, "post: not done yet, no bonus");
		Q.Set(2, EResult::Perfect);
		Check(Q.AllDone() && Q.Pay(10.0) == 10 + 2 + 10 + 10 && Q.Stamp(0) == "rounds/post", "post: done in time");
	}

	// --- the data: a ride through every route
	Check(Routes.size() == 4, "four paper routes (T1..T4)");
	Check(PFrom == 6.0 && PDead > PUntil && PUntil > PFrom && Spare >= 2, "the paper window: the bundle from six, the deadline after it");
	Check(QDead > QUntil && QUntil > QFrom && Parcels >= 3, "the post window");
	std::set<int> Terraces;
	for (const FRouteData& R : Routes)
	{
		Terraces.insert(R.Terrace);
		Check(R.Stops.size() >= 8, R.Id + ": at least eight doors");
		// the ride: kiosk to the first door, then door to door, at a teenager's racer's 6 m/s plus 20 s a door
		double Metres = std::hypot(R.Stops.front().X - BundleX, R.Stops.front().Y - BundleY) / 100.0;
		for (size_t I = 1; I < R.Stops.size(); ++I)
		{
			Metres += std::hypot(R.Stops[I].X - R.Stops[I - 1].X, R.Stops[I].Y - R.Stops[I - 1].Y) / 100.0;
			Check(R.Stops[I].Y >= R.Stops[I - 1].Y, R.Id + ": the doors in riding order");
			Check(std::hypot(R.Stops[I].X - R.Stops[I - 1].X, R.Stops[I].Y - R.Stops[I - 1].Y) > 2.0 * GoodCm,
				R.Id + ": two doors never so close one paper serves both");
		}
		const double Hours = (Metres / 6.0 + 20.0 * R.Stops.size()) / 3600.0;
		Check(PUntil + Hours <= PDead + 0.25, R.Id + ": a boy who takes the bundle last can still make it");
		std::printf("  %s: %zu doors, %.0f m from the kiosk to the last door, %.0f min by bike\n", R.Id.c_str(), R.Stops.size(), Metres, Hours * 60.0);
		for (const FStop& S : R.Stops)
		{
			Check(S.Terrace == R.Terrace, R.Id + ": " + S.Id + " on its terrace");
		}
		const int Paper = (int)R.Stops.size() + Spare;
		// careful: every paper on the step
		std::vector<int> Entries;
		for (int I = 0; I < (int)R.Stops.size(); ++I)
		{
			Entries.push_back(I);
		}
		FRound Careful;
		Careful.Begin(EKind::Paper, 0, Entries, Paper, 6.0, PDead);
		for (int I = 0; I < (int)R.Stops.size(); ++I)
		{
			const FStop& S = R.Stops[(size_t)I];
			Careful.Set(I, JudgeThrow(S, S.X + 60.0, S.Y - 40.0, S.Z));
			--Careful.Papers;
		}
		Check(Careful.AllDone() && Careful.Pay(6.0 + Hours) == 4 * (int)R.Stops.size() + 15 && !Careful.Stamp(R.Terrace).empty(),
			R.Id + ": the careful boy is paid in full and gets the stamp");
		// sloppy: every other paper two and a half metres off, a stray or two, papers to spare
		FRound Sloppy;
		Sloppy.Begin(EKind::Paper, 0, Entries, Paper, 6.0, PDead);
		for (int I = 0; I < (int)R.Stops.size(); ++I)
		{
			const FStop& S = R.Stops[(size_t)I];
			const double Off = I % 2 ? 240.0 : 90.0;
			Sloppy.Set(I, JudgeThrow(S, S.X + Off, S.Y, S.Z));
			--Sloppy.Papers;
		}
		const int SP = Sloppy.Pay(6.0 + Hours);
		Check(SP > 0 && SP < Careful.Pay(6.0 + Hours), R.Id + ": the sloppy boy earns less");
		// lazy: he gives up halfway, out of papers he threw into the sea
		FRound Lazy;
		Lazy.Begin(EKind::Paper, 0, Entries, Paper, 6.0, PDead);
		for (int I = 0; I < (int)R.Stops.size() / 2; ++I)
		{
			Lazy.Set(I, EResult::Missed);
		}
		Lazy.Papers = 0;
		Check(Lazy.AllDone() && Lazy.Pay(9.0) == 0 && Lazy.Stamp(R.Terrace).empty(), R.Id + ": the lazy boy earns nothing");
	}
	Check(Terraces == std::set<int>({1, 2, 3, 4}), "a route on every terrace T1..T4");

	// the post: two months of days, every parcel a flat with somebody, the ride from the postman's door
	Check(Post.size() >= 20, "enough flats for the post");
	std::set<std::string> People;
	for (const FStop& S : Post)
	{
		Check(!S.Person.empty() && S.Person != "-", "post " + S.Id + ": somebody to hand it to");
		People.insert(S.Id);
	}
	Check(People.size() == Post.size(), "a flat once in the post");
	double Longest = 0.0;
	for (int Day = 0; Day < 60; ++Day)
	{
		const std::vector<int> P = PostPicks(Day, (int)Post.size(), Parcels);
		double Metres = 0.0, X = PostmanX, Y = PostmanY;
		for (const int I : P)
		{
			Metres += std::hypot(Post[(size_t)I].X - X, Post[(size_t)I].Y - Y) / 100.0;
			X = Post[(size_t)I].X;
			Y = Post[(size_t)I].Y;
		}
		Longest = std::max(Longest, Metres);
		const double Hours = (Metres / 6.0 + 40.0 * P.size()) / 3600.0;
		Check(QUntil + Hours <= QDead, "post day " + std::to_string(Day) + ": the round fits before the deadline");
	}
	std::printf("  post: %zu flats, the longest day %.0f m from the postman's door\n", Post.size(), Longest);

	std::printf("%d checks, %d failed\n", Checks, Failed);
	return Failed ? 1 : 0;
}
