// The tests of the species' signature behaviours (Private/Birds/BirdHabit.cpp, docs/bird_habits.md): the table (every
// species has its own), each way a habit moves a bird (in place, a pair, a run, a sally, a swoop to the water), the day's
// budget near the hero, walls and ledges, a fright in the middle, the weather and the season, the habits the bird life
// already does (a walnut cracked on the road, a cormorant's wings), no cost while every bird is far, and a soak on the map.
// Part of `tools/birdsim/run.sh test`; BIRD_ONLY=habits runs only these.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdCorvid.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdHabit.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace LyudBirds;

std::string BirdDataDir();      // birdsim.cpp: Content/BirdsData

namespace HabitTest
{
	int Pass = 0, Fail = 0;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What.c_str());
		(bOk ? Pass : Fail)++;
	}

	std::string F(const char* Fmt, ...)
	{
		char Buf[512];
		va_list A;
		va_start(A, Fmt);
		std::vsnprintf(Buf, sizeof(Buf), Fmt, A);
		va_end(A);
		return Buf;
	}

	/** A flat world (ground at 0, the sea far below), a July noon, no hawks, no nests, no corvid minds, the camera at the origin. */
	void Flat(FWorld& W, float Doy = 190.f, float Hour = 12.f)
	{
		W = FWorld();
		W.NoGridZ = 0.f;
		W.SeaZ = W.MeanSeaZ = -43.f;
		W.Opt.bPredators = false;
		W.Opt.bNests = false;
		W.Opt.bCorvids = false;
		W.DayOfYear = Doy;
		const float Half = 6.15f + 2.6f * std::sin((Doy - 80.f) / 365.f * 2.f * 3.14159265f);
		W.Sunrise = 12.3f - Half;
		W.Sunset = 12.3f + Half;
		W.Hour = Hour;
		W.TempC = 20.f;
		W.Camera = V3(0.f, 0.f, 1.7f);
	}

	/** Count birds feeding on the ground round P, on a site of this kind. */
	int Feeding(FWorld& W, const char* Species, const V3& P, int Count, EHabitat Kind, float Radius = 4.f)
	{
		const int Id = W.AddFlock(FindSpecies(Species), P, Count, Radius);
		if (Id >= 0)
		{
			W.Sites[W.Birds[Id].Site].Kind = Kind;
		}
		return Id;
	}

	/** One bird sitting Up metres over P on a site of this kind, its group resting (Plan 0) or feeding there (1). */
	int Perched(FWorld& W, const char* Species, const V3& P, float Up, EHabitat Kind, int Plan = 0)
	{
		const int Id = W.AddBird(FindSpecies(Species), V3(P.X, P.Y, P.Z + Up));
		if (Id < 0)
		{
			return -1;
		}
		FBird& B = W.Birds[Id];
		FSite& S = W.Sites[B.Site];
		S.Kind = Kind;
		S.Radius = 0.f;
		W.Groups[B.Group].Plan = Plan;
		B.Pos = B.Spot = S.A;
		B.Act = EAct::Perch;
		B.ActLen = 1e9f;
		return Id;
	}

	struct FSeen
	{
		int Starts = 0, Beats = 0, Ends = 0;
		int LastArg = -1;
		std::vector<double> StartTimes;
	};

	/** Steps the world; Each sees every step; habit events of Habit are counted into Seen. */
	void Step(FWorld& W, float Seconds, float Dt, EHabit Habit, FSeen* Seen, const std::function<void()>& Each = nullptr)
	{
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			W.Hour += Dt / 3600.f;
			W.Step(Dt);
			if (Seen && W.Habits())
			{
				for (const FHabitEvent& E : W.Habits()->Events)
				{
					if (E.Habit == Habit)
					{
						Seen->Starts += E.Kind == EHabitEvent::Start;
						Seen->Beats += E.Kind == EHabitEvent::Beat;
						Seen->Ends += E.Kind == EHabitEvent::End;
						if (E.Kind == EHabitEvent::Beat)
						{
							Seen->LastArg = E.Arg;
						}
						if (E.Kind == EHabitEvent::Start)
						{
							Seen->StartTimes.push_back(W.Time);
						}
					}
				}
			}
			if (Each)
			{
				Each();
			}
			W.Events.clear();
		}
	}

	void Table()
	{
		std::printf("the table:\n");
		const std::vector<FSpecies>& Sp = SpeciesTable();
		int Without = 0, Many = 0, Unknown = 0, Wrong = 0;
		std::string Missing;
		for (int S = 0; S < (int)Sp.size(); S++)
		{
			const int N = (int)HabitsOf(S).size();
			if (N == 0)
			{
				Without++;
				Missing += " " + Sp[S].Id;
			}
			Many += N > 2;
		}
		std::set<std::string> Names;
		int PerFamily[(int)EBirdFamily::Num] = {};
		for (int I = 1; I < NumHabits; I++)
		{
			const FHabitDef& D = HabitDef((EHabit)I);
			Wrong += (int)D.Id != I;
			Unknown += FindSpecies(D.Species) < 0;
			Names.insert(D.Name);
			PerFamily[(int)D.Family]++;
			Wrong += FindHabit(D.Name) != D.Id;
			Wrong += D.From == EHabitFrom::Dice && (D.Acts == 0 || D.PerDay < 1 || D.PerDay > 8 || D.Gap < 120.f || D.LenMin <= 0.f || D.LenMax < D.LenMin);
		}
		bool bFamilies = true;
		for (int N : PerFamily)
		{
			bFamilies &= N >= 5;
		}
		std::printf("  %d species, %d habits, families:", (int)Sp.size(), NumHabits - 1);
		for (int K = 0; K < (int)EBirdFamily::Num; K++)
		{
			std::printf(" %s %d", FamilyName((EBirdFamily)K), PerFamily[K]);
		}
		std::printf("\n");
		Check(Without == 0, "every species has a habit of its own" + (Missing.empty() ? std::string() : " (none:" + Missing + ")"));
		Check(Many == 0, "and none has more than two");
		Check(Unknown == 0 && Wrong == 0 && (int)Names.size() == NumHabits - 1, "every habit has its species, a name of its own and sane numbers");
		Check(bFamilies, "each of the six families has habits to find for its stamp");
	}

	void Still()
	{
		std::printf("in place:\n");
		FWorld W;
		Flat(W);
		const int Id = Feeding(W, "Blackbird", V3(8.f, 0.f, 0.f), 1, EHabitat::ForestFloor);
		W.Step(0.05f);
		FBird& B = W.Birds[Id];
		const V3 At = B.Pos;
		Check(W.Habits() && W.Habits()->Start(W, B, EHabit::BlackbirdLeafToss), "a blackbird feeding on the forest floor starts tossing leaves");
		const float Len = W.Habits()->Of(Id) ? W.Habits()->Of(Id)->Len : 0.f;
		FSeen Seen;
		float Moved = 0.f;
		bool bHeld = true;
		Step(W, Len - 0.2f, 0.02f, EHabit::BlackbirdLeafToss, &Seen, [&]()
		{
			Moved = std::max(Moved, (W.Birds[Id].Pos - At).Len());
			bHeld &= W.Birds[Id].Habit == (uint8_t)EHabit::BlackbirdLeafToss;
		});
		Check(bHeld && Moved < 0.01f, "it stays where it is the whole time");
		Step(W, 1.f, 0.02f, EHabit::BlackbirdLeafToss, &Seen);
		Check(Seen.Beats == 4 && Seen.Ends == 1, F("four tosses, then it ends (beats %d, ends %d)", Seen.Beats, Seen.Ends));
		const EAct After = W.Birds[Id].Act;
		Check(W.Birds[Id].Habit == 0 && (After == EAct::Peck || After == EAct::Forage || After == EAct::Perch), "and it goes on feeding");
		Check(!W.Habits()->Start(W, W.Birds[Id], EHabit::WagtailDash), "a habit of another species never starts on it");
	}

	void Budget()
	{
		std::printf("the day's budget near the hero:\n");
		FWorld W;
		Flat(W, 190.f, 8.f);
		Feeding(W, "Blackbird", V3(10.f, 0.f, 0.f), 12, EHabitat::ForestFloor, 6.f);
		FSeen Seen;
		int MostAtOnce = 0;
		Step(W, 3.f * 3600.f, 0.1f, EHabit::BlackbirdLeafToss, &Seen, [&]()
		{
			int N = 0;
			for (const FHabitRun& R : W.Habits()->Active)
			{
				N += !R.bMapped && !R.bSecond;
			}
			MostAtOnce = std::max(MostAtOnce, N);
		});
		const FHabitDef& D = HabitDef(EHabit::BlackbirdLeafToss);
		double MinGap = 1e9;
		for (size_t I = 1; I < Seen.StartTimes.size(); I++)
		{
			MinGap = std::min(MinGap, Seen.StartTimes[I] - Seen.StartTimes[I - 1]);
		}
		std::printf("  twelve blackbirds, three hours: %d leaf tosses, the closest two %.0f s apart, %d habits at once at most\n", Seen.Starts, MinGap, MostAtOnce);
		Check(Seen.Starts >= 3 && Seen.Starts <= D.PerDay, F("a patient hero sees it a few times, never more than %d a day", D.PerDay));
		Check(MinGap >= D.Gap - 1.f, "minutes apart");
		Check(MostAtOnce <= W.Habits()->MaxActive, "few habits run at once");
		// the next morning the budget is fresh
		const int Before = Seen.Starts;
		W.DayOfYear += 1.f;
		W.Hour = 8.f;
		Step(W, 1800.f, 0.1f, EHabit::BlackbirdLeafToss, &Seen);
		Check(Seen.Starts > Before || Before < D.PerDay, "the next day it is seen again");
	}

	void Far()
	{
		std::printf("far from the hero:\n");
		FWorld W;
		Flat(W, 190.f, 8.f);
		Feeding(W, "Blackbird", V3(10.f, 0.f, 0.f), 12, EHabitat::ForestFloor, 6.f);
		Feeding(W, "WhiteWagtail", V3(20.f, 5.f, 0.f), 4, EHabitat::Beach, 6.f);
		W.Camera = V3(900.f, 0.f, 1.7f);
		Step(W, 900.f, 0.1f, EHabit::None, nullptr);
		int Total = 0;
		for (int C : W.Habits()->Count)
		{
			Total += C;
		}
		Check(W.Habits()->Considered == 0 && Total == 0 && W.Habits()->Active.empty(), F("no bird is looked at, none starts a habit (looked at %ld, started %d)", W.Habits()->Considered, Total));
		W.Camera = V3(0.f, 0.f, 1.7f);
		Step(W, 1800.f, 0.1f, EHabit::None, nullptr);
		Total = 0;
		for (int C : W.Habits()->Count)
		{
			Total += C;
		}
		Check(Total > 0, F("with the hero near they do (%d)", Total));
	}

	void Walls()
	{
		std::printf("walls and ledges:\n");
		// a wall 0.8 m east of the bird (anything crossing X = 10.8 is blocked)
		const auto WallAt = [](float X)
		{
			return [X](const V3& A, const V3& B) { return (A.X - X) * (B.X - X) > 0.f; };
		};
		{
			FWorld W;
			Flat(W);
			W.Sight = WallAt(10.8f);
			const int Id = Feeding(W, "WhiteWagtail", V3(10.f, 0.f, 0.f), 1, EHabitat::Beach, 5.f);
			W.Step(0.05f);
			float MaxX = -1e9f;
			int Runs = 0;
			for (int K = 0; K < 60; K++)
			{
				FBird& B = W.Birds[Id];
				B.Pos = B.Spot = V3(10.f, (K % 7) * 0.3f, 0.f);
				B.Yaw = K * 0.7f;           // every way, the wall's too
				B.Act = EAct::Peck;
				B.Alert = 0.f;
				if (W.Habits()->Start(W, B, EHabit::WagtailDash))
				{
					Runs++;
					Step(W, 8.f, 0.02f, EHabit::None, nullptr, [&]()
					{
						if (W.Birds[Id].Habit != 0)
						{
							MaxX = std::max(MaxX, W.Birds[Id].Pos.X);
						}
					});
				}
				W.Habits()->Resettle(W);
			}
			std::printf("  %d dashes beside a wall, the farthest east %.2f m (the wall at 10.80)\n", Runs, MaxX);
			Check(Runs >= 20 && MaxX < 10.8f, "a wagtail's dashes never go through the wall beside it");
		}
		{
			// boxed in: walls on every side at half a metre
			FWorld W;
			Flat(W);
			W.Sight = [](const V3& A, const V3& B) { return std::fabs(B.X - 10.f) < 0.5f && std::fabs(B.Y) < 0.5f && std::fabs(A.X - 10.f) < 0.5f && std::fabs(A.Y) < 0.5f; };
			const int Id = Feeding(W, "WhiteWagtail", V3(10.f, 0.f, 0.f), 1, EHabitat::Beach, 5.f);
			const int Jay = Feeding(W, "Jay", V3(10.f, 0.2f, 0.f), 1, EHabitat::ForestFloor, 5.f);
			W.Step(0.05f);
			int Started = 0;
			for (int K = 0; K < 40; K++)
			{
				W.Birds[Id].Yaw = K * 0.5f;
				W.Birds[Jay].Yaw = K * 0.5f;
				Started += W.Habits()->Start(W, W.Birds[Id], EHabit::WagtailDash);
				Started += W.Habits()->Start(W, W.Birds[Jay], EHabit::JayAnting);
				W.Habits()->Resettle(W);
			}
			Check(Started == 0, "boxed in between walls it does not start: no dash, no wings spread on an anthill");
		}
		{
			// a bird that needs room and faces a wall turns its back to it
			FWorld W;
			Flat(W);
			W.Sight = WallAt(10.4f);
			const int Jay = Feeding(W, "Jay", V3(10.f, 0.f, 0.f), 1, EHabitat::ForestFloor, 5.f);
			W.Step(0.05f);
			W.Birds[Jay].Pos = W.Birds[Jay].Spot = V3(10.f, 0.f, 0.f);
			W.Birds[Jay].Yaw = 0.f;       // facing the wall
			const bool bOk = W.Habits()->Start(W, W.Birds[Jay], EHabit::JayAnting);
			Check(bOk && std::cos(W.Birds[Jay].Yaw) < -0.9f, "facing a wall, it turns round before it spreads its wings");
		}
		{
			// a ledge: the ground drops a metre east of X = 11
			FWorld W;
			Flat(W);
			W.Probe = [](float X, float Y, float& Z) { Z = X > 11.f ? -1.f : 0.f; return true; };
			const int Id = Feeding(W, "WhiteWagtail", V3(10.f, 0.f, 0.f), 1, EHabitat::Beach, 5.f);
			W.Step(0.05f);
			float MaxX = -1e9f, MinZ = 1e9f;
			int Runs = 0;
			for (int K = 0; K < 40; K++)
			{
				FBird& B = W.Birds[Id];
				B.Pos = B.Spot = V3(10.3f, 0.f, 0.f);
				B.Yaw = K * 0.9f;
				B.Act = EAct::Peck;
				B.Alert = 0.f;
				if (W.Habits()->Start(W, B, EHabit::WagtailDash))
				{
					Runs++;
					Step(W, 8.f, 0.02f, EHabit::None, nullptr, [&]()
					{
						if (W.Birds[Id].Habit != 0)
						{
							MaxX = std::max(MaxX, W.Birds[Id].Pos.X);
							MinZ = std::min(MinZ, W.Birds[Id].Pos.Z);
						}
					});
				}
				W.Habits()->Resettle(W);
			}
			Check(Runs >= 15 && MaxX <= 11.01f && MinZ > -0.05f, F("it never runs off a ledge (east %.2f, lowest %.2f)", MaxX, MinZ));
		}
	}

	void Runs()
	{
		std::printf("a run:\n");
		FWorld W;
		Flat(W);
		const int Id = Feeding(W, "WhiteWagtail", V3(10.f, 0.f, 0.f), 1, EHabitat::Beach, 5.f);
		W.Step(0.05f);
		const bool bOk = W.Habits()->Start(W, W.Birds[Id], EHabit::WagtailDash);
		FSeen Seen;
		float Fastest = 0.f, Far = 0.f, Path = 0.f;
		V3 Last = W.Birds[Id].Pos;
		Step(W, 12.f, 0.02f, EHabit::WagtailDash, &Seen, [&]()
		{
			const FBird& B = W.Birds[Id];
			if (B.Habit != 0)
			{
				Fastest = std::max(Fastest, B.Vel.Len());
				Path += (B.Pos - Last).Len();
			}
			Last = B.Pos;
			Far = std::max(Far, (B.Pos - V3(10.f, 0.f, 0.f)).Len2D());
		});
		std::printf("  wagtail: %d runs, %.1f m in all, up to %.1f m/s\n", Seen.Beats, Path, Fastest);
		Check(bOk && Seen.Beats >= 2 && Seen.Ends == 1, "a wagtail on the wet sand dashes, stops, dashes again");
		Check(Fastest > 2.2f && Fastest < 3.2f, "much faster than it walks");
		Check(Far < 5.f + 1.5f + 3.6f && std::fabs(W.Birds[Id].Pos.Z) < 0.01f, "it keeps to its patch of sand, on the ground");
		const EAct A = W.Birds[Id].Act;
		Check(W.Birds[Id].Habit == 0 && (A == EAct::Peck || A == EAct::Forage || A == EAct::Perch), "and then feeds on from where it stopped");
	}

	void Sallies()
	{
		std::printf("a sally from the perch:\n");
		{
			FWorld W;
			Flat(W);
			const int Id = Perched(W, "Chaffinch", V3(8.f, 0.f, 0.f), 4.f, EHabitat::Broadleaf, 1);
			W.Step(0.05f);
			const V3 Twig = W.Birds[Id].Pos;
			const bool bOk = W.Habits()->Start(W, W.Birds[Id], EHabit::ChaffinchSally);
			FSeen Seen;
			float Far = 0.f;
			bool bFlew = false;
			Step(W, 8.f, 0.02f, EHabit::ChaffinchSally, &Seen, [&]()
			{
				Far = std::max(Far, (W.Birds[Id].Pos - Twig).Len());
				bFlew |= W.Birds[Id].Habit != 0 && IsAirborne(W.Birds[Id].Act);
			});
			Check(bOk && bFlew && Far > 1.5f && Far < 4.8f, F("a chaffinch flies out after an insect (%.1f m)", Far));
			Check(Seen.Beats == 1 && Seen.Ends == 1 && (W.Birds[Id].Pos - Twig).Len() < 0.01f && !IsAirborne(W.Birds[Id].Act), "snaps it up and is back on its own twig");
		}
		{
			FWorld W;
			Flat(W);
			const int Id = Perched(W, "Robin", V3(8.f, 0.f, 0.f), 2.f, EHabitat::Garden, 1);
			W.Step(0.05f);
			const V3 Twig = W.Birds[Id].Pos;
			const bool bOk = W.Habits()->Start(W, W.Birds[Id], EHabit::RobinPounce);
			float Lowest = 1e9f, OnGround = 0.f;
			Step(W, 10.f, 0.02f, EHabit::None, nullptr, [&]()
			{
				Lowest = std::min(Lowest, W.Birds[Id].Pos.Z);
				OnGround += W.Birds[Id].Pos.Z < 0.05f && W.Birds[Id].Act == EAct::Peck ? 0.02f : 0.f;
			});
			Check(bOk && Lowest > -0.001f && Lowest < 0.05f && OnGround > 0.8f, F("a robin drops from its perch to the ground and pecks (%.1f s down there)", OnGround));
			Check((W.Birds[Id].Pos - Twig).Len() < 0.01f && W.Birds[Id].Habit == 0, "then is back on its perch");
		}
		{
			// a fright in the middle of it
			FWorld W;
			Flat(W);
			const int Id = Perched(W, "Chaffinch", V3(8.f, 0.f, 0.f), 4.f, EHabitat::Broadleaf, 1);
			W.Step(0.05f);
			W.Habits()->Start(W, W.Birds[Id], EHabit::ChaffinchSally);
			Step(W, 0.3f, 0.02f, EHabit::None, nullptr);
			const bool bMid = W.Birds[Id].Habit != 0;
			W.Scare(W.Birds[Id].Pos, 10.f, 3.f);
			bool bFled = false;
			Step(W, 2.f, 0.02f, EHabit::None, nullptr, [&]() { bFled |= W.Birds[Id].Act == EAct::Flee; });
			Check(bMid && bFled && W.Birds[Id].Habit == 0 && !W.Birds[Id].bHabitMoves, "a bang in the middle of it: the habit is over, the bird flies off like any other");
			bool bDown = false;
			Step(W, 400.f, 0.05f, EHabit::None, nullptr, [&]() { bDown |= !IsAirborne(W.Birds[Id].Act); });
			Check(bDown, "and comes down again somewhere");
		}
	}

	void Pairs()
	{
		std::printf("a pair:\n");
		FWorld W;
		Flat(W);
		const int A = Feeding(W, "FeralPigeon", V3(8.f, 0.f, 0.f), 1, EHabitat::Street);
		W.Step(0.05f);
		Check(!W.Habits()->Start(W, W.Birds[A], EHabit::PigeonBowCoo), "a pigeon alone has nobody to bow to");
		const int B = Feeding(W, "FeralPigeon", V3(9.8f, 0.6f, 0.f), 1, EHabitat::Street);
		W.Birds[A].Pos = W.Birds[A].Spot = V3(8.f, 0.f, 0.f);
		W.Birds[B].Pos = W.Birds[B].Spot = V3(9.8f, 0.6f, 0.f);
		W.Step(0.02f);
		const bool bOk = W.Habits()->Start(W, W.Birds[A], EHabit::PigeonBowCoo);
		const bool bBoth = W.Birds[A].Habit != 0 && W.Birds[B].Habit == W.Birds[A].Habit;
		FSeen Seen;
		float Nearest = 1e9f, Facing = 1.f;
		Step(W, 25.f, 0.02f, EHabit::PigeonBowCoo, &Seen, [&]()
		{
			if (W.Birds[A].Habit != 0 && !W.Birds[A].bHabitMoves)
			{
				Nearest = std::min(Nearest, (W.Birds[A].Pos - W.Birds[B].Pos).Len());
				Facing = std::cos(W.Birds[A].Yaw - W.Birds[B].Yaw);
			}
		});
		Check(bOk && bBoth, "with another one two metres off, both are in it");
		Check(Nearest < 0.5f && Nearest > 0.15f && Facing < -0.9f, F("it walks up to her and they face each other (%.2f m)", Nearest));
		Check(Seen.Beats == 4 && W.Birds[A].Habit == 0 && W.Birds[B].Habit == 0, F("four bows, and both are free again (beats %d)", Seen.Beats));
		// a fright takes both out of it
		W.Birds[A].HabitCool = W.Birds[B].HabitCool = 0.f;
		W.Birds[A].Pos = W.Birds[A].Spot = V3(8.f, 0.f, 0.f);
		W.Birds[B].Pos = W.Birds[B].Spot = V3(9.5f, 0.f, 0.f);
		W.Birds[A].Alert = W.Birds[B].Alert = 0.f;
		if (IsAirborne(W.Birds[A].Act) || IsAirborne(W.Birds[B].Act))
		{
			W.Birds[A].Act = W.Birds[B].Act = EAct::Peck;
		}
		W.Step(0.02f);
		const bool bAgain = W.Habits()->Start(W, W.Birds[A], EHabit::PigeonBowCoo);
		Step(W, 1.f, 0.02f, EHabit::None, nullptr);
		W.Scare(W.Birds[B].Pos, 6.f, 3.f);
		Step(W, 1.5f, 0.02f, EHabit::None, nullptr);
		Check(bAgain && W.Birds[A].Habit == 0 && W.Birds[B].Habit == 0 && W.Habits()->Active.empty(), "when one of them is frightened off, the other is not left bowing to nobody");
	}

	void Swoops()
	{
		std::printf("on the wing:\n");
		{
			FWorld W;
			Flat(W);
			const int Id = Perched(W, "BarnSwallow", V3(0.f, 0.f, 0.f), 12.f, EHabitat::Meadow, 1);
			FBird& B = W.Birds[Id];
			B.Act = EAct::Hawk;
			B.ActLen = 1e6f;
			B.Vel = V3(8.f, 0.f, 0.f);
			W.Step(0.02f);
			Check(!W.Habits()->Start(W, W.Birds[Id], EHabit::SwallowDrink), "with no water near a swallow has nowhere to drink");
			W.Habits()->Waters.push_back(V3(40.f, 5.f, 0.3f));       // a pool of the brook, 0.3 m over the ground round it
			W.Birds[Id].Pos = V3(0.f, 0.f, 12.f);
			const V3 From = W.Birds[Id].Pos;
			const bool bOk = W.Habits()->Start(W, W.Birds[Id], EHabit::SwallowDrink);
			FSeen Seen;
			float Lowest = 1e9f, Closest = 1e9f, Fastest = 0.f;
			V3 End;
			Step(W, 20.f, 0.02f, EHabit::SwallowDrink, &Seen, [&]()
			{
				const FBird& S = W.Birds[Id];
				if (S.Habit != 0)
				{
					Lowest = std::min(Lowest, S.Pos.Z);
					Closest = std::min(Closest, (S.Pos - V3(40.f, 5.f, 0.36f)).Len());
					Fastest = std::max(Fastest, S.Vel.Len());
					End = S.Pos;
				}
			});
			std::printf("  swallow: lowest %.2f m (the water at 0.30), %.2f m from the spot, up to %.1f m/s, ends %.1f m up\n", Lowest, Closest, Fastest, End.Z);
			Check(bOk && Closest < 0.2f && Lowest > 0.3f, "a swallow dips to the pool and touches the water, never under it");
			Check(Seen.Beats == 1 && Seen.Ends == 1 && std::fabs(End.Z - From.Z) < 1.5f && W.Birds[Id].Act == EAct::Hawk, "one sip, then it is up at its height again and hunting on");
			Check(Fastest < SpeciesTable()[W.Birds[Id].Species].Sprint * 2.5f, "at a swallow's speed");
		}
		{
			// a snatch: a herring gull and a sandwich on a bench
			FWorld W;
			Flat(W);
			const int Id = Perched(W, "HerringGull", V3(0.f, 0.f, 0.f), 5.f, EHabitat::Roof);
			W.Step(0.02f);
			const V3 Roof = W.Birds[Id].Pos;
			Check(!W.Habits()->Start(W, W.Birds[Id], EHabit::GullSnatch), "a gull with nothing to snatch sits on");
			W.Habits()->Morsels.push_back({ 7, V3(18.f, 4.f, 0.5f), 0 });
			const bool bOk = W.Habits()->Start(W, W.Birds[Id], EHabit::GullSnatch);
			FSeen Seen;
			float Closest = 1e9f;
			Step(W, 15.f, 0.02f, EHabit::GullSnatch, &Seen, [&]() { Closest = std::min(Closest, (W.Birds[Id].Pos - V3(18.f, 4.f, 0.5f)).Len()); });
			Check(bOk && Closest < 0.2f && Seen.Beats == 1 && Seen.LastArg == 7, "a herring gull comes down on the sandwich on the bench (the game hears which)");
			Check(W.Habits()->Morsels.empty() && (W.Birds[Id].Pos - Roof).Len() < 0.01f, "the sandwich is gone, the gull back on its roof");
		}
		{
			// held on the wind; the sky dance
			FWorld W;
			Flat(W, 60.f, 12.f);
			const int Buzz = Perched(W, "CommonBuzzard", V3(0.f, 0.f, 0.f), 60.f, EHabitat::Meadow);
			const int Eagle = Perched(W, "GoldenEagle", V3(300.f, 0.f, 0.f), 150.f, EHabitat::Meadow);
			for (int Id : { Buzz, Eagle })
			{
				FBird& B = W.Birds[Id];
				B.Act = EAct::Soar;
				B.ActLen = 1e6f;
				B.FlyFrom = B.Pos;
			}
			W.Wind = V3(-8.f, 0.f, 0.f);
			W.Camera = V3(100.f, 0.f, 1.7f);
			W.Step(0.02f);
			const V3 At = W.Birds[Buzz].Pos;
			const bool bHang = W.Habits()->Start(W, W.Birds[Buzz], EHabit::BuzzardHang, false);
			float Drift = 0.f;
			bool bInto = true;
			Step(W, 5.f, 0.02f, EHabit::None, nullptr, [&]()
			{
				Drift = std::max(Drift, (W.Birds[Buzz].Pos - At).Len());
				bInto &= std::cos(W.Birds[Buzz].Yaw) > 0.9f;
			});
			Check(bHang && Drift < 1.f && bInto, F("a buzzard hangs on the wind over one spot, head into it (%.2f m of drift)", Drift));
			W.Wind = V3();
			Check(!W.Habits()->Fits(W, W.Birds[Buzz], HabitDef(EHabit::BuzzardHang)) || W.Birds[Buzz].Habit != 0, "(not in a calm)");
			const float Top = W.Birds[Eagle].Pos.Z;
			const bool bDance = W.Habits()->Start(W, W.Birds[Eagle], EHabit::GoldenEagleDance);
			FSeen Seen;
			float Low = 1e9f, High = -1e9f;
			Step(W, 25.f, 0.05f, EHabit::GoldenEagleDance, &Seen, [&]()
			{
				if (W.Birds[Eagle].Habit != 0)
				{
					Low = std::min(Low, W.Birds[Eagle].Pos.Z);
					High = std::max(High, W.Birds[Eagle].Pos.Z);
				}
			});
			Check(bDance && Seen.Beats == 3 && Top - Low > 8.f && Low > 100.f && High < Top + 0.5f, F("a golden eagle's sky dance: three dives of %.0f m and up again, high over the ground", Top - Low));
		}
	}

	void Weather()
	{
		std::printf("the weather, the hour, the season:\n");
		FWorld W;
		Flat(W);
		const int Sparrow = Feeding(W, "HouseSparrow", V3(6.f, 0.f, 0.f), 1, EHabitat::Street);
		const int Tree = Feeding(W, "TreeSparrow", V3(6.f, 5.f, 0.f), 1, EHabitat::Street);
		const int Waxwing = Perched(W, "Waxwing", V3(6.f, -5.f, 0.f), 3.f, EHabitat::Garden, 1);
		const int Crow = Perched(W, "HoodedCrow", V3(-6.f, 0.f, 0.f), 6.f, EHabitat::Roof);
		W.Step(0.05f);
		FHabits& H = *W.Habits();
		const auto Fits = [&](int Id, EHabit Hb) { return H.Fits(W, W.Birds[Id], HabitDef(Hb)); };
		Check(Fits(Sparrow, EHabit::SparrowDustBath) && !Fits(Tree, EHabit::TreeSparrowPuddle), "a dry July noon: sparrows bathe in the dust, there are no puddles");
		// an hour of rain, then it stops
		W.Rain = 3.f;
		Step(W, 3600.f, 1.f, EHabit::None, nullptr);
		Check(!Fits(Sparrow, EHabit::SparrowDustBath), "in the rain nobody bathes in dust");
		W.Rain = 0.f;
		Step(W, 600.f, 1.f, EHabit::None, nullptr);
		for (int Id : { Sparrow, Tree })
		{
			W.Birds[Id].Alert = 0.f;
			W.Birds[Id].HabitCool = 0.f;
		}
		Check(H.Wet > 0.5f && Fits(Tree, EHabit::TreeSparrowPuddle) && !Fits(Sparrow, EHabit::SparrowDustBath), F("after the rain the tree sparrows bathe in the puddles (wet %.2f)", H.Wet));
		Step(W, 4.f * 3600.f, 2.f, EHabit::None, nullptr);
		Check(H.Wet < 0.05f, "a few hours later the ground is dry again");
		Check(!Fits(Waxwing, EHabit::WaxwingGulp), "no waxwings gulping rowan berries in July");
		Check(!Fits(Crow, EHabit::CrowRoofSlide), "no crow sliding down a roof without frost");
		W.DayOfYear = 350.f;
		W.Hour = 12.f;
		W.TempC = -6.f;
		W.Sunrise = 8.8f;
		W.Sunset = 15.8f;
		for (int Id : { Waxwing, Crow })
		{
			W.Birds[Id].Act = EAct::Perch;
			W.Birds[Id].Alert = 0.f;
			W.Birds[Id].Habit = 0;
		}
		Check(Fits(Waxwing, EHabit::WaxwingGulp) && Fits(Crow, EHabit::CrowRoofSlide), "in a December frost both happen");
		W.Hour = 22.f;
		Check(!Fits(Waxwing, EHabit::WaxwingGulp), "nothing at night");
	}

	void Mapped()
	{
		std::printf("what the bird life already does:\n");
		{
			// the corvids' walnut (as in corvids_test.cpp)
			FWorld W;
			Flat(W, 297.f, 11.f);
			W.Opt.bCorvids = true;
			FSite Street;
			Street.Kind = EHabitat::Street;
			Street.A = V3(40.f, -10.f, 0.f);
			Street.B = V3(40.f, 10.f, 0.f);
			Street.Radius = 4.f;
			W.AddSite(Street);
			const int Crow = W.AddFlock(FindSpecies("HoodedCrow"), V3(25.f, 0.f, 0.f), 1, 2.f);
			W.Step(0.01f);
			W.Corvids()->Minds[Crow].Hoard = 0.f;
			W.Birds[Crow].Hunger = 0.8f;
			W.Corvids()->Spawn(W, EItem::Walnut, V3(26.f, 0.5f, 0.f));
			FSeen Seen;
			bool bCracking = false;
			Step(W, 400.f, 1.f / 30.f, EHabit::CrowNutDrop, &Seen, [&]()
			{
				const FCorvid* M = W.Corvids()->Mind(Crow);
				const bool bTask = M && M->Task == ETask::Crack;
				const bool bHabit = W.Birds[Crow].Habit == (uint8_t)EHabit::CrowNutDrop;
				bCracking |= bTask && bHabit;
			});
			std::printf("  walnut: announced %d time(s), %d beat(s)\n", Seen.Starts, Seen.Beats);
			Check(Seen.Starts >= 1 && bCracking && Seen.Ends == Seen.Starts, "a hooded crow dropping a walnut on the road is its habit, for as long as it is at it");
			Check(Seen.Beats >= 1, "each drop is a beat");
			Check(W.Birds[Crow].Habit == 0 && !W.Birds[Crow].bHabitMoves, "and the crow's own mind flew it all the while");
		}
		{
			FWorld W;
			Flat(W);
			const int Corm = Perched(W, "Cormorant", V3(8.f, 0.f, 0.f), 0.f, EHabitat::Beach);
			W.Birds[Corm].Act = EAct::Preen;
			W.Birds[Corm].ActLen = 90.f;
			FSeen Seen;
			Step(W, 5.f, 0.05f, EHabit::CormorantWings, &Seen);
			const bool bOn = W.Birds[Corm].Habit == (uint8_t)EHabit::CormorantWings;
			Step(W, 120.f, 0.05f, EHabit::CormorantWings, &Seen);
			Check(Seen.Starts == 1 && bOn && Seen.Ends == 1, "a cormorant on the beach with its wings out to dry: announced once, while it lasts");
		}
		{
			FWorld W;
			Flat(W);
			const int Gull = Perched(W, "HerringGull", V3(8.f, 0.f, 0.f), 0.f, EHabitat::Beach);
			W.Step(0.02f);
			W.Events.push_back({ EEvent::Call, Gull, W.Birds[Gull].Pos, EVoice::LongCall });
			FSeen Seen;
			Step(W, 0.1f, 0.02f, EHabit::GullLongCall, &Seen);
			const bool bOn = W.Birds[Gull].Habit == (uint8_t)EHabit::GullLongCall;
			Step(W, 6.f, 0.02f, EHabit::GullLongCall, &Seen);
			Check(Seen.Starts == 1 && bOn && Seen.Ends == 1, "a herring gull's long call comes with its posture");
		}
	}

	std::string Dir()
	{
		return BirdDataDir();
	}

	void Soak()
	{
		std::printf("on the map (the whole population, the hero standing in three places):\n");
		FWorld W;
		if (!W.LoadHabitats(Dir()))
		{
			std::printf("  (no map data: skipped)\n");
			return;
		}
		W.Opt.bPredators = true;
		W.DayOfYear = 190.f;
		W.Sunrise = 4.4f;
		W.Sunset = 21.2f;
		W.Hour = 7.f;
		W.TempC = 19.f;
		W.Populate();
		// three places with birds about: a street, the beach, a garden
		std::vector<V3> Places;
		for (EHabitat K : { EHabitat::Street, EHabitat::Beach, EHabitat::Garden })
		{
			int Best = -1, BestN = -1;
			for (int I = 0; I < (int)W.Sites.size(); I++)
			{
				if (W.Sites[I].Kind != K)
				{
					continue;
				}
				int N = 0;
				for (const FBird& B : W.Birds)
				{
					N += !B.bGone && (B.Pos - W.Sites[I].Center()).Len2D() < 60.f;
				}
				if (N > BestN)
				{
					BestN = N;
					Best = I;
				}
			}
			if (Best >= 0)
			{
				Places.push_back(W.Sites[Best].Center() + V3(6.f, 0.f, 1.7f));
			}
		}
		float Under = 0.f;
		int MostAtOnce = 0, Stuck = 0;
		const float Dt = 0.1f;
		for (const V3& P : Places)
		{
			W.Camera = P;
			W.Player.bPresent = true;
			W.Player.Feet = P - V3(0.f, 0.f, 1.7f);
			W.Player.Eye = P;
			for (float T = 0.f; T < 2400.f; T += Dt)
			{
				W.Hour += Dt / 3600.f;
				W.Step(Dt);
				int N = 0;
				for (const FHabitRun& R : W.Habits()->Active)
				{
					N += !R.bMapped && !R.bSecond;
					const FBird& B = W.Birds[R.Bird];
					if (R.bMoves)
					{
						// (only the ones a habit flies: a bird sitting on the cliff's face is under the coarse grid's ground there)
						Under = std::max(Under, std::max(W.GroundZ(B.Pos.X, B.Pos.Y), W.SeaZ) - B.Pos.Z);
					}
					Stuck += !R.bMapped && R.T > 120.f;
				}
				MostAtOnce = std::max(MostAtOnce, N);
				W.Events.clear();
			}
		}
		int Kinds = 0, Total = 0;
		for (int I = 1; I < NumHabits; I++)
		{
			Kinds += W.Habits()->Count[I] > 0;
			Total += W.Habits()->Count[I];
		}
		std::printf("  %s\n", W.Habits()->Report(W).c_str());
		Check(Kinds >= 6, F("two hours of a July morning: %d habits of %d kinds seen", Total, Kinds));
		Check(Under < 0.35f, F("no bird in a habit is ever under the ground (deepest %.2f m)", Under));
		Check(MostAtOnce <= W.Habits()->MaxActive && Stuck == 0, "few at once, and none gets stuck");
	}
}

int HabitTests(int* Passed)
{
	using namespace HabitTest;
	Pass = Fail = 0;
	std::printf("species' habits:\n");
	Table();
	Still();
	Budget();
	Far();
	Walls();
	Runs();
	Sallies();
	Pairs();
	Swoops();
	Weather();
	Mapped();
	Soak();
	if (Passed)
	{
		*Passed += Pass;
	}
	else
	{
		std::printf("habits: %d passed, %d failed\n", Pass, Fail);
	}
	return Fail;
}
