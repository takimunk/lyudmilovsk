// The animal life outside the engine: a day on the real map (CSV for render_animals.py) and behaviour tests.
//   tools/animalsim/run.sh day [start_hour hours]
//   tools/animalsim/run.sh test
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Animals/AnimalSim.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

using namespace LyudAnimals;

static const char* DataDir = "unreal/Lyudmilovsk/Content/AnimalsData";
static int Failures = 0;

#define CHECK(Cond, ...) do { if (Cond) { std::printf("  ok   "); } else { std::printf("  FAIL "); Failures++; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static void SetClock(FWorld& W, float H)
{
	W.Hour = std::fmod(H, 24.f);
}

static void Run(FWorld& W, float Seconds, float Dt, float& Hour, const std::function<void(float)>& Each = nullptr)
{
	for (float T = 0.f; T < Seconds; T += Dt)
	{
		Hour += Dt / 3600.f;
		SetClock(W, Hour);
		if (Each)
		{
			Each(T);
		}
		W.Step(Dt);
		W.Events.clear();
	}
}

static int Day(int Argc, char** Argv)
{
	const float Start = Argc > 2 ? (float)std::atof(Argv[2]) : 0.f;
	const float Hours = Argc > 3 ? (float)std::atof(Argv[3]) : 24.f;
	FWorld W;
	if (!W.Load(DataDir))
	{
		std::printf("load failed: %s\n", W.LastError.c_str());
		return 1;
	}
	W.Hour = Start;
	W.Populate();
	// the camera at the Academgorodok's square: animals there think and move every frame
	for (const FPlace& P : W.Places)
	{
		if (P.Kind == EPlace::Square && P.Tag == "academ")
		{
			W.Camera = P.Pos;
		}
	}
	std::printf("%d animals, %d packs, %d places\n", (int)W.Animals.size(), (int)W.Packs.size(), (int)W.Places.size());
	for (const FPack& P : W.Packs)
	{
		std::printf("  pack of %d at (%.0f %.0f) %s\n", (int)P.Members.size(), P.Home.X, P.Home.Y, W.Places[P.Den].Tag.c_str());
	}
	FILE* F = std::fopen("Saved/AnimalSim/day.csv", "w");
	std::fprintf(F, "hour,id,kind,act,goal,x,y\n");
	float Hour = Start;
	const float Dt = 0.2f;
	std::vector<std::map<int, float>> ActTime(W.Animals.size());
	std::map<std::string, int> EventCount;
	double Ms = 0.0;
	long Steps = 0;
	float NextDump = 0.f;
	for (float T = 0.f; T < Hours * 3600.f; T += Dt)
	{
		Hour += Dt / 3600.f;
		SetClock(W, Hour);
		const auto T0 = std::chrono::high_resolution_clock::now();
		W.Step(Dt);
		Ms += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count();
		Steps++;
		for (const FEvent& E : W.Events)
		{
			EventCount[EventName(E.Kind)]++;
		}
		W.Events.clear();
		W.ScaredBirds.clear();
		for (const FAnimal& A : W.Animals)
		{
			ActTime[A.Id][(int)A.Act] += Dt;
		}
		if (T >= NextDump)
		{
			NextDump += 600.f;
			for (const FAnimal& A : W.Animals)
			{
				std::fprintf(F, "%.2f,%d,%s,%s,%s,%.1f,%.1f\n", std::fmod(Hour, 24.f), A.Id, A.Kind == EKind::Cat ? "Cat" : "Dog", ActName(A.Act), GoalName(A.Goal), A.Pos.X, A.Pos.Y);
			}
			if (std::fmod(T, 7200.f) < Dt)
			{
				std::printf("%s\n", W.Report().c_str());
			}
		}
	}
	std::fclose(F);
	// a day's budget: how long each kind sleeps, eats, walks
	for (int K = 0; K < 2; K++)
	{
		std::map<int, float> Sum;
		int N = 0;
		int Meals = 0;
		for (const FAnimal& A : W.Animals)
		{
			if ((int)A.Kind != K)
			{
				continue;
			}
			N++;
			Meals += A.Meals;
			for (auto& KV : ActTime[A.Id])
			{
				Sum[KV.first] += KV.second;
			}
		}
		std::printf("%s (%d), hours a day each:", K == 0 ? "cats" : "dogs", N);
		for (auto& KV : Sum)
		{
			std::printf(" %s %.1f", ActName((EAct)KV.first), KV.second / 3600.f / std::max(N, 1) * (24.f / Hours));
		}
		std::printf(" | meals %.1f\n", Meals / (float)std::max(N, 1) * (24.f / Hours));
	}
	std::printf("events:");
	for (auto& KV : EventCount)
	{
		std::printf(" %s %d", KV.first.c_str(), KV.second);
	}
	std::printf("\nstep %.4f ms (%ld steps)\n", Ms / Steps, Steps);
	return 0;
}

// --- tests -------------------------------------------------------------------------------------------

static FWorld Empty()
{
	FWorld W;
	W.NoGridZ = 0.f;
	W.Hour = 17.5f;        // dusk: everybody is up
	return W;
}

static void TestChaseAndTree()
{
	std::printf("a dog after a cat: the cat runs up a tree, the dog barks below, the cat comes down later\n");
	FWorld W = Empty();
	W.AddRefuge(V3(8.f, 0.f, 0.f), 3.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(0.f, 0.f, 0.f));
	const int Dog = W.AddAnimal(EKind::Dog, 0, 0, V3(-12.f, 0.f, 0.f));
	W.Animals[Dog].Hunter = 1.f;
	W.Animals[Dog].Excite = 1.f;
	W.Animals[Cat].Bold = 0.3f;
	W.Animals[Cat].Tired = 0.f;
	W.Animals[Dog].Tired = 0.f;
	W.Animals[Dog].Range = 15.f;            // it keeps round here
	W.Animals[Dog].HomePos = V3(-6.f, 0.f, 0.f);
	W.Animals[Cat].Act = EAct::Sit;
	W.Animals[Cat].ActLen = 60.f;
	float H = 17.5f;
	bool bChase = false, bUp = false, bBark = false, bDown = false;
	float UpAt = -1.f;
	Run(W, 400.f, 0.05f, H, [&](float T)
	{
		if (getenv("ANIM_DEBUG") && std::fmod(T, 2.f) < 0.05f) std::printf("    %.1f %s | %s\n", T, W.Describe(Cat).c_str(), W.Describe(Dog).c_str());
		if (!bChase)
		{
			W.Animals[Dog].Excite = 1.f;      // a dog in the mood for it, until it goes
		}
		const FAnimal& C = W.Animals[Cat];
		const FAnimal& D = W.Animals[Dog];
		bChase |= D.Act == EAct::Chase && D.Target == Cat;
		if (C.Act == EAct::Perch && !bUp)
		{
			bUp = true;
			UpAt = T;
		}
		bBark |= bUp && D.Act == EAct::Bark;
		bDown |= bUp && C.Up <= 0.f && C.Act != EAct::Perch && C.Act != EAct::Climb;
	});
	CHECK(bChase, "the dog chased the cat");
	CHECK(bUp, "the cat went up the tree (%.0f s in)", UpAt);
	CHECK(bBark, "the dog barked at it up there");
	CHECK(bDown, "the cat came down after the dog left");
}

static void TestHissStandoff()
{
	std::printf("no tree near: a bold cat stands its ground and hisses, the dog stops\n");
	FWorld W = Empty();
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(0.f, 0.f, 0.f));
	const int Dog = W.AddAnimal(EKind::Dog, 0, 0, V3(-6.f, 0.f, 0.f));
	W.Animals[Cat].Bold = 0.9f;
	W.Animals[Cat].Act = EAct::Sit;
	W.Animals[Cat].ActLen = 60.f;
	W.Animals[Dog].Hunter = 1.f;
	W.Animals[Dog].Excite = 1.f;
	W.Animals[Dog].Range = 10.f;
	W.Animals[Dog].HomePos = V3(-4.f, 0.f, 0.f);
	W.Animals[Dog].Bold = 0.8f;
	float H = 17.5f;
	bool bHiss = false, bStop = false, bHurt = false;
	Run(W, 240.f, 0.05f, H, [&](float T)
	{
		if (getenv("ANIM_DEBUG") && std::fmod(T, 0.5f) < 0.05f) std::printf("    %.1f %s | %s\n", T, W.Describe(Cat).c_str(), W.Describe(Dog).c_str());
		bHiss |= W.Animals[Cat].Act == EAct::Hiss;
		bStop |= bHiss && W.Animals[Dog].Act == EAct::Bark;
		bHurt |= (W.Animals[Cat].Pos - W.Animals[Dog].Pos).Len2D() < 0.25f;
	});
	CHECK(bHiss, "the cat hissed");
	CHECK(bStop, "the dog stopped and barked");
	CHECK(!bHurt, "they never touched");
}

static void TestStalk()
{
	std::printf("a cat and a sparrow on the ground: it stalks, crouches, pounces (the bird is scared off)\n");
	FWorld W = Empty();
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(0.f, 0.f, 0.f));
	W.Animals[Cat].Hunter = 1.f;
	W.Animals[Cat].Hunger = 0.4f;
	W.Animals[Cat].Tired = 0.f;
	W.Animals[Cat].Lively = 1.f;
	FPrey Bird;
	Bird.Id = 77;
	Bird.Pos = V3(9.f, 3.f, 0.f);
	Bird.Size = 0.15f;
	W.Prey.push_back(Bird);
	float H = 17.5f;
	bool bStalk = false, bCrouch = false, bPounce = false, bScared = false;
	Run(W, 240.f, 0.05f, H, [&](float)
	{
		const EAct A = W.Animals[Cat].Act;
		bStalk |= A == EAct::Stalk;
		bCrouch |= bStalk && A == EAct::Crouch;
		bPounce |= bCrouch && A == EAct::Pounce;
		for (int Id : W.ScaredBirds)
		{
			if (Id == 77)
			{
				bScared = true;
				W.Prey.clear();         // it flies
			}
		}
		W.ScaredBirds.clear();
	});
	CHECK(bStalk, "it stalked");
	CHECK(bCrouch, "it crouched before the pounce");
	CHECK(bPounce, "it pounced");
	CHECK(bScared, "the bird was put up");
}

static void TestPlayer()
{
	std::printf("the player: running at a cat frightens it; standing still, a friendly dog comes up; a trusting cat rubs\n");
	{
		FWorld W = Empty();
		const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(0.f, 0.f, 0.f));
		W.Animals[Cat].Bold = 0.4f;
		W.Animals[Cat].Friendly = 0.3f;
		W.Animals[Cat].Tired = 0.f;
		W.Player.bPresent = true;
		float H = 17.5f;
		bool bFled = false;
		V3 P(-20.f, 0.f, 0.f);
		Run(W, 8.f, 0.05f, H, [&](float)
		{
			P.X += 4.5f * 0.05f;
			W.Player.Feet = P;
			W.Player.Eye = P + V3(0.f, 0.f, 1.65f);
			W.Player.Vel = V3(4.5f, 0.f, 0.f);
			W.Player.Facing = V3(1.f, 0.f, 0.f);
			bFled |= W.Animals[Cat].Act == EAct::Flee;
		});
		CHECK(bFled, "a runner made the cat flee (at %.1f m)", (W.Animals[Cat].Pos - P).Len2D());
	}
	{
		FWorld W = Empty();
		const int Dog = W.AddAnimal(EKind::Dog, 0, 0, V3(9.f, 0.f, 0.f));
		W.Animals[Dog].Friendly = 0.95f;
		W.Animals[Dog].Curious = 0.9f;
		W.Animals[Dog].Hunger = 0.7f;
		W.Animals[Dog].Tired = 0.f;
		W.Player.bPresent = true;
		W.Player.Feet = V3(0.f, 0.f, 0.f);
		W.Player.Eye = V3(0.f, 0.f, 1.65f);
		W.Player.Facing = V3(1.f, 0.f, 0.f);
		float H = 17.5f;
		bool bCame = false, bBeg = false;
		Run(W, 120.f, 0.05f, H, [&](float T)
		{
			if (getenv("ANIM_DEBUG") && std::fmod(T, 2.f) < 0.05f) std::printf("    %.1f %s\n", T, W.Describe(Dog).c_str());
			bCame |= W.Animals[Dog].Act == EAct::Approach;
			bBeg |= W.Animals[Dog].Act == EAct::Beg;
		});
		CHECK(bCame, "the friendly dog came up to a player standing still");
		CHECK(bBeg, "and begged (it was hungry)");
	}
	{
		FWorld W = Empty();
		const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(6.f, 0.f, 0.f));
		W.Animals[Cat].Friendly = 0.9f;
		W.Animals[Cat].Curious = 0.9f;
		W.Animals[Cat].Trust = 0.6f;
		W.Animals[Cat].Tired = 0.f;
		W.Player.bPresent = true;
		W.Player.bCrouched = true;
		W.Player.Feet = V3(0.f, 0.f, 0.f);
		W.Player.Eye = V3(0.f, 0.f, 0.9f);
		float H = 17.5f;
		bool bRub = false;
		Run(W, 180.f, 0.05f, H, [&](float) { bRub |= W.Animals[Cat].Act == EAct::Rub; });
		CHECK(bRub, "a trusting cat came and rubbed the crouching player's legs");
	}
}

static void TestHail()
{
	std::printf("the player speaks to Maru (E): she turns round and barks back gladly (one call of two or three barks)\n");
	FWorld W = Empty();
	const int Dog = W.AddAnimal(EKind::Dog, 3, 0, V3(3.f, 0.f, 0.f));
	W.Animals[Dog].Yaw = 0.f;            // her back to the player
	W.Animals[Dog].Friendly = 0.95f;
	W.Animals[Dog].Tired = 0.f;
	W.Player.bPresent = true;
	W.Player.Feet = V3(0.f, 0.f, 0.f);
	W.Player.Eye = V3(0.f, 0.f, 1.65f);
	float H = 15.f;
	SetClock(W, H);
	W.Step(0.05f);
	W.Events.clear();
	const bool bOk = W.Hail(Dog);
	int Glad = 0, Other = 0;
	bool bFaced = false;
	for (float T = 0.f; T < 4.f; T += 0.05f)
	{
		W.Step(0.05f);
		for (const FEvent& E : W.Events)
		{
			if (E.Animal == Dog)
			{
				(E.Kind == EEvent::BarkHappy ? Glad : Other)++;
			}
		}
		W.Events.clear();
		const float D = std::fabs(std::remainder(W.Animals[Dog].Yaw - 3.14159265f, 6.2831853f));
		bFaced |= D < 0.35f && W.Animals[Dog].Act == EAct::Bark;
	}
	CHECK(bOk, "she heard");
	CHECK(Glad == 1 && Other == 0, "one glad call (%d, other calls %d)", Glad, Other);
	CHECK(bFaced, "she turned to face the player while barking");
	CHECK(W.Animals[Dog].Act != EAct::Bark, "and stopped (%s)", ActName(W.Animals[Dog].Act));
}

static void TestPackPlay()
{
	std::printf("two pack dogs in a playful mood: a bow, then they chase each other round\n");
	FWorld W = Empty();
	const int A = W.AddAnimal(EKind::Dog, 0, 0, V3(0.f, 0.f, 0.f));
	const int B = W.AddAnimal(EKind::Dog, 2, 0, V3(3.f, 1.f, 0.f), true);
	W.Animals[A].Playful = 1.f;
	W.Animals[B].Playful = 1.f;
	W.Animals[A].Playfulness = 1.f;
	W.Animals[B].Playfulness = 1.f;
	W.Animals[A].Tired = W.Animals[B].Tired = 0.f;
	W.Animals[A].Hunger = W.Animals[B].Hunger = 0.f;
	W.Animals[A].Curious = W.Animals[B].Curious = 0.f;
	// they know each other
	W.Animals[A].Pack = W.Animals[B].Pack = -1;
	float H = 17.5f;
	// make them packmates through the public API: a pack of two
	FPack P;
	P.Members = { A, B };
	P.Leader = A;
	W.Packs.push_back(P);
	W.Animals[A].Pack = W.Animals[B].Pack = 0;
	W.SetFamiliarity(A, B, 1.f);
	bool bBow = false, bPlay = false;
	int Swaps = 0;
	bool Was = false;
	Run(W, 300.f, 0.05f, H, [&](float)
	{
		bBow |= W.Animals[A].Act == EAct::PlayBow || W.Animals[B].Act == EAct::PlayBow;
		const bool Both = W.Animals[A].Act == EAct::Play && W.Animals[B].Act == EAct::Play;
		bPlay |= Both;
		if (Both && W.Animals[A].bChaser != Was)
		{
			Swaps++;
			Was = W.Animals[A].bChaser;
		}
	});
	CHECK(bBow, "a play bow");
	CHECK(bPlay, "they played together");
	CHECK(Swaps >= 2, "the chaser and the chased swapped (%d times)", Swaps);
}

static void TestMap()
{
	std::printf("the real map: paths across the town, everybody placed on walkable ground, a day's routine\n");
	FWorld W;
	if (!W.Load(DataDir))
	{
		CHECK(false, "load: %s", W.LastError.c_str());
		return;
	}
	W.Hour = 16.f;
	W.Populate();
	int Cats = 0, Dogs = 0, OnGround = 0, Academ = 0;
	for (const FAnimal& A : W.Animals)
	{
		(A.Kind == EKind::Cat ? Cats : Dogs)++;
		OnGround += W.Walkable(A.Pos.X, A.Pos.Y) ? 1 : 0;
		if (A.Kind == EKind::Cat && A.Home >= 0 && (W.Places[A.Home].Tag == "academ" || W.Places[A.Home].Tag == "gastronom"))
		{
			Academ++;
		}
	}
	CHECK(Cats >= 18 && Dogs >= 15, "%d cats and %d dogs (%d packs)", Cats, Dogs, (int)W.Packs.size());
	CHECK(OnGround == (int)W.Animals.size(), "%d of %d on walkable ground", OnGround, (int)W.Animals.size());
	CHECK(Academ >= Cats / 3, "%d cats live at the Academgorodok", Academ);
	{
		V3 Door;
		for (const FPlace& P : W.Places)
		{
			if (P.Kind == EPlace::ClubDoor)
			{
				Door = P.Pos;
			}
		}
		float H = 16.f;
		int CatNear = 0, DogNear = 0, Samples = 0;
		Run(W, 1800.f, 0.25f, H, [&](float T)
		{
			if (std::fmod(T, 60.f) < 0.25f)
			{
				Samples++;
				for (const FAnimal& A : W.Animals)
				{
					if ((A.Pos - Door).Len2D() < 20.f)
					{
						(A.Kind == EKind::Cat ? CatNear : DogNear)++;
					}
				}
			}
		});
		int Maru = 0;
		for (const FAnimal& A : W.Animals)
		{
			Maru += A.Kind == EKind::Dog && A.Breed == 3 && A.NameKey == "animal.maru.name" && A.bFemale && (A.Pos - Door).Len2D() < 20.f;
		}
		CHECK(Maru == 1, "Maru lives by the club's door (%d)", Maru);
		CHECK(CatNear >= Samples && DogNear >= Samples, "a cat and a dog by the club's door all half an hour (%d / %d of %d looks)", CatNear, DogNear, Samples);
	}
	// paths between random places
	int Ok = 0, N = 0;
	double Ms = 0.0;
	uint32_t R = 99;
	for (int K = 0; K < 60; K++)
	{
		R = R * 1664525u + 1013904223u;
		const FPlace& A = W.Places[R % W.Places.size()];
		R = R * 1664525u + 1013904223u;
		const FPlace& B = W.Places[R % W.Places.size()];
		if (A.Kind == EPlace::Refuge || B.Kind == EPlace::Refuge || A.Tag == "club" || B.Tag == "club" || A.Tag == "spur" || B.Tag == "spur"
			|| A.Tag == "upper" || B.Tag == "upper")
		{
			continue;
		}
		std::vector<V3> Path;
		const auto T0 = std::chrono::high_resolution_clock::now();
		const bool bOk = W.FindPath(A.Pos, B.Pos, EKind::Dog, Path);
		Ms += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count();
		Ok += bOk;
		N++;
	}
	CHECK(Ok >= N * 9 / 10, "%d of %d paths found, %.2f ms each", Ok, N, Ms / std::max(N, 1));
}

static int Trace(int Argc, char** Argv)
{
	const int Id = Argc > 2 ? std::atoi(Argv[2]) : 0;
	const float Start = Argc > 3 ? (float)std::atof(Argv[3]) : 16.f;
	const float Hours = Argc > 4 ? (float)std::atof(Argv[4]) : 1.f;
	const float Every = Argc > 5 ? (float)std::atof(Argv[5]) : 20.f;
	FWorld W;
	if (!W.Load(DataDir))
	{
		return 1;
	}
	W.Hour = Start;
	W.Populate();
	for (const FPlace& P : W.Places)
	{
		if (P.Kind == EPlace::Square && P.Tag == "academ")
		{
			W.Camera = P.Pos;
		}
	}
	float Hour = Start, Next = 0.f;
	Run(W, Hours * 3600.f, 0.2f, Hour, [&](float T)
	{
		if (T >= Next)
		{
			Next += Every;
			const FAnimal& A = W.Animals[Id];
			std::printf("%6.0f %s tier %d path %d/%d goalpos (%.0f %.0f)\n", T, W.Describe(Id).c_str(), A.Tier, A.PathI, (int)A.Path.size(), A.GoalPos.X, A.GoalPos.Y);
		}
	});
	return 0;
}

int main(int Argc, char** Argv)
{
	const char* Mode = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Mode, "trace"))
	{
		return Trace(Argc, Argv);
	}
	if (!std::strcmp(Mode, "day"))
	{
		return Day(Argc, Argv);
	}
	TestChaseAndTree();
	TestHissStandoff();
	TestStalk();
	TestPlayer();
	TestHail();
	TestPackPlay();
	TestMap();
	std::printf(Failures ? "%d FAILED\n" : "all passed\n", Failures);
	return Failures ? 1 : 0;
}
