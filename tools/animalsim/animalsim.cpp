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
	std::printf(Failures ? "%d FAILED\n" : "all passed\n", Failures);
	return Failures ? 1 : 0;
}
