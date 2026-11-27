#include "KitchenModel.h"

#include <algorithm>

namespace LyudKitchen
{
	const char* const AnyFish = "fish";

	const std::vector<FRecipe>& Recipes()
	{
		// the order is the page's order (1 .. 5)
		static const std::vector<FRecipe> All = {
			{ "ukha", "dish_ukha", 2, 1, EHeat::Plate, 40.0, "steam_hiss",
				{ { "fish", 1 }, { "potatoes", 2 }, { "dill", 1 } },
				{ { 0.30, "skim" }, { 0.75, "salt" } } },
			{ "fried_potatoes", "dish_fried_potatoes", 1, 1, EHeat::Plate, 30.0, "pan_sizzle",
				{ { "potatoes", 3 }, { "dill", 1 } },
				{ { 0.35, "stir" }, { 0.70, "stir" } } },
			{ "fried_fish", "dish_fried_fish", 1, 0, EHeat::Plate, 24.0, "pan_sizzle",
				{ { "fish", 1 } },
				{ { 0.50, "turn" } } },
			{ "salad", "dish_salad", 1, 1, EHeat::Plate, 8.0, "",
				{ { "tomatoes", 2 }, { "radish", 2 }, { "dill", 1 } },
				{ { 0.60, "salt" } } },
			{ "jam", "jam_strawberry", 1, 0, EHeat::Stove, 45.0, "steam_hiss",
				{ { "strawberries", 6 } },
				{ { 0.25, "skim" }, { 0.55, "stir" }, { 0.85, "skim" } } },
			// taught by the town (docs/kitchen.md "Recipes the town teaches"): Baltic dishes of their season
			{ "okroshka", "dish_okroshka", 1, 1, EHeat::Plate, 10.0, "",
				{ { "kefir", 1 }, { "radish", 2 }, { "potatoes", 1 }, { "dill", 1 } },
				{ { 0.55, "salt" } }, "recipes/okroshka", Season({ 6, 7, 8 }) },
			{ "fried_smelt", "dish_fried_smelt", 1, 1, EHeat::Plate, 22.0, "pan_sizzle",
				{ { "fish_smelt", 3 } },
				{ { 0.40, "flour" }, { 0.70, "turn" } }, "recipes/fried_smelt", Season({ 3, 4, 5 }) },
			{ "herring_potatoes", "dish_herring_potatoes", 2, 0, EHeat::Plate, 36.0, "steam_hiss",
				{ { "fish_herring", 2 }, { "potatoes", 2 }, { "dill", 1 } },
				{ { 0.35, "skim" }, { 0.80, "salt" } }, "recipes/herring_potatoes", Season({ 8, 9, 10, 11 }) },
			{ "draniki", "dish_draniki", 1, 1, EHeat::Plate, 28.0, "pan_sizzle",
				{ { "potatoes", 4 } },
				{ { 0.30, "grate" }, { 0.65, "turn" } }, "recipes/draniki", Season({ 9, 10, 11, 12, 1, 2, 3 }) },
			{ "carrot_salad", "dish_carrot_salad", 1, 1, EHeat::Plate, 8.0, "",
				{ { "carrot", 2 }, { "apple", 1 } },
				{ { 0.50, "grate" } }, "recipes/carrot_salad", 0 },
		};
		return All;
	}

	unsigned Season(std::initializer_list<int> Months)
	{
		unsigned Bits = 0;
		for (const int M : Months)
		{
			if (M >= 1 && M <= 12)
			{
				Bits |= 1u << M;
			}
		}
		return Bits;
	}

	bool Knows(const FRecipe& Recipe, const std::function<bool(const std::string& Stamp)>& HasStamp)
	{
		return Recipe.Learn.empty() || (HasStamp && HasStamp(Recipe.Learn));
	}

	int FindRecipe(const std::string& Id)
	{
		const std::vector<FRecipe>& All = Recipes();
		for (size_t I = 0; I < All.size(); ++I)
		{
			if (All[I].Id == Id)
			{
				return static_cast<int>(I);
			}
		}
		return -1;
	}

	std::vector<FNeed> Missing(const FRecipe& Recipe, const FPockets& Pockets)
	{
		std::vector<FNeed> Out;
		for (const FNeed& N : Recipe.Needs)
		{
			const int Has = Pockets ? Pockets(N.Item) : 0;
			if (Has < N.Count)
			{
				Out.push_back({ N.Item, N.Count - Has });
			}
		}
		return Out;
	}

	bool AnyToCook(EHeat Heat, const FPockets& Pockets, const std::function<bool(const std::string& Stamp)>& HasStamp)
	{
		for (const FRecipe& R : Recipes())
		{
			if (CanCookOn(R, Heat) && Knows(R, HasStamp) && Missing(R, Pockets).empty())
			{
				return true;
			}
		}
		return false;
	}

	double FPot::Progress() const
	{
		const std::vector<FRecipe>& All = Recipes();
		if (Recipe < 0 || Recipe >= static_cast<int>(All.size()))
		{
			return 0.0;
		}
		return std::min(1.0, T / All[static_cast<size_t>(Recipe)].Seconds);
	}

	bool Begin(FPot& Pot, int Recipe, EHeat Heat)
	{
		const std::vector<FRecipe>& All = Recipes();
		if (Pot.Busy() || Recipe < 0 || Recipe >= static_cast<int>(All.size()) || !CanCookOn(All[static_cast<size_t>(Recipe)], Heat))
		{
			return false;
		}
		Pot = FPot();
		Pot.State = EPot::Cooking;
		Pot.Recipe = Recipe;
		return true;
	}

	EPotEvent Step(FPot& Pot, double Dt)
	{
		if (!Pot.Busy() || Pot.State == EPot::Burnt || Dt <= 0.0)
		{
			return EPotEvent::None;
		}
		const FRecipe& R = Recipes()[static_cast<size_t>(Pot.Recipe)];
		Pot.T += Dt;
		if (Pot.State == EPot::Cooking)
		{
			// a step left waiting too long is a slip
			if (Pot.OpenStep >= 0 && Pot.T - Pot.OpenAt > StepWindow)
			{
				Pot.OpenStep = -1;
				Pot.Missed++;
				return EPotEvent::StepMissed;
			}
			if (Pot.OpenStep < 0 && Pot.NextStep < static_cast<int>(R.Steps.size()) && Pot.T >= R.Steps[static_cast<size_t>(Pot.NextStep)].At * R.Seconds)
			{
				Pot.OpenStep = Pot.NextStep++;
				Pot.OpenAt = Pot.T;
				return EPotEvent::StepOpened;
			}
			if (Pot.T >= R.Seconds && Pot.OpenStep < 0)
			{
				Pot.State = EPot::Ready;
				return EPotEvent::Ready;
			}
			return EPotEvent::None;
		}
		if (Pot.State == EPot::Ready && Pot.T >= R.Seconds + ReadyWindow)
		{
			Pot.State = EPot::Over;
			return EPotEvent::Overdone;
		}
		if (Pot.State == EPot::Over && Pot.T >= R.Seconds + ReadyWindow + OverWindow)
		{
			Pot.State = EPot::Burnt;
			return EPotEvent::Burnt;
		}
		return EPotEvent::None;
	}

	bool Tend(FPot& Pot)
	{
		if (Pot.State != EPot::Cooking || Pot.OpenStep < 0)
		{
			return false;
		}
		Pot.OpenStep = -1;
		Pot.Hit++;
		return true;
	}

	FResult TakeOff(FPot& Pot)
	{
		FResult Out;
		if (!CanTakeOff(Pot))
		{
			return Out;
		}
		const FRecipe& R = Recipes()[static_cast<size_t>(Pot.Recipe)];
		if (Pot.State != EPot::Burnt)
		{
			Out.bOk = true;
			Out.Dish = R.Dish;
			Out.Quality = std::max(1, 3 - Pot.Missed);
			if (Pot.State == EPot::Over)
			{
				Out.Quality = 1;
			}
			Out.Count = R.Yield + (Out.Quality >= 3 ? R.Bonus : 0);
		}
		Pot = FPot();
		return Out;
	}

	int JamQuality(int Berries, int Cooking)
	{
		const int B = std::min(3, std::max(1, Berries));
		const int C = std::min(3, std::max(1, Cooking));
		// good berries forgive nothing: the jar is as good as the worse of the two, a fine cooking lifts middling berries by none
		return std::min(B, C);
	}

	int VegPrice(const std::string& Item)
	{
		// a little over the market scales' best price (tools/garden/data.py SELL): the kitchen wants it today, not on Sunday
		static const struct { const char* Item; int Price; } Prices[] = {
			{ "strawberries", 9 }, { "tomatoes", 8 }, { "dill", 6 }, { "potatoes", 5 }, { "radish", 5 },
		};
		for (const auto& P : Prices)
		{
			if (Item == P.Item)
			{
				return P.Price;
			}
		}
		return 0;
	}

	std::vector<FVegPick> VegOffer(const FPockets& Pockets, int Left, int& Roubles)
	{
		static const char* const Order[] = { "strawberries", "tomatoes", "dill", "potatoes", "radish" };
		std::vector<FVegPick> Out;
		Roubles = 0;
		for (const char* Item : Order)
		{
			if (Left <= 0)
			{
				break;
			}
			const int Has = Pockets ? Pockets(Item) : 0;
			const int Take = std::min(Has, Left);
			if (Take > 0)
			{
				const int Sum = Take * VegPrice(Item);
				Out.push_back({ Item, Take, Sum });
				Roubles += Sum;
				Left -= Take;
			}
		}
		return Out;
	}
}
