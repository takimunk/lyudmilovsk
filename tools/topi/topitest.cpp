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

// A point given in Topi's own frame (TopiModel.cpp: LX along the edge, LY across it, the sand from LY -775 on).
static FVec2 L2M(double LX, double LY) { return FModel::FromLocal(LX, LY); }

static int Test(const FModel& M)
{
	// the two frames agree
	const FVec2 Back = FModel::ToLocal(L2M(123.0, -1456.0).X, L2M(123.0, -1456.0).Y);
	Check(std::fabs(Back.X - 123.0) + std::fabs(Back.Y + 1456.0) < 1e-9, "local <-> map round trip", Back.X);
	Check(std::fabs(L2M(0.0, -900.0).X - WestX) < 1e-9, "Topi is whole at the map's east edge", L2M(0.0, -900.0).X);

	// the sand hides under the map's ground before its edge and stands over it where the map ends
	double WorstBefore = -1e9, WorstEdge = 1e9;
	for (double Y = M.Edge.Y0 + 70.0; Y <= M.Edge.Y0 + (M.Edge.NY - 1) * M.Edge.Cell - 70.0; Y += 7.0)
	{
		WorstBefore = std::fmax(WorstBefore, M.Height(WestX + 165.0, Y) - M.Edge.At(WestX + 165.0, Y));
		for (double X = WestX + 1.0; X <= WestX + 7.0; X += 3.0)
		{
			WorstEdge = std::fmin(WorstEdge, M.Height(X, Y) - M.Edge.At(X, Y));
		}
	}
	Check(WorstBefore < -4.0, "the sand's skirt is under the map's ground before Topi, m", WorstBefore);
	Check(WorstEdge > 3.0, "the sand covers the map's ground at the map's east edge, m", WorstEdge);

	// continuity: no steps anywhere (a 0.5 m move never changes the height by more than a slip face allows)
	double WorstStep = 0.0, WorstSlope = 0.0, MaxZ = -1e9, MinLand = 1e9;
	unsigned Seed = 12345;
	const auto Rand = [&Seed] { Seed = Seed * 1664525u + 1013904223u; return double(Seed >> 8) / double(1 << 24); };
	for (int I = 0; I < 200000; ++I)
	{
		const FVec2 P = L2M(-1200.0 + Rand() * 3400.0, -740.0 - Rand() * 2600.0);
		const double X = P.X, Y = P.Y;
		const double A = Rand() * 6.2831853;
		const double Z0 = M.Height(X, Y), Z1 = M.Height(X + 0.5 * std::cos(A), Y + 0.5 * std::sin(A));
		const bool bShows = X < WestX || Z0 > M.Edge.At(X, Y);      // not the part hidden under the map's own ground
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
	Check(WorstSlope < 50.0, "no sand cliffs (the steepest face), degrees", WorstSlope);
	Check(MinLand > BaseZ - 12.0, "the lowest sand (the river's bed), m", MinLand);
	Check(MaxZ > 10.0 && MaxZ < 400.0, "the highest sand, m", MaxZ);

	// the far ground (coarse cells) stays close to the fine one on average
	double Diff = 0.0;
	int N = 0;
	for (double LX = -1500.0; LX < 1500.0; LX += 97.0)
	{
		for (double LY = -4000.0; LY < -1000.0; LY += 89.0)
		{
			const FVec2 P = L2M(LX, LY);
			Diff += M.Height(P.X, P.Y, 128.0) - M.Height(P.X, P.Y);
			++N;
		}
	}
	Check(std::fabs(Diff / N) < 1.5, "coarse ground sits at the fine ground's mean, m", Diff / N);

	// the path: it starts on the town's main axis, is walkable, and ends at the water
	const FVec2 Start = FModel::Path().front();
	Check(std::fabs(Start.Y - 905.0) < 1.0 && Start.X < -1100.0 && Start.X > WestX, "the path starts east of the town on its main axis, X", Start.X);
	double PathSteep = 0.0;
	for (double S = 0.0; S <= FModel::PathLength(); S += 2.0)
	{
		const FVec2 A = FModel::PathPoint(S), B = FModel::PathPoint(S + 2.0);
		PathSteep = std::fmax(PathSteep, std::fabs(M.Height(B.X, B.Y) - M.Height(A.X, A.Y)) / 2.0);
	}
	Check(std::atan(PathSteep) * 57.2958 < 36.0, "the path's steepest step, degrees", std::atan(PathSteep) * 57.2958);
	Check(FModel::PathLength() > 900.0 && FModel::PathLength() < 1500.0, "the path's length, m", FModel::PathLength());
	const FVec2 End = FModel::Path().back();
	Check(End.X < WestX - 700.0, "the path leads east, deep into Topi, X", End.X);
	const double EndOff = FModel::RiverOffset(End.X, End.Y);
	Check(EndOff < 0.0 && EndOff > -(RiverHalfWidth + 12.0), "the path ends on the river's near bank, m from the middle", EndOff);
	const FVec2 Mid = FModel::RiverPoint(0.0);
	const FVec2 Dir = FModel::RiverDirection(0.0);
	Check(std::fabs(FModel::RiverOffset(Mid.X, Mid.Y)) < 0.01, "RiverPoint lies on the middle line", FModel::RiverOffset(Mid.X, Mid.Y));
	Check(M.RiverDepth(Mid.X, Mid.Y) > 1.8, "the river is deep enough to go under in the middle, m", M.RiverDepth(Mid.X, Mid.Y));
	// across the stream, toward the town: (-Dir.Y, Dir.X) turned so that it points west
	const double AX = Dir.Y < 0.0 ? -Dir.Y : Dir.Y, AY = Dir.Y < 0.0 ? Dir.X : -Dir.X;
	const double BankX = Mid.X + AX * (RiverHalfWidth + 8.0), BankY = Mid.Y + AY * (RiverHalfWidth + 8.0);
	Check(M.RiverDepth(BankX, BankY) == 0.0 && FModel::RiverOffset(BankX, BankY) < 0.0, "the near bank is dry, m over the water", M.Height(BankX, BankY) - RiverZ);
	const FVec2 Flow = FModel::RiverFlow(Mid.X, Mid.Y);
	Check(Flow.Y < -1.5 && Flow.X * Dir.X + Flow.Y * Dir.Y > 2.0, "the river runs south, fast, m/s", -Flow.Y);
	const FVec2 Next = FModel::RiverPoint(200.0);
	Check(std::fabs(FModel::RiverOffset(Next.X, Next.Y)) < 0.01 && Next.Y < Mid.Y - 150.0, "RiverPoint follows the stream", Mid.Y - Next.Y);

	// the mind
	const FVec2 Before = L2M(250.0, -800.0), DayIn = L2M(200.0, -1300.0), NightOff = L2M(600.0, -1300.0);
	Check(FModel::Pressure(Before.X, Before.Y, false) == 0.0, "no pressure before Topi", 0.0);
	Check(FModel::Pressure(-800.0, 905.0, false) == 0.0 && !FModel::Near(-800.0, 905.0, 150.0), "nothing of Topi in the town", 0.0);
	Check(FModel::Near(Start.X, Start.Y, 0.0) && FModel::Near(End.X, End.Y, 0.0), "Near() knows Topi", 1.0);
	Check(FModel::Pressure(DayIn.X, DayIn.Y, false) >= 1.0, "by day the hero cannot stand 400 m in", FModel::Pressure(DayIn.X, DayIn.Y, false));
	double PathWorst = 0.0;
	for (double S = 0.0; S <= FModel::PathLength(); S += 5.0)
	{
		const FVec2 P = FModel::PathPoint(S);
		PathWorst = std::fmax(PathWorst, FModel::Pressure(P.X, P.Y, true));
		PathWorst = std::fmax(PathWorst, FModel::Pressure(P.X, P.Y + 15.0, true));
	}
	Check(PathWorst == 0.0, "at night the path is free of pressure", PathWorst);
	Check(FModel::Pressure(NightOff.X, NightOff.Y, true) >= 1.0, "at night away from the path he cannot stand it", FModel::Pressure(NightOff.X, NightOff.Y, true));
	const FVec2 InRiver = FModel::RiverPoint(60.0);
	Check(FModel::Pressure(InRiver.X, InRiver.Y, true) == 0.0, "calm in the river", 0.0);
	Check(FModel::Pressure(Mid.X - 400.0, Mid.Y, true) >= 1.0, "no way past the river", FModel::Pressure(Mid.X - 400.0, Mid.Y, true));
	const FVec2 OutWay = FModel::WayOut(L2M(120.0, -1350.0).X, L2M(120.0, -1350.0).Y);
	Check(OutWay.X > 0.3, "the way out leads west, back to the town", OutWay.X);
	const FVec2 OutFar = FModel::WayOut(L2M(900.0, -1350.0).X, L2M(900.0, -1350.0).Y);
	Check(OutFar.X > 0.99, "away from the path the way out is straight west", OutFar.X);

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
		Shade(M, "Saved/Topi/topi_near.ppm", -1050.0, -2450.0, 1700.0, 100.0, 1.0, true);
		Shade(M, "Saved/Topi/topi_far.ppm", -1000.0, -13000.0, 7000.0, -5000.0, 10.0, false);
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
			std::printf("%8.1f %8.1f  z %7.2f  map %7.2f  region %.2f  in %6.1f\n", X, Y, M.Height(X, Y), M.Edge.At(X, Y), FModel::Region(X, Y), FModel::Depth(X, Y));
		}
		return 0;
	}
	std::printf("usage: run.sh test|map|profile|section\n");
	return 1;
}
