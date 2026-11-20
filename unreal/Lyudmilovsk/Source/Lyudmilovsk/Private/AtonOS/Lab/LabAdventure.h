// The text adventure of the lab's ATON machines («Ночная вахта»), plain C++ (no Unreal types): the game is data
// (Content/AtonOS/adventure.json), this plays it. Shared by the game (the ATON OS program) and tools/atonsim.
//
// The player picks numbered choices, so it plays the same in every language; the state is the node he is at and a set
// of flags (things carried, things done). Every text is a string key (ui.aton.adv.*), never words.
//
//   {"title": "<key>", "start": "<node>", "best": "<ending id>",
//    "nodes": {"<id>": {
//       "text":    ["<key>", {"k": "<key>", "if": "<cond>"}],      printed on coming to the node, those whose cond holds
//       "set":     ["flag", "-flag"],                              done after the node's text is printed
//       "choices": [{"text": "<key>", "if": "<cond>",              offered while cond holds
//                    "say":  ["<key>", {"k": "<key>", "if": "<cond>"}],   printed first (cond judged before "set")
//                    "set":  ["flag", "-flag"],
//                    "go":   "<node>",                             then that node's text; none: stay, only "say" is printed
//                    "end":  "<ending id>"}]}}}                    the game ends here (no "go")
//
// cond: flags joined by & (all), groups joined by | (any), ! before a flag = not: "lit&!opened|tea_done". Empty = always.
// Ids and flags are ASCII letters, digits and _; at most 64 flags. A flag a cond asks for must be set somewhere.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AtonLab
{
namespace Adv
{
	namespace Detail
	{
		struct FTerm
		{
			uint64_t Need = 0;     // flags that must be set
			uint64_t Not = 0;      // flags that must be clear
		};
		/** Any of the groups; none = always. */
		struct FCond
		{
			std::vector<FTerm> Any;
			bool Holds(uint64_t Flags) const;
		};
		struct FLine
		{
			std::string Key;
			FCond If;
		};
		struct FChoice
		{
			std::string Text;
			FCond If;
			std::vector<FLine> Say;
			uint64_t Set = 0, Clear = 0;
			int Go = -1;
			std::string End;
			int Id = 0;            // its number among all the choices of the data
		};
		struct FNode
		{
			std::string Id;
			std::vector<FLine> Text;
			uint64_t Set = 0, Clear = 0;
			std::vector<FChoice> Choices;
		};
	}

	class FAdventure
	{
	public:
		/** Reads the game; false (and why, in Error) on bad JSON or a broken link. The game in play is dropped. */
		bool Load(const std::string& JsonText, std::string* Error = nullptr);
		/** A new game: at the start node, its text in Output(). */
		void Start();
		/** String keys of the paragraphs the last Start() / Choose() printed, in order: what the choice said, then the node's text. */
		const std::vector<std::string>& Output() const { return Out; }
		/** String keys of the choices open now, in order; the player types 1..N. */
		std::vector<std::string> Choices() const;
		/** 1-based; false when out of range or the game has ended. */
		bool Choose(int Number);
		bool Ended() const { return !EndId.empty(); }
		/** The ending's id ("" while playing). */
		std::string Ending() const { return EndId; }
		std::string TitleKey() const { return Title; }
		int Moves() const { return MoveCount; }
		/** One ASCII line, no '|' and no newline: "1;<node>;<moves>;<flag,flag>;<ending>". */
		std::string Save() const;
		/** False on garbage (the state is unchanged). Output() becomes the node's text (nothing when the game had ended). */
		bool Restore(const std::string& S);
		/** Every string key the data names. */
		std::vector<std::string> AllKeys() const;
		/** Every ending id in the data, in the order they first appear. */
		std::vector<std::string> Endings() const;
		/** The ending the data calls the best one ("" if it names none). */
		std::string BestEnding() const { return Best; }

		// for the checks (tools/atonsim)
		std::string Node() const;
		std::vector<std::string> Nodes() const;
		int TotalChoices() const { return ChoiceCount; }
		/** The data-wide numbers (0..TotalChoices-1) of the choices open now, in the order of Choices(). */
		std::vector<int> OpenChoiceIds() const;
		/** Node and flags, without the count of moves: two games with the same key play the same from here. */
		std::string StateKey() const;

	private:
		std::vector<const Detail::FChoice*> Open() const;
		void PrintNode();

		std::vector<Detail::FNode> NodeList;
		std::vector<std::string> FlagNames;
		std::string Title, Best;
		int StartNode = 0;
		int ChoiceCount = 0;

		int At = 0;
		uint64_t Flags = 0;
		int MoveCount = 0;
		std::string EndId;
		std::vector<std::string> Out;
	};
}
}
