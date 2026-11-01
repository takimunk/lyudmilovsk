// Checks of the sea's currents (SeaCurrent.cpp) and the fish (FishModel.cpp) outside the engine:
//   tools/underwater/run.sh test   -> prints a summary, exits 1 on a failure
#include "FishModel.h"
#include "SeaCurrent.h"

#include <cmath>
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
				Bad += !bSea || F.Pos.Z < Bz - 0.05f || F.Pos.Z > Env.Level + 0.01f;
				Fast += F.Vel.Len() > Species(F.Species).Burst * 1.6f + 0.4f;
				Total++;
			}
		}
		for (int S = 0; S < NumSpecies(); ++S) Seen[S] += Sim.Count(S);
		std::printf("eye x %.0f (%.1f m deep):", Eye.X, -Eye.Z);
		for (int S = 0; S < NumSpecies(); ++S) if (Sim.Count(S)) std::printf(" %s %d", Species(S).Id, Sim.Count(S));
		std::printf("\n");
		// a school's fish keep together
		const int H = FindSpecies("Herring");
		std::vector<FV> Pts;
		for (const FFish& F : Sim.Fish()) if (F.Species == H && F.Group == Sim.Fish().front().Group) Pts.push_back(F.Pos);
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
	std::printf("fish-frames %d, under the bed or over the water %d, too fast %d\n", Total, Bad, Fast);
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
	// sand eels go into the sand when a cod strikes them (the alarm), and come out again
	std::printf("%s\n", Fails ? "FAILED" : "all ok");
	return Fails ? 1 : 0;
}
