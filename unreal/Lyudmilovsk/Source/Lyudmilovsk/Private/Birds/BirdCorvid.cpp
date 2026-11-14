// The crow family's minds, tasks and the things they carry (BirdCorvid.h, docs/corvids.md).
#include "BirdCorvid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace LyudBirds
{
	namespace
	{
		constexpr float Pi = 3.14159265f;

		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
		float Smooth(float T) { T = Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
		constexpr uint32_t Bit(EItem K) { return 1u << (uint32_t)K; }

		const FItemKind Kinds[NumItems] = {
			//  food   hard   big    shiny  cache  stick
			{ 0.15f, false, false, false, true,  false },   // Acorn
			{ 0.20f, false, false, false, true,  false },   // Hazelnut
			{ 0.35f, true,  false, false, true,  false },   // Walnut
			{ 0.25f, false, false, false, true,  false },   // PineCone
			{ 0.30f, true,  false, false, false, false },   // Mussel
			{ 0.30f, false, false, false, true,  false },   // Crust
			{ 3.00f, false, true,  false, false, false },   // Fish
			{ 1.00f, false, true,  false, false, false },   // FeedPile
			{ 0.f,   false, false, true,  true,  false },   // BottleCap
			{ 0.f,   false, false, true,  true,  false },   // Foil
			{ 0.f,   false, false, true,  true,  false },   // Ring
			{ 0.f,   false, false, true,  true,  false },   // Button
			{ 0.f,   false, false, true,  true,  false },   // Coin
			{ 0.f,   false, false, true,  true,  false },   // Key
			{ 0.f,   false, false, true,  true,  false },   // SeaGlass
			{ 0.f,   false, false, false, false, true  },   // Stick
		};

		// the tempers: what each species is known for (Emery & Clayton 2004 on caching and pilfering, Heinrich on ravens' play
		// and carcasses, Marzluff on crows' faces and grudges, Cristol & Switzer on crows dropping shells, Bossema on jays' acorns)
		const uint32_t Nuts = Bit(EItem::Acorn) | Bit(EItem::Hazelnut) | Bit(EItem::Walnut);
		// clang-format off
		const FCorvidKind Table[] = {
			// id             hoard play  steal shiny clever soc  gift  crack tease  far    pecks out where it feeds
			{ "Rook",         0.3f, 0.4f, 0.3f, 0.1f, 0.5f,  1.0f, 0.25f,0.5f, 0.1f,  40.f, Bit(EItem::Walnut) | Bit(EItem::Acorn) },
			{ "HoodedCrow",   0.5f, 0.5f, 0.7f, 0.4f, 0.8f,  0.5f, 0.6f, 1.0f, 0.5f,  50.f, Bit(EItem::Mussel) | Bit(EItem::Walnut) | Bit(EItem::Crust) },
			{ "CarrionCrow",  0.5f, 0.5f, 0.7f, 0.4f, 0.8f,  0.5f, 0.6f, 1.0f, 0.5f,  50.f, Bit(EItem::Mussel) | Bit(EItem::Walnut) | Bit(EItem::Crust) },
			{ "Jackdaw",      0.3f, 0.8f, 0.6f, 1.0f, 0.5f,  1.0f, 0.4f, 0.2f, 0.3f,  30.f, Bit(EItem::Acorn) | Bit(EItem::Crust) },
			{ "Magpie",       0.5f, 0.4f, 0.8f, 0.6f, 0.6f,  0.4f, 0.2f, 0.1f, 1.0f,  40.f, Bit(EItem::Acorn) | Bit(EItem::Crust) | Bit(EItem::Hazelnut) },
			{ "Raven",        0.6f, 1.0f, 0.8f, 0.3f, 1.0f,  0.6f, 0.3f, 0.4f, 0.5f, 120.f, Bit(EItem::Crust) | Bit(EItem::Mussel) | Bit(EItem::Walnut) },
			{ "Jay",          1.0f, 0.2f, 0.4f, 0.1f, 0.7f,  0.2f, 0.15f,0.0f, 0.1f, 250.f, Bit(EItem::Acorn) | Bit(EItem::Hazelnut) },
			{ "Nutcracker",   1.0f, 0.1f, 0.2f, 0.0f, 0.5f,  0.2f, 0.15f,0.0f, 0.0f, 300.f, Bit(EItem::Hazelnut) | Bit(EItem::PineCone) | Nuts },
		};
		// clang-format on
		constexpr int NumKinds = sizeof(Table) / sizeof(Table[0]);

		/** Where an item grows or turns up (the habitats a bird pecks it out of, and the world puts it out in). */
		bool GrowsIn(EItem K, EHabitat H)
		{
			switch (K)
			{
			case EItem::Acorn: return H == EHabitat::Broadleaf || H == EHabitat::ForestFloor || H == EHabitat::Garden;
			case EItem::Hazelnut: return H == EHabitat::Garden || H == EHabitat::ForestFloor || H == EHabitat::Broadleaf;
			case EItem::Walnut: return H == EHabitat::Garden || H == EHabitat::Street;
			case EItem::PineCone: return H == EHabitat::Conifer || H == EHabitat::ForestFloor;
			case EItem::Mussel: return H == EHabitat::Beach;
			case EItem::Crust: return H == EHabitat::Street || H == EHabitat::Garden;
			default: return false;
			}
		}

		/** A hard surface a shell breaks on. */
		bool IsHard(EHabitat H) { return H == EHabitat::Street || H == EHabitat::Roof || H == EHabitat::Tower || H == EHabitat::Cliff; }

		uint32_t Hash(uint32_t X)
		{
			X ^= X >> 16;
			X *= 0x7feb352du;
			X ^= X >> 15;
			X *= 0x846ca68bu;
			X ^= X >> 16;
			return X;
		}
	}

	const char* ItemName(EItem K)
	{
		static const char* const Names[] = { "Acorn", "Hazelnut", "Walnut", "PineCone", "Mussel", "Crust", "Fish", "FeedPile",
			"BottleCap", "Foil", "Ring", "Button", "Coin", "Key", "SeaGlass", "Stick" };
		return (int)K < NumItems ? Names[(int)K] : "?";
	}

	const FItemKind& ItemKind(EItem K) { return Kinds[(int)K < NumItems ? (int)K : 0]; }

	const char* TaskName(ETask T)
	{
		static const char* const Names[] = { "None", "Fetch", "Harvest", "Eat", "Cache", "Retrieve", "Pilfer", "Crack", "Snatch", "Steal",
			"Gift", "Scold", "Follow", "Preen", "Play", "Tease", "Feast" };
		return (int)T < (int)ETask::Num ? Names[(int)T] : "?";
	}

	const char* CorvidEventName(ECorvidEvent E)
	{
		static const char* const Names[] = { "Pick", "Drop", "Cache", "Dig", "Rob", "Steal", "Snatch", "Gift", "Scold", "Play", "Tease", "Crack", "Eat" };
		return (int)E < (int)ECorvidEvent::Num ? Names[(int)E] : "?";
	}

	int FCorvids::KindOf(const FSpecies& S)
	{
		for (int K = 0; K < NumKinds; K++)
		{
			if (S.Id == Table[K].Id)
			{
				return K;
			}
		}
		return -1;
	}

	const FCorvidKind& FCorvids::Kind(int K) { return Table[K >= 0 && K < NumKinds ? K : 0]; }

	// --- bookkeeping ----------------------------------------------------------------------------------

	void FCorvids::Sync(FWorld& W)
	{
		if (Minds.size() == W.Birds.size())
		{
			return;
		}
		const size_t From = Minds.size();
		Minds.resize(W.Birds.size());
		for (size_t I = From; I < Minds.size(); I++)
		{
			FBird& B = W.Birds[I];
			FCorvid& C = Minds[I];
			C.Kind = KindOf(SpeciesTable()[B.Species]);
			if (C.Kind < 0)
			{
				continue;
			}
			C.Hoard = W.Rand(B.Rng) * 0.5f;
			C.Social = W.Rand(B.Rng) * 0.5f;
			C.Play = W.Rand(B.Rng) * 0.5f;
			C.Cooldown = 5.f + W.Rand(B.Rng) * 30.f;
		}
		// mates: the crow family lives in pairs, in a flock too (a jackdaw flock is pairs); pair a group's members in turn
		for (const FGroup& G : W.Groups)
		{
			int Open = -1;
			for (int Id : G.Members)
			{
				if (Id < (int)From || !IsCorvid(Id) || Minds[Id].Mate >= 0)
				{
					continue;
				}
				if (Open < 0)
				{
					Open = Id;
				}
				else
				{
					Minds[Open].Mate = Id;
					Minds[Id].Mate = Open;
					Open = -1;
				}
			}
		}
	}

	int FCorvids::NewItem(FWorld& W, EItem Kind, const V3& P, EItemState State)
	{
		int Id;
		if (!FreeItems.empty())
		{
			Id = FreeItems.back();
			FreeItems.pop_back();
		}
		else
		{
			if (Items.size() >= 4000)
			{
				return -1;
			}
			Id = (int)Items.size();
			Items.push_back(FItem());
		}
		FItem& It = Items[Id];
		It = FItem();
		It.Id = Id;
		It.Kind = Kind;
		It.State = State;
		It.Pos = P;
		It.Yaw = (float)(Hash((uint32_t)Id * 31u + (uint32_t)W.Time) % 6283) / 1000.f;
		It.Since = W.Time;
		return Id;
	}

	void FCorvids::Remove(FWorld& W, int Item)
	{
		(void)W;
		if (Item < 0 || Item >= (int)Items.size() || Items[Item].State == EItemState::Gone)
		{
			return;
		}
		FItem& It = Items[Item];
		if (It.Holder >= 0 && IsCorvid(It.Holder) && Minds[It.Holder].Held == Item)
		{
			Minds[It.Holder].Held = -1;
		}
		It.State = EItemState::Gone;
		It.Holder = -1;
		It.Flags = 0;
		FreeItems.push_back(Item);
	}

	void FCorvids::Emit(ECorvidEvent E, const FBird& B, int Item)
	{
		Events.push_back({ E, B.Id, Item, B.Pos });
		Count[(int)E]++;
	}

	int FCorvids::Spawn(FWorld& W, EItem Kind, const V3& P, uint16_t Flags)
	{
		const int Id = NewItem(W, Kind, P, EItemState::Ground);
		if (Id >= 0)
		{
			Items[Id].Flags = Flags;
		}
		return Id;
	}

	int FCorvids::GiveItem(FWorld& W, FBird& B, EItem Kind)
	{
		Sync(W);
		FCorvid* C = Mind(B.Id);
		if (!C)
		{
			return -1;
		}
		const int Id = NewItem(W, Kind, B.Pos, EItemState::Ground);
		if (Id >= 0)
		{
			Pick(W, B, *C, Id);
		}
		return Id;
	}

	bool FCorvids::UseItem(FWorld& W, FBird& B, int Item)
	{
		FCorvid* C = Mind(B.Id);
		if (!C || C->Held != Item || Item < 0)
		{
			return false;
		}
		Remove(W, Item);
		C->Held = -1;
		return true;
	}

	int FCorvids::WashUp(FWorld& W, const V3& P)
	{
		return Spawn(W, EItem::Fish, P);
	}

	int FCorvids::PlayerDrop(FWorld& W, EItem Kind, const V3& P)
	{
		return Spawn(W, Kind, P, ItemPlayer);
	}

	V3 FCorvids::GroundAt(const FWorld& W, float X, float Y) const
	{
		float Z;
		if (W.Probe && (V3(X, Y, 0.f) - W.Camera.Flat()).Len2D() < 200.f && W.Probe(X, Y, Z))
		{
			return V3(X, Y, Z);
		}
		return V3(X, Y, W.GroundZ(X, Y));
	}

	bool FCorvids::Near(const FBird& B, const V3& P, float R) const
	{
		return (B.Pos - P).Len2D() < R && std::fabs(B.Pos.Z - P.Z) < std::max(R, 1.f);
	}

	float FCorvids::Season(const FWorld& W, EItem K) const
	{
		// the window of the year when it lies about (day of year), soft at the ends
		auto Window = [&](float A, float B) { return Smooth((W.DayOfYear - A) / 15.f) * Smooth((B - W.DayOfYear) / 15.f); };
		switch (K)
		{
		case EItem::Acorn: return Window(245.f, 340.f);
		case EItem::Hazelnut: return Window(225.f, 315.f);
		case EItem::Walnut: return Window(250.f, 335.f);
		default: return 1.f;
		}
	}

	bool FCorvids::PlayerSees(FWorld& W, const V3& P, float MaxD) const
	{
		const FPlayer& Pl = W.Player;
		if (!Pl.bPresent || Pl.bClaimedBird)
		{
			return false;
		}
		const V3 To = P - Pl.Eye;
		const float D = To.Len();
		if (D > MaxD || Pl.Facing.Dot(To * (1.f / std::max(D, 0.01f))) < 0.75f)
		{
			return false;
		}
		return !W.Sight || W.Sight(Pl.Eye, P + V3(0.f, 0.f, 0.15f));
	}

	// --- the world's things ---------------------------------------------------------------------------

	/** Acorns under the oaks, nuts in the gardens, cones under the pines, mussels on the wet sand at low tide, crusts and the
	 *  odd bottle cap in the streets, sea glass on the beach, sticks in the wood: round the camera only, put out again as it
	 *  moves (what nobody touched goes when it is far behind). */
	void FCorvids::Seed(FWorld& W)
	{
		const float Near = 140.f, Far = 230.f;
		std::vector<uint8_t> PerSite(W.Sites.size(), 0);
		int Seeded = 0;
		for (FItem& It : Items)
		{
			if (It.State != EItemState::Ground || !(It.Flags & ItemSeeded) || It.Source < 0 || It.Source >= (int)W.Sites.size())
			{
				continue;
			}
			if ((W.Sites[It.Source].Center() - W.Camera).Len2D() > Far)
			{
				Remove(W, It.Id);
				continue;
			}
			PerSite[It.Source]++;
			Seeded++;
		}
		const bool bLowTide = W.SeaZ < W.MeanSeaZ - 0.15f;
		const bool bDay = W.Hour > W.Sunrise && W.Hour < W.Sunset + 1.f;
		static const EItem Litter[] = { EItem::BottleCap, EItem::BottleCap, EItem::Foil, EItem::Foil, EItem::Coin, EItem::Button, EItem::Key, EItem::Ring };
		for (int S = 0; S < (int)W.Sites.size() && Seeded < 700; S++)
		{
			const FSite& Site = W.Sites[S];
			if ((Site.Center() - W.Camera).Len2D() > Near || (Site.Flags & SiteFeeder))
			{
				continue;
			}
			const uint32_t H = Hash((uint32_t)S * 2654435761u + 977u);
			const int Pick = (int)(H % 100);
			EItem K = EItem::Num;
			int Max = 1;
			switch (Site.Kind)
			{
			case EHabitat::Broadleaf:
				if (Pick < 40 && Season(W, EItem::Acorn) > 0.2f) { K = EItem::Acorn; Max = 3; }
				break;
			case EHabitat::Garden:
				if (Pick < 25 && Season(W, EItem::Walnut) > 0.2f) { K = EItem::Walnut; Max = 2; }
				else if (Pick < 55 && Season(W, EItem::Hazelnut) > 0.2f) { K = EItem::Hazelnut; Max = 2; }
				else if (Pick >= 90 && bDay) { K = EItem::Crust; }
				break;
			case EHabitat::ForestFloor:
				if (Pick < 30 && Season(W, EItem::Hazelnut) > 0.2f) { K = EItem::Hazelnut; }
				else if (Pick < 60) { K = EItem::Stick; }
				else if (Pick >= 85) { K = EItem::PineCone; }
				break;
			case EHabitat::Conifer:
				if (Pick < 50) { K = EItem::PineCone; Max = 2; }
				break;
			case EHabitat::Beach:
				if (Pick < 60 && bLowTide) { K = EItem::Mussel; Max = 2; }
				else if (Pick >= 92) { K = EItem::SeaGlass; }
				break;
			case EHabitat::Street:
				if (Pick < 30 && bDay) { K = EItem::Crust; }
				else if (Pick < 38) { K = Litter[(H >> 8) % 8]; }
				break;
			default:
				break;
			}
			if (K == EItem::Num || PerSite[S] >= Max)
			{
				continue;
			}
			// under a crown, on the patch of a ground site; never in the water
			uint32_t R = (H ^ (uint32_t)(W.Time * 7.0)) | 1u;
			V3 P = W.SpotOn(Site, R);
			if (!IsGroundHabitat(Site.Kind))
			{
				const float A = W.Rand(R) * 2.f * Pi, D = 0.5f + W.Rand(R) * 3.f;
				P += V3(std::cos(A) * D, std::sin(A) * D, 0.f);
			}
			P = GroundAt(W, P.X, P.Y);
			if (P.Z < W.SeaZ + 0.05f)
			{
				continue;
			}
			const int Id = NewItem(W, K, P, EItemState::Ground);
			if (Id < 0)
			{
				return;
			}
			Items[Id].Flags = ItemSeeded;
			Items[Id].Source = S;
			Seeded++;
		}
	}

	void FCorvids::Step(FWorld& W, float Dt)
	{
		Sync(W);
		// the base events this step: a corvid that ate from the palm warms to the player
		for (const FEvent& E : W.Events)
		{
			if (E.Kind == EEvent::Eat && IsCorvid(E.Bird))
			{
				Minds[E.Bird].Bond = std::min(1.f, Minds[E.Bird].Bond + 0.15f);
			}
		}
		for (FItem& It : Items)
		{
			switch (It.State)
			{
			case EItemState::Carried:
			{
				if (It.Holder < 0 || W.Birds[It.Holder].bGone || !IsCorvid(It.Holder) || Minds[It.Holder].Held != It.Id)
				{
					It.State = EItemState::Falling;
					It.Holder = -1;
					break;
				}
				// in the bill: ahead of the body, a little up (the game puts it on the jaw bone of a near body)
				const FBird& B = W.Birds[It.Holder];
				const float L = SpeciesTable()[B.Species].Length;
				const V3 Fwd(std::cos(B.Yaw), std::sin(B.Yaw), 0.f);
				It.Pos = W.RenderPos(B) + Fwd * (L * 0.5f) + V3(0.f, 0.f, IsAirborne(B.Act) ? 0.f : L * 0.3f);
				It.Vel = B.Vel;
				It.Yaw = B.Yaw;
				break;
			}
			case EItemState::Falling:
			{
				It.Vel.Z -= 9.8f * Dt;
				It.Vel.X *= 1.f - 0.3f * Dt;
				It.Vel.Y *= 1.f - 0.3f * Dt;
				It.Pos += It.Vel * Dt;
				const float G = std::max(W.GroundZ(It.Pos.X, It.Pos.Y), W.SeaZ);
				if (It.Pos.Z <= G + 0.02f)
				{
					const float Speed = -It.Vel.Z;
					It.Pos = GroundAt(W, It.Pos.X, It.Pos.Y);
					It.Pos.Z = std::max(It.Pos.Z, G);
					It.Vel = V3();
					It.State = EItemState::Ground;
					It.Since = W.Time;
					if (ItemKind(It.Kind).bHard && !(It.Flags & ItemCracked))
					{
						// a walnut or mussel cracks on the road or a roof (a crow drops it from 5-10 m), hardly ever on grass
						int Site = -1;
						float Best = 1e9f;
						for (int S = 0; S < (int)W.Sites.size(); S++)
						{
							const float D = (W.Sites[S].Center() - It.Pos).Len2D() - W.Sites[S].Radius;
							if (D < Best)
							{
								Best = D;
								Site = S;
							}
						}
						const bool bHard = Site >= 0 && Best < 6.f && IsHard(W.Sites[Site].Kind);
						const float P = (bHard ? (It.Kind == EItem::Mussel ? 0.45f : 0.3f) : 0.04f) * Clamp(Speed / 12.f, 0.3f, 1.3f);
						uint32_t R = Hash((uint32_t)It.Id * 7919u + (uint32_t)(W.Time * 100.0)) | 1u;
						if (W.Rand(R) < P)
						{
							It.Flags |= ItemCracked;
							Count[(int)ECorvidEvent::Crack]++;
							Events.push_back({ ECorvidEvent::Crack, -1, It.Id, It.Pos });
						}
					}
				}
				break;
			}
			case EItemState::Ground:
				if (It.Food <= 0.f)
				{
					Remove(W, It.Id);
				}
				else if (It.Pos.Z < W.SeaZ - 0.05f && !(It.Flags & ItemGift))
				{
					Remove(W, It.Id);     // the tide took it
				}
				break;
			default:
				break;
			}
		}
		SeedTimer -= Dt;
		if (SeedTimer <= 0.f)
		{
			SeedTimer = 4.f;
			Seed(W);
		}
		// now and then a dead herring washes up somewhere on the beach (a feast for the ravens and crows)
		FishTimer -= Dt;
		if (FishTimer <= 0.f)
		{
			uint32_t R = Hash((uint32_t)(W.Time * 13.0)) | 1u;
			FishTimer = 1200.f + W.Rand(R) * 1500.f;
			std::vector<int> Beach;
			for (int S = 0; S < (int)W.Sites.size(); S++)
			{
				if (W.Sites[S].Kind == EHabitat::Beach)
				{
					Beach.push_back(S);
				}
			}
			int Fish = 0;
			for (const FItem& It : Items)
			{
				Fish += It.State == EItemState::Ground && It.Kind == EItem::Fish;
			}
			if (!Beach.empty() && Fish < 2)
			{
				const FSite& S = W.Sites[Beach[(size_t)(W.Rand(R) * Beach.size()) % Beach.size()]];
				V3 P = W.SpotOn(S, R);
				P = GroundAt(W, P.X, P.Y);
				if (P.Z > W.SeaZ)
				{
					WashUp(W, P);
				}
			}
		}
		SocialTimer -= Dt;
		if (SocialTimer <= 0.f)
		{
			SocialStep(W, 1.f - SocialTimer);
			SocialTimer = 1.f;
		}
	}

	/** Once a second: the needs grow, feelings fade slowly, stores are forgotten when there are too many. */
	void FCorvids::SocialStep(FWorld& W, float Dt)
	{
		for (FItem& It : Items)
		{
			if (It.State == EItemState::Taken && W.Time - It.Since > 600.0)
			{
				Remove(W, It.Id);      // (kept a while so its owner finds the hole empty)
			}
		}
		const float Autumn = Smooth((W.DayOfYear - 225.f) / 20.f) * Smooth((340.f - W.DayOfYear) / 20.f);
		for (int Id = 0; Id < (int)Minds.size(); Id++)
		{
			FCorvid& C = Minds[Id];
			if (C.Kind < 0)
			{
				continue;
			}
			FBird& B = W.Birds[Id];
			const FCorvidKind& K = Table[C.Kind];
			// the urge to hoard is an autumn thing (jays bury thousands of acorns in October), the rest of the year a whim
			C.Hoard = Clamp(C.Hoard + Dt * 0.004f * K.Hoard * (0.25f + 0.75f * Autumn), 0.f, 1.f);
			const bool bMateNear = C.Mate >= 0 && (W.Birds[C.Mate].Pos - B.Pos).Len() < 20.f;
			C.Social = Clamp(C.Social + Dt * 0.003f * K.Social * (bMateNear ? 1.f : 0.3f), 0.f, 1.f);
			// play comes when it is fed and nothing threatens (young ravens play most)
			C.Play = Clamp(C.Play + Dt * 0.003f * K.Play * (B.Hunger < 0.5f && B.Alert < 0.2f ? 1.f : 0.2f), 0.f, 1.f);
			// crows remember a face for years; here a grudge fades over game days, a bond more slowly still
			C.Grudge = std::max(0.f, C.Grudge - Dt * 0.00002f);
			C.Bond = std::max(0.f, C.Bond - Dt * 0.00001f);
			// eating near a calm player who brought the food (the feeder, thrown feed) is how a bond starts
			if (W.Player.bPresent && !W.Player.bClaimedBird && B.Act == EAct::Peck && B.Alert < 0.3f && (B.Pos - W.Player.Feet).Len() < 30.f)
			{
				const int F = B.Site >= 0 ? W.FeederAt(B.Site) : -1;
				const bool bPlayerFood = F >= 0 || (C.Task == ETask::Eat && C.Item >= 0 && (Items[C.Item].Flags & ItemPlayer));
				if (bPlayerFood)
				{
					C.Bond = std::min(1.f, C.Bond + Dt * 0.004f);
				}
			}
			// a bird it trusts is one it lets come nearer
			B.Trust = std::max(B.Trust, C.Bond * 0.7f);
			// too many stores: the oldest are forgotten (and grow into oaks, nuts trees, pines)
			while (C.Caches.size() > 12)
			{
				const int Old = C.Caches.front();
				C.Caches.erase(C.Caches.begin());
				if (Old >= 0 && Old < (int)Items.size() && Items[Old].State == EItemState::Cached && Items[Old].Owner == Id)
				{
					Remove(W, Old);
				}
			}
			C.Caches.erase(std::remove_if(C.Caches.begin(), C.Caches.end(), [&](int I)
			{
				return I < 0 || I >= (int)Items.size() || Items[I].Owner != Id
					|| (Items[I].State != EItemState::Cached && Items[I].State != EItemState::Taken);
			}), C.Caches.end());
			C.Spied.erase(std::remove_if(C.Spied.begin(), C.Spied.end(), [&](int I)
			{
				return I < 0 || I >= (int)Items.size() || Items[I].State != EItemState::Cached || Items[I].Owner == Id;
			}), C.Spied.end());
			if (C.Spied.size() > 8)
			{
				C.Spied.erase(C.Spied.begin(), C.Spied.begin() + (C.Spied.size() - 8));
			}
		}
	}

	void FCorvids::Resettle(FWorld& W)
	{
		for (int Id = 0; Id < (int)Minds.size(); Id++)
		{
			FCorvid& C = Minds[Id];
			if (C.Kind < 0)
			{
				continue;
			}
			if (C.Held >= 0)
			{
				Remove(W, C.Held);
			}
			C.Held = -1;
			C.Task = ETask::None;
			C.Cooldown = 5.f;
		}
		for (FItem& It : Items)
		{
			if (It.State == EItemState::Falling || It.State == EItemState::Carried)
			{
				Remove(W, It.Id);
			}
		}
	}

	// --- tasks: plumbing ------------------------------------------------------------------------------

	bool FCorvids::Busy(int Bird) const
	{
		return IsCorvid(Bird) && (Minds[Bird].Task != ETask::None || Minds[Bird].Held >= 0);
	}

	void FCorvids::Start(FWorld& W, FBird& B, FCorvid& C, ETask T, float Len)
	{
		(void)W;
		(void)B;
		C.Task = T;
		C.Step = 0;
		C.TaskTime = 0.f;
		C.TaskLen = Len;
		C.Tries = 0;
		TaskStarts[(int)T]++;
	}

	void FCorvids::End(FWorld& W, FBird& B, FCorvid& C, bool bBackToGroup)
	{
		const ETask Was = C.Task;
		C.Task = ETask::None;
		C.Step = 0;
		C.Item = -1;
		C.Cooldown = 3.f + W.Rand(B.Rng) * 8.f;
		if (Was == ETask::Preen && C.Mate >= 0 && IsCorvid(C.Mate) && Minds[C.Mate].Task == ETask::Preen)
		{
			FBird& M = W.Birds[C.Mate];
			Minds[C.Mate].Task = ETask::None;
			Minds[C.Mate].Step = 0;
			Minds[C.Mate].Cooldown = 5.f;
			W.SetAct(M, EAct::Perch, 1.f + W.Rand(M.Rng) * 2.f);
		}
		const FGroup& G = W.Groups[B.Group];
		if (bBackToGroup && C.Held < 0 && G.Site >= 0 && B.Act != EAct::Flee && B.Act != EAct::Claimed)
		{
			if (B.Site == G.Site && !IsAirborne(B.Act))
			{
				W.SetAct(B, EAct::Perch, 0.5f + W.Rand(B.Rng));
			}
			else
			{
				W.FlyTo(B, G.Site, W.SpotFor(B, G.Site));
			}
		}
		else if (!IsAirborne(B.Act) && B.Act != EAct::Flee)
		{
			W.SetAct(B, EAct::Perch, 0.5f + W.Rand(B.Rng));
		}
	}

	void FCorvids::GoTo(FWorld& W, FBird& B, const V3& P, int Site)
	{
		const bool bGround = !IsAirborne(B.Act) && (B.Site < 0 || IsGroundHabitat(W.Sites[B.Site].Kind) || B.Act == EAct::Forage || B.Act == EAct::Peck);
		if (bGround && (P - B.Pos).Len2D() < 7.f && std::fabs(P.Z - B.Pos.Z) < 1.f)
		{
			B.Spot = P;
			W.SetAct(B, EAct::Forage, 0.f);       // walks or hops there
			return;
		}
		W.FlyTo(B, Site, P);
	}

	void FCorvids::Pick(FWorld& W, FBird& B, FCorvid& C, int Item)
	{
		if (C.Held >= 0)
		{
			Release(W, B, C, false);
		}
		FItem& It = Items[Item];
		It.State = EItemState::Carried;
		It.Holder = B.Id;
		It.Since = W.Time;
		It.Flags &= ~(ItemSeeded | ItemSeen);
		C.Held = Item;
		Emit(ECorvidEvent::Pick, B, Item);
	}

	void FCorvids::Release(FWorld& W, FBird& B, FCorvid& C, bool bFall)
	{
		if (C.Held < 0)
		{
			return;
		}
		FItem& It = Items[C.Held];
		It.Holder = -1;
		It.Since = W.Time;
		if (bFall || IsAirborne(B.Act))
		{
			It.State = EItemState::Falling;
			It.Vel = B.Vel * 0.6f;
		}
		else
		{
			It.State = EItemState::Ground;
			It.Pos = GroundAt(W, It.Pos.X, It.Pos.Y);
			It.Vel = V3();
		}
		Emit(ECorvidEvent::Drop, B, C.Held);
		C.Held = -1;
	}

	/** What to do with what it just picked up. */
	void FCorvids::AfterPick(FWorld& W, FBird& B, FCorvid& C)
	{
		const FCorvidKind& K = Table[C.Kind];
		if (C.Held < 0)
		{
			End(W, B, C);
			return;
		}
		const FItem& It = Items[C.Held];
		const FItemKind& IK = ItemKind(It.Kind);
		const float R = W.Rand(B.Rng);
		const bool bPlayerNear = W.Player.bPresent && !W.Player.bClaimedBird && (W.Player.Feet - B.Pos).Len() < 150.f;
		const bool bForGift = C.Task == ETask::Fetch && C.Other == -2;
		if (bPlayerNear && C.Bond > 0.5f && (bForGift || (W.Time - C.LastGift > 300.0 && (IK.bShiny || IK.bCache) && R < K.Gift * C.Bond)))
		{
			Start(W, B, C, ETask::Gift, 60.f);
			return;
		}
		if (IK.bHard && !(It.Flags & ItemCracked) && K.Crack > 0.15f)
		{
			Start(W, B, C, ETask::Crack, 120.f);
			return;
		}
		if (IK.bStick)
		{
			if (C.Play > 0.4f && K.Play > 0.35f)
			{
				Start(W, B, C, ETask::Play, 60.f);
				C.Step = 10;              // stick play
				C.Anchor = B.Pos;
				return;
			}
			Release(W, B, C, false);
			End(W, B, C);
			return;
		}
		const bool bWantsToHide = IK.bCache && (C.Hoard * K.Hoard > 0.2f || (IK.bShiny && K.Shiny > 0.3f)) && (B.Hunger < 0.6f || R < K.Hoard * 0.7f);
		if (bWantsToHide)
		{
			Start(W, B, C, ETask::Cache, 90.f);
			return;
		}
		if (IK.Food > 0.f && (!IK.bHard || (It.Flags & ItemCracked)))
		{
			Start(W, B, C, ETask::Eat, 20.f);
			C.Item = C.Held;
			return;
		}
		Release(W, B, C, false);
		End(W, B, C);
	}

	/** A place to hide something: open ground (a meadow, a garden, the forest floor, the sand above the tide) MinD..CacheFar
	 *  from here, within the group's range. */
	bool FCorvids::CacheSpot(FWorld& W, const FBird& B, const FCorvid& C, V3& Out, float MinD) const
	{
		const FCorvidKind& K = Table[C.Kind];
		const FGroup& G = W.Groups[B.Group];
		const FSpecies& S = SpeciesTable()[B.Species];
		uint32_t R = B.Rng ^ Hash((uint32_t)(W.Time * 10.0));
		float Best = -1e9f;
		for (int Try = 0; Try < 24; Try++)
		{
			const int I = (int)(W.Rand(R) * W.Sites.size()) % std::max<int>(1, (int)W.Sites.size());
			if (W.Sites.empty())
			{
				break;
			}
			const FSite& Site = W.Sites[I];
			const bool bGood = Site.Kind == EHabitat::Meadow || Site.Kind == EHabitat::Garden || Site.Kind == EHabitat::ForestFloor
				|| (Site.Kind == EHabitat::Beach && S.Forage[(int)EHabitat::Beach] > 0.5f);
			if (!bGood || (Site.Flags & SiteFeeder))
			{
				continue;
			}
			const float D = (Site.Center() - B.Pos).Len2D();
			if (D < MinD || D > K.CacheFar * 2.f || (Site.Center() - G.Home).Len2D() > S.HomeRange)
			{
				continue;
			}
			// rather alone, not too far
			const float Score = -std::fabs(D - K.CacheFar * 0.6f) / K.CacheFar - Site.Occupants * 0.3f + W.Rand(R) * 0.5f;
			if (Score > Best)
			{
				Best = Score;
				uint32_t R2 = R;
				Out = W.SpotOn(Site, R2);
			}
		}
		if (Best <= -1e8f)
		{
			// a test level, or nothing near: a few metres off on the same ground
			const float A = W.Rand(R) * 2.f * Pi, D = std::max(MinD, 8.f) + W.Rand(R) * 10.f;
			Out = B.Pos + V3(std::cos(A) * D, std::sin(A) * D, 0.f);
		}
		Out = GroundAt(W, Out.X, Out.Y);
		return Out.Z > W.SeaZ + 0.05f;
	}

	/** A perch MinD..MaxD from P (a roof, the tower, a tree, a wire), -1 if none. */
	int FCorvids::PerchNear(const FWorld& W, const V3& P, float MinD, float MaxD, uint32_t& Rng) const
	{
		int Best = -1;
		float BestS = -1e9f;
		for (int I = 0; I < (int)W.Sites.size(); I++)
		{
			const FSite& S = W.Sites[I];
			if ((S.Kind != EHabitat::Roof && S.Kind != EHabitat::Tower && S.Kind != EHabitat::Broadleaf && S.Kind != EHabitat::Conifer
				&& S.Kind != EHabitat::Wire && S.Kind != EHabitat::Garden) || (S.Flags & SiteFeeder))
			{
				continue;
			}
			const float D = (S.Center() - P).Len2D();
			if (D < MinD || D > MaxD)
			{
				continue;
			}
			const float Score = -D * 0.05f + (S.Center().Z - P.Z) * 0.08f + W.Rand(Rng) * 0.6f;
			if (Score > BestS)
			{
				BestS = Score;
				Best = I;
			}
		}
		return Best;
	}

	/** The nearest hard ground for dropping a shell on (a street, a roof, the tower), -1 if none within reach. */
	int FCorvids::HardGround(const FWorld& W, const V3& P) const
	{
		int Best = -1;
		float BestD = 450.f;
		for (int I = 0; I < (int)W.Sites.size(); I++)
		{
			const FSite& S = W.Sites[I];
			if (S.Kind != EHabitat::Street)
			{
				continue;
			}
			const float D = (S.Center() - P).Len2D();
			if (D < BestD)
			{
				BestD = D;
				Best = I;
			}
		}
		return Best;
	}

	/** Other clever corvids near enough to see where it hides something (and remember it). */
	bool FCorvids::Watched(FWorld& W, const FBird& B, int* Out) const
	{
		int N = 0;
		for (int I = 0; I < 3; I++)
		{
			Out[I] = -1;
		}
		W.ForNear(B.Pos, 40.f, [&](int I)
		{
			if (I == B.Id || N >= 3 || !IsCorvid(I))
			{
				return;
			}
			const FBird& O = W.Birds[I];
			if (O.bGone || O.Act == EAct::Sleep || O.Act == EAct::Claimed || Table[Minds[I].Kind].Clever < 0.3f || I == Minds[B.Id].Mate)
			{
				return;
			}
			Out[N++] = I;
		});
		return N > 0;
	}

	void FCorvids::HideAt(FWorld& W, FBird& B, FCorvid& C)
	{
		if (C.Held < 0)
		{
			return;
		}
		FItem& It = Items[C.Held];
		It.State = EItemState::Cached;
		It.Holder = -1;
		It.Owner = B.Id;
		It.Since = W.Time;
		It.Pos = GroundAt(W, B.Pos.X + std::cos(B.Yaw) * 0.15f, B.Pos.Y + std::sin(B.Yaw) * 0.15f);
		int Seen[3];
		Watched(W, B, Seen);
		for (int I = 0; I < 3; I++)
		{
			It.Watchers[I] = Seen[I];
			if (Seen[I] >= 0)
			{
				Minds[Seen[I]].Spied.push_back(It.Id);
			}
		}
		if (PlayerSees(W, It.Pos, 50.f))
		{
			It.Flags |= ItemSeen;
		}
		C.Caches.push_back(It.Id);
		C.Hoard = std::max(0.f, C.Hoard - 0.25f);
		Emit(ECorvidEvent::Cache, B, It.Id);
		C.Held = -1;
	}

	/** Others of its kind near it pick up how it feels about the player (crows learn dangerous faces from each other). */
	void FCorvids::Learn(FWorld& W, const FBird& From, float Grudge, float Bond)
	{
		W.ForNear(From.Pos, 70.f, [&](int I)
		{
			if (I == From.Id || !IsCorvid(I) || W.Birds[I].Act == EAct::Sleep || W.Birds[I].bGone)
			{
				return;
			}
			FCorvid& O = Minds[I];
			const float K = Table[O.Kind].Social * (W.Birds[I].Species == From.Species ? 1.f : 0.5f);
			if (Grudge > O.Grudge)
			{
				O.Grudge = std::min(Grudge * 0.75f, O.Grudge + 0.06f * K);
			}
			if (Bond > O.Bond)
			{
				O.Bond = std::min(Bond * 0.5f, O.Bond + 0.01f * K);
			}
		});
	}

	// --- choosing -------------------------------------------------------------------------------------

	void FCorvids::Choose(FWorld& W, FBird& B, FCorvid& C)
	{
		const FCorvidKind& K = Table[C.Kind];
		const FSpecies& S = SpeciesTable()[B.Species];
		const FGroup& G = W.Groups[B.Group];
		const FPlayer& P = W.Player;
		const bool bPlayer = P.bPresent && !P.bClaimedBird;
		const float PlayerD = bPlayer ? (B.Pos - P.Feet).Len() : 1e9f;
		const bool bGround = !IsAirborne(B.Act) && (B.Site < 0 || IsGroundHabitat(W.Sites[B.Site].Kind));
		C.Cooldown = 2.f + W.Rand(B.Rng) * 4.f;

		ETask Best = ETask::None;
		float BestScore = 0.35f;
		int BestItem = -1, BestOther = -1;
		auto Offer = [&](ETask T, float Score, int Item = -1, int Other = -1)
		{
			Score *= 0.75f + 0.5f * W.Rand(B.Rng);
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = T;
				BestItem = Item;
				BestOther = Other;
			}
		};

		// what it already holds comes first
		if (C.Held >= 0)
		{
			const FItemKind& IK = ItemKind(Items[C.Held].Kind);
			if (IK.Food > 0.f && B.Hunger > 0.6f && (!IK.bHard || (Items[C.Held].Flags & ItemCracked)))
			{
				Start(W, B, C, ETask::Eat, 20.f);
				C.Item = C.Held;
			}
			else if (IK.bHard && !(Items[C.Held].Flags & ItemCracked) && K.Crack > 0.15f)
			{
				Start(W, B, C, ETask::Crack, 120.f);
			}
			else if (IK.bCache)
			{
				Start(W, B, C, ETask::Cache, 90.f);
			}
			else
			{
				Release(W, B, C, false);
			}
			return;
		}
		// the player: a grudge is shouted, a bond followed; food held out is grabbed by the bold
		if (bPlayer && C.Grudge > 0.35f && PlayerD < 100.f && (B.bNoticed || PlayerD < 60.f) && W.Time - C.LastScold > 20.0)
		{
			Offer(ETask::Scold, 1.2f * C.Grudge + 0.3f);
		}
		if (bPlayer && P.bOffer && K.Steal > 0.45f && (B.Pos - P.Palm).Len() < 14.f && B.Trust > 0.2f && B.Alert < 0.45f && B.Hunger > 0.2f
			&& (B.Species >= 64 || P.OfferLike[B.Species] > 0.3f) && W.Approacher != B.Id)
		{
			Offer(ETask::Snatch, K.Steal * (0.6f + B.Trust));
		}
		if (bPlayer && C.Bond > 0.6f && PlayerD < 70.f && W.Time - C.LastFollow > 600.0)
		{
			Offer(ETask::Follow, 0.4f + 0.4f * C.Bond);
		}
		if (bPlayer && C.Bond > 0.5f && PlayerD < 150.f && W.Time - C.LastGift > 300.0)
		{
			// a present: something shiny or a nut it can find near by
			Offer(ETask::Fetch, 0.3f + K.Gift * C.Bond, -2);
		}
		// things lying about: what it likes of them, near enough to notice
		if (B.Tier <= 1)
		{
			const float Look = IsAirborne(B.Act) ? 30.f : 22.f + 10.f * S.Curiosity;
			for (const FItem& It : Items)
			{
				if (It.State != EItemState::Ground)
				{
					continue;
				}
				const float D = (It.Pos - B.Pos).Len();
				const FItemKind& IK = ItemKind(It.Kind);
				const float Range = It.Kind == EItem::Fish ? (S.Id == "Raven" ? 600.f : 350.f) : (IK.bBig ? 60.f : Look);
				if (D > Range)
				{
					continue;
				}
				float V = 0.f;
				if (IK.bBig)
				{
					V = B.Hunger * 1.3f + 0.2f;
				}
				else if (IK.Food > 0.f)
				{
					V = B.Hunger * IK.Food * 2.5f + (IK.bCache ? C.Hoard * K.Hoard : 0.f) + (IK.bHard ? K.Crack * 0.3f : 0.f);
				}
				else if (IK.bShiny)
				{
					V = K.Shiny * (0.5f + 0.5f * S.Curiosity);
				}
				else if (IK.bStick)
				{
					V = C.Play * K.Play * 0.6f;
				}
				if (It.Flags & ItemPlayer)
				{
					V += 0.2f;        // new, and the player's: worth a look
				}
				V *= 1.f - 0.5f * D / Range;
				Offer(IK.bBig ? (It.Kind == EItem::Fish ? ETask::Feast : ETask::Eat) : ETask::Fetch, V, It.Id);
			}
			// another bird flying with something in its bill: chase it until it drops it
			if (S.bKlepto && K.Steal > 0.5f && B.Hunger > 0.3f)
			{
				W.ForNear(B.Pos, 40.f, [&](int I)
				{
					if (I == B.Id || !IsCorvid(I) || Minds[I].Held < 0 || I == C.Mate)
					{
						return;
					}
					const FBird& O = W.Birds[I];
					if (SpeciesTable()[O.Species].Dominance >= S.Dominance + 5 || O.Group == B.Group)
					{
						return;
					}
					const FItemKind& IK = ItemKind(Items[Minds[I].Held].Kind);
					Offer(ETask::Steal, K.Steal * (IK.Food * B.Hunger * 2.f + (IK.bShiny ? K.Shiny * 0.6f : 0.f)), Minds[I].Held, I);
				});
			}
			// a cat or a dog lying or strolling: pull its tail
			if (K.Tease > 0.4f && bGround && !W.Beasts.empty())
			{
				for (int M = 0; M < (int)W.Beasts.size(); M++)
				{
					const FBeast& Bst = W.Beasts[M];
					if ((Bst.Pos - B.Pos).Len() < 35.f && Bst.Vel.Len2D() < 1.2f)
					{
						Offer(ETask::Tease, K.Tease * 0.55f * (0.5f + C.Play), -1, M);
					}
				}
			}
		}
		// its own stores when hungry (most of all in winter), stores it saw others make
		if (B.Hunger > 0.6f && !C.Caches.empty())
		{
			Offer(ETask::Retrieve, B.Hunger * (W.DayOfYear > 330.f || W.DayOfYear < 80.f ? 1.2f : 0.7f));
		}
		if (!C.Spied.empty() && K.Clever > 0.4f)
		{
			const FItem& It = Items[C.Spied.back()];
			const bool bOwnerAway = It.Owner < 0 || (W.Birds[It.Owner].Pos - It.Pos).Len() > 50.f || W.Birds[It.Owner].Act == EAct::Sleep;
			if (bOwnerAway)
			{
				Offer(ETask::Pilfer, K.Clever * (0.4f + B.Hunger * 0.6f));
			}
		}
		// pecking out acorns, nuts, cones, mussels where the group feeds (to eat, or above all in autumn to hide)
		if (G.Plan == 1 && B.Site >= 0 && B.Site == G.Site && K.Harvest)
		{
			const EHabitat Here = W.Sites[B.Site].Kind;
			for (int I = 0; I < NumItems; I++)
			{
				const EItem It = (EItem)I;
				if (!(K.Harvest & Bit(It)) || !GrowsIn(It, Here))
				{
					continue;
				}
				if (It == EItem::Mussel && !(W.SeaZ < W.MeanSeaZ - 0.1f))
				{
					continue;            // only at low tide
				}
				Offer(ETask::Harvest, Season(W, It) * (C.Hoard * K.Hoard * 0.6f + B.Hunger * 0.3f + 0.1f), I);
			}
		}
		// its mate, resting near: preen each other
		if (C.Mate >= 0 && G.Plan == 0 && !IsAirborne(B.Act) && C.Social > 0.5f)
		{
			const FBird& M = W.Birds[C.Mate];
			if (!M.bGone && !IsAirborne(M.Act) && M.Act != EAct::Sleep && (M.Pos - B.Pos).Len() < 10.f && !Busy(C.Mate))
			{
				Offer(ETask::Preen, C.Social * 0.7f);
			}
		}
		// a stiff wind over the cliff, the tower, the roofs: jackdaws, ravens, crows play in it
		const float Wind = W.Wind.Len2D();
		if (K.Play > 0.35f && Wind > 7.f && G.Plan != 1 && W.WindStress(S) < 0.9f)
		{
			Offer(ETask::Play, C.Play * K.Play * Clamp((Wind - 6.f) / 6.f, 0.f, 1.f) * 1.1f);
		}

		if (Best == ETask::None)
		{
			return;
		}
		C.Item = BestItem;
		C.Other = BestOther;
		switch (Best)
		{
		case ETask::Fetch:
			if (BestItem == -2)
			{
				// a present: the nearest shiny thing or nut lying about, else something it pecks out where it is
				int Found = -1;
				float FoundD = 60.f;
				for (const FItem& It : Items)
				{
					const FItemKind& IK = ItemKind(It.Kind);
					if (It.State == EItemState::Ground && (IK.bShiny || (IK.bCache && IK.Food > 0.f)) && !(It.Flags & (ItemGift | ItemPlayer)))
					{
						const float D = (It.Pos - B.Pos).Len();
						if (D < FoundD)
						{
							FoundD = D;
							Found = It.Id;
						}
					}
				}
				if (Found < 0)
				{
					return;
				}
				C.Item = Found;
				C.Other = -2;           // (AfterPick: it is for the player)
			}
			Start(W, B, C, ETask::Fetch, 60.f);
			break;
		case ETask::Feast: Start(W, B, C, ETask::Feast, 150.f + W.Rand(B.Rng) * 150.f); break;
		case ETask::Eat: Start(W, B, C, ETask::Eat, 30.f + W.Rand(B.Rng) * 40.f); C.Step = 5; break;     // at a pile of feed
		case ETask::Steal: Start(W, B, C, ETask::Steal, 12.f); break;
		case ETask::Tease: Start(W, B, C, ETask::Tease, 40.f); break;
		case ETask::Retrieve:
		{
			// the nearest of its stores
			float BestD = 1e9f;
			for (int I : C.Caches)
			{
				if (Items[I].State == EItemState::Cached || Items[I].State == EItemState::Taken || Items[I].State == EItemState::Gone)
				{
					const float D = (Items[I].Pos - B.Pos).Len2D();
					if (D < BestD)
					{
						BestD = D;
						C.Item = I;
					}
				}
			}
			Start(W, B, C, ETask::Retrieve, 90.f);
			break;
		}
		case ETask::Pilfer: C.Item = C.Spied.back(); Start(W, B, C, ETask::Pilfer, 90.f); break;
		case ETask::Harvest: Start(W, B, C, ETask::Harvest, 20.f); break;
		case ETask::Preen: Start(W, B, C, ETask::Preen, 10.f + W.Rand(B.Rng) * 15.f); break;
		case ETask::Scold: Start(W, B, C, ETask::Scold, 40.f + W.Rand(B.Rng) * 50.f); C.LastScold = W.Time; break;
		case ETask::Follow: Start(W, B, C, ETask::Follow, 120.f + W.Rand(B.Rng) * 180.f); C.LastFollow = W.Time; break;
		case ETask::Snatch: Start(W, B, C, ETask::Snatch, 15.f); break;
		case ETask::Play: Start(W, B, C, ETask::Play, 30.f + W.Rand(B.Rng) * 60.f); break;
		default: break;
		}
	}

	// --- thinking -------------------------------------------------------------------------------------

	bool FCorvids::Think(FWorld& W, FBird& B, float Dt)
	{
		Sync(W);
		FCorvid* CP = Mind(B.Id);
		if (!CP || B.bGone || B.Act == EAct::Claimed)
		{
			return false;
		}
		FCorvid& C = *CP;
		const FGroup& G = W.Groups[B.Group];
		const FSpecies& S = SpeciesTable()[B.Species];
		C.Cooldown -= Dt;
		// the night, the murmuration, a bird coming to the hand: no tasks (what it carries goes into its throat pouch)
		const bool bNight = G.Plan == 2 || G.Plan == 3 || G.Plan == 4 || B.Act == EAct::Sleep;
		if (bNight || (B.Act == EAct::Approach && C.Task != ETask::Snatch) || B.Act == EAct::Eat)
		{
			if (C.Task != ETask::None)
			{
				End(W, B, C, false);
			}
			if (C.Held >= 0 && bNight)
			{
				Remove(W, C.Held);
				C.Held = -1;
			}
			return false;
		}
		// a crow that knows the player (a friend, or an enemy it means to shout at) doesn't circle out there for minutes because
		// they turned up: once calm it comes back down
		if (B.Act == EAct::Flee && B.Site < 0 && (C.Bond > 0.5f || C.Grudge > 0.35f) && B.Alert < 0.3f && W.Player.bPresent && (B.Pos - B.FlyFrom).Len2D() > 25.f)
		{
			if (G.Site >= 0)
			{
				W.FlyTo(B, G.Site, W.SpotFor(B, G.Site));
			}
			else
			{
				W.FlyTo(B, -1, GroundAt(W, B.Pos.X, B.Pos.Y));
			}
			return true;
		}
		if (C.Task == ETask::None)
		{
			// a crow with a grudge that has the player in its eye shouts at them instead of only watching
			const bool bAngry = C.Grudge > 0.35f && W.Player.bPresent && B.Act == EAct::Watch;
			if (C.Cooldown > 0.f || B.Tier > 2 || B.Act == EAct::Flee || ((B.Act == EAct::Watch || B.Alert > 0.45f) && !bAngry))
			{
				return false;
			}
			if (IsAirborne(B.Act) && C.Held < 0)
			{
				return false;              // decides when it has landed
			}
			Choose(W, B, C);
			if (C.Task == ETask::None)
			{
				return false;
			}
		}
		// fear first: a watching bird watches (BirdSim), the task waits
		if (B.Act == EAct::Flee || (B.Act == EAct::Watch && C.Task != ETask::Scold))
		{
			return false;
		}
		C.TaskTime += Dt;
		// its feelings turned while it was at it: no present for an enemy, no scolding a friend
		if (((C.Task == ETask::Gift || C.Task == ETask::Follow) && C.Bond < 0.3f) || (C.Task == ETask::Scold && C.Grudge < 0.2f))
		{
			End(W, B, C);
			return true;
		}
		if (C.TaskTime > C.TaskLen)
		{
			if (C.Held >= 0 && C.Task != ETask::Cache && C.Task != ETask::Gift)
			{
				Release(W, B, C, false);
			}
			End(W, B, C);
			return true;
		}
		const bool bAir = IsAirborne(B.Act);
		const bool bSettled = !bAir && B.Act != EAct::Forage && B.ActTime > B.ActLen;
		const FPlayer& P = W.Player;
		// left circling or hovering by an act its task doesn't fly (a soar, a mob it was in before): down to the flock
		const bool bFlown = (C.Task == ETask::Play && C.Step != 0 && C.Step != 10) || (C.Task == ETask::Crack && (C.Step == 1 || C.Step == 2))
			|| (C.Task == ETask::Scold && C.Step == 3) || C.Task == ETask::Steal || C.Task == ETask::Snatch;
		if (bAir && !bFlown && (B.Act == EAct::Soar || B.Act == EAct::Mob || B.Act == EAct::Hawk))
		{
			if (G.Site >= 0)
			{
				W.FlyTo(B, G.Site, W.SpotFor(B, G.Site));
			}
			else
			{
				W.FlyTo(B, -1, GroundAt(W, B.Pos.X, B.Pos.Y));
			}
			return true;
		}
		// walked (rather than flew) to the place it was going: the same as landing there
		if (!bAir && B.Act == EAct::Peck && B.ActTime < 0.2f && C.Step == 1 && (C.Task == ETask::Cache || C.Task == ETask::Gift)
			&& Near(B, C.Goal, 0.8f))
		{
			Arrive(W, B);
			return true;
		}
		switch (C.Task)
		{
		case ETask::Fetch:
		{
			FItem* It = C.Item >= 0 && C.Item < (int)Items.size() ? &Items[C.Item] : nullptr;
			if (!It || It->State != EItemState::Ground)
			{
				// someone was quicker: if a bird carries it off, a bold one gives chase
				if (It && It->State == EItemState::Carried && It->Holder >= 0 && S.bKlepto && W.Rand(B.Rng) < Table[C.Kind].Steal)
				{
					C.Other = It->Holder;
					C.Task = ETask::Steal;
					C.TaskTime = 0.f;
					C.TaskLen = 12.f;
				}
				else
				{
					End(W, B, C);
				}
				return true;
			}
			if (!bAir && Near(B, It->Pos, 0.45f))
			{
				Pick(W, B, C, It->Id);
				AfterPick(W, B, C);
			}
			else if (!bAir && B.Act != EAct::Forage)
			{
				const V3 To = (B.Pos - It->Pos).Flat().Norm();
				GoTo(W, B, It->Pos + To * 0.2f);
			}
			return true;
		}
		case ETask::Harvest:
			if (C.Step == 0 && !bAir)
			{
				W.SetAct(B, EAct::Peck, 2.f + W.Rand(B.Rng) * 3.f);
				C.Step = 1;
			}
			else if (C.Step == 1 && bSettled)
			{
				const EItem K = (EItem)std::max(0, C.Item);
				const int Id = NewItem(W, K, B.Pos, EItemState::Ground);
				if (Id < 0)
				{
					End(W, B, C);
					return true;
				}
				Pick(W, B, C, Id);
				AfterPick(W, B, C);
			}
			return true;
		case ETask::Eat:
		{
			// Step 5: at a pile of the player's feed (several birds eat at it at once); else what it holds
			FItem* It = C.Item >= 0 && C.Item < (int)Items.size() ? &Items[C.Item] : nullptr;
			if (!It || It->State == EItemState::Gone || It->State == EItemState::Taken || It->Food <= 0.f)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 5)
			{
				if (It->State != EItemState::Ground)
				{
					End(W, B, C);
					return true;
				}
				const float D = (B.Pos - It->Pos).Len2D();
				if (D > 0.55f || bAir)
				{
					if (!bAir && B.Act != EAct::Forage)
					{
						const float A = W.Rand(B.Rng) * 2.f * Pi;
						GoTo(W, B, It->Pos + V3(std::cos(A), std::sin(A), 0.f) * (0.15f + 0.25f * W.Rand(B.Rng)));
					}
					return true;
				}
				if (B.Act != EAct::Peck || B.ActTime > B.ActLen)
				{
					W.SetAct(B, EAct::Peck, 0.6f + W.Rand(B.Rng) * 1.2f);
					B.Yaw = std::atan2(It->Pos.Y - B.Pos.Y, It->Pos.X - B.Pos.X);
				}
				const float Bite = Dt * 0.012f;
				It->Food -= Bite;
				B.Hunger = std::max(0.f, B.Hunger - Bite * ItemKind(It->Kind).Food * 1.5f);
				// a jay or nutcracker fills its throat pouch and flies off to hide it
				if (Table[C.Kind].Hoard > 0.8f && C.Hoard > 0.3f && W.Rand(B.Rng) < Dt * 0.15f)
				{
					const int Id = NewItem(W, EItem::FeedPile, B.Pos, EItemState::Ground);
					if (Id >= 0)
					{
						It->Food -= 0.1f;
						Items[Id].Food = 0.1f;
						Items[Id].Flags = ItemPlayer;
						Pick(W, B, C, Id);
						Start(W, B, C, ETask::Cache, 90.f);
					}
				}
				else if (B.Hunger < 0.1f && W.Rand(B.Rng) < Dt * 0.1f)
				{
					End(W, B, C);
				}
				return true;
			}
			if (!bAir && (B.Act != EAct::Peck || B.ActTime > B.ActLen))
			{
				W.SetAct(B, EAct::Peck, 0.8f + W.Rand(B.Rng) * 1.5f);
			}
			if (!bAir)
			{
				It->Food -= Dt / 8.f;          // a nut or a crust takes a few seconds
				if (It->Food <= 0.f)
				{
					B.Hunger = std::max(0.f, B.Hunger - ItemKind(It->Kind).Food);
					Emit(ECorvidEvent::Eat, B, It->Id);
					if (C.Held == It->Id)
					{
						C.Held = -1;
					}
					Remove(W, It->Id);
					End(W, B, C);
				}
			}
			return true;
		}
		case ETask::Cache:
			if (C.Held < 0)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				V3 Spot;
				if (!CacheSpot(W, B, C, Spot, 6.f))
				{
					Release(W, B, C, false);
					End(W, B, C);
					return true;
				}
				C.Goal = Spot;
				C.Step = 1;
				GoTo(W, B, Spot);
			}
			else if (C.Step == 2 && bSettled)
			{
				HideAt(W, B, C);
				End(W, B, C);
			}
			return true;
		case ETask::Retrieve:
		case ETask::Pilfer:
		{
			FItem* It = C.Item >= 0 && C.Item < (int)Items.size() ? &Items[C.Item] : nullptr;
			if (!It)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				C.Goal = It->Pos;
				C.Step = 1;
				GoTo(W, B, It->Pos + V3(0.2f, 0.f, 0.f));
			}
			else if (C.Step == 1 && !bAir && B.Act != EAct::Forage && Near(B, C.Goal, 0.6f))
			{
				W.SetAct(B, EAct::Peck, 1.5f + W.Rand(B.Rng) * 2.f);      // digging
				C.Step = 2;
			}
			else if (C.Step == 2 && bSettled)
			{
				C.Caches.erase(std::remove(C.Caches.begin(), C.Caches.end(), It->Id), C.Caches.end());
				C.Spied.erase(std::remove(C.Spied.begin(), C.Spied.end(), It->Id), C.Spied.end());
				if (It->State == EItemState::Cached)
				{
					const int Owner = It->Owner;
					Pick(W, B, C, It->Id);
					if (C.Task == ETask::Pilfer)
					{
						Emit(ECorvidEvent::Rob, B, It->Id);
						// the owner, if it sees, goes after the thief
						if (Owner >= 0 && IsCorvid(Owner) && Minds[Owner].Task == ETask::None && (W.Birds[Owner].Pos - B.Pos).Len() < 40.f
							&& W.Birds[Owner].Act != EAct::Sleep)
						{
							FBird& O = W.Birds[Owner];
							Start(W, O, Minds[Owner], ETask::Steal, 12.f);
							Minds[Owner].Other = B.Id;
							Minds[Owner].Item = It->Id;
						}
					}
					else
					{
						Emit(ECorvidEvent::Dig, B, It->Id);
					}
					AfterPick(W, B, C);
				}
				else
				{
					// gone: robbed. If the player took it and is there, that is remembered.
					Emit(ECorvidEvent::Dig, B, -1);
					if (It->State == EItemState::Taken && C.Task == ETask::Retrieve && P.bPresent && (P.Feet - B.Pos).Len() < 40.f)
					{
						C.Grudge = std::min(1.f, C.Grudge + 0.3f);
						Learn(W, B, C.Grudge, 0.f);
					}
					End(W, B, C);
				}
			}
			return true;
		}
		case ETask::Crack:
		{
			if (C.Step == 0)
			{
				if (C.Held < 0)
				{
					End(W, B, C);
					return true;
				}
				// up over the road, then let go
				const int Road = HardGround(W, B.Pos);
				V3 Over;
				if (Road >= 0)
				{
					uint32_t R = B.Rng;
					Over = W.SpotOn(W.Sites[Road], R);
				}
				else
				{
					Over = B.Pos;
				}
				Over = GroundAt(W, Over.X, Over.Y);
				C.Anchor = Over;
				C.Goal = Over + V3(0.f, 0.f, 6.f + W.Rand(B.Rng) * 5.f);
				C.Step = 1;
				W.FlyTo(B, -1, C.Goal);
			}
			else if (C.Step == 3 && !bAir)
			{
				FItem& It = Items[C.Item];
				if (It.State != EItemState::Ground)
				{
					End(W, B, C);         // somebody took it
					return true;
				}
				if (!Near(B, It.Pos, 0.5f))
				{
					if (B.Act != EAct::Forage)
					{
						GoTo(W, B, It.Pos + (B.Pos - It.Pos).Flat().Norm() * 0.2f);
					}
					return true;
				}
				Pick(W, B, C, It.Id);
				if (It.Flags & ItemCracked)
				{
					Start(W, B, C, ETask::Eat, 25.f);
					C.Item = C.Held;
				}
				else if (++C.Tries < 6)
				{
					C.Step = 0;
				}
				else
				{
					Release(W, B, C, false);
					End(W, B, C);
				}
			}
			return true;
		}
		case ETask::Snatch:
			if (!P.bOffer || !P.bPresent)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				B.FlyFrom = B.Pos;
				B.Spot = P.Palm;
				B.Site = -1;
				B.Vel = (P.Palm - B.Pos).Flat().Norm() * 3.f + V3(0.f, 0.f, 2.f);
				W.SetAct(B, EAct::Approach, 0.f);       // in fast, low, at the hand (FlightGoal grabs)
				C.Step = 1;
			}
			else if (C.Step == 2 && !bAir)
			{
				// landed out of reach: hide it or eat it
				if (C.Held >= 0)
				{
					AfterPick(W, B, C);
				}
				else
				{
					End(W, B, C);
				}
			}
			return true;
		case ETask::Steal:
		{
			const int V = C.Other;
			FItem* It = C.Item >= 0 && C.Item < (int)Items.size() ? &Items[C.Item] : nullptr;
			if (V < 0 || !It || It->State == EItemState::Gone || It->State == EItemState::Cached || It->State == EItemState::Taken
				|| (It->State == EItemState::Carried && It->Holder != V))
			{
				End(W, B, C);
				return true;
			}
			if (It->State == EItemState::Ground && !bAir)
			{
				if (Near(B, It->Pos, 0.5f))
				{
					Pick(W, B, C, It->Id);
					Emit(ECorvidEvent::Steal, B, It->Id);
					AfterPick(W, B, C);
				}
				else if (B.Act != EAct::Forage)
				{
					GoTo(W, B, It->Pos + (B.Pos - It->Pos).Flat().Norm() * 0.2f);
				}
				return true;
			}
			if (C.Step == 0)
			{
				B.Target = V;
				B.FlyFrom = B.Pos;
				W.SetAct(B, EAct::Chase, 6.f + W.Rand(B.Rng) * 4.f);
				if (!bAir)
				{
					B.Vel = V3(0.f, 0.f, 2.5f);
				}
				C.Step = 1;
				W.Emit(EEvent::ChaseStart, B);
				W.Emit(EEvent::Call, B);
			}
			else if (B.Act != EAct::Chase && It->State == EItemState::Carried)
			{
				End(W, B, C);              // gave up
			}
			return true;
		}
		case ETask::Gift:
		{
			if (C.Held < 0 || !P.bPresent || P.bClaimedBird || (P.Feet - B.Pos).Len() > 100.f)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				// in front of the player, a few metres off, where they will see it land
				const V3 F = P.Facing.Flat().Norm();
				const V3 Side(-F.Y, F.X, 0.f);
				V3 Spot = P.Feet + F * (2.5f + W.Rand(B.Rng) * 2.f) + Side * ((W.Rand(B.Rng) - 0.5f) * 3.f);
				Spot = GroundAt(W, Spot.X, Spot.Y);
				C.Goal = Spot;
				C.Step = 1;
				GoTo(W, B, Spot);
			}
			else if (C.Step == 1 && bAir && (C.Goal - P.Feet).Len2D() > 8.f)
			{
				C.Step = 0;        // they walked on: land where they are now
			}
			else if (C.Step == 2 && bSettled)
			{
				const int Id = C.Held;
				Release(W, B, C, false);
				Items[Id].Flags |= ItemGift;
				Items[Id].Owner = B.Id;
				C.LastGift = W.Time;
				Emit(ECorvidEvent::Gift, B, Id);
				W.Emit(EEvent::Call, B);
				B.Look = P.Eye;
				B.bLook = true;
				W.SetAct(B, EAct::Perch, 1.5f + W.Rand(B.Rng) * 1.5f);
				C.Step = 3;
			}
			else if (C.Step == 3 && bSettled)
			{
				End(W, B, C);
			}
			return true;
		}
		case ETask::Scold:
		{
			const float D = P.bPresent ? (B.Pos - P.Feet).Len() : 1e9f;
			if (!P.bPresent || P.bClaimedBird || D > 90.f)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				// a perch over them, else straight at their head
				uint32_t R = B.Rng;
				const int Perch = PerchNear(W, P.Feet, 6.f, 30.f, R);
				if (Perch >= 0 && W.Rand(B.Rng) < 0.7f)
				{
					C.Other = Perch;
					C.Step = 1;
					W.FlyTo(B, Perch, W.SpotFor(B, Perch));
				}
				else
				{
					C.Step = 3;
					B.FlyFrom = B.Pos;
					W.SetAct(B, EAct::Mob, 8.f + W.Rand(B.Rng) * 7.f);
					B.Vel = (P.Feet - B.Pos).Flat().Norm() * 3.f + V3(0.f, 0.f, 2.5f);
				}
			}
			else if (C.Step == 2)
			{
				// perched, calling at them; now and then a dive past their head
				B.Look = P.Eye;
				B.bLook = true;
				if (B.ActTime > B.ActLen)
				{
					W.SetAct(B, EAct::Perch, 1.f + W.Rand(B.Rng) * 1.5f);
					Emit(ECorvidEvent::Scold, B, -1);
					W.Emit(EEvent::Call, B);
					Learn(W, B, C.Grudge, 0.f);
					if (C.Grudge > 0.6f && W.Rand(B.Rng) < 0.2f)
					{
						C.Step = 3;
						B.FlyFrom = B.Pos;
						W.SetAct(B, EAct::Mob, 6.f + W.Rand(B.Rng) * 6.f);
						B.Vel = (P.Feet - B.Pos).Flat().Norm() * 3.f + V3(0.f, 0.f, 2.f);
					}
					else if (D > 35.f)
					{
						C.Step = 0;       // they walked off: after them
					}
				}
			}
			else if (C.Step == 3)
			{
				if (W.Rand(B.Rng) < Dt * 0.7f)
				{
					Emit(ECorvidEvent::Scold, B, -1);
					W.Emit(EEvent::Call, B);
					Learn(W, B, C.Grudge, 0.f);
				}
				if (B.ActTime > B.ActLen)
				{
					// up to a perch near them again (or back to the flock, and round once more)
					uint32_t R = B.Rng;
					const int Perch = PerchNear(W, P.Feet, 6.f, 30.f, R);
					const int To = Perch >= 0 ? Perch : G.Site;
					if (To >= 0)
					{
						W.FlyTo(B, To, W.SpotFor(B, To));
					}
					else
					{
						W.FlyTo(B, -1, GroundAt(W, B.Pos.X + 8.f, B.Pos.Y));
					}
					C.Step = Perch >= 0 ? 1 : 4;
				}
			}
			else if (C.Step == 4 && !bAir)
			{
				C.Step = 0;
			}
			return true;
		}
		case ETask::Follow:
		{
			const float D = P.bPresent ? (B.Pos - P.Feet).Len2D() : 1e9f;
			if (!P.bPresent || P.bClaimedBird || D > 100.f)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 1 && !bAir && B.Act != EAct::Forage)
			{
				C.Step = 2;
			}
			if ((C.Step == 0 || (C.Step == 2 && D > 26.f)) && !bAir)
			{
				uint32_t R = B.Rng;
				const int Perch = PerchNear(W, P.Feet, 5.f, 20.f, R);
				if (Perch >= 0 && W.Rand(B.Rng) < 0.6f)
				{
					W.FlyTo(B, Perch, W.SpotFor(B, Perch));
				}
				else
				{
					// on the ground, a few hops from them
					const float A = W.Rand(B.Rng) * 2.f * Pi;
					GoTo(W, B, GroundAt(W, P.Feet.X + std::cos(A) * 7.f, P.Feet.Y + std::sin(A) * 7.f));
				}
				C.Step = 1;
			}
			else if (C.Step == 2 && bSettled)
			{
				W.SetAct(B, W.Rand(B.Rng) < 0.3f ? EAct::Preen : EAct::Perch, 2.f + W.Rand(B.Rng) * 4.f);
				if (W.Rand(B.Rng) < 0.5f)
				{
					B.Look = P.Eye;
					B.bLook = true;
				}
			}
			return true;
		}
		case ETask::Preen:
		{
			FBird& M = W.Birds[C.Mate];
			if (M.bGone || IsAirborne(M.Act) || M.Act == EAct::Flee || (M.Pos - B.Pos).Len() > 12.f)
			{
				End(W, B, C);
				return true;
			}
			if (C.Step == 5)
			{
				// the mate waits where it is for the other to come over
				if (Minds[C.Mate].Task != ETask::Preen)
				{
					End(W, B, C, false);
				}
				else if (!bAir && B.Act != EAct::Perch && B.Act != EAct::Preen)
				{
					W.SetAct(B, EAct::Perch, 30.f);
				}
				return true;
			}
			if (C.Step == 0 && !bAir)
			{
				FCorvid& MC = Minds[C.Mate];
				if (MC.Task == ETask::None && MC.Held < 0)
				{
					Start(W, M, MC, ETask::Preen, C.TaskLen + 15.f);
					MC.Step = 5;
					if (!IsAirborne(M.Act))
					{
						W.SetAct(M, EAct::Perch, 30.f);
					}
				}
				const V3 Off = (B.Pos - M.Pos).Flat().Norm() * (SpeciesTable()[M.Species].Length * 0.8f);
				const V3 Spot = M.Pos + Off;
				if ((Spot - B.Pos).Len() > 0.1f)
				{
					if (M.Site >= 0 && !IsGroundHabitat(W.Sites[M.Site].Kind))
					{
						W.FlyTo(B, M.Site, Spot);
					}
					else
					{
						GoTo(W, B, Spot);
					}
				}
				C.Step = 1;
			}
			else if (C.Step == 1 && !bAir && B.Act != EAct::Forage)
			{
				// side by side: both preen, facing each other
				C.Step = 2;
				B.Yaw = std::atan2(M.Pos.Y - B.Pos.Y, M.Pos.X - B.Pos.X);
				W.SetAct(B, EAct::Preen, C.TaskLen);
				FCorvid& MC = Minds[C.Mate];
				if ((MC.Task == ETask::None && MC.Held < 0) || (MC.Task == ETask::Preen && MC.Step == 5))
				{
					if (MC.Task == ETask::None)
					{
						Start(W, M, MC, ETask::Preen, C.TaskLen);
					}
					MC.Step = 2;
					MC.TaskTime = 0.f;
					MC.TaskLen = C.TaskLen;
					M.Yaw = std::atan2(B.Pos.Y - M.Pos.Y, B.Pos.X - M.Pos.X);
					W.SetAct(M, EAct::Preen, C.TaskLen);
				}
			}
			else if (C.Step == 2)
			{
				C.Social = std::max(0.f, C.Social - Dt * 0.08f);
				if (B.Act != EAct::Preen)
				{
					W.SetAct(B, EAct::Preen, 3.f);
				}
			}
			return true;
		}
		case ETask::Play:
			if (C.Step == 0 && !bAir)
			{
				// wind play: over the cliff edge, the tower or a ridge into the wind, tumbling in the updraught
				int Best = -1;
				float BestD = 600.f;
				for (int I = 0; I < (int)W.Sites.size(); I++)
				{
					const EHabitat H = W.Sites[I].Kind;
					if (H != EHabitat::Cliff && H != EHabitat::Tower && H != EHabitat::Roof)
					{
						continue;
					}
					const float D = (W.Sites[I].Center() - G.Home).Len2D() * (H == EHabitat::Cliff ? 0.6f : 1.f);
					if (D < BestD)
					{
						BestD = D;
						Best = I;
					}
				}
				C.Anchor = (Best >= 0 ? W.Sites[Best].Center() : B.Pos) + V3(0.f, 0.f, 10.f);
				C.Step = 1;
				B.FlyFrom = B.Pos;
				W.SetAct(B, EAct::Soar, C.TaskLen);
				B.Vel = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f) * 4.f + V3(0.f, 0.f, 3.f);
				// the flock comes along (jackdaws play as a party)
				for (int Id : G.Members)
				{
					if (Id == B.Id || !IsCorvid(Id) || Busy(Id) || IsAirborne(W.Birds[Id].Act) || W.Birds[Id].Alert > 0.4f)
					{
						continue;
					}
					FCorvid& O = Minds[Id];
					if (O.Play > 0.25f && W.Rand(B.Rng) < Table[O.Kind].Social)
					{
						FBird& OB = W.Birds[Id];
						Start(W, OB, O, ETask::Play, C.TaskLen * (0.7f + 0.5f * W.Rand(OB.Rng)));
						O.Anchor = C.Anchor + V3((W.Rand(OB.Rng) - 0.5f) * 10.f, (W.Rand(OB.Rng) - 0.5f) * 10.f, 0.f);
						O.Step = 1;
						OB.FlyFrom = OB.Pos;
						W.SetAct(OB, EAct::Soar, O.TaskLen);
						OB.Vel = V3(0.f, 0.f, 3.f);
					}
				}
				Emit(ECorvidEvent::Play, B, -1);
			}
			else if (C.Step == 1)
			{
				C.Play = std::max(0.f, C.Play - Dt * 0.01f);
				if (W.Rand(B.Rng) < Dt * 0.15f)
				{
					W.Emit(EEvent::Call, B);
				}
				if (!bAir)
				{
					End(W, B, C);
				}
			}
			else if (C.Step == 10 && !bAir)
			{
				// stick play: up with it, let go, dive after it and catch it again
				C.Goal = B.Pos + V3(0.f, 0.f, 14.f + W.Rand(B.Rng) * 8.f);
				C.Step = 11;
				B.FlyFrom = B.Pos;
				W.SetAct(B, EAct::Mob, C.TaskLen);
				B.Vel = V3(0.f, 0.f, 3.f);
				Emit(ECorvidEvent::Play, B, C.Held);
			}
			else if (C.Step == 13 && !bAir)
			{
				// the stick fell to the ground: leave it
				End(W, B, C);
			}
			return true;
		case ETask::Tease:
		{
			if (C.Other < 0 || C.Other >= (int)W.Beasts.size())
			{
				End(W, B, C);
				return true;
			}
			const FBeast& M = W.Beasts[C.Other];
			const V3 Head = M.Vel.Len2D() > 0.2f ? M.Vel.Flat().Norm() : (B.Pos - M.Pos).Flat().Norm() * -1.f;
			const V3 Tail = M.Pos - Head * (M.bCat ? 0.35f : 0.5f);
			if (M.Vel.Len2D() > 2.f && (B.Pos - M.Pos).Len() < 6.f)
			{
				W.Flush(B, M.Pos, -4);        // it turned on us
				return true;
			}
			if (bAir)
			{
				return true;
			}
			if (C.Step == 0)
			{
				GoTo(W, B, GroundAt(W, Tail.X - Head.X * 1.5f, Tail.Y - Head.Y * 1.5f));
				C.Step = 1;
			}
			else if (C.Step == 1 && B.Act != EAct::Forage)
			{
				B.Spot = GroundAt(W, Tail.X, Tail.Y);
				W.SetAct(B, EAct::Forage, 0.f);
				C.Step = 2;
			}
			else if (C.Step == 2 && B.Act != EAct::Forage)
			{
				B.Yaw = std::atan2(Tail.Y - B.Pos.Y, Tail.X - B.Pos.X);
				W.SetAct(B, EAct::Peck, 0.35f);
				Emit(ECorvidEvent::Tease, B, C.Other);
				C.Step = 3;
			}
			else if (C.Step == 3 && bSettled)
			{
				// a hop back, out of reach; again, a couple of times
				const V3 Away = (B.Pos - M.Pos).Flat().Norm();
				B.Spot = GroundAt(W, B.Pos.X + Away.X * 1.6f, B.Pos.Y + Away.Y * 1.6f);
				W.SetAct(B, EAct::Forage, 0.f);
				C.Step = ++C.Tries < 3 ? 1 : 4;
			}
			else if (C.Step == 4 && B.Act != EAct::Forage)
			{
				End(W, B, C);
			}
			return true;
		}
		case ETask::Feast:
		{
			FItem* It = C.Item >= 0 && C.Item < (int)Items.size() ? &Items[C.Item] : nullptr;
			if (!It || It->State != EItemState::Ground || It->Food <= 0.f || B.Hunger < 0.08f)
			{
				End(W, B, C);
				return true;
			}
			if (bAir)
			{
				return true;
			}
			const float D = (B.Pos - It->Pos).Len2D();
			if (C.Step == 0)
			{
				// to the ring round it (a raven calls the others in: young ravens yell at a carcass)
				const float A = W.Rand(B.Rng) * 2.f * Pi, R = 2.f + W.Rand(B.Rng) * 2.f;
				GoTo(W, B, GroundAt(W, It->Pos.X + std::cos(A) * R, It->Pos.Y + std::sin(A) * R));
				C.Step = 1;
				if (S.Id == "Raven")
				{
					W.Emit(EEvent::Call, B);
				}
				return true;
			}
			if (B.Act == EAct::Forage)
			{
				return true;
			}
			// who eats: the strongest at the fish now; the rest wait round it and dart in
			int Top = -1;
			float TopRank = -1.f;
			W.ForNear(It->Pos, 5.f, [&](int I)
			{
				if (!IsCorvid(I) || Minds[I].Task != ETask::Feast || Minds[I].Item != It->Id || IsAirborne(W.Birds[I].Act))
				{
					return;
				}
				const FBird& O = W.Birds[I];
				const float Rank = (float)SpeciesTable()[O.Species].Dominance + O.Boldness * 5.f;
				if (Rank > TopRank)
				{
					TopRank = Rank;
					Top = I;
				}
			});
			if (Top == B.Id || C.Step == 3)
			{
				if (D > 0.45f)
				{
					B.Spot = It->Pos + (B.Pos - It->Pos).Flat().Norm() * 0.3f;
					W.SetAct(B, EAct::Forage, 0.f);
					return true;
				}
				if (B.Act != EAct::Peck || B.ActTime > B.ActLen)
				{
					B.Yaw = std::atan2(It->Pos.Y - B.Pos.Y, It->Pos.X - B.Pos.X);
					W.SetAct(B, EAct::Peck, C.Step == 3 ? 0.5f + W.Rand(B.Rng) * 0.5f : 1.f + W.Rand(B.Rng) * 2.f);
					if (C.Step == 3 && ++C.Tries > 1)
					{
						// a dart is one or two bites, then back before the big one minds
						C.Step = 2;
						C.Tries = 0;
						const V3 Away = (B.Pos - It->Pos).Flat().Norm();
						B.Spot = GroundAt(W, It->Pos.X + Away.X * 2.5f, It->Pos.Y + Away.Y * 2.5f);
						W.SetAct(B, EAct::Forage, 0.f);
						return true;
					}
				}
				It->Food -= Dt * 0.004f;
				B.Hunger = std::max(0.f, B.Hunger - Dt * 0.012f);
				return true;
			}
			// waiting: watch the fish, step about; a bold one darts in now and then
			C.Step = 2;
			if (B.Act != EAct::Watch && B.Act != EAct::Perch)
			{
				W.SetAct(B, EAct::Perch, 1.f + W.Rand(B.Rng) * 2.f);
			}
			B.Look = It->Pos;
			B.bLook = true;
			if (W.Rand(B.Rng) < Dt * 0.05f * (0.3f + Table[C.Kind].Steal) * B.Boldness)
			{
				C.Step = 3;
				C.Tries = 0;
				B.Spot = It->Pos + (B.Pos - It->Pos).Flat().Norm() * 0.3f;
				W.SetAct(B, EAct::Forage, 0.f);
			}
			return true;
		}
		default:
			End(W, B, C);
			return true;
		}
	}

	bool FCorvids::Arrive(FWorld& W, FBird& B)
	{
		FCorvid* CP = Mind(B.Id);
		if (!CP || CP->Task == ETask::None)
		{
			return false;
		}
		FCorvid& C = *CP;
		const FPlayer& P = W.Player;
		switch (C.Task)
		{
		case ETask::Cache:
			if (C.Step == 1)
			{
				// watched by a clever neighbour: off to another place (up to twice), else dig it in
				int Seen[3];
				if (Watched(W, B, Seen) && Table[C.Kind].Clever > 0.5f && C.Tries < 2)
				{
					C.Tries++;
					V3 Spot;
					if (CacheSpot(W, B, C, Spot, 15.f))
					{
						C.Goal = Spot;
						W.FlyTo(B, -1, Spot);
						return true;
					}
				}
				W.SetAct(B, EAct::Peck, 2.5f + W.Rand(B.Rng) * 2.f);
				C.Step = 2;
				return true;
			}
			break;
		case ETask::Gift:
			if (C.Step == 1)
			{
				W.SetAct(B, EAct::Perch, 0.4f);
				B.Look = P.Eye;
				B.bLook = true;
				C.Step = 2;
				return true;
			}
			break;
		case ETask::Scold:
			if (C.Step == 1)
			{
				C.Step = 2;
				W.SetAct(B, EAct::Perch, 0.3f);
				return true;
			}
			break;
		case ETask::Follow:
			if (C.Step == 1)
			{
				C.Step = 2;
				W.SetAct(B, EAct::Perch, 0.5f);
				return true;
			}
			break;
		case ETask::Crack:
			if (C.Step == 2)
			{
				C.Step = 3;
				W.SetAct(B, EAct::Perch, 0.2f);
				return true;
			}
			break;
		case ETask::Snatch:
			if (C.Step == 1 || C.Step == 2)
			{
				C.Step = 2;
				W.SetAct(B, EAct::Perch, 0.3f);
				return true;
			}
			break;
		case ETask::Play:
			if (C.Step == 12 || C.Step == 13)
			{
				if (C.Held >= 0)
				{
					Release(W, B, C, false);
				}
				C.Step = 13;
				W.SetAct(B, EAct::Perch, 0.5f);
				return true;
			}
			break;
		default:
			break;
		}
		// any other landing on a task (after a watch, a step aside): a moment's pause, then the task goes on
		W.SetAct(B, IsGroundHabitat(B.Site >= 0 ? W.Sites[B.Site].Kind : EHabitat::Meadow) ? EAct::Peck : EAct::Perch, 0.3f);
		return true;
	}

	void FCorvids::FlightGoal(FWorld& W, FBird& B, V3& Goal, float& Speed, bool& bLanding)
	{
		FCorvid* CP = Mind(B.Id);
		if (!CP || CP->Task == ETask::None)
		{
			return;
		}
		FCorvid& C = *CP;
		const FSpecies& S = SpeciesTable()[B.Species];
		const FPlayer& P = W.Player;
		auto ChaseItem = [&](int Item, bool bCatch) -> bool
		{
			if (Item < 0 || Item >= (int)Items.size())
			{
				return false;
			}
			FItem& It = Items[Item];
			if (It.State == EItemState::Falling)
			{
				Goal = It.Pos + It.Vel * 0.3f;
				Speed = std::min(S.Sprint, S.Cruise * 1.3f);
				bLanding = false;
				if (bCatch && (B.Pos - It.Pos).Len() < 1.2f)
				{
					Pick(W, B, C, Item);       // caught it in the air
					return true;
				}
			}
			else if (It.State == EItemState::Ground)
			{
				Goal = It.Pos + (B.Pos - It.Pos).Flat().Norm() * 0.25f;
				B.Spot = Goal;
				bLanding = true;
			}
			return false;
		};
		switch (C.Task)
		{
		case ETask::Snatch:
			if (C.Step == 1 && P.bOffer)
			{
				Goal = P.Palm;
				Speed = std::min(S.Cruise, 7.f);
				if ((B.Pos - P.Palm).Len() < 0.5f)
				{
					// the grab: a beakful from the palm, and away before the hand closes
					const int Id = NewItem(W, EItem::FeedPile, P.Palm, EItemState::Ground);
					if (Id >= 0)
					{
						Items[Id].Food = 0.3f;
						Items[Id].Flags = ItemPlayer | ItemStolen;
						Pick(W, B, C, Id);
						Emit(ECorvidEvent::Snatch, B, Id);
						C.Bond = std::min(1.f, C.Bond + 0.03f);
					}
					const V3 Away = (B.Pos - P.Feet).Flat().Norm();
					V3 Spot = B.Pos + Away * (20.f + W.Rand(B.Rng) * 20.f);
					Spot = GroundAt(W, Spot.X, Spot.Y);
					W.FlyTo(B, -1, Spot);
					B.Vel = Away * S.Cruise * 0.6f + V3(0.f, 0.f, 3.f);
					C.Step = 2;
					Goal = Spot;
					bLanding = true;
				}
			}
			break;
		case ETask::Crack:
			if (C.Step == 1)
			{
				// the climb over the road: let go once high enough over it
				Goal = C.Goal;
				Speed = S.Cruise * 0.7f;
				bLanding = false;
				if (B.Act == EAct::Fly || B.Act == EAct::Takeoff)
				{
					B.Act = EAct::Mob;       // flaps on the spot, up
				}
				if ((B.Pos - C.Anchor).Len2D() < 3.f && B.Pos.Z - C.Anchor.Z > 5.f && C.Held >= 0)
				{
					C.Item = C.Held;
					Release(W, B, C, true);
					Items[C.Item].Vel = V3();
					C.Step = 2;
					W.Emit(EEvent::Call, B);
				}
			}
			else if (C.Step == 2)
			{
				ChaseItem(C.Item, false);
			}
			break;
		case ETask::Steal:
			if (C.Item >= 0 && C.Item < (int)Items.size() && Items[C.Item].State != EItemState::Carried)
			{
				ChaseItem(C.Item, true);
				if (C.Held >= 0)
				{
					Emit(ECorvidEvent::Steal, B, C.Held);
					C.Task = ETask::Cache;
					C.Step = 0;
					C.TaskTime = 0.f;
					C.TaskLen = 90.f;
					const int G = W.Groups[B.Group].Site;
					if (G >= 0)
					{
						Goal = W.SpotFor(B, G);
						W.FlyTo(B, G, Goal);
						bLanding = true;
					}
				}
			}
			break;
		case ETask::Scold:
			if (C.Step == 3)
			{
				// circles at head height and just over it, close
				const float A = (float)W.Time * 1.6f + B.Id * 2.1f;
				Goal = P.Feet + V3(std::cos(A) * 3.5f, std::sin(A) * 3.5f, 2.6f + 1.2f * std::sin(A * 1.7f));
				Speed = S.Cruise * 0.55f;
				bLanding = false;
			}
			break;
		case ETask::Play:
			if (C.Step == 1)
			{
				// loops and rolls in the updraught, upwind of the edge
				const float T = (float)W.Time * 0.8f + B.Id * 1.3f;
				const V3 Up = W.Wind.Len2D() > 0.5f ? W.Wind.Flat().Norm() * -6.f : V3();
				Goal = C.Anchor + Up + V3(std::cos(T) * 9.f, std::sin(T * 1.3f) * 7.f, 4.f * std::sin(T * 2.f));
				Speed = S.Cruise * 0.7f;
				bLanding = false;
				if (C.TaskTime > C.TaskLen - 1.f)
				{
					const int G = W.Groups[B.Group].Site;
					if (G >= 0)
					{
						Goal = W.SpotFor(B, G);
						W.FlyTo(B, G, Goal);
						bLanding = true;
					}
				}
			}
			else if (C.Step == 11)
			{
				Goal = C.Goal;
				Speed = S.Cruise * 0.6f;
				bLanding = false;
				if (B.Pos.Z > C.Goal.Z - 1.5f && C.Held >= 0)
				{
					C.Item = C.Held;
					Release(W, B, C, true);
					Items[C.Item].Vel = B.Vel * 0.3f;
					C.Step = 12;
				}
			}
			else if (C.Step == 12)
			{
				if (ChaseItem(C.Item, true))
				{
					Emit(ECorvidEvent::Play, B, C.Held);
					if (++C.Tries < 3)
					{
						C.Goal = B.Pos + V3(0.f, 0.f, 10.f + W.Rand(B.Rng) * 8.f);
						C.Step = 11;
					}
					else
					{
						// enough: let it drop and come down after it
						C.Item = C.Held;
						Release(W, B, C, true);
						C.Step = 13;
					}
				}
			}
			else if (C.Step == 13)
			{
				ChaseItem(C.Item, false);
			}
			break;
		default:
			break;
		}
	}

	void FCorvids::OnFlush(FWorld& W, FBird& B, const V3& From, int Cause)
	{
		(void)From;
		FCorvid* CP = Mind(B.Id);
		if (!CP)
		{
			return;
		}
		FCorvid& C = *CP;
		// chased by a thief it mostly drops it; in plain fright it keeps hold of small things
		bool bChased = false;
		W.ForNear(B.Pos, 15.f, [&](int I)
		{
			bChased |= W.Birds[I].Act == EAct::Chase && W.Birds[I].Target == B.Id;
		});
		if (C.Held >= 0)
		{
			const FItemKind& IK = ItemKind(Items[C.Held].Kind);
			if ((bChased && W.Rand(B.Rng) < 0.75f) || IK.bBig || W.Rand(B.Rng) < 0.3f)
			{
				Release(W, B, C, true);
			}
		}
		// a player who runs at it or throws things is remembered
		const FPlayer& P = W.Player;
		if (Cause == -1 && P.bPresent && (P.Vel.Len2D() > 3.f || P.ExtraThreat > 0.4f))
		{
			C.Grudge = std::min(1.f, C.Grudge + 0.06f * (1.f + C.Grudge));
		}
		if (C.Task != ETask::None)
		{
			End(W, B, C, false);
		}
	}

	float FCorvids::DangerScale(const FBird& B) const
	{
		const FCorvid* C = Mind(B.Id);
		if (!C)
		{
			return 1.f;
		}
		float K = std::max(0.3f, 1.f + 0.6f * C->Grudge - 0.7f * C->Bond);      // a friend sits still a few steps off
		switch (C->Task)
		{
		case ETask::Snatch: K *= 0.25f; break;
		case ETask::Gift: K *= 0.15f; break;
		case ETask::Follow: K *= 0.35f; break;
		case ETask::Scold: K *= 0.3f; break;
		case ETask::Eat: K *= Items.size() > (size_t)std::max(C->Item, 0) && C->Item >= 0 && (Items[C->Item].Flags & ItemPlayer) ? 0.7f : 1.f; break;
		default: break;
		}
		return K;
	}

	float FCorvids::PlayerWariness(const FBird& B) const
	{
		const FCorvid* C = Mind(B.Id);
		return C ? std::clamp(1.f - 0.9f * C->Bond + 0.5f * C->Grudge, 0.1f, 1.5f) : 1.f;
	}

	float FCorvids::BeastScale(const FBird& B, const FBeast& M) const
	{
		const FCorvid* C = Mind(B.Id);
		if (!C)
		{
			return 1.f;
		}
		const bool bCalm = M.Vel.Len2D() < 1.8f && !M.bStalking;
		if (C->Task == ETask::Tease && bCalm)
		{
			return 0.f;              // it knows this one: lying there, it can't catch a magpie at its tail
		}
		// a crow-sized bird minds a strolling cat little, a dog somewhat more
		return bCalm ? (M.bCat ? 0.4f : 0.7f) : 1.f;
	}

	// --- the player -----------------------------------------------------------------------------------

	void FCorvids::PickableNear(const FWorld& W, const V3& P, float R, std::vector<int>& Out) const
	{
		(void)W;
		Out.clear();
		for (const FItem& It : Items)
		{
			const bool bLying = It.State == EItemState::Ground && It.Kind != EItem::FeedPile && It.Kind != EItem::Fish;
			const bool bStore = It.State == EItemState::Cached && (It.Flags & ItemSeen);
			if ((bLying || bStore) && (It.Pos - P).Len() < R)
			{
				Out.push_back(It.Id);
			}
		}
		std::sort(Out.begin(), Out.end(), [&](int A, int B) { return (Items[A].Pos - P).Len() < (Items[B].Pos - P).Len(); });
	}

	bool FCorvids::PlayerTake(FWorld& W, int Id)
	{
		if (Id < 0 || Id >= (int)Items.size())
		{
			return false;
		}
		FItem& It = Items[Id];
		if (It.State != EItemState::Ground && It.State != EItemState::Cached)
		{
			return false;
		}
		// robbing a store under its owner's eyes, or snatching what a bird was going for, makes an enemy
		const bool bStore = It.State == EItemState::Cached;
		for (int I = 0; I < (int)Minds.size(); I++)
		{
			FCorvid& C = Minds[I];
			if (C.Kind < 0)
			{
				continue;
			}
			const FBird& B = W.Birds[I];
			const bool bSees = !B.bGone && B.Act != EAct::Sleep && (B.Pos - It.Pos).Len() < 45.f;
			if (!bSees)
			{
				continue;
			}
			if ((bStore && It.Owner == I) || ((C.Task == ETask::Fetch || C.Task == ETask::Steal) && C.Item == Id))
			{
				C.Grudge = std::min(1.f, C.Grudge + (bStore ? 0.45f : 0.15f));
				Learn(W, B, C.Grudge, 0.f);
			}
		}
		It.State = EItemState::Taken;
		It.Since = W.Time;
		It.Holder = -1;
		return true;
	}

	std::string FCorvids::Report(const FWorld& W) const
	{
		(void)W;
		int States[6] = {};
		for (const FItem& It : Items)
		{
			States[(int)It.State]++;
		}
		int Tasks[(int)ETask::Num] = {};
		int Corvids = 0;
		float Grudge = 0.f, Bond = 0.f;
		for (const FCorvid& C : Minds)
		{
			if (C.Kind >= 0)
			{
				Corvids++;
				Tasks[(int)C.Task]++;
				Grudge = std::max(Grudge, C.Grudge);
				Bond = std::max(Bond, C.Bond);
			}
		}
		char Buf[512];
		std::snprintf(Buf, sizeof(Buf), "corvids %d: items ground %d carried %d falling %d cached %d taken %d; max grudge %.2f bond %.2f; now",
			Corvids, States[0], States[1], States[2], States[3], States[5], Grudge, Bond);
		std::string Out = Buf;
		for (int T = 1; T < (int)ETask::Num; T++)
		{
			if (Tasks[T])
			{
				Out += std::string(" ") + TaskName((ETask)T) + "=" + std::to_string(Tasks[T]);
			}
		}
		Out += "; since start";
		for (int E = 0; E < (int)ECorvidEvent::Num; E++)
		{
			if (Count[E])
			{
				Out += std::string(" ") + CorvidEventName((ECorvidEvent)E) + "=" + std::to_string(Count[E]);
			}
		}
		return Out;
	}
}
