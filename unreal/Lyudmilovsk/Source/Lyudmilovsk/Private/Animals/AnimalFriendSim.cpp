// Friends among the animals (docs/animals.md "Friends"): the part of the animal life (AnimalSim.h, plain C++) that is
// about the player making friends with a cat or a dog. A hand held out low is sniffed by an animal that trusts the
// player enough; after that it lets itself be stroked (it leans in, a cat purrs, either may roll over), and it takes
// food it likes from the palm. Both grow its bond; at FriendBond it is a friend. A befriended dog walks with the
// player (at heel, sitting when they stand, waiting outside where they went in, paddling after them near the shore,
// off home at night); a befriended cat never follows, but comes when the player sits down near its home and sleeps
// on their lap.
//
// Everything here is on top of an animal's Act: FAnimal::Friend says what it does with the player, and while it is
// not None the rest of its life (needs, other animals, birds) waits.
#include "AnimalSim.h"

#include <algorithm>
#include <cmath>

namespace LyudAnimals
{
	// (names of their own: a unity build puts this file next to AnimalSim.cpp)
	static constexpr float FrPi = 3.14159265f;
	static float FrClamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
	static float FrClamp01(float V) { return FrClamp(V, 0.f, 1.f); }
	static float FrWrap(float A)
	{
		while (A > FrPi) A -= 2.f * FrPi;
		while (A < -FrPi) A += 2.f * FrPi;
		return A;
	}
	static V3 FrDir(float Yaw) { return V3(std::cos(Yaw), std::sin(Yaw), 0.f); }
	static float FrYaw(const V3& D) { return std::atan2(D.Y, D.X); }
	static bool FrCat(const FAnimal& A) { return A.Kind == EKind::Cat; }
	static bool FrWalking(EFriend F) { return F == EFriend::Heel || F == EFriend::Wait || F == EFriend::SwimAfter || F == EFriend::BySeat; }
	/** Gone, or in no state to be asked anything. */
	static bool FrAway(const FAnimal& A)
	{
		return A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch || A.Act == EAct::Pounce || A.Act == EAct::Chase
			|| A.Act == EAct::Hiss || (A.Up > 0.f && A.Friend != EFriend::OnLap) || A.Fear > 0.4f;
	}

	// how long things take, s
	static constexpr float SniffLen = 2.4f, StrokeLen = 3.4f, RollLen = 5.f, EatLen = 2.8f, HopLen = 0.5f;
	// how near the player must be, m (from their feet to the animal)
	static constexpr float HandReach = 2.6f, StrokeReach = 1.7f;

	const char* FriendName(EFriend F)
	{
		static const char* N[] = { "", "ToHand", "AtHand", "Stroked", "Rolled", "ToFood", "EatHand", "Heel", "Wait", "SwimAfter", "BySeat", "ToLap",
			"OnLap", "GoHome" };
		static_assert(sizeof(N) / sizeof(N[0]) == (size_t)EFriend::Num, "FriendName");
		return (int)F < (int)EFriend::Num ? N[(int)F] : "?";
	}

	// --- a ground for tests -------------------------------------------------------------------------

	void FWorld::MakeGrid(int NX, int NY, float X0, float Y0, float Cell, float Z)
	{
		Grid = FGrid();
		Grid.NX = NX;
		Grid.NY = NY;
		Grid.X0 = X0;
		Grid.Y0 = Y0;
		Grid.Cell = Cell;
		Grid.Z.assign((size_t)NX * NY, (int16_t)std::lround(Z * 100.f));
		Grid.Kind.assign((size_t)NX * NY, (uint8_t)EGround::Grass);
		BuildCoarse();
		HashNX = 0;
	}

	void FWorld::SetGround(float X, float Y, EGround Kind, float Z)
	{
		const int I = (int)std::lround((X - Grid.X0) / Grid.Cell);
		const int J = (int)std::lround((Y - Grid.Y0) / Grid.Cell);
		if (I < 0 || J < 0 || I >= Grid.NX || J >= Grid.NY)
		{
			return;
		}
		Grid.Kind[(size_t)J * Grid.NX + I] = (uint8_t)Kind;
		Grid.Z[(size_t)J * Grid.NX + I] = (int16_t)std::lround(Z * 100.f);
		BuildCoarse();
	}

	// --- helpers ------------------------------------------------------------------------------------

	bool FWorld::PlayerOnGround() const
	{
		return Player.bPresent && !Player.bSwimming && Walkable(Player.Feet.X, Player.Feet.Y);
	}

	void FWorld::AddBond(FAnimal& A, float D)
	{
		const bool bWas = A.Bond >= FriendBond;
		A.Bond = FrClamp01(A.Bond + D);
		if (!bWas && A.Bond >= FriendBond)
		{
			Emit(EEvent::Befriended, A);
		}
	}

	/** The friend state ends: it goes on with Then for Len s, still minding the player. */
	void FWorld::FriendEnd(FAnimal& A, EAct Then, float Len)
	{
		if (A.Id == Walker && FrWalking(A.Friend))
		{
			Walker = -1;
		}
		if (A.Id == LapCat)
		{
			LapCat = -1;
		}
		A.Friend = EFriend::None;
		A.FriendTime = 0.f;
		A.Path.clear();
		A.PathI = 0;
		A.Speed = std::min(A.Speed, 0.3f);
		A.Up = 0.f;
		A.Goal = EGoal::Person;
		A.Target = -1;
		SetAct(A, Then, Len);
	}

	/** Turns and steps A toward To at up to Speed m/s (true once there). bAnyGround: over what the grid calls blocked too
	 *  (the water). The height is the caller's. */
	bool FWorld::Steer(FAnimal& A, const V3& To, float Speed, float Dt, bool bAnyGround)
	{
		const V3 D = (To - A.Pos).Flat();
		const float L = D.Len2D();
		float Target = std::min(Speed, 0.25f + L * 1.8f);     // slows into the spot
		if (L < 0.1f)
		{
			Target = 0.f;
		}
		if (L > 0.03f)
		{
			float Yaw = FrYaw(D);
			if (!bAnyGround)
			{
				// what is right ahead: blocked ground makes it turn along the wall
				const float Look = std::max(0.5f, A.Speed * 0.3f);
				const V3 Ahead = A.Pos + FrDir(Yaw) * std::min(Look, L);
				if (!Walkable(Ahead.X, Ahead.Y))
				{
					bool bFound = false;
					for (int K = 1; K <= 5 && !bFound; K++)
					{
						for (int S = -1; S <= 1; S += 2)
						{
							const float Y2 = Yaw + S * K * 0.45f;
							const V3 P = A.Pos + FrDir(Y2) * Look;
							if (Walkable(P.X, P.Y))
							{
								Yaw = Y2;
								bFound = true;
								break;
							}
						}
					}
					if (!bFound)
					{
						Target = 0.f;
					}
				}
			}
			const float Off = FrWrap(Yaw - A.Yaw);
			const float MaxTurn = (A.Speed > 3.f ? 5.f : 4.f) * Dt;
			const float Turn = FrClamp(Off, -MaxTurn, MaxTurn);
			A.YawRate = Turn / std::max(Dt, 1e-4f);
			A.Yaw = FrWrap(A.Yaw + Turn);
			Target *= FrClamp01(1.3f - std::fabs(Off) / FrPi * 1.6f);       // a sharp turn slows it down
		}
		else
		{
			A.YawRate = 0.f;
		}
		const float Brake = A.Speed > 3.f ? 14.f : 7.f;
		A.Speed += FrClamp(Target - A.Speed, -Brake * Dt, 4.5f * Dt);
		if (A.Speed > 0.01f)
		{
			const V3 P = A.Pos + FrDir(A.Yaw) * std::min(A.Speed * Dt, std::max(L, 0.02f));
			if (bAnyGround || Walkable(P.X, P.Y))
			{
				A.Pos.X = P.X;
				A.Pos.Y = P.Y;
			}
			else
			{
				A.Speed *= 0.3f;
			}
		}
		A.Vel = FrDir(A.Yaw) * A.Speed;
		return L < 0.22f;
	}

	/** The point to steer at on the way to To: To itself when the line is clear, else along a route (looked for again once a
	 *  second). bNoWay: there is none. */
	V3 FWorld::Route(FAnimal& A, const V3& To, float Dt, bool& bNoWay)
	{
		bNoWay = false;
		if (LineWalkable(A.Pos, To))
		{
			A.Path.clear();
			A.Repath = 0.f;
			return To;
		}
		A.Repath -= Dt;
		if (A.Repath <= 0.f || A.Path.empty())
		{
			A.Repath = 1.f;
			if (!FindPath(A.Pos, To, A.Kind, A.Path))
			{
				A.Path.clear();
			}
			A.PathI = 0;
		}
		if (A.Path.empty())
		{
			bNoWay = true;
			return A.Pos;
		}
		while (A.PathI + 1 < (int)A.Path.size() && (A.Path[A.PathI] - A.Pos).Len2D() < 0.9f)
		{
			A.PathI++;
		}
		return A.Path[std::min(A.PathI, (int)A.Path.size() - 1)];
	}

	// --- what the player asks -----------------------------------------------------------------------

	bool FWorld::OfferHand(int Id)
	{
		if (Id < 0 || Id >= (int)Animals.size() || !Player.bPresent)
		{
			return false;
		}
		FAnimal& A = Animals[Id];
		if (FrAway(A) || FrWalking(A.Friend) || A.Friend == EFriend::OnLap || (A.Pos - Player.Feet).Len2D() > HandReach)
		{
			return false;
		}
		if (A.Friend != EFriend::None)
		{
			return true;        // already at the hand
		}
		// a friend knows the hand; a stranger comes to it only if it trusts the player some (a friendly one sooner)
		const float Need = A.Bond >= FriendBond ? 0.f : (FrCat(A) ? 0.34f - 0.26f * A.Friendly : 0.16f - 0.17f * A.Friendly);
		A.Look = Player.Palm;
		A.bLook = true;
		if (A.Trust < Need)
		{
			// not yet: it watches the hand from where it is; a shy one moves off a little. Patience pays.
			A.Trust = std::min(1.f, A.Trust + 0.015f);
			A.Path.clear();
			A.Goal = EGoal::Person;
			if (A.Act == EAct::Sleep)
			{
				return false;
			}
			if (FrCat(A) && A.Trust < Need * 0.5f && (A.Pos - Player.Feet).Len2D() < 1.5f)
			{
				GiveWay(A, Player.Feet);
			}
			else
			{
				SetAct(A, EAct::Watch, 2.5f);
			}
			Emit(EEvent::HandRefuse, A);
			return false;
		}
		A.Friend = EFriend::ToHand;
		A.FriendTime = 0.f;
		A.OfferLike = -1.f;
		A.Path.clear();
		A.Goal = EGoal::Person;
		A.Target = -1;
		SetAct(A, EAct::Approach, 0.f);
		return true;
	}

	bool FWorld::Stroke(int Id)
	{
		if (Id < 0 || Id >= (int)Animals.size() || !Player.bPresent)
		{
			return false;
		}
		FAnimal& A = Animals[Id];
		const bool bOurs = A.Bond >= FriendBond;
		if (FrAway(A) || (A.Pos - Player.Feet).Len2D() > StrokeReach || (!bOurs && A.Sniffed <= 0.f))
		{
			return false;
		}
		if (A.Friend == EFriend::Rolled)
		{
			// a belly rub: it stays over a little longer
			A.FriendTime = std::min(A.FriendTime, RollLen - 2.5f);
			AddBond(A, 0.02f);
			return true;
		}
		if (A.Friend == EFriend::Stroked)
		{
			return true;
		}
		if (A.Friend != EFriend::None && A.Friend != EFriend::AtHand && !FrWalking(A.Friend) && A.Friend != EFriend::OnLap)
		{
			return false;
		}
		if (FrCat(A) && A.PetFill >= 1.f && A.Friend != EFriend::OnLap)
		{
			return false;       // it has had enough for now
		}
		// (a dog at heel, a cat on the lap: stroked where they are, and they go back to it after)
		if (A.Friend == EFriend::OnLap || FrWalking(A.Friend))
		{
			A.Lean = std::max(A.Lean, 0.3f);
			A.PetFill = std::min(1.f, A.PetFill + 0.05f);
			AddBond(A, 0.01f);
			A.Excite = std::max(A.Excite, FrCat(A) ? 0.f : 0.7f);
			A.PurrTimer = 0.f;
			A.Still = std::min(A.Still, 3.f);
			Emit(EEvent::Stroked, A);
			return true;
		}
		A.Friend = EFriend::Stroked;
		A.FriendTime = 0.f;
		A.PurrTimer = 0.f;
		A.Path.clear();
		A.Speed = 0.f;
		A.Goal = EGoal::Person;
		SetAct(A, EAct::Petted, StrokeLen);
		Emit(EEvent::Stroked, A);
		return true;
	}

	void FWorld::EndStroke(int Id)
	{
		if (Id < 0 || Id >= (int)Animals.size())
		{
			return;
		}
		FAnimal& A = Animals[Id];
		if (A.Friend == EFriend::Stroked)
		{
			A.FriendTime = std::max(A.FriendTime, StrokeLen - 0.3f);
		}
	}

	bool FWorld::OfferFood(int Id, float Like)
	{
		if (Id < 0 || Id >= (int)Animals.size() || !Player.bPresent || !Player.bOffer)
		{
			return false;
		}
		FAnimal& A = Animals[Id];
		if (FrAway(A) || A.Friend == EFriend::OnLap || A.Friend == EFriend::SwimAfter || (A.Pos - Player.Feet).Len2D() > 6.f)
		{
			return false;
		}
		if (A.Friend == EFriend::ToFood || A.Friend == EFriend::EatHand)
		{
			return true;
		}
		const float Need = (A.Bond >= FriendBond ? 0.f : (FrCat(A) ? 0.34f - 0.26f * A.Friendly : 0.16f - 0.17f * A.Friendly)) * 0.5f;
		A.Look = Player.Palm;
		A.bLook = true;
		if (A.Trust < Need)
		{
			A.Trust = std::min(1.f, A.Trust + 0.02f);
			if (A.Act != EAct::Sleep)
			{
				A.Path.clear();
				A.Goal = EGoal::Person;
				SetAct(A, EAct::Watch, 3.f);
			}
			Emit(EEvent::HandRefuse, A);
			return false;
		}
		A.Friend = EFriend::ToFood;
		A.FriendTime = 0.f;
		A.OfferLike = FrClamp01(Like);
		A.Path.clear();
		A.Goal = EGoal::Person;
		A.Target = -1;
		SetAct(A, EAct::Approach, 0.f);
		return true;
	}

	bool FWorld::StartWalk(int Id)
	{
		if (Id < 0 || Id >= (int)Animals.size() || !Player.bPresent)
		{
			return false;
		}
		FAnimal& A = Animals[Id];
		const bool bNight = Hour >= BedHour || Hour < UpHour;
		if (FrCat(A) || A.Bond < FriendBond || FrAway(A) || bNight)
		{
			return false;
		}
		if (Walker == Id)
		{
			return true;
		}
		if (Walker >= 0)
		{
			EndWalk(false);
		}
		Walker = Id;
		A.Friend = EFriend::Heel;
		A.FriendTime = 0.f;
		A.Still = 0.f;
		A.Unreach = 0.f;
		A.bSaidWait = false;
		A.Path.clear();
		A.Goal = EGoal::Follow;
		A.Target = -1;
		A.Excite = 0.9f;
		SetAct(A, EAct::Follow, 0.f);
		Emit(EEvent::WalkStart, A);
		return true;
	}

	void FWorld::EndWalk(bool bHome)
	{
		if (Walker < 0 || Walker >= (int)Animals.size())
		{
			Walker = -1;
			return;
		}
		FAnimal& A = Animals[Walker];
		Walker = -1;
		A.Path.clear();
		A.Up = 0.f;
		if (!Walkable(A.Pos.X, A.Pos.Y))
		{
			A.Pos = A.WaitPos;      // out of the water where it went in
		}
		Emit(EEvent::WalkEnd, A);
		if (!bHome)
		{
			FriendEnd(A, EAct::Sit, 4.f);
			return;
		}
		A.Friend = EFriend::GoHome;
		A.FriendTime = 0.f;
		GoTo(A, A.HomePos, (A.HomePos - A.Pos).Len2D() > 40.f ? EAct::Trot : EAct::Walk, EGoal::Sleep, A.Home);
	}

	// --- every step ---------------------------------------------------------------------------------

	void FWorld::FriendStep(float)
	{
		if (PlayerOnGround())
		{
			PlayerLastOk = Player.Feet;
			bPlayerOk = true;
		}
		if (Walker >= 0 && (Walker >= (int)Animals.size() || !FrWalking(Animals[Walker].Friend)))
		{
			Walker = -1;
		}
		if (LapCat >= 0 && (LapCat >= (int)Animals.size() || (Animals[LapCat].Friend != EFriend::ToLap && Animals[LapCat].Friend != EFriend::OnLap)))
		{
			LapCat = -1;
		}
	}

	void FWorld::FriendNeeds(FAnimal& A, float Dt)
	{
		A.Sniffed = std::max(0.f, A.Sniffed - Dt);
		A.PetFill = std::max(0.f, A.PetFill - Dt / (FrCat(A) ? 240.f : 60.f));
		A.FoodFill = std::max(0.f, A.FoodFill - Dt / 2400.f);
		A.PurrTimer = std::max(0.f, A.PurrTimer - Dt);
		// an old friend's bond keeps growing a little in calm company
		if (Player.bPresent && A.Bond >= FriendBond && A.Bond < 1.f && A.Fear < 0.1f && (A.Pos - Player.Feet).Len2D() < 5.f)
		{
			A.Bond = std::min(1.f, A.Bond + Dt * 0.0004f);
		}
	}

	bool FWorld::FriendThink(FAnimal& A, float Dt)
	{
		const bool bCat = FrCat(A);
		if (A.Friend == EFriend::None)
		{
			// a befriended cat comes to the player sitting near its home, and onto their lap
			if (bCat && Player.bPresent && Player.bSeated && LapCat < 0 && A.Bond >= FriendBond && !FrAway(A) && A.Act != EAct::Eat
				&& A.PurrTimer <= 0.f && (A.Pos - Player.Lap).Len2D() < 30.f && (Player.Lap - A.HomePos).Len2D() < A.Range + 20.f
				&& Rand(A.Rng) < std::min(Dt, 1.f) * 0.35f)
			{
				LapCat = A.Id;
				A.Friend = EFriend::ToLap;
				A.FriendTime = 0.f;
				A.Path.clear();
				A.Goal = EGoal::Person;
				A.Target = -1;
				SetAct(A, EAct::Walk, 0.f);
				Emit(EEvent::Meow, A);
				return true;
			}
			return false;
		}
		// frightened off (a bang, a dog after it): the state is dropped, the fright goes on
		if (A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch)
		{
			if (A.Id == Walker)
			{
				Walker = -1;
			}
			if (A.Id == LapCat)
			{
				LapCat = -1;
			}
			A.Friend = EFriend::None;
			return false;
		}
		if (!Player.bPresent)
		{
			if (A.Friend == EFriend::GoHome)
			{
				return true;
			}
			if (A.Id == Walker)
			{
				return true;        // the player is somebody else for now (a bird): it waits where it is
			}
			FriendEnd(A, EAct::Stand, 2.f);
			return true;
		}
		const float Dist = (A.Pos - Player.Feet).Len2D();
		const float Speed = Player.Vel.Len2D();
		// a bang, or the player rushing at one that is not yet a friend
		if (Player.ExtraThreat > 0.6f && A.Bond < FriendBond && A.Friend != EFriend::GoHome)
		{
			FriendEnd(A, EAct::Stand, 0.5f);
			A.Trust = std::max(0.f, A.Trust - 0.05f);
			StartFlee(A, Player.Feet, -1);
			return true;
		}
		switch (A.Friend)
		{
		case EFriend::ToHand:
		case EFriend::ToFood:
			if (A.FriendTime > 14.f || Dist > 8.f || Speed > 2.4f || (A.Friend == EFriend::ToFood && !Player.bOffer))
			{
				FriendEnd(A, EAct::Watch, 2.5f);
				A.Look = Player.Eye;
				A.bLook = true;
			}
			return true;
		case EFriend::AtHand:
			A.Look = Player.Palm;
			A.bLook = true;
			if (Dist > HandReach + 0.6f)
			{
				FriendEnd(A, EAct::Watch, 2.f);
			}
			else if (A.FriendTime > SniffLen)
			{
				if (A.OfferLike >= 0.f)
				{
					// it came for the food, sniffed it, and does not eat that
					Emit(EEvent::HandRefuse, A);
					FriendEnd(A, bCat ? EAct::Sit : EAct::Stand, 4.f);
					A.bLook = false;
					A.Cooldown = std::max(A.Cooldown, 20.f);
					return true;
				}
				A.Sniffed = 45.f;
				A.Trust = std::min(1.f, A.Trust + 0.05f);
				if (A.Bond < 0.15f)
				{
					AddBond(A, 0.03f);
				}
				FriendEnd(A, bCat ? EAct::Sit : EAct::Stand, 12.f);     // it stays by the hand: now it can be stroked
				A.Look = Player.Eye;
				A.bLook = true;
			}
			return true;
		case EFriend::Stroked:
		{
			A.bLook = false;
			A.Excite = std::max(A.Excite, bCat ? 0.f : 0.75f);        // a dog's tail goes
			// the hand grows the bond: a friendly animal's faster, a dog's faster than a cat's
			AddBond(A, Dt * (bCat ? 0.022f : 0.03f) * (0.6f + 0.6f * A.Friendly));
			A.Trust = std::min(1.f, A.Trust + Dt * 0.01f);
			A.Lonely = std::max(0.f, A.Lonely - Dt * 0.05f);
			if (bCat && A.PurrTimer <= 0.f)
			{
				A.PurrTimer = 5.f;
				Emit(EEvent::Purr, A);
			}
			if (Dist > StrokeReach + 0.4f)
			{
				FriendEnd(A, bCat ? EAct::Sit : EAct::Stand, 3.f);
				return true;
			}
			if (A.FriendTime > StrokeLen)
			{
				A.PetFill = std::min(1.f, A.PetFill + (bCat ? 0.34f : 0.12f));
				A.Sniffed = std::max(A.Sniffed, 30.f);
				// it may roll over, belly up: a cat that trusts the hand, a dog that is a good friend
				const bool bRoll = bCat ? (A.Bond > 0.3f && A.Trust > 0.5f && Rand(A.Rng) < 0.35f) : (A.Bond > 0.6f && Rand(A.Rng) < 0.3f);
				if (bRoll)
				{
					A.Friend = EFriend::Rolled;
					A.FriendTime = 0.f;
					SetAct(A, EAct::RollOver, RollLen);
					Emit(EEvent::Roll, A);
				}
				else if (bCat && A.PetFill >= 1.f)
				{
					// enough: it gets up and walks off a little way, and washes
					Emit(EEvent::Enough, A);
					A.PetFill = 1.5f;       // and will not be stroked again for a couple of minutes
					FriendEnd(A, EAct::Groom, 8.f);
					A.Cooldown = std::max(A.Cooldown, 60.f);
					{
						// a few steps off, no further
						const V3 Away = (A.Pos - Player.Feet).Flat();
						const V3 To = A.Pos + (Away.Len2D() > 0.05f ? Away.Norm() : FrDir(A.Yaw)) * 3.f;
						if (LineWalkable(A.Pos, To))
						{
							A.Path.assign(1, V3(To.X, To.Y, GroundZ(To.X, To.Y)));
							A.PathI = 0;
							A.GoalPos = To;
							A.Goal = EGoal::Rest;
							A.GoalPlace = -1;
							SetAct(A, EAct::Walk, 0.f);
						}
					}
				}
				else
				{
					FriendEnd(A, EAct::Sit, 10.f);
					A.Look = Player.Eye;
					A.bLook = true;
				}
			}
			return true;
		}
		case EFriend::Rolled:
			A.bLook = false;
			AddBond(A, Dt * 0.012f);
			if (bCat && A.PurrTimer <= 0.f)
			{
				A.PurrTimer = 5.f;
				Emit(EEvent::Purr, A);
			}
			if (A.FriendTime > RollLen || Dist > StrokeReach + 0.6f)
			{
				if (bCat && A.PetFill >= 1.f)
				{
					Emit(EEvent::Enough, A);
					A.PetFill = 1.5f;
					FriendEnd(A, EAct::Groom, 6.f);
				}
				else
				{
					FriendEnd(A, EAct::Sit, 8.f);
				}
			}
			return true;
		case EFriend::EatHand:
			A.Look = Player.Palm;
			A.bLook = true;
			if (!Player.bOffer || Dist > HandReach + 0.6f)
			{
				FriendEnd(A, EAct::Watch, 2.f);     // the hand went away before it had done
			}
			else if (A.FriendTime > EatLen)
			{
				Emit(EEvent::Ate, A);
				A.Meals++;
				A.Hunger = std::max(0.f, A.Hunger - 0.35f * A.OfferLike);
				A.Trust = std::min(1.f, A.Trust + 0.12f * A.OfferLike);
				// the first treats count for most: a full animal is glad of it and no more
				AddBond(A, 0.16f * A.OfferLike * FrClamp01(1.2f - A.FoodFill));
				A.FoodFill = std::min(1.f, A.FoodFill + 0.4f);
				A.Sniffed = 60.f;
				FriendEnd(A, bCat ? EAct::Groom : EAct::Sit, bCat ? 5.f : 8.f);
				A.Look = Player.Eye;
				A.bLook = !bCat;
			}
			return true;
		case EFriend::Heel:
		case EFriend::Wait:
		case EFriend::SwimAfter:
		case EFriend::BySeat:
			A.Lonely = 0.f;
			A.Curious = std::max(0.f, A.Curious - Dt * 0.01f);
			// the day is over: off home to the door she sleeps by
			if ((Hour >= BedHour || Hour < UpHour) && A.Friend != EFriend::SwimAfter)
			{
				EndWalk(true);
			}
			return true;
		case EFriend::ToLap:
			if (!Player.bSeated || A.FriendTime > 40.f)
			{
				FriendEnd(A, EAct::Sit, 4.f);
				A.PurrTimer = 60.f;         // not again at once
			}
			return true;
		case EFriend::OnLap:
			A.Tired = std::max(0.f, A.Tired - Dt / 3600.f);
			A.Lonely = 0.f;
			if (A.Bond < 1.f)
			{
				A.Bond = std::min(1.f, A.Bond + Dt * 0.001f);
			}
			if (A.PurrTimer <= 0.f)
			{
				A.PurrTimer = 9.f;
				Emit(EEvent::Purr, A);
			}
			if (!Player.bSeated)
			{
				// the player gets up: it hops down beside them, and washes
				const V3 Down = FreeSpot(Player.Feet + FrDir(Player.SeatYaw) * 0.5f, 0.6f, A.Rng);
				A.Pos = V3(Down.X, Down.Y, GroundZ(Down.X, Down.Y));
				Emit(EEvent::LapDown, A);
				FriendEnd(A, EAct::Groom, 5.f);
				A.PurrTimer = 90.f;
			}
			return true;
		case EFriend::GoHome:
			if (!IsMoving(A.Act))
			{
				if ((A.Pos - A.HomePos).Len2D() < 5.f)
				{
					A.Friend = EFriend::None;       // Arrive() has laid her down by her door
				}
				else if ((A.Pos - Camera).Len() > 35.f || A.FriendTime > 25.f)
				{
					// no way home on foot from here (the far side of a cable-car ride): she is there when nobody looks
					A.Friend = EFriend::None;
					A.Pos = A.HomePos;
					A.Pos.Z = GroundZ(A.Pos.X, A.Pos.Y);
					A.Goal = EGoal::Sleep;
					SetAct(A, EAct::Rest, 600.f);
				}
				else
				{
					A.Look = Player.Eye;
					A.bLook = true;
				}
			}
			return true;
		default:
			return true;
		}
	}

	bool FWorld::FriendMove(FAnimal& A, float Dt)
	{
		// the lean into the hand eases in and out
		const float LeanTo = A.Friend == EFriend::Stroked || A.Friend == EFriend::Rolled ? 1.f : 0.f;
		if (A.Lean != LeanTo)
		{
			A.Lean += FrClamp(LeanTo - A.Lean, -Dt * 2.f, Dt * 2.5f);
		}
		if (A.Friend == EFriend::None || A.Friend == EFriend::GoHome || A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch)
		{
			return false;       // GoHome walks its route like anybody
		}
		A.FriendTime += Dt;
		const bool bCat = FrCat(A);
		const V3 Feet = Player.Feet;
		auto Ground = [&]() { A.Up = 0.f; A.Pos.Z = ZAt(A); };
		auto Stay = [&]()
		{
			A.Speed = std::max(0.f, A.Speed - 8.f * Dt);
			A.Vel = FrDir(A.Yaw) * A.Speed;
			A.YawRate = 0.f;
		};
		auto TurnTo = [&](float Yaw, float Rate)
		{
			const float Turn = FrClamp(FrWrap(Yaw - A.Yaw), -Rate * Dt, Rate * Dt);
			A.YawRate = Turn / std::max(Dt, 1e-4f);
			A.Yaw = FrWrap(A.Yaw + Turn);
		};
		auto Become = [&](EAct Act)
		{
			if (A.Act != Act)
			{
				SetAct(A, Act, 0.f);
			}
		};
		if (!Player.bPresent)
		{
			Stay();
			Ground();
			return true;
		}

		switch (A.Friend)
		{
		case EFriend::ToHand:
		case EFriend::ToFood:
		{
			// to where its nose is at the palm
			const V3 From = (A.Pos - Player.Palm).Flat();
			const V3 Spot = Player.Palm + (From.Len2D() > 0.05f ? From.Norm() : FrDir(A.Yaw + FrPi)) * (bCat ? 0.24f : 0.44f);
			A.Look = Player.Palm;
			A.bLook = true;
			Become(EAct::Approach);
			bool bNoWay = false;
			const V3 Want = Route(A, V3(Spot.X, Spot.Y, 0.f), Dt, bNoWay);
			const float Gait = GaitSpeed(A, EAct::Approach) * (A.Bond >= FriendBond ? 1.6f : 1.f) * (A.Friend == EFriend::ToFood ? 1.3f : 1.f);
			const bool bThere = !bNoWay && Steer(A, Want, Gait, Dt) && (V3(Spot.X, Spot.Y, 0.f) - A.Pos.Flat()).Len2D() < 0.3f;
			Ground();
			if (bThere)
			{
				A.Speed = 0.f;
				A.Yaw = FrYaw(Player.Palm - A.Pos);
				const bool bEat = A.Friend == EFriend::ToFood && A.OfferLike >= 0.15f && A.FoodFill < 1.f;
				A.Friend = bEat ? EFriend::EatHand : EFriend::AtHand;
				A.FriendTime = 0.f;
				SetAct(A, bEat ? EAct::Eat : EAct::SniffHand, bEat ? EatLen : SniffLen);
				if (!bEat)
				{
					Emit(EEvent::HandSniff, A);
				}
			}
			return true;
		}
		case EFriend::AtHand:
		case EFriend::EatHand:
			Stay();
			TurnTo(FrYaw(Player.Palm - A.Pos), 2.f);
			Ground();
			return true;
		case EFriend::Stroked:
		case EFriend::Rolled:
			Stay();
			Ground();
			return true;
		case EFriend::Heel:
		{
			if (Player.bSwimming)
			{
				if ((A.Pos - Feet).Len2D() < SwimReach + 10.f)
				{
					A.Friend = EFriend::SwimAfter;
					A.FriendTime = 0.f;
					A.WaitPos = A.Pos;
					A.bSaidWait = false;
					A.Path.clear();
					return true;
				}
			}
			if (!PlayerOnGround())
			{
				// gone where she can't (a door, a cabin, the water far out): she waits where they went in
				A.Friend = EFriend::Wait;
				A.FriendTime = 0.f;
				A.Still = 0.f;
				A.bSaidWait = false;
				const V3 Near = bPlayerOk ? PlayerLastOk : A.Pos;
				// a step to the side of the way in, not on the threshold
				const V3 In = (Feet - Near).Flat().Norm();
				V3 Spot = Near + V3(-In.Y, In.X, 0.f) * 1.1f - In * 0.5f;
				if (!Walkable(Spot.X, Spot.Y) || !LineWalkable(Near, Spot))
				{
					Spot = Near + V3(In.Y, -In.X, 0.f) * 1.1f - In * 0.5f;
				}
				if (!Walkable(Spot.X, Spot.Y) || !LineWalkable(Near, Spot))
				{
					Spot = Near;
				}
				A.WaitPos = Spot;
				A.Path.clear();
				return true;
			}
			if (Player.bSeated)
			{
				A.Friend = EFriend::BySeat;
				A.FriendTime = 0.f;
				A.Still = 0.f;
				A.Path.clear();
				return true;
			}
			const float PSpeed = Player.Vel.Len2D();
			const V3 Fwd = PSpeed > 0.4f ? Player.Vel.Flat().Norm() : Player.Facing.Flat().Norm();
			const V3 Left(-Fwd.Y, Fwd.X, 0.f);
			// at the left heel; where there is no room, behind
			V3 T = Feet + Left * 0.75f - Fwd * 0.2f;
			if (!Walkable(T.X, T.Y) || !LineWalkable(Feet, T))
			{
				T = Feet - Fwd * 0.85f;
				if (!Walkable(T.X, T.Y))
				{
					T = Feet;
				}
			}
			const float D = (T - A.Pos).Len2D();
			A.Still = PSpeed < 0.25f ? A.Still + Dt : 0.f;
			const bool bSat = A.Act == EAct::Sit || A.Act == EAct::Rest;
			if ((A.Still > 1.2f && D < 1.3f) || (bSat && PSpeed < 0.4f && D < 2.4f))
			{
				// the player stands: she sits by them looking up, and lies down if it is a long stop
				Stay();
				Become(A.Still > 25.f ? EAct::Rest : EAct::Sit);
				TurnTo(FrYaw(Feet - A.Pos) + (A.Still > 25.f ? 1.2f : 0.f), 2.5f);
				A.Look = Player.Eye;
				A.bLook = A.Still < 25.f;
				A.Unreach = 0.f;
				Ground();
				return true;
			}
			bool bNoWay = false;
			const V3 Want = Route(A, T, Dt, bNoWay);
			// her pace is the player's, plus what it takes to close the gap; a long gap is run down
			float V = PSpeed + (D - 0.35f) * 1.5f;
			if (D > 7.f)
			{
				V = std::max(V, GaitSpeed(A, EAct::Trot) * 1.3f);
			}
			V = FrClamp(V, 0.f, GaitSpeed(A, EAct::Run));
			Become(EAct::Follow);
			A.bLook = false;
			if (bNoWay)
			{
				Stay();
			}
			else
			{
				Steer(A, Want, V, Dt);
			}
			Ground();
			// out of reach (the far side of a wall with no way round, a ride): she catches up from behind, unseen
			A.Unreach = (bNoWay || D > 45.f) ? A.Unreach + Dt : (D < 15.f ? 0.f : A.Unreach);
			if (A.Unreach > 5.f || D > 90.f)
			{
				for (int K = 0; K < 16; K++)
				{
					const float Ang = FrYaw(Fwd) + FrPi + (Rand(A.Rng) - 0.5f) * 1.6f;
					const V3 P = Feet + FrDir(Ang) * (9.f + 7.f * Rand(A.Rng));
					if (Walkable(P.X, P.Y) && LineWalkable(P, Feet))
					{
						A.Pos = V3(P.X, P.Y, GroundZ(P.X, P.Y));
						A.Yaw = FrYaw(Feet - A.Pos);
						A.Speed = GaitSpeed(A, EAct::Trot);
						A.Path.clear();
						A.Unreach = 0.f;
						A.Excite = 0.9f;
						Emit(EEvent::Rejoin, A);
						break;
					}
				}
				A.Unreach = std::min(A.Unreach, 4.f);       // tries again in a second
			}
			return true;
		}
		case EFriend::Wait:
		{
			if (PlayerOnGround())
			{
				// out again: up and to them, glad
				A.Friend = EFriend::Heel;
				A.FriendTime = 0.f;
				A.Still = 0.f;
				A.Unreach = 0.f;
				A.Excite = 1.f;
				if (A.bSaidWait)
				{
					Emit(EEvent::Rejoin, A);
				}
				return true;
			}
			if (Player.bSwimming && (A.Pos - Feet).Len2D() < SwimReach)
			{
				A.Friend = EFriend::SwimAfter;
				A.FriendTime = 0.f;
				A.WaitPos = A.Pos;
				A.bSaidWait = false;
				return true;
			}
			const float D = (A.WaitPos - A.Pos).Len2D();
			if (D > 0.45f && A.Still <= 0.f)
			{
				bool bNoWay = false;
				const V3 Want = Route(A, A.WaitPos, Dt, bNoWay);
				Become(EAct::Follow);
				if (bNoWay)
				{
					A.WaitPos = A.Pos;
				}
				else
				{
					Steer(A, Want, GaitSpeed(A, D > 8.f ? EAct::Trot : EAct::Walk), Dt);
				}
				Ground();
				return true;
			}
			// sits facing where they went, then lies down
			A.Still += Dt;
			Stay();
			Become(A.Still > 12.f ? EAct::Rest : EAct::Sit);
			TurnTo(FrYaw(Feet - A.Pos), 2.5f);
			A.Look = Feet + V3(0.f, 0.f, 1.2f);
			A.bLook = true;
			if (!A.bSaidWait && A.Still > 1.5f)
			{
				A.bSaidWait = true;
				Emit(EEvent::WaitDoor, A);
			}
			Ground();
			return true;
		}
		case EFriend::SwimAfter:
		{
			const bool bDry = Walkable(A.Pos.X, A.Pos.Y);
			V3 To;
			bool bBack = false;
			if (Player.bSwimming)
			{
				const V3 Side = V3(-Player.Facing.Y, Player.Facing.X, 0.f).Flat().Norm();
				To = Feet + Side * 1.3f;
				if ((To - A.WaitPos).Len2D() > SwimReach)
				{
					To = A.WaitPos;         // too far out: she turns back and waits on the shore
					bBack = true;
				}
			}
			else
			{
				To = PlayerOnGround() ? Feet : A.WaitPos;
				if (bDry)
				{
					A.Friend = PlayerOnGround() ? EFriend::Heel : EFriend::Wait;
					A.FriendTime = 0.f;
					A.Still = 0.f;
					A.Excite = 0.9f;        // out of the water (the body shakes itself in the game)
					if (A.Friend == EFriend::Wait)
					{
						A.WaitPos = A.Pos;
					}
					return true;
				}
			}
			const float Depth = Player.WaterZ - GroundZ(A.Pos.X, A.Pos.Y);
			const bool bDeep = Depth > 0.38f;
			if (bBack && bDry && (To - A.Pos).Len2D() < 1.f)
			{
				Stay();
				Become(EAct::Sit);
				TurnTo(FrYaw(Feet - A.Pos), 2.5f);
				A.Look = Player.Eye;
				A.bLook = true;
				if (!A.bSaidWait)
				{
					A.bSaidWait = true;
					Emit(EEvent::Whine, A);
				}
			}
			else
			{
				Become(bDeep ? EAct::Swim : EAct::Follow);
				A.bLook = false;
				Steer(A, To, bDeep ? 1.15f : GaitSpeed(A, EAct::Trot), Dt, true);
			}
			A.Up = 0.f;
			A.Pos.Z = bDeep ? Player.WaterZ - 0.34f : GroundZ(A.Pos.X, A.Pos.Y);
			return true;
		}
		case EFriend::BySeat:
		{
			if (!Player.bSeated)
			{
				A.Friend = EFriend::Heel;
				A.FriendTime = 0.f;
				A.Still = 0.f;
				return true;
			}
			const V3 F = FrDir(Player.SeatYaw);
			const V3 Right(F.Y, -F.X, 0.f);
			V3 Spot = Player.Lap.Flat() + Right * 0.85f + F * 0.25f;
			if (!Walkable(Spot.X, Spot.Y))
			{
				Spot = Player.Lap.Flat() - Right * 0.85f + F * 0.25f;
			}
			if (!Walkable(Spot.X, Spot.Y))
			{
				Spot = Player.Lap.Flat() + F * 0.9f;
			}
			const float D = (Spot - A.Pos.Flat()).Len2D();
			if (D > 0.4f && A.Still <= 0.f)
			{
				bool bNoWay = false;
				const V3 Want = Route(A, Spot, Dt, bNoWay);
				Become(EAct::Follow);
				if (bNoWay)
				{
					A.Still = 0.01f;
				}
				else
				{
					Steer(A, Want, GaitSpeed(A, EAct::Walk), Dt);
				}
			}
			else
			{
				A.Still += Dt;
				Stay();
				Become(A.Still > 30.f ? EAct::Rest : EAct::Sit);
				TurnTo(Player.SeatYaw, 2.5f);
				A.bLook = false;
			}
			Ground();
			return true;
		}
		case EFriend::ToLap:
		{
			const V3 F = FrDir(Player.SeatYaw);
			const V3 Spot = Player.Lap.Flat() + F * 0.5f;
			bool bNoWay = false;
			const V3 Want = Route(A, Spot, Dt, bNoWay);
			Become(EAct::Walk);
			A.Look = Player.Eye;
			A.bLook = true;
			const bool bThere = !bNoWay && Steer(A, Want, GaitSpeed(A, EAct::Walk), Dt) && (Spot - A.Pos.Flat()).Len2D() < 0.35f;
			Ground();
			if (bThere || (bNoWay && (Spot - A.Pos.Flat()).Len2D() < 1.2f))
			{
				A.Friend = EFriend::OnLap;
				A.FriendTime = 0.f;
				A.WaitPos = A.Pos;
				A.Speed = 0.f;
				A.Vel = V3();
				A.PurrTimer = 1.5f;
				SetAct(A, EAct::Lap, 0.f);
				Emit(EEvent::LapUp, A);
			}
			return true;
		}
		case EFriend::OnLap:
		{
			// a hop up from the ground, then curled across the lap
			const float T = FrClamp01(A.FriendTime / HopLen);
			const float S = T * T * (3.f - 2.f * T);
			const V3 P = A.WaitPos + (Player.Lap - A.WaitPos) * S;
			A.Pos = V3(P.X, P.Y, P.Z + 0.22f * std::sin(T * FrPi));
			A.Up = std::max(0.05f, A.Pos.Z - GroundZ(A.Pos.X, A.Pos.Y));
			A.Speed = 0.f;
			A.Vel = V3();
			A.YawRate = 0.f;
			A.Yaw = T < 1.f ? FrYaw(Player.Lap - A.WaitPos) : Player.SeatYaw + FrPi * 0.5f;
			A.bLook = false;
			return true;
		}
		default:
			return false;
		}
	}
}
