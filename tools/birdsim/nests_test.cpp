// The breeding year's tests (Private/Birds/BirdNest.cpp, docs/bird_nests.md): the table, the nests on the real map, every
// nest's timeline through a year, then mornings in the season near the camera (building, sitting, feeding, fledglings,
// alarm and dives at the player, a deserted clutch) and what it costs. Part of `tools/birdsim/run.sh test`;
// BIRD_ONLY=nests runs only these.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdCorvid.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdNest.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace LyudBirds;

namespace
{
	int Pass = 0, Fail = 0;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What.c_str());
		(bOk ? Pass : Fail)++;
	}

	const char* Data() { return std::getenv("BIRD_DATA") ? std::getenv("BIRD_DATA") : "unreal/Lyudmilovsk/Content/BirdsData"; }

	/** The real map on a day of the year at an hour (sun times roughly for the day at 55 N). */
	bool Map(FWorld& W, float Doy, float Hour, bool bNests = true)
	{
		W = FWorld();
		if (!W.LoadHabitats(Data()))
		{
			return false;
		}
		W.Opt.bNests = bNests;
		W.DayOfYear = Doy;
		W.Year = 2027;
		// day length at 55 N: ~7 h in December, ~17.5 h in June
		const float Half = 6.15f + 2.6f * std::sin((Doy - 80.f) / 365.f * 2.f * 3.14159265f);
		W.Sunrise = 12.3f - Half;
		W.Sunset = 12.3f + Half;
		W.Hour = Hour;
		W.Camera = V3(-1.93f, 11.9f, 1.7f);
		W.Populate();
		return true;
	}

	std::vector<int> Trace;

	void Run(FWorld& W, float Seconds, float Dt, const std::function<void()>& Each = nullptr)
	{
		int Steps = 0;
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			if (std::getenv("BIRD_TRACE") && (Steps++ % (int)(2.f / Dt)) == 0)
			{
				for (int Id : Trace)
				{
					const FBird& B = W.Birds[Id];
					std::printf("    t%5.0f #%d %-8s len %4.1f/%4.1f duty %-6s site %d pos %.1f %.1f %.1f spot %.1f %.1f %.1f tier %d\n", T, Id, ActName(B.Act),
						B.ActTime, B.ActLen, DutyName(W.Nesting()->DutyOf(Id)), B.Site, B.Pos.X, B.Pos.Y, B.Pos.Z, B.Spot.X, B.Spot.Y, B.Spot.Z, B.Tier);
				}
			}
			W.Hour += Dt / 3600.f;
			W.Step(Dt);
			if (Each)
			{
				Each();
			}
			W.Events.clear();
			if (W.Nesting())
			{
				W.Nesting()->Events.clear();
			}
			if (W.Corvids())
			{
				W.Corvids()->Events.clear();
			}
		}
	}

	/** The nest of a species nearest P (optionally in a stage). */
	int NearestNest(FNesting& N, const std::string& Species, const V3& P, EStage Stage = EStage::Num)
	{
		int Best = -1;
		float BestD = 1e9f;
		for (const FNest& Nest : N.Nests)
		{
			if (SpeciesTable()[Nest.Species].Id != Species || (Stage != EStage::Num && Nest.State.Stage != Stage))
			{
				continue;
			}
			const float D = (Nest.Pos - P).Len2D();
			if (D < BestD)
			{
				BestD = D;
				Best = Nest.Id;
			}
		}
		return Best;
	}

	/** A day this nest is in a stage, this year (-1 none). */
	float DayIn(FNesting& N, int Nest, EStage Stage, float From = 1.f, float Frac = 0.5f)
	{
		float First = -1.f, Last = -1.f;
		for (float D = From; D < 365.f; D += 0.25f)
		{
			if (N.StateOn(Nest, N.Year, D).Stage == Stage)
			{
				if (First < 0.f)
				{
					First = D;
				}
				Last = D;
			}
			else if (First >= 0.f)
			{
				break;
			}
		}
		return First < 0.f ? -1.f : First + (Last - First) * Frac;
	}

	void LookAt(FWorld& W, const V3& P)
	{
		W.Camera = P + V3(6.f, 0.f, 1.7f);
		W.Player.bPresent = false;
	}
}

int NestTests(int* Passed)
{
	Pass = Fail = 0;
	std::printf("nests:\n");

	// the table: every species of species.json that breeds here has a row, the rest are known not to
	{
		std::ifstream F(std::string(Data()) + "/species.json");
		std::string Text((std::istreambuf_iterator<char>(F)), std::istreambuf_iterator<char>());
		int Rows = 0, Missing = 0;
		std::string Miss;
		for (size_t At = Text.find("\"id\""); At != std::string::npos; At = Text.find("\"id\"", At + 4))
		{
			const size_t A = Text.find('"', At + 4) + 1, B = Text.find('"', A);
			const std::string Id = Text.substr(A, B - A);
			Rows++;
			if (!BreedOf(Id) && Id != "Waxwing" && Id != "BlackBrowedAlbatross")
			{
				Missing++;
				Miss += " " + Id;
			}
		}
		Check(Rows >= 36 && Missing == 0, "every breeding species of species.json has a breeding row (" + std::to_string(Rows) + " species;" + Miss + ")");
		bool bKnown = true;
		for (const FBreedKind& K : BreedTable())
		{
			bKnown &= FindSpecies(K.Id) >= 0;
			bKnown &= K.LayFrom <= K.LayUntil && K.LayUntil <= K.LayLast && K.ClutchMin <= K.ClutchMax && K.IncubDays > 0;
		}
		Check(bKnown, "every row names a species of the bird life and has sane dates");
		std::ofstream("Saved/BirdSim/nest_table.json") << FNesting::TableJson();
	}

	FWorld W;
	if (!Map(W, 297.f, 11.f))
	{
		std::printf("  (no habitats in %s: map tests skipped)\n", Data());
		if (Passed)
		{
			*Passed = Pass;
		}
		return Fail;
	}
	FNesting& N = *W.Nesting();
	// the nests on the map
	{
		std::map<std::string, int> Per;
		std::map<std::string, int> Stage;
		int Floating = 0;
		for (const FNest& Nest : N.Nests)
		{
			Per[SpeciesTable()[Nest.Species].Id]++;
			Stage[StageName(Nest.State.Stage)]++;
			const float G = W.GroundZ(Nest.Pos.X, Nest.Pos.Y);
			if (Nest.Pos.Z < G - 3.f && W.Sites[Nest.Site].Kind != EHabitat::Cliff)
			{
				Floating++;
				std::printf("  under the ground: %s at %.0f %.0f %.1f (ground %.1f, site %s)\n", SpeciesTable()[Nest.Species].Id.c_str(), Nest.Pos.X, Nest.Pos.Y, Nest.Pos.Z, G,
					HabitatName(W.Sites[Nest.Site].Kind));
			}
		}
		std::string L;
		for (auto& P : Per)
		{
			L += " " + P.first + " " + std::to_string(P.second);
		}
		std::printf("  nests by species:%s\n", L.c_str());
		L.clear();
		for (auto& P : Stage)
		{
			L += " " + P.first + " " + std::to_string(P.second);
		}
		std::printf("  late October:%s\n", L.c_str());
		int Species = 0;
		for (const FBreedKind& K : BreedTable())
		{
			Species += Per.count(K.Id) ? 1 : 0;
		}
		Check(Species >= (int)BreedTable().size() - 3, "nests for nearly every breeding species (" + std::to_string(Species) + " of " + std::to_string(BreedTable().size()) + ")");
		Check(Per["Rook"] >= 10 && Per["SandMartin"] >= 5, "a rookery and a sand martin colony");
		Check(Floating == 0, "no nest is deep under the ground (" + std::to_string(Floating) + ")");
		int Busy = 0;
		for (const FNest& Nest : N.Nests)
		{
			const EStage S = Nest.State.Stage;
			Busy += (S == EStage::Build || S == EStage::Eggs || S == EStage::Chicks) && SpeciesTable()[Nest.Species].Id != "FeralPigeon" ? 1 : 0;
		}
		Check(Busy == 0 && Stage["Old"] > (int)N.Nests.size() / 2, "late October: old nests about, nobody breeding but the feral pigeons");
		// colonies: the rooks' nests close together, several to a tree
		int RookNear = 0, Rooks = 0;
		for (const FNest& A : N.Nests)
		{
			if (SpeciesTable()[A.Species].Id != "Rook")
			{
				continue;
			}
			Rooks++;
			for (const FNest& B : N.Nests)
			{
				if (&A != &B && B.Species == A.Species && (A.Pos - B.Pos).Len2D() < 50.f)
				{
					RookNear++;
					break;
				}
			}
		}
		Check(Rooks > 0 && RookNear >= Rooks * 0.8f, "rooks nest in rookeries (" + std::to_string(RookNear) + " of " + std::to_string(Rooks) + " have a neighbour within 50 m)");
	}

	// each nest's year: the stages in order, laying in the species' window, deterministic
	{
		bool bOrder = true, bWindow = true, bSame = true, bCycle = true;
		std::map<std::string, std::vector<float>> Lays;
		for (const FNest& Nest : N.Nests)
		{
			const FBreedKind& K = BreedTable()[Nest.Kind];
			EStage Prev = EStage::None;
						float FirstLay = -1.f;
			for (float D = 1.f; D < 365.f; D += 0.5f)
			{
				const FNestState S = N.StateOn(Nest.Id, N.Year, D);
				if (S.Stage == EStage::Eggs && FirstLay < 0.f)
				{
					FirstLay = D;
				}
				// within an attempt the stage only goes forward
				static const int Rank[] = { 0, 0, 1, 2, 3, 4, 5 };
				if (S.Stage != Prev && S.Stage != EStage::Failed && S.Stage != EStage::Old && S.Stage != EStage::Build
					&& Rank[(int)S.Stage] < Rank[(int)Prev] && Prev != EStage::Failed && Prev != EStage::Old)
				{
					bOrder = false;
				}
				Prev = S.Stage;
			}
			if (FirstLay >= 0.f)
			{
				Lays[K.Id].push_back(FirstLay);
				bWindow &= FirstLay >= K.LayFrom - 1 && FirstLay <= K.LayLast + 1;
			}
			const FNestState A = N.StateOn(Nest.Id, N.Year, 140.3f), B = N.StateOn(Nest.Id, N.Year + 1, 140.3f), C = N.StateOn(Nest.Id, N.Year, 140.3f);
			bSame &= A.Stage == C.Stage && A.Eggs == C.Eggs && A.Chicks == C.Chicks;
			(void)B;
			(void)bCycle;
		}
		Check(bOrder, "a nest's stages go forward: building, eggs, chicks, fledged");
		Check(bWindow, "first eggs are laid within each species' window");
		Check(bSame, "the state on a day is the same however it is asked (another year in between)");
		auto Median = [&](const char* Id)
		{
			std::vector<float>& V = Lays[Id];
			if (V.empty())
			{
				return -1.f;
			}
			std::sort(V.begin(), V.end());
			return V[V.size() / 2];
		};
		const float Raven = Median("Raven"), Heron = Median("GreyHeron"), Tit = Median("GreatTit"), Swift = Median("CommonSwift"), Blackbird = Median("Blackbird");
		std::printf("  median first egg: raven %.0f, heron %.0f, blackbird %.0f, great tit %.0f, swift %.0f\n", Raven, Heron, Blackbird, Tit, Swift);
		Check(Raven > 0 && Raven < 80 && Heron < 100 && Tit > 105 && Tit < 130 && Swift > 140, "ravens and herons lay in March, tits in late April, swifts at the end of May");
		// the rookery in April: nearly every nest has eggs; in late June it is empty again
		int RookEggs = 0, RookNests = 0, RookLateJune = 0;
		for (const FNest& Nest : N.Nests)
		{
			if (SpeciesTable()[Nest.Species].Id == "Rook")
			{
				RookNests++;
				RookEggs += N.StateOn(Nest.Id, N.Year, 105.f).Stage == EStage::Eggs ? 1 : 0;
				const EStage S = N.StateOn(Nest.Id, N.Year, 200.f).Stage;
				RookLateJune += S == EStage::Old || S == EStage::Failed || S == EStage::Fledged ? 1 : 0;
			}
		}
		Check(RookEggs >= RookNests * 0.5f && RookLateJune == RookNests, "the rookery sits in mid-April (" + std::to_string(RookEggs) + "/" + std::to_string(RookNests) + ") and is done by late July");
		// blackbirds: second broods; some nests fail, most raise young. Counted over five seasons of the same pairs: the
		// town holds only ~10 pairs, too few for one season's dice (each attempt fledges ~1 in 5) to show every outcome.
		// With daily nest survival 0.94 and up to six tries, about half the pair-seasons raise young: "most" sat on the
		// mean and flipped with any change to the world, so the check asks for a third.
		int Second = 0, Failed = 0, Fledged = 0, Total = 0, Pairs = 0;
		const int Year0 = N.Year;
		for (FNest& Nest : N.Nests)
		{
			if (SpeciesTable()[Nest.Species].Id != "Blackbird")
			{
				continue;
			}
			Pairs++;
			for (int Y = Year0; Y < Year0 + 5; Y++)
			{
				N.StateOn(Nest.Id, Y, 100.f);
				Total++;
				int Good = 0;
				for (const FAttempt& A : Nest.Plan)
				{
					Failed += A.Fail < 1e8f ? 1 : 0;
					Good += A.Fledglings > 0 ? 1 : 0;
				}
				Second += Good >= 2 ? 1 : 0;
				Fledged += Good > 0 ? 1 : 0;
			}
			N.StateOn(Nest.Id, Year0, 100.f);
		}
		Check(Total > 0 && Second > 0 && Failed > 0 && Fledged * 3 > Total, "blackbirds: losses, repeat clutches and second broods (" + std::to_string(Pairs) + " pairs over 5 seasons, "
			+ std::to_string(Failed) + " lost, " + std::to_string(Second) + " with two broods, " + std::to_string(Fledged) + " of " + std::to_string(Total) + " raised young)");
	}

	// a May morning near a blackbird's nest with eggs: she sits, he sings and brings nothing; then the chicks: both feed them
	{
		Map(W, 130.f, 6.f);
		FNesting& M = *W.Nesting();
		int Nest = -1;
		float Day = -1.f;
		for (const FNest& Ne : M.Nests)
		{
			if (SpeciesTable()[Ne.Species].Id == "Blackbird" && (Day = DayIn(M, Ne.Id, EStage::Eggs, 100.f)) > 0.f)
			{
				Nest = Ne.Id;
				break;
			}
		}
		Check(Nest >= 0, "a blackbird's nest with eggs this spring");
		if (Nest >= 0)
		{
			W.DayOfYear = Day;
			W.Hour = 9.f;
			LookAt(W, M.Nests[Nest].Pos);
			W.Resettle();
			const int She = M.Nests[Nest].Female;
			Trace = { M.Nests[Nest].Male };
			float OnNest = 0.f, Total = 0.f;
			int Songs = 0;
			Run(W, 1800.f, 0.1f, [&]()
			{
				Total += 0.1f;
				OnNest += (W.Birds[She].Pos - M.Nests[Nest].Pos).Len() < 0.5f ? 0.1f : 0.f;
				for (const FNestEvent& E : M.Events)
				{
					Songs += E.Kind == ENestEvent::Song && E.Nest == Nest ? 1 : 0;
				}
			});
			std::printf("  blackbird eggs, day %.0f: she on the nest %.0f%% of 30 min, %d songs; %s\n", Day, 100.f * OnNest / Total, Songs, M.Report(W).c_str());
			Check(OnNest > 0.5f * Total, "she sits on the eggs most of the time");
			Check(Songs > 0, "he sings near the nest in the morning");
			// at night she sleeps on it
			W.Hour = 23.f;
			W.Resettle();
			Run(W, 300.f, 0.25f);
			Check((W.Birds[She].Pos - M.Nests[Nest].Pos).Len() < 0.5f && W.Birds[She].Act == EAct::Sleep, "she sleeps on the nest at night");

			const float ChickDay = DayIn(M, Nest, EStage::Chicks, Day, 0.7f);
			if (ChickDay > 0.f)
			{
				W.DayOfYear = ChickDay;
				W.Hour = 10.f;
				W.Resettle();
				int Feeds = 0;
				Trace = { M.Nests[Nest].Male, M.Nests[Nest].Female };
				Run(W, 1800.f, 0.1f, [&]()
				{
					for (const FNestEvent& E : M.Events)
					{
						if (E.Kind == ENestEvent::Feed && E.Nest == Nest)
						{
							Feeds++;
						}
					}
				});
				std::printf("  blackbird chicks, day %.0f: %d feeds in 30 min, duty starts:", ChickDay, Feeds);
				for (int D = 1; D < (int)EDuty::Num; D++)
				{
					std::printf(" %s %d", DutyName((EDuty)D), M.DutyStarts[D]);
				}
				std::printf("\n");
				Check(Feeds >= 6, "the parents bring food to the chicks every few minutes");
			}
		}
	}

	// building: a rook carries sticks (real stick items) to its nest in the rookery
	{
		Map(W, 80.f, 8.f);
		FNesting& M = *W.Nesting();
		int Nest = -1;
		float Day = -1.f;
		for (const FNest& Ne : M.Nests)
		{
			if (SpeciesTable()[Ne.Species].Id == "Rook" && (Day = DayIn(M, Ne.Id, EStage::Build, 60.f)) > 0.f)
			{
				Nest = Ne.Id;
				break;
			}
		}
		if (Nest >= 0)
		{
			W.DayOfYear = Day;
			W.Hour = 9.f;
			LookAt(W, M.Nests[Nest].Pos);
			W.Resettle();
			int Material = 0, Sticks = 0;
			Run(W, 1800.f, 0.1f, [&]()
			{
				for (const FNestEvent& E : M.Events)
				{
					Material += E.Kind == ENestEvent::Material && SpeciesTable()[M.Nests[E.Nest].Species].Id == "Rook" ? 1 : 0;
				}
				if (W.Corvids())
				{
					for (const FCorvidEvent& E : W.Corvids()->Events)
					{
						Sticks += E.Kind == ECorvidEvent::Pick && W.Corvids()->Items[E.Item].Kind == EItem::Stick ? 1 : 0;
					}
				}
			});
			std::printf("  rookery building, day %.0f: %d loads woven in, %d sticks picked up\n", Day, Material, Sticks);
			Check(Material >= 10 && Sticks >= 10, "rooks fly sticks to the rookery while building");
		}
		else
		{
			Check(false, "a rook's nest being built in March");
		}
	}

	// the player at a gull colony with young: dives at the head; at a thrush's nest with eggs: alarm; standing there long: given up
	{
		Map(W, 150.f, 11.f);
		FNesting& M = *W.Nesting();
		int Gull = -1;
		float Day = -1.f;
		for (const FNest& Ne : M.Nests)
		{
			const std::string& Id = SpeciesTable()[Ne.Species].Id;
			if ((Id == "HerringGull" || Id == "CommonGull" || Id == "BlackHeadedGull") && (Day = DayIn(M, Ne.Id, EStage::Fledged, 120.f, 0.2f)) > 0.f)
			{
				Gull = Ne.Id;
				break;
			}
		}
		if (Gull >= 0)
		{
			W.DayOfYear = Day;
			W.Hour = 11.f;
			LookAt(W, M.Nests[Gull].Pos);
			W.Resettle();
			Run(W, 600.f, 0.1f);       // (the parents come back to their young from wherever they were)
			W.Player.bPresent = true;
			W.Player.Feet = M.Nests[Gull].Pos + V3(4.f, 0.f, 0.f);
			W.Player.Eye = W.Player.Feet + V3(0.f, 0.f, 1.7f);
			int Dives = 0, Young = 0;
			for (const FNest& Ne : M.Nests)
			{
				Young += (int)Ne.Young.size();
			}
			Run(W, 60.f, 1.f / 30.f, [&]()
			{
				for (const FNestEvent& E : M.Events)
				{
					Dives += E.Kind == ENestEvent::Dive ? 1 : 0;
				}
			});
			std::printf("  gull colony (%s, %s), day %.0f: %d young about, %d dives at the player in a minute (dive duties %d, guard %d); %s\n",
				SpeciesTable()[M.Nests[Gull].Species].Id.c_str(), HabitatName(W.Sites[M.Nests[Gull].Site].Kind), Day, Young, Dives,
				M.DutyStarts[(int)EDuty::Dive], M.DutyStarts[(int)EDuty::Guard], M.Report(W).c_str());
			Check(Young > 0, "the gull chicks are out round the nests");
			Check(Dives >= 3, "the gulls dive at the player's head");
		}
		else
		{
			Check(false, "a gull nest with young in late May / June");
		}

		int Thrush = -1;
		for (const FNest& Ne : M.Nests)
		{
			if (SpeciesTable()[Ne.Species].Id == "SongThrush" && (Day = DayIn(M, Ne.Id, EStage::Eggs, 90.f, 0.3f)) > 0.f)
			{
				Thrush = Ne.Id;
				break;
			}
		}
		if (Thrush >= 0)
		{
			W.Player.bPresent = false;
			W.DayOfYear = Day;
			W.Hour = 10.f;
			LookAt(W, M.Nests[Thrush].Pos);
			W.Resettle();
			Run(W, 20.f, 0.1f);
			W.Player.bPresent = true;
			W.Player.Feet = M.Nests[Thrush].Pos + V3(1.5f, 0.f, 0.f);
			W.Player.Feet.Z = W.GroundZ(W.Player.Feet.X, W.Player.Feet.Y);
			W.Player.Eye = W.Player.Feet + V3(0.f, 0.f, 1.7f);
			int Alarms = 0;
			Run(W, 360.f, 0.1f, [&]()
			{
				for (const FNestEvent& E : M.Events)
				{
					Alarms += E.Kind == ENestEvent::Alarm && E.Nest == Thrush ? 1 : 0;
				}
			});
			const EStage After = M.Nests[Thrush].State.Stage;
			std::printf("  song thrush eggs, the player 1.5 m off for 6 min: %d alarm calls, the nest now %s\n", Alarms, StageName(After));
			Check(Alarms > 0, "the thrushes scold the player at their nest");
			Check(After == EStage::Failed, "standing at the nest that long makes them give it up");
		}
	}

	// cost: a May morning with and without the nests, in this one process
	{
		double Ms[2] = {};
		for (int Pass2 = 0; Pass2 < 2; Pass2++)
		{
			Map(W, 135.f, 7.f, Pass2 == 0);
			const auto A = std::chrono::steady_clock::now();
			Run(W, 600.f, 1.f / 30.f);
			Ms[Pass2] = std::chrono::duration<double>(std::chrono::steady_clock::now() - A).count() * 1000.0 / (600.0 * 30.0);
		}
		std::printf("  step with nests %.3f ms, without %.3f ms\n", Ms[0], Ms[1]);
		Check(Ms[0] < Ms[1] * 1.25 + 0.05, "the nests cost little (under a quarter more per step)");
		int Lost = 0;
		for (const FBird& B : W.Birds)
		{
			Lost += !B.bGone && !(std::isfinite(B.Pos.X) && std::isfinite(B.Pos.Z)) ? 1 : 0;
		}
		Check(Lost == 0, "no bird lost its way (NaN)");
	}

	std::printf("nests: %d passed, %d failed\n", Pass, Fail);
	if (Passed)
	{
		*Passed = Pass;
	}
	return Fail;
}
