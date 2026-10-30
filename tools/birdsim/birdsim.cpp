// Runs the bird life (Private/Birds/BirdSim.cpp) outside the engine, for tuning and for the debug maps.
//
//   tools/birdsim/run.sh day  [start_hour hours]      a simulated day -> Saved/BirdSim/day.csv (+ maps via render_birds.py)
//   tools/birdsim/run.sh test                        scripted player scenarios (approach, run, offer food) -> pass/fail
//
// The player stands at the club's door unless a scenario moves them. The sun times come from a simple
// solar formula for the game's place (54.96 N, 20.40 E, UTC+2) on the game's date.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdMurmur.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudBirds;

namespace
{
	const float Pi = 3.14159265f;

	/** Sun altitude (deg) at local clock Hour on DayOfYear (NOAA approximation). */
	float SunAltitude(float DayOfYear, float Hour, float Lat = 54.96f, float Lon = 20.40f, float Utc = 2.f)
	{
		const float G = 2.f * Pi / 365.f * (DayOfYear - 1.f + (Hour - 12.f) / 24.f);
		const float Eq = 229.18f * (0.000075f + 0.001868f * std::cos(G) - 0.032077f * std::sin(G) - 0.014615f * std::cos(2 * G) - 0.040849f * std::sin(2 * G));
		const float Decl = 0.006918f - 0.399912f * std::cos(G) + 0.070257f * std::sin(G) - 0.006758f * std::cos(2 * G) + 0.000907f * std::sin(2 * G);
		const float TrueMin = Hour * 60.f + Eq + 4.f * Lon - 60.f * Utc;
		const float Ha = (TrueMin / 4.f - 180.f) * Pi / 180.f;
		const float La = Lat * Pi / 180.f;
		const float CosZ = std::sin(La) * std::sin(Decl) + std::cos(La) * std::cos(Decl) * std::cos(Ha);
		return 90.f - std::acos(CosZ) * 180.f / Pi;
	}

	void SunTimes(float Doy, float& Rise, float& Set)
	{
		Rise = 6.f;
		Set = 18.f;
		for (float H = 0.f; H < 24.f; H += 1.f / 60.f)
		{
			const float A = SunAltitude(Doy, H), B = SunAltitude(Doy, H + 1.f / 60.f);
			if (A < -0.83f && B >= -0.83f)
			{
				Rise = H;
			}
			if (A >= -0.83f && B < -0.83f)
			{
				Set = H;
			}
		}
	}

	std::string Dir()
	{
		const char* E = std::getenv("BIRD_DATA");
		return E ? E : "unreal/Lyudmilovsk/Content/BirdsData";
	}

	/** Day of the year of a month-day (the game's year has no 29 February before October). */
	float Doy(int Month, int Day)
	{
		static const int Before[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
		return (float)(Before[Month - 1] + Day);
	}

	/**
	 * Who is away on a day, as the game does it from species.json (months, stragglers): sets bMigrant and the stragglers'
	 * days in the species table. A small reader for the two fields only.
	 */
	void ApplyMonths(float DayOfYear)
	{
		std::ifstream F(Dir() + "/species.json");
		std::stringstream SS;
		SS << F.rdbuf();
		const std::string T = SS.str();
		int Month = 1;
		while (Month < 12 && Doy(Month + 1, 1) <= DayOfYear)
		{
			Month++;
		}
		for (FSpecies& S : MutableSpeciesTable())
		{
			const size_t At = T.find("\"id\": \"" + S.Id + "\"");
			if (At == std::string::npos)
			{
				continue;
			}
			const size_t End = std::min(T.find("\"id\":", At + 5), T.size());
			const size_t M = T.find("\"months\":", At);
			if (M == std::string::npos || M > End)
			{
				continue;
			}
			bool bHere = false;
			for (const char* P = T.c_str() + T.find('[', M); *P && *P != ']'; P++)
			{
				if (*P >= '0' && *P <= '9')
				{
					char* E;
					bHere |= std::strtol(P, &E, 10) == Month;
					P = E - 1;
				}
			}
			S.bMigrant = !bHere;
			const size_t L = T.find("\"stragglers\":", At);
			if (L != std::string::npos && L < End)
			{
				int Count = 0, M0 = 0, D0 = 0, M1 = 0, D1 = 0;
				std::sscanf(T.c_str() + L, "\"stragglers\": {\"count\": %d, \"from\": \"%d-%d\", \"until\": \"%d-%d\"", &Count, &M0, &D0, &M1, &D1);
				S.Stragglers = Count;
				S.LeaveFrom = Doy(M0, D0);
				S.LeaveUntil = Doy(M1, D1);
			}
		}
	}

	// the club's door in Unreal metres (map origin, portal face y 790 cm)
	const V3 Door(-1.93f, 7.9f + 4.f, 0.f);

	bool Setup(FWorld& W, float StartHour)
	{
		if (!W.LoadHabitats(Dir()))
		{
			std::fprintf(stderr, "birdsim: %s\n", W.LastError.c_str());
			return false;
		}
		W.DayOfYear = 297.f;
		SunTimes(W.DayOfYear, W.Sunrise, W.Sunset);
		W.Hour = StartHour;
		W.Camera = Door + V3(0.f, 0.f, 1.7f);
		W.Populate();
		std::printf("birdsim: %zu sites, %zu birds in %zu groups; sunrise %.2f sunset %.2f\n", W.Sites.size(), W.Birds.size(), W.Groups.size(), W.Sunrise, W.Sunset);
		return true;
	}

	int Day(int Argc, char** Argv)
	{
		const float Start = Argc > 2 ? (float)std::atof(Argv[2]) : 4.f;
		const float Hours = Argc > 3 ? (float)std::atof(Argv[3]) : 24.f;
		FWorld W;
		if (!Setup(W, Start))
		{
			return 1;
		}
		std::system("mkdir -p Saved/BirdSim");
		FILE* F = std::fopen("Saved/BirdSim/day.csv", "w");
		std::fprintf(F, "hour,id,species,act,x,y,z,site_kind,alert\n");
		const float Dt = 0.2f;
		const auto T0 = std::chrono::steady_clock::now();
		double StepTime = 0.0;
		long Steps = 0;
		float NextSnap = Start;
		std::map<int, int> Events;
		std::map<std::string, int> ByKind;
		for (float T = 0.f; T < Hours * 3600.f; T += Dt)
		{
			W.Hour = std::fmod(Start + T / 3600.f, 24.f);
			const auto A = std::chrono::steady_clock::now();
			W.Step(Dt);
			StepTime += std::chrono::duration<double>(std::chrono::steady_clock::now() - A).count();
			Steps++;
			for (const FEvent& E : W.Events)
			{
				Events[(int)E.Kind]++;
				if (E.Kind == EEvent::ChaseStart || E.Kind == EEvent::Flee)
				{
					ByKind[std::string(EventName(E.Kind)) + ":" + SpeciesTable()[W.Birds[E.Bird].Species].Id]++;
				}
			}
			W.Events.clear();
			const float Clock = Start + T / 3600.f;
			if (Clock >= NextSnap)
			{
				NextSnap += 0.25f;
				for (const FBird& B : W.Birds)
				{
					if (B.bGone)
					{
						continue;
					}
					const char* Kind = B.Site >= 0 && !IsAirborne(B.Act) ? HabitatName(W.Sites[B.Site].Kind) : "air";
					std::fprintf(F, "%.2f,%d,%s,%s,%.1f,%.1f,%.1f,%s,%.2f\n", W.Hour, B.Id, SpeciesTable()[B.Species].Id.c_str(), ActName(B.Act), B.Pos.X, B.Pos.Y, B.Pos.Z, Kind, B.Alert);
				}
				if (std::fmod(Clock, 1.f) < 0.01f)
				{
					std::printf("%s\n", W.Report().c_str());
				}
			}
		}
		std::fclose(F);
		const double Wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count();
		std::printf("birdsim: %.0f s simulated in %.1f s; step %.3f ms mean\n", Hours * 3600.f, Wall, StepTime / Steps * 1000.0);
		std::printf("events:");
		for (auto& [K, N] : Events)
		{
			std::printf(" %s %d", EventName((EEvent)K), N);
		}
		std::printf("\n");
		for (auto& [K, N] : ByKind)
		{
			std::printf("  %s %d\n", K.c_str(), N);
		}
		return 0;
	}

	// --- scripted player scenarios ------------------------------------------------------------------

	struct FResult { int Pass = 0, Fail = 0; };

	void Check(FResult& R, bool bOk, const char* What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What);
		(bOk ? R.Pass : R.Fail)++;
	}

	/** A world with one bird of a species standing on open ground at P, the player at Feet. */
	int Lone(FWorld& W, const char* Species, V3& P)
	{
		// on a real meadow of the map (its ground grid), alone
		W = FWorld();
		W.LoadHabitats(Dir());
		for (const FSite& M : W.Sites)
		{
			if (M.Kind == EHabitat::Meadow && M.A.X < -200.f)
			{
				P = M.A;
				break;
			}
		}
		W.Sites.clear();
		W.Opt.bPredators = false;
		FSite S;
		S.Kind = std::string(Species) == "Robin" ? EHabitat::ForestFloor : EHabitat::Meadow;
		S.A = S.B = P;
		S.Radius = 1.5f;
		S.Capacity = 10;
		W.Sites.push_back(S);
		FSite Tree;
		Tree.Kind = EHabitat::Broadleaf;
		Tree.A = Tree.B = P + V3(-60.f, 30.f, 12.f);
		Tree.Radius = 2.f;
		W.Sites.push_back(Tree);
		W.Birds.clear();
		W.Groups.clear();
		W.Hour = 11.f;
		W.Sunrise = 7.3f;
		W.Sunset = 17.f;
		W.Opt.PopulationScale = 0.f;
		W.Populate();   // no birds (scale 0) except predators: drop them
		W.Birds.clear();
		W.Groups.clear();
		FGroup G;
		G.Species = FindSpecies(Species);
		G.Home = P;
		G.Roost = 1;
		G.Site = 0;
		G.Plan = 1;
		G.StayTimer = 1e9f;
		FBird B;
		B.Id = 0;
		B.Species = G.Species;
		B.Group = 0;
		B.Site = 0;
		B.Pos = B.Spot = P;
		B.Rng = 12345;
		B.Boldness = 1.f;
		B.Hunger = 0.7f;
		B.Act = EAct::Peck;
		B.ActLen = 1.f;
		G.Members.push_back(0);
		W.Groups.push_back(G);
		W.Birds.push_back(B);
		return 0;
	}

	struct FWalk
	{
		V3 From, To;
		float Speed;
		bool bCrouch;
		float TurnRate = 0.f;
		bool bOffer = false;
		float Wait = 0.f;   // s standing still at To
	};

	/** Moves the player along Walk, steps the world; returns the first time an event happened (or -1) and the distance then. */
	float Run(FWorld& W, const FWalk& Wk, EEvent Want, float& DistAt, std::map<int, float>* Firsts = nullptr)
	{
		const float Dt = 1.f / 60.f;
		const V3 Dir = (Wk.To - Wk.From).Norm();
		const float Len = (Wk.To - Wk.From).Len();
		V3 Feet = Wk.From;
		float T = 0.f, Walked = 0.f;
		const float Total = Len / std::max(Wk.Speed, 0.01f) + Wk.Wait;
		DistAt = -1.f;
		float Found = -1.f;
		while (T < Total + 2.f)
		{
			const bool bMoving = Walked < Len;
			W.Player.bPresent = true;
			W.Player.Feet = Feet;
			W.Player.Eye = Feet + V3(0.f, 0.f, Wk.bCrouch ? 1.0f : 1.65f);
			W.Player.Vel = bMoving ? Dir * Wk.Speed : V3();
			W.Player.Facing = (W.Birds[0].Pos - W.Player.Eye).Norm();
			W.Player.bCrouched = Wk.bCrouch;
			W.Player.TurnRate = Wk.TurnRate;
			W.Player.bOffer = Wk.bOffer;
			W.Player.Palm = W.Player.Eye + V3(Dir.X, Dir.Y, 0.f) * 0.55f - V3(0.f, 0.f, 0.3f);
			W.Player.HandSpeed = 0.f;
			W.Camera = W.Player.Eye;
			W.Step(Dt);
			for (const FEvent& E : W.Events)
			{
				if (Firsts && !Firsts->count((int)E.Kind))
				{
					(*Firsts)[(int)E.Kind] = T;
				}
				if (E.Kind == Want && E.Bird == 0 && Found < 0.f)
				{
					Found = T;
					DistAt = (W.Birds[0].Pos - Feet).Len2D();
				}
			}
			W.Events.clear();
			if (std::getenv("BIRD_TRACE") && std::fmod(T, 0.5f) < Dt)
			{
				const FBird& B = W.Birds[0];
				std::printf("    t %5.1f d %5.1f act %-8s alert %.2f trust %.2f noticed %d watch %d delay %.2f pos %.1f %.1f %.1f\n", T, (B.Pos - Feet).Len2D(), ActName(B.Act), B.Alert, B.Trust, B.bNoticed, B.bWatching, B.Delay, B.Pos.X, B.Pos.Y, B.Pos.Z);
			}
			if (bMoving)
			{
				Feet += Dir * (Wk.Speed * Dt);
				Walked += Wk.Speed * Dt;
			}
			T += Dt;
		}
		return Found;
	}

	/** Runs the world on the clock from Hour for Hours (game time = real time), calling Fn after each step. */
	template <typename F>
	void Advance(FWorld& W, float Hours, float Dt, F&& Fn)
	{
		const float Start = W.Hour;
		for (float T = 0.f; T < Hours * 3600.f; T += Dt)
		{
			W.Hour = std::fmod(Start + T / 3600.f, 24.f);
			W.Step(Dt);
			W.Events.clear();
			Fn();
		}
	}

	void Wind(FResult& R);
	bool IsGullName(const std::string& Id) { return Id.find("Gull") != std::string::npos; }

	/** The species' days (docs/bird_life.md "Daily routines"): starlings and their murmuration, swallows on the wires, the tide. */
	void Routines(FResult& R)
	{
		const int Star = FindSpecies("Starling");
		// --- the starlings' evening: gather, murmurate over the reedbed by the shore, drop in, sleep ---
		{
			FWorld W;
			if (!Setup(W, 15.5f))
			{
				Check(R, false, "the world loads");
				return;
			}
			W.Camera = V3(0.f, -2000.f, 500.f);       // far away: everybody coarse, as most of the time in the game
			FMurmuration* M = W.Murmuration();
			Check(R, M != nullptr, "the starlings have a murmuration and a roost");
			if (!M)
			{
				return;
			}
			const V3 Roost = M->RoostCentre();
			std::printf("starlings' roost: reed at %.0f %.0f, %.0f m from the sea\n", Roost.X, Roost.Y, W.Inland(Roost.X, Roost.Y));
			Check(R, W.Inland(Roost.X, Roost.Y) < 250.f, "the starlings roost in a reedbed near the shore");
			int Total = 0;
			for (const FBird& B : W.Birds)
			{
				Total += B.Species == Star && !B.bGone;
			}
			int MaxIn = 0, Asleep = 0, Over = 0;
			float Spread = 0.f, CostMax = 0.f;
			double Cost = 0.0;
			long CostN = 0;
			bool bSpreadSeen = false;
			const float Dusk = W.Sunset + 0.6f;      // civil dusk at the end of October
			const float Dt = 0.1f;
			Advance(W, (Dusk + 1.0f) - 15.5f, Dt, [&]()
			{
				const auto A = std::chrono::steady_clock::now();
				MaxIn = std::max(MaxIn, M->Count());
				static int Printed = 0;
				if (std::getenv("BIRD_TRACE") && Printed < 12)
				{
					for (const FBird& B : W.Birds)
					{
						if (B.Act == EAct::Murmur && W.Offshore(B.Pos.X, B.Pos.Y) > 220.f && Printed < 12)
						{
							Printed++;
							std::printf("    OUT %.3f h bird %d at %.0f %.0f %.0f vel %.1f %.1f off %.0f from roost %.0f; %s\n", W.Hour, B.Id, B.Pos.X, B.Pos.Y, B.Pos.Z,
								B.Vel.X, B.Vel.Y, W.Offshore(B.Pos.X, B.Pos.Y), (B.Pos - M->RoostCentre()).Len2D(), M->Describe(W).c_str());
						}
					}
				}
				if (M->Count() > 0)
				{
					const double Ms = std::chrono::duration<double>(std::chrono::steady_clock::now() - A).count() * 1000.0;
					(void)Ms;
				}
				// the flock's body: 90% of the birds within this of its centre, ten minutes before sunset
				if (!bSpreadSeen && W.Hour > W.Sunset - 0.17f && M->Count() > Total / 2)
				{
					bSpreadSeen = true;
					std::vector<float> D;
					V3 C;
					for (const FBird& B : W.Birds)
					{
						if (B.Act == EAct::Murmur)
						{
							C += B.Pos;
							D.push_back(0.f);
						}
					}
					C = C * (1.f / D.size());
					D.clear();
					for (const FBird& B : W.Birds)
					{
						if (B.Act == EAct::Murmur)
						{
							D.push_back((B.Pos - C).Len());
						}
					}
					std::sort(D.begin(), D.end());
					Spread = D[D.size() * 9 / 10];
					std::printf("  %.2f h: %s\n", W.Hour, M->Describe(W).c_str());
				}
				if (std::fmod(W.Hour * 60.f, 10.f) < Dt / 60.f && W.Hour > W.Sunset - 1.2f)
				{
					int Roosting = 0, Staging = 0;
					for (const FBird& B : W.Birds)
					{
						if (B.Species == Star)
						{
							Roosting += B.Act == EAct::Sleep;
							Staging += W.Groups[B.Group].Plan == 3;
						}
					}
					std::printf("  %.2f h (sunset %+.0f min): %d in the murmuration, %d gathering before it, %d asleep; max offshore %.0f m\n", W.Hour,
						(W.Hour - W.Sunset) * 60.f, M->Count(), Staging, Roosting, M->MaxOffshore);
				}
			});
			for (const FBird& B : W.Birds)
			{
				if (B.Species == Star && !B.bGone)
				{
					Asleep += B.Act == EAct::Sleep && B.Site >= 0 && M->IsRoostSite(B.Site);
				}
			}
			(void)Over;
			(void)Cost;
			(void)CostN;
			(void)CostMax;
			std::printf("murmuration cost: %.3f ms per simulated second (%.3f ms a frame at 60 fps)\n", M->CpuMs / ((Dusk + 1.f - 15.5f) * 3600.f),
				M->CpuMs / ((Dusk + 1.f - 15.5f) * 3600.f) / 60.f);
			std::printf("murmuration: up to %d of %d starlings, 90%% within %.0f m of its centre at sunset-10 min; farthest over the sea %.0f m "
				"(bound %.0f); %.0f m from the roost at most; %d escape rolls; %d asleep in the reed at dusk+1 h\n",
				MaxIn, Total, Spread, M->MaxOffshore, W.Opt.MurmurOffshore, M->MaxFromRoost, M->Escapes, Asleep);
			Check(R, MaxIn > Total * 0.7f, "most starlings join the evening murmuration");
			Check(R, Spread > 10.f && Spread < 160.f, "the murmuration is one flock (90% within 160 m of its centre)");
			Check(R, M->MaxOffshore <= W.Opt.MurmurOffshore, "the murmuration never goes farther out to sea than the coastal strip");
			Check(R, M->MaxFromRoost < 450.f, "it keeps over its roost and round it");
			Check(R, M->Escapes > 50, "the peregrine's attacks send escape waves through it");
			Check(R, Asleep == Total, "every starling is asleep in the reed by dusk + 1 h");

			// --- the morning: out of the reed in waves before sunrise, feeding by midday ---
			W.Hour = W.Sunrise - 1.f;
			std::set<int> Left;
			float First = -1.f, Last = -1.f;
			int InTown = 0, Feeding = 0;
			std::map<std::string, int> Where;
			Advance(W, 3.5f, 0.2f, [&]()
			{
				for (const FBird& B : W.Birds)
				{
					if (B.Species == Star && !Left.count(B.Group) && IsAirborne(B.Act) && W.Hour < W.Sunrise + 1.f)
					{
						Left.insert(B.Group);
						First = First < 0.f ? W.Hour : First;
						Last = W.Hour;
					}
				}
			});
			for (const FBird& B : W.Birds)
			{
				if (B.Species != Star || B.Site < 0 || IsAirborne(B.Act))
				{
					continue;
				}
				const EHabitat K = W.Sites[B.Site].Kind;
				Where[HabitatName(K)]++;
				Feeding += B.Act == EAct::Peck || B.Act == EAct::Forage;
				InTown += K == EHabitat::Street;
			}
			std::printf("starlings' morning: %zu groups left the reed from %+.0f to %+.0f min of sunrise; at %.1f h on:", Left.size(), (First - W.Sunrise) * 60.f,
				(Last - W.Sunrise) * 60.f, W.Hour);
			for (auto& [K, N] : Where)
			{
				std::printf(" %s %d", K.c_str(), N);
			}
			std::printf("; %d feeding\n", Feeding);
			Check(R, First < W.Sunrise && Last - First > 0.15f, "they leave the roost before sunrise, group after group");
			Check(R, Where["Reed"] < Total / 10 && Feeding > Total / 4, "by late morning they are out feeding");
			Check(R, Where["Meadow"] > 0 && InTown > 0, "on the fields and in the town's lawns and streets");
		}
		if (std::getenv("BIRD_ONLY") && std::string(std::getenv("BIRD_ONLY")) == "murm")
		{
			return;
		}
		// --- the swallows: October stragglers on the Academgorodok's wires, the reed at night, gone in November ---
		{
			FWorld W;
			W.Opt.bMigration = true;
			ApplyMonths(Doy(10, 29));
			if (!W.LoadHabitats(Dir()))
			{
				return;
			}
			W.DayOfYear = Doy(10, 29);
			SunTimes(W.DayOfYear, W.Sunrise, W.Sunset);
			W.Hour = 11.f;
			W.Camera = Door;
			W.Populate();
			const int Sw = FindSpecies("BarnSwallow"), Sm = FindSpecies("SandMartin"), Swift = FindSpecies("CommonSwift");
			int Swallows = 0, Martins = 0, Swifts = 0;
			for (const FBird& B : W.Birds)
			{
				Swallows += B.Species == Sw && !B.bGone;
				Martins += B.Species == Sm && !B.bGone;
				Swifts += B.Species == Swift && !B.bGone;
			}
			std::printf("29 October with migration: %d barn swallows, %d sand martins, %d swifts\n", Swallows, Martins, Swifts);
			Check(R, Swallows > 0 && Swallows <= 14 && Swifts == 0, "a few swallows stay late into October; the swifts have gone");
			int Hawked = 0, OnWires = 0, Close = 0, InReed = 0, Perched = 0;
			Advance(W, (W.Sunset - 0.9f) - 11.f, 0.2f, [&]()
			{
				for (const FBird& B : W.Birds)
				{
					Hawked += (B.Species == Sw || B.Species == Sm) && B.Act == EAct::Hawk;
				}
			});
			// an hour before the night: in rows on the wires
			std::vector<const FBird*> Wired;
			for (const FBird& B : W.Birds)
			{
				if ((B.Species == Sw || B.Species == Sm) && !B.bGone)
				{
					Perched++;
					if (!IsAirborne(B.Act) && B.Site >= 0 && W.Sites[B.Site].Kind == EHabitat::Wire)
					{
						Wired.push_back(&B);
					}
				}
			}
			OnWires = (int)Wired.size();
			int Along = 0;
			for (const FBird* B : Wired)
			{
				const FSite& Wr = W.Sites[B->Site];
				const V3 D = (Wr.B - Wr.A).Flat().Norm();
				Along += std::fabs(D.X * std::cos(B->Yaw) + D.Y * std::sin(B->Yaw)) > 0.6f;
			}
			Check(R, Along == 0, "facing across the wire, not along it");
			for (size_t I = 0; I < Wired.size(); I++)
			{
				for (size_t J = I + 1; J < Wired.size(); J++)
				{
					Close += (Wired[I]->Pos - Wired[J]->Pos).Len() < 0.1f;
				}
			}
			std::printf("swallows and martins: hawked %d bird-steps by day; %d of %d on the wires before dusk, %d pairs closer than 10 cm\n", Hawked,
				OnWires, Perched, Close);
			Check(R, Hawked > 0, "they hunt insects on the wing by day");
			Check(R, OnWires > Perched / 2, "they gather on the wires before the night");
			Check(R, Close == 0, "sitting in a row a body apart (no two closer than 10 cm)");
			Advance(W, 2.f, 0.2f, [] {});
			for (const FBird& B : W.Birds)
			{
				InReed += (B.Species == Sw || B.Species == Sm) && !B.bGone && B.Act == EAct::Sleep && B.Site >= 0 && W.Sites[B.Site].Kind == EHabitat::Reed;
			}
			std::printf("at %.1f h %d of them asleep in the reed\n", W.Hour, InReed);
			Check(R, InReed == Perched, "and sleep in the reed");
			// a week on: gone south, but never from under the player's nose (they would simply vanish, popping)
			W.DayOfYear = Doy(11, 6);
			W.Hour = 23.f;
			FBird* Watched = nullptr;
			for (FBird& B : W.Birds)
			{
				Watched = !Watched && (B.Species == Sw || B.Species == Sm) && !B.bGone ? &B : Watched;
			}
			const V3 Away = W.Camera;
			if (Watched)
			{
				W.Camera = Watched->Pos + V3(3.f, 0.f, 1.f);
				W.Step(0.f);           // the tiers by the new camera
				W.UpdateMigration();
				Check(R, !Watched->bGone, "a straggler 3 m from the camera doesn't vanish (it leaves out of sight)");
			}
			W.Camera = Away;
			W.Step(0.f);           // the tiers by the new camera
			W.UpdateMigration();
			int Left = 0;
			for (const FBird& B : W.Birds)
			{
				Left += (B.Species == Sw || B.Species == Sm) && !B.bGone;
			}
			Check(R, Left == 0, "by 6 November the last stragglers have left");
			ApplyMonths(Doy(10, 29));
			for (FSpecies& S : MutableSpeciesTable())
			{
				S.bMigrant = false;
				S.Stragglers = 0;
			}
		}
		// --- other days: gulls follow the tide, swifts sleep aloft, rooks gather on a field before the rookery ---
		{
			FWorld W;
			Setup(W, 12.f);
			W.Camera = Door;
			const int Gull = FindSpecies("BlackHeadedGull"), Rook = FindSpecies("Rook"), Swift = FindSpecies("CommonSwift");
			auto OnBeach = [&](float Tide)
			{
				W.SeaZ = W.MeanSeaZ + Tide;
				W.Hour = 12.f;
				W.Resettle();
				int N = 0;
				Advance(W, 0.5f, 0.25f, [] {});
				for (const FBird& B : W.Birds)
				{
					N += B.Species == Gull && !IsAirborne(B.Act) && B.Site >= 0 && W.Sites[B.Site].Kind == EHabitat::Beach;
				}
				return N;
			};
			const int Low = OnBeach(-0.8f), High = OnBeach(0.8f);
			W.SeaZ = W.MeanSeaZ;
			std::printf("black-headed gulls on the beach at noon: %d at low tide, %d at high tide\n", Low, High);
			Check(R, Low > High, "gulls go down to the sand the low tide lays bare");
			W.Hour = W.Sunset - 1.3f;
			W.Resettle();
			int Gathering = 0, Rooks = 0;
			Advance(W, 0.5f, 0.25f, [] {});
			for (const FGroup& G : W.Groups)
			{
				if (G.Species == Rook)
				{
					Rooks++;
					Gathering += G.Plan == 3;
				}
			}
			std::printf("rooks an hour before sunset: %d of %d flocks at the pre-roost gathering\n", Gathering, Rooks);
			Check(R, Gathering > Rooks / 2, "rooks gather on a field before going to the rookery");
			W.Hour = 1.f;
			W.Resettle();
			float Low_ = 1e9f;
			int N = 0;
			for (const FBird& B : W.Birds)
			{
				if (B.Species == Swift && !B.bGone)
				{
					Low_ = std::min(Low_, B.Pos.Z - W.GroundZ(B.Pos.X, B.Pos.Y));
					N++;
				}
			}
			std::printf("swifts at 1 am: %d, the lowest %.0f m over the ground\n", N, Low_);
			Check(R, N > 0 && Low_ > 250.f, "swifts sleep on the wing high up");
		}
		Wind(R);
	}

	/** The wind (BirdBio's flying qualities): in a gale the small birds keep to the lee, the soarers take to the air. */
	void Wind(FResult& R)
	{
		struct FCount { int SmallOpen = 0, Small = 0, Soaring = 0, Soarers = 0, Albatross = 0, Bad = 0; };
		std::map<std::string, int> SoarBy;
		auto Census = [&](float Ms)
		{
			SoarBy.clear();
			FWorld W;
			FCount C;
			if (!Setup(W, 11.f))
			{
				return C;
			}
			W.Camera = Door;
			// from the west-south-west (compass to world: east = -X, north = +Y)
			W.Wind = V3(-0.92f, 0.38f, 0.f) * Ms;
			W.Gustiness = 0.3f;
			W.Resettle();
			Advance(W, 1.f, 0.25f, [&]
			{
				for (const FBird& B : W.Birds)
				{
					const FSpecies& S = SpeciesTable()[B.Species];
					if (B.bGone) continue;
					C.Soarers += (S.Flight == EFlight::FlapGlide || S.Flight == EFlight::Soar) && !S.bPredator && S.Id != "Peregrine";
					C.Soaring += B.Act == EAct::Soar;
					if (B.Act == EAct::Soar && std::getenv("BIRD_WIND_ACTS")) SoarBy[S.Id]++;
					C.Albatross += S.Id == "BlackBrowedAlbatross" && IsAirborne(B.Act);
				}
			});
			if (std::getenv("BIRD_WIND_ACTS"))
			{
				std::map<std::string, int> Acts;
				for (const FBird& B : W.Birds)
				{
					const FSpecies& S = SpeciesTable()[B.Species];
					if (!B.bGone) Acts[std::string(S.Length < 0.3f ? "small " : IsGullName(S.Id) ? "gull " : "other ") + ActName(B.Act) + (B.Site >= 0 ? std::string(" ") + HabitatName(W.Sites[B.Site].Kind) : "")]++;
				}
				for (auto& [K, N] : Acts) std::printf("    %5d %s\n", N, K.c_str());
				int Shown = 0;
				for (const FBird& B : W.Birds)
				{
					const FSpecies& S = SpeciesTable()[B.Species];
					if (B.Act == EAct::Fly && Shown < 12 && (Shown < 6 ? IsGullName(S.Id) : S.Length < 0.3f))
					{
						Shown++;
						std::printf("    %s fly %.0fs site %d (%.0f,%.0f) pos (%.0f,%.0f) target %d: to spot %.0f m (dz %.0f), vel %.1f,%.1f,%.1f struggle %.1f z-floor %.1f\n", S.Id.c_str(), B.ActTime, B.Site, B.Site >= 0 ? W.Sites[B.Site].Center().X : 0.f, B.Site >= 0 ? W.Sites[B.Site].Center().Y : 0.f, B.Pos.X, B.Pos.Y, B.Target, (B.Spot - B.Pos).Len2D(), B.Spot.Z - B.Pos.Z, B.Vel.X, B.Vel.Y, B.Vel.Z, B.Struggle, B.Pos.Z - W.TopZ(B.Pos.X, B.Pos.Y));
					}
				}
			}
			for (auto& [K, N] : SoarBy) std::printf("    soar %s %d\n", K.c_str(), N);
			for (const FBird& B : W.Birds)
			{
				const FSpecies& S = SpeciesTable()[B.Species];
				if (!std::isfinite(B.Pos.X) || !std::isfinite(B.Pos.Z) || B.Pos.Z < W.GroundZ(B.Pos.X, B.Pos.Y) - 3.f) C.Bad++;
				if (B.bGone || IsAirborne(B.Act) || B.Site < 0 || S.Length >= 0.3f || S.Flight == EFlight::Aerial) continue;
				const EHabitat K = W.Sites[B.Site].Kind;
				C.Small++;
				C.SmallOpen += K == EHabitat::Meadow || K == EHabitat::Wire || K == EHabitat::Roof || K == EHabitat::Beach || K == EHabitat::Tower;
			}
			return C;
		};
		const FCount Calm = Census(1.5f), Gale = Census(17.f);
		std::printf("wind 1.5 m/s: %d of %d small birds in the open, soaring %d bird-steps (albatross aloft %d)\n", Calm.SmallOpen, Calm.Small, Calm.Soaring, Calm.Albatross);
		std::printf("wind 17 m/s: %d of %d small birds in the open, soaring %d bird-steps (albatross aloft %d); %d birds out of the world\n", Gale.SmallOpen, Gale.Small, Gale.Soaring, Gale.Albatross, Gale.Bad);
		Check(R, Gale.Small > 0 && Calm.Small > 0 && Gale.SmallOpen * Calm.Small < Calm.SmallOpen * Gale.Small / 2, "in a gale the small birds leave the open for the lee (half as many or fewer)");
		Check(R, Gale.Soaring >= Calm.Soaring * 0.8f, "the gulls and the big soarers keep circling in a gale");
		Check(R, Gale.Albatross > Calm.Albatross * 5, "the albatross mostly sits on the sea in a calm and flies in a blow");
		Check(R, Gale.Bad == 0, "no bird is blown out of the world or into the ground");
	}


	/**
	 * The player's feeder (docs/bird_taming.md "Feeds and the feeder"): put up in the birds' range with a feed they like, after
	 * a couple of hours they come and peck on its tray; a feed nobody near likes, or a feeder out of every range, stays empty.
	 */
	void Feeders(FResult& R)
	{
		struct FCase { const char* Name; std::vector<std::pair<const char*, float>> Like; V3 Where; bool bExpect; };
		FWorld W;
		if (!Setup(W, 9.f))
		{
			Check(R, false, "feeders: world loads");
			return;
		}
		// a garden near the club's door, where the small birds live, and far out on the sea
		V3 Garden = Door + V3(12.f, 18.f, 0.f);
		for (const FSite& S : W.Sites)
		{
			if (S.Kind == EHabitat::Garden && (S.Center() - Door).Len2D() < 150.f)
			{
				Garden = S.Center() + V3(4.f, 0.f, 0.f);
				break;
			}
		}
		const std::vector<FCase> Cases = {
			{ "suet in the garden", { { "GreatTit", 1.f }, { "BlueTit", 1.f }, { "Robin", 0.6f } }, Garden, true },
			{ "grain in the garden", { { "FeralPigeon", 1.f }, { "HouseSparrow", 0.6f }, { "TreeSparrow", 0.5f }, { "Chaffinch", 0.4f } }, Garden, true },
			{ "a feed nobody eats", {}, Garden, false },
		};
		const std::vector<FSite> Base = W.Sites;
		for (const FCase& C : Cases)
		{
			FWorld X;
			Setup(X, 9.f);
			V3 Tray = C.Where;
			Tray.Z = X.GroundZ(Tray.X, Tray.Y) + 1.2f;
			const int F = X.AddFeeder(Tray, 0.2f, X.KindsAround(Tray));
			for (const auto& L : C.Like)
			{
				const int Sp = FindSpecies(L.first);
				if (Sp >= 0 && Sp < 64)
				{
					X.Feeders[F].Like[Sp] = L.second;
				}
			}
			X.Feeders[F].Food = 5.f;
			X.Player.bPresent = false;
			std::map<std::string, int> Seen;
			int Peaks = 0;
			float FirstAt = -1.f;
			float T = 0.f;
			Advance(X, 3.f, 0.1f, [&]()
			{
				T += 0.1f;
				X.Feeders[F].Ready = std::min(1.f, std::max(0.f, T / 3600.f - 1.5f));      // as the game ramps it: found between 1.5 and 2.5 h
				int On = 0;
				for (const FBird& B : X.Birds)
				{
					if (B.Site == X.Feeders[F].Site && !IsAirborne(B.Act) && !B.bGone)
					{
						On++;
						Seen[SpeciesTable()[B.Species].Id]++;
						if (FirstAt < 0.f)
						{
							FirstAt = T / 3600.f;
						}
					}
				}
				Peaks = std::max(Peaks, On);
			});
			std::string Kinds;
			for (int K = 0; K < NumHabitats; K++)
			{
				if (X.Feeders[F].Around & (1u << K))
				{
					Kinds += std::string(Kinds.empty() ? "" : " ") + HabitatName((EHabitat)K);
				}
			}
			std::printf("feeder, %s (%s): first bird on it after %.2f h, at most %d at once, food left %.2f;", C.Name, Kinds.c_str(), FirstAt, Peaks, X.Feeders[F].Food);
			bool bOnlyLiked = true;
			for (const auto& KV : Seen)
			{
				std::printf(" %s", KV.first.c_str());
				bool bLiked = false;
				for (const auto& L : C.Like)
				{
					bLiked |= KV.first == L.first;
				}
				bOnlyLiked &= bLiked;
			}
			std::printf("\n");
			if (C.bExpect)
			{
				Check(R, FirstAt > 1.4f && Peaks > 0, (std::string("feeder: birds come to ") + C.Name + " after a couple of hours").c_str());
				Check(R, bOnlyLiked, (std::string("feeder: only birds that like ") + C.Name + " come").c_str());
				Check(R, X.Feeders[F].Food < 5.f, "feeder: the feed goes down as they peck");
			}
			else
			{
				Check(R, Peaks == 0, (std::string("feeder: ") + C.Name + " stays empty").c_str());
			}
		}
		(void)Base;
	}

	int Test()
	{
		FResult R;
		if (std::getenv("BIRD_ONLY") && !std::strcmp(std::getenv("BIRD_ONLY"), "feeders"))
		{
			Feeders(R);
			std::printf("birdsim test: %d passed, %d failed\n", R.Pass, R.Fail);
			return R.Fail ? 1 : 0;
		}
		if (std::getenv("BIRD_ONLY"))
		{
			// BIRD_ONLY=routines (or murm: the starlings' evening and morning only; wind: the gale only)
			if (!std::strcmp(std::getenv("BIRD_ONLY"), "wind")) Wind(R);
			else Routines(R);
			std::printf("birdsim test: %d passed, %d failed\n", R.Pass, R.Fail);
			return R.Fail ? 1 : 0;
		}
		FWorld W;
		V3 P(100.f, 100.f, 0.f);
		float D;

		std::printf("flight distance by species, a person walking straight at the bird (1.4 m/s):\n");
		const char* Species[] = { "HouseSparrow", "FeralPigeon", "Robin", "GreatTit", "Rook", "HerringGull", "HoodedCrow", "Fieldfare" };
		std::map<std::string, float> Walk, RunD, Creep;
		for (const char* Sp : Species)
		{
			Lone(W, Sp, P);
			Run(W, { P + V3(-60.f, 0.f, 0.f), P, 1.4f, false }, EEvent::Flee, D);
			Walk[Sp] = D;
			Lone(W, Sp, P);
			Run(W, { P + V3(-60.f, 0.f, 0.f), P, 5.5f, false }, EEvent::Flee, D);
			RunD[Sp] = D;
			Lone(W, Sp, P);
			Run(W, { P + V3(-60.f, 0.f, 0.f), P + V3(-1.f, 0.f, 0.f), 0.6f, true }, EEvent::Flee, D);
			Creep[Sp] = D;
			std::printf("  %-12s walk %5.1f m   run %5.1f m   creep (crouched, 0.6 m/s) %5.1f m\n", Sp, Walk[Sp], RunD[Sp], Creep[Sp]);
		}
		Check(R, Walk["Rook"] > Walk["HouseSparrow"] * 2.f, "rooks are much warier than sparrows");
		Check(R, Walk["FeralPigeon"] < 6.f && Walk["FeralPigeon"] > 0.f, "town pigeons let you come within a few metres");
		Check(R, RunD["HouseSparrow"] > Walk["HouseSparrow"] * 1.8f, "running flushes a sparrow from much farther");
		Check(R, Creep["Robin"] < 0.f || Creep["Robin"] < Walk["Robin"] * 0.6f, "creeping crouched lets you come closer to a robin");

		// the befriending sequence: crouch, hold out food, wait
		{
			Lone(W, "GreatTit", P);
			std::map<int, float> First;
			Run(W, { P + V3(-30.f, 0.f, 0.f), P + V3(-3.5f, 0.f, 0.f), 0.5f, true, 0.f, true, 40.f }, EEvent::Eat, D, &First);
			auto At = [&](EEvent E) { return First.count((int)E) ? First[(int)E] : -1.f; };
			std::printf("great tit, crouched creep with food then wait: notice %.1f turn %.1f watch %.1f approach %.1f eat %.1f flee %.1f s\n",
				At(EEvent::Notice), At(EEvent::Turn), At(EEvent::Watch), At(EEvent::Approach), At(EEvent::Eat), At(EEvent::Flee));
			Check(R, At(EEvent::Notice) >= 0.f && At(EEvent::Turn) > At(EEvent::Notice), "it notices, then turns its head");
			Check(R, At(EEvent::Approach) > At(EEvent::Turn), "then it comes to the hand");
			Check(R, At(EEvent::Eat) > At(EEvent::Approach), "and eats from it");
			Check(R, At(EEvent::Flee) < 0.f, "without flying off");
			const V3 Palm = W.Player.Palm;
			Check(R, W.Birds[0].Act == EAct::Eat && (W.Birds[0].Pos - Palm).Len() < 0.05f, "it sits on the palm");
		}
		// the same with a sharp mouse turn on the way: it gets uneasy before it flies
		{
			Lone(W, "GreatTit", P);
			std::map<int, float> First;
			Run(W, { P + V3(-30.f, 0.f, 0.f), P + V3(-3.f, 0.f, 0.f), 0.5f, true, 420.f, true, 5.f }, EEvent::Flee, D, &First);
			auto At = [&](EEvent E) { return First.count((int)E) ? First[(int)E] : -1.f; };
			Check(R, At(EEvent::Flee) >= 0.f, "sharp mouse turns scare it off");
			Check(R, At(EEvent::Watch) >= 0.f && At(EEvent::Watch) < At(EEvent::Flee), "it watches (uneasy) before it flies");
		}
		// running at a rook: it flees away from the player
		{
			Lone(W, "Rook", P);
			Run(W, { P + V3(-80.f, 0.f, 0.f), P, 6.f, false }, EEvent::Flee, D);
			const FBird& B = W.Birds[0];
			Check(R, B.Pos.X > P.X - 5.f || B.Site == 1, "the rook flies away from the runner, not past them");
		}
		// after a fright it stays wary and keeps away from the person (who stands where the bird was)
		{
			Lone(W, "HouseSparrow", P);
			Run(W, { P + V3(-25.f, 0.f, 0.f), P + V3(-0.5f, 0.f, 0.f), 5.5f, false }, EEvent::Flee, D);
			// stand still at the spot for 40 s more
			float MinAlert = 1.f, Closest = 1e9f;
			bool bLandedNear = false;
			const V3 Feet = W.Player.Feet;
			for (int K = 0; K < 40 * 60; K++)
			{
				W.Player.Vel = V3();
				W.Step(1.f / 60.f);
				W.Events.clear();
				const FBird& B = W.Birds[0];
				if (K == 3 * 60 || K == 20 * 60)
				{
					MinAlert = std::min(MinAlert, B.Alert);
				}
				if (!IsAirborne(B.Act))
				{
					const float Dd = (B.Pos - Feet).Len2D();
					Closest = std::min(Closest, Dd);
					bLandedNear |= Dd < 15.f;
				}
			}
			std::printf("sparrow after a fright: alert at 3 s and 20 s >= %.2f; nearest it settled to the person: %.1f m\n", MinAlert, Closest);
			Check(R, MinAlert > 0.3f, "a frightened bird stays alert for tens of seconds");
			Check(R, !bLandedNear, "and doesn't settle near the person again");
		}
		// the pigeons by the door: they keep to their lawn, come back after a scare, and one comes to the hand
		{
			FWorld F;
			F.LoadHabitats(Dir());
			F.DayOfYear = 297.f;
			SunTimes(F.DayOfYear, F.Sunrise, F.Sunset);
			F.Hour = 11.f;
			F.Opt.PopulationScale = 0.f;
			F.Opt.bPredators = false;
			F.Populate();
			const V3 Lawn(-1.93f, 21.5f, F.GroundZ(-1.93f, 21.5f));
			const int First = F.AddFlock(FindSpecies("FeralPigeon"), Lawn, 6, 1.1f);
			F.Camera = Lawn;
			for (int K = 0; K < 600 * 60 / 10; K++)
			{
				F.Step(1.f / 6.f);
				F.Events.clear();
			}
			// someone standing at the door looking at them, then walking out along the path past them
			F.Player.bPresent = true;
			F.Player.Feet = V3(-1.93f - 2.5f, 11.5f + 3.f, 0.f);
			F.Player.Eye = F.Player.Feet + V3(0.f, 0.f, 1.65f);
			F.Player.Facing = (Lawn - F.Player.Eye).Norm();
			int Fled = 0;
			for (int K = 0; K < 30 * 60; K++)
			{
				const float T = K / 60.f;
				F.Player.Vel = T > 10.f && T < 20.f ? V3(0.f, 1.4f, 0.f) : V3();
				F.Player.Feet += F.Player.Vel * (1.f / 60.f);
				F.Player.Eye = F.Player.Feet + V3(0.f, 0.f, 1.65f);
				F.Camera = F.Player.Eye;
				F.Step(1.f / 60.f);
				for (const FEvent& E : F.Events)
				{
					Fled += E.Kind == EEvent::Flee && E.Bird >= First;
					if (E.Kind == EEvent::Flee && E.Bird >= First && std::getenv("BIRD_TRACE"))
					{
						const FBird& Bd = F.Birds[E.Bird];
						std::printf("    t %.1f bird %d flees at %.1f m from the person, alert %.2f target %d\n", T, E.Bird, (Bd.Pos - F.Player.Feet).Len2D(), Bd.Alert, Bd.Target);
					}
				}
				F.Events.clear();
			}
			std::printf("door pigeons: %d flushes while someone stood at the door 10 s and walked past them on the grass 2.5 m off the path\n", Fled);
			Check(R, Fled == 0, "the door pigeons let you walk past on the path");
			F.Player.bPresent = false;
			float Far = 0.f;
			for (int I = First; I < First + 6; I++)
			{
				Far = std::max(Far, (F.Birds[I].Pos - Lawn).Len2D());
			}
			std::printf("door pigeons after 10 min alone: farthest %.1f m from their lawn\n", Far);
			Check(R, Far < 4.f, "the door pigeons stay on their lawn");
			F.Scare(Lawn, 20.f, 2.f);
			float Away = 0.f;
			for (int K = 0; K < 150 * 6; K++)
			{
				F.Step(1.f / 6.f);
				F.Events.clear();
				if (K == 20 * 6)
				{
					for (int I = First; I < First + 6; I++)
					{
						Away = std::max(Away, (F.Birds[I].Pos - Lawn).Len2D());
					}
				}
			}
			Far = 0.f;
			for (int I = First; I < First + 6; I++)
			{
				Far = std::max(Far, (F.Birds[I].Pos - Lawn).Len2D());
			}
			std::printf("door pigeons after a bang: up to %.1f m away at 20 s, back within %.1f m at 150 s\n", Away, Far);
			Check(R, Away > 15.f && Away < 300.f && Far < 4.f, "they fly off from a bang, stay near, and come back");
		}
		// a flock: one flushes, the others follow
		{
			FWorld F;
			F.LoadHabitats(Dir());
			F.DayOfYear = 297.f;
			SunTimes(F.DayOfYear, F.Sunrise, F.Sunset);
			F.Hour = 11.f;
			F.Camera = Door;
			F.Populate();
			int Flock = -1;
			for (int G = 0; G < (int)F.Groups.size(); G++)
			{
				if (SpeciesTable()[F.Groups[G].Species].Id == "FeralPigeon" && F.Groups[G].Members.size() >= 8)
				{
					Flock = G;
					break;
				}
			}
			if (Flock >= 0)
			{
				const FGroup& G = F.Groups[Flock];
				// put the player on them, still; then run a step into the flock
				int OnGround = 0;
				for (int Id : G.Members)
				{
					OnGround += !IsAirborne(F.Birds[Id].Act);
				}
				const V3 C = F.Birds[G.Members[0]].Pos;
				F.Camera = C;
				F.Player.bPresent = true;
				F.Player.Feet = C + V3(-2.f, 0.f, 0.f);
				F.Player.Eye = F.Player.Feet + V3(0.f, 0.f, 1.65f);
				F.Player.Vel = V3(6.f, 0.f, 0.f);
				int Fled = 0;
				for (int K = 0; K < 120; K++)
				{
					F.Step(1.f / 60.f);
					for (const FEvent& E : F.Events)
					{
						Fled += E.Kind == EEvent::Flee && F.Birds[E.Bird].Group == Flock;
					}
					F.Events.clear();
				}
				if (std::getenv("BIRD_TRACE"))
				{
					for (int Id : G.Members)
					{
						const FBird& B = F.Birds[Id];
						std::printf("    %d %s alert %.2f d %.1f tier %d delay %.2f\n", Id, ActName(B.Act), B.Alert, (B.Pos - F.Player.Feet).Len(), B.Tier, B.Delay);
					}
				}
				std::printf("pigeon flock of %zu (%d on the ground): %d fled within 2 s of a run into it\n", G.Members.size(), OnGround, Fled);
				Check(R, Fled >= (int)(OnGround * 0.7f), "the flock goes up together");
			}
		}
		// cats and dogs (the animal life): no player, an animal coming straight at a lone bird
		{
			auto Beast = [&](const char* Sp, bool bCat, bool bStalk, float Speed)
			{
				Lone(W, Sp, P);
				W.Player.bPresent = false;
				V3 M = P + V3(-40.f, 0.f, 0.f);
				for (float T = 0.f; T < 40.f / Speed + 3.f; T += 1.f / 60.f)
				{
					W.Beasts.assign(1, FBeast{ M, V3(Speed, 0.f, 0.f), bCat, bStalk });
					W.Camera = M;
					W.Step(1.f / 60.f);
					for (const FEvent& E : W.Events)
					{
						if (E.Kind == EEvent::Flee && E.Bird == 0)
						{
							return (P - M).Len2D();
						}
					}
					W.Events.clear();
					M.X += Speed / 60.f;
				}
				return -1.f;
			};
			const float PigeonDog = Beast("FeralPigeon", false, false, 5.f);
			const float PigeonCat = Beast("FeralPigeon", true, false, 0.7f);
			const float SparrowDog = Beast("HouseSparrow", false, false, 5.f);
			const float SparrowCat = Beast("HouseSparrow", true, false, 0.7f);
			const float SparrowStalk = Beast("HouseSparrow", true, true, 0.28f);
			std::printf("animals at a bird: pigeon flushes from a running dog at %.1f m, a walking cat %.1f m; sparrow dog %.1f, cat %.1f, stalking cat %.1f m\n",
				PigeonDog, PigeonCat, SparrowDog, SparrowCat, SparrowStalk);
			Check(R, PigeonDog > 3.f && SparrowDog > 5.f, "a running dog puts birds up from well off");
			Check(R, SparrowCat > 2.f && SparrowCat < SparrowDog, "a walking cat puts a sparrow up nearer");
			Check(R, SparrowStalk > 0.f && SparrowStalk < SparrowCat, "a stalking cat gets closer still");
		}
		Feeders(R);
		Routines(R);
		std::printf("birdsim test: %d passed, %d failed\n", R.Pass, R.Fail);
		return R.Fail ? 1 : 0;
	}
}

int main(int Argc, char** Argv)
{
	const char* Mode = Argc > 1 ? Argv[1] : "day";
	if (!std::strcmp(Mode, "day"))
	{
		return Day(Argc, Argv);
	}
	if (!std::strcmp(Mode, "trace") && Argc > 5)
	{
		// trace <species> <speed> <crouch 0/1> <offer 0/1> [stop_short wait]: one walk at a lone bird, printed
		setenv("BIRD_TRACE", "1", 1);
		FWorld W;
		V3 P(100.f, 100.f, 0.f);
		Lone(W, Argv[2], P);
		float D;
		const float Short = Argc > 6 ? (float)std::atof(Argv[6]) : 0.f;
		const float Wait = Argc > 7 ? (float)std::atof(Argv[7]) : 5.f;
		Run(W, { P + V3(-30.f, 0.f, 0.f), P + V3(-Short, 0.f, 0.f), (float)std::atof(Argv[3]), Argv[4][0] == '1', 0.f, Argv[5][0] == '1', Wait }, EEvent::Flee, D);
		return 0;
	}
	if (!std::strcmp(Mode, "test"))
	{
		return Test();
	}
	std::fprintf(stderr, "usage: birdsim day [start hours] | test\n");
	return 2;
}
