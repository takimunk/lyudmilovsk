// Children's games in the yards: the rules (YardGames.h, docs/yard_games.md).
#include "YardGames.h"
#include "MiniJson.h"

#include <algorithm>
#include <cmath>

namespace LyudYard
{
	namespace
	{
		constexpr float YPi = 3.14159265f;
		constexpr float Deg = YPi / 180.f;

		float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

		float AngleDelta(float From, float To)
		{
			float D = std::fmod(To - From + 540.f, 360.f) - 180.f;
			return D < -180.f ? D + 360.f : D;
		}

		FV2 Vec(const LyudJson::FValue& V) { return FV2(V[size_t(0)].F(), V[size_t(1)].F()); }

		/** Area frame: Along (the yaw), Across (90 degrees left of it). */
		FV2 Frame(const FV2& Origin, float Yaw, float Along, float Across)
		{
			const FV2 F = FV2::Dir(Yaw);
			const FV2 L(-F.Y, F.X);
			return Origin + F * Along + L * Across;
		}

		float RandRange(uint32_t& R, float Lo, float Hi) { return Lo + (Hi - Lo) * Rand(R); }
		int RandInt(uint32_t& R, int N) { return N <= 0 ? 0 : std::min(N - 1, (int)(Rand(R) * (float)N)); }
	}

	float Rand(uint32_t& State)
	{
		uint32_t X = State ? State : 0x9e3779b9u;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		State = X;
		return (float)(X >> 8) / 16777216.f;
	}

	float FV2::Len() const { return std::sqrt(X * X + Y * Y); }
	FV2 FV2::Norm() const
	{
		const float L = Len();
		return L > 1e-6f ? FV2(X / L, Y / L) : FV2(0.f, 0.f);
	}
	FV2 FV2::Dir(float YawDeg) { return FV2(std::cos(YawDeg * Deg), std::sin(YawDeg * Deg)); }
	float FV2::Yaw() const { return std::atan2(Y, X) / Deg; }

	const char* GameName(EGame G)
	{
		switch (G)
		{
		case EGame::Hopscotch: return "hopscotch";
		case EGame::Rope: return "rope";
		case EGame::Hide: return "hide";
		case EGame::Football: return "football";
		case EGame::Tag: return "tag";
		default: return "?";
		}
	}

	EGame GameFromName(const std::string& Name)
	{
		for (int I = 0; I < (int)EGame::Num; I++)
		{
			if (Name == GameName((EGame)I))
			{
				return (EGame)I;
			}
		}
		return EGame::Num;
	}

	const char* RoleName(ERole R)
	{
		static const char* const Names[] = { "none", "wait", "hopper", "turner", "jumper", "seeker", "hider", "found", "player", "keeper", "it", "runner" };
		return Names[(int)R];
	}

	const char* EventName(EEvent E)
	{
		static const char* const Names[] = { "start", "join", "leave", "roles", "found", "saved", "tagged", "goal", "miss", "swap", "round_end", "end" };
		return Names[(int)E];
	}

	FV2 HopRowCentre(const FArea& A, int Row)
	{
		return Frame(A.Chalk, A.ChalkYaw, HopCell * (0.5f + (float)Row), 0.f);
	}

	void RopePoint(const FArea& A, float Beat, float S, float& OutX, float& OutY, float& OutZ)
	{
		// the hands at the turners' height; between them the rope bows out by a half sine, turned round the line between the
		// hands by Beat (0: straight up, pi: down, its middle on the ground)
		const FV2 F = FV2::Dir(A.RopeYaw);
		const FV2 L(-F.Y, F.X);
		const float Bow = std::sin(YPi * S) * RopeHands;
		const FV2 P = A.Rope + F * (RopeHalf * (2.f * S - 1.f)) + L * (Bow * std::sin(Beat));
		OutX = P.X;
		OutY = P.Y;
		OutZ = RopeHands + Bow * std::cos(Beat);
	}

	FKid* FGame::Find(int Id)
	{
		for (FKid& K : Kids)
		{
			if (K.Id == Id)
			{
				return &K;
			}
		}
		return nullptr;
	}

	const FKid* FGame::Find(int Id) const
	{
		for (const FKid& K : Kids)
		{
			if (K.Id == Id)
			{
				return &K;
			}
		}
		return nullptr;
	}

	int FGame::NumIn(ERole R) const
	{
		int N = 0;
		for (const FKid& K : Kids)
		{
			N += K.Role == R ? 1 : 0;
		}
		return N;
	}

	bool FYard::Near(const FArea& A, const FV2& Pos, bool bDoing, bool bInItsGame)
	{
		const float D = (Pos - A.C).Len();
		if (bInItsGame)
		{
			return D < A.Catch + 25.f;
		}
		return D < A.Catch && (bDoing || D < A.R + 4.f);
	}

	int FYard::MinPlayers(EGame G)
	{
		switch (G)
		{
		case EGame::Hopscotch: return 2;
		case EGame::Rope: return 3;
		case EGame::Hide: return 3;
		case EGame::Football: return 4;
		default: return 3;
		}
	}

	int FYard::MaxPlayers(EGame G)
	{
		switch (G)
		{
		case EGame::Hopscotch: return 5;
		case EGame::Rope: return 7;
		case EGame::Hide: return 8;
		case EGame::Football: return 10;
		default: return 7;
		}
	}

	bool FYard::Load(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "yard_games.json: " + LastError;
			return false;
		}
		Areas.clear();
		for (const LyudJson::FValue& V : J["areas"].Items)
		{
			FArea A;
			A.Id = V["id"].S();
			A.Kind = V["kind"].S();
			A.C = Vec(V["c"]);
			A.Z = V["c"][size_t(2)].F();
			A.R = V["r"].F(10.f);
			A.Catch = V["catch"].F(25.f);
			A.Yaw = V["yaw"].F();
			for (const LyudJson::FValue& G : V["games"].Items)
			{
				const EGame E = GameFromName(G.S());
				if (E != EGame::Num)
				{
					A.Games |= 1u << (int)E;
				}
			}
			for (const LyudJson::FValue& C : V["cover"].Items)
			{
				A.Cover.push_back({ Vec(C["at"]), Vec(C["behind"]) });
			}
			A.Wall = Vec(V["wall"]);
			A.WallYaw = V["wall_yaw"].F();
			A.Chalk = Vec(V["chalk"]);
			A.ChalkYaw = V["chalk_yaw"].F();
			A.Rope = Vec(V["rope"]);
			A.RopeYaw = V["rope_yaw"].F();
			A.FieldHalfL = V["field"]["half"][size_t(0)].F(12.f);
			A.FieldHalfW = V["field"]["half"][size_t(1)].F(6.f);
			A.FieldYaw = V["field"]["yaw"].F(A.Yaw + 90.f);
			for (const LyudJson::FValue& P : V["places"].Items)
			{
				A.Places.push_back(P.S());
			}
			for (const LyudJson::FValue& K : V["kit"].Items)
			{
				FKit Kt;
				Kt.Kind = K["kind"].S();
				Kt.Species = K["species"].S();
				Kt.At = Vec(K["at"]);
				Kt.Yaw = K["yaw"].F();
				Kt.Age = K["age"].F();
				Kt.R = K["r"].F();
				Kt.Variant = K["variant"].I();
				Kt.Seed = K["seed"].I();
				A.Kit.push_back(Kt);
			}
			if (A.Id.empty() || A.Games == 0)
			{
				continue;
			}
			Areas.push_back(A);
		}
		Cooldown.assign(Areas.size(), 0.f);
		LastGame.assign(Areas.size(), -1);
		if (Areas.empty())
		{
			LastError = "yard_games.json: no areas";
			return false;
		}
		return true;
	}

	int FYard::FindArea(const std::string& Id) const
	{
		for (int I = 0; I < (int)Areas.size(); I++)
		{
			if (Areas[(size_t)I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	int FYard::GameAt(int Area) const
	{
		for (int I = 0; I < (int)Games.size(); I++)
		{
			if (Games[(size_t)I].Area == Area)
			{
				return I;
			}
		}
		return -1;
	}

	int FYard::InGame(int Id) const
	{
		for (int I = 0; I < (int)Games.size(); I++)
		{
			if (Games[(size_t)I].Find(Id))
			{
				return I;
			}
		}
		return -1;
	}

	void FYard::Emit(FGame& G, EEvent E, int Who, int Other)
	{
		FEvent Ev;
		Ev.Kind = E;
		Ev.Game = G.Uid;
		Ev.G = G.Kind;
		Ev.Area = G.Area;
		Ev.Who = Who;
		Ev.Other = Other;
		Events.push_back(Ev);
	}

	bool FYard::AddKid(FGame& G, const FCandidate& C)
	{
		if ((int)G.Kids.size() >= MaxPlayers(G.Kind) || G.Find(C.Id))
		{
			return false;
		}
		FKid K;
		K.Id = C.Id;
		K.Age = C.Age;
		K.Pos = C.Pos;
		K.Goal = C.Pos;
		K.Role = G.Phase == EPhase::Gather ? ERole::None : ERole::Wait;
		const FArea& A = Areas[(size_t)G.Area];
		K.Yaw = (A.C - C.Pos).Yaw();
		if (G.Phase == EPhase::Play)
		{
			switch (G.Kind)
			{
			case EGame::Football:
			{
				// onto the side with fewer
				int N0 = 0, N1 = 0;
				for (const FKid& O : G.Kids)
				{
					(O.Team == 0 ? N0 : N1)++;
				}
				K.Team = N0 <= N1 ? 0 : 1;
				K.Role = ERole::Player;
				break;
			}
			case EGame::Tag: K.Role = ERole::Runner; break;
			case EGame::Hide: K.bPending = true; break;
			case EGame::Rope: K.Phase = G.T; break;
			default: break;
			}
		}
		G.Kids.push_back(K);
		if (G.Phase != EPhase::Gather)
		{
			Emit(G, EEvent::Join, C.Id);
		}
		return true;
	}

	bool FYard::Start(int Area, EGame Kind, const std::vector<FCandidate>& Kids, float Length, bool bForced)
	{
		if (Area < 0 || Area >= (int)Areas.size() || !Areas[(size_t)Area].Fits(Kind) || GameAt(Area) >= 0)
		{
			return false;
		}
		std::vector<FCandidate> Free;
		for (const FCandidate& C : Kids)
		{
			if (InGame(C.Id) < 0)
			{
				Free.push_back(C);
			}
		}
		if ((int)Free.size() < MinPlayers(Kind))
		{
			return false;
		}
		FGame G;
		G.Uid = NextUid++;
		G.Kind = Kind;
		G.Area = Area;
		G.bForced = bForced;
		G.Rng = Seed ^ (uint32_t)(G.Uid * 2654435761u) ^ (uint32_t)(Area * 40503u);
		Rand(G.Rng);
		static const float Lo[] = { 8.f, 8.f, 10.f, 12.f, 8.f }, Hi[] = { 14.f, 14.f, 20.f, 20.f, 14.f };
		G.Length = Length > 0.f ? Length : 60.f * RandRange(G.Rng, Lo[(int)Kind], Hi[(int)Kind]);
		// the oldest leads (the first to say "let's play...")
		std::sort(Free.begin(), Free.end(), [](const FCandidate& A, const FCandidate& B) { return A.Age != B.Age ? A.Age > B.Age : A.Id < B.Id; });
		for (const FCandidate& C : Free)
		{
			AddKid(G, C);
		}
		G.Leader = G.Kids.front().Id;
		G.Phase = EPhase::Gather;
		for (int I = 0; I < (int)G.Kids.size(); I++)
		{
			FKid& K = G.Kids[(size_t)I];
			K.Goal = StartSpot(G, Areas[(size_t)Area], K, I);
		}
		LastGame[(size_t)Area] = (int)Kind;
		Games.push_back(G);
		Emit(Games.back(), EEvent::Start, Games.back().Leader);
		return true;
	}

	void FYard::Offer(int Area, const std::vector<FCandidate>& Kids)
	{
		if (Area < 0 || Area >= (int)Areas.size())
		{
			return;
		}
		const FArea& A = Areas[(size_t)Area];
		const int Gi = GameAt(Area);
		if (Gi >= 0)
		{
			FGame& G = Games[(size_t)Gi];
			if (G.bForced)
			{
				return;
			}
			for (FKid& K : G.Kids)
			{
				K.bPresent = K.Id == HeroId;
			}
			for (const FCandidate& C : Kids)
			{
				if (FKid* K = G.Find(C.Id))
				{
					K->bPresent = true;
				}
				else if (InGame(C.Id) < 0 && G.Phase != EPhase::Over)
				{
					AddKid(G, C);
				}
			}
			std::vector<int> Gone;
			for (const FKid& K : G.Kids)
			{
				if (!K.bPresent)
				{
					Gone.push_back(K.Id);
				}
			}
			for (int Id : Gone)
			{
				Leave(G, Id);
			}
			int Real = 0;
			for (const FKid& K : G.Kids)
			{
				Real += K.bPending ? 0 : 1;
			}
			if (Real < MinPlayers(G.Kind))
			{
				EndGame(Gi, 1);
			}
			return;
		}
		if (Cooldown[(size_t)Area] > 0.f || (int)Games.size() >= MaxGames)
		{
			return;
		}
		std::vector<FCandidate> Free;
		int Older = 0, Younger = 0;
		for (const FCandidate& C : Kids)
		{
			if (InGame(C.Id) < 0)
			{
				Free.push_back(C);
				Older += C.Age >= 10 ? 1 : 0;
				Younger += C.Age <= 7 ? 1 : 0;
			}
		}
		if ((int)Free.size() < 2)
		{
			return;
		}
		// what to play: whatever the area fits with this many, not what was played here last; football and hide-and-seek
		// more with the older ones, tag and hopscotch with the little ones
		float W[(int)EGame::Num] = {};
		float Sum = 0.f;
		for (int I = 0; I < (int)EGame::Num; I++)
		{
			const EGame E = (EGame)I;
			if (!A.Fits(E) || (int)Free.size() < MinPlayers(E))
			{
				continue;
			}
			float Wt = 1.f;
			if (E == EGame::Football) Wt = 0.6f + 0.25f * (float)Older;
			if (E == EGame::Hide) Wt = 1.1f + 0.1f * (float)Older;
			if (E == EGame::Tag) Wt = 0.8f + 0.2f * (float)Younger;
			// hopscotch is what two play; a bigger group would rather run about
			if (E == EGame::Hopscotch) Wt = Free.size() <= 2 ? 1.f : 0.35f + 0.1f * (float)Younger;
			if (LastGame[(size_t)Area] == I) Wt *= 0.25f;
			W[I] = Wt;
			Sum += Wt;
		}
		if (Sum <= 0.f)
		{
			return;
		}
		uint32_t R = Seed ^ (uint32_t)(NextUid * 7919u + Area * 104729u);
		float Pick = Rand(R) * Sum;
		for (int I = 0; I < (int)EGame::Num; I++)
		{
			if (W[I] <= 0.f)
			{
				continue;
			}
			Pick -= W[I];
			if (Pick <= 0.f)
			{
				Start(Area, (EGame)I, Free);
				return;
			}
		}
	}

	void FYard::Leave(FGame& G, int Id)
	{
		FKid* K = G.Find(Id);
		if (!K)
		{
			return;
		}
		const ERole Was = K->Role;
		G.Kids.erase(std::remove_if(G.Kids.begin(), G.Kids.end(), [Id](const FKid& O) { return O.Id == Id; }), G.Kids.end());
		Emit(G, EEvent::Leave, Id);
		if (G.Kids.empty())
		{
			return;
		}
		if (G.Leader == Id)
		{
			G.Leader = G.Kids.front().Id;
		}
		if (G.Phase != EPhase::Play)
		{
			return;
		}
		// whoever had a part nobody else can do hands it on
		switch (G.Kind)
		{
		case EGame::Hopscotch:
			if (G.Turn == Id) G.Turn = -1;
			break;
		case EGame::Rope:
			if (Was == ERole::Turner)
			{
				for (FKid& O : G.Kids)
				{
					if (O.Role == ERole::Wait || (O.Role == ERole::Jumper && G.NumIn(ERole::Wait) == 0))
					{
						O.Role = ERole::Turner;
						O.bArrived = false;
						break;
					}
				}
			}
			if (G.Turn == Id) G.Turn = -1;
			break;
		case EGame::Hide:
			if (Was == ERole::Seeker && !G.bHeroSeeks)
			{
				NewRound(G);
			}
			break;
		case EGame::Tag:
			if (G.It == Id)
			{
				G.It = G.Kids[(size_t)RandInt(G.Rng, (int)G.Kids.size())].Id;
				if (FKid* N = G.Find(G.It)) N->Role = ERole::It;
			}
			break;
		default: break;
		}
	}

	void FYard::EndGame(int Index, int Why)
	{
		if (Index < 0 || Index >= (int)Games.size())
		{
			return;
		}
		FGame& G = Games[(size_t)Index];
		Emit(G, EEvent::End, G.Leader, Why);
		Cooldown[(size_t)G.Area] = 60.f + 90.f * Rand(G.Rng);
		Games.erase(Games.begin() + Index);
	}

	void FYard::EndAll()
	{
		while (!Games.empty())
		{
			EndGame((int)Games.size() - 1, 2);
		}
	}

	FV2 FYard::StartSpot(const FGame& G, const FArea& A, const FKid& K, int Index) const
	{
		(void)K;
		switch (G.Kind)
		{
		case EGame::Hopscotch:
			// a row beside the squares' start, facing along them
			return Frame(A.Chalk, A.ChalkYaw, -0.7f - 0.15f * (float)(Index % 2), 1.0f + 0.65f * (float)Index);
		case EGame::Rope:
			return Frame(A.Rope, A.RopeYaw, -1.2f + 0.8f * (float)(Index % 4), 2.4f + 0.7f * (float)(Index / 4));
		case EGame::Hide:
			// round the counting wall
			return Frame(A.Wall, A.WallYaw + 180.f, 1.6f + 0.5f * (float)(Index / 4), -1.5f + 1.0f * (float)(Index % 4));
		case EGame::Football:
			return Frame(A.C, A.FieldYaw, (Index % 2 == 0 ? -1.f : 1.f) * (2.5f + 1.5f * (float)(Index / 2)), 0.8f * (float)((Index / 2) % 3 - 1));
		default:
		{
			const float Ang = A.Yaw + 360.f * (float)Index / (float)std::max<size_t>(1, G.Kids.size());
			return A.C + FV2::Dir(Ang) * 2.0f;
		}
		}
	}

	void FYard::Step(float Dt)
	{
		Dt = Clamp(Dt, 0.f, 0.25f);
		for (float& C : Cooldown)
		{
			C = std::max(0.f, C - Dt);
		}
		for (int I = 0; I < (int)Games.size(); I++)
		{
			FGame& G = Games[(size_t)I];
			if (FKid* H = G.Find(HeroId))
			{
				if (!bHeroHere)
				{
					Leave(G, HeroId);
				}
				else
				{
					H->Pos = HeroPos;
					H->Speed = HeroVel.Len();
					if (H->Speed > 0.3f) H->Yaw = HeroVel.Yaw();
				}
			}
			StepGame(G, Dt);
			int Real = 0;
			for (const FKid& K : G.Kids)
			{
				Real += K.bPending ? 0 : 1;
			}
			if (G.T >= G.Length && G.Phase == EPhase::Play)
			{
				// hide-and-seek finishes its round first
				if (G.Kind != EGame::Hide || G.Count < -900.f || G.T >= G.Length + 150.f)
				{
					EndGame(I--, 0);
					continue;
				}
			}
			if (Real < MinPlayers(G.Kind) && G.Phase == EPhase::Play)
			{
				EndGame(I--, 1);
			}
		}
	}

	void FYard::Begin(FGame& G)
	{
		G.Phase = EPhase::Play;
		G.PhaseT = 0.f;
		const FArea& A = Areas[(size_t)G.Area];
		const int N = (int)G.Kids.size();
		switch (G.Kind)
		{
		case EGame::Hopscotch:
			for (FKid& K : G.Kids) K.Role = ERole::Wait;
			G.Turn = -1;
			break;
		case EGame::Rope:
		{
			// the two youngest turn first (the big ones jump), everyone else waits
			std::vector<int> Order(N);
			for (int I = 0; I < N; I++) Order[I] = I;
			std::sort(Order.begin(), Order.end(), [&G](int X, int Y) { return G.Kids[(size_t)X].Age < G.Kids[(size_t)Y].Age; });
			for (int I = 0; I < N; I++)
			{
				G.Kids[(size_t)Order[I]].Role = I < 2 ? ERole::Turner : ERole::Wait;
				G.Kids[(size_t)Order[I]].bArrived = false;
				G.Kids[(size_t)Order[I]].Phase = (float)I;
			}
			G.Turn = -1;
			break;
		}
		case EGame::Hide:
			NewRound(G);
			break;
		case EGame::Football:
		{
			// picked in turns by age, so the sides are even; the last of each side is in goal when there are six or more
			std::vector<int> Order(N);
			for (int I = 0; I < N; I++) Order[I] = I;
			std::sort(Order.begin(), Order.end(), [&G](int X, int Y) { return G.Kids[(size_t)X].Age > G.Kids[(size_t)Y].Age; });
			for (int I = 0; I < N; I++)
			{
				FKid& K = G.Kids[(size_t)Order[I]];
				K.Team = (I % 4 == 0 || I % 4 == 3) ? 0 : 1;
				K.Role = ERole::Player;
			}
			if (N >= 6)
			{
				for (int T = 0; T < 2; T++)
				{
					for (int I = N - 1; I >= 0; I--)
					{
						FKid& K = G.Kids[(size_t)Order[I]];
						if (K.Team == T)
						{
							K.Role = ERole::Keeper;
							break;
						}
					}
				}
			}
			G.Ball = FBall();
			G.Ball.Pos = A.C;
			G.Kickoff = 2.f;
			break;
		}
		case EGame::Tag:
		{
			// counted out ("eeny, meeny..."): anybody but the leader
			int Pick = RandInt(G.Rng, N);
			if (G.Kids[(size_t)Pick].Id == G.Leader && N > 1) Pick = (Pick + 1) % N;
			for (FKid& K : G.Kids) K.Role = ERole::Runner;
			G.It = G.Kids[(size_t)Pick].Id;
			G.Kids[(size_t)Pick].Role = ERole::It;
			G.Kids[(size_t)Pick].Timer = 3.f;    // counts to three
			break;
		}
		default: break;
		}
		Emit(G, EEvent::Roles, G.Leader);
	}

	void FYard::NewRound(FGame& G)
	{
		const FArea& A = Areas[(size_t)G.Area];
		G.Round++;
		G.Visited.clear();
		G.SeekTarget = -1;
		for (FKid& K : G.Kids) K.bPending = false;
		// the next seeker: the first found last round, else (the first round, or the seeker left) counted out
		int Next = G.FirstFound;
		if (G.bHeroSeeks && G.Find(HeroId))
		{
			Next = HeroId;
		}
		if (Next == -1 || !G.Find(Next))
		{
			const int N = (int)G.Kids.size();
			int Pick = RandInt(G.Rng, N);
			if (G.Kids[(size_t)Pick].Id == G.Leader && N > 1) Pick = (Pick + 1) % N;
			if (G.Kids[(size_t)Pick].Id == HeroId && N > 1) Pick = (Pick + 1) % N;
			Next = G.Kids[(size_t)Pick].Id;
		}
		G.Seeker = Next;
		G.FirstFound = -1;
		G.Count = Next == HeroId ? 15.f : 18.f + 6.f * Rand(G.Rng);
		// the hiding places, the far ones first, shared out at random among the hiders
		std::vector<int> Spots;
		for (int I = 0; I < (int)A.Cover.size(); I++) Spots.push_back(I);
		for (int I = (int)Spots.size() - 1; I > 0; I--) std::swap(Spots[(size_t)I], Spots[(size_t)RandInt(G.Rng, I + 1)]);
		size_t S = 0;
		for (FKid& K : G.Kids)
		{
			K.Cover = -1;
			K.bArrived = false;
			K.Timer = 0.f;
			if (K.Id == G.Seeker)
			{
				K.Role = ERole::Seeker;
				K.Goal = A.Wall;
				continue;
			}
			K.Role = ERole::Hider;
			if (K.Id == HeroId)
			{
				continue;
			}
			if (S < Spots.size())
			{
				K.Cover = Spots[S++];
				K.Goal = A.Cover[(size_t)K.Cover].At;
			}
			else
			{
				// more hiders than places: behind the same thing as somebody else, a step aside
				const int C = Spots.empty() ? -1 : Spots[S % Spots.size()];
				K.Cover = C;
				K.Goal = C >= 0 ? A.Cover[(size_t)C].At + FV2::Dir(RandRange(G.Rng, 0.f, 360.f)) * 0.6f : A.C;
				S++;
			}
			K.Timer = RandRange(G.Rng, 0.f, 1.5f);   // a moment's dither before they run
		}
		Emit(G, EEvent::Roles, G.Seeker);
	}

	void FYard::Move(FGame& G, const FArea& A, FKid& K, float Speed, float Dt)
	{
		if (K.Id == HeroId)
		{
			return;
		}
		FV2 To = K.Goal - K.Pos;
		const float D = To.Len();
		float Want = D < 0.04f ? 0.f : std::min(Speed, D * 2.5f);
		// a little room between children, and round the yard's things
		FV2 Push;
		for (const FKid& O : G.Kids)
		{
			if (&O == &K) continue;
			const FV2 Off = K.Pos - O.Pos;
			const float L = Off.Len();
			if (L < 0.55f && L > 1e-4f) Push += Off * ((0.55f - L) / L * 2.f);
		}
		for (const FKit& Kt : A.Kit)
		{
			const float R = Kt.Kind == "sandpit" ? 1.9f : (Kt.Kind == "bench_park" ? 1.0f : (Kt.Kind == "flowerbed" ? 1.5f : 0.6f));
			const FV2 Off = K.Pos - Kt.At;
			const float L = Off.Len();
			if (L < R + 0.3f && L > 1e-4f) Push += Off * ((R + 0.3f - L) / L * 3.f);
		}
		FV2 V = (D > 1e-4f ? To * (1.f / D) : FV2()) * Want + Push;
		const float VL = V.Len();
		// speed up and slow down over a fraction of a second, not at once
		K.Speed += Clamp(VL - K.Speed, -9.f * Dt, 7.f * Dt);
		if (VL > 1e-4f)
		{
			K.Pos += V * (K.Speed / VL * Dt);
		}
		if (K.Speed > 0.3f && VL > 1e-4f)
		{
			K.Yaw += Clamp(AngleDelta(K.Yaw, V.Yaw()), -600.f * Dt, 600.f * Dt);
		}
		// never off the area (the drop at a terrace's edge, the road)
		const FV2 Out = K.Pos - A.C;
		const float Far = A.R + 7.f;
		if (Out.Len() > Far)
		{
			K.Pos = A.C + Out.Norm() * Far;
		}
		K.bArrived = D < 0.25f;
	}

	void FYard::StepGame(FGame& G, float Dt)
	{
		const FArea& A = Areas[(size_t)G.Area];
		G.T += Dt;
		G.PhaseT += Dt;
		for (FKid& K : G.Kids)
		{
			K.ActT += Dt;
			K.Lift = 0.f;
		}
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		if (G.Phase == EPhase::Gather)
		{
			bool bAll = true;
			for (int I = 0; I < (int)G.Kids.size(); I++)
			{
				FKid& K = G.Kids[(size_t)I];
				K.Goal = StartSpot(G, A, K, I);
				Move(G, A, K, 2.6f, Dt);
				SetAct(K, K.Speed > 1.8f ? EAct::Run : (K.Speed > 0.2f ? EAct::Walk : EAct::Stand));
				bAll = bAll && (K.bArrived || K.Id == HeroId);
			}
			if (bAll || G.PhaseT > 15.f)
			{
				Begin(G);
			}
			return;
		}
		switch (G.Kind)
		{
		case EGame::Hopscotch: StepHopscotch(G, A, Dt); break;
		case EGame::Rope: StepRope(G, A, Dt); break;
		case EGame::Hide: StepHide(G, A, Dt); break;
		case EGame::Football: StepFootball(G, A, Dt); break;
		case EGame::Tag: StepTag(G, A, Dt); break;
		default: break;
		}
	}

	// --- hopscotch: in turns up the squares and back on one foot (both feet astride the pairs), one more square each
	// good trip; a foot on a line ends the turn and the next one goes
	void FYard::StepHopscotch(FGame& G, const FArea& A, float Dt)
	{
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		if (G.Turn < 0 || !G.Find(G.Turn))
		{
			// the next in the queue: the one after the last hopper, round in the order they stand
			// (Round counts the turns: round the children in the order they joined)
			for (FKid& O : G.Kids)
			{
				if (O.Role == ERole::Hopper) O.Role = ERole::Wait;
			}
			const int Pick = G.Round % (int)G.Kids.size();
			G.Round = Pick + 1;
			FKid& N = G.Kids[(size_t)Pick];
			N.Role = ERole::Hopper;
			N.Phase = -1.f;     // walking to the start
			N.Timer = 0.f;
			G.Turn = N.Id;
		}
		int WaitIdx = 0;
		for (FKid& K : G.Kids)
		{
			if (K.Role == ERole::Hopper)
			{
				if (K.Id == HeroId)
				{
					continue;
				}
				if (K.Phase < 0.f)
				{
					K.Goal = Frame(A.Chalk, A.ChalkYaw, -0.45f, 0.f);
					Move(G, A, K, 1.4f, Dt);
					SetAct(K, K.Speed > 0.2f ? EAct::Walk : EAct::Stand);
					if (K.bArrived)
					{
						K.Phase = 0.f;
						K.Yaw = A.ChalkYaw;
					}
					continue;
				}
				// hops: up rows 0..6, turn on "home", down 5..0, off: 14 hops of 0.5 s; the trip is as long as the level
				const int Up = std::min(HopRows - 1, 2 + K.Level);
				const int Hops = 2 * Up + 2;
				K.Phase += Dt / 0.5f;
				const int Hop = (int)K.Phase;
				const float P = K.Phase - (float)Hop;
				if (Hop >= Hops)
				{
					K.Level++;
					SetAct(K, EAct::Cheer);
					for (FKid& O : G.Kids) if (O.Role == ERole::Wait) SetAct(O, EAct::Clap);
					if (K.Level > HopRows)
					{
						K.Level = 0;   // all the way: starts again from the first
					}
					K.Phase = -1.f;
					G.Turn = -1;
					K.Role = ERole::Wait;
					continue;
				}
				// where it lands: the row of this hop (up, then back), from the start line before the first square
				auto RowOf = [Up](int H) { return H <= Up ? H : (H <= 2 * Up ? 2 * Up - H : -1); };
				const int From = Hop == 0 ? -1 : RowOf(Hop - 1);
				const int To = RowOf(Hop);
				const FV2 PFrom = From < 0 ? Frame(A.Chalk, A.ChalkYaw, -0.45f, 0.f) : HopRowCentre(A, From);
				const FV2 PTo = To < 0 ? Frame(A.Chalk, A.ChalkYaw, -0.45f, 0.f) : HopRowCentre(A, To);
				K.Pos = PFrom + (PTo - PFrom) * P;
				K.Lift = 0.16f * std::sin(YPi * P);
				K.Speed = 1.0f;
				K.Yaw = Hop < Up ? A.ChalkYaw : A.ChalkYaw + 180.f;
				SetAct(K, EAct::Hop);
				// a foot on a line: once a hop, more for the little ones
				const float MissP = 0.035f + (K.Age < 7 ? 0.035f : 0.f);
				if (P - Dt / 0.5f < 0.f && Hop > 0 && Rand(G.Rng) < MissP)
				{
					Emit(G, EEvent::Miss, K.Id);
					K.Role = ERole::Wait;
					K.Phase = -1.f;
					G.Turn = -1;
				}
				continue;
			}
			// the queue: beside the squares, watching the hopper
			K.Goal = Frame(A.Chalk, A.ChalkYaw, -0.7f - 0.15f * (float)(WaitIdx % 2), 1.1f + 0.6f * (float)WaitIdx);
			WaitIdx++;
			Move(G, A, K, 1.4f, Dt);
			if (K.Speed > 0.2f)
			{
				SetAct(K, EAct::Walk);
			}
			else
			{
				if (const FKid* H = G.Find(G.Turn)) K.Yaw += Clamp(AngleDelta(K.Yaw, (H->Pos - K.Pos).Yaw()), -200.f * Dt, 200.f * Dt);
				if (K.Act != EAct::Clap || K.ActT > 2.5f) SetAct(K, EAct::Stand);
			}
		}
	}

	// --- the long rope: two turn it, one jumps in at a time, a jump a turn; who trips takes a turner's place
	void FYard::StepRope(FGame& G, const FArea& A, float Dt)
	{
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		const float Before = G.Beat;
		G.Beat = std::fmod(G.Beat + 2.f * YPi * RopeHz * Dt, 2.f * YPi);
		// the rope passed under the jumper's feet this frame (its bottom is at pi)
		const bool bPass = Before < YPi && G.Beat >= YPi;
		int TurnerIdx = 0, WaitIdx = 0;
		FKid* Jumper = nullptr;
		for (FKid& K : G.Kids) if (K.Role == ERole::Jumper) Jumper = &K;
		if (!Jumper && G.PhaseT > 2.f)
		{
			// the next in the queue runs in as the rope comes up
			// (a waiter's Phase: when they joined the queue)
			for (FKid& K : G.Kids)
			{
				if (K.Role == ERole::Wait && K.Id != HeroId && (!Jumper || K.Phase < Jumper->Phase)) Jumper = &K;
			}
			if (Jumper)
			{
				Jumper->Role = ERole::Jumper;
				Jumper->Timer = (float)(8 + RandInt(G.Rng, 18));    // jumps they mean to do
				Jumper->bArrived = false;
				G.Turn = Jumper->Id;
			}
		}
		for (FKid& K : G.Kids)
		{
			switch (K.Role)
			{
			case ERole::Turner:
			{
				const float Side = TurnerIdx++ == 0 ? -1.f : 1.f;
				K.Goal = Frame(A.Rope, A.RopeYaw, Side * (RopeHalf + 0.35f), 0.f);
				Move(G, A, K, 2.0f, Dt);
				if (K.bArrived || K.Speed < 0.2f)
				{
					K.Yaw += Clamp(AngleDelta(K.Yaw, (A.Rope - K.Pos).Yaw()), -400.f * Dt, 400.f * Dt);
					K.Phase = Side < 0.f ? G.Beat : -G.Beat;   // facing each other: the far one's hand goes round the other way
					SetAct(K, EAct::Turn);
				}
				else
				{
					SetAct(K, EAct::Walk);
				}
				break;
			}
			case ERole::Jumper:
			{
				K.Goal = A.Rope;
				if (!K.bArrived)
				{
					Move(G, A, K, 2.2f, Dt);
					SetAct(K, K.Speed > 0.2f ? EAct::Run : EAct::Stand);
					break;
				}
				K.Pos = A.Rope;
				K.Speed = 0.f;
				K.Yaw += Clamp(AngleDelta(K.Yaw, A.RopeYaw + 90.f), -400.f * Dt, 400.f * Dt);
				// airborne while the rope sweeps under: a hop round the bottom of its turn
				float Dist = std::fabs(G.Beat - YPi);
				if (Dist < 0.8f) K.Lift = 0.24f * std::cos(Dist / 0.8f * YPi * 0.5f);
				SetAct(K, EAct::Jump);
				if (bPass)
				{
					K.Timer -= 1.f;
					const float MissP = 0.025f + (K.Age < 7 ? 0.03f : 0.f);
					if (Rand(G.Rng) < MissP)
					{
						// tripped: takes the place of the turner who has turned longest
						FKid* Tired = nullptr;
						for (FKid& O : G.Kids) if (O.Role == ERole::Turner && O.Id != HeroId && (!Tired || O.ActT > Tired->ActT)) Tired = &O;
						Emit(G, EEvent::Miss, K.Id);
						if (Tired)
						{
							Emit(G, EEvent::Swap, K.Id, Tired->Id);
							Tired->Role = ERole::Wait;
							Tired->bArrived = false;
							Tired->Phase = G.T;
							K.Role = ERole::Turner;
							K.bArrived = false;
						}
						else
						{
							K.Role = ERole::Wait;
							K.Phase = G.T;
						}
						G.Turn = -1;
					}
					else if (K.Timer <= 0.f)
					{
						// enough: out the other side, to the back of the queue
						K.Role = ERole::Wait;
						K.bArrived = false;
						K.Phase = G.T;
						G.Turn = -1;
						for (FKid& O : G.Kids) if (O.Role == ERole::Wait && O.Id != K.Id) SetAct(O, EAct::Clap);
					}
				}
				break;
			}
			default:
			{
				K.Goal = Frame(A.Rope, A.RopeYaw, -1.0f + 0.75f * (float)(WaitIdx % 4), 2.4f + 0.7f * (float)(WaitIdx / 4));
				WaitIdx++;
				Move(G, A, K, 1.6f, Dt);
				if (K.Speed > 0.2f)
				{
					SetAct(K, EAct::Walk);
				}
				else
				{
					K.Yaw += Clamp(AngleDelta(K.Yaw, (A.Rope - K.Pos).Yaw()), -200.f * Dt, 200.f * Dt);
					if (K.Act != EAct::Clap || K.ActT > 2.5f) SetAct(K, EAct::Stand);
				}
				break;
			}
			}
		}
		// a turner gone (left the game): the queue's first steps in
		if (G.NumIn(ERole::Turner) < 2)
		{
			for (FKid& K : G.Kids)
			{
				if (K.Role == ERole::Wait)
				{
					K.Role = ERole::Turner;
					K.bArrived = false;
					break;
				}
			}
		}
	}

	// --- hide-and-seek: the seeker counts at the wall, the others hide behind the yard's things; then the seeker looks
	// behind them one after another; the found go to the wall; one who sees the seeker far off may run home free; the
	// first found seeks next round
	void FYard::StepHide(FGame& G, const FArea& A, float Dt)
	{
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		FKid* S = G.Find(G.Seeker);
		if (!S)
		{
			NewRound(G);
			return;
		}
		const bool bCounting = G.Count > 0.f;
		if (G.Count > -900.f)
		{
			G.Count -= Dt;
		}
		const float SeekTime = 100.f;
		// the round's state: counting (Count > 0), seeking (Count from 0 down to -SeekTime), between rounds (all found)
		int Left = 0;
		for (const FKid& K : G.Kids) Left += (K.Role == ERole::Hider) ? 1 : 0;
		if (!bCounting && (Left == 0 || G.Count < -SeekTime))
		{
			// the round is over; a short break by the wall, then the next
			if (G.Count > -1000.f)
			{
				Emit(G, EEvent::RoundEnd, G.FirstFound >= 0 ? G.FirstFound : G.Seeker, Left);
				G.Count = -1000.f;
				G.PhaseT = -5.f;
				for (FKid& K : G.Kids)
				{
					if (K.Role == ERole::Hider) K.Role = ERole::Found;   // they come out by themselves
				}
				if (G.FirstFound < 0) G.FirstFound = G.Seeker == HeroId && !G.bHeroSeeks ? -1 : G.Seeker;
			}
			for (int I = 0; I < (int)G.Kids.size(); I++)
			{
				FKid& K = G.Kids[(size_t)I];
				K.Goal = StartSpot(G, A, K, I);
				Move(G, A, K, 1.8f, Dt);
				SetAct(K, K.Speed > 0.2f ? EAct::Walk : EAct::Stand);
			}
			if (G.PhaseT >= 0.f)
			{
				if (G.T < G.Length)
				{
					G.PhaseT = 0.f;
					NewRound(G);
				}
			}
			return;
		}
		// the seeker
		if (S->Id != HeroId)
		{
			if (G.Count > 0.f)
			{
				S->Goal = A.Wall;
				Move(G, A, *S, 2.5f, Dt);
				if (S->bArrived)
				{
					S->Yaw += Clamp(AngleDelta(S->Yaw, A.WallYaw), -400.f * Dt, 400.f * Dt);
					SetAct(*S, EAct::Count);
				}
				else
				{
					SetAct(*S, EAct::Run);
				}
			}
			else
			{
				// pick the next place to look behind: the nearest not looked at yet, now and then one further off; the hero's
				// spot is looked at too once he's crouched somewhere
				auto Pick = [&]()
				{
					int Best = -1;
					float BestD = 1e9f;
					for (int C = 0; C < (int)A.Cover.size(); C++)
					{
						if (std::find(G.Visited.begin(), G.Visited.end(), C) != G.Visited.end()) continue;
						const float D = (A.Cover[(size_t)C].At - S->Pos).Len() + RandRange(G.Rng, 0.f, 6.f);
						if (D < BestD) { BestD = D; Best = C; }
					}
					const FKid* H = G.Find(HeroId);
					if (H && H->Role == ERole::Hider && (Best < 0 || Rand(G.Rng) < 0.2f)) return -2;
					return Best;
				};
				if (G.SeekTarget == -1)
				{
					G.SeekTarget = Pick();
					if (G.SeekTarget == -1)
					{
						G.Visited.clear();     // looked everywhere: again, maybe somebody moved
						G.SeekTarget = Pick();
					}
				}
				const FKid* H = G.Find(HeroId);
				if (G.SeekTarget == -2)
				{
					S->Goal = H ? H->Pos : A.C;
				}
				else if (G.SeekTarget >= 0)
				{
					S->Goal = A.Cover[(size_t)G.SeekTarget].At;
				}
				Move(G, A, *S, 2.3f, Dt);
				SetAct(*S, S->Speed > 1.8f ? EAct::Run : (S->Speed > 0.2f ? EAct::Walk : EAct::Look));
				if ((S->Goal - S->Pos).Len() < 1.2f || (G.SeekTarget == -2 && !H))
				{
					if (G.SeekTarget >= 0) G.Visited.push_back(G.SeekTarget);
					G.SeekTarget = -1;
				}
				// whoever is in sight near the seeker is found: behind the place looked at (2 m), or out in the open (6 m,
				// in front of them)
				for (FKid& K : G.Kids)
				{
					if (K.Role != ERole::Hider) continue;
					const FV2 Off = K.Pos - S->Pos;
					const float D = Off.Len();
					bool bSeen = D < 1.9f;
					if (!bSeen && D < 6.f)
					{
						const float Ahead = Off.Norm().Dot(FV2::Dir(S->Yaw));
						// in the open: not crouched behind a thing that is between them
						bool bCovered = false;
						for (const FCover& C : A.Cover)
						{
							if ((C.At - K.Pos).Len() < 1.2f)
							{
								const FV2 ToThing = C.Behind - S->Pos;
								bCovered = bCovered || (ToThing.Len() < D && ToThing.Norm().Dot(Off.Norm()) > 0.8f);
							}
						}
						bSeen = Ahead > 0.4f && !bCovered && (K.Id != HeroId || K.Speed > 0.4f || !bCovered);
					}
					if (bSeen)
					{
						K.Role = ERole::Found;
						SetAct(K, EAct::Stand);
						SetAct(*S, EAct::Point);
						if (G.FirstFound < 0) G.FirstFound = K.Id;
						Emit(G, EEvent::Found, K.Id, S->Id);
					}
				}
			}
		}
		else if (G.Count <= 0.f)
		{
			// the hero seeks: HeroFind does the finding; time runs out by itself
		}
		// the hiders
		for (int I = 0; I < (int)G.Kids.size(); I++)
		{
			FKid& K = G.Kids[(size_t)I];
			if (K.Id == HeroId || K.Id == S->Id) continue;
			if (K.bPending)
			{
				K.Goal = StartSpot(G, A, K, I);
				Move(G, A, K, 1.6f, Dt);
				SetAct(K, K.Speed > 0.2f ? EAct::Walk : EAct::Stand);
				continue;
			}
			if (K.Role == ERole::Found)
			{
				K.Goal = StartSpot(G, A, K, I);
				Move(G, A, K, 1.8f, Dt);
				SetAct(K, K.Speed > 0.2f ? EAct::Walk : EAct::Stand);
				if (K.bArrived) K.Yaw += Clamp(AngleDelta(K.Yaw, (S->Pos - K.Pos).Yaw()), -200.f * Dt, 200.f * Dt);
				continue;
			}
			if (K.Role != ERole::Hider) continue;
			if (K.Timer > 0.f)
			{
				K.Timer -= Dt;
				SetAct(K, EAct::Stand);
				continue;
			}
			if (K.Cover == -3)
			{
				// running home free
				K.Goal = A.Wall + FV2::Dir(A.WallYaw + 180.f) * 0.8f;
				Move(G, A, K, K.Fast(), Dt);
				SetAct(K, EAct::Run);
				if (K.bArrived)
				{
					K.Role = ERole::Found;
					Emit(G, EEvent::Saved, K.Id, S->Id);
				}
				continue;
			}
			Move(G, A, K, G.Count > 0.f ? 3.3f : 1.0f, Dt);
			if (K.bArrived)
			{
				// crouched, facing the thing between them and the wall
				if (K.Cover >= 0) K.Yaw += Clamp(AngleDelta(K.Yaw, (A.Cover[(size_t)K.Cover].Behind - K.Pos).Yaw()), -300.f * Dt, 300.f * Dt);
				SetAct(K, EAct::Crouch);
				// the seeker far off and turned away: a dash home free now and then
				if (G.Count < 0.f && S->Id != HeroId)
				{
					const float MeHome = (K.Pos - A.Wall).Len();
					const float SeekerHome = (S->Pos - A.Wall).Len();
					const float Away = (K.Pos - S->Pos).Norm().Dot(FV2::Dir(S->Yaw));
					if (SeekerHome > MeHome + 5.f && Away < 0.f && Rand(G.Rng) < 0.035f * Dt * 4.f) K.Cover = -3;
				}
			}
			else
			{
				SetAct(K, K.Speed > 1.8f ? EAct::Run : (K.Speed > 0.2f ? EAct::Walk : EAct::Stand));
			}
		}
	}

	// --- football: two sides, goals at the field's ends; the nearest of each side goes for the ball, the others spread
	// out between it and the goals; a kick at the goal or a pass; the ball rolls, slows on the ground and bounces off the
	// field's edge (the kerbs, the bushes)
	void FYard::StepFootball(FGame& G, const FArea& A, float Dt)
	{
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		const FV2 F = FV2::Dir(A.FieldYaw);
		const FV2 L(-F.Y, F.X);
		auto Local = [&](const FV2& P) { const FV2 D = P - A.C; return FV2(D.Dot(F), D.Dot(L)); };
		auto World = [&](float U, float V) { return A.C + F * U + L * V; };
		FBall& B = G.Ball;
		// the ball
		if (G.Kickoff > 0.f)
		{
			G.Kickoff -= Dt;
			B.Pos = A.C;
			B.Vel = FV2();
			B.Z = 0.f;
			B.VZ = 0.f;
		}
		else
		{
			const float Sp = B.Vel.Len();
			if (Sp > 1e-3f)
			{
				const float Slow = (B.Z > 0.02f ? 0.3f : 2.2f) * Dt;
				B.Vel = B.Vel * (std::max(0.f, Sp - Slow) / Sp);
			}
			B.Pos += B.Vel * Dt;
			B.Spin += B.Vel.Len() * Dt / 0.11f;
			B.VZ -= 9.8f * Dt;
			B.Z += B.VZ * Dt;
			if (B.Z < 0.f)
			{
				B.Z = 0.f;
				B.VZ = B.VZ < -1.f ? -B.VZ * 0.45f : 0.f;
			}
			FV2 P = Local(B.Pos);
			// a goal: over the end line between the posts, low enough
			if (std::fabs(P.X) > A.FieldHalfL && std::fabs(P.Y) < GoalHalf && B.Z < 1.2f)
			{
				const int Team = P.X > 0.f ? 0 : 1;     // side 0 attacks +X
				G.Score[Team]++;
				Emit(G, EEvent::Goal, B.LastKick, Team);
				for (FKid& K : G.Kids) if (K.Team == Team) SetAct(K, EAct::Cheer);
				G.Kickoff = 3.f;
				B.LastKick = -1;
			}
			else
			{
				// the edge: bounce back in
				const FV2 V(B.Vel.Dot(F), B.Vel.Dot(L));
				FV2 NV = V;
				if (std::fabs(P.X) > A.FieldHalfL + 0.3f) { NV.X = -V.X * 0.55f; P.X = (P.X > 0.f ? 1.f : -1.f) * (A.FieldHalfL + 0.3f); }
				if (std::fabs(P.Y) > A.FieldHalfW + 0.5f) { NV.Y = -V.Y * 0.55f; P.Y = (P.Y > 0.f ? 1.f : -1.f) * (A.FieldHalfW + 0.5f); }
				B.Pos = World(P.X, P.Y);
				B.Vel = F * NV.X + L * NV.Y;
			}
		}
		// the hero can kick it too: running into it sends it his way
		if (bHeroHere && (HeroPos - B.Pos).Len() < 0.55f && HeroVel.Len() > 0.8f && G.Kickoff <= 0.f)
		{
			B.Vel = HeroVel.Norm() * (HeroVel.Len() * 1.8f + 3.f);
			B.VZ = 0.8f;
			B.LastKick = HeroId;
		}
		// the nearest of each side chases
		int Chaser[2] = { -1, -1 };
		float Best[2] = { 1e9f, 1e9f };
		for (int I = 0; I < (int)G.Kids.size(); I++)
		{
			const FKid& K = G.Kids[(size_t)I];
			if (K.Id == HeroId || K.Role == ERole::Keeper) continue;
			const float D = (K.Pos - B.Pos).Len();
			if (D < Best[K.Team]) { Best[K.Team] = D; Chaser[K.Team] = I; }
		}
		const FV2 BL = Local(B.Pos);
		for (int I = 0; I < (int)G.Kids.size(); I++)
		{
			FKid& K = G.Kids[(size_t)I];
			if (K.Id == HeroId) continue;
			const float Dir = K.Team == 0 ? 1.f : -1.f;     // the way this side attacks, along the field
			K.Timer = std::max(0.f, K.Timer - Dt);
			float Speed = K.Fast() * 0.9f;
			if (G.Kickoff > 0.f)
			{
				// back to their own half
				K.Goal = World(-Dir * (2.f + 1.2f * (float)(I / 2)), 1.6f * (float)((I / 2) % 3 - 1));
				Speed = 2.f;
			}
			else if (K.Role == ERole::Keeper)
			{
				// on the line, across with the ball
				K.Goal = World(-Dir * (A.FieldHalfL - 0.6f), Clamp(BL.Y * 0.6f, -GoalHalf, GoalHalf));
				Speed = 2.6f;
				if ((K.Pos - B.Pos).Len() < 1.0f && B.Z < 1.1f && K.Timer <= 0.f)
				{
					B.Vel = (World(0.f, RandRange(G.Rng, -A.FieldHalfW, A.FieldHalfW)) - K.Pos).Norm() * RandRange(G.Rng, 7.f, 10.f);
					B.VZ = RandRange(G.Rng, 1.f, 3.5f);
					B.LastKick = K.Id;
					K.Timer = 1.f;
					SetAct(K, EAct::Kick);
				}
			}
			else if (I == Chaser[K.Team])
			{
				// to the ball, coming at it from behind (its own goal's side)
				K.Goal = B.Pos + F * (-Dir * 0.35f);
				if ((K.Pos - B.Pos).Len() < 0.65f && B.Z < 0.35f && K.Timer <= 0.f)
				{
					// a shot when near enough to the goal, else a pass ahead to somebody of their side, else a run with it
					const FV2 Goal = World(Dir * (A.FieldHalfL + 0.5f), RandRange(G.Rng, -GoalHalf * 0.9f, GoalHalf * 0.9f));
					FV2 To = Goal;
					float Power = RandRange(G.Rng, 7.f, 10.f);
					const float ToGoal = (Goal - K.Pos).Len();
					if (ToGoal > 8.f)
					{
						const FKid* Mate = nullptr;
						for (const FKid& O : G.Kids)
						{
							if (&O == &K || O.Team != K.Team || O.Role == ERole::Keeper) continue;
							if ((Local(O.Pos).X - Local(K.Pos).X) * Dir > 1.5f && (!Mate || Rand(G.Rng) < 0.5f)) Mate = &O;
						}
						if (Mate && Rand(G.Rng) < 0.55f)
						{
							To = Mate->Pos + F * (Dir * 1.0f);
							Power = std::min(9.f, 2.5f + (To - K.Pos).Len() * 0.9f);
						}
						else
						{
							// a dribble: a little tap ahead
							To = K.Pos + (Goal - K.Pos).Norm() * 3.f;
							Power = RandRange(G.Rng, 3.f, 4.5f);
						}
					}
					// aimed, but not perfectly
					const float Err = RandRange(G.Rng, -20.f, 20.f) * (K.Age < 8 ? 1.5f : 1.f);
					B.Vel = FV2::Dir((To - K.Pos).Yaw() + Err) * Power;
					B.VZ = Power > 6.f ? RandRange(G.Rng, 0.f, 2.f) : 0.f;
					B.LastKick = K.Id;
					K.Timer = 0.6f;
					SetAct(K, EAct::Kick);
				}
			}
			else
			{
				// spread out: forwards ahead of the ball, the others behind it, each in their own lane
				const float Lane = (float)((I * 7) % 3 - 1) * (A.FieldHalfW * 0.6f);
				const float Ahead = (I % 2 == 0 ? 3.5f : -3.f) * Dir;
				K.Goal = World(Clamp(BL.X + Ahead, -A.FieldHalfL + 1.f, A.FieldHalfL - 1.f), Lane);
				Speed = K.Fast() * 0.55f;
			}
			if (K.Act == EAct::Kick && K.ActT < 0.35f) { Move(G, A, K, 1.f, Dt); continue; }
			if (K.Act == EAct::Cheer && K.ActT < 2.f) { Move(G, A, K, 0.5f, Dt); continue; }
			Move(G, A, K, Speed, Dt);
			SetAct(K, K.Speed > 1.8f ? EAct::Run : (K.Speed > 0.2f ? EAct::Walk : EAct::Stand));
			if (K.Speed < 0.2f) K.Yaw += Clamp(AngleDelta(K.Yaw, (B.Pos - K.Pos).Yaw()), -300.f * Dt, 300.f * Dt);
		}
	}

	// --- tag: it chases the nearest, the others run from it inside the area; a touch makes the touched one it, who
	// counts to three before chasing, and can't tag the one who tagged them straight back
	void FYard::StepTag(FGame& G, const FArea& A, float Dt)
	{
		auto SetAct = [](FKid& K, EAct Act) { if (K.Act != Act) { K.Act = Act; K.ActT = 0.f; } };
		G.Immune = std::max(0.f, G.Immune - Dt);
		FKid* It = G.Find(G.It);
		if (!It)
		{
			if (G.Kids.empty()) return;
			It = &G.Kids[(size_t)RandInt(G.Rng, (int)G.Kids.size())];
			G.It = It->Id;
			It->Role = ERole::It;
		}
		const float Room = A.R + 3.f;
		// it
		if (It->Id != HeroId)
		{
			It->Timer = std::max(0.f, It->Timer - Dt);
			if (It->Timer > 0.f)
			{
				SetAct(*It, EAct::Count);
				It->Speed = 0.f;
			}
			else
			{
				// after the nearest it may tag (not the one who just tagged it); after a while on one it tries another
				// (G.Turn: who it is after, Beat: since when)
				FKid* Prey = nullptr;
				float BestD = 1e9f;
				const bool bBored = G.T - G.Beat > 7.f;
				for (FKid& K : G.Kids)
				{
					if (&K == It || (K.Id == G.LastIt && G.Immune > 0.f)) continue;
					const float D = (K.Pos - It->Pos).Len() + (K.Id == HeroId ? -1.f : 0.f) + (bBored && K.Id == G.Turn ? 6.f : 0.f)
						- (1.f - K.Breath) * 2.f;
					if (D < BestD) { BestD = D; Prey = &K; }
				}
				if (Prey && Prey->Id != G.Turn)
				{
					G.Turn = Prey->Id;
					G.Beat = G.T;
				}
				if (Prey) BestD = (Prey->Pos - It->Pos).Len();
				if (Prey)
				{
					// aim a little ahead of where they run
					// (past them: it doesn't slow down as it gets there)
					It->Goal = Prey->Pos + FV2::Dir(Prey->Yaw) * (std::min(BestD, 4.f) * 0.25f * Prey->Speed / 3.f) + (Prey->Pos - It->Pos).Norm() * 2.f;
					Move(G, A, *It, It->Fast() * 1.02f, Dt);
					SetAct(*It, BestD < 1.6f ? EAct::Reach : EAct::Run);
					if (BestD < 0.9f)
					{
						Emit(G, EEvent::Tagged, Prey->Id, It->Id);
						It->Role = ERole::Runner;
						G.LastIt = It->Id;
						G.Immune = 3.f;
						Prey->Role = ERole::It;
						Prey->Timer = Prey->Id == HeroId ? 0.f : 2.f;
						G.It = Prey->Id;
						It = Prey;
					}
				}
			}
		}
		// the runners
		for (FKid& K : G.Kids)
		{
			if (&K == It || K.Id == HeroId) continue;
			const FV2 Away = K.Pos - It->Pos;
			const float D = Away.Len();
			FV2 Want;
			float Speed;
			if (D < 6.f || (K.Id == G.LastIt && G.Immune > 0.f))
			{
				// away from it, bending back in toward the middle near the edge
				const FV2 In = (A.C - K.Pos) * (1.f / Room);
				Want = Away.Norm() + In * (In.Len() > 0.75f ? 2.5f : 0.6f);
				Speed = K.Fast() * (D < 3.f ? 0.98f : 0.8f) * (0.72f + 0.28f * K.Breath);
				K.Breath = std::max(0.f, K.Breath - (D < 3.f ? 0.1f : 0.04f) * Dt);
			}
			else
			{
				// it is far: a jog about, keeping an eye on it
				if (K.Timer <= 0.f || (K.Goal - K.Pos).Len() < 0.5f)
				{
					K.Goal = A.C + FV2::Dir(RandRange(G.Rng, 0.f, 360.f)) * RandRange(G.Rng, 1.f, A.R);
					K.Timer = RandRange(G.Rng, 1.5f, 4.f);
				}
				K.Timer -= Dt;
				Want = K.Goal - K.Pos;
				Speed = 1.4f;
				K.Breath = std::min(1.f, K.Breath + 0.2f * Dt);
			}
			if (D < 6.f) K.Goal = K.Pos + Want.Norm() * 3.f;
			Move(G, A, K, Speed, Dt);
			SetAct(K, K.Speed > 1.8f ? EAct::Run : (K.Speed > 0.2f ? EAct::Walk : EAct::Stand));
		}
	}

	// --- the hero

	bool FYard::HeroJoin(int Index)
	{
		if (Index < 0 || Index >= (int)Games.size() || InGame(HeroId) >= 0)
		{
			return false;
		}
		FGame& G = Games[(size_t)Index];
		if (G.Kind != EGame::Hide || G.Phase == EPhase::Over)
		{
			return false;
		}
		FKid H;
		H.Id = HeroId;
		H.Age = 15;
		H.Pos = HeroPos;
		// straight in while they are still counting (or gathering), else from the next round
		const bool bNow = G.Phase == EPhase::Gather || G.Count > 3.f;
		H.Role = bNow && G.Phase == EPhase::Play ? ERole::Hider : ERole::Wait;
		H.bPending = !bNow;
		G.Kids.push_back(H);
		Emit(G, EEvent::Join, HeroId);
		return true;
	}

	bool FYard::HeroSeek(int Index)
	{
		if (Index < 0 || Index >= (int)Games.size() || Games[(size_t)Index].Kind != EGame::Hide)
		{
			return false;
		}
		FGame& G = Games[(size_t)Index];
		if (!G.Find(HeroId))
		{
			FKid H;
			H.Id = HeroId;
			H.Age = 15;
			H.Pos = HeroPos;
			G.Kids.push_back(H);
			Emit(G, EEvent::Join, HeroId);
		}
		G.bHeroSeeks = true;
		if (G.Phase == EPhase::Play)
		{
			G.FirstFound = HeroId;
			G.Count = -1000.f;
			for (FKid& K : G.Kids) if (K.Role == ERole::Hider) K.Role = ERole::Found;
			G.PhaseT = 0.f;
			NewRound(G);
		}
		return true;
	}

	int FYard::HeroFind()
	{
		const int Gi = InGame(HeroId);
		if (Gi < 0)
		{
			return -1;
		}
		FGame& G = Games[(size_t)Gi];
		if (G.Kind != EGame::Hide || G.Seeker != HeroId || G.Count > 0.f || G.Count < -900.f)
		{
			return -1;
		}
		FKid* Best = nullptr;
		float BestD = 2.6f;
		for (FKid& K : G.Kids)
		{
			if (K.Role != ERole::Hider) continue;
			const float D = (K.Pos - HeroPos).Len();
			if (D < BestD) { BestD = D; Best = &K; }
		}
		if (!Best)
		{
			return -1;
		}
		Best->Role = ERole::Found;
		Best->Act = EAct::Stand;
		Best->ActT = 0.f;
		if (G.FirstFound < 0) G.FirstFound = Best->Id;
		Emit(G, EEvent::Found, Best->Id, HeroId);
		// the next round's seeker is the first found, not the hero again
		G.bHeroSeeks = false;
		return Best->Id;
	}

	void FYard::HeroLeave()
	{
		const int Gi = InGame(HeroId);
		if (Gi >= 0)
		{
			Games[(size_t)Gi].bHeroSeeks = false;
			Leave(Games[(size_t)Gi], HeroId);
		}
	}
}
