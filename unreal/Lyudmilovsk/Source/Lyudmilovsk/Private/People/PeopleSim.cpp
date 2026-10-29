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
	static const char* PoseNames[] = { "hidden", "stand", "walk", "talk", "look", "sit", "computer", "eat", "read", "sleep", "work" };
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
		return LoadPlaces(Places_) && LoadActivities(Acts) && LoadPeople(Folk);
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
		for (const LyudJson::FValue& V : J["people"].Items)
		{
			FPerson P;
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

	float FWorld::GroundAt(const V3& P) const
	{
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
			int Size = 0;
			for (const FPerson& Q : People)
			{
				Size += Q.Household == P.Household ? 1 : 0;
			}
			int Pick = -1, PickBeds = -1;
			for (const int F : Flats)
			{
				if (Places[F].Residents.empty() && Beds(F) >= Size)
				{
					Pick = F;
					break;
				}
			}
			// none with enough beds left: the free flat with the most (the rest sleep on a sofa)
			for (const int F : Flats)
			{
				if (Pick < 0 && Places[F].Residents.empty() && Beds(F) > PickBeds)
				{
					Pick = -1;
					PickBeds = Beds(F);
				}
			}
			for (const int F : Flats)
			{
				if (Pick < 0 && PickBeds > 0 && Places[F].Residents.empty() && Beds(F) == PickBeds)
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
		if (InHours(P.BedTime, P.Wake, Hour))
		{
			bSleep = true;
			return FindActivity("sleep");
		}
		for (const FDuty& D : P.Duties)
		{
			// a weekday duty that started yesterday evening (a night shift) counts for yesterday
			const bool bDay = (D.Days >> Weekday) & 1;
			// set off a little early: they go there first
			if (bDay && InHours(D.From - 0.3f, D.To, Hour))
			{
				return FindActivity(D.Activity);
			}
		}
		return -1;
	}

	float FWorld::FreeHoursLeft(const FPerson& P) const
	{
		float Left = P.BedTime - Hour;
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
		if (!A.Slots.empty() && !Has(A.Slots, Sl.Kind))
		{
			return false;
		}
		bool bPlace = false;
		for (const std::string& K : A.Places)
		{
			if (K == "home" ? Sl.Place == P.Home : K == Pl.Kind)
			{
				bPlace = true;
			}
		}
		if (!bPlace || Pl.Island < 0)
		{
			return false;
		}
		if (Sl.Owner >= 0 && Sl.Owner != P.Id && !(Sl.Kind == "bed" && Pl.Kind == "flat" && Sl.Place == P.Home && Sl.Capacity > 1))
		{
			return false;
		}
		// a bed: only their own (a sofa at home for a nap is fine)
		if (Sl.Kind == "bed" && S != P.BedSlot)
		{
			return false;
		}
		// nobody else's flat
		if (Pl.Kind == "flat" && Sl.Place != P.Home)
		{
			return false;
		}
		// a sofa somebody of the household sleeps on is theirs from a while before their bedtime
		if (Sl.Kind == "sofa" && Pl.Kind == "flat")
		{
			for (const int Id : Pl.Residents)
			{
				const FPerson& Q = People[Id];
				if (Id != P.Id && Q.BedSlot == S && InHours(Q.BedTime - 1.6f, Q.Wake, Hour))
				{
					return false;
				}
			}
		}
		return Sl.Free(P.Id);
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
		for (int Pi = 0; Pi < (int)Places.size(); Pi++)
		{
			const FPlace& Pl = Places[Pi];
			bool bKind = false;
			for (const std::string& K : A.Places)
			{
				bKind |= K == "home" ? Pi == P.Home : K == Pl.Kind;
			}
			if (!bKind)
			{
				continue;
			}
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
		}
	}

	void FWorld::Unreserve(FPerson& P)
	{
		for (FSlot& S : Slots)
		{
			S.Users.erase(std::remove(S.Users.begin(), S.Users.end(), P.Id), S.Users.end());
		}
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
		Reserve(P, Slot);
		P.Activity = Act;
		P.Place = Sl.Place;
		P.Slot = Slot;
		P.Minutes = (float)Minutes;
		P.Why = Why;
		P.Choices++;
		P.StateStart = Time;
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
			float End = bSleep ? P.Wake : 0.f;
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
		}
		int Best = -1, BestSlot = -1;
		float BestScore = -1e8f;
		int Urgent = -1;
		if (bBreakNow)
		{
			Urgent = P.N[(int)ENeed::Hunger] >= P.N[(int)ENeed::Tired] ? (int)ENeed::Hunger : (int)ENeed::Tired;
		}
		for (int I = 0; I < (int)Activities.size(); I++)
		{
			if (Urgent >= 0 && Activities[I].Need[Urgent] <= 0.f)
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
		if (Best < 0)
		{
			// nothing to do: go home
			const int Home = FindActivity("rest_home");
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
				continue;
			}
			P.Pos = P.Pos + D * (Budget / L);
			Budget = 0.f;
		}
		if (!P.bIndoors && !bLine)
		{
			P.Pos.Z = GroundAt(P.Pos);
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
		P.Yaw = A.Pose == EPose::Look ? SunYaw : Sl.Yaw;
	}

	void FWorld::Leave(FPerson& P)
	{
		const EPose Was = P.Pose;
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
		std::vector<int> Options;
		for (int S = 0; S < (int)Slots.size(); S++)
		{
			const FSlot& Sl = Slots[S];
			const FPlace& Pl = Places[Sl.Place];
			if (S == P.Slot || Pl.bIndoor || Pl.Island != Island || !Has(A.Places, Pl.Kind) || !Sl.Free(P.Id))
			{
				continue;
			}
			const float D = (Sl.Approach - P.Pos).Len2D();
			if (D > A.WanderNear && D < A.WanderFar)
			{
				Options.push_back(S);
			}
		}
		P.WanderAt = Time + 1e9;
		if (Options.empty())
		{
			return;
		}
		const int S = Options[(size_t)(Rand(P.Rng) * Options.size()) % Options.size()];
		Reserve(P, S);
		P.Slot = S;
		P.Place = Slots[S].Place;
		P.Path.clear();
		OutdoorWay(P.Pos, Slots[S].Approach, P.Path);
		P.PathI = 0;
		P.Pose = EPose::Walk;
	}

	void FWorld::TalkPairs()
	{
		// people who came to talk find somebody free at the same place within a few metres; friends first
		for (FPerson& P : People)
		{
			if (P.State != EState::Doing || P.Partner >= 0 || P.Activity < 0 || !Activities[P.Activity].bTalk)
			{
				continue;
			}
			int Best = -1;
			float BestD = 1e9f;
			for (FPerson& Q : People)
			{
				if (Q.Id == P.Id || Q.State != EState::Doing || Q.Partner >= 0 || Q.Place != P.Place)
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
				const bool bDutyCalls = Duty >= 0 && Duty != P.Activity && !(P.bOnBreak && !bSleep);
				if (Time >= P.Until || bDutyCalls || (P.Path.empty() && Interrupt(P, Dt)))
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
						if (A.bWander)
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
				if (Activities[P.Activity].bWander)
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
			// how tired and hungry they'd be at this hour
			float Awake = Hour - P.Wake;
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
		for (FPerson& P : People)
		{
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
		for (const FPerson& P : People)
		{
			S += P.Key + "=" + (P.Activity >= 0 ? Activities[P.Activity].Id : std::string("-")) + "/" + StateName(P.State) + " ";
		}
		return S;
	}
}
