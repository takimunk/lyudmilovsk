// Offline checks of the cruise's seascape (Private/Cruise/SeascapeMath.h) and of a whole voyage of the cruise's model
// against the kit's pieces. Run: tools/seascapesim/run.sh [table]
#include "Cruise/SeascapeMath.h"
#include "Cruise/CruiseModel.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int Fails = 0;
static int Checks = 0;
#define CHECK(c, ...) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string Read(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

static bool Near(double A, double B, double Eps) { return std::fabs(A - B) <= Eps; }

static void Maths()
{
	using namespace LyudSeascape;
	// --- chart -> stage
	{
		FShip S;
		S.Chart = { 12000.0, 7000.0 };
		S.Heading = 0.7;
		S.StageYaw = 0.0;
		// a point a kilometre dead ahead on the chart is a kilometre along the stage ship's bow (+X), whatever the heading
		for (double H = -3.0; H <= 3.0; H += 0.37)
		{
			S.Heading = H;
			const FVec2 Ahead = { S.Chart.X + 1000.0 * std::cos(H), S.Chart.Y + 1000.0 * std::sin(H) };
			const FVec2 L = ChartToStage(S, Ahead);
			CHECK(Near(L.X, 1000.0, 1e-6) && Near(L.Y, 0.0, 1e-6), "ahead at heading %.2f -> %.3f %.3f", H, L.X, L.Y);
			// abeam to the side the heading grows towards stays on +Y
			const FVec2 Beam = { S.Chart.X - 500.0 * std::sin(H), S.Chart.Y + 500.0 * std::cos(H) };
			const FVec2 B = ChartToStage(S, Beam);
			CHECK(Near(B.X, 0.0, 1e-6) && Near(B.Y, 500.0, 1e-6), "abeam at heading %.2f -> %.3f %.3f", H, B.X, B.Y);
		}
		// a turned stage ship: ahead is along her bow
		S.StageYaw = 1.2;
		S.Heading = -2.0;
		const FVec2 L = ChartToStage(S, { S.Chart.X + 300.0 * std::cos(-2.0), S.Chart.Y + 300.0 * std::sin(-2.0) });
		CHECK(Near(L.X, 300.0 * std::cos(1.2), 1e-6) && Near(L.Y, 300.0 * std::sin(1.2), 1e-6), "turned stage: %.3f %.3f", L.X, L.Y);
		// distances are kept, yaws turn with the scenery
		const FVec2 Far = ChartToStage(S, { 45000.0, 12000.0 });
		CHECK(Near(std::hypot(Far.X, Far.Y), std::hypot(45000.0 - 12000.0, 12000.0 - 7000.0), 1e-6), "distance kept");
		CHECK(Near(YawToStage(S, 0.3), WrapAngle(0.3 + 1.2 + 2.0), 1e-12), "yaw");
		CHECK(Near(WrapAngle(3.0 * Pi), Pi, 1e-9) || Near(WrapAngle(3.0 * Pi), -Pi, 1e-9), "wrap");
		// the water goes straight aft of the stage ship at her speed
		const FVec2 W = WaterVelocity(S, 3.0);
		CHECK(Near(W.X, -3.0 * std::cos(1.2), 1e-12) && Near(W.Y, -3.0 * std::sin(1.2), 1e-12), "water velocity");
		// when the ship turns to port (the heading grows) an island ahead swings to starboard (-Y... +Y is the side the heading grows towards)
		S.StageYaw = 0.0;
		S.Heading = 0.0;
		const FVec2 Isle = { S.Chart.X + 2000.0, S.Chart.Y };
		S.Heading = 0.2;
		const FVec2 Swung = ChartToStage(S, Isle);
		CHECK(Swung.Y < 0.0 && Swung.X > 0.0, "the scenery swings against the turn: %.1f %.1f", Swung.X, Swung.Y);
	}
	// --- hull down
	{
		const double R = 2.5e6;
		CHECK(HullDownM(1000.0, 8.0, R) == 0.0 && HullDownM(6000.0, 8.0, R) == 0.0, "nothing hidden inside the horizon (%.0f m)", std::sqrt(2.0 * R * 8.0));
		double Last = 0.0;
		for (double D = 0.0; D <= 30000.0; D += 250.0)
		{
			const double H = HullDownM(D, 8.0, R);
			CHECK(H >= Last, "hull down grows with the distance");
			Last = H;
		}
		const double At18 = HullDownM(18000.0, 8.0, R);
		CHECK(At18 > 20.0 && At18 < 40.0, "at 18 km the foot of a 38 m stack is hidden but not its top: %.1f m", At18);
		CHECK(HullDownM(18000.0, 20.0, R) < At18, "a higher eye sees more");
		CHECK(HullDownM(12000.0, 8.0, R) < 8.0, "at 12 km little is hidden: %.1f", HullDownM(12000.0, 8.0, R));
		// shown: in range, inside the air's reach, not wholly under the horizon
		CHECK(IsShown(10000.0, 18000.0, 30000.0, 30.0, HullDownM(10000.0, 8.0, R)), "an island at 10 km is shown");
		CHECK(!IsShown(19000.0, 18000.0, 30000.0, 30.0, 0.0), "out of range");
		CHECK(!IsShown(10000.0, 18000.0, 5000.0, 30.0, 0.0), "in a mist of 5 km nothing at 10 km");
		CHECK(!IsShown(17000.0, 18000.0, 30000.0, 3.0, HullDownM(17000.0, 8.0, R)), "rocks awash are under the horizon at 17 km");
		// the far level: no flapping about the threshold
		CHECK(!UseFar(4100.0, 4000.0, false) && UseFar(4100.0, 4000.0, true) && UseFar(4500.0, 4000.0, false) && !UseFar(3500.0, 4000.0, true), "far level hysteresis");
	}
	// --- the light: one flash a turn, towards the eye
	{
		int Flashes = 0;
		bool bOn = false;
		for (double T = 0.0; T < 30.0; T += 0.01)
		{
			const bool bNow = BeamAt(T, 10.0, 1.0, 0.14) > 0.5;
			Flashes += bNow && !bOn ? 1 : 0;
			bOn = bNow;
		}
		CHECK(Flashes == 3, "three flashes in thirty seconds, got %d", Flashes);
		CHECK(BeamAt(10.0 * (1.0 / (2.0 * Pi)), 10.0, 1.0, 0.14) > 0.999, "brightest when the beam points at the eye");
	}
	// --- the wake
	{
		const FWakeRates A = WakeRates(0.0f, 2.f, 1.f);
		CHECK(A.Strength == 0.f && A.BowRings == 0.f && A.SideStreaks == 0.f && A.SternPatches == 0.f && A.Spray == 0.f, "nothing at anchor");
		const FWakeRates Slow = WakeRates(1.5f, 0.5f, 1.f), Full = WakeRates(3.5f, 0.5f, 1.f);
		CHECK(Slow.Strength < Full.Strength && Slow.BowRings < Full.BowRings && Slow.SideStreaks < Full.SideStreaks, "more foam at more speed");
		CHECK(Full.Strength == 1.f, "full strength at the cruise's full speed");
		CHECK(Slow.Spray == 0.f && WakeRates(3.5f, 2.2f, 1.f).Spray > 20.f && WakeRates(3.5f, 2.2f, 0.f).Spray == 0.f, "spray only in a head sea");
		for (float V = 0.f; V <= 6.f; V += 0.25f)
		{
			const FWakeRates R = WakeRates(V, 1.f, 1.f);
			CHECK(WakeMaxFoam(V) <= 900, "the pool (900) holds the foam at %.2f m/s: %d", V, WakeMaxFoam(V));
			CHECK(V < WakeMinSpeed || Near(V * R.SternLife, WakeRoadM, 1.0) || R.SternLife == 40.f, "the wash lies %.0f m astern at %.2f m/s", WakeRoadM, V);
		}
	}
	// --- the waves' drift: the carried pattern is the old pattern moved
	{
		const double K = 0.21, Dx = 0.6, Dy = -0.8, Base = 1.234;
		const double DriftX = 123456.7, DriftY = -9876.5;      // 48 hours at 2 m/s is 345 km: far past where a float phase would hold
		const double P = DriftPhase(Base, K, Dx, Dy, DriftX, DriftY);
		CHECK(P >= 0.0 && P < 2.0 * Pi, "the phase is wrapped: %.6f", P);
		for (double X = -50.0; X <= 50.0; X += 7.3)
		{
			const double Y = 0.4 * X + 3.0;
			const double Moved = std::sin(K * (Dx * X + Dy * Y) + P);
			const double Old = std::sin(K * (Dx * (X - DriftX) + Dy * (Y - DriftY)) + Base);
			CHECK(Near(Moved, Old, 1e-6), "surface(x) = old surface(x - drift): %.9f vs %.9f", Moved, Old);
		}
		CHECK(Near(DriftPhase(Base, K, Dx, Dy, 0.0, 0.0), Base, 1e-15), "no drift, no change");
		// water going aft: the crests of a train running with the ship's way come to meet the bow less fast... a train
		// along +X carried towards -X shows, at a fixed point, the phase growing
		CHECK(DriftPhase(0.5, 1.0, 1.0, 0.0, -0.1, 0.0) > 0.5, "carried aft, the pattern's phase at a point grows");
	}
	// --- bells
	{
		CHECK(BellsAt(30 * 60) == 1 && BellsAt(4 * 3600) == 8 && BellsAt(12 * 3600) == 8 && BellsAt(8 * 3600 + 1800) == 1 && BellsAt(11 * 3600 + 1800) == 7, "the watches' bells");
		CHECK(BellsAt(15 * 3600 + 59 * 60) == 0 && BellsAt(16 * 3600 + 10, 20) == 8 && BellsAt(16 * 3600 + 30, 20) == 0, "only as the half hour begins");
		CHECK(BellsAt(0) == 8, "eight bells at midnight");
	}
}

struct FPiece
{
	std::string Id;
	LyudSeascape::FVec2 Chart;
	double Yaw = 0.0, Top = 0.0, Radius = 0.0;
	bool bAnchor = false;
	LyudSeascape::FVec2 Anchorage;       // piece frame, m
	// the voyage
	double MinDist = 1e18, MinAt = 0.0, SeenSec = 0.0, SideY = 0.0;
};

static LyudSeascape::FVec2 Rot(const LyudSeascape::FVec2& V, double Deg)
{
	const double A = Deg * LyudSeascape::Pi / 180.0;
	return { V.X * std::cos(A) - V.Y * std::sin(A), V.X * std::sin(A) + V.Y * std::cos(A) };
}

static void Voyage(bool bTable)
{
	using namespace LyudSeascape;
	const std::string CruiseText = Read("unreal/Lyudmilovsk/Content/Cruise/cruise.json");
	const std::string KitText = Read("unreal/Lyudmilovsk/Content/Cruise/seascape.json");
	if (CruiseText.empty() || KitText.empty())
	{
		std::printf("seascapesim: no cruise.json or seascape.json: the voyage is not checked\n");
		return;
	}
	LyudCruise::FModel M;
	CHECK(M.Load(CruiseText), "cruise.json loads");
	LyudJson::FValue Kit, CruiseJson;
	std::string Error;
	CHECK(LyudJson::FParser(KitText).Parse(Kit, Error), "seascape.json: %s", Error.c_str());
	CHECK(LyudJson::FParser(CruiseText).Parse(CruiseJson, Error), "cruise.json: %s", Error.c_str());
	std::vector<FPiece> Pieces;
	for (const auto& F : Kit["islands"].Fields)
	{
		FPiece P;
		P.Id = F.first;
		const LyudJson::FValue& O = F.second;
		P.Chart = { O["chart"][size_t(0)].Num, O["chart"][size_t(1)].Num };
		P.Yaw = O["yaw"].Num;
		P.Top = O["top"].Num;
		P.Radius = O["radius"].Num;
		CHECK(!O["mesh"].S().empty() && P.Top > 0.0 && P.Radius > 0.0, "%s: mesh, top, radius", P.Id.c_str());
		if (const LyudJson::FValue* A = O["sockets"].Find("anchorage"))
		{
			P.bAnchor = true;
			P.Anchorage = { (*A)[size_t(0)].Num, (*A)[size_t(1)].Num };
		}
		// as ALyudCruiseSeascape::Setup: cruise.json's place wins, the bay takes the anchorage socket
		for (const LyudJson::FValue& I : CruiseJson["islands"].Items)
		{
			if (I["id"].S() != P.Id)
			{
				continue;
			}
			P.Chart = { I["x"].Num, I["y"].Num };
			if (const LyudJson::FValue* Bay = I.Find("bay"))
			{
				if (P.bAnchor)
				{
					const FVec2 Want = { (*Bay)[size_t(0)].Num - P.Chart.X, (*Bay)[size_t(1)].Num - P.Chart.Y };
					FVec2 Turned = Rot(P.Anchorage, P.Yaw);
					if (Turned.X * Want.X + Turned.Y * Want.Y < 0.0)
					{
						P.Yaw += 180.0;
						Turned = Rot(P.Anchorage, P.Yaw);
					}
					P.Chart = { (*Bay)[size_t(0)].Num - Turned.X, (*Bay)[size_t(1)].Num - Turned.Y };
				}
			}
		}
		Pieces.push_back(P);
	}
	CHECK(Pieces.size() >= 8, "the six islands, the coast and the cones: %zu pieces", Pieces.size());
	// the first voyage that sails after 1 June 2026
	const double From = 151.0 * 86400.0;
	long long K = M.VoyageAt(From);
	while (!M.VoyageRuns(K))
	{
		++K;
	}
	const double Sail = M.SailTime(K);
	const double Earth = 2.5e6, Eye = 8.0;
	double MaxTurn = 0.0, MaxSpeed = 0.0;
	FVec2 AnchorAt;
	bool bAnchored = false;
	const double Step = 10.0;
	for (double Tv = 0.0; Tv <= M.TotalSec; Tv += Step)
	{
		const LyudCruise::FState St = M.At(Sail + Tv);
		FShip S;
		S.Chart = { St.Pos.X, St.Pos.Y };
		S.Heading = St.Heading;
		MaxTurn = std::max(MaxTurn, std::fabs(St.TurnRate));
		MaxSpeed = std::max(MaxSpeed, St.Speed);
		if (St.Phase == LyudCruise::EPhase::Anchored)
		{
			AnchorAt = S.Chart;
			bAnchored = true;
		}
		for (FPiece& P : Pieces)
		{
			const FVec2 L = ChartToStage(S, P.Chart);
			const double D = std::hypot(L.X, L.Y);
			if (D < P.MinDist)
			{
				P.MinDist = D;
				P.MinAt = Tv;
				P.SideY = L.Y;
			}
			const bool bVolcano = P.Id.rfind("volcano_", 0) == 0;
			const double Range = bVolcano ? 60000.0 : (P.Id == "home" ? 26000.0 : 18000.0);
			if (IsShown(D, Range, 30000.0, P.Top, HullDownM(D, Eye, Earth)))
			{
				P.SeenSec += Step;
			}
		}
	}
	CHECK(MaxSpeed > 1.0 && MaxSpeed < 6.0, "the ship's speed stays a slow motor ship's: %.2f m/s", MaxSpeed);
	// the scenery swings with the turn: at 800 m an island then moves across the view at TurnRate x 800 m/s more
	CHECK(MaxTurn < 0.02, "no turn faster than about a degree a second (the scenery would whirl): %.4f rad/s", MaxTurn);
	for (const FPiece& P : Pieces)
	{
		const bool bVolcano = P.Id.rfind("volcano_", 0) == 0;
		if (bTable)
		{
			std::printf("%-16s chart %7.0f %7.0f  nearest %7.0f m at %5.1f h (%s side)  in sight %5.1f h  radius %5.0f top %5.0f\n", P.Id.c_str(), P.Chart.X, P.Chart.Y,
				P.MinDist, P.MinAt / 3600.0, P.SideY > 0.0 ? "+Y: starboard of a stage with her bow to +X" : "-Y: port", P.SeenSec / 3600.0, P.Radius, P.Top);
		}
		if (P.Id == "home")
		{
			CHECK(P.MinDist < 1500.0, "the voyage begins on the roads: %.0f m from the coast piece's origin", P.MinDist);
			continue;
		}
		CHECK(P.SeenSec > 600.0, "%s is in sight for more than ten minutes of the voyage (%.0f s)", P.Id.c_str(), P.SeenSec);
		if (bVolcano)
		{
			CHECK(P.MinDist > P.Radius + 1000.0, "the route keeps a kilometre off the cone %s (radius %.0f): nearest %.0f m", P.Id.c_str(), P.Radius, P.MinDist);
		}
		else if (P.bAnchor)
		{
			// the long island: at anchor the ship lies off the bay's socket, in the water outside the island's body
			CHECK(bAnchored, "the model anchors for the night");
			const FVec2 Sock = Rot(P.Anchorage, P.Yaw);
			const FVec2 Bay = { P.Chart.X + Sock.X, P.Chart.Y + Sock.Y };
			const double Off = std::hypot(AnchorAt.X - Bay.X, AnchorAt.Y - Bay.Y);
			CHECK(Off < 700.0, "the anchorage is within 700 m of the bay's socket: %.0f m", Off);
			const FVec2 Local = Rot({ AnchorAt.X - P.Chart.X, AnchorAt.Y - P.Chart.Y }, -P.Yaw);
			// the island's body: 2.6 km x 2 x 235 m about its origin; the bay is on its +Y... the socket's side
			const double SockSide = P.Anchorage.Y >= 0.0 ? 1.0 : -1.0;
			CHECK(Local.Y * SockSide > 235.0 || std::fabs(Local.X) > 1320.0, "at anchor she is outside the island's body: local %.0f %.0f", Local.X, Local.Y);
			if (bTable)
			{
				std::printf("    at anchor: chart %.0f %.0f, %.0f m from the bay's socket, island frame %.0f %.0f\n", AnchorAt.X, AnchorAt.Y, Off, Local.X, Local.Y);
			}
		}
		else
		{
			CHECK(P.MinDist > P.Radius + 60.0, "the ship keeps clear of %s (radius %.0f m): nearest %.0f m", P.Id.c_str(), P.Radius, P.MinDist);
			CHECK(P.MinDist < 1500.0, "%s is passed close enough to look at: %.0f m", P.Id.c_str(), P.MinDist);
		}
	}
}

int main(int Argc, char** Argv)
{
	const bool bTable = Argc > 1 && std::strcmp(Argv[1], "table") == 0;
	Maths();
	Voyage(bTable);
	std::printf("seascapesim: %d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
