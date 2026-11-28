// Friends among the residents (docs/friends.md), in plain C++ (no Unreal types) so tools/mannersim can run it outside the
// engine. How close each named resident is to the hero (a bond: points and the level they make), what makes it grow
// (greetings, lines heard, quests done for them, gifts, postcards, an hour on a bench, tea at their home, their family's
// outing), what each of them likes to be given (by persona), the nickname a friend gives him, and the invitations that come
// of it: tea with the ten open-home hosts, the weekend outing with a close friend's family. Saved as a flat map.
// Data: Content/PeopleData/friends.json (tools/friends/build_friends.py). The game side: ULyudFriends (Public/People/LyudFriends.h).
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace LyudFriends
{
	/** Levels: 0 a stranger, 1 acquainted, 2 friendly, 3 a friend, 4 a close friend. */
	constexpr int MaxLevel = 4;

	struct FRules
	{
		int Levels[MaxLevel + 1] = { 0, 10, 30, 70, 130 };   // points a level starts at
		int Greet = 2;          // the first greeting of a day
		int Heard = 1;          // a line of theirs said near him (at most HeardCap a day)
		int HeardCap = 3;
		int Visit = 4;          // a host: his first visit to their home on a day
		int Quest = 20;         // a quest done for them (a host's home quest)
		int GiftLoved = 12, GiftLiked = 6, GiftPlain = 2;
		int Card = 8;           // a postcard of his delivered to them
		int Bench = 6;          // an hour on a bench together (once a day)
		int Tea = 12;
		int Outing = 12;
		int Festival = 8;       // a holiday spent together: met at the festival (once a day a friend)
		int FestivalLevel = 3;  // from a friend up
		int Chat = 1;           // a talk with him (E on them, docs/town_talk.md "A talk"): the first topic of a day
		int ChatAnswer = 1;     // he answered their question
		int ChatCap = 2;        // a day's talks give at most this
		int TeaLevel = 2;       // a host invites to tea from this level
		int TeaEvery = 7;       // days between a tea (or a missed invitation) and the next invitation
		int TeaAhead = 2;       // the tea is this many days after the invitation is written (its letter takes a post day)
		int OutingLevel = 4;    // a close friend's family invites him to their outing
		int OutingEvery = 21;   // days between outings he is invited to
		int BenchLevel = 3;     // a friend comes to sit with him
		int WaveLevel = 2;      // waves first from across the street
		int NickLevel = 3;      // gives him a nickname and calls him by it
	};

	struct FGift
	{
		std::string Item;
		std::vector<std::string> Hints;     // who loves it: elder, kid, teen, adult, reader, feeder, knitter, sweet, ... any
	};

	/** One of the ten open homes' people: tea is at their table. */
	struct FHost
	{
		std::string Id;             // holm, okada, ...
		std::string Name;           // the name's string key (friends.host.<id>.name)
		std::string Person;         // their townsperson's key if the town's life has them (t_tadesse_valentina), else ""
		std::string Correspondent;  // their letters' addressee (h_holm), "" none
		std::string Quest;          // their home quest (home_holm)
		std::string Plot;           // their home's plot (P-T1H2): a visit there counts
		std::string Chair;          // the hero's chair at their table (an interiors thing id)
		std::string Table;          // where the tea things stand (an interiors thing id; "" = by the chair)
		float ChairAt[3] = { 0.f, 0.f, 0.f };   // the chair's floor point (world cm) and the way it faces (deg)
		float ChairYaw = 0.f;
		std::string HostChair;      // where the host sits if the town's life has them ("" none)
		float HostChairAt[3] = { 0.f, 0.f, 0.f };
		float HostChairYaw = 0.f;
		float HostChairHeight = 0.45f;  // m
		int Hour = 17;              // the tea's hour
		int Topics = 3;
		/** The bond's key: their person's key, else their correspondent's id, else "h_" + Id. */
		std::string Key() const { return !Person.empty() ? Person : (!Correspondent.empty() ? Correspondent : "h_" + Id); }
	};

	struct FData
	{
		FRules R;
		std::vector<std::pair<std::string, std::string>> Nicknames;     // id, hint
		std::vector<FGift> Gifts;
		std::vector<FHost> Hosts;
		std::vector<std::string> OutingActs;    // the outings with an invitation letter (friends.outing.invite.<act>)
		std::map<std::string, std::string> Jobs;    // occupation -> its words' key (the diary)
		/** Reads friends.json's text; false (and Error) when it isn't usable. */
		bool Load(const std::string& Json, std::string& Error);
		const FHost* HostByKey(const std::string& Key) const;
		const FHost* HostById(const std::string& Id) const;
		const FHost* HostByPlot(const std::string& Plot) const;
		const FHost* HostByQuest(const std::string& Quest) const;
		const FGift* Gift(const std::string& Item) const;
	};

	/** What the caller knows of somebody (from the people's life and their persona), for likes and nicknames. */
	struct FWho
	{
		std::string Key;
		int Age = 30;
		std::string Job;            // occupation
		std::vector<std::string> Habits;    // persona habits by name (reader, feeder, knitter, sweet, photographer, ...)
		float Warmth = 0.5f, Curiosity = 0.5f, Tidy = 0.5f;
		bool bHost = false;
	};
	/** The words a gift's hints are matched against: age group, habits, job group (sea, artist, ...), strong traits. */
	std::vector<std::string> Tags(const FWho& Who);

	enum class EGift : uint8_t { Loved, Liked, Plain, Again };
	const char* GiftWord(EGift G);      // loved, liked, plain, again

	struct FBond
	{
		int Points = 0;
		int FirstDay = -1;          // the first day anything counted
		int LastDay = -1;           // the last day anything counted
		int GreetDay = -1;
		int HeardDay = -1, HeardToday = 0;
		int VisitDay = -1;
		int GiftDay = -1;
		int BenchDay = -1;
		int Cards = 0, Teas = 0, Outings = 0, Benches = 0, Gifts = 0;
		int CardDay = -1;           // the last card of his delivered to them (they mention it for two days)
		std::set<std::string> Quests;       // quests counted
		std::set<std::string> Loves;        // items he has found out they love
		std::string Nick;           // the nickname id they gave him ("" none yet)
		int TeaDay = -1;            // an invitation to tea: on this day (-1 none)
		int TeaHour = 17;
		int TeaLast = -1;           // the day of the last tea, or of the last invitation missed
		int OutingDay = -1;         // an invitation to their outing: on this day (-1 none)
		int OutingLast = -1;        // the day of the last outing he was invited to
		int FestDay = -1;           // the last holiday they met at (once a day)
		int ChatDay = -1, ChatToday = 0;    // the last day they talked (E on them), the points the talks of that day gave
		int Chats = 0;              // days they talked on
		int LevelShown = 0;         // the highest level already told (the game's toast once each)
	};

	/** Something to post as a letter (ULyudLetters) or to tell: a host's invitation to tea, a family's to their outing. */
	struct FInvite
	{
		std::string Kind;           // tea, outing
		std::string Key;            // the bond's key
		std::string Body;           // the letter's string key
		std::string From;           // the sender's name: a string key (a host's) or "" (the caller names a townsperson)
		int Day = -1;               // the day it is on
		int Hour = 0, Minute = 0;
	};

	/** A family's weekend outing as the people's life has it (FPerson::Outing*), for the invitations. */
	struct FOutingInfo
	{
		std::string Act;            // activities.json id (sea_air, stroll, ...)
		int Weekday = -1;           // 0 Monday .. 6 Sunday
		float Hour = 0.f;
	};

	class FModel
	{
	public:
		FData Data;
		std::map<std::string, FBond> Bonds;

		int Level(const std::string& Key) const;
		static int LevelOf(const FRules& R, int Points);
		/** friendly, friend, close for town talk's bond = (levels 2, 3, 4), "" below. */
		const char* BondWord(const std::string& Key) const;
		const FBond* Find(const std::string& Key) const;

		// --- what makes it grow: each returns the points added (0: nothing counted)
		int Greeted(const std::string& Key, int Day);
		int Heard(const std::string& Key, int Day);
		int Visited(const std::string& HostKey, int Day);
		int QuestDone(const std::string& Key, const std::string& Quest, int Day);
		/** A gift: what they think of it (Again: he gave them something today already: it counts nothing). */
		EGift Give(const std::string& Key, const std::string& Item, const FWho& Who, int Day, int* Added = nullptr);
		int Card(const std::string& Key, int Day);
		int Bench(const std::string& Key, int Day);
		/** Tea at a host's: the invitation is used up. */
		int Tea(const std::string& Key, int Day);
		int Outing(const std::string& Key, int Day);
		/** They met at a holiday's gathering (docs/festivals.md "The new toys"): counts once a day, from a friend up. */
		int Festival(const std::string& Key, int Day);
		/** A talk with him (docs/town_talk.md "A talk"): a topic heard (the day's first counts), his answer to their question;
		 *  a day's talks give at most FRules::ChatCap. */
		int Chatted(const std::string& Key, int Day);
		int ChatAnswered(const std::string& Key, int Day);

		/** What they think of an item (no state). */
		EGift Likes(const std::string& Item, const FWho& Who) const;
		/** The nickname they give him (deterministic from who they are; "" for an unknown persona list). */
		std::string PickNick(const FWho& Who) const;
		/** Gives the nickname once they are a friend: true the first time (the game tells it). */
		bool EnsureNick(const std::string& Key, const FWho& Who);

		/** A tea he can have now: the host's bond key whose invitation is on Day within [Hour - 0.5, Hour + 2.5). */
		bool TeaNow(const std::string& Key, int Day, float Hour) const;
		/** A family's outing he is invited to now: within [Hour - 0.5, Hour + 1.5). */
		bool OutingNow(const std::string& Key, int Day, float Hour, const FOutingInfo& O) const;

		/**
		 * Once a game hour or so: the invitations due (each once), and invitations whose day has gone by forgotten. Outing:
		 * the outing of a townsperson's family (empty Act: none), by their key; Weekday(Day) is the caller's calendar.
		 */
		std::vector<FInvite> Step(int Day, float Hour, int Weekday, const std::map<std::string, FOutingInfo>& Outings);

		/** Developer mode: Key becomes a close friend (points to the top level). */
		void DevBefriend(const std::string& Key, int Day);
		/** Developer mode: a host's invitation for today, now. */
		bool DevTeaNow(const std::string& HostKey, int Day, int Hour);

		void Reset() { Bonds.clear(); }
		void Save(std::map<std::string, std::string>& Out) const;
		void LoadState(const std::map<std::string, std::string>& In);

		static uint32_t Hash(const std::string& S);

	private:
		FBond& At(const std::string& Key, int Day);
		int Add(const std::string& Key, int Day, int Points);
	};
}
