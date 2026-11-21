#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * Cooking from what the hero grew and caught (docs/kitchen.md): the recipes, the pot on the fire, the restaurant's buying of
 * fresh vegetables. Plain C++, no engine: ULyudKitchen (Kitchen/LyudKitchen) feeds it the pockets and the seconds and says
 * what it says; tools/kitchensim/run.sh checks it.
 */
namespace LyudKitchen
{
	/** What a place has to cook on: a hot plate (one ring: a room's), a stove (a canteen's, a kitchen's: big pots too). */
	enum class EHeat : uint8_t { Plate, Stove };

	/** "fish" in a recipe: any fish of the catch (fish_<species>), the biggest one in the pocket. */
	extern const char* const AnyFish;

	struct FNeed
	{
		std::string Item;
		int Count = 1;
	};

	/** A moment the pot asks for a hand: skim the froth, stir, turn the fish over. */
	struct FStep
	{
		/** When, as a share of the cooking time (0 .. 1). */
		double At = 0.5;
		/** "skim", "stir", "turn", "salt": the words are ui.cook.step.<what>. */
		std::string What;
	};

	struct FRecipe
	{
		std::string Id;
		/** The pocket item it gives and how many portions; one more when cooked without a slip. */
		std::string Dish;
		int Yield = 1;
		int Bonus = 0;
		/** The least fire it needs (jam wants the stove's big pan). */
		EHeat Heat = EHeat::Plate;
		/** Seconds on the fire (the hero's own, he stands by the pot). */
		double Seconds = 30.0;
		/** The sound it makes on the fire: "pan_sizzle" or "steam_hiss" (empty: a salad makes none). */
		std::string Sound;
		std::vector<FNeed> Needs;
		std::vector<FStep> Steps;
	};

	const std::vector<FRecipe>& Recipes();
	/** -1 when there is none. */
	int FindRecipe(const std::string& Id);

	/** How many of an item the pockets hold ("fish": every fish of the catch together). */
	using FPockets = std::function<int(const std::string& Item)>;

	/** What of the recipe the pockets lack (Count: how many more); empty when he can cook it. */
	std::vector<FNeed> Missing(const FRecipe& Recipe, const FPockets& Pockets);
	inline bool CanCookOn(const FRecipe& Recipe, EHeat Heat) { return static_cast<int>(Heat) >= static_cast<int>(Recipe.Heat); }
	/** Is there anything he could cook here now. */
	bool AnyToCook(EHeat Heat, const FPockets& Pockets);

	// ------------------------------------------------------------------ the pot

	/** A step waits for the hand this long, s. */
	constexpr double StepWindow = 5.0;
	/** A ready dish waits this long before it is overdone, then this long before it burns, s. */
	constexpr double ReadyWindow = 10.0;
	constexpr double OverWindow = 14.0;

	enum class EPot : uint8_t { Empty, Cooking, Ready, Over, Burnt };
	enum class EPotEvent : uint8_t { None, StepOpened, StepMissed, Ready, Overdone, Burnt };

	struct FPot
	{
		EPot State = EPot::Empty;
		int Recipe = -1;
		/** Seconds on the fire. */
		double T = 0.0;
		int NextStep = 0;
		/** The step waiting for the hand now (-1 none) and since when. */
		int OpenStep = -1;
		double OpenAt = 0.0;
		int Hit = 0;
		int Missed = 0;

		bool Busy() const { return State != EPot::Empty; }
		/** 0 .. 1 of the cooking time. */
		double Progress() const;
	};

	struct FResult
	{
		bool bOk = false;
		std::string Dish;
		int Count = 0;
		/** 0 burnt, 1 edible, 2 good, 3 fine (not a slip). */
		int Quality = 0;
	};

	/** The ingredients went in: the pot is on. False when it is busy or the fire is too small for the recipe. */
	bool Begin(FPot& Pot, int Recipe, EHeat Heat);
	/** Dt seconds on the fire; one event at most a call (call with small steps). */
	EPotEvent Step(FPot& Pot, double Dt);
	/** The hand at the pot (the key): does the waiting step; true when there was one. */
	bool Tend(FPot& Pot);
	/** Can it be taken off the fire now (ready, overdone or burnt). */
	inline bool CanTakeOff(const FPot& Pot) { return Pot.State == EPot::Ready || Pot.State == EPot::Over || Pot.State == EPot::Burnt; }
	/** Off the fire: what came of it; the pot is empty again. */
	FResult TakeOff(FPot& Pot);

	/** The jar the market's contest tastes: by the berries (the bed's quality 1 .. 3) and the cooking (1 .. 3). */
	int JamQuality(int Berries, int Cooking);

	// ------------------------------------------------------------------ the restaurant buys fresh vegetables

	/** What the «Янтарь»'s kitchen pays for one, roubles (0: it does not buy that). */
	int VegPrice(const std::string& Item);
	/** The kitchen takes this many things a day. */
	constexpr int VegPerDay = 12;

	struct FVegPick
	{
		std::string Item;
		int Count = 0;
		int Roubles = 0;
	};

	/** What it would take of the pockets now, the dearest first, Left things at most; the sum in Roubles. */
	std::vector<FVegPick> VegOffer(const FPockets& Pockets, int Left, int& Roubles);
}
