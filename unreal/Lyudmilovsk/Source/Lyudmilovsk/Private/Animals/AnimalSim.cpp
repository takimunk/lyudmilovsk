#include "AnimalSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>

namespace LyudAnimals
{
	namespace
	{
		constexpr float Pi = 3.14159265f;
		constexpr float HourS = 3600.f;

		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }
		float Clamp01(float V) { return Clamp(V, 0.f, 1.f); }
		float Lerp(float A, float B, float T) { return A + (B - A) * T; }
		float WrapPi(float A)
		{
			while (A > Pi) A -= 2.f * Pi;
			while (A < -Pi) A += 2.f * Pi;
			return A;
		}
		/** 1 at 0 falling to 0 at |X| = W (hours on a 24 h circle). */
		float Bump(float X, float W)
		{
			X = std::fmod(std::fabs(X), 24.f);
			X = std::min(X, 24.f - X);
			const float T = Clamp01(1.f - X / W);
			return T * T * (3.f - 2.f * T);
		}
		V3 Dir(float Yaw) { return V3(std::cos(Yaw), std::sin(Yaw), 0.f); }

		bool IsCat(const FAnimal& A) { return A.Kind == EKind::Cat; }
		bool IsDog(const FAnimal& A) { return A.Kind == EKind::Dog; }
		/** Busy with something it won't drop for a whim (it still flees). */
		bool Busy(const FAnimal& A)
		{
			return A.Act == EAct::Eat || A.Act == EAct::Flee || A.Act == EAct::Climb || A.Act == EAct::Perch || A.Act == EAct::Pounce
				|| A.Act == EAct::Chase || A.Act == EAct::Hiss || A.Act == EAct::Play || A.Act == EAct::PlayBow || A.Act == EAct::Follow;
		}
		bool Asleep(const FAnimal& A) { return A.Act == EAct::Sleep; }
		/** Going somewhere (sniffing without a route is sniffing on the spot). */
		bool Traveling(const FAnimal& A) { return IsMoving(A.Act) && (A.Act != EAct::Sniff || !A.Path.empty()); }
	}

	float V3::Len() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	float V3::Len2D() const { return std::sqrt(X * X + Y * Y); }
	V3 V3::Norm() const
	{
		const float L = Len();
		return L > 1e-6f ? V3(X / L, Y / L, Z / L) : V3(0.f, 0.f, 0.f);
	}

	const char* PlaceName(EPlace P)
	{
		static const char* N[] = { "Doorstep", "ShopDoor", "Bins", "WarmHatch", "Bench", "SunnyWall", "Den", "Square", "Beach",
			"ClubDoor", "Station", "Feeder", "Basement", "Lookout", "Refuge" };
		return (int)P < (int)EPlace::Num ? N[(int)P] : "?";
	}

	const char* ActName(EAct A)
	{
		static const char* N[] = { "Sleep", "Rest", "Sit", "Groom", "Stand", "Walk", "Trot", "Run", "Sniff", "Eat", "Watch", "Approach",
			"Greet", "Beg", "Rub", "PlayBow", "Play", "Stalk", "Crouch", "Pounce", "Chase", "Flee", "Climb", "Perch", "Hiss", "Bark", "Follow" };
		return (int)A < (int)EAct::Num ? N[(int)A] : "?";
	}

	const char* GoalName(EGoal G)
	{
		static const char* N[] = { "None", "Sleep", "Eat", "Bask", "Roam", "Visit", "Play", "Hunt", "Person", "Rest", "Refuge", "Follow" };
		return (int)G < (int)EGoal::Num ? N[(int)G] : "?";
	}

	const char* EventName(EEvent E)
	{
		static const char* N[] = { "Notice", "Watch", "Approach", "Flee", "Bark", "Growl", "Whine", "Meow", "Purr", "Hiss", "Pounce",
			"ChaseStart", "Climb", "Greet", "PlayStart" };
		return (int)E < (int)EEvent::Num ? N[(int)E] : "?";
	}

	float FWorld::Rand(uint32_t& S) const
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xFFFFFF) / float(0x1000000);
	}

	// --- the map ----------------------------------------------------------------------------------------

	bool FWorld::Load(const std::string& Dir)
	{
		std::ifstream G(Dir + "/animal_world.bin", std::ios::binary);
		if (!G)
		{
			LastError = "no " + Dir + "/animal_world.bin";
			return false;
		}
		char Magic[4];
		int32_t NX = 0, NY = 0;
		float H[3];
		G.read(Magic, 4);
		G.read((char*)&NX, 4);
		G.read((char*)&NY, 4);
		G.read((char*)H, 12);
		if (std::memcmp(Magic, "AWG1", 4) != 0 || NX <= 0 || NY <= 0 || (int64_t)NX * NY > 8000000)
		{
			LastError = "animal_world.bin: bad header";
			return false;
		}
		Grid.NX = NX;
		Grid.NY = NY;
		Grid.X0 = H[0];
		Grid.Y0 = H[1];
		Grid.Cell = H[2];
		Grid.Z.resize((size_t)NX * NY);
		Grid.Kind.resize((size_t)NX * NY);
		G.read((char*)Grid.Z.data(), Grid.Z.size() * 2);
		G.read((char*)Grid.Kind.data(), Grid.Kind.size());
		if (!G)
		{
			LastError = "animal_world.bin: short";
			Grid = FGrid();
			return false;
		}
		BuildCoarse();

		std::ifstream F(Dir + "/animal_places.json", std::ios::binary);
		if (!F)
		{
			LastError = "no " + Dir + "/animal_places.json";
			return false;
		}
		std::stringstream SS;
		SS << F.rdbuf();
		const std::string Text = SS.str();
		const size_t At = Text.find("\"places\"");
		if (At == std::string::npos)
		{
			LastError = "animal_places.json has no places";
			return false;
		}
		// [[kind, x, y, z, yaw, capacity, sun, warm, food, height, "tag"], ...]
		const char* P = Text.c_str() + Text.find('[', At) + 1;
		Places.clear();
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
			std::string Tag;
			if (*P == '"')
			{
				P++;
				while (*P && *P != '"')
				{
					Tag += *P++;
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
			FPlace S;
			S.Kind = (EPlace)Clamp(V[0], 0.f, (float)EPlace::Num - 1.f);
			S.Pos = { V[1], V[2], V[3] };
			S.Yaw = V[4];
			S.Capacity = (int)V[5];
			S.Sun = V[6];
			S.Warm = V[7];
			S.Food = V[8];
			S.Height = V[9];
			S.Tag = Tag;
			Places.push_back(S);
		}
		return !Places.empty();
	}

	void FWorld::AddRefuge(const V3& Base, float Height)
	{
		FPlace P;
		P.Kind = EPlace::Refuge;
		P.Pos = Base;
		P.Height = Height;
		P.Capacity = 2;
		P.Tag = "tree";
		Places.push_back(P);
	}

	void FWorld::BuildCoarse()
	{
		Grid.CNX = (Grid.NX + CoarseK - 1) / CoarseK;
		Grid.CNY = (Grid.NY + CoarseK - 1) / CoarseK;
		Grid.CoarseWalk.assign((size_t)Grid.CNX * Grid.CNY, 0);
		Grid.CoarseKind.assign((size_t)Grid.CNX * Grid.CNY, 0);
		for (int CJ = 0; CJ < Grid.CNY; CJ++)
		{
			for (int CI = 0; CI < Grid.CNX; CI++)
			{
				int N = 0;
				int Counts[8] = {};
				for (int J = CJ * CoarseK; J < std::min(Grid.NY, (CJ + 1) * CoarseK); J++)
				{
					for (int I = CI * CoarseK; I < std::min(Grid.NX, (CI + 1) * CoarseK); I++)
					{
						const uint8_t K = Grid.Kind[(size_t)J * Grid.NX + I] & 7;
						if (K != 0)
						{
							N++;
							Counts[K]++;
						}
					}
				}
				int Best = 0;
				for (int K = 1; K < 8; K++)
				{
					if (Counts[K] > Counts[Best])
					{
						Best = K;
					}
				}
				Grid.CoarseWalk[(size_t)CJ * Grid.CNX + CI] = (uint8_t)N;
				Grid.CoarseKind[(size_t)CJ * Grid.CNX + CI] = (uint8_t)Best;
			}
		}
	}

	EGround FWorld::GroundAt(float X, float Y) const
	{
		if (Grid.NX == 0)
		{
			return EGround::Grass;
		}
		const int I = (int)std::lround((X - Grid.X0) / Grid.Cell);
		const int J = (int)std::lround((Y - Grid.Y0) / Grid.Cell);
		if (I < 0 || J < 0 || I >= Grid.NX || J >= Grid.NY)
		{
			return EGround::Blocked;
		}
		return (EGround)(Grid.Kind[(size_t)J * Grid.NX + I] & 7);
	}

	bool FWorld::Walkable(float X, float Y) const
	{
		return GroundAt(X, Y) != EGround::Blocked;
	}

	float FWorld::GroundZ(float X, float Y) const
	{
		if (Grid.NX == 0)
		{
			return NoGridZ;
		}
		const float U = Clamp((X - Grid.X0) / Grid.Cell, 0.f, Grid.NX - 1.001f);
		const float V = Clamp((Y - Grid.Y0) / Grid.Cell, 0.f, Grid.NY - 1.001f);
		const int I = (int)U, J = (int)V;
		const float FU = U - I, FV = V - J;
		auto Z = [&](int A, int B) { return Grid.Z[(size_t)B * Grid.NX + A] * 0.01f; };
		return Lerp(Lerp(Z(I, J), Z(I + 1, J), FU), Lerp(Z(I, J + 1), Z(I + 1, J + 1), FU), FV);
	}

	float FWorld::ZAt(const FAnimal& A) const
	{
		float Z;
		if (A.Tier == 0 && Probe && Probe(A.Pos.X, A.Pos.Y, Z))
		{
			return Z;
		}
		return GroundZ(A.Pos.X, A.Pos.Y);
	}

	bool FWorld::LineWalkable(const V3& A, const V3& B) const
	{
		const V3 D = (B - A).Flat();
		const float L = D.Len2D();
		const int N = std::max(1, (int)(L / (Grid.Cell * 0.35f)));
		for (int K = 1; K <= N; K++)
		{
			const V3 P = A + D * (K / (float)N);
			if (!Walkable(P.X, P.Y))
			{
				return false;
			}
		}
		return true;
	}

	V3 FWorld::FreeSpot(const V3& Near, float R, uint32_t& Rng) const
	{
		for (int K = 0; K < 12; K++)
		{
			const float A = Rand(Rng) * 2.f * Pi, D = R * std::sqrt(Rand(Rng));
			V3 P = Near + V3(std::cos(A) * D, std::sin(A) * D, 0.f);
			if (Walkable(P.X, P.Y))
			{
				P.Z = GroundZ(P.X, P.Y);
				return P;
			}
		}
		V3 P = Near;
		P.Z = GroundZ(P.X, P.Y);
		return P;
	}

	/** A* on the coarse grid (CoarseK fine cells a side), then pulled straight where the fine grid allows. */
	bool FWorld::FindPath(const V3& From, const V3& To, EKind Kind, std::vector<V3>& Out) const
	{
		Out.clear();
		if (Grid.NX == 0 || LineWalkable(From, To))
		{
			Out.push_back(To);
			return true;
		}
		const float CC = Grid.Cell * CoarseK;
		const int W = Grid.CNX, H = Grid.CNY;
		auto Cell = [&](const V3& P, int& CI, int& CJ)
		{
			CI = (int)Clamp(std::floor((P.X - Grid.X0 + Grid.Cell * 0.5f) / CC), 0.f, W - 1.f);
			CJ = (int)Clamp(std::floor((P.Y - Grid.Y0 + Grid.Cell * 0.5f) / CC), 0.f, H - 1.f);
		};
		auto Open = [&](int I, int J) { return I >= 0 && J >= 0 && I < W && J < H && Grid.CoarseWalk[(size_t)J * W + I] >= 6; };
		int SI, SJ, GI, GJ;
		Cell(From, SI, SJ);
		Cell(To, GI, GJ);
		// nearest open cells to the ends
		auto Snap = [&](int& I, int& J)
		{
			if (Open(I, J))
			{
				return true;
			}
			for (int R = 1; R < 4; R++)
			{
				for (int DJ = -R; DJ <= R; DJ++)
				{
					for (int DI = -R; DI <= R; DI++)
					{
						if (Open(I + DI, J + DJ))
						{
							I += DI;
							J += DJ;
							return true;
						}
					}
				}
			}
			return false;
		};
		if (!Snap(SI, SJ) || !Snap(GI, GJ))
		{
			return false;
		}
		// cost of a step on each ground kind: cats keep to the yards and the bases of walls, dogs to streets and paths
		static const float CatCost[8] = { 9.f, 1.0f, 1.35f, 0.85f, 1.6f, 1.25f, 1.f, 1.f };
		static const float DogCost[8] = { 9.f, 1.0f, 0.85f, 1.1f, 1.0f, 1.3f, 1.f, 1.f };
		const float* Cost = Kind == EKind::Cat ? CatCost : DogCost;
		const size_t N = (size_t)W * H;
		std::vector<float> Gs(N, 1e30f);
		std::vector<int> From_(N, -1);
		std::vector<uint8_t> Closed(N, 0);
		using QE = std::pair<float, int>;
		std::priority_queue<QE, std::vector<QE>, std::greater<QE>> Q;
		const int Start = SJ * W + SI, Goal = GJ * W + GI;
		Gs[Start] = 0.f;
		Q.push({ 0.f, Start });
		int Expanded = 0;
		bool bFound = false;
		static const int DI[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static const int DJ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
		while (!Q.empty() && Expanded < 30000)
		{
			const int C = Q.top().second;
			Q.pop();
			if (Closed[C])
			{
				continue;
			}
			Closed[C] = 1;
			Expanded++;
			if (C == Goal)
			{
				bFound = true;
				break;
			}
			const int CI = C % W, CJ = C / W;
			for (int K = 0; K < 8; K++)
			{
				const int NI = CI + DI[K], NJ = CJ + DJ[K];
				if (!Open(NI, NJ) || (K >= 4 && (!Open(CI + DI[K], CJ) || !Open(CI, CJ + DJ[K]))))
				{
					continue;
				}
				const int NC = NJ * W + NI;
				if (Closed[NC])
				{
					continue;
				}
				const float Share = Grid.CoarseWalk[NC] / 16.f;
				const float Step = (K >= 4 ? 1.4142f : 1.f) * Cost[Grid.CoarseKind[NC] & 7] * (Share < 0.75f ? 1.6f : 1.f);
				const float G2 = Gs[C] + Step;
				if (G2 < Gs[NC])
				{
					Gs[NC] = G2;
					From_[NC] = C;
					const float Hx = (float)std::abs(NI - GI), Hy = (float)std::abs(NJ - GJ);
					Q.push({ G2 + 0.85f * (std::max(Hx, Hy) + 0.4142f * std::min(Hx, Hy)), NC });
				}
			}
		}
		if (!bFound)
		{
			return false;
		}
		std::vector<V3> Raw;
		for (int C = Goal; C != -1; C = From_[C])
		{
			const int CI = C % W, CJ = C / W;
			// the walkable fine cell nearest the coarse cell's centre
			V3 Best(Grid.X0 + (CI * CoarseK + 1.5f) * Grid.Cell, Grid.Y0 + (CJ * CoarseK + 1.5f) * Grid.Cell, 0.f);
			if (!Walkable(Best.X, Best.Y))
			{
				float BD = 1e9f;
				for (int J = CJ * CoarseK; J < std::min(Grid.NY, (CJ + 1) * CoarseK); J++)
				{
					for (int I = CI * CoarseK; I < std::min(Grid.NX, (CI + 1) * CoarseK); I++)
					{
						if ((Grid.Kind[(size_t)J * Grid.NX + I] & 7) == 0)
						{
							continue;
						}
						const V3 P(Grid.X0 + I * Grid.Cell, Grid.Y0 + J * Grid.Cell, 0.f);
						const float D = (P - Best).Len2D();
						if (D < BD)
						{
							BD = D;
							Best = P;
						}
					}
				}
			}
			Raw.push_back(Best);
		}
		std::reverse(Raw.begin(), Raw.end());
		Raw.back() = To;
		// pull it straight: from each kept point, the farthest point it sees
		V3 At = From;
		size_t K = 0;
		while (K < Raw.size())
		{
			size_t Far = K;
			for (size_t L = K; L < Raw.size() && L < K + 12; L++)
			{
				if (LineWalkable(At, Raw[L]))
				{
					Far = L;
				}
			}
			At = Raw[Far];
			At.Z = GroundZ(At.X, At.Y);
			Out.push_back(At);
			K = Far + 1;
		}
		Out.back() = To;
		return true;
	}

	// --- spatial hash -------------------------------------------------------------------------------

	void FWorld::BuildHash()
	{
		if (HashNX == 0)
		{
			float X0 = 1e9f, Y0 = 1e9f, X1 = -1e9f, Y1 = -1e9f;
			if (Grid.NX > 0)
			{
				X0 = Grid.X0;
				Y0 = Grid.Y0;
				X1 = Grid.X0 + Grid.NX * Grid.Cell;
				Y1 = Grid.Y0 + Grid.NY * Grid.Cell;
			}
			else
			{
				X0 = -500.f; Y0 = -500.f; X1 = 500.f; Y1 = 500.f;
			}
			HashX0 = X0 - 50.f;
			HashY0 = Y0 - 50.f;
			HashNX = (int)((X1 - X0 + 100.f) / HashCell) + 1;
			HashNY = (int)((Y1 - Y0 + 100.f) / HashCell) + 1;
		}
		CellHead.assign((size_t)HashNX * HashNY, -1);
		CellNext.assign(Animals.size(), -1);
		for (int I = 0; I < (int)Animals.size(); I++)
		{
			const V3& P = Animals[I].Pos;
			const int CX = (int)Clamp((P.X - HashX0) / HashCell, 0.f, HashNX - 1.f);
			const int CY = (int)Clamp((P.Y - HashY0) / HashCell, 0.f, HashNY - 1.f);
			const size_t C = (size_t)CY * HashNX + CX;
			CellNext[I] = CellHead[C];
			CellHead[C] = I;
		}
	}

	void FWorld::ForNear(const V3& P, float R, const std::function<void(int)>& Fn) const
	{
		if (CellHead.empty())
		{
			return;
		}
		const int X0 = (int)Clamp((P.X - R - HashX0) / HashCell, 0.f, HashNX - 1.f);
		const int X1 = (int)Clamp((P.X + R - HashX0) / HashCell, 0.f, HashNX - 1.f);
		const int Y0 = (int)Clamp((P.Y - R - HashY0) / HashCell, 0.f, HashNY - 1.f);
		const int Y1 = (int)Clamp((P.Y + R - HashY0) / HashCell, 0.f, HashNY - 1.f);
		const float R2 = R * R;
		for (int CY = Y0; CY <= Y1; CY++)
		{
			for (int CX = X0; CX <= X1; CX++)
			{
				for (int I = CellHead[(size_t)CY * HashNX + CX]; I >= 0; I = CellNext[I])
				{
					const V3 D = Animals[I].Pos - P;
					if (D.X * D.X + D.Y * D.Y <= R2)
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
		std::sort(Out.begin(), Out.end(), [&](int A, int B) { return (Animals[A].Pos - P).Len() < (Animals[B].Pos - P).Len(); });
	}

	float FWorld::Fam(int A, int B) const
	{
		const size_t N = Animals.size();
		return (A >= 0 && B >= 0 && (size_t)A < N && (size_t)B < N && Familiar.size() == N * N) ? Familiar[(size_t)A * N + B] : 0.f;
	}

	void FWorld::AddFam(int A, int B, float D)
	{
		const size_t N = Animals.size();
		if (A < 0 || B < 0 || (size_t)A >= N || (size_t)B >= N || Familiar.size() != N * N)
		{
			return;
		}
		const float V = Clamp01(Familiar[(size_t)A * N + B] + D);
		Familiar[(size_t)A * N + B] = V;
		Familiar[(size_t)B * N + A] = V;
	}

	// --- creating them ------------------------------------------------------------------------------

	namespace
	{
		/** Size (m, at the shoulder) and the walk/trot/run speeds of each body (m/s). */
		struct FBody { float Walk, Trot, Run, Sprint, Stalk; };
		FBody BodyOf(const FAnimal& A)
		{
			if (A.Kind == EKind::Cat)
			{
				return { 0.65f, 1.7f, 4.5f, 8.0f, 0.28f };
			}
			const float K = A.Breed == 2 ? 0.75f : (A.Breed == 1 ? 1.1f : 1.f);
			return { 1.15f * K, 2.4f * K, 5.5f * K, 8.5f * K, 0.4f };
		}
	}

	float FWorld::GaitSpeed(const FAnimal& A, EAct Gait) const
	{
		const FBody B = BodyOf(A);
		const float Young = A.bYoung ? 0.8f : 1.f;
		switch (Gait)
		{
		case EAct::Walk: return B.Walk * (0.85f + 0.3f * A.Lively) * Young;
		case EAct::Sniff: return B.Walk * 0.45f;
		case EAct::Approach: return B.Walk * 0.75f;
		case EAct::Stalk: return B.Stalk;      // the creep; from farther off it walks low (Move)
		case EAct::Trot: return B.Trot * Young;
		case EAct::Run: return B.Run * Young;
		case EAct::Chase: return B.Sprint * 0.85f * Young;
		case EAct::Flee: return B.Sprint * Young;
		case EAct::Play: return B.Run * 0.8f * Young;
		case EAct::Follow: return B.Walk;
		default: return 0.f;
		}
	}

	int FWorld::AddAnimal(EKind Kind, int Breed, int Coat, const V3& P, bool bYoung)
	{
		FAnimal A;
		A.Id = (int)Animals.size();
		A.Kind = Kind;
		A.Breed = (uint8_t)Breed;
		A.Coat = (uint8_t)Coat;
		A.bYoung = bYoung;
		A.Rng = Opt.Seed * 2654435761u + (uint32_t)A.Id * 40503u + 7u;
		A.bFemale = Rand(A.Rng) < 0.5f;
		A.Size = (bYoung ? 0.6f : 1.f) * (0.92f + 0.16f * Rand(A.Rng));
		A.Pos = P;
		A.HomePos = P;
		A.Yaw = Rand(A.Rng) * 2.f * Pi;
		A.Range = Kind == EKind::Cat ? 60.f : 300.f;
		A.Bold = 0.5f;
		A.Friendly = 0.6f;
		A.Social = Kind == EKind::Dog ? 0.7f : 0.3f;
		A.Hunter = Kind == EKind::Cat ? 0.8f : 0.4f;
		A.Playfulness = bYoung ? 0.9f : 0.4f;
		A.Lively = 0.5f;
		A.Hunger = 0.3f;
		A.Tired = 0.2f;
		A.Act = EAct::Stand;
		A.ActLen = 3.f;
		Animals.push_back(A);
		const size_t N = Animals.size();
		std::vector<float> F(N * N, 0.f);
		for (size_t I = 0; I + 1 < N; I++)
		{
			for (size_t J = 0; J + 1 < N; J++)
			{
				F[I * N + J] = Familiar.size() == (N - 1) * (N - 1) ? Familiar[I * (N - 1) + J] : 0.f;
			}
		}
		Familiar.swap(F);
		return A.Id;
	}

	void FWorld::Populate()
	{
		Animals.clear();
		Packs.clear();
		uint32_t R = Opt.Seed;
		auto Pick = [&](const std::vector<int>& V) { return V.empty() ? -1 : V[(size_t)(Rand(R) * V.size()) % V.size()]; };
		auto Dist = [&](int A, int B) { return (Places[A].Pos - Places[B].Pos).Len2D(); };

		// --- cats: by the houses, a third of them round the Academgorodok
		std::vector<int> Academ, Town;
		for (int I = 0; I < (int)Places.size(); I++)
		{
			const FPlace& P = Places[I];
			if (P.Kind != EPlace::Doorstep && P.Kind != EPlace::Basement && P.Kind != EPlace::Feeder && P.Kind != EPlace::ShopDoor)
			{
				continue;
			}
			const bool bAcadem = P.Tag == "academ" || P.Tag == "gastronom";
			const int Weight = P.Kind == EPlace::Doorstep ? 1 : (P.Kind == EPlace::Basement ? 4 : 6);
			for (int K = 0; K < Weight; K++)
			{
				(bAcadem ? Academ : Town).push_back(I);
			}
		}
		const int NCats = std::max(0, (int)std::lround(Opt.Cats * Opt.PopulationScale));
		std::vector<int> CatHomes;
		// coats: 0 tabby, 1 ginger, 2 black, 3 black-and-white, 4 grey, 5 tortoiseshell (females), 6 white-and-tabby
		static const float CoatW[7] = { 0.26f, 0.13f, 0.14f, 0.15f, 0.09f, 0.11f, 0.12f };
		auto PickCoat = [&](bool bFemale)
		{
			for (int Try = 0; Try < 8; Try++)
			{
				float X = Rand(R), Acc = 0.f;
				for (int C = 0; C < 7; C++)
				{
					Acc += CoatW[C];
					if (X <= Acc)
					{
						if (C == 5 && !bFemale) break;            // tortoiseshells are females
						if (C == 1 && bFemale && Rand(R) < 0.6f) break;   // most gingers are toms
						return C;
					}
				}
			}
			return 0;
		};
		for (int C = 0; C < NCats; C++)
		{
			const bool bAcadem = C < NCats * 0.36f;
			int Home = -1;
			for (int Try = 0; Try < 30; Try++)
			{
				const int H = Pick(bAcadem ? Academ : Town);
				if (H < 0)
				{
					break;
				}
				bool bFree = true;
				int Crowd = 0;
				for (int O : CatHomes)
				{
					const float D = Dist(O, H);
					if (D < 12.f)
					{
						Crowd++;
					}
					if (D < 6.f && Places[H].Kind == EPlace::Doorstep)
					{
						bFree = false;
					}
				}
				if (bFree && Crowd < (Places[H].Kind == EPlace::Doorstep ? 2 : 4))
				{
					Home = H;
					break;
				}
			}
			if (Home < 0)
			{
				continue;
			}
			const int Id = AddAnimal(EKind::Cat, 0, 0, Places[Home].Pos);
			FAnimal& A = Animals[Id];
			A.Coat = (uint8_t)PickCoat(A.bFemale);
			A.Home = Home;
			A.Range = (A.bFemale ? 45.f : 90.f) * (0.7f + 0.7f * Rand(A.Rng));
			A.Bold = Clamp01(0.25f + 0.6f * Rand(A.Rng) + (A.bFemale ? 0.f : 0.1f));
			A.Friendly = Clamp01(0.15f + 0.8f * Rand(A.Rng) + (Places[Home].Kind == EPlace::Feeder ? 0.1f : 0.f));
			A.Social = Clamp01(0.1f + 0.5f * Rand(A.Rng));
			A.Hunter = Clamp01(0.5f + 0.5f * Rand(A.Rng));
			A.Playfulness = Clamp01(0.1f + 0.4f * Rand(A.Rng));
			A.Lively = Clamp01(0.3f + 0.6f * Rand(A.Rng));
			CatHomes.push_back(Home);
		}
		// two litters: a mother with two half-grown kittens each
		int Litters = 0;
		for (int I = 0; I < (int)Animals.size() && Litters < 2; I++)
		{
			FAnimal& M = Animals[I];
			if (!IsCat(M) || !M.bFemale || M.bYoung || (Litters == 0) != (Places[M.Home].Tag == "academ" || Places[M.Home].Tag == "gastronom"))
			{
				continue;
			}
			for (int K = 0; K < 2; K++)
			{
				const int Id = AddAnimal(EKind::Cat, 0, 0, Animals[I].HomePos + V3(0.4f * K, 0.3f, 0.f), true);
				FAnimal& Kit = Animals[Id];
				const FAnimal& Mom = Animals[I];
				Kit.Coat = (uint8_t)(Rand(R) < 0.5f ? Mom.Coat : PickCoat(Kit.bFemale));
				Kit.Home = Mom.Home;
				Kit.Range = 25.f;
				Kit.Bold = 0.4f + 0.3f * Rand(Kit.Rng);
				Kit.Friendly = 0.5f + 0.4f * Rand(Kit.Rng);
				Kit.Social = 0.8f;
				Kit.Hunter = 0.6f;
				Kit.Playfulness = 0.95f;
				Kit.Lively = 0.9f;
				Kit.Pack = -2 - I;                 // follows its mother (Pack <= -2: the mother's id is -2 - Pack)
				AddFam(Id, I, 1.f);
			}
			AddFam((int)Animals.size() - 1, (int)Animals.size() - 2, 1.f);
			Litters++;
		}

		// --- dogs: packs with a den each, loners by a house, the club's dog on the spur
		auto NearestPlace = [&](const V3& P, EPlace Kind, const char* Tag)
		{
			int Best = -1;
			float BD = 1e9f;
			for (int I = 0; I < (int)Places.size(); I++)
			{
				if (Places[I].Kind != Kind || (Tag && Places[I].Tag != Tag))
				{
					continue;
				}
				const float D = (Places[I].Pos - P).Len2D();
				if (D < BD)
				{
					BD = D;
					Best = I;
				}
			}
			return Best;
		};
		V3 AcademC, TownC, BeachC, NorthC;
		{
			int NA = 0, NT = 0, NB = 0;
			float MaxY = -1e9f;
			for (const FPlace& P : Places)
			{
				if (P.Tag == "academ" && P.Kind == EPlace::Doorstep) { AcademC += P.Pos; NA++; }
				if (P.Kind == EPlace::Square && P.Tag == "town") { TownC += P.Pos; NT++; }
				if (P.Kind == EPlace::Beach) { BeachC += P.Pos; NB++; }
				if (P.Kind == EPlace::Doorstep && P.Pos.Y > MaxY) { MaxY = P.Pos.Y; NorthC = P.Pos; }
			}
			AcademC = NA ? AcademC * (1.f / NA) : V3();
			TownC = NT ? TownC * (1.f / NT) : V3();
			BeachC = NB ? BeachC * (1.f / NB) : V3();
		}
		struct FPackSpec { V3 Anchor; int Count; int Breeds[6]; float Range; };
		const float DogScale = Opt.PopulationScale;
		const FPackSpec Specs[] = {
			{ AcademC, 4, { 0, 1, 0, 2, 0, 0 }, 300.f },     // the Gastronom's yard dogs
			{ TownC, 5, { 1, 0, 0, 2, 0, 0 }, 450.f },       // the town square pack (with a pup)
			{ BeachC, 3, { 0, 0, 1, 0, 0, 0 }, 500.f },      // the beach and the promenade
			{ NorthC, 3, { 0, 2, 0, 0, 0, 0 }, 400.f },      // the town's north edge
		};
		const int NDogs = std::max(0, (int)std::lround(Opt.Dogs * DogScale));
		int Made = 0;
		for (const FPackSpec& Sp : Specs)
		{
			if (Grid.NX > 0 && Sp.Anchor.Len2D() < 1.f)
			{
				continue;
			}
			const int Den = NearestPlace(Sp.Anchor, EPlace::Den, nullptr);
			if (Den < 0 || Made + Sp.Count > NDogs)
			{
				continue;
			}
			FPack P;
			P.Den = Den;
			P.Home = Places[Den].Pos;
			P.Range = Sp.Range;
			const int PackId = (int)Packs.size();
			for (int K = 0; K < Sp.Count; K++)
			{
				const bool bPup = PackId == 1 && K == Sp.Count - 1;
				const int Id = AddAnimal(EKind::Dog, Sp.Breeds[K], 0, FreeSpot(P.Home, 4.f, R), bPup);
				FAnimal& A = Animals[Id];
				A.Coat = (uint8_t)(Rand(R) < 0.5f ? 0 : 1);
				A.Home = Den;
				A.HomePos = P.Home;
				A.Pack = PackId;
				A.Range = Sp.Range;
				A.Bold = Clamp01(0.3f + 0.6f * Rand(A.Rng) + (K == 0 ? 0.2f : 0.f));
				A.Friendly = Clamp01(0.3f + 0.6f * Rand(A.Rng));
				A.Social = Clamp01(0.6f + 0.4f * Rand(A.Rng));
				A.Hunter = Clamp01(0.15f + 0.6f * Rand(A.Rng));
				A.Playfulness = bPup ? 0.95f : Clamp01(0.2f + 0.5f * Rand(A.Rng));
				A.Lively = Clamp01(0.4f + 0.5f * Rand(A.Rng));
				P.Members.push_back(Id);
				Made++;
			}
			P.Leader = P.Members.empty() ? -1 : P.Members[0];
			if (P.Leader >= 0)
			{
				Animals[P.Leader].bLeader = true;
			}
			for (int A : P.Members)
			{
				for (int B : P.Members)
				{
					if (A != B)
					{
						AddFam(A, B, 1.f);
					}
				}
			}
			Packs.push_back(P);
		}
		// loners: yard dogs by a house in the town, the dog of the lower station, the club's dog on the spur
		struct FLone { EPlace Kind; const char* Tag; int Breed; float Friendly; float Range; };
		const FLone Lone[] = {
			{ EPlace::ClubDoor, "club", 0, 0.92f, 90.f },          // the friendliest: the one to make friends with, later
			{ EPlace::Station, "lower", 2, 0.7f, 180.f },
			{ EPlace::Doorstep, "house", 1, 0.35f, 150.f },
			{ EPlace::Doorstep, "house", 0, 0.6f, 250.f },
		};
		for (const FLone& L : Lone)
		{
			if (Made >= NDogs)
			{
				break;
			}
			int Home = -1;
			if (L.Kind == EPlace::Doorstep)
			{
				std::vector<int> C;
				for (int I = 0; I < (int)Places.size(); I++)
				{
					if (Places[I].Kind == EPlace::Doorstep && Places[I].Tag == L.Tag && (Places[I].Pos - TownC).Len2D() < 380.f)
					{
						C.push_back(I);
					}
				}
				Home = Pick(C);
			}
			else
			{
				Home = NearestPlace(V3(), L.Kind, L.Tag);
			}
			if (Home < 0)
			{
				continue;
			}
			const int Id = AddAnimal(EKind::Dog, L.Breed, 0, FreeSpot(Places[Home].Pos, 2.f, R));
			FAnimal& A = Animals[Id];
			A.Coat = (uint8_t)(Rand(R) < 0.5f ? 0 : 1);
			A.Home = Home;
			A.HomePos = Places[Home].Pos;
			A.Range = L.Range;
			A.Friendly = L.Friendly;
			A.Bold = Clamp01(0.4f + 0.4f * Rand(A.Rng));
			A.Social = Clamp01(0.3f + 0.5f * Rand(A.Rng));
			A.Hunter = Clamp01(0.2f + 0.5f * Rand(A.Rng));
			A.Playfulness = Clamp01(0.3f + 0.5f * Rand(A.Rng));
			A.Lively = Clamp01(0.3f + 0.5f * Rand(A.Rng));
			Made++;
		}

		// for testing (temporary): the club's dog and a friendly cat stay by the club's door
		if (Opt.bClubPair)
		{
			const int Door = NearestPlace(V3(), EPlace::ClubDoor, "club");
			if (Door >= 0)
			{
				for (FAnimal& A : Animals)
				{
					if (A.Kind == EKind::Dog && A.Home == Door)
					{
						A.Range = 15.f;
						A.Friendly = 0.95f;
					}
				}
				const int Id = AddAnimal(EKind::Cat, 0, 0, FreeSpot(Places[Door].Pos, 3.f, R));
				FAnimal& C = Animals[Id];
				C.Coat = 1;                     // a ginger tom, easy to see
				C.bFemale = false;
				C.Home = Door;
				C.HomePos = Places[Door].Pos;
				C.Range = 12.f;
				C.Bold = 0.7f;
				C.Friendly = 0.9f;
				C.Social = 0.4f;
				C.Hunter = 0.35f;              // the door's pigeons are for the player's feed: it stalks them only now and then
				C.Playfulness = 0.4f;
				C.Lively = 0.7f;
			}
		}

		// neighbours know each other: cats whose homes are close, cats and dogs of the same yard
		for (size_t I = 0; I < Animals.size(); I++)
		{
			for (size_t J = I + 1; J < Animals.size(); J++)
			{
				const FAnimal& A = Animals[I];
				const FAnimal& B = Animals[J];
				const float D = (A.HomePos - B.HomePos).Len2D();
				float F = Fam((int)I, (int)J);
				if (IsCat(A) && IsCat(B))
				{
					F = std::max(F, Clamp01(0.8f - D / 70.f) * (0.6f + 0.4f * Rand(R)));
				}
				else if (IsCat(A) != IsCat(B))
				{
					F = std::max(F, Clamp01(0.85f - D / 300.f) * (0.5f + 0.5f * Rand(R)));
				}
				else
				{
					F = std::max(F, Clamp01(0.4f - D / 600.f));
				}
				AddFam((int)I, (int)J, F - Fam((int)I, (int)J));
			}
		}
		for (FAnimal& A : Animals)
		{
			A.Hunger = 0.15f + 0.5f * Rand(A.Rng);
			A.Tired = 0.1f + 0.5f * Rand(A.Rng);
			A.Curious = 0.2f + 0.6f * Rand(A.Rng);
			A.Playful = 0.1f + 0.5f * Rand(A.Rng);
			A.Lonely = 0.1f + 0.3f * Rand(A.Rng);
			A.ThinkTimer = Rand(A.Rng) * 2.f;
			PlaceAtStart(A);
		}
		LastHour = Hour;
	}

	void FWorld::PlaceAtStart(FAnimal& A)
	{
		A.Path.clear();
		A.Speed = 0.f;
		A.Up = 0.f;
		A.Refuge = -1;
		A.Target = -1;
		A.TargetBird = -1;
		const float Act = Activity(A);
		if (Rand(A.Rng) > Act)
		{
			// asleep or resting where it sleeps
			const int P = PickPlace(A, EGoal::Sleep);
			if (P >= 0)
			{
				A.Pos = FreeSpot(Places[P].Pos, 1.5f, A.Rng);
				A.GoalPlace = P;
			}
			A.Goal = EGoal::Sleep;
			A.Tired = std::max(A.Tired, 0.5f);
			SetAct(A, Rand(A.Rng) < 0.7f ? EAct::Sleep : EAct::Rest, 600.f + Rand(A.Rng) * 1800.f);
		}
		else
		{
			const V3 C = A.Pack >= 0 && A.Pack < (int)Packs.size() ? Packs[A.Pack].Home : A.HomePos;
			A.Pos = FreeSpot(C, A.Range * 0.4f, A.Rng);
			A.Goal = EGoal::None;
			SetAct(A, EAct::Stand, 1.f + Rand(A.Rng) * 4.f);
		}
		if (A.Pack <= -2)
		{
			const int Mom = -2 - A.Pack;
			if (Mom >= 0 && Mom < (int)Animals.size() && Mom != A.Id)
			{
				A.Pos = FreeSpot(Animals[Mom].Pos, 1.5f, A.Rng);
				SetAct(A, Animals[Mom].Act == EAct::Sleep ? EAct::Sleep : EAct::Stand, Animals[Mom].ActLen);
			}
		}
		A.Pos.Z = GroundZ(A.Pos.X, A.Pos.Y);
	}

	void FWorld::Resettle()
	{
		for (FAnimal& A : Animals)
		{
			if (A.Act != EAct::Follow)
			{
				PlaceAtStart(A);
			}
		}
	}

	// --- the day ------------------------------------------------------------------------------------

	/** 0..1 how much it is up and about at this hour: cats at dawn, dusk and by night; dogs mornings and evenings. */
	float FWorld::Activity(const FAnimal& A) const
	{
		const float H = Hour;
		float V;
		if (IsCat(A))
		{
			V = 0.22f + 0.55f * Bump(H - Sunrise - 0.3f, 2.f) + 0.6f * Bump(H - Sunset - 0.8f, 2.6f) + 0.18f * Bump(H - 1.f, 3.5f)
				- 0.1f * Bump(H - 13.5f, 2.f);
		}
		else
		{
			V = 0.12f + 0.62f * Bump(H - Sunrise - 1.8f, 3.2f) + 0.4f * Bump(H - 13.f, 3.f) + 0.66f * Bump(H - Sunset - 1.5f, 3.f)
				- 0.05f * Bump(H - 14.f, 1.2f);
		}
		if (A.bYoung)
		{
			V += 0.1f;
		}
		return Clamp01(V * (0.75f + 0.5f * A.Lively));
	}

	void FWorld::UpdatePlaces(float Dt)
	{
		// food: the feeders' bowls at morning and evening, the shop's scraps three times a day, bins now and then
		const float Was = LastHour < 0.f ? Hour : LastHour;
		LastHour = Hour;
		auto Passed = [&](float At)
		{
			if (Was <= Hour)
			{
				return Was < At && Hour >= At;
			}
			return Was < At || Hour >= At;            // over midnight
		};
		for (FPlace& P : Places)
		{
			switch (P.Kind)
			{
			case EPlace::Feeder:
				if (Passed(8.f) || Passed(18.5f))
				{
					P.FoodLeft = (float)P.Capacity;
				}
				break;
			case EPlace::ShopDoor:
				if (Passed(9.5f) || Passed(14.f) || Passed(20.f))
				{
					P.FoodLeft = (float)P.Capacity;
				}
				break;
			case EPlace::Bins:
				P.FoodLeft = std::min((float)P.Capacity, P.FoodLeft + Dt * P.Food / (3.f * HourS));
				break;
			case EPlace::Beach:
				P.FoodLeft = std::min(1.f, P.FoodLeft + Dt * P.Food / 7200.f);
				break;
			default:
				break;
			}
		}
	}

	void FWorld::UpdateNeeds(FAnimal& A, float Dt)
	{
		const bool bCat = IsCat(A);
		const bool bSleep = Asleep(A);
		const bool bLie = A.Act == EAct::Rest || A.Act == EAct::Perch || A.Act == EAct::Sit;
		A.Hunger = Clamp01(A.Hunger + Dt / ((bCat ? 9.f : 11.f) * HourS) * (bSleep ? 0.5f : 1.f));
		if (bSleep)
		{
			A.Tired = Clamp01(A.Tired - Dt / ((bCat ? 1.3f : 2.6f) * HourS));
		}
		else if (bLie)
		{
			A.Tired = Clamp01(A.Tired - Dt / (6.f * HourS));
		}
		else
		{
			const float Effort = 1.f + std::min(A.Speed / 2.5f, 2.f);
			A.Tired = Clamp01(A.Tired + Dt * Effort / ((bCat ? 1.6f : 4.f) * HourS) * (1.3f - 0.5f * Activity(A)));
		}
		const float PlayT = A.bYoung ? 0.35f : (bCat ? 5.f : 1.6f) * (1.6f - A.Playfulness);
		A.Playful = Clamp01(A.Playful + (A.Act == EAct::Play ? -Dt / 90.f : (bSleep ? 0.f : Dt / (PlayT * HourS))));
		// company: dogs want their pack near, cats hardly anybody
		bool bCompany = false;
		if (A.Pack >= 0 && A.Pack < (int)Packs.size())
		{
			for (int M : Packs[A.Pack].Members)
			{
				if (M != A.Id && (Animals[M].Pos - A.Pos).Len2D() < 20.f)
				{
					bCompany = true;
					break;
				}
			}
		}
		else if (A.Pack <= -2)
		{
			const int Mom = -2 - A.Pack;
			bCompany = Mom < (int)Animals.size() && (Animals[Mom].Pos - A.Pos).Len2D() < 12.f;
		}
		A.Lonely = Clamp01(A.Lonely + (bCompany ? -Dt / 240.f : Dt / ((bCat ? 5.f : 0.8f) * HourS) * (0.4f + A.Social)));
		const bool bExploring = A.Act == EAct::Sniff || (A.Goal == EGoal::Roam && IsMoving(A.Act)) || A.Goal == EGoal::Person;
		A.Curious = Clamp01(A.Curious + (bExploring ? -Dt / 400.f : (bSleep ? 0.f : Dt / ((bCat ? 1.6f : 0.7f) * HourS) * (0.5f + A.Lively))));
		A.Fear = Clamp01(A.Fear - Dt / (A.Fear > 0.5f ? 12.f : 25.f));
		A.Alert = Clamp01(A.Alert - Dt / 8.f);
		A.Excite = Clamp01(A.Excite - Dt / 7.f);
		A.Cooldown = std::max(0.f, A.Cooldown - Dt);
		A.BarkTimer = std::max(0.f, A.BarkTimer - Dt);
	}

	// --- choosing what to do ------------------------------------------------------------------------

	float FWorld::PlaceScore(const FAnimal& A, const FPlace& P, EGoal Goal) const
	{
		const float D = (P.Pos - A.Pos).Len2D();
		const float FromHome = (P.Pos - A.HomePos).Len2D();
		if (FromHome > A.Range * 1.3f + 20.f)
		{
			return -1.f;
		}
		const bool bCat = IsCat(A);
		const bool bDay = Hour > Sunrise + 0.5f && Hour < Sunset - 0.5f;
		const bool bNight = Hour < Sunrise - 0.5f || Hour > Sunset + 1.f;
		float S = -1.f;
		switch (Goal)
		{
		case EGoal::Sleep:
			if (bCat)
			{
				if (P.Kind == EPlace::Basement) S = 1.2f + P.Warm;
				else if (P.Kind == EPlace::WarmHatch) S = 0.9f + P.Warm * 0.6f;
				else if (P.Kind == EPlace::Doorstep) S = 0.6f + (bDay ? P.Sun * 0.6f : 0.f);
				else if (P.Kind == EPlace::SunnyWall && bDay) S = 1.f + P.Sun;
				else if (P.Kind == EPlace::Bench && bDay) S = 0.4f;
			}
			else
			{
				if (P.Kind == EPlace::Den) S = 1.1f + (bNight ? 0.4f : 0.f);
				else if (P.Kind == EPlace::WarmHatch) S = 0.8f + P.Warm * (bNight ? 0.8f : 0.2f);
				else if (P.Kind == EPlace::Station || P.Kind == EPlace::ClubDoor) S = 0.8f;
				else if (P.Kind == EPlace::SunnyWall && bDay) S = 0.6f + 0.4f * P.Sun;
			}
			break;
		case EGoal::Eat:
			if (P.Kind == EPlace::Feeder || P.Kind == EPlace::ShopDoor || P.Kind == EPlace::Bins || P.Kind == EPlace::Beach)
			{
				const float Food = P.FoodLeft;
				// cats know when the bowls are filled and come to wait a while before
				const bool bSoon = P.Kind == EPlace::Feeder && (Bump(Hour - 8.f, 0.4f) > 0.f || Bump(Hour - 18.5f, 0.4f) > 0.f);
				S = (Food > 0.2f ? 1.2f : (bSoon ? 0.7f : 0.05f)) * (bCat ? (P.Kind == EPlace::Feeder ? 1.3f : (P.Kind == EPlace::ShopDoor ? 1.f : 0.5f))
					: (P.Kind == EPlace::Feeder ? 0.5f : 1.1f));
			}
			break;
		case EGoal::Bask:
			if (bDay && (P.Kind == EPlace::SunnyWall || P.Kind == EPlace::Bench || P.Kind == EPlace::Doorstep || P.Kind == EPlace::Square || P.Kind == EPlace::Lookout))
			{
				S = P.Sun * (P.Kind == EPlace::SunnyWall ? 1.2f : 0.8f);
			}
			break;
		case EGoal::Roam:
			S = 0.2f + (P.Food > 0.f ? 0.3f : 0.f) + (bCat ? (P.Kind == EPlace::Doorstep || P.Kind == EPlace::Basement ? 0.2f : 0.f)
				: (P.Kind == EPlace::Square || P.Kind == EPlace::Beach || P.Kind == EPlace::Station ? 0.4f : 0.f));
			if (P.Kind == EPlace::Refuge)
			{
				S = -1.f;
			}
			break;
		case EGoal::Hunt:
			if (P.Kind == EPlace::Square || P.Kind == EPlace::Bench || P.Kind == EPlace::Bins || P.Kind == EPlace::SunnyWall || P.Kind == EPlace::Beach)
			{
				S = 0.8f;
			}
			break;
		case EGoal::Rest:
			if (P.Kind != EPlace::Refuge && P.Kind != EPlace::Bins)
			{
				S = 0.3f + P.Sun * (bDay ? 0.4f : 0.f) + P.Warm * 0.3f;
			}
			break;
		default:
			break;
		}
		if (S <= 0.f)
		{
			return -1.f;
		}
		// near is better; home is better for sleep; crowded is worse (cats mind it more)
		S *= 1.f / (1.f + D / (bCat ? 60.f : 200.f));
		if (Goal == EGoal::Sleep && &P == &Places[std::max(0, A.Home)])
		{
			S *= 1.6f;
		}
		if (P.Occupants >= P.Capacity)
		{
			S *= bCat ? 0.2f : 0.5f;
		}
		return S;
	}

	int FWorld::PickPlace(const FAnimal& A, EGoal Goal) const
	{
		int Best = -1;
		float BS = 0.f;
		uint32_t R = A.Rng ^ (uint32_t)(Time * 7.0);
		for (int I = 0; I < (int)Places.size(); I++)
		{
			const FPlace& P = Places[I];
			if (std::fabs(P.Pos.X - A.HomePos.X) > A.Range * 1.4f + 30.f || std::fabs(P.Pos.Y - A.HomePos.Y) > A.Range * 1.4f + 30.f)
			{
				continue;
			}
			const float S = PlaceScore(A, P, Goal);
			if (S <= 0.f)
			{
				continue;
			}
			const float J = S * (0.6f + 0.8f * Rand(R));
			if (J > BS)
			{
				BS = J;
				Best = I;
			}
		}
		return Best;
	}

	void FWorld::SetAct(FAnimal& A, EAct Act, float Len)
	{
		A.Act = Act;
		A.ActTime = 0.f;
		A.ActLen = Len;
	}

	void FWorld::GoTo(FAnimal& A, const V3& To, EAct Gait, EGoal Goal, int Place)
	{
		A.Goal = Goal;
		A.GoalPlace = Place;
		A.GoalPos = To;
		A.Stuck = 0.f;
		A.BestD = 1e9f;
		A.NoProgress = 0.f;
		A.Replans = 0;
		if (!FindPath(A.Pos, To, A.Kind, A.Path) || A.Path.empty())
		{
			A.Path.clear();
			SetAct(A, EAct::Stand, 2.f);
			A.Goal = EGoal::None;
			return;
		}
		A.PathI = 0;
		SetAct(A, Gait, 0.f);
	}

	void FWorld::Choose(FAnimal& A)
	{
		const bool bCat = IsCat(A);
		const float Act = Activity(A);
		A.bLook = false;
		A.Target = -1;
		A.TargetBird = -1;
		// a kitten keeps close to its mother
		if (A.Pack <= -2)
		{
			const int Mom = -2 - A.Pack;
			if (Mom >= 0 && Mom < (int)Animals.size())
			{
				const FAnimal& M = Animals[Mom];
				const float D = (M.Pos - A.Pos).Len2D();
				if (D > 6.f && M.Up <= 0.f)
				{
					GoTo(A, FreeSpot(M.Pos, 1.5f, A.Rng), D > 20.f ? EAct::Trot : EAct::Walk, EGoal::Visit);
					return;
				}
				if (Asleep(M) && A.Tired > 0.2f)
				{
					A.Goal = EGoal::Sleep;
					SetAct(A, EAct::Sleep, 300.f + Rand(A.Rng) * 900.f);
					return;
				}
			}
		}
		// a pack dog goes where its pack goes
		if (A.Pack >= 0 && A.Pack < (int)Packs.size() && !A.bLeader)
		{
			const FPack& P = Packs[A.Pack];
			if (P.Place >= 0 && P.Timer > 0.f)
			{
				const float D = (Places[P.Place].Pos - A.Pos).Len2D();
				if (D > 12.f && A.Tired < 0.85f)
				{
					GoTo(A, FreeSpot(Places[P.Place].Pos, 5.f, A.Rng), D > 60.f ? EAct::Trot : EAct::Walk, EGoal::Visit, P.Place);
					return;
				}
			}
		}

		struct FOpt { EGoal G; float U; };
		FOpt Opts[9];
		int N = 0;
		const bool bSun = Hour > Sunrise + 1.f && Hour < Sunset - 1.f;
		Opts[N++] = { EGoal::Sleep, std::pow(A.Tired, 1.2f) * (1.4f - Act) * (bCat ? 1.6f : 1.f) };
		Opts[N++] = { EGoal::Rest, 0.1f + 0.35f * A.Tired * (1.f - Act) };
		// hungry: to where there is food now (or soon); none anywhere, it puts up with it (and hunts, a cat)
		float EatU = Clamp01((A.Hunger - 0.3f) / 0.6f) * 1.4f;
		if (EatU > 0.f)
		{
			const int P = PickPlace(A, EGoal::Eat);
			if (P < 0 || PlaceScore(A, Places[P], EGoal::Eat) < 0.2f)
			{
				EatU *= 0.25f;
			}
		}
		Opts[N++] = { EGoal::Eat, EatU };
		Opts[N++] = { EGoal::Bask, bSun ? (0.15f + 0.4f * A.Tired) * (bCat ? 1.f : 0.5f) : 0.f };
		Opts[N++] = { EGoal::Roam, (0.08f + A.Curious * (bCat ? 0.55f : 0.9f)) * (0.4f + Act) };
		Opts[N++] = { EGoal::Visit, A.Lonely * A.Social * (bCat ? 0.3f : 0.9f) };
		Opts[N++] = { EGoal::Play, A.Playful * (0.3f + A.Playfulness) * Act };
		Opts[N++] = { EGoal::Hunt, bCat ? A.Hunter * Act * 0.45f : 0.f };
		float Best = -1.f;
		EGoal G = EGoal::Rest;
		for (int I = 0; I < N; I++)
		{
			const float U = Opts[I].U + Rand(A.Rng) * 0.12f;
			if (U > Best)
			{
				Best = U;
				G = Opts[I].G;
			}
		}

		switch (G)
		{
		case EGoal::Sleep:
		case EGoal::Eat:
		case EGoal::Bask:
		case EGoal::Hunt:
		case EGoal::Rest:
		{
			const int P = PickPlace(A, G);
			if (P < 0)
			{
				A.Goal = G;
				SetAct(A, G == EGoal::Sleep ? EAct::Sleep : (G == EGoal::Eat ? EAct::Sniff : EAct::Rest), 60.f + Rand(A.Rng) * 120.f);
				return;
			}
			const float D = (Places[P].Pos - A.Pos).Len2D();
			const V3 Spot = FreeSpot(Places[P].Pos, G == EGoal::Hunt ? 6.f : 1.2f, A.Rng);
			if (D < 2.5f)
			{
				A.GoalPlace = P;
				A.Goal = G;
				A.GoalPos = Spot;
				Arrive(A);
				return;
			}
			const EAct Gait = D > (bCat ? 80.f : 120.f) ? EAct::Trot : EAct::Walk;
			GoTo(A, Spot, Gait, G, P);
			return;
		}
		case EGoal::Visit:
		{
			// the nearest one it knows well (its pack, its neighbour cat)
			int Best_ = -1;
			float BD = 1e9f;
			for (const FAnimal& O : Animals)
			{
				if (O.Id == A.Id || O.Kind != A.Kind || Fam(A.Id, O.Id) < 0.55f)
				{
					continue;
				}
				const float D = (O.Pos - A.Pos).Len2D();
				if (D < BD && D < A.Range * 2.f)
				{
					BD = D;
					Best_ = O.Id;
				}
			}
			if (Best_ < 0 || BD < 4.f)
			{
				A.Goal = EGoal::Rest;
				SetAct(A, A.Tired > 0.4f ? EAct::Rest : EAct::Sit, 30.f + Rand(A.Rng) * 90.f);
				return;
			}
			A.Target = Best_;
			GoTo(A, FreeSpot(Animals[Best_].Pos, 2.f, A.Rng), BD > 60.f ? EAct::Trot : EAct::Walk, EGoal::Visit);
			A.Target = Best_;
			return;
		}
		case EGoal::Play:
		{
			int Mate = -1;
			float BD = 1e9f;
			for (const FAnimal& O : Animals)
			{
				if (O.Id == A.Id || O.Kind != A.Kind || Fam(A.Id, O.Id) < 0.5f || Asleep(O) || Busy(O) || O.Up > 0.f || O.Playful < 0.2f)
				{
					continue;
				}
				const float D = (O.Pos - A.Pos).Len2D();
				if (D < BD && D < 60.f)
				{
					BD = D;
					Mate = O.Id;
				}
			}
			if (Mate < 0)
			{
				// nobody to play with: a cat plays with a leaf, a dog runs about a bit
				A.Goal = EGoal::Play;
				A.Playful *= 0.7f;
				if (bCat)
				{
					SetAct(A, EAct::Crouch, 1.5f + Rand(A.Rng) * 2.f);
					A.Excite = 0.6f;
				}
				else
				{
					GoTo(A, FreeSpot(A.Pos, 15.f, A.Rng), EAct::Run, EGoal::Play);
					A.Excite = 0.7f;
				}
				return;
			}
			A.Target = Mate;
			if (BD > 3.f)
			{
				GoTo(A, FreeSpot(Animals[Mate].Pos, 1.5f, A.Rng), BD > 30.f ? EAct::Trot : EAct::Walk, EGoal::Play);
				A.Target = Mate;
			}
			else
			{
				A.Goal = EGoal::Play;
				Arrive(A);
			}
			return;
		}
		case EGoal::Roam:
		default:
		{
			// out to somewhere in its range: a place of interest now and then, otherwise any spot it can walk to
			V3 To;
			int P = -1;
			if (Rand(A.Rng) < 0.45f)
			{
				P = PickPlace(A, EGoal::Roam);
			}
			if (P >= 0)
			{
				To = FreeSpot(Places[P].Pos, 4.f, A.Rng);
			}
			else
			{
				const V3 C = A.Pack >= 0 && A.Pack < (int)Packs.size() ? Packs[A.Pack].Home : A.HomePos;
				To = FreeSpot(C, A.Range, A.Rng);
				if ((To - A.Pos).Len2D() > A.Range * 0.8f)
				{
					To = FreeSpot(A.Pos + (To - A.Pos) * 0.4f, 10.f, A.Rng);
				}
			}
			const float D = (To - A.Pos).Len2D();
			GoTo(A, To, bCat ? EAct::Walk : (D > 80.f && Rand(A.Rng) < 0.6f ? EAct::Trot : EAct::Walk), EGoal::Roam, P);
			return;
		}
		}
	}

	void FWorld::Arrive(FAnimal& A)
	{
		A.Path.clear();
		A.Speed = 0.f;
		const bool bCat = IsCat(A);
		FPlace* P = A.GoalPlace >= 0 && A.GoalPlace < (int)Places.size() ? &Places[A.GoalPlace] : nullptr;
		if (P && (A.Goal == EGoal::Sleep || A.Goal == EGoal::Rest || A.Goal == EGoal::Bask))
		{
			// face out from the wall / door it lies by
			A.Yaw = P->Yaw + (Rand(A.Rng) - 0.5f) * 1.2f;
		}
		switch (A.Goal)
		{
		case EGoal::Sleep:
			SetAct(A, EAct::Sleep, (bCat ? 1200.f : 2400.f) * (0.5f + Rand(A.Rng)));
			break;
		case EGoal::Rest:
			SetAct(A, Rand(A.Rng) < 0.6f ? EAct::Rest : EAct::Sit, 40.f + Rand(A.Rng) * 160.f);
			break;
		case EGoal::Bask:
			SetAct(A, EAct::Rest, 300.f + Rand(A.Rng) * 900.f);
			break;
		case EGoal::Eat:
			if (P && P->FoodLeft > 0.2f)
			{
				SetAct(A, EAct::Eat, 60.f + Rand(A.Rng) * 100.f);
				P->FoodLeft = std::max(0.f, P->FoodLeft - 1.f);
				if (bCat && Rand(A.Rng) < 0.5f)
				{
					Emit(EEvent::Meow, A);
				}
			}
			else if (P && P->Kind == EPlace::Feeder)
			{
				SetAct(A, EAct::Sit, 120.f + Rand(A.Rng) * 300.f);      // waits for the bowl
			}
			else
			{
				SetAct(A, EAct::Sniff, 8.f + Rand(A.Rng) * 12.f);
				A.Hunger = std::max(0.f, A.Hunger - 0.05f);
			}
			break;
		case EGoal::Hunt:
			SetAct(A, bCat ? EAct::Sit : EAct::Sniff, 30.f + Rand(A.Rng) * 90.f);   // waits for a bird to come down
			break;
		case EGoal::Play:
			if (A.Target >= 0 && A.Target < (int)Animals.size())
			{
				FAnimal& O = Animals[A.Target];
				if (!Busy(O) && !Asleep(O) && O.Up <= 0.f && (O.Pos - A.Pos).Len2D() < 6.f)
				{
					// an invitation (a dog's bow, a kitten's pounce at its sibling), then they chase each other round
					SetAct(A, bCat ? EAct::Crouch : EAct::PlayBow, 1.2f);
					A.bChaser = true;
					A.Center = (A.Pos + O.Pos) * 0.5f;
					O.Target = A.Id;
					O.Goal = EGoal::Play;
					O.bChaser = false;
					O.Center = A.Center;
					SetAct(O, EAct::Play, 20.f + Rand(A.Rng) * 40.f);
					O.Path.clear();
					A.ActLen = 1.2f;
					Emit(EEvent::PlayStart, A, O.Id);
					break;
				}
			}
			SetAct(A, EAct::Stand, 2.f);
			break;
		case EGoal::Visit:
			if (A.Target >= 0 && A.Target < (int)Animals.size() && (Animals[A.Target].Pos - A.Pos).Len2D() < 4.f && !Asleep(Animals[A.Target]))
			{
				SetAct(A, EAct::Greet, 3.f + Rand(A.Rng) * 3.f);
				A.Look = Animals[A.Target].Pos + V3(0.f, 0.f, 0.3f);
				A.bLook = true;
				Emit(EEvent::Greet, A, A.Target);
			}
			else if (A.Tired > 0.5f || (A.Target >= 0 && Asleep(Animals[A.Target])))
			{
				A.Goal = EGoal::Sleep;
				SetAct(A, EAct::Sleep, 600.f + Rand(A.Rng) * 1200.f);     // lies down by its packmate / its mother
			}
			else
			{
				SetAct(A, Rand(A.Rng) < 0.5f ? EAct::Rest : EAct::Sniff, 20.f + Rand(A.Rng) * 60.f);
			}
			A.Lonely = std::max(0.f, A.Lonely - 0.3f);
			break;
		case EGoal::Roam:
		default:
			SetAct(A, Rand(A.Rng) < (bCat ? 0.4f : 0.6f) ? EAct::Sniff : (Rand(A.Rng) < 0.5f ? EAct::Stand : EAct::Sit), 4.f + Rand(A.Rng) * 10.f);
			if (!bCat && Rand(A.Rng) < 0.1f)
			{
				SetAct(A, EAct::Groom, 3.f + Rand(A.Rng) * 3.f);        // a scratch behind the ear
			}
			A.Curious = std::max(0.f, A.Curious - 0.12f);
			break;
		}
	}

	void FWorld::PackThink(FPack& P, float Dt)
	{
		P.Timer -= Dt;
		if (P.Leader < 0)
		{
			return;
		}
		FAnimal& L = Animals[P.Leader];
		// the pack's errand is the leader's: wherever it goes for a while, the others come along
		if (L.Goal != EGoal::None && L.GoalPlace >= 0 && IsMoving(L.Act) && L.Goal != EGoal::Visit && (L.GoalPos - L.Pos).Len2D() > 20.f)
		{
			if (P.Place != L.GoalPlace)
			{
				P.Place = L.GoalPlace;
				P.Timer = 600.f;
				for (int M : P.Members)
				{
					FAnimal& A = Animals[M];
					if (M != P.Leader && !Asleep(A) && !Busy(A) && (A.Pos - L.Pos).Len2D() < 60.f)
					{
						A.ThinkTimer = 0.3f + 1.5f * Rand(A.Rng);      // they get up one after another
						A.Goal = EGoal::None;
						SetAct(A, EAct::Stand, 0.5f);
					}
				}
			}
		}
		else if (Asleep(L) && P.Timer > 0.f && P.Place != L.GoalPlace && L.GoalPlace >= 0)
		{
			P.Place = L.GoalPlace;
			P.Timer = 900.f;
		}
	}

	// --- perception ---------------------------------------------------------------------------------

	void FWorld::StartFlee(FAnimal& A, const V3& From, int Cause)
	{
		A.Center = From;
		A.Target = Cause;
		A.Goal = EGoal::Refuge;
		A.Path.clear();
		A.Fear = std::max(A.Fear, 0.8f);
		A.Excite = 1.f;
		// a cat runs for the nearest tree or wall away from the danger, or into the cellar, else home
		if (IsCat(A))
		{
			const int R = NearestRefuge(A.Pos, 30.f, From);
			if (R >= 0)
			{
				A.Refuge = R;
				A.GoalPos = Places[R].Pos;
				SetAct(A, EAct::Flee, 0.f);
				Emit(EEvent::Flee, A, Cause);
				return;
			}
		}
		A.Refuge = -1;
		const V3 Away = (A.Pos - From).Flat().Norm();
		V3 To = A.Pos + Away * (IsCat(A) ? 18.f : 25.f);
		if (!LineWalkable(A.Pos, To))
		{
			for (int K = 1; K <= 4; K++)
			{
				for (int S = -1; S <= 1; S += 2)
				{
					const float Ang = std::atan2(Away.Y, Away.X) + S * K * 0.5f;
					const V3 T = A.Pos + Dir(Ang) * (IsCat(A) ? 14.f : 20.f);
					if (LineWalkable(A.Pos, T))
					{
						To = T;
						K = 9;
						break;
					}
				}
			}
		}
		A.GoalPos = To;
		A.Path.assign(1, To);
		A.PathI = 0;
		SetAct(A, EAct::Flee, 0.f);
		Emit(EEvent::Flee, A, Cause);
	}

	void FWorld::GiveWay(FAnimal& A, const V3& From)
	{
		const V3 Away = (A.Pos - From).Flat().Norm();
		const float Base = std::atan2(Away.Y, Away.X);
		for (int K = 0; K <= 4; K++)
		{
			for (int S = -1; S <= 1; S += 2)
			{
				const V3 To = A.Pos + Dir(Base + S * K * 0.4f) * (IsCat(A) ? 10.f : 20.f);
				if (LineWalkable(A.Pos, To))
				{
					A.Path.assign(1, V3(To.X, To.Y, GroundZ(To.X, To.Y)));
					A.PathI = 0;
					A.GoalPos = To;
					A.Goal = EGoal::Roam;
					A.GoalPlace = -1;
					SetAct(A, IsCat(A) ? EAct::Walk : EAct::Trot, 0.f);
					return;
				}
			}
		}
		SetAct(A, EAct::Watch, 2.f);
	}

	int FWorld::NearestRefuge(const V3& P, float MaxD, const V3& AwayFrom) const
	{
		int Best = -1;
		float BS = 1e9f;
		for (int I = 0; I < (int)Places.size(); I++)
		{
			const FPlace& R = Places[I];
			if (R.Kind != EPlace::Refuge && R.Kind != EPlace::Basement)
			{
				continue;
			}
			const V3 D = R.Pos - P;
			const float L = D.Len2D();
			if (L > MaxD)
			{
				continue;
			}
			// not toward the danger
			const float Toward = (AwayFrom - P).Flat().Norm().Dot(D.Flat().Norm());
			if (L > 3.f && Toward > 0.5f && (R.Pos - AwayFrom).Len2D() < L + 2.f)
			{
				continue;
			}
			const float S = L + Toward * 6.f;
			if (S < BS)
			{
				BS = S;
				Best = I;
			}
		}
		return Best;
	}

	void FWorld::StartChase(FAnimal& A, int Target)
	{
		A.Target = Target;
		A.Goal = EGoal::None;
		A.Path.clear();
		A.Excite = 1.f;
		SetAct(A, EAct::Chase, 6.f + Rand(A.Rng) * 6.f);
		Emit(EEvent::ChaseStart, A, Target);
		if (Rand(A.Rng) < 0.6f)
		{
			Emit(EEvent::Bark, A, Target);
		}
	}

	void FWorld::PerceivePlayer(FAnimal& A, float Dt)
	{
		if (!Player.bPresent)
		{
			A.bNoticed = false;
			return;
		}
		const bool bCat = IsCat(A);
		const V3 Body = Player.Feet + V3(0.f, 0.f, Player.bCrouched ? 0.6f : 1.f);
		const float Dist = (A.Pos - Player.Feet).Len2D();
		if (Dist > 45.f)
		{
			A.Unseen += Dt;
			if (A.Unseen > 10.f)
			{
				A.bNoticed = false;
			}
			return;
		}
		const float Speed = Player.Vel.Len2D();
		const float Hearing = Player.bCrouched ? 3.f : (Speed < 0.2f ? 0.f : (Speed > 3.5f ? 22.f : 8.f));
		bool bSeen = false;
		if (!Asleep(A) || Dist < 2.f)
		{
			const V3 Head = A.bLook ? (A.Look - A.Pos).Flat().Norm() : Dir(A.Yaw);
			bSeen = Dist < 3.f || Head.Dot((Body - A.Pos).Flat().Norm()) > std::cos((bCat ? 110.f : 120.f) * Pi / 180.f)
				|| (A.bNoticed && Dist < 25.f)                                   // keeps an eye on them
				|| (Dist < 15.f && Rand(A.Rng) < Dt * (bCat ? 0.3f : 0.5f));     // a glance round (a dog's nose)
			if (bSeen && A.Tier == 0 && Sight && Dist > 2.f)
			{
				bSeen = Sight(A.Pos + V3(0.f, 0.f, bCat ? 0.25f : 0.45f), Player.Eye);
			}
		}
		const bool bHeard = Dist < Hearing * (Asleep(A) ? 0.4f : 1.f);
		if (!bSeen && !bHeard)
		{
			A.Unseen += Dt;
			if (A.Unseen > 8.f)
			{
				A.bNoticed = false;
			}
			return;
		}
		A.Unseen = 0.f;
		// the danger: how close they let a person come, as they move now
		const V3 ToMe = (A.Pos - Player.Feet).Flat().Norm();
		const float Closing = std::max(0.f, Player.Vel.Dot(ToMe));
		const float Motion = 0.35f + 0.65f * std::min(Speed / 1.4f, 2.2f);
		const float Approach = 1.f + 0.5f * std::min(Closing / 1.4f, 1.f);
		const float Posture = Player.bCrouched ? 0.55f : 1.f;
		const float Shy = bCat ? 3.2f * (1.7f - A.Bold - 0.5f * A.Friendly) : 2.2f * (1.5f - A.Bold - 0.4f * A.Friendly);
		if (Dist < 20.f)
		{
			A.Habituation = Clamp01(A.Habituation + Dt * (Speed < 0.5f && Player.ExtraThreat <= 0.f ? 1.f / 12.f : -Speed / 4.f));
		}
		const float FleeD = std::max(0.3f, Shy * Motion * Approach * Posture * (1.f - 0.6f * A.Habituation) * (1.f - 0.85f * A.Trust)
			+ Player.ExtraThreat * 4.f);
		if (Asleep(A))
		{
			if (Dist < FleeD * 0.6f || (bHeard && Speed > 3.5f && Dist < 6.f))
			{
				SetAct(A, EAct::Stand, 1.f);            // wakes up
				A.Alert = 0.6f;
			}
			else
			{
				return;
			}
		}
		if (!A.bNoticed)
		{
			A.bNoticed = true;
			A.Seen = 0.f;
			Emit(EEvent::Notice, A);
		}
		A.Seen += Dt;
		A.Look = Player.Eye;
		A.bLook = true;
		if (Dist < FleeD * 2.2f)
		{
			A.Alert = std::max(A.Alert, Clamp01((FleeD * 2.2f - Dist) / std::max(FleeD * 1.2f, 0.5f)));
		}
		// trust grows when the player is calm near it, and falls when they frighten it
		if (Dist < 8.f && Speed < 1.6f && A.Fear < 0.3f)
		{
			A.Trust = std::min(1.f, A.Trust + Dt * 0.006f * (0.3f + A.Friendly) * (0.5f + A.Habituation) * (Player.bCrouched ? 1.5f : 1.f));
		}
		if (Dist < FleeD && A.Act != EAct::Flee && A.Act != EAct::Climb && A.Act != EAct::Perch)
		{
			A.Trust = std::max(0.f, A.Trust - 0.05f);
			A.Habituation *= 0.5f;
			StartFlee(A, Player.Feet, -1);
			return;
		}
		if (Busy(A) || A.Up > 0.f)
		{
			return;
		}
		// a first long look when it notices them, uneasy or curious
		if (A.Seen < 0.3f && Dist < 14.f && A.Act != EAct::Watch && A.Act != EAct::Approach && A.Act != EAct::Beg && A.Act != EAct::Rub)
		{
			A.Path.clear();
			SetAct(A, EAct::Watch, 1.5f + Rand(A.Rng) * 2.5f);
			Emit(EEvent::Watch, A);
			return;
		}
		// curious and friendly: comes up to them (slowly), sniffs, a cat rubs their legs, a hungry dog begs
		const bool bCalm = Speed < 1.2f && A.Fear < 0.15f && A.Alert < 0.6f;
		const float Want = A.Friendly * (0.4f + A.Curious) * (0.4f + A.Trust + A.Habituation * 0.4f) * (A.Hunger > 0.5f ? 1.4f : 1.f);
		if (bCalm && Dist < 15.f && Dist > (bCat ? 0.55f : 0.9f) && A.Goal != EGoal::Person && A.Cooldown <= 0.f && Want > (bCat ? 0.35f : 0.25f)
			&& Rand(A.Rng) < Dt * 0.25f && A.Act != EAct::Eat)
		{
			A.Goal = EGoal::Person;
			A.Target = -1;
			A.Path.assign(1, Player.Feet + ToMe * (bCat ? 0.45f : 0.85f));
			A.PathI = 0;
			SetAct(A, EAct::Approach, 25.f);
			Emit(EEvent::Approach, A);
			if (bCat && Rand(A.Rng) < 0.6f)
			{
				Emit(EEvent::Meow, A);
			}
			return;
		}
		if (A.Goal == EGoal::Person && A.Act == EAct::Approach)
		{
			A.Path.assign(1, Player.Feet + ToMe * (bCat ? 0.45f : 0.85f));      // keeps on to where they are now
			A.PathI = 0;
		}
		if (A.Goal == EGoal::Person && !IsMoving(A.Act) && A.Act != EAct::Rub && A.Act != EAct::Beg && A.ActTime > A.ActLen)
		{
			A.Goal = EGoal::None;
			A.Cooldown = 30.f + Rand(A.Rng) * 90.f;
		}
		// a dog that does not like strangers barks at them from a distance (more by its den, at night)
		if (!bCat && A.Friendly < 0.4f && A.Bold > 0.5f && Dist < 12.f && A.BarkTimer <= 0.f && A.Trust < 0.3f)
		{
			const bool bHome = (A.Pos - A.HomePos).Len2D() < 40.f;
			if (bHome || Hour < Sunrise || Hour > Sunset + 1.f)
			{
				A.Path.clear();
				SetAct(A, EAct::Bark, 2.f + Rand(A.Rng) * 3.f);
				A.BarkTimer = 15.f + Rand(A.Rng) * 30.f;
				Emit(EEvent::Bark, A);
			}
		}
	}

	/** Dogs and cats meeting: returns true if it took over what the animal does. */
	bool FWorld::PerceiveAnimals(FAnimal& A, float Dt)
	{
		const bool bCat = IsCat(A);
		bool bTook = false;
		int Threat = -1;
		float ThreatD = 1e9f;
		int Chaser = -1;
		ForNear(A.Pos, bCat ? 22.f : 25.f, [&](int I)
		{
			if (I == A.Id || bTook)
			{
				return;
			}
			FAnimal& O = Animals[I];
			const float D = (O.Pos - A.Pos).Len2D();
			const float F = Fam(A.Id, I);
			if (bCat && IsDog(O))
			{
				// a dog after this cat, or one coming at it that it doesn't know
				if (O.Act == EAct::Chase && O.Target == A.Id)
				{
					Chaser = I;
				}
				else if (F < 0.5f && D < ThreatD && D < 8.f && IsMoving(O.Act) && O.Speed > 1.f && O.Vel.Dot((A.Pos - O.Pos).Norm()) > 0.6f)
				{
					Threat = I;
					ThreatD = D;
				}
			}
			else if (!bCat && IsCat(O))
			{
				// a cat hissing at it: a bold dog barks back (keeping its distance), the rest give way
				if (O.Act == EAct::Hiss && O.Target == A.Id && D < 4.5f && A.Act != EAct::Bark && !Asleep(A))
				{
					A.Path.clear();
					A.Cooldown = 60.f + Rand(A.Rng) * 60.f;
					if (A.Bold > 0.5f)
					{
						SetAct(A, EAct::Bark, 2.f + Rand(A.Rng) * 3.f);
						A.Target = I;
						A.Look = O.Pos + V3(0.f, 0.f, 0.2f);
						A.bLook = true;
						Emit(EEvent::Bark, A, I);
					}
					else
					{
						GiveWay(A, O.Pos);
					}
					bTook = true;
					return;
				}
				// a dog sees a cat it doesn't know: some go for it
				if (!Busy(A) && !Asleep(A) && A.Cooldown <= 0.f && O.Up <= 0.f && O.Act != EAct::Climb && D < 16.f && D > 1.f && A.Act != EAct::Eat
					&& !(O.Act == EAct::Sleep && (GroundAt(O.Pos.X, O.Pos.Y) == EGround::Yard) && D > 5.f))
				{
					const float Drive = A.Hunter * (1.f - F) * (0.4f + A.Excite) * (O.Act == EAct::Flee || O.Speed > 2.f ? 2.f : 1.f);
					if (Rand(A.Rng) < Dt * Drive * 0.07f)
					{
						StartChase(A, I);
						bTook = true;
					}
				}
			}
			else if (O.Kind == A.Kind && !Asleep(A) && !Busy(A) && !Asleep(O))
			{
				// the same kind: friends greet (and dogs of a pack play), strangers watch, posture, bark, give way
				if (F >= 0.5f)
				{
					if (D < 3.f && A.Cooldown <= 0.f && (A.Act == EAct::Stand || A.Act == EAct::Walk || A.Act == EAct::Sniff) && Rand(A.Rng) < Dt * 0.08f)
					{
						A.Path.clear();
						SetAct(A, EAct::Greet, 2.5f + Rand(A.Rng) * 2.f);
						A.Look = O.Pos + V3(0.f, 0.f, 0.3f);
						A.bLook = true;
						A.Cooldown = 60.f;
						A.Target = I;
						AddFam(A.Id, I, 0.01f);
						Emit(EEvent::Greet, A, I);
						bTook = true;
					}
				}
				else if (D < (bCat ? 9.f : 18.f))
				{
					// a stranger: a long stare; close up, the less bold one goes (cats hiss first, dogs bark)
					const float MyHome = (A.Pos - A.HomePos).Len2D();
					const float Edge = A.Bold + (MyHome < (bCat ? 25.f : 60.f) ? 0.25f : -0.1f) - (O.Bold + ((O.Pos - O.HomePos).Len2D() < (bCat ? 25.f : 60.f) ? 0.25f : -0.1f));
					if (A.Act != EAct::Watch && A.Act != EAct::Hiss && A.Act != EAct::Bark && A.Cooldown <= 0.f)
					{
						A.Path.clear();
						A.Look = O.Pos + V3(0.f, 0.f, 0.3f);
						A.bLook = true;
						A.Target = I;
						A.Alert = std::max(A.Alert, 0.5f);
						if (D < (bCat ? 3.f : 8.f) && Edge < -0.05f)
						{
							A.Cooldown = 30.f;
							GiveWay(A, O.Pos);            // gives way: walks off smartly
						}
						else if (D < (bCat ? 3.5f : 10.f) && Edge >= 0.f)
						{
							SetAct(A, bCat ? EAct::Hiss : EAct::Bark, 2.f + Rand(A.Rng) * 3.f);
							Emit(bCat ? EEvent::Hiss : EEvent::Bark, A, I);
							if (!bCat)
							{
								Emit(EEvent::Growl, A, I);
							}
							A.Cooldown = 60.f;
						}
						else
						{
							SetAct(A, EAct::Watch, 2.f + Rand(A.Rng) * 3.f);
						}
						AddFam(A.Id, I, 0.004f);   // they get used to each other over many meetings
						bTook = true;
					}
				}
			}
			else if (!bCat && IsDog(O) && O.Act == EAct::Play && O.Target == A.Id)
			{
				// it is being played with (handled in Move); nothing to decide
			}
		});
		if (bTook)
		{
			return true;
		}
		if (bCat && A.Up <= 0.f && A.Act != EAct::Climb && A.Act != EAct::Hiss && !(A.Act == EAct::Flee && A.Refuge >= 0))
		{
			const int From = Chaser >= 0 ? Chaser : Threat;
			if (From >= 0)
			{
				const FAnimal& Dg = Animals[From];
				const float D = (Dg.Pos - A.Pos).Len2D();
				const int R = NearestRefuge(A.Pos, 30.f, Dg.Pos);
				// no tree near and bold (or caught up with): it stands its ground and hisses, side on
				const bool bStand = R < 0 && (A.Bold > 0.6f || (A.Act == EAct::Flee && D < 2.5f));
				if (bStand && D < 4.f)
				{
					if (A.Act != EAct::Hiss)
					{
						A.Path.clear();
						SetAct(A, EAct::Hiss, 3.f + Rand(A.Rng) * 4.f);
						const V3 To = (Dg.Pos - A.Pos).Flat();
						A.Yaw = std::atan2(To.Y, To.X) + Pi * 0.5f;
						A.Look = Dg.Pos + V3(0.f, 0.f, 0.35f);
						A.bLook = true;
						A.Target = From;
						A.Fear = 0.9f;
						A.Excite = 1.f;
						Emit(EEvent::Hiss, A, From);
					}
					return true;
				}
				if (bStand && A.Bold > 0.6f)
				{
					if (A.Act != EAct::Watch)
					{
						A.Path.clear();
						SetAct(A, EAct::Watch, 4.f);           // turns to face it, ready
					}
					A.Look = Dg.Pos + V3(0.f, 0.f, 0.35f);
					A.bLook = true;
					A.Alert = 1.f;
					return true;
				}
				if (A.Act == EAct::Flee)
				{
					return false;
				}
				StartFlee(A, Dg.Pos, From);
				return true;
			}
		}
		return false;
	}

	bool FWorld::PerceivePrey(FAnimal& A, float Dt)
	{
		if (Prey.empty() || A.Up > 0.f || Asleep(A) || A.Fear > 0.3f || A.Cooldown > 0.f || A.Hunger > 0.9f)
		{
			return false;
		}
		const bool bCat = IsCat(A);
		if (A.Act == EAct::Stalk || A.Act == EAct::Crouch || A.Act == EAct::Pounce || (A.Act == EAct::Chase && A.TargetBird >= 0))
		{
			return false;
		}
		if (Busy(A) || A.Act == EAct::Eat || A.Goal == EGoal::Person)
		{
			return false;
		}
		int Best = -1;
		float BD = bCat ? 18.f : 14.f;
		int Flock = 0;
		for (int I = 0; I < (int)Prey.size(); I++)
		{
			const FPrey& P = Prey[I];
			if (!P.bGround || (bCat && P.Size > 0.5f))
			{
				continue;
			}
			const float D = (P.Pos - A.Pos).Len2D();
			if (D < 25.f)
			{
				Flock++;
			}
			if (D < BD)
			{
				BD = D;
				Best = I;
			}
		}
		if (Best < 0)
		{
			return false;
		}
		const float Act = Activity(A);
		if (bCat)
		{
			if (Rand(A.Rng) < Dt * A.Hunter * (0.3f + Act) * 0.7f)
			{
				A.TargetBird = Prey[Best].Id;
				A.Goal = EGoal::Hunt;
				A.Path.assign(1, Prey[Best].Pos);
				A.PathI = 0;
				A.Look = Prey[Best].Pos;
				A.bLook = true;
				SetAct(A, EAct::Stalk, 90.f);
				A.Excite = 0.6f;
				return true;
			}
		}
		else if (Flock >= 2 || Prey[Best].Size > 0.3f)
		{
			// a dog runs at pigeons, gulls on the beach, rooks on a field: for the fun of seeing them go up
			if (Rand(A.Rng) < Dt * (0.15f + A.Playfulness * 0.5f + A.Hunter * 0.3f) * (0.3f + Act) * 0.4f)
			{
				A.TargetBird = Prey[Best].Id;
				A.Target = -2;
				A.Goal = EGoal::Play;
				A.Path.clear();
				A.Center = Prey[Best].Pos;
				SetAct(A, EAct::Chase, 5.f + Rand(A.Rng) * 3.f);
				A.Excite = 1.f;
				Emit(EEvent::ChaseStart, A, -2);
				return true;
			}
		}
		return false;
	}

	void FWorld::Perceive(FAnimal& A, float Dt)
	{
		if (A.Act == EAct::Climb || A.Act == EAct::Pounce)
		{
			return;
		}
		PerceivePlayer(A, Dt);
		if (A.Act == EAct::Flee && A.ActTime < 0.05f)
		{
			return;
		}
		if (A.Tier <= 2 && PerceiveAnimals(A, Dt))
		{
			return;
		}
		if (A.Tier <= 1)
		{
			PerceivePrey(A, Dt);
		}
	}

	// --- thinking -----------------------------------------------------------------------------------

	void FWorld::Think(FAnimal& A, float Dt)
	{
		UpdateNeeds(A, Dt);
		Perceive(A, Dt);
		const bool bCat = IsCat(A);

		switch (A.Act)
		{
		case EAct::Sleep:
			// it sleeps on through the hours it is not about (a dog's night), and wakes rested, or hungry
			if ((A.Tired < 0.04f && (Activity(A) > 0.3f || Rand(A.Rng) < Dt / 1800.f)) || (A.ActTime > A.ActLen && A.Tired < 0.2f && Activity(A) > 0.4f)
				|| (A.Hunger > 0.9f && A.ActTime > 600.f))
			{
				// wakes: a stretch, a wash
				SetAct(A, bCat ? EAct::Groom : EAct::Stand, 3.f + Rand(A.Rng) * 5.f);
				A.Goal = EGoal::None;
			}
			return;
		case EAct::Perch:
		{
			// up the tree until the dog is gone (and a while more), then down
			bool bDog = false;
			ForNear(A.Pos, 15.f, [&](int I) { if (IsDog(Animals[I]) && Fam(A.Id, I) < 0.5f) bDog = true; });
			if (bDog)
			{
				A.ActTime = std::min(A.ActTime, 0.f);
			}
			if (!bDog && A.ActTime > A.ActLen)
			{
				A.UpFrom = A.Up;
				A.UpTo = 0.f;
				SetAct(A, EAct::Climb, 1.4f);
			}
			return;
		}
		case EAct::Bark:
			if (A.Target >= 0 && A.Target < (int)Animals.size() && Animals[A.Target].Up > 0.f)
			{
				A.Look = Animals[A.Target].Pos;
				A.bLook = true;
				if (Rand(A.Rng) < std::min(Dt, 1.f) * 0.7f)
				{
					Emit(EEvent::Bark, A, A.Target);
				}
			}
			else if (Rand(A.Rng) < std::min(Dt, 1.f) * 0.5f)
			{
				Emit(EEvent::Bark, A, A.Target);
			}
			if (A.ActTime > A.ActLen)
			{
				A.Cooldown = std::max(A.Cooldown, 60.f + Rand(A.Rng) * 120.f);
				A.Goal = EGoal::None;
				A.Target = -1;
				SetAct(A, EAct::Stand, 1.f);
			}
			return;
		case EAct::Hiss:
			if (A.ActTime > A.ActLen)
			{
				// the dog lost interest (or the cat ends it by walking off)
				if (A.Target >= 0 && A.Target < (int)Animals.size() && IsDog(Animals[A.Target]) && (Animals[A.Target].Pos - A.Pos).Len2D() < 4.f)
				{
					StartFlee(A, Animals[A.Target].Pos, A.Target);
				}
				else
				{
					SetAct(A, EAct::Groom, 4.f + Rand(A.Rng) * 4.f);
					A.Goal = EGoal::None;
				}
			}
			return;
		case EAct::Crouch:
			if (A.ActTime > A.ActLen)
			{
				if (A.TargetBird >= 0)
				{
					// the pounce: a leap at where the bird is
					for (const FPrey& P : Prey)
					{
						if (P.Id == A.TargetBird)
						{
							A.GoalPos = P.Pos;
						}
					}
					SetAct(A, EAct::Pounce, 0.55f);
					A.Center = A.Pos;
					const V3 To = (A.GoalPos - A.Pos).Flat();
					A.Yaw = std::atan2(To.Y, To.X);
					ScaredBirds.push_back(A.TargetBird);
					Emit(EEvent::Pounce, A, -2);
				}
				else if (A.Goal == EGoal::Play && A.Target >= 0)
				{
					// kittens: the pounce opens the game
					SetAct(A, EAct::Play, 15.f + Rand(A.Rng) * 25.f);
				}
				else
				{
					SetAct(A, EAct::Pounce, 0.45f);            // at a leaf
					A.Center = A.Pos;
					A.GoalPos = A.Pos + Dir(A.Yaw) * 0.8f;
				}
			}
			return;
		case EAct::PlayBow:
			if (A.ActTime > A.ActLen)
			{
				SetAct(A, EAct::Play, 20.f + Rand(A.Rng) * 40.f);
			}
			return;
		default:
			break;
		}

		if (Traveling(A) || A.Act == EAct::Climb || A.Act == EAct::Pounce)
		{
			if (A.Act == EAct::Play && A.ActTime > A.ActLen)
			{
				A.Playful = std::max(0.f, A.Playful - 0.5f);
				A.Cooldown = 120.f;
				A.Goal = EGoal::Rest;
				SetAct(A, EAct::Rest, 30.f + Rand(A.Rng) * 60.f);        // panting
			}
			else if (A.Act == EAct::Chase && A.ActTime > A.ActLen)
			{
				A.Cooldown = 90.f + Rand(A.Rng) * 120.f;
				A.Goal = EGoal::None;
				A.TargetBird = -1;
				SetAct(A, EAct::Stand, 2.f);
			}
			else if (A.Act == EAct::Stalk && A.ActTime > A.ActLen)
			{
				A.Cooldown = 60.f;
				A.TargetBird = -1;
				SetAct(A, EAct::Sit, 5.f);
			}
			else if (A.Act == EAct::Approach && A.ActTime > A.ActLen)
			{
				A.Goal = EGoal::None;
				A.Cooldown = 40.f;
				SetAct(A, EAct::Stand, 2.f);
			}
			return;
		}
		if (A.ActTime < A.ActLen)
		{
			// lying somewhere and very tired: falls asleep where it is
			if ((A.Act == EAct::Rest || A.Act == EAct::Sit) && A.Tired > 0.75f && A.Fear < 0.1f && Rand(A.Rng) < Dt * 0.02f)
			{
				SetAct(A, EAct::Sleep, 900.f + Rand(A.Rng) * 1500.f);
			}
			// sitting by the feeder: the bowl came
			if (A.Act == EAct::Sit && A.Goal == EGoal::Eat && A.GoalPlace >= 0 && Places[A.GoalPlace].FoodLeft > 0.2f)
			{
				Arrive(A);
			}
			return;
		}
		if (A.Act == EAct::Eat)
		{
			A.Hunger = std::max(0.f, A.Hunger - 0.6f);
			A.Meals++;
			A.Goal = EGoal::None;
			SetAct(A, bCat ? EAct::Groom : EAct::Stand, bCat ? 6.f + Rand(A.Rng) * 10.f : 2.f);
			return;
		}
		if (A.Act == EAct::Rub || A.Act == EAct::Beg)
		{
			A.Goal = EGoal::None;
			A.Cooldown = 60.f + Rand(A.Rng) * 120.f;
		}
		Choose(A);
	}

	// --- moving -------------------------------------------------------------------------------------

	void FWorld::Move(FAnimal& A, float Dt)
	{
		const bool bCat = IsCat(A);
		A.ActTime += Dt;
		A.Phase += Dt * (0.4f + A.Speed * (bCat ? 2.2f : 1.4f));

		// climbing: straight up (or down) the trunk
		if (A.Act == EAct::Climb)
		{
			const float T = Clamp01(A.ActTime / std::max(A.ActLen, 0.1f));
			A.Up = Lerp(A.UpFrom, A.UpTo, T * T * (3.f - 2.f * T));
			A.Speed = 0.f;
			if (T >= 1.f)
			{
				if (A.UpTo > 0.f)
				{
					SetAct(A, EAct::Perch, 25.f + Rand(A.Rng) * 70.f);
				}
				else
				{
					A.Refuge = -1;
					A.Goal = EGoal::None;
					A.Fear = std::min(A.Fear, 0.3f);
					SetAct(A, EAct::Groom, 4.f + Rand(A.Rng) * 4.f);
				}
			}
			A.Pos.Z = ZAt(A) + A.Up;
			return;
		}
		if (A.Act == EAct::Perch)
		{
			A.Pos.Z = ZAt(A) + A.Up;
			A.Speed = 0.f;
			return;
		}
		if (A.Act == EAct::Pounce)
		{
			const float T = Clamp01(A.ActTime / A.ActLen);
			const V3 P = A.Center + (A.GoalPos - A.Center).Flat() * T;
			if (Walkable(P.X, P.Y))
			{
				A.Pos.X = P.X;
				A.Pos.Y = P.Y;
			}
			A.Up = 0.35f * std::sin(T * Pi);
			A.Pos.Z = ZAt(A) + A.Up;
			A.Speed = (A.GoalPos - A.Center).Len2D() / A.ActLen;
			if (T >= 1.f)
			{
				A.Up = 0.f;
				A.Speed = 0.f;
				A.TargetBird = -1;
				A.Cooldown = 60.f + Rand(A.Rng) * 120.f;
				A.Goal = EGoal::None;
				SetAct(A, EAct::Groom, 3.f + Rand(A.Rng) * 4.f);   // the bird is gone: it washes, as if it didn't care
			}
			return;
		}

		// where it wants to go now
		bool bGo = Traveling(A);
		V3 Want = A.Pos;
		EAct Gait = A.Act;
		if (A.Act == EAct::Chase)
		{
			if (A.TargetBird >= 0)
			{
				// at the birds (they are gone by the time it gets there)
				Want = A.Center;
				bool bThere = false;
				for (const FPrey& P : Prey)
				{
					if (P.Id == A.TargetBird)
					{
						Want = P.Pos;
						bThere = true;
					}
				}
				if ((Want - A.Pos).Len2D() < 5.f && bThere)
				{
					ScaredBirds.push_back(A.TargetBird);
				}
				if ((Want - A.Pos).Len2D() < 1.f)
				{
					A.ActLen = std::min(A.ActLen, A.ActTime + 0.5f);
				}
			}
			else if (A.Target >= 0 && A.Target < (int)Animals.size())
			{
				const FAnimal& O = Animals[A.Target];
				Want = O.Pos;
				// the cat is up the tree, or turned to face it hissing: it stops and barks
				if (O.Up > 0.4f || O.Act == EAct::Perch || (O.Act == EAct::Hiss && (O.Pos - A.Pos).Len2D() < 2.2f))
				{
					A.Path.clear();
					SetAct(A, EAct::Bark, O.Up > 0.4f ? 10.f + Rand(A.Rng) * 25.f : 3.f + Rand(A.Rng) * 3.f);
					A.Look = O.Pos;
					A.bLook = true;
					Emit(EEvent::Bark, A, A.Target);
					bGo = false;
				}
				else if ((O.Pos - A.Pos).Len2D() < 1.2f + A.Speed * A.Speed / 30.f)
				{
					// it never means to catch it: it pulls up, barks, the cat gets its chance
					A.Path.clear();
					SetAct(A, EAct::Bark, 2.f + Rand(A.Rng) * 2.f);
					A.Look = O.Pos;
					A.bLook = true;
					Emit(EEvent::Bark, A, A.Target);
					bGo = false;
				}
			}
		}
		else if (A.Act == EAct::Flee && A.Refuge >= 0)
		{
			Want = Places[A.Refuge].Pos;
			if ((Want - A.Pos).Len2D() < 0.8f)
			{
				if (Places[A.Refuge].Kind == EPlace::Basement)
				{
					// into the cellar window: it sits just inside (drawn hidden), out a while later
					A.Speed = 0.f;
					SetAct(A, EAct::Rest, 60.f + Rand(A.Rng) * 120.f);
					A.Goal = EGoal::Rest;
					A.Refuge = -1;
					return;
				}
				A.UpFrom = 0.f;
				A.UpTo = std::max(1.2f, Places[A.Refuge].Height);
				A.Speed = 0.f;
				SetAct(A, EAct::Climb, 1.1f + A.UpTo * 0.15f);
				Emit(EEvent::Climb, A, A.Target);
				return;
			}
		}
		else if (A.Act == EAct::Play && A.Target >= 0 && A.Target < (int)Animals.size())
		{
			FAnimal& O = Animals[A.Target];
			if (O.Act != EAct::Play && O.Act != EAct::PlayBow && O.Act != EAct::Crouch)
			{
				SetAct(A, EAct::Rest, 20.f + Rand(A.Rng) * 30.f);
				A.Goal = EGoal::Rest;
				bGo = false;
			}
			else if (A.bChaser)
			{
				Want = O.Pos;
				if ((O.Pos - A.Pos).Len2D() < 0.9f)
				{
					// caught: a tumble, then the roles swap
					A.bChaser = false;
					O.bChaser = true;
					Want = A.Pos;
					A.Speed *= 0.3f;
				}
			}
			else
			{
				// round the spot they play at, never far
				const V3 R = (A.Pos - A.Center).Flat();
				const float Ang = std::atan2(R.Y, R.X) + (A.Id % 2 ? 0.9f : -0.9f);
				const float Rad = bCat ? 2.5f : 7.f;
				Want = A.Center + Dir(Ang) * Rad;
			}
			if (A.ActTime > 4.f && Rand(A.Rng) < Dt * 0.15f && O.Act == EAct::Play)
			{
				std::swap(A.bChaser, O.bChaser);
			}
		}
		else if (bGo && !A.Path.empty())
		{
			if (A.PathI >= (int)A.Path.size())
			{
				A.PathI = (int)A.Path.size() - 1;
			}
			Want = A.Path[A.PathI];
			// no headway to this waypoint for a while (round a corner the grid cut): a new route, or it gives up
			const float WD = (Want - A.Pos).Len2D();
			if (WD < A.BestD - 0.3f)
			{
				A.BestD = WD;
				A.NoProgress = 0.f;
			}
			else
			{
				A.NoProgress += Dt;
			}
			if (A.NoProgress > 4.f && A.Act != EAct::Approach)
			{
				A.NoProgress = 0.f;
				A.BestD = 1e9f;
				if (++A.Replans > 2 || !FindPath(A.Pos, A.GoalPos, A.Kind, A.Path) || A.Path.empty())
				{
					A.Path.clear();
					A.Replans = 0;
					if ((A.GoalPos - A.Pos).Len2D() < 8.f)
					{
						Arrive(A);
					}
					else
					{
						A.Goal = EGoal::None;
						SetAct(A, EAct::Stand, 1.f + Rand(A.Rng) * 2.f);
					}
					return;
				}
				A.PathI = 0;
				Want = A.Path[0];
			}
			const float Reach = std::max(A.Act == EAct::Approach ? 0.25f : 0.7f, A.Speed * Dt * 1.1f);   // coarse steps far away
			if ((Want - A.Pos).Len2D() < Reach)
			{
				A.PathI++;
				A.BestD = 1e9f;
				A.NoProgress = 0.f;
				if (A.PathI >= (int)A.Path.size())
				{
					A.Path.clear();
					if (A.Act == EAct::Flee)
					{
						A.Goal = EGoal::None;
						SetAct(A, EAct::Watch, 3.f + Rand(A.Rng) * 4.f);
						A.Look = A.Center;
						A.bLook = true;
					}
					else if (A.Act == EAct::Stalk)
					{
						SetAct(A, EAct::Crouch, 1.5f + Rand(A.Rng) * 3.f);
					}
					else if (A.Act == EAct::Approach && A.Goal == EGoal::Person)
					{
						// at their feet: a cat rubs their legs if it trusts them, a hungry dog begs, others sniff and sit
						const V3 To = (Player.Feet - A.Pos).Flat();
						A.Yaw = std::atan2(To.Y, To.X);
						if (bCat && A.Trust > 0.45f)
						{
							SetAct(A, EAct::Rub, 6.f + Rand(A.Rng) * 8.f);
							Emit(EEvent::Purr, A);
						}
						else if (!bCat && A.Hunger > 0.5f)
						{
							SetAct(A, EAct::Beg, 6.f + Rand(A.Rng) * 10.f);
							Emit(EEvent::Whine, A);
						}
						else
						{
							SetAct(A, bCat ? EAct::Sit : EAct::Greet, 4.f + Rand(A.Rng) * 6.f);
							if (bCat)
							{
								Emit(EEvent::Meow, A);
							}
						}
						A.Curious = std::max(0.f, A.Curious - 0.3f);
					}
					else
					{
						Arrive(A);
					}
					A.Speed = std::min(A.Speed, 0.3f);
					bGo = false;
				}
			}
		}
		else if (bGo)
		{
			Arrive(A);
			bGo = false;
		}
		if (A.Act == EAct::Stalk && A.TargetBird >= 0)
		{
			bool bThere = false;
			for (const FPrey& P : Prey)
			{
				if (P.Id == A.TargetBird && P.bGround)
				{
					bThere = true;
					A.Look = P.Pos;
					A.bLook = true;
					const float D = (P.Pos - A.Pos).Len2D();
					if (D < 2.2f)
					{
						A.Path.clear();
						A.Speed = 0.f;
						SetAct(A, EAct::Crouch, 1.2f + Rand(A.Rng) * 2.5f);
						bGo = false;
					}
					else
					{
						A.Path.assign(1, P.Pos);
						A.PathI = 0;
					}
				}
			}
			if (!bThere)
			{
				// the bird flew: a long look after it
				A.Path.clear();
				A.TargetBird = -1;
				A.Cooldown = 45.f + Rand(A.Rng) * 60.f;
				SetAct(A, EAct::Watch, 2.f + Rand(A.Rng) * 2.f);
				bGo = false;
			}
		}

		float Target = bGo ? GaitSpeed(A, Gait) : 0.f;
		if (A.Act == EAct::Stalk && (Want - A.Pos).Len2D() > 6.f)
		{
			Target = GaitSpeed(A, EAct::Walk) * 0.6f;
		}
		if (bGo)
		{
			V3 To = (Want - A.Pos).Flat();
			const float L = To.Len2D();
			if (L > 0.01f)
			{
				To = To * (1.f / L);
			}
			// keep clear of the others and of the player's feet
			V3 Push;
			ForNear(A.Pos, 1.2f, [&](int I)
			{
				if (I == A.Id || (A.Act == EAct::Play && I == A.Target) || (A.Act == EAct::Chase && I == A.Target))
				{
					return;
				}
				const V3 D = (A.Pos - Animals[I].Pos).Flat();
				const float DL = D.Len2D();
				if (DL > 0.01f && DL < 0.9f)
				{
					Push += D * ((0.9f - DL) / DL);
				}
			});
			if (Player.bPresent && A.Goal != EGoal::Person)
			{
				const V3 D = (A.Pos - Player.Feet).Flat();
				const float DL = D.Len2D();
				if (DL > 0.01f && DL < 1.f)
				{
					Push += D * ((1.f - DL) / DL * 2.f);
				}
			}
			To = (To + Push).Flat().Norm();
			// slow down for the last metres; approach the player hesitantly
			if (A.Act != EAct::Chase && A.Act != EAct::Flee && A.Act != EAct::Play && A.Path.size() > 0 && A.PathI == (int)A.Path.size() - 1)
			{
				Target = std::min(Target, 0.4f + L * (A.Act == EAct::Approach ? 0.5f : 1.2f));
			}
			if (A.Act == EAct::Approach && A.Alert > 0.3f)
			{
				Target *= 0.5f;
			}
			// turning
			float Yaw = std::atan2(To.Y, To.X);
			// what is right ahead: blocked ground makes it turn along the wall
			const float Look = std::max(0.6f, A.Speed * 0.35f);
			const V3 Ahead = A.Pos + Dir(Yaw) * Look;
			if (!Walkable(Ahead.X, Ahead.Y))
			{
				bool bFound = false;
				for (int K = 1; K <= 5 && !bFound; K++)
				{
					for (int S = -1; S <= 1; S += 2)
					{
						const float Y2 = Yaw + S * K * 0.45f;
						const V3 P = A.Pos + Dir(Y2) * Look;
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
			const float D = WrapPi(Yaw - A.Yaw);
			const float MaxTurn = (A.Speed > 3.f ? 5.f : 3.5f) * Dt;
			const float Turn = Clamp(D, -MaxTurn, MaxTurn);
			A.YawRate = Turn / std::max(Dt, 1e-4f);
			A.Yaw = WrapPi(A.Yaw + Turn);
			// a sharp turn slows it down
			Target *= Clamp01(1.2f - std::fabs(D) / Pi);
		}
		else
		{
			A.YawRate = 0.f;
		}
		const float Acc = (A.Act == EAct::Flee || A.Act == EAct::Chase || A.Act == EAct::Pounce) ? (bCat ? 14.f : 9.f) : 3.5f;
		const float Brake = A.Speed > 3.f ? 18.f : Acc * 1.5f;       // a dog pulling up skids to a stop
		A.Speed += Clamp(Target - A.Speed, -Brake * Dt, Acc * Dt);
		if (A.Speed > 0.01f)
		{
			const V3 Step = Dir(A.Yaw) * (A.Speed * Dt);
			const V3 P = A.Pos + Step;
			if (Walkable(P.X, P.Y))
			{
				A.Pos.X = P.X;
				A.Pos.Y = P.Y;
				A.Stuck = std::max(0.f, A.Stuck - Dt);
			}
			else
			{
				A.Speed *= 0.3f;
				A.Stuck += Dt;
			}
			if (A.Stuck > 2.5f && bGo)
			{
				// stuck: try again from here (a new path), or give up
				A.Stuck = 0.f;
				if (A.Act == EAct::Flee || A.Act == EAct::Chase)
				{
					SetAct(A, EAct::Stand, 1.f);
				}
				else if (!FindPath(A.Pos, A.GoalPos, A.Kind, A.Path))
				{
					A.Path.clear();
					SetAct(A, EAct::Stand, 1.f);
					A.Goal = EGoal::None;
				}
				else
				{
					A.PathI = 0;
				}
			}
		}
		A.Vel = Dir(A.Yaw) * A.Speed;
		A.Up = std::max(0.f, A.Up - Dt * 2.f);
		A.Pos.Z = ZAt(A) + A.Up;
	}

	// --- the step -----------------------------------------------------------------------------------

	void FWorld::UpdateTiers()
	{
		for (int& C : TierCount)
		{
			C = 0;
		}
		for (FAnimal& A : Animals)
		{
			const float D = (A.Pos - Camera).Len();
			A.Tier = D < Opt.NearDist ? 0 : (D < Opt.MidDist ? 1 : (D < Opt.FarDist ? 2 : 3));
			// whoever deals with an animal near the player thinks often too
			if (A.Tier > 1 && (A.Act == EAct::Chase || A.Act == EAct::Play || A.Act == EAct::Flee) && A.Target >= 0 && A.Target < (int)Animals.size()
				&& Animals[A.Target].Tier <= 1)
			{
				A.Tier = 1;
			}
			TierCount[A.Tier]++;
		}
	}

	void FWorld::Step(float Dt)
	{
		Dt = Clamp(Dt, 0.f, 0.25f);
		Time += Dt;
		Player.ExtraThreat = std::max(0.f, Player.ExtraThreat - Dt * 1.5f);
		UpdateTiers();
		BuildHash();
		UpdatePlaces(Dt);
		static const double PlacesEvery = 2.0;
		if (std::floor(Time / PlacesEvery) != std::floor((Time - Dt) / PlacesEvery))
		{
			for (FPlace& P : Places)
			{
				P.Occupants = 0;
			}
			for (const FAnimal& A : Animals)
			{
				if (A.GoalPlace >= 0 && A.GoalPlace < (int)Places.size() && !IsMoving(A.Act))
				{
					Places[A.GoalPlace].Occupants++;
				}
			}
			for (FPack& P : Packs)
			{
				PackThink(P, (float)PlacesEvery);
			}
		}
		for (int T = 2; T <= 3; T++)
		{
			StepAcc[T] += Dt;
		}
		const bool bMoveFar = StepAcc[2] >= Opt.StepFar;
		const bool bMoveDormant = StepAcc[3] >= Opt.StepDormant;
		const float FarDt = StepAcc[2], DormantDt = StepAcc[3];
		const float ThinkEvery[4] = { Opt.ThinkNear, Opt.ThinkMid, Opt.ThinkFar, Opt.ThinkDormant };
		for (FAnimal& A : Animals)
		{
			A.ThinkTimer -= Dt;
			if (A.ThinkTimer <= 0.f)
			{
				const float Since = std::max(Dt, ThinkEvery[A.Tier] - A.ThinkTimer);
				A.ThinkTimer = ThinkEvery[A.Tier] * (0.8f + 0.4f * Rand(A.Rng));
				Think(A, std::min(Since, 6.f));
			}
			if (A.Tier <= 1)
			{
				Move(A, Dt);
			}
			else if (A.Tier == 2 && bMoveFar)
			{
				Move(A, FarDt);
			}
			else if (A.Tier == 3 && bMoveDormant)
			{
				Move(A, DormantDt);
			}
		}
		if (bMoveFar)
		{
			StepAcc[2] = 0.f;
		}
		if (bMoveDormant)
		{
			StepAcc[3] = 0.f;
		}
	}

	V3 FWorld::RenderPos(const FAnimal& A) const
	{
		if (A.Tier >= 2 && A.Speed > 0.01f)
		{
			return A.Pos + A.Vel * StepAcc[A.Tier];
		}
		return A.Pos;
	}

	void FWorld::Scare(const V3& P, float R, float Strength)
	{
		ForNear(P, R, [&](int I)
		{
			FAnimal& A = Animals[I];
			const float K = Strength * (1.f - (A.Pos - P).Len2D() / R);
			if (IsCat(A))
			{
				if (K > 0.4f && A.Up <= 0.f && A.Act != EAct::Flee && A.Act != EAct::Climb)
				{
					StartFlee(A, P, -1);
				}
			}
			else
			{
				A.Alert = std::max(A.Alert, K);
				if (K > 0.5f && !Busy(A))
				{
					if (A.Bold > 0.55f)
					{
						SetAct(A, EAct::Bark, 2.f + Rand(A.Rng) * 2.f);
						A.Look = P;
						A.bLook = true;
						Emit(EEvent::Bark, A);
					}
					else
					{
						StartFlee(A, P, -1);
					}
				}
			}
		});
	}

	std::string FWorld::Report() const
	{
		int Acts[2][(int)EAct::Num] = {};
		int N[2] = {};
		for (const FAnimal& A : Animals)
		{
			Acts[(int)A.Kind][(int)A.Act]++;
			N[(int)A.Kind]++;
		}
		std::ostringstream S;
		S.precision(3);
		S << "hour " << Hour << " | cats " << N[0] << ":";
		for (int K = 0; K < (int)EAct::Num; K++)
		{
			if (Acts[0][K])
			{
				S << " " << ActName((EAct)K) << "=" << Acts[0][K];
			}
		}
		S << " | dogs " << N[1] << ":";
		for (int K = 0; K < (int)EAct::Num; K++)
		{
			if (Acts[1][K])
			{
				S << " " << ActName((EAct)K) << "=" << Acts[1][K];
			}
		}
		S << " | tiers " << TierCount[0] << "/" << TierCount[1] << "/" << TierCount[2] << "/" << TierCount[3];
		return S.str();
	}

	std::string FWorld::Describe(int Id) const
	{
		if (Id < 0 || Id >= (int)Animals.size())
		{
			return "no such animal";
		}
		const FAnimal& A = Animals[Id];
		char B[512];
		std::snprintf(B, sizeof(B), "%s %d (breed %d coat %d %s%s) %s goal %s at (%.1f %.1f %.1f) v %.2f | hunger %.2f tired %.2f play %.2f lonely %.2f curious %.2f fear %.2f trust %.2f",
			IsCat(A) ? "cat" : "dog", A.Id, A.Breed, A.Coat, A.bFemale ? "f" : "m", A.bYoung ? " young" : "", ActName(A.Act), GoalName(A.Goal),
			A.Pos.X, A.Pos.Y, A.Pos.Z, A.Speed, A.Hunger, A.Tired, A.Playful, A.Lonely, A.Curious, A.Fear, A.Trust);
		return B;
	}
}
