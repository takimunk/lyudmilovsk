// The crow family's tests (Private/Birds/BirdCorvid.cpp, docs/corvids.md): scripted little worlds for each thing they do,
// then a morning on the real map. Part of `tools/birdsim/run.sh test`; BIRD_ONLY=corvids runs only these.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdCorvid.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace LyudBirds;

namespace
{
	int Pass = 0, Fail = 0;
	/** BIRD_TRACE=1: these birds' acts and tasks every half second. */
	std::vector<int> Trace;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What.c_str());
		(bOk ? Pass : Fail)++;
	}

	/** A flat little world (ground at 0, the sea far below), midday in late October, the camera close: a meadow round the
	 *  origin, a street 40 m east, an oak, a church tower. */
	void Little(FWorld& W, float Doy = 297.f)
	{
		W = FWorld();
		Trace.clear();
		W.NoGridZ = 0.f;
		W.SeaZ = W.MeanSeaZ = -43.f;
		W.Hour = 11.f;
		W.Sunrise = 7.4f;
		W.Sunset = 17.3f;
		W.DayOfYear = Doy;
		W.Opt.bPredators = false;
		W.Camera = V3(0.f, 0.f, 1.7f);
		FSite Street;
		Street.Kind = EHabitat::Street;
		Street.A = V3(40.f, -10.f, 0.f);
		Street.B = V3(40.f, 10.f, 0.f);
		Street.Radius = 4.f;
		W.AddSite(Street);
		FSite Oak;
		Oak.Kind = EHabitat::Broadleaf;
		Oak.A = Oak.B = V3(10.f, 30.f, 8.f);
		Oak.Radius = 3.f;
		W.AddSite(Oak);
		FSite Tower;
		Tower.Kind = EHabitat::Tower;
		Tower.A = Tower.B = V3(-30.f, 20.f, 18.f);
		W.AddSite(Tower);
		FSite Field;
		Field.Kind = EHabitat::Meadow;
		Field.A = Field.B = V3(-20.f, -25.f, 0.f);
		Field.Radius = 12.f;
		W.AddSite(Field);
	}

	FCorvid& MindOf(FWorld& W, int Id)
	{
		W.Step(0.01f);          // (minds are made on the first step)
		W.Events.clear();
		return W.Corvids()->Minds[Id];
	}

	/** Steps Seconds of the world at 30 Hz; Fn sees each step's corvid events; stops early when Fn returns true. */
	float Tick(FWorld& W, float Seconds, const std::function<bool(const FCorvidEvent&)>& Fn = nullptr, const std::function<void()>& Each = nullptr)
	{
		const float Dt = 1.f / 30.f;
		const bool bTrace = std::getenv("BIRD_TRACE") != nullptr;
		int N = 0;
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			W.Step(Dt);
			W.Events.clear();
			if (bTrace && N++ % 15 == 0)
			{
				for (int Id : Trace)
				{
					const FBird& B = W.Birds[Id];
					const FCorvid& C = W.Corvids()->Minds[Id];
					std::printf("    %5.1f #%d %s %s/%d pos %.1f %.1f %.1f alert %.2f held %d\n", T, Id, ActName(B.Act), TaskName(C.Task), C.Step, B.Pos.X, B.Pos.Y, B.Pos.Z, B.Alert, C.Held);
				}
				for (const FCorvidEvent& E : W.Corvids()->Events)
				{
					std::printf("    ev %s #%d item %d\n", CorvidEventName(E.Kind), E.Bird, E.Item);
				}
			}
			bool bStop = false;
			for (const FCorvidEvent& E : W.Corvids()->Events)
			{
				if (Fn && Fn(E))
				{
					bStop = true;
				}
			}
			W.Corvids()->Events.clear();
			if (Each)
			{
				Each();
			}
			if (bStop)
			{
				return T;
			}
		}
		return -1.f;
	}

	void Player(FWorld& W, const V3& Feet, const V3& Facing, bool bCrouch = false)
	{
		W.Player.bPresent = true;
		W.Player.Feet = Feet;
		W.Player.Eye = Feet + V3(0.f, 0.f, bCrouch ? 1.0f : 1.65f);
		W.Player.Facing = Facing.Norm();
		W.Player.bCrouched = bCrouch;
		W.Player.Vel = V3();
		W.Player.Palm = W.Player.Eye + W.Player.Facing * 0.5f + V3(0.f, 0.f, -0.35f);
	}

	int Events(FCorvids* C, ECorvidEvent E) { return C->Count[(int)E]; }
}

int CorvidTests(int* Passed)
{
	Pass = Fail = 0;
	std::printf("corvids:\n");
	const auto T0 = std::chrono::steady_clock::now();

	// a jay buries an acorn in October, and digs it up again when hungry
	{
		FWorld W;
		Little(W, 290.f);
		const int Jay = W.AddFlock(FindSpecies("Jay"), V3(0.f, 0.f, 0.f), 1, 2.f);
		FCorvid& C = MindOf(W, Jay);
		C.Hoard = 1.f;
		W.Birds[Jay].Hunger = 0.3f;
		W.Corvids()->Spawn(W, EItem::Acorn, V3(3.f, 1.f, 0.f));
		int Acorn = -1;
		bool bPicked = false, bCached = false;
		V3 PickedAt;
		Trace = { Jay };
		Tick(W, 150.f, [&](const FCorvidEvent& E)
		{
			if (E.Kind == ECorvidEvent::Pick && W.Corvids()->Items[E.Item].Kind == EItem::Acorn && !bPicked)
			{
				bPicked = true;
				Acorn = E.Item;
				PickedAt = E.Pos;
			}
			bCached |= E.Kind == ECorvidEvent::Cache && E.Item == Acorn;
			return bCached;
		});
		const FItem& It = W.Corvids()->Items[std::max(Acorn, 0)];
		Check(bPicked, "a jay picks up an acorn (the one lying there, or one from the tree)");
		Check(bCached && It.State == EItemState::Cached && It.Owner == Jay, "and buries it somewhere else (a store)");
		Check((It.Pos - PickedAt).Len2D() > 5.f, "the store is away from where it found it");
		W.Birds[Jay].Hunger = 0.95f;
		bool bDug = false;
		Tick(W, 200.f, [&](const FCorvidEvent& E)
		{
			bDug |= E.Kind == ECorvidEvent::Dig && E.Item == Acorn;
			return bDug;
		});
		Check(bDug, "hungry, it digs its acorn up again");
	}

	// a crow watches another hide something, and robs the store once the owner has gone
	{
		FWorld W;
		Little(W);
		const int Owner = W.AddFlock(FindSpecies("HoodedCrow"), V3(0.f, 0.f, 0.f), 1, 2.f);
		Trace = {};
		const int Thief = W.AddFlock(FindSpecies("HoodedCrow"), V3(-8.f, -12.f, 0.f), 1, 2.f);
		Trace = { Owner, Thief };
		FCorvid& O = MindOf(W, Owner);
		O.Hoard = 1.f;
		W.Corvids()->Minds[Thief].Cooldown = 1e9f;      // just watches for now
		W.Birds[Owner].Hunger = 0.2f;
		const int Crust = W.Corvids()->Spawn(W, EItem::Crust, V3(1.f, 0.f, 0.f));
		bool bCached = false;
		Tick(W, 150.f, [&](const FCorvidEvent& E)
		{
			bCached |= E.Kind == ECorvidEvent::Cache && E.Item == Crust;
			return bCached;
		});
		const FCorvid& T = W.Corvids()->Minds[Thief];
		const bool bSpied = std::find(T.Spied.begin(), T.Spied.end(), Crust) != T.Spied.end();
		Check(bCached && bSpied, "a crow near by sees where another hides a crust");
		// the owner goes off (keeps away), the thief gets hungry
		W.Groups[W.Birds[Owner].Group].bStayPut = true;
		W.Birds[Owner].Pos = W.Birds[Owner].Spot = V3(400.f, 0.f, 0.f);
		W.Sites[W.Groups[W.Birds[Owner].Group].Site].A = W.Sites[W.Groups[W.Birds[Owner].Group].Site].B = V3(400.f, 0.f, 0.f);
		W.Corvids()->Minds[Owner].Cooldown = 1e9f;
		W.Corvids()->Minds[Thief].Cooldown = 0.f;
		W.Birds[Thief].Hunger = 0.8f;
		bool bRobbed = false;
		Tick(W, 200.f, [&](const FCorvidEvent& E)
		{
			bRobbed |= E.Kind == ECorvidEvent::Rob && E.Item == Crust && E.Bird == Thief;
			return bRobbed;
		});
		Check(bRobbed, "with the owner away it robs the store");
	}

	// a crow drops a walnut on the road from a height until it cracks, then eats it
	{
		FWorld W;
		Little(W);
		const int Crow = W.AddFlock(FindSpecies("HoodedCrow"), V3(25.f, 0.f, 0.f), 1, 2.f);
		FCorvid& C = MindOf(W, Crow);
		C.Hoard = 0.f;
		W.Birds[Crow].Hunger = 0.8f;
		const int Nut = W.Corvids()->Spawn(W, EItem::Walnut, V3(26.f, 0.5f, 0.f));
		int Drops = 0;
		float MaxZ = 0.f;
		bool bAte = false;
		Tick(W, 400.f, [&](const FCorvidEvent& E)
		{
			Drops += E.Kind == ECorvidEvent::Drop && E.Item == Nut;
			bAte |= E.Kind == ECorvidEvent::Eat && E.Item == Nut;
			return bAte;
		}, [&]()
		{
			const FItem& It = W.Corvids()->Items[Nut];
			if (It.State == EItemState::Falling)
			{
				MaxZ = std::max(MaxZ, It.Pos.Z);
			}
		});
		std::printf("  walnut: %d drops (from up to %.1f m), cracked %d, eaten %d\n", Drops, MaxZ, Events(W.Corvids(), ECorvidEvent::Crack), bAte);
		Check(Drops >= 1 && MaxZ > 4.f, "it flies up over the road and drops the walnut from a height");
		Check(bAte && Events(W.Corvids(), ECorvidEvent::Crack) >= 1, "until it cracks, then eats it");
	}

	// a crow chases a jackdaw carrying a crust until it drops it, and takes it
	{
		FWorld W;
		Little(W);
		const int Jd = W.AddFlock(FindSpecies("Jackdaw"), V3(0.f, 0.f, 0.f), 1, 2.f);
		const int Crow = W.AddFlock(FindSpecies("HoodedCrow"), V3(15.f, 5.f, 0.f), 1, 2.f);
		Trace = { Jd, Crow };
		MindOf(W, Jd);
		FCorvids& Cv = *W.Corvids();
		Cv.Minds[Jd].Hoard = 1.f;
		Cv.Minds[Crow].Cooldown = 1e9f;
		Cv.Minds[Jd].Cooldown = 0.f;
		W.Birds[Crow].Hunger = 0.9f;
		W.Birds[Jd].Hunger = 0.1f;
		const int Crust = Cv.Spawn(W, EItem::Crust, V3(0.5f, 0.f, 0.f));
		// the jackdaw picks it up first; then the crow notices
		Tick(W, 60.f, [&](const FCorvidEvent& E) { return E.Kind == ECorvidEvent::Pick && E.Bird == Jd; });
		Check(Cv.Minds[Jd].Held == Crust, "a jackdaw picks up a crust (to hide it)");
		Cv.Minds[Crow].Cooldown = 0.f;
		bool bStolen = false;
		Tick(W, 240.f, [&](const FCorvidEvent& E)
		{
			bStolen |= E.Kind == ECorvidEvent::Steal && E.Bird == Crow;
			return bStolen;
		});
		Check(bStolen, "a crow chases a jackdaw with a crust until it drops it, and takes it");
		(void)Crust;
	}

	// a magpie grabs a beakful from the player's palm instead of eating there
	{
		FWorld W;
		Little(W);
		const int Mg = W.AddFlock(FindSpecies("Magpie"), V3(0.f, 0.f, 0.f), 1, 2.f);
		MindOf(W, Mg);
		W.Birds[Mg].Trust = 0.35f;
		W.Birds[Mg].Hunger = 0.7f;
		Player(W, V3(-6.f, 0.f, 0.f), V3(1.f, 0.f, -0.3f), true);
		W.Player.bOffer = true;
		bool bSnatch = false;
		float Closest = 1e9f;
		Tick(W, 120.f, [&](const FCorvidEvent& E)
		{
			bSnatch |= E.Kind == ECorvidEvent::Snatch && E.Bird == Mg;
			return false;
		}, [&]() { Closest = std::min(Closest, (W.Birds[Mg].Pos - W.Player.Palm).Len()); });
		const FCorvid& C = W.Corvids()->Minds[Mg];
		Check(bSnatch && Closest < 0.6f, "a magpie snatches feed from the held-out palm");
		Check(W.Birds[Mg].Act != EAct::Eat && (W.Birds[Mg].Pos - W.Player.Palm).Len() > 5.f, "and is off with it, not eating on the palm");
		Check(C.Bond > 0.f, "it got food from the player: the start of a bond");
	}

	// a crow that has been fed brings the player a present
	{
		FWorld W;
		Little(W);
		const int Crow = W.AddFlock(FindSpecies("HoodedCrow"), V3(0.f, 0.f, 0.f), 1, 2.f);
		FCorvid& C = MindOf(W, Crow);
		C.Bond = 0.85f;
		C.Hoard = 0.f;
		C.Cooldown = 0.f;
		W.Birds[Crow].Hunger = 0.1f;
		Player(W, V3(-15.f, 8.f, 0.f), V3(1.f, -0.5f, 0.f));
		const int Cap = W.Corvids()->Spawn(W, EItem::BottleCap, V3(4.f, -2.f, 0.f));
		bool bGift = false;
		Tick(W, 240.f, [&](const FCorvidEvent& E)
		{
			bGift |= E.Kind == ECorvidEvent::Gift && E.Item == Cap;
			return bGift;
		});
		const FItem& It = W.Corvids()->Items[Cap];
		std::printf("  present: %s at %.1f m from the player\n", bGift ? "left" : "none", (It.Pos - W.Player.Feet).Len2D());
		Check(bGift && (It.Flags & ItemGift) && It.State == EItemState::Ground, "a crow with a bond to the player brings them a bottle cap");
		Check((It.Pos - W.Player.Feet).Len2D() < 7.f, "and leaves it on the ground in front of them");
		std::vector<int> Near;
		W.Corvids()->PickableNear(W, W.Player.Feet, 8.f, Near);
		Check(std::find(Near.begin(), Near.end(), Cap) != Near.end() && W.Corvids()->PlayerTake(W, Cap), "the player can pick the present up");
	}

	// a crow with a grudge scolds the player, and the others pick it up
	{
		FWorld W;
		Little(W);
		const int A = W.AddFlock(FindSpecies("HoodedCrow"), V3(0.f, 0.f, 0.f), 1, 2.f);
		const int B = W.AddFlock(FindSpecies("HoodedCrow"), V3(8.f, 6.f, 0.f), 1, 2.f);
		FCorvid& CA = MindOf(W, A);
		CA.Grudge = 0.8f;
		CA.Cooldown = 0.f;
		W.Corvids()->Minds[B].Cooldown = 1e9f;
		Player(W, V3(-20.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
		int Scolds = 0;
		float Closest = 1e9f;
		Tick(W, 60.f, [&](const FCorvidEvent& E)
		{
			Scolds += E.Kind == ECorvidEvent::Scold;
			return false;
		}, [&]() { Closest = std::min(Closest, (W.Birds[A].Pos - W.Player.Eye).Len()); });
		const float Learned = W.Corvids()->Minds[B].Grudge;
		std::printf("  scolding: %d calls, closest %.1f m, the other crow's grudge %.2f\n", Scolds, Closest, Learned);
		Check(Scolds >= 4, "a crow with a grudge calls at the player again and again");
		Check(Closest < 12.f, "from close by (a perch over them, or diving past their head)");
		Check(Learned > 0.15f, "and another crow near by learns to dislike them too");
	}

	// robbing a store under its owner's eyes makes an enemy
	{
		FWorld W;
		Little(W);
		const int Crow = W.AddFlock(FindSpecies("HoodedCrow"), V3(0.f, 0.f, 0.f), 1, 2.f);
		FCorvid& C = MindOf(W, Crow);
		C.Hoard = 1.f;
		W.Birds[Crow].Hunger = 0.1f;
		Player(W, V3(-25.f, 10.f, 0.f), V3(0.4f, -1.f, -0.2f));
		Trace = { Crow };
		const int Nut = W.Corvids()->Spawn(W, EItem::Hazelnut, V3(1.f, 0.f, 0.f));
		bool bCached = false;
		Tick(W, 150.f, [&](const FCorvidEvent& E)
		{
			bCached |= E.Kind == ECorvidEvent::Cache && E.Item == Nut;
			return bCached;
		});
		const bool bSeen = (W.Corvids()->Items[Nut].Flags & ItemSeen) != 0;
		Check(bCached && bSeen, "the player watched a crow hide a nut");
		const float Before = W.Corvids()->Minds[Crow].Grudge;
		W.Player.Feet = W.Corvids()->Items[Nut].Pos;
		Check(W.Corvids()->PlayerTake(W, Nut), "the player digs it up");
		Check(W.Corvids()->Minds[Crow].Grudge > Before + 0.3f, "under the crow's eyes: it holds a grudge now");
	}

	// a dead fish on the beach: the strongest eats, the others wait round it and dart in
	{
		FWorld W;
		Little(W);
		const int Raven = W.AddFlock(FindSpecies("Raven"), V3(30.f, 0.f, 0.f), 1, 2.f);
		const int Fish = W.Corvids()->WashUp(W, V3(0.f, 0.f, 0.f));
		const int Crow1 = W.AddFlock(FindSpecies("HoodedCrow"), V3(-30.f, 5.f, 0.f), 1, 2.f);
		const int Crow2 = W.AddFlock(FindSpecies("HoodedCrow"), V3(-25.f, -8.f, 0.f), 1, 2.f);
		const int Mg = W.AddFlock(FindSpecies("Magpie"), V3(10.f, 25.f, 0.f), 1, 2.f);
		MindOf(W, Raven);
		for (int Id : { Raven, Crow1, Crow2, Mg })
		{
			W.Birds[Id].Hunger = 0.9f;
			W.Corvids()->Minds[Id].Cooldown = 0.f;
		}
		int Samples = 0, Crowded = 0, RavenFirst = 0, Atting = 0;
		Tick(W, 180.f, nullptr, [&]()
		{
			int AtFish = 0;
			bool bRavenAt = false;
			for (int Id : { Raven, Crow1, Crow2, Mg })
			{
				const FBird& B = W.Birds[Id];
				if ((B.Pos - W.Corvids()->Items[Fish].Pos).Len2D() < 0.6f && B.Act == EAct::Peck)
				{
					AtFish++;
					bRavenAt |= Id == Raven;
				}
			}
			int Round = 0;
			for (int Id : { Raven, Crow1, Crow2, Mg })
			{
				Round += W.Corvids()->Minds[Id].Task == ETask::Feast;
			}
			if (Round >= 3)
			{
				Samples++;
				Crowded += AtFish > 1;
				Atting += AtFish > 0;
				RavenFirst += bRavenAt;
			}
		});
		std::printf("  dead fish: %d samples with 3+ birds round it, someone eating %d, two at once %d, the raven eating %d; left %.2f\n", Samples, Atting,
			Crowded, RavenFirst, W.Corvids()->Items[Fish].Food);
		Check(Samples > 300, "ravens, crows and a magpie gather round a dead fish");
		Check(RavenFirst > Atting / 3, "the raven eats first when it is there (the strongest)");
		Check(Crowded < Atting / 4, "the others mostly wait their turn round it (darting in now and then)");
		Check(W.Corvids()->Items[Fish].Food < 1.f, "the fish goes down");
	}

	// a magpie pulls a resting cat's tail
	{
		FWorld W;
		Little(W);
		const int Mg = W.AddFlock(FindSpecies("Magpie"), V3(0.f, 0.f, 0.f), 1, 2.f);
		Trace = { Mg };
		FCorvid& C = MindOf(W, Mg);
		C.Play = 0.8f;
		C.Cooldown = 0.f;
		W.Birds[Mg].Hunger = 0.1f;
		int Teases = 0;
		Tick(W, 120.f, [&](const FCorvidEvent& E)
		{
			Teases += E.Kind == ECorvidEvent::Tease;
			return false;
		}, [&]()
		{
			W.Beasts.clear();
			FBeast Cat;
			Cat.Pos = V3(8.f, 4.f, 0.f);
			Cat.bCat = true;
			W.Beasts.push_back(Cat);
		});
		Check(Teases >= 2, "a magpie pulls a resting cat's tail, hopping back between goes");
	}

	// jackdaws play in a gale over the church tower, the whole party together
	{
		FWorld W;
		Little(W);
		W.Wind = V3(12.f, 0.f, 0.f);
		const int First = W.AddFlock(FindSpecies("Jackdaw"), V3(-20.f, 10.f, 0.f), 6, 4.f);
		MindOf(W, First);
		for (int I = 0; I < 6; I++)
		{
			W.Corvids()->Minds[First + I].Play = 0.9f;
			W.Corvids()->Minds[First + I].Cooldown = I == 0 ? 0.f : 1e9f;
			W.Birds[First + I].Hunger = 0.1f;
		}
		W.Groups[W.Birds[First].Group].Plan = 0;
		int MaxTogether = 0;
		Tick(W, 120.f, nullptr, [&]()
		{
			int N = 0;
			for (int I = 0; I < 6; I++)
			{
				N += W.Birds[First + I].Act == EAct::Soar && W.Corvids()->Minds[First + I].Task == ETask::Play;
			}
			MaxTogether = std::max(MaxTogether, N);
		});
		Check(MaxTogether >= 3, "jackdaws tumble in a gale over the tower, several of the flock together");
	}

	// a pair preens each other at rest
	{
		FWorld W;
		Little(W);
		const int First = W.AddFlock(FindSpecies("HoodedCrow"), V3(0.f, 0.f, 0.f), 2, 3.f);
		Trace = { First, First + 1 };
		FCorvid& C = MindOf(W, First);
		W.Groups[W.Birds[First].Group].Plan = 0;
		C.Social = 1.f;
		C.Cooldown = 0.f;
		W.Corvids()->Minds[First + 1].Cooldown = 1e9f;
		W.Birds[First].Hunger = W.Birds[First + 1].Hunger = 0.1f;
		bool bBoth = false;
		Tick(W, 120.f, nullptr, [&]()
		{
			bBoth |= W.Birds[First].Act == EAct::Preen && W.Birds[First + 1].Act == EAct::Preen && (W.Birds[First].Pos - W.Birds[First + 1].Pos).Len() < 1.f;
		});
		Check(C.Mate == First + 1 && bBoth, "a crow pair sits close and preens each other");
	}

	// thrown feed: a flock of rooks comes down to it and eats together, and warms to the player
	{
		FWorld W;
		Little(W);
		const int First = W.AddFlock(FindSpecies("Rook"), V3(-20.f, -25.f, 0.f), 8, 6.f);
		MindOf(W, First);
		Player(W, V3(-5.f, 0.f, 0.f), V3(1.f, 0.f, 0.f));
		const int Pile = W.Corvids()->PlayerDrop(W, EItem::FeedPile, V3(-12.f, -12.f, 0.f));
		for (int I = 0; I < 8; I++)
		{
			W.Birds[First + I].Hunger = 0.8f;
			W.Birds[First + I].Trust = 0.5f;
			W.Corvids()->Minds[First + I].Cooldown = 0.f;
		}
		int MaxAt = 0;
		Tick(W, 120.f, nullptr, [&]()
		{
			int N = 0;
			for (int I = 0; I < 8; I++)
			{
				N += (W.Birds[First + I].Pos - W.Corvids()->Items[Pile].Pos).Len2D() < 0.7f && W.Birds[First + I].Act == EAct::Peck;
			}
			MaxAt = std::max(MaxAt, N);
		});
		float Bond = 0.f;
		for (int I = 0; I < 8; I++)
		{
			Bond = std::max(Bond, W.Corvids()->Minds[First + I].Bond);
		}
		std::printf("  thrown feed: up to %d rooks at once, left %.2f, best bond %.3f\n", MaxAt, W.Corvids()->Items[Pile].Food, Bond);
		Check(MaxAt >= 3, "several rooks eat at the thrown feed together");
		Check(Bond > 0.02f, "eating the player's food near them starts a bond");
	}

	// a morning on the real map: jays bury acorns, crows and rooks go about their business, and nobody gets lost
	if (!std::getenv("BIRD_CORVID_QUICK"))
	{
		FWorld W;
		if (W.LoadHabitats(std::getenv("BIRD_DATA") ? std::getenv("BIRD_DATA") : "unreal/Lyudmilovsk/Content/BirdsData"))
		{
			W.DayOfYear = 297.f;
			W.Sunrise = 7.40f;
			W.Sunset = 17.32f;
			W.Hour = 8.f;
			W.Camera = V3(-1.93f, 11.9f, 1.7f);
			W.Populate();
			const auto A = std::chrono::steady_clock::now();
			double StepS = 0.0;
			long Steps = 0;
			for (float T = 0.f; T < 3.f * 3600.f; T += 0.1f)
			{
				W.Hour = 8.f + T / 3600.f;
				const auto S0 = std::chrono::steady_clock::now();
				W.Step(0.1f);
				StepS += std::chrono::duration<double>(std::chrono::steady_clock::now() - S0).count();
				Steps++;
				W.Events.clear();
				W.Corvids()->Events.clear();
			}
			(void)A;
			FCorvids& C = *W.Corvids();
			std::printf("  map 8-11 h: %s\n  step %.3f ms\n", C.Report(W).c_str(), StepS / Steps * 1000.0);
			int Lost = 0;
			for (const FBird& B : W.Birds)
			{
				Lost += !B.bGone && (std::isnan(B.Pos.X) || std::fabs(B.Pos.X) > 20000.f || B.Pos.Z < W.SeaZ - 3.f);
			}
			Check(C.Count[(int)ECorvidEvent::Cache] > 10, "over a morning the corvids hide things (jays their acorns)");
			Check(C.TaskStarts[(int)ETask::Harvest] > 10 && C.Count[(int)ECorvidEvent::Pick] > 20, "they peck out and pick up acorns, nuts, mussels");
			Check(Lost == 0, "no bird is lost out of the world");
			// and they still go to roost at dusk, carrying nothing
			for (float T = 0.f; T < (W.Sunset + 1.3f - 11.f) * 3600.f; T += 0.5f)
			{
				W.Hour = 11.f + T / 3600.f;
				W.Step(0.5f);
				W.Events.clear();
				W.Corvids()->Events.clear();
			}
			int Corv = 0, Asleep = 0, Holding = 0;
			for (const FBird& B : W.Birds)
			{
				if (!C.IsCorvid(B.Id) || B.bGone)
				{
					continue;
				}
				Corv++;
				Asleep += B.Act == EAct::Sleep;
				Holding += C.Minds[B.Id].Held >= 0 || C.Minds[B.Id].Task != ETask::None;
			}
			std::printf("  dusk + 1.3 h: %d of %d corvids asleep, %d still busy\n", Asleep, Corv, Holding);
			Check(Asleep > Corv * 0.8f && Holding == 0, "at night the corvids sleep, nothing in their bills");
		}
	}

	std::printf("corvids: %d passed, %d failed (%.1f s)\n", Pass, Fail, std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count());
	if (Passed)
	{
		*Passed = Pass;
	}
	return Fail;
}
