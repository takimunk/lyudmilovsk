// Checks of the snow and sea-ice model (Private/Winter/SnowModel.*) on a made-up world: a coast to the west (X grows
// to the sea, as the game's map), a valley floor, a hill, a house 16 m high, a path past it, a railway bank out to sea.
// tools/winter/run.sh test | perf | images
#include "SnowModel.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <algorithm>
#include <string>
#include <thread>
#include <vector>

using namespace LyudWinter;

static int Fails = 0;
static void Check(bool bOk, const char* What, double A = 0.0, double B = 0.0)
{
	std::printf("%s %-58s %10.3f %10.3f\n", bOk ? "ok  " : "FAIL", What, A, B);
	if (!bOk)
	{
		Fails++;
	}
}

static void Threads(int N, const std::function<void(int)>& Body)
{
	const int T = std::max(1u, std::thread::hardware_concurrency());
	std::vector<std::thread> Pool;
	std::atomic<int> Next(0);
	for (int K = 0; K < T; K++)
	{
		Pool.emplace_back([&]() { for (int I; (I = Next++) < N;) Body(I); });
	}
	for (std::thread& Th : Pool)
	{
		Th.join();
	}
}

static double Now()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// the made-up world, world cm. The shore runs along X = 0 (the sea at X > 0), the beach rises 6% to the east.
static const float SeaZ = -4300.f;
static float GroundOf(double X, double Y)
{
	const double XM = X * 0.01, YM = Y * 0.01;
	double Z = XM > 0 ? -XM * 0.06 : std::min(-XM * 0.06, 4.0); // the beach up to a 4 m terrace
	// a round hill east of the house
	const double HX = -260.0, HY = 120.0;
	Z += 18.0 * std::exp(-((XM - HX) * (XM - HX) + (YM - HY) * (YM - HY)) / (2 * 40.0 * 40.0));
	// the railway's bank out to sea at Y = -100 m: 6 m wide, its top 0.3 m under high water
	if (XM > -20 && std::fabs(YM + 100.0) < 3.0)
	{
		Z = std::max(Z, -0.3);
	}
	return SeaZ + float(Z * 100.0);
}
// the house: 30 x 12 m, 16 m high, at (-120, 0) m
static bool InHouse(double X, double Y) { return std::fabs(X * 0.01 + 120.0) < 15.0 && std::fabs(Y * 0.01) < 6.0; }
static bool OnPath(double X, double Y)
{
	// a path along Y = 12 m past the house's north side, and one down to the beach
	const double XM = X * 0.01, YM = Y * 0.01;
	return (std::fabs(YM - 12.0) < 1.0 && XM > -200 && XM < -30) || (std::fabs(XM + 40.0) < 1.0 && YM > -60 && YM < 12);
}

static void MakeCover(FCover& C, int W, int H, double X0, double Y0)
{
	C.Init(W, H, X0, Y0, 200.f);
	for (int Y = 0; Y < H; Y++)
	{
		for (int X = 0; X < W; X++)
		{
			const double WX = X0 + X * 200.0, WY = Y0 + Y * 200.0;
			const int I = C.Index(X, Y);
			C.Ground[I] = GroundOf(WX, WY);
			C.Top[I] = InHouse(WX, WY) ? C.Ground[I] + 1600.f : C.Ground[I];
			// a lamp post: a lone tall cell that must not make a hole in the snow
			if (X == W / 2 + 7 && Y == H / 2 + 9)
			{
				C.Top[I] = C.Ground[I] + 600.f;
			}
		}
	}
	C.Classify(SeaZ);
	C.CleanRoofs();
}

static void WritePgm(const std::string& Path, int W, int H, const std::function<float(int, int)>& F)
{
	FILE* Fp = std::fopen(Path.c_str(), "wb");
	if (!Fp)
	{
		return;
	}
	std::fprintf(Fp, "P5\n%d %d\n255\n", W, H);
	for (int Y = H - 1; Y >= 0; Y--)
	{
		for (int X = W - 1; X >= 0; X--) // west (the sea) to the left
		{
			const unsigned char V = (unsigned char)std::lround(std::min(std::max(F(X, Y), 0.f), 1.f) * 255.f);
			std::fputc(V, Fp);
		}
	}
	std::fclose(Fp);
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	if (Mode == "perf")
	{
		// the game's map: 2650 x 3150 m at 2 m cover, 1 m field
		FCover C;
		const double T0 = Now();
		MakeCover(C, 1325, 1575, -132500.0, -90000.0);
		FDriftBasis B;
		const double T1 = Now();
		B.Build(C, Threads);
		const double T2 = Now();
		FSnowField F;
		F.Setup(C, 100.f);
		FSnowInputs In;
		In.LevelCm = 30.f;
		In.Transport = 0.8f;
		F.UpdateFactor(C, B, In, Threads);
		const double T3 = Now();
		std::vector<uint8_t> Mask(F.Depth.size(), 0);
		for (int Y = 0; Y < F.H; Y++)
			for (int X = 0; X < F.W; X++)
				Mask[size_t(Y) * F.W + X] = OnPath(F.X0 + X * 100.0, F.Y0 + Y * 100.0) || (X % 97 < 2) ? 1 : 0;
		FSnowPaths P;
		P.Build(F.W, F.H, F.X0, F.Y0, F.Cell, Mask);
		const double T4 = Now();
		F.ComposeAll(C, In, &P, Threads);
		const double T5 = Now();
		FIceMask Ice;
		Ice.Init(1333, 1033, -130000.0, -90000.0, 300.f);
		for (int Y = 0; Y < Ice.H; Y++)
			for (int X = 0; X < Ice.W; X++)
				Ice.Bed[size_t(Y) * Ice.W + X] = GroundOf(Ice.X0 + X * 300.0, Ice.Y0 + Y * 300.0);
		const double T6 = Now();
		Ice.Update(SeaZ, 0.4f, Threads);
		const double T7 = Now();
		std::printf("cover %.0f ms (made up), basis %.0f ms, factor %.0f ms, paths %.0f ms (%zu patches), compose %.0f ms, ice dt %.0f ms\n",
			(T1 - T0) * 1e3, (T2 - T1) * 1e3, (T3 - T2) * 1e3, (T4 - T3) * 1e3, P.Patches.size(), (T5 - T4) * 1e3, (T7 - T6) * 1e3);
		return 0;
	}

	// a 600 x 400 m world: X -500..100 m (the sea west of 0), Y -200..200 m
	FCover C;
	MakeCover(C, 300, 200, -50000.0, -20000.0);
	FDriftBasis B;
	B.Build(C, Threads);
	FSnowField F;
	F.Setup(C, 100.f);
	FSnowInputs In;
	In.LevelCm = 30.f;
	In.Transport = 1.f;
	for (float& R : In.Rose) R = 0.f;
	AddToRose(In.Rose, 270.f, 12.f, 1.f); // from the west, off the sea: to the east is downwind (-X)
	F.UpdateFactor(C, B, In, Threads);
	std::vector<uint8_t> Mask(F.Depth.size(), 0);
	for (int Y = 0; Y < F.H; Y++)
		for (int X = 0; X < F.W; X++)
			Mask[size_t(Y) * F.W + X] = OnPath(F.X0 + X * 100.0, F.Y0 + Y * 100.0) ? 1 : 0;
	FSnowPaths P;
	P.Build(F.W, F.H, F.X0, F.Y0, F.Cell, Mask);
	SettlePaths(P, In, 0.0);
	F.ComposeAll(C, In, &P, Threads);

	auto Depth = [&](double XM, double YM) { return F.DepthAt(XM * 100.0, YM * 100.0); };
	// the wind is from the west (X+): the house's lee is its east side (smaller X)
	const float Lee = Depth(-139.0, -2.0), Windward = Depth(-101.0, -2.0), Open = Depth(-60.0, -40.0), Inside = Depth(-120.0, 0.0);
	Check(Inside == 0.f, "no snow inside the house", Inside);
	Check(Lee > 0.75f, "a deep drift in the house's lee (m)", Lee);
	Check(Lee > Open * 2.f, "lee drift deeper than open ground", Lee, Open);
	Check(Windward < Open, "the windward foot scooped", Windward, Open);
	const float HillTop = Depth(-260.0, 120.0), HillLee = Depth(-300.0, 120.0);
	Check(HillTop < In.LevelCm * 0.01f * 0.8f, "the hill's top blown thin", HillTop);
	Check(HillLee > HillTop * 1.5f, "the hill's lee side deeper than its top", HillLee, HillTop);
	float Max = 0.f;
	double Sum = 0.0;
	int N = 0;
	for (int Y = 0; Y < F.H; Y++)
		for (int X = 0; X < F.W; X++)
		{
			const size_t I = size_t(Y) * F.W + X;
			Max = std::max(Max, F.Depth[I]);
			if (F.Open[I] && P.Patch[I] < 0 && P.Near[I] < 0)
			{
				Sum += F.DriftedM(C, In, X, Y);
				N++;
			}
		}
	Check(Max <= In.MaxDepthM + 0.26f, "no drift deeper than the cap (+berm)", Max, In.MaxDepthM);
	Check(std::fabs(Sum / N - In.LevelCm * 0.01) < 0.06, "snow moved, not made: mean ~ level", Sum / N, In.LevelCm * 0.01);
	const float LampFoot = F.DepthAt(F.X0 + (C.W / 2 + 7) * 200.0, F.Y0 + (C.H / 2 + 9) * 200.0);
	Check(LampFoot > 0.05f, "a lamp post makes no hole in the snow", LampFoot);
	const float PathD = Depth(-150.0, 12.0), BermD = Depth(-150.0, 13.0 + 0.9), Beside = Depth(-150.0, 18.0);
	Check(PathD < 0.005f, "the path is clear", PathD);
	Check(BermD > Beside + 0.15f, "a berm of shovelled snow beside it", BermD, Beside);
	Check(!P.Patches.empty() && P.Patches.size() < 40, "the paths cut into patches", double(P.Patches.size()));

	// it snows on the cleared paths, then somebody clears one stretch halfway
	std::vector<int> Changed;
	In.SnowfallCmPerHour = 2.f;
	StepPaths(P, In, In.LevelCm, 1.0, 3.f, -150.0 * 100.0, 12.0 * 100.0, 4.f, 60.f, Changed);
	F.ComposeAll(C, In, &P, Threads);
	Check(Depth(-150.0, 12.0) > 0.05f, "fresh snow lies on the path", Depth(-150.0, 12.0));
	const int Pk = P.PatchAt(-150.0 * 100.0, 12.0 * 100.0, F.W, F.H, F.X0, F.Y0, F.Cell);
	Check(Pk >= 0, "a patch under the path", Pk);
	if (Pk >= 0)
	{
		FPatch& Pt = P.Patches[Pk];
		Pt.FreshBefore = Pt.FreshCm;
		Pt.FreshCm = 0.f;
		Pt.Progress = 0.5f;
		F.ComposeAll(C, In, &P, Threads);
		const double MidX = F.X0 + (Pt.bAlongX ? (Pt.MinX + 0.25 * (Pt.MaxX - Pt.MinX)) : Pt.MinX) * F.Cell;
		const double LateX = F.X0 + (Pt.bAlongX ? (Pt.MinX + 0.85 * (Pt.MaxX - Pt.MinX)) : Pt.MinX) * F.Cell;
		const float Swept = F.DepthAt(MidX, Pt.Y), Unswept = F.DepthAt(LateX, Pt.Y);
		Check(Swept < 0.005f && Unswept > 0.05f, "half the patch swept, half not yet", Swept, Unswept);
	}
	// far patches clear by themselves after some hours (the camera stays by the house)
	In.SnowfallCmPerHour = 0.f;
	int Cleared = 0, Snowy = 0;
	StepPaths(P, In, In.LevelCm, 20.0, 1.f, -150.0 * 100.0, 12.0 * 100.0, 4.f, 60.f, Changed);
	for (const FPatch& Pt : P.Patches)
	{
		const double DX = (Pt.X + 15000.0) * 0.01, DY = (Pt.Y - 1200.0) * 0.01;
		if (DX * DX + DY * DY > 60.0 * 60.0)
		{
			(Pt.FreshCm < 0.1f ? Cleared : Snowy)++;
		}
	}
	Check(Cleared > 0 && Snowy == 0, "far patches cleared by somebody unseen", Cleared, Snowy);

	// sea ice: the strip along the shore stays open; the tide moves it; the bank keeps a lead
	FIceMask Ice;
	Ice.Init(240, 140, -10000.0, -20000.0, 300.f); // X -100..620 m
	for (int Y = 0; Y < Ice.H; Y++)
		for (int X = 0; X < Ice.W; X++)
			Ice.Bed[size_t(Y) * Ice.W + X] = GroundOf(Ice.X0 + X * 300.0, Ice.Y0 + Y * 300.0);
	FIceParams IP;
	IP.ThicknessCm = 25.f;
	IP.Cover = 1.f;
	IP.SeaZ = SeaZ;
	Ice.Update(IP.SeaZ, IP.ShallowM, Threads);
	// the shore at high water: where the depth is ShallowM, X = 0.4/0.06 = 6.7 m
	auto IceAtM = [&](double XM, double YM) { return Ice.IceAt(XM * 100.0, YM * 100.0, IP); };
	float NearMax = 0.f, FarMin = 1.f;
	for (double Y = 0.0; Y < 150.0; Y += 1.0)
	{
		NearMax = std::max(NearMax, IceAtM(6.7 + 7.0, Y));
		FarMin = std::min(FarMin, IceAtM(6.7 + 19.0, Y));
	}
	Check(NearMax == 0.f, "open water 7 m off the waterline", NearMax);
	Check(FarMin == 1.f, "solid ice 19 m off the waterline (past the pancakes)", FarMin);
	// low water 2 m down: the waterline walks out to X = 2.4/0.06 = 40 m
	IP.SeaZ = SeaZ - 200.f;
	Ice.Update(IP.SeaZ, IP.ShallowM, Threads);
	Check(IceAtM(40.0 + 7.0, 50.0) == 0.f && IceAtM(40.0 + 19.0, 50.0) == 1.f, "the strip follows the tide", IceAtM(47.0, 50.0), IceAtM(59.0, 50.0));
	IP.SeaZ = SeaZ;
	Ice.Update(IP.SeaZ, IP.ShallowM, Threads);
	Check(IceAtM(300.0, -100.0 + 3.0 + 6.0) == 0.f && IceAtM(300.0, -100.0 + 3.0 + 22.0) == 1.f, "a lead along the railway's bank",
		IceAtM(300.0, -91.0), IceAtM(300.0, -77.0));
	IP.RailX0 = 50000.0;
	IP.RailX1 = 2000000.0;
	IP.RailY = -10000.0;
	Check(Ice.IceAt(900.0 * 100.0, -100.0 * 100.0 + 800.0, IP) == 0.f && Ice.IceAt(900.0 * 100.0, -100.0 * 100.0 + 2700.0, IP) == 1.f,
		"the lead goes on along the line beyond the grid");
	IP.Cover = 0.5f;
	int On = 0, Tot = 0;
	for (double X = 100.0; X < 600.0; X += 7.0)
		for (double Y = 0.0; Y < 200.0; Y += 7.0)
		{
			On += IceAtM(X, Y) > 0.5f;
			Tot++;
		}
	Check(On > Tot * 0.25 && On < Tot * 0.75, "half cover: floes and leads", double(On) / Tot);
	Check(!Ice.Walkable(10.0 * 100.0, 0.0, IP), "no walking on the open strip");
	IP.Cover = 1.f;
	IP.ThicknessCm = 5.f;
	Check(!Ice.Walkable(300.0 * 100.0, 0.0, IP), "5 cm ice does not hold");
	IP.ThicknessCm = 20.f;
	Check(Ice.Walkable(300.0 * 100.0, 0.0, IP), "20 cm ice holds");

	if (Mode == "images")
	{
		WritePgm("Saved/Winter/depth.pgm", F.W, F.H, [&](int X, int Y) { return F.Depth[size_t(Y) * F.W + X] / 1.2f; });
		WritePgm("Saved/Winter/factor.pgm", C.W, C.H, [&](int X, int Y) { return F.Factor[C.Index(X, Y)] / 4.f; });
		IP.Cover = 0.6f;
		WritePgm("Saved/Winter/ice.pgm", 720, 400, [&](int X, int Y) { return Ice.IceAt(-10000.0 + X * 100.0, -20000.0 + Y * 100.0, IP); });
		std::printf("wrote Saved/Winter/depth.pgm factor.pgm ice.pgm\n");
	}
	std::printf("%s (%d failed)\n", Fails ? "FAILED" : "all ok", Fails);
	return Fails ? 1 : 0;
}
