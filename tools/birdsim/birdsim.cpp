// Runs the bird life (Private/Birds/BirdSim.cpp) outside the engine, for tuning and for the debug maps.
//
//   tools/birdsim/run.sh day  [start_hour hours]      a simulated day -> Saved/BirdSim/day.csv (+ maps via render_birds.py)
//   tools/birdsim/run.sh test                        scripted player scenarios (approach, run, offer food) -> pass/fail
//
// The player stands at the club's door unless a scenario moves them. The sun times come from a simple
// solar formula for the game's place (54.96 N, 20.40 E, UTC+2) on the game's date.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

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

	int Test()
	{
		FResult R;
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
			const V3 Lawn(2.07f, 21.5f, F.GroundZ(2.07f, 21.5f));
			const int First = F.AddFlock(FindSpecies("FeralPigeon"), Lawn, 6, 2.5f);
			F.Camera = Lawn;
			for (int K = 0; K < 600 * 60 / 10; K++)
			{
				F.Step(1.f / 6.f);
				F.Events.clear();
			}
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
