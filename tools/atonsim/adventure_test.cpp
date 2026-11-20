// The checks of the lab's text adventure (Content/AtonOS/adventure.json) outside the engine:
//   adventure_test <adventure.json> <tools/aton/strings/adventure.json>
// Every key has its words in ru/en/ja and no word is left unused; a search over every state the player can reach proves
// there is no dead end, every node, choice and ending can be reached and every state can still reach an ending; the
// shortest way to the best ending is 25..45 choices (printed); Save/Restore round trips along it.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/AtonOS/Lab/LabAdventure.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/People/MiniJson.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static int GChecks = 0, GFailed = 0;

static void Expect(bool bOk, const std::string& What)
{
	GChecks++;
	if (!bOk)
	{
		GFailed++;
		std::printf("FAIL %s\n", What.c_str());
	}
}

static std::string ReadFile(const char* Path)
{
	std::ifstream In(Path, std::ios::binary);
	std::stringstream Buf;
	Buf << In.rdbuf();
	return Buf.str();
}

struct FState
{
	std::string Save;
	int From = -1;         // the state it was first reached from
	int Choice = 0;        // by this choice (1-based)
	std::string ChoiceKey;
	std::string Ending;
	std::vector<int> Next;
};

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: adventure_test <adventure.json> <strings/adventure.json>\n");
		return 2;
	}
	const std::string Data = ReadFile(Argv[1]);
	const std::string Words = ReadFile(Argv[2]);
	Expect(!Data.empty(), "the adventure file is read");
	Expect(!Words.empty(), "the strings file is read");

	AtonLab::Adv::FAdventure Game;
	std::string Error;
	const bool bLoaded = Game.Load(Data, &Error);
	Expect(bLoaded, "the adventure loads: " + Error);
	if (!bLoaded)
	{
		std::printf("adventure: %d checks, %d failed\n", GChecks, GFailed);
		return 1;
	}

	// ---- bad data is refused with a reason
	{
		AtonLab::Adv::FAdventure Bad;
		std::string Why;
		Expect(!Bad.Load("{\"nodes\": {", &Why) && !Why.empty(), "cut JSON is refused with an error");
		Why.clear();
		Expect(!Bad.Load("{\"start\": \"a\", \"nodes\": {\"a\": {\"choices\": [{\"text\": \"t\", \"go\": \"nowhere\"}]}}}", &Why) && !Why.empty(), "a link to an unknown node is refused");
		Why.clear();
		Expect(!Bad.Load("{\"start\": \"a\", \"nodes\": {\"a\": {\"choices\": [{\"text\": \"t\", \"if\": \"ghost\", \"go\": \"a\"}]}}}", &Why) && !Why.empty(), "a flag never set is refused");
		Why.clear();
		Expect(!Bad.Load("{\"start\": \"a\", \"nodes\": {\"a\": {\"choices\": []}}}", &Why) && !Why.empty(), "a node without choices is refused");
		Expect(Bad.Choices().empty() && !Bad.Choose(1), "a game that failed to load offers nothing");
	}

	// ---- words
	LyudJson::FValue Table;
	std::string JsonError;
	Expect(LyudJson::Parse(Words, Table, JsonError) && Table.Type == LyudJson::FValue::Object, "the strings file parses: " + JsonError);
	std::set<std::string> Used;
	for (const std::string& Key : Game.AllKeys())
	{
		Used.insert(Key);
	}
	Used.insert("ui.aton.adv.title");
	for (const std::string& End : Game.Endings())
	{
		Used.insert("ui.aton.adv.end." + End);
	}
	Expect(Game.TitleKey() == "ui.aton.adv.title", "the title key is ui.aton.adv.title");
	for (const std::string& Key : Used)
	{
		const LyudJson::FValue* V = Table.Find(Key);
		Expect(V != nullptr, "words for " + Key);
		if (V)
		{
			for (const char* Lang : { "ru", "en", "ja" })
			{
				Expect(!(*V)[Lang].S().empty(), Key + " has " + Lang);
			}
		}
		Expect(Key.compare(0, 12, "ui.aton.adv.") == 0, Key + " is under ui.aton.adv.");
	}
	std::set<std::string> SeenKeys;
	for (const auto& Field : Table.Fields)
	{
		Expect(Used.count(Field.first) == 1, "the word " + Field.first + " is used");
		Expect(SeenKeys.insert(Field.first).second, "the word " + Field.first + " is defined once");
	}

	// ---- every state the player can reach
	const std::vector<std::string> Endings = Game.Endings();
	const std::string Best = Game.BestEnding();
	Expect(Endings.size() == 3, "three endings");
	Expect(!Best.empty(), "the data names the best ending");
	const size_t NodeCount = Game.Nodes().size();
	Expect(NodeCount >= 30 && NodeCount <= 45, "30..45 nodes (" + std::to_string(NodeCount) + ")");

	Game.Start();
	Expect(!Game.Output().empty(), "the start prints something");
	Expect(!Game.Choose(0) && !Game.Choose(int(Game.Choices().size()) + 1) && !Game.Choose(-3), "a choice out of range is refused");
	Expect(Game.Moves() == 0, "a refused choice is not a move");

	std::vector<FState> States;
	std::map<std::string, int> Index;
	std::set<std::string> NodesSeen;
	std::set<int> ChoicesSeen;
	std::queue<int> Todo;
	States.push_back({ Game.Save(), -1, 0, "", "", {} });
	Index[Game.StateKey()] = 0;
	Todo.push(0);
	int DeadEnds = 0;
	while (!Todo.empty())
	{
		const int Cur = Todo.front();
		Todo.pop();
		const std::string Save = States[size_t(Cur)].Save;
		Game.Restore(Save);
		if (Game.Ended())
		{
			continue;
		}
		NodesSeen.insert(Game.Node());
		const std::vector<std::string> Keys = Game.Choices();
		const std::vector<int> Ids = Game.OpenChoiceIds();
		if (Keys.empty())
		{
			DeadEnds++;
			std::printf("FAIL dead end at %s\n", Game.StateKey().c_str());
		}
		for (int C = 1; C <= int(Keys.size()); ++C)
		{
			Game.Restore(Save);
			Game.Choose(C);
			ChoicesSeen.insert(Ids[size_t(C - 1)]);
			const std::string Key = Game.StateKey();
			auto Found = Index.find(Key);
			int To;
			if (Found == Index.end())
			{
				To = int(States.size());
				States.push_back({ Game.Save(), Cur, C, Keys[size_t(C - 1)], Game.Ending(), {} });
				Index[Key] = To;
				Todo.push(To);
			}
			else
			{
				To = Found->second;
			}
			States[size_t(Cur)].Next.push_back(To);
		}
	}
	Expect(DeadEnds == 0, "no state without a choice");
	Expect(NodesSeen.size() == NodeCount, "every node is reached (" + std::to_string(NodesSeen.size()) + " of " + std::to_string(NodeCount) + ")");
	for (const std::string& Id : Game.Nodes())
	{
		if (!NodesSeen.count(Id))
		{
			std::printf("  never reached: node %s\n", Id.c_str());
		}
	}
	Expect(int(ChoicesSeen.size()) == Game.TotalChoices(), "every choice is taken somewhere (" + std::to_string(ChoicesSeen.size()) + " of " + std::to_string(Game.TotalChoices()) + ")");
	for (int Id = 0; Id < Game.TotalChoices(); ++Id)
	{
		if (!ChoicesSeen.count(Id))
		{
			std::printf("  never offered: choice #%d (in file order)\n", Id);
		}
	}

	// every state can still reach an ending: walk the edges backwards from the endings
	std::vector<std::vector<int>> Back(States.size());
	std::vector<char> CanEnd(States.size(), 0);
	std::queue<int> Ends;
	std::map<std::string, int> FirstEnd;   // BFS order = fewest choices
	for (size_t I = 0; I < States.size(); ++I)
	{
		for (const int To : States[I].Next)
		{
			Back[size_t(To)].push_back(int(I));
		}
		if (!States[I].Ending.empty())
		{
			CanEnd[I] = 1;
			Ends.push(int(I));
			if (!FirstEnd.count(States[I].Ending))
			{
				FirstEnd[States[I].Ending] = int(I);
			}
		}
	}
	while (!Ends.empty())
	{
		const int Cur = Ends.front();
		Ends.pop();
		for (const int From : Back[size_t(Cur)])
		{
			if (!CanEnd[size_t(From)])
			{
				CanEnd[size_t(From)] = 1;
				Ends.push(From);
			}
		}
	}
	int Stuck = 0;
	for (size_t I = 0; I < States.size(); ++I)
	{
		if (!CanEnd[I])
		{
			if (Stuck++ < 5)
			{
				std::printf("  cannot end from: %s\n", States[I].Save.c_str());
			}
		}
	}
	Expect(Stuck == 0, "every state can still reach an ending (" + std::to_string(Stuck) + " cannot)");
	for (const std::string& End : Endings)
	{
		Expect(FirstEnd.count(End) == 1, "the ending '" + End + "' can be reached");
	}

	// ---- the shortest way to the best ending, played again from the start with Save/Restore at every step
	if (FirstEnd.count(Best))
	{
		std::vector<int> Path;
		for (int S = FirstEnd[Best]; States[size_t(S)].From >= 0; S = States[size_t(S)].From)
		{
			Path.insert(Path.begin(), S);
		}
		Expect(Path.size() >= 25 && Path.size() <= 45, "the best ending takes 25..45 choices (" + std::to_string(Path.size()) + ")");
		for (const auto& Other : FirstEnd)
		{
			int Len = 0;
			for (int S = Other.second; States[size_t(S)].From >= 0; S = States[size_t(S)].From)
			{
				Len++;
			}
			std::printf("ending '%s': shortest way %d choices\n", Other.first.c_str(), Len);
		}
		Game.Start();
		AtonLab::Adv::FAdventure Copy;
		Copy.Load(Data);
		bool bRound = true, bPlayed = true;
		std::printf("the way to '%s':\n", Best.c_str());
		for (size_t I = 0; I < Path.size(); ++I)
		{
			const FState& S = States[size_t(Path[I])];
			const std::string Saved = Game.Save();
			bRound = bRound && Saved.find('|') == std::string::npos && Saved.find('\n') == std::string::npos;
			for (const char Ch : Saved)
			{
				bRound = bRound && Ch >= 32 && Ch < 127;
			}
			bRound = bRound && Copy.Restore(Saved) && Copy.Save() == Saved && Copy.Choices() == Game.Choices() && Copy.Moves() == Game.Moves();
			std::printf("  %2zu. [%s] %d  %s\n", I + 1, Game.Node().c_str(), S.Choice, S.ChoiceKey.c_str());
			bPlayed = bPlayed && Game.Choose(S.Choice);
			bPlayed = bPlayed && (Game.Ended() || !Game.Output().empty() || !Game.Choices().empty());
		}
		Expect(bPlayed && Game.Ended() && Game.Ending() == Best, "the way plays to the best ending");
		Expect(Game.Moves() == int(Path.size()), "the moves are counted");
		Expect(bRound, "Save/Restore round trips at every step");
		Expect(!Game.Choose(1) && Game.Choices().empty(), "nothing can be chosen after the end");
		const std::string EndSave = Game.Save();
		Expect(Copy.Restore(EndSave) && Copy.Ended() && Copy.Ending() == Best, "an ended game restores as ended");
		for (const char* Garbage : { "", "hello", "1;no_such_node;0;;", "1;start;x;;", "2;start;0;;", "1;start;0;no_such_flag;", "1;start;0;;no_such_end", "1;start;0" })
		{
			Expect(!Copy.Restore(Garbage) && Copy.Save() == EndSave, std::string("garbage is refused and changes nothing: '") + Garbage + "'");
		}
	}

	// every choice says or does something: after any choice there are words, or the game ended
	{
		int Silent = 0;
		for (const FState& S : States)
		{
			if (S.From < 0)
			{
				continue;
			}
			Game.Restore(States[size_t(S.From)].Save);
			Game.Choose(S.Choice);
			if (Game.Output().empty())
			{
				if (Silent++ < 5)
				{
					std::printf("  silent choice: %s from %s\n", S.ChoiceKey.c_str(), States[size_t(S.From)].Save.c_str());
				}
			}
		}
		Expect(Silent == 0, "every choice prints something");
	}

	std::printf("nodes %zu, choices %d, flags reachable states %zu\n", NodeCount, Game.TotalChoices(), States.size());
	std::printf("adventure: %d checks, %d failed\n", GChecks, GFailed);
	return GFailed ? 1 : 0;
}
