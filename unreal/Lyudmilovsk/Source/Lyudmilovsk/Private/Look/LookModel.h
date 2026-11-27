#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * The hero's look (docs/look.md): the barber's haircuts, which grow out, the clothes of the department store worn over the
 * weather, and what the townsfolk say about either. Plain C++, no engine: ULyudLook (Look/LyudLook) puts the hair and the
 * clothes on the hero and has the residents speak; tools/looksim/run.sh checks it.
 */
namespace LyudLook
{
	// ------------------------------------------------------------------ haircuts

	struct FCut
	{
		/** "shaggy" (his own, grown out: nothing to buy), "crop", "short", "parted". */
		std::string Id;
		/** Days until it has grown out into "shaggy" again (0: it is the grown-out one). */
		int LastsDays = 0;
	};

	const std::vector<FCut>& Cuts();
	/** -1 when there is none. */
	int FindCut(const std::string& Id);
	/** The cuts the barber offers, in the page's order (all but "shaggy"). */
	std::vector<std::string> Offered();

	// ------------------------------------------------------------------ clothes

	struct FWear
	{
		/** The pocket item (LyudInventory.cpp, kind Wear). */
		std::string Item;
		/** "outer" or "head". */
		std::string Slot;
		/** The department store's price, roubles (0: not sold, he has it from the start). */
		int Price = 0;
		/** How many degrees warmer he is in it. */
		double Warm = 0.0;
		bool bRain = false;
		/** How the outer layer looks in it: the cloth's colour and roughness (bOwnCloth: the body's own corduroy, untouched). */
		bool bOwnCloth = false;
		float Tint[3] = { 1.f, 1.f, 1.f };
		float Rough = 0.8f;
	};

	const std::vector<FWear>& Clothes();
	/** nullptr when the item is not a piece of clothing of this table. */
	const FWear* FindWear(const std::string& Item);

	/** How what he wears suits the weather. */
	enum class EFit : uint8_t { Fine, Wet, Cold, Hot };
	/** Outer: the item in the outer slot (empty: only the hoodie); bCap: a cap on. Rain first: a wet boy is told before a cold one. */
	EFit Fit(const std::string& Outer, bool bCap, double TempC, bool bRain);
	/** "wet", "cold", "hot" (empty for Fine): the remark's key part. */
	const char* FitId(EFit Fit);

	// ------------------------------------------------------------------ what he looks like now, and who has said so

	/** A new haircut is talked about for this many days, by this many people at most, not more often than this (hours). */
	constexpr double FreshDays = 3.0;
	constexpr int MaxComments = 6;
	constexpr double CommentGapHours = 0.5;

	struct FState
	{
		/** The last cut the barber gave and the game day (days, fractional) he gave it. */
		std::string Cut = "shaggy";
		double CutDay = -1.0e6;
		/** Counts the new haircuts: comments are per haircut. */
		int Serial = 0;
		/** Who has spoken of this haircut (person ids), and when the last did (hours). */
		std::vector<int> Told;
		double LastToldHour = -1.0e9;
		/** The day (whole) somebody last remarked on his clothes in this weather. */
		int RemarkDay = -1000000;
	};

	/** The cut he wears on a day: the barber's until it has grown out. */
	std::string CutNow(const FState& State, double Day);
	/** The barber's chair: false for an unknown cut or "shaggy". The same cut again is a trim: fresh for longer, nothing new to say. */
	bool GetCut(FState& State, const std::string& Cut, double Day);
	/** Would this resident say something about the haircut now (Hour: hours of the game's clock, absolute). */
	bool WillComment(const FState& State, int Person, double Day, double Hour);
	void NoteComment(FState& State, int Person, double Hour);
	/** Which of a cut's lines a resident says (1 .. Lines): the same person the same line. */
	int CommentLine(int Person, int Serial, int Lines);
	/** One remark a day about clothes that do not suit the weather. */
	bool WillRemark(const FState& State, EFit Fit, double Day, double Hour);
	void NoteRemark(FState& State, double Day, double Hour);

	std::string Save(const FState& State);
	bool Load(FState& State, const std::string& Text);
}
