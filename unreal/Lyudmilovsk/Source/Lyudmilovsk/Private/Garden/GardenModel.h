// The allotment (docs/allotment.md), in plain C++ (no Unreal types) so tools/gardensim can run it outside the engine over
// a whole season of the game's own weather (LyudWeather). The hero rents a bed in the commune house's allotments; each bed
// has cells; a cell is dug with a spade, sown from a seed packet (or planted from a seedling), watered from the can,
// weeded, covered with film on cold nights; the crop grows by the hour from the air's warmth in the bed (the film warms it,
// most in the sun), the soil's water (rain wets an open bed, the warmth and the wind dry it) and the weeds round it; frost
// below the crop's limit hurts it and kills it. A ripe crop is picked into the pocket (some come again: dill, strawberries,
// tomatoes); left too long it spoils. How well it was kept (its care) makes the harvest's quality: the market pays for it,
// the jam contest's jury tastes it. Nothing is a number on screen: the game turns the state into words.
// Data: Content/Garden/garden.json (tools/garden/build_garden.py). The game side: ULyudGarden (Public/Garden/LyudGarden.h).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace LyudGarden
{
	/** One crop the hero can grow. Days are at the best warmth with water and no weeds; the warmth (C) is the bed's. */
	struct FCrop
	{
		std::string Id;             // radish, dill, potato, tomato, sunflower, strawberry
		std::string Species;        // the procedural plant system's species (PlantSpeciesLibrary) that draws it
		std::string Seed;           // the pocket item it is sown from
		std::string Yield;          // the pocket item it gives
		int YieldCount = 1;         // at the best care (fewer when poorly kept, at least 1)
		float Days = 3.f;           // sown to ripe at the best warmth
		float Base = 4.f;           // no growth at or below this
		float Best = 18.f;          // full speed from here
		float Hot = 28.f;           // slower above this, nothing at Hot + 8
		float Thirst = 0.35f;       // the soil water it drinks a day at 20 C
		float FrostKill = -2.f;     // the bed's warmth that starts to hurt it
		float RipeDays = 3.f;       // days it keeps ripe before it starts to spoil
		float Again = 0.f;          // > 0: after a picking it ripens again in this many days (dill, strawberries, tomatoes)
		int Pickings = 1;           // how many pickings in all (Again > 0)
		bool bPerennial = false;    // stays through the winter (strawberries): frost kills only far below FrostKill
		bool bSeedling = false;     // planted from a seedling (starts a little grown)
	};

	/** The rules' numbers (garden.json "rules"). */
	struct FRules
	{
		float CoverNight = 2.0f;    // the film's warmth at night, C
		float CoverSun = 10.0f;     // and in full sun (times the sun's height's sine and the clear sky)
		float OpenSun = 1.5f;       // an open bed in full sun
		float RainWet = 0.08f;      // soil water from 1 mm of rain on an open bed
		float CanWater = 0.55f;     // one watering
		int CanFills = 6;           // waterings in a full can
		float WeedDays = 4.f;       // weeds go from none to rampant in this many warm days
		float WeedBase = 5.f;
		float DryBelow = 0.3f;      // soil water below this slows the crop
		float FrostHurt = 0.25f;    // damage per degree below the limit per hour (1 kills)
		float SpoilDays = 4.f;      // a ripe crop past its RipeDays spoils over this many days
		float CatchUpDays = 60.f;   // the longest gap stepped (a save left for months)
		int RentDays = 0;           // 0: the bed is the hero's for good once rented
		int Rent = 30;              // roubles
	};

	/** The air at the beds for one hour (the game: ULyudWeatherSubsystem; the sim: LyudWeather::Full). */
	struct FAir
	{
		float TempC = 10.f;
		float RainMm = 0.f;         // mm of water this hour
		float Snow = 0.f;           // 0 rain .. 1 snow
		float Cloud = 0.5f;
		float SunElev = 0.f;        // degrees
		float Wind = 4.f;           // m/s
	};

	enum class ECell : uint8_t { Grass, Dug, Growing, Ripe, Spoilt, Dead };
	const char* CellWord(ECell C);  // grass, dug, growing, ripe, spoilt, dead

	struct FCell
	{
		ECell State = ECell::Grass;
		std::string Crop;           // crop id while Growing / Ripe / Spoilt / Dead
		float Growth = 0.f;         // 0 sown .. 1 ripe
		float Water = 0.5f;         // soil water 0 .. 1
		float Weeds = 0.f;          // 0 .. 1
		float Hurt = 0.f;           // frost and drought damage 0 .. 1 (1: dead)
		float Care = 1.f;           // running mean of how well it is kept (0 .. 1): the harvest's quality
		float CareHours = 0.f;
		float RipeHours = 0.f;      // hours since it ripened
		int Picked = 0;             // pickings so far
		double SownAt = 0.0;        // game hours (since 2000) it was sown
		float DryHours = 0.f;       // hours on end with the soil dry
		float Rich = 0.f;           // compost dug in: grows faster while it lasts (1 fresh .. 0 used up, two weeks)
	};

	/** What the game shows and the neighbour talks about: the worst need of a cell first. */
	enum class ENeed : uint8_t { None, Dig, Sow, Water, Weed, Cover, Uncover, Pick, Clear, Wait };
	const char* NeedWord(ENeed N);

	/** A crop's stage for the words and the drawing: seed (nothing above ground), sprout, young, grown, ripe. */
	int Stage(const FCell& C);

	struct FBed
	{
		std::string Id;             // hero, maarja, ...
		std::vector<FCell> Cells;
		bool bCovered = false;      // the film is down
		bool bRented = false;       // the hero's: rented from the house committee
	};

	struct FHarvest
	{
		std::string Item;
		int Count = 0;
		int Quality = 1;            // 1 .. 3
	};

	struct FData
	{
		FRules R;
		std::vector<FCrop> Crops;
		int Cells = 6;              // cells in the hero's bed
		bool Load(const std::string& Json, std::string& Error);
		const FCrop* Crop(const std::string& Id) const;
		const FCrop* CropBySeed(const std::string& Item) const;
		const FCrop* CropByYield(const std::string& Item) const;
	};

	class FModel
	{
	public:
		FData Data;
		FBed Bed;                   // the hero's bed (rented or not)
		double Now = -1.0;          // game hours (since 2000) the beds were stepped to (-1: never)
		int CanLeft = 0;            // waterings left in his can
		/** Quality of the strawberries given to the neighbour for jam (0: none waiting), the hour she was given them. */
		int JamQuality = 0;
		double JamAt = 0.0;
		int JamJarQuality = 0;      // the last jar she cooked (the contest tastes it)
		/** The quality of the last picking of each harvest item (the market pays by it; the jam is cooked from it). */
		std::map<std::string, int> LastQualities;
		int LastQuality(const std::string& Item) const
		{
			const auto It = LastQualities.find(Item);
			return It == LastQualities.end() ? 2 : It->second;
		}

		void Init();                // an empty, unrented bed of Data.Cells grass cells
		bool Rent(double Hours);

		/** The bed's warmth for an hour of air (the film warms it, most in the sun). */
		float BedTemp(const FAir& A, bool bCovered) const;
		/** One hour (or less: Dt hours) of growing. */
		void Step(const FAir& A, float Dt = 1.f);
		/** Brings the bed to Hours an hour at a time, asking Air(hour) for each (at most CatchUpDays). */
		template <typename F> int AdvanceTo(double Hours, F&& Air)
		{
			if (Now < 0.0) { Now = Hours; return 0; }
			const double From = Now < Hours - Data.R.CatchUpDays * 24.0 ? Hours - Data.R.CatchUpDays * 24.0 : Now;
			int N = 0;
			double T = From;
			for (; T + 1.0 <= Hours; T += 1.0, ++N) { Step(Air(T)); }
			if (Hours - T > 1e-3) { Step(Air(T), float(Hours - T)); }
			Now = Hours;
			return N;
		}

		// --- what the hero does (each true if it did something)
		bool Dig(int Cell);
		bool Sow(int Cell, const std::string& SeedItem, double Hours);
		bool Water(int Cell);           // uses one watering from the can
		bool WaterAll();                // the whole bed (one watering a cell while the can lasts)
		bool Weed(int Cell);
		bool Clear(int Cell);           // a dead or spoilt crop pulled out: the cell is dug again
		bool Pick(int Cell, FHarvest& Out);
		bool Compost(int Cell);         // a sack of compost dug into a growing or dug cell
		void Fill() { CanLeft = Data.R.CanFills; }
		void SetCover(bool bDown) { Bed.bCovered = bDown; }

		/** What a cell needs most now (Air: the coming night's lowest warmth, for the film). */
		ENeed Need(int Cell, float ColdestSoon = 10.f, float HottestSoon = 15.f) const;
		/** The quality a harvest of this cell would have now (1..3). */
		int Quality(const FCell& C) const;

		void Save(std::map<std::string, std::string>& Out) const;
		void LoadState(const std::map<std::string, std::string>& In);

	private:
		void StepCell(FCell& C, const FAir& A, float T, float Dt);
	};

	/**
	 * The neighbour's own bed: no care needed, a pure function of the day of the year (she sows in May, picks in August,
	 * digs it over in October). Growth 0..1 of each of her crops on Day (1..366), or -1 when the cell is bare.
	 */
	float NeighbourGrowth(const std::string& Crop, float DayOfYear);

	/** Season of the year for the advice: 0 spring (Mar-May), 1 summer, 2 autumn, 3 winter. */
	int Season(float DayOfYear);
}
