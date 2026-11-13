// Checks of the sea's currents (SeaCurrent.cpp) and the fish (FishModel.cpp) outside the engine:
//   tools/underwater/run.sh test   -> prints a summary, exits 1 on a failure
#include "EcoModel.h"
#include "FishModel.h"
#include "SeaCurrent.h"

#include <cmath>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace LyudFish;
namespace C = LyudSeaCurrent;

static int Fails = 0;
static void Check(bool Ok, const char* What)
{
	std::printf("%s %s\n", Ok ? "ok  " : "FAIL", What);
	Fails += !Ok;
}

/** A bed like the one off the club: shelving from the beach (x 0) to 15 m, a reef band, eelgrass in the shallows. */
struct FTestEnv : FEnv
{
	float Level = 0.f;
	bool Bed(float X, float Y, float& Z) const override
	{
		if (X < 5.f) return false;
		Z = -std::min(0.04f * (X - 5.f) + 0.3f, 15.f) + (std::fabs(X - 120.f) < 10.f ? 1.5f : 0.f) + 0.2f * std::sin(Y * 0.3f);
		return Z < Level - 0.1f;
	}
	float SurfaceZ() const override { return Level; }
	void Habitat(float X, float Y, float& Sand, float& Stone, float& Weed) const override
	{
		Stone = std::fabs(X - 120.f) < 12.f ? 1.f : (std::sin(X * 0.1f) * std::sin(Y * 0.13f) > 0.6f ? 0.6f : 0.f);
		Weed = (X > 40.f && X < 100.f && std::sin(Y * 0.05f) > -0.2f) ? 0.9f : 0.f;
		Sand = 1.f - Stone;
	}
	FV Current(const FV& P) const override { return FV(0.f, 0.15f, 0.f); }
};

int main()
{
	// --- currents
	C::FDrive D;
	D.TideRate = 3.2e-4f;   // flood at springs
	C::FPlace P;
	P.Depth = 8.f;
	P.OverBed = 4.f;
	const C::FVec3 Flood = C::Mean(D, P);
	D.TideRate = -3.2e-4f;
	const C::FVec3 Ebb = C::Mean(D, P);
	std::printf("flood %.3f %.3f, ebb %.3f %.3f m/s\n", Flood.X, Flood.Y, Ebb.X, Ebb.Y);
	Check(Flood.Y > 0.2f && Flood.Y < 0.5f && Ebb.Y < -0.2f, "tidal stream along the coast, north with the flood, 0.2-0.5 m/s at springs");
	D.TideRate = 0.f;
	D.WindY = 10.f;
	P.Depth = 3.f;
	P.OverBed = 1.5f;
	const C::FVec3 Wind = C::Mean(D, P);
	Check(Wind.Y > 0.15f && Wind.Y < 0.5f, "a 10 m/s alongshore wind drives a 0.15-0.5 m/s longshore current in the bars");
	P.OverBed = 0.05f;
	const C::FVec3 Near = C::Mean(D, P);
	Check(Near.Y < Wind.Y * 0.75f && Near.Y > 0.f, "slower near the bed (log layer)");
	// eddies: divergence free (the water neither appears nor vanishes), a share of the mean
	D.WindY = 6.f;
	D.TideRate = 2e-4f;
	P.OverBed = 2.f;
	P.Depth = 9.f;
	P.Rough = 1.f;
	double MaxDiv = 0.0, Dev = 0.0;
	const C::FVec3 M0 = C::Mean(D, P);
	for (int I = 0; I < 400; ++I)
	{
		D.Time = I * 7.3;
		P.X = I * 1.7;
		P.Y = I * 0.9;
		const float H = 0.01f;
		C::FPlace A = P, B = P, E = P, F = P;
		A.X += H; B.X -= H; E.Y += H; F.Y -= H;
		const double Div = (C::At(D, A).X - C::At(D, B).X + C::At(D, E).Y - C::At(D, F).Y) / (2.0 * H);
		MaxDiv = std::max(MaxDiv, std::fabs(Div));
		const C::FVec3 U = C::At(D, P);
		Dev += std::hypot(U.X - M0.X, U.Y - M0.Y);
	}
	std::printf("eddies: mean deviation %.3f m/s of %.3f, max divergence %.2e /s\n", Dev / 400, std::hypot(M0.X, M0.Y), MaxDiv);
	Check(MaxDiv < 1e-3, "eddies are divergence free");
	Check(Dev / 400 > 0.02 && Dev / 400 < 0.25, "eddies over a reef stir 0.02-0.25 m/s");

	// --- fish
	FTestEnv Env;
	FSim Sim;
	FClock Clock;
	Clock.Month = 10;
	Clock.Daylight = 1.f;
	const FV Spots[] = { FV(25.f, 0.f, -0.5f), FV(70.f, 0.f, -1.5f), FV(120.f, 0.f, -3.f), FV(250.f, 30.f, -7.f) };
	std::vector<int> Seen(NumSpecies(), 0);
	int Bad = 0, Fast = 0, Total = 0;
	std::vector<int> BadBy(NumSpecies(), 0);
	float SchoolSpread = 0.f;
	int SchoolN = 0;
	for (const FV& Eye : Spots)
	{
		Sim.Clear();
		for (int T = 0; T < 1800; ++T)
		{
			Sim.Step(1.f / 30.f, Env, Eye, Clock);
			for (const FFish& F : Sim.Fish())
			{
				if (F.bHidden) continue;
				float Bz;
				const bool bSea = Env.Bed(F.Pos.X, F.Pos.Y, Bz);
				const bool bBad = !bSea || F.Pos.Z < Bz - 0.05f || F.Pos.Z > Env.Level + 0.01f;
				if (bBad) BadBy[F.Species]++;
				Bad += bBad;
				Fast += F.Vel.Len() > Species(F.Species).Burst * 1.6f + 0.4f;
				Total++;
			}
			// a school's fish keep together (sampled once a second over the whole run)
			if (T % 30 == 0)
			{
				const int H = FindSpecies("Herring");
				for (const FFish& F : Sim.Fish())
					if (F.Species == H)
					{
						FV C0;
						int N = 0;
						for (const FFish& O : Sim.Fish()) if (O.Group == F.Group) { C0 += O.Pos; N++; }
						SchoolSpread += (F.Pos - C0 * (1.f / N)).Len();
						SchoolN++;
					}
			}
		}
		for (int S = 0; S < NumSpecies(); ++S) Seen[S] += Sim.Count(S);
		std::printf("eye x %.0f (%.1f m deep):", Eye.X, -Eye.Z);
		for (int S = 0; S < NumSpecies(); ++S) if (Sim.Count(S)) std::printf(" %s %d", Species(S).Id, Sim.Count(S));
		std::printf("\n");
	}
	std::printf("fish-frames %d, under the bed or over the water %d, too fast %d\n", Total, Bad, Fast);
	for (int S = 0; S < NumSpecies(); ++S) if (BadBy[S]) std::printf("  out of place: %s %d\n", Species(S).Id, BadBy[S]);
	Check(Bad == 0, "no fish under the bed or out of the water");
	Check(Fast < Total / 200 + 1, "no fish faster than it can swim");
	int Kinds = 0;
	for (int S = 0; S < NumSpecies(); ++S) Kinds += Seen[S] > 0;
	std::printf("species seen in October: %d of %d\n", Kinds, NumSpecies());
	Check(Kinds >= 11, "at least 11 species around in October");
	Check(Seen[FindSpecies("Garfish")] == 0, "no garfish in October (they leave in September)");
	if (SchoolN) std::printf("herring: mean distance to the school's centre %.2f m\n", SchoolSpread / SchoolN);
	Check(SchoolN == 0 || SchoolSpread / SchoolN < 3.5f, "herring keep in a school");

	// a flounder darts off when the eye comes close; it stays when the eye keeps away
	{
		Sim.Clear();
		const FV Far(90.f, 0.f, -1.5f);
		for (int T = 0; T < 60; ++T) Sim.Step(1.f / 30.f, Env, Far, Clock);
		const int Fl = FindSpecies("Flounder");
		FV At;
		bool bFound = Sim.Nearest(Fl, Far, At);
		Check(bFound, "flounders lie on the sand");
		if (bFound)
		{
			const FV Close = At + FV(0.6f, 0.f, 0.6f);
			for (int T = 0; T < 90; ++T) Sim.Step(1.f / 30.f, Env, Close, Clock);
			FV Now;
			Sim.Nearest(Fl, At, Now);
			std::printf("flounder moved %.2f m when approached\n", (Now - At).Len2D());
			Check((Now - At).Len2D() > 1.5f, "a flounder glides off when you come close");
		}
	}
	// the food web among the fish round the eye: who eats whom comes from the coast's web (EcoModel)
	{
		bool bAllNodes = true;
		for (int S = 0; S < NumSpecies(); ++S)
			if (LyudEco::Find(Species(S).Id) < 0) { std::printf("  no food-web node for %s\n", Species(S).Id); bAllNodes = false; }
		Check(bAllNodes, "every fish species is a node of the food web");
		LyudEco::FModel Web;
		FSim Eco;
		for (int S = 0; S < NumSpecies(); ++S)
			for (int Q = 0; Q < NumSpecies(); ++Q)
			{
				const int J = LyudEco::Find(Species(S).Id), I = LyudEco::Find(Species(Q).Id);
				if (I >= 0 && J >= 0) Eco.SetDiet(S, Q, Web.DietShare(J, I));
			}
		std::vector<int> Pairs(NumSpecies() * NumSpecies(), 0);
		int Wrong = 0, Meals = 0, Misses = 0;
		const int Seal = FindSpecies("GreySeal");
		float SealHigh = -100.f, SealNear = 1e9f;
		bool bSealSeen = false;
		for (const FV& Eye : { FV(120.f, 0.f, -2.5f), FV(250.f, 30.f, -6.f), FV(70.f, 10.f, -1.f) })
		{
			Eco.Clear();
			for (int T = 0; T < 30 * 600; ++T)
			{
				Eco.Step(1.f / 30.f, Env, Eye, Clock);
				for (const FEaten& E : Eco.Eaten())
				{
					Wrong += !Eco.Eats(E.Predator, E.Prey);
					if (!E.bCaught) { Misses++; continue; }
					Pairs[E.Predator * NumSpecies() + E.Prey]++;
					Meals++;
				}
				for (const FFish& F : Eco.Fish())
					if (F.Species == Seal)
					{
						bSealSeen = true;
						SealHigh = std::max(SealHigh, F.Pos.Z - Env.Level);
						SealNear = std::min(SealNear, (F.Pos - Eye).Len());
					}
			}
		}
		std::printf("meals seen in 30 minutes at three spots: %d (and %d misses);", Meals, Misses);
		std::vector<std::pair<int, int>> Top;
		for (int K = 0; K < int(Pairs.size()); ++K) if (Pairs[K]) Top.push_back({ Pairs[K], K });
		std::sort(Top.rbegin(), Top.rend());
		for (int K = 0; K < int(Top.size()) && K < 12; ++K)
			std::printf(" %s<-%s %d", Species(Top[K].second / NumSpecies()).Id, Species(Top[K].second % NumSpecies()).Id, Top[K].first);
		std::printf("\n");
		Check(Wrong == 0, "nothing is eaten by what does not eat it");
		Check(Meals > 20 && Top.size() >= 6, "predators catch prey, of several kinds");
		Check(Misses > Meals / 2, "strikes often miss (over half as many misses as catches): the prey get away too");
		int Cruisers = 0, Ambushes = 0;
		for (const auto& T : Top)
		{
			const EWay W = Species(T.second / NumSpecies()).Way;
			(W == EWay::Cruiser ? Cruisers : Ambushes) += T.first;
		}
		Check(Cruisers > 0 && Ambushes > 0, "both hunters (cod, seals, trout) and ambushers (sculpins, gobies, perch) feed");
		if (bSealSeen)
		{
			std::printf("grey seal: highest %.2f m from the surface, nearest the diver %.1f m\n", SealHigh, SealNear);
			Check(SealHigh > -0.8f, "a grey seal comes up to breathe");
			Check(SealNear < 6.f, "a grey seal comes to look at the diver");
		}
		else Check(false, "a grey seal about in 30 minutes");
	}
	// round 2: day and night, spawning, the young, pike, cormorants and eiders (with the web's diet)
	{
		LyudEco::FModel Web;
		auto Diet = [&](FSim& S)
		{
			for (int A = 0; A < NumSpecies(); ++A)
				for (int B = 0; B < NumSpecies(); ++B)
					S.SetDiet(A, B, Web.DietShare(LyudEco::Find(Species(A).Id), LyudEco::Find(Species(B).Id)));
		};
		auto Run = [&](FSim& S, const FV& Eye, const FClock& Ck, int Steps, auto&& Each)
		{
			for (int T = 0; T < Steps; ++T) { S.Step(1.f / 30.f, Env, Eye, Ck); Each(S, T); }
		};
		FClock Day = Clock, Night = Clock;
		Day.Daylight = 1.f;
		Night.Daylight = 0.f;
		const FV Sand(250.f, 30.f, -7.f), Weedy(70.f, 0.f, -1.f), Reef(120.f, 0.f, -2.5f);
		// shrimp: buried by day, out at night
		{
			const int Sh = FindSpecies("BrownShrimp");
			float Hid[2] = { 0.f, 0.f };
			for (int K = 0; K < 2; ++K)
			{
				FSim S;
				int N = 0, H = 0;
				Run(S, Sand, K ? Night : Day, 1800, [&](FSim& Q, int T)
				{
					if (T < 900 || T % 30) return;
					for (const FFish& F : Q.Fish()) if (F.Species == Sh) { N++; H += F.bHidden; }
				});
				Hid[K] = N ? float(H) / N : 0.f;
			}
			std::printf("brown shrimp buried: by day %.0f%%, at night %.0f%%\n", Hid[0] * 100.f, Hid[1] * 100.f);
			Check(Hid[0] > 0.7f && Hid[1] < 0.3f, "brown shrimp lie buried by day and come out at night");
		}
		// mysids rise off the bed at night; eels go about at night
		{
			const int My = FindSpecies("Mysid"), Ee = FindSpecies("Eel");
			float Over[2] = { 0.f, 0.f }, Path[2] = { 0.f, 0.f };
			for (int K = 0; K < 2; ++K)
			{
				FSim S, R;
				double Sum = 0.0, Walk = 0.0;
				int N = 0, NE = 0;
				std::vector<FV> Last;
				Run(S, Sand, K ? Night : Day, 2400, [&](FSim& Q, int T)
				{
					if (T < 600 || T % 30) return;
					for (const FFish& F : Q.Fish())
					{
						float Bz;
						if (F.Species == My && Env.Bed(F.Pos.X, F.Pos.Y, Bz)) { Sum += F.Pos.Z - Bz; N++; }
					}
				});
				Run(R, Reef, K ? Night : Day, 3600, [&](FSim& Q, int T)
				{
					if (T < 600 || T % 30) return;
					int I = 0;
					for (const FFish& F : Q.Fish())
						if (F.Species == Ee)
						{
							if (I < int(Last.size()) && T > 630) { Walk += (F.Pos - Last[I]).Len2D(); NE++; }
							if (I >= int(Last.size())) Last.push_back(F.Pos); else Last[I] = F.Pos;
							++I;
						}
				});
				if (K)
				{
					float Far = 0.f;
					for (const FFish& F : R.Fish()) if (F.Species == Ee) Far = std::max(Far, (F.Pos - F.Slot).Len2D());
					std::printf("eels after a night hour's minute: farthest from its stone %.1f m\n", Far);
					Check(Far < 6.f, "a night roaming eel keeps within a few metres of its stone");
				}
				Over[K] = N ? float(Sum / N) : 0.f;
				Path[K] = NE ? float(Walk / NE) * 60.f : 0.f;
			}
			std::printf("mysids over the bed: by day %.2f m, at night %.2f m; an eel's way per minute: by day %.2f m, at night %.2f m\n", Over[0], Over[1], Path[0], Path[1]);
			Check(Over[1] > Over[0] + 1.f, "the mysid clouds rise off the bed at night");
			Check(Path[1] > 2.f * Path[0] + 0.3f, "eels go about at night, keep to their stones by day");
		}
		// April: herring spawn low over the weed; lumpsuckers guard; June: stickleback nests; August: young herring
		{
			FClock Apr = Clock, Jun = Clock, Aug = Clock, Mar = Clock;
			Apr.Month = 4; Jun.Month = 6; Aug.Month = 8; Mar.Month = 3;
			const int He = FindSpecies("Herring"), St = FindSpecies("Stickleback"), Lu = FindSpecies("Lumpsucker");
			float HOver[2] = { 0.f, 0.f };
			for (int K = 0; K < 2; ++K)
			{
				FSim S;
				double Sum = 0.0;
				int N = 0;
				Run(S, Reef, K ? Apr : Clock, 2400, [&](FSim& Q, int T)
				{
					if (T < 600 || T % 30) return;
					for (const FFish& F : Q.Fish())
					{
						float Bz;
						if (F.Species == He && Env.Bed(F.Pos.X, F.Pos.Y, Bz)) { Sum += F.Pos.Z - Bz; N++; }
					}
				});
				HOver[K] = N ? float(Sum / N) : 99.f;
			}
			std::printf("herring over the bed: October %.2f m, April (spawning) %.2f m\n", HOver[0], HOver[1]);
			Check(HOver[1] < 1.3f && HOver[1] < HOver[0] - 0.5f, "herring spawn low over the bed in April");
			int Nests = 0;
			{
				FSim S;
				Run(S, Weedy, Jun, 60, [&](FSim& Q, int) {});
				for (const FFish& F : S.Fish()) Nests += F.Species == St && F.bNest;
			}
			std::printf("stickleback nests round the eye in June: %d\n", Nests);
			Check(Nests >= 3, "stickleback males keep nests in the weed in June");
			{
				int Young = 0, Old = 0;
				float YL = 0.f, OL = 0.f;
				for (const FV& Eye : { Sand, FV(200.f, -40.f, -5.f), FV(300.f, 80.f, -9.f), FV(160.f, 60.f, -4.f) })
				{
					FSim S;
					S.Density = 3.f;
					Run(S, Eye, Aug, 3600, [&](FSim& Q, int T)
					{
						if (T % 300) return;
						for (const FFish& F : Q.Fish())
							if (F.Species == He) { (F.bYoung ? Young : Old)++; (F.bYoung ? YL : OL) += F.Len; }
					});
				}
				std::printf("herring in August: %d young (%.0f mm), %d older (%.0f mm)\n", Young, Young ? YL / Young * 1000.f : 0.f, Old, Old ? OL / Old * 1000.f : 0.f);
				Check(Young > 0 && Old > 0 && YL / std::max(Young, 1) < 0.5f * OL / std::max(Old, 1), "the young of the year school in August, a third of the size");
			}
			// a lumpsucker on its eggs in March stays when the diver comes close; in June (done spawning) it moves off
			float Moved[2] = { 0.f, 0.f };
			for (int K = 0; K < 2; ++K)
			{
				FSim S;
				FClock Ck = K ? Jun : Mar;
				Ck.Daylight = 1.f;
				const FV Far(120.f, 0.f, -2.5f);
				Run(S, Far, Ck, 60, [&](FSim& Q, int) {});
				FV At;
				if (!S.Nearest(Lu, Far, At)) { Moved[K] = -1.f; continue; }
				Run(S, At + FV(0.3f, 0.f, 0.3f), Ck, 120, [&](FSim& Q, int) {});
				FV Now;
				S.Nearest(Lu, At, Now);
				Moved[K] = (Now - At).Len2D();
			}
			std::printf("a lumpsucker approached: in March moved %.2f m, in June %.2f m\n", Moved[0], Moved[1]);
			Check(Moved[0] >= 0.f && Moved[0] < 0.05f && Moved[1] > 0.3f, "a lumpsucker male guards his eggs in March, keeps off a diver in June");
		}
		// a pike in the weed lunges; cormorants dive and come up; eiders go down to the bed
		{
			const int Pk = FindSpecies("Pike"), Co = FindSpecies("Cormorant"), Ei = FindSpecies("Eider");
			int PikeCaught = 0, PikeStrikes = 0, CoMeals = 0;
			float CoHigh = -99.f, CoLow = 99.f, EiBed = 99.f;
			int CoSurf = 0, CoUnder = 0;
			bool bEiSeen = false;
			FClock Winter = Clock;
			Winter.Month = 11;
			for (const FV& Eye : { Weedy, FV(60.f, 20.f, -1.f), FV(85.f, -15.f, -1.5f) })
			{
				FSim S;
				Diet(S);
				S.Density = 2.f;
				Run(S, Eye, Winter, 30 * 600, [&](FSim& Q, int T)
				{
					for (const FEaten& E : Q.Eaten())
					{
						if (E.Predator == Pk) { PikeStrikes++; PikeCaught += E.bCaught; }
						if (E.Predator == Co && E.bCaught) CoMeals++;
					}
					if (T % 15) return;
					for (const FFish& F : Q.Fish())
					{
						if (F.Species == Co)
						{
							CoHigh = std::max(CoHigh, F.Pos.Z - Env.Level);
							CoLow = std::min(CoLow, F.Pos.Z - Env.Level);
							(F.Pos.Z > Env.Level - 0.2f ? CoSurf : CoUnder)++;
						}
						float Bz;
						if (F.Species == Ei && Env.Bed(F.Pos.X, F.Pos.Y, Bz)) { bEiSeen = true; EiBed = std::min(EiBed, F.Pos.Z - Bz); }
					}
				});
			}
			std::printf("pike: %d strikes, %d caught; cormorant: from %.2f to %.2f m, %d%% of the time on the surface, %d fish; eider: down to %.2f m over the bed\n",
				PikeStrikes, PikeCaught, CoHigh, CoLow, CoSurf + CoUnder ? 100 * CoSurf / (CoSurf + CoUnder) : 0, CoMeals, EiBed);
			Check(PikeStrikes > 0 && PikeCaught > 0, "a pike in the weed strikes, and catches");
			Check(CoHigh > -0.1f && CoLow < -1.f && CoSurf > 0 && CoUnder > 0, "cormorants float on the surface and dive");
			Check(CoSurf > CoUnder, "a resting cormorant floats on the water, not under it (more time up than down)");
			Check(bEiSeen && EiBed < 0.6f, "eiders dive down to the bed");
		}
	}
	// the cost of a step with the food web on, at a busy spot (the ambush checks go through a grid)
	{
		LyudEco::FModel Web;
		FSim Busy;
		for (int S = 0; S < NumSpecies(); ++S)
			for (int Q = 0; Q < NumSpecies(); ++Q)
				Busy.SetDiet(S, Q, Web.DietShare(LyudEco::Find(Species(S).Id), LyudEco::Find(Species(Q).Id)));
		Busy.Density = 2.f;
		const FV Eye(120.f, 0.f, -2.5f);
		for (int T = 0; T < 900; ++T) Busy.Step(1.f / 30.f, Env, Eye, Clock);
		const auto T0 = std::chrono::steady_clock::now();
		for (int T = 0; T < 600; ++T) Busy.Step(1.f / 30.f, Env, Eye, Clock);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count() / 600.0;
		std::printf("a step with %d animals round the eye (double density): %.3f ms\n", int(Busy.Fish().size()), Ms);
		Check(Ms < 0.5, "a step at a busy spot takes under 0.5 ms");
	}
	std::printf("%s\n", Fails ? "FAILED" : "all ok");
	return Fails ? 1 : 0;
}
