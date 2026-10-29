#include "BirdSim.h"

#include "BirdMurmur.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace LyudBirds
{
	namespace
	{
		constexpr float Pi = 3.14159265f;

		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
		float Smooth(float T) { T = Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
		float Lerp(float A, float B, float T) { return A + (B - A) * T; }

		/** Wing beats a second from body mass (Pennycuick 1996: ~ m^-1/3). */
		float WingbeatHz(const FSpecies& S) { return S.Wingbeat > 0.f ? S.Wingbeat : Clamp(3.6f * std::pow(S.Mass, -0.32f), 2.5f, 16.f); }

		bool IsSmall(const FSpecies& S) { return S.Length < 0.3f; }
		bool IsAerial(const FSpecies& S) { return S.Flight == EFlight::Aerial; }
		bool IsGull(const FSpecies& S) { return S.Flight == EFlight::FlapGlide && !S.bPredator; }
	}

	float V3::Len() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	float V3::Len2D() const { return std::sqrt(X * X + Y * Y); }
	V3 V3::Norm() const
	{
		const float L = Len();
		return L > 1e-6f ? V3(X / L, Y / L, Z / L) : V3(0.f, 0.f, 0.f);
	}

	const char* HabitatName(EHabitat H)
	{
		static const char* const Names[] = { "Sea", "Beach", "Cliff", "Meadow", "Reed", "Street", "Roof", "Tower", "Wire", "Garden", "Broadleaf", "Conifer", "ForestFloor" };
		return (int)H < NumHabitats ? Names[(int)H] : "?";
	}

	bool IsGroundHabitat(EHabitat H)
	{
		return H == EHabitat::Sea || H == EHabitat::Beach || H == EHabitat::Meadow || H == EHabitat::Street || H == EHabitat::ForestFloor || H == EHabitat::Reed;
	}

	const char* ActName(EAct A)
	{
		static const char* const Names[] = { "Sleep", "Perch", "Preen", "Forage", "Peck", "Swim", "Watch", "Takeoff", "Fly", "Land", "Hawk", "Soar", "Flee", "Chase", "Mob", "Approach", "Eat", "Claimed", "Murmur" };
		return (int)A < (int)EAct::Num ? Names[(int)A] : "?";
	}

	const char* EventName(EEvent E)
	{
		static const char* const Names[] = { "Notice", "Turn", "Watch", "Call", "Approach", "Eat", "Flee", "Alarm", "Song", "Supplant", "ChaseStart" };
		return (int)E < (int)EEvent::Num ? Names[(int)E] : "?";
	}

	// --- loading ------------------------------------------------------------------------------------

	bool FWorld::LoadHabitats(const std::string& Dir)
	{
		std::ifstream F(Dir + "/bird_habitats.json", std::ios::binary);
		if (!F)
		{
			LastError = "no " + Dir + "/bird_habitats.json";
			return false;
		}
		std::stringstream SS;
		SS << F.rdbuf();
		const std::string Text = SS.str();
		const size_t At = Text.find("\"sites\"");
		if (At == std::string::npos)
		{
			LastError = "bird_habitats.json has no sites";
			return false;
		}
		// [[kind, ax, ay, az, bx, by, bz, radius, capacity, flags], ...]
		const char* P = Text.c_str() + Text.find('[', At) + 1;
		Sites.clear();
		while (*P)
		{
			while (*P && *P != '[' && *P != ']')
			{
				P++;
			}
			if (*P != '[')
			{
				break;
			}
			P++;
			float V[10] = {};
			for (int K = 0; K < 10; K++)
			{
				char* End = nullptr;
				V[K] = std::strtof(P, &End);
				P = End;
				while (*P == ',' || *P == ' ')
				{
					P++;
				}
			}
			while (*P && *P != ']')
			{
				P++;
			}
			if (*P)
			{
				P++;
			}
			FSite S;
			S.Kind = (EHabitat)Clamp(V[0], 0.f, NumHabitats - 1.f);
			S.A = { V[1], V[2], V[3] };
			S.B = { V[4], V[5], V[6] };
			S.Radius = V[7];
			S.Capacity = (int)V[8];
			S.Flags = (uint8_t)V[9];
			Sites.push_back(S);
		}

		std::ifstream G(Dir + "/bird_world.bin", std::ios::binary);
		if (G)
		{
			char Magic[4];
			int32_t NX = 0, NY = 0;
			float H[3];
			G.read(Magic, 4);
			G.read((char*)&NX, 4);
			G.read((char*)&NY, 4);
			G.read((char*)H, 12);
			if (std::memcmp(Magic, "BWG1", 4) == 0 && NX > 0 && NY > 0 && NX * NY < 4000000)
			{
				Grid.NX = NX;
				Grid.NY = NY;
				Grid.X0 = H[0];
				Grid.Y0 = H[1];
				Grid.Cell = H[2];
				Grid.Ground.resize((size_t)NX * NY);
				Grid.Top.resize((size_t)NX * NY);
				G.read((char*)Grid.Ground.data(), Grid.Ground.size() * 2);
				G.read((char*)Grid.Top.data(), Grid.Top.size() * 2);
			}
		}
		if (Grid.NX == 0)
		{
			LastError = "no bird_world.bin (ground grid): flights keep a fixed height";
		}
		OffshoreD.clear();
		return !Sites.empty();
	}

	void FWorld::RemoveSites(uint8_t Flag)
	{
		Sites.erase(std::remove_if(Sites.begin(), Sites.end(), [Flag](const FSite& S) { return (S.Flags & Flag) != 0; }), Sites.end());
	}

	static float GridAt(const std::vector<int16_t>& V, int NX, int NY, float X0, float Y0, float Cell, float X, float Y, float Empty)
	{
		if (V.empty())
		{
			return Empty;
		}
		const float U = (X - X0) / Cell, W = (Y - Y0) / Cell;
		int I = (int)std::floor(U), J = (int)std::floor(W);
		const float FU = U - I, FW = W - J;
		auto H = [&](int A, int B)
		{
			A = A < 0 ? 0 : (A >= NX ? NX - 1 : A);
			B = B < 0 ? 0 : (B >= NY ? NY - 1 : B);
			return V[(size_t)B * NX + A] * 0.01f;
		};
		return Lerp(Lerp(H(I, J), H(I + 1, J), FU), Lerp(H(I, J + 1), H(I + 1, J + 1), FU), FW);
	}

	float FWorld::GroundZ(float X, float Y) const { return GridAt(Grid.Ground, Grid.NX, Grid.NY, Grid.X0, Grid.Y0, Grid.Cell, X, Y, NoGridZ); }
	float FWorld::TopZ(float X, float Y) const { return GridAt(Grid.Top, Grid.NX, Grid.NY, Grid.X0, Grid.Y0, Grid.Cell, X, Y, NoGridZ); }

	float FWorld::Rand(uint32_t& S) const
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xffffff) / 16777216.f;
	}

	// --- the birds' places ----------------------------------------------------------------------------

	V3 FWorld::SpotOn(const FSite& Site, uint32_t& Rng) const
	{
		const float T = Rand(Rng);
		V3 P = Site.A + (Site.B - Site.A) * T;
		if (Site.Radius > 0.f)
		{
			const float A = Rand(Rng) * 2.f * Pi, R = Site.Radius * std::sqrt(Rand(Rng));
			P.X += std::cos(A) * R;
			P.Y += std::sin(A) * R;
			if (Site.Kind == EHabitat::Sea)
			{
				P.Z = SeaZ;
			}
			else if (IsGroundHabitat(Site.Kind))
			{
				float Z;
				if (Probe && Probe(P.X, P.Y, Z))
				{
					P.Z = Z;
				}
				else if (!Grid.Ground.empty() && R > 1.f)
				{
					P.Z = Site.A.Z + (GroundZ(P.X, P.Y) - GroundZ(Site.A.X, Site.A.Y));
				}
			}
			else
			{
				// a crown: anywhere in its outer half, from its middle up
				P.Z += (Rand(Rng) - 0.3f) * Site.Radius * 0.8f;
			}
		}
		return P;
	}

	/** How much this species likes a kind of place for what it means to do now (the season, the tide). */
	float FWorld::Taste(const FSpecies& S, int Plan, EHabitat Kind) const
	{
		const int K = (int)Kind;
		float W;
		switch (Plan)
		{
		case 1: W = S.bWinterForage && IsWinter() ? S.WinterForage[K] : S.Forage[K]; break;
		case 2: W = S.Roost[K]; break;
		case 3: W = S.Stage[K]; break;
		case 5: W = Kind == EHabitat::Beach ? 1.f : (Kind == EHabitat::Reed ? 0.8f : (Kind == EHabitat::Sea && S.Length > 0.3f ? 0.6f : 0.f)); break;
		default: W = S.Perch[K]; break;
		}
		if (Kind == EHabitat::Beach && (Plan == 1 || Plan == 5))
		{
			// the tide: low water lays bare the wet sand (waders, gulls, crows and wagtails go down to it), high water covers it
			const float Low = Clamp((MeanSeaZ - SeaZ) / 0.6f, 0.f, 1.f), High = Clamp((SeaZ - MeanSeaZ) / 0.6f, 0.f, 1.f);
			W *= (1.f - 0.85f * High) * (S.bTidal ? 1.f + 2.5f * Low : 1.f);
		}
		return W;
	}

	float FWorld::SiteScore(const FGroup& G, const FSite& Site, int Plan) const
	{
		const FSpecies& S = SpeciesTable()[G.Species];
		float W = Taste(S, Plan, Site.Kind);
		if (W <= 0.f)
		{
			return 0.f;
		}
		const V3 C = Site.Center();
		// gathering before the night: near the roost, not near home
		const V3 From = Plan == 3 && G.Roost >= 0 ? Sites[G.Roost].Center() : G.Home;
		const float D = (C - From).Len2D();
		const float Range = Plan == 3 ? 700.f : (Plan == 2 ? S.HomeRange * 2.f : S.HomeRange);
		if (D > Range * 1.5f)
		{
			return 0.f;
		}
		W *= 1.f - 0.8f * Smooth(D / Range);
		// the next place is usually not far from this one (short hops, not trips across the map)
		if (G.Site >= 0 && Plan != 2 && Plan != 3)
		{
			W /= 1.f + (C - Sites[G.Site].Center()).Len2D() / (0.3f * Range);
		}
		// crowding: a site full of others is worth less, full of bigger birds much less
		const int Size = (int)G.Members.size();
		const float Free = 1.f - Clamp((float)(Site.Occupants + Size) / (float)(Site.Capacity + Size), 0.f, 1.f) * 0.8f;
		W *= Free;
		if (Site.TopDominance > S.Dominance + 10 && Site.Occupants > 0)
		{
			W *= 0.25f;
		}
		if (Site.Disturbed > Time)
		{
			W *= 0.1f;
		}
		if (Avoided(G, C))
		{
			W *= 0.02f;
		}
		// a single point (the spire's tip) holds a bird or two, and never a swift (it needs a wall)
		if ((Site.B - Site.A).Len() < 0.01f && Site.Radius <= 0.f)
		{
			if (S.bClings)
			{
				return 0.f;
			}
			if (Site.Occupants + Size > 2)
			{
				W *= 0.05f;
			}
		}
		return W;
	}

	/** Is P too near what frightened the group lately (the player, wherever they are now, or a hawk's attack)? */
	bool FWorld::Avoided(const FGroup& G, const V3& P) const
	{
		if (Time > G.AvoidUntil)
		{
			return false;
		}
		const V3 From = G.bAvoidPlayer && Player.bPresent ? Player.Feet : G.Avoid;
		return (P - From).Len2D() < G.AvoidR;
	}

	int FWorld::ChooseSite(const FGroup& G, int Plan, int Except) const
	{
		// sample: the best of a few dozen candidates (weights by taste, closeness, crowding)
		uint32_t R = (uint32_t)(G.Members.empty() ? 7 : Birds[G.Members[0]].Rng) * 2654435761u + (uint32_t)(Time * 13.0) + 1u;
		int Best = -1;
		float BestW = 0.f;
		const int N = (int)Sites.size();
		if (N == 0)
		{
			return -1;
		}
		for (int K = 0; K < 96; K++)
		{
			const int I = (int)(Rand(R) * N) % N;
			if (I == Except)
			{
				continue;
			}
			const float W = SiteScore(G, Sites[I], Plan) * (0.6f + 0.8f * Rand(R));
			if (W > BestW)
			{
				BestW = W;
				Best = I;
			}
		}
		return Best;
	}

	// --- creating the birds ---------------------------------------------------------------------------

	void FWorld::Populate()
	{
		Birds.clear();
		Groups.clear();
		Predators.clear();
		Murm.reset();
		if (OffshoreD.empty())
		{
			BuildOffshore();
		}
		uint32_t R = Opt.Seed | 1u;
		const std::vector<FSpecies>& T = SpeciesTable();
		// near-uniform candidate sites for homes: one list per species of sites it likes at all
		for (int Sp = 0; Sp < (int)T.size(); Sp++)
		{
			const FSpecies& S = T[Sp];
			// a migrant away this month is created but gone; a few late stragglers may stay on (and leave one by one)
			const bool bStragglers = Opt.bMigration && S.bMigrant && S.Stragglers > 0 && DayOfYear < S.LeaveUntil;
			const bool bAway = Opt.bMigration && S.bMigrant && !bStragglers;
			int Count = bStragglers ? S.Stragglers : (int)std::lround(S.Population * Opt.PopulationScale);
			const int Created = Count;
			if (S.bPredator)
			{
				Count = Opt.bPredators ? S.Population : 0;          // never scaled away
			}
			std::vector<int> Candidates;
			std::vector<float> Weights;
			float Total = 0.f;
			for (int I = 0; I < (int)Sites.size(); I++)
			{
				const float W = S.Forage[(int)Sites[I].Kind] + 0.5f * S.Perch[(int)Sites[I].Kind];
				if (W > 0.f)
				{
					Candidates.push_back(I);
					Weights.push_back(W);
					Total += W;
				}
			}
			if (Candidates.empty() || Count <= 0)
			{
				continue;
			}
			std::vector<V3> Homes;
			const int FirstGroup = (int)Groups.size();
			while (Count > 0)
			{
				FGroup G;
				G.Species = Sp;
				const float Season = IsWinter() ? S.WinterFlock : 1.f;
				const int FMin = (int)std::lround(S.FlockMin * Season), FMax = (int)std::lround(S.FlockMax * Season);
				const int Size = std::min(Count, FMin + (int)(Rand(R) * (FMax - FMin + 1)));
				Count -= Size;
				// home: of 10 weighted picks the one farthest from this species' other homes (spreads them out);
				// some groups near the place the player is most
				const bool bFocus = Opt.FocusShare > 0.f && Rand(R) < Opt.FocusShare && !S.bPredator;
				float BestD = -1.f;
				for (int K = 0; K < (bFocus ? 40 : 10); K++)
				{
					float Pick = Rand(R) * Total;
					int I = 0;
					while (I < (int)Candidates.size() - 1 && Pick > Weights[I])
					{
						Pick -= Weights[I++];
					}
					const V3 C = Sites[Candidates[I]].Center();
					if (bFocus && (C - Opt.Focus).Len2D() > Opt.FocusRadius && K < 39)
					{
						continue;
					}
					float D = 1e9f;
					for (const V3& H : Homes)
					{
						D = std::min(D, (H - C).Len2D());
					}
					if (D > BestD)
					{
						BestD = D;
						G.Home = C;
					}
				}
				Homes.push_back(G.Home);
				G.WakeShift = Rand(R) * S.WakeSpread;
				G.Slot0 = (int)(Rand(R) * 1000.f);
				for (int M = 0; M < Size; M++)
				{
					FBird B;
					B.Id = (int)Birds.size();
					B.Species = Sp;
					B.Group = (int)Groups.size();
					B.Rng = (R ^ (uint32_t)(B.Id * 0x9E3779B9u)) | 1u;
					Rand(B.Rng);
					B.Boldness = 0.6f + 0.8f * Rand(B.Rng);
					B.bFemale = Rand(B.Rng) < 0.5f;
					B.Hunger = 0.3f + 0.5f * Rand(B.Rng);
					B.Phase = Rand(B.Rng) * 10.f;
					B.CallTimer = 2.f + Rand(B.Rng) * 30.f;
					B.ThinkTimer = Rand(B.Rng);
					B.bGone = bAway;
					if (bStragglers)
					{
						// the stragglers leave one by one over their days
						const float K = ((float)(Created - Count - Size + M) + 0.5f + (Rand(B.Rng) - 0.5f) * 0.8f) / (float)std::max(Created, 1);
						B.LeaveDay = S.LeaveFrom + (S.LeaveUntil - S.LeaveFrom) * Clamp(K, 0.f, 1.f);
					}
					B.Trust = S.Tameness > 0.7f && S.Population > 60 ? 0.1f : 0.f;   // town pigeons and sparrows know people
					G.Members.push_back(B.Id);
					Birds.push_back(B);
				}
				Groups.push_back(G);
			}
			// roosts: one for the whole species (rooks, gulls), else the best near home
			int Shared = -1;
			if (S.bCommunalRoost)
			{
				V3 Mid;
				for (const V3& H : Homes)
				{
					Mid += H * (1.f / Homes.size());
				}
				float BestW = 0.f;
				for (int I = 0; I < (int)Sites.size(); I++)
				{
					const bool bPoint = (Sites[I].B - Sites[I].A).Len() < 0.01f && Sites[I].Radius <= 0.f;
					float W = bPoint ? 0.f : S.Roost[(int)Sites[I].Kind] * Sites[I].Capacity / (1.f + (Sites[I].Center() - Mid).Len2D() / 600.f);
					if (S.bMurmurates && W > 0.f)
					{
						// a murmuration's roost is a big reedbed (the more reed round it the better) by the shore
						int Bed = 0;
						for (const FSite& O : Sites)
						{
							Bed += O.Kind == Sites[I].Kind && (O.Center() - Sites[I].Center()).Len2D() < 60.f;
						}
						const V3 C = Sites[I].Center();
						W = S.Roost[(int)Sites[I].Kind] * (float)Bed / (1.f + Inland(C.X, C.Y) / 250.f);
					}
					if (W > BestW)
					{
						BestW = W;
						Shared = I;
					}
				}
			}
			if (S.bMurmurates && Shared >= 0 && FirstGroup < (int)Groups.size())
			{
				Murm = std::make_shared<FMurmuration>(*this, Sp, Shared);
			}
			for (int Gi = FirstGroup; Gi < (int)Groups.size(); Gi++)
			{
				FGroup& G = Groups[Gi];
				if (Shared >= 0)
				{
					G.Roost = Shared;
					continue;
				}
				float BestW = 0.f;
				for (int I = 0; I < (int)Sites.size(); I++)
				{
					const bool bPoint = (Sites[I].B - Sites[I].A).Len() < 0.01f && Sites[I].Radius <= 0.f;
					const float W = bPoint && (S.bClings || G.Members.size() > 2) ? 0.f
						: S.Roost[(int)Sites[I].Kind] / (1.f + (Sites[I].Center() - G.Home).Len2D() / 150.f) * (0.7f + 0.6f * Rand(R));
					if (W > BestW)
					{
						BestW = W;
						G.Roost = I;
					}
				}
			}
		}
		// no roost of its kind on the map (the town's roofs are being rebuilt): the nearest place it would perch, or feed
		for (FGroup& G : Groups)
		{
			if (G.Roost >= 0)
			{
				continue;
			}
			const FSpecies& S = SpeciesTable()[G.Species];
			float BestW = 0.f;
			for (int I = 0; I < (int)Sites.size(); I++)
			{
				const float Taste = S.Perch[(int)Sites[I].Kind] + 0.3f * S.Forage[(int)Sites[I].Kind];
				const float W = Taste / (1.f + (Sites[I].Center() - G.Home).Len2D() / 100.f);
				if (W > BestW)
				{
					BestW = W;
					G.Roost = I;
				}
			}
		}
		// communal roosts hold everybody: widen their capacity
		for (FGroup& G : Groups)
		{
			if (G.Roost >= 0)
			{
				Sites[G.Roost].Capacity = std::max(Sites[G.Roost].Capacity, 8);
			}
		}
		for (FGroup& G : Groups)
		{
			for (int Id : G.Members)
			{
				PlaceAtStart(Birds[Id], G);
			}
			G.StayTimer = 20.f + Rand(Birds[G.Members[0]].Rng) * 120.f;
			if (SpeciesTable()[G.Species].bPredator)
			{
				FPredator P;
				P.Bird = G.Members[0];
				P.Timer = 300.f + Rand(Birds[P.Bird].Rng) * 900.f;
				Predators.push_back(P);
			}
		}
		Approacher = -1;
		UpdateMigration();
		BuildHash();
	}

	int FWorld::AddBird(int Species, const V3& P)
	{
		return AddFlock(Species, P, 1, 1.2f);
	}

	int FWorld::AddFlock(int Species, const V3& P, int Count, float Radius)
	{
		if (Species < 0 || Species >= (int)SpeciesTable().size() || Count <= 0)
		{
			return -1;
		}
		const FSpecies& S = SpeciesTable()[Species];
		FSite Site;
		Site.Kind = S.Forage[(int)EHabitat::ForestFloor] > S.Forage[(int)EHabitat::Meadow] ? EHabitat::ForestFloor : EHabitat::Meadow;
		Site.A = Site.B = P;
		Site.Radius = Radius;
		Site.Capacity = std::max(20, Count * 2);
		Sites.push_back(Site);
		FGroup G;
		G.Species = Species;
		G.Home = P;
		G.Site = G.Roost = (int)Sites.size() - 1;
		G.Plan = 1;
		G.StayTimer = 1e9f;
		G.bStayPut = true;
		const int First = (int)Birds.size();
		for (int K = 0; K < Count; K++)
		{
			FBird B;
			B.Id = (int)Birds.size();
			B.Species = Species;
			B.Group = (int)Groups.size();
			B.Site = G.Site;
			B.Rng = (Opt.Seed ^ (uint32_t)(B.Id * 0x9E3779B9u)) | 1u;
			Rand(B.Rng);
			B.Pos = B.Spot = Count > 1 ? SpotFor(B, G.Site) : P;
			B.Boldness = Count > 1 ? 0.7f + 0.6f * Rand(B.Rng) : 1.f;
			B.Hunger = 0.6f + 0.3f * Rand(B.Rng);
			B.bFemale = Rand(B.Rng) < 0.5f;
			B.Yaw = Rand(B.Rng) * 2.f * Pi;
			B.Phase = Rand(B.Rng) * 10.f;
			B.CallTimer = 3.f + Rand(B.Rng) * 20.f;
			SetAct(B, EAct::Peck, 0.5f + Rand(B.Rng) * 1.5f);
			G.Members.push_back(B.Id);
			Birds.push_back(B);
		}
		Groups.push_back(G);
		BuildHash();
		return First;
	}

	void FWorld::Resettle()
	{
		if (Murm)
		{
			Murm->Clear();
		}
		for (FGroup& G : Groups)
		{
			bool bBusy = false;
			for (int Id : G.Members)
			{
				const EAct A = Birds[Id].Act;
				bBusy |= A == EAct::Claimed || A == EAct::Approach || A == EAct::Eat;
			}
			if (bBusy || SpeciesTable()[G.Species].bPredator)
			{
				continue;
			}
			if (!G.bStayPut)
			{
				G.Site = -1;
			}
			for (int Id : G.Members)
			{
				FBird& B = Birds[Id];
				B.Delay = 0.f;
				B.Target = -1;
				B.Alert = 0.f;
				B.bNoticed = B.bWatching = false;
				PlaceAtStart(B, G);
			}
			G.StayTimer = 20.f + Rand(Birds[G.Members[0]].Rng) * 120.f;
		}
		Approacher = -1;
		BuildHash();
	}

	void FWorld::PlaceAtStart(FBird& B, FGroup& G)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		if (G.Site < 0)
		{
			// the group's plan and place, once for all its members
			float Forage;
			int Plan;
			ScheduleOf(S, &G, Forage, Plan);
			if (Plan == 1 || Plan == 0)
			{
				Plan = Rand(B.Rng) < Forage ? 1 : 0;
			}
			if (Plan == 5)
			{
				Plan = 0;
			}
			G.Plan = Plan;
			G.Site = Plan == 2 || Plan == 4 ? G.Roost : ChooseSite(G, Plan, -1);
			if (G.Site < 0)
			{
				G.Site = G.Roost;
			}
		}
		const int Plan = G.Plan;
		B.Site = Plan == 2 ? RoostSpotSite(G, B) : G.Site;
		if (B.Site < 0)
		{
			B.Pos = G.Home;
			SetAct(B, EAct::Perch, 5.f);
			return;
		}
		B.Spot = SpotFor(B, B.Site);
		B.Pos = B.Spot;
		B.Yaw = Rand(B.Rng) * 2.f * Pi;
		Settle(B);
		if (Plan == 4 && Murm)
		{
			Murm->Place(*this, B.Id);      // already in the evening's murmuration
		}
		else if (Plan == 2 && S.bSleepsAloft)
		{
			SleepAloft(B, G);
		}
		else if (Plan == 2)
		{
			SetAct(B, EAct::Sleep, 30.f);
		}
	}

	// --- the day --------------------------------------------------------------------------------------

	void FWorld::ScheduleOf(const FSpecies& S, const FGroup* G, float& Forage, int& Plan) const
	{
		const float Wake = Sunrise + (S.WakeMinutes + (G ? G->WakeShift : 0.f)) / 60.f;
		const float Bed = Sunset + S.RoostMinutes / 60.f;
		Forage = 0.f;
		if (S.bMurmurates && Murm)
		{
			// starlings: feed all day, gather on wires and trees near the roost, murmurate over it round sunset, then drop in
			const float MurmFrom = Sunset + FMurmuration::StartMinutes / 60.f;
			if (Hour < Wake || Hour > Bed)
			{
				Plan = 2;
				return;
			}
			if (Hour > MurmFrom)
			{
				Plan = 4;
				return;
			}
			if (Hour > MurmFrom - S.StageMinutes / 60.f)
			{
				Plan = 3;
				return;
			}
		}
		else if (Hour < Wake || Hour > Bed - 0.6f)
		{
			Plan = 2;          // asleep, or on the way to the roost in the last ~40 minutes
			return;
		}
		else if (S.StageMinutes > 0.f && Hour > Bed - 0.6f - S.StageMinutes / 60.f)
		{
			Plan = 3;          // the pre-roost gathering: rooks on a field, swallows on the wires, gulls on the beach
			return;
		}
		const float T = (Hour - Wake) / std::max(Bed - Wake, 1.f);
		auto Bell = [](float X, float C, float W) { const float D = (X - C) / W; return std::exp(-D * D); };
		// hungry after the night, again before it; a rest at midday
		Forage = 0.5f + 0.45f * Bell(T, 0.08f, 0.15f) + 0.4f * Bell(T, 0.85f, 0.12f) - 0.55f * S.MiddayRest * Bell(T, 0.5f, 0.17f);
		Forage = Clamp(Forage, 0.08f, 0.97f);
		Plan = 1;
		// the midday bath (and a long preen after it), for those that do
		uint32_t H = (uint32_t)(G ? G - Groups.data() : 0) * 2654435761u ^ (uint32_t)(Time / 900.0) * 40503u;
		H = (H ^ (H >> 13)) * 0x5bd1e995u;
		if (S.Bathe > 0.f && G && Bell(T, 0.5f, 0.12f) > 0.5f && (H >> 8) / 16777216.f < S.Bathe * 0.5f)
		{
			Plan = 5;
		}
	}

	/** Swifts: the night on the wing, circling high over their home. */
	void FWorld::SleepAloft(FBird& B, const FGroup& G)
	{
		B.Site = -1;
		B.FlyFrom = G.Home + V3((Rand(B.Rng) - 0.5f) * 200.f, (Rand(B.Rng) - 0.5f) * 200.f, 0.f);
		B.FlyFrom.Z = std::max(GroundZ(B.FlyFrom.X, B.FlyFrom.Y), SeaZ) + 400.f + Rand(B.Rng) * 300.f;
		if (!IsAirborne(B.Act))
		{
			B.Pos = B.FlyFrom;
		}
		SetAct(B, EAct::Soar, 3600.f);
		B.Vel = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f) * 6.f;
	}

	void FWorld::GroupThink(FGroup& G, float Dt)
	{
		const FSpecies& S = SpeciesTable()[G.Species];
		if (S.bPredator || G.Members.empty())
		{
			return;
		}
		float Forage;
		int Plan;
		ScheduleOf(S, &G, Forage, Plan);
		G.StayTimer -= Dt;
		// after a flush the group gathers again once it has calmed down
		bool bBusy = false, bScattered = false;
		int Present = 0;
		for (int Id : G.Members)
		{
			const FBird& B = Birds[Id];
			if (B.bGone)
			{
				continue;
			}
			Present++;
			bBusy |= B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Claimed || B.Act == EAct::Flee || B.Act == EAct::Chase || B.Act == EAct::Mob;
			bScattered |= B.Site != G.Site && !(G.Plan == 2 && InRoost(G, B.Site)) && B.Act != EAct::Murmur && B.Target != -6;
		}
		if (bBusy || Present == 0)
		{
			return;
		}
		if (G.bStayPut)
		{
			// a test bird comes back to its spot a while after a scare, and sleeps there at night
			G.Plan = Plan == 2 ? 2 : 1;
			if (bScattered && Time - G.FlushedAt > 20.0 && !Avoided(G, Sites[G.Site].Center()))
			{
				SendGroup(G, G.Site);
			}
			return;
		}
		const bool bAfterFlush = Time - G.FlushedAt < 600.0 && bScattered;
		if (bAfterFlush && Time - G.FlushedAt < 12.0)
		{
			return;
		}
		if (Plan == 4)
		{
			// to the murmuration over the roost
			if (G.Plan != 4)
			{
				G.Plan = 4;
				G.Site = G.Roost;
				JoinMurmuration(G);
			}
			return;
		}
		if (Plan == 2)
		{
			if (S.bSleepsAloft)
			{
				if (G.Plan != 2)
				{
					G.Plan = 2;
					for (int Id : G.Members)
					{
						if (!Birds[Id].bGone && Birds[Id].Act != EAct::Claimed)
						{
							SleepAloft(Birds[Id], G);
						}
					}
				}
				return;
			}
			if (G.Roost >= 0 && (G.Plan != 2 || !InRoost(G, G.Site) || bScattered))
			{
				const bool bFromMurmur = G.Plan == 4;
				G.Plan = 2;
				if (!bFromMurmur)
				{
					SendGroup(G, G.Roost);
				}
				else
				{
					G.Site = G.Roost;      // the murmuration sends its birds down itself
				}
			}
			return;
		}
		if (Plan == 3)
		{
			// the pre-roost gathering: one place near the roost for the evening (several groups end up together)
			if (G.Plan != 3 || bScattered)
			{
				int Site = G.Plan == 3 && G.Site >= 0 ? G.Site : ChooseSite(G, 3, -1);
				if (Site < 0)
				{
					Site = G.Roost;
				}
				G.Plan = 3;
				if (Site >= 0)
				{
					SendGroup(G, Site);
				}
			}
			return;
		}
		if (G.Plan == 2 || G.Plan == 3 || G.Plan == 4 || G.StayTimer <= 0.f || bAfterFlush || G.Site < 0)
		{
			uint32_t& R = Birds[G.Members[0]].Rng;
			const int NewPlan = Plan == 5 ? 5 : (Rand(R) < Forage ? 1 : 0);
			int Site = -1;
			// winter: blue tits and the like roam with the great tits' flock
			if (S.bMixedFlock && IsWinter() && NewPlan != 5 && Rand(R) < 0.6f)
			{
				Site = MixedFlockSite(G);
			}
			if (Site < 0)
			{
				Site = ChooseSite(G, NewPlan, (G.Plan == NewPlan || NewPlan == 5) && !bAfterFlush ? G.Site : -1);
			}
			if (Site < 0)
			{
				Site = ChooseSite(G, NewPlan == 1 ? 0 : 1, -1);
			}
			G.Plan = NewPlan;
			// stay longer when it is good; aerial feeders keep moving over their beat
			G.StayTimer = (NewPlan == 1 ? 120.f + 300.f * Rand(R) : 200.f + 500.f * Rand(R)) * (IsAerial(S) ? 0.7f : 1.f) * (NewPlan == 5 ? 0.4f : 1.f);
			if (Site >= 0 && (Site != G.Site || bScattered))
			{
				SendGroup(G, Site);
			}
		}
	}

	/** The site of the nearest great tits' flock within reach (a winter mixed flock), or -1. */
	int FWorld::MixedFlockSite(const FGroup& G) const
	{
		static const int Core = FindSpecies("GreatTit");
		const FSpecies& S = SpeciesTable()[G.Species];
		int Best = -1;
		float BestD = S.HomeRange;
		for (const FGroup& O : Groups)
		{
			if (O.Species != Core || &O == &G || O.Site < 0 || O.Plan == 2)
			{
				continue;
			}
			const float D = (Sites[O.Site].Center() - G.Home).Len2D();
			if (D < BestD)
			{
				BestD = D;
				Best = O.Site;
			}
		}
		return Best;
	}

	/** The group takes off for the murmuration: each bird flies at the roost from where it is, joining the flock as it comes. */
	void FWorld::JoinMurmuration(FGroup& G)
	{
		if (!Murm)
		{
			return;
		}
		const V3 C = Murm->Centre();
		for (int Id : G.Members)
		{
			FBird& B = Birds[Id];
			if (B.bGone || B.Act == EAct::Claimed || B.Act == EAct::Murmur || B.Act == EAct::Approach || B.Act == EAct::Eat)
			{
				continue;
			}
			B.Delay = Rand(B.Rng) * 1.5f;
			B.Site = -1;
			B.Spot = C + V3((Rand(B.Rng) - 0.5f) * 60.f, (Rand(B.Rng) - 0.5f) * 60.f, 0.f);
			B.Spot.Z = std::max(GroundZ(B.Spot.X, B.Spot.Y), SeaZ) + 45.f;
			B.Target = -6;            // -6: to the murmuration (FMurmuration::Join on the way)
		}
	}

	void FWorld::SendGroup(FGroup& G, int Site)
	{
		G.Site = Site;
		const FSpecies& S = SpeciesTable()[G.Species];
		for (int Id : G.Members)
		{
			FBird& B = Birds[Id];
			if (B.Act == EAct::Approach || B.Act == EAct::Eat || B.Act == EAct::Claimed || B.Act == EAct::Murmur || B.bGone)
			{
				continue;
			}
			if (Site == G.Roost && G.Plan == 2)
			{
				if (!IsAirborne(B.Act) && InRoost(G, B.Site))
				{
					continue;                       // already in the bed
				}
				B.Site = RoostSpotSite(G, B);   // a reedbed: its own stems, anywhere in the bed
				B.Delay = Rand(B.Rng) * (S.FlockMax > 4 ? 1.5f : 0.6f);
				B.Spot = SpotFor(B, B.Site);
				B.Target = -2;
				continue;
			}
			if (B.Site == Site && !IsAirborne(B.Act) && B.Act != EAct::Sleep)
			{
				continue;
			}
			// the flock leaves together, a bird at a time over a second or two
			B.Delay = Rand(B.Rng) * (S.FlockMax > 4 ? 1.5f : 0.6f);
			B.Site = Site;
			B.Spot = SpotFor(B, Site);
			B.Target = -2;            // -2: fly to Spot when Delay runs out
		}
	}

	// --- acts -----------------------------------------------------------------------------------------

	void FWorld::SetAct(FBird& B, EAct A, float Len)
	{
		B.Act = A;
		B.ActTime = 0.f;
		B.ActLen = Len;
	}

	void FWorld::FlyTo(FBird& B, int Site, const V3& Spot)
	{
		B.Site = Site;
		B.Spot = Spot;
		B.FlyFrom = B.Pos;
		B.Target = -1;
		const FSpecies& S = SpeciesTable()[B.Species];
		if (IsAirborne(B.Act))
		{
			SetAct(B, EAct::Fly, 0.f);
		}
		else
		{
			SetAct(B, EAct::Takeoff, 0.35f + 0.3f * Rand(B.Rng));
			const V3 To = (Spot - B.Pos).Flat().Norm();
			B.Vel = To * (S.Cruise * 0.3f) + V3(0.f, 0.f, 2.5f);
			B.Yaw = std::atan2(To.Y, To.X);
		}
		B.bLook = false;
	}

	/** A swift on a wall: just below the ledge or on the cliff face, body against the side where the wall rises, facing it. */
	void FWorld::Cling(FBird& B, const FSite& Site)
	{
		V3 Along = (Site.B - Site.A).Flat();
		if (Along.Len2D() < 0.1f)
		{
			Along = V3(1.f, 0.f, 0.f);
		}
		Along = Along.Norm();
		V3 N(-Along.Y, Along.X, 0.f);
		const V3 P = B.Spot;
		if (TopZ(P.X - N.X * 4.f, P.Y - N.Y * 4.f) > TopZ(P.X + N.X * 4.f, P.Y + N.Y * 4.f))
		{
			N = N * -1.f;          // N points at the wall
		}
		if (Site.Kind == EHabitat::Tower || Site.Kind == EHabitat::Roof)
		{
			B.Spot = P - N * 0.12f - V3(0.f, 0.f, 0.8f);   // under the ledge, on the outside of the wall
		}
		B.Pos = B.Spot;
		B.Yaw = std::atan2(N.Y, N.X);
	}

	/** Arrived on a site: what it does there. */
	void FWorld::Settle(FBird& B)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		B.Vel = V3();
		B.Pos = B.Spot;
		B.Target = -1;
		if (S.bClings && B.Site >= 0 && !IsGroundHabitat(Sites[B.Site].Kind))
		{
			Cling(B, Sites[B.Site]);
		}
		if (B.Site < 0)
		{
			SetAct(B, EAct::Perch, 3.f);
			return;
		}
		const FSite& Site = Sites[B.Site];
		const FGroup& G = Groups[B.Group];
		if (Site.Kind == EHabitat::Wire)
		{
			// a row on the wire: everybody faces the same way, across it
			const V3 Along = (Site.B - Site.A).Flat().Norm();
			B.Yaw = std::atan2(Along.Y, Along.X) + ((G.Slot0 & 1) ? 0.5f : -0.5f) * Pi + (Rand(B.Rng) - 0.5f) * 0.3f;
		}
		if (G.Plan == 2 && InRoost(G, B.Site))
		{
			float F;
			int Plan;
			ScheduleOf(S, &G, F, Plan);
			if (Site.Kind == EHabitat::Reed && B.Pos.Z < Site.A.Z + 0.6f)
			{
				// up on the reed stems, out of reach of a fox
				B.Pos.Z += 0.7f + Rand(B.Rng) * 1.1f;
				B.Spot = B.Pos;
			}
			SetAct(B, Plan == 2 && (Hour < Sunrise || Hour > Sunset + S.RoostMinutes / 60.f - 0.25f) ? EAct::Sleep : EAct::Perch, 20.f + Rand(B.Rng) * 20.f);
			return;
		}
		if (S.bDriesWings && G.Plan == 0 && Site.Kind != EHabitat::Sea)
		{
			SetAct(B, EAct::Preen, 60.f + Rand(B.Rng) * 120.f);   // a cormorant on the beach, wings held out to dry
			return;
		}
		if (G.Plan == 5 && Site.Kind != EHabitat::Sea)
		{
			SetAct(B, EAct::Preen, 6.f + Rand(B.Rng) * 10.f);     // the bath at the water's edge, then a long preen
			return;
		}
		if (Site.Kind == EHabitat::Sea)
		{
			SetAct(B, EAct::Swim, 3.f + Rand(B.Rng) * 8.f);
		}
		else if (G.Plan == 1 && IsAerial(S))
		{
			SetAct(B, EAct::Hawk, 2.f + Rand(B.Rng) * 3.f);
			B.Spot = B.Pos + V3(0.f, 0.f, 3.f);
		}
		else if (IsGroundHabitat(Site.Kind) && G.Plan == 1)
		{
			SetAct(B, EAct::Peck, 0.5f + Rand(B.Rng) * 1.5f);
		}
		else
		{
			SetAct(B, EAct::Perch, 2.f + Rand(B.Rng) * 6.f);
		}
	}

	void FWorld::Flush(FBird& B, const V3& From, int Cause)
	{
		if (B.Act == EAct::Claimed || B.Act == EAct::Flee || B.Act == EAct::Murmur)
		{
			return;                 // (a murmuration escapes as one: FMurmuration)
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		if (B.Id == Approacher)
		{
			Approacher = -1;
		}
		// away from the danger: small birds dive into the nearest cover that is not toward it,
		// the rest fly off a good way and decide later
		V3 Away = (B.Pos - From).Flat();
		if (Away.Len2D() < 0.1f)
		{
			Away = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f);
		}
		Away = Away.Norm();
		int Cover = -1;
		if (IsSmall(S) && !IsAerial(S))
		{
			float Best = 1e9f;
			for (int I = 0; I < (int)Sites.size(); I++)
			{
				const FSite& C = Sites[I];
				if (C.Kind != EHabitat::Garden && C.Kind != EHabitat::Broadleaf && C.Kind != EHabitat::Conifer && C.Kind != EHabitat::Reed)
				{
					continue;
				}
				const V3 D = C.Center() - B.Pos;
				const float L = D.Len2D();
				if (L < 8.f || L > 140.f || D.Flat().Norm().Dot(Away) < 0.2f || (C.Center() - From).Len2D() < 2.5f * S.FleeDistance + 15.f)
				{
					continue;
				}
				if (L < Best)
				{
					Best = L;
					Cover = I;
				}
			}
		}
		B.FlyFrom = B.Pos;
		if (Cover >= 0)
		{
			B.Site = Cover;
			B.Spot = SpotFor(B, Cover);
		}
		else
		{
			const float Dist = (S.Length > 0.3f ? 80.f : 40.f) + Rand(B.Rng) * 80.f;
			const float Turn = (Rand(B.Rng) - 0.5f) * 0.8f;
			const V3 Dir(Away.X * std::cos(Turn) - Away.Y * std::sin(Turn), Away.X * std::sin(Turn) + Away.Y * std::cos(Turn), 0.f);
			B.Spot = B.Pos + Dir * Dist;
			B.Spot.Z = std::max(TopZ(B.Spot.X, B.Spot.Y), SeaZ) + S.FlightClearance + 6.f;
			B.Site = -1;
		}
		const bool bWasAir = IsAirborne(B.Act);
		SetAct(B, EAct::Flee, 0.f);
		if (!bWasAir)
		{
			B.Vel = Away * (S.Cruise * 0.5f) + V3(0.f, 0.f, 3.5f);
		}
		B.Yaw = std::atan2(Away.Y, Away.X);
		B.Target = Cause;
		B.bLook = false;
		B.bWatching = false;
		B.bAte = false;
		B.Alert = 1.f;
		if (Cause == -1)
		{
			B.Trust *= 0.6f;
		}
		FGroup& G = Groups[B.Group];
		G.FlushedAt = Time;
		// it stays watchful a good while, and the group keeps away from the danger
		B.Wary = 45.f + Rand(B.Rng) * 45.f;
		const bool bPlayer = Cause == -1 || (Player.bPresent && (From - Player.Feet).Len() < 3.f);
		G.AvoidUntil = Time + (bPlayer ? 120.0 : 60.0);
		G.Avoid = From;
		G.AvoidR = 2.5f * S.FleeDistance + 15.f;
		G.bAvoidPlayer = bPlayer;
		if (G.Site >= 0 && Cause != -3)
		{
			Sites[G.Site].Disturbed = (float)Time + 240.f;
		}
		Emit(EEvent::Flee, B);
		Alarm(B, 18.f, 1.f);
	}

	void FWorld::Alarm(const FBird& From, float Radius, float Strength)
	{
		const FSpecies& S = SpeciesTable()[From.Species];
		const V3 Danger = From.FlyFrom;
		ForNear(From.Pos, Radius, [&](int I)
		{
			FBird& B = Birds[I];
			if (I == From.Id || B.Act == EAct::Flee || B.Act == EAct::Claimed || B.bGone || IsAirborne(B.Act))
			{
				return;
			}
			const FSpecies& T = SpeciesTable()[B.Species];
			if (T.bPredator)
			{
				return;
			}
			// the flock goes up together; others nearby are startled, small ones by a big bird more
			float K = 1.4f;
			if (B.Group != From.Group)
			{
				K = (B.Species == From.Species ? 0.9f : 0.6f) * Clamp(0.8f + (S.Length - T.Length) * 2.f, 0.5f, 1.4f);
			}
			const float D = (B.Pos - From.Pos).Len();
			const float Add = Strength * K * (B.Group == From.Group ? 1.f : 1.f - D / (Radius * 1.2f));
			// an alarm call makes the others look up and get uneasy, but only a bird actually flying off sends them up
			B.Alert = Strength < 0.5f ? std::min(B.Alert + Add, std::max(B.Alert, 0.7f)) : B.Alert + Add;
			B.Habituation = 0.f;
			if (B.Alert >= 1.f && B.Delay <= 0.f)
			{
				B.Delay = 0.08f + Rand(B.Rng) * 0.25f + D * 0.015f;   // the wave spreads through the flock
				B.FlyFrom = Danger;
				B.Target = -3;
			}
		});
	}

	void FWorld::Scare(const V3& P, float R, float Strength)
	{
		ForNear(P, R, [&](int I)
		{
			FBird& B = Birds[I];
			if (B.Act == EAct::Claimed || B.bGone)
			{
				return;
			}
			B.Alert += Strength * (1.f - (B.Pos - P).Len() / R);
			if (B.Alert >= 1.f)
			{
				B.Delay = 0.05f + Rand(B.Rng) * 0.2f;
				B.FlyFrom = P;
				B.Target = -3;
			}
		});
	}

	// --- perception -----------------------------------------------------------------------------------

	/** How close (m) the player may come before this bird flies, given how they move now. */
	float FWorld::Danger(const FBird& B, float Dist, bool bSeen) const
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		const FPlayer& P = Player;
		const V3 ToBird = (B.Pos - P.Feet).Flat().Norm();
		const float Speed = P.Vel.Len2D();
		const float Posture = P.bCrouched ? 0.6f : 1.f;
		const float Motion = 0.3f + 0.7f * std::min(Speed / 1.4f, 4.f);
		const float Closing = std::max(0.f, P.Vel.Dot(ToBird));
		const float Approach = 1.f + 0.6f * std::min(Closing / 1.4f, 1.5f);
		const float Gaze = bSeen && P.Facing.Dot((B.Pos - P.Eye).Norm()) > 0.97f ? 1.25f : 1.f;
		const float Turn = std::max(0.f, P.TurnRate - 100.f) / 200.f;
		const float Hand = P.bOffer ? std::max(0.f, P.HandSpeed - 0.8f) * 0.8f : 0.f;
		const float Sudden = (Turn + Hand + P.ExtraThreat) * (Dist < 25.f ? 1.f : 25.f / Dist) * (bSeen ? 1.f : 0.3f);
		// 1 = a person walking straight at the bird, looking at it (what FleeDistance is measured for);
		// a wary bird is never fooled completely by a crouch
		float L = std::max(Posture * Motion * Approach * Gaze / 2.f, 0.35f) * (1.f - 0.6f * B.Habituation) + Sudden;
		if (P.bClaimedBird)
		{
			L *= 0.15f;
		}
		return std::max(0.4f, S.FleeDistance * L * (1.f - 0.75f * B.Trust) / B.Boldness);
	}

	void FWorld::PerceivePlayer(FBird& B, float Dt)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		if (!Player.bPresent || S.bPredator)
		{
			B.Alert = std::max(0.f, B.Alert - Dt / 8.f);
			return;
		}
		const V3 Body = Player.Feet + V3(0.f, 0.f, Player.bCrouched ? 0.7f : 1.1f);
		const float Dist = (B.Pos - Body).Len();
		// hearing: footsteps carry farther when running
		const float Speed = Player.Vel.Len2D();
		const float Hearing = Player.bCrouched ? 2.5f * std::min(Speed / 0.6f, 1.f) : (Speed < 0.2f ? 0.f : (Speed > 3.5f ? 25.f : 10.f));
		bool bSeen = false;
		if (Dist < S.SightRange)
		{
			const V3 Head = B.bLook ? (B.Look - B.Pos).Norm() : V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f);
			const float Cos = Head.Dot((Body - B.Pos).Norm());
			bSeen = Cos > std::cos(S.Fov * 0.5f * Pi / 180.f);
			if (bSeen && B.Tier == 0 && Sight)
			{
				bSeen = Sight(B.Pos + V3(0.f, 0.f, 0.1f), Player.Eye);
			}
		}
		const bool bHeard = Dist < Hearing;
		// a person who keeps still (and quiet in their hands) stops being frightening after a while
		const float Jerk = std::max(0.f, Player.TurnRate - 60.f) / 120.f + (Player.bOffer ? std::max(0.f, Player.HandSpeed - 0.4f) : 0.f) + Player.ExtraThreat;
		if (Dist < 30.f)
		{
			B.Habituation = Clamp(B.Habituation + Dt * (Speed < 0.3f && Jerk <= 0.f ? 1.f / 8.f : -(Speed / 1.5f + Jerk * 2.f)), 0.f, 1.f);
		}
		float Target = 0.f;
		float D = 0.f;
		bool bPerceived = false;
		if (bSeen || bHeard)
		{
			D = Danger(B, Dist, bSeen);
			// it notices the player well before it minds them; unease grows from AlertDist in to D
			const float Notice = std::min(std::max(D * 2.6f, S.FleeDistance * 1.6f) + 2.f, bSeen ? S.SightRange : Hearing);
			const float AlertDist = std::min(D * 2.5f + 1.5f, Notice);
			Target = Dist >= AlertDist ? 0.f : Clamp((AlertDist - Dist) / std::max(AlertDist - D, 0.5f), 0.f, 1.3f);
			bPerceived = Dist < Notice * (B.bNoticed ? 1.4f : 1.f);
		}
		B.Unseen = bPerceived ? 0.f : B.Unseen + Dt;
		if (B.Act == EAct::Approach || B.Act == EAct::Eat)
		{
			// at the hand it only minds sudden moves
			Target = Clamp((D - 0.6f) * 0.8f, 0.f, 1.3f);
		}
		if (Target > B.Alert)
		{
			B.Alert += (Target - B.Alert) * std::min(1.f, Dt / 0.35f);
		}
		else
		{
			B.Alert = std::max(Target, B.Alert - Dt / (B.Wary > 0.f ? 45.f : 7.f));
		}

		// befriending: food held out by someone who keeps calm and low makes it trust them, slowly
		const float PalmDist = (B.Pos - Player.Palm).Len();
		if (B.bNoticed && Player.bOffer && PalmDist < 16.f && B.Alert < 0.6f && Dist > 0.f)
		{
			const float Calm = Clamp(1.f - (D / std::max(S.FleeDistance, 0.5f) - 0.3f), 0.f, 1.f);
			B.Trust = std::min(1.f, B.Trust + Dt * 0.05f * (0.2f + 0.8f * B.Habituation) * S.Tameness * (0.5f + 0.5f * B.Boldness) * (1.f - B.Alert) * Calm * (PalmDist < 8.f ? 1.4f : 1.f));
		}
		else if (B.bNoticed && Dist < 20.f && B.Alert < 0.3f)
		{
			B.Trust = std::min(0.4f, B.Trust + Dt * 0.004f * S.Tameness);   // used to people around
		}

		// the stages the player can read: notice, turn the head, watch, call, (approach), flee
		if (!B.bNoticed && bPerceived && (B.Alert > 0.04f || Player.bOffer || Dist < S.FleeDistance * 2.f))
		{
			B.bNoticed = true;
			B.bTurned = false;
			B.Seen = 0.f;
			Emit(EEvent::Notice, B);
		}
		if (B.bNoticed)
		{
			B.Seen += Dt;
			if (B.Alert > 0.04f || B.Act == EAct::Eat || B.Act == EAct::Approach)
			{
				B.Look = Player.bOffer && B.Trust > 0.3f ? Player.Palm : Player.Eye;
				B.bLook = true;
			}
			if (!B.bTurned && B.Seen > 0.35f)
			{
				B.bTurned = true;
				if (!IsAirborne(B.Act))
				{
					const V3 To = (Body - B.Pos).Flat();
					B.Yaw = std::atan2(To.Y, To.X) + (Rand(B.Rng) - 0.5f) * 1.2f;    // half turns its body to look
				}
				Emit(EEvent::Turn, B);
			}
			if (B.Unseen > 6.f)
			{
				B.bNoticed = false;
				B.bLook = false;
			}
		}
		const bool bCanWatch = !IsAirborne(B.Act) && B.Act != EAct::Sleep && B.Act != EAct::Eat;
		// curious: food held out near it is worth a long look (and a call) even when it is not afraid
		const bool bCurious = B.bNoticed && Player.bOffer && PalmDist < 16.f && B.Seen > 1.f && Rand(B.Rng) < Dt * (0.3f + S.Curiosity);
		if (bCanWatch && !B.bWatching && (B.Alert > 0.35f || bCurious))
		{
			B.bWatching = true;
			SetAct(B, EAct::Watch, 1.f + Rand(B.Rng) * 2.f);
			Emit(EEvent::Watch, B);
			if (B.Alert > 0.35f && Rand(B.Rng) < 0.5f + 0.5f * B.Alert)
			{
				Emit(EEvent::Call, B);           // an alarm note: the others look up
				Alarm(B, 25.f, 0.12f);
			}
			else if (B.Trust > 0.3f && Rand(B.Rng) < 0.5f)
			{
				Emit(EEvent::Call, B);           // a contact call to the hand
			}
		}
		else if (B.bWatching && B.Alert < 0.2f && B.Act == EAct::Watch && B.ActTime > 1.5f && !(Player.bOffer && PalmDist < 16.f && B.ActTime < 6.f))
		{
			B.bWatching = false;
			Settle(B);
		}
		// almost too close: a nervous step away first (the player can still stop)
		if (B.Act == EAct::Watch && B.Alert > 0.75f && B.Alert < 1.f && B.ActTime > 0.6f && B.Site >= 0 && IsGroundHabitat(Sites[B.Site].Kind)
			&& Sites[B.Site].Kind != EHabitat::Sea)
		{
			const V3 Away = (B.Pos - Body).Flat().Norm();
			B.Spot = B.Pos + Away * (0.6f + Rand(B.Rng) * 0.8f);
			if (Probe)
			{
				float Z;
				if (Probe(B.Spot.X, B.Spot.Y, Z))
				{
					B.Spot.Z = Z;
				}
			}
			SetAct(B, EAct::Forage, 0.f);
			B.bWatching = false;
		}
		// town birds (pigeons, sparrows) walk out of the way of someone strolling past rather than fly
		// passing by = their straight line misses the bird by a metre or more
		const V3 ToBird = (B.Pos - Body).Flat();
		const V3 Heading = Player.Vel.Flat().Norm();
		const float Miss = std::fabs(Heading.X * ToBird.Y - Heading.Y * ToBird.X);
		const bool bPassing = Speed < 0.3f || Heading.Dot(ToBird) < 0.f || Miss > 1.f;
		const bool bOnFoot = S.FleeDistance <= 6.f && S.Forage[(int)EHabitat::Street] >= 0.8f && Speed < 2.f && bPassing && B.Alert < 1.25f && !IsAirborne(B.Act) && B.Act != EAct::Eat
			&& B.Site >= 0 && IsGroundHabitat(Sites[B.Site].Kind) && Sites[B.Site].Kind != EHabitat::Sea && Dist > 0.8f;
		if (B.Alert >= 1.f && bOnFoot && B.Act != EAct::Forage)
		{
			const V3 Away = (B.Pos - Body).Flat().Norm();
			B.Spot = B.Pos + Away * (1.2f + Rand(B.Rng) * 1.2f);
			if (Probe)
			{
				float Z;
				if (Probe(B.Spot.X, B.Spot.Y, Z))
				{
					B.Spot.Z = Z;
				}
			}
			SetAct(B, EAct::Forage, 0.f);
			B.bWatching = false;
			B.Alert = 0.85f;
		}
		else if (B.Alert >= 1.f && !bOnFoot && B.Delay <= 0.f && B.Act != EAct::Flee)
		{
			B.Delay = 0.05f + Rand(B.Rng) * 0.2f;
			B.FlyFrom = Body;
			B.Target = -1;
		}
	}

	void FWorld::PerceiveBirds(FBird& B, float Dt)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		if (B.Act == EAct::Sleep || IsAirborne(B.Act) || B.Act == EAct::Eat)
		{
			return;
		}
		const float Personal = 0.5f + S.Length * 2.f;
		int Stronger = -1;
		int Prey = -1;
		ForNear(B.Pos, S.bKlepto ? 22.f : Personal + 1.5f, [&](int I)
		{
			if (I == B.Id)
			{
				return;
			}
			const FBird& O = Birds[I];
			if (O.bGone || IsAirborne(O.Act) || O.Act == EAct::Claimed)
			{
				return;
			}
			const FSpecies& T = SpeciesTable()[O.Species];
			const float D = (O.Pos - B.Pos).Len();
			if (D < Personal + T.Length && T.Dominance > S.Dominance + 8)
			{
				Stronger = I;
			}
			if (S.bKlepto && O.Act == EAct::Peck && O.ActLen > 2.5f && T.Dominance < S.Dominance + (O.Species == B.Species ? 1 : 0) && O.Group != B.Group)
			{
				Prey = I;
			}
		});
		// a bigger bird walks up: give way (hop aside, or move along the ridge / to another branch)
		if (Stronger >= 0 && B.Act != EAct::Watch && Rand(B.Rng) < Dt * 1.5f)
		{
			const V3 Away = (B.Pos - Birds[Stronger].Pos).Flat().Norm();
			if (B.Site >= 0 && IsGroundHabitat(Sites[B.Site].Kind))
			{
				B.Spot = B.Pos + Away * (0.8f + S.Length * 4.f);
				if (Probe)
				{
					float Z;
					if (Probe(B.Spot.X, B.Spot.Y, Z))
					{
						B.Spot.Z = Z;
					}
				}
				SetAct(B, EAct::Forage, 0.f);
			}
			else if (B.Site >= 0)
			{
				FlyTo(B, B.Site, SpotFor(B, B.Site));
			}
			Emit(EEvent::Supplant, B);
		}
		// gulls and crows go for a bird that found something big
		if (Prey >= 0 && Rand(B.Rng) < Dt * 0.025f)
		{
			B.Target = Prey;
			B.FlyFrom = B.Pos;
			SetAct(B, EAct::Chase, 4.f + Rand(B.Rng) * 4.f);
			B.Vel = V3(0.f, 0.f, 2.5f);
			Emit(EEvent::ChaseStart, B);
			Emit(EEvent::Call, B);
		}
	}

	/** Cats and dogs: a running dog puts a flock up from far off, a walking cat from about where a person would, a stalking
	 *  cat only when it is almost there; birds up a tree or on a roof hardly mind a dog, but they scold a cat. */
	void FWorld::PerceiveBeasts(FBird& B, float Dt)
	{
		if (Beasts.empty() || IsAirborne(B.Act) || B.Act == EAct::Claimed || B.bGone || B.Act == EAct::Approach || B.Act == EAct::Eat)
		{
			return;
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		if (S.bPredator)
		{
			return;
		}
		const bool bGround = B.Site >= 0 && IsGroundHabitat(Sites[B.Site].Kind) && Sites[B.Site].Kind != EHabitat::Sea;
		float Worst = 0.f;
		V3 From;
		bool bCatNear = false;
		for (const FBeast& M : Beasts)
		{
			const V3 D = B.Pos - M.Pos;
			const float Dist = D.Len();
			if (Dist > 45.f)
			{
				continue;
			}
			const float Speed = M.Vel.Len2D();
			const float Closing = std::max(0.f, M.Vel.Dot(D.Flat().Norm()));
			float K = M.bCat ? (M.bStalking ? 0.17f : 0.9f + 0.3f * std::min(Speed / 2.f, 2.f)) : 0.7f + 0.55f * std::min(Speed / 2.5f, 2.5f);
			K *= 1.f + 0.4f * std::min(Closing / 2.f, 1.f);
			const float Up = B.Pos.Z - M.Pos.Z;
			if (!bGround)
			{
				K *= Up > 2.5f ? (M.bCat ? 0.35f : 0.12f) : 0.6f;    // a cat climbs; a dog can't
			}
			if (B.Act == EAct::Sleep)
			{
				K *= 0.6f;
			}
			const float FleeD = std::max(0.8f, S.FleeDistance * K) / B.Boldness;
			const float A = Clamp((FleeD * 2.f - Dist) / FleeD, 0.f, 1.3f);
			if (A > Worst)
			{
				Worst = A;
				From = M.Pos;
			}
			bCatNear |= M.bCat && Dist < 20.f && !M.bStalking;
		}
		if (Worst <= 0.f)
		{
			return;
		}
		if (Worst > B.Alert)
		{
			B.Alert += (Worst - B.Alert) * std::min(1.f, Dt / 0.3f);
		}
		B.Habituation = 0.f;
		if (!B.bLook || Worst > 0.3f)
		{
			B.Look = From + V3(0.f, 0.f, 0.2f);
			B.bLook = true;
		}
		// tits, blackbirds, robins scold a cat they can see from their twig
		if (bCatNear && S.bMobs && !bGround && Rand(B.Rng) < Dt * 0.4f)
		{
			Emit(EEvent::Call, B);
			Alarm(B, 20.f, 0.08f);
		}
		if (B.Alert >= 1.f && B.Delay <= 0.f && B.Act != EAct::Flee)
		{
			B.Delay = 0.04f + Rand(B.Rng) * 0.15f;
			B.FlyFrom = From;
			B.Target = -4;
		}
	}

	// --- hand feeding ---------------------------------------------------------------------------------

	void FWorld::UpdateApproach(FBird& B, float Dt)
	{
		(void)Dt;
		const FSpecies& S = SpeciesTable()[B.Species];
		if (!Player.bOffer || Approacher >= 0 || B.bGone || S.bPredator || IsAerial(S) || S.Length > 0.5f)
		{
			return;
		}
		if (!B.bNoticed || B.Trust < 0.55f || B.Habituation < 0.5f || B.Alert > 0.35f || B.Hunger < 0.15f || IsAirborne(B.Act) || B.Act == EAct::Sleep)
		{
			return;
		}
		if ((B.Pos - Player.Palm).Len() > 14.f || Danger(B, 3.f, true) > 2.f)
		{
			return;
		}
		// the boldest trusting bird around comes; it calls first sometimes
		Approacher = B.Id;
		Emit(EEvent::Approach, B);
		if (Rand(B.Rng) < 0.5f)
		{
			Emit(EEvent::Call, B);
		}
		B.FlyFrom = B.Pos;
		B.Spot = Player.Palm;
		B.Site = -1;
		const V3 To = (Player.Palm - B.Pos).Flat().Norm();
		B.Vel = To * 2.f + V3(0.f, 0.f, 2.f);
		SetAct(B, EAct::Approach, 0.f);
		B.bAte = false;
	}

	// --- predators ------------------------------------------------------------------------------------

	void FWorld::PredatorThink(FPredator& P, float Dt)
	{
		FBird& H = Birds[P.Bird];
		const FSpecies& S = SpeciesTable()[H.Species];
		FGroup& G = Groups[H.Group];
		float Forage;
		int Plan;
		ScheduleOf(S, &G, Forage, Plan);
		if (H.Act == EAct::Claimed)
		{
			return;
		}
		// a falcon comes to the starlings' murmuration at dusk, again and again
		const bool bMurmHunt = Murm && Murm->Count() > 60 && S.Length > 0.38f && Opt.bPredators;
		if (bMurmHunt && Plan == 2)
		{
			Plan = 1;
		}
		if (Plan == 2)
		{
			if (H.Site != G.Roost && !IsAirborne(H.Act) && G.Roost >= 0)
			{
				G.Plan = 2;
				FlyTo(H, G.Roost, SpotOn(Sites[G.Roost], H.Rng));
			}
			return;
		}
		// mobbed on its perch: the hawk gives up and goes elsewhere
		if (P.Mobbed > 12.f && !IsAirborne(H.Act))
		{
			P.Mobbed = 0.f;
			const int Site = ChooseSite(G, 0, H.Site);
			if (Site >= 0)
			{
				FlyTo(H, Site, SpotOn(Sites[Site], H.Rng));
			}
			return;
		}
		if (H.Act == EAct::Chase)
		{
			// the attack: alarm everything small round it; give up after a few seconds
			Alarm(H, 40.f, 2.f);
			if (H.ActTime > H.ActLen || P.Prey < 0 || Birds[P.Prey].bGone)
			{
				const int Site = ChooseSite(G, 0, -1);
				if (Site >= 0)
				{
					FlyTo(H, Site, SpotOn(Sites[Site], H.Rng));
				}
				P.Prey = -1;
				P.Timer = bMurmHunt ? 30.f + Rand(H.Rng) * 90.f : 600.f + Rand(H.Rng) * 1200.f;
			}
			return;
		}
		P.Timer -= Dt;
		if (P.Timer > 0.f || IsAirborne(H.Act))
		{
			if (!IsAirborne(H.Act) && H.ActTime > 120.f)
			{
				const int Site = ChooseSite(G, 0, H.Site);   // a new lookout now and then
				if (Site >= 0)
				{
					FlyTo(H, Site, SpotOn(Sites[Site], H.Rng));
				}
			}
			return;
		}
		// hunt: the nearest worthwhile prey within 250 m, sooner near the player (so it can be seen); at dusk a bird on the
		// murmuration's rim
		int Best = bMurmHunt ? Murm->RimBird(*this, H.Rng) : -1;
		float BestW = Best >= 0 ? 1e9f : 0.f;
		for (int I = 0; I < (int)Birds.size(); I++)
		{
			const FBird& O = Birds[I];
			const FSpecies& T = SpeciesTable()[O.Species];
			if (T.PreyValue <= 0.f || O.bGone || O.Act == EAct::Sleep || O.Act == EAct::Claimed || O.Act == EAct::Eat || O.Act == EAct::Approach)
			{
				continue;
			}
			const float D = (O.Pos - H.Pos).Len();
			if (D > 250.f || Best >= 0)
			{
				continue;
			}
			const float W = T.PreyValue / (1.f + D / 60.f) * (O.Tier <= 1 ? 2.f : 1.f) * (0.5f + Rand(H.Rng));
			if (W > BestW)
			{
				BestW = W;
				Best = I;
			}
		}
		if (Best < 0)
		{
			P.Timer = 60.f;
			return;
		}
		P.Prey = Best;
		H.Target = Best;
		H.FlyFrom = H.Pos;
		if (!IsAirborne(H.Act))
		{
			H.Vel = (Birds[Best].Pos - H.Pos).Norm() * 6.f;
		}
		if (bMurmHunt && H.Tier >= 3 && (H.Pos - Birds[Best].Pos).Len() > 150.f)
		{
			// from high above, out of the sun: a stoop
			H.Pos = Birds[Best].Pos + V3((Rand(H.Rng) - 0.5f) * 120.f, (Rand(H.Rng) - 0.5f) * 120.f, 90.f + Rand(H.Rng) * 40.f);
			H.Vel = (Birds[Best].Pos - H.Pos).Norm() * 25.f;
		}
		SetAct(H, EAct::Chase, (bMurmHunt ? 9.f : 6.f) + Rand(H.Rng) * 4.f);
		Emit(EEvent::ChaseStart, H);
	}

	// --- the step -------------------------------------------------------------------------------------

	void FWorld::BuildHash()
	{
		if (HashNX == 0)
		{
			const float W = Grid.NX ? Grid.NX * Grid.Cell : 3000.f, H = Grid.NY ? Grid.NY * Grid.Cell : 3500.f;
			HashX0 = Grid.NX ? Grid.X0 : -1500.f;
			HashY0 = Grid.NY ? Grid.Y0 : -1000.f;
			HashNX = (int)(W / HashCell) + 1;
			HashNY = (int)(H / HashCell) + 1;
		}
		CellHead.assign((size_t)HashNX * HashNY, -1);
		CellNext.assign(Birds.size(), -1);
		for (int I = 0; I < (int)Birds.size(); I++)
		{
			const FBird& B = Birds[I];
			if (B.bGone)
			{
				continue;
			}
			const int CX = (int)((B.Pos.X - HashX0) / HashCell), CY = (int)((B.Pos.Y - HashY0) / HashCell);
			if (CX < 0 || CY < 0 || CX >= HashNX || CY >= HashNY)
			{
				continue;
			}
			int& Head = CellHead[(size_t)CY * HashNX + CX];
			CellNext[I] = Head;
			Head = I;
		}
	}

	void FWorld::ForNear(const V3& P, float R, const std::function<void(int)>& Fn) const
	{
		if (R <= 0.f || CellHead.empty())
		{
			return;
		}
		const int X0 = std::max(0, (int)((P.X - R - HashX0) / HashCell)), X1 = std::min(HashNX - 1, (int)((P.X + R - HashX0) / HashCell));
		const int Y0 = std::max(0, (int)((P.Y - R - HashY0) / HashCell)), Y1 = std::min(HashNY - 1, (int)((P.Y + R - HashY0) / HashCell));
		const float R2 = R * R;
		for (int CY = Y0; CY <= Y1; CY++)
		{
			for (int CX = X0; CX <= X1; CX++)
			{
				for (int I = CellHead[(size_t)CY * HashNX + CX]; I >= 0; I = CellNext[I])
				{
					const V3 D = Birds[I].Pos - P;
					if (D.Dot(D) <= R2)
					{
						Fn(I);
					}
				}
			}
		}
	}

	void FWorld::Near(const V3& P, float R, std::vector<int>& Out) const
	{
		Out.clear();
		ForNear(P, R, [&](int I) { Out.push_back(I); });
	}

	void FWorld::UpdateTiers()
	{
		for (int& C : TierCount)
		{
			C = 0;
		}
		for (FBird& B : Birds)
		{
			const float D = std::min((B.Pos - Camera).Len(), Player.bPresent ? (B.Pos - Player.Eye).Len() : 1e9f);
			B.Tier = D < Opt.NearDist ? 0 : (D < Opt.MidDist ? 1 : (D < Opt.FarDist ? 2 : 3));
			if (B.Act == EAct::Approach || B.Act == EAct::Eat || B.Id == Approacher)
			{
				B.Tier = 0;
			}
			TierCount[B.Tier]++;
		}
	}

	void FWorld::Step(float Dt)
	{
		Dt = Clamp(Dt, 0.f, 0.25f);
		Time += Dt;
		Player.ExtraThreat = std::max(0.f, Player.ExtraThreat - Dt * 1.5f);
		UpdateTiers();
		BuildHash();

		// site crowding, every couple of seconds
		static const double CrowdEvery = 2.0;
		if (std::floor(Time / CrowdEvery) != std::floor((Time - Dt) / CrowdEvery))
		{
			for (FSite& S : Sites)
			{
				S.Occupants = 0;
				S.TopDominance = 0;
			}
			for (const FBird& B : Birds)
			{
				if (B.Site >= 0 && !B.bGone && !IsAirborne(B.Act))
				{
					FSite& S = Sites[B.Site];
					S.Occupants++;
					S.TopDominance = std::max(S.TopDominance, SpeciesTable()[B.Species].Dominance);
				}
			}
			for (FGroup& G : Groups)
			{
				GroupThink(G, (float)CrowdEvery);
			}
		}
		for (FPredator& P : Predators)
		{
			PredatorThink(P, Dt);
		}

		// coarse tiers move in bigger steps; render positions are extrapolated in between
		for (int T = 2; T <= 3; T++)
		{
			StepAcc[T] += Dt;
		}
		const bool bMoveFar = StepAcc[2] >= Opt.StepFar;
		const bool bMoveDormant = StepAcc[3] >= Opt.StepDormant;
		const float FarDt = StepAcc[2], DormantDt = StepAcc[3];
		const float ThinkEvery[4] = { Opt.ThinkNear, Opt.ThinkMid, Opt.ThinkFar, Opt.ThinkDormant };
		for (FBird& B : Birds)
		{
			if (B.bGone || B.Act == EAct::Claimed)
			{
				continue;
			}
			B.ThinkTimer -= Dt;
			B.ActTime += Dt;
			if (B.Delay > 0.f)
			{
				B.Delay -= Dt;
				if (B.Delay <= 0.f)
				{
					if (B.Target == -2 && B.Site >= 0)
					{
						const V3 Spot = B.Spot;
						FlyTo(B, B.Site, Spot);
					}
					else if (B.Target == -6)
					{
						const V3 Spot = B.Spot;
						FlyTo(B, -1, Spot);
						B.Target = -6;
					}
					else if (B.Alert >= 1.f)
					{
						Flush(B, B.FlyFrom, B.Target);
					}
				}
			}
			if (B.ThinkTimer <= 0.f)
			{
				const float Since = std::max(Dt, ThinkEvery[B.Tier] - B.ThinkTimer);
				B.ThinkTimer = ThinkEvery[B.Tier] * (0.8f + 0.4f * Rand(B.Rng));
				Think(B, std::min(Since, 3.f));
			}
			if (B.Act == EAct::Murmur)
			{
				// flown by the murmuration below
			}
			else if (B.Tier <= 1)
			{
				Move(B, Dt);
			}
			else if (B.Tier == 2 && bMoveFar)
			{
				Move(B, FarDt);
			}
			else if (B.Tier == 3 && bMoveDormant)
			{
				Move(B, DormantDt);
			}
		}
		if (Murm)
		{
			Murm->Step(*this, Dt);
		}
		if (bMoveFar)
		{
			StepAcc[2] = 0.f;
		}
		if (bMoveDormant)
		{
			StepAcc[3] = 0.f;
		}
		if (Approacher >= 0 && (!Player.bOffer && Birds[Approacher].Act != EAct::Approach && Birds[Approacher].Act != EAct::Eat))
		{
			Approacher = -1;
		}
		for (int& C : SpeciesCount)
		{
			C = 0;
		}
		for (const FBird& B : Birds)
		{
			if (!B.bGone && B.Species < 64)
			{
				SpeciesCount[B.Species]++;
			}
		}
	}

	V3 FWorld::RenderPos(const FBird& B) const
	{
		if (B.Act == EAct::Murmur && Murm)
		{
			return B.Pos + B.Vel * Murm->Ahead();
		}
		if (B.Tier >= 2 && IsAirborne(B.Act))
		{
			return B.Pos + B.Vel * StepAcc[B.Tier];
		}
		return B.Pos;
	}

	void FWorld::Think(FBird& B, float Dt)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		if (B.Act == EAct::Murmur)
		{
			// high in the murmuration: the flock is all it minds (its calls are the roar of the wings and the chatter)
			B.Alert = 0.f;
			B.bNoticed = B.bWatching = false;
		}
		else if (B.Tier <= 2)
		{
			PerceivePlayer(B, Dt);
			PerceiveBeasts(B, Dt);
			if (B.Tier <= 1)
			{
				PerceiveBirds(B, Dt);
				UpdateApproach(B, Dt);
			}
		}
		else
		{
			B.Alert = std::max(0.f, B.Alert - Dt / (B.Wary > 0.f ? 45.f : 7.f));
			B.bNoticed = B.bWatching = false;
		}
		B.Wary = std::max(0.f, B.Wary - Dt);
		B.Hunger = Clamp(B.Hunger + Dt * (B.Act == EAct::Peck || B.Act == EAct::Eat || B.Act == EAct::Hawk ? -0.004f : 0.0012f), 0.f, 1.f);

		// voices: contact calls, the dawn song, all at their own times (only heard near the player)
		if (B.Tier <= 1 && B.Act != EAct::Sleep)
		{
			B.CallTimer -= Dt;
			if (B.CallTimer <= 0.f)
			{
				const float Wake = Sunrise + S.WakeMinutes / 60.f;
				const bool bDawn = Hour > Wake - 0.3f && Hour < Wake + 1.5f;
				const bool bDusk = Hour > Sunset - 0.5f && Hour < Sunset + 0.6f;
				const float Rate = std::max(0.05f, S.CallRate + (bDawn || (bDusk && S.DawnSong > 2.f) ? S.DawnSong : 0.f));
				B.CallTimer = 60.f / Rate * (0.4f + 1.2f * Rand(B.Rng));
				Emit(bDawn && S.DawnSong > 1.f && !IsAirborne(B.Act) ? EEvent::Song : EEvent::Call, B);
			}
		}

		// hawks and approachers have their own logic; arrivals are handled by Move
		switch (B.Act)
		{
		case EAct::Sleep:
			if (Groups[B.Group].Plan != 2)
			{
				Settle(B);             // morning: awake where it slept, until the group moves off
			}
			break;
		case EAct::Perch:
		case EAct::Preen:
			if (B.ActTime > B.ActLen)
			{
				const FGroup& G = Groups[B.Group];
				const bool bGround = B.Site >= 0 && IsGroundHabitat(Sites[B.Site].Kind);
				const float R = Rand(B.Rng);
				if (G.Plan == 1 && B.Site >= 0 && (bGround || R < 0.5f))
				{
					// feed: on the ground walk to a new spot, in a crown flit to another twig
					B.Spot = SpotFor(B, B.Site);
					if (bGround && Sites[B.Site].Kind == EHabitat::Sea)
					{
						SetAct(B, EAct::Swim, 3.f + Rand(B.Rng) * 6.f);     // paddles about
					}
					else if (bGround)
					{
						SetAct(B, EAct::Forage, 0.f);
					}
					else
					{
						FlyTo(B, B.Site, B.Spot);
					}
				}
				else if (IsGull(S) && G.Plan == 0 && R < 0.12f && B.Site >= 0)
				{
					B.Spot = Sites[B.Site].Center() + V3(0.f, 0.f, 25.f + Rand(B.Rng) * 25.f);
					B.FlyFrom = B.Spot;           // the centre of its circles
					SetAct(B, EAct::Soar, 30.f + Rand(B.Rng) * 60.f);
					B.Vel = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.4f) * 4.f;
				}
				else if (G.Plan == 2 && InRoost(G, B.Site) && (Hour > Sunset + S.RoostMinutes / 60.f - 0.25f || Hour < Sunrise))
				{
					SetAct(B, EAct::Sleep, 30.f);       // dark: tuck the head in
				}
				else
				{
					SetAct(B, R < 0.25f ? EAct::Preen : EAct::Perch, 2.f + Rand(B.Rng) * 8.f);
				}
			}
			// head: quick looks about
			if (!B.bNoticed && Rand(B.Rng) < Dt * 1.2f)
			{
				const float A = B.Yaw + (Rand(B.Rng) - 0.5f) * 3.f;
				B.Look = B.Pos + V3(std::cos(A), std::sin(A), (Rand(B.Rng) - 0.4f) * 0.8f) * 5.f;
				B.bLook = true;
			}
			break;
		case EAct::Peck:
			if (B.ActTime > B.ActLen)
			{
				// many eyes: in a big flock a bird looks up less often
				const int N = (int)Groups[B.Group].Members.size();
				const float Vigilance = 0.35f / std::sqrt((float)std::max(N, 1));
				if (Rand(B.Rng) < Vigilance)
				{
					SetAct(B, EAct::Perch, 0.6f + Rand(B.Rng) * 1.2f);
				}
				else if (B.Site >= 0)
				{
					// next spot nearby (a step or two), now and then a big find that takes a while
					const FSite& Site = Sites[B.Site];
					const float Step = std::max(0.3f, S.Length * 6.f) * (0.5f + Rand(B.Rng));
					const float A = B.Yaw + (Rand(B.Rng) - 0.5f) * 2.5f;
					V3 Next = B.Pos + V3(std::cos(A), std::sin(A), 0.f) * Step;
					if ((Next - Site.Center()).Len2D() > Site.Radius + (Site.B - Site.A).Len() * 0.5f + 1.f)
					{
						Next = SpotOn(Site, B.Rng);
					}
					Next.Z = Site.Kind == EHabitat::Sea ? SeaZ : B.Pos.Z;
					if (Probe && Site.Kind != EHabitat::Sea && B.Tier <= 1)
					{
						float Z;
						if (Probe(Next.X, Next.Y, Z))
						{
							Next.Z = Z;
						}
					}
					else if (Site.Kind != EHabitat::Sea)
					{
						Next.Z = B.Pos.Z + (GroundZ(Next.X, Next.Y) - GroundZ(B.Pos.X, B.Pos.Y));
					}
					B.Spot = Next;
					SetAct(B, Site.Kind == EHabitat::Sea ? EAct::Swim : EAct::Forage, 0.f);
				}
			}
			break;
		case EAct::Swim:
			if (B.ActTime > B.ActLen)
			{
				const bool bDiver = S.Id == "Cormorant";
				SetAct(B, Rand(B.Rng) < (bDiver ? 0.5f : 0.3f) ? EAct::Peck : EAct::Swim, bDiver ? 15.f + Rand(B.Rng) * 20.f : 2.f + Rand(B.Rng) * 6.f);
				B.Vel = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f) * 0.2f;
				B.Yaw += (Rand(B.Rng) - 0.5f) * 1.5f;
			}
			break;
		case EAct::Watch:
			if (B.ActTime > B.ActLen && B.Alert < 0.35f && !(Player.bOffer && B.bNoticed && B.ActTime < 6.f))
			{
				B.bWatching = false;
				Settle(B);
			}
			break;
		case EAct::Hawk:
			if (B.ActTime > B.ActLen)
			{
				// a new turn of its beat over the site
				const FGroup& G = Groups[B.Group];
				const V3 C = B.Site >= 0 ? Sites[B.Site].Center() : B.Pos;
				const float A = Rand(B.Rng) * 2.f * Pi, R = 15.f + Rand(B.Rng) * (S.Id == "CommonSwift" ? 90.f : 45.f);
				V3 P = C + V3(std::cos(A) * R, std::sin(A) * R, 0.f);
				P.Z = std::max(GroundZ(P.X, P.Y), SeaZ) + S.HawkAlt * (0.4f + 1.2f * Rand(B.Rng));
				P.Z = std::max(P.Z, TopZ(P.X, P.Y) + 3.f);
				B.Spot = P;
				SetAct(B, EAct::Hawk, 1.5f + Rand(B.Rng) * 3.f);
				if (G.Plan != 1)
				{
					// done feeding: to a wire or roof, or keep going (swifts hardly sit)
					const int Site = G.Site;
					if (Site >= 0 && S.Id != "CommonSwift")
					{
						FlyTo(B, Site, SpotFor(B, Site));
					}
				}
			}
			break;
		case EAct::Soar:
			if (B.ActTime > B.ActLen)
			{
				if (B.Site >= 0)
				{
					FlyTo(B, B.Site, SpotFor(B, B.Site));
				}
			}
			break;
		case EAct::Mob:
			if (B.ActTime > B.ActLen || B.Target < 0 || IsAirborne(Birds[B.Target].Act))
			{
				if (B.Site >= 0)
				{
					FlyTo(B, B.Site, SpotFor(B, B.Site));
				}
			}
			else if (Rand(B.Rng) < Dt * 1.5f)
			{
				Emit(EEvent::Call, B);
			}
			break;
		case EAct::Eat:
			if (!Player.bOffer)
			{
				// the hand went in: off to its flock
				Approacher = -1;
				B.bAte = false;
				const int Site = Groups[B.Group].Site;
				if (Site >= 0)
				{
					FlyTo(B, Site, SpotFor(B, Site));
				}
			}
			else if (!B.bAte && B.ActTime > 0.35f)
			{
				B.bAte = true;
				Emit(EEvent::Eat, B);
			}
			break;
		case EAct::Approach:
			if (!Player.bOffer)
			{
				Approacher = -1;
				const int Site = Groups[B.Group].Site;
				if (Site >= 0)
				{
					FlyTo(B, Site, SpotFor(B, Site));
				}
			}
			break;
		default:
			break;
		}

		// mobbing: a hawk perched near small birds that mob
		if (S.bMobs && !IsAirborne(B.Act) && B.Act != EAct::Sleep && B.Tier <= 2 && Rand(B.Rng) < Dt * 0.5f)
		{
			for (FPredator& P : Predators)
			{
				const FBird& H = Birds[P.Bird];
				if (!IsAirborne(H.Act) && H.Act != EAct::Sleep && (H.Pos - B.Pos).Len() < 60.f)
				{
					B.Target = P.Bird;
					B.FlyFrom = H.Pos;
					SetAct(B, EAct::Mob, 15.f + Rand(B.Rng) * 20.f);
					B.Vel = (H.Pos - B.Pos).Norm() * 3.f + V3(0.f, 0.f, 2.f);
					P.Mobbed += 1.f;
					Emit(EEvent::Call, B);
					break;
				}
			}
		}
	}

	// --- movement -------------------------------------------------------------------------------------

	void FWorld::Move(FBird& B, float Dt)
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		B.Phase += Dt * (IsAirborne(B.Act) ? WingbeatHz(S) : 2.f);
		switch (B.Act)
		{
		case EAct::Forage:
		{
			// walk or hop to the spot
			const V3 D = B.Spot - B.Pos;
			const float L = D.Len2D();
			const float V = S.Gait * (S.bHops ? (std::fmod(B.Phase, 1.f) < 0.5f ? 1.8f : 0.2f) : 1.f);
			if (L < V * Dt + 0.02f)
			{
				B.Pos = B.Spot;
				B.Vel = V3();
				SetAct(B, Sites.empty() || B.Site < 0 || Sites[B.Site].Kind != EHabitat::Sea ? EAct::Peck : EAct::Swim, 0.4f + Rand(B.Rng) * 1.6f);
				// sometimes it found something big (a crust, a shell): eats it a while, gulls notice
				if (Rand(B.Rng) < 0.06f)
				{
					B.ActLen = 3.f + Rand(B.Rng) * 4.f;
				}
			}
			else
			{
				const V3 Dir = D * (1.f / std::max(D.Len(), 1e-4f));
				B.Vel = Dir * V;
				B.Pos += B.Vel * Dt;
				B.Yaw = std::atan2(Dir.Y, Dir.X);
			}
			break;
		}
		case EAct::Swim:
			B.Pos += B.Vel * Dt;
			B.Pos.Z = SeaZ;
			if (B.Site >= 0 && (B.Pos - Sites[B.Site].Center()).Len2D() > Sites[B.Site].Radius + 5.f)
			{
				B.Yaw = std::atan2(Sites[B.Site].A.Y - B.Pos.Y, Sites[B.Site].A.X - B.Pos.X);
				B.Vel = V3(std::cos(B.Yaw), std::sin(B.Yaw), 0.f) * 0.3f;
			}
			break;
		case EAct::Peck:
			if (S.Id == "Cormorant" && B.Site >= 0 && Sites[B.Site].Kind == EHabitat::Sea)
			{
				B.Pos.Z = SeaZ - 1.5f;      // under water
				if (B.ActTime > B.ActLen)
				{
					B.Pos.Z = SeaZ;
				}
			}
			break;
		case EAct::Sleep:
		case EAct::Perch:
		case EAct::Preen:
		case EAct::Watch:
			B.Vel = V3();
			if (S.Id == "Cormorant" && B.Pos.Z < SeaZ - 0.5f)
			{
				B.Pos.Z = SeaZ;
			}
			break;
		case EAct::Eat:
			B.Pos = Player.Palm;
			B.Vel = V3();
			break;
		case EAct::Claimed:
			break;
		default:
		{
			// in the air: steer toward the goal over the obstacles, land on the spot
			V3 Goal = B.Spot;
			float Speed = S.Cruise;
			bool bLanding = true;
			if (B.Act == EAct::Approach)
			{
				Goal = Player.Palm;
				B.Spot = Goal;
				Speed = std::min(S.Cruise, 6.f);
			}
			else if (B.Act == EAct::Flee)
			{
				Speed = S.Cruise * 1.35f;
				bLanding = B.Site >= 0;
			}
			else if (B.Act == EAct::Hawk)
			{
				Speed = S.Cruise * (0.8f + 0.4f * std::sin(B.Phase * 0.07f));
				bLanding = false;
			}
			else if (B.Act == EAct::Soar)
			{
				const float A = (float)Time * 0.12f + B.Id;
				Goal = B.FlyFrom + V3(std::cos(A) * 45.f, std::sin(A) * 45.f, std::sin(A * 0.3f) * 8.f);
				Speed = S.Cruise * 0.8f;
				bLanding = false;
			}
			else if (B.Act == EAct::Chase && B.Target >= 0)
			{
				FBird& T = Birds[B.Target];
				Goal = T.Pos + V3(0.f, 0.f, 0.3f);
				Speed = S.Cruise * (S.bPredator ? 1.7f : 1.25f);
				bLanding = false;
				// the chased bird flees when the chaser gets close
				if ((T.Pos - B.Pos).Len() < 10.f && T.Act != EAct::Flee && T.Act != EAct::Claimed && T.Act != EAct::Murmur)
				{
					T.FlyFrom = B.Pos;
					T.Alert = 1.f;
					Flush(T, B.Pos, -3);
				}
				if ((B.ActTime > B.ActLen || T.bGone) && !S.bPredator)       // a hawk's hunt ends in PredatorThink
				{
					const int Site = Groups[B.Group].Site >= 0 ? Groups[B.Group].Site : B.Site;
					B.Target = -1;
					if (Site >= 0)
					{
						FlyTo(B, Site, SpotFor(B, Site));
					}
					else
					{
						SetAct(B, EAct::Fly, 0.f);
					}
					return;
				}
			}
			else if (B.Act == EAct::Mob && B.Target >= 0)
			{
				const FBird& H = Birds[B.Target];
				const float A = (float)Time * 1.3f + B.Id * 1.7f;
				Goal = H.Pos + V3(std::cos(A) * 5.f, std::sin(A) * 5.f, 2.f + std::sin(A * 2.1f) * 1.5f);
				Speed = S.Cruise * 0.6f;
				bLanding = false;
			}
			const V3 To = Goal - B.Pos;
			const float Dist = To.Len();
			const float Dist2D = To.Len2D();
			// height: cruise over everything on the way, glide down onto the spot (~25 deg)
			const V3 Ahead = B.Pos + B.Vel * 1.5f;
			const float Floor = std::max({ TopZ(B.Pos.X, B.Pos.Y), TopZ(Ahead.X, Ahead.Y), SeaZ });
			float WantZ = Goal.Z;
			if (bLanding || B.Act == EAct::Approach)
			{
				WantZ = std::min(std::max(Goal.Z, Floor + S.FlightClearance), Goal.Z + Dist2D * 0.47f);
				if (Dist2D > 12.f)
				{
					WantZ = std::max(WantZ, Floor + 2.5f);
				}
			}
			else if (B.Act != EAct::Hawk && B.Act != EAct::Chase && B.Act != EAct::Mob)
			{
				WantZ = std::max(Goal.Z, Floor + S.FlightClearance);
			}
			else
			{
				WantZ = std::max(Goal.Z, Floor + 1.5f);
			}
			if (bLanding && Dist < 6.f)
			{
				Speed = std::min(Speed, std::max(1.2f, Dist * 1.4f));
			}
			V3 Want = V3(To.X, To.Y, 0.f).Norm() * Speed;
			Want.Z = Clamp((WantZ - B.Pos.Z) * 1.2f, -5.f, 4.f);
			if (bLanding && Dist < 3.f)
			{
				Want = To * (Speed / std::max(Dist, 0.05f));
			}
			// keep away from the player when escaping (never through them)
			if (B.Act == EAct::Flee && Player.bPresent)
			{
				const V3 Off = B.Pos - Player.Feet - V3(0.f, 0.f, 1.f);
				const float D = Off.Len();
				if (D < 8.f)
				{
					Want += Off.Norm() * (S.Cruise * (8.f - D) / 8.f * 2.f);
				}
			}
			const float Accel = (IsSmall(S) ? 14.f : 8.f) * (B.Act == EAct::Flee || B.Act == EAct::Chase ? 1.6f : 1.f);
			V3 DV = Want - B.Vel;
			const float DL = DV.Len();
			if (DL > Accel * Dt)
			{
				DV = DV * (Accel * Dt / DL);
			}
			B.Vel += DV;
			B.Pos += B.Vel * Dt;
			// never under the ground or the water while flying
			const float G = std::max(GroundZ(B.Pos.X, B.Pos.Y), SeaZ) + 0.3f;
			if (B.Pos.Z < G && !(bLanding && Dist < 4.f))
			{
				B.Pos.Z = G;
				B.Vel.Z = std::max(B.Vel.Z, 0.5f);
			}
			if (B.Vel.Len2D() > 0.3f)
			{
				B.Yaw = std::atan2(B.Vel.Y, B.Vel.X);
			}
			if (B.Act == EAct::Takeoff && B.ActTime > B.ActLen)
			{
				SetAct(B, EAct::Fly, 0.f);
			}
			if (B.Act == EAct::Fly && bLanding && Dist < 4.f)
			{
				SetAct(B, EAct::Land, 0.f);
			}
			if (bLanding && Dist < std::max(0.08f, Speed * Dt * 1.2f))
			{
				if (B.Act == EAct::Approach)
				{
					B.Pos = Goal;
					B.Vel = V3();
					B.Yaw = std::atan2(Player.Eye.Y - Goal.Y, Player.Eye.X - Goal.X);
					SetAct(B, EAct::Eat, 0.f);
					B.Look = Goal + V3(0.f, 0.f, -0.05f);
					B.bLook = true;
				}
				else if (B.Act == EAct::Flee && B.Site < 0)
				{
					SetAct(B, EAct::Fly, 0.f);
				}
				else
				{
					B.Alert = std::min(B.Alert, 0.5f);
					Settle(B);
				}
			}
			// an escape to open air ends in a circle back once it is far enough
			if (B.Act == EAct::Flee && B.Site < 0 && Dist < 10.f)
			{
				const int Site = Groups[B.Group].Site >= 0 && Sites[Groups[B.Group].Site].Disturbed <= Time ? Groups[B.Group].Site : -1;
				FGroup& Grp = Groups[B.Group];
				int Next = Site >= 0 ? Site : ChooseSite(Grp, Grp.Plan == 2 ? 2 : Grp.Plan, Grp.Site);
				if (Next < 0 || Grp.bStayPut)
				{
					Next = Grp.Site;      // nowhere better: back where it was
				}
				if (Next >= 0 && Avoided(Grp, Sites[Next].Center()))
				{
					// keep circling out here until the danger has gone
					const V3 Danger = Grp.bAvoidPlayer && Player.bPresent ? Player.Feet : Grp.Avoid;
					const V3 Off = (B.Pos - Danger).Flat().Norm();
					B.Spot = Danger + Off * (Grp.AvoidR + 20.f) + V3(Off.Y, -Off.X, 0.f) * 25.f;   // round it, at a distance
					B.Spot.Z = std::max(TopZ(B.Spot.X, B.Spot.Y), SeaZ) + S.FlightClearance + 6.f;
					Next = -2;
				}
				if (Next >= 0)
				{
					FlyTo(B, Next, SpotFor(B, Next));
				}
			}
			break;
		}
		}
	}

	// --- the player -----------------------------------------------------------------------------------

	bool FWorld::Claim(int Id)
	{
		if (Id < 0 || Id >= (int)Birds.size() || Birds[Id].bGone)
		{
			return false;
		}
		if (Approacher == Id)
		{
			Approacher = -1;
		}
		SetAct(Birds[Id], EAct::Claimed, 0.f);
		return true;
	}

	void FWorld::SetClaimedPos(int Id, const V3& Pos, const V3& Vel)
	{
		if (Id >= 0 && Id < (int)Birds.size() && Birds[Id].Act == EAct::Claimed)
		{
			Birds[Id].Pos = Pos;
			Birds[Id].Vel = Vel;
			if (Vel.Len2D() > 0.1f)
			{
				Birds[Id].Yaw = std::atan2(Vel.Y, Vel.X);
			}
		}
	}

	void FWorld::Release(int Id)
	{
		if (Id < 0 || Id >= (int)Birds.size() || Birds[Id].Act != EAct::Claimed)
		{
			return;
		}
		FBird& B = Birds[Id];
		B.Alert = 0.f;
		const int Site = Groups[B.Group].Site;
		B.Vel = V3();
		SetAct(B, EAct::Perch, 1.f);
		if (Murm && Murm->Species() == B.Species && Groups[B.Group].Plan == 4)
		{
			// back into the evening's murmuration
			FlyTo(B, -1, Murm->Centre());
			B.Target = -6;
		}
		else if (Site >= 0)
		{
			FlyTo(B, Site, SpotFor(B, Site));
		}
	}

	// --- migration, the coast, places on wires and in the reed ---------------------------------------------

	void FWorld::UpdateMigration()
	{
		for (FBird& B : Birds)
		{
			const FSpecies& S = SpeciesTable()[B.Species];
			if (!Opt.bMigration || !S.bMigrant || S.Stragglers <= 0 || B.bGone || B.LeaveDay > 1e8f)
			{
				continue;
			}
			// the day has come: it goes south (out of sight: far from the camera, or by night out of the near ring;
			// never from under the player's nose, where it would simply vanish)
			if (DayOfYear >= B.LeaveDay && (B.Tier >= 2 || ((Hour < Sunrise || Hour > Sunset) && B.Tier >= 1)) && B.Act != EAct::Claimed
				&& B.Act != EAct::Approach && B.Act != EAct::Eat)
			{
				B.bGone = true;
			}
		}
	}

	/** Distance fields over the ground grid: from land over the sea, from the sea over land (two-pass chamfer). */
	void FWorld::BuildOffshore()
	{
		const int NX = Grid.NX, NY = Grid.NY;
		if (NX == 0)
		{
			return;
		}
		auto Field = [&](bool bFromLand, std::vector<uint16_t>& Out)
		{
			std::vector<float> D((size_t)NX * NY, 1e9f);
			for (int J = 0; J < NY; J++)
			{
				for (int I = 0; I < NX; I++)
				{
					const bool bLand = Grid.Ground[(size_t)J * NX + I] * 0.01f > MeanSeaZ + 0.3f;
					if (bLand == bFromLand)
					{
						D[(size_t)J * NX + I] = 0.f;
					}
				}
			}
			const float C = Grid.Cell, Dg = Grid.Cell * 1.41421f;
			for (int J = 0; J < NY; J++)
			{
				for (int I = 0; I < NX; I++)
				{
					float& V = D[(size_t)J * NX + I];
					if (I > 0) V = std::min(V, D[(size_t)J * NX + I - 1] + C);
					if (J > 0) V = std::min(V, D[(size_t)(J - 1) * NX + I] + C);
					if (I > 0 && J > 0) V = std::min(V, D[(size_t)(J - 1) * NX + I - 1] + Dg);
					if (I < NX - 1 && J > 0) V = std::min(V, D[(size_t)(J - 1) * NX + I + 1] + Dg);
				}
			}
			for (int J = NY - 1; J >= 0; J--)
			{
				for (int I = NX - 1; I >= 0; I--)
				{
					float& V = D[(size_t)J * NX + I];
					if (I < NX - 1) V = std::min(V, D[(size_t)J * NX + I + 1] + C);
					if (J < NY - 1) V = std::min(V, D[(size_t)(J + 1) * NX + I] + C);
					if (I < NX - 1 && J < NY - 1) V = std::min(V, D[(size_t)(J + 1) * NX + I + 1] + Dg);
					if (I > 0 && J < NY - 1) V = std::min(V, D[(size_t)(J + 1) * NX + I - 1] + Dg);
				}
			}
			Out.resize(D.size());
			for (size_t K = 0; K < D.size(); K++)
			{
				Out[K] = (uint16_t)std::min(D[K] * 10.f, 65000.f);
			}
		};
		Field(true, OffshoreD);
		Field(false, InlandD);
	}

	static float FieldAt(const std::vector<uint16_t>& F, int NX, int NY, float X0, float Y0, float Cell, float X, float Y, float Outside)
	{
		if (F.empty())
		{
			return Outside;
		}
		const int I = (int)std::floor((X - X0) / Cell + 0.5f), J = (int)std::floor((Y - Y0) / Cell + 0.5f);
		if (I < 0 || J < 0 || I >= NX || J >= NY)
		{
			return Outside;
		}
		return F[(size_t)J * NX + I] * 0.1f;
	}

	float FWorld::Offshore(float X, float Y) const
	{
		// beyond the grid lies the open sea to the west (the map's +X): far out
		return FieldAt(OffshoreD, Grid.NX, Grid.NY, Grid.X0, Grid.Y0, Grid.Cell, X, Y, 5000.f);
	}

	float FWorld::Inland(float X, float Y) const
	{
		return FieldAt(InlandD, Grid.NX, Grid.NY, Grid.X0, Grid.Y0, Grid.Cell, X, Y, 0.f);
	}

	/** Its place in its group's row along the wire: birds a body and a half apart (swallows ~13 cm), the row starting at the group's slot. */
	V3 FWorld::WireSpot(const FSite& Site, const FBird& B) const
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		const float Len = (Site.B - Site.A).Len();
		const float Gap = std::max(0.1f, S.Length * 0.7f);
		const int N = std::max(1, (int)(Len / Gap));
		int K = 0, Slot0 = 0;
		if (B.Group >= 0 && B.Group < (int)Groups.size())
		{
			const FGroup& G = Groups[B.Group];
			Slot0 = G.Slot0;
			for (int M = 0; M < (int)G.Members.size(); M++)
			{
				if (G.Members[M] == B.Id)
				{
					K = M;
					break;
				}
			}
		}
		// rows of other groups on the same wire start elsewhere (Slot0 differs); a few birds leave a gap
		const int Slot = (Slot0 + K + K / 5) % N;
		const float T = (Slot + 0.5f) / N;
		return Site.A + (Site.B - Site.A) * T;
	}

	V3 FWorld::SpotFor(FBird& B, int Site)
	{
		if (Site < 0 || Site >= (int)Sites.size())
		{
			return B.Pos;
		}
		const FSite& S = Sites[Site];
		if (S.Kind == EHabitat::Wire)
		{
			return WireSpot(S, B);
		}
		return SpotOn(S, B.Rng);
	}

	bool FWorld::InRoost(const FGroup& G, int Site) const
	{
		if (Site < 0)
		{
			return false;
		}
		if (Site == G.Roost)
		{
			return true;
		}
		return Murm && Murm->Species() == G.Species && Murm->IsRoostSite(Site);
	}

	int FWorld::RoostSpotSite(const FGroup& G, FBird& B)
	{
		if (Murm && Murm->Species() == G.Species)
		{
			return Murm->PickRoostSite(*this, B.Rng);
		}
		return G.Roost;
	}

	std::string FWorld::Report() const
	{
		int Acts[(int)EAct::Num] = {};
		int Habs[NumHabitats] = {};
		int Air = 0, Gone = 0;
		for (const FBird& B : Birds)
		{
			if (B.bGone)
			{
				Gone++;
				continue;
			}
			Acts[(int)B.Act]++;
			Air += IsAirborne(B.Act);
			if (!IsAirborne(B.Act) && B.Site >= 0)
			{
				Habs[(int)Sites[B.Site].Kind]++;
			}
		}
		char Buf[256];
		std::string Out;
		std::snprintf(Buf, sizeof(Buf), "birds %d (%d away) at %.2f h (sun %.2f-%.2f, sea %.2f), %d in the air, tiers %d/%d/%d/%d; acts:", (int)Birds.size(), Gone,
			Hour, Sunrise, Sunset, SeaZ, Air, TierCount[0], TierCount[1], TierCount[2], TierCount[3]);
		Out += Buf;
		for (int A = 0; A < (int)EAct::Num; A++)
		{
			if (Acts[A])
			{
				std::snprintf(Buf, sizeof(Buf), " %s %d", ActName((EAct)A), Acts[A]);
				Out += Buf;
			}
		}
		if (Murm && Murm->Count())
		{
			Out += "; " + Murm->Describe(*this);
		}
		Out += "; on:";
		for (int H = 0; H < NumHabitats; H++)
		{
			if (Habs[H])
			{
				std::snprintf(Buf, sizeof(Buf), " %s %d", HabitatName((EHabitat)H), Habs[H]);
				Out += Buf;
			}
		}
		return Out;
	}
}
