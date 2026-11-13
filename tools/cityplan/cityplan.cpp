// tools/cityplan: the town's engine-free C++ checked outside Unreal (docs/city/public_spaces.md, docs/city_plan.md).
//  - CityKit (Private/City/CityKit.cpp, compiled as the game has it minus FMesh::Build): every prop kind the public
//    spaces use, the fountain stone, the fair's rotors and gondola, the paving blocks: valid, finite, consistent meshes.
//  - The window schedules (ALyudCityWindows::ScheduleLit, ULyudWindowLights::SeededFlat / InHours, cut out by
//    extract.py): dark by day, a flat's evening holds together across midnight, nobody up at 03:00.
// Input: Saved/CityPlan/kit_input.txt (prop kinds + their extra numbers, the fountain stone, the ride sizes) and
// Saved/CityPlan/flats.txt (wall flats and their windows), written by run.sh from the committed JSON.
// Prints `ok   <check>` / `FAIL <check>: ...`, then `all ok` or the failure count; exit 1 on failure.
#include "CoreMinimal.h"
#include "CityKit.h"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

namespace
{
	int Fails = 0;
	void Report(const std::string& Name, const std::vector<std::string>& Problems)
	{
		if (Problems.empty())
		{
			std::printf("ok   %s\n", Name.c_str());
			return;
		}
		Fails++;
		std::printf("FAIL %s: %zu problem(s):", Name.c_str(), Problems.size());
		for (size_t I = 0; I < Problems.size() && I < 6; I++) std::printf(" %s;", Problems[I].c_str());
		std::printf("%s\n", Problems.size() > 6 ? " ..." : "");
	}
	std::string F(const char* Fmt, ...) __attribute__((format(printf, 1, 2)));
	std::string F(const char* Fmt, ...)
	{
		char B[512];
		va_list A;
		va_start(A, Fmt);
		std::vsnprintf(B, sizeof B, Fmt, A);
		va_end(A);
		return B;
	}
	bool Fin(float X) { return std::isfinite(X); }
}

// ------------------------------------------------------------------------------------------------ the windows

struct FWindow { uint32 Hash = 0, WinHash = 0; bool bShop = false, bLamp = false, bUpper = false; };
struct FTimespan
{
	int Minutes = 0;
	FTimespan(int H, int M, int S) : Minutes(H * 60 + M + S / 60) {}
};
struct FDateTime
{
	int Day = 1, Hour = 0, Minute = 0;      // day of year 1..365 (a year of 365 days)
	int GetHour() const { return Hour; }
	int GetMinute() const { return Minute; }
	int GetDayOfYear() const { return Day; }
	FDateTime operator-(const FTimespan& T) const
	{
		int M = ((Day - 1) * 24 + Hour) * 60 + Minute - T.Minutes;
		M = ((M % (365 * 1440)) + 365 * 1440) % (365 * 1440);
		FDateTime R;
		R.Day = M / 1440 + 1;
		R.Hour = (M % 1440) / 60;
		R.Minute = M % 60;
		return R;
	}
};
struct FWindowSeed { float Nobody = 1.f, Home = 0.f, Bed = 0.f, Early = 1.f, Up = 0.f; bool bRoom = false; };
namespace Win
{
#include "windows_free.inc"
	FDateTime At(int Day, float H)
	{
		FDateTime T;
		T.Day = Day;
		T.Hour = int(H);
		T.Minute = int((H - int(H)) * 60.f + 0.01f);
		return T;
	}
}

// ------------------------------------------------------------------------------------------------ mesh checks

struct FBounds { FVector Lo{ 1e9, 1e9, 1e9 }, Hi{ -1e9, -1e9, -1e9 }; };

static FBounds MeshCheck(const std::string& Name, const CityKit::FMesh& M, std::vector<std::string>& P, std::vector<std::string>& Wind,
	int32* DegenOut = nullptr)
{
	FBounds B;
	const int32 N = M.Pos.Num();
	if (M.Nrm.Num() != N || M.Col.Num() != N || M.UV0.Num() != N || M.UV1.Num() != N || M.UV2.Num() != N)
	{
		P.push_back(Name + ": vertex streams of different lengths");
		return B;
	}
	if (M.Idx.Num() % 3 != 0) P.push_back(Name + ": index count not a multiple of 3");
	for (int32 I = 0; I < N; I++)
	{
		const FVector3f& V = M.Pos[I];
		const FVector3f& Nm = M.Nrm[I];
		const FVector4f& C = M.Col[I];
		if (!Fin(V.X) || !Fin(V.Y) || !Fin(V.Z) || !Fin(Nm.X) || !Fin(Nm.Y) || !Fin(Nm.Z) || !Fin(M.UV0[I].X) || !Fin(M.UV0[I].Y))
		{
			P.push_back(F("%s: vertex %d not finite", Name.c_str(), I));
			break;
		}
		if (std::fabs(Nm.Size() - 1.f) > 1e-3f)
		{
			P.push_back(F("%s: vertex %d normal length %.3f", Name.c_str(), I, Nm.Size()));
			break;
		}
		if (C.X < 0 || C.X > 1 || C.Y < 0 || C.Y > 1 || C.Z < 0 || C.Z > 1 || C.W < 0 || C.W > 1)
		{
			P.push_back(F("%s: vertex %d colour/roughness out of 0..1", Name.c_str(), I));
			break;
		}
		if (M.UV1[I].X < 0 || M.UV1[I].X > 6 || M.UV1[I].X != std::floor(M.UV1[I].X) || (M.UV2[I].X != 0.f && M.UV2[I].X != 1.f))
		{
			P.push_back(F("%s: vertex %d pattern %.2f / glow %.2f", Name.c_str(), I, M.UV1[I].X, M.UV2[I].X));
			break;
		}
		B.Lo = FVector(std::min<double>(B.Lo.X, V.X), std::min<double>(B.Lo.Y, V.Y), std::min<double>(B.Lo.Z, V.Z));
		B.Hi = FVector(std::max<double>(B.Hi.X, V.X), std::max<double>(B.Hi.Y, V.Y), std::max<double>(B.Hi.Z, V.Z));
	}
	int32 Degen = 0, Flipped = 0;
	for (int32 I = 0; I + 2 < M.Idx.Num(); I += 3)
	{
		const int32 A = M.Idx[I], Bv = M.Idx[I + 1], Cv = M.Idx[I + 2];
		if (A < 0 || A >= N || Bv < 0 || Bv >= N || Cv < 0 || Cv >= N)
		{
			P.push_back(F("%s: triangle %d index out of range", Name.c_str(), I / 3));
			return B;
		}
		const FVector3f Fn = FVector3f::CrossProduct(M.Pos[Bv] - M.Pos[A], M.Pos[Cv] - M.Pos[A]);
		if (Fn.Size() < 1e-4f)          // under 0.5e-4 cm^2: a sliver (a cone's tip, a sphere's pole)
		{
			Degen++;
			continue;
		}
		// Tri() winds every triangle round the way it faces (its Out); the stored vertex normals must agree with it, or
		// that face lights from behind (and Unreal culls it as a back face)
		const FVector3f Avg = M.Nrm[A] + M.Nrm[Bv] + M.Nrm[Cv];
		if (FVector3f::DotProduct(Fn.GetSafeNormal(), Avg.GetSafeNormal()) < 0.f)
		{
			Flipped++;
		}
	}
	if (Flipped) Wind.push_back(F("%s: %d of %d triangles wound against their normals", Name.c_str(), Flipped, M.Idx.Num() / 3));
	// slivers are expected where a lathe closes on its axis (a sphere's poles, a cone's tip: one vertex repeated), up to
	// ~20% of a small sphere's triangles; harmless (the mesh build drops them). Only a mesh mostly made of them is wrong
	if (Degen * 3 > M.Idx.Num() / 3) P.push_back(F("%s: %d of %d triangles degenerate", Name.c_str(), Degen, M.Idx.Num() / 3));
	if (DegenOut) *DegenOut = Degen;
	for (const auto& C : M.Colliders)
	{
		if (!(C.Half.X > 0 && C.Half.Y > 0 && C.Half.Z > 0) || !std::isfinite(C.C.X + C.C.Y + C.C.Z + C.Yaw))
		{
			P.push_back(Name + ": a collider with no size or not finite");
			break;
		}
	}
	return B;
}

int main(int Argc, char** Argv)
{
	const std::string Dir = Argc > 1 ? Argv[1] : "Saved/CityPlan";
	// ---- the kit's input: "prop <kind> <variant> [r <n> | r2 <rx> <ry>]", "stone <key> <value>", "ride <wheel_r> <hub_z>"
	struct FProp { std::string Kind; int32 Variant = 0; TSharedPtr<FJsonObject> X; };
	std::vector<FProp> Props;
	auto Stone = std::make_shared<FJsonObject>();
	float WheelR = 0.f, WheelHubZ = 0.f;
	{
		std::ifstream In(Dir + "/kit_input.txt");
		std::string Line;
		while (std::getline(In, Line))
		{
			std::istringstream L(Line);
			std::string What;
			L >> What;
			if (What == "prop")
			{
				FProp Pr;
				L >> Pr.Kind >> Pr.Variant;
				auto X = std::make_shared<FJsonObject>();
				std::string K;
				while (L >> K)
				{
					if (K == "r") { double R; L >> R; X->Numbers["r"] = R; }
					else if (K == "r2") { double A, B; L >> A >> B; X->Arrays["r"] = { TSharedPtr<FJsonValue>(std::make_shared<FJsonValue>(A)), TSharedPtr<FJsonValue>(std::make_shared<FJsonValue>(B)) }; }
				}
				Pr.X = X;
				Props.push_back(Pr);
			}
			else if (What == "stone") { std::string K; double V; L >> K >> V; Stone->Numbers[K] = V; }
			else if (What == "ride") { L >> WheelR >> WheelHubZ; }
		}
	}
	if (Props.empty())
	{
		std::printf("FAIL no kit input in %s/kit_input.txt\n", Dir.c_str());
		return 1;
	}

	// ---- every prop kind of public_spaces.json
	{
		std::vector<std::string> P, Wind, Empty, Ground, Size, Coll, Glow;
		std::set<std::string> Done;
		for (const FProp& Pr : Props)
		{
			const std::string Key = Pr.Kind + "_" + std::to_string(Pr.Variant);
			if (!Done.insert(Key).second) continue;
			CityKit::FMesh M;
			CityKit::BuildProp(M, FString(Pr.Kind), Pr.Variant, Pr.X);
			if (M.Idx.Num() == 0)
			{
				Empty.push_back(Pr.Kind);            // LyudPublicSpaces skips it with a warning: the prop is missing in game
				continue;
			}
			const FBounds B = MeshCheck(Key, M, P, Wind);
			// metres: the origin is on the ground, nothing sinks under it (a pond's kerb foot 10 cm), nothing huge
			// (a beach umbrella's pole is planted 30 cm in the sand)
			if (B.Lo.Z < (Pr.Kind == "umbrella" ? -35.0 : -15.0)) Ground.push_back(F("%s bottom %.2f m", Key.c_str(), B.Lo.Z / 100));
			const double Ext = std::max({ B.Hi.X - B.Lo.X, B.Hi.Y - B.Lo.Y, B.Hi.Z - B.Lo.Z }) / 100;
			if (Ext > 60.0 || Ext < 0.05) Size.push_back(F("%s extent %.2f m", Key.c_str(), Ext));
			if (CityKit::PropCollides(FString(Pr.Kind)) && M.Colliders.Num() == 0) Coll.push_back(Key + " collides but has no collider boxes");
			for (const auto& C : M.Colliders)
			{
				// a collider (yawed box: its radius in plan) reaching past the mesh's bounds blocks the player in thin air
				const double R = std::sqrt(C.Half.X * C.Half.X + C.Half.Y * C.Half.Y);
				if (C.C.X * 100 - R * 100 > B.Hi.X + 30 || C.C.X * 100 + R * 100 < B.Lo.X - 30 || C.C.Y * 100 - R * 100 > B.Hi.Y + 30 ||
					C.C.Y * 100 + R * 100 < B.Lo.Y - 30 || (C.C.Z - C.Half.Z) * 100 > B.Hi.Z + 30 || (C.C.Z + C.Half.Z) * 100 < B.Lo.Z - 30)
				{
					Coll.push_back(Key + " collider outside the mesh");
					break;
				}
			}
			bool bGlowVerts = false;
			for (const auto& U : M.UV2) bGlowVerts |= U.X > 0.5f;
			const bool bGlowKind = CityKit::PropGlows(FString(Pr.Kind));
			// glass on a prop not given the glow material stays dark at night; the glow material on a prop without glass is harmless
			if (bGlowVerts && !bGlowKind) Glow.push_back(Key + " has lamp glass but PropGlows is false");
			if ((Pr.Kind == "lamp" || Pr.Kind == "lamp_park") && !bGlowVerts) Glow.push_back(Key + " lamp without glass");
		}
		Report("kit: every prop kind in public_spaces.json builds a mesh (none skipped in game)", Empty);
		Report("kit: prop meshes valid (equal streams, indices in range, finite, unit normals, colours 0..1)", P);
		Report("kit: every triangle faces the way its normals say (no back-facing or dark faces)", Wind);
		Report("kit: props stand on their origin and have sane sizes", Ground.empty() && Size.empty() ? Ground : (Ground.insert(Ground.end(), Size.begin(), Size.end()), Ground));
		Report("kit: colliding props have collider boxes inside their mesh", Coll);
		Report("kit: lamp glass only on the props given the glowing material", Glow);
	}

	// ---- the fountain stone, the rides, the paving blocks
	{
		std::vector<std::string> P, Wind;
		CityKit::FMesh S;
		CityKit::BuildFountainStone(S, Stone);
		const FBounds B = MeshCheck("fountain_stone", S, P, Wind);
		const double RimR = Stone->HasField("rim_r") ? Stone->GetNumberField("rim_r") : 9.0;
		if (std::fabs(B.Hi.X / 100 - (RimR + 0.1)) > 0.02 || B.Lo.Z < -1.0) P.push_back(F("fountain stone bounds %.2f..%.2f", B.Lo.X / 100, B.Hi.X / 100));
		CityKit::FMesh D;
		CityKit::BuildFountainStone(D, TSharedPtr<FJsonObject>());
		MeshCheck("fountain_stone_default", D, P, Wind);
		CityKit::FMesh C;
		CityKit::BuildCarouselRotor(C);
		MeshCheck("carousel_rotor", C, P, Wind);
		CityKit::FMesh Wr;
		CityKit::BuildWheelRotor(Wr, WheelR > 0 ? WheelR : 7.5f, 12);
		MeshCheck("wheel_rotor", Wr, P, Wind);
		for (int32 V = 0; V < 4; V++)
		{
			CityKit::FMesh G;
			CityKit::BuildGondola(G, V);
			MeshCheck(F("gondola_%d", V), G, P, Wind);
		}
		for (const char* Skin : { "deck", "square", "granite", "gravel", "rubber", "basin", "boards", "concrete" })
		{
			CityKit::FMesh K;
			CityKit::AddBlock(K, FVector(1, 2, 0.3), 30.f, FVector(4, 2, 0.6), FString(Skin), -36.f);
			MeshCheck(std::string("block_") + Skin, K, P, Wind);
			if (K.Idx.Num() != 5 * 6) P.push_back(std::string("block ") + Skin + ": not a top and four sides");
			// the top's UV0 runs in metres from the block's corner: its far corner reads (size x, size y)
			if (std::fabs(K.UV0[2].X - 4.f) > 1e-4f || std::fabs(K.UV0[2].Y - 2.f) > 1e-4f) P.push_back(std::string("block ") + Skin + ": top UV not in metres");
		}
		CityKit::FMesh Cube;
		CityKit::BuildBlock(Cube, FString("concrete"));
		const FBounds CB = MeshCheck("collision_cube", Cube, P, Wind);
		if (std::fabs(CB.Hi.X - 50) > 1e-3 || std::fabs(CB.Lo.Z + 50) > 1e-3 || Cube.Idx.Num() != 36) P.push_back("collision cube is not a closed 1 m cube");
		Report("kit: fountain stone, carousel and wheel rotors, gondolas, paving blocks and the collision cube valid", P);
		Report("kit: rotors, stone and blocks face the way their normals say", Wind);
	}

	// ---- the wheel: its lowest gondola clears the boarding platform (the wheel turns through it every pass)
	{
		std::vector<std::string> P;
		CityKit::FMesh Frame, G;
		CityKit::BuildProp(Frame, FString("wheel"), 0, TSharedPtr<FJsonObject>());
		CityKit::BuildGondola(G, 0);
		// the platform: the frame's box round the origin (|x|, |y| < 1.7 m); the gondola's lowest point under its pivot
		float PlatformTop = -1e9f, GondolaLow = 1e9f;
		for (const auto& V : Frame.Pos) if (std::fabs(V.X) < 170.f && std::fabs(V.Y) < 170.f && V.Z < 100.f) PlatformTop = std::max(PlatformTop, V.Z);
		for (const auto& V : G.Pos) GondolaLow = std::min(GondolaLow, V.Z);
		const float Lowest = WheelHubZ * 100.f - WheelR * 100.f + GondolaLow;
		if (WheelR <= 0.f) P.push_back("no ride sizes read from LyudPublicSpaces.cpp");
		else if (Lowest < PlatformTop + 2.f)
			P.push_back(F("lowest gondola bottom %.1f cm, platform top %.1f cm (hub %.2f m, radius %.2f m)", Lowest, PlatformTop, WheelHubZ, WheelR));
		Report("rides: the Ferris wheel's lowest gondola passes over its boarding platform", P);
	}

	// ---- window schedules
	{
		std::vector<std::pair<std::string, std::vector<std::string>>> Flats;
		std::ifstream In(Dir + "/flats.txt");
		std::string Line;
		while (std::getline(In, Line))
		{
			std::istringstream L(Line);
			std::pair<std::string, std::vector<std::string>> Fl;
			L >> Fl.first;
			std::string W;
			while (L >> W) Fl.second.push_back(W);
			Flats.push_back(Fl);
		}
		std::vector<std::string> Day, Night, Mid, Lamp, Count;
		if (Flats.size() < 100) Count.push_back(F("only %zu flats read", Flats.size()));
		Report("windows: the wall flats and their windows read", Count);
		int64_t LitEvening = 0, Total = 0;
		for (const auto& Fl : Flats)
		{
			const uint32 FlatHash = Win::HashName(FString(Fl.first));
			for (size_t K = 0; K < Fl.second.size(); K++)
			{
				FWindow W;
				W.Hash = FlatHash;
				W.WinHash = Win::HashName(FString(Fl.second[K]));
				W.bUpper = K > 0;
				for (int D = 1; D <= 365; D += 7)
				{
					for (float H = 0.f; H < 24.f; H += 0.25f)
					{
						if (Win::ScheduleLit(W, Win::At(D, H), 10.f)) Day.push_back(Fl.second[K]);
						if (H >= 2.f && H < 5.5f && Win::ScheduleLit(W, Win::At(D, H), -20.f)) Night.push_back(F("%s day %d %.2f", Fl.second[K].c_str(), D, H));
					}
					Total++;
					LitEvening += Win::ScheduleLit(W, Win::At(D, 20.5f), -10.f);
				}
			}
		}
		if (Total && (LitEvening * 100 / Total < 25 || LitEvening * 100 / Total > 90)) Night.push_back(F("%lld%% of windows lit at 20:30", (long long)(LitEvening * 100 / Total)));
		Report("windows: no flat window lit in daylight", Day);
		Report("windows: lit in the evening (25..90% at 20:30), nobody up 02:00..05:30", Night);

		// a flat's evening must hold across midnight: whoever went to bed before 00:00 stays dark until morning, whoever
		// is still up at 23:59 keeps the light on past it (not all switching at the stroke of midnight). The window
		// tested is the flat's lower one with a pane hash whose room-by-room flicker lets it burn all night, so the
		// window shows exactly whether the household is awake.
		int Relit = 0, Tested = 0, OffA = 0, OffB = 0, OffC = 0;     // went dark 23:15->23:45, 23:45->00:15, 00:15->00:45
		for (const auto& Fl : Flats)
		{
			const uint32 FlatHash = Win::HashName(FString(Fl.first));
			for (int D = 1; D <= 365; D++)
			{
				FWindow W;
				W.Hash = FlatHash;
				bool bFound = false;
				for (uint32 Wh = 1; Wh < 4000 && !bFound; Wh++)
				{
					bool bAll = true;
					for (uint32 S : { uint32(D * 7 + 21), uint32(D * 7 + 22), uint32(D * 7 + 23), uint32((D % 365 + 1) * 7), uint32((D % 365 + 1) * 7 + 1) })
						bAll = bAll && Win::Frac(Wh * 2654435761u, S) < 0.85f;
					if (bAll) { W.WinHash = Wh * 2654435761u; bFound = true; }
				}
				if (!bFound) continue;
				Tested++;
				const int Next = D % 365 + 1;
				const bool b2230 = Win::ScheduleLit(W, Win::At(D, 22.5f), -20.f);
				const bool b2315 = Win::ScheduleLit(W, Win::At(D, 23.25f), -20.f);
				const bool b2345 = Win::ScheduleLit(W, Win::At(D, 23.75f), -20.f);
				const bool b0015 = Win::ScheduleLit(W, Win::At(Next, 0.25f), -20.f);
				const bool b0045 = Win::ScheduleLit(W, Win::At(Next, 0.75f), -20.f);
				if (!b2230 && !b2345 && (b0015 || b0045)) Relit++;
				OffA += b2315 && !b2345;
				OffB += b2345 && !b0015;
				OffC += b0015 && !b0045;
			}
		}
		// bedtimes spread evenly over 21:30..01:30: as many flats go to bed in the half hour across midnight as in the
		// half hours either side of it; none that has gone to bed lights up again before morning
		if (Relit * 1000 > Tested || OffB * 2 > 3 * std::max(OffA, OffC))
			Mid.push_back(F("%d flat-nights: %d dark from 22:30 lit again after 00:00; went dark 23:15-23:45 %d, 23:45-00:15 %d, 00:15-00:45 %d",
				Tested, Relit, OffA, OffB, OffC));
		Report("windows: a flat's evening holds across midnight (no relighting or cut-off at 00:00)", Mid);

		FWindow L;
		L.bLamp = true;
		for (float Alt : { -20.f, 0.f, 3.9f }) if (!Win::ScheduleLit(L, Win::At(100, 12.f), Alt)) Lamp.push_back(F("lamp dark at sun %.1f", Alt));
		for (float Alt : { 4.f, 30.f }) if (Win::ScheduleLit(L, Win::At(100, 12.f), Alt)) Lamp.push_back(F("lamp lit at sun %.1f", Alt));
		FWindow Sh;
		Sh.bShop = true;
		Sh.Hash = Win::HashName(FString("W1-U01"));
		if (Win::ScheduleLit(Sh, Win::At(100, 12.f), 20.f)) Lamp.push_back("shop lit by day");
		if (!Win::ScheduleLit(Sh, Win::At(355, 17.f), -5.f)) Lamp.push_back("shop dark on a winter evening while open");
		if (Win::ScheduleLit(Sh, Win::At(355, 23.f), -20.f) || Win::ScheduleLit(Sh, Win::At(355, 8.f), -5.f)) Lamp.push_back("shop lit while shut");
		Report("windows: street lamps lit in the dark only, shops lit while open in the dark", Lamp);
	}

	// ---- the buildings' seeded evenings (ULyudWindowLights::SeededFlat, InHours)
	{
		std::vector<std::string> P;
		struct { float H, A, B; bool bIn; } Cases[] = {
			{ 23.5f, 22.f, 25.f, true }, { 0.5f, 22.f, 25.f, true }, { 1.5f, 22.f, 25.f, false }, { 21.f, 22.f, 25.f, false },
			{ 10.f, 9.f, 17.f, true }, { 17.f, 9.f, 17.f, false }, { 3.f, 0.f, 24.f, true }, { 5.f, 18.f, 7.f, true }, { 12.f, 18.f, 7.f, false },
			{ 23.9f, 21.5f, 25.5f, true }, { 1.4f, 21.5f, 25.5f, true }, { 1.6f, 21.5f, 25.5f, false } };
		for (const auto& C : Cases)
			if (Win::InHours(C.H, C.A, C.B) != C.bIn) P.push_back(F("InHours(%.1f, %.1f, %.1f) != %d", C.H, C.A, C.B, C.bIn));
		FWindowSeed S;
		S.Nobody = 0.1f; S.Home = 17.f; S.Bed = 24.5f; S.Early = 0.1f; S.Up = 6.f; S.bRoom = true;
		if (Win::SeededFlat(S, 20.f, 10.f) != 0.f) P.push_back("lit by day");
		if (Win::SeededFlat(S, 20.f, -5.f) != 1.f || Win::SeededFlat(S, 0.25f, -5.f) != 1.f) P.push_back("dark while home after dusk");
		if (Win::SeededFlat(S, 3.f, -5.f) != 0.f) P.push_back("lit at 03:00");
		if (Win::SeededFlat(S, 6.5f, -5.f) != 1.f) P.push_back("early riser dark at 06:30");
		S.Nobody = 0.9f;
		if (Win::SeededFlat(S, 20.f, -5.f) != 0.f) P.push_back("empty flat lit");
		Report("buildings: InHours wraps midnight, seeded flats dark by day / lit at home / empty stay dark", P);
	}

	std::printf(Fails ? "%d check(s) failed\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}
