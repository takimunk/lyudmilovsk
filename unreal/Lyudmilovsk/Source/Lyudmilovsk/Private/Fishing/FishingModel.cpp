#include "FishingModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace LyudFishing
{
	namespace
	{
		using D = EDiet;
		// Id, diet, length (m, as the fish model's), max of it, K, fight, shy, take s, roubles a kilo, min price.
		// Lengths and weights after the Baltic coast's catches (FishBase; the Kaliningrad anglers' usual sizes).
		const FCatchable GFish[] = {
			{ "Herring",              D::Plankton, 0.20f,  1.5f, 0.75f, 1.0f, 0.5f, 0.6f, 120, 3 },
			{ "Sprat",                D::Plankton, 0.11f,  1.3f, 0.70f, 0.8f, 0.5f, 0.5f, 100, 1 },
			{ "SandEel",              D::Plankton, 0.16f,  1.3f, 0.35f, 0.8f, 0.4f, 0.5f, 80,  1 },
			{ "Stickleback",          D::Plankton, 0.06f,  1.3f, 1.10f, 0.5f, 0.2f, 0.5f, 0,   0 },
			{ "Perch",                D::Mixed,    0.22f,  1.9f, 1.30f, 1.1f, 0.3f, 0.9f, 220, 10 },
			{ "Cod",                  D::Hunter,   0.50f,  2.2f, 0.95f, 1.0f, 0.3f, 1.2f, 280, 30 },
			{ "SeaTrout",             D::Hunter,   0.55f,  1.8f, 1.10f, 1.8f, 0.2f, 0.9f, 650, 120 },
			{ "Garfish",              D::Hunter,   0.60f,  1.3f, 0.18f, 1.5f, 0.4f, 0.6f, 200, 20 },
			{ "RoundGoby",            D::Bottom,   0.15f,  1.6f, 1.40f, 0.7f, 0.1f, 1.2f, 90,  2 },
			{ "SandGoby",             D::Bottom,   0.065f, 1.3f, 1.00f, 0.4f, 0.2f, 0.8f, 0,   0 },
			{ "Eelpout",              D::Bottom,   0.28f,  1.6f, 0.55f, 0.8f, 0.2f, 1.4f, 150, 8 },
			{ "Sculpin",              D::Bottom,   0.22f,  1.6f, 1.40f, 0.7f, 0.1f, 1.3f, 60,  4 },
			{ "Lumpsucker",           D::Bottom,   0.30f,  1.4f, 2.60f, 0.5f, 0.5f, 1.0f, 120, 20 },
			{ "Flounder",             D::Bottom,   0.30f,  1.6f, 1.25f, 0.9f, 0.6f, 1.0f, 260, 15 },
			{ "Turbot",               D::Hunter,   0.42f,  1.6f, 1.60f, 1.0f, 0.4f, 1.1f, 700, 90 },
			{ "Smelt",                D::Plankton, 0.18f,  1.5f, 0.65f, 0.8f, 0.4f, 0.6f, 250, 4 },
			{ "TwaiteShad",           D::Hunter,   0.38f,  1.3f, 0.95f, 1.4f, 0.4f, 0.7f, 150, 15 },
			{ "Whitefish",            D::Mixed,    0.40f,  1.5f, 0.95f, 1.3f, 0.6f, 0.7f, 450, 40 },
			{ "Salmon",               D::Hunter,   0.75f,  1.6f, 1.05f, 2.0f, 0.3f, 0.9f, 900, 400 },
			{ "Roach",                D::Mixed,    0.22f,  1.6f, 1.20f, 0.9f, 0.7f, 0.6f, 70,  3 },
			{ "Vimba",                D::Mixed,    0.30f,  1.4f, 1.10f, 1.0f, 0.6f, 0.8f, 200, 15 },
			{ "Eel",                  D::Bottom,   0.65f,  1.6f, 0.17f, 1.6f, 0.4f, 1.5f, 900, 80 },
			{ "RockGunnel",           D::Bottom,   0.20f,  1.3f, 0.30f, 0.5f, 0.2f, 1.0f, 0,   0 },
			{ "FourhornSculpin",      D::Bottom,   0.25f,  1.4f, 1.20f, 0.7f, 0.1f, 1.2f, 40,  3 },
			{ "BlackGoby",            D::Bottom,   0.12f,  1.5f, 1.30f, 0.6f, 0.1f, 1.1f, 60,  2 },
			{ "NinespineStickleback", D::Plankton, 0.065f, 1.2f, 1.00f, 0.4f, 0.2f, 0.5f, 0,   0 },
		};
		constexpr int GNumFish = int(sizeof(GFish) / sizeof(GFish[0]));

		// liking per diet: Plankton, Bottom, Mixed, Hunter
		const FBait GBaits[] = {
			// a sea worm (lugworm, ragworm) from the tackle counter: the bottom fish's and the cod's
			{ "bait_worm",   false, { 0.25f, 1.0f, 0.9f, 0.5f } },
			// maggots: the small fish's, roach and perch
			{ "bait_maggot", false, { 0.9f, 0.35f, 1.0f, 0.1f } },
			// a silver spoon, cast and wound in: the hunters'
			{ "lure_spoon",  true,  { 0.15f, 0.03f, 0.45f, 1.0f } },
		};
		constexpr int GNumBaits = int(sizeof(GBaits) / sizeof(GBaits[0]));

		/** A species' own taste where its diet's average is wrong: perch go for a spoon, roach never; garfish and sea trout rarely touch a worm on the bottom. */
		float Override(const char* Fish, const char* Bait)
		{
			struct FO { const char* F; const char* B; float L; };
			static const FO O[] = {
				{ "Perch", "lure_spoon", 1.0f }, { "Roach", "lure_spoon", 0.02f }, { "Vimba", "lure_spoon", 0.02f },
				{ "Whitefish", "lure_spoon", 0.15f }, { "Garfish", "bait_worm", 0.15f }, { "SeaTrout", "bait_worm", 0.3f },
				{ "Salmon", "bait_worm", 0.1f }, { "Cod", "bait_worm", 0.9f }, { "Turbot", "bait_worm", 0.7f },
				{ "Herring", "lure_spoon", 0.15f }, { "Eel", "bait_maggot", 0.1f }, { "Lumpsucker", "bait_worm", 0.15f },
			};
			for (const FO& X : O)
			{
				if (!std::strcmp(X.F, Fish) && !std::strcmp(X.B, Bait)) return X.L;
			}
			return -1.f;
		}

		float Clamp01(float X) { return X < 0.f ? 0.f : (X > 1.f ? 1.f : X); }
		float Approach(float X, float Goal, float Rate, float Dt) { return X + (Goal - X) * (1.f - std::exp(-Rate * Dt)); }
	}

	int NumCatchable() { return GNumFish; }
	const FCatchable& Catchable(int Index) { return GFish[Index]; }
	int FindCatchable(const char* Id)
	{
		for (int I = 0; I < GNumFish; I++)
		{
			if (!std::strcmp(GFish[I].Id, Id)) return I;
		}
		return -1;
	}
	std::string ItemOf(const char* Id)
	{
		std::string S = "fish_";
		for (const char* C = Id; *C; C++) S += char(std::tolower((unsigned char)*C));
		return S;
	}

	int NumBaits() { return GNumBaits; }
	const FBait& Bait(int Index) { return GBaits[Index]; }
	int FindBait(const char* Item)
	{
		for (int I = 0; I < GNumBaits; I++)
		{
			if (!std::strcmp(GBaits[I].Item, Item)) return I;
		}
		return -1;
	}
	float BaitLiking(int Fish, int BaitIndex)
	{
		if (Fish < 0 || Fish >= GNumFish) return 0.f;
		if (BaitIndex < 0 || BaitIndex >= GNumBaits) return 0.03f;      // a bare hook: now and then a goby
		const FCatchable& F = GFish[Fish];
		// a tiny mouth seldom gets round a hook (sand gobies, sticklebacks)
		const float Mouth = std::min(1.f, std::max(0.15f, (F.Length - 0.05f) / 0.07f));
		const float O = Override(F.Id, GBaits[BaitIndex].Item);
		if (O >= 0.f) return O * Mouth;
		return F.Diet == EDiet::None ? 0.f : GBaits[BaitIndex].Like[int(F.Diet)] * Mouth;
	}

	float FRand::Normal()
	{
		const float A = std::max(U(), 1e-6f), B = U();
		return std::sqrt(-2.f * std::log(A)) * std::cos(6.2831853f * B);
	}

	FFish RollFish(int Species, FRand& R)
	{
		FFish F;
		F.Species = Species;
		const FCatchable& C = GFish[Species];
		// mostly near the usual size, some small ones; now and then a big old one
		float M = std::exp(0.2f * R.Normal());
		if (R.U() < 0.04f) M = R.Range(1.2f, C.Max);
		M = std::min(std::max(M, 0.55f), C.Max);
		F.Length = C.Length * M;
		const float Cm = F.Length * 100.f;
		F.Grams = C.K * Cm * Cm * Cm / 100.f;
		const float Hg = F.Grams / 100.f;
		F.Pull = 0.3f * C.Fight * std::pow(Hg, 0.4f);
		F.Endurance = 4.f + 2.5f * C.Fight * std::sqrt(Hg);
		F.RunSpeed = 0.5f + 0.6f * C.Fight * std::pow(Hg, 0.2f);
		return F;
	}

	int PriceOf(int Species, float Grams)
	{
		if (Species < 0 || Species >= GNumFish) return 0;
		const FCatchable& C = GFish[Species];
		if (C.PerKilo <= 0) return 0;
		return std::max(C.MinPrice, int(std::lround(C.PerKilo * Grams / 1000.f)));
	}

	// ------------------------------------------------------------------ the session

	float FSession::MaxCast() const
	{
		return Bait >= 0 && GBaits[Bait].bLure ? Tune.LureCast : Tune.FloatCast;
	}

	float FSession::RichNow(bool bLure) const
	{
		(void)bLure;
		float Sum = 0.f;
		const int B = IsBaited() ? Bait : -1;
		for (int I = 0; I < int(Weights.size()) && I < GNumFish; I++)
		{
			Sum += Weights[I] * BaitLiking(I, B);
		}
		return Sum;
	}

	float FSession::BitesPerMinute() const
	{
		const bool bLure = Bait >= 0 && GBaits[Bait].bLure;
		const float Rich = RichNow(bLure);
		const float Every = bLure ? Tune.LureEvery : Tune.BiteEvery;
		return 60.f * (2.f / Every) * Rich / (Rich + Tune.RichWeight);
	}

	int FSession::PickFish()
	{
		const int B = IsBaited() ? Bait : -1;
		float Sum = 0.f;
		for (int I = 0; I < int(Weights.size()) && I < GNumFish; I++) Sum += Weights[I] * BaitLiking(I, B);
		if (Sum <= 0.f) return -1;
		float X = R.U() * Sum;
		for (int I = 0; I < int(Weights.size()) && I < GNumFish; I++)
		{
			X -= Weights[I] * BaitLiking(I, B);
			if (X <= 0.f) return I;
		}
		return std::min(int(Weights.size()), GNumFish) - 1;
	}

	void FSession::Go(EState S)
	{
		St = S;
		T = 0.f;
	}

	void FSession::SetLanded(bool bWater, float Distance)
	{
		if (St != EState::Flying) return;
		Dist = std::max(0.f, Distance);
		if (bWater)
		{
			Emit(EEvent::Landed);
			Go(EState::Waiting);
			// the splash scares what is close for a moment
			Calm = 2.f;
			NextBite = -1.f;
		}
		else
		{
			Emit(EEvent::Ashore);
			Go(EState::Winding);
		}
	}

	void FSession::StartBite()
	{
		BiteFish = PickFish();
		if (BiteFish < 0) return;
		const FCatchable& C = GFish[BiteFish];
		Nibbles = int(C.Shy * 4.f * R.U() + 0.3f);
		NextNibble = R.Range(0.2f, 0.8f);
		NibbleT = 0.f;
		const bool bLure = Bait >= 0 && GBaits[Bait].bLure;
		if (bLure)
		{
			// a hit on the spoon: no nibbling
			Nibbles = 0;
			NextNibble = 0.f;
		}
		Go(EState::Nibbling);
	}

	void FSession::Hooked()
	{
		Hook = RollFish(BiteFish, R);
		Stam = 1.f;
		bRun = true;
		RunLeft = R.Range(0.6f, 1.6f);
		RestLeft = 0.f;
		OverT = SlackT = 0.f;
		SideGoal = R.Range(-1.f, 1.f);
		Emit(EEvent::Hooked);
		Emit(EEvent::Run);
		Go(EState::Fighting);
	}

	void FSession::FightStep(float Dt, bool bReel)
	{
		// the fish: runs and rests, shorter and weaker as it tires; small fish only wriggle
		if (bRun)
		{
			RunLeft -= Dt;
			if (RunLeft <= 0.f)
			{
				bRun = false;
				RestLeft = R.Range(0.8f, 3.2f) / (0.4f + Stam);
			}
		}
		else
		{
			RestLeft -= Dt;
			if (RestLeft <= 0.f && Stam > 0.08f)
			{
				bRun = true;
				RunLeft = R.Range(0.5f, 2.2f) * (0.3f + Stam);
				SideGoal = std::max(-1.f, std::min(1.f, SideNow + R.Range(-1.2f, 1.2f)));
				Emit(EEvent::Run);
			}
		}
		const float Strength = 0.35f + 0.65f * Stam;
		const float Pull = Hook.Pull * (bRun ? 1.f : 0.25f) * Strength;
		const float Speed = Hook.RunSpeed * (0.4f + 0.6f * Stam);
		float Goal;
		if (bReel)
		{
			// the reel locked and turning: the rod takes the pull and the line comes in when the fish gives
			Goal = Pull + 0.12f;
			const float Give = std::max(0.f, 1.f - Pull / 0.9f);
			Dist -= Tune.FightReel * (bRun ? 0.15f : 1.f) * Give * Dt;
		}
		else
		{
			// the clutch: the rod holds what it can, more goes out as line
			Goal = Pull * 0.8f;
			if (Goal > Tune.ClutchCap)
			{
				Goal = Tune.ClutchCap;
			}
			// a run against the clutch's brake goes slower than a free one
			Dist += (bRun ? Speed * (Goal >= Tune.ClutchCap ? 0.6f : 1.f) : 0.04f) * Dt;
		}
		Tens = Approach(Tens, Goal, bReel ? 9.f : 6.f, Dt);
		SideNow = Approach(SideNow, SideGoal, 1.5f, Dt);
		// tiring: hard running, and the pull of the line
		Stam -= Dt / Hook.Endurance * (bRun ? 1.f + Tens : 0.25f * Tens);
		Stam = Clamp01(Stam);
		// the line breaks
		if (Tens > 1.f)
		{
			OverT += Dt;
			if (OverT > Tune.SnapAfter || Tens > 1.35f)
			{
				Emit(EEvent::Snapped);
				if (Bait >= 0 && GBaits[Bait].bLure) Bait = -1;
				bBaited = false;
				Tens = 0.f;
				Go(EState::Winding);
				Dist = 0.f;
				return;
			}
		}
		else
		{
			OverT = 0.f;
		}
		// slack: the hook may come out
		if (Tens < Tune.SlackTension && Hook.Pull > 0.08f)
		{
			SlackT += Dt;
			if (SlackT > Tune.SlackAfter && R.U() < 0.6f * Dt)
			{
				Emit(EEvent::Escaped);
				bBaited = false;
				Go(EState::Winding);
				return;
			}
		}
		else
		{
			SlackT = 0.f;
		}
		// a fresh fish shaking its head on a run
		if (bRun && R.U() < 0.006f * Stam * Dt)
		{
			Emit(EEvent::Escaped);
			bBaited = false;
			Go(EState::Winding);
			return;
		}
		if (Dist > Tune.LineLength)
		{
			Emit(EEvent::Snapped);
			if (Bait >= 0 && GBaits[Bait].bLure) Bait = -1;
			bBaited = false;
			Tens = 0.f;
			Dist = 0.f;
			Go(EState::Winding);
			return;
		}
		if (Dist <= Tune.LandAt)
		{
			// lifted out: a big fish still full of fight may tear free at the edge
			if (Hook.Grams > 1500.f && Stam > 0.45f && R.U() < 0.5f)
			{
				Emit(EEvent::Escaped);
				bBaited = false;
				Go(EState::Winding);
				return;
			}
			Emit(EEvent::Landing);
			Go(EState::Landing);
		}
	}

	void FSession::DebugBite(int Fish)
	{
		if (Fish < 0 || Fish >= GNumFish || (St != EState::Waiting && St != EState::Nibbling)) return;
		BiteFish = Fish;
		Emit(EEvent::Take);
		TakeLeft = 2.5f;
		Go(EState::Taking);
	}

	void FSession::DebugHook(int Fish)
	{
		if (Fish < 0 || Fish >= GNumFish) return;
		if (St == EState::Idle || St == EState::Charging || St == EState::Swing || St == EState::Flying || St == EState::Winding)
		{
			Dist = std::max(Dist, 10.f);
		}
		BiteFish = Fish;
		Hooked();
	}

	void FSession::Reset()
	{
		St = EState::Idle;
		T = 0.f;
		Dist = Dip = Tens = SideNow = ChargeT = 0.f;
		NextBite = -1.f;
		bRun = false;
		Ev.clear();
	}

	void FSession::Step(float Dt, const FInput& In)
	{
		Ev.clear();
		T += Dt;
		const bool bPressed = In.bButton && !bWasDown;
		const bool bReleased = !In.bButton && bWasDown;
		bWasDown = In.bButton;
		const bool bLure = Bait >= 0 && GBaits[Bait].bLure;
		Calm = std::max(0.f, Calm - Dt);
		if (St != EState::Taking && St != EState::Nibbling) Dip = Approach(Dip, 0.f, 8.f, Dt);
		if (St != EState::Fighting) Tens = Approach(Tens, 0.f, 6.f, Dt);

		switch (St)
		{
		case EState::Idle:
			Dist = 0.f;
			if (bPressed)
			{
				Go(EState::Charging);
				ChargeT = 0.f;
			}
			break;
		case EState::Charging:
			ChargeT = Clamp01(T / Tune.ChargeTime);
			if (bReleased || !In.bButton)
			{
				Throw = Tune.MinCast + (MaxCast() - Tune.MinCast) * std::pow(ChargeT, 0.8f);
				Emit(EEvent::CastStart);
				Go(EState::Swing);
			}
			break;
		case EState::Swing:
			if (T >= Tune.SwingTime)
			{
				Emit(EEvent::Cast);
				Go(EState::Flying);
			}
			break;
		case EState::Flying:
			// the caller flies the float and calls SetLanded; without one (tests), it falls in the water
			if (T > 4.f) SetLanded(true, Throw);
			break;
		case EState::Waiting:
		{
			const bool bWind = bLure ? (In.bButton || In.bReelIn) : false;
			if (!bLure && In.bReelIn)
			{
				Go(EState::Winding);
				break;
			}
			if (!bLure && bPressed)
			{
				// a strike at nothing: the float jumps, the fish near it are wary a while
				Emit(EEvent::Missed);
				Calm = 3.f;
				NextBite = -1.f;
			}
			if (bLure && bWind)
			{
				Dist -= Tune.ReelSpeed * 0.75f * Dt;
				if (Dist <= Tune.LandAt)
				{
					Emit(EEvent::Retrieved);
					Go(EState::Idle);
					break;
				}
			}
			if (NextBite < 0.f)
			{
				const float Rich = RichNow(bLure);
				const float Every = bLure ? Tune.LureEvery : Tune.BiteEvery;
				const float Rate = (2.f / Every) * Rich / (Rich + Tune.RichWeight);
				NextBite = Rate > 1e-5f ? -std::log(std::max(R.U(), 1e-6f)) / Rate : 1e9f;
			}
			// a spoon is only taken while it moves; a float's bait whenever the fish come
			if (Calm <= 0.f && (!bLure || bWind))
			{
				NextBite -= Dt;
				if (NextBite <= 0.f)
				{
					NextBite = -1.f;
					StartBite();
				}
			}
			break;
		}
		case EState::Nibbling:
		{
			NibbleT += Dt;
			// the nibble's twitch fades
			Dip = Approach(Dip, 0.f, 10.f, Dt);
			if (bPressed)
			{
				// struck too early: a bold fish may still be hooked
				if (R.U() < (1.f - GFish[BiteFish].Shy) * 0.3f)
				{
					Hooked();
					break;
				}
				Emit(EEvent::Missed);
				if (R.U() < 0.5f && !bLure)
				{
					bBaited = false;
					Emit(EEvent::BaitStolen);
				}
				Calm = 4.f;
				Go(EState::Waiting);
				break;
			}
			if (bLure && In.bReelIn == false && In.bButton == false && T > 1.5f)
			{
				// the spoon stopped: the follower turns away
				Go(EState::Waiting);
				break;
			}
			if (NibbleT >= NextNibble)
			{
				NibbleT = 0.f;
				if (Nibbles > 0)
				{
					Nibbles--;
					Dip = std::max(Dip, R.Range(0.2f, 0.4f));
					Emit(EEvent::Nibble);
					NextNibble = R.Range(0.7f, 2.5f);
				}
				else
				{
					Emit(EEvent::Take);
					TakeLeft = GFish[BiteFish].TakeTime * R.Range(0.8f, 1.3f);
					Go(EState::Taking);
					// a spoon hit while winding hooks itself as often as not
					if (bLure && In.bButton && R.U() < 0.55f)
					{
						Hooked();
					}
				}
			}
			break;
		}
		case EState::Taking:
			Dip = Approach(Dip, 1.f, 14.f, Dt);
			// a spoon: still winding through the hit sets the hook as well as a strike
			if (bPressed || (bLure && In.bButton && T >= 0.3f * TakeLeft && T - Dt < 0.3f * TakeLeft))
			{
				if (R.U() < 0.88f)
				{
					Hooked();
				}
				else
				{
					Emit(EEvent::Missed);
					if (!bLure && R.U() < 0.5f)
					{
						bBaited = false;
						Emit(EEvent::BaitStolen);
					}
					Calm = 3.f;
					Go(EState::Waiting);
				}
				break;
			}
			if (T >= TakeLeft)
			{
				Emit(EEvent::Gone);
				if (!bLure && R.U() < 0.55f)
				{
					bBaited = false;
					Emit(EEvent::BaitStolen);
				}
				Calm = 3.f;
				Go(EState::Waiting);
			}
			break;
		case EState::Fighting:
			FightStep(Dt, In.bButton || In.bReelIn);
			break;
		case EState::Landing:
			Dist = Approach(Dist, 0.3f, 4.f, Dt);
			Tens = Approach(Tens, 0.35f, 6.f, Dt);
			if (T >= 0.9f)
			{
				Emit(EEvent::Caught);
				if (!bLure) bBaited = false;
				Dist = 0.f;
				Go(EState::Idle);
			}
			break;
		case EState::Winding:
			Dist -= Tune.ReelSpeed * 1.6f * Dt;
			if (Dist <= 0.5f)
			{
				Dist = 0.f;
				Emit(EEvent::Retrieved);
				Go(EState::Idle);
			}
			break;
		}
	}
}
