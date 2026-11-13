// Topi's model outside the engine (tools/topi/run.sh): checks, and pictures of the land.
#include "TopiModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace LyudTopi;

static int GFailed = 0, GPassed = 0;
static void Check(bool bOk, const char* Name, double Value)
{
	std::printf("%s  %s (%.3f)\n", bOk ? "ok  " : "FAIL", Name, Value);
	bOk ? ++GPassed : ++GFailed;
}

static bool Load(FModel& M, const char* Path)
{
	FILE* F = std::fopen(Path, "rb");
	if (!F)
	{
		return false;
	}
	std::vector<uint8_t> Bytes;
	uint8_t Buf[65536];
	size_t N;
	while ((N = std::fread(Buf, 1, sizeof(Buf), F)) > 0)
	{
		Bytes.insert(Bytes.end(), Buf, Buf + N);
	}
	std::fclose(F);
	const bool bOk = M.Edge.Read(Bytes.data(), Bytes.size());
	M.Prepare();
	return bOk;
}

static double SlopeDeg(const FModel& M, double X, double Y, double E = 1.0)
{
	const double HX = (M.Height(X + E, Y) - M.Height(X - E, Y)) / (2 * E);
	const double HY = (M.Height(X, Y + E) - M.Height(X, Y - E)) / (2 * E);
	return std::atan(std::hypot(HX, HY)) * 180.0 / 3.14159265358979;
}

static int Test(const FModel& M)
{
	// the sand hides under the map's ground at its north edge and stands over it where Topi is whole
	double WorstNorth = -1e9, WorstSouth = 1e9;
	for (double X = -1290.0; X <= 250.0; X += 7.0)
	{
		WorstNorth = std::fmax(WorstNorth, M.Height(X, -735.0) - M.Edge.At(X, -735.0));
		for (double Y = -899.0; Y <= -893.0; Y += 3.0)
		{
			WorstSouth = std::fmin(WorstSouth, M.Height(X, Y) - M.Edge.At(X, Y));
		}
	}
	Check(WorstNorth < -4.0, "the sand's skirt is under the map's ground at its north edge, m", WorstNorth);
	Check(WorstSouth > 3.0, "the sand covers the map's ground at the map's south edge, m", WorstSouth);

	// continuity: no steps anywhere (a 0.5 m move never changes the height by more than a slip face allows)
	double WorstStep = 0.0, WorstSlope = 0.0, MaxZ = -1e9, MinLand = 1e9;
	unsigned Seed = 12345;
	const auto Rand = [&Seed] { Seed = Seed * 1664525u + 1013904223u; return double(Seed >> 8) / double(1 << 24); };
	for (int I = 0; I < 200000; ++I)
	{
		const double Y = -740.0 - Rand() * 2600.0;
		const double X = FModel::ShoreX(Y) - 70.0 - Rand() * 2200.0;
		const double A = Rand() * 6.2831853;
		const double Z0 = M.Height(X, Y), Z1 = M.Height(X + 0.5 * std::cos(A), Y + 0.5 * std::sin(A));
		const bool bShows = Y < FullY || Z0 > M.Edge.At(X, Y);      // not the part hidden under the map's own ground
		if (FModel::Region(X, Y) > 0.45 && bShows)
		{
			WorstStep = std::fmax(WorstStep, std::fabs(Z1 - Z0));
		}
		if (FModel::Region(X, Y) >= 1.0 && bShows)
		{
			WorstSlope = std::fmax(WorstSlope, SlopeDeg(M, X, Y));
			MinLand = std::fmin(MinLand, Z0);
		}
		MaxZ = std::fmax(MaxZ, Z0);
	}
	Check(WorstStep < 0.75, "no steps in the ground (worst change over 0.5 m), m", WorstStep);
	Check(WorstSlope < 50.0, "no sand cliffs (the steepest face, where the sand leans on the map's cliff), degrees", WorstSlope);
	Check(MinLand > SeaZ + 0.5, "the land stays over the sea", MinLand);
	Check(MaxZ > 10.0 && MaxZ < 400.0, "the highest sand, m", MaxZ);

	// the far ground (coarse cells) stays close to the fine one on average
	double Diff = 0.0;
	int N = 0;
	for (double X = -3000.0; X < 0.0; X += 97.0)
	{
		for (double Y = -4000.0; Y < -1000.0; Y += 89.0)
		{
			Diff += M.Height(X, Y, 128.0) - M.Height(X, Y);
			++N;
		}
	}
	Check(std::fabs(Diff / N) < 1.5, "coarse ground sits at the fine ground's mean, m", Diff / N);

	// the path: walkable, and it ends at the water
	double PathSteep = 0.0;
	for (double S = 0.0; S <= FModel::PathLength(); S += 2.0)
	{
		const FVec2 A = FModel::PathPoint(S), B = FModel::PathPoint(S + 2.0);
		PathSteep = std::fmax(PathSteep, std::fabs(M.Height(B.X, B.Y) - M.Height(A.X, A.Y)) / 2.0);
	}
	Check(std::atan(PathSteep) * 57.2958 < 36.0, "the path's steepest step, degrees", std::atan(PathSteep) * 57.2958);
	Check(FModel::PathLength() > 900.0 && FModel::PathLength() < 1500.0, "the path's length, m", FModel::PathLength());
	const FVec2 End = FModel::Path().back();
	Check(std::fabs(FModel::RiverOffset(End.X, End.Y)) < RiverHalfWidth + 12.0, "the path ends on the river's bank, m from the middle", FModel::RiverOffset(End.X, End.Y));
	const double MidY = FModel::RiverY(End.X);
	Check(M.RiverDepth(End.X, MidY) > 1.8, "the river is deep enough to go under in the middle, m", M.RiverDepth(End.X, MidY));
	Check(M.RiverDepth(End.X, MidY + RiverHalfWidth + 8.0) == 0.0, "the bank is dry", M.Height(End.X, MidY + RiverHalfWidth + 8.0) - RiverZ);
	const FVec2 Flow = FModel::RiverFlow(End.X, MidY);
	Check(Flow.X > 1.5, "the river runs west, fast, m/s", Flow.X);
	Check(FModel::RiverStrength(FModel::ShoreX(MidY) - 100.0) == 0.0, "the river has sunk into the sand before the coast", 0.0);

	// the mind
	Check(FModel::Pressure(250.0, -800.0, false) == 0.0, "no pressure before Topi", 0.0);
	Check(FModel::Pressure(200.0, -1300.0, false) >= 1.0, "by day the hero cannot stand 400 m in", FModel::Pressure(200.0, -1300.0, false));
	double PathWorst = 0.0;
	for (double S = 0.0; S <= FModel::PathLength(); S += 5.0)
	{
		const FVec2 P = FModel::PathPoint(S);
		PathWorst = std::fmax(PathWorst, FModel::Pressure(P.X, P.Y, true));
		PathWorst = std::fmax(PathWorst, FModel::Pressure(P.X + 15.0, P.Y, true));
	}
	Check(PathWorst == 0.0, "at night the path is free of pressure", PathWorst);
	Check(FModel::Pressure(-200.0, -1300.0, true) >= 1.0, "at night away from the path he cannot stand it", FModel::Pressure(-200.0, -1300.0, true));
	Check(FModel::Pressure(End.X + 60.0, MidY, true) == 0.0, "calm in the river", 0.0);
	Check(FModel::Pressure(End.X, MidY - 400.0, true) >= 1.0, "no way past the river", FModel::Pressure(End.X, MidY - 400.0, true));
	const FVec2 Out = FModel::WayOut(120.0, -1350.0);
	Check(Out.Y > 0.3, "the way out leads north", Out.Y);

	std::printf("%d passed, %d failed\n", GPassed, GFailed);
	return GFailed ? 1 : 0;
}

// A shaded picture: X0..X1 (west on the left like the M map), Y0..Y1, metres per pixel.
static void Shade(const FModel& M, const char* File, double XW, double XE, double YN, double YS, double Step, bool bMarks)
{
	const int W = int((XW - XE) / Step), H = int((YN - YS) / Step);
	std::vector<uint8_t> Px(size_t(W) * H * 3);
	for (int J = 0; J < H; ++J)
	{
		for (int I = 0; I < W; ++I)
		{
			const double X = XW - I * Step, Y = YN - J * Step;
			FSample S;
			M.Sample(X, Y, Step * 0.5, S);
			const double E = Step * 0.5;
			const double HX = (M.Height(X + E, Y, E) - M.Height(X - E, Y, E)) / (2 * E);
			const double HY = (M.Height(X, Y + E, E) - M.Height(X, Y - E, E)) / (2 * E);
			const double NL = std::sqrt(HX * HX + HY * HY + 1.0);
			// low sun from the south-west
			double L = (-HX * 0.55 + HY * 0.45 + 0.7) / NL / 0.99;
			L = std::fmin(std::fmax(L, 0.08), 1.25);
			double R = 226, G = 205, B = 160;
			if (S.Region < 0.5) { R = 96; G = 118; B = 78; }
			R = R * (1 - 0.45 * S.Grass) + 120 * 0.45 * S.Grass;
			G = G * (1 - 0.45 * S.Grass) + 150 * 0.45 * S.Grass;
			B = B * (1 - 0.45 * S.Grass) + 90 * 0.45 * S.Grass;
			if (S.Z < SeaZ) { R = 60; G = 110; B = 150; L = 1.0; }
			if (M.RiverDepth(X, Y) > 0.0) { R = 90; G = 235; B = 215; L = 1.0; }
			if (bMarks && FModel::PathDistance(X, Y) < Step * 0.8) { R = 230; G = 60; B = 40; L = 1.0; }
			uint8_t* P = &Px[(size_t(J) * W + I) * 3];
			P[0] = uint8_t(std::fmin(255.0, R * L));
			P[1] = uint8_t(std::fmin(255.0, G * L));
			P[2] = uint8_t(std::fmin(255.0, B * L));
		}
	}
	FILE* F = std::fopen(File, "wb");
	std::fprintf(F, "P6\n%d %d\n255\n", W, H);
	std::fwrite(Px.data(), 1, Px.size(), F);
	std::fclose(F);
	std::printf("wrote %s (%d x %d)\n", File, W, H);
}

int main(int Argc, char** Argv)
{
	FModel M;
	if (!Load(M, "unreal/Lyudmilovsk/Content/Topi/topi_edge.tbl"))
	{
		std::printf("no unreal/Lyudmilovsk/Content/Topi/topi_edge.tbl: run python3 tools/topi.py\n");
		return 1;
	}
	const std::string Cmd = Argc > 1 ? Argv[1] : "test";
	if (Cmd == "test")
	{
		return Test(M);
	}
	if (Cmd == "map")
	{
		Shade(M, "Saved/Topi/topi_near.ppm", 900.0, -700.0, -700.0, -2100.0, 1.0, true);
		Shade(M, "Saved/Topi/topi_far.ppm", 6000.0, -10000.0, -700.0, -12700.0, 10.0, false);
		return 0;
	}
	if (Cmd == "profile")
	{
		// heights along the path: metres along, x, y, z, slope
		for (double S = 0.0; S <= FModel::PathLength(); S += 10.0)
		{
			const FVec2 P = FModel::PathPoint(S);
			std::printf("%6.0f %7.1f %7.1f %7.2f\n", S, P.X, P.Y, M.Height(P.X, P.Y));
		}
		return 0;
	}
	if (Cmd == "section" && Argc > 6)
	{
		// run.sh section x0 y0 x1 y1 n: ground, the map's own ground and the region along a line
		const double AX = std::atof(Argv[2]), AY = std::atof(Argv[3]), BX = std::atof(Argv[4]), BY = std::atof(Argv[5]);
		const int N = std::atoi(Argv[6]);
		for (int I = 0; I <= N; ++I)
		{
			const double X = AX + (BX - AX) * I / N, Y = AY + (BY - AY) * I / N;
			std::printf("%8.1f %8.1f  z %7.2f  map %7.2f  region %.2f  shore-x %6.1f\n", X, Y, M.Height(X, Y), M.Edge.At(X, Y), FModel::Region(X, Y), FModel::ShoreX(Y) - X);
		}
		return 0;
	}
	std::printf("usage: run.sh test|map|profile|section\n");
	return 1;
}
