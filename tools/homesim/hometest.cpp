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
	Check(M.Put("top_1", "item:record_sea_1") && M.HoldsItem("record_sea_1") && M.Take("top_1") == "item:record_sea_1" && !M.HoldsItem("record_sea_1"),
		"a record on a shelf is held by the room, and no more once taken");

	// --- his own photographs: a print hangs on the board or a nail, once, and is no item
	typedef LyudHome::FModel FM;
	int Roll = -1, Frame = -1;
	Check(FM::PrintId(3, 12) == "print:3:12" && FM::ParsePrint("print:3:12", Roll, Frame) && Roll == 3 && Frame == 12, "a print's id both ways");
	Check(!FM::ParsePrint("print:", Roll, Frame) && !FM::ParsePrint("print:3", Roll, Frame) && !FM::ParsePrint("print:3:", Roll, Frame)
		&& !FM::ParsePrint("print::4", Roll, Frame) && !FM::ParsePrint("print:a:4", Roll, Frame) && !FM::ParsePrint("print:3:4:5", Roll, Frame)
		&& !FM::ParsePrint("item:print:3:4", Roll, Frame) && !FM::ParsePrint("print:12345678:1", Roll, Frame), "a broken print id is no print");
	Check(FM::PrintFits("card") && FM::PrintFits("frame") && !FM::PrintFits("shelf") && !FM::PrintFits("pot"), "prints go on the wall");
	Check(M.Put("pin_3", "print:1:2") && M.At("pin_3") == "print:1:2", "a print on the board");
	Check(!M.Put("pin_4", "print:1:2") && !M.Put("frame_1", "print:1:2"), "one print hangs in one place");
	Check(M.Put("frame_1", "print:1:3") && !M.Put("shelf_3", "print:1:4") && !M.Put("pin_4", "print:x"), "a nail takes a print, a shelf does not");
	Check(M.Take("pin_3") == "print:1:2" && M.Put("pin_4", "print:1:2"), "a print taken down hangs again elsewhere");
	Check(M.HoldsItem("postcard") && !M.HoldsItem("daily_paper") && !M.HoldsItem("print:1:2") && !M.HoldsItem(""), "HoldsItem");
	Check(M.NumPlacedOn("card") == 2 && M.NumPlacedOn("frame") == 1 && M.NumPlacedOn("shelf") == 1 && M.NumPlacedOn("pot") == 0, "NumPlacedOn");
	Check(M.NumPlaced() == 4, "four things now");

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

	// thirst: the leaves hang after days without water, a watering sets it right, a cactus keeps its own
	Check(Lazy.DryDays(D0) == 0 && Lazy.DryDays(D0 + 4) == 4 && !Lazy.Droops(D0 + 4) && Lazy.Droops(D0 + 5), "five dry days and the leaves hang");
	Check(!M.Droops(D0 + 30) && M.DryDays(D0 + 33) == 3, "a watered plant stands");
	{
		LyudHome::FModel Dry = Lazy;
		Check(Dry.Water(D0 + 9) && !Dry.Droops(D0 + 9) && !Dry.Droops(D0 + 13) && Dry.Droops(D0 + 14), "one watering sets it right");
		LyudHome::FModel Away = Lazy;
		const double Before = Away.GrowthDays(D0 + 7);
		Check(!Away.Mind(D0 + 5) && !Away.Mind(D0 + 6), "the owner does not come the first day it hangs");
		Check(Away.Mind(D0 + 7) && !Away.Droops(D0 + 7) && Away.Plant.Minded == 1 && Away.WateredToday(D0 + 7), "then he waters it once");
		Check(Away.GrowthDays(D0 + 7) == Before, "his watering is no care of the hero's: no extra growth");
		Check(!Away.Mind(D0 + 8) && !Away.Mind(D0 + 13) && Away.Mind(D0 + 14) && Away.Plant.Minded == 2, "and again only after another dry spell");
		LyudHome::FModel Empty = Lazy;
		Empty.Uproot();
		Check(!Empty.Mind(D0 + 99) && !Empty.Droops(D0 + 99) && Empty.DryDays(D0 + 99) == 0, "an empty pot needs nobody");
		bool bCactus = false;
		for (const std::string& Sp : M.Species)
		{
			Check(FM::Thirst(Sp) >= 5, "every plant stands some days dry");
			bCactus = bCactus || Sp == "Cactus";
		}
		Check(bCactus && FM::Thirst("Cactus") > FM::Thirst("Aloe") && FM::Thirst("Aloe") > FM::Thirst("Begonia"), "a cactus outlasts an aloe, an aloe a begonia");
		std::map<std::string, std::string> Kept;
		Away.SaveState(Kept);
		LyudHome::FModel Back;
		Check(Back.Load(Buf.str(), Error), "loads for the minded plant");
		Back.LoadState(Kept);
		Check(Back.Plant.Minded == 2 && Back.Plant.LastWater == Away.Plant.LastWater, "the owner's waterings are saved");
		Kept["plant"] = M.Species[0] + "|20000|20003|3";
		Back.LoadState(Kept);
		Check(Back.HasPlant() && Back.Plant.Watered == 3 && Back.Plant.Minded == 0, "an older save's plant still loads");
	}

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
	Saved["p:pin_5"] = "print:2:7";
	Saved["p:shelf_5"] = "print:2:8";
	Saved["p:frame_2"] = "print:oops";
	LyudHome::FModel O;
	Check(O.Load(Buf.str(), Error), "loads a third time");
	O.LoadState(Saved);
	Check(O.Lost.size() == 1 && O.Lost[0] == "postcard", "an item of a lost place is handed back");
	Check(O.At("pin_4") == "item:daily_paper" && O.At("gone_1").empty(), "the rest stands");
	Check(O.At("pin_5") == "print:2:7" && O.At("shelf_5").empty() && O.At("frame_2").empty(), "a print comes back to a wall place only, a broken one is dropped");
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
