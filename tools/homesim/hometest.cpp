// The checks of the hero's room outside the engine (docs/hero_room.md): the rules of Private/Home/HomeModel.h on the real
// Content/Home/home.json. Built and run by tools/homesim/run.sh.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Home/HomeModel.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

static int GFails = 0;
static int GChecks = 0;

static void Check(bool bOk, const char* What)
{
	++GChecks;
	if (!bOk)
	{
		++GFails;
		std::printf("FAIL: %s\n", What);
	}
}

static LyudHome::FLook Look(bool bFlat, double D, double W, double H)
{
	LyudHome::FLook L;
	L.bFlat = bFlat;
	L.Depth = D;
	L.Width = W;
	L.Height = H;
	return L;
}

int main(int Argc, char** Argv)
{
	const std::string Path = Argc > 1 ? Argv[1] : "unreal/Lyudmilovsk/Content/Home/home.json";
	std::ifstream In(Path);
	if (!In)
	{
		std::printf("no %s: run python3 tools/home/home.py\n", Path.c_str());
		return 2;
	}
	std::stringstream Buf;
	Buf << In.rdbuf();
	LyudHome::FModel M;
	std::string Error;
	if (!M.Load(Buf.str(), Error))
	{
		std::printf("FAIL: %s does not load: %s\n", Path.c_str(), Error.c_str());
		return 1;
	}

	// --- the data: the room has what the design says, every place is inside its box, no two places share a spot
	int Cards = 0, Frames = 0, Shelves = 0, Pots = 0;
	std::set<std::string> Ids;
	for (const LyudHome::FSlot& S : M.Slots)
	{
		Cards += S.Type == "card";
		Frames += S.Type == "frame";
		Shelves += S.Type == "shelf";
		Pots += S.Type == "pot";
		Check(LyudHome::FModel::KnownType(S.Type), "a place has a known type");
		Check(Ids.insert(S.Id).second, "place ids are unique");
		Check(M.InBox(S.X, S.Y, S.Z), "a place is inside the room's box");
		for (const LyudHome::FSlot& O : M.Slots)
		{
			if (&O != &S)
			{
				const double D2 = (O.X - S.X) * (O.X - S.X) + (O.Y - S.Y) * (O.Y - S.Y) + (O.Z - S.Z) * (O.Z - S.Z);
				Check(D2 > 20.0 * 20.0, "two places are at least 20 cm apart");
			}
		}
	}
	Check(Cards >= 4 && Frames >= 2 && Shelves >= 6 && Pots == 1, "cards, nails, shelf spots and one pot");
	Check(M.Meshes.size() == 2, "the shell and the detail mesh");
	Check(!M.Lamps.empty() && !M.Species.empty() && !M.Trophies.empty(), "lamps, plants, trophies");
	Check(M.DoorY1 - M.DoorY0 >= 108.0 && M.DoorZ >= 210.0, "the doorway is a standard door or wider");
	Check(M.DoorY0 > M.WallY0 && M.DoorY1 < M.WallY1, "the doorway is in the hall's wall");
	Check(M.BoxMax[0] <= M.WallX1 + 1.0, "the room is behind the wall, not in the hall");
	Check(M.FindSlot("pot") >= 0 && M.FindSlot("nowhere") < 0, "FindSlot");

	// --- what goes where
	const LyudHome::FLook Postcard = Look(true, 0.2, 15, 10.5), Diploma = Look(true, 0.3, 21, 30), Sleeve = Look(true, 0.4, 31, 31);
	const LyudHome::FLook Bottle = Look(false, 6, 6, 24), Crate = Look(false, 40, 60, 40), Stamp = Look(true, 0.1, 3, 3);
	Check(LyudHome::FModel::Fits("card", Postcard) && !LyudHome::FModel::Fits("card", Sleeve) && !LyudHome::FModel::Fits("card", Stamp), "cards take small flat things");
	Check(LyudHome::FModel::Fits("frame", Diploma) && LyudHome::FModel::Fits("frame", Sleeve) && !LyudHome::FModel::Fits("frame", Bottle), "nails take bigger flat things");
	Check(LyudHome::FModel::Fits("shelf", Bottle) && LyudHome::FModel::Fits("shelf", Postcard) && !LyudHome::FModel::Fits("shelf", Crate), "shelves take what stands under the next board");
	Check(!LyudHome::FModel::Fits("pot", Bottle) && !LyudHome::FModel::Fits("", Bottle), "the pot takes no item");
	Check(LyudHome::FModel::TrophyFits("shelf") && !LyudHome::FModel::TrophyFits("card") && !LyudHome::FModel::TrophyFits("frame"), "trophies stand on shelves");

	// --- putting and taking
	Check(M.NumPlaced() == 0 && M.At("pin_1").empty(), "empty at first");
	Check(M.Put("pin_1", "item:postcard") && M.At("pin_1") == "item:postcard", "put");
	Check(!M.Put("pin_1", "item:daily_paper"), "a taken place takes nothing more");
	Check(!M.Put("nowhere", "item:postcard") && !M.Put("pot", "item:postcard") && !M.Put("pin_2", ""), "no such place, the pot, nothing");
	Check(M.Put("pin_2", "item:postcard"), "the same item twice, on two places");
	const std::string Cup = M.Trophies[0].Id;
	Check(M.FindTrophy(Cup) && !M.FindTrophy("cup:none"), "FindTrophy");
	Check(M.Put("shelf_1", Cup) && M.WhereIs(Cup) == "shelf_1", "a trophy on a shelf");
	Check(!M.Put("shelf_2", Cup), "a trophy stands in one place only");
	Check(!M.Put("pin_3", Cup), "a trophy does not hang on the pin board");
	Check(M.Take("pin_1") == "item:postcard" && M.At("pin_1").empty() && M.Take("pin_1").empty(), "take");
	Check(M.NumPlaced() == 2, "two things left");

	// --- the plant
	const long long D0 = 20000;
	Check(!M.HasPlant() && !M.Water(D0) && M.PlantStage(D0) == 0, "no plant: nothing to water");
	Check(!M.PlantIn("Oak", D0), "only the room's plants");
	Check(M.PlantIn(M.Species[0], D0) && M.HasPlant() && !M.PlantIn(M.Species[0], D0), "planted once");
	const double Age0 = M.PlantAge(D0);
	Check(Age0 > 0.05 && Age0 < 0.3, "a cutting on the first day");
	Check(M.Water(D0) && !M.Water(D0) && M.WateredToday(D0) && !M.WateredToday(D0 + 1), "watered once a day");
	LyudHome::FModel Lazy = M;
	for (int Day = 1; Day <= 30; ++Day)
	{
		Check(M.Water(D0 + Day), "watered the next day");
		Check(M.PlantAge(D0 + Day) > M.PlantAge(D0 + Day - 1), "it grows by the day");
	}
	Check(M.GrowthDays(D0 + 30) == 61.0 && Lazy.GrowthDays(D0 + 30) == 31.0, "a looked-after plant grows twice as fast");
	Check(M.PlantStage(D0 + 30) == 3 && Lazy.PlantStage(D0 + 30) == 2 && Lazy.PlantStage(D0 + 2) == 0 && Lazy.PlantStage(D0 + 5) == 1, "the stages");
	Check(M.PlantAge(D0 + 30) < 6.0, "the plant stays a house plant");
	Check(M.GrowthDays(D0 - 5) >= 0.0, "a clock moved back does not shrink it below a cutting");

	// --- the save
	M.bLight = false;
	std::map<std::string, std::string> Saved;
	M.SaveState(Saved);
	LyudHome::FModel N;
	Check(N.Load(Buf.str(), Error), "loads again");
	N.LoadState(Saved);
	Check(N.Placed == M.Placed && N.bLight == M.bLight, "the places and the lamp come back");
	Check(N.Plant.Species == M.Plant.Species && N.Plant.Day == M.Plant.Day && N.Plant.LastWater == M.Plant.LastWater && N.Plant.Watered == M.Plant.Watered,
		"the plant comes back");
	Check(N.Lost.empty(), "nothing lost");
	// a save from a room with a place this one no longer has: the item goes back to the pocket, a trophy just leaves
	Saved["p:gone_1"] = "item:postcard";
	Saved["p:gone_2"] = Cup;
	Saved["p:pin_4"] = "item:daily_paper";
	LyudHome::FModel O;
	Check(O.Load(Buf.str(), Error), "loads a third time");
	O.LoadState(Saved);
	Check(O.Lost.size() == 1 && O.Lost[0] == "postcard", "an item of a lost place is handed back");
	Check(O.At("pin_4") == "item:daily_paper" && O.At("gone_1").empty(), "the rest stands");
	// an empty save: a fresh room
	LyudHome::FModel P = M;
	P.LoadState(std::map<std::string, std::string>());
	Check(P.NumPlaced() == 0 && !P.HasPlant() && P.bLight, "an empty save is a fresh room");
	M.StartOver();
	Check(M.NumPlaced() == 0 && !M.HasPlant() && M.bLight, "a new game is a fresh room");

	std::printf("homesim: %d checks, %d failed (%d places: %d cards, %d nails, %d shelf spots; %d plants, %d trophies)\n", GChecks, GFails,
		(int)M.Slots.size(), Cards, Frames, Shelves, (int)M.Species.size(), (int)M.Trophies.size());
	return GFails ? 1 : 0;
}
