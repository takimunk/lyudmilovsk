#include "PeopleSim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace LyudPeople
{
	namespace
	{
		constexpr float HourS = 3600.f;
		constexpr float SitSeconds = 2.4f;      // the SitDown / StandUp clips
		constexpr float LieSeconds = 3.f;       // LieDown / GetUp

		/** How much each need grows by itself per hour awake (Tired: per hour awake; while asleep it is the sleep's). */
		const float Grow[NumNeeds] = { 0.17f, 0.062f, 0.08f, 0.09f, 0.035f, 0.035f, 0.05f };

		float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

		std::string ReadFile(const std::string& Path)
		{
			std::ifstream F(Path, std::ios::binary);
			if (!F)
			{
				return std::string();
			}
			std::stringstream SS;
			SS << F.rdbuf();
			return SS.str();
		}

		V3 Vec(const LyudJson::FValue& V) { return V3(V[0].F(), V[1].F(), V[2].F()); }

		std::vector<V3> Line(const LyudJson::FValue& V)
		{
			std::vector<V3> Out;
			for (const LyudJson::FValue& P : V.Items)
			{
				Out.push_back(Vec(P));
			}
			return Out;
		}

		bool Same(const V3& A, const V3& B) { return (A - B).Len() < 0.02f; }

		float YawTo(const V3& From, const V3& To)
		{
			return std::atan2(To.Y - From.Y, To.X - From.X) * 57.29578f;
		}

		float WrapYaw(float D)
		{
			while (D > 180.f)
			{
				D -= 360.f;
			}
			while (D < -180.f)
			{
				D += 360.f;
			}
			return D;
		}

		bool Has(const std::vector<std::string>& V, const std::string& S)
		{
			return std::find(V.begin(), V.end(), S) != V.end();
		}
	}

	static const char* NeedNames[] = { "hunger", "tired", "social", "fun", "nature", "mind", "sun" };
	static const char* PoseNames[] = { "hidden", "stand", "walk", "talk", "look", "sit", "computer", "eat", "read", "sleep", "work", "shovel" };
	static const char* StateNames[] = { "idle", "going", "riding", "settling", "doing", "leaving", "away", "waiting" };
	static const char* EventNames[] = { "Start", "Arrive", "Leave", "TalkStart", "TalkEnd", "Ride", "Away", "Back" };

	const char* NeedName(ENeed N) { return N < ENeed::Num ? NeedNames[(int)N] : "?"; }
	const char* PoseName(EPose P) { return P < EPose::Num ? PoseNames[(int)P] : "?"; }
	const char* StateName(EState S) { return S < EState::Num ? StateNames[(int)S] : "?"; }
	const char* EventName(EEvent E) { return E < EEvent::Num ? EventNames[(int)E] : "?"; }

	ENeed NeedFromName(const std::string& S)
	{
		for (int I = 0; I < NumNeeds; I++)
		{
			if (S == NeedNames[I])
			{
				return (ENeed)I;
			}
		}
		return ENeed::Num;
	}

	EPose PoseFromName(const std::string& S)
	{
		for (int I = 0; I < (int)EPose::Num; I++)
		{
			if (S == PoseNames[I])
			{
				return (EPose)I;
			}
		}
		return EPose::Stand;
	}

	bool FSlot::Free(int Person) const
	{
		if (std::find(Users.begin(), Users.end(), Person) != Users.end())
		{
			return true;
		}
		return (int)Users.size() < Capacity;
	}

	// --- loading ------------------------------------------------------------------------------------------

	bool FWorld::Load(const std::string& Dir, const std::string& AnimalsDir)
	{
		if (!AnimalsDir.empty())
		{
			bHasGround = Ground.Load(AnimalsDir);
			if (!bHasGround)
			{
				LastError = "walk grid: " + Ground.LastError;
				return false;
			}
		}
		const std::string Places_ = ReadFile(Dir + "/people_places.json");
		const std::string Acts = ReadFile(Dir + "/activities.json");
		const std::string Folk = ReadFile(Dir + "/people.json");
		if (Places_.empty() || Acts.empty() || Folk.empty())
		{
			LastError = "missing people_places.json, activities.json or people.json in " + Dir;
			return false;
		}
		if (!(LoadPlaces(Places_) && LoadActivities(Acts) && LoadPeople(Folk)))
		{
			return false;
		}
		// the town (tools/town/town_life.py): its buildings and spots, and the people who live in them
		const std::string TownPlaces = bLoadTown ? ReadFile(Dir + "/town_places.json") : std::string();
		const std::string Townsfolk = bLoadTown ? ReadFile(Dir + "/townsfolk.json") : std::string();
		if (!TownPlaces.empty() && !LoadTownPlaces(TownPlaces))
		{
			return false;
		}
		if (!Townsfolk.empty() && !LoadTownsfolk(Townsfolk))
		{
			return false;
		}
		Index();
		return true;
	}

	bool FWorld::LoadActivities(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "activities.json: " + LastError;
			return false;
		}
		Activities.clear();
		for (const LyudJson::FValue& V : J["activities"].Items)
		{
			FActivity A;
			A.Id = V["id"].S();
			for (const LyudJson::FValue& S : V["places"].Items)
			{
				A.Places.push_back(S.S());
			}
			for (const LyudJson::FValue& S : V["slots"].Items)
			{
				A.Slots.push_back(S.S());
			}
			A.Pose = PoseFromName(V["pose"].S("stand"));
			A.MinMinutes = V["minutes"][0].F(20.f);
			A.MaxMinutes = V["minutes"][1].F(A.MinMinutes);
			A.From = V["hours"][0].F(0.f);
			A.To = V["hours"][1].F(24.f);
			A.MinAge = V["ages"][0].I(0);
			A.MaxAge = V["ages"][1].I(200);
			for (const auto& F : V["meets"].Fields)
			{
				const ENeed N = NeedFromName(F.first);
				if (N != ENeed::Num)
				{
					A.Need[(int)N] = F.second.F();
				}
			}
			for (const auto& F : V["costs"].Fields)
			{
				const ENeed N = NeedFromName(F.first);
				if (N != ENeed::Num)
				{
					A.Cost[(int)N] = F.second.F();
				}
			}
			A.Base = V["base"].F(0.f);
			A.Trait = V["trait"].S();
			A.bDaylight = V["daylight"].F() > 0.f;
			A.bSunset = V["sunset"].F() > 0.f;
			A.bTalk = V["talk"].F() > 0.f;
			A.bWander = V["wander"].F() > 0.f;
			A.bSnow = V["snow"].F() > 0.f;
			A.bSunrise = V["sunrise"].F() > 0.f;
			A.bHolidayOff = V["holiday"].S() == "off";
			A.bGather = V["gather"].F() > 0.f;
			A.bStaff = V["staff"].F() > 0.f;
			if (A.bGather)
			{
				const std::string M = V["mode"].S("mingle");
				A.GatherMode = M == "watch" ? (int)EGatherMode::Watch : M == "sit" ? (int)EGatherMode::Sit : M == "fair" ? (int)EGatherMode::Fair : (int)EGatherMode::Mingle;
			}
			if (V["wander_m"].Items.size() == 2)
			{
				A.WanderNear = V["wander_m"].Items[0].F();
				A.WanderFar = V["wander_m"].Items[1].F();
			}
			if (V["wander_pause"].Items.size() == 2)
			{
				A.PauseMin = V["wander_pause"].Items[0].F();
				A.PauseMax = V["wander_pause"].Items[1].F();
				A.bPauseSet = true;
			}
			A.bAway = V["away"].F() > 0.f;
			A.bFree = V.Find("free") ? V["free"].F() > 0.f : true;
			if (V["days"].Size() > 0)
			{
				A.Days = 0;
				for (const LyudJson::FValue& Day : V["days"].Items)
				{
					A.Days |= 1 << (Day.I() % 7);
				}
			}
			if (!A.Id.empty())
			{
				Activities.push_back(A);
			}
		}
		if (FindActivity("sleep") < 0)
		{
			LastError = "activities.json has no sleep";
			return false;
		}
		return true;
	}

	bool FWorld::LoadPeople(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "people.json: " + LastError;
			return false;
		}
		People.clear();
		return AddPeople(J, false);
	}

	bool FWorld::LoadTownsfolk(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "townsfolk.json: " + LastError;
			return false;
		}
		const bool bOk = AddPeople(J, true);
		Index();
		return bOk;
	}

	bool FWorld::AddPeople(const LyudJson::FValue& J, bool bTown)
	{
		for (const LyudJson::FValue& V : J["people"].Items)
		{
			FPerson P;
			P.bTown = bTown || V["town"].F() > 0.f;
			P.Chronotype = V["chronotype"].S();
			P.HomeId = V["home"].S();
			P.Spread = V["spread"].F(0.f);
			P.Id = (int)People.size();
			P.Key = V["key"].S();
			P.Mesh = V["mesh"].S();
			P.Body = V["body"].S(P.Mesh);
			P.ComputerClip = V["computer_clip"].S("Sit_KeyboardMouse");
			P.Occupation = V["occupation"].S();
			P.Household = V["household"].S(P.Key);
			P.Age = V["age"].I(30);
			P.bFemale = V["female"].F() > 0.f;
			P.bChild = P.Age < 16;
			P.WalkSpeed = V["walk_speed"].F(P.bChild ? 1.35f : 1.25f);
			const LyudJson::FValue& T = V["temper"];
			P.Social = T["social"].F(0.5f);
			P.Active = T["active"].F(0.5f);
			P.Studious = T["studious"].F(0.5f);
			P.Gamer = T["gamer"].F(0.5f);
			P.NatureLove = T["nature"].F(0.5f);
			P.SunLove = T["sun"].F(0.5f);
			P.Wake = V["wake"].F(7.5f);
			P.BedTime = V["bed"].F(23.f);
			for (const LyudJson::FValue& D : V["duties"].Items)
			{
				FDuty Duty;
				Duty.Activity = D["activity"].S();
				Duty.PlaceId = D["place"].S();
				Duty.Role = D["role"].S();
				Duty.From = D["hours"][0].F(9.f);
				Duty.To = D["hours"][1].F(17.f);
				Duty.Days = 0;
				for (const LyudJson::FValue& Day : D["days"].Items)
				{
					Duty.Days |= 1 << (Day.I() % 7);
				}
				if (D["days"].IsNull())
				{
					Duty.Days = 0x7F;
				}
				P.Duties.push_back(Duty);
			}
			P.ClubSeat = V["club_seat"].S();
			P.OwnSlot = V["own_slot"].S();
			P.StartActivity = V["start"]["activity"].S();
			P.StartMinutes[0] = V["start"]["minutes"][0].F(60.f);
			P.StartMinutes[1] = V["start"]["minutes"][1].F(P.StartMinutes[0] * 2.f);
			for (const LyudJson::FValue& F : V["friends"].Items)
			{
				P.Friends.push_back(F.S());
			}
			for (const auto& L : V["likes"].Fields)
			{
				P.Likes.emplace_back(L.first, L.second.F());
			}
			P.Rng = Opt.Seed ^ (uint32_t)(P.Id * 2654435761u) ^ 0x9E3779B9u;
			if (P.Rng == 0)
			{
				P.Rng = 1;
			}
			for (int N = 0; N < NumNeeds; N++)
			{
				P.N[N] = 0.2f + 0.3f * Rand(P.Rng);
			}
			if (!P.Key.empty())
			{
				People.push_back(P);
			}
		}
		return true;
	}

	bool FWorld::LoadPlaces(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "people_places.json: " + LastError;
			return false;
		}
		Places.clear();
		Slots.clear();
		// a slot (or one per sitter of a sofa or a bench: "spots"); indoors its way in is the place's prefix (stairs,
		// the flat's door; the library's door; the club's entry) and its own path from there (tools/people_map.py)
		auto AddSlot = [&](FPlace& Pl, int PlaceIdx, const LyudJson::FValue& S, const std::vector<V3>& Prefix, const std::string& DefaultKind)
		{
			FSlot Sl;
			Sl.Kind = S["kind"].S(DefaultKind);
			Sl.Place = PlaceIdx;
			Sl.Pos = Vec(S["pos"]);
			Sl.Yaw = S["yaw"].F();
			Sl.Approach = S.Find("approach") ? Vec(S["approach"]) : Sl.Pos;
			Sl.Height = S["height"].F();
			Sl.Capacity = std::max(1, S["capacity"].I(1));
			Sl.Tag = S["tag"].S();
			if (Pl.bIndoor)
			{
				Sl.Inside = Prefix;
				for (const V3& P : Line(S["path"]))
				{
					if (Sl.Inside.empty() || !Same(Sl.Inside.back(), P))
					{
						Sl.Inside.push_back(P);
					}
				}
				if (Sl.Inside.empty() || !Same(Sl.Inside.back(), Sl.Approach))
				{
					Sl.Inside.push_back(Sl.Approach);
				}
			}
			const LyudJson::FValue& Spots = S["spots"];
			if (Sl.Kind == "bed")
			{
				for (const LyudJson::FValue& Spot : Spots.Items)
				{
					Sl.Spots.push_back(Vec(Spot));
				}
			}
			if (Spots.Size() > 1 && Sl.Kind != "bed")
			{
				// one slot per sitter, side by side, each with the same way in (a double bed stays one: they share it)
				for (const LyudJson::FValue& Spot : Spots.Items)
				{
					FSlot One = Sl;
					One.Pos = Vec(Spot);
					One.Capacity = 1;
					if (!Pl.bIndoor)
					{
						One.Approach = One.Pos + (Sl.Approach - Sl.Pos);
					}
					Pl.Slots.push_back((int)Slots.size());
					Slots.push_back(One);
				}
				return;
			}
			Pl.Slots.push_back((int)Slots.size());
			Slots.push_back(Sl);
		};

		// stairs: the way from the street door up to each floor's landing
		struct FStair { std::string Id; V3 Street; std::vector<std::vector<V3>> Floors; };
		std::vector<FStair> Stairs;
		for (const LyudJson::FValue& S : J["stairs"].Items)
		{
			FStair St;
			St.Id = S["id"].S();
			if (S["street"].IsNull())
			{
				continue;       // no way in from the street (Novikova 1's middle stair behind the library)
			}
			St.Street = Vec(S["street"]);
			for (const LyudJson::FValue& F : S["floors"].Items)
			{
				const int K = F["floor"].I();
				if ((int)St.Floors.size() <= K)
				{
					St.Floors.resize(K + 1);
				}
				St.Floors[K] = Line(F["path"]);
			}
			Stairs.push_back(St);
			// the lobby inside the street door (where a bike can stand): the ground floor's way a few metres in
			if (!St.Floors.empty() && St.Floors[0].size() >= 4)
			{
				Lobbies.push_back({ St.Id, St.Street, St.Floors[0][3] });
			}
		}

		for (const LyudJson::FValue& F : J["flats"].Items)
		{
			FPlace Pl;
			Pl.Id = F["id"].S();
			Pl.Kind = "flat";
			Pl.House = F["house"].S();
			Pl.Floor = F["floor"].I();
			Pl.Number = F["number"].I();
			Pl.bIndoor = true;
			const FStair* St = nullptr;
			for (const FStair& S : Stairs)
			{
				if (S.Id == F["stair"].S())
				{
					St = &S;
				}
			}
			if (!St)
			{
				continue;
			}
			Pl.Portal = St->Street;
			Pl.Stair = St->Id;
			std::vector<V3> ToDoor;
			if (Pl.Floor < (int)St->Floors.size())
			{
				ToDoor = St->Floors[Pl.Floor];
			}
			ToDoor.push_back(Vec(F["door"]));
			const int PlaceIdx = (int)Places.size();
			for (const LyudJson::FValue& S : F["slots"].Items)
			{
				const int Room = S["room"].I(-1);
				std::vector<V3> Prefix = ToDoor;
				LyudJson::FValue Copy = S;
				if (Room >= 0 && Room < (int)F["rooms"].Size())
				{
					const LyudJson::FValue& R = F["rooms"][Room];
					if (S["path"].IsNull())
					{
						for (const V3& P : Line(R["path"]))
						{
							Prefix.push_back(P);
						}
					}
					if (Copy.Find("tag") == nullptr)
					{
						Copy.Fields.emplace_back("tag", R["kind"]);
					}
				}
				AddSlot(Pl, PlaceIdx, Copy, Prefix, "sofa");
			}
			Places.push_back(Pl);
		}

		for (const LyudJson::FValue& V : J["places"].Items)
		{
			FPlace Pl;
			Pl.Id = V["id"].S();
			Pl.Kind = V["kind"].S();
			Pl.Yaw = V["yaw"].F();
			const std::vector<V3> Path = Line(V["path"]);
			Pl.bIndoor = !Path.empty();
			Pl.Portal = V.Find("door") ? Vec(V["door"]) : Vec(V["pos"]);
			std::vector<V3> Prefix;
			if (Pl.bIndoor)
			{
				Prefix.push_back(Pl.Portal);
				// slots with a path of their own start it at the door
				if (V["slots"].Size() == 0 || V["slots"][0]["path"].IsNull())
				{
					for (const V3& P : Path)
					{
						Prefix.push_back(P);
					}
				}
			}
			const int PlaceIdx = (int)Places.size();
			for (const LyudJson::FValue& S : V["slots"].Items)
			{
				AddSlot(Pl, PlaceIdx, S, Prefix, "stand");
			}
			if (!V["staff"].IsNull())
			{
				LyudJson::FValue Staff = V["staff"];
				if (Staff.Find("kind") == nullptr)
				{
					Staff.Fields.emplace_back("kind", LyudJson::FValue());
					Staff.Fields.back().second.Type = LyudJson::FValue::String;
					Staff.Fields.back().second.Str = "staff";
				}
				AddSlot(Pl, PlaceIdx, Staff, Prefix, "staff");
			}
			// a halt of the sea railway: the way from the ground up onto its platform; its slots are boarding points
			Pl.Stop = V["stop"].S();
			Pl.RailWay = Line(V["way"]);
			if (!Pl.RailWay.empty() && !Same(Pl.RailWay.front(), Pl.Portal))
			{
				Pl.RailWay.insert(Pl.RailWay.begin(), Pl.Portal);
			}
			// on the sea island (off the walk grid: reached by the train)
			if (V["sea_island"].F() > 0.f)
			{
				Pl.Island = 2;
			}
			if (Pl.Slots.empty())
			{
				// a spot outdoors: people stand round it
				FSlot Sl;
				Sl.Kind = "stand";
				Sl.Place = PlaceIdx;
				Sl.Pos = Sl.Approach = Pl.Portal;
				Sl.Yaw = Pl.Yaw;
				Sl.Capacity = 8;
				Pl.Slots.push_back((int)Slots.size());
				Slots.push_back(Sl);
			}
			Places.push_back(Pl);
		}

		// the club: in through the blast door and the hall, along the walkway to the stations
		const LyudJson::FValue& C = J["club"];
		if (!C.IsNull())
		{
			FPlace Pl;
			Pl.Id = "club";
			Pl.Kind = "club";
			Pl.bIndoor = true;
			const std::vector<V3> Entry = Line(C["entry_path"]);
			if (!Entry.empty())
			{
				Pl.Portal = Entry.front();
				const int PlaceIdx = (int)Places.size();
				for (const LyudJson::FValue& S : C["seats"].Items)
				{
					if (S.Find("reachable") && S["reachable"].F() <= 0.f)
					{
						continue;           // behind the invisible wall
					}
					LyudJson::FValue Copy = S;
					LyudJson::FValue T;
					T.Type = LyudJson::FValue::String;
					T.Str = std::to_string(S["index"].I());
					Copy.Fields.erase(std::remove_if(Copy.Fields.begin(), Copy.Fields.end(), [](const auto& F) { return F.first == "tag"; }), Copy.Fields.end());
					Copy.Fields.emplace_back("tag", T);
					if (T.Str == "10")
					{
						continue;           // the hero's seat
					}
					AddSlot(Pl, PlaceIdx, Copy, Entry, "club_seat");
				}
				for (const LyudJson::FValue& S : C["vending"].Items)
				{
					AddSlot(Pl, PlaceIdx, S, Entry, "vending");
				}
				if (!C["booth"]["owner"].IsNull())
				{
					AddSlot(Pl, PlaceIdx, C["booth"]["owner"], Entry, "booth");
				}
				if (!C["hall_chair"].IsNull())
				{
					AddSlot(Pl, PlaceIdx, C["hall_chair"], Entry, "chair");
				}
				Places.push_back(Pl);
			}
		}
		if (Places.empty())
		{
			LastError = "people_places.json: no places";
			return false;
		}
		for (FPlace& Pl : Places)
		{
			Pl.Island = Pl.Island == 2 ? 2 : IslandOf(Pl.Portal);
		}
		// ground off the walk grid (the sea island's own height field)
		Patches.clear();
		for (const LyudJson::FValue& G : J["ground_patches"].Items)
		{
			FPatch Pa;
			Pa.X0 = G["x0"].F();
			Pa.Y0 = G["y0"].F();
			Pa.Cell = std::max(0.1f, G["cell"].F(4.f));
			Pa.NX = G["nx"].I();
			Pa.NY = G["ny"].I();
			for (const LyudJson::FValue& Z : G["z"].Items)
			{
				Pa.Z.push_back(Z.F());
			}
			if (Pa.NX > 1 && Pa.NY > 1 && (int)Pa.Z.size() == Pa.NX * Pa.NY)
			{
				Patches.push_back(Pa);
			}
		}
		return true;
	}

	bool FWorld::FPatch::Has(float X, float Y) const
	{
		return X >= X0 && Y >= Y0 && X <= X0 + Cell * (NX - 1) && Y <= Y0 + Cell * (NY - 1);
	}

	float FWorld::FPatch::At(float X, float Y) const
	{
		const float FX = std::min(std::max((X - X0) / Cell, 0.f), NX - 1.001f);
		const float FY = std::min(std::max((Y - Y0) / Cell, 0.f), NY - 1.001f);
		const int I = (int)FX, J = (int)FY;
		const float U = FX - I, V = FY - J;
		const float* R0 = &Z[J * NX + I];
		const float* R1 = &Z[(J + 1) * NX + I];
		return (R0[0] * (1.f - U) + R0[1] * U) * (1.f - V) + (R1[0] * (1.f - U) + R1[1] * U) * V;
	}

	int FWorld::FTownGround::LevelOf(float Z) const
	{
		int Best = 0;
		for (int I = 1; I < (int)Levels.size(); I++)
		{
			if (std::fabs(Levels[I] - Z) < std::fabs(Levels[Best] - Z))
			{
				Best = I;
			}
		}
		return Best;
	}

	void FWorld::TownWay(const V3& From, const V3& To, std::vector<FPerson::FWay>& Out) const
	{
		const int LA = Town.LevelOf(From.Z), LB = Town.LevelOf(To.Z);
		// the stairs' ends as nodes (2k bottom, 2k+1 top); a shortest way from From to To over them (Dijkstra, ~140 nodes)
		const int N = (int)Town.Stairs.size() * 2;
		auto NodeP = [&](int K) { const FTownGround::FStair& S = Town.Stairs[K / 2]; return K % 2 ? S.Top : S.Bottom; };
		auto NodeL = [&](int K) { const FTownGround::FStair& S = Town.Stairs[K / 2]; return K % 2 ? S.Hi : S.Lo; };
		std::vector<int> Via;
		if (LA != LB && N > 0)
		{
			std::vector<float> Dist(N, 1e30f);
			std::vector<int> Prev(N, -1);
			std::vector<char> Done(N, 0);
			for (int K = 0; K < N; K++)
			{
				if (NodeL(K) == LA)
				{
					Dist[K] = (NodeP(K) - From).Len2D();
				}
			}
			int End = -1;
			float EndD = 1e30f;
			for (;;)
			{
				int U = -1;
				for (int K = 0; K < N; K++)
				{
					if (!Done[K] && Dist[K] < 1e29f && (U < 0 || Dist[K] < Dist[U]))
					{
						U = K;
					}
				}
				if (U < 0 || Dist[U] >= EndD)
				{
					break;
				}
				Done[U] = 1;
				if (NodeL(U) == LB && Dist[U] + (NodeP(U) - To).Len2D() < EndD)
				{
					EndD = Dist[U] + (NodeP(U) - To).Len2D();
					End = U;
				}
				// up or down its flight (a little dearer than the flat), or across the terrace to another stair
				const int Other = U ^ 1;
				const float Flight = (NodeP(U) - NodeP(Other)).Len() * 1.4f + 4.f;
				if (Dist[U] + Flight < Dist[Other])
				{
					Dist[Other] = Dist[U] + Flight;
					Prev[Other] = U;
				}
				for (int K = 0; K < N; K++)
				{
					if (K != U && NodeL(K) == NodeL(U) && !Done[K])
					{
						const float D = Dist[U] + (NodeP(K) - NodeP(U)).Len2D();
						if (D < Dist[K])
						{
							Dist[K] = D;
							Prev[K] = U;
						}
					}
				}
			}
			for (int K = End; K >= 0; K = Prev[K])
			{
				Via.push_back(K);
			}
			std::reverse(Via.begin(), Via.end());
		}
		// a leg along one terrace: the walk grid's way if it has one, at the terrace's height; else straight
		auto Leg = [&](const V3& A, const V3& B, int Level)
		{
			const float Z = Town.Levels[Level];
			std::vector<V3> Pts;
			if (!bHasGround || (A - B).Len2D() < 6.f || !Ground.FindPath(A, B, LyudAnimals::EKind::Dog, Pts) || Pts.size() > 400)
			{
				Pts.clear();
				const int Steps = std::max(1, (int)std::ceil((B - A).Len2D() / 12.f));
				for (int K = 1; K <= Steps; K++)
				{
					Pts.push_back(A + (B - A) * ((float)K / Steps));
				}
			}
			for (const V3& Pt : Pts)
			{
				FPerson::FWay W;
				W.P = V3(Pt.X, Pt.Y, Z);
				Out.push_back(W);
			}
		};
		V3 Cur = From;
		int CurL = LA;
		for (size_t I = 0; I < Via.size(); I++)
		{
			const int K = Via[I];
			const V3 P = NodeP(K);
			if (I > 0 && (Via[I - 1] ^ 1) == K)
			{
				// up or down a flight: along its line in 3D
				FPerson::FWay W;
				W.P = P;
				W.Kind = 4;
				Out.push_back(W);
			}
			else
			{
				Leg(Cur, P, CurL);
			}
			Cur = P;
			CurL = NodeL(K);
		}
		Leg(Cur, To, CurL);
		if (!Out.empty())
		{
			Out.back().P = To;
		}
	}

	float FWorld::GroundAt(const V3& P) const
	{
		if (Town.Inside(P))
		{
			// on their terrace (the stairs are walked as lines); below the lowest the sand slopes to the sea: as the way goes
			const int L = Town.LevelOf(P.Z);
			return L == 0 && P.Z < Town.Levels[0] ? P.Z : Town.Levels[L];
		}
		for (const FPatch& Pa : Patches)
		{
			if (Pa.Has(P.X, P.Y))
			{
				return Pa.At(P.X, P.Y);
			}
		}
		return bHasGround ? Ground.GroundZ(P.X, P.Y) : P.Z;
	}

	int FWorld::RailStopOf(int Island) const
	{
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Kind == "rail_stop" && Places[I].Island == Island && !Places[I].Slots.empty())
			{
				return I;
			}
		}
		return -1;
	}

	bool FWorld::SetRailStop(const std::string& Stop, const V3& Foot, const std::vector<V3>& Boarding)
	{
		for (FPlace& Pl : Places)
		{
			if (Pl.Kind != "rail_stop" || Pl.Stop != Stop)
			{
				continue;
			}
			// the stair's foot and top move with the foot; the boarding points are the game's
			if (Pl.RailWay.size() >= 3)
			{
				const V3 D = Foot - Pl.RailWay[Pl.RailWay.size() - 2];
				Pl.RailWay[Pl.RailWay.size() - 2] = Foot;
				Pl.RailWay.back() = Pl.RailWay.back() + D;
			}
			for (size_t K = 0; K < Pl.Slots.size() && K < Boarding.size(); K++)
			{
				Slots[Pl.Slots[K]].Pos = Slots[Pl.Slots[K]].Approach = Boarding[K];
			}
			return true;
		}
		return false;
	}

	bool FWorld::Hops(int From, int To, std::vector<FHop>& Out) const
	{
		Out.clear();
		if (From == To)
		{
			return true;
		}
		auto Cable = [&](int A, int B)
		{
			const int SA = StationOf(A), SB = StationOf(B);
			if (SA < 0 || SB < 0)
			{
				return false;
			}
			Out.push_back({ 2, SA, SB, B });
			return true;
		};
		auto Train = [&](int A, int B)
		{
			const int SA = RailStopOf(A), SB = RailStopOf(B);
			if (SA < 0 || SB < 0)
			{
				return false;
			}
			Out.push_back({ 3, SA, SB, B });
			return true;
		};
		if ((From == 0 && To == 1) || (From == 1 && To == 0))
		{
			return Cable(From, To);
		}
		if ((From == 0 && To == 2) || (From == 2 && To == 0))
		{
			return Train(From, To);
		}
		if (From == 1 && To == 2)
		{
			return Cable(1, 0) && Train(0, 2);
		}
		if (From == 2 && To == 1)
		{
			return Train(2, 0) && Cable(0, 1);
		}
		return false;
	}

	float FWorld::WaySeconds(const FPerson& P, const V3& From, int FromIsland, int ToSlot, double At) const
	{
		const FSlot& Sl = Slots[ToSlot];
		const int ToIsland = Places[Sl.Place].Island;
		const float Walk = 1.3f / P.WalkSpeed;
		float T = 0.f;
		V3 Cur = From;
		std::vector<FHop> H;
		if (FromIsland >= 0 && FromIsland != ToIsland && Hops(FromIsland, ToIsland, H))
		{
			for (const FHop& Hop : H)
			{
				const FPlace& A = Places[Hop.A];
				const FPlace& B = Places[Hop.B];
				T += (A.Portal - Cur).Len2D() * Walk;
				if (Hop.Kind == 2)
				{
					T += Opt.RideSeconds + 240.f;
				}
				else
				{
					T += 60.f;      // the stair, along the platform
					double Doors = 0.0;
					const bool bTimed = TrainNext && TrainNext(A.Stop, B.Stop, At + T, Doors);
					const float Ride = TrainRide ? (float)TrainRide(A.Stop, B.Stop) : Opt.TrainSeconds;
					T = bTimed ? (float)(Doors - At) + Ride : T + Opt.TrainSeconds;
					T += 60.f;
				}
				Cur = B.Portal;
			}
		}
		return T + (Sl.Approach - Cur).Len2D() * Walk;
	}

	int FWorld::IslandOf(const V3& P) const
	{
		if (!bHasGround)
		{
			return 0;
		}
		std::vector<V3> Tmp;
		for (int Island = 0; Island < 2; Island++)
		{
			const int S = StationOf(Island);
			if (S < 0)
			{
				return 0;
			}
			if ((Places[S].Portal - P).Len2D() < 1.f || Ground.FindPath(P, Places[S].Portal, LyudAnimals::EKind::Dog, Tmp))
			{
				return Island;
			}
		}
		return -1;
	}

	void FWorld::FinishRide(FPerson& P)
	{
		// out at the other station (the train: onto the other halt's platform)
		P.bRideExternal = false;
		P.bTrain = false;
		const int To = P.PathI > 0 && P.PathI <= (int)P.Path.size() ? P.Path[P.PathI - 1].ToIsland : -1;
		if (P.PathI < (int)P.Path.size())
		{
			P.Pos = P.Path[P.PathI].P;
			P.PathI++;
		}
		P.Island = To >= 0 ? To : 1 - P.Island;
		P.State = EState::Going;
		P.Pose = EPose::Walk;
		Emit(EEvent::Back, P);
		if (P.PathI >= (int)P.Path.size())
		{
			Arrive(P);
		}
	}

	void FWorld::EndRide(int Person)
	{
		if (Person >= 0 && Person < (int)People.size() && People[Person].State == EState::Riding && !People[Person].bTrain)
		{
			FinishRide(People[Person]);
		}
	}

	int FWorld::StationOf(int Island) const
	{
		const char* Kind = Island == 0 ? "station_lower" : "station_upper";
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Kind == Kind)
			{
				return I;
			}
		}
		return -1;
	}

	int FWorld::FindPerson(const std::string& Key) const
	{
		for (const FPerson& P : People)
		{
			if (P.Key == Key)
			{
				return P.Id;
			}
		}
		return -1;
	}

	int FWorld::FindPlace(const std::string& Id) const
	{
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	int FWorld::FindActivity(const std::string& Id) const
	{
		for (int I = 0; I < (int)Activities.size(); I++)
		{
			if (Activities[I].Id == Id)
			{
				return I;
			}
		}
		return -1;
	}

	// --- the town (town_places.json, townsfolk.json: docs/town_life.md) ------------------------------------

	bool FWorld::LoadTownPlaces(const std::string& Text)
	{
		LyudJson::FValue J;
		if (!LyudJson::Parse(Text, J, LastError))
		{
			LastError = "town_places.json: " + LastError;
			return false;
		}
		const LyudJson::FValue& T = J["town"];
		if (!T.IsNull() && T["box"].Size() == 4)
		{
			Town = FTownGround();
			Town.X0 = T["box"][0].F();
			Town.Y0 = T["box"][1].F();
			Town.X1 = T["box"][2].F();
			Town.Y1 = T["box"][3].F();
			for (const LyudJson::FValue& L : T["levels"].Items)
			{
				Town.Levels.push_back(L["z"].F());
			}
			Town.bValid = !Town.Levels.empty();
			for (const LyudJson::FValue& S : T["stairs"].Items)
			{
				FTownGround::FStair St;
				St.Bottom = Vec(S["bottom"]);
				St.Top = Vec(S["top"]);
				St.Lo = Town.LevelOf(St.Bottom.Z);
				St.Hi = Town.LevelOf(St.Top.Z);
				if (St.Lo != St.Hi)
				{
					Town.Stairs.push_back(St);
				}
			}
		}
		for (const LyudJson::FValue& V : J["places"].Items)
		{
			FPlace Pl;
			Pl.Id = V["id"].S();
			if (Pl.Id.empty() || FindPlace(Pl.Id) >= 0)
			{
				continue;
			}
			Pl.Kind = V["kind"].S();
			Pl.Yaw = V["yaw"].F();
			Pl.Portal = V.Find("door") ? Vec(V["door"]) : Vec(V["pos"]);
			Pl.bShell = V["shell"].F() > 0.f;
			Pl.bIndoor = Pl.bShell;
			// a walked interior (the restaurant): in by its door and the way inside to each slot
			std::vector<V3> Prefix;
			const std::vector<V3> InPath = Line(V["path"]);
			if (!Pl.bShell && !InPath.empty())
			{
				Pl.bIndoor = true;
				Prefix.push_back(Pl.Portal);
				Prefix.insert(Prefix.end(), InPath.begin(), InPath.end());
			}
			Pl.Building = V["building"].S();
			Pl.Island = V.Find("island") ? V["island"].I() : IslandOf(Pl.Portal);
			for (const LyudJson::FValue& W : V["windows"].Items)
			{
				Pl.Windows.push_back(W.S());
			}
			for (const LyudJson::FValue& R : V["roles"].Items)
			{
				Pl.Roles.push_back(R.S());
			}
			const int PlaceIdx = (int)Places.size();
			for (const LyudJson::FValue& S : V["slots"].Items)
			{
				FSlot Sl;
				Sl.Kind = S["kind"].S("stand");
				Sl.Place = PlaceIdx;
				// a shell's slots are all at its door: whoever is in is inside, not drawn
				Sl.Pos = Pl.bShell || S["pos"].IsNull() ? Pl.Portal : Vec(S["pos"]);
				Sl.Yaw = S.Find("yaw") ? S["yaw"].F() : Pl.Yaw;
				Sl.Approach = S.Find("approach") && !Pl.bShell ? Vec(S["approach"]) : Sl.Pos;
				Sl.Height = S["height"].F();
				Sl.Capacity = std::max(1, S["capacity"].I(1));
				Sl.Tag = S["tag"].S();
				if (!Prefix.empty())
				{
					Sl.Inside = Prefix;
					if (!Same(Sl.Inside.back(), Sl.Approach))
					{
						Sl.Inside.push_back(Sl.Approach);
					}
				}
				Pl.Slots.push_back((int)Slots.size());
				Slots.push_back(Sl);
			}
			if (Pl.Slots.empty())
			{
				FSlot Sl;
				Sl.Kind = "stand";
				Sl.Place = PlaceIdx;
				Sl.Pos = Sl.Approach = Pl.Portal;
				Sl.Yaw = Pl.Yaw;
				Sl.Capacity = 8;
				Pl.Slots.push_back((int)Slots.size());
				Slots.push_back(Sl);
			}
			Places.push_back(Pl);
		}
		Index();
		return true;
	}

	int FWorld::KindId(const std::string& Kind)
	{
		if (KindNames.empty())
		{
			KindNames = { "bed", "sofa", "flat", "snow" };
		}
		for (int I = 0; I < (int)KindNames.size(); I++)
		{
			if (KindNames[I] == Kind)
			{
				return I;
			}
		}
		KindNames.push_back(Kind);
		return (int)KindNames.size() - 1;
	}

	void FWorld::Index()
	{
		for (FSlot& Sl : Slots)
		{
			Sl.K = KindId(Sl.Kind);
		}
		for (FPlace& Pl : Places)
		{
			Pl.K = KindId(Pl.Kind);
		}
		for (FActivity& A : Activities)
		{
			A.SlotK.clear();
			A.PlaceK.clear();
			for (const std::string& K : A.Slots)
			{
				A.SlotK.push_back(KindId(K));
			}
			A.bHome = false;
			for (const std::string& K : A.Places)
			{
				if (K == "home")
				{
					A.bHome = true;
				}
				else
				{
					A.PlaceK.push_back(KindId(K));
				}
			}
			A.bK = true;
			A.PlaceIdx.clear();
			for (int Pi = 0; Pi < (int)Places.size(); Pi++)
			{
				if (Has(A.Places, Places[Pi].Kind))
				{
					A.PlaceIdx.push_back(Pi);
				}
			}
		}
		for (FPerson& P : People)
		{
			for (FDuty& D : P.Duties)
			{
				D.Place = D.PlaceId.empty() ? -1 : FindPlace(D.PlaceId);
				D.Act = FindActivity(D.Activity);
			}
		}
		SleepAct = FindActivity("sleep");
		RestHomeAct = FindActivity("rest_home");
		FlatLit.assign(Places.size(), -1);
		Indexed = Places.size();
	}

	const FDuty* FWorld::DutyFor(const FPerson& P, const FActivity& A) const
	{
		const int AI = (int)(&A - Activities.data());
		for (const FDuty& D : P.Duties)
		{
			if (D.Place >= 0 && (D.Act >= 0 ? D.Act == AI : D.Activity == A.Id))
			{
				return &D;
			}
		}
		return nullptr;
	}

	bool FWorld::InsideShell(const FPerson& P) const
	{
		return P.Place >= 0 && P.Slot >= 0 && Places[P.Place].bShell
			&& (P.State == EState::Doing || P.State == EState::Settling || P.State == EState::Leaving);
	}

	void FWorld::StaffOnShift(const std::string& PlaceId, const std::string& Role, std::vector<int>& Out) const
	{
		Out.clear();
		const int Pl = FindPlace(PlaceId);
		if (Pl < 0)
		{
			return;
		}
		for (const FPerson& P : People)
		{
			if (P.Place != Pl || P.Activity < 0 || (P.State != EState::Doing && P.State != EState::Settling) || !Activities[P.Activity].bStaff)
			{
				continue;
			}
			const FDuty* D = DutyFor(P, Activities[P.Activity]);
			if (D && D->Place == Pl && (Role.empty() || D->Role == Role))
			{
				Out.push_back(P.Id);
			}
		}
	}

	void FWorld::SetGathering(const std::string& Id, const std::string& PlaceOrKind, float From, float To, float Pull, EGatherMode Mode, bool bFace, const V3& Face)
	{
		ClearGathering(Id);
		FGathering G;
		G.Id = Id;
		G.PlaceOrKind = PlaceOrKind;
		G.From = From;
		G.To = To;
		G.Pull = std::max(0.f, std::min(Pull, 3.f));
		G.Mode = Mode;
		G.bFace = bFace;
		G.Face = Face;
		const int Pl = FindPlace(PlaceOrKind);
		if (Pl >= 0)
		{
			G.Places.push_back(Pl);
		}
		else
		{
			for (int I = 0; I < (int)Places.size(); I++)
			{
				if (Places[I].Kind == PlaceOrKind && !Places[I].bShell)
				{
					G.Places.push_back(I);
				}
			}
		}
		Gatherings.push_back(G);
	}

	void FWorld::ClearGathering(const std::string& Id)
	{
		Gatherings.erase(std::remove_if(Gatherings.begin(), Gatherings.end(), [&](const FGathering& G) { return G.Id == Id; }), Gatherings.end());
	}

	const FWorld::FGathering* FWorld::GatheringAt(int Place) const
	{
		for (const FGathering& G : Gatherings)
		{
			if (InHours(G.From, G.To, Hour) && std::find(G.Places.begin(), G.Places.end(), Place) != G.Places.end())
			{
				return &G;
			}
		}
		return nullptr;
	}

	bool FWorld::FlatLitNow(int Place) const
	{
		const FPlace& Pl = Places[Place];
		// daylight indoors is enough till a little before sunset (and from a little after sunrise)
		const bool bDark = !bSunUp || Hour > Sunset - 0.4f || Hour < Sunrise + 0.3f;
		if (!bDark)
		{
			return false;
		}
		for (const int Id : Pl.Residents)
		{
			const FPerson& P = People[Id];
			if (P.Place != Place || P.Activity < 0)
			{
				continue;
			}
			const bool bThere = P.State == EState::Doing || P.State == EState::Settling || P.State == EState::Leaving || P.State == EState::Idle;
			if (!bThere)
			{
				continue;
			}
			if (Activities[P.Activity].Pose != EPose::Sleep)
			{
				return true;
			}
			// a lamp by the bed a while after lying down (some read, some put it out at once)
			uint32_t H = (uint32_t)(P.Id * 2246822519u) ^ (uint32_t)(Time / 86400.0) * 3266489917u;
			const double Lamp = 60.0 * (Rand(H) < 0.4f ? 2.0 : 6.0 + 20.0 * Rand(H));
			if (P.State == EState::Settling || Time - P.StateStart < Lamp)
			{
				return true;
			}
		}
		return false;
	}

	void FWorld::UpdateLights(std::vector<int>& Changed)
	{
		Changed.clear();
		if (FlatLit.size() != Places.size())
		{
			FlatLit.assign(Places.size(), -1);
		}
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Windows.empty())
			{
				continue;
			}
			const int8_t Now = FlatLitNow(I) ? 1 : 0;
			if (FlatLit[I] != Now)
			{
				FlatLit[I] = Now;
				Changed.push_back(I);
			}
		}
	}

	void FWorld::RollDays()
	{
		DayRoll++;
		for (FPerson& P : People)
		{
			if (P.Spread <= 0.f)
			{
				P.WakeJ = P.BedJ = 0.f;
				continue;
			}
			uint32_t H = (uint32_t)(P.Id * 2654435761u) ^ (uint32_t)(DayRoll * 40503u) ^ Opt.Seed;
			H |= 1u;
			Rand(H);
			// mostly near their usual, now and then a lot off (up before dawn, a late night)
			const float A = Rand(H) * 2.f - 1.f, B = Rand(H) * 2.f - 1.f;
			P.WakeJ = P.Spread * A * std::fabs(A);
			P.BedJ = P.Spread * B * std::fabs(B);
			// weekends: up later, to bed later (not the children)
			if (!P.bChild && (Weekday == 4 || Weekday == 5))
			{
				P.BedJ += 0.6f * P.Spread;
			}
			if (!P.bChild && (Weekday == 4 || Weekday == 5))
			{
				P.WakeJ += 0.8f * P.Spread;
			}
		}
	}

	// --- homes -------------------------------------------------------------------------------------------

	bool FWorld::SetHome(const std::string& Household, const std::string& FlatId)
	{
		const int F = FindPlace(FlatId);
		if (F < 0 || Places[F].Kind != "flat")
		{
			return false;
		}
		for (FPlace& Pl : Places)
		{
			if (Pl.Kind == "flat" && Pl.Id != FlatId)
			{
				Pl.Residents.erase(std::remove_if(Pl.Residents.begin(), Pl.Residents.end(),
					[&](int Id) { return People[Id].Household == Household; }), Pl.Residents.end());
			}
		}
		FPlace& Flat = Places[F];
		for (const int Id : Flat.Residents)
		{
			if (People[Id].Household != Household)
			{
				return false;       // somebody else's
			}
		}
		for (FPerson& P : People)
		{
			if (P.Household == Household)
			{
				P.Home = F;
				if (std::find(Flat.Residents.begin(), Flat.Residents.end(), P.Id) == Flat.Residents.end())
				{
					Flat.Residents.push_back(P.Id);
				}
			}
		}
		return true;
	}

	void FWorld::AssignHomes()
	{
		// households in the order of people.json; each takes the first free flat with enough beds, spread over the
		// houses and stairs (a seeded shuffle), unless it already has one (SetHome from the save)
		std::vector<int> Flats;
		for (int I = 0; I < (int)Places.size(); I++)
		{
			if (Places[I].Kind == "flat")
			{
				Flats.push_back(I);
			}
		}
		uint32_t R = Opt.Seed ^ 0x51ED270Bu;
		for (int I = (int)Flats.size() - 1; I > 0; I--)
		{
			const int K = (int)(Rand(R) * (I + 1)) % (I + 1);
			std::swap(Flats[I], Flats[K]);
		}
		auto Beds = [&](int F)
		{
			int N = 0;
			for (const int S : Places[F].Slots)
			{
				if (Slots[S].Kind == "bed")
				{
					N += Slots[S].Capacity;
				}
			}
			return N;
		};
		std::vector<std::string> Done;
		// the townsfolk's flats are given by the generator (townsfolk.json home)
		for (const FPerson& P : People)
		{
			if (P.Home < 0 && !P.HomeId.empty() && !Has(Done, P.Household))
			{
				Done.push_back(P.Household);
				SetHome(P.Household, P.HomeId);
			}
		}
		Done.clear();
		for (const FPerson& P : People)
		{
			if (Has(Done, P.Household))
			{
				continue;
			}
			Done.push_back(P.Household);
			if (P.Home >= 0)
			{
				continue;
			}
			// the Academgorodok's people keep to its flats, the town's to the town's buildings
			auto Ok = [&](int F) { return Places[F].bShell == P.bTown; };
			int Size = 0;
			for (const FPerson& Q : People)
			{
				Size += Q.Household == P.Household ? 1 : 0;
			}
			int Pick = -1, PickBeds = -1;
			for (const int F : Flats)
			{
				if (Ok(F) && Places[F].Residents.empty() && Beds(F) >= Size)
				{
					Pick = F;
					break;
				}
			}
			// none with enough beds left: the free flat with the most (the rest sleep on a sofa)
			for (const int F : Flats)
			{
				if (Pick < 0 && Ok(F) && Places[F].Residents.empty() && Beds(F) > PickBeds)
				{
					Pick = -1;
					PickBeds = Beds(F);
				}
			}
			for (const int F : Flats)
			{
				if (Pick < 0 && PickBeds > 0 && Ok(F) && Places[F].Residents.empty() && Beds(F) == PickBeds)
				{
					Pick = F;
				}
			}
			if (Pick >= 0)
			{
				SetHome(P.Household, Places[Pick].Id);
			}
		}
		ReserveOwnSlots();
		// beds: one each while there are beds; a double bed takes two of a household (a couple, a mother and her
		// child); the rest sleep on a sofa
		for (FPlace& Flat : Places)
		{
			if (Flat.Kind != "flat")
			{
				continue;
			}
			for (const int S : Flat.Slots)
			{
				Slots[S].Owner = -1;
				Slots[S].Users.clear();
			}
			std::vector<int> Taken(Flat.Slots.size(), 0);
			for (int Pass = 0; Pass < 2; Pass++)
			{
				for (const int Id : Flat.Residents)
				{
					FPerson& P = People[Id];
					if (Pass == 0)
					{
						P.BedSlot = -1;
					}
					if (P.BedSlot >= 0)
					{
						continue;
					}
					// pass 0: an empty bed (single beds first); pass 1: the free half of a double one, else a sofa
					for (size_t K = 0; K < Flat.Slots.size() && P.BedSlot < 0; K++)
					{
						const FSlot& Sl = Slots[Flat.Slots[K]];
						const bool bFits = Pass == 0 ? Sl.Kind == "bed" && Taken[K] == 0 && Sl.Capacity == 1
							: Sl.Kind == "bed" && Taken[K] < Sl.Capacity;
						if (bFits)
						{
							P.BedSlot = Flat.Slots[K];
							Taken[K]++;
						}
					}
					if (Pass == 0)
					{
						for (size_t K = 0; K < Flat.Slots.size() && P.BedSlot < 0; K++)
						{
							if (Slots[Flat.Slots[K]].Kind == "bed" && Taken[K] == 0)
							{
								P.BedSlot = Flat.Slots[K];
								Taken[K]++;
							}
						}
					}
					for (size_t K = 0; Pass == 1 && K < Flat.Slots.size() && P.BedSlot < 0; K++)
					{
						if (Slots[Flat.Slots[K]].Kind == "sofa" && Taken[K] == 0)
						{
							P.BedSlot = Flat.Slots[K];
							Taken[K]++;
						}
					}
					if (P.BedSlot >= 0 && Slots[P.BedSlot].Owner < 0 && Slots[P.BedSlot].Kind == "bed")
					{
						Slots[P.BedSlot].Owner = Id;
					}
				}
			}
		}
	}

	void FWorld::ReserveOwnSlots()
	{
		for (const FPerson& P : People)
		{
			const size_t Colon = P.OwnSlot.find(':');
			if (Colon == std::string::npos)
			{
				continue;
			}
			const int Pl = FindPlace(P.OwnSlot.substr(0, Colon));
			const std::string Tag = P.OwnSlot.substr(Colon + 1);
			if (Pl < 0)
			{
				continue;
			}
			for (const int S : Places[Pl].Slots)
			{
				if (Slots[S].Tag == Tag)
				{
					Slots[S].Owner = P.Id;
				}
			}
		}
	}

	// --- the day -----------------------------------------------------------------------------------------

	float FWorld::Rand(uint32_t& S) const
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xFFFFFF) / 16777216.f;
	}

	bool FWorld::InHours(float From, float To, float H) const
	{
		if (To - From >= 24.f || To == From)
		{
			return true;
		}
		From = std::fmod(From + 48.f, 24.f);
		To = std::fmod(To + 48.f, 24.f);
		if (From <= To)
		{
			return H >= From && H < To;
		}
		return H >= From || H < To;
	}

	float FWorld::Trait(const FPerson& P, const std::string& T) const
	{
		if (T == "social") return P.Social;
		if (T == "active") return P.Active;
		if (T == "studious") return P.Studious;
		if (T == "gamer") return P.Gamer;
		if (T == "nature") return P.NatureLove;
		if (T == "sun") return P.SunLove;
		return 0.5f;
	}

	int FWorld::DutyNow(const FPerson& P, bool& bSleep) const
	{
		bSleep = false;
		if (InHours(BedOf(P), WakeOf(P), Hour))
		{
			bSleep = true;
			return SleepAct >= 0 ? SleepAct : FindActivity("sleep");
		}
		for (const FDuty& D : P.Duties)
		{
			if (bDayOff)
			{
				const int A = ActOf(D);
				if (A >= 0 && Activities[A].bHolidayOff)
				{
					continue;       // a public holiday: no school, the offices shut
				}
			}
			// a weekday duty that started yesterday evening (a night shift) counts for yesterday
			const bool bDay = (D.Days >> Weekday) & 1;
			// set off a little early: they go there first
			if (bDay && InHours(D.From - 0.3f, D.To, Hour))
			{
				return ActOf(D);
			}
		}
		return -1;
	}

	float FWorld::FreeHoursLeft(const FPerson& P) const
	{
		float Left = BedOf(P) - Hour;
		if (Left < 0.f)
		{
			Left += 24.f;
		}
		for (const FDuty& D : P.Duties)
		{
			const float In = D.From - Hour;
			if (((D.Days >> Weekday) & 1) && In > 0.f && In < Left)
			{
				Left = In;
			}
		}
		return Left;
	}

	bool FWorld::SlotFits(const FPerson& P, const FActivity& A, int S) const
	{
		const FSlot& Sl = Slots[S];
		const FPlace& Pl = Places[Sl.Place];
		// interned kinds (Index) when the slot and the place have them: no string compares in the hot loop
		const bool bK = A.bK && Sl.K >= 0 && Pl.K >= 0;
		if (bK)
		{
			if (!A.SlotK.empty() && std::find(A.SlotK.begin(), A.SlotK.end(), Sl.K) == A.SlotK.end())
			{
				return false;
			}
		}
		else if (!A.Slots.empty() && !Has(A.Slots, Sl.Kind))
		{
			return false;
		}
		const bool bBed = bK ? Sl.K == 0 : Sl.Kind == "bed";
		const bool bSofa = bK ? Sl.K == 1 : Sl.Kind == "sofa";
		const bool bFlat = bK ? Pl.K == 2 : Pl.Kind == "flat";
		const bool bSnow = bK ? Pl.K == 3 : Pl.Kind == "snow";
		bool bPlace = false;
		if (A.bGather)
		{
			// a gathering's activity: only where one of its mode is on now
			const FGathering* G = GatheringAt(Sl.Place);
			bPlace = G && (int)G->Mode == A.GatherMode;
		}
		else if (A.bStaff)
		{
			// at work: their own workplace, a slot of their role (or one for anybody of its staff)
			const FDuty* D = DutyFor(P, A);
			bPlace = D && D->Place == Sl.Place && (Sl.Tag.empty() || Sl.Tag == D->Role);
		}
		else if (bK)
		{
			bPlace = (A.bHome && Sl.Place == P.Home) || std::find(A.PlaceK.begin(), A.PlaceK.end(), Pl.K) != A.PlaceK.end();
		}
		else
		{
			for (const std::string& K : A.Places)
			{
				if (K == "home" ? Sl.Place == P.Home : K == Pl.Kind)
				{
					bPlace = true;
				}
			}
		}
		if (!bPlace || Pl.Island < 0 || (bSnow && Sl.SnowJob < 0))
		{
			return false;
		}
		if (Sl.Owner >= 0 && Sl.Owner != P.Id && !(bBed && bFlat && Sl.Place == P.Home && Sl.Capacity > 1))
		{
			return false;
		}
		// a bed: only their own (a sofa at home for a nap is fine)
		if (bBed && S != P.BedSlot)
		{
			return false;
		}
		// nobody else's flat
		if (bFlat && Sl.Place != P.Home)
		{
			return false;
		}
		// a sofa somebody of the household sleeps on is theirs from a while before their bedtime
		if (bSofa && bFlat)
		{
			for (const int Id : Pl.Residents)
			{
				const FPerson& Q = People[Id];
				if (Id != P.Id && Q.BedSlot == S && InHours(BedOf(Q) - 1.6f, WakeOf(Q), Hour))
				{
					return false;
				}
			}
		}
		// away (work in another town, school, the lab's gate): a door to leave the world by, not a seat: no room to keep
		return A.bAway || Sl.Free(P.Id);
	}

	int FWorld::PickSlot(const FPerson& P, const FActivity& A, float& OutMinutes) const
	{
		OutMinutes = A.MinMinutes;
		if (A.Id == "sleep" && P.BedSlot >= 0)
		{
			return P.BedSlot;
		}
		const V3 From = Where(P);
		const int Island = P.Island;
		int Best = -1;
		float BestScore = 1e30f;
		// the places to look at: the activity's kinds (indexed), their home, a gathering's places, their own workplace
		thread_local std::vector<int> Cand;
		Cand.clear();
		if (A.bGather)
		{
			for (const FGathering& G : Gatherings)
			{
				if (InHours(G.From, G.To, Hour) && (int)G.Mode == A.GatherMode)
				{
					Cand.insert(Cand.end(), G.Places.begin(), G.Places.end());
				}
			}
		}
		else if (A.bStaff)
		{
			if (const FDuty* D = DutyFor(P, A))
			{
				Cand.push_back(D->Place);
			}
		}
		else if (Indexed == Places.size())
		{
			Cand = A.PlaceIdx;
			if (P.Home >= 0 && Has(A.Places, "home"))
			{
				Cand.push_back(P.Home);
			}
		}
		else
		{
			for (int Pi = 0; Pi < (int)Places.size(); Pi++)
			{
				bool bKind = false;
				for (const std::string& K : A.Places)
				{
					bKind |= K == "home" ? Pi == P.Home : K == Places[Pi].Kind;
				}
				if (bKind)
				{
					Cand.push_back(Pi);
				}
			}
		}
		for (const int Pi : Cand)
		{
			const FPlace& Pl = Places[Pi];
			for (const int S : Pl.Slots)
			{
				if (!SlotFits(P, A, S))
				{
					continue;
				}
				const FSlot& Sl = Slots[S];
				float Score = (Sl.Approach - From).Len2D();
				if (Pl.Island != Island)
				{
					Score += 600.f;     // the cable car
				}
				// their usual seat in the club; others' usual seats only if they're free
				if (Sl.Kind == "club_seat")
				{
					if (Sl.Tag == P.ClubSeat)
					{
						Score -= 1000.f;
					}
					else
					{
						for (const FPerson& Q : People)
						{
							if (Q.Id != P.Id && Q.ClubSeat == Sl.Tag)
							{
								Score += 300.f;
							}
						}
					}
				}
				// a little variety among equal spots
				uint32_t H = (uint32_t)(S * 2654435761u) ^ (uint32_t)(P.Id * 40503u) ^ (uint32_t)(Time / 1800.0);
				Score += Rand(H) * 25.f;
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = S;
				}
			}
		}
		if (Best >= 0)
		{
			uint32_t R = P.Rng;
			OutMinutes = A.MinMinutes + (A.MaxMinutes - A.MinMinutes) * Rand(R);
		}
		return Best;
	}

	float FWorld::Score(const FPerson& P, const FActivity& A, int& OutSlot) const
	{
		OutSlot = -1;
		if (!A.bFree || P.Age < A.MinAge || P.Age > A.MaxAge || !InHours(A.From, A.To, Hour))
		{
			return -1e9f;
		}
		if ((A.bDaylight && !bSunUp) || !((A.Days >> Weekday) & 1))
		{
			return -1e9f;
		}
		if (A.bSunset && !(Hour > Sunset - 0.45f && Hour < Sunset + 0.2f))
		{
			return -1e9f;
		}
		if (A.bSunrise && !(Hour > Sunrise - 0.6f && Hour < Sunrise + 0.3f))
		{
			return -1e9f;
		}
		float Minutes = 0.f;
		const int S = PickSlot(P, A, Minutes);
		if (S < 0)
		{
			return -1e9f;
		}
		OutSlot = S;
		// time left before bed or the day's next duty: free time ends 20 minutes before it
		const float Left = FreeHoursLeft(P);
		const V3 From = Where(P);
		const FSlot& Sl = Slots[S];
		const float TravelH = WaySeconds(P, From, P.Island, S, Time) / HourS;
		const bool bHome = Sl.Place == P.Home;
		if (!bHome && Left - TravelH * 2.f - 0.33f < A.MinMinutes / 60.f)
		{
			return -1e9f;
		}
		// how much of each need it would meet over the time they'd spend on it; a need counts more the more urgent it is
		const float DurH = (A.MinMinutes + A.MaxMinutes) * 0.5f / 60.f;
		float Gain = 0.f;
		for (int N = 0; N < NumNeeds; N++)
		{
			const float Met = std::min(P.N[N], A.Need[N] * DurH);
			Gain += Met * (0.5f + P.N[N]) * 2.f - A.Cost[N] * DurH * 0.3f;
		}
		const float T = A.Trait.empty() ? 0.5f : Trait(P, A.Trait);
		float Score = A.Base + Gain * (0.6f + 0.8f * T);
		for (const auto& L : P.Likes)
		{
			if (L.first == A.Id)
			{
				Score += L.second;
			}
		}
		Score -= TravelH * Opt.WalkCostPerHour;
		// a holiday on: the town comes out (the sociable ones most; children and the old a little less late at night)
		if (A.bGather)
		{
			if (const FGathering* G = GatheringAt(Sl.Place))
			{
				const float Late = (Hour > 22.f || Hour < 5.f) && (P.bChild || P.Age > 70) ? 0.4f : 1.f;
				Score += G->Pull * (0.45f + 0.9f * P.Social) * Late;
			}
		}
		// friends there: company draws people (and the club's regulars)
		if (A.bTalk || Places[Sl.Place].Kind == "club")
		{
			int Friends = 0;
			for (const FPerson& Q : People)
			{
				if (Q.Id != P.Id && Q.Place == Sl.Place && Q.State != EState::Away && Has(P.Friends, Q.Key))
				{
					Friends++;
				}
			}
			Score += std::min(Friends, 3) * 0.08f * (0.5f + P.Social);
		}
		uint32_t R = P.Rng;
		for (const char Ch : A.Id)
		{
			R = R * 31u + (uint8_t)Ch;
		}
		R |= 1u;
		Score += (Rand(R) - 0.5f) * 2.f * Opt.Jitter;
		return Score;
	}

	V3 FWorld::Where(const FPerson& P) const
	{
		if (P.State == EState::Going || P.State == EState::Riding || P.State == EState::Waiting)
		{
			return P.Pos;
		}
		if (P.Slot >= 0 && Places[Slots[P.Slot].Place].bIndoor)
		{
			return Places[Slots[P.Slot].Place].Portal;    // indoors: counts from its door
		}
		return P.Pos;
	}

	float FWorld::YawAt(const FPerson& P, int Slot, EPose Pose) const
	{
		// lying on a sofa: along it, head to the sofa's left end (a bed's yaw is already head to foot)
		const FSlot& Sl = Slots[Slot];
		return Pose == EPose::Sleep && Sl.Kind != "bed" ? Sl.Yaw + 90.f : Sl.Yaw;
	}

	V3 FWorld::SpotOf(const FPerson& P, int Slot) const
	{
		// a double bed: each sleeper their own half
		const FSlot& Sl = Slots[Slot];
		if (Sl.Spots.size() > 1)
		{
			const auto It = std::find(Sl.Users.begin(), Sl.Users.end(), P.Id);
			const size_t I = It == Sl.Users.end() ? 0 : (size_t)(It - Sl.Users.begin());
			return Sl.Spots[std::min(I, Sl.Spots.size() - 1)];
		}
		return Sl.Pos;
	}

	void FWorld::Reserve(FPerson& P, int Slot)
	{
		Unreserve(P);
		if (Slot >= 0)
		{
			Slots[Slot].Users.push_back(P.Id);
			P.Reserved = Slot;
		}
	}

	void FWorld::Unreserve(FPerson& P)
	{
		// one slot at a time (Reserve): only that one to let go of (a slot's users cleared meanwhile: nothing there)
		if (P.Reserved >= 0 && P.Reserved < (int)Slots.size())
		{
			std::vector<int>& U = Slots[P.Reserved].Users;
			U.erase(std::remove(U.begin(), U.end(), P.Id), U.end());
		}
		P.Reserved = -1;
	}

	void FWorld::OutdoorWay(const V3& From, const V3& To, std::vector<FPerson::FWay>& Out) const
	{
		// off the walk grid (the sea island): straight, over its own ground
		for (const FPatch& Pa : Patches)
		{
			if (Pa.Has(From.X, From.Y) || Pa.Has(To.X, To.Y))
			{
				const float L = (To - From).Len2D();
				const int N = std::max(1, (int)std::ceil(L / 4.f));
				for (int K = 1; K <= N; K++)
				{
					FPerson::FWay W;
					W.P = From + (To - From) * ((float)K / N);
					W.P.Z = K == N ? To.Z : GroundAt(W.P);
					Out.push_back(W);
				}
				return;
			}
		}
		if (Town.Inside(From) && Town.Inside(To))
		{
			TownWay(From, To, Out);
			return;
		}
		std::vector<V3> Pts;
		if (!bHasGround || !Ground.FindPath(From, To, LyudAnimals::EKind::Dog, Pts))
		{
			Pts.assign(1, To);
		}
		for (const V3& P : Pts)
		{
			FPerson::FWay W;
			W.P = V3(P.X, P.Y, bHasGround ? Ground.GroundZ(P.X, P.Y) : P.Z);
			W.Kind = 0;
			Out.push_back(W);
		}
		Out.back().P = To;
	}

	bool FWorld::Route(const FPerson& P, const V3& From, int FromSlot, int ToSlot, std::vector<FPerson::FWay>& Out) const
	{
		Out.clear();
		const FSlot& To = Slots[ToSlot];
		const FPlace& ToPl = Places[To.Place];
		const FSlot* Fr = FromSlot >= 0 ? &Slots[FromSlot] : nullptr;
		const bool bFromInside = Fr && Places[Fr->Place].bIndoor && !Fr->Inside.empty();
		auto Push = [&](const V3& Pt, uint8_t Kind)
		{
			FPerson::FWay W;
			W.P = Pt;
			W.Kind = Kind;
			Out.push_back(W);
		};
		// within one indoor place: back to where the two ways part, then on
		if (bFromInside && Fr->Place == To.Place)
		{
			size_t Common = 0;
			while (Common < Fr->Inside.size() && Common < To.Inside.size() && Same(Fr->Inside[Common], To.Inside[Common]))
			{
				Common++;
			}
			for (int I = (int)Fr->Inside.size() - 2; I >= (int)Common - 1 && I >= 0; I--)
			{
				Push(Fr->Inside[I], 1);
			}
			for (size_t I = Common; I < To.Inside.size(); I++)
			{
				Push(To.Inside[I], 1);
			}
			if (Out.empty())
			{
				Push(To.Approach, 1);
			}
			return true;
		}
		V3 Out0 = From;
		int FromIsland = P.Island;
		if (bFromInside)
		{
			for (int I = (int)Fr->Inside.size() - 2; I >= 0; I--)
			{
				Push(Fr->Inside[I], 1);
			}
			Out0 = Places[Fr->Place].Portal;
			FromIsland = Places[Fr->Place].Island;
		}
		const V3 Goal = ToPl.bIndoor ? ToPl.Portal : To.Approach;
		const int ToIsland = ToPl.Island;
		if (ToIsland < 0)
		{
			return false;
		}
		std::vector<FHop> H;
		if (FromIsland >= 0 && !Hops(FromIsland, ToIsland, H))
		{
			return false;
		}
		V3 Cur = Out0;
		for (const FHop& Hop : H)
		{
			const FPlace& A = Places[Hop.A];
			const FPlace& B = Places[Hop.B];
			OutdoorWay(Cur, A.Portal, Out);
			if (Hop.Kind == 2)
			{
				// the cable car from this station to the other
				Out.back().Kind = 2;
				Out.back().Ride = Opt.RideSeconds;
				Out.back().ToIsland = (int8_t)Hop.To;
				Push(B.Portal, 0);
			}
			else
			{
				// the train: up onto the platform to a boarding point (people spread along it), the ride, out onto the other
				// halt's platform and down its stair
				const int SA = A.Slots[(size_t)(P.Id * 5) % A.Slots.size()];
				const int SB = B.Slots[(size_t)(P.Id * 3) % B.Slots.size()];
				for (size_t I = 1; I < A.RailWay.size(); I++)
				{
					Push(A.RailWay[I], 4);
				}
				Push(Slots[SA].Pos, 3);
				Out.back().ToIsland = (int8_t)Hop.To;
				Out.back().RailFrom = Hop.A;
				Out.back().RailTo = Hop.B;
				Push(Slots[SB].Pos, 4);
				for (int I = (int)B.RailWay.size() - 1; I >= 0; I--)
				{
					Push(B.RailWay[I], 4);
				}
				if (B.RailWay.empty())
				{
					Push(B.Portal, 4);
				}
			}
			Cur = B.Portal;
		}
		OutdoorWay(Cur, Goal, Out);
		if (ToPl.bIndoor)
		{
			for (size_t I = 1; I < To.Inside.size(); I++)
			{
				Push(To.Inside[I], 1);
			}
		}
		return !Out.empty();
	}

	bool FWorld::Start(FPerson& P, int Act, int Slot, double Minutes, bool bSnap, const char* Why)
	{
		if (Act < 0 || Slot < 0)
		{
			return false;
		}
		const FActivity& A = Activities[Act];
		const FSlot& Sl = Slots[Slot];
		const int FromSlot = P.Slot;
		P.SweepT = -1.f;
		if (P.Partner >= 0)
		{
			FPerson& Q = People[P.Partner];
			if (Q.Partner == P.Id)
			{
				Q.Partner = -1;
				Emit(EEvent::TalkEnd, Q, P.Id);
			}
			P.Partner = -1;
		}
		if (A.bAway)
		{
			Unreserve(P);       // gone from the world: holds no seat (any number go out by one door)
		}
		else
		{
			Reserve(P, Slot);
		}
		P.Activity = Act;
		P.Place = Sl.Place;
		P.Slot = Slot;
		P.Minutes = (float)Minutes;
		P.Why = Why;
		P.Choices++;
		// snapped asleep (a load, a clock jump): they've been asleep a while, no bedside lamp just switched on
		P.StateStart = bSnap && A.Id == "sleep" ? Time - 3600.0 : Time;
		Emit(EEvent::Start, P);
		if (bSnap)
		{
			P.Path.clear();
			P.PathI = 0;
			P.Pos = IsSeated(A.Pose) || A.Pose == EPose::Sleep || A.Pose == EPose::Work ? SpotOf(P, Slot) : Sl.Approach;
			P.Yaw = YawAt(P, Slot, A.Pose);
			P.bIndoors = Places[Sl.Place].bIndoor;
			P.Island = std::max(0, Places[Sl.Place].Island);
			P.Until = Time + Minutes * 60.0;
			P.State = A.bAway ? EState::Away : EState::Doing;
			P.Pose = A.bAway ? EPose::Hidden : A.Pose;
			P.WanderAt = Time + 60.0;
			CatchTrainBack(P);
			return true;
		}
		if (!Route(P, P.Pos, FromSlot, Slot, P.Path))
		{
			Unreserve(P);
			P.Activity = -1;
			P.Slot = -1;
			P.Why = std::string("no way: ") + Why;
			P.State = EState::Idle;
			P.Think = 30.f;
			return false;
		}
		P.PathI = 0;
		P.State = EState::Going;
		P.Pose = EPose::Walk;
		PlanBike(P);
		// late: a duty (not bed) that will have begun before they'd get there walking
		P.bHurry = false;
		float WayM = 0.f;
		bool bRide = false;
		for (int I = 0; I < (int)P.Path.size(); I++)
		{
			WayM += ((I ? P.Path[I - 1].P : P.Pos) - P.Path[I].P).Len();
			bRide |= P.Path[I].Kind == 2;
			WayM += P.Path[I].Kind == 3 ? Opt.TrainSeconds * P.WalkSpeed : 0.f;
		}
		const float ArriveH = Hour + (WayM / P.WalkSpeed + (bRide ? Opt.RideSeconds : 0.f)) / HourS;
		for (const FDuty& D : P.Duties)
		{
			if (D.Activity == A.Id && InHours(D.From - 0.5f, D.To, Hour) && InHours(D.From, D.To, std::fmod(ArriveH, 24.f)))
			{
				P.bHurry = true;
			}
		}
		return true;
	}

	bool FWorld::Interrupt(FPerson& P, float Dt)
	{
		// now and then people look up from what they do: something much better to do now (the sun going down), or a need
		// that can't wait (hunger, sleep) that this doesn't meet
		P.Recheck -= Dt;
		if (P.Recheck > 0.f || P.Activity < 0)
		{
			return false;
		}
		P.Recheck = 90.f + 60.f * Rand(P.Rng);
		if (P.Why == "ordered")
		{
			return false;
		}
		const FActivity& Cur = Activities[P.Activity];
		bool bSleep = false;
		const int Duty = DutyNow(P, bSleep);
		if (Duty == P.Activity && bSleep)
		{
			return false;
		}
		for (const ENeed N : { ENeed::Hunger, ENeed::Tired })
		{
			if (P.N[(int)N] > (N == ENeed::Hunger ? 0.85f : 0.96f) && Cur.Need[(int)N] <= 0.f)
			{
				// whoever works where food is (the restaurant's, the café's, the fair's people) eats there, in the kitchen
				if (N == ENeed::Hunger && Duty == P.Activity && P.bTown && P.Place >= 0)
				{
					const std::string& K = Places[P.Place].Kind;
					if (K == "restaurant" || K == "cafe" || K == "fair_stall" || K == "kiosk" || K == "market")
					{
						P.N[(int)ENeed::Hunger] = 0.15f;
						return false;
					}
				}
				if (Duty == P.Activity)
				{
					P.bBreak = true;    // a break from work to eat; back to it after
				}
				return true;
			}
		}
		// something they chose: they give it at least its shortest time first
		if (!Cur.bFree || Duty >= 0 || Time - P.StateStart < Cur.MinMinutes * 60.0)
		{
			return false;
		}
		int S = -1;
		const float Now = Score(P, Cur, S) + 0.25f;
		for (int I = 0; I < (int)Activities.size(); I++)
		{
			if (I == P.Activity)
			{
				continue;
			}
			int S2 = -1;
			if (Score(P, Activities[I], S2) > Now + 0.6f)
			{
				return true;
			}
		}
		return false;
	}

	void FWorld::Think(FPerson& P, bool bSnap)
	{
		P.Think = Opt.ThinkEvery * (0.7f + 0.6f * Rand(P.Rng));
		bool bSleep = false;
		int Duty = DutyNow(P, bSleep);
		const bool bBreakNow = P.bBreak && !bSleep;
		if (bBreakNow)
		{
			// a break: one free activity for the need that can't wait, then back to the duty
			P.bBreak = false;
			Duty = -1;
		}
		if (Duty >= 0)
		{
			float Minutes = 0.f;
			const int S = PickSlot(P, Activities[Duty], Minutes);
			// it lasts till the duty's end (sleep: till they wake)
			float End = bSleep ? WakeOf(P) : 0.f;
			for (const FDuty& D : P.Duties)
			{
				if (D.Activity == Activities[Duty].Id)
				{
					End = D.To;
				}
			}
			float Left = End - Hour;
			if (Left < 0.f)
			{
				Left += 24.f;
			}
			if (S >= 0)
			{
				Start(P, Duty, S, Left * 60.f + 1.f, bSnap, bSleep ? "bedtime" : "duty");
				return;
			}
			// nowhere for the duty now (its places full or cut off): free time for a while, not called back every step
			P.DutyBlockedUntil = Time + 900.0;
		}
		int Best = -1, BestSlot = -1;
		float BestScore = -1e8f;
		int Urgent = -1;
		if (bBreakNow)
		{
			Urgent = P.N[(int)ENeed::Hunger] >= P.N[(int)ENeed::Tired] ? (int)ENeed::Hunger : (int)ENeed::Tired;
		}
		// a break from work is taken at the work's own place when it has what's needed (the owner eats in the club, not
		// at home); otherwise anywhere
		const std::string WorkKind = bBreakNow && P.Slot >= 0 ? Places[Slots[P.Slot].Place].Kind : std::string();
		for (int Pass = WorkKind.empty() ? 1 : 0; Pass < 2 && Best < 0; Pass++)
		{
			for (int I = 0; I < (int)Activities.size(); I++)
			{
				if (Urgent >= 0 && Activities[I].Need[Urgent] <= 0.f)
				{
					continue;
				}
				if (Pass == 0 && !Has(Activities[I].Places, WorkKind))
				{
					continue;
				}
				int S = -1;
				const float Sc = Score(P, Activities[I], S);
				if (Sc > BestScore)
				{
					BestScore = Sc;
					Best = I;
					BestSlot = S;
				}
			}
		}
		if (Best < 0)
		{
			// nothing to do: go home
			const int Home = RestHomeAct >= 0 ? RestHomeAct : FindActivity("rest_home");
			float M = 0.f;
			if (Home >= 0)
			{
				const int S = PickSlot(P, Activities[Home], M);
				Start(P, Home, S, M, bSnap, "nothing else");
			}
			return;
		}
		float Minutes = 0.f;
		PickSlot(P, Activities[Best], Minutes);
		// free time ends before bed (or the next duty)
		const float Left = FreeHoursLeft(P);
		Minutes = std::max(Activities[Best].MinMinutes * 0.5f, std::min(Minutes, Left * 60.f - 25.f));
		if (bBreakNow)
		{
			Minutes = std::min(Minutes, 35.f);
		}
		char Why[96];
		std::snprintf(Why, sizeof Why, bBreakNow ? "a break %.2f" : "free %.2f", BestScore);
		Start(P, Best, BestSlot, Minutes, bSnap, Why);
		P.bOnBreak = bBreakNow;
	}

	void FWorld::UpdateNeeds(FPerson& P, float Dt)
	{
		const float H = Dt / HourS;
		const FActivity* A = P.Activity >= 0 && (P.State == EState::Doing || P.State == EState::Away) ? &Activities[P.Activity] : nullptr;
		const bool bAsleep = A && A->Pose == EPose::Sleep && P.State == EState::Doing;
		for (int N = 0; N < NumNeeds; N++)
		{
			float G = Grow[N];
			if (N == (int)ENeed::Tired)
			{
				G = bAsleep ? 0.f : (P.bChild ? 0.072f : 0.062f);
			}
			else if (N == (int)ENeed::Hunger && bAsleep)
			{
				G *= 0.3f;
			}
			else if (N == (int)ENeed::Social)
			{
				G *= 0.5f + P.Social;
			}
			else if (N == (int)ENeed::Fun)
			{
				G *= 0.6f + 0.8f * P.Gamer;
			}
			else if (N == (int)ENeed::Nature)
			{
				G *= 0.5f + P.NatureLove;
			}
			else if (N == (int)ENeed::Mind)
			{
				G *= 0.3f + 1.2f * P.Studious;
			}
			else if (N == (int)ENeed::Sun)
			{
				// people miss the sun by day; it is enough to be out under it
				G = bSunUp ? G * (0.4f + 1.2f * P.SunLove) : 0.f;
				if (bSunUp && !P.bIndoors && P.State != EState::Away && P.State != EState::Riding)
				{
					G -= 0.35f;
				}
			}
			if (bAsleep && N != (int)ENeed::Hunger && N != (int)ENeed::Tired)
			{
				G *= 0.2f;
			}
			float V = P.N[N] + G * H;
			if (A)
			{
				V -= A->Need[N] * H;
				V += A->Cost[N] * H;
			}
			if (N == (int)ENeed::Nature && !P.bIndoors && P.State == EState::Going)
			{
				V -= 0.15f * H;     // the way there is fresh air too
			}
			if (N == (int)ENeed::Social && P.Partner >= 0)
			{
				V -= 0.9f * H;
			}
			P.N[N] = Clamp01(V);
		}
	}

	float FWorld::Pace(const FPerson& P) const
	{
		if (P.Bike >= 0 && P.BikeMode == 2)
		{
			return P.Age > 60 ? std::min(BikeSpeed, 3.6f) : BikeSpeed;
		}
		if (P.Bike >= 0 && P.BikeMode == 1)
		{
			return P.WalkSpeed * 0.95f;
		}
		const bool bRush = P.bHurry || (bRain && !P.bIndoors && P.State == EState::Going);
		if (!bRush)
		{
			return P.WalkSpeed;
		}
		// no running on stairs and in rooms; outdoors a jog (~2.4 m/s; children a little faster for their size)
		return P.bIndoors ? P.WalkSpeed * 1.25f : std::max(2.3f, P.WalkSpeed * 1.95f);
	}

	void FWorld::Move(FPerson& P, float Dt)
	{
		// a stop with the bike (taking it, getting on or off, parking): they stand there till it's done
		if (P.BikeAct != EBikeAct::None)
		{
			P.Speed = 0.f;
			if (Time < P.BikeActUntil)
			{
				return;
			}
			if (P.BikeAct == EBikeAct::Park && P.Bike >= 0)
			{
				Bikes[P.Bike].Rider = -1;
				P.Bike = -1;
				P.BikeMode = 0;
			}
			P.BikeAct = EBikeAct::None;
		}
		const float Speed = Pace(P);
		float Budget = Speed * Dt;
		P.Speed = Speed;
		bool bLine = false;
		while (Budget > 0.f && P.PathI < (int)P.Path.size())
		{
			const FPerson::FWay& W = P.Path[P.PathI];
			V3 D = W.P - P.Pos;
			const bool bIn = W.Kind == 1;
			bLine = bIn || W.Kind == 3 || W.Kind == 4;     // along a line in 3D (stairs, a halt's stair and platform)
			if (!bLine)
			{
				D.Z = 0.f;
			}
			const float L = bLine ? D.Len() : D.Len2D();
			if (L > 0.01f)
			{
				P.Yaw = YawTo(P.Pos, W.P);
			}
			P.bIndoors = bIn;
			if (L <= Budget)
			{
				P.Pos = W.P;
				Budget -= L;
				if (W.Kind == 2)
				{
					// at the station: the cable car
					P.State = EState::Riding;
					P.Pose = EPose::Hidden;
					P.StateStart = Time;
					P.PathI++;
					P.bRideExternal = RideHook && P.PathI < (int)P.Path.size() && RideHook(P.Id, P.Pos, P.Path[P.PathI].P);
					Emit(EEvent::Ride, P);
					return;
				}
				if (W.Kind == 3)
				{
					// at the boarding point: the train
					BoardTrain(P);
					return;
				}
				P.PathI++;
				if (W.BikeAct != EBikeAct::None && P.Bike >= 0)
				{
					BikeStop(P, W.BikeAct);
					if (P.Bike >= 0 && P.BikeMode > 0)
					{
						Bikes[P.Bike].Pos = P.Pos;
						Bikes[P.Bike].Yaw = P.Yaw;
					}
					return;
				}
				continue;
			}
			P.Pos = P.Pos + D * (Budget / L);
			Budget = 0.f;
		}
		if (!P.bIndoors && !bLine)
		{
			P.Pos.Z = GroundAt(P.Pos);
		}
		if (P.Bike >= 0 && P.BikeMode > 0)
		{
			Bikes[P.Bike].Pos = P.Pos;
			Bikes[P.Bike].Yaw = P.Yaw;
		}
		if (P.PathI >= (int)P.Path.size())
		{
			Arrive(P);
		}
	}

	void FWorld::BoardTrain(FPerson& P)
	{
		const FPerson::FWay& W = P.Path[P.PathI];
		P.bTrain = true;
		P.RailFrom = W.RailFrom;
		P.RailTo = W.RailTo;
		P.PathI++;
		P.Speed = 0.f;
		P.StateStart = Time;
		const bool bStops = P.RailFrom >= 0 && P.RailTo >= 0;
		double Doors = 0.0;
		if (!bStops || !TrainNext || !TrainNext(Places[P.RailFrom].Stop, Places[P.RailTo].Stop, Time, Doors))
		{
			// no timetable (no railway in the level): gone for the ride, as the cable car once was
			P.State = EState::Riding;
			P.Pose = EPose::Hidden;
			P.RideUntil = Time + Opt.TrainSeconds;
			Emit(EEvent::Ride, P);
			return;
		}
		// wait on the platform facing the track
		P.DoorsAt = Doors;
		P.Think = 10.f;
		P.State = EState::Waiting;
		P.Pose = EPose::Stand;
		if (P.PathI < (int)P.Path.size())
		{
			for (const int S : Places[P.RailFrom].Slots)
			{
				if ((Slots[S].Pos - P.Pos).Len() < 0.05f)
				{
					P.Yaw = Slots[S].Yaw;
				}
			}
		}
	}

	void FWorld::CatchTrainBack(FPerson& P)
	{
		// on the sea island: they leave what they do in time to walk to the halt for a train back (the one nearest the
		// time they meant to stay, never less than half of what they came for)
		if (P.Slot < 0 || P.Activity < 0 || Places[Slots[P.Slot].Place].Island != 2 || !TrainNext)
		{
			return;
		}
		const int A = RailStopOf(2), B = RailStopOf(0);
		if (A < 0 || B < 0)
		{
			return;
		}
		const float Walk = (Places[A].Portal - Slots[P.Slot].Approach).Len2D() * 1.3f / P.WalkSpeed + 90.f;
		const double Earliest = Time + Activities[P.Activity].MinMinutes * 30.0;
		bool bBest = false;
		double Best = 0.0;
		for (const double From : { P.Until + Walk - 3600.0, P.Until + Walk })
		{
			double Doors = 0.0;
			if (!TrainNext(Places[A].Stop, Places[B].Stop, From, Doors))
			{
				continue;
			}
			const double Leave = Doors - Walk - 60.0;
			if (Leave >= Earliest && (!bBest || std::fabs(Leave - P.Until) < std::fabs(Best - P.Until)))
			{
				Best = Leave;
				bBest = true;
			}
		}
		double Doors = 0.0;
		if (!bBest && TrainNext(Places[A].Stop, Places[B].Stop, Earliest + Walk, Doors))
		{
			Best = Doors - Walk - 60.0;
			bBest = true;
		}
		if (bBest && Best > Time)
		{
			P.Until = Best;
		}
	}

	void FWorld::Arrive(FPerson& P)
	{
		DropBike(P);
		P.Speed = 0.f;
		P.bHurry = false;
		P.Path.clear();
		P.PathI = 0;
		if (P.Activity < 0 || P.Slot < 0)
		{
			P.State = EState::Idle;
			P.Pose = EPose::Stand;
			return;
		}
		const FActivity& A = Activities[P.Activity];
		const FSlot& Sl = Slots[P.Slot];
		P.bIndoors = Places[Sl.Place].bIndoor;
		P.Until = Time + P.Minutes * 60.0;
		P.StateStart = Time;
		P.WanderAt = Time + (A.bPauseSet ? A.PauseMin + (A.PauseMax - A.PauseMin) * Rand(P.Rng) : 30.0 + 60.0 * Rand(P.Rng));
		Emit(EEvent::Arrive, P);
		CatchTrainBack(P);
		if (A.bAway)
		{
			P.State = EState::Away;
			P.Pose = EPose::Hidden;
			Emit(EEvent::Away, P);
			return;
		}
		if (IsSeated(A.Pose) || A.Pose == EPose::Sleep || A.Pose == EPose::Work)
		{
			P.State = EState::Settling;
			P.Transit = A.Pose == EPose::Sleep ? LieSeconds : (A.Pose == EPose::Work ? 0.5f : SitSeconds);
			P.Pos = SpotOf(P, P.Slot);
			P.Yaw = YawAt(P, P.Slot, A.Pose);
			P.Pose = A.Pose;
			return;
		}
		P.State = EState::Doing;
		P.Pose = A.Pose;
		P.Yaw = A.Pose == EPose::Look ? (A.bSunrise ? SunriseYaw : SunYaw) : Sl.Yaw;
		if (A.bGather)
		{
			// a holiday: face the stage (or the fireworks over the sea) if there is one, else the spot's way
			const FGathering* G = GatheringAt(Sl.Place);
			P.Yaw = G && G->bFace ? YawTo(P.Pos, G->Face) : Sl.Yaw;
		}
	}

	void FWorld::Leave(FPerson& P)
	{
		const EPose Was = P.Pose;
		if (P.SweepT >= 0.f)
		{
			// called away half way along a stretch of snow: somebody else will finish it
			if (SnowProgress && P.Slot >= 0 && Slots[P.Slot].SnowJob >= 0)
			{
				SnowProgress(P.Id, Slots[P.Slot].SnowJob, -1.f);
			}
			P.SweepT = -1.f;
		}
		if (P.Partner >= 0)
		{
			FPerson& Q = People[P.Partner];
			if (Q.Partner == P.Id)
			{
				Q.Partner = -1;
				if (Q.Pose == EPose::Talk)
				{
					Q.Pose = EPose::Stand;
				}
				Emit(EEvent::TalkEnd, Q, P.Id);
			}
			P.Partner = -1;
		}
		Emit(EEvent::Leave, P);
		if (IsSeated(Was) || Was == EPose::Sleep)
		{
			P.State = EState::Leaving;
			P.Transit = Was == EPose::Sleep ? LieSeconds : SitSeconds;
			P.StateStart = Time;
			return;
		}
		Finish(P);
	}

	void FWorld::Finish(FPerson& P)
	{
		// they stay reserved on their slot till the next choice moves them: a stood-up person is at its approach
		if (P.Slot >= 0)
		{
			const FSlot& Sl = Slots[P.Slot];
			if (IsSeated(P.Pose) || P.Pose == EPose::Sleep || P.Pose == EPose::Work)
			{
				P.Pos = Sl.Approach;
			}
		}
		P.State = EState::Idle;
		P.Pose = EPose::Stand;
		P.Think = 0.f;
		P.bOnBreak = false;
	}

	void FWorld::Wander(FPerson& P)
	{
		// a walk: on to another spot of the same kinds (within ~200 m, or the activity's wander_m) on this side of the cable car
		if (Time < P.WanderAt)
		{
			return;
		}
		const FActivity& A = Activities[P.Activity];
		const int Island = P.Island;
		// only the places of the activity's kinds (or the gatherings' places); one picked at random as they come, no list
		int S = -1, Seen = 0;
		auto Look = [&](int PlI)
		{
			const FPlace& Pl = Places[PlI];
			if (Pl.bIndoor || Pl.Island != Island || (A.bGather ? GatheringAt(PlI) == nullptr : !Has(A.Places, Pl.Kind)))
			{
				return;
			}
			for (const int SI : Pl.Slots)
			{
				const FSlot& Sl = Slots[SI];
				if (SI == P.Slot || !Sl.Free(P.Id) || (Pl.Kind == "snow" && Sl.SnowJob < 0))
				{
					continue;
				}
				const float D = (Sl.Approach - P.Pos).Len2D();
				if (D > A.WanderNear && D < A.WanderFar && Rand(P.Rng) * (float)(++Seen) < 1.f)
				{
					S = SI;
				}
			}
		};
		if (A.bGather)
		{
			for (size_t GI = 0; GI < Gatherings.size(); GI++)
			{
				for (const int PlI : Gatherings[GI].Places)
				{
					// a place in two gatherings counts once
					bool bEarlier = false;
					for (size_t GJ = 0; GJ < GI && !bEarlier; GJ++)
					{
						bEarlier = std::find(Gatherings[GJ].Places.begin(), Gatherings[GJ].Places.end(), PlI) != Gatherings[GJ].Places.end();
					}
					if (!bEarlier)
					{
						Look(PlI);
					}
				}
			}
		}
		else if (Indexed == Places.size())
		{
			for (const int PlI : A.PlaceIdx)
			{
				Look(PlI);
			}
		}
		else
		{
			for (int PlI = 0; PlI < (int)Places.size(); PlI++)
			{
				Look(PlI);
			}
		}
		P.WanderAt = Time + 1e9;
		if (S < 0)
		{
			if (A.bSnow)
			{
				P.Until = std::min(P.Until, Time);   // nothing more to clear near here
			}
			return;
		}
		Reserve(P, S);
		P.Slot = S;
		P.Place = Slots[S].Place;
		P.Path.clear();
		OutdoorWay(P.Pos, Slots[S].Approach, P.Path);
		P.PathI = 0;
		P.Pose = EPose::Walk;
	}

	void FWorld::Sweep(FPerson& P, float Dt)
	{
		if (P.Slot < 0)
		{
			return;
		}
		FSlot& Sl = Slots[P.Slot];
		if (Sl.SnowJob < 0)
		{
			P.SweepT = -1.f;
			P.WanderAt = Time;
			Wander(P);
			return;
		}
		if (P.SweepT < 0.f)
		{
			// take the stretch (somebody else may have it after all)
			if (SnowProgress && !SnowProgress(P.Id, Sl.SnowJob, 0.f))
			{
				Sl.SnowJob = -1;
				P.WanderAt = Time;
				Wander(P);
				return;
			}
			P.SweepT = 0.f;
			P.SweepSent = 0.f;
		}
		// the pusher's pace (the Push clip's feet: 0.36 m/s); a long stretch takes a few minutes
		const float Pace = 0.36f;
		const float L = std::max((Sl.SnowTo - Sl.Pos).Len2D(), 0.5f);
		P.SweepT = std::min(1.f, P.SweepT + Pace * Dt / L);
		V3 At = Sl.Pos + (Sl.SnowTo - Sl.Pos) * P.SweepT;
		At.Z = GroundAt(At);
		P.Pos = At;
		P.Yaw = YawTo(Sl.Pos, Sl.SnowTo);
		P.Pose = EPose::Shovel;
		P.Speed = Pace;
		if (P.SweepT >= 1.f || P.SweepT - P.SweepSent > 0.04f)
		{
			if (SnowProgress)
			{
				SnowProgress(P.Id, Sl.SnowJob, P.SweepT);
			}
			P.SweepSent = P.SweepT;
		}
		if (P.SweepT >= 1.f)
		{
			// done: on to the next stretch near here, if there is one
			Sl.SnowJob = -1;
			P.SweepT = -1.f;
			P.Speed = 0.f;
			P.Pose = EPose::Stand;
			P.WanderAt = Time;
			Wander(P);
		}
	}

	void FWorld::UpdateSnowSlots(const V3& Near)
	{
		if (!SnowJobs)
		{
			return;
		}
		int Pi = FindPlace("snow_jobs");
		if (Pi < 0)
		{
			// one outdoor place whose slots stand for the stretches of path that want clearing now
			FPlace Pl;
			Pl.Id = "snow_jobs";
			Pl.Kind = "snow";
			Pl.Portal = Near;
			Pi = (int)Places.size();
			Places.push_back(Pl);
			for (int K = 0; K < 6; K++)
			{
				FSlot Sl;
				Sl.Kind = "shovel";
				Sl.Place = Pi;
				Sl.Pos = Sl.Approach = Sl.SnowTo = Near;
				Places[Pi].Slots.push_back((int)Slots.size());
				Slots.push_back(Sl);
			}
		}
		FPlace& Pl = Places[Pi];
		std::vector<int> Held, Free;
		for (const int S : Pl.Slots)
		{
			(Slots[S].Users.empty() ? Free : Held).push_back(S);
		}
		std::vector<FSnowJob> Jobs;
		SnowJobs(Near, (int)Pl.Slots.size() + 4, Jobs);
		size_t J = 0;
		for (const int S : Free)
		{
			FSlot& Sl = Slots[S];
			Sl.SnowJob = -1;
			for (; J < Jobs.size(); J++)
			{
				const FSnowJob& Jb = Jobs[J];
				bool bTaken = IslandOf(Jb.From) != 0;
				for (const int H : Held)
				{
					bTaken |= Slots[H].SnowJob == Jb.Job;
				}
				if (!bTaken)
				{
					Sl.SnowJob = Jb.Job;
					Sl.Pos = Sl.Approach = Jb.From;
					Sl.SnowTo = Jb.To;
					Sl.Yaw = YawTo(Jb.From, Jb.To);
					J++;
					break;
				}
			}
		}
		if (!Jobs.empty())
		{
			Pl.Portal = Jobs[0].From;
		}
	}

	void FWorld::TalkPairs()
	{
		// people who came to talk find somebody free at the same place within a few metres; friends first.
		// Everybody doing something is bucketed by place first (a town of ~900: no all-pairs scan)
		static thread_local std::vector<int> Head, Next;
		Head.assign(Places.size() + 1, -1);
		Next.assign(People.size(), -1);
		for (int I = (int)People.size() - 1; I >= 0; I--)
		{
			const FPerson& Q = People[I];
			if (Q.State == EState::Doing && Q.Partner < 0 && Q.Activity >= 0 && Q.Place >= -1 && Q.Place < (int)Places.size())
			{
				Next[I] = Head[Q.Place + 1];
				Head[Q.Place + 1] = I;
			}
		}
		for (FPerson& P : People)
		{
			if (P.State != EState::Doing || P.Partner >= 0 || P.Activity < 0 || !Activities[P.Activity].bTalk
				|| P.Place < -1 || P.Place >= (int)Places.size())
			{
				continue;
			}
			int Best = -1;
			float BestD = 1e9f;
			for (int QI = Head[P.Place + 1]; QI >= 0; QI = Next[QI])
			{
				FPerson& Q = People[QI];
				if (Q.Id == P.Id || Q.State != EState::Doing || Q.Partner >= 0)
				{
					continue;
				}
				const FActivity& QA = Activities[Q.Activity];
				const bool bFree = QA.bTalk || QA.Pose == EPose::Stand || QA.Pose == EPose::Sit;
				if (!bFree)
				{
					continue;
				}
				float D = (Q.Pos - P.Pos).Len();
				if (D >= 24.f)
				{
					continue;
				}
				if (Has(P.Friends, Q.Key))
				{
					D *= 0.5f;
				}
				if (D < 12.f && D < BestD)
				{
					BestD = D;
					Best = Q.Id;
				}
			}
			if (Best >= 0)
			{
				FPerson& Q = People[Best];
				P.Partner = Q.Id;
				Q.Partner = P.Id;
				// the one who came walks up to the other: they stand ~1 m apart facing each other
				const V3 D = (Q.Pos - P.Pos).Flat();
				const float L = D.Len();
				if (L > 1.1f && !IsSeated(P.Pose))
				{
					P.Path.clear();
					FPerson::FWay W;
					W.P = Q.Pos - D * (1.f / L);
					W.Kind = P.bIndoors ? 1 : 0;
					P.Path.push_back(W);
					P.PathI = 0;
				}
				if (!IsSeated(P.Pose))
				{
					P.Pose = EPose::Talk;
				}
				if (!IsSeated(Q.Pose))
				{
					Q.Pose = EPose::Talk;
				}
				P.Yaw = YawTo(P.Pos, Q.Pos);
				Q.Yaw = YawTo(Q.Pos, P.Pos);
				Emit(EEvent::TalkStart, P, Q.Id);
			}
		}
	}

	void FWorld::Step(float Dt)
	{
		if (Dt <= 0.f)
		{
			return;
		}
		Time += Dt;
		// everybody's day is rolled once a day at 14:00 (tonight's bedtime, tomorrow's waking)
		if (PrevHour >= 0.f && PrevHour < 14.f && Hour >= 14.f)
		{
			RollDays();
		}
		PrevHour = Hour;
		for (int N = 0; N < Opt.SettleBatch && SettleI < SettleQueue.size(); N++)
		{
			FPerson& P = People[SettleQueue[SettleI++]];
			if (P.State == EState::Idle && P.Activity < 0)
			{
				Think(P, true);
			}
		}
		PairTimer -= Dt;
		if (PairTimer <= 0.f)
		{
			PairTimer = 3.f;
			TalkPairs();
		}
		for (FPerson& P : People)
		{
			UpdateNeeds(P, Dt);
			switch (P.State)
			{
			case EState::Idle:
				P.Think -= Dt;
				if (P.Think <= 0.f)
				{
					Think(P);
				}
				break;
			case EState::Going:
				Move(P, Dt);
				break;
			case EState::Waiting:
				// the timetable may move under them (the railway's clock set): look again every 10 s till the doors are near
				P.Think -= Dt;
				if (P.Think <= 0.f && P.DoorsAt - Time > 15.0)
				{
					P.Think = 10.f;
					double Doors = 0.0;
					if (TrainNext && TrainNext(Places[P.RailFrom].Stop, Places[P.RailTo].Stop, Time, Doors))
					{
						P.DoorsAt = Doors;
					}
				}
				// the doors open: in they get (a few seconds apart); should no train come in two hours, gone as without one
				if (Time >= P.DoorsAt + 2.0 + (P.Id % 7) * 2.0 || Time - P.StateStart > 7200.0)
				{
					const bool bLate = Time < P.DoorsAt;
					const double Ride = TrainRide && P.RailFrom >= 0 && P.RailTo >= 0 ? TrainRide(Places[P.RailFrom].Stop, Places[P.RailTo].Stop) : -1.0;
					P.State = EState::Riding;
					P.Pose = EPose::Hidden;
					P.StateStart = Time;
					P.RideUntil = !bLate && Ride > 0.0 ? P.DoorsAt + Ride + 3.0 + (P.Id % 5) * 2.0 : Time + Opt.TrainSeconds;
					Emit(EEvent::Ride, P);
				}
				break;
			case EState::Riding:
				if (P.bTrain)
				{
					if (Time >= P.RideUntil)
					{
						FinishRide(P);
					}
				}
				else if (P.bRideExternal ? Time - P.StateStart >= 3600.0
					: Time - P.StateStart >= (P.PathI > 0 ? P.Path[P.PathI - 1].Ride : Opt.RideSeconds))
				{
					FinishRide(P);
				}
				break;
			case EState::Settling:
				P.Transit -= Dt;
				if (P.Transit <= 0.f)
				{
					P.Transit = 0.f;
					P.State = EState::Doing;
				}
				break;
			case EState::Doing:
			{
				bool bSleep = false;
				const int Duty = DutyNow(P, bSleep);
				// a duty starts (bed, work, school): drop what they're doing
				// (a break from work: the way there, 35 min at most, the way back)
				const bool bDutyCalls = Duty >= 0 && Duty != P.Activity && !(P.bOnBreak && !bSleep) && (bSleep || Time >= P.DutyBlockedUntil);
				// a stretch of snow half cleared is finished first
				const bool bMidSweep = P.SweepT >= 0.f;
				if ((Time >= P.Until && !bMidSweep) || bDutyCalls || (P.Path.empty() && !bMidSweep && Interrupt(P, Dt)))
				{
					Leave(P);
					break;
				}
				if (!P.Path.empty())
				{
					// a step within the activity (walking up to the one they talk to, a walk's next spot)
					const EPose Keep = P.Pose;
					float Budget = P.WalkSpeed * Dt;
					while (Budget > 0.f && P.PathI < (int)P.Path.size())
					{
						V3 D = P.Path[P.PathI].P - P.Pos;
						if (P.Path[P.PathI].Kind != 1)
						{
							D.Z = 0.f;
						}
						const float L = D.Len();
						if (L > 0.01f)
						{
							P.Yaw = YawTo(P.Pos, P.Path[P.PathI].P);
						}
						if (L <= Budget)
						{
							P.Pos = P.Path[P.PathI].P;
							Budget -= L;
							P.PathI++;
						}
						else
						{
							P.Pos = P.Pos + D * (Budget / L);
							Budget = 0.f;
						}
					}
					if (!P.bIndoors)
					{
						P.Pos.Z = GroundAt(P.Pos);
					}
					P.Speed = P.WalkSpeed;
					if (P.PathI >= (int)P.Path.size())
					{
						P.Path.clear();
						P.PathI = 0;
						P.Speed = 0.f;
						const FActivity& A = Activities[P.Activity];
						if (A.bSnow)
						{
							P.Pose = EPose::Shovel;
							P.SweepT = -1.f;
						}
						else if (A.bWander)
						{
							P.Pose = EPose::Look;
							P.Yaw = bSunUp && Rand(P.Rng) < 0.5f ? SunYaw : Slots[P.Slot].Yaw;
							P.WanderAt = Time + A.PauseMin + (A.PauseMax - A.PauseMin) * Rand(P.Rng);
						}
						else if (P.Partner >= 0)
						{
							P.Pose = EPose::Talk;
							P.Yaw = YawTo(P.Pos, People[P.Partner].Pos);
						}
						else
						{
							P.Pose = A.Pose;
						}
					}
					else
					{
						P.Pose = Keep == EPose::Talk ? EPose::Walk : Keep;
					}
					break;
				}
				if (Activities[P.Activity].bSnow)
				{
					Sweep(P, Dt);
				}
				else if (Activities[P.Activity].bWander)
				{
					Wander(P);
				}
				break;
			}
			case EState::Leaving:
				P.Transit -= Dt;
				if (P.Transit <= 0.f)
				{
					P.Transit = 0.f;
					Finish(P);
				}
				break;
			case EState::Away:
			{
				bool bSleep = false;
				const int Duty = DutyNow(P, bSleep);
				if (Time >= P.Until || (Duty >= 0 && Duty != P.Activity))
				{
					// back from town: out of the way they went
					P.State = EState::Idle;
					P.Pose = EPose::Stand;
					P.Think = 0.f;
					if (P.Slot >= 0)
					{
						P.Pos = Slots[P.Slot].Approach;
					}
					Emit(EEvent::Back, P);
				}
				break;
			}
			default:
				break;
			}
		}
	}

	void FWorld::Resettle()
	{
		for (FSlot& S : Slots)
		{
			S.Users.clear();
		}
		for (FPerson& P : People)
		{
			P.Path.clear();
			P.PathI = 0;
			P.Partner = -1;
			P.Activity = -1;
			P.Slot = -1;
			P.State = EState::Idle;
			P.bTrain = false;
			P.bRideExternal = false;
			P.Bike = -1;
			P.BikeMode = 0;
			P.BikeAct = EBikeAct::None;
			// how tired and hungry they'd be at this hour
			float Awake = Hour - WakeOf(P);
			if (Awake < 0.f)
			{
				Awake += 24.f;
			}
			P.N[(int)ENeed::Tired] = Clamp01(Awake * 0.055f);
			const float SinceMeal = std::fmod(Awake + 24.f, 5.f);
			P.N[(int)ENeed::Hunger] = Clamp01(0.1f + SinceMeal * 0.15f);
			if (P.Home >= 0)
			{
				P.Pos = Places[P.Home].Portal;
				P.Island = std::max(0, Places[P.Home].Island);
			}
		}
		// the bikes back in their entrances (one the player has or left somewhere stays)
		for (FBike& B : Bikes)
		{
			B.Rider = -1;
			if (!B.bHeld && B.Pos.Len2D() > 0.f && !B.Stair.empty())
			{
				B.Pos = B.Home;
				B.Yaw = B.HomeYaw;
			}
		}
		SettleQueue.clear();
		SettleI = 0;
		for (FPerson& P : People)
		{
			if (P.bTown && Opt.SettleBatch > 0)
			{
				// placed over the next steps (a few hundred decisions at once is a long frame): unseen and still till then
				P.Pose = EPose::Hidden;
				P.Think = 1e9f;
				SettleQueue.push_back(P.Id);
				continue;
			}
			Think(P, true);
		}
		TalkPairs();
	}

	void FWorld::NewGame()
	{
		Resettle();
		for (FPerson& P : People)
		{
			const int A = FindActivity(P.StartActivity);
			if (A < 0)
			{
				continue;
			}
			bool bSleep = false;
			const int Duty = DutyNow(P, bSleep);
			float Minutes = 0.f;
			Unreserve(P);
			P.Slot = -1;
			const int S = PickSlot(P, Activities[A], Minutes);
			if (S < 0)
			{
				continue;
			}
			if (Duty == A)
			{
				Think(P, true);
				continue;
			}
			Minutes = P.StartMinutes[0] + (P.StartMinutes[1] - P.StartMinutes[0]) * Rand(P.Rng);
			Start(P, A, S, Minutes, true, "the club scene");
		}
	}

	bool FWorld::Order(int Person, const std::string& Activity, const std::string& PlaceId)
	{
		if (Person < 0 || Person >= (int)People.size())
		{
			return false;
		}
		const int A = FindActivity(Activity);
		if (A < 0)
		{
			return false;
		}
		FPerson& P = People[Person];
		int Best = -1;
		float BestD = 1e30f;
		for (int S = 0; S < (int)Slots.size(); S++)
		{
			if (!PlaceId.empty() && Places[Slots[S].Place].Id != PlaceId)
			{
				continue;
			}
			if (!SlotFits(P, Activities[A], S) && !(Activity == "sleep" && S == P.BedSlot))
			{
				continue;
			}
			const float D = (Slots[S].Approach - Where(P)).Len2D() - (Slots[S].Tag == P.ClubSeat ? 1000.f : 0.f);
			if (D < BestD)
			{
				BestD = D;
				Best = S;
			}
		}
		if (Best < 0)
		{
			return false;
		}
		const FActivity& Act = Activities[A];
		return Start(P, A, Best, (Act.MinMinutes + Act.MaxMinutes) * 0.5f, false, "ordered");
	}

	float BikeActSeconds(EBikeAct A)
	{
		switch (A)
		{
		case EBikeAct::Take: return 2.f;
		case EBikeAct::Mount: return 1.8f;
		case EBikeAct::Dismount: return 1.5f;
		case EBikeAct::Park: return 1.6f;
		case EBikeAct::TakeMount: return 3.6f;
		default: return 0.f;
		}
	}

	int FWorld::AddBike(const std::string& Id, const std::string& Stair, const V3& Pos, float Yaw)
	{
		FBike B;
		B.Id = Id;
		B.Stair = Stair;
		B.Home = B.Pos = Pos;
		B.HomeYaw = B.Yaw = Yaw;
		Bikes.push_back(B);
		return (int)Bikes.size() - 1;
	}

	void FWorld::AssignBikes()
	{
		std::vector<std::string> Has;
		for (FBike& B : Bikes)
		{
			B.Owner.clear();
		}
		for (FBike& B : Bikes)
		{
			if (B.Stair.empty())
			{
				continue;
			}
			// the households up this stair, the first that has a grown-up who'd ride and no bike yet
			for (const FPerson& P : People)
			{
				if (P.Home < 0 || P.bChild || P.Age > 76 || Places[P.Home].Stair != B.Stair
					|| std::find(Has.begin(), Has.end(), P.Household) != Has.end())
				{
					continue;
				}
				B.Owner = P.Household;
				Has.push_back(P.Household);
				break;
			}
		}
	}

	void FWorld::SetBikeHeld(int Bike, bool bHeld, const V3& Pos, float Yaw)
	{
		if (Bike < 0 || Bike >= (int)Bikes.size())
		{
			return;
		}
		FBike& B = Bikes[Bike];
		if (bHeld && B.Rider >= 0)
		{
			// taken from under its owner's nose (before they got to it): they go on foot
			FPerson& P = People[B.Rider];
			if (P.Bike == Bike)
			{
				for (FPerson::FWay& W : P.Path)
				{
					W.Bike = 0;
					W.BikeAct = EBikeAct::None;
				}
				P.Bike = -1;
				P.BikeMode = 0;
				P.BikeAct = EBikeAct::None;
			}
			B.Rider = -1;
		}
		B.bHeld = bHeld;
		B.Pos = Pos;
		B.Yaw = Yaw;
	}

	void FWorld::DropBike(FPerson& P)
	{
		if (P.Bike < 0)
		{
			return;
		}
		FBike& B = Bikes[P.Bike];
		if (P.BikeMode > 0)
		{
			B.Pos = P.Pos;
			B.Yaw = P.Yaw;
		}
		B.Rider = -1;
		P.Bike = -1;
		P.BikeMode = 0;
		P.BikeAct = EBikeAct::None;
	}

	void FWorld::BikeStop(FPerson& P, EBikeAct Act)
	{
		FBike& B = Bikes[P.Bike];
		switch (Act)
		{
		case EBikeAct::Take: P.BikeMode = 1; P.Yaw = B.Yaw; break;
		case EBikeAct::TakeMount: P.BikeMode = 2; P.Yaw = B.Yaw; break;
		case EBikeAct::Mount: P.BikeMode = 2; break;
		case EBikeAct::Dismount: P.BikeMode = 1; break;
		case EBikeAct::Park:
			// in its place in the entrance as it always stands, else facing the way they came
			if ((P.Pos - B.Home).Len() < 0.05f)
			{
				B.Pos = B.Home;
				B.Yaw = B.HomeYaw;
			}
			else
			{
				B.Pos = P.Pos;
				B.Yaw = P.Yaw;
			}
			break;
		default: break;
		}
		P.BikeAct = Act;
		P.BikeActStart = Time;
		P.BikeActUntil = Time + BikeActSeconds(Act);
	}

	void FWorld::PlanBike(FPerson& P)
	{
		DropBike(P);
		if (P.bChild || P.Age > 76 || P.Path.empty() || Bikes.empty())
		{
			return;
		}
		int Bi = -1;
		for (int I = 0; I < (int)Bikes.size() && Bi < 0; I++)
		{
			const FBike& B = Bikes[I];
			if (!B.Owner.empty() && B.Owner == P.Household && B.Rider < 0 && !B.bHeld)
			{
				Bi = I;
			}
		}
		if (Bi < 0)
		{
			return;
		}
		// on the valley's ground only (no cable car, no train), and far enough
		int First = -1, Last = -1;
		for (int I = 0; I < (int)P.Path.size(); I++)
		{
			if (P.Path[I].Kind >= 2)
			{
				return;
			}
			if (P.Path[I].Kind == 0)
			{
				if (First < 0)
				{
					First = I;
				}
				else if (Last != I - 1)
				{
					return;          // out, in and out again: not a single stretch
				}
				Last = I;
			}
		}
		if (First < 0 || Last <= First)
		{
			return;
		}
		float L = 0.f;
		for (int I = First + 1; I <= Last; I++)
		{
			L += (P.Path[I].P - P.Path[I - 1].P).Len2D();
		}
		if (L < MinBikeWay)
		{
			return;
		}
		FBike& Bk = Bikes[Bi];
		const bool bAtHome = (Bk.Pos - Bk.Home).Len() < 0.05f && !Bk.Stair.empty();
		int TakeAt = -1;
		const V3 Start = First > 0 ? P.Path[First - 1].P : P.Pos;
		if (bAtHome)
		{
			for (int I = 0; I < First; I++)
			{
				if ((P.Path[I].P - Bk.Home).Len() < 0.05f)
				{
					TakeAt = I;
					break;
				}
			}
			if (TakeAt < 0)
			{
				return;              // not setting off by its stair
			}
		}
		else if ((Bk.Pos - Start).Len2D() > 25.f)
		{
			return;
		}
		int HomeAt = -1;
		for (int I = Last + 1; I < (int)P.Path.size() && !Bk.Stair.empty(); I++)
		{
			if ((P.Path[I].P - Bk.Home).Len() < 0.05f)
			{
				HomeAt = I;
				break;
			}
		}
		std::vector<FPerson::FWay> Out;
		auto Copy = [&](int I, uint8_t Bike)
		{
			Out.push_back(P.Path[I]);
			Out.back().Bike = Bike;
		};
		if (TakeAt >= 0)
		{
			// down the stair to the bike, out of the door with it, on outside
			for (int I = 0; I <= TakeAt; I++)
			{
				Copy(I, 0);
			}
			Out.back().BikeAct = EBikeAct::Take;
			for (int I = TakeAt + 1; I < First; I++)
			{
				Copy(I, 1);
			}
			if (Out.back().BikeAct == EBikeAct::Take)
			{
				Out.back().BikeAct = EBikeAct::TakeMount;
			}
			else
			{
				Out.back().BikeAct = EBikeAct::Mount;
			}
		}
		else
		{
			for (int I = 0; I < First; I++)
			{
				Copy(I, 0);
			}
			FPerson::FWay W;
			W.P = Bk.Pos;
			W.BikeAct = EBikeAct::TakeMount;
			Out.push_back(W);
		}
		// riding, off a few metres short of the goal
		const V3 Goal = P.Path[Last].P;
		int D = First;
		for (int I = First; I < Last; I++)
		{
			if ((P.Path[I].P - Goal).Len2D() >= 4.f)
			{
				D = I;
			}
		}
		for (int I = First; I <= D; I++)
		{
			Copy(I, 2);
		}
		Out.back().BikeAct = EBikeAct::Dismount;
		if (HomeAt >= 0)
		{
			// home: led in through the door to its place
			for (int I = D + 1; I <= HomeAt; I++)
			{
				Copy(I, 1);
			}
			Out.back().BikeAct = EBikeAct::Park;
			for (int I = HomeAt + 1; I < (int)P.Path.size(); I++)
			{
				Copy(I, 0);
			}
		}
		else
		{
			// left on its stand beside the way, a step short of the door
			const V3 From = P.Path[D].P;
			V3 Dir = (Goal - From).Flat();
			Dir = Dir.Len2D() > 0.1f ? Dir.Norm() : V3(1.f, 0.f, 0.f);
			const V3 Right(-Dir.Y, Dir.X, 0.f);
			V3 Spot = Goal - Dir * 1.6f + Right * 1.3f;
			Spot.Z = GroundAt(Spot);
			FPerson::FWay W;
			W.P = Spot;
			W.Bike = 1;
			W.BikeAct = EBikeAct::Park;
			Out.push_back(W);
			for (int I = D + 1; I < (int)P.Path.size(); I++)
			{
				Copy(I, 0);
			}
		}
		P.Path = Out;
		P.PathI = 0;
		P.Bike = Bi;
		P.BikeMode = 0;
		Bk.Rider = P.Id;
	}

	float FWorld::Ground_Z(const V3& P) const
	{
		return GroundAt(P);
	}

	std::string FWorld::Describe(int Id) const
	{
		if (Id < 0 || Id >= (int)People.size())
		{
			return "no such person";
		}
		const FPerson& P = People[Id];
		char Buf[512];
		std::snprintf(Buf, sizeof Buf, "%s (%d, %s) %s %s at %s/%s pose %s pos %.1f %.1f %.1f home %s | ", P.Key.c_str(), P.Age,
			P.Occupation.c_str(), StateName(P.State), P.Activity >= 0 ? Activities[P.Activity].Id.c_str() : "-",
			P.Place >= 0 ? Places[P.Place].Id.c_str() : "-", P.Slot >= 0 ? Slots[P.Slot].Kind.c_str() : "-", PoseName(P.Pose),
			P.Pos.X, P.Pos.Y, P.Pos.Z, P.Home >= 0 ? Places[P.Home].Id.c_str() : "none");
		std::string S = Buf;
		for (int N = 0; N < NumNeeds; N++)
		{
			std::snprintf(Buf, sizeof Buf, "%s %.2f ", NeedName((ENeed)N), P.N[N]);
			S += Buf;
		}
		S += "| " + P.Why;
		return S;
	}

	std::string FWorld::Scores(int Id) const
	{
		std::string S;
		if (Id < 0 || Id >= (int)People.size())
		{
			return S;
		}
		char Buf[96];
		for (const FActivity& A : Activities)
		{
			int Slot = -1;
			const float Sc = Score(People[Id], A, Slot);
			if (Sc > -1e8f)
			{
				std::snprintf(Buf, sizeof Buf, "%s %.2f ", A.Id.c_str(), Sc);
				S += Buf;
			}
		}
		return S;
	}

	std::string FWorld::Report() const
	{
		std::string S;
		char Buf[64];
		std::snprintf(Buf, sizeof Buf, "%02d:%02d ", (int)Hour, (int)(std::fmod(Hour, 1.f) * 60.f));
		S = Buf;
		// the story's people one by one; the townsfolk counted by what they do (there are hundreds)
		std::vector<std::pair<std::string, int>> TownActs;
		int NTown = 0;
		for (const FPerson& P : People)
		{
			const std::string Act = P.Activity >= 0 ? Activities[P.Activity].Id : std::string("-");
			if (!P.bTown)
			{
				S += P.Key + "=" + Act + "/" + StateName(P.State) + " ";
				continue;
			}
			NTown++;
			auto It = std::find_if(TownActs.begin(), TownActs.end(), [&](const std::pair<std::string, int>& E) { return E.first == Act; });
			if (It == TownActs.end())
			{
				TownActs.emplace_back(Act, 1);
			}
			else
			{
				It->second++;
			}
		}
		if (NTown > 0)
		{
			std::sort(TownActs.begin(), TownActs.end(), [](const std::pair<std::string, int>& A, const std::pair<std::string, int>& B) { return A.second > B.second; });
			S += "| townsfolk " + std::to_string(NTown) + ":";
			for (const auto& E : TownActs)
			{
				S += " " + E.first + " " + std::to_string(E.second);
			}
		}
		return S;
	}
}
