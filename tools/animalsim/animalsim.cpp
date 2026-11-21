// The animal life outside the engine: a day on the real map (CSV for render_animals.py) and behaviour tests.
//   tools/animalsim/run.sh day [start_hour hours]
//   tools/animalsim/run.sh test
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Animals/AnimalSim.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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


// --- friends (AnimalFriendSim.cpp) ---------------------------------------------------------------------

struct FSeen
{
	int Count[(int)EEvent::Num] = {};
	void Take(FWorld& W, int Id)
	{
		for (const FEvent& E : W.Events)
		{
			if (E.Animal == Id)
			{
				Count[(int)E.Kind]++;
			}
		}
	}
	int operator[](EEvent E) const { return Count[(int)E]; }
};

/** Steps the world with the player as given (Each moves them), counting Id's events. */
static void Live(FWorld& W, float Seconds, int Id, FSeen& Seen, const std::function<void(float)>& Each = nullptr)
{
	const float Dt = 0.05f;
	for (float T = 0.f; T < Seconds; T += Dt)
	{
		if (Each)
		{
			Each(T);
		}
		W.Step(Dt);
		Seen.Take(W, Id);
		W.Events.clear();
	}
}

static void Crouch(FWorld& W, const V3& At, const V3& Facing)
{
	W.Player.bPresent = true;
	W.Player.bCrouched = true;
	W.Player.Feet = At;
	W.Player.Vel = V3();
	W.Player.Eye = At + V3(0.f, 0.f, 0.95f);
	W.Player.Facing = Facing;
	W.Player.Palm = At + Facing * 0.55f + V3(0.f, 0.f, 0.3f);
}

static void Stand(FWorld& W, const V3& At, const V3& Facing, const V3& Vel = V3())
{
	W.Player.bPresent = true;
	W.Player.bCrouched = false;
	W.Player.Feet = At;
	W.Player.Vel = Vel;
	W.Player.Eye = At + V3(0.f, 0.f, 1.65f);
	W.Player.Facing = Facing;
	W.Player.Palm = At + Facing * 0.5f + V3(0.f, 0.f, 0.9f);
}

static void TestHandAndStroke()
{
	std::printf("friends: a cat that trusts the player comes to the hand held out, sniffs it, lets itself be stroked, has enough; fish makes it a friend\n");
	FWorld W = Empty();
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(2.f, 0.3f, 0.f));
	W.Animals[Cat].Friendly = 0.6f;
	W.Animals[Cat].Trust = 0.3f;
	W.Animals[Cat].Tired = 0.f;
	W.Animals[Cat].Act = EAct::Sit;
	W.Animals[Cat].ActLen = 600.f;
	Crouch(W, V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	FSeen S;
	Live(W, 1.f, Cat, S);
	CHECK(!W.Stroke(Cat), "a stranger is not stroked before it has sniffed the hand");
	const bool bCome = W.OfferHand(Cat);
	bool bAtHand = false;
	float Nose = 9.f;
	Live(W, 10.f, Cat, S, [&](float)
	{
		if (W.Animals[Cat].Act == EAct::SniffHand)
		{
			bAtHand = true;
			Nose = std::min(Nose, (W.Animals[Cat].Pos - W.Player.Palm).Len2D());
		}
	});
	CHECK(bCome && bAtHand && S[EEvent::HandSniff] == 1, "it came to the hand and sniffed it (%d sniff, nose %.2f m from the palm)", S[EEvent::HandSniff], Nose);
	CHECK(Nose < 0.5f && (W.Animals[Cat].Pos - W.Player.Feet).Len2D() > 0.4f, "nose at the palm, not on the player's feet");
	const float Bond0 = W.Animals[Cat].Bond;
	int Strokes = 0;
	bool bPetted = false, bLean = false;
	for (int K = 0; K < 6; K++)
	{
		if (W.Stroke(Cat))
		{
			Strokes++;
		}
		Live(W, 5.f, Cat, S, [&](float)
		{
			bPetted |= W.Animals[Cat].Act == EAct::Petted;
			bLean |= W.Animals[Cat].Lean > 0.9f;
		});
	}
	CHECK(bPetted && bLean && S[EEvent::Purr] >= 2, "stroked: it leans into the hand and purrs (%d purrs)", S[EEvent::Purr]);
	CHECK(W.Animals[Cat].Bond > Bond0 + 0.1f, "the bond grew (%.2f -> %.2f)", Bond0, W.Animals[Cat].Bond);
	CHECK(Strokes >= 2 && Strokes <= 4 && S[EEvent::Enough] == 1, "after %d strokes it had enough and walked off (%d)", Strokes, S[EEvent::Enough]);
	CHECK(W.Animals[Cat].Bond < FWorld::FriendBond && S[EEvent::Befriended] == 0, "not a friend in one meeting (bond %.2f)", W.Animals[Cat].Bond);
	// fish on the palm, twice
	W.Player.bOffer = true;
	int Ate = 0;
	for (int K = 0; K < 3 && W.Animals[Cat].Bond < FWorld::FriendBond; K++)
	{
		const bool bOk = W.OfferFood(Cat, 1.f);
		bool bEat = false;
		Live(W, 14.f, Cat, S, [&](float) { bEat |= W.Animals[Cat].Act == EAct::Eat && W.Animals[Cat].Friend == EFriend::EatHand; });
		Ate += bOk && bEat;
		W.Animals[Cat].FoodFill = 0.f;      // another day
	}
	W.Player.bOffer = false;
	CHECK(Ate >= 1 && S[EEvent::Ate] == Ate, "it ate from the palm (%d times, %d Ate events)", Ate, S[EEvent::Ate]);
	CHECK(W.IsFriend(Cat) && S[EEvent::Befriended] == 1, "a friend now, told once (bond %.2f, %d)", W.Animals[Cat].Bond, S[EEvent::Befriended]);
	CHECK(!W.StartWalk(Cat), "a cat never walks at heel");
}

static void TestShyAndFussy()
{
	std::printf("friends: a shy cat keeps off the hand; a dog will not eat sweets; a rush at a stranger ends it\n");
	{
		FWorld W = Empty();
		const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(2.2f, 0.f, 0.f));
		W.Animals[Cat].Friendly = 0.2f;
		W.Animals[Cat].Bold = 0.6f;
		W.Animals[Cat].Trust = 0.02f;
		W.Animals[Cat].Tired = 0.f;
		W.Animals[Cat].Habituation = 1.f;
		Crouch(W, V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
		FSeen S;
		const bool bCome = W.OfferHand(Cat);
		float Nearest = 9.f;
		Live(W, 6.f, Cat, S, [&](float) { Nearest = std::min(Nearest, (W.Animals[Cat].Pos - W.Player.Palm).Len2D()); });
		CHECK(!bCome && S[EEvent::HandRefuse] == 1 && S[EEvent::HandSniff] == 0 && Nearest > 1.f, "the shy cat did not come (nearest %.1f m), and trusts a little more for the patience (%.3f)",
			Nearest, W.Animals[Cat].Trust);
	}
	{
		FWorld W = Empty();
		const int Dog = W.AddAnimal(EKind::Dog, 0, 0, V3(3.f, 0.f, 0.f));
		W.Animals[Dog].Friendly = 0.8f;
		W.Animals[Dog].Trust = 0.3f;
		W.Animals[Dog].Tired = 0.f;
		Crouch(W, V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
		W.Player.bOffer = true;
		FSeen S;
		const bool bCome = W.OfferFood(Dog, 0.05f);
		Live(W, 12.f, Dog, S);
		CHECK(bCome && S[EEvent::HandSniff] == 1 && S[EEvent::HandRefuse] == 1 && S[EEvent::Ate] == 0, "the dog came, sniffed the sweet and left it (%d sniff, %d refuse, %d ate)",
			S[EEvent::HandSniff], S[EEvent::HandRefuse], S[EEvent::Ate]);
		const float Bond = W.Animals[Dog].Bond;
		W.OfferFood(Dog, 1.f);
		Live(W, 12.f, Dog, S);
		CHECK(S[EEvent::Ate] == 1 && W.Animals[Dog].Bond > Bond + 0.1f && W.Animals[Dog].Hunger < 0.3f, "a sausage it ate (bond %.2f -> %.2f)", Bond, W.Animals[Dog].Bond);
	}
	{
		FWorld W = Empty();
		const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(1.6f, 0.f, 0.f));
		W.Animals[Cat].Friendly = 0.6f;
		W.Animals[Cat].Trust = 0.4f;
		W.Animals[Cat].Tired = 0.f;
		Crouch(W, V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
		FSeen S;
		W.OfferHand(Cat);
		Live(W, 8.f, Cat, S);
		W.Stroke(Cat);
		Live(W, 1.f, Cat, S);
		W.Player.ExtraThreat = 1.f;        // a shout
		bool bFled = false;
		Live(W, 3.f, Cat, S, [&](float) { bFled |= W.Animals[Cat].Act == EAct::Flee; });
		CHECK(bFled && W.Animals[Cat].Friend == EFriend::None, "a shout while stroking a stranger: it ran");
	}
}

/** A friend dog (Maru) at P. */
static int FriendDog(FWorld& W, const V3& P)
{
	const int Dog = W.AddAnimal(EKind::Dog, 3, 0, P);
	FAnimal& A = W.Animals[Dog];
	A.Friendly = 0.95f;
	A.Trust = 0.9f;
	A.Bond = 0.7f;
	A.Tired = 0.f;
	A.Hunger = 0.1f;
	A.bFemale = true;
	A.NameKey = "animal.maru.name";
	return Dog;
}

static void TestWalk()
{
	std::printf("friends: Maru is stroked into a friend, then walks at the player's heel at his pace, sits when he stands, catches up from afar\n");
	FWorld W = Empty();
	W.Hour = 12.f;
	const int Dog = W.AddAnimal(EKind::Dog, 3, 0, V3(1.5f, 0.5f, 0.f));
	W.Animals[Dog].Friendly = 0.95f;
	W.Animals[Dog].Tired = 0.f;
	W.Animals[Dog].Hunger = 0.1f;
	Crouch(W, V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	FSeen S;
	CHECK(!W.StartWalk(Dog), "a dog that is not a friend yet does not come along");
	W.OfferHand(Dog);
	Live(W, 8.f, Dog, S);
	int Strokes = 0;
	bool bWag = false;
	for (int K = 0; K < 14 && !W.IsFriend(Dog); K++)
	{
		Strokes += W.Stroke(Dog) ? 1 : 0;
		Live(W, 4.5f, Dog, S, [&](float) { bWag |= W.Animals[Dog].Act == EAct::Petted && W.Animals[Dog].Excite > 0.5f; });
	}
	CHECK(W.IsFriend(Dog) && Strokes >= 3 && Strokes <= 8 && bWag && S[EEvent::Befriended] == 1, "a friend after %d strokes (tail going, told %d time)", Strokes, S[EEvent::Befriended]);
	CHECK(W.StartWalk(Dog) && W.Walker == Dog, "she comes along");
	// a walk: 60 m east at 1.3 m/s, a corner, 30 m north
	V3 P(0.f, 0.f, 0.f);
	float Sum = 0.f, Far = 0.f, Slide = 0.f;
	int N = 0;
	bool bLeft = true;
	Live(W, 70.f, Dog, S, [&](float T)
	{
		const V3 V = T < 46.f ? V3(1.3f, 0.f, 0.f) : V3(0.f, 1.3f, 0.f);
		P += V * 0.05f;
		Stand(W, P, V.Norm(), V);
		if (T > 4.f)
		{
			const FAnimal& A = W.Animals[Dog];
			const float D = (A.Pos - P).Len2D();
			Sum += D;
			Far = std::max(Far, D);
			N++;
			Slide = std::max(Slide, std::fabs(A.Speed - 1.3f));
			// on his left
			const V3 L(-V.Norm().Y, V.Norm().X, 0.f);
			if (T > 8.f && (T < 44.f || T > 52.f))
			{
				bLeft &= (A.Pos - P).Dot(L) > 0.2f;
			}
		}
	});
	CHECK(Sum / N < 1.3f && Far < 2.6f, "at heel for 90 m: %.2f m away on average, %.2f at most", Sum / N, Far);
	CHECK(bLeft, "on his left side");
	CHECK(W.Animals[Dog].Act == EAct::Follow && std::fabs(W.Animals[Dog].Speed - 1.3f) < 0.25f, "at his pace (%.2f m/s to his 1.30)", W.Animals[Dog].Speed);
	// he stops
	bool bSit = false, bLie = false;
	Live(W, 40.f, Dog, S, [&](float T)
	{
		Stand(W, P, V3(0.f, 1.f, 0.f));
		bSit |= T < 20.f && W.Animals[Dog].Act == EAct::Sit;
		bLie |= T > 30.f && W.Animals[Dog].Act == EAct::Rest;
	});
	CHECK(bSit && bLie, "he stops: she sits beside him, and lies down when it is a long stop");
	// a run
	float Lag = 0.f;
	Live(W, 12.f, Dog, S, [&](float T)
	{
		const V3 V(0.f, 5.f, 0.f);
		P += V * 0.05f;
		Stand(W, P, V.Norm(), V);
		if (T > 5.f)
		{
			Lag = std::max(Lag, (W.Animals[Dog].Pos - P).Len2D());
		}
	});
	CHECK(Lag < 4.f, "he runs: she keeps up (%.1f m behind at most)", Lag);
	// he is suddenly 300 m away (a cabin of the cable car set him down)
	P = V3(300.f, 200.f, 0.f);
	float Back = -1.f;
	Live(W, 20.f, Dog, S, [&](float T)
	{
		Stand(W, P, V3(1.f, 0.f, 0.f));
		if (Back < 0.f && (W.Animals[Dog].Pos - P).Len2D() < 2.5f)
		{
			Back = T;
		}
	});
	CHECK(Back > 0.f && S[EEvent::Rejoin] >= 1, "300 m away at once: she is back at his side in %.0f s", Back);
	// sent home
	W.EndWalk(false);
	S.Take(W, Dog);
	CHECK(W.Walker < 0 && W.Animals[Dog].Friend == EFriend::None && S[EEvent::WalkEnd] == 1, "sent off: she stays behind (walker %d, %s, %d ends)", W.Walker, FriendName(W.Animals[Dog].Friend), S[EEvent::WalkEnd]);
}

static void TestDoorAndNight()
{
	std::printf("friends: Maru waits outside a door she may not enter and lies down there; at night she goes home\n");
	FWorld W = Empty();
	W.Hour = 15.f;
	W.MakeGrid(60, 40, -20.f, -40.f, 2.f, 0.f);
	// a house: x 20..36, y -10..10, its door at (20, 0) on the west wall
	for (float X = 20.f; X <= 36.f; X += 2.f)
	{
		for (float Y = -10.f; Y <= 10.f; Y += 2.f)
		{
			W.SetGround(X, Y, EGround::Blocked, 0.f);
		}
	}
	const int Dog = FriendDog(W, V3(1.f, 1.f, 0.f));
	W.Animals[Dog].HomePos = V3(-10.f, -30.f, 0.f);
	V3 P(0.f, 0.f, 0.f);
	Stand(W, P, V3(1.f, 0.f, 0.f));
	FSeen S;
	Live(W, 0.5f, Dog, S);
	W.StartWalk(Dog);
	bool bSit = false, bLie = false, bInside = false;
	float WaitAt = 9.f;
	Live(W, 60.f, Dog, S, [&](float T)
	{
		// to the door and in, 6 m inside
		const V3 V = P.X < 25.f ? V3(1.3f, 0.f, 0.f) : V3();
		P += V * 0.05f;
		Stand(W, P, V3(1.f, 0.f, 0.f), V);
		const FAnimal& A = W.Animals[Dog];
		bInside |= !W.Walkable(A.Pos.X, A.Pos.Y);
		if (A.Friend == EFriend::Wait)
		{
			bSit |= A.Act == EAct::Sit;
			bLie |= A.Act == EAct::Rest;
			if (T > 50.f)
			{
				WaitAt = (A.Pos - V3(19.f, 0.f, 0.f)).Len2D();
			}
		}
	});
	CHECK(!bInside, "she did not go in");
	CHECK(bSit && bLie && S[EEvent::WaitDoor] == 1, "she sat down outside, then lay down (told %d time)", S[EEvent::WaitDoor]);
	CHECK(WaitAt < 2.5f, "by the door (%.1f m from it), not on the threshold", WaitAt);
	// out again
	float Up = -1.f;
	Live(W, 20.f, Dog, S, [&](float T)
	{
		const V3 V = P.X > 12.f ? V3(-1.3f, 0.f, 0.f) : V3();
		P += V * 0.05f;
		Stand(W, P, V3(-1.f, 0.f, 0.f), V);
		if (Up < 0.f && W.Animals[Dog].Friend == EFriend::Heel)
		{
			Up = T;
		}
	});
	CHECK(Up >= 0.f && S[EEvent::Rejoin] == 1 && (W.Animals[Dog].Pos - P).Len2D() < 2.f, "he comes out: she is up and at his heel again (%.1f m)", (W.Animals[Dog].Pos - P).Len2D());
	// round the house: the heel point's line is cut by the corner, she takes the route
	float Far = 0.f;
	Live(W, 50.f, Dog, S, [&](float T)
	{
		V3 V = P.Y < 14.f && P.X < 40.f ? V3(0.f, 1.3f, 0.f) : (P.X < 40.f ? V3(1.3f, 0.f, 0.f) : V3());
		P += V * 0.05f;
		Stand(W, P, V.Len2D() > 0.f ? V.Norm() : V3(1.f, 0.f, 0.f), V);
		Far = std::max(Far, (W.Animals[Dog].Pos - P).Len2D());
	});
	CHECK(Far < 3.5f && (W.Animals[Dog].Pos - P).Len2D() < 2.f, "round the corner of the house with him (%.1f m at most)", Far);
	// night
	W.Hour = 22.6f;
	bool bHome = false;
	Live(W, 120.f, Dog, S, [&](float)
	{
		Stand(W, P, V3(1.f, 0.f, 0.f));
		bHome |= (W.Animals[Dog].Pos - W.Animals[Dog].HomePos).Len2D() < 5.f;
	});
	CHECK(S[EEvent::WalkEnd] == 1 && W.Walker < 0 && bHome, "half past ten: she left for home and got there (%.0f m from it)", (W.Animals[Dog].Pos - W.Animals[Dog].HomePos).Len2D());
	CHECK(!W.StartWalk(Dog), "and will not set off again at night");
}

static void TestSwim()
{
	std::printf("friends: Maru swims after the player near the shore, turns back when he goes far out, and waits\n");
	FWorld W = Empty();
	W.Hour = 12.f;
	W.MakeGrid(80, 30, -40.f, -30.f, 2.f, 1.f);
	// the sea east of x = 10: the bottom falls away at 1 in 8
	for (float X = 10.f; X < 120.f; X += 2.f)
	{
		for (float Y = -30.f; Y < 30.f; Y += 2.f)
		{
			W.SetGround(X, Y, EGround::Blocked, 1.f - (X - 8.f) / 8.f);
		}
	}
	const int Dog = FriendDog(W, V3(1.f, 1.f, 1.f));
	V3 P(0.f, 0.f, 1.f);
	Stand(W, P, V3(1.f, 0.f, 0.f));
	FSeen S;
	Live(W, 0.5f, Dog, S);
	W.StartWalk(Dog);
	bool bSwam = false, bHeadOut = true;
	float Out = 0.f;
	Live(W, 60.f, Dog, S, [&](float)
	{
		const V3 V = P.X < 30.f ? V3(1.0f, 0.f, 0.f) : V3();
		P += V * 0.05f;
		Stand(W, V3(P.X, P.Y, P.X > 16.f ? -0.3f : 1.f - std::max(0.f, P.X - 8.f) / 8.f), V3(1.f, 0.f, 0.f), V);
		W.Player.bSwimming = P.X > 16.f;
		W.Player.WaterZ = 0.f;
		const FAnimal& A = W.Animals[Dog];
		if (A.Act == EAct::Swim)
		{
			bSwam = true;
			bHeadOut &= std::fabs(A.Pos.Z + 0.34f) < 0.01f;
			Out = std::max(Out, A.Pos.X - 10.f);
		}
	});
	CHECK(bSwam && bHeadOut && (W.Animals[Dog].Pos - P).Len2D() < 3.f, "20 m out: she paddles beside him, her back at the surface (%.1f m from him)", (W.Animals[Dog].Pos - P).Len2D());
	// far out
	bool bWaits = false;
	Live(W, 90.f, Dog, S, [&](float)
	{
		const V3 V = P.X < 70.f ? V3(1.2f, 0.f, 0.f) : V3();
		P += V * 0.05f;
		Stand(W, V3(P.X, P.Y, -0.3f), V3(1.f, 0.f, 0.f), V);
		W.Player.bSwimming = true;
		const FAnimal& A = W.Animals[Dog];
		Out = std::max(Out, A.Pos.X - 10.f);
		bWaits |= A.Act == EAct::Sit && W.Walkable(A.Pos.X, A.Pos.Y);
	});
	CHECK(Out < W.SwimReach + 2.f && bWaits && S[EEvent::Whine] == 1, "60 m out: she went no further than %.0f m, swam back and waits on the shore, whining once", Out);
	// back
	Live(W, 80.f, Dog, S, [&](float)
	{
		const V3 V = P.X > 2.f ? V3(-1.2f, 0.f, 0.f) : V3();
		P += V * 0.05f;
		const bool bWet = P.X > 16.f;
		Stand(W, V3(P.X, P.Y, bWet ? -0.3f : 1.f - std::max(0.f, P.X - 8.f) / 8.f), V3(-1.f, 0.f, 0.f), V);
		W.Player.bSwimming = bWet;
	});
	CHECK(W.Animals[Dog].Friend == EFriend::Heel && (W.Animals[Dog].Pos - P).Len2D() < 2.f, "he comes ashore: at his heel again");
}

static void TestBench()
{
	std::printf("friends: the player sits on a bench: Maru sits by it, a befriended cat comes and sleeps on his lap; a stranger cat does not\n");
	FWorld W = Empty();
	W.Hour = 14.f;
	const int Dog = FriendDog(W, V3(1.f, 1.f, 0.f));
	const int Cat = W.AddAnimal(EKind::Cat, 0, 1, V3(12.f, 6.f, 0.f));
	const int Other = W.AddAnimal(EKind::Cat, 0, 2, V3(10.f, -6.f, 0.f));
	for (int C : { Cat, Other })
	{
		W.Animals[C].Friendly = 0.7f;
		W.Animals[C].Trust = 0.8f;
		W.Animals[C].Tired = 0.2f;
		W.Animals[C].Hunger = 0.1f;
		W.Animals[C].Hunter = 0.f;
		W.SetFamiliarity(C, Dog, 1.f);
	}
	W.SetFamiliarity(Cat, Other, 1.f);
	W.Animals[Cat].Bond = 0.6f;
	V3 P(0.f, 0.f, 0.f);
	Stand(W, P, V3(1.f, 0.f, 0.f));
	FSeen S, SO;
	Live(W, 0.5f, Dog, S);
	W.StartWalk(Dog);
	// he sits down facing east
	W.Player.bSeated = true;
	W.Player.SeatYaw = 0.f;
	W.Player.Lap = V3(0.3f, 0.f, 0.55f);
	FSeen SC;
	bool bDogSat = false, bOnLap = false, bOtherLap = false;
	float LapAt = -1.f;
	const float Dt = 0.05f;
	for (float T = 0.f; T < 120.f; T += Dt)
	{
		Stand(W, P, V3(1.f, 0.f, 0.f));
		W.Player.Eye = V3(0.f, 0.f, 1.15f);
		W.Step(Dt);
		SC.Take(W, Cat);
		SO.Take(W, Other);
		W.Events.clear();
		const FAnimal& D = W.Animals[Dog];
		bDogSat |= D.Friend == EFriend::BySeat && (D.Act == EAct::Sit || D.Act == EAct::Rest) && (D.Pos - P).Len2D() < 1.6f;
		const FAnimal& C = W.Animals[Cat];
		if (C.Act == EAct::Lap && C.FriendTime > 1.f && (C.Pos - W.Player.Lap).Len() < 0.05f)
		{
			if (!bOnLap)
			{
				LapAt = T;
			}
			bOnLap = true;
		}
		bOtherLap |= W.Animals[Other].Friend != EFriend::None;
	}
	CHECK(bDogSat, "Maru sits by the bench");
	CHECK(bOnLap && SC[EEvent::LapUp] == 1 && SC[EEvent::Purr] >= 2, "the befriended cat came from 13 m and lies on his lap, purring (after %.0f s)", LapAt);
	CHECK(!bOtherLap && SO[EEvent::LapUp] == 0, "the cat that is no friend stayed where it was");
	CHECK(W.Stroke(Cat), "it can be stroked there");
	// he gets up
	W.Player.bSeated = false;
	Live(W, 6.f, Cat, SC, [&](float) { Stand(W, P, V3(1.f, 0.f, 0.f)); });
	const FAnimal& C = W.Animals[Cat];
	CHECK(SC[EEvent::LapDown] == 1 && C.Up <= 0.f && std::fabs(C.Pos.Z) < 0.01f && C.Friend == EFriend::None, "he gets up: it hops down to the ground (%s)", ActName(C.Act));
	CHECK(W.Animals[Dog].Friend == EFriend::Heel, "and Maru is at heel again");
}

static void TestFriendsOnMap()
{
	std::printf("friends on the real map: Maru walks with the player from the club's door along the spur\n");
	FWorld W;
	if (!W.Load(DataDir))
	{
		CHECK(false, "load: %s", W.LastError.c_str());
		return;
	}
	W.Hour = 12.f;
	W.Populate();
	int Dog = -1;
	for (const FAnimal& A : W.Animals)
	{
		if (A.NameKey == "animal.maru.name")
		{
			Dog = A.Id;
		}
	}
	if (Dog < 0)
	{
		CHECK(false, "no Maru");
		return;
	}
	W.Animals[Dog].Bond = 0.7f;
	W.Animals[Dog].Trust = 0.9f;
	// a walk of 80 m and more from her door: the farthest place she can reach on foot within 150 m
	const V3 Home = W.Animals[Dog].HomePos;
	std::vector<V3> Route;
	float Best = 0.f;
	for (const FPlace& Pl : W.Places)
	{
		const float D = (Pl.Pos - Home).Len2D();
		std::vector<V3> R;
		if (D > Best && D < 150.f && W.FindPath(Home, Pl.Pos, EKind::Dog, R) && !R.empty())
		{
			Best = D;
			Route = R;
		}
	}
	CHECK(Best > 30.f, "a walk of %.0f m from her door", Best);
	V3 P = W.Animals[Dog].Pos + V3(1.f, 0.f, 0.f);
	W.Camera = P;
	Stand(W, P, V3(1.f, 0.f, 0.f));
	FSeen S;
	Live(W, 0.5f, Dog, S);
	CHECK(W.StartWalk(Dog), "she comes along");
	size_t I = 0;
	float Sum = 0.f, Far = 0.f;
	int N = 0, Off = 0;
	const float Dt = 0.05f;
	for (float T = 0.f; T < 300.f && I < Route.size(); T += Dt)
	{
		V3 To = (Route[I] - P).Flat();
		if (To.Len2D() < 0.5f)
		{
			I++;
			continue;
		}
		const V3 V = To.Norm() * 1.3f;
		P += V * Dt;
		P.Z = W.GroundZ(P.X, P.Y);
		W.Camera = P;
		Stand(W, P, V.Norm(), V);
		W.Step(Dt);
		S.Take(W, Dog);
		W.Events.clear();
		const FAnimal& A = W.Animals[Dog];
		const float D = (A.Pos - P).Len2D();
		if (T > 5.f)
		{
			Sum += D;
			Far = std::max(Far, D);
			N++;
		}
		Off += W.Walkable(A.Pos.X, A.Pos.Y) ? 0 : 1;
	}
	CHECK(I >= Route.size() && N > 0 && Sum / std::max(N, 1) < 2.f && Far < 8.f, "at heel all the way: %.2f m on average, %.1f at most", Sum / std::max(N, 1), Far);
	CHECK(Off == 0, "never off the ground she can walk");
	// sent home from there
	W.EndWalk(true);
	bool bHome = false;
	Live(W, 240.f, Dog, S, [&](float)
	{
		Stand(W, P, V3(1.f, 0.f, 0.f));
		bHome |= (W.Animals[Dog].Pos - Home).Len2D() < 6.f;
	});
	CHECK(bHome, "sent home: she went back to the club's door");
	// the day goes on with a walker in it: a step's cost
	W.StartWalk(Dog);
	const auto T0 = std::chrono::high_resolution_clock::now();
	for (int K = 0; K < 2000; K++)
	{
		W.Step(Dt);
		W.Events.clear();
	}
	const double Ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - T0).count() / 2000.0;
	CHECK(Ms < 0.5, "a step with a friend at heel costs %.4f ms", Ms);
}

static void TestFriends()
{
	TestHandAndStroke();
	TestShyAndFussy();
	TestWalk();
	TestDoorAndNight();
	TestSwim();
	TestBench();
	TestFriendsOnMap();
}

// --- animals with people (AnimalPetsSim.cpp) --------------------------------------------------------

/** The straight line from A to B crosses only ground an animal walks on (sampled every 20 cm). */
static bool ClearLine(const FWorld& W, const V3& A, const V3& B)
{
	const float L = (B - A).Len2D();
	const int N = std::max(1, (int)(L / 0.2f));
	for (int I = 0; I <= N; I++)
	{
		const V3 P = A + (B - A) * ((float)I / N);
		if (!W.Walkable(P.X, P.Y))
		{
			return false;
		}
	}
	return true;
}

/** A yard with a house in it: blocked from (0, 0) to (20, 20). */
static FWorld Yard()
{
	FWorld W = Empty();
	W.MakeGrid(60, 60, -40.f, -40.f, 2.f, 0.f);
	for (float X = 0.f; X <= 20.f; X += 2.f)
	{
		for (float Y = 0.f; Y <= 20.f; Y += 2.f)
		{
			W.SetGround(X, Y, EGround::Blocked, 0.f);
		}
	}
	W.Camera = V3(-5.f, -5.f, 1.6f);
	return W;
}

/** An owner walking round the house, close to its walls, stopping now and then. */
struct FOwnerWalk
{
	std::vector<V3> Route;
	V3 Pos, Vel;
	int Leg = 0;
	float Pause = 0.f;
	float Speed = 1.3f;
	void Step(float Dt, float T)
	{
		Vel = V3();
		if (Pause > 0.f)
		{
			Pause -= Dt;
			return;
		}
		const V3 To = Route[Leg % Route.size()];
		const V3 D = (To - Pos).Flat();
		if (D.Len2D() < 0.1f)
		{
			Leg++;
			Pause = Leg % 3 == 0 ? 16.f : 0.f;       // a long stop every third corner
			return;
		}
		Vel = D.Norm() * Speed;
		Pos += Vel * Dt;
	}
	V3 Hand() const { return Pos + V3(0.f, 0.f, 0.85f); }
};

static void TestLeash()
{
	std::printf("pets: a household's dog walks on the leash round the house\n");
	FWorld W = Yard();
	const V3 Door(-2.4f, 10.f, 0.f);
	const int Dog = W.AddAnimal(EKind::Dog, 2, 0, V3(-10.f, -10.f, 0.f));
	const int Stray = W.AddAnimal(EKind::Dog, 0, 0, V3(24.5f, 10.f, 0.f));
	W.Animals[Stray].Tired = 0.f;
	CHECK(W.AdoptDog(Dog, 7, Door) && W.Animals[Dog].Pet == EPet::Indoors && W.Animals[Dog].Owner == 7 && W.Animals[Dog].Pack < 0,
		"adopted: it lives indoors, out of any pack");
	CHECK(!W.AdoptDog(Dog, 8, Door), "nobody adopts it twice");
	std::vector<int> Near;
	W.Step(0.05f);
	W.Near(Door, 5.f, Near);
	CHECK(std::find(Near.begin(), Near.end(), Dog) == Near.end(), "indoors nobody meets it");
	FOwnerWalk O;
	O.Route = { V3(-2.4f, -2.4f, 0.f), V3(22.4f, -2.4f, 0.f), V3(22.4f, 22.4f, 0.f), V3(-2.4f, 22.4f, 0.f) };
	O.Pos = Door;
	float MaxLen = 0.f, SumLen = 0.f, MaxJump = 0.f;
	int Steps = 0, Through = 0, OffGround = 0, Sniffs = 0, Sat = 0, Greets = 0, Heel = 0, Moving = 0;
	bool bWasSniff = false;
	V3 Last;
	const float Dt = 0.05f;
	for (float T = 0.f; T < 420.f; T += Dt)
	{
		O.Step(Dt, T);
		W.OwnerWalk(Dog, true, O.Pos, O.Vel, O.Hand());
		W.Step(Dt);
		const FAnimal& A = W.Animals[Dog];
		for (const FEvent& E : W.Events)
		{
			Greets += E.Kind == EEvent::Greet && E.Animal == Dog && E.Other == Stray ? 1 : 0;
		}
		W.Events.clear();
		const float Len = (A.Pos - O.Pos).Len2D();
		MaxLen = std::max(MaxLen, Len);
		SumLen += Len;
		Through += ClearLine(W, O.Pos, A.Pos) ? 0 : 1;
		OffGround += W.Walkable(A.Pos.X, A.Pos.Y) ? 0 : 1;
		const bool bSniff = A.Act == EAct::Sniff;
		Sniffs += bSniff && !bWasSniff ? 1 : 0;
		bWasSniff = bSniff;
		Sat += A.Act == EAct::Sit ? 1 : 0;
		if (O.Vel.Len2D() > 0.5f)
		{
			Moving++;
			// at heel: beside the owner or just behind, never out in front pulling
			Heel += (A.Pos - O.Pos).Flat().Dot(O.Vel.Norm()) < 0.9f ? 1 : 0;
		}
		if (Steps > 0)
		{
			if ((A.Pos - Last).Len2D() > 1.f && std::getenv("ANIM_DEBUG"))
			{
				std::printf("    jump %.2f at t %.1f: dog (%.2f %.2f) <- (%.2f %.2f), owner (%.2f %.2f) act %s\n", (A.Pos - Last).Len2D(), T, A.Pos.X, A.Pos.Y, Last.X, Last.Y, O.Pos.X, O.Pos.Y, ActName(A.Act));
			}
			MaxJump = std::max(MaxJump, (A.Pos - Last).Len2D());
		}
		Last = A.Pos;
		Steps++;
	}
	CHECK(W.Animals[Dog].Pet == EPet::Leash, "out with its owner it is on the leash");
	CHECK(MaxLen <= FWorld::LeashLen + 0.02f, "the leash is never longer than %.1f m (longest %.2f, mean %.2f)", FWorld::LeashLen, MaxLen, SumLen / Steps);
	CHECK(Through == 0, "the leash never goes through the house (%d of %d steps)", Through, Steps);
	CHECK(OffGround == 0, "the dog never stands in a wall (%d steps)", OffGround);
	CHECK(SumLen / Steps < 1.5f && Heel >= Moving * 9 / 10, "it keeps at heel (mean %.2f m, %d of %d moving steps not ahead)", SumLen / Steps, Heel, Moving);
	CHECK(MaxJump < 1.3f, "no big jumps when the leash pulls it round a corner (%.2f m)", MaxJump);
	CHECK(Sniffs >= 3, "it stops to sniff on the way (%d times)", Sniffs);
	CHECK(Sat > 100, "it sits down by the owner at a long stop (%d steps)", Sat);
	CHECK(Greets >= 1, "it greets the yard dog they pass (%d)", Greets);
	// may I stroke it?
	Crouch(W, W.Animals[Dog].Pos + V3(0.9f, 0.f, 0.f), V3(-1.f, 0.f, 0.f));
	W.Animals[Dog].Sniffed = 0.f;
	CHECK(!W.OfferHand(Dog) && !W.Stroke(Dog), "a stranger's hand is not let near without asking");
	W.Animals[Dog].Bond = 0.9f;
	CHECK(!W.StartWalk(Dog), "it does not leave its owner for a walk with the player, friend or not");
	W.Animals[Dog].Bond = 0.f;
	CHECK(W.AllowStroke(Dog) && W.Stroke(Dog), "the owner said yes: stroked");
	int Through2 = 0;
	float MaxLen2 = 0.f;
	for (float T = 0.f; T < 6.f; T += Dt)
	{
		W.OwnerWalk(Dog, true, O.Pos, V3(), O.Hand());
		W.Step(Dt);
		W.Events.clear();
		MaxLen2 = std::max(MaxLen2, (W.Animals[Dog].Pos - O.Pos).Len2D());
		Through2 += ClearLine(W, O.Pos, W.Animals[Dog].Pos) ? 0 : 1;
	}
	CHECK(MaxLen2 <= FWorld::LeashLen + 0.02f && Through2 == 0, "and the leash holds while it is stroked (%.2f m)", MaxLen2);
	W.Player.bPresent = false;
	W.OwnerWalk(Dog, false, O.Pos, V3(), O.Hand());
	W.Step(Dt);
	CHECK(W.Animals[Dog].Pet == EPet::Indoors && (W.Animals[Dog].Pos - Door).Len2D() < 0.1f && !W.AllowStroke(Dog), "home again: indoors");
}

static void TestFlatCat()
{
	std::printf("pets: a flat's cat keeps the sill by day and the door at feeding time\n");
	FWorld W = Yard();
	W.Sunrise = 6.f;
	W.Sunset = 20.f;
	const V3 Sill(-1.2f, 6.f, 1.1f), Door(-2.6f, 12.f, 0.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 1, V3(-12.f, 0.f, 0.f));
	const int Dog = W.AddAnimal(EKind::Dog, 0, 0, V3(-4.f, 6.f, 0.f));
	W.Animals[Dog].Tired = 0.f;
	CHECK(W.AdoptCat(Cat, 3, Sill, 3.14159f, Door), "adopted by the flat");
	float Hour = 12.f;
	int OnSill = 0, Looks = 0, Fled = 0, Acts = 0;
	EAct LastAct = EAct::Num;
	Run(W, 1200.f, 0.1f, Hour, [&](float)
	{
		const FAnimal& A = W.Animals[Cat];
		OnSill += A.Pet == EPet::Sill && (A.Pos - Sill).Len() < 0.01f ? 1 : 0;
		Fled += A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Hiss ? 1 : 0;
		Acts += A.Act != LastAct ? 1 : 0;
		LastAct = A.Act;
		Looks++;
	});
	CHECK(OnSill >= Looks - 2, "noon: on its window sill (%d of %d looks)", OnSill, Looks);
	CHECK(Fled == 0, "the dog under the window is nothing to it up there");
	CHECK(Acts >= 3, "it sits, loafs, dozes and washes there (%d changes)", Acts);
	Stand(W, V3(-5.f, 6.f, 0.f), V3(1.f, 0.f, 0.f));
	W.Animals[Cat].Act = EAct::Sit;
	Hour = 12.5f;
	bool bWatch = false;
	Run(W, 10.f, 0.1f, Hour, [&](float) { bWatch = bWatch || (W.Animals[Cat].Act == EAct::Watch && W.Animals[Cat].bLook); });
	CHECK(bWatch, "it watches the player who stops under the window");
	W.Player.bPresent = false;
	Hour = 18.1f;
	int Meows = 0;
	bool bDoor = false;
	for (float T = 0.f; T < 600.f; T += 0.1f)
	{
		Hour += 0.1f / 3600.f;
		SetClock(W, Hour);
		W.Step(0.1f);
		for (const FEvent& E : W.Events)
		{
			Meows += E.Kind == EEvent::Meow && E.Animal == Cat ? 1 : 0;
		}
		W.Events.clear();
		const FAnimal& A = W.Animals[Cat];
		bDoor = A.Pet == EPet::Door && (A.Pos - Door).Len2D() < 1.2f && W.Walkable(A.Pos.X, A.Pos.Y) && A.Act == EAct::Sit;
	}
	CHECK(bDoor, "18:10: sitting at the flat's door");
	CHECK(Meows >= 10, "asking to be fed (%d meows in ten minutes)", Meows);
	Hour = 22.f;
	Run(W, 30.f, 0.1f, Hour);
	std::vector<int> Near;
	W.Near(Door, 6.f, Near);
	CHECK(W.Animals[Cat].Pet == EPet::Indoors && std::find(Near.begin(), Near.end(), Cat) == Near.end() && W.Animals[Cat].Hunger < 0.3f, "night: indoors and fed");
}

static void TestFeederLady()
{
	std::printf("pets: the feeders are filled by the lady with her bowls\n");
	FWorld W = Yard();
	FPlace F;
	F.Kind = EPlace::Feeder;
	F.Pos = V3(-10.f, -10.f, 0.f);
	F.Capacity = 4;
	F.Food = 1.f;
	W.AddPlace(F);
	const int Place = (int)W.Places.size() - 1;
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(-14.f, -12.f, 0.f));
	W.Animals[Cat].Hunger = 0.9f;
	W.Animals[Cat].Tired = 0.f;
	W.bFeedByPerson = true;
	float Hour = 7.9f;
	Run(W, 900.f, 0.1f, Hour);
	CHECK(W.Places[Place].FoodLeft < 0.01f && W.Animals[Cat].Meals == 0, "8:00 passes and no bowl appears by itself (%.1f)", W.Places[Place].FoodLeft);
	CHECK((W.Animals[Cat].Pos - F.Pos).Len2D() < 8.f, "the hungry cat waits by the feeder (%.1f m)", (W.Animals[Cat].Pos - F.Pos).Len2D());
	std::vector<int> Feeders;
	W.FeedersNear(V3(0.f, -10.f, 0.f), 30.f, Feeders);
	CHECK(Feeders.size() == 1 && Feeders[0] == Place, "the lady's round finds the feeder");
	W.PutFood(Place);
	CHECK(W.Places[Place].FoodLeft == 4.f, "she puts the bowls down: full");
	Run(W, 600.f, 0.1f, Hour);
	CHECK(W.Animals[Cat].Meals >= 1 && W.Places[Place].FoodLeft < 4.f, "and the cat eats (%d meals, %.1f left)", W.Animals[Cat].Meals, W.Places[Place].FoodLeft);
	W.bFeedByPerson = false;
	W.Places[Place].FoodLeft = 0.f;
	Hour = 18.45f;
	Run(W, 400.f, 0.1f, Hour);
	CHECK(W.Places[Place].FoodLeft > 0.f || W.Animals[Cat].Meals >= 2, "without her the clock still fills them (the old way)");
}

static void TestHedgehogs()
{
	std::printf("pets: hedgehogs at dusk\n");
	FWorld W = Yard();
	W.Sunrise = 5.f;
	W.Sunset = 21.f;
	W.Month = 7;
	const V3 Home(-14.f, -14.f, 0.f);
	const int H = W.AddHedgehog(Home, 8.f);
	W.Camera = Home + V3(0.f, 0.f, 1.6f);
	float Hour = 14.f;
	Run(W, 60.f, 0.1f, Hour);
	CHECK(!W.HedgehogsOut() && W.Hedgehogs[H].State == EHog::Away, "by day it is not about");
	Hour = 20.6f;
	float Far = 0.f, Walked = 0.f;
	int OffGround = 0;
	V3 Last = W.Hedgehogs[H].Pos;
	Run(W, 600.f, 0.1f, Hour, [&](float)
	{
		const FHedgehog& G = W.Hedgehogs[H];
		Far = std::max(Far, (G.Pos - Home).Len2D());
		Walked += (G.Pos - Last).Len2D();
		Last = G.Pos;
		OffGround += W.Walkable(G.Pos.X, G.Pos.Y) ? 0 : 1;
	});
	CHECK(W.HedgehogsOut() && W.Hedgehogs[H].State != EHog::Away, "half an hour before sunset it comes out");
	CHECK(Walked > 6.f && Walked < 600.f * 0.3f && Far <= 8.2f && OffGround == 0, "a slow shuffle round its home (%.0f m in ten minutes, never over %.1f m away)", Walked, Far);
	// the player walks up
	const V3 At = W.Hedgehogs[H].Pos;
	Stand(W, At + V3(8.f, 0.f, 0.f), V3(-1.f, 0.f, 0.f));
	Run(W, 3.f, 0.1f, Hour);
	CHECK(W.Hedgehogs[H].State != EHog::Curl, "eight metres off it does not mind");
	Stand(W, W.Hedgehogs[H].Pos + V3(1.8f, 0.f, 0.f), V3(-1.f, 0.f, 0.f));
	Run(W, 1.f, 0.1f, Hour);
	const V3 Ball = W.Hedgehogs[H].Pos;
	CHECK(W.Hedgehogs[H].State == EHog::Curl && W.Hedgehogs[H].Curl >= 1.f, "close by: a ball");
	Run(W, 20.f, 0.1f, Hour);
	CHECK(W.Hedgehogs[H].State == EHog::Curl && (W.Hedgehogs[H].Pos - Ball).Len2D() < 0.01f, "and stays one while they stand there");
	W.Player.bPresent = false;
	Run(W, 12.f, 0.1f, Hour);
	CHECK(W.Hedgehogs[H].State != EHog::Curl && W.Hedgehogs[H].Curl < 0.5f, "they go: it uncurls and shuffles on");
	// a cat
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, W.Hedgehogs[H].Pos + V3(3.f, 1.f, 0.f));
	W.Animals[Cat].Tired = 0.f;
	W.Animals[Cat].Hunger = 0.f;
	W.Animals[Cat].Lively = 0.9f;
	int Hiss = 0;
	bool bNose = false;
	float Hour2 = Hour;
	for (float T = 0.f; T < 400.f && Hiss == 0; T += 0.1f)
	{
		Hour2 += 0.1f / 3600.f;
		SetClock(W, Hour2);
		W.Step(0.1f);
		for (const FEvent& E : W.Events)
		{
			Hiss += E.Kind == EEvent::Hiss && E.Animal == Cat ? 1 : 0;
		}
		W.Events.clear();
		bNose = bNose || (W.Animals[Cat].Pos - W.Hedgehogs[H].Pos).Len2D() < 0.9f;
	}
	CHECK(bNose && Hiss == 1 && W.Hedgehogs[H].Curl >= 1.f, "a cat comes to sniff it and gets the spines (%d)", Hiss);
	Run(W, 8.f, 0.1f, Hour2);
	CHECK((W.Animals[Cat].Pos - W.Hedgehogs[H].Pos).Len2D() > 2.5f, "and backs off (%.1f m)", (W.Animals[Cat].Pos - W.Hedgehogs[H].Pos).Len2D());
	W.Month = 12;
	Run(W, 2.f, 0.1f, Hour2);
	CHECK(!W.HedgehogsOut() && W.Hedgehogs[H].State == EHog::Away, "in December they sleep");
}

static void TestFavourite()
{
	std::printf("town cats: a cat sees the resident it likes home\n");
	FWorld W = Yard();
	const V3 Door(22.6f, 10.f, 0.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 1, V3(-12.f, -6.f, 0.f));
	const int Other = W.AddAnimal(EKind::Cat, 0, 2, V3(-36.f, -36.f, 0.f));
	W.Animals[Cat].Tired = 0.f;
	W.Animals[Cat].Hunger = 0.2f;
	W.Animals[Other].Tired = 0.f;
	W.SetFavourite(Cat, 11);
	CHECK(!W.FavouriteHome(Other, V3(-10.f, -2.4f, 0.f), V3(1.3f, 0.f, 0.f), Door), "a cat that likes nobody follows nobody");
	// the resident comes along the house's south wall, turns its corner and goes up to the door in the east wall
	const std::vector<V3> Way = { V3(-20.f, -2.4f, 0.f), V3(22.6f, -2.4f, 0.f), Door };
	V3 P = Way[0];
	size_t Leg = 1;
	float Hour = 17.5f;
	int Tails = 0, SeeIns = 0, Meows = 0, With = 0, Near = 0, Under = 0, Frights = 0, Through = 0;
	bool bIn = false, bSat = false;
	float InAt = 0.f, DoneAt = -1.f, DoorD = 1e9f;
	const float Dt = 0.05f;
	for (float T = 0.f; T < 200.f; T += Dt)
	{
		Hour += Dt / 3600.f;
		SetClock(W, Hour);
		V3 Vel;
		if (!bIn)
		{
			const V3 D = (Way[Leg] - P).Flat();
			if (D.Len2D() < 0.1f)
			{
				Leg++;
				if (Leg >= Way.size())
				{
					bIn = true;
					InAt = T;
				}
			}
			else
			{
				Vel = D.Norm() * 1.3f;
				P = P + Vel * Dt;
			}
		}
		if (!bIn)
		{
			W.FavouriteHome(Cat, P, Vel, Door);
		}
		W.Step(Dt);
		const FAnimal& A = W.Animals[Cat];
		for (const FEvent& E : W.Events)
		{
			Tails += E.Kind == EEvent::Tail && E.Animal == Cat ? 1 : 0;
			SeeIns += E.Kind == EEvent::SeeIn && E.Animal == Cat ? 1 : 0;
			Meows += E.Kind == EEvent::Meow && E.Animal == Cat && bIn ? 1 : 0;
		}
		W.Events.clear();
		if (!bIn && A.Do == ECatDo::Tail)
		{
			const float D = (A.Pos - P).Len2D();
			With++;
			Near += D < 6.f ? 1 : 0;
			Under += D < 0.7f ? 1 : 0;
			Through += W.Walkable(A.Pos.X, A.Pos.Y) ? 0 : 1;
		}
		Frights += A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Hiss ? 1 : 0;
		if (bIn && A.Do == ECatDo::SeeIn && A.bDoThere)
		{
			bSat = bSat || A.Act == EAct::Sit;
			DoorD = std::min(DoorD, (A.Pos - Door).Len2D());
		}
		if (bIn && DoneAt < 0.f && A.Do == ECatDo::None)
		{
			DoneAt = T - InAt;
		}
	}
	CHECK(Tails == 1, "it falls in behind them once (%d)", Tails);
	CHECK(With > 400 && Near > With * 0.8f, "and keeps behind them round the corner (%d of %d steps within 6 m)", Near, With);
	CHECK(Under == 0 && Through == 0 && Frights == 0, "never under their feet, never through the wall (%d, %d)", Under, Through);
	CHECK(SeeIns == 1 && bSat && DoorD < 2.5f, "they go in: it sits down at the door (%.1f m from it)", DoorD);
	CHECK(Meows >= 1, "and says so (%d meows)", Meows);
	CHECK(DoneAt > 15.f && DoneAt < 80.f, "then goes its own way (after %.0f s)", DoneAt);
	CHECK(!W.FavouriteHome(Cat, W.Animals[Cat].Pos + V3(8.f, 0.f, 0.f), V3(1.f, 0.f, 0.f), V3(-30.f, -30.f, 0.f)), "not twice in an evening");
}

static void TestFishStall()
{
	std::printf("town cats: the fish stall\n");
	FWorld W = Yard();
	const V3 StallPos(-20.f, -20.f, 0.f);
	const int Stall = W.AddStall(StallPos, 0.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(-6.f, -30.f, 0.f));
	const int Fed = W.AddAnimal(EKind::Cat, 0, 1, V3(-8.f, -28.f, 0.f));
	const int Shy = W.AddAnimal(EKind::Cat, 0, 2, V3(-8.f, -32.f, 0.f));
	for (int Id : { Cat, Fed, Shy })
	{
		W.Animals[Id].Tired = 0.f;
		W.Animals[Id].Hunger = 0.8f;
		W.Animals[Id].Bold = 0.8f;
	}
	W.Animals[Fed].Hunger = 0.05f;
	W.Animals[Shy].Bold = 0.1f;
	float Hour = 11.f;
	int Came = 0;
	Run(W, 120.f, 0.1f, Hour, [&](float)
	{
		for (int Id : { Cat, Fed, Shy })
		{
			Came += W.Animals[Id].Do == ECatDo::Beg ? 1 : 0;
		}
	});
	CHECK(Came == 0, "shut: nobody comes");
	W.StallOpen(Stall, true);
	int BegEvents = 0, Meows = 0, Wrong = 0;
	for (float T = 0.f; T < 150.f; T += 0.1f)
	{
		Hour += 0.1f / 3600.f;
		SetClock(W, Hour);
		W.Step(0.1f);
		for (const FEvent& E : W.Events)
		{
			BegEvents += E.Kind == EEvent::BegStall && E.Animal == Cat ? 1 : 0;
			Meows += E.Kind == EEvent::Meow && E.Animal == Cat && W.Animals[Cat].bDoThere ? 1 : 0;
		}
		W.Events.clear();
		Wrong += W.Animals[Fed].Do == ECatDo::Beg || W.Animals[Shy].Do == ECatDo::Beg ? 1 : 0;
	}
	const FAnimal& A = W.Animals[Cat];
	const V3 Spot = StallPos + V3(1.3f, 0.f, 0.f);
	const V3 Fwd(std::cos(A.Yaw), std::sin(A.Yaw), 0.f);
	const V3 ToStall = (StallPos - A.Pos).Flat().Norm();
	CHECK(BegEvents == 1 && A.Do == ECatDo::Beg && A.Act == EAct::Beg && W.Begging(Stall) == 1,
		"open: the hungry bold cat comes and sits begging (%s, %s)", CatDoName(A.Do), ActName(A.Act));
	CHECK((A.Pos - Spot).Len2D() < 0.8f && Fwd.X * ToStall.X + Fwd.Y * ToStall.Y > 0.8f && A.bLook,
		"in front of the counter, looking up at the seller (%.2f m off its spot)", (A.Pos - Spot).Len2D());
	CHECK(Meows >= 3, "it asks (%d meows)", Meows);
	CHECK(Wrong == 0, "the fed one and the shy one stay away");
	CHECK(W.ThrowScrap(Stall), "the seller throws a scrap");
	bool bAte = false;
	Run(W, 6.f, 0.1f, Hour, [&](float) { bAte = bAte || (W.Animals[Cat].Do == ECatDo::Scrap && W.Animals[Cat].Act == EAct::Eat); });
	CHECK(bAte, "it eats it there");
	Run(W, 30.f, 0.1f, Hour);
	CHECK(W.Animals[Cat].Do == ECatDo::None && W.Animals[Cat].Hunger < 0.2f && W.Animals[Cat].NoStall > 3600.f,
		"and is off, fed (hunger %.2f)", W.Animals[Cat].Hunger);
	CHECK(!W.ThrowScrap(Stall), "nobody left to throw to");
	W.Animals[Cat].Hunger = 0.9f;
	int Back = 0;
	Run(W, 200.f, 0.1f, Hour, [&](float) { Back += W.Animals[Cat].Do == ECatDo::Beg ? 1 : 0; });
	CHECK(Back == 0, "it does not come straight back for more");
	// another comes; the stall shuts under it
	W.Animals[Fed].Hunger = 0.9f;
	bool bThere = false;
	Run(W, 150.f, 0.1f, Hour, [&](float) { W.Animals[Fed].Hunger = 0.9f; bThere = bThere || (W.Animals[Fed].Do == ECatDo::Beg && W.Animals[Fed].bDoThere); });
	W.StallOpen(Stall, false);
	Run(W, 5.f, 0.1f, Hour);
	CHECK(bThere && W.Animals[Fed].Do == ECatDo::None, "the stall shuts: the next beggar gives up");
}

static void TestShoo()
{
	std::printf("town cats: the cleaner's broom\n");
	FWorld W = Yard();
	W.AddRefuge(V3(-12.f, -14.f, 0.f), 3.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 0, V3(-10.f, -10.f, 0.f));
	const int Far = W.AddAnimal(EKind::Cat, 0, 1, V3(-30.f, -30.f, 0.f));
	W.Animals[Cat].Tired = 0.9f;
	W.Animals[Cat].Act = EAct::Sleep;
	W.Animals[Cat].ActLen = 1e9f;
	float Hour = 23.f;
	const V3 Broom(-11.5f, -10.f, 0.f);
	CHECK(W.Shoo(Broom, 3.f) == 1 && W.Animals[Far].Do == ECatDo::None, "only the cat in the way is sent off");
	int Events = 0;
	for (const FEvent& E : W.Events)
	{
		Events += E.Kind == EEvent::Shooed && E.Animal == Cat ? 1 : 0;
	}
	W.Events.clear();
	int Frights = 0, Off = 0;
	bool bLooked = false;
	float MaxD = 0.f;
	Run(W, 16.f, 0.05f, Hour, [&](float)
	{
		const FAnimal& A = W.Animals[Cat];
		Frights += A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch || A.Act == EAct::Hiss ? 1 : 0;
		Off += W.Walkable(A.Pos.X, A.Pos.Y) ? 0 : 1;
		MaxD = std::max(MaxD, (A.Pos - Broom).Len2D());
		if (A.Do == ECatDo::Shooed && A.bDoThere && A.DoTime > 2.f)
		{
			const V3 Fwd(std::cos(A.Yaw), std::sin(A.Yaw), 0.f);
			const V3 To = (Broom - A.Pos).Flat().Norm();
			bLooked = bLooked || (A.Act == EAct::Sit && A.bLook && Fwd.X * To.X + Fwd.Y * To.Y > 0.7f);
		}
	});
	CHECK(Events == 1 && MaxD > 9.f, "it wakes and trots out of the way (%.1f m)", MaxD);
	CHECK(Frights == 0 && Off == 0, "no fright in it: no tree, no hiss");
	CHECK(bLooked, "then sits and looks back at the broom");
	CHECK(W.Animals[Cat].Do == ECatDo::None, "and is its own cat again");
	// a cat begging at the stall is swept from it and does not come straight back
	const int Stall = W.AddStall(V3(-20.f, -20.f, 0.f), 0.f);
	W.StallOpen(Stall, true);
	FAnimal& B = W.Animals[Far];
	B.Hunger = 0.9f;
	B.Bold = 0.9f;
	B.Tired = 0.f;
	B.Act = EAct::Stand;        // it slept through the night's sweeping
	B.ActTime = 0.f;
	B.ActLen = 1.f;
	Hour = 11.f;
	Run(W, 150.f, 0.1f, Hour, [&](float) { W.Animals[Far].Hunger = 0.9f; });
	const bool bBegged = B.Do == ECatDo::Beg && B.bDoThere;
	const int N = W.Shoo(V3(-20.f, -20.f, 0.f), 3.f);
	int Back = 0;
	Run(W, 120.f, 0.1f, Hour, [&](float) { W.Animals[Far].Hunger = 0.9f; Back += W.Animals[Far].Do == ECatDo::Beg ? 1 : 0; });
	CHECK(bBegged && N == 1 && Back == 0, "the seller's broom clears the counter for a while (begging %d, sent off %d, back %d)", (int)bBegged, N, Back);
}

static void TestSillWeather()
{
	std::printf("town cats: the window sill by the weather\n");
	FWorld W = Yard();
	W.Sunrise = 6.f;
	W.Sunset = 20.f;
	const V3 Sill(-1.2f, 6.f, 1.1f), Door(-2.6f, 12.f, 0.f);
	const int Cat = W.AddAnimal(EKind::Cat, 0, 1, V3(-12.f, 0.f, 0.f));
	W.AdoptCat(Cat, 3, Sill, 3.14159f, Door);
	auto Noon = [&](bool bRain, float Temp, float Sun, float Seconds, int& Asleep)
	{
		W.Weather.bRain = bRain;
		W.Weather.TempC = Temp;
		W.Weather.Sun = Sun;
		float Hour = 12.f;
		int On = 0, Looks = 0;
		Asleep = 0;
		Run(W, Seconds, 0.5f, Hour, [&](float T)
		{
			if (T > 5.f)
			{
				On += W.Animals[Cat].Pet == EPet::Sill ? 1 : 0;
				Asleep += W.Animals[Cat].Act == EAct::Sleep ? 1 : 0;
				Looks++;
			}
		});
		return (float)On / (float)std::max(1, Looks);
	};
	int Sleep = 0, SleepSun = 0, SleepGrey = 0;
	CHECK(Noon(false, 18.f, 0.4f, 60.f, Sleep) > 0.99f, "a dry mild noon: on the sill");
	CHECK(Noon(true, 18.f, 0.f, 60.f, Sleep) < 0.01f, "rain: indoors");
	CHECK(Noon(false, 2.f, 0.1f, 60.f, Sleep) < 0.01f, "a grey cold day: indoors");
	CHECK(Noon(false, 2.f, 0.9f, 60.f, Sleep) > 0.99f, "cold but the sun is on the glass: on the sill");
	CHECK(Noon(false, -8.f, 0.9f, 60.f, Sleep) < 0.01f, "hard frost: indoors whatever the sun");
	Noon(false, 22.f, 0.9f, 7200.f, SleepSun);
	Noon(false, 22.f, 0.1f, 7200.f, SleepGrey);
	CHECK(SleepSun > SleepGrey * 1.3f && SleepSun > 2000, "a warm sunny sill is slept on, a grey one watched from (%d against %d half-seconds asleep)", SleepSun, SleepGrey);
}

static void TestTownCats()
{
	TestFavourite();
	TestFishStall();
	TestShoo();
	TestSillWeather();
}

static void TestMaruMoments()
{
	std::printf("the hero's dog: she waits up, welcomes, brings things, minds the weather and him, has her spots\n");
	FWorld W = Yard();
	W.Sunrise = 6.f;
	W.Sunset = 20.f;
	W.Month = 7;
	const V3 Home(-10.f, -10.f, 0.f);
	FPlace Wall, Hatch, Bench;
	Wall.Kind = EPlace::SunnyWall;
	Wall.Pos = V3(-28.f, -10.f, 0.f);
	Wall.Sun = 1.f;
	Hatch.Kind = EPlace::WarmHatch;
	Hatch.Pos = V3(-10.f, -28.f, 0.f);
	Hatch.Warm = 1.f;
	Bench.Kind = EPlace::Bench;
	Bench.Pos = V3(-28.f, -28.f, 0.f);
	W.AddPlace(Wall);
	W.AddPlace(Hatch);
	W.AddPlace(Bench);
	const int WallId = (int)W.Places.size() - 3, HatchId = WallId + 1, BenchId = WallId + 2;
	const int Dog = W.AddAnimal(EKind::Dog, 3, 0, Home);
	auto Count = [&](EEvent Kind, int* Other = nullptr)
	{
		int N = 0;
		for (const FEvent& E : W.Events)
		{
			if (E.Kind == Kind && E.Animal == Dog)
			{
				N++;
				if (Other)
				{
					*Other = E.Other;
				}
			}
		}
		return N;
	};
	auto Noted = [&](const char* Note)
	{
		return std::find(W.MomentNotes.begin(), W.MomentNotes.end(), std::string(Note)) != W.MomentNotes.end();
	};
	auto Fresh = [&]()
	{
		FAnimal& A = W.Animals[Dog];
		A.Tired = 0.f;
		A.Hunger = 0.1f;
		A.NoWait = A.NoGift = A.NoComfort = A.NoSpot = 0.f;
		A.Moment = EMoment::None;
		A.Act = EAct::Stand;
		A.ActTime = 0.f;
		A.ActLen = 1.f;
		A.Path.clear();
		W.MomentNotes.clear();
		W.Weather = FPetWeather();
		W.Player.bPresent = false;
		W.Player.bCrouched = false;
		W.HeroStill = 0.f;
	};
	// not his dog yet: nothing of this
	W.HeroDog = Dog;
	W.Animals[Dog].Bond = 0.2f;
	Fresh();
	float Hour = 21.5f;
	int Any = 0;
	for (float T = 0.f; T < 120.f; T += 0.1f)
	{
		SetClock(W, Hour);
		W.Step(0.1f);
		Any += W.Animals[Dog].Moment != EMoment::None ? 1 : 0;
		W.Events.clear();
	}
	CHECK(Any == 0, "a dog that is not his friend waits for nobody");

	// he is late: she waits at the door, and runs to meet him
	W.Animals[Dog].Bond = 0.9f;
	Fresh();
	W.Animals[Dog].Pos = V3(-22.f, -20.f, 0.f);
	int Waits = 0, Sat = 0, Lay = 0, Whines = 0;
	for (float T = 0.f; T < 420.f; T += 0.1f)
	{
		Hour += 0.1f / 3600.f;
		SetClock(W, Hour);
		W.Step(0.1f);
		Waits += Count(EEvent::WaitLate);
		Whines += Count(EEvent::Whine);
		W.Events.clear();
		const FAnimal& A = W.Animals[Dog];
		Sat += A.Moment == EMoment::WaitLate && A.bMomentThere && A.Act == EAct::Sit ? 1 : 0;
		Lay += A.Moment == EMoment::WaitLate && A.bMomentThere && A.Act == EAct::Rest ? 1 : 0;
	}
	CHECK(Waits == 1 && (W.Animals[Dog].Pos - Home).Len2D() < 2.5f, "dark and he is not back: she goes to the door (%.1f m from it)", (W.Animals[Dog].Pos - Home).Len2D());
	CHECK(Sat > 100 && Lay > 100 && W.Animals[Dog].Act != EAct::Sleep, "sits, then lies there awake (%d, %d steps), whining now and then (%d)", Sat, Lay, Whines);
	Stand(W, Home + V3(-20.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	int Welcomes = 0, Barks = 0;
	float MinD = 1e9f;
	bool bRan = false, bGreet = false;
	for (float T = 0.f; T < 15.f; T += 0.05f)
	{
		W.Step(0.05f);
		Welcomes += Count(EEvent::Welcome);
		Barks += Count(EEvent::BarkHappy);
		W.Events.clear();
		const FAnimal& A = W.Animals[Dog];
		MinD = std::min(MinD, (A.Pos - W.Player.Feet).Len2D());
		bRan = bRan || A.Act == EAct::Run;
		bGreet = bGreet || A.Act == EAct::Greet;
	}
	CHECK(Welcomes == 1 && Barks >= 1 && bRan && MinD < 1.6f && bGreet, "he comes: she runs to him barking and greets him (to %.1f m)", MinD);
	CHECK(Noted("wait"), "the diary hears of it");
	int Again = 0;
	W.Player.bPresent = false;
	Run(W, 60.f, 0.1f, Hour, [&](float) { Again += W.Animals[Dog].Moment == EMoment::WaitLate ? 1 : 0; });
	CHECK(Again == 0, "one waiting an evening");

	// a gift: she goes off, finds a thing, lays it at his feet
	Fresh();
	Hour = 12.f;
	W.Animals[Dog].Pos = Home;
	Stand(W, Home + V3(-6.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	int Gifts = 0, What = -1;
	float Off = 0.f, GiveD = 1e9f;
	bool bCarried = false, bSniffed = false;
	for (float T = 0.f; T < 60.f; T += 0.05f)
	{
		SetClock(W, Hour);
		W.Step(0.05f);
		const int G = Count(EEvent::Gift, &What);
		const FAnimal& A = W.Animals[Dog];
		if (G)
		{
			GiveD = (A.Pos - W.Player.Feet).Len2D();
		}
		Gifts += G;
		W.Events.clear();
		Off = A.Moment == EMoment::Fetch ? std::max(Off, (A.Pos - W.Player.Feet).Len2D()) : Off;
		bCarried = bCarried || (A.Moment == EMoment::Bring && A.Carry == FWorld::Shell);
		bSniffed = bSniffed || (A.Moment == EMoment::Fetch && A.Act == EAct::Sniff);
	}
	CHECK(Gifts == 1 && What == FWorld::Shell && W.GiftNow() == FWorld::Shell, "in summer she brings him a shell (%d gift, kind %d)", Gifts, What);
	CHECK(Off > 8.f && bSniffed && bCarried && GiveD < 1.6f, "found a little way off (%.1f m), carried back, laid at his feet (%.1f m)", Off, GiveD);
	CHECK(Noted("gift_shell") && W.Animals[Dog].Carry < 0 && W.Animals[Dog].NoGift > 3600.f, "noted, and one a day");
	W.Month = 1;
	const int Winter = W.GiftNow();
	W.Month = 10;
	const int Autumn = W.GiftNow();
	W.Month = 4;
	CHECK(Winter == FWorld::Mitten && Autumn == FWorld::Cone && W.GiftNow() == FWorld::Stick, "a mitten in winter, a cone in autumn, a stick in spring");

	// thunder: she presses against him
	Fresh();
	W.Animals[Dog].NoGift = 1e6f;
	W.Animals[Dog].Pos = Home;
	Stand(W, Home + V3(-10.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	W.Weather.bRain = true;
	W.Weather.bThunder = true;
	int Comforts = 0;
	Run(W, 0.f, 0.1f, Hour);
	for (float T = 0.f; T < 25.f; T += 0.05f)
	{
		W.Step(0.05f);
		Comforts += Count(EEvent::Comfort);
		W.Events.clear();
	}
	CHECK(Comforts == 1 && (W.Animals[Dog].Pos - W.Player.Feet).Len2D() < 1.f && W.Animals[Dog].Lean > 0.5f && Noted("thunder"),
		"thunder: she comes and presses against him (%.2f m, lean %.1f)", (W.Animals[Dog].Pos - W.Player.Feet).Len2D(), W.Animals[Dog].Lean);
	// he sits low and still a long while: she comes too
	Fresh();
	W.Animals[Dog].NoGift = 1e6f;
	W.Animals[Dog].Pos = Home;
	Stand(W, Home + V3(-10.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
	W.Player.bCrouched = true;
	Comforts = 0;
	float CameAt = -1.f;
	for (float T = 0.f; T < 80.f; T += 0.05f)
	{
		W.Step(0.05f);
		if (Count(EEvent::Comfort))
		{
			Comforts++;
			CameAt = T;
		}
		W.Events.clear();
	}
	CHECK(Comforts == 1 && CameAt > 40.f && Noted("comfort"), "he sits low and still: after a while she lies down against him (at %.0f s)", CameAt);

	// rain with him away: under the porch
	Fresh();
	W.Animals[Dog].Pos = V3(-24.f, -22.f, 0.f);
	W.Weather.bRain = true;
	int Shelters = 0;
	for (float T = 0.f; T < 60.f; T += 0.1f)
	{
		SetClock(W, Hour);
		W.Step(0.1f);
		Shelters += Count(EEvent::Shelter);
		W.Events.clear();
	}
	CHECK(Shelters == 1 && (W.Animals[Dog].Pos - Home).Len2D() < 2.f && W.Animals[Dog].Act == EAct::Rest && Noted("shelter"),
		"rain: she lies under the porch (%.1f m from the door)", (W.Animals[Dog].Pos - Home).Len2D());
	W.Weather.bRain = false;
	Run(W, 3.f, 0.1f, Hour);
	CHECK(W.Animals[Dog].Moment != EMoment::Shelter, "it stops: she is up again (%s)", MomentName(W.Animals[Dog].Moment));

	// her places by the season
	W.Month = 1;
	const int SpotWinter = W.FavouriteSpot(Dog);
	W.Month = 7;
	const int SpotSummer = W.FavouriteSpot(Dog);
	W.Month = 10;
	const int SpotAutumn = W.FavouriteSpot(Dog);
	W.Month = 4;
	CHECK(SpotWinter == HatchId && SpotSummer == BenchId && SpotAutumn == WallId && W.FavouriteSpot(Dog) == WallId,
		"the warm hatch in winter, the bench in summer, the sunny wall in spring and autumn");
	Fresh();
	W.Animals[Dog].Pos = Home;
	Hour = 11.f;
	Run(W, 90.f, 0.1f, Hour);
	CHECK((W.Animals[Dog].Pos - Wall.Pos).Len2D() < 2.5f && W.Animals[Dog].Act == EAct::Rest && Noted("spot_spring"),
		"an April morning without him: she lies by the sunny wall (%.1f m from it, %s)", (W.Animals[Dog].Pos - Wall.Pos).Len2D(), ActName(W.Animals[Dog].Act));
}

static void TestPets()
{
	TestLeash();
	TestFlatCat();
	TestFeederLady();
	TestHedgehogs();
	TestTownCats();
	TestMaruMoments();
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
	std::setvbuf(stdout, nullptr, _IOLBF, 0);
	const char* Mode = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Mode, "trace"))
	{
		return Trace(Argc, Argv);
	}
	if (!std::strcmp(Mode, "day"))
	{
		return Day(Argc, Argv);
	}
	const char* Only = std::getenv("ANIM_ONLY");
	if (!Only || !std::strcmp(Only, "life"))
	{
		TestChaseAndTree();
		TestHissStandoff();
		TestStalk();
		TestPlayer();
		TestHail();
		TestPackPlay();
		TestMap();
	}
	if (!Only || !std::strcmp(Only, "friends"))
	{
		TestFriends();
	}
	if (!Only || !std::strcmp(Only, "pets"))
	{
		TestPets();
	}
	std::printf(Failures ? "%d FAILED\n" : "all passed\n", Failures);
	return Failures ? 1 : 0;
}
