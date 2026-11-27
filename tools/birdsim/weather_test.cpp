// The tests of the birds' weather behaviour (BirdSim.cpp "the weather", docs/bird_life.md "Weather"): shelter from the
// rain, sitting out a gust, heads into the wind on perches in the open, gulls riding the updraft at a cliff the wind
// blows onto. Part of `tools/birdsim/run.sh test`; BIRD_ONLY=weather runs only these.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace LyudBirds;

std::string BirdDataDir();      // birdsim.cpp: Content/BirdsData

namespace WeatherTest
{
	int Pass = 0, Fail = 0;
	constexpr float Pi = 3.14159265f;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What.c_str());
		(bOk ? Pass : Fail)++;
	}

	float AngleOff(float A, float B)
	{
		float D = std::fmod(A - B + Pi, 2.f * Pi);
		return std::fabs((D < 0.f ? D + 2.f * Pi : D) - Pi);
	}

	bool IsOpen(EHabitat K)
	{
		return K == EHabitat::Beach || K == EHabitat::Cliff || K == EHabitat::Meadow || K == EHabitat::Wire || K == EHabitat::Tower || K == EHabitat::Roof
			|| K == EHabitat::Mouth || K == EHabitat::Street;
	}
	bool IsCover(EHabitat K) { return K == EHabitat::Broadleaf || K == EHabitat::Conifer || K == EHabitat::Garden || K == EHabitat::Reed; }
	bool IsSitting(EAct A) { return A == EAct::Perch || A == EAct::Preen || A == EAct::Sleep; }

	/** A flat world (ground at 0, the sea far below), a July noon, no hawks, no nests, no corvid minds, no habits. */
	void Flat(FWorld& W)
	{
		W = FWorld();
		W.NoGridZ = 0.f;
		W.SeaZ = W.MeanSeaZ = -43.f;
		W.Opt.bPredators = false;
		W.Opt.bNests = false;
		W.Opt.bCorvids = false;
		W.Opt.bHabits = false;
		W.DayOfYear = 190.f;
		W.Sunrise = 4.4f;
		W.Sunset = 21.2f;
		W.Hour = 12.f;
		W.TempC = 18.f;
		W.Camera = V3(0.f, 0.f, 1.7f);
	}

	/** The real map on a July morning, no hawks (a hawk's pass empties the open too). */
	bool Map(FWorld& W, float Hour)
	{
		W = FWorld();
		if (!W.LoadHabitats(BirdDataDir()))
		{
			return false;
		}
		W.Opt.bPredators = false;
		W.DayOfYear = 190.f;
		W.Sunrise = 4.4f;
		W.Sunset = 21.2f;
		W.Hour = Hour;
		W.TempC = 17.f;
		W.Populate();
		return true;
	}

	void Run(FWorld& W, float Seconds, float Dt, const std::function<void()>& Each = nullptr)
	{
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			W.Hour += Dt / 3600.f;
			W.Step(Dt);
			if (Each)
			{
				Each();
			}
		}
	}

	/** A flock on one site of this kind, its group resting (0) or feeding (1) there. */
	int Flock(FWorld& W, const char* Species, const V3& P, int Count, EHabitat Kind, float Radius, int Plan)
	{
		const int Id = W.AddFlock(FindSpecies(Species), P, Count, Radius);
		if (Id >= 0)
		{
			W.Sites[W.Birds[Id].Site].Kind = Kind;
			W.Groups[W.Birds[Id].Group].Plan = Plan;
		}
		return Id;
	}

	// --- rain ------------------------------------------------------------------------------------------

	struct FRainCount
	{
		int Open = 0, Cover = 0, Land = 0, Hunched = 0, GullsOpen = 0, Gulls = 0;
	};

	FRainCount CountRain(const FWorld& W, const std::vector<char>& Minds)
	{
		FRainCount C;
		for (const FBird& B : W.Birds)
		{
			if (B.bGone || B.Site < 0 || B.Act == EAct::Fly || B.Act == EAct::Takeoff || B.Act == EAct::Flee || B.Act == EAct::Hawk || B.Act == EAct::Soar)
			{
				continue;
			}
			const EHabitat K = W.Sites[B.Site].Kind;
			const bool bGull = SpeciesTable()[B.Species].Id.find("Gull") != std::string::npos;
			C.Gulls += bGull;
			C.GullsOpen += bGull && IsOpen(K);
			if (!Minds[B.Species])
			{
				continue;
			}
			C.Land++;
			C.Open += IsOpen(K);
			C.Cover += IsCover(K);
			C.Hunched += B.Weather == (uint8_t)EWeathering::Hunched;
		}
		return C;
	}

	void Rain()
	{
		std::printf("rain:\n");
		FWorld Dry, Wet;
		if (!Map(Dry, 9.f) || !Map(Wet, 9.f))
		{
			std::printf("  (no map data: skipped)\n");
			return;
		}
		Wet.Rain = 6.f;
		std::vector<char> Minds(SpeciesTable().size(), 0);
		int Kinds = 0;
		for (size_t I = 0; I < SpeciesTable().size(); I++)
		{
			Minds[I] = Wet.RainStress(SpeciesTable()[I]) > 0.5f;
			Kinds += Minds[I];
		}
		Check(Wet.RainStress(SpeciesTable()[FindSpecies("HouseSparrow")]) > 0.9f && Wet.RainStress(SpeciesTable()[FindSpecies("HerringGull")]) == 0.f
			&& Wet.RainStress(SpeciesTable()[FindSpecies("MuteSwan")]) == 0.f && Wet.RainStress(SpeciesTable()[FindSpecies("BarnSwallow")]) == 0.f
			&& Dry.RainStress(SpeciesTable()[FindSpecies("HouseSparrow")]) == 0.f,
			"a steady rain is the small land birds' trouble, not the gulls', the swan's or the swallows' (and nobody's when it is dry)");
		Dry.Resettle();
		Wet.Resettle();
		Run(Dry, 2400.f, 0.25f);
		Run(Wet, 2400.f, 0.25f);
		const FRainCount D = CountRain(Dry, Minds), R = CountRain(Wet, Minds);
		std::printf("  %d species mind the rain; dry: %d of %d of them under the open sky, %d under cover; in 6 mm/h: %d of %d, %d under cover, %d sitting hunched\n",
			Kinds, D.Open, D.Land, D.Cover, R.Open, R.Land, R.Cover, R.Hunched);
		std::printf("  gulls on the ground in the open: dry %d of %d, in the rain %d of %d\n", D.GullsOpen, D.Gulls, R.GullsOpen, R.Gulls);
		Check(D.Land > 200 && R.Open * D.Land * 2 < D.Open * R.Land, "in the rain half as many of them (or fewer) sit under the open sky");
		Check(R.Cover * D.Land > D.Cover * R.Land && R.Hunched > 20 && D.Hunched == 0, "they are under leaves and in the reeds, many sitting tight (none when it is dry)");
		Check(R.GullsOpen * 10 >= D.GullsOpen * 7, "the gulls stay where they were");
		// the rain comes on over a dry morning: nobody is told, the flocks find cover as they move
		Dry.Rain = 6.f;
		Run(Dry, 1800.f, 0.25f);
		const FRainCount Later = CountRain(Dry, Minds);
		std::printf("  half an hour after the rain came on: %d of %d under the open sky (%d before)\n", Later.Open, Later.Land, D.Open);
		Check(Later.Open * D.Land * 3 < D.Open * Later.Land * 2, "half an hour into a shower a third of them or more have left the open");
	}

	// --- gusts -----------------------------------------------------------------------------------------

	void Gusts()
	{
		std::printf("gusts:\n");
		const FSpecies& Sp = SpeciesTable()[FindSpecies("HouseSparrow")];
		struct FSeen { int InGust = 0, InLull = 0; long Crouched = 0, Steps = 0; float Longest = 0.f; };
		auto Watch = [&](float Wind, float Gustiness)
		{
			FWorld W;
			Flat(W);
			const int First = Flock(W, "HouseSparrow", V3(10.f, 0.f, 6.f), 12, EHabitat::Roof, 4.f, 1);
			W.Wind = V3(-Wind, 0.f, 0.f);
			W.Gustiness = Gustiness;
			FSeen S;
			std::vector<char> Sat(W.Birds.size(), 0), Held(W.Birds.size(), 0);
			std::vector<float> Stretch(W.Birds.size(), 0.f);
			const int Group = W.Birds[First].Group;
			Run(W, 900.f, 0.05f, [&]
			{
				W.Groups[Group].Plan = 1;
				for (const FBird& B : W.Birds)
				{
					const bool bFlew = B.Act == EAct::Fly || B.Act == EAct::Takeoff;
					if (bFlew && Sat[B.Id])
					{
						(Held[B.Id] ? S.InGust : S.InLull)++;
					}
					Sat[B.Id] = B.Act == EAct::Perch || B.Act == EAct::Preen;
					Held[B.Id] = Sat[B.Id] && W.GustAt(B) > 0.75f * Sp.WindLimit;      // (well inside a gust, not on its edge)
					const bool bLow = B.Weather == (uint8_t)EWeathering::Crouched;
					S.Crouched += bLow;
					S.Steps++;
					Stretch[B.Id] = bLow ? Stretch[B.Id] + 0.05f : 0.f;
					S.Longest = std::max(S.Longest, Stretch[B.Id]);
				}
			});
			return S;
		};
		const float Blow = 0.62f * Sp.WindLimit;
		const FSeen Gusty = Watch(Blow, 0.4f), Steady = Watch(Blow, 0.f), Calm = Watch(2.f, 0.4f);
		std::printf("  sparrows on a roof ridge, wind %.1f m/s (they hold on from %.1f): gusty: %d flits in the lulls, %d into a gust, crouched %.0f%% of the time (%.1f s at the longest); steady: %d flits, crouched %.0f%%; a light air: %d flits, crouched %.0f%%\n",
			Blow, 0.7f * Sp.WindLimit, Gusty.InLull, Gusty.InGust, 100.f * Gusty.Crouched / std::max(1L, Gusty.Steps), Gusty.Longest,
			Steady.InLull + Steady.InGust, 100.f * Steady.Crouched / std::max(1L, Steady.Steps), Calm.InLull + Calm.InGust, 100.f * Calm.Crouched / std::max(1L, Calm.Steps));
		Check(Gusty.InGust == 0 && Gusty.InLull > 10, "in a gusty wind the sparrows flit in the lulls, never into a gust");
		Check(Gusty.Crouched > 0 && Gusty.Longest < 15.f, "through a gust they crouch on the ridge, for as long as it lasts");
		Check(Steady.Crouched == 0 && Calm.Crouched == 0, "a steady wind of the same strength, or a light air with its puffs, holds nobody down");
	}

	/** A steady gale comes up over a calm morning: the small land birds it is too much for leave the open for the lee
	 *  within half an hour (gusts over a perch hold a bird for a moment, never through a gale that does not let up). */
	void Gale()
	{
		std::printf("a steady gale:\n");
		FWorld Calm, Blown;
		if (!Map(Calm, 9.f) || !Map(Blown, 9.f))
		{
			std::printf("  (no map data: skipped)\n");
			return;
		}
		for (FWorld* W : { &Calm, &Blown })
		{
			W->Wind = V3(-3.f, 0.f, 0.f);
			W->Gustiness = 0.1f;
			W->Resettle();
			Run(*W, 1200.f, 0.25f);
		}
		Blown.Wind = V3(-11.f, 0.f, 0.f);       // a steady gale, hardly gusting: its lulls never fall below a small bird's limit
		std::vector<char> Minds(SpeciesTable().size(), 0);
		int Kinds = 0;
		for (size_t I = 0; I < SpeciesTable().size(); I++)
		{
			const FSpecies& S = SpeciesTable()[I];
			Minds[I] = Blown.WindStress(S) > 0.75f && S.Id.find("Gull") == std::string::npos && S.Flight != EFlight::Soar;
			Kinds += Minds[I];
		}
		Run(Calm, 1800.f, 0.25f);
		long Crouched = 0, Seen = 0;
		Run(Blown, 1800.f, 0.25f, [&]
		{
			for (const FBird& B : Blown.Birds)
			{
				if (!B.bGone && Minds[B.Species] && B.Site >= 0 && IsSitting(B.Act))
				{
					Seen++;
					Crouched += B.Weather == (uint8_t)EWeathering::Crouched;
				}
			}
		});
		const FRainCount C = CountRain(Calm, Minds), G = CountRain(Blown, Minds);
		std::printf("  %d species find 11 m/s too much; calm: %d of %d of them in the open; half an hour into the gale: %d of %d (crouched %.1f%% of their sitting time)\n",
			Kinds, C.Open, C.Land, G.Open, G.Land, 100.f * Crouched / std::max(1L, Seen));
		Check(C.Land > 100 && G.Land > 100 && G.Open * C.Land * 3 < C.Open * G.Land * 2, "a steady gale moves the small birds out of the open within half an hour (a third or more of them)");
	}

	// --- heads to the wind -----------------------------------------------------------------------------

	void Facing()
	{
		std::printf("heads to the wind:\n");
		{
			FWorld W;
			Flat(W);
			const int Gulls = Flock(W, "BlackHeadedGull", V3(20.f, 0.f, 0.f), 10, EHabitat::Beach, 5.f, 0);
			const int Tits = Flock(W, "GreatTit", V3(-40.f, 0.f, 0.f), 8, EHabitat::Broadleaf, 3.f, 0);
			const int Wire = Flock(W, "BarnSwallow", V3(0.f, 60.f, 7.f), 8, EHabitat::Wire, 0.f, 0);
			FSite& Line = W.Sites[W.Birds[Wire].Site];
			Line.A = V3(-6.f, 60.f, 7.f);
			Line.B = V3(6.f, 60.f, 7.f);
			W.Resettle();
			auto Hold = [&](int Group, int Plan) { W.Groups[W.Birds[Group].Group].Plan = Plan; };
			auto Sit = [&]
			{
				Hold(Gulls, 0);
				Hold(Tits, 0);
				Hold(Wire, 0);
			};
			// the birds of a flock that have sat for a few seconds, looked at through Seconds: how many looks, and the one
			// furthest off the way Toward
			auto Spread = [&](float Seconds, int First, int Count, float Toward, float& Worst)
			{
				int Looks = 0;
				Worst = 0.f;
				Run(W, Seconds, 0.05f, [&]
				{
					Sit();
					for (int I = First; I < First + Count; I++)
					{
						const FBird& B = W.Birds[I];
						if (IsSitting(B.Act) && B.ActTime > 4.f)
						{
							Looks++;
							Worst = std::max(Worst, AngleOff(B.Yaw, Toward));
						}
					}
				});
				return Looks;
			};
			W.Wind = V3(-1.5f, 0.f, 0.f);
			Run(W, 30.f, 0.05f, Sit);
			float CalmWorst = 0.f, GullWorst = 0.f, TitWorst = 0.f, WireWorst = 0.f, Unused = 0.f;
			const int CalmSitting = Spread(60.f, Gulls, 10, 0.f, CalmWorst);
			// the wind gets up from the east of this world (it blows toward -X: up the wind is +X) and across the wire
			W.Wind = V3(-8.f, 0.f, 0.f);
			Run(W, 15.f, 0.05f, Sit);
			const int GullSitting = Spread(60.f, Gulls, 10, 0.f, GullWorst);
			const int TitSitting = Spread(20.f, Tits, 8, 0.f, TitWorst);
			W.Wind = V3(-2.f, -8.f, 0.f);
			Run(W, 15.f, 0.05f, Sit);
			const int WireSitting = Spread(30.f, Wire, 8, 0.5f * Pi, WireWorst);
			(void)Unused;
			std::printf("  gulls on the sand: in a light air the furthest off the wind's line %.0f deg (%d looks); at 8 m/s %.0f deg (%d looks); tits in a crown %.0f deg (%d); swallows on a wire across the wind %.0f deg off square to it, upwind side (%d)\n",
				CalmWorst * 180.f / Pi, CalmSitting, GullWorst * 180.f / Pi, GullSitting, TitWorst * 180.f / Pi, TitSitting, WireWorst * 180.f / Pi, WireSitting);
			Check(GullSitting >= 200 && GullWorst < 0.35f, "gulls standing on the beach in a fresh wind all face into it");
			Check(CalmSitting >= 200 && CalmWorst > 0.8f, "in a light air they face any way");
			Check(TitSitting >= 100 && TitWorst > 0.8f, "in a tree's crown nobody turns to the wind");
			Check(WireSitting >= 100 && WireWorst < 0.05f, "swallows on a wire sit square across it, breasts to the wind");
		}
		FWorld W;
		if (!Map(W, 13.f))
		{
			std::printf("  (no map data: skipped)\n");
			return;
		}
		W.Wind = V3(-0.92f, 0.38f, 0.f) * 8.f;
		W.Gustiness = 0.25f;
		W.Resettle();
		Run(W, 900.f, 0.25f);
		const float Up = std::atan2(-W.Wind.Y, -W.Wind.X);
		int InOpen = 0, Facing = 0, Sheltered = 0, ShelteredFacing = 0;
		for (const FBird& B : W.Birds)
		{
			if (B.bGone || B.Site < 0 || !IsSitting(B.Act) || B.ActTime < 3.f)
			{
				continue;
			}
			const EHabitat K = W.Sites[B.Site].Kind;
			const bool bToWind = AngleOff(B.Yaw, Up) < 0.5f;
			if (W.FacesWind(B) && K != EHabitat::Wire)
			{
				InOpen++;
				Facing += bToWind;
			}
			else if (IsCover(K))
			{
				Sheltered++;
				ShelteredFacing += bToWind;
			}
		}
		std::printf("  the map, wind 8 m/s from the west-south-west: %d of %d birds sitting in the open face it, %d of %d under cover happen to\n", Facing, InOpen, ShelteredFacing, Sheltered);
		Check(InOpen > 30 && Facing * 10 >= InOpen * 9, "on the map nine in ten of the birds sitting in the open face the wind");
		Check(Sheltered > 30 && ShelteredFacing * 2 < Sheltered, "and those under cover sit as they like");
	}

	// --- the updraft at the cliff ----------------------------------------------------------------------

	void Ridge()
	{
		std::printf("the cliff's updraft:\n");
		struct FSeen { long Riders = 0, Facing = 0, Under = 0, Far = 0; double Speed = 0.0, Agl = 0.0; int Birds = 0; long Circling = 0; };
		V3 MeanOut;
		int Cliffs = 0;
		auto Blow = [&](float Onshore)
		{
			FSeen S;
			FWorld W;
			if (!Map(W, 11.f))
			{
				return S;
			}
			MeanOut = V3();
			Cliffs = 0;
			for (const FSite& Site : W.Sites)
			{
				if (Site.Kind == EHabitat::Cliff)
				{
					MeanOut += W.CliffOut(Site);
					Cliffs++;
				}
			}
			if (!Cliffs)
			{
				return S;
			}
			MeanOut = MeanOut.Flat().Norm();
			W.Wind = MeanOut * -Onshore;
			W.Gustiness = 0.2f;
			W.Resettle();
			std::vector<char> Rode(W.Birds.size(), 0);
			const float Up = std::atan2(-W.Wind.Y, -W.Wind.X);
			long Step = 0;
			Run(W, 3600.f, 0.25f, [&]
			{
				if (++Step % 8)
				{
					return;
				}
				for (const FBird& B : W.Birds)
				{
					if (B.bGone || B.Act != EAct::Soar)
					{
						continue;
					}
					if (B.Weather != (uint8_t)EWeathering::Ridge)
					{
						S.Circling++;
						continue;
					}
					if (B.ActTime < 20.f)
					{
						continue;                   // (still coming over from where it stood)
					}
					Rode[B.Id] = 1;
					S.Riders++;
					S.Facing += AngleOff(B.Yaw, Up) < 1.f;
					S.Speed += B.Vel.Len2D();
					const float Agl = B.Pos.Z - std::max(W.GroundZ(B.Pos.X, B.Pos.Y), W.SeaZ);
					S.Agl += Agl;
					S.Under += Agl < 0.5f;
					if (Agl < 0.5f && std::getenv("BIRD_TRACE"))
					{
						std::printf("    low: %s t %.0f pos %.0f,%.0f,%.1f ground %.1f beat at %.0f,%.0f,%.1f vel %.1f,%.1f,%.1f\n", SpeciesTable()[B.Species].Id.c_str(), B.ActTime, B.Pos.X, B.Pos.Y, B.Pos.Z,
							W.GroundZ(B.Pos.X, B.Pos.Y), B.FlyFrom.X, B.FlyFrom.Y, B.FlyFrom.Z, B.Vel.X, B.Vel.Y, B.Vel.Z);
					}
					// off its beat: more than 25 m from the line along the rim it rides, or past the beat's ends
					const V3 Along(std::cos(B.RidgeYaw), std::sin(B.RidgeYaw), 0.f);
					const V3 Off = (B.Pos - B.FlyFrom).Flat();
					const float AlongM = Off.Dot(Along);
					S.Far += B.Pos.Z > B.FlyFrom.Z - 8.f && ((Off - Along * AlongM).Len2D() > 25.f || std::fabs(AlongM) > 75.f);      // (once it is up at the rim)
				}
			});
			for (const char R : Rode)
			{
				S.Birds += R;
			}
			return S;
		};
		const FSeen On = Blow(9.f);
		if (!Cliffs)
		{
			std::printf("  (no map data or no cliffs: skipped)\n");
			return;
		}
		const FSeen Off = Blow(-9.f), Light = Blow(2.5f);
		const float N = (float)std::max(1L, On.Riders);
		std::printf("  %d cliff sites, facing (%.2f, %.2f) on average; wind 9 m/s onto them: %d gulls rode the updraft in an hour (%ld looks): %.0f%% head to the wind, %.1f m/s over the ground, %.0f m over the ground or the sea, %ld looks off the beat, %ld under 0.5 m\n",
			Cliffs, MeanOut.X, MeanOut.Y, On.Birds, On.Riders, 100.f * On.Facing / N, On.Speed / N, On.Agl / N, On.Far, On.Under);
		std::printf("  wind 9 m/s off the land: %d riders (circling instead: %ld looks); a light onshore air: %d riders\n", Off.Birds, Off.Circling, Light.Birds);
		Check(On.Birds >= 5, "with the wind onto the cliff gulls ride its updraft");
		Check(On.Facing * 10 >= On.Riders * 8 && On.Speed / N < 7.f, "they hang head to the wind and slide slowly along the rim");
		Check(On.Far * 20 <= On.Riders && On.Under == 0 && On.Agl / N > 3.f, "in the band of rising air over the rim, never into the cliff");
		Check(Off.Birds == 0 && Light.Birds == 0 && Off.Circling > 0, "with the wind off the land or too light there is no updraft: they circle as before");
	}
}

int WeatherTests(int* Passed)
{
	using namespace WeatherTest;
	Pass = Fail = 0;
	std::printf("the weather:\n");
	Rain();
	Gusts();
	Gale();
	Facing();
	Ridge();
	if (Passed)
	{
		*Passed += Pass;
	}
	else
	{
		std::printf("weather: %d passed, %d failed\n", Pass, Fail);
	}
	return Fail;
}
