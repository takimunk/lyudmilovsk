// The photo club's monthly contest (docs/photo.md "The contest"), in plain C++ so that tools/photosim checks it outside
// the engine: the theme of a month, the score of a print, the other entrants, the places and what the hero entered.
#pragma once

#include "PhotoModel.h"

#include <string>
#include <vector>

/** The photo club's monthly contest: a theme by the month, one print from the hero by the 25th, the jury on the 26th. */
namespace PhotoContest
{
	constexpr int Themes = 4;
	/** Prints are taken on days 1..LastEntryDay of the month; the jury sits on JudgeDay. */
	constexpr int LastEntryDay = 25;
	constexpr int JudgeDay = 26;
	/** The other entrants of every contest (the club's own photographers). */
	constexpr int Rivals = 5;
	/** Places 1..PrizePlaces get a prize and hang on the club's wall. */
	constexpr int PrizePlaces = 3;

	/** "sea", "birds", "work", "festival" (index clamped). */
	const char* ThemeId(int Theme);
	/** The theme of a calendar month (1..12): they go round in the order above, January the sea. */
	int ThemeOf(int Month);
	/** A contest's number: one per calendar month. */
	int ContestOf(int Year, int Month);
	int YearOf(int Contest);
	int MonthOf(int Contest);

	/**
	 * The jury's marks for a print, each part and the sum (0..100).
	 * Subject, 0..50: what is in the frame against the theme, from the tags the shutter kept (PhotoModel::FSubject):
	 *   sea: "sea"; birds: any "bird:<Species>"; work: "work" (a resident busy with something); festival: "festival:<id>".
	 *   The theme's own subject gives 30 and up to 10 more for how sharp it is; a second thing that suits the theme gives
	 *   up to 10 (sea: a sunset, a bird, a fish; birds: a second species; work and festival: more people). A print off
	 *   the theme gets 5.
	 * Light, 0..25: the sun's height from the tag "sun:<degrees>": best low over the horizon (0..10), less by midday,
	 *   poor at night unless the aurora is in the frame; a sunset in the frame adds a little. No tag: a middling 12.
	 * Sharpness, 0..25: the theme's subject's own sharpness, or the whole frame's when the print is off the theme.
	 */
	struct FMarks
	{
		float Subject = 0.f;
		float Light = 0.f;
		float Sharp = 0.f;
		float Sum() const { return Subject + Light + Sharp; }
	};
	FMarks Judge(const PhotoModel::FShot& Shot, int Theme);

	/** The score of the other entrant Index (0..Rivals-1) of a contest, 40..85: the same in every game. */
	float RivalScore(int Contest, int Index);
	/** The place a score takes among the rivals of a contest, 1..Rivals+1 (a tie goes to the hero). */
	int PlaceOf(int Contest, float Score);
	/** The prize of a place, roubles (0 below the prize places). */
	int PrizeOf(int Place);

	struct FEntry
	{
		int Contest = 0;
		/** The print: its film and frame (PhotoModel::FShot::Roll, Frame). */
		int Roll = 0;
		int Frame = 0;
		float Score = 0.f;
		/** The map place the print was taken at ("" when nowhere known). */
		std::string Place;
		/** The hero has heard the result and taken the prize. */
		bool bTold = false;
	};

	class FState
	{
	public:
		/** True on days 1..LastEntryDay of a month the hero has not entered yet. */
		bool CanEnter(int Year, int Month, int Day) const;
		/** Hands a print in for this month's contest; false when CanEnter is. */
		bool Enter(int Year, int Month, int Day, const PhotoModel::FShot& Shot);
		/** The hero's entry of a contest, null when there is none. */
		const FEntry* Find(int Contest) const;
		/** The jury has sat on this entry by the date given. */
		static bool IsJudged(const FEntry& Entry, int Year, int Month, int Day);
		/** The oldest judged entry the hero has not heard about, null when none; Told marks it heard. */
		const FEntry* Untold(int Year, int Month, int Day) const;
		void Told(int Contest);
		/** Every entry, oldest first. */
		const std::vector<FEntry>& Entries() const { return All; }
		/** The entries that won a prize and were judged by the date: the club's wall, oldest first. */
		std::vector<const FEntry*> Wall(int Year, int Month, int Day) const;
		/** One line for the save, and back: "c1|e=contest:roll:frame:score:told:place". Garbage: false, the state left empty. */
		std::string Save() const;
		bool Restore(const std::string& Line);

	private:
		std::vector<FEntry> All;
	};
}
