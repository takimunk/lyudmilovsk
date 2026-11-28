// A talk between a townsperson and the hero (docs/town_talk.md "A talk"), in plain C++ (no Unreal types) so
// tools/townchatsim can run it on the town's people outside the engine. E on a resident who is free for a word opens a
// short talk: their greeting, two to four things to ask about (their work, their day, the weather, the holiday, what the
// town says of the hero, their family, a friend, what they like, the town), now and then a question back, a goodbye.
// Here: who may be talked to now (CanChat), which topics they have today (Build) and whom a topic names. The words are
// the townsfolk's own lines: sets with on = tc_* in town_barks.txt (tools/barks/lines/18_townchat.txt), picked by
// LyudBarks::FBook by who they are and the moment, as every other line of theirs. Data: Content/PeopleData/town_chat.json
// (tools/townchat/build_town_chat.py). The game side: ULyudFriends (the page, the bond), ULyudTownTalkSubsystem (the lines).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace LyudPeople
{
	class FWorld;
	struct FPerson;
}

namespace LyudBarks
{
	class FBook;
	struct FWho;
	struct FNow;
}

namespace LyudTownChat
{
	struct FRules
	{
		int TopicsMin = 2, TopicsMax = 4;
		int KidAge = 12;            // under it they are shy with somebody they don't know
		int KidShyMet = 2;          // ... till they greeted him on this many days (or the bond is level 1)
		float DidHours = 4.f;       // what they did before this counts as the day's news for so long
		float AskChance = 0.45f;    // how often they ask something back (more from the curious and the warm)
	};

	/** A question they ask him back: tc_ask_<id> their line, townchat.ask.<id>.a<n> his answers, tc_reply_<id>_<n> theirs. */
	struct FAsk
	{
		std::string Id;
		int Answers = 2;
		int MinAge = 0, MaxAge = 200;   // who asks it
	};

	struct FData
	{
		FRules R;
		std::map<std::string, std::string> Did;     // activity id -> the group of tc_did_<group>
		std::map<std::string, std::string> Jobs;    // occupation -> the work topic's title (work, study, retired, home, guest)
		std::vector<std::string> Hobbies;           // the likes with lines (tc_hobby_<like>), in the file's order
		std::vector<FAsk> Asks;
		std::map<std::string, float> Weights;       // topic id -> how often against the others
		std::vector<std::string> Names;             // the given names with forms (townchat.fn.<id>.<form>), sorted
		/** Reads town_chat.json's text; false (and Error) when it isn't usable. */
		bool Load(const std::string& Json, std::string& Error);
		bool HasName(const std::string& Id) const;
		float Weight(const std::string& Topic) const;
	};

	/** Why somebody can't be talked to now. */
	enum class ERefuse : uint8_t
	{
		None,
		Gone,       // asleep, away, inside a building, between two things: nobody to ask
		Riding,     // on a bicycle, the cable car, the train
		Work,       // serving somebody at their counter
		Hurry,      // late for work or school
		Shy,        // a child who doesn't know him yet
	};
	/** What they say instead (a set's on =): tc_no_work, tc_no_hurry, tc_no_shy; "" when nothing is said. */
	const char* RefuseEvent(ERefuse R);
	/** Met: the days they greeted him on; Level: the bond's (docs/friends.md). */
	ERefuse CanChat(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, const FData& D, int Met, int Level);

	struct FTopic
	{
		std::string Id;         // day, work, festival, rumour, weather, hobby, kin, friend, town
		std::string Title;      // the hero's question: a string key (townchat.topic.<...>)
		std::string Event;      // their answer: a line of the sets with on = Event
		int Other = -1;         // whom it names (a person id; the line's {2}..{7} are their given name's forms), -1 nobody
	};

	struct FTalk
	{
		int Person = -1;
		std::string Hello, Bye;         // the sets' on = for the greeting and the goodbye
		std::vector<FTopic> Topics;     // in the order shown
		int Ask = -1;                   // FData::Asks' index of the question they ask back, -1 none
		int AskAfter = 1;               // ... once this many topics were heard
		void Reset();
	};

	/** Is there a line with on = Event that fits Who now. */
	bool HasLine(const LyudBarks::FBook& Book, const LyudBarks::FWho& Who, const LyudBarks::FNow& Now, const std::string& Event);

	/**
	 * The talk somebody has in them now. Who and Now as for any line of theirs (LyudBarks::FillWho; the caller adds the
	 * bond, how well they know him, their temper, and Heard: the rumours about him they know). Day: the calendar's day
	 * number (the same talk all through a part of a day, another one tomorrow). Only topics with a line now are offered.
	 */
	void Build(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, const FData& D, const LyudBarks::FBook& Book,
		const LyudBarks::FWho& Who, const LyudBarks::FNow& Now, int Day, FTalk& Out);

	/** A townsperson's given name's id: the key's last part (t_sokolov_ilya: ilya; the same for their twin aboard the
	 *  motor ship, c_sokolov_ilya: docs/ship.md). */
	std::string NameId(const std::string& PersonKey);
	/** How P would speak of Other of their household: tc_kin_son, daughter, mum, dad, sister, brother, wife, husband, mate. */
	const char* KinEvent(const LyudPeople::FPerson& P, const LyudPeople::FPerson& Other);
	/** tc_kin_<x>'s topic title: kids, parents, sibling, spouse, mate. */
	const char* KinTitle(const char* KinEvent);
	/** The one of their household they would speak of (a townsperson whose name has forms), -1 none. */
	int KinOf(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, const FData& D, uint32_t Seed);
	/** The friend they would speak of, -1 none. */
	int FriendOf(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, const FData& D, uint32_t Seed);
	/** Their day's news: tc_did_<group> (what they did before this), tc_day_before_work, tc_day_after_work, tc_day_off,
	 *  or tc_day (the hour, what they are doing now): one of those that have a line now, by Seed. */
	std::string DayEvent(const LyudPeople::FWorld& W, const LyudPeople::FPerson& P, const FData& D, const LyudBarks::FBook& Book,
		const LyudBarks::FWho& Who, const LyudBarks::FNow& Now, uint32_t Seed);

	uint32_t Hash(const std::string& S);
}
