// The hero's own room in the club (docs/hero_room.md): what stands on which place, the plant in the pot, the lamp. Plain
// C++ with no engine types, so the rules are checked offline (tools/homesim/run.sh) on the real Content/Home/home.json.
#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "../People/MiniJson.h"

namespace LyudHome
{
	/** A place for one of the hero's things: a card on the pin board, a nail for something framed, a spot on a shelf, the pot. */
	struct FSlot
	{
		std::string Id;      // "pin_1" (the town's thing is "P-HOME/pin_1")
		std::string Type;    // card | frame | shelf | pot
		double X = 0, Y = 0, Z = 0;   // world cm
		double Yaw = 0;      // the way it faces, Unreal degrees
	};

	/** How big a thing is, as the item table says (cm). Flat: a card, a paper, a record. */
	struct FLook
	{
		bool bFlat = false;
		double Depth = 0, Width = 0, Height = 0;
	};

	/** A thing that is not an item: a cup won at a club tournament, the bird book. Shown while the hero has its stamp (Stamp empty:
	 *  always his). */
	struct FTrophy
	{
		std::string Id;      // "cup:cog_chess"
		std::string Stamp;   // "cups/cog_chess"
		std::string Name;    // string key
		std::string Shape;   // cup | book
		int Tint = 0;
	};

	struct FLamp
	{
		double X = 0, Y = 0, Z = 0, Lumens = 0, Radius = 0;
		std::string Colour;
	};

	/** The plant: its species ("" none), the day it was planted, the days it was watered on, the times somebody else watered
	 *  it while he was away. */
	struct FPlant
	{
		std::string Species;
		long long Day = 0;
		long long LastWater = -1;
		int Watered = 0;
		int Minded = 0;
	};

	inline bool StartsWith(const std::string& S, const char* Prefix)
	{
		return S.rfind(Prefix, 0) == 0;
	}

	class FModel
	{
	public:
		// --- the data (Content/Home/home.json, written by tools/home/home.py)
		double OriginX = 0, OriginY = 0, OriginZ = 0, Yaw = 0;
		std::vector<std::string> Meshes;
		std::vector<FSlot> Slots;
		std::vector<FLamp> Lamps;
		std::vector<FTrophy> Trophies;
		std::vector<std::string> Species;
		/** The room's box and the hole in the hall's wall (world cm). */
		double BoxMin[3] = { 0, 0, 0 }, BoxMax[3] = { 0, 0, 0 };
		double WallX0 = 0, WallX1 = 0, WallY0 = 0, WallY1 = 0, WallZ1 = 0, PaintZ = 0, DoorY0 = 0, DoorY1 = 0, DoorZ = 0;
		std::string WallMeshPrefix;

		// --- the state (saved)
		std::map<std::string, std::string> Placed;   // slot id -> "item:<id>" | a trophy id | "print:<roll>:<frame>"
		FPlant Plant;
		bool bLight = true;

		bool Load(const std::string& Json, std::string& Error)
		{
			LyudJson::FValue Root;
			if (!LyudJson::Parse(Json, Root, Error))
			{
				return false;
			}
			auto Num = [](const LyudJson::FValue& V, size_t I) { return V[I].F(); };
			const LyudJson::FValue& O = Root["origin"];
			OriginX = Num(O, 0);
			OriginY = Num(O, 1);
			OriginZ = Num(O, 2);
			Yaw = Root["yaw"].F();
			for (const LyudJson::FValue& M : Root["meshes"].Items)
			{
				Meshes.push_back(M.S());
			}
			for (const LyudJson::FValue& S : Root["slots"].Items)
			{
				FSlot Slot;
				Slot.Id = S["id"].S();
				Slot.Type = S["type"].S();
				Slot.X = Num(S["ue"], 0);
				Slot.Y = Num(S["ue"], 1);
				Slot.Z = Num(S["ue"], 2);
				Slot.Yaw = S["yaw"].F();
				if (Slot.Id.empty() || !KnownType(Slot.Type))
				{
					Error = "slot " + Slot.Id + ": unknown type " + Slot.Type;
					return false;
				}
				Slots.push_back(Slot);
			}
			for (const LyudJson::FValue& L : Root["lamps"].Items)
			{
				FLamp Lamp;
				Lamp.X = Num(L["ue"], 0);
				Lamp.Y = Num(L["ue"], 1);
				Lamp.Z = Num(L["ue"], 2);
				Lamp.Lumens = L["lumens"].F();
				Lamp.Radius = L["radius"].F();
				Lamp.Colour = L["colour"].S();
				Lamps.push_back(Lamp);
			}
			for (const LyudJson::FValue& T : Root["trophies"].Items)
			{
				FTrophy Trophy;
				Trophy.Id = T["id"].S();
				Trophy.Stamp = T["stamp"].S();
				Trophy.Name = T["name"].S();
				Trophy.Shape = T["shape"].S();
				Trophy.Tint = T["tint"].I();
				Trophies.push_back(Trophy);
			}
			for (const LyudJson::FValue& P : Root["plants"].Items)
			{
				Species.push_back(P.S());
			}
			const LyudJson::FValue& B = Root["box"];
			for (size_t I = 0; I < 3; ++I)
			{
				BoxMin[I] = Num(B["min"], I);
				BoxMax[I] = Num(B["max"], I);
			}
			const LyudJson::FValue& W = Root["wall"];
			WallX0 = Num(W["x"], 0);
			WallX1 = Num(W["x"], 1);
			WallY0 = Num(W["y"], 0);
			WallY1 = Num(W["y"], 1);
			WallZ1 = W["z1"].F();
			PaintZ = W["paint_z"].F();
			DoorY0 = Num(W["door_y"], 0);
			DoorY1 = Num(W["door_y"], 1);
			DoorZ = W["door_z"].F();
			WallMeshPrefix = W["mesh_prefix"].S();
			if (Slots.empty() || Meshes.empty())
			{
				Error = "no slots or no meshes";
				return false;
			}
			return true;
		}

		static bool KnownType(const std::string& Type)
		{
			return Type == "card" || Type == "frame" || Type == "shelf" || Type == "pot";
		}

		int FindSlot(const std::string& Id) const
		{
			for (size_t I = 0; I < Slots.size(); ++I)
			{
				if (Slots[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}

		const FTrophy* FindTrophy(const std::string& Id) const
		{
			for (const FTrophy& T : Trophies)
			{
				if (T.Id == Id)
				{
					return &T;
				}
			}
			return nullptr;
		}

		bool InBox(double X, double Y, double Z, double Margin = 0.0) const
		{
			return X >= BoxMin[0] - Margin && X <= BoxMax[0] + Margin && Y >= BoxMin[1] - Margin && Y <= BoxMax[1] + Margin
				&& Z >= BoxMin[2] - Margin && Z <= BoxMax[2] + Margin;
		}

		// --- what goes where

		/** Does an item of this look go on a place of this type? A card: a small flat thing (a postcard, a print, a ticket). A nail:
		 *  a bigger flat thing (the diploma, a record's sleeve, a newspaper). A shelf: anything that stands under the next board. */
		static bool Fits(const std::string& Type, const FLook& L)
		{
			const double Long = std::max(L.Width, L.Height);
			if (Type == "card")
			{
				return L.bFlat && Long >= 6.0 && Long <= 22.0;
			}
			if (Type == "frame")
			{
				return L.bFlat && Long >= 14.0 && Long <= 36.0;
			}
			if (Type == "shelf")
			{
				return L.Height > 0.0 && L.Height <= 36.0 && L.Width <= 40.0;
			}
			return false;
		}
		/** A trophy stands on a shelf; nowhere else. */
		static bool TrophyFits(const std::string& Type)
		{
			return Type == "shelf";
		}

		// --- his own photographs (docs/photo.md): a print of the album is "print:<roll>:<frame>". It is no item: the album keeps
		// the picture, the wall shows it. One print hangs in one place.
		static bool IsPrint(const std::string& What)
		{
			return StartsWith(What, "print:");
		}
		static std::string PrintId(int Roll, int Frame)
		{
			return "print:" + std::to_string(Roll) + ":" + std::to_string(Frame);
		}
		/** The roll and the frame of a print's id; false when it is not one. */
		static bool ParsePrint(const std::string& What, int& Roll, int& Frame)
		{
			if (!IsPrint(What))
			{
				return false;
			}
			const size_t Colon = What.find(':', 6);
			if (Colon == std::string::npos || Colon == 6 || Colon + 1 >= What.size())
			{
				return false;
			}
			for (size_t I = 6; I < What.size(); ++I)
			{
				if (I != Colon && (What[I] < '0' || What[I] > '9'))
				{
					return false;
				}
			}
			if (Colon - 6 > 6 || What.size() - Colon - 1 > 6)
			{
				return false;
			}
			Roll = std::atoi(What.substr(6, Colon - 6).c_str());
			Frame = std::atoi(What.substr(Colon + 1).c_str());
			return true;
		}
		/** A print is pinned to the board or hangs framed on a nail; it does not stand on a shelf. */
		static bool PrintFits(const std::string& Type)
		{
			return Type == "card" || Type == "frame";
		}
		/** Is this item standing somewhere in the room (a record on a shelf is still his record)? */
		bool HoldsItem(const std::string& Id) const
		{
			return !WhereIs("item:" + Id).empty();
		}
		/** How many places of a kind are taken: for the words about the room. */
		int NumPlacedOn(const std::string& Type) const
		{
			int N = 0;
			for (const FSlot& S : Slots)
			{
				N += S.Type == Type && Placed.count(S.Id) ? 1 : 0;
			}
			return N;
		}

		const std::string& At(const std::string& Slot) const
		{
			static const std::string None;
			const auto It = Placed.find(Slot);
			return It != Placed.end() ? It->second : None;
		}
		/** Where a trophy stands now ("" nowhere): one cup is one cup. */
		std::string WhereIs(const std::string& What) const
		{
			for (const auto& KV : Placed)
			{
				if (KV.second == What)
				{
					return KV.first;
				}
			}
			return std::string();
		}
		/** Puts What on a free place of the right kind; false when the place is taken, unknown or the pot. */
		bool Put(const std::string& Slot, const std::string& What)
		{
			const int I = FindSlot(Slot);
			if (I < 0 || What.empty() || Slots[(size_t)I].Type == "pot" || Placed.count(Slot))
			{
				return false;
			}
			int Roll = 0, Frame = 0;
			if (IsPrint(What))
			{
				if (!ParsePrint(What, Roll, Frame) || !PrintFits(Slots[(size_t)I].Type) || !WhereIs(What).empty())
				{
					return false;
				}
			}
			else if (!StartsWith(What, "item:") && (!FindTrophy(What) || !TrophyFits(Slots[(size_t)I].Type) || !WhereIs(What).empty()))
			{
				return false;
			}
			Placed[Slot] = What;
			return true;
		}
		/** Takes what stands on a place; "" when it was free. */
		std::string Take(const std::string& Slot)
		{
			const auto It = Placed.find(Slot);
			if (It == Placed.end())
			{
				return std::string();
			}
			const std::string What = It->second;
			Placed.erase(It);
			return What;
		}
		int NumPlaced() const { return (int)Placed.size(); }

		// --- the plant

		bool HasPlant() const { return !Plant.Species.empty(); }
		bool PlantIn(const std::string& Sp, long long Day)
		{
			if (HasPlant() || std::find(Species.begin(), Species.end(), Sp) == Species.end())
			{
				return false;
			}
			Plant = FPlant();
			Plant.Species = Sp;
			Plant.Day = Day;
			return true;
		}
		void Uproot() { Plant = FPlant(); }
		bool WateredToday(long long Day) const { return HasPlant() && Plant.LastWater == Day; }
		/** Once a day; false when there is nothing to water or it has had its water today. */
		bool Water(long long Day)
		{
			if (!HasPlant() || WateredToday(Day))
			{
				return false;
			}
			Plant.LastWater = Day;
			++Plant.Watered;
			return true;
		}
		/** How many days a plant stands without water before its leaves hang: a cactus and an aloe keep their own. */
		static int Thirst(const std::string& Sp)
		{
			return Sp == "Cactus" ? 21 : Sp == "Aloe" ? 14 : 5;
		}
		/** The days since its last water (since it was planted, when it has had none). */
		long long DryDays(long long Day) const
		{
			return HasPlant() ? std::max(0LL, Day - std::max(Plant.Day, Plant.LastWater)) : 0;
		}
		/** Its leaves hang: it has had no water for too long. It does not die: one watering sets it right. */
		bool Droops(long long Day) const
		{
			return HasPlant() && DryDays(Day) >= Thirst(Plant.Species);
		}
		/** Somebody looks in while he is away: when the plant has hung for two days and more, the owner waters it once (it
		 *  does not count as his care: no extra growth). True when it happened today: the engine leaves a note. */
		bool Mind(long long Day)
		{
			if (!HasPlant() || DryDays(Day) < Thirst(Plant.Species) + 2)
			{
				return false;
			}
			Plant.LastWater = Day;
			++Plant.Minded;
			return true;
		}
		/** The days of growth it has had: one a day, and one more for every day it was watered (so a looked-after plant grows
		 *  twice as fast as a forgotten one, and a forgotten one still lives). */
		double GrowthDays(long long Day) const
		{
			return HasPlant() ? (double)std::max(0LL, Day - Plant.Day) + (double)Plant.Watered : 0.0;
		}
		/** Its age for the plant generator, years: a cutting at first, about grown after a month and a half of care. */
		double PlantAge(long long Day) const
		{
			return 0.12 + GrowthDays(Day) * 0.05;
		}
		/** 0 a cutting, 1 rooted, 2 grown, 3 big: for the words. */
		int PlantStage(long long Day) const
		{
			const double G = GrowthDays(Day);
			return G < 4.0 ? 0 : G < 16.0 ? 1 : G < 45.0 ? 2 : 3;
		}

		// --- the save

		void SaveState(std::map<std::string, std::string>& Out) const
		{
			for (const auto& KV : Placed)
			{
				Out["p:" + KV.first] = KV.second;
			}
			if (HasPlant())
			{
				Out["plant"] = Plant.Species + "|" + std::to_string(Plant.Day) + "|" + std::to_string(Plant.LastWater) + "|" + std::to_string(Plant.Watered)
					+ "|" + std::to_string(Plant.Minded);
			}
			Out["light"] = bLight ? "1" : "0";
		}
		void LoadState(const std::map<std::string, std::string>& In)
		{
			Placed.clear();
			Plant = FPlant();
			bLight = true;
			for (const auto& KV : In)
			{
				if (StartsWith(KV.first, "p:"))
				{
					const std::string Slot = KV.first.substr(2);
					const int I = FindSlot(Slot);
					// a place or a trophy the data no longer has is dropped (an item goes back to the pocket: the engine asks Lost)
					int Roll = 0, Frame = 0;
					if (I >= 0 && Slots[(size_t)I].Type != "pot" && (StartsWith(KV.second, "item:") || FindTrophy(KV.second)
						|| (ParsePrint(KV.second, Roll, Frame) && PrintFits(Slots[(size_t)I].Type))))
					{
						Placed[Slot] = KV.second;
					}
					else if (StartsWith(KV.second, "item:"))
					{
						Lost.push_back(KV.second.substr(5));
					}
				}
				else if (KV.first == "plant")
				{
					std::vector<std::string> Parts;
					size_t From = 0;
					for (;;)
					{
						const size_t Bar = KV.second.find('|', From);
						Parts.push_back(KV.second.substr(From, Bar == std::string::npos ? std::string::npos : Bar - From));
						if (Bar == std::string::npos)
						{
							break;
						}
						From = Bar + 1;
					}
					// (a save from before the owner minded the plant has four parts)
					if ((Parts.size() == 4 || Parts.size() == 5) && std::find(Species.begin(), Species.end(), Parts[0]) != Species.end())
					{
						Plant.Species = Parts[0];
						Plant.Day = std::atoll(Parts[1].c_str());
						Plant.LastWater = std::atoll(Parts[2].c_str());
						Plant.Watered = std::atoi(Parts[3].c_str());
						Plant.Minded = Parts.size() > 4 ? std::atoi(Parts[4].c_str()) : 0;
					}
				}
				else if (KV.first == "light")
				{
					bLight = KV.second != "0";
				}
			}
		}
		void StartOver()
		{
			Placed.clear();
			Plant = FPlant();
			bLight = true;
			Lost.clear();
		}
		/** Items of a save whose place is gone: the engine gives them back and clears this. */
		std::vector<std::string> Lost;
	};
}
