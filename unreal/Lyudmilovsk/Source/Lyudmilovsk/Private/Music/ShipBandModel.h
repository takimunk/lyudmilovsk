#pragma once

// The motor ship's jazz quartet «Полуденный квартет» (docs/ship.md "The band"): piano, double bass, brushes and a reed
// (clarinet / tenor sax), synthesised as they play from lead sheets. No recordings, nothing paid. Plain C++ (no engine
// types): tools/bandsim renders and checks every tune offline.
//
//   LyudBand::FBook Book = LyudBand::Parse(Text, Errors);       // Content/Music/band_tunes.txt
//   LyudBand::FPlayer P(32000);
//   P.Start(Book.Tunes[0], Seed, /*soft*/ false, /*at seconds*/ 0.0);
//   P.Render(InterleavedLR, Frames);                             // stereo floats, |x| < 0.98
//
// band_tunes.txt: a block per tune, then the sets.
//   [careless_love]
//   composer = traditional            who wrote the MELODY (nothing is sung); the house rule: dead by 1955
//   died = 0                          the year (0: traditional / anonymous, older than that)
//   source = ...                      where the tune comes from, one line
//   bpm = 132                         quarter notes a minute
//   meter = 4                         beats in a bar (3 for a waltz)
//   feel = swing                      swing | foxtrot | waltz | tango | ballad | ragtime
//   key = F                           the tonic ("Dm": minor)
//   reed = clarinet                   clarinet | sax (the timbre of the fourth voice)
//   swing = 0.64                      where the off-beat eighth falls (0.5 straight .. 0.67 triplets); default by feel
//   plan = head solo piano fours out  one word per chorus (default by length): head | pianohead | solo | piano | fours | out
//   pickup = C4/0.5 D4/0.5            the notes before bar 1 (played into every head)
//   form = A A B A                    the sections in order = one chorus
//   A: notes = E4 E4/0.5 r/0.5 | ...  pitch/beats (1 by default), r a rest, | a bar line (CHECKED: it must fall on one)
//   A: chords = C/4 G7/2 C/2          symbol/beats; symbols: C C6 Cmaj7 C7 C9 C7b9 Cm Cm6 Cm7 Cdim Cdim7 Cm7b5 Caug C7#5 Csus4
//   [set concert1]
//   tunes = a b c                     the programme in order
//   pause = 22                        seconds between tunes (applause is the soundscape's)
//   interval = 600                    seconds after the last tune before the programme comes round again
//   style = soft                      soft: the dinner trio (the reed only in the last chorus, everything quieter)
// Titles are the string keys band.tune.<id>.title.
//
// The arranger makes every bar a pure function of (tune, seed, bar number): a set can be joined at any moment (Start's
// AtSeconds) and sounds as it would have, had it been played from the top.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "Music/InstrumentSynth.h"

namespace LyudBand
{
	enum class EFeel : uint8_t { Swing, Foxtrot, Waltz, Tango, Ballad, Ragtime };
	enum class ERole : uint8_t { Intro, Head, PianoHead, Solo, PianoSolo, Fours, Out, Tag, Final };
	enum EInst : int { Piano = 0, Bass = 1, Reed = 2, Drums = 3, NumInst = 4 };
	enum EDrum : int { Kick = 0, Tap = 1, Swish = 2, Ride = 3, Hat = 4, Crash = 5 };

	/** The ranges the arranger keeps to (MIDI). */
	constexpr int BassLo = 28, BassHi = 55;
	constexpr int ClarLo = 50, ClarHi = 89;
	constexpr int SaxLo = 44, SaxHi = 80;
	constexpr int PianoLo = 33, PianoHi = 98;

	// ------------------------------------------------------------------------------------------------------ chords

	struct FKind
	{
		const char* Name;
		int Tones[4];    // the chord's notes above its root (-1: none)
		int Voice[3];    // what the pianist's left hand plays when comping (rootless)
		int Scale[8];    // the scale an improviser plays over it (-1 ends)
	};

	inline const FKind* Kinds(int& Count)
	{
		static const FKind K[] = {
			{ "",      { 0, 4, 7, -1 },  { 4, 7, 9 },   { 0, 2, 4, 7, 9, -1, -1, -1 } },
			{ "6",     { 0, 4, 7, 9 },   { 4, 7, 9 },   { 0, 2, 4, 7, 9, -1, -1, -1 } },
			{ "maj7",  { 0, 4, 7, 11 },  { 4, 7, 11 },  { 0, 2, 4, 7, 9, 11, -1, -1 } },
			{ "7",     { 0, 4, 7, 10 },  { 4, 10, 14 }, { 0, 2, 4, 5, 7, 9, 10, -1 } },
			{ "9",     { 0, 4, 7, 10 },  { 4, 10, 14 }, { 0, 2, 4, 5, 7, 9, 10, -1 } },
			{ "7b9",   { 0, 4, 7, 10 },  { 4, 10, 13 }, { 0, 1, 4, 5, 7, 8, 10, -1 } },
			{ "m",     { 0, 3, 7, -1 },  { 3, 7, 12 },  { 0, 2, 3, 5, 7, 8, 10, -1 } },
			{ "m6",    { 0, 3, 7, 9 },   { 3, 7, 9 },   { 0, 2, 3, 5, 7, 9, 10, -1 } },
			{ "m7",    { 0, 3, 7, 10 },  { 3, 7, 10 },  { 0, 2, 3, 5, 7, 9, 10, -1 } },
			{ "dim",   { 0, 3, 6, 9 },   { 3, 6, 9 },   { 0, 2, 3, 5, 6, 8, 9, 11 } },
			{ "dim7",  { 0, 3, 6, 9 },   { 3, 6, 9 },   { 0, 2, 3, 5, 6, 8, 9, 11 } },
			{ "m7b5",  { 0, 3, 6, 10 },  { 3, 6, 10 },  { 0, 1, 3, 5, 6, 8, 10, -1 } },
			{ "aug",   { 0, 4, 8, -1 },  { 4, 8, 12 },  { 0, 2, 4, 6, 8, 10, -1, -1 } },
			{ "7#5",   { 0, 4, 8, 10 },  { 4, 8, 10 },  { 0, 2, 4, 6, 8, 10, -1, -1 } },
			{ "sus4",  { 0, 5, 7, -1 },  { 5, 7, 12 },  { 0, 2, 5, 7, 9, 10, -1, -1 } },
		};
		Count = (int)(sizeof(K) / sizeof(K[0]));
		return K;
	}
	constexpr int KindDom7 = 3, KindDom7b9 = 5, KindMajor = 0, KindMaj6 = 1, KindMinor = 6, KindMin6 = 7;

	inline const FKind& Kind(int Index)
	{
		int N = 0;
		const FKind* K = Kinds(N);
		return K[Index >= 0 && Index < N ? Index : 0];
	}

	/** "Bb7" -> root 10, the kind's index; false: not a symbol we know. */
	inline bool ParseChord(const std::string& S, int& Root, int& KindIndex)
	{
		static const int Base[7] = { 9, 11, 0, 2, 4, 5, 7 };
		if (S.empty() || S[0] < 'A' || S[0] > 'G')
		{
			return false;
		}
		Root = Base[S[0] - 'A'];
		size_t At = 1;
		if (At < S.size() && S[At] == '#')
		{
			Root = (Root + 1) % 12;
			++At;
		}
		else if (At < S.size() && S[At] == 'b')
		{
			Root = (Root + 11) % 12;
			++At;
		}
		const std::string Rest = S.substr(At);
		int N = 0;
		const FKind* K = Kinds(N);
		for (int I = 0; I < N; ++I)
		{
			if (Rest == K[I].Name)
			{
				KindIndex = I;
				return true;
			}
		}
		return false;
	}

	struct FChordAt
	{
		double Beat = 0.0, Len = 4.0;
		int Root = 0, KindIndex = 0;
	};

	inline unsigned Mask(const int* Intervals, int Count, int Root)
	{
		unsigned M = 0;
		for (int I = 0; I < Count; ++I)
		{
			if (Intervals[I] >= 0)
			{
				M |= 1u << ((Root + Intervals[I]) % 12);
			}
		}
		return M;
	}

	// ------------------------------------------------------------------------------------------------- the lead sheets

	struct FNote
	{
		double Beat = 0.0, Len = 1.0;
		int Midi = 60;
	};

	struct FTune
	{
		std::string Id, Composer, Source;
		int Died = -1;
		double Bpm = 120.0;
		int Meter = 4;
		EFeel Feel = EFeel::Swing;
		double Swing = 0.0;
		int KeyRoot = 0;
		bool bMinor = false;
		bool bSax = false;
		std::vector<ERole> Plan;
		std::vector<FNote> Pickup;   // beats < 0, before bar 1
		std::vector<FNote> Notes;    // one chorus, sounding notes only, by beat
		std::vector<FChordAt> Chords;
		std::vector<int> BarNote;    // per bar of the form: the first note at or after its start (FormBars + 1 entries)
		int FormBars = 0;
		bool bPickupFree = true;     // the chorus's last bar is silent where the pick-up falls: every head gets its pick-up
		double Seconds = 0.0, SecondsSoft = 0.0;  // how long it plays (the two styles), tail included

		double FormBeats() const { return (double)FormBars * Meter; }
		const FChordAt& ChordAtBeat(double Beat) const
		{
			const double L = FormBeats();
			double B = std::fmod(Beat, L);
			if (B < 0.0)
			{
				B += L;
			}
			size_t Pick = 0;
			for (size_t I = 0; I < Chords.size(); ++I)
			{
				if (Chords[I].Beat <= B + 1e-6)
				{
					Pick = I;
				}
				else
				{
					break;
				}
			}
			return Chords[Pick];
		}
	};

	struct FSet
	{
		std::string Id;
		std::vector<int> Tunes;
		double Pause = 22.0, Interval = 600.0;
		bool bSoft = false;
	};

	struct FBook
	{
		std::vector<FTune> Tunes;
		std::vector<FSet> Sets;
		int FindTune(const std::string& Id) const
		{
			for (size_t I = 0; I < Tunes.size(); ++I)
			{
				if (Tunes[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}
		int FindSet(const std::string& Id) const
		{
			for (size_t I = 0; I < Sets.size(); ++I)
			{
				if (Sets[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}
	};

	inline uint32_t Hash(uint32_t A, uint32_t B, uint32_t C = 0, uint32_t D = 0)
	{
		uint32_t H = 0x811C9DC5u ^ A;
		H = (H ^ (H >> 15)) * 0x2C1B3C6Du;
		H ^= B + 0x9E3779B9u + (H << 6) + (H >> 2);
		H = (H ^ (H >> 13)) * 0x297A2D39u;
		H ^= C + 0x85EBCA6Bu + (H << 6) + (H >> 2);
		H = (H ^ (H >> 16)) * 0x7FEB352Du;
		H ^= D + 0xC2B2AE35u + (H << 6) + (H >> 2);
		H = (H ^ (H >> 15)) * 0x846CA68Bu;
		return H ^ (H >> 16);
	}

	inline uint32_t HashString(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (const char C : S)
		{
			H = (H ^ (uint8_t)C) * 16777619u;
		}
		return H;
	}

	struct FRng
	{
		uint32_t S;
		explicit FRng(uint32_t Seed) : S(Seed ? Seed : 1u) {}
		uint32_t Next()
		{
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			return S;
		}
		/** 0..1 */
		float U() { return (float)(Next() >> 8) / 16777216.f; }
		int R(int N) { return N > 0 ? (int)(Next() % (uint32_t)N) : 0; }
		bool P(float Chance) { return U() < Chance; }
	};

	/** One bar of a performance. */
	struct FBar
	{
		ERole Role = ERole::Head;
		int FormBar = 0;      // which bar of the chorus
		int Chorus = 0;       // which chorus of the plan (-1: intro)
		double Start = 0.0;   // seconds
		double Spb0 = 0.5, Spb1 = 0.5;  // seconds a beat at its start and its end (a ritardando)
		bool bLastOfRole = false;
	};

	/** The whole performance of a tune, bar by bar (no notes yet): intro, the plan's choruses, a tag, the last chord. */
	inline void BuildBars(const FTune& T, bool bSoft, std::vector<FBar>& Out, double& Seconds)
	{
		Out.clear();
		const double Spb = 60.0 / T.Bpm;
		const int Intro = T.FormBars >= 8 ? 4 : 2;
		auto Add = [&Out](ERole R, int FormBar, int Chorus)
		{
			FBar B;
			B.Role = R;
			B.FormBar = FormBar;
			B.Chorus = Chorus;
			Out.push_back(B);
		};
		for (int I = 0; I < Intro; ++I)
		{
			Add(ERole::Intro, T.FormBars - Intro + I, -1);
		}
		for (size_t C = 0; C < T.Plan.size(); ++C)
		{
			ERole R = T.Plan[C];
			if (bSoft)
			{
				// the dinner trio: the piano has the tune, the reed comes in for the last chorus only
				R = R == ERole::Head ? ERole::PianoHead : (R == ERole::Solo || R == ERole::Fours ? ERole::PianoSolo : R);
			}
			for (int I = 0; I < T.FormBars; ++I)
			{
				Add(R, I, (int)C);
			}
		}
		const bool bTag = (T.Feel == EFeel::Swing || T.Feel == EFeel::Foxtrot || T.Feel == EFeel::Ragtime) && T.FormBars >= 16;
		if (bTag)
		{
			for (int I = 0; I < 4; ++I)
			{
				Add(ERole::Tag, T.FormBars - 4 + I, (int)T.Plan.size());
			}
		}
		Add(ERole::Final, T.FormBars - 1, (int)T.Plan.size());
		const int N = (int)Out.size();
		double At = 0.0;
		for (int I = 0; I < N; ++I)
		{
			FBar& B = Out[(size_t)I];
			B.bLastOfRole = I + 1 >= N || Out[(size_t)I + 1].Role != B.Role || Out[(size_t)I + 1].Chorus != B.Chorus;
			double M0 = 1.0, M1 = 1.0;
			const bool bTango = T.Feel == EFeel::Tango;
			if (I == N - 3)
			{
				M1 = bTango ? 1.0 : 1.12;
			}
			else if (I == N - 2)
			{
				M0 = bTango ? 1.0 : 1.12;
				M1 = bTango ? 1.12 : 1.5;
			}
			else if (I == N - 1)
			{
				M0 = M1 = bTango ? 1.15 : 1.7;
			}
			B.Spb0 = Spb * M0;
			B.Spb1 = Spb * M1;
			B.Start = At;
			At += T.Meter * 0.5 * (B.Spb0 + B.Spb1);
		}
		Seconds = At + 3.0;  // the last chord dies away
	}

	// ----------------------------------------------------------------------------------------------------- the parser

	inline std::string Trim(const std::string& S)
	{
		size_t A = 0, B = S.size();
		while (A < B && (S[A] == ' ' || S[A] == '\t' || S[A] == '\r'))
		{
			++A;
		}
		while (B > A && (S[B - 1] == ' ' || S[B - 1] == '\t' || S[B - 1] == '\r'))
		{
			--B;
		}
		return S.substr(A, B - A);
	}

	struct FSection
	{
		std::string Name;
		std::vector<FNote> Notes;
		std::vector<FChordAt> Chords;
		double NoteBeats = 0.0, ChordBeats = 0.0;
	};

	inline bool ParseLen(const std::string& Tok, std::string& Head, double& Len)
	{
		Head = Tok;
		Len = 1.0;
		const size_t Slash = Tok.find('/');
		if (Slash != std::string::npos)
		{
			Head = Tok.substr(0, Slash);
			char* End = nullptr;
			const std::string L = Tok.substr(Slash + 1);
			Len = std::strtod(L.c_str(), &End);
			if (!End || *End != 0 || !(Len > 0.0) || Len > 64.0)
			{
				return false;
			}
		}
		return true;
	}

	/** Beats snapped to the twelfth (so three "/0.333" make a beat exactly). */
	inline double Snap(double Beat) { return std::floor(Beat * 12.0 + 0.5) / 12.0; }

	inline void ParseNotes(const std::string& Value, int Meter, std::vector<FNote>& Notes, double& Beats, const std::string& Where,
		std::vector<std::string>& Errors, bool bBars)
	{
		std::istringstream In(Value);
		std::string W;
		while (In >> W)
		{
			if (W == "|")
			{
				const double Bar = Beats / Meter;
				if (bBars && std::fabs(Bar - std::floor(Bar + 0.5)) > 1e-6)
				{
					Errors.push_back(Where + ": a bar line at beat " + std::to_string(Beats) + ", not on a bar");
				}
				continue;
			}
			std::string Head;
			double Len = 1.0;
			if (!ParseLen(W, Head, Len))
			{
				Errors.push_back(Where + ": bad length in " + W);
				continue;
			}
			if (Head != "r")
			{
				FNote N;
				N.Midi = LyudInstr::NoteFromName(Head.c_str());
				if (N.Midi < 0)
				{
					Errors.push_back(Where + ": bad pitch " + W);
					continue;
				}
				N.Beat = Beats;
				N.Len = Len;
				Notes.push_back(N);
			}
			Beats = Snap(Beats + Len);
		}
	}

	inline ERole RoleFromWord(const std::string& W, bool& bOk)
	{
		bOk = true;
		if (W == "head") { return ERole::Head; }
		if (W == "pianohead") { return ERole::PianoHead; }
		if (W == "solo") { return ERole::Solo; }
		if (W == "piano") { return ERole::PianoSolo; }
		if (W == "fours") { return ERole::Fours; }
		if (W == "out") { return ERole::Out; }
		bOk = false;
		return ERole::Head;
	}

	/** Reads band_tunes.txt; Errors gets a line per problem. */
	inline FBook Parse(const std::string& Text, std::vector<std::string>& Errors)
	{
		FBook Book;
		std::istringstream In(Text);
		std::string Line;
		FTune* Tune = nullptr;
		FSet* Set = nullptr;
		std::vector<FSection> Sections;
		std::vector<std::string> Form;
		std::vector<std::string> SetTunes;
		std::vector<std::vector<std::string>> AllSetTunes;
		double PickupBeats = 0.0;

		auto Finish = [&]()
		{
			if (!Tune)
			{
				return;
			}
			FTune& T = *Tune;
			const std::string Where = "[" + T.Id + "]";
			double At = 0.0;
			for (const std::string& Name : Form)
			{
				const FSection* S = nullptr;
				for (const FSection& X : Sections)
				{
					if (X.Name == Name)
					{
						S = &X;
					}
				}
				if (!S)
				{
					Errors.push_back(Where + ": form names a section " + Name + " that is not written");
					continue;
				}
				for (FNote N : S->Notes)
				{
					N.Beat += At;
					T.Notes.push_back(N);
				}
				for (FChordAt C : S->Chords)
				{
					C.Beat += At;
					T.Chords.push_back(C);
				}
				At += S->ChordBeats;
			}
			for (const FSection& S : Sections)
			{
				const double Bars = S.ChordBeats / T.Meter;
				if (std::fabs(S.NoteBeats - S.ChordBeats) > 1e-6)
				{
					Errors.push_back(Where + " " + S.Name + ": the notes are " + std::to_string(S.NoteBeats) + " beats, the chords " +
						std::to_string(S.ChordBeats));
				}
				if (S.ChordBeats <= 0.0 || std::fabs(Bars - std::floor(Bars + 0.5)) > 1e-6)
				{
					Errors.push_back(Where + " " + S.Name + ": " + std::to_string(S.ChordBeats) + " beats is not whole bars");
				}
			}
			T.FormBars = (int)std::floor(At / T.Meter + 0.5);
			if (T.FormBars < 4 || T.Chords.empty() || T.Notes.empty())
			{
				Errors.push_back(Where + ": no form (needs form, notes and chords of 4 bars or more)");
				T.FormBars = T.FormBars < 4 ? 4 : T.FormBars;
				if (T.Chords.empty())
				{
					T.Chords.push_back(FChordAt());
				}
			}
			for (FNote& N : T.Pickup)
			{
				N.Beat -= PickupBeats;
			}
			for (const FNote& N : T.Notes)
			{
				T.bPickupFree = T.bPickupFree && N.Beat + N.Len <= At - PickupBeats + 1e-6;
			}
			T.BarNote.assign((size_t)T.FormBars + 1, (int)T.Notes.size());
			for (int B = T.FormBars - 1; B >= 0; --B)
			{
				int First = T.BarNote[(size_t)B + 1];
				while (First > 0 && T.Notes[(size_t)First - 1].Beat >= (double)B * T.Meter - 1e-6)
				{
					--First;
				}
				T.BarNote[(size_t)B] = First;
			}
			if (T.Swing <= 0.0)
			{
				T.Swing = T.Feel == EFeel::Swing ? 0.64 : T.Feel == EFeel::Foxtrot ? 0.6 : T.Feel == EFeel::Ballad ? 0.58 : 0.5;
			}
			if (T.Plan.empty())
			{
				const double Chorus = T.FormBeats() * 60.0 / T.Bpm;
				const int N = (int)std::fmin(6.0, std::fmax(2.0, std::floor(170.0 / Chorus + 0.5)));
				const bool bJazz = T.Feel == EFeel::Swing || T.Feel == EFeel::Foxtrot;
				static const ERole Mid[4] = { ERole::Solo, ERole::PianoSolo, ERole::Fours, ERole::Solo };
				T.Plan.push_back(ERole::Head);
				for (int I = 0; I < N - 2; ++I)
				{
					ERole R = Mid[I % 4];
					if (R == ERole::Fours && (!bJazz || T.FormBars < 16 || T.FormBars % 8 != 0))
					{
						R = ERole::Solo;
					}
					T.Plan.push_back(R);
				}
				T.Plan.push_back(ERole::Out);
			}
			std::vector<FBar> Bars;
			BuildBars(T, false, Bars, T.Seconds);
			BuildBars(T, true, Bars, T.SecondsSoft);
			Tune = nullptr;
		};

		int LineNo = 0;
		while (std::getline(In, Line))
		{
			++LineNo;
			const std::string T = Trim(Line);
			if (T.empty() || T[0] == '#')
			{
				continue;
			}
			if (T[0] == '[')
			{
				Finish();
				if (Set)
				{
					AllSetTunes.push_back(SetTunes);
					Set = nullptr;
				}
				const size_t E = T.find(']');
				const std::string Name = Trim(T.substr(1, E == std::string::npos ? std::string::npos : E - 1));
				Sections.clear();
				Form.clear();
				SetTunes.clear();
				PickupBeats = 0.0;
				if (Name.compare(0, 4, "set ") == 0)
				{
					Book.Sets.push_back(FSet());
					Set = &Book.Sets.back();
					Set->Id = Trim(Name.substr(4));
				}
				else
				{
					Book.Tunes.push_back(FTune());
					Tune = &Book.Tunes.back();
					Tune->Id = Name;
				}
				continue;
			}
			const size_t Eq = T.find('=');
			if ((!Tune && !Set) || Eq == std::string::npos)
			{
				Errors.push_back("line " + std::to_string(LineNo) + ": not understood: " + T);
				continue;
			}
			std::string Key = Trim(T.substr(0, Eq));
			const std::string Value = Trim(T.substr(Eq + 1));
			const std::string Where = "line " + std::to_string(LineNo);
			if (Set)
			{
				if (Key == "tunes")
				{
					std::istringstream Ws(Value);
					std::string W;
					while (Ws >> W)
					{
						SetTunes.push_back(W);
					}
				}
				else if (Key == "pause") { Set->Pause = std::atof(Value.c_str()); }
				else if (Key == "interval") { Set->Interval = std::atof(Value.c_str()); }
				else if (Key == "style") { Set->bSoft = Value == "soft"; }
				else { Errors.push_back(Where + ": unknown key " + Key); }
				continue;
			}
			std::string Sec;
			const size_t Colon = Key.find(':');
			if (Colon != std::string::npos)
			{
				Sec = Trim(Key.substr(0, Colon));
				Key = Trim(Key.substr(Colon + 1));
			}
			if (!Sec.empty())
			{
				FSection* S = nullptr;
				for (FSection& X : Sections)
				{
					if (X.Name == Sec)
					{
						S = &X;
					}
				}
				if (!S)
				{
					Sections.push_back(FSection());
					S = &Sections.back();
					S->Name = Sec;
				}
				if (Key == "notes")
				{
					ParseNotes(Value, Tune->Meter, S->Notes, S->NoteBeats, Where, Errors, true);
				}
				else if (Key == "chords")
				{
					std::istringstream Ws(Value);
					std::string W;
					while (Ws >> W)
					{
						if (W == "|")
						{
							continue;
						}
						std::string Head;
						FChordAt C;
						if (!ParseLen(W, Head, C.Len) || !ParseChord(Head, C.Root, C.KindIndex))
						{
							Errors.push_back(Where + ": unknown chord " + W);
							C.Len = C.Len > 0.0 ? C.Len : 1.0;
						}
						C.Beat = S->ChordBeats;
						S->Chords.push_back(C);
						S->ChordBeats = Snap(S->ChordBeats + C.Len);
					}
				}
				else
				{
					Errors.push_back(Where + ": unknown key " + Key);
				}
				continue;
			}
			if (Key == "composer") { Tune->Composer = Value; }
			else if (Key == "source") { Tune->Source = Value; }
			else if (Key == "died") { Tune->Died = std::atoi(Value.c_str()); }
			else if (Key == "bpm") { Tune->Bpm = std::fmin(320.0, std::fmax(30.0, std::atof(Value.c_str()))); }
			else if (Key == "meter")
			{
				Tune->Meter = std::atoi(Value.c_str());
				if (Tune->Meter != 3 && Tune->Meter != 4)
				{
					Errors.push_back(Where + ": meter must be 3 or 4");
					Tune->Meter = 4;
				}
			}
			else if (Key == "feel")
			{
				static const char* Names[6] = { "swing", "foxtrot", "waltz", "tango", "ballad", "ragtime" };
				bool bFound = false;
				for (int I = 0; I < 6; ++I)
				{
					if (Value == Names[I])
					{
						Tune->Feel = (EFeel)I;
						bFound = true;
					}
				}
				if (!bFound)
				{
					Errors.push_back(Where + ": unknown feel " + Value);
				}
			}
			else if (Key == "key")
			{
				std::string K = Value;
				Tune->bMinor = !K.empty() && K.back() == 'm';
				if (Tune->bMinor)
				{
					K.pop_back();
				}
				int Unused = 0;
				if (!ParseChord(K, Tune->KeyRoot, Unused))
				{
					Errors.push_back(Where + ": unknown key " + Value);
				}
			}
			else if (Key == "reed") { Tune->bSax = Value == "sax"; }
			else if (Key == "swing") { Tune->Swing = std::fmin(0.72, std::fmax(0.5, std::atof(Value.c_str()))); }
			else if (Key == "plan")
			{
				std::istringstream Ws(Value);
				std::string W;
				while (Ws >> W)
				{
					bool bOk = false;
					const ERole R = RoleFromWord(W, bOk);
					if (!bOk)
					{
						Errors.push_back(Where + ": unknown plan word " + W);
					}
					Tune->Plan.push_back(R);
				}
			}
			else if (Key == "pickup") { ParseNotes(Value, Tune->Meter, Tune->Pickup, PickupBeats, Where, Errors, false); }
			else if (Key == "form")
			{
				std::istringstream Ws(Value);
				std::string W;
				while (Ws >> W)
				{
					Form.push_back(W);
				}
			}
			else { Errors.push_back(Where + ": unknown key " + Key); }
		}
		Finish();
		if (Set)
		{
			AllSetTunes.push_back(SetTunes);
		}
		for (size_t S = 0; S < Book.Sets.size() && S < AllSetTunes.size(); ++S)
		{
			for (const std::string& Id : AllSetTunes[S])
			{
				const int Ix = Book.FindTune(Id);
				if (Ix < 0)
				{
					Errors.push_back("[set " + Book.Sets[S].Id + "]: no tune " + Id);
				}
				else
				{
					Book.Sets[S].Tunes.push_back(Ix);
				}
			}
			if (Book.Sets[S].Tunes.empty())
			{
				Errors.push_back("[set " + Book.Sets[S].Id + "]: empty");
			}
		}
		return Book;
	}

	// ------------------------------------------------------------------------------------------- the set as a timetable

	struct FSetPos
	{
		int Tune = -1;        // the book's index; -1: a pause or the interval
		int Slot = -1;        // which number of the programme
		int Cycle = 0;        // how many times the programme has come round
		double Offset = 0.0;  // seconds into the tune
		uint32_t Seed = 1;    // the performance's seed (another every time the programme comes round)
	};

	inline double SetCycleSeconds(const FBook& Book, const FSet& Set)
	{
		double L = Set.Interval;
		for (const int T : Set.Tunes)
		{
			L += (Set.bSoft ? Book.Tunes[(size_t)T].SecondsSoft : Book.Tunes[(size_t)T].Seconds) + Set.Pause;
		}
		return L;
	}

	/** What the band plays Seconds into a set: a pure function, so a clock jump or a load joins the set where it is. */
	inline FSetPos Locate(const FBook& Book, const FSet& Set, double Seconds)
	{
		FSetPos Pos;
		const double Cycle = SetCycleSeconds(Book, Set);
		if (Set.Tunes.empty() || Cycle <= 1.0 || Seconds < 0.0)
		{
			return Pos;
		}
		Pos.Cycle = (int)std::floor(Seconds / Cycle);
		double At = Seconds - Pos.Cycle * Cycle;
		At -= 4.0;  // a breath before the first number
		for (size_t I = 0; I < Set.Tunes.size(); ++I)
		{
			const FTune& T = Book.Tunes[(size_t)Set.Tunes[I]];
			const double Len = Set.bSoft ? T.SecondsSoft : T.Seconds;
			if (At < 0.0)
			{
				return Pos;
			}
			if (At < Len)
			{
				Pos.Tune = Set.Tunes[I];
				Pos.Slot = (int)I;
				Pos.Offset = At;
				Pos.Seed = Hash(HashString(Set.Id), (uint32_t)Pos.Cycle, (uint32_t)I, HashString(T.Id)) | 1u;
				return Pos;
			}
			At -= Len + Set.Pause;
		}
		return Pos;
	}

	// ------------------------------------------------------------------------------------------------- the instruments

	inline float SinTable(float Phase01)
	{
		// a 2048-step sine, linear between steps: no sin() per sample
		struct FTable
		{
			float V[2049];
			FTable()
			{
				for (int I = 0; I <= 2048; ++I)
				{
					V[I] = (float)std::sin(2.0 * 3.14159265358979323846 * I / 2048.0);
				}
			}
		};
		static const FTable T;
		const float X = Phase01 * 2048.f;
		const int I = (int)X;
		const float F = X - (float)I;
		const int J = I & 2047;
		return T.V[J] + (T.V[J + 1] - T.V[J]) * F;
	}

	struct FNoise
	{
		uint32_t S = 0x1234567u;
		float Next()
		{
			S = S * 1664525u + 1013904223u;
			return (float)((S >> 9) & 0x7fff) / 16383.5f - 1.f;
		}
	};

	/** A plucked double bass: one string sounding at a time (the next note damps the last), a thump, a little finger noise. */
	class FBassVoice
	{
	public:
		void Init(int InRate)
		{
			Rate = (float)InRate;
			LpK = 1.f - std::exp(-2.f * 3.14159265f * 1500.f / Rate);
			NoiseK = 1.f - std::exp(-2.f * 3.14159265f * 700.f / Rate);
			SqueakK = 1.f - std::exp(-2.f * 3.14159265f * 2600.f / Rate);
		}
		void NoteOn(int Midi, float Vel, bool bSlide)
		{
			// the old note is stopped by the finger: a quick fade under the new one
			Old = Cur;
			Old.Damp = std::exp(-1.f / (0.012f * Rate));
			Cur = FStringVoice();
			Cur.bOn = true;
			Cur.Hz = (float)LyudInstr::MidiHz(Midi);
			Cur.Bend = bSlide ? 0.94f : 0.985f;  // the finger slides up into the note
			Cur.BendK = std::exp(-1.f / ((bSlide ? 0.05f : 0.012f) * Rate));
			Cur.Amp = 0.2f + 0.8f * Vel;
			const float Ring = std::fmin(2.6f, std::fmax(1.1f, 2.4f * std::pow(55.f / Cur.Hz, 0.4f)));
			for (int K = 0; K < 5; ++K)
			{
				static const float A[5] = { 1.f, 0.8f, 0.5f, 0.3f, 0.16f };
				Cur.H[K] = A[K] * (K == 0 ? 1.f : (0.55f + 0.45f * Vel));
				Cur.HK[K] = std::exp(-1.f / (Ring / (1.f + 0.3f * (float)K * (float)K) * Rate));
			}
			Cur.Damp = 1.f;
			Thump = 0.9f * Vel;
			Squeak = bSlide ? 0.35f : ((Midi % 3) == 0 ? 0.16f : 0.f);
			Age = 0;
			Noise.S = 0x1234567u ^ (uint32_t)(Midi * 7919);  // each note its own noise: the same note is the same sound
		}
		void NoteOff()
		{
			Cur.Damp = std::exp(-1.f / (0.07f * Rate));
		}
		void Kill()
		{
			Cur = FStringVoice();
			Old = FStringVoice();
			Thump = Squeak = 0.f;
			Lp = 0.f;
		}
		void Render(float* Out, int Count1)
		{
			if (!Cur.bOn && !Old.bOn && Thump < 1e-4f)
			{
				return;
			}
			const float ThumpK = std::exp(-1.f / (0.012f * Rate));
			const float SqueakDecay = std::exp(-1.f / (0.03f * Rate));
			const float Attack = 1.f / (0.004f * Rate);
			for (int I = 0; I < Count1; ++I)
			{
				float S = One(Cur) * std::fmin(1.f, (float)Age * Attack) + One(Old);
				++Age;
				if (Thump > 1e-4f || Squeak > 1e-4f)
				{
					const float W = Noise.Next();
					NoiseLp += NoiseK * (W - NoiseLp);
					SqueakLp += SqueakK * (W - SqueakLp);
					S += NoiseLp * Thump * 1.6f + (SqueakLp - NoiseLp) * Squeak * 0.25f;
					Thump *= ThumpK;
					Squeak *= SqueakDecay;
				}
				Lp += LpK * (S - Lp);
				Out[I] += Lp * 0.5f;
			}
		}

	private:
		struct FStringVoice
		{
			bool bOn = false;
			float Hz = 55.f, Phase = 0.f, Bend = 1.f, BendK = 0.f, Amp = 0.f, Damp = 1.f, Level = 1.f;
			float H[5] = { 0.f, 0.f, 0.f, 0.f, 0.f };
			float HK[5] = { 1.f, 1.f, 1.f, 1.f, 1.f };
		};
		float One(FStringVoice& V)
		{
			if (!V.bOn)
			{
				return 0.f;
			}
			V.Bend = 1.f - (1.f - V.Bend) * V.BendK;
			V.Phase += V.Hz * V.Bend / Rate;
			if (V.Phase >= 1.f)
			{
				V.Phase -= 1.f;
			}
			float S = 0.f;
			float Ph = V.Phase;
			for (int K = 0; K < 5; ++K)
			{
				S += SinTable(Ph) * V.H[K];
				V.H[K] *= V.HK[K];
				Ph += V.Phase;
				if (Ph >= 1.f)
				{
					Ph -= 1.f;
				}
			}
			V.Level *= V.Damp;
			if (V.Level < 0.001f || V.H[0] < 0.0005f)
			{
				V.bOn = false;
			}
			return S * V.Amp * V.Level;
		}
		float Rate = 32000.f;
		FStringVoice Cur, Old;
		float Thump = 0.f, Squeak = 0.f, NoiseLp = 0.f, SqueakLp = 0.f, Lp = 0.f, LpK = 0.2f, NoiseK = 0.1f, SqueakK = 0.3f;
		int Age = 0;
		FNoise Noise;
	};

	/** The reed: a clarinet (odd harmonics, hollow) or a tenor sax (all of them, a formant, more breath). One voice: a note
	 *  begun while the last still sounds is slurred (the pitch glides, no new attack). Vibrato comes in late on long notes. */
	class FReedVoice
	{
	public:
		static constexpr int MaxH = 12;
		void Init(int InRate)
		{
			Rate = (float)InRate;
			BreathLoK = 1.f - std::exp(-2.f * 3.14159265f * 900.f / Rate);
			BreathHiK = 1.f - std::exp(-2.f * 3.14159265f * 4200.f / Rate);
		}
		void SetSax(bool bIn) { bSax = bIn; }
		bool IsSax() const { return bSax; }
		void NoteOn(int Midi, float Vel, bool bScoop)
		{
			const float Hz = (float)LyudInstr::MidiHz(Midi);
			const bool bSlur = bGate && Env > 0.2f;
			Target = Hz;
			if (!bSlur)
			{
				Cur = Hz * (bScoop ? 0.955f : 0.992f);  // a scoop: from most of a semitone under
				GlideK = std::exp(-1.f / ((bScoop ? 0.055f : 0.012f) * Rate));
				Age = 0.f;
				Chiff = 1.f;
				// a tongued note starts the same way every time (the same note is the same sound, whenever the tune was joined)
				Phase = 0.f;
				VibPhase = 0.f;
				BreathLo = BreathHi = 0.f;
				Noise.S = 0x7654321u ^ (uint32_t)(Midi * 104729);
			}
			else
			{
				GlideK = std::exp(-1.f / (0.016f * Rate));
				Age = std::fmin(Age, 0.12f * Rate);  // the vibrato starts over, the breath does not
			}
			bGate = true;
			Level = 0.25f + 0.75f * Vel;
			SetSpectrum(Hz, Vel);
		}
		void NoteOff() { bGate = false; }
		void Kill()
		{
			bGate = false;
			Env = 0.f;
		}
		bool IsSounding() const { return bGate || Env > 0.0005f; }
		void Render(float* Out, int Count)
		{
			if (!bGate && Env < 0.0005f)
			{
				Env = 0.f;
				return;
			}
			const float AttackK = 1.f - std::exp(-1.f / ((bSax ? 0.022f : 0.03f) * Rate));
			const float ReleaseK = 1.f - std::exp(-1.f / (0.045f * Rate));
			const float VibRate = (bSax ? 5.6f : 5.1f) / Rate;
			const float VibDepth = bSax ? 0.0085f : 0.0055f;  // of the frequency: 15 and 9.5 cents
			const float ChiffK = std::exp(-1.f / (0.03f * Rate));
			const float Breath = bSax ? 0.085f : 0.03f;
			for (int I = 0; I < Count; ++I)
			{
				Env += (bGate ? AttackK : ReleaseK) * ((bGate ? 1.f : 0.f) - Env);
				Cur = Target - (Target - Cur) * GlideK;
				Age += 1.f;
				// the vibrato grows from a third of a second into the note to its full depth at a second
				float Vib = (Age / Rate - 0.32f) * 1.5f;
				Vib = Vib < 0.f ? 0.f : (Vib > 1.f ? 1.f : Vib);
				VibPhase += VibRate;
				if (VibPhase >= 1.f)
				{
					VibPhase -= 1.f;
				}
				const float Wob = SinTable(VibPhase) * Vib;
				Phase += Cur * (1.f + VibDepth * Wob) / Rate;
				if (Phase >= 1.f)
				{
					Phase -= 1.f;
				}
				// the harmonics by the recurrence sin(k x) = 2 cos x sin((k-1) x) - sin((k-2) x)
				const float S1 = SinTable(Phase);
				float Cp = Phase + 0.25f;
				if (Cp >= 1.f)
				{
					Cp -= 1.f;
				}
				const float C2 = 2.f * SinTable(Cp);
				// blowing harder is brighter: each harmonic k is scaled by Bright^(k-1)
				const float Bright = Bright0 + 0.16f * Env * Level + 0.02f * Wob;
				float Prev = 0.f, This = S1, Gain = 1.f, S = 0.f;
				for (int K = 0; K < NumH; ++K)
				{
					S += This * Amp[K] * Gain;
					const float Next = C2 * This - Prev;
					Prev = This;
					This = Next;
					Gain *= Bright;
				}
				// the breath: band-passed noise with the note, a puff at its start
				const float W = Noise.Next();
				BreathLo += BreathLoK * (W - BreathLo);
				BreathHi += BreathHiK * (W - BreathHi);
				const float Air = (BreathHi - BreathLo) * (Breath + 0.3f * Chiff);
				Chiff *= ChiffK;
				Out[I] += (S * Norm + Air) * Env * Level * (1.f + 0.06f * Wob) * 0.36f;
			}
		}

	private:
		void SetSpectrum(float Hz, float Vel)
		{
			NumH = 0;
			float Sum = 0.f;
			for (int K = 1; K <= MaxH; ++K)
			{
				const float F = Hz * (float)K;
				if (F > Rate * 0.42f)
				{
					break;
				}
				float A;
				if (bSax)
				{
					// all harmonics, a broad formant about 1.1 kHz
					const float D = (F - 1100.f) / 700.f;
					A = (1.f / std::pow((float)K, 0.8f)) * (0.55f + 1.1f * std::exp(-D * D));
				}
				else
				{
					// the clarinet's closed tube: the even harmonics nearly missing low down, coming in up the instrument
					const float Even = 0.06f + 0.5f * std::fmin(1.f, std::fmax(0.f, (Hz - 350.f) / 700.f));
					A = (1.f / std::pow((float)K, 0.8f)) * ((K & 1) ? 1.f : Even);
				}
				Amp[NumH++] = A;
				Sum += A;
			}
			Norm = Sum > 0.f ? 1.4f / (0.6f + Sum) : 1.f;
			Bright0 = (bSax ? 0.76f : 0.84f) + 0.06f * Vel;
		}
		float Rate = 32000.f;
		bool bSax = false, bGate = false;
		float Target = 440.f, Cur = 440.f, GlideK = 0.f, Phase = 0.f, VibPhase = 0.f, Env = 0.f, Age = 0.f, Level = 0.5f;
		float Amp[MaxH] = { 0.f };
		int NumH = 0;
		float Norm = 1.f, Bright0 = 0.6f, Chiff = 0.f, BreathLo = 0.f, BreathHi = 0.f, BreathLoK = 0.1f, BreathHiK = 0.4f;
		FNoise Noise;
	};

	/** Brushes on a snare, a ride cymbal, the hi-hat's foot, a soft bass drum: filtered noise and a few partials. */
	class FDrumKit
	{
	public:
		void Init(int InRate)
		{
			Rate = (float)InRate;
			static const float Partials[4] = { 3140.f, 4730.f, 6270.f, 8930.f };
			for (int K = 0; K < 4; ++K)
			{
				const float W = 2.f * 3.14159265f * std::fmin(Partials[K], Rate * 0.44f) / Rate;
				RideCr[K] = std::cos(W);
				RideCi[K] = std::sin(W);
			}
		}
		/** Len: a swish's length in seconds (else unused). */
		void Hit(int Type, float Vel, float Len)
		{
			FHit* Pick = &Hits[0];
			for (FHit& H : Hits)
			{
				if (!H.bOn)
				{
					Pick = &H;
					break;
				}
				if (H.Age > Pick->Age)
				{
					Pick = &H;
				}
			}
			*Pick = FHit();
			Pick->bOn = true;
			Pick->Type = Type;
			Pick->Vel = Vel;
			Pick->Len = std::fmax(0.05f, Len) * Rate;
			if (Type == Ride || Type == Crash)
			{
				for (int K = 0; K < 4; ++K)
				{
					Pick->Re[K] = 0.f;
					Pick->Im[K] = 1.f;
				}
			}
		}
		void Kill()
		{
			for (FHit& H : Hits)
			{
				H.bOn = false;
			}
		}
		void Render(float* Out, int Count)
		{
			const float Hp1K = 1.f - std::exp(-2.f * 3.14159265f * 1300.f / Rate);
			const float Lp1K = 1.f - std::exp(-2.f * 3.14159265f * std::fmin(6500.f, Rate * 0.4f) / Rate);
			const float Hp2K = 1.f - std::exp(-2.f * 3.14159265f * std::fmin(5200.f, Rate * 0.35f) / Rate);
			for (FHit& H : Hits)
			{
				if (!H.bOn)
				{
					continue;
				}
				switch (H.Type)
				{
				case Kick:
				{
					const float AmpK = std::exp(-1.f / (0.085f * Rate));
					const float SweepK = std::exp(-1.f / (0.028f * Rate));
					if (H.Age == 0.f)
					{
						H.A = 1.f;
						H.B = 1.f;
					}
					for (int I = 0; I < Count; ++I)
					{
						H.Phase += (46.f + 52.f * H.B) / Rate;
						if (H.Phase >= 1.f)
						{
							H.Phase -= 1.f;
						}
						Out[I] += SinTable(H.Phase) * H.A * H.Vel * 0.5f;
						H.A *= AmpK;
						H.B *= SweepK;
					}
					H.Age += (float)Count;
					H.bOn = H.A > 0.001f;
					break;
				}
				case Tap:
				{
					const float AmpK = std::exp(-1.f / (0.055f * Rate));
					const float ToneK = std::exp(-1.f / (0.04f * Rate));
					if (H.Age == 0.f)
					{
						H.A = 1.f;
						H.B = 1.f;
					}
					for (int I = 0; I < Count; ++I)
					{
						const float W = Noise.Next();
						H.F1 += Hp1K * (W - H.F1);
						H.F2 += Lp1K * ((W - H.F1) - H.F2);
						H.Phase += 188.f / Rate;
						if (H.Phase >= 1.f)
						{
							H.Phase -= 1.f;
						}
						Out[I] += (H.F2 * H.A * 1.5f + SinTable(H.Phase) * H.B * 0.25f) * H.Vel;
						H.A *= AmpK;
						H.B *= ToneK;
					}
					H.Age += (float)Count;
					H.bOn = H.A > 0.001f;
					break;
				}
				case Swish:
				{
					// the brush drawn across the head: a soft swell and fall of hiss
					for (int I = 0; I < Count; ++I)
					{
						const float X = H.Age / H.Len;
						if (X >= 1.f)
						{
							H.bOn = false;
							break;
						}
						const float E = X < 0.3f ? X / 0.3f : (1.f - X) / 0.7f;
						const float W = Noise.Next();
						H.F1 += Hp1K * (W - H.F1);
						H.F2 += Lp1K * ((W - H.F1) - H.F2);
						Out[I] += H.F2 * E * E * H.Vel * 0.9f;
						H.Age += 1.f;
					}
					break;
				}
				case Hat:
				{
					const float AmpK = std::exp(-1.f / (0.022f * Rate));
					if (H.Age == 0.f)
					{
						H.A = 1.f;
					}
					for (int I = 0; I < Count; ++I)
					{
						const float W = Noise.Next();
						H.F1 += Hp2K * (W - H.F1);
						Out[I] += (W - H.F1) * H.A * H.Vel * 0.45f;
						H.A *= AmpK;
					}
					H.Age += (float)Count;
					H.bOn = H.A > 0.001f;
					break;
				}
				default:  // Ride, Crash
				{
					const bool bCrash = H.Type == Crash;
					const float WashK = std::exp(-1.f / ((bCrash ? 1.1f : 0.32f) * Rate));
					const float PingK = std::exp(-1.f / ((bCrash ? 0.9f : 0.2f) * Rate));
					const float TickK = std::exp(-1.f / (0.004f * Rate));
					if (H.Age == 0.f)
					{
						H.A = 1.f;
						H.B = 1.f;
						H.C = 1.f;
					}
					for (int I = 0; I < Count; ++I)
					{
						const float W = Noise.Next();
						H.F1 += Hp2K * (W - H.F1);
						float Ping = 0.f;
						for (int K = 0; K < 4; ++K)
						{
							const float Re = H.Re[K] * RideCr[K] - H.Im[K] * RideCi[K];
							const float Im = H.Re[K] * RideCi[K] + H.Im[K] * RideCr[K];
							H.Re[K] = Re;
							H.Im[K] = Im;
							Ping += Re;
						}
						Out[I] += ((W - H.F1) * (H.A * 0.3f + H.C * 0.5f) + Ping * H.B * 0.05f) * H.Vel * 0.8f;
						H.A *= WashK;
						H.B *= PingK;
						H.C *= TickK;
					}
					H.Age += (float)Count;
					H.bOn = H.A > 0.001f;
					break;
				}
				}
			}
		}

	private:
		struct FHit
		{
			bool bOn = false;
			int Type = 0;
			float Vel = 0.5f, Len = 1.f, Age = 0.f, A = 0.f, B = 0.f, C = 0.f, Phase = 0.f, F1 = 0.f, F2 = 0.f;
			float Re[4] = { 0.f, 0.f, 0.f, 0.f };
			float Im[4] = { 0.f, 0.f, 0.f, 0.f };
		};
		float Rate = 32000.f;
		FHit Hits[10];
		float RideCr[4] = { 1.f, 1.f, 1.f, 1.f };
		float RideCi[4] = { 0.f, 0.f, 0.f, 0.f };
		FNoise Noise;
	};

	/** The saloon: four damped combs and two allpasses (Schroeder), stereo by taking the combs with different signs. */
	class FRoom
	{
	public:
		void Init(int Rate)
		{
			static const float CombMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
			static const float ApMs[2] = { 5.0f, 1.7f };
			for (int I = 0; I < 4; ++I)
			{
				Comb[I].assign((size_t)(CombMs[I] * 0.001f * (float)Rate) | 1u, 0.f);
				CombPos[I] = 0;
				CombLp[I] = 0.f;
				// about 0.9 s to fall 60 dB
				CombFb[I] = std::pow(0.001f, CombMs[I] * 0.001f / 0.9f);
			}
			for (int I = 0; I < 2; ++I)
			{
				Ap[I].assign((size_t)(ApMs[I] * 0.001f * (float)Rate) | 1u, 0.f);
				ApPos[I] = 0;
			}
			DampK = 1.f - std::exp(-2.f * 3.14159265f * 3800.f / (float)Rate);
		}
		void Clear()
		{
			for (int I = 0; I < 4; ++I)
			{
				std::fill(Comb[I].begin(), Comb[I].end(), 0.f);
				CombLp[I] = 0.f;
			}
			for (int I = 0; I < 2; ++I)
			{
				std::fill(Ap[I].begin(), Ap[I].end(), 0.f);
			}
		}
		void Process(float In, float& L, float& R)
		{
			float C[4];
			for (int I = 0; I < 4; ++I)
			{
				std::vector<float>& D = Comb[I];
				const float Y = D[CombPos[I]];
				CombLp[I] += DampK * (Y - CombLp[I]);
				D[CombPos[I]] = In + CombLp[I] * CombFb[I];
				CombPos[I] = CombPos[I] + 1 < D.size() ? CombPos[I] + 1 : 0;
				C[I] = Y;
			}
			float Lx = C[0] + C[1] - C[2] + C[3];
			float Rx = C[0] - C[1] + C[2] + C[3];
			// the allpasses smear both alike (one on each side keeps it cheap)
			Lx = Allpass(0, Lx);
			Rx = Allpass(1, Rx);
			L = Lx * 0.25f;
			R = Rx * 0.25f;
		}

	private:
		float Allpass(int I, float X)
		{
			std::vector<float>& D = Ap[I];
			const float Z = D[ApPos[I]];
			const float Y = Z - 0.5f * X;
			D[ApPos[I]] = X + 0.5f * Y;
			ApPos[I] = ApPos[I] + 1 < D.size() ? ApPos[I] + 1 : 0;
			return Y;
		}
		std::vector<float> Comb[4], Ap[2];
		size_t CombPos[4] = { 0, 0, 0, 0 }, ApPos[2] = { 0, 0 };
		float CombLp[4] = { 0.f, 0.f, 0.f, 0.f }, CombFb[4] = { 0.f, 0.f, 0.f, 0.f };
		float DampK = 0.3f;
	};

	// -------------------------------------------------------------------------------------------------- the performance

	struct FEvent
	{
		double Time = 0.0;      // seconds from the tune's start
		uint32_t Seq = 0;
		uint32_t Id = 0;        // a note's on and off share it
		int8_t Inst = 0;
		int8_t Type = 0;        // 1 on, 0 off; the drums: 1
		int16_t Midi = 0;       // the drums: EDrum
		float Vel = 0.f;
		float Len = 0.f;        // seconds (the drums' swish)
		uint8_t Flags = 0;      // 1: scoop / slide
	};

	struct FStats
	{
		double Peak = 0.0, SumSq = 0.0;
		long long Frames = 0;
		int MaxPianoVoices = 0, PianoOverflow = 0, RangeErrors = 0, QueueOverflow = 0, Notes[NumInst] = { 0, 0, 0, 0 };
	};

	/** Plays one tune: the arranger writes each bar's notes a bar ahead, the four instruments sound them. */
	class FPlayer
	{
	public:
		explicit FPlayer(int SampleRate = 32000) : Rate(SampleRate > 8000 ? SampleRate : 8000), PianoSynth(Rate)
		{
			PianoSynth.SetInstrument(LyudInstr::EInstrument::Piano);
			PianoSynth.Gain = 1.f;
			BassVoice.Init(Rate);
			ReedVoice.Init(Rate);
			Kit.Init(Rate);
			Room.Init(Rate);
			Queue.reserve(QueueMax);
			Bars.reserve(1024);
			for (int I = 0; I < NumInst; ++I)
			{
				Solo[I] = true;
			}
		}

		int SampleRate() const { return Rate; }
		/** Which instruments sound (all by default): for the offline stems. */
		bool Solo[NumInst];
		/** Dry (no room), for measuring. */
		bool bDry = false;
		/** If set, every event sounded is appended (offline checks only: it allocates). */
		std::vector<FEvent>* Log = nullptr;
		FStats Stats;

		/** Begins Tune at AtSeconds into its performance (0: the top). Soft: the dinner trio. The tune must outlive the playing. */
		void Start(const FTune& InTune, uint32_t InSeed, bool bInSoft, double AtSeconds = 0.0)
		{
			Stop(true);
			// every performance from the same state: the same seed is the same sound, sample for sample
			BassVoice = FBassVoice();
			BassVoice.Init(Rate);
			ReedVoice = FReedVoice();
			ReedVoice.Init(Rate);
			Kit = FDrumKit();
			Kit.Init(Rate);
			SeqCounter = IdCounter = BassId = ReedId = 0;
			for (uint32_t& G : PianoGen)
			{
				G = 0;
			}
			Tune = &InTune;
			Seed = InSeed ? InSeed : 1u;
			bSoft = bInSoft;
			BuildBars(InTune, bInSoft, Bars, Length);
			ReedVoice.SetSax(InTune.bSax);
			ReedLo = InTune.bSax ? SaxLo : ClarLo;
			ReedHi = InTune.bSax ? SaxHi : ClarHi;
			ReedShift = FitShift(ReedLo + 3, ReedHi - 5);
			PianoShift = FitShift(62, 91);
			Frame = (long long)(std::fmax(0.0, AtSeconds) * Rate);
			Floor = (double)Frame / Rate;
			NextBar = 0;
			while (NextBar + 1 < (int)Bars.size() && Bars[(size_t)NextBar + 1].Start <= Floor)
			{
				++NextBar;
			}
			FadeIn = AtSeconds > 0.01 ? 0.f : 1.f;
			bPlaying = Floor < Length;
		}

		void Stop(bool bNow)
		{
			if (bNow)
			{
				PianoSynth.AllOff(true);
				BassVoice.Kill();
				ReedVoice.Kill();
				Kit.Kill();
				Room.Clear();
				PianoLp = 0.f;
				Queue.clear();
				bPlaying = false;
				Tune = nullptr;
				FadeOut = 1.f;
				bFading = false;
			}
			else if (bPlaying)
			{
				bFading = true;
			}
		}

		bool IsPlaying() const { return bPlaying; }
		const FTune* Playing() const { return bPlaying ? Tune : nullptr; }
		double Position() const { return (double)Frame / Rate; }
		double Duration() const { return Length; }
		const std::vector<FBar>& BarPlan() const { return Bars; }

		/** Writes Frames stereo frames (L R L R ...), silence when nothing plays. */
		void Render(float* Out, int Frames)
		{
			int Done = 0;
			while (Done < Frames)
			{
				if (!bPlaying)
				{
					for (int I = Done * 2; I < Frames * 2; ++I)
					{
						Out[I] = 0.f;
					}
					return;
				}
				// the arranger keeps a bar ahead of the sound
				while (NextBar < (int)Bars.size() && (NextBar == 0 || Bars[(size_t)NextBar - 1].Start <= Position() + 0.05))
				{
					WriteBar(NextBar);
					++NextBar;
				}
				while (!Queue.empty() && Queue.front().Time * Rate <= (double)Frame)
				{
					std::pop_heap(Queue.begin(), Queue.end(), Later);
					const FEvent E = Queue.back();
					Queue.pop_back();
					Fire(E);
				}
				int N = Frames - Done;
				if (N > Block)
				{
					N = Block;
				}
				if (!Queue.empty())
				{
					const long long To = (long long)std::ceil(Queue.front().Time * Rate) - Frame;
					if (To < N)
					{
						N = To < 1 ? 1 : (int)To;
					}
				}
				RenderBlock(Out + Done * 2, N);
				Done += N;
				Frame += N;
				if (Position() >= Length || (bFading && FadeOut <= 0.f))
				{
					Stop(true);
				}
			}
		}

	private:
		static constexpr int Block = 256;
		static constexpr size_t QueueMax = 4096;

		static bool Later(const FEvent& A, const FEvent& B)
		{
			return A.Time > B.Time || (A.Time == B.Time && A.Seq > B.Seq);
		}

		int Rate;
		const FTune* Tune = nullptr;
		uint32_t Seed = 1;
		bool bSoft = false, bPlaying = false, bFading = false;
		std::vector<FBar> Bars;
		double Length = 0.0, Floor = 0.0;
		long long Frame = 0;
		int NextBar = 0;
		float FadeIn = 1.f, FadeOut = 1.f;
		int ReedLo = ClarLo, ReedHi = ClarHi, ReedShift = 0, PianoShift = 0;

		LyudInstr::FSynth PianoSynth;
		FBassVoice BassVoice;
		FReedVoice ReedVoice;
		FDrumKit Kit;
		FRoom Room;
		std::vector<FEvent> Queue;
		uint32_t SeqCounter = 0, IdCounter = 0;
		uint32_t PianoGen[128] = { 0 };
		uint32_t BassId = 0, ReedId = 0;
		float BufP[Block], BufB[Block], BufR[Block], BufD[Block];
		float PianoLp = 0.f;

		// --- writing state of the bar being arranged
		int WBar = 0;
		uint32_t WCount = 0;

		/** The octave shift that puts the melody best inside Lo..Hi. */
		int FitShift(int Lo, int Hi) const
		{
			int Best = 0, BestBad = 1 << 30;
			static const int Tries[5] = { 0, -12, 12, -24, 24 };
			for (const int S : Tries)
			{
				int Bad = 0;
				for (const FNote& N : Tune->Notes)
				{
					Bad += (N.Midi + S < Lo ? Lo - (N.Midi + S) : 0) + (N.Midi + S > Hi ? (N.Midi + S) - Hi : 0);
				}
				if (Bad < BestBad)
				{
					BestBad = Bad;
					Best = S;
				}
			}
			return Best;
		}

		static int Fold(int Midi, int Lo, int Hi)
		{
			while (Midi < Lo)
			{
				Midi += 12;
			}
			while (Midi > Hi)
			{
				Midi -= 12;
			}
			return Midi;
		}

		// ---------------------------------------------------------------------------------------------- sounding

		void Fire(const FEvent& E)
		{
			if (Log)
			{
				Log->push_back(E);
			}
			if (!Solo[E.Inst])
			{
				return;
			}
			switch (E.Inst)
			{
			case Piano:
				if (E.Type == 1)
				{
					const int Active = PianoSynth.ActiveVoices();
					Stats.MaxPianoVoices = std::max(Stats.MaxPianoVoices, Active + 1);
					Stats.PianoOverflow += Active >= 14 ? 1 : 0;
					PianoGen[E.Midi & 127] = E.Id;
					PianoSynth.NoteOn(E.Midi, E.Vel);
				}
				else if (PianoGen[E.Midi & 127] == E.Id)
				{
					PianoSynth.NoteOff(E.Midi);
				}
				break;
			case Bass:
				if (E.Type == 1)
				{
					BassId = E.Id;
					BassVoice.NoteOn(E.Midi, E.Vel, (E.Flags & 1) != 0);
				}
				else if (BassId == E.Id)
				{
					BassVoice.NoteOff();
				}
				break;
			case Reed:
				if (E.Type == 1)
				{
					ReedId = E.Id;
					ReedVoice.NoteOn(E.Midi, E.Vel, (E.Flags & 1) != 0);
				}
				else if (ReedId == E.Id)
				{
					ReedVoice.NoteOff();
				}
				break;
			default:
				Kit.Hit(E.Midi, E.Vel, E.Len);
				break;
			}
		}

		void RenderBlock(float* Out, int N)
		{
			PianoSynth.Render(BufP, N);
			for (int I = 0; I < N; ++I)
			{
				BufB[I] = 0.f;
				BufR[I] = 0.f;
				BufD[I] = 0.f;
			}
			BassVoice.Render(BufB, N);
			ReedVoice.Render(BufR, N);
			Kit.Render(BufD, N);
			// where they stand on the bandstand: the piano to the left, the drums to the right, bass and reed near the middle
			const float Master = bSoft ? 0.8f : 1.f;
			const float FadeStep = 1.f / (0.25f * (float)Rate);
			const float PianoLpK = 1.f - std::exp(-2.f * 3.14159265f * 1100.f / (float)Rate);
			for (int I = 0; I < N; ++I)
			{
				// the piano's lid opened a little: its top lifted (the instrument's own voice is a mellow upright's)
				PianoLp += PianoLpK * (BufP[I] - PianoLp);
				const float P = (BufP[I] + 1.6f * (BufP[I] - PianoLp)) * 0.95f, B = BufB[I] * 0.5f, R = BufR[I] * 1.9f, D = BufD[I] * 0.7f;
				float L = P * 0.80f + B * 0.66f + R * 0.62f + D * 0.48f;
				float Rt = P * 0.50f + B * 0.74f + R * 0.74f + D * 0.84f;
				if (!bDry)
				{
					float Wl = 0.f, Wr = 0.f;
					Room.Process(P * 0.2f + R * 0.24f + D * 0.2f + B * 0.04f, Wl, Wr);
					L += Wl;
					Rt += Wr;
				}
				if (FadeIn < 1.f)
				{
					FadeIn = std::fmin(1.f, FadeIn + FadeStep);
				}
				if (bFading)
				{
					FadeOut = std::fmax(0.f, FadeOut - FadeStep);
				}
				const float G = Master * FadeIn * FadeOut;
				L = Limit(L * G);
				Rt = Limit(Rt * G);
				Out[2 * I] = L;
				Out[2 * I + 1] = Rt;
				const double A = std::fmax(std::fabs((double)L), std::fabs((double)Rt));
				Stats.Peak = A > Stats.Peak ? A : Stats.Peak;
				Stats.SumSq += 0.5 * ((double)L * L + (double)Rt * Rt);
			}
			Stats.Frames += N;
		}

		/** Straight up to 0.7, then bending over so that nothing passes 0.97. */
		static float Limit(float X)
		{
			const float A = std::fabs(X);
			if (A <= 0.7f)
			{
				return X;
			}
			const float Y = 0.7f + 0.27f * std::tanh((A - 0.7f) / 0.27f);
			return X < 0.f ? -Y : Y;
		}

		// ---------------------------------------------------------------------------------------------- arranging

		double BarTime(const FBar& B, double Beat) const
		{
			const double M = Tune->Meter;
			if (Beat <= 0.0)
			{
				return B.Start + B.Spb0 * Beat;
			}
			if (Beat >= M)
			{
				return B.Start + M * 0.5 * (B.Spb0 + B.Spb1) + B.Spb1 * (Beat - M);
			}
			return B.Start + B.Spb0 * Beat + (B.Spb1 - B.Spb0) * Beat * Beat / (2.0 * M);
		}

		/** The off-beat eighth falls late (the swing). */
		double Sw(double Beat) const
		{
			const double F = std::floor(Beat + 1e-6);
			const double Frac = Beat - F;
			if (std::fabs(Frac - 0.5) < 0.02)
			{
				return F + Tune->Swing;
			}
			return Beat;
		}

		void Push(const FEvent& E)
		{
			if (Queue.size() >= QueueMax)
			{
				++Stats.QueueOverflow;
				return;
			}
			Queue.push_back(E);
			std::push_heap(Queue.begin(), Queue.end(), Later);
		}

		/** A note of bar WBar: Beat and Len in beats (already swung by the caller). Lay: seconds behind the beat. */
		void Note(int Inst, int Midi, float Vel, double Beat, double Len, bool bScoop = false, double Lay = 0.0)
		{
			const FBar& B = Bars[(size_t)WBar];
			const uint32_t H = Hash(Seed, (uint32_t)WBar, (uint32_t)Inst + 16u, WCount++);
			const double Jitter = ((double)(H & 0xffff) / 65535.0 - 0.5) * (Inst == Drums ? 0.008 : 0.014);
			const float VelJ = 1.f + ((float)((H >> 16) & 0xff) / 255.f - 0.5f) * 0.16f;
			const double On = BarTime(B, Beat) + Lay + Jitter;
			double Off = BarTime(B, Beat + Len) + Lay;
			if (Off < On + 0.04)
			{
				Off = On + 0.04;
			}
			if (On < Floor)
			{
				return;  // joined mid-tune: what began before is not played
			}
			const int Lo = Inst == Bass ? BassLo : Inst == Reed ? ReedLo : PianoLo;
			const int Hi = Inst == Bass ? BassHi : Inst == Reed ? ReedHi : PianoHi;
			if (Inst != Drums && (Midi < Lo || Midi > Hi))
			{
				++Stats.RangeErrors;
				Midi = Fold(Midi, Lo, Hi);
			}
			FEvent E;
			E.Time = On;
			E.Seq = ++SeqCounter;
			E.Id = ++IdCounter;
			E.Inst = (int8_t)Inst;
			E.Type = 1;
			E.Midi = (int16_t)Midi;
			const float Dyn = (bSoft ? 0.8f : 1.f) * (Inst == Piano ? 1.3f : 1.f);  // the piano struck a little harder: brighter
			E.Vel = std::fmin(1.f, std::fmax(0.05f, Vel * VelJ * Dyn));
			E.Len = (float)(Off - On);
			E.Flags = bScoop ? 1 : 0;
			Push(E);
			++Stats.Notes[Inst];
			if (Inst != Drums)
			{
				E.Time = Off;
				E.Seq = ++SeqCounter;
				E.Type = 0;
				Push(E);
			}
		}

		struct FSeg
		{
			double Beat = 0.0;
			const FChordAt* Chord = nullptr;
		};

		/** The chords of a bar of the form (up to 4 changes), in order. */
		int BarChords(int FormBar, FSeg* Out) const
		{
			const double M = Tune->Meter;
			const double From = FormBar * M;
			int N = 0;
			Out[N].Beat = 0.0;
			Out[N].Chord = &Tune->ChordAtBeat(From);
			++N;
			for (const FChordAt& C : Tune->Chords)
			{
				if (C.Beat > From + 1e-6 && C.Beat < From + M - 1e-6 && N < 4)
				{
					Out[N].Beat = C.Beat - From;
					Out[N].Chord = &C;
					++N;
				}
			}
			return N;
		}

		static const FChordAt& SegAt(const FSeg* Segs, int N, double Beat)
		{
			int Pick = 0;
			for (int I = 1; I < N; ++I)
			{
				if (Segs[I].Beat <= Beat + 1e-6)
				{
					Pick = I;
				}
			}
			return *Segs[Pick].Chord;
		}

		/** The tonic chord the tune ends on. */
		FChordAt TonicChord() const
		{
			FChordAt C;
			C.Root = Tune->KeyRoot;
			const bool bPlain = Tune->Feel == EFeel::Waltz || Tune->Feel == EFeel::Tango || Tune->Feel == EFeel::Ragtime;
			C.KindIndex = Tune->bMinor ? (bPlain ? KindMinor : KindMin6) : (bPlain ? KindMajor : KindMaj6);
			return C;
		}

		/** The first chord of the bar after Index (the tonic after the last). */
		FChordAt NextChord(int Index) const
		{
			if (Index + 1 >= (int)Bars.size() || Bars[(size_t)Index + 1].Role == ERole::Final)
			{
				return TonicChord();
			}
			return Tune->ChordAtBeat((double)Bars[(size_t)Index + 1].FormBar * Tune->Meter);
		}

		unsigned ScaleMask(const FChordAt& C) const
		{
			// a dominant in a minor key leans on the minor's own notes
			int K = C.KindIndex;
			if (Tune->bMinor && K == KindDom7 && C.Root == (Tune->KeyRoot + 7) % 12)
			{
				K = KindDom7b9;
			}
			return Mask(Kind(K).Scale, 8, C.Root);
		}
		static unsigned ToneMask(const FChordAt& C) { return Mask(Kind(C.KindIndex).Tones, 4, C.Root); }

		/** The note of Mask nearest Midi (the lower of two as near). */
		static int Nearest(unsigned M, int Midi)
		{
			for (int D = 0; D < 7; ++D)
			{
				if (M & (1u << (((Midi - D) % 12 + 12) % 12)))
				{
					return Midi - D;
				}
				if (M & (1u << (((Midi + D) % 12 + 12) % 12)))
				{
					return Midi + D;
				}
			}
			return Midi;
		}

		/** Steps up (+) or down (-) the notes of Mask from the one nearest Midi. */
		static int Step(unsigned M, int Midi, int Steps)
		{
			int At = Nearest(M, Midi);
			const int Dir = Steps < 0 ? -1 : 1;
			for (int I = 0; I < (Steps < 0 ? -Steps : Steps); ++I)
			{
				for (int D = 1; D <= 12; ++D)
				{
					if (M & (1u << (((At + Dir * D) % 12 + 12) % 12)))
					{
						At += Dir * D;
						break;
					}
				}
			}
			return At;
		}

		/** The bass's root: the chord's root between E1 and Eb2. */
		static int BassRoot(const FChordAt& C)
		{
			int M = 24 + C.Root;
			while (M < BassLo)
			{
				M += 12;
			}
			return M;
		}

		float RoleLevel(const FBar& B) const
		{
			switch (B.Role)
			{
			case ERole::Intro: return 0.78f;
			case ERole::Head: case ERole::PianoHead: return 0.85f;
			case ERole::Out: case ERole::Tag: case ERole::Final: return 1.f;
			default: return 0.93f;
			}
		}

		bool IsJazz() const { return Tune->Feel == EFeel::Swing || Tune->Feel == EFeel::Foxtrot || Tune->Feel == EFeel::Ballad; }

		/** In trading fours: this bar is the drummer's alone. */
		static bool DrumsAlone(const FBar& B) { return B.Role == ERole::Fours && ((B.FormBar / 4) % 2) == 1; }

		void WriteBar(int Index)
		{
			WBar = Index;
			WCount = 0;
			const FBar& B = Bars[(size_t)Index];
			if (B.Role == ERole::Final)
			{
				WriteFinal();
				return;
			}
			FSeg Segs[4];
			const int NumSegs = BarChords(B.FormBar, Segs);
			const FChordAt Next = NextChord(Index);
			const bool bAlone = DrumsAlone(B);
			if (!bAlone)
			{
				WriteBass(B, Segs, NumSegs, Next);
				WriteComp(B, Segs, NumSegs, Next);
			}
			WriteDrums(B, bAlone);
			// who has the tune
			switch (B.Role)
			{
			case ERole::Intro:
				if (IsJazz() || Tune->Feel == EFeel::Ragtime)
				{
					WriteMelody(B, Piano, 0.6f, true);
				}
				// the pick-up into the head
				if (B.bLastOfRole)
				{
					const bool bPianoLeads = Index + 1 < (int)Bars.size() && Bars[(size_t)Index + 1].Role == ERole::PianoHead;
					WritePickup(bPianoLeads ? Piano : Reed);
				}
				break;
			case ERole::Head:
				WriteMelody(B, Reed, 0.7f, false);
				if (Tune->Feel == EFeel::Ragtime && (B.Chorus % 2) == 0)
				{
					WriteMelody(B, Piano, 0.55f, false);
				}
				WriteFill(B, Segs, NumSegs);
				break;
			case ERole::PianoHead:
				WriteMelody(B, Piano, 0.72f, false);
				break;
			case ERole::Out:
			case ERole::Tag:
				WriteMelody(B, Reed, bSoft ? 0.6f : 0.8f, false);
				if (Tune->Feel == EFeel::Ragtime || Tune->Feel == EFeel::Tango)
				{
					WriteMelody(B, Piano, 0.6f, false);
				}
				else
				{
					WriteFill(B, Segs, NumSegs);
				}
				break;
			case ERole::Solo:
				WriteSolo(B, Segs, NumSegs, Reed);
				break;
			case ERole::PianoSolo:
				WriteSolo(B, Segs, NumSegs, Piano);
				break;
			case ERole::Fours:
				if (!bAlone)
				{
					WriteSolo(B, Segs, NumSegs, ((B.FormBar / 8) % 2) == 0 ? Reed : Piano);
				}
				break;
			default:
				break;
			}
			// the pick-up into a later head (the chorus before leaves room: its last bar is a held note)
			if (B.bLastOfRole && B.Role != ERole::Intro && Index + 1 < (int)Bars.size())
			{
				const ERole R = Bars[(size_t)Index + 1].Role;
				const bool bHeadNow = B.Role == ERole::Head || B.Role == ERole::PianoHead;
				if ((R == ERole::Out || R == ERole::Head || R == ERole::PianoHead) && (!bHeadNow || Tune->bPickupFree))
				{
					WritePickup(R == ERole::PianoHead ? Piano : Reed);
				}
			}
		}

		// --- the bass

		void WriteBass(const FBar& B, const FSeg* Segs, int NumSegs, const FChordAt& Next)
		{
			const int M = Tune->Meter;
			FRng Rng(Hash(Seed, (uint32_t)WBar, 101u));
			const float Lv = RoleLevel(B);
			const EFeel Feel = Tune->Feel;
			const FChordAt& C0 = *Segs[0].Chord;
			const int Root = BassRoot(C0);
			const int NextRoot = BassRoot(Next);
			const int Third = Root + Kind(C0.KindIndex).Tones[1];
			const int FifthIv = Kind(C0.KindIndex).Tones[2];
			const int Fifth = Root + FifthIv;
			const bool bSolos = B.Role == ERole::Solo || B.Role == ERole::PianoSolo || B.Role == ERole::Fours;
			const bool bWalk = Feel == EFeel::Swing || (Feel == EFeel::Foxtrot && bSolos);
			auto Approach = [&Rng](int Target, int From)
			{
				// a half step from under or over, or the fifth below: whichever is not the note just played
				int A = Rng.P(0.6f) ? Target - 1 : Target + 1;
				if (Rng.P(0.2f))
				{
					A = Target - 5;
				}
				A = A < BassLo ? A + 12 : A;
				if (A == From)
				{
					// not the same note twice: from the other side
					A = Target + (From < Target ? 1 : -1);
					A = A < BassLo ? Target + 1 : A;
				}
				return A;
			};
			if (bWalk && M == 4)
			{
				int Line[4];
				const int Shape = Rng.R(5);
				int Prev = Root;
				for (int K = 0; K < 4; ++K)
				{
					const FChordAt& C = SegAt(Segs, NumSegs, (double)K);
					const bool bStarts = K == 0 || &C != &SegAt(Segs, NumSegs, (double)K - 1);
					const bool bEnds = K == 3 || &C != &SegAt(Segs, NumSegs, (double)K + 1);
					const int R = BassRoot(C);
					int N;
					if (bStarts)
					{
						N = R;
					}
					else if (bEnds)
					{
						N = Approach(K == 3 ? NextRoot : BassRoot(SegAt(Segs, NumSegs, (double)K + 1)), Prev);
					}
					else
					{
						const int T3 = R + Kind(C.KindIndex).Tones[1];
						const int T5 = R + Kind(C.KindIndex).Tones[2];
						const int Up = R + 12 <= 50 ? R + 12 : T5;
						const unsigned Sm = ScaleMask(C);
						switch (Shape)
						{
						case 0: N = K == 1 ? T3 : T5; break;
						case 1: N = K == 1 ? T5 : T3; break;
						case 2: N = K == 1 ? Step(Sm, R, 1) : Step(Sm, R, 2); break;
						case 3: N = K == 1 ? T5 : Up; break;
						default: N = K == 1 ? Step(Sm, R + 12, -1) : T5; break;
						}
					}
					Line[K] = N;
					Prev = N;
				}
				for (int K = 0; K < 4; ++K)
				{
					Note(Bass, Line[K], (K == 0 ? 0.74f : (K == 2 ? 0.68f : 0.62f)) * Lv, (double)K, 0.92, false, -0.004);
				}
				// now and then a skipped eighth into the next beat
				if (Rng.P(0.18f))
				{
					const int K = 1 + Rng.R(3);
					Note(Bass, Line[K], 0.4f * Lv, Sw((double)K - 0.5), 0.3, false, -0.004);
				}
				return;
			}
			if (Feel == EFeel::Waltz || M == 3)
			{
				// one to the bar: root, the fifth the next bar of the same chord, sometimes a step up into the next
				const bool bSame = B.FormBar > 0 && &Tune->ChordAtBeat((double)(B.FormBar - 1) * M) == &C0;
				const int N = bSame && (B.FormBar % 2) == 1 ? (Fifth - 12 >= BassLo ? Fifth - 12 : Fifth) : Root;
				const bool bLead = Next.Root != C0.Root && Rng.P(0.45f);
				Note(Bass, N, 0.74f * Lv, 0.0, bLead ? 1.9 : 2.7);
				if (bLead)
				{
					Note(Bass, Approach(NextRoot, N), 0.5f * Lv, (double)M - 1.0, 0.9);
				}
				return;
			}
			if (Feel == EFeel::Tango)
			{
				// the habanera: long, short, two even
				const FChordAt& C2 = SegAt(Segs, NumSegs, 2.0);
				const int R2 = BassRoot(C2);
				const int F2 = R2 + Kind(C2.KindIndex).Tones[2];
				Note(Bass, Root, 0.8f * Lv, 0.0, 1.35);
				Note(Bass, Fifth, 0.55f * Lv, 1.5, 0.45);
				Note(Bass, &C2 != &C0 ? R2 : (Root + 12 <= 48 ? Root + 12 : Fifth), 0.7f * Lv, 2.0, 0.85);
				Note(Bass, Next.Root != C2.Root ? Approach(NextRoot, R2) : F2, 0.62f * Lv, 3.0, 0.8);
				return;
			}
			// two to the bar (the ballad, the foxtrot's head, ragtime, the intro of anything in four)
			{
				const FChordAt& C2 = SegAt(Segs, NumSegs, 2.0);
				const int R2 = BassRoot(C2);
				const bool bShort = Feel == EFeel::Ragtime;
				int Second = &C2 != &C0 ? R2 : (Fifth - 12 >= BassLo && Rng.P(0.5f) ? Fifth - 12 : Fifth);
				if (&C2 == &C0 && Rng.P(0.2f))
				{
					Second = Third;
				}
				Note(Bass, Root, 0.76f * Lv, 0.0, bShort ? 0.9 : 1.8, !bShort && Rng.P(0.12f));
				Note(Bass, Second, 0.66f * Lv, 2.0, bShort ? 0.9 : (Next.Root != C2.Root && Rng.P(0.5f) ? 1.0 : 1.8));
				if (!bShort && Next.Root != C2.Root && Rng.P(0.4f))
				{
					Note(Bass, Approach(NextRoot, Second), 0.5f * Lv, Feel == EFeel::Ballad ? 3.0 : Sw(3.5), 0.5);
				}
			}
		}

		// --- the piano's left hand (and the stride of the waltz, the tango, the rag)

		/** The comping voicing of a chord: three notes about middle C, the root left to the bass. Returns how many. */
		int Voicing(const FChordAt& C, int* Out, bool bPlain) const
		{
			const FKind& K = Kind(C.KindIndex);
			int N = 0;
			for (int I = 0; I < 3; ++I)
			{
				const int Iv = bPlain ? K.Tones[I == 0 ? 1 : (I == 1 ? 2 : (K.Tones[3] >= 0 ? 3 : 0))] : K.Voice[I];
				Out[N++] = Fold(48 + (C.Root + Iv) % 12, bPlain ? 55 : 52, bPlain ? 66 : 63);
			}
			std::sort(Out, Out + N);
			return N;
		}

		void Chord(const FChordAt& C, float Vel, double Beat, double Len, bool bPlain, double Roll = 0.0)
		{
			int V[3];
			const int N = Voicing(C, V, bPlain);
			for (int I = 0; I < N; ++I)
			{
				Note(Piano, V[I], Vel * (I == N - 1 ? 1.f : 0.88f), Beat + Roll * I, Len - Roll * I);
			}
		}

		void WriteComp(const FBar& B, const FSeg* Segs, int NumSegs, const FChordAt& Next)
		{
			const int M = Tune->Meter;
			FRng Rng(Hash(Seed, (uint32_t)WBar, 202u));
			const EFeel Feel = Tune->Feel;
			const bool bPianoLeads = B.Role == ERole::PianoHead || B.Role == ERole::PianoSolo ||
				(B.Role == ERole::Fours && ((B.FormBar / 8) % 2) == 1);
			const float Lv = RoleLevel(B) * (bPianoLeads ? 0.85f : 1.f);
			const FChordAt& C0 = *Segs[0].Chord;
			if (Feel == EFeel::Waltz || M == 3)
			{
				// oom-pah-pah: the left hand's low note with the bass, the chord on two and three
				Note(Piano, Fold(36 + C0.Root, 38, 49), 0.42f * Lv, 0.0, 0.9);
				Chord(SegAt(Segs, NumSegs, 1.0), 0.36f * Lv, 1.0, 0.5, true);
				Chord(SegAt(Segs, NumSegs, 2.0), 0.33f * Lv, 2.0, 0.5, true);
				return;
			}
			if (Feel == EFeel::Tango)
			{
				// marcato: four dry chords, the first the heaviest; now and then the síncopa
				const bool bSync = Rng.P(0.3f);
				Note(Piano, Fold(36 + C0.Root, 38, 49), 0.5f * Lv, 0.0, 0.5);
				Chord(C0, 0.5f * Lv, 0.0, 0.3, true);
				Chord(SegAt(Segs, NumSegs, 1.0), 0.36f * Lv, bSync ? 1.5 : 1.0, 0.3, true);
				Chord(SegAt(Segs, NumSegs, 2.0), 0.42f * Lv, 2.0, 0.3, true);
				Chord(SegAt(Segs, NumSegs, 3.0), 0.38f * Lv, 3.0, 0.3, true);
				if (B.bLastOfRole || Rng.P(0.25f))
				{
					Chord(SegAt(Segs, NumSegs, 3.0), 0.3f * Lv, 3.5, 0.2, true);
				}
				return;
			}
			if (Feel == EFeel::Ragtime)
			{
				// stride: low note, chord, low fifth, chord
				const FChordAt& C2 = SegAt(Segs, NumSegs, 2.0);
				const int Low = Fold(36 + C0.Root, 36, 47);
				const int Low2 = &C2 != &C0 ? Fold(36 + C2.Root, 36, 47) : Fold(Low + Kind(C0.KindIndex).Tones[2], 36, 47);
				Note(Piano, Low, 0.5f * Lv, 0.0, 0.6);
				Chord(SegAt(Segs, NumSegs, 1.0), 0.38f * Lv, 1.0, 0.45, true);
				Note(Piano, Low2, 0.44f * Lv, 2.0, 0.6);
				Chord(SegAt(Segs, NumSegs, 3.0), 0.38f * Lv, 3.0, 0.45, true);
				return;
			}
			if (Feel == EFeel::Ballad)
			{
				// spread chords, let ring; a second touch late in the bar
				Chord(C0, 0.4f * Lv, 0.0, NumSegs > 1 ? Segs[1].Beat - 0.1 : 2.6, false, 0.06);
				if (NumSegs > 1)
				{
					Chord(*Segs[1].Chord, 0.38f * Lv, Segs[1].Beat, (double)M - Segs[1].Beat - 0.2, false, 0.05);
				}
				else if (Rng.P(0.5f))
				{
					Chord(C0, 0.3f * Lv, Sw(2.5), 0.8, false);
				}
				return;
			}
			// swing and foxtrot: a few rhythms, chosen bar by bar; "3.5" is the push into the next bar's chord
			struct FHitAt { double Beat; double Len; };
			static const FHitAt One[7][3] = {
				{ { 0.0, 1.0 }, { 1.5, 0.35 }, { -1, 0 } },
				{ { 1.5, 1.0 }, { 3.0, 0.35 }, { -1, 0 } },
				{ { 1.0, 0.35 }, { 2.5, 1.0 }, { -1, 0 } },
				{ { 0.0, 0.35 }, { 2.5, 0.35 }, { 3.5, 1.0 } },
				{ { 1.0, 0.35 }, { 3.0, 0.35 }, { -1, 0 } },
				{ { 0.0, 1.6 }, { -1, 0 }, { -1, 0 } },
				{ { 0.5, 0.35 }, { 2.0, 1.0 }, { 3.5, 0.8 } },
			};
			static const FHitAt Two[3][3] = {
				{ { 0.0, 1.0 }, { 2.0, 1.0 }, { -1, 0 } },
				{ { 0.0, 0.35 }, { 1.5, 0.35 }, { 2.0, 1.0 } },
				{ { 0.0, 1.0 }, { 2.5, 0.35 }, { 3.5, 0.8 } },
			};
			const FHitAt* Pat = NumSegs > 1 ? Two[Rng.R(3)] : One[Rng.R(bPianoLeads ? 5 : 7)];
			for (int I = 0; I < 3; ++I)
			{
				if (Pat[I].Beat < 0.0 || (bPianoLeads && I == 2))
				{
					continue;
				}
				const bool bPush = Pat[I].Beat >= 3.4;
				const FChordAt& C = bPush ? Next : SegAt(Segs, NumSegs, Pat[I].Beat);
				Chord(C, (0.42f + 0.08f * Rng.U()) * Lv, Sw(Pat[I].Beat), Pat[I].Len, false);
			}
		}

		// --- the drums

		void WriteDrums(const FBar& B, bool bAlone)
		{
			const int M = Tune->Meter;
			FRng Rng(Hash(Seed, (uint32_t)WBar, 303u));
			const float Lv = RoleLevel(B) * (bSoft ? 0.8f : 1.f);
			const EFeel Feel = Tune->Feel;
			const bool bSolos = B.Role == ERole::Solo || B.Role == ERole::PianoSolo || B.Role == ERole::Fours;
			auto Hit1 = [this](int Type, float Vel, double Beat, double LenBeats)
			{
				Note(Drums, Type, Vel, Beat, LenBeats);
			};
			if (bAlone)
			{
				// the drummer's four bars: taps in swung eighths by a motif, the bass drum under, a crash to hand back
				const int Motif = (int)(Hash(Seed, (uint32_t)(WBar - B.FormBar % 4), 304u) % 4u);
				static const double Pats[4][8] = {
					{ 0, 0.5, 1, 2, 2.5, 3, -1, -1 }, { 0, 1, 1.5, 2, 3, 3.5, -1, -1 },
					{ 0.5, 1, 1.5, 2.5, 3, 3.5, -1, -1 }, { 0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5 } };
				const int Use = (B.FormBar % 4) == 2 ? (Motif + 1) % 4 : ((B.FormBar % 4) == 3 ? 3 : Motif);
				for (int I = 0; I < 8; ++I)
				{
					const double At = Pats[Use][I];
					if (At >= 0.0 && At < M)
					{
						const bool bAcc = std::fabs(At - std::floor(At)) > 0.1 ? Rng.P(0.4f) : Rng.P(0.6f);
						Hit1(Tap, (bAcc ? 0.8f : 0.5f) * Lv, Sw(At), 0.2);
					}
				}
				Hit1(Kick, 0.6f * Lv, 0.0, 0.2);
				Hit1(Kick, 0.45f * Lv, 2.0, 0.2);
				Hit1(Hat, 0.5f * Lv, 1.0, 0.1);
				Hit1(Hat, 0.5f * Lv, 3.0, 0.1);
				return;
			}
			if (B.Role == ERole::Fours && (B.FormBar % 4) == 0 && B.FormBar > 0)
			{
				Hit1(Crash, 0.5f * Lv, 0.0, 0.2);
			}
			const bool bFill = (B.FormBar % 8) == 7 || B.bLastOfRole;
			if (Feel == EFeel::Waltz || M == 3)
			{
				Hit1(Kick, 0.5f * Lv, 0.0, 0.2);
				Hit1(Swish, 0.5f * Lv, 0.0, 1.0);
				Hit1(Tap, 0.34f * Lv, 1.0, 0.2);
				Hit1(Tap, 0.3f * Lv, 2.0, 0.2);
				if (bFill)
				{
					Hit1(Tap, 0.3f * Lv, 2.5, 0.2);
				}
				return;
			}
			if (Feel == EFeel::Tango)
			{
				Hit1(Kick, 0.55f * Lv, 0.0, 0.2);
				Hit1(Kick, 0.4f * Lv, 2.0, 0.2);
				for (int K = 0; K < 4; ++K)
				{
					Hit1(Tap, (K == 0 ? 0.5f : 0.36f) * Lv, (double)K, 0.2);
				}
				Hit1(Tap, 0.3f * Lv, 3.5, 0.2);
				if (bFill)
				{
					Hit1(Tap, 0.34f * Lv, 3.25, 0.1);
					Hit1(Tap, 0.4f * Lv, 3.75, 0.1);
				}
				return;
			}
			if (Feel == EFeel::Ballad)
			{
				for (int K = 0; K < 4; ++K)
				{
					Hit1(Swish, ((K & 1) ? 0.62f : 0.45f) * Lv, (double)K, 1.05);
				}
				Hit1(Hat, 0.3f * Lv, 1.0, 0.1);
				Hit1(Hat, 0.3f * Lv, 3.0, 0.1);
				Hit1(Kick, 0.3f * Lv, 0.0, 0.2);
				if (bFill)
				{
					Hit1(Tap, 0.25f * Lv, Sw(3.5), 0.2);
				}
				return;
			}
			if (Feel == EFeel::Ragtime)
			{
				Hit1(Kick, 0.5f * Lv, 0.0, 0.2);
				Hit1(Kick, 0.4f * Lv, 2.0, 0.2);
				Hit1(Tap, 0.42f * Lv, 1.0, 0.2);
				Hit1(Tap, 0.42f * Lv, 3.0, 0.2);
				for (int K = 0; K < 8; ++K)
				{
					Hit1(Swish, 0.3f * Lv, K * 0.5, 0.5);
				}
				if (bFill)
				{
					Hit1(Tap, 0.3f * Lv, 3.5, 0.2);
				}
				return;
			}
			// swing, foxtrot: the brushes stir on every beat and tap two and four; the ride comes in for the solos
			for (int K = 0; K < 4; ++K)
			{
				Hit1(Swish, ((K & 1) ? 0.6f : 0.42f) * Lv, (double)K, 1.0);
			}
			Hit1(Tap, 0.36f * Lv, 1.0, 0.2);
			Hit1(Tap, 0.4f * Lv, 3.0, 0.2);
			Hit1(Hat, 0.42f * Lv, 1.0, 0.1);
			Hit1(Hat, 0.42f * Lv, 3.0, 0.1);
			Hit1(Kick, 0.3f * Lv, 0.0, 0.2);
			Hit1(Kick, 0.22f * Lv, 2.0, 0.2);
			if (bSolos || B.Role == ERole::Out || B.Role == ERole::Tag)
			{
				static const double RideAt[6] = { 0.0, 1.0, 1.5, 2.0, 3.0, 3.5 };
				for (int K = 0; K < 6; ++K)
				{
					const bool bSkip = std::fabs(RideAt[K] - std::floor(RideAt[K])) > 0.1;
					Hit1(Ride, (bSkip ? 0.3f : (K == 1 || K == 4 ? 0.5f : 0.4f)) * Lv, Sw(RideAt[K]), 0.2);
				}
			}
			if (bFill)
			{
				Hit1(Tap, 0.4f * Lv, Sw(2.5), 0.2);
				Hit1(Tap, 0.5f * Lv, Sw(3.5), 0.2);
				if (Rng.P(0.5f))
				{
					Hit1(Tap, 0.3f * Lv, 3.0 + Tune->Swing * 0.5, 0.2);
				}
			}
		}

		// --- the tune itself

		double LayBack(int Inst) const
		{
			// the lead sits behind the beat, more on a ballad
			if (Inst == Reed)
			{
				return Tune->Feel == EFeel::Ballad ? 0.03 : (IsJazz() ? 0.014 : 0.004);
			}
			return Tune->Feel == EFeel::Ballad ? 0.012 : 0.0;
		}

		int LeadPitch(int Inst, int Midi) const
		{
			return Inst == Reed ? Fold(Midi + ReedShift, ReedLo, ReedHi) : Fold(Midi + PianoShift, 60, PianoHi - 3);
		}

		void WriteMelody(const FBar& B, int Inst, float Vel, bool bIntro)
		{
			const int M = Tune->Meter;
			const double From = (double)B.FormBar * M;
			const int First = Tune->BarNote[(size_t)B.FormBar];
			const int End = Tune->BarNote[(size_t)B.FormBar + 1];
			FRng Rng(Hash(Seed, (uint32_t)WBar, 404u + (uint32_t)Inst));
			const float Lv = RoleLevel(B);
			const double Lay = LayBack(Inst);
			const int Count = (int)Tune->Notes.size();
			double PickupBeats = 0.0;
			for (const FNote& P : Tune->Pickup)
			{
				PickupBeats = std::fmax(PickupBeats, -P.Beat);
			}
			for (int I = First; I < End; ++I)
			{
				const FNote& N = Tune->Notes[(size_t)I];
				const double X = N.Beat - From;
				if (bIntro && B.bLastOfRole && X + N.Len > M - PickupBeats + 1e-6)
				{
					// the intro's last bar leaves the pick-up to whoever has the head
					if (X >= M - PickupBeats - 1e-6)
					{
						continue;
					}
				}
				const double On = Sw(X);
				double Off = Sw(X + N.Len);
				const bool bHasNext = I + 1 < Count;
				const double Gap = bHasNext ? Tune->Notes[(size_t)I + 1].Beat - (N.Beat + N.Len) : 1.0;
				const double PrevGap = I > 0 ? N.Beat - (Tune->Notes[(size_t)I - 1].Beat + Tune->Notes[(size_t)I - 1].Len) : 1.0;
				bool bScoop = false;
				if (Inst == Reed)
				{
					const bool bSlur = Gap < 0.01 && N.Len <= 1.01 && Rng.P(IsJazz() ? 0.65f : 0.5f);
					if (bSlur)
					{
						Off = Sw(X + N.Len) + 0.06;  // into the next note: one breath
					}
					else
					{
						Off = On + (Off - On) * (N.Len >= 1.0 ? 0.9 : 0.78);
					}
					bScoop = IsJazz() && N.Len >= 1.0 && (PrevGap > 0.4 || I == First) && Rng.P(B.Role == ERole::Head ? 0.35f : 0.55f);
				}
				else
				{
					Off = On + (Off - On) * 0.92;
				}
				// louder up the phrase and on the beat
				const double Frac = X - std::floor(X + 1e-6);
				float V = Vel * Lv * (Frac < 0.05 ? 1.f : 0.9f) * (1.f + (float)(N.Midi - 67) * 0.008f);
				if (Tune->Feel == EFeel::Swing && Frac > 0.4 && Frac < 0.6 && Gap > 0.4)
				{
					V *= 1.12f;  // the off-beat note before a silence is leaned on
				}
				Note(Inst, LeadPitch(Inst, N.Midi), V, On, Off - On, bScoop, Lay);
			}
		}

		void WritePickup(int Inst)
		{
			const int M = Tune->Meter;
			for (const FNote& N : Tune->Pickup)
			{
				const double X = (double)M + N.Beat;
				const double On = Sw(X);
				const double Off = Sw(X + N.Len);
				Note(Inst, LeadPitch(Inst, N.Midi), 0.62f, On, (Off - On) * 0.9, false, LayBack(Inst));
			}
		}

		/** The pianist's right hand answers where the tune holds a long note or rests at the end of a bar. */
		void WriteFill(const FBar& B, const FSeg* Segs, int NumSegs)
		{
			if (!IsJazz() || bSoft)
			{
				return;
			}
			const int M = Tune->Meter;
			const double From = (double)B.FormBar * M;
			const int First = Tune->BarNote[(size_t)B.FormBar];
			const int End = Tune->BarNote[(size_t)B.FormBar + 1];
			// free from: the last onset of the bar (a long note may ring on under the fill)
			double LastOn = -1.0;
			for (int I = First; I < End; ++I)
			{
				LastOn = Tune->Notes[(size_t)I].Beat - From;
			}
			// and nothing may start in the next bar's first beat's pick-up zone: fills stop by the bar line anyway
			if (LastOn > (double)M - 2.5 + 1e-6)
			{
				return;
			}
			FRng Rng(Hash(Seed, (uint32_t)WBar, 505u));
			if (!Rng.P((B.FormBar % 4) == 3 ? 0.85f : 0.4f))
			{
				return;
			}
			const float Lv = RoleLevel(B);
			const double Start = std::fmax(LastOn + 1.0, (double)M - 2.0);
			static const int Shapes[4][4] = { { 0, 1, 2, 4 }, { 4, 2, 1, 0 }, { 0, 2, 1, 3 }, { 2, 0, 1, 2 } };
			const int* Shape = Shapes[Rng.R(4)];
			const int Anchor = 72 + Rng.R(5);
			int K = 0;
			for (double At = Start; At < (double)M - 0.01 && K < 4; At += 0.5, ++K)
			{
				const FChordAt& C = SegAt(Segs, NumSegs, At);
				const int Midi = Fold(Step(K == 0 || K == 3 ? ToneMask(C) : ScaleMask(C), Anchor, Shape[K]), 67, 91);
				Note(Piano, Midi, (0.36f + 0.03f * K) * Lv, Sw(At), 0.42);
			}
		}

		/** The bar's note to hang a solo on: the tune's longest note in it (in the soloist's register). */
		int Anchor(const FBar& B, const FChordAt& C, int Inst) const
		{
			const int First = Tune->BarNote[(size_t)B.FormBar];
			const int End = Tune->BarNote[(size_t)B.FormBar + 1];
			double Best = 0.0;
			int Midi = -1;
			for (int I = First; I < End; ++I)
			{
				if (Tune->Notes[(size_t)I].Len > Best)
				{
					Best = Tune->Notes[(size_t)I].Len;
					Midi = Tune->Notes[(size_t)I].Midi;
				}
			}
			if (Midi < 0)
			{
				// a bar of rest in the tune: the chord's third
				Midi = 60 + (C.Root + Kind(C.KindIndex).Tones[1]) % 12;
			}
			return LeadPitch(Inst, Midi + (Inst == Piano ? 12 : 0));
		}

		/** An improvised bar. Phrases of four bars: a motif, the motif again on the new chord, a longer run, a long note and a
		 *  breath; or the tune itself turned about (its long notes kept, the gaps filled from the chord's scale). */
		void WriteSolo(const FBar& B, const FSeg* Segs, int NumSegs, int Inst)
		{
			const int M = Tune->Meter;
			const int InPhrase = B.FormBar % 4;
			FRng Phrase(Hash(Seed, (uint32_t)(WBar - InPhrase), 606u));
			FRng Rng(Hash(Seed, (uint32_t)WBar, 607u));
			const float Lv = RoleLevel(B);
			const double Lay = LayBack(Inst);
			const int Lo = Inst == Reed ? ReedLo + 2 : 64;
			const int Hi = Inst == Reed ? ReedHi - 2 : 88;
			const EFeel Feel = Tune->Feel;
			const float MotifChance = Feel == EFeel::Swing || Feel == EFeel::Foxtrot ? 0.6f : (Feel == EFeel::Ballad ? 0.3f : 0.f);
			const bool bMotif = Phrase.P(MotifChance);
			const int RhythmPick = Phrase.R(6);
			const int ShapePick = Phrase.R(6);
			const int RunPick = Phrase.R(3);
			const float Base = Inst == Reed ? 0.7f : 0.66f;
			if (bMotif && M == 4)
			{
				struct FStep { double Beat; double Len; };
				static const FStep Rhythms[6][6] = {
					{ { 0.5, 0.5 }, { 1.0, 0.5 }, { 1.5, 0.5 }, { 2.0, 1.5 }, { -1, 0 }, { -1, 0 } },
					{ { 0.0, 1.0 }, { 1.5, 0.5 }, { 2.0, 0.5 }, { 2.5, 1.2 }, { -1, 0 }, { -1, 0 } },
					{ { 1.0, 0.5 }, { 1.5, 0.5 }, { 2.0, 0.5 }, { 2.5, 0.5 }, { 3.0, 1.0 }, { -1, 0 } },
					{ { 0.0, 1.5 }, { 1.5, 0.5 }, { 2.0, 1.6 }, { -1, 0 }, { -1, 0 }, { -1, 0 } },
					{ { 0.0, 0.5 }, { 0.5, 0.5 }, { 1.0, 1.0 }, { 2.5, 0.5 }, { 3.0, 0.8 }, { -1, 0 } },
					{ { 0.5, 0.5 }, { 1.0, 1.0 }, { 2.5, 0.5 }, { 3.0, 0.5 }, { 3.5, 0.5 }, { -1, 0 } },
				};
				static const int Shapes[6][6] = {
					{ 0, 1, 2, 1, 0, -1 }, { 0, -1, -2, 0, 2, 1 }, { 2, 1, 0, -1, 0, 1 },
					{ 0, 2, 4, 2, 1, 0 }, { 0, 1, -1, 0, 3, 2 }, { 3, 2, 0, 1, 0, -2 },
				};
				static const int Runs[3][8] = { { 0, 1, 2, 3, 4, 3, 2, 1 }, { 4, 3, 2, 1, 0, -1, 0, 1 }, { 0, 2, 1, 3, 2, 4, 3, 2 } };
				if (InPhrase == 3)
				{
					// the phrase comes home: a chord note held, then a breath
					const FChordAt& C = *Segs[0].Chord;
					const int Midi = Fold(Nearest(ToneMask(C), Anchor(B, C, Inst)), Lo, Hi);
					if (Rng.P(0.5f))
					{
						Note(Inst, Fold(Step(ScaleMask(C), Midi, Rng.P(0.5f) ? 1 : -1), Lo, Hi), Base * 0.85f * Lv, Sw(0.5), 0.45, false, Lay);
						Note(Inst, Midi, Base * Lv, 1.0, 1.7, false, Lay);
					}
					else
					{
						Note(Inst, Midi, Base * Lv, 0.0, 2.2, Inst == Reed, Lay);
					}
					return;
				}
				if (InPhrase == 2)
				{
					// the run: eighths up and down the scale from the tune's note
					const int Skip = Rng.R(2);
					for (int K = Skip; K < 8; ++K)
					{
						const double At = K * 0.5;
						const FChordAt& C = SegAt(Segs, NumSegs, At);
						const int A = Anchor(B, C, Inst);
						const bool bStrong = (K % 4) == 0;
						// up and down the scale; the notes on the beat pulled onto the chord
						int Midi = Step(ScaleMask(C), A, Runs[RunPick][K] - 1);
						Midi = Fold(bStrong ? Nearest(ToneMask(C), Midi) : Midi, Lo, Hi);
						const bool bLast = K == 7;
						Note(Inst, Midi, Base * (bStrong ? 1.f : 0.86f) * Lv, Sw(At), bLast ? 0.4 : Sw(At + 0.5) - Sw(At) + (Inst == Reed ? 0.05 : -0.04),
							false, Lay);
					}
					return;
				}
				// the motif, and its answer a bar later on that bar's chord
				const FStep* R = Rhythms[RhythmPick];
				const int* S = Shapes[ShapePick];
				for (int K = 0; K < 6 && R[K].Beat >= 0.0; ++K)
				{
					const FChordAt& C = SegAt(Segs, NumSegs, R[K].Beat);
					const int A = Anchor(B, C, Inst);
					const bool bLong = R[K].Len >= 1.0;
					int Midi = Step(ScaleMask(C), A, S[K] + (InPhrase == 1 && K >= 4 ? 1 : 0));
					Midi = Fold(bLong ? Nearest(ToneMask(C), Midi) : Midi, Lo, Hi);
					const double On = Sw(R[K].Beat);
					const bool bJoin = Inst == Reed && !bLong && K + 1 < 6 && R[K + 1].Beat >= 0.0 && R[K + 1].Beat - R[K].Beat < 0.51;
					const double Len = bJoin ? Sw(R[K].Beat + 0.5) - On + 0.05 : (Sw(R[K].Beat + R[K].Len) - On) * 0.88;
					Note(Inst, Midi, Base * (bLong ? 1.f : 0.88f) * Lv, On, Len, Inst == Reed && bLong && K == 0, Lay);
				}
				return;
			}
			// the tune turned about
			const double From = (double)B.FormBar * M;
			const int First = Tune->BarNote[(size_t)B.FormBar];
			const int End = Tune->BarNote[(size_t)B.FormBar + 1];
			const int Count = (int)Tune->Notes.size();
			const bool bBusy = Feel != EFeel::Ballad;
			if (First == End && InPhrase != 3)
			{
				// the tune rests here: an arpeggio up to the next bar
				const FChordAt& C = *Segs[0].Chord;
				const int A = Anchor(B, C, Inst);
				for (int K = 0; K < M; ++K)
				{
					Note(Inst, Fold(Step(ToneMask(C), A, K - 1), Lo, Hi), Base * 0.85f * Lv, (double)K, 0.85, false, Lay);
				}
				return;
			}
			for (int I = First; I < End; ++I)
			{
				const FNote& N = Tune->Notes[(size_t)I];
				const double X = N.Beat - From;
				const FChordAt& C = SegAt(Segs, NumSegs, X);
				const int Own = LeadPitch(Inst, N.Midi + (Inst == Piano ? 12 : 0));
				const int NextMidi = I + 1 < Count ? LeadPitch(Inst, Tune->Notes[(size_t)I + 1].Midi + (Inst == Piano ? 12 : 0)) : Own;
				const unsigned Sm = ScaleMask(C);
				if (N.Len >= 1.99)
				{
					// a long note: held a while, then two or three notes walking to the next one
					const int Walk = bBusy && Rng.P(0.75f) ? (Rng.P(0.5f) ? 3 : 2) : (Rng.P(0.4f) ? 2 : 0);
					const double Tail = Walk * 0.5;
					Note(Inst, Own, Base * Lv, Sw(X), Sw(X + N.Len - Tail) - Sw(X) - (Walk > 0 ? 0.0 : 0.15), Inst == Reed && Rng.P(0.5f), Lay);
					const int Dir = NextMidi >= Own ? -1 : 1;  // come to the next note from the far side
					for (int K = 0; K < Walk; ++K)
					{
						const double At = X + N.Len - Tail + K * 0.5;
						const int Midi = Fold(Step(Sm, NextMidi, Dir * (Walk - K)), Lo, Hi);
						Note(Inst, Midi, Base * 0.84f * Lv, Sw(At), Sw(At + 0.5) - Sw(At) + (Inst == Reed ? 0.04 : -0.04), false, Lay);
					}
				}
				else if (N.Len >= 0.99 && Rng.P(bBusy ? 0.5f : 0.25f))
				{
					// a beat: the note and a neighbour, or the neighbour first
					const int Nb = Fold(Step(Sm, Own, Rng.P(0.5f) ? 1 : -1), Lo, Hi);
					const bool bFirst = Rng.P(0.35f);
					Note(Inst, bFirst ? Nb : Own, Base * 0.95f * Lv, Sw(X), Sw(X + 0.5) - Sw(X) + (Inst == Reed ? 0.04 : -0.04), false, Lay);
					Note(Inst, bFirst ? Own : Nb, Base * 0.86f * Lv, Sw(X + 0.5), (Sw(X + N.Len) - Sw(X + 0.5)) * 0.85, false, Lay);
				}
				else
				{
					// kept, or (a short one, sometimes) its neighbour in the chord
					const bool bSwap = N.Len < 0.99 && Rng.P(0.22f);
					const int Midi = bSwap ? Fold(Nearest(ToneMask(C), Own + (Rng.P(0.5f) ? 2 : -2)), Lo, Hi) : Own;
					Note(Inst, Midi, Base * 0.94f * Lv, Sw(X), (Sw(X + N.Len) - Sw(X)) * (N.Len >= 1.0 ? 0.9 : 0.84), false, Lay);
				}
			}
		}

		/** The last chord, held: everyone on the tonic, the piano rolled, a cymbal. The tango ends on two dry strokes. */
		void WriteFinal()
		{
			const FChordAt C = TonicChord();
			const int M = Tune->Meter;
			const int Root = BassRoot(C);
			const int Last = Tune->Notes.empty() ? 60 + Tune->KeyRoot : Tune->Notes.back().Midi;
			if (Tune->Feel == EFeel::Tango)
			{
				FChordAt V;
				V.Root = (C.Root + 7) % 12;
				V.KindIndex = KindDom7;
				Note(Bass, BassRoot(V), 0.85f, 0.0, 0.5);
				Chord(V, 0.62f, 0.0, 0.4, true);
				Note(Piano, Fold(36 + V.Root, 38, 49), 0.6f, 0.0, 0.4);
				Note(Drums, Tap, 0.7f, 0.0, 0.2);
				Note(Drums, Kick, 0.6f, 0.0, 0.2);
				Note(Bass, Root, 0.9f, 1.0, 1.2);
				Chord(C, 0.66f, 1.0, 1.0, true);
				Note(Piano, Fold(36 + C.Root, 38, 49), 0.66f, 1.0, 1.0);
				Note(Drums, Tap, 0.8f, 1.0, 0.2);
				Note(Drums, Kick, 0.7f, 1.0, 0.2);
				Note(Reed, LeadPitch(Reed, Nearest(ToneMask(V), Last + 1)), 0.7f, 0.0, 0.5);
				Note(Reed, LeadPitch(Reed, Nearest(ToneMask(C), Last)), 0.75f, 1.0, 0.8);
				return;
			}
			const double Hold = (double)M - 0.4;
			Note(Bass, Root, 0.85f, 0.0, Hold, true);
			Note(Piano, Fold(36 + C.Root, 36, 47), 0.6f, 0.0, Hold);
			Chord(C, 0.55f, 0.0, Hold, false, 0.07);
			Note(Piano, Fold(Nearest(ToneMask(C), 76), 70, 84), 0.5f, 0.3, Hold - 0.3);
			Note(Drums, Crash, 0.45f, 0.0, 1.0);
			Note(Drums, Kick, 0.5f, 0.0, 0.2);
			for (int K = 0; K < M; ++K)
			{
				Note(Drums, Swish, 0.5f - 0.08f * K, (double)K, 1.0);
			}
			Note(Reed, LeadPitch(Reed, Nearest(ToneMask(C), Last)), bSoft ? 0.55f : 0.75f, 0.0, Hold - 0.2, true, LayBack(Reed));
		}
	};
}
