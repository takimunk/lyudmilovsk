#include "LabAdventure.h"

#include "../../People/MiniJson.h"

#include <algorithm>

namespace AtonLab
{
namespace Adv
{
	namespace Detail
	{
		bool FCond::Holds(uint64_t Flags) const
		{
			if (Any.empty())
			{
				return true;
			}
			for (const FTerm& Term : Any)
			{
				if ((Flags & Term.Need) == Term.Need && (Flags & Term.Not) == 0)
				{
					return true;
				}
			}
			return false;
		}

		bool IsId(const std::string& S)
		{
			if (S.empty())
			{
				return false;
			}
			for (const char C : S)
			{
				const bool bOk = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_';
				if (!bOk)
				{
					return false;
				}
			}
			return true;
		}

		/** What Load() gathers on the way: the flags by name, which were set and which were asked for, the first error. */
		struct FLoading
		{
			std::vector<std::string> Flags;
			uint64_t Settable = 0, Asked = 0;
			std::string Error;

			bool Fail(const std::string& Why)
			{
				if (Error.empty())
				{
					Error = Why;
				}
				return false;
			}
			/** The flag's bit; 0 with an error when the name is bad or there are too many. */
			uint64_t Bit(const std::string& Name)
			{
				if (!IsId(Name))
				{
					Fail("adventure: bad flag name '" + Name + "'");
					return 0;
				}
				for (size_t I = 0; I < Flags.size(); ++I)
				{
					if (Flags[I] == Name)
					{
						return uint64_t(1) << I;
					}
				}
				if (Flags.size() >= 64)
				{
					Fail("adventure: more than 64 flags");
					return 0;
				}
				Flags.push_back(Name);
				return uint64_t(1) << (Flags.size() - 1);
			}
			bool Cond(const std::string& Text, FCond& Out)
			{
				FTerm Term;
				std::string Word;
				bool bNot = false, bAnyWord = false;
				for (size_t I = 0; I <= Text.size(); ++I)
				{
					const char C = I < Text.size() ? Text[I] : '|';
					if (C == ' ')
					{
						continue;
					}
					if (C == '!' && Word.empty())
					{
						bNot = !bNot;
						continue;
					}
					if (C == '&' || C == '|')
					{
						if (Word.empty())
						{
							if (I == Text.size() && !bAnyWord && !bNot)
							{
								break;   // an empty condition
							}
							return Fail("adventure: bad condition '" + Text + "'");
						}
						const uint64_t B = Bit(Word);
						if (!B)
						{
							return false;
						}
						Asked |= B;
						(bNot ? Term.Not : Term.Need) |= B;
						Word.clear();
						bNot = false;
						bAnyWord = true;
						if (C == '|')
						{
							Out.Any.push_back(Term);
							Term = FTerm();
						}
						continue;
					}
					Word += C;
				}
				return true;
			}
			bool Lines(const LyudJson::FValue& V, std::vector<FLine>& Out)
			{
				if (V.IsNull())
				{
					return true;
				}
				if (V.Type != LyudJson::FValue::Array)
				{
					return Fail("adventure: a text list must be an array");
				}
				for (const LyudJson::FValue& Item : V.Items)
				{
					FLine Line;
					if (Item.Type == LyudJson::FValue::String)
					{
						Line.Key = Item.Str;
					}
					else if (Item.Type == LyudJson::FValue::Object)
					{
						Line.Key = Item["k"].S();
						if (!Cond(Item["if"].S(), Line.If))
						{
							return false;
						}
					}
					if (Line.Key.empty())
					{
						return Fail("adventure: a text line without a key");
					}
					Out.push_back(Line);
				}
				return true;
			}
			bool Sets(const LyudJson::FValue& V, uint64_t& Set, uint64_t& Clear)
			{
				if (V.IsNull())
				{
					return true;
				}
				if (V.Type != LyudJson::FValue::Array)
				{
					return Fail("adventure: \"set\" must be an array");
				}
				for (const LyudJson::FValue& Item : V.Items)
				{
					std::string Name = Item.S();
					const bool bClear = !Name.empty() && Name[0] == '-';
					if (bClear)
					{
						Name.erase(0, 1);
					}
					const uint64_t B = Bit(Name);
					if (!B)
					{
						return false;
					}
					if (bClear)
					{
						Clear |= B;
					}
					else
					{
						Set |= B;
						Settable |= B;
					}
				}
				return true;
			}
		};

		void AddUnique(std::vector<std::string>& List, const std::string& S)
		{
			if (!S.empty() && std::find(List.begin(), List.end(), S) == List.end())
			{
				List.push_back(S);
			}
		}

		std::vector<std::string> Split(const std::string& S, char Sep)
		{
			std::vector<std::string> Parts(1);
			for (const char C : S)
			{
				if (C == Sep)
				{
					Parts.emplace_back();
				}
				else
				{
					Parts.back() += C;
				}
			}
			return Parts;
		}
	}

	bool FAdventure::Load(const std::string& JsonText, std::string* Error)
	{
		NodeList.clear();
		FlagNames.clear();
		Title.clear();
		Best.clear();
		StartNode = 0;
		ChoiceCount = 0;
		At = 0;
		Flags = 0;
		MoveCount = 0;
		EndId.clear();
		Out.clear();

		Detail::FLoading L;
		LyudJson::FValue Root;
		std::string JsonError;
		bool bOk = LyudJson::Parse(JsonText, Root, JsonError);
		if (!bOk)
		{
			L.Fail("adventure: " + JsonError);
		}
		else if (Root.Type != LyudJson::FValue::Object || Root["nodes"].Type != LyudJson::FValue::Object || Root["nodes"].Size() == 0)
		{
			bOk = L.Fail("adventure: no \"nodes\"");
		}

		std::vector<Detail::FNode> Loaded;
		std::vector<std::vector<std::string>> Targets;   // per node, per choice: the "go" id
		if (bOk)
		{
			for (const auto& Field : Root["nodes"].Fields)
			{
				Detail::FNode Node;
				Node.Id = Field.first;
				const LyudJson::FValue& V = Field.second;
				if (!Detail::IsId(Node.Id))
				{
					bOk = L.Fail("adventure: bad node id '" + Node.Id + "'");
					break;
				}
				for (const Detail::FNode& Other : Loaded)
				{
					if (Other.Id == Node.Id)
					{
						bOk = L.Fail("adventure: node '" + Node.Id + "' twice");
					}
				}
				bOk = bOk && L.Lines(V["text"], Node.Text) && L.Sets(V["set"], Node.Set, Node.Clear);
				if (bOk && (V["choices"].Type != LyudJson::FValue::Array || V["choices"].Size() == 0))
				{
					bOk = L.Fail("adventure: node '" + Node.Id + "' has no choices");
				}
				if (!bOk)
				{
					break;
				}
				Targets.emplace_back();
				for (const LyudJson::FValue& C : V["choices"].Items)
				{
					Detail::FChoice Choice;
					Choice.Text = C["text"].S();
					Choice.End = C["end"].S();
					Choice.Id = ChoiceCount++;
					const std::string Go = C["go"].S();
					if (Choice.Text.empty())
					{
						bOk = L.Fail("adventure: a choice of '" + Node.Id + "' has no text");
					}
					else if (!Choice.End.empty() && (!Go.empty() || !Detail::IsId(Choice.End)))
					{
						bOk = L.Fail("adventure: a choice of '" + Node.Id + "' has a bad \"end\" (or both \"end\" and \"go\")");
					}
					bOk = bOk && L.Cond(C["if"].S(), Choice.If) && L.Lines(C["say"], Choice.Say) && L.Sets(C["set"], Choice.Set, Choice.Clear);
					if (!bOk)
					{
						break;
					}
					Targets.back().push_back(Go);
					Node.Choices.push_back(Choice);
				}
				if (!bOk)
				{
					break;
				}
				Loaded.push_back(Node);
			}
		}
		auto IndexOf = [&Loaded](const std::string& Id)
		{
			for (size_t I = 0; I < Loaded.size(); ++I)
			{
				if (Loaded[I].Id == Id)
				{
					return int(I);
				}
			}
			return -1;
		};
		int First = 0;
		if (bOk)
		{
			for (size_t N = 0; N < Loaded.size() && bOk; ++N)
			{
				for (size_t C = 0; C < Loaded[N].Choices.size() && bOk; ++C)
				{
					const std::string& Go = Targets[N][C];
					if (!Go.empty())
					{
						Loaded[N].Choices[C].Go = IndexOf(Go);
						if (Loaded[N].Choices[C].Go < 0)
						{
							bOk = L.Fail("adventure: '" + Loaded[N].Id + "' goes to unknown node '" + Go + "'");
						}
					}
				}
			}
			First = IndexOf(Root["start"].S());
			if (bOk && First < 0)
			{
				bOk = L.Fail("adventure: unknown start node '" + Root["start"].S() + "'");
			}
			if (bOk && (L.Asked & ~L.Settable) != 0)
			{
				for (size_t I = 0; I < L.Flags.size(); ++I)
				{
					if (((L.Asked & ~L.Settable) >> I) & 1)
					{
						bOk = L.Fail("adventure: flag '" + L.Flags[I] + "' is asked for but never set");
						break;
					}
				}
			}
		}
		if (!bOk)
		{
			ChoiceCount = 0;
			if (Error)
			{
				*Error = L.Error.empty() ? std::string("adventure: bad data") : L.Error;
			}
			return false;
		}
		NodeList = Loaded;
		FlagNames = L.Flags;
		Title = Root["title"].S();
		Best = Root["best"].S();
		StartNode = First;
		return true;
	}

	void FAdventure::PrintNode()
	{
		const Detail::FNode& Node = NodeList[size_t(At)];
		for (const Detail::FLine& Line : Node.Text)
		{
			if (Line.If.Holds(Flags))
			{
				Out.push_back(Line.Key);
			}
		}
		Flags = (Flags | Node.Set) & ~Node.Clear;
	}

	void FAdventure::Start()
	{
		Out.clear();
		EndId.clear();
		Flags = 0;
		MoveCount = 0;
		At = StartNode;
		if (!NodeList.empty())
		{
			PrintNode();
		}
	}

	std::vector<const Detail::FChoice*> FAdventure::Open() const
	{
		std::vector<const Detail::FChoice*> List;
		if (NodeList.empty() || Ended())
		{
			return List;
		}
		for (const Detail::FChoice& C : NodeList[size_t(At)].Choices)
		{
			if (C.If.Holds(Flags))
			{
				List.push_back(&C);
			}
		}
		return List;
	}

	std::vector<std::string> FAdventure::Choices() const
	{
		std::vector<std::string> Keys;
		for (const Detail::FChoice* C : Open())
		{
			Keys.push_back(C->Text);
		}
		return Keys;
	}

	std::vector<int> FAdventure::OpenChoiceIds() const
	{
		std::vector<int> Ids;
		for (const Detail::FChoice* C : Open())
		{
			Ids.push_back(C->Id);
		}
		return Ids;
	}

	bool FAdventure::Choose(int Number)
	{
		const std::vector<const Detail::FChoice*> List = Open();
		if (Number < 1 || Number > int(List.size()))
		{
			return false;
		}
		const Detail::FChoice& C = *List[size_t(Number - 1)];
		Out.clear();
		for (const Detail::FLine& Line : C.Say)
		{
			if (Line.If.Holds(Flags))
			{
				Out.push_back(Line.Key);
			}
		}
		Flags = (Flags | C.Set) & ~C.Clear;
		MoveCount++;
		if (!C.End.empty())
		{
			EndId = C.End;
		}
		else if (C.Go >= 0)
		{
			At = C.Go;
			PrintNode();
		}
		return true;
	}

	std::string FAdventure::Node() const
	{
		return NodeList.empty() ? std::string() : NodeList[size_t(At)].Id;
	}

	std::vector<std::string> FAdventure::Nodes() const
	{
		std::vector<std::string> Ids;
		for (const Detail::FNode& N : NodeList)
		{
			Ids.push_back(N.Id);
		}
		return Ids;
	}

	std::string FAdventure::StateKey() const
	{
		std::string Key = Node() + ";";
		bool bFirst = true;
		for (size_t I = 0; I < FlagNames.size(); ++I)
		{
			if ((Flags >> I) & 1)
			{
				Key += (bFirst ? "" : ",") + FlagNames[I];
				bFirst = false;
			}
		}
		return Key + ";" + EndId;
	}

	std::string FAdventure::Save() const
	{
		std::string S = "1;" + Node() + ";" + std::to_string(MoveCount) + ";";
		bool bFirst = true;
		for (size_t I = 0; I < FlagNames.size(); ++I)
		{
			if ((Flags >> I) & 1)
			{
				S += (bFirst ? "" : ",") + FlagNames[I];
				bFirst = false;
			}
		}
		return S + ";" + EndId;
	}

	bool FAdventure::Restore(const std::string& S)
	{
		const std::vector<std::string> Parts = Detail::Split(S, ';');
		if (Parts.size() != 5 || Parts[0] != "1" || NodeList.empty())
		{
			return false;
		}
		int NewAt = -1;
		for (size_t I = 0; I < NodeList.size(); ++I)
		{
			if (NodeList[I].Id == Parts[1])
			{
				NewAt = int(I);
			}
		}
		if (NewAt < 0 || Parts[2].empty() || Parts[2].size() > 7)
		{
			return false;
		}
		int NewMoves = 0;
		for (const char C : Parts[2])
		{
			if (C < '0' || C > '9')
			{
				return false;
			}
			NewMoves = NewMoves * 10 + (C - '0');
		}
		uint64_t NewFlags = 0;
		if (!Parts[3].empty())
		{
			for (const std::string& Name : Detail::Split(Parts[3], ','))
			{
				uint64_t B = 0;
				for (size_t I = 0; I < FlagNames.size(); ++I)
				{
					if (FlagNames[I] == Name)
					{
						B = uint64_t(1) << I;
					}
				}
				if (!B)
				{
					return false;
				}
				NewFlags |= B;
			}
		}
		if (!Parts[4].empty())
		{
			const std::vector<std::string> Known = Endings();
			if (std::find(Known.begin(), Known.end(), Parts[4]) == Known.end())
			{
				return false;
			}
		}
		At = NewAt;
		Flags = NewFlags;
		MoveCount = NewMoves;
		EndId = Parts[4];
		Out.clear();
		if (!Ended())
		{
			// the node's words again, as the flags stand now (its own "set" was done when he first came)
			for (const Detail::FLine& Line : NodeList[size_t(At)].Text)
			{
				if (Line.If.Holds(Flags))
				{
					Out.push_back(Line.Key);
				}
			}
		}
		return true;
	}

	std::vector<std::string> FAdventure::AllKeys() const
	{
		std::vector<std::string> Keys;
		Detail::AddUnique(Keys, Title);
		for (const Detail::FNode& N : NodeList)
		{
			for (const Detail::FLine& Line : N.Text)
			{
				Detail::AddUnique(Keys, Line.Key);
			}
			for (const Detail::FChoice& C : N.Choices)
			{
				Detail::AddUnique(Keys, C.Text);
				for (const Detail::FLine& Line : C.Say)
				{
					Detail::AddUnique(Keys, Line.Key);
				}
			}
		}
		return Keys;
	}

	std::vector<std::string> FAdventure::Endings() const
	{
		std::vector<std::string> Ids;
		for (const Detail::FNode& N : NodeList)
		{
			for (const Detail::FChoice& C : N.Choices)
			{
				Detail::AddUnique(Ids, C.End);
			}
		}
		return Ids;
	}
}
}
