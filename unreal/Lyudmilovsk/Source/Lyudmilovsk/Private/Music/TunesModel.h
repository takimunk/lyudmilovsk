#pragma once

// The tunes the hero can play and the rhythm game that plays them (docs/instruments.md). Plain C++, no engine types:
// tools/instrumentsim checks the data (Content/Music/tunes.txt) and the judging.
//
// tunes.txt: a block per tune.
//   [ode_joy]
//   bpm = 108                         quarter notes (beats) a minute
//   for = piano guitar                which instruments play it
//   start = yes                       he knows it from the start (else he learns it from its sheet music)
//   sheet = school_music_board        the city act(s) whose thing holds its sheet music (docs/city_interiors.md)
//   sing = yes                        people round a bonfire sing it (town talk: hero_sing_<id>)
//   notes = E4 E4 F4 G4/2 r/0.5 C4+E4+G4/2 ...   pitch[+pitch]/beats (1 by default); r is a rest; | is ignored
//   chords = C/4 G/2 C/2 ...          the guitar's strummed chords under the melody (name/beats)
// The title is the string key music.tune.<id>.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "Music/InstrumentSynth.h"

namespace LyudTunes
{
	struct FNote
	{
		double Beat = 0.0;
		double Len = 1.0;
		std::vector<int> Midi;  // empty: a rest
	};

	struct FChord
	{
		double Beat = 0.0;
		double Len = 4.0;
		std::string Name;
	};

	struct FTune
	{
		std::string Id;
		double Bpm = 100.0;
		bool bPiano = true, bGuitar = false, bStart = false, bSing = false;
		std::vector<std::string> Sheets;
		std::vector<FNote> Notes;    // the sounding notes and rests in order
		std::vector<FChord> Chords;
		double Beats = 0.0;          // its length
		double SecondsPerBeat() const { return 60.0 / (Bpm > 1.0 ? Bpm : 1.0); }
		int NumPlayed() const
		{
			int N = 0;
			for (const FNote& Nt : Notes)
			{
				N += Nt.Midi.empty() ? 0 : 1;
			}
			return N;
		}
	};

	/** The guitar's open chords (low string to high, MIDI): a strum plays them a few ms apart. Empty if unknown. */
	inline std::vector<int> ChordNotes(const std::string& Name)
	{
		struct FRow { const char* Name; int N[6]; };
		static const FRow Rows[] = {
			{ "C", { 48, 52, 55, 60, 64, 0 } }, { "C7", { 48, 52, 58, 60, 64, 0 } },
			{ "D", { 50, 57, 62, 66, 0, 0 } }, { "D7", { 50, 57, 60, 66, 0, 0 } }, { "Dm", { 50, 57, 62, 65, 0, 0 } },
			{ "E", { 40, 47, 52, 56, 59, 64 } }, { "E7", { 40, 47, 50, 56, 59, 64 } }, { "Em", { 40, 47, 52, 55, 59, 64 } },
			{ "F", { 41, 48, 53, 57, 60, 65 } }, { "G", { 43, 47, 50, 55, 59, 67 } }, { "G7", { 43, 47, 50, 53, 59, 65 } },
			{ "A", { 45, 52, 57, 61, 64, 0 } }, { "A7", { 45, 52, 55, 61, 64, 0 } }, { "Am", { 45, 52, 57, 60, 64, 0 } },
			{ "Bb", { 46, 53, 58, 62, 65, 0 } }, { "B7", { 47, 51, 57, 59, 66, 0 } }, { "Bm", { 47, 54, 59, 62, 66, 0 } },
		};
		for (const FRow& R : Rows)
		{
			if (Name == R.Name)
			{
				std::vector<int> Out;
				for (int X : R.N)
				{
					if (X > 0)
					{
						Out.push_back(X);
					}
				}
				return Out;
			}
		}
		return {};
	}

	/** The chords the free play's number keys strum (1..8). */
	inline const char* FreeChord(int Index)
	{
		static const char* Names[8] = { "C", "G", "Am", "F", "Dm", "E", "Em", "D" };
		return Index >= 0 && Index < 8 ? Names[Index] : "";
	}

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

	inline std::vector<std::string> Words(const std::string& S)
	{
		std::vector<std::string> Out;
		std::istringstream In(S);
		std::string W;
		while (In >> W)
		{
			if (W != "|")
			{
				Out.push_back(W);
			}
		}
		return Out;
	}

	/** "E4/1.5" -> its pitches and beats; false (with Error) when it doesn't read. */
	inline bool ParseNote(const std::string& Tok, FNote& Out, std::string& Error)
	{
		std::string Pitch = Tok;
		Out.Len = 1.0;
		const size_t Slash = Tok.find('/');
		if (Slash != std::string::npos)
		{
			Pitch = Tok.substr(0, Slash);
			char* End = nullptr;
			const std::string L = Tok.substr(Slash + 1);
			Out.Len = std::strtod(L.c_str(), &End);
			if (!End || *End != 0 || !(Out.Len > 0.0) || Out.Len > 32.0)
			{
				Error = "bad length in " + Tok;
				return false;
			}
		}
		Out.Midi.clear();
		if (Pitch == "r")
		{
			return true;
		}
		size_t From = 0;
		while (From <= Pitch.size())
		{
			const size_t Plus = Pitch.find('+', From);
			const std::string One = Pitch.substr(From, Plus == std::string::npos ? std::string::npos : Plus - From);
			const int M = LyudInstr::NoteFromName(One.c_str());
			if (M < 0)
			{
				Error = "bad pitch " + One + " in " + Tok;
				return false;
			}
			Out.Midi.push_back(M);
			if (Plus == std::string::npos)
			{
				break;
			}
			From = Plus + 1;
		}
		return true;
	}

	/** Reads tunes.txt; Errors gets one line per problem (the tune is still read where it can be). */
	inline std::vector<FTune> Parse(const std::string& Text, std::vector<std::string>& Errors)
	{
		std::vector<FTune> Out;
		std::istringstream In(Text);
		std::string Line;
		int LineNo = 0;
		FTune* Cur = nullptr;
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
				const size_t E = T.find(']');
				Out.push_back(FTune());
				Cur = &Out.back();
				Cur->Id = T.substr(1, E == std::string::npos ? std::string::npos : E - 1);
				Cur->bPiano = Cur->bGuitar = false;
				continue;
			}
			const size_t Eq = T.find('=');
			if (!Cur || Eq == std::string::npos)
			{
				Errors.push_back("line " + std::to_string(LineNo) + ": not in a tune or no =");
				continue;
			}
			const std::string K = Trim(T.substr(0, Eq));
			const std::string V = Trim(T.substr(Eq + 1));
			const std::string Where = Cur->Id + " (line " + std::to_string(LineNo) + ")";
			if (K == "bpm")
			{
				Cur->Bpm = std::atof(V.c_str());
				if (Cur->Bpm < 30.0 || Cur->Bpm > 260.0)
				{
					Errors.push_back(Where + ": bpm out of 30..260");
				}
			}
			else if (K == "for")
			{
				for (const std::string& W : Words(V))
				{
					if (W == "piano")
					{
						Cur->bPiano = true;
					}
					else if (W == "guitar")
					{
						Cur->bGuitar = true;
					}
					else
					{
						Errors.push_back(Where + ": unknown instrument " + W);
					}
				}
			}
			else if (K == "start")
			{
				Cur->bStart = V == "yes";
			}
			else if (K == "sing")
			{
				Cur->bSing = V == "yes";
			}
			else if (K == "sheet")
			{
				Cur->Sheets = Words(V);
			}
			else if (K == "notes")
			{
				double Beat = Cur->Notes.empty() ? 0.0 : Cur->Notes.back().Beat + Cur->Notes.back().Len;
				for (const std::string& W : Words(V))
				{
					FNote N;
					std::string Err;
					if (!ParseNote(W, N, Err))
					{
						Errors.push_back(Where + ": " + Err);
						continue;
					}
					N.Beat = Beat;
					Beat += N.Len;
					Cur->Notes.push_back(N);
				}
				Cur->Beats = std::max(Cur->Beats, Beat);
			}
			else if (K == "chords")
			{
				double Beat = Cur->Chords.empty() ? 0.0 : Cur->Chords.back().Beat + Cur->Chords.back().Len;
				for (const std::string& W : Words(V))
				{
					FChord C;
					const size_t Slash = W.find('/');
					C.Name = W.substr(0, Slash);
					C.Len = Slash == std::string::npos ? 4.0 : std::atof(W.substr(Slash + 1).c_str());
					if (C.Name != "r" && ChordNotes(C.Name).empty())
					{
						Errors.push_back(Where + ": unknown chord " + C.Name);
					}
					if (!(C.Len > 0.0))
					{
						Errors.push_back(Where + ": bad chord length " + W);
						C.Len = 1.0;
					}
					C.Beat = Beat;
					Beat += C.Len;
					Cur->Chords.push_back(C);
				}
			}
			else
			{
				Errors.push_back(Where + ": unknown key " + K);
			}
		}
		for (const FTune& Tn : Out)
		{
			if (!Tn.bPiano && !Tn.bGuitar)
			{
				Errors.push_back(Tn.Id + ": for = piano and/or guitar");
			}
			if (Tn.NumPlayed() < 8)
			{
				Errors.push_back(Tn.Id + ": fewer than 8 notes");
			}
			if (!Tn.bStart && Tn.Sheets.empty())
			{
				Errors.push_back(Tn.Id + ": neither start = yes nor a sheet = place: he can never learn it");
			}
		}
		return Out;
	}

	enum class EHit : uint8_t { None, Perfect, Good, Miss, Stray };

	/**
	 * The rhythm game: the tune's notes come at their times (a lead-in of LeadBeats first); each press of the key plays the
	 * note due if it is near enough in time. A note let pass is a miss (it stays silent). Times in seconds of the game's
	 * real clock from Start.
	 */
	struct FRhythm
	{
		static constexpr double PerfectWindow = 0.07;
		static constexpr double GoodWindow = 0.17;
		static constexpr double LeadBeats = 4.0;

		const FTune* Tune = nullptr;
		double T0 = 0.0;       // when beat 0 falls
		std::vector<EHit> Judged;  // per note (rests: Perfect from the start, never shown)
		int Perfect = 0, Good = 0, Miss = 0, Stray = 0;
		int Next = 0;          // the first note not judged yet
		bool bDone = false;

		void Start(const FTune& T, double Now)
		{
			Tune = &T;
			T0 = Now + LeadBeats * T.SecondsPerBeat();
			Judged.assign(T.Notes.size(), EHit::None);
			for (size_t I = 0; I < T.Notes.size(); ++I)
			{
				if (T.Notes[I].Midi.empty())
				{
					Judged[I] = EHit::Perfect;
				}
			}
			Perfect = Good = Miss = Stray = 0;
			Next = 0;
			bDone = false;
			Skip();
		}

		double TimeOf(int Note) const { return T0 + Tune->Notes[(size_t)Note].Beat * Tune->SecondsPerBeat(); }
		double EndTime() const { return Tune ? T0 + Tune->Beats * Tune->SecondsPerBeat() : 0.0; }
		/** Where the music is, in beats (negative during the lead-in). */
		double BeatAt(double Now) const { return Tune ? (Now - T0) / Tune->SecondsPerBeat() : 0.0; }

		/** A press now: the note it plays (-1: none, a stray press), and how well it was timed. */
		int Press(double Now, EHit& Out)
		{
			Out = EHit::Stray;
			if (!Tune || bDone)
			{
				return -1;
			}
			Update(Now);
			// the nearest unjudged note in the window (normally Next; a quick player may be early for it)
			for (int I = Next; I < (int)Judged.size(); ++I)
			{
				if (Judged[(size_t)I] != EHit::None)
				{
					continue;
				}
				const double D = Now - TimeOf(I);
				if (D < -GoodWindow)
				{
					break;
				}
				if (std::fabs(D) <= GoodWindow)
				{
					Out = std::fabs(D) <= PerfectWindow ? EHit::Perfect : EHit::Good;
					Judged[(size_t)I] = Out;
					(Out == EHit::Perfect ? Perfect : Good)++;
					Skip();
					return I;
				}
			}
			++Stray;
			return -1;
		}

		/** Notes let pass become misses; returns how many just did. */
		int Update(double Now)
		{
			if (!Tune || bDone)
			{
				return 0;
			}
			int Missed = 0;
			while (Next < (int)Judged.size() && Now - TimeOf(Next) > GoodWindow)
			{
				if (Judged[(size_t)Next] == EHit::None)
				{
					Judged[(size_t)Next] = EHit::Miss;
					++Miss;
					++Missed;
				}
				++Next;
				Skip();
			}
			if (Next >= (int)Judged.size() && Now >= EndTime())
			{
				bDone = true;
			}
			return Missed;
		}

		/** 0..1: perfect notes count whole, good ones 0.7, stray presses take a little off. */
		double Score() const
		{
			const int N = Tune ? Tune->NumPlayed() : 0;
			if (N <= 0)
			{
				return 0.0;
			}
			const double S = (Perfect + 0.7 * Good - 0.15 * Stray) / N;
			return S < 0.0 ? 0.0 : (S > 1.0 ? 1.0 : S);
		}

		/** 0..3 stars: 0 under half, 1 from 0.5, 2 from 0.75, 3 from 0.92. */
		int Stars() const
		{
			const double S = Score();
			return S >= 0.92 ? 3 : (S >= 0.75 ? 2 : (S >= 0.5 ? 1 : 0));
		}

	private:
		void Skip()
		{
			while (Next < (int)Judged.size() && Judged[(size_t)Next] != EHit::None)
			{
				++Next;
			}
		}
	};
}
