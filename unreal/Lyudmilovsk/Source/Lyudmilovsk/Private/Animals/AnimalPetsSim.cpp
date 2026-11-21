// Animals that live with people, and the hedgehogs (docs/animals.md "With people"): the part of LyudAnimals::FWorld
// that ties some dogs and cats to the town's households. Plain C++ like the rest of the sim (tools/animalsim).
//
//  * A household's dog lives indoors and comes out only with its owner, on a leash (OwnerWalk, fed by the town's life):
//    it keeps at the owner's left heel, stops to sniff while the leash lets it, greets the dogs they pass. The leash is
//    never longer than LeashLen and never goes through a wall: after everybody has moved, LeashHold brings the dog back
//    along the owner's own trail until the owner can see it again.
//  * A flat's cat sits on its window sill by day, waits at the flat's door at feeding time, and is indoors otherwise.
//  * The feeders can be filled by a person (bFeedByPerson + PutFood): the lady with her bowls at 8:00 and 18:30.
//  * Hedgehogs: out at dusk from April to October, a slow shuffle round their home, a ball when the player or a dog
//    comes close; a cat comes to sniff one and backs off.
#include "AnimalSim.h"

#include <algorithm>
#include <cmath>

namespace LyudAnimals
{
	static constexpr float PtPi = 3.14159265f;
	static float PtClamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
	static float PtWrap(float A)
	{
		while (A > PtPi) A -= 2.f * PtPi;
		while (A < -PtPi) A += 2.f * PtPi;
		return A;
	}
	static V3 PtDir(float Yaw) { return V3(std::cos(Yaw), std::sin(Yaw), 0.f); }
	static float PtYaw(const V3& D) { return std::atan2(D.Y, D.X); }

	// the leash
	static constexpr float HeelSide = 0.55f, HeelBack = 0.15f;  // where the dog walks: left of the owner, a little behind
	static constexpr float TrailStep = 0.3f;                    // the owner's trail: a point every so far
	static constexpr int TrailMax = 16;
	static constexpr float GreetReach = 3.6f, GreetLen = 4.5f;
	// hedgehogs
	static constexpr float HogSpeed = 0.22f, HogCurlNear = 2.6f, HogCurlRun = 4.5f, HogDog = 3.2f, HogUncurl = 7.f;

	const char* PetName(EPet P)
	{
		static const char* N[] = { "", "Indoors", "Leash", "Sill", "Door" };
		static_assert(sizeof(N) / sizeof(N[0]) == (size_t)EPet::Num, "PetName");
		return N[(size_t)P < (size_t)EPet::Num ? (size_t)P : 0];
	}

	// --- owning -------------------------------------------------------------------------------------

	bool FWorld::AdoptDog(int Dog, int Owner, const V3& Door)
	{
		if (Dog < 0 || Dog >= (int)Animals.size() || Animals[Dog].Kind != EKind::Dog || Animals[Dog].Pet != EPet::None || Dog == Walker)
		{
			return false;
		}
		FAnimal& A = Animals[Dog];
		// out of its pack: it has a home of its own now
		if (A.Pack >= 0 && A.Pack < (int)Packs.size())
		{
			FPack& P = Packs[A.Pack];
			P.Members.erase(std::remove(P.Members.begin(), P.Members.end(), Dog), P.Members.end());
			if (P.Leader == Dog)
			{
				P.Leader = P.Members.empty() ? -1 : P.Members[0];
				if (P.Leader >= 0)
				{
					Animals[P.Leader].bLeader = true;
				}
			}
		}
		A.Pack = -1;
		A.bLeader = false;
		A.Owner = Owner;
		A.DoorPos = Door;
		A.HomePos = Door;
		A.Home = -1;
		A.Pet = EPet::Indoors;
		A.PetTime = 0.f;
		A.Pos = Door;
		A.Friend = EFriend::None;
		A.Path.clear();
		A.Speed = 0.f;
		A.Vel = V3();
		SetAct(A, EAct::Rest, 1e9f);
		FLeash L;
		L.Dog = Dog;
		Leashes.push_back(L);
		return true;
	}

	bool FWorld::AdoptCat(int Cat, int Owner, const V3& Sill, float SillYaw, const V3& Door)
	{
		if (Cat < 0 || Cat >= (int)Animals.size() || Animals[Cat].Kind != EKind::Cat || Animals[Cat].Pet != EPet::None || Cat == LapCat)
		{
			return false;
		}
		FAnimal& A = Animals[Cat];
		A.Owner = Owner;
		A.SillPos = Sill;
		A.SillYaw = SillYaw;
		A.DoorPos = Door;
		A.HomePos = Door;
		A.Home = -1;
		A.Pet = EPet::Indoors;
		A.PetTime = 0.f;
		A.Pos = Door;
		A.Friend = EFriend::None;
		A.Path.clear();
		A.Speed = 0.f;
		A.Vel = V3();
		A.Up = 0.f;
		SetAct(A, EAct::Rest, 1e9f);
		return true;
	}

	FLeash* FWorld::LeashFor(int Dog)
	{
		for (FLeash& L : Leashes)
		{
			if (L.Dog == Dog)
			{
				return &L;
			}
		}
		return nullptr;
	}

	const FLeash* FWorld::LeashOf(int Dog) const
	{
		for (const FLeash& L : Leashes)
		{
			if (L.Dog == Dog)
			{
				return &L;
			}
		}
		return nullptr;
	}

	void FWorld::OwnerWalk(int Dog, bool bOut, const V3& Feet, const V3& Vel, const V3& Hand)
	{
		FLeash* L = LeashFor(Dog);
		if (!L)
		{
			return;
		}
		FAnimal& A = Animals[Dog];
		if (!bOut)
		{
			if (L->bOut)
			{
				// home again: in at the door
				L->bOut = false;
				L->Trail.clear();
				if (A.Friend != EFriend::None)
				{
					FriendEnd(A, EAct::Rest, 1e9f);
				}
				A.Pet = EPet::Indoors;
				A.PetTime = 0.f;
				A.Pos = A.DoorPos;
				A.Speed = 0.f;
				A.Vel = V3();
				A.Path.clear();
				SetAct(A, EAct::Rest, 1e9f);
			}
			return;
		}
		L->Owner = Feet;
		L->Vel = Vel;
		L->Hand = Hand;
		if (!L->bOut)
		{
			// out of the door with the owner: at their left side
			L->bOut = true;
			L->Trail.clear();
			L->Sniff = L->Greet = L->Still = 0.f;
			L->NextSniff = 5.f + 8.f * Rand(A.Rng);
			L->GreetDog = -1;
			const float Yaw = Vel.Len2D() > 0.1f ? PtYaw(Vel) : A.Yaw;
			V3 P = Feet + PtDir(Yaw + PtPi * 0.5f) * HeelSide;
			if (!Walkable(P.X, P.Y) || !LineWalkable(Feet, P))
			{
				P = Feet;
			}
			A.Pos = P;
			A.Pos.Z = GroundZ(P.X, P.Y);
			A.Yaw = Yaw;
			A.Pet = EPet::Leash;
			A.PetTime = 0.f;
			A.Fear = A.Alert = 0.f;
			A.Path.clear();
			A.Goal = EGoal::Follow;
			SetAct(A, EAct::Stand, 1e9f);
		}
		if (L->Trail.empty() || (L->Trail.back() - Feet).Len2D() >= TrailStep)
		{
			L->Trail.push_back(Feet);
			if ((int)L->Trail.size() > TrailMax)
			{
				L->Trail.erase(L->Trail.begin());
			}
		}
	}

	bool FWorld::AllowStroke(int Dog, float Seconds)
	{
		if (Dog < 0 || Dog >= (int)Animals.size() || Animals[Dog].Pet != EPet::Leash)
		{
			return false;
		}
		Animals[Dog].Sniffed = std::max(Animals[Dog].Sniffed, Seconds);
		return true;
	}

	void FWorld::PutFood(int Place)
	{
		if (Place >= 0 && Place < (int)Places.size() && Places[Place].Kind == EPlace::Feeder)
		{
			Places[Place].FoodLeft = (float)Places[Place].Capacity;
		}
	}

	void FWorld::FeedersNear(const V3& P, float R, std::vector<int>& Out) const
	{
		Out.clear();
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Kind == EPlace::Feeder && (Places[I].Pos - P).Len2D() <= R)
			{
				Out.push_back(I);
			}
		}
		std::sort(Out.begin(), Out.end(), [&](int A, int B) { return (Places[A].Pos - P).Len2D() < (Places[B].Pos - P).Len2D(); });
	}

	// --- thinking and moving ------------------------------------------------------------------------

	bool FWorld::PetThink(FAnimal& A, float Dt)
	{
		if (A.Hog >= 0)
		{
			// a cat on its way to a hedgehog (HedgehogStep minds the meeting); a fright ends it
			if (A.Hog >= (int)Hedgehogs.size() || A.Pet != EPet::None || A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Hiss
				|| A.Act == EAct::Perch || Hedgehogs[A.Hog].Cat != A.Id)
			{
				A.Hog = -1;
				A.bLook = false;
				return false;
			}
			return true;
		}
		if (A.Do != ECatDo::None)
		{
			// with the townsfolk (CatDoMove steers it); a fright, the player's hand or a new home ends it
			if (A.Pet != EPet::None || A.Friend != EFriend::None || A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch
				|| A.Act == EAct::Hiss || A.Act == EAct::Chase || A.Act == EAct::Pounce)
			{
				CatDoEnd(A, 300.f);
				return false;
			}
			return true;
		}
		if (A.Pet == EPet::None)
		{
			return false;
		}
		A.PetTime += Dt;
		if (A.Kind == EKind::Dog)
		{
			// fed and rested at home: its needs never drive it
			A.Hunger = std::min(A.Hunger, 0.4f);
			A.Tired = std::min(A.Tired, 0.4f);
			return true;        // LeashMove steers it; indoors it does nothing
		}
		// a flat's cat: the clock says where it is
		const bool bFeed = FeedingTime(Hour);
		const bool bDay = Hour >= Sunrise + 0.5f && Hour < Sunset;
		// the sill by the weather: not in the rain, and in the cold only when the sun is on it
		const EPet Want = bFeed ? EPet::Door : (bDay && SillWeather() ? EPet::Sill : EPet::Indoors);
		if (Want != A.Pet)
		{
			A.Pet = Want;
			A.PetTime = 0.f;
			A.Speed = 0.f;
			A.Vel = V3();
			A.Path.clear();
			A.bLook = false;
			if (Want == EPet::Door)
			{
				A.Pos = FreeSpot(A.DoorPos, 0.8f, A.Rng);
				A.Yaw = PtYaw((A.DoorPos - A.Pos).Flat()) ;
				A.Up = 0.f;
				A.CallTimer = 2.f + 6.f * Rand(A.Rng);
				SetAct(A, EAct::Sit, 1e9f);
			}
			else if (Want == EPet::Sill)
			{
				A.Pos = A.SillPos;
				A.Yaw = A.SillYaw;
				SetAct(A, EAct::Sit, 20.f + 60.f * Rand(A.Rng));
			}
			else
			{
				A.Pos = A.DoorPos;
				A.Hunger = 0.1f;        // fed
				A.Meals++;
				SetAct(A, EAct::Rest, 1e9f);
			}
		}
		if (A.Pet == EPet::Door)
		{
			// asks to be let in and fed
			A.Look = A.DoorPos + V3(0.f, 0.f, 0.6f);
			A.bLook = true;
			A.CallTimer -= Dt;
			if (A.CallTimer <= 0.f)
			{
				A.CallTimer = 8.f + 14.f * Rand(A.Rng);
				Emit(EEvent::Meow, A);
			}
		}
		else if (A.Pet == EPet::Sill)
		{
			// the day on the sill: sits, loafs, dozes, washes, watches whoever passes below
			A.Tired = std::min(A.Tired, 0.5f);
			const bool bPasser = Player.bPresent && (Player.Feet - A.Pos).Len2D() < 12.f;
			if (bPasser && A.Act != EAct::Sleep)
			{
				A.Look = Player.Eye;
				A.bLook = true;
				if (A.Act != EAct::Watch && A.Act != EAct::Groom)
				{
					SetAct(A, EAct::Watch, 6.f + 6.f * Rand(A.Rng));
				}
			}
			else if (A.ActTime >= A.ActLen)
			{
				A.bLook = false;
				const float R = Rand(A.Rng);
				// a warm sill in the sun is for sleeping; a grey day keeps it awake and watching
				const bool bWarm = Weather.Sun > 0.6f ? Weather.TempC >= 10.f : (Weather.Sun > 0.3f && Hour > 11.f && Hour < 16.f);
				if (R < (bWarm ? 0.4f : 0.15f))
				{
					SetAct(A, EAct::Sleep, 120.f + 240.f * Rand(A.Rng));
				}
				else if (R < 0.6f)
				{
					SetAct(A, EAct::Rest, 60.f + 120.f * Rand(A.Rng));
				}
				else if (R < 0.78f)
				{
					SetAct(A, EAct::Groom, 10.f + 20.f * Rand(A.Rng));
				}
				else
				{
					SetAct(A, EAct::Sit, 30.f + 60.f * Rand(A.Rng));
				}
			}
		}
		return true;
	}

	bool FWorld::PetMove(FAnimal& A, float Dt)
	{
		if (A.Hog >= 0 && A.Hog < (int)Hedgehogs.size() && A.Pet == EPet::None && A.Act != EAct::Flee && A.Act != EAct::Climb)
		{
			// up to the hedgehog, slowly, nose first; then a sniff at it
			const FHedgehog& H = Hedgehogs[A.Hog];
			const V3 D = (H.Pos - A.Pos).Flat();
			const float L = D.Len2D();
			A.Look = H.Pos;
			A.bLook = true;
			if (L > 0.6f)
			{
				if (A.Act != EAct::Approach)
				{
					SetAct(A, EAct::Approach, 1e9f);
				}
				Steer(A, H.Pos - D.Norm() * 0.5f, 0.7f, Dt);
			}
			else
			{
				if (A.Act != EAct::Sniff)
				{
					SetAct(A, EAct::Sniff, 1e9f);
				}
				A.Speed = 0.f;
				A.Vel = V3();
				const float Off = PtWrap(PtYaw(D) - A.Yaw);
				A.Yaw = PtWrap(A.Yaw + PtClamp(Off, -3.f * Dt, 3.f * Dt));
			}
			A.Pos.Z = ZAt(A);
			return true;
		}
		if (A.Do != ECatDo::None && A.Pet == EPet::None && A.Friend == EFriend::None && A.Act != EAct::Flee && A.Act != EAct::Climb)
		{
			return CatDoMove(A, Dt);
		}
		if (A.Pet == EPet::None)
		{
			return false;
		}
		if (A.Pet == EPet::Leash)
		{
			if (FLeash* L = LeashFor(A.Id))
			{
				LeashMove(A, *L, Dt);
			}
			return true;
		}
		// indoors, on the sill, at the door: where it was put (and put back, should a jump of the clock have resettled it)
		A.Speed = 0.f;
		A.Vel = V3();
		A.Path.clear();
		if (A.Pet == EPet::Sill)
		{
			A.Pos = A.SillPos;
			A.Yaw = A.SillYaw;
		}
		else if (A.Pet == EPet::Indoors)
		{
			A.Pos = A.DoorPos;
		}
		else if (A.Pet == EPet::Door)
		{
			if ((A.Pos - A.DoorPos).Len2D() > 1.5f)
			{
				A.Pos = FreeSpot(A.DoorPos, 0.8f, A.Rng);
			}
			if (A.Act != EAct::Sit)
			{
				SetAct(A, EAct::Sit, 1e9f);
			}
			A.Pos.Z = ZAt(A);
			const float Off = PtWrap(PtYaw((A.DoorPos - A.Pos).Flat()) - A.Yaw);
			A.Yaw = PtWrap(A.Yaw + PtClamp(Off, -2.f * Dt, 2.f * Dt));
		}
		return true;
	}

	void FWorld::LeashMove(FAnimal& A, FLeash& L, float Dt)
	{
		const float OwnerSpeed = L.Vel.Len2D();
		const bool bStill = OwnerSpeed < 0.15f;
		L.Still = bStill ? L.Still + Dt : 0.f;
		L.NoGreet = std::max(0.f, L.NoGreet - Dt);
		const float FromOwner = (A.Pos - L.Owner).Len2D();
		const float OwnerYaw = bStill ? PtYaw((L.Owner - A.Pos).Flat()) + PtPi : PtYaw(L.Vel);

		// another dog close by: nose to nose as far as the leash lets it
		if (L.Greet <= 0.f && L.NoGreet <= 0.f && A.Tier <= 1)
		{
			int Best = -1;
			float BestD = GreetReach;
			ForNear(A.Pos, GreetReach, [&](int I)
			{
				const FAnimal& B = Animals[I];
				const float D = (B.Pos - A.Pos).Len2D();
				if (I != A.Id && B.Kind == EKind::Dog && B.Act != EAct::Sleep && B.Act != EAct::Flee && D < BestD && LineWalkable(A.Pos, B.Pos))
				{
					Best = I;
					BestD = D;
				}
			});
			if (Best >= 0)
			{
				L.Greet = GreetLen;
				L.GreetDog = Best;
				L.Sniff = 0.f;
				Emit(EEvent::Greet, A, Best);
				AddFam(A.Id, Best, 0.05f);
			}
		}
		V3 To;
		float Speed;
		if (L.Greet > 0.f && L.GreetDog >= 0 && L.GreetDog < (int)Animals.size())
		{
			L.Greet -= Dt;
			const FAnimal& B = Animals[L.GreetDog];
			const V3 D = (B.Pos - A.Pos).Flat();
			To = D.Len2D() > 0.7f ? B.Pos - D.Norm() * 0.6f : A.Pos;
			Speed = 1.2f;
			A.Look = B.Pos + V3(0.f, 0.f, 0.3f);
			A.bLook = true;
			A.Excite = std::min(1.f, A.Excite + Dt * 0.5f);
			if (D.Len2D() < 1.f)
			{
				if (A.Act != EAct::Greet)
				{
					SetAct(A, EAct::Greet, GreetLen);
				}
				const float Off = PtWrap(PtYaw(D) - A.Yaw);
				A.Yaw = PtWrap(A.Yaw + PtClamp(Off, -4.f * Dt, 4.f * Dt));
			}
			if (L.Greet <= 0.f || (B.Pos - L.Owner).Len2D() > LeashLen + 1.5f)
			{
				L.Greet = 0.f;
				L.GreetDog = -1;
				L.NoGreet = 45.f;
				A.bLook = false;
			}
		}
		else if (L.Sniff > 0.f)
		{
			// nose down at something by the path until the leash is nearly out
			L.Sniff -= Dt;
			To = A.Pos;
			Speed = 0.f;
			if (A.Act != EAct::Sniff)
			{
				SetAct(A, EAct::Sniff, L.Sniff);
			}
			if (FromOwner > LeashLen * 0.8f || !LineWalkable(L.Owner, A.Pos))
			{
				L.Sniff = 0.f;
			}
		}
		else
		{
			A.bLook = false;
			L.NextSniff -= Dt;
			if (L.NextSniff <= 0.f && (bStill ? L.Still > 1.5f : FromOwner < LeashLen * 0.5f))
			{
				L.NextSniff = 9.f + 14.f * Rand(A.Rng);
				L.Sniff = 2.f + 3.f * Rand(A.Rng);
			}
			if (bStill && L.Still > 12.f && FromOwner < 1.2f)
			{
				// the owner has stopped (a talk, a bench): it sits down by them
				To = A.Pos;
				Speed = 0.f;
				if (A.Act != EAct::Sit)
				{
					SetAct(A, EAct::Sit, 1e9f);
				}
				A.Look = L.Hand;
				A.bLook = true;
			}
			else
			{
				To = L.Owner + PtDir(OwnerYaw + PtPi * 0.5f) * HeelSide - PtDir(OwnerYaw) * HeelBack;
				if (!Walkable(To.X, To.Y) || !LineWalkable(L.Owner, To))
				{
					To = L.Owner - PtDir(OwnerYaw) * 0.5f;      // no room at the side: behind
				}
				const float D = (To - A.Pos).Len2D();
				Speed = D < 0.25f ? 0.f : std::min(4.5f, std::max(OwnerSpeed, 0.6f) + D * 1.5f);
				const EAct Gait = Speed < 0.05f ? EAct::Stand : (Speed < 1.9f ? EAct::Walk : EAct::Trot);
				if (A.Act != Gait)
				{
					SetAct(A, Gait, 1e9f);
				}
			}
		}
		if (Speed > 0.f)
		{
			Steer(A, To, Speed, Dt);
		}
		else
		{
			A.Speed = std::max(0.f, A.Speed - 6.f * Dt);
			A.Vel = PtDir(A.Yaw) * A.Speed;
			A.Pos += A.Vel * Dt;
		}
		LeashHold(A, L);
		A.Pos.Z = ZAt(A);
	}

	/** The leash itself: never longer than LeashLen, never through a wall. */
	void FWorld::LeashHold(FAnimal& A, FLeash& L)
	{
		V3 D = (A.Pos - L.Owner).Flat();
		float Len = D.Len2D();
		if (Len > LeashLen)
		{
			// pulled in
			const V3 P = L.Owner + D * (LeashLen / Len);
			if (Walkable(P.X, P.Y))
			{
				A.Pos.X = P.X;
				A.Pos.Y = P.Y;
				Len = LeashLen;
			}
			else
			{
				Len = 1e9f;     // (the trail below)
			}
		}
		if (Len <= LeashLen && LineWalkable(L.Owner, A.Pos))
		{
			return;
		}
		// round a corner from the owner (or pulled onto a wall): back onto the owner's own trail, at the oldest point of
		// it the owner still sees within the leash
		V3 Best = L.Owner;
		float BestD = (A.Pos - L.Owner).Len2D();
		for (int I = (int)L.Trail.size() - 1; I >= 0; I--)
		{
			const V3& T = L.Trail[I];
			if ((T - L.Owner).Len2D() > LeashLen * 0.9f || !LineWalkable(L.Owner, T))
			{
				break;
			}
			const float D2 = (T - A.Pos).Len2D();
			if (D2 < BestD)     // the nearest such point to where the dog is: the least of a tug
			{
				Best = T;
				BestD = D2;
			}
		}
		A.Pos.X = Best.X;
		A.Pos.Y = Best.Y;
		A.Path.clear();
	}

	void FWorld::PetStep(float Dt)
	{
		// whoever steered them this step (the leash, the player's hand): the leash holds
		for (FLeash& L : Leashes)
		{
			if (L.bOut && L.Dog >= 0 && L.Dog < (int)Animals.size())
			{
				FAnimal& A = Animals[L.Dog];
				if (A.Friend != EFriend::None || A.Tier >= 2)
				{
					LeashHold(A, L);
					A.Pos.Z = ZAt(A);
				}
			}
		}
		HedgehogStep(Dt);
		StallStep(Dt);
	}

	// --- yard cats and the townsfolk ----------------------------------------------------------------

	const char* CatDoName(ECatDo D)
	{
		static const char* N[] = { "None", "Tail", "SeeIn", "Beg", "Scrap", "Shooed" };
		static_assert(sizeof(N) / sizeof(N[0]) == (size_t)ECatDo::Num, "CatDoName");
		return (int)D < (int)ECatDo::Num ? N[(int)D] : "?";
	}

	static constexpr float TailBack = 2.2f;         // m behind the person a cat keeps
	static constexpr float TailSee = 30.f;          // m from which it sets off after them
	static constexpr float StallReach = 90.f;       // m from which a hungry cat comes to an open stall
	static constexpr float StallFront = 1.3f;       // m in front of the counter
	static constexpr float ShooRun = 14.f;          // m it gets out of the way

	/** Free to go about with people: nobody's cat, awake enough, not busy with the player, a dog or a bird. */
	static bool CatFree(const FAnimal& A)
	{
		return A.Kind == EKind::Cat && A.Pet == EPet::None && A.Friend == EFriend::None && A.Do == ECatDo::None && A.Hog < 0
			&& A.Act != EAct::Flee && A.Act != EAct::Climb && A.Act != EAct::Perch && A.Act != EAct::Hiss && A.Act != EAct::Chase
			&& A.Act != EAct::Stalk && A.Act != EAct::Crouch && A.Act != EAct::Pounce && A.Act != EAct::Lap && A.Act != EAct::Petted;
	}

	void FWorld::CatDoEnd(FAnimal& A, float NoDo)
	{
		A.Do = ECatDo::None;
		A.DoTime = 0.f;
		A.bDoThere = false;
		A.Stall = -1;
		A.NoDo = std::max(A.NoDo, NoDo);
		A.bLook = false;
		A.Path.clear();
		if (A.Act != EAct::Flee && A.Act != EAct::Climb && A.Act != EAct::Perch && A.Act != EAct::Hiss)
		{
			A.Speed = 0.f;
			A.Vel = V3();
			SetAct(A, EAct::Stand, 1.f + 2.f * Rand(A.Rng));     // then its own life again (Think chooses)
		}
	}

	void FWorld::SetFavourite(int Cat, int Person)
	{
		if (Cat >= 0 && Cat < (int)Animals.size() && Animals[Cat].Kind == EKind::Cat)
		{
			Animals[Cat].Favourite = Person;
		}
	}

	bool FWorld::FavouriteHome(int Cat, const V3& Feet, const V3& Vel, const V3& Door)
	{
		if (Cat < 0 || Cat >= (int)Animals.size())
		{
			return false;
		}
		FAnimal& A = Animals[Cat];
		if (A.Do == ECatDo::Tail)
		{
			A.DoPos = Feet;
			A.DoVel = Vel;
			A.DoGoal = Door;
			A.DoFresh = 0.f;
			return true;
		}
		// it sets off after them: awake, not deep asleep at night, near enough to have seen them, and not twice an evening
		if (!CatFree(A) || A.NoDo > 0.f || A.Favourite < 0 || A.Act == EAct::Sleep || A.Act == EAct::Eat
			|| (A.Pos - Feet).Len2D() > TailSee || (Feet - Door).Len2D() < 6.f)
		{
			return false;
		}
		A.Do = ECatDo::Tail;
		A.DoTime = 0.f;
		A.DoFresh = 0.f;
		A.bDoThere = false;
		A.DoPos = Feet;
		A.DoVel = Vel;
		A.DoGoal = Door;
		A.Path.clear();
		A.Repath = 0.f;
		Emit(EEvent::Tail, A);
		Emit(EEvent::Meow, A);
		return true;
	}

	int FWorld::AddStall(const V3& Pos, float Yaw)
	{
		FStall S;
		S.Id = (int)Stalls.size();
		S.Pos = Pos;
		S.Yaw = Yaw;
		S.Next = 5.f;
		Stalls.push_back(S);
		return S.Id;
	}

	void FWorld::StallOpen(int Stall, bool bOpen)
	{
		if (Stall >= 0 && Stall < (int)Stalls.size())
		{
			Stalls[Stall].bOpen = bOpen;
		}
	}

	int FWorld::Begging(int Stall) const
	{
		int N = 0;
		for (const FAnimal& A : Animals)
		{
			N += A.Do == ECatDo::Beg && A.Stall == Stall && A.bDoThere ? 1 : 0;
		}
		return N;
	}

	bool FWorld::ThrowScrap(int Stall)
	{
		if (Stall < 0 || Stall >= (int)Stalls.size())
		{
			return false;
		}
		FAnimal* Best = nullptr;
		float BestD = 1e9f;
		for (FAnimal& A : Animals)
		{
			const float D = (A.Pos - Stalls[Stall].Pos).Len2D();
			if (A.Do == ECatDo::Beg && A.Stall == Stall && A.bDoThere && D < BestD)
			{
				Best = &A;
				BestD = D;
			}
		}
		if (!Best)
		{
			return false;
		}
		Best->Do = ECatDo::Scrap;
		Best->DoTime = 0.f;
		Best->bLook = false;
		SetAct(*Best, EAct::Eat, 1e9f);
		Emit(EEvent::Scrap, *Best);
		return true;
	}

	int FWorld::Shoo(const V3& P, float R)
	{
		int N = 0;
		for (FAnimal& A : Animals)
		{
			if (A.Kind != EKind::Cat || A.Pet != EPet::None || A.Friend != EFriend::None || A.Do == ECatDo::Shooed
				|| A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch || A.Act == EAct::Lap
				|| std::fabs(A.Pos.Z + A.Up - P.Z) > 2.5f)
			{
				continue;
			}
			const V3 D = (A.Pos - P).Flat();
			const float L = D.Len2D();
			if (L > R)
			{
				continue;
			}
			// away from the broom, to ground it can stand on (a few bearings tried)
			const float Yaw0 = L > 0.05f ? PtYaw(D) : A.Yaw;
			V3 To = A.Pos;
			for (int K = 0; K < 7; K++)
			{
				const float Yaw = Yaw0 + (K == 0 ? 0.f : ((K & 1) ? 0.5f : -0.5f) * (float)((K + 1) / 2));
				const V3 T = A.Pos + PtDir(Yaw) * ShooRun;
				if (LineWalkable(A.Pos, T))
				{
					To = T;
					break;
				}
			}
			if (A.Do != ECatDo::None)
			{
				A.NoStall = std::max(A.NoStall, 600.f);
			}
			A.Do = ECatDo::Shooed;
			A.DoTime = 0.f;
			A.bDoThere = false;
			A.Stall = -1;
			A.Hog = -1;
			A.DoPos = P;
			A.DoGoal = To;
			A.Path.clear();
			A.bLook = false;
			Emit(EEvent::Shooed, A);
			N++;
		}
		return N;
	}

	void FWorld::StallStep(float Dt)
	{
		for (FAnimal& A : Animals)
		{
			A.NoDo = std::max(0.f, A.NoDo - Dt);
			A.NoStall = std::max(0.f, A.NoStall - Dt);
		}
		for (FStall& S : Stalls)
		{
			S.Next -= Dt;
			if (!S.bOpen || S.Next > 0.f)
			{
				continue;
			}
			S.Next = 20.f;
			// two cats at a stall at most: the nearest hungry bold one comes
			int There = 0;
			FAnimal* Best = nullptr;
			float BestD = StallReach;
			for (FAnimal& A : Animals)
			{
				There += A.Do == ECatDo::Beg && A.Stall == S.Id ? 1 : 0;
				const float D = (A.Pos - S.Pos).Len2D();
				if (CatFree(A) && A.NoStall <= 0.f && A.Hunger > 0.35f && A.Bold > 0.35f && A.Act != EAct::Sleep && A.Act != EAct::Eat && D < BestD)
				{
					Best = &A;
					BestD = D;
				}
			}
			if (Best && There < 2)
			{
				Best->Do = ECatDo::Beg;
				Best->DoTime = 0.f;
				Best->bDoThere = false;
				Best->Stall = S.Id;
				Best->Path.clear();
				Best->Repath = 0.f;
			}
		}
	}

	bool FWorld::CatDoMove(FAnimal& A, float Dt)
	{
		A.DoTime += Dt;
		auto Become = [&](EAct Act)
		{
			if (A.Act != Act)
			{
				SetAct(A, Act, 1e9f);
			}
		};
		auto Stay = [&](const V3& Face)
		{
			A.Speed = 0.f;
			A.Vel = V3();
			const V3 D = (Face - A.Pos).Flat();
			if (D.Len2D() > 0.05f)
			{
				const float Off = PtWrap(PtYaw(D) - A.Yaw);
				A.Yaw = PtWrap(A.Yaw + PtClamp(Off, -3.f * Dt, 3.f * Dt));
			}
		};
		auto Go = [&](const V3& To, float Speed)
		{
			bool bNoWay = false;
			const V3 Want = Route(A, To, Dt, bNoWay);
			if (bNoWay)
			{
				A.Speed = 0.f;
				A.Vel = V3();
				return false;
			}
			Become(Speed > GaitSpeed(A, EAct::Walk) * 1.15f ? EAct::Trot : EAct::Walk);
			Steer(A, Want, Speed, Dt);
			return true;
		};
		auto Call = [&](float Min, float Max)
		{
			A.CallTimer -= Dt;
			if (A.CallTimer <= 0.f)
			{
				A.CallTimer = Min + (Max - Min) * Rand(A.Rng);
				Emit(EEvent::Meow, A);
			}
		};
		switch (A.Do)
		{
		case ECatDo::Tail:
		{
			A.DoFresh += Dt;
			const float D = (A.DoPos - A.Pos).Len2D();
			if (A.DoFresh > 1.5f)
			{
				// they are gone: in at their door (it sees them in), or out of its sight
				if ((A.Pos - A.DoGoal).Len2D() < 10.f)
				{
					A.Do = ECatDo::SeeIn;
					A.DoTime = 0.f;
					A.bDoThere = false;
				}
				else
				{
					CatDoEnd(A, 900.f);
				}
				break;
			}
			if (D > 45.f || A.DoTime > 600.f)
			{
				CatDoEnd(A, 900.f);
				break;
			}
			const float PSpeed = A.DoVel.Len2D();
			A.Look = A.DoPos + V3(0.f, 0.f, 1.5f);
			A.bLook = true;
			if (D <= TailBack + 0.3f && PSpeed < 0.3f)
			{
				// they stopped: it sits down by them and looks up
				Become(EAct::Sit);
				Stay(A.DoPos);
			}
			else
			{
				const V3 Back = PSpeed > 0.3f ? A.DoVel.Flat().Norm() : (A.DoPos - A.Pos).Flat().Norm();
				const float V = PtClamp(PSpeed + (D - TailBack) * 1.2f, 0.f, GaitSpeed(A, EAct::Trot) * 1.2f);
				if (!Go(A.DoPos - Back * TailBack, V))
				{
					Become(EAct::Stand);
				}
			}
			break;
		}
		case ECatDo::SeeIn:
		{
			const float D = (A.DoGoal - A.Pos).Len2D();
			A.Look = A.DoGoal + V3(0.f, 0.f, 0.8f);
			A.bLook = true;
			if (!A.bDoThere)
			{
				if (D < 1.2f || A.DoTime > 20.f || !Go(A.DoGoal, GaitSpeed(A, EAct::Walk)))
				{
					A.bDoThere = true;
					A.DoTime = 0.f;
					A.DoFresh = 18.f + 20.f * Rand(A.Rng);      // how long it sits there
					A.CallTimer = 1.f + 2.f * Rand(A.Rng);
					Emit(EEvent::SeeIn, A);
				}
				break;
			}
			Become(EAct::Sit);
			Stay(A.DoGoal);
			Call(9.f, 16.f);
			if (A.DoTime > A.DoFresh)
			{
				CatDoEnd(A, 7200.f);
			}
			break;
		}
		case ECatDo::Beg:
		{
			if (A.Stall < 0 || A.Stall >= (int)Stalls.size() || !Stalls[A.Stall].bOpen || A.DoTime > 420.f)
			{
				A.NoStall = std::max(A.NoStall, A.DoTime > 420.f ? 3600.f : 300.f);     // nothing came of it: later
				CatDoEnd(A, 0.f);
				break;
			}
			const FStall& S = Stalls[A.Stall];
			const V3 Side = PtDir(S.Yaw + PtPi * 0.5f) * ((A.Id & 1) ? 0.45f : -0.45f);
			const V3 Spot = S.Pos + PtDir(S.Yaw) * StallFront + Side;
			A.Look = S.Pos + V3(0.f, 0.f, 1.4f);
			if (!A.bDoThere)
			{
				A.bLook = false;
				if ((Spot - A.Pos).Len2D() < 0.35f)
				{
					A.bDoThere = true;
					A.DoTime = 0.f;
					A.CallTimer = 1.f + 3.f * Rand(A.Rng);
					Emit(EEvent::BegStall, A);
				}
				else if (!Go(Spot, GaitSpeed(A, EAct::Walk)))
				{
					A.NoStall = 600.f;
					CatDoEnd(A, 0.f);
				}
				break;
			}
			A.bLook = true;
			Become(EAct::Beg);
			Stay(S.Pos);
			Call(8.f, 18.f);
			break;
		}
		case ECatDo::Scrap:
			Become(EAct::Eat);
			A.Speed = 0.f;
			A.Vel = V3();
			if (A.DoTime > 9.f)
			{
				A.Hunger = std::min(A.Hunger, 0.1f);
				A.Meals++;
				A.NoStall = 3.f * 3600.f;
				CatDoEnd(A, 0.f);
			}
			break;
		case ECatDo::Shooed:
		{
			if (!A.bDoThere)
			{
				// off at a trot, tail low; not a fright: no tree, no hiss
				Become(EAct::Trot);
				A.bLook = false;
				const bool bThere = Steer(A, A.DoGoal, GaitSpeed(A, EAct::Trot), Dt);
				if (bThere || (A.DoGoal - A.Pos).Len2D() < 0.6f || A.DoTime > 9.f || (A.Speed < 0.05f && A.DoTime > 1.5f))
				{
					A.bDoThere = true;
					A.DoTime = 0.f;
				}
				break;
			}
			// far enough: it turns, sits and looks back at whoever it was
			Become(EAct::Sit);
			Stay(A.DoPos);
			A.Look = A.DoPos + V3(0.f, 0.f, 1.2f);
			A.bLook = true;
			if (A.DoTime > 5.f)
			{
				CatDoEnd(A, 600.f);
			}
			break;
		}
		default:
			CatDoEnd(A, 0.f);
			break;
		}
		A.Pos.Z = ZAt(A);
		return true;
	}

	// --- hedgehogs ----------------------------------------------------------------------------------

	int FWorld::AddHedgehog(const V3& Home, float Range)
	{
		FHedgehog H;
		H.Id = (int)Hedgehogs.size();
		H.Home = Home;
		H.Home.Z = GroundZ(Home.X, Home.Y);
		H.Pos = H.Goal = H.Home;
		H.Range = Range;
		H.Rng = Opt.Seed * 2654435761u + (uint32_t)(H.Id + 1) * 40503u;
		H.Rng = H.Rng ? H.Rng : 1u;
		H.Yaw = Rand(H.Rng) * 2.f * PtPi;
		Hedgehogs.push_back(H);
		return H.Id;
	}

	bool FWorld::HedgehogsOut() const
	{
		if (Month < 4 || Month > 10)
		{
			return false;       // asleep under the leaves
		}
		// from half an hour before sunset into the night
		const float Since = Hour >= Sunset - 0.5f ? Hour - (Sunset - 0.5f) : Hour + 24.f - (Sunset - 0.5f);
		return Since >= 0.f && Since < 4.5f;
	}

	void FWorld::HedgehogStep(float Dt)
	{
		if (Hedgehogs.empty())
		{
			return;
		}
		const bool bOut = HedgehogsOut();
		for (FHedgehog& H : Hedgehogs)
		{
			H.NoCat = std::max(0.f, H.NoCat - Dt);
			if (!bOut)
			{
				if (H.State != EHog::Away)
				{
					H.State = EHog::Away;
					H.Pos = H.Home;
					H.Curl = 0.f;
					H.Speed = 0.f;
					H.Cat = -1;
				}
				continue;
			}
			if (H.State == EHog::Away)
			{
				H.State = EHog::Sniff;
				H.Timer = 2.f + 4.f * Rand(H.Rng);
				H.Pos = H.Home;
			}
			if ((H.Pos - Camera).Len() > Opt.MidDist)
			{
				continue;       // nobody near: it waits where it is
			}
			// danger: the player close (closer still when they creep), a dog
			bool bDanger = false;
			if (Player.bPresent)
			{
				const float D = (Player.Feet - H.Pos).Len2D();
				const float PlayerSpeed = Player.Vel.Len2D();
				bDanger = D < (PlayerSpeed > 2.5f ? HogCurlRun : (Player.bCrouched && PlayerSpeed < 0.8f ? HogCurlNear * 0.5f : HogCurlNear));
			}
			bool bCatAtIt = false;
			if (!bDanger)
			{
				ForNear(H.Pos, HogDog, [&](int I)
				{
					const FAnimal& B = Animals[I];
					bDanger = bDanger || B.Kind == EKind::Dog;
					bCatAtIt = bCatAtIt || (I == H.Cat && (B.Pos - H.Pos).Len2D() < 0.9f);
				});
			}
			if (bDanger || bCatAtIt)
			{
				H.State = EHog::Curl;
				H.Timer = 0.f;
				H.Speed = 0.f;
			}
			switch (H.State)
			{
			case EHog::Curl:
				H.Curl = std::min(1.f, H.Curl + Dt * 4.f);
				H.Timer += Dt;
				if (H.Timer > HogUncurl)
				{
					H.State = EHog::Sniff;
					H.Timer = 2.f + 3.f * Rand(H.Rng);
				}
				break;
			case EHog::Sniff:
				H.Curl = std::max(0.f, H.Curl - Dt * 0.7f);
				H.Speed = 0.f;
				H.Timer -= Dt;
				if (H.Timer <= 0.f && H.Curl <= 0.f)
				{
					H.Goal = FreeSpot(H.Home, H.Range, H.Rng);
					H.State = LineWalkable(H.Pos, H.Goal) ? EHog::Shuffle : EHog::Sniff;
					H.Timer = 1.f;
				}
				break;
			case EHog::Shuffle:
			{
				H.Curl = std::max(0.f, H.Curl - Dt * 0.7f);
				const V3 D = (H.Goal - H.Pos).Flat();
				const float L = D.Len2D();
				if (L < 0.15f)
				{
					H.State = EHog::Sniff;
					H.Timer = 3.f + 6.f * Rand(H.Rng);
					H.Speed = 0.f;
					break;
				}
				const float Off = PtWrap(PtYaw(D) - H.Yaw);
				H.Yaw = PtWrap(H.Yaw + PtClamp(Off, -2.5f * Dt, 2.5f * Dt));
				H.Speed = std::fabs(Off) < 0.6f ? HogSpeed : HogSpeed * 0.3f;
				const V3 P = H.Pos + PtDir(H.Yaw) * (H.Speed * Dt);
				if (Walkable(P.X, P.Y))
				{
					H.Pos = P;
					H.Pos.Z = GroundZ(P.X, P.Y);
				}
				else
				{
					H.State = EHog::Sniff;
					H.Timer = 1.f;
				}
				H.Phase += Dt * 9.f;
				break;
			}
			default:
				break;
			}
			// a cat that sees it comes to sniff, gets the spines on its nose and backs off
			if (H.Cat < 0 && H.NoCat <= 0.f && H.State != EHog::Curl)
			{
				ForNear(H.Pos, 7.f, [&](int I)
				{
					FAnimal& B = Animals[I];
					if (H.Cat < 0 && B.Kind == EKind::Cat && B.Pet == EPet::None && B.Friend == EFriend::None && B.Tier <= 1 && B.Up <= 0.f
						&& (B.Act == EAct::Sit || B.Act == EAct::Stand || B.Act == EAct::Walk || B.Act == EAct::Rest || B.Act == EAct::Watch)
						&& B.Fear < 0.2f && LineWalkable(B.Pos, H.Pos) && Rand(B.Rng) < Dt * 0.25f * (0.4f + B.Lively))
					{
						H.Cat = I;
						H.CatTime = 0.f;
						B.Hog = H.Id;
						B.Path.clear();
						B.Goal = EGoal::Visit;
						SetAct(B, EAct::Approach, 1e9f);
					}
				});
			}
			else if (H.Cat >= 0 && H.Cat < (int)Animals.size())
			{
				FAnimal& B = Animals[H.Cat];
				H.CatTime += Dt;
				const float D = (B.Pos - H.Pos).Flat().Len2D();
				const bool bLost = B.Hog != H.Id || B.Friend != EFriend::None || H.CatTime > 40.f;
				if (bLost)
				{
					if (B.Hog == H.Id)
					{
						B.Hog = -1;
						B.bLook = false;
						SetAct(B, EAct::Stand, 1.f);
					}
					H.Cat = -1;
					H.NoCat = 120.f;
				}
				else if (D < 0.9f && H.Curl >= 1.f)
				{
					// pricked: a jump back, then off at a trot, looking over its shoulder
					const V3 Away = (B.Pos - H.Pos).Flat().Norm();
					Emit(EEvent::Hiss, B);
					B.Alert = 1.f;
					B.Cooldown = 60.f;
					B.bLook = false;
					B.Hog = -1;
					GoTo(B, FreeSpot(B.Pos + Away * 5.f, 2.f, B.Rng), EAct::Trot, EGoal::Roam);
					H.Cat = -1;
					H.NoCat = 180.f;
				}
			}
			else
			{
				H.Cat = -1;
			}
		}
	}
}
