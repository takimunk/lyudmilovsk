// The kitchen model's checks (docs/kitchen.md): recipes against the item table, the pot's timing, quality, the vegetables' sale.
#include "KitchenModel.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace LyudKitchen;

static int Checks = 0, Failed = 0;
#define CHECK(Cond, ...) do { ++Checks; if (!(Cond)) { ++Failed; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadAll(const std::string& Path)
{
	std::ifstream In(Path);
	std::stringstream S;
	S << In.rdbuf();
	return S.str();
}

/** Cooks a recipe: Tends says which of its steps the cook does (bit per step), TakeAfter how long after ready he takes it off. */
static FResult Cook(int Recipe, EHeat Heat, unsigned Tends, double TakeAfter, int* OutEvents = nullptr)
{
	FPot Pot;
	FResult None;
	if (!Begin(Pot, Recipe, Heat))
	{
		return None;
	}
	int Events = 0;
	double Ready = -1.0;
	for (int I = 0; I < 20000; ++I)
	{
		const EPotEvent E = Step(Pot, 0.05);
		if (E != EPotEvent::None)
		{
			++Events;
		}
		if (E == EPotEvent::StepOpened && (Tends & (1u << Pot.OpenStep)))
		{
			Tend(Pot);
		}
		if (E == EPotEvent::Ready)
		{
			Ready = Pot.T;
		}
		if (Ready >= 0.0 && Pot.T - Ready >= TakeAfter)
		{
			break;
		}
	}
	if (OutEvents)
	{
		*OutEvents = Events;
	}
	return TakeOff(Pot);
}

static int Test(const std::string& Root)
{
	const std::vector<FRecipe>& All = Recipes();
	CHECK(All.size() == 10 && static_cast<int>(All.size()) <= MaxPage, "ten recipes, the page's ten digits: %zu", All.size());
	const std::string Items = ReadAll(Root + "/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/LyudInventory.cpp");
	const std::string Food = ReadAll(Root + "/unreal/Lyudmilovsk/Content/Food/food.json");
	// the kitchen's words are in the town's tables (tools/city/content/strings/kitchen.json), the jam's in the garden's
	const std::string Loc = Root + "/unreal/Lyudmilovsk/Content/Localization/";
	const std::string Ru = ReadAll(Loc + "city/strings_ru.json") + ReadAll(Loc + "garden/strings_ru.json");
	const std::string En = ReadAll(Loc + "city/strings_en.json") + ReadAll(Loc + "garden/strings_en.json");
	const std::string Ja = ReadAll(Loc + "city/strings_ja.json") + ReadAll(Loc + "garden/strings_ja.json");
	const std::string Assets = ReadAll(Root + "/localization/assets.json");
	CHECK(!Items.empty() && !Food.empty() && !Ru.empty(), "the item table, food.json and the strings are read");
	std::map<std::string, int> Seen;
	for (const FRecipe& R : All)
	{
		CHECK(++Seen[R.Id] == 1, "recipe %s twice", R.Id.c_str());
		CHECK(Items.find("TEXT(\"" + R.Dish + "\")") != std::string::npos, "%s: the dish %s is not in LyudInventory.cpp", R.Id.c_str(), R.Dish.c_str());
		CHECK(Food.find("\"" + R.Dish + "\"") != std::string::npos, "%s: the dish %s is not in food.json (it could not be eaten)", R.Id.c_str(), R.Dish.c_str());
		CHECK(Assets.find("\"icon_" + R.Dish + "\"") != std::string::npos, "%s: no icon_%s in assets.json", R.Id.c_str(), R.Dish.c_str());
		for (const std::string* S : { &Ru, &En, &Ja })
		{
			CHECK(S->find("\"item." + R.Dish + ".name\"") != std::string::npos, "%s: item.%s.name is missing", R.Id.c_str(), R.Dish.c_str());
			CHECK(S->find("\"ui.cook.recipe." + R.Id + "\"") != std::string::npos, "ui.cook.recipe.%s is missing", R.Id.c_str());
		}
		CHECK(!R.Needs.empty() && R.Yield >= 1 && R.Seconds >= 5.0 && R.Seconds <= 60.0, "%s: needs, yield, seconds", R.Id.c_str());
		for (const FNeed& N : R.Needs)
		{
			CHECK(N.Count >= 1, "%s: need count", R.Id.c_str());
			if (N.Item.rfind("fish_", 0) == 0)
			{
				// the catch's items are made in code from the species (FishingModel.cpp): their names are in the main table
				CHECK(ReadAll(Loc + "strings_ru.json").find("\"item." + N.Item + ".name\"") != std::string::npos, "%s: no fish %s", R.Id.c_str(), N.Item.c_str());
			}
			else if (N.Item != AnyFish)
			{
				CHECK(Items.find("TEXT(\"" + N.Item + "\")") != std::string::npos, "%s: the ingredient %s is not an item", R.Id.c_str(), N.Item.c_str());
			}
		}
		double Last = 0.0;
		CHECK(!R.Steps.empty(), "%s: a step", R.Id.c_str());
		for (const FStep& S : R.Steps)
		{
			CHECK(S.At > Last && S.At < 1.0, "%s: steps in order inside the cooking", R.Id.c_str());
			// the next step must not open while this one still waits
			CHECK((S.At - Last) * R.Seconds >= 2.0, "%s: steps too close", R.Id.c_str());
			Last = S.At;
			for (const std::string* T : { &Ru, &En, &Ja })
			{
				CHECK(T->find("\"ui.cook.step." + S.What + "\"") != std::string::npos, "ui.cook.step.%s is missing", S.What.c_str());
			}
		}
		CHECK((1.0 - Last) * R.Seconds + 0.01 >= 0.0, "%s", R.Id.c_str());
	}
	CHECK(FindRecipe("ukha") == 0 && FindRecipe("jam") == 4 && FindRecipe("borscht") == -1, "FindRecipe");

	// the recipes the town teaches (docs/kitchen.md "Recipes the town teaches"): a stamp from a resident's answer
	{
		const std::string Content = ReadAll(Root + "/tools/city/content/talk.json") + ReadAll(Root + "/tools/city/content/x_kitchen.json");
		const std::string Collections = ReadAll(Root + "/tools/city/content/x_kitchen_recipes.json");
		const std::string Cook = ReadAll(Root + "/tools/city/content/x_kitchen.json");
		const auto None = [](const std::string&) { return false; };
		const auto Every = [](const std::string&) { return true; };
		int Taught = 0;
		for (const FRecipe& R : All)
		{
			CHECK(Cook.find("\"" + R.Id + "\"") != std::string::npos, "%s: not a piece of the collection cook (x_kitchen.json)", R.Id.c_str());
			if (R.Learn.empty())
			{
				CHECK(Knows(R, None) && R.Months == 0, "%s: a start recipe, known all year", R.Id.c_str());
				continue;
			}
			++Taught;
			CHECK(!Knows(R, None) && Knows(R, Every) && !Knows(R, nullptr), "%s: known only with its stamp", R.Id.c_str());
			CHECK(R.Learn == "recipes/" + R.Id, "%s: its stamp is recipes/<id> (%s)", R.Id.c_str(), R.Learn.c_str());
			CHECK(Content.find("\"collect\": \"" + R.Learn + "\"") != std::string::npos, "%s: no resident's answer gives %s", R.Id.c_str(), R.Learn.c_str());
			CHECK(Collections.find("\"" + R.Id + "\"") != std::string::npos, "%s: not a piece of the collection recipes", R.Id.c_str());
			int Months = 0;
			for (int M = 1; M <= 12; ++M) Months += InSeason(R, M) ? 1 : 0;
			CHECK(Months >= 3, "%s: in season %d months", R.Id.c_str(), Months);
		}
		CHECK(Taught == 5, "five taught recipes, %d", Taught);
		const FRecipe& Smelt = All[static_cast<size_t>(FindRecipe("fried_smelt"))];
		CHECK(InSeason(Smelt, 4) && !InSeason(Smelt, 8), "smelt in April, not in August");
		CHECK(InSeason(All[static_cast<size_t>(FindRecipe("carrot_salad"))], 1), "carrot salad all year");
		CHECK(Season({ 6, 7, 8 }) == ((1u << 6) | (1u << 7) | (1u << 8)) && Season({ 0, 13 }) == 0u, "Season bits");
		// a recipe of one kind of fish wants that kind, not any fish
		std::map<std::string, int> Catch = { { "fish_perch", 5 } };
		const FPockets P = [&Catch](const std::string& Item) { const auto It = Catch.find(Item); return It == Catch.end() ? 0 : It->second; };
		CHECK(Missing(Smelt, P).size() == 1 && Missing(Smelt, P)[0].Item == "fish_smelt", "perch is not smelt");
		Catch["fish_smelt"] = 3;
		CHECK(Missing(Smelt, P).empty(), "three smelt fry");
		// what he doesn't know isn't offered as "something to cook here"
		CHECK(!AnyToCook(EHeat::Plate, P) && AnyToCook(EHeat::Plate, P, Every), "smelt only once he has been taught it");
	}

	// the pockets
	std::map<std::string, int> Bag = { { "fish", 1 }, { "potatoes", 2 }, { "dill", 1 } };
	const FPockets Pockets = [&Bag](const std::string& Item) { const auto It = Bag.find(Item); return It == Bag.end() ? 0 : It->second; };
	CHECK(Missing(All[0], Pockets).empty(), "ukha: all there");
	const std::vector<FNeed> Lack = Missing(All[1], Pockets);
	CHECK(Lack.size() == 1 && Lack[0].Item == "potatoes" && Lack[0].Count == 1, "fried potatoes lack one potato");
	CHECK(AnyToCook(EHeat::Plate, Pockets), "something to cook on a plate");
	Bag = { { "strawberries", 6 } };
	CHECK(!AnyToCook(EHeat::Plate, Pockets) && AnyToCook(EHeat::Stove, Pockets), "jam only on the stove");
	Bag.clear();
	CHECK(!AnyToCook(EHeat::Stove, Pockets) && !AnyToCook(EHeat::Stove, nullptr), "empty pockets cook nothing");

	// the pot
	for (size_t R = 0; R < All.size(); ++R)
	{
		const unsigned AllSteps = (1u << All[R].Steps.size()) - 1u;
		int Events = 0;
		const FResult Fine = Cook(static_cast<int>(R), EHeat::Stove, AllSteps, 1.0, &Events);
		CHECK(Fine.bOk && Fine.Quality == 3 && Fine.Dish == All[R].Dish && Fine.Count == All[R].Yield + All[R].Bonus, "%s: fine: q %d count %d",
			All[R].Id.c_str(), Fine.Quality, Fine.Count);
		CHECK(Events == static_cast<int>(All[R].Steps.size()) + 1, "%s: an event a step and ready, %d", All[R].Id.c_str(), Events);
		const FResult Slip = Cook(static_cast<int>(R), EHeat::Stove, AllSteps & ~1u, 1.0);
		CHECK(Slip.bOk && Slip.Quality == 2 && Slip.Count == All[R].Yield, "%s: one slip: q %d count %d", All[R].Id.c_str(), Slip.Quality, Slip.Count);
		const FResult Idle = Cook(static_cast<int>(R), EHeat::Stove, 0u, 1.0);
		CHECK(Idle.bOk && Idle.Quality == std::max(1, 3 - static_cast<int>(All[R].Steps.size())), "%s: untended: q %d", All[R].Id.c_str(), Idle.Quality);
		const FResult Late = Cook(static_cast<int>(R), EHeat::Stove, AllSteps, ReadyWindow + 2.0);
		CHECK(Late.bOk && Late.Quality == 1 && Late.Count == All[R].Yield, "%s: overdone: q %d", All[R].Id.c_str(), Late.Quality);
		const FResult Burnt = Cook(static_cast<int>(R), EHeat::Stove, AllSteps, ReadyWindow + OverWindow + 2.0);
		CHECK(!Burnt.bOk && Burnt.Count == 0 && Burnt.Quality == 0, "%s: burnt", All[R].Id.c_str());
	}
	{
		FPot Pot;
		CHECK(!Begin(Pot, FindRecipe("jam"), EHeat::Plate) && !Pot.Busy(), "no jam on a hot plate");
		CHECK(Begin(Pot, FindRecipe("jam"), EHeat::Stove) && Pot.Busy(), "jam on the stove");
		CHECK(!Begin(Pot, 0, EHeat::Stove), "a busy pot takes no second dish");
		CHECK(!Tend(Pot), "nothing to tend yet");
		CHECK(!CanTakeOff(Pot) && !TakeOff(Pot).bOk && Pot.Busy(), "not off the fire before it is ready");
		// a step missed while the last one waits still lets it get ready after the slip
		int Missed = 0, Opened = 0;
		bool bReady = false;
		for (int I = 0; I < 4000 && !bReady; ++I)
		{
			const EPotEvent E = Step(Pot, 0.05);
			Missed += E == EPotEvent::StepMissed;
			Opened += E == EPotEvent::StepOpened;
			bReady = E == EPotEvent::Ready;
		}
		CHECK(bReady && Opened == 3 && Missed == 3, "jam untended: %d opened %d missed", Opened, Missed);
		CHECK(Pot.Progress() >= 1.0, "progress at ready");
		const FResult R = TakeOff(Pot);
		CHECK(R.bOk && R.Quality == 1 && !Pot.Busy() && Pot.Recipe == -1, "an untended jam is still jam");
		CHECK(Step(Pot, 1.0) == EPotEvent::None, "an empty pot does nothing");
	}
	CHECK(JamQuality(3, 3) == 3 && JamQuality(3, 1) == 1 && JamQuality(1, 3) == 1 && JamQuality(0, 9) == 1 && JamQuality(2, 3) == 2, "jam quality");

	// the restaurant's buying
	Bag = { { "strawberries", 5 }, { "tomatoes", 4 }, { "dill", 2 }, { "potatoes", 6 }, { "radish", 10 }, { "seeds", 3 } };
	int Roubles = 0;
	std::vector<FVegPick> Picks = VegOffer(Pockets, VegPerDay, Roubles);
	int Things = 0;
	for (const FVegPick& P : Picks) { Things += P.Count; CHECK(P.Roubles == P.Count * VegPrice(P.Item), "pick sum"); }
	CHECK(Things == VegPerDay && Roubles == 5 * 9 + 4 * 8 + 2 * 6 + 1 * 5, "a day's want, the dearest first: %d things %d roubles", Things, Roubles);
	Picks = VegOffer(Pockets, 0, Roubles);
	CHECK(Picks.empty() && Roubles == 0, "nothing more today");
	CHECK(VegPrice("seeds") == 0 && VegPrice("jam_strawberry") == 0, "only vegetables and berries");
	// the kitchen pays over the market's best (tools/garden/data.py SELL quality 3)
	const std::string Garden = ReadAll(Root + "/tools/garden/data.py");
	CHECK(Garden.find("\"radish\": [2, 3, 4], \"dill\": [2, 4, 5], \"potatoes\": [2, 3, 4], \"tomatoes\": [3, 5, 7], \"strawberries\": [4, 6, 8]") != std::string::npos,
		"the market's prices moved: look at VegPrice again");

	std::printf("kitchensim: %d checks, %d failed\n", Checks, Failed);
	return Failed ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	std::string Root = ".";
	if (const char* Env = std::getenv("LYUD_ROOT"))
	{
		Root = Env;
	}
	if (Mode == "recipes")
	{
		for (const FRecipe& R : Recipes())
		{
			std::printf("%-15s -> %s x%d(+%d)  %2.0f s  %s:", R.Id.c_str(), R.Dish.c_str(), R.Yield, R.Bonus, R.Seconds, R.Heat == EHeat::Stove ? "stove" : "plate");
			for (const FNeed& N : R.Needs) std::printf(" %s x%d", N.Item.c_str(), N.Count);
			std::printf(" |");
			for (const FStep& S : R.Steps) std::printf(" %s@%.0fs", S.What.c_str(), S.At * R.Seconds);
			std::printf("\n");
		}
		return 0;
	}
	return Test(Root);
}
