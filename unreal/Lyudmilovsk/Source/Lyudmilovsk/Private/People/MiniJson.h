// A small JSON reader in plain C++ for the people's data files (Content/PeopleData), so the life simulation can load
// them inside and outside the engine (tools/peoplesim). Reads objects, arrays, strings, numbers, true/false/null.
#pragma once

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace LyudJson
{
	struct FValue
	{
		enum EType { Null, Bool, Number, String, Array, Object } Type = Null;
		double Num = 0.0;
		bool B = false;
		std::string Str;
		std::vector<FValue> Items;
		std::vector<std::pair<std::string, FValue>> Fields;

		const FValue* Find(const std::string& Key) const
		{
			for (const auto& F : Fields)
			{
				if (F.first == Key)
				{
					return &F.second;
				}
			}
			return nullptr;
		}
		const FValue& operator[](const std::string& Key) const
		{
			static const FValue None;
			const FValue* V = Find(Key);
			return V ? *V : None;
		}
		const FValue& operator[](size_t I) const
		{
			static const FValue None;
			return I < Items.size() ? Items[I] : None;
		}
		size_t Size() const { return Type == Array ? Items.size() : Fields.size(); }
		bool IsNull() const { return Type == Null; }
		float F(float Default = 0.f) const { return Type == Number ? (float)Num : (Type == Bool ? (B ? 1.f : 0.f) : Default); }
		int I(int Default = 0) const { return Type == Number ? (int)Num : Default; }
		std::string S(const std::string& Default = std::string()) const { return Type == String ? Str : Default; }
	};

	class FParser
	{
	public:
		explicit FParser(const std::string& InText) : T(InText) {}

		bool Parse(FValue& Out, std::string& Error)
		{
			if (!Value(Out))
			{
				Error = "json: bad value near byte " + std::to_string(P);
				return false;
			}
			return true;
		}

	private:
		const std::string& T;
		size_t P = 0;

		void Ws()
		{
			while (P < T.size() && (T[P] == ' ' || T[P] == '\n' || T[P] == '\r' || T[P] == '\t'))
			{
				P++;
			}
		}
		bool Lit(const char* S)
		{
			size_t N = 0;
			while (S[N])
			{
				N++;
			}
			if (T.compare(P, N, S) == 0)
			{
				P += N;
				return true;
			}
			return false;
		}
		bool Str(std::string& Out)
		{
			if (P >= T.size() || T[P] != '"')
			{
				return false;
			}
			P++;
			while (P < T.size() && T[P] != '"')
			{
				char C = T[P++];
				if (C == '\\' && P < T.size())
				{
					const char E = T[P++];
					switch (E)
					{
					case 'n': C = '\n'; break;
					case 't': C = '\t'; break;
					case 'r': C = '\r'; break;
					case 'b': C = '\b'; break;
					case 'f': C = '\f'; break;
					case 'u':
					{
						// \uXXXX as UTF-8 (no surrogate pairs: the data files are plain)
						unsigned Code = (unsigned)std::strtoul(T.substr(P, 4).c_str(), nullptr, 16);
						P += 4;
						if (Code < 0x80)
						{
							Out += (char)Code;
						}
						else if (Code < 0x800)
						{
							Out += (char)(0xC0 | (Code >> 6));
							Out += (char)(0x80 | (Code & 0x3F));
						}
						else
						{
							Out += (char)(0xE0 | (Code >> 12));
							Out += (char)(0x80 | ((Code >> 6) & 0x3F));
							Out += (char)(0x80 | (Code & 0x3F));
						}
						continue;
					}
					default: C = E; break;
					}
				}
				Out += C;
			}
			if (P >= T.size())
			{
				return false;
			}
			P++;
			return true;
		}
		bool Value(FValue& V)
		{
			Ws();
			if (P >= T.size())
			{
				return false;
			}
			const char C = T[P];
			if (C == '{')
			{
				V.Type = FValue::Object;
				P++;
				Ws();
				if (P < T.size() && T[P] == '}')
				{
					P++;
					return true;
				}
				while (true)
				{
					Ws();
					std::string Key;
					if (!Str(Key))
					{
						return false;
					}
					Ws();
					if (P >= T.size() || T[P] != ':')
					{
						return false;
					}
					P++;
					V.Fields.emplace_back(Key, FValue());
					if (!Value(V.Fields.back().second))
					{
						return false;
					}
					Ws();
					if (P < T.size() && T[P] == ',')
					{
						P++;
						continue;
					}
					if (P < T.size() && T[P] == '}')
					{
						P++;
						return true;
					}
					return false;
				}
			}
			if (C == '[')
			{
				V.Type = FValue::Array;
				P++;
				Ws();
				if (P < T.size() && T[P] == ']')
				{
					P++;
					return true;
				}
				while (true)
				{
					V.Items.emplace_back();
					if (!Value(V.Items.back()))
					{
						return false;
					}
					Ws();
					if (P < T.size() && T[P] == ',')
					{
						P++;
						continue;
					}
					if (P < T.size() && T[P] == ']')
					{
						P++;
						return true;
					}
					return false;
				}
			}
			if (C == '"')
			{
				V.Type = FValue::String;
				return Str(V.Str);
			}
			if (Lit("true"))
			{
				V.Type = FValue::Bool;
				V.B = true;
				return true;
			}
			if (Lit("false"))
			{
				V.Type = FValue::Bool;
				return true;
			}
			if (Lit("null"))
			{
				return true;
			}
			char* End = nullptr;
			V.Num = std::strtod(T.c_str() + P, &End);
			if (End == T.c_str() + P)
			{
				return false;
			}
			V.Type = FValue::Number;
			P = End - T.c_str();
			return true;
		}
	};

	inline bool Parse(const std::string& Text, FValue& Out, std::string& Error)
	{
		FParser Parser(Text);
		return Parser.Parse(Out, Error);
	}
}
