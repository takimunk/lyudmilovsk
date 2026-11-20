// The town's post in plain C++ (docs/letters.md): postcards the hero sends to people he knows, letters that come to his
// mailbox at the club (replies, thanks after a quest, invitations to the holidays) and the long correspondence with a pen
// friend in a lighthouse town far north. No Unreal types: the engine side (ULyudLetters) feeds it the calendar and what
// the hero has done, and shows what it says; tools/lettersim runs it outside the engine.
//
// Time is a civil day number (days since 1970-01-01) and the minute of that day. The post works Monday to Saturday:
// postboxes are emptied at Collect, the town's letters are carried the next post day by DeliverTown, the club's mailbox
// gets its letters at DeliverHero. Mail to and from the north takes PenTravel post days each way.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LyudLetters
{
	// --- the calendar

	/** Days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's days_from_civil). */
	int DaysFromCivil(int Y, int M, int D);
	void CivilFromDays(int Days, int& Y, int& M, int& D);
	/** 0 Monday .. 6 Sunday. */
	int Weekday(int Day);
	/** The post works Monday to Saturday. */
	inline bool IsPostDay(int Day) { return Weekday(Day) != 6; }
	/** The first post day after Day. */
	int NextPostDay(int Day);

	struct FTime
	{
		int Day = 0;
		int Minute = 0;
		bool operator<(const FTime& O) const { return Day != O.Day ? Day < O.Day : Minute < O.Minute; }
		bool operator>=(const FTime& O) const { return !(*this < O); }
	};

	// --- the data (Content/Letters/letters.json, written by tools/letters/build_letters.py)

	struct FCorrespondent
	{
		std::string Id;          // h_holm
		std::string Name;        // string key of the name
		std::string Home;        // townsfolk-style home id (a plot or a flat) for the address
		std::string Person;      // a townsperson's key when one of the town's people is this correspondent ("" none)
		double Near[3] = { 0, 0, 0 };   // world cm: being within NearRadius of it makes them known (their open home)
		double NearRadius = 800.0;
		double Shelf[3] = { 0, 0, 0 };  // world cm: where the hero's postcards stand in their home
		float ShelfYaw = 0.f;
		bool bHasShelf = false;
		std::vector<std::string> Replies;   // their letters back, one per postcard received, in order
		std::string Invite;      // their invitation (after InviteAfter postcards; "" none)
		std::string Shelfnote;   // the line when the hero sees his postcard at theirs
	};

	struct FThanks
	{
		std::string Quest;
		std::string From;        // name key
		std::string Body;
		int Delay = 1;           // days after the quest is done before it is posted
	};

	struct FPenLetter
	{
		std::string Body;        // the letter (paragraphs separated by a blank line)
		std::string Diary;       // the diary's line for it
		std::string Ask;         // what she asks about: bird, fish, place, friend, sky ("" the last letter)
	};

	struct FPost
	{
		int Collect = 17 * 60;
		int DeliverTown = 11 * 60;
		int DeliverHero = 10 * 60;
		int PenTravel = 3;
		int PenStartDay = 2;     // days after the first day of play, with a bird met (or PenStartLate without)
		int PenStartLate = 5;
		int PenNudgeDays = 10;   // no answer for this long after her letter: she asks if it came
		int InviteAfter = 2;
		int FestivalNotice = 2;  // invitations come this many days before a holiday
		int ThanksWindow = 4;    // a recipient says thank you in the street within this many days of the card
	};

	struct FData
	{
		FPost Post;
		std::vector<std::string> Pictures;          // ids: letters.pic.<id>
		std::vector<std::string> Openings;          // phrase ids: letters.phrase.<id>
		std::vector<std::string> Wishes;
		std::map<std::string, std::vector<std::string>> Replies;   // kid, teen, adult, elder -> keys of townsfolk replies
		std::vector<FCorrespondent> Correspondents;
		std::vector<FThanks> Thanks;
		std::string InviteGeneric;                  // the town's invitation, {0} the holiday
		std::map<std::string, std::string> InviteFestival;   // a holiday's own invitation
		std::string PenWho, PenPlace;
		std::vector<FPenLetter> Pen;
		std::string PenNudge;
		std::map<std::string, std::vector<std::string>> PenAcks;   // answer kind (or "none") -> acknowledgements, {0} the fact
		std::map<std::string, std::string> PenAnswers;             // answer kind -> the hero's sentence, {0} the fact
		std::string PenAnswerNone;
		std::map<std::string, std::vector<std::string>> Homes;     // home id -> {terrace, house, flat}

		bool Load(const std::string& Json, std::string& Error);
		const FCorrespondent* FindCorrespondent(const std::string& Id) const;
		int CorrespondentIndex(const std::string& Id) const;
	};

	// --- what the hero has done (the engine side fills it from the bird book, the angler's log, the map, the quests)

	struct FFacts
	{
		std::vector<std::string> Birds;      // species met, in the order met (ids: bird.<id>.name after the engine maps them)
		std::vector<std::string> Friends;    // species of birds that ate from his hand
		std::vector<std::string> Fish;       // fish species caught
		std::vector<std::string> Places;     // map places found
		std::set<std::string> QuestsDone;
		std::vector<std::pair<int, std::string>> Festivals;   // (day, id) of the next holidays
	};

	// --- the state (saved as a flat map of strings)

	struct FCard
	{
		std::string To;
		std::string Picture;
		std::string Opening, Wish;
		std::string Group;       // a townsperson's age group for their reply (kid, teen, adult, elder); "" a correspondent
		FTime Posted;
		int Deliver = 0;         // the day it is carried to them (at DeliverTown)
		bool bDelivered = false;
	};

	struct FLetter
	{
		std::string Id;          // pen:3, thanks:<quest>, reply:<to>:<n>, home:<id>:<n>, invite:<festival>, homeinvite:<id>
		std::string From;        // name key (or a townsperson's key: t_*)
		std::string Kind;        // pen, thanks, reply, home, invite
		std::string Body;        // string key
		std::vector<std::string> Args;   // the body's {0}, {1}: a key starting with @ is shown in the current language
		std::string Ack;         // pen letters: the acknowledgement line said first ("" none)
		std::vector<std::string> AckArgs;
		int Arrive = 0;          // the day it is in the club's mailbox (from DeliverHero)
		bool bArrived = false;
		bool bRead = false;
		int ReadDay = 0;
	};

	struct FAnswer
	{
		std::string Kind;        // bird, fish, place, friend, none
		std::string Fact;        // the species, the fish, the place ("" for none)
	};

	struct FEvents
	{
		std::vector<std::string> Delivered;   // cards carried to them now (their ids)
		std::vector<int> Arrived;             // letters now in the mailbox (indexes into Letters)
		bool bPenReplyDelivered = false;      // the hero's letter reached the north now
	};

	class FModel
	{
	public:
		FData Data;

		// state
		int FirstDay = -1;                     // the first day the post ran (a new game)
		std::vector<FCard> Cards;              // every card the hero sent
		std::vector<FLetter> Letters;          // every letter to the hero (arrived or on its way)
		std::map<std::string, int> Got;        // cards delivered to each addressee
		std::map<std::string, int> GotDay;     // the day the last one was
		std::set<std::string> Thanked;         // "<to>:<n>": that card was thanked for in the street
		std::set<std::string> Known;           // correspondents the hero has been to see (their homes)
		std::set<std::string> Done;            // once-only things: thanks:<quest>, invite:<festival>@<day>, homeinvite:<id>
		int PenSent = 0;                       // her letters posted so far (1 = the first)
		int PenAnswered = 0;                   // the hero's answers posted (to letter n)
		int PenAnswerArrive = -1;              // the day the hero's last answer reaches her (-1 none on the way)
		std::vector<FAnswer> PenAnswers;       // what he answered, by letter
		bool bPenNudged = false;               // the nudge for the letter now waiting

		/** Advances the post to Now: cards carried, replies written, letters arriving. Call often (a few times a game minute
		 *  is plenty); jumps of days are fine. */
		FEvents Step(const FTime& Now, const FFacts& Facts);

		/** The day a card posted at T reaches someone in town. */
		int DeliveryDay(const FTime& T) const;
		/** The day the hero's letter posted at T reaches the north. */
		int PenDeliveryDay(const FTime& T) const;
		/** The day a letter written on Day in town arrives at the club (posted the next day, carried the post day after). */
		int TownReplyArrive(int Day) const;

		/** Sends a card (the item is the engine's to take). False: unknown picture or phrases, nobody to send to. */
		bool PostCard(const FTime& Now, const std::string& To, const std::string& Picture, const std::string& Opening, const std::string& Wish,
			const std::string& Group = "adult");

		/** Is her latest letter read and not yet answered? */
		bool PenWaiting() const;
		/** What she asks about in the letter waiting for an answer ("" none). */
		std::string PenAsk() const;
		/** Up to Max answers from what the hero really did for the letter waiting (newest first), then "none". */
		std::vector<FAnswer> PenOptions(const FFacts& Facts, int Max = 3) const;
		/** Posts the hero's answer to her waiting letter. False when none waits. */
		bool PostPenAnswer(const FTime& Now, const FAnswer& Answer);

		/** Marks a letter read (it goes into the diary on the engine's side). */
		void MarkRead(int Letter, int Day);
		int NumUnread() const;
		int NumOnTheWay() const;

		/** Recipients whose card came within ThanksWindow days and who have not said thank you for it yet. */
		std::vector<std::string> WantsToThank(int Today) const;
		void NoteThanked(const std::string& To);

		/** A correspondent's open home was visited: they are known from now (true when new). */
		bool Meet(const std::string& Correspondent);

		/** The address parts of a home id: terrace, house, flat (empty when unknown). */
		std::vector<std::string> AddressOf(const std::string& Home) const;

		/** Developer mode: every card and letter on its way is due now (the next Step carries them). */
		void DevDeliverAll(const FTime& Now);
		/** Developer mode: her next letter is written now (answering for the hero with "nothing special" if needed). */
		void DevPenNext(const FTime& Now);

		void Reset();
		void Save(std::map<std::string, std::string>& Out) const;
		void LoadState(const std::map<std::string, std::string>& In);

		/** A deterministic 0..99 for an addressee and a count (the replies' dice: the same save, the same letters). */
		static int Roll(const std::string& Key, int N);

	private:
		void Schedule(FLetter&& L);
		void StepPen(const FTime& Now, const FFacts& Facts, FEvents& Out);
		int NextArrival(const FTime& Now) const;
	};

	/** The age group of a townsperson for the replies: kid (<13), teen (<18), adult, elder (>=62). */
	std::string AgeGroup(int Age);
}
