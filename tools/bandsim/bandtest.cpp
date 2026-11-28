// Offline checks and renders of the ship's band (tools/bandsim/run.sh). Plain C++ on Private/Music/ShipBandModel.h.

#include <ctime>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Music/ShipBandModel.h"

namespace
{
	int Fails = 0, Checks = 0;

	void Check(bool bOk, const std::string& What)
	{
		++Checks;
		if (!bOk)
		{
			++Fails;
			std::printf("FAIL %s\n", What.c_str());
		}
	}

	bool WriteWav(const std::string& Path, const std::vector<float>& Stereo, int Rate)
	{
		std::ofstream F(Path, std::ios::binary);
		if (!F)
		{
			return false;
		}
		const uint32_t Bytes = (uint32_t)(Stereo.size() * 2);
		auto U32 = [&F](uint32_t V) { F.write((const char*)&V, 4); };
		auto U16 = [&F](uint16_t V) { F.write((const char*)&V, 2); };
		F.write("RIFF", 4);
		U32(36 + Bytes);
		F.write("WAVEfmt ", 8);
		U32(16);
		U16(1);
		U16(2);
		U32((uint32_t)Rate);
		U32((uint32_t)Rate * 4);
		U16(4);
		U16(16);
		F.write("data", 4);
		U32(Bytes);
		for (const float X : Stereo)
		{
			const int16_t S = (int16_t)(X * 32767.f);
			F.write((const char*)&S, 2);
		}
		return true;
	}

	/** Renders a whole tune in blocks of 512 frames; returns the stereo samples. */
	std::vector<float> RenderTune(LyudBand::FPlayer& P, const LyudBand::FTune& T, uint32_t Seed, bool bSoft, double At = 0.0, double Seconds = -1.0)
	{
		P.Stats = LyudBand::FStats();
		P.Start(T, Seed, bSoft, At);
		std::vector<float> Out;
		const double Len = Seconds > 0.0 ? Seconds : P.Duration() - At;
		const long long Frames = (long long)(Len * P.SampleRate()) + 1024;
		Out.resize((size_t)Frames * 2);
		long long Done = 0;
		while (Done < Frames)
		{
			const int N = (int)std::min<long long>(512, Frames - Done);
			P.Render(Out.data() + Done * 2, N);
			Done += N;
		}
		return Out;
	}

	uint64_t HashSamples(const std::vector<float>& V)
	{
		uint64_t H = 1469598103934665603ull;
		for (const float X : V)
		{
			uint32_t B;
			std::memcpy(&B, &X, 4);
			H = (H ^ B) * 1099511628211ull;
		}
		return H;
	}

	double Rms(const std::vector<float>& V, size_t From, size_t To)
	{
		double S = 0.0;
		To = std::min(To, V.size());
		for (size_t I = From; I < To; ++I)
		{
			S += (double)V[I] * V[I];
		}
		return To > From ? std::sqrt(S / (double)(To - From)) : 0.0;
	}

	const char* RoleName(LyudBand::ERole R)
	{
		static const char* N[] = { "intro", "head", "pianohead", "solo", "piano", "fours", "out", "tag", "final" };
		return N[(int)R];
	}

	int Test(const LyudBand::FBook& Book, const std::vector<std::string>& Errors)
	{
		using namespace LyudBand;
		for (const std::string& E : Errors)
		{
			Check(false, "band_tunes.txt: " + E);
		}
		Check(Book.Tunes.size() >= 14, "14 tunes or more (" + std::to_string(Book.Tunes.size()) + ")");
		for (const char* S : { "concert1", "captains_dinner", "concert2" })
		{
			const int Ix = Book.FindSet(S);
			Check(Ix >= 0 && Book.Sets[(size_t)Ix].Tunes.size() >= 5, std::string("set ") + S + " with 5 tunes or more");
		}
		{
			const int Ix = Book.FindSet("captains_dinner");
			Check(Ix >= 0 && Book.Sets[(size_t)Ix].bSoft, "the captain's dinner is soft");
		}
		FPlayer P(32000);
		double TotalAudio = 0.0;
		std::printf("%-18s %-22s %5s %4s %-8s %5s %6s %6s %5s %5s  notes p/b/r/d\n", "tune", "composer", "died", "bars", "feel", "sec", "peak", "rms", "poly", "soft");
		for (const FTune& T : Book.Tunes)
		{
			const std::string W = "[" + T.Id + "] ";
			Check(!T.Composer.empty(), W + "names its composer");
			Check(T.Died >= 0 && T.Died <= 1955, W + "composer dead by 1955 (died = " + std::to_string(T.Died) + ")");
			Check(!T.Source.empty(), W + "has a source note");
			Check(T.Seconds > 60.0 && T.Seconds < 330.0, W + "plays 1 to 5.5 minutes (" + std::to_string(T.Seconds) + ")");
			// the melody under its chords: most of the long notes belong to the chord's scale
			int Long = 0, Fit = 0;
			for (const FNote& N : T.Notes)
			{
				if (N.Len >= 1.0)
				{
					const FChordAt& C = T.ChordAtBeat(N.Beat);
					unsigned M = Mask(Kind(C.KindIndex).Scale, 8, C.Root) | Mask(Kind(C.KindIndex).Tones, 4, C.Root);
					if (C.KindIndex == KindDom7)
					{
						M |= Mask(Kind(KindDom7b9).Scale, 8, C.Root);
					}
					++Long;
					Fit += (M >> (N.Midi % 12)) & 1u;
				}
			}
			Check(Long == 0 || Fit * 100 >= Long * 85, W + "long melody notes fit their chords (" + std::to_string(Fit) + "/" + std::to_string(Long) + ")");
			double Peaks[2] = { 0, 0 }, RmsV[2] = { 0, 0 };
			int Poly = 0;
			int NoteCount[NumInst] = { 0, 0, 0, 0 };
			for (int Soft = 0; Soft < 2; ++Soft)
			{
				const std::vector<float> A = RenderTune(P, T, 7u, Soft != 0);
				const FStats S = P.Stats;
				TotalAudio += (double)A.size() / 2.0 / P.SampleRate();
				Peaks[Soft] = S.Peak;
				RmsV[Soft] = std::sqrt(S.SumSq / (double)std::max<long long>(1, S.Frames));
				const std::string V = W + (Soft ? "soft: " : "");
				Check(S.RangeErrors == 0, V + "every note in its instrument's range (" + std::to_string(S.RangeErrors) + " out)");
				Check(S.PianoOverflow == 0, V + "the piano never out of voices (max " + std::to_string(S.MaxPianoVoices) + ")");
				Check(S.QueueOverflow == 0, V + "the event queue never full");
				Check(S.Peak < 0.98, V + "peak < 0.98 (" + std::to_string(S.Peak) + ")");
				Check(RmsV[Soft] > 0.03 && RmsV[Soft] < 0.25, V + "rms in 0.03..0.25 (" + std::to_string(RmsV[Soft]) + ")");
				Check(S.Notes[Piano] > 20 && S.Notes[Bass] > 20 && S.Notes[Drums] > 20, V + "piano, bass and drums play");
				Check(S.Notes[Reed] > (Soft ? 3 : 20), V + "the reed plays");
				Check(!P.IsPlaying(), V + "ends by itself");
				// the end is quiet: the last quarter second
				Check(Rms(A, A.size() > 16000 ? A.size() - 16000 : 0, A.size()) < 0.02, V + "dies away by its end");
				if (!Soft)
				{
					Poly = S.MaxPianoVoices;
					for (int I = 0; I < NumInst; ++I)
					{
						NoteCount[I] = S.Notes[I];
					}
				}
			}
			std::printf("%-18s %-22.22s %5d %4d %-8d %5.0f %6.3f %6.3f %5d %5.0f  %d/%d/%d/%d\n", T.Id.c_str(), T.Composer.c_str(), T.Died, T.FormBars,
				(int)T.Feel, T.Seconds, Peaks[0], RmsV[0], Poly, T.SecondsSoft, NoteCount[0], NoteCount[1], NoteCount[2], NoteCount[3]);
		}
		// determinism, another seed another performance, joining mid-tune
		for (size_t Ix = 0; Ix < Book.Tunes.size(); Ix += 3)
		{
			const FTune& T = Book.Tunes[Ix];
			const std::string W = "[" + T.Id + "] ";
			std::vector<FEvent> LogA, LogB;
			P.Log = &LogA;
			const std::vector<float> A = RenderTune(P, T, 11u, false);
			P.Log = nullptr;
			const std::vector<float> B = RenderTune(P, T, 11u, false);
			const std::vector<float> C = RenderTune(P, T, 12u, false);
			Check(HashSamples(A) == HashSamples(B), W + "the same seed renders the same samples");
			Check(HashSamples(A) != HashSamples(C), W + "another seed is another performance");
			const double At = T.Seconds * 0.43;
			P.Log = &LogB;
			const std::vector<float> D = RenderTune(P, T, 11u, false, At);
			P.Log = nullptr;
			// the notes begun after the join are the play-through's notes
			std::vector<FEvent> OnA, OnB;
			const double Floor = (double)(long long)(At * P.SampleRate()) / P.SampleRate();
			for (const FEvent& E : LogA)
			{
				if (E.Type == 1 && E.Time >= Floor + 4.0)
				{
					OnA.push_back(E);
				}
			}
			for (const FEvent& E : LogB)
			{
				if (E.Type == 1 && E.Time >= Floor + 4.0)
				{
					OnB.push_back(E);
				}
			}
			bool bSame = OnA.size() == OnB.size() && !OnA.empty();
			for (size_t I = 0; bSame && I < OnA.size(); ++I)
			{
				bSame = OnA[I].Time == OnB[I].Time && OnA[I].Inst == OnB[I].Inst && OnA[I].Midi == OnB[I].Midi && OnA[I].Vel == OnB[I].Vel;
			}
			Check(bSame, W + "joined mid-tune it plays the play-through's notes (" + std::to_string(OnA.size()) + " / " + std::to_string(OnB.size()) + ")");
			// and sounds like it a few bars on
			const size_t Skip = (size_t)(At * P.SampleRate()) * 2;
			const size_t From = (size_t)(6.0 * P.SampleRate()) * 2, To = (size_t)(16.0 * P.SampleRate()) * 2;
			const double Ra = Rms(A, Skip + From, Skip + To), Rb = Rms(D, From, To);
			Check(Ra > 0.01 && std::fabs(Ra - Rb) < 0.12 * Ra, W + "joined mid-tune it sounds as loud (" + std::to_string(Ra) + " / " + std::to_string(Rb) + ")");
			// sample for sample too, the brushes left out (their hiss is noise: never the same twice over)
			P.Solo[Drums] = false;
			const std::vector<float> A2 = RenderTune(P, T, 11u, false);
			const std::vector<float> D2 = RenderTune(P, T, 11u, false, At);
			P.Solo[Drums] = true;
			double Diff = 0.0, Ref = 0.0;
			for (size_t I = From; I < To && Skip + I < A2.size() && I < D2.size(); ++I)
			{
				Diff += ((double)A2[Skip + I] - D2[I]) * ((double)A2[Skip + I] - D2[I]);
				Ref += (double)A2[Skip + I] * A2[Skip + I];
			}
			Check(Diff < 0.1 * Ref, W + "joined mid-tune the samples match the play-through after 6 s (difference " + std::to_string(Ref > 0 ? Diff / Ref : 1.0) + ")");
		}
		// the sets as timetables
		for (const FSet& S : Book.Sets)
		{
			const double Cycle = SetCycleSeconds(Book, S);
			int Slots = 0, Last = -2;
			bool bOrder = true;
			for (double At = 0.0; At < Cycle; At += 0.5)
			{
				const FSetPos Pos = Locate(Book, S, At);
				if (Pos.Tune >= 0 && Pos.Slot != Last)
				{
					bOrder = bOrder && Pos.Slot == Slots && Pos.Offset < 0.51;
					++Slots;
					Last = Pos.Slot;
				}
				else if (Pos.Tune < 0)
				{
					Last = -1;
				}
			}
			Check(bOrder && Slots == (int)S.Tunes.size(), "[set " + S.Id + "] every number comes in its turn (" + std::to_string(Slots) + ")");
			const FSetPos A = Locate(Book, S, 100.0), B = Locate(Book, S, 100.0 + Cycle);
			Check(A.Tune == B.Tune && A.Seed != B.Seed && B.Cycle == A.Cycle + 1, "[set " + S.Id + "] the second time round is another performance");
			std::printf("set %-16s %2d numbers, %5.0f s of music + %3.0f s interval = %5.0f s a round%s\n", S.Id.c_str(), (int)S.Tunes.size(), Cycle - S.Interval,
				S.Interval, Cycle, S.bSoft ? " (soft)" : "");
		}
		// the cost, at 48 kHz on one core
		{
			FPlayer Q(48000);
			double Audio = 0.0;
			const std::clock_t T0 = std::clock();  // this process's own time: other work on the machine does not count
			for (const FTune& T : Book.Tunes)
			{
				const std::vector<float> A = RenderTune(Q, T, 5u, false);
				Audio += (double)A.size() / 2.0 / 48000.0;
			}
			const double Took = (double)(std::clock() - T0) / CLOCKS_PER_SEC;
			const double Speed = Audio / std::max(1e-9, Took);
			std::printf("cost at 48 kHz: %.0f s of music in %.2f s = %.0fx real time (%.2f %% of a core)\n", Audio, Took, Speed, 100.0 / Speed);
			Check(Speed >= 30.0, "renders 30x real time or faster at 48 kHz (" + std::to_string(Speed) + "x)");
		}
		std::printf("%d checks, %d failed (%.0f s of music rendered)\n", Checks, Fails, TotalAudio);
		return Fails ? 1 : 0;
	}
}

int main(int Argc, char** Argv)
{
	using namespace LyudBand;
	const std::string Path = "unreal/Lyudmilovsk/Content/Music/band_tunes.txt";
	std::ifstream In(Path);
	if (!In)
	{
		std::printf("FAIL no %s (run from the repository root)\n", Path.c_str());
		return 1;
	}
	std::stringstream Ss;
	Ss << In.rdbuf();
	std::vector<std::string> Errors;
	const FBook Book = Parse(Ss.str(), Errors);
	const std::string Cmd = Argc > 1 ? Argv[1] : "test";
	if (Cmd == "test")
	{
		return Test(Book, Errors);
	}
	for (const std::string& E : Errors)
	{
		std::printf("band_tunes.txt: %s\n", E.c_str());
	}
	if (Cmd == "wav" && Argc > 2)
	{
		FPlayer P(32000);
		for (const FTune& T : Book.Tunes)
		{
			if (Argc > 3 && T.Id != Argv[3])
			{
				continue;
			}
			const bool bSoft = Argc > 4 && std::string(Argv[4]) == "soft";
			const std::vector<float> A = RenderTune(P, T, Hash(HashString(T.Id), 1u) | 1u, bSoft);
			const std::string File = std::string(Argv[2]) + "/band_" + T.Id + (bSoft ? "_soft" : "") + ".wav";
			std::printf("%s %s (%.0f s, peak %.2f)\n", WriteWav(File, A, 32000) ? "wrote" : "FAILED", File.c_str(), (double)A.size() / 64000.0, P.Stats.Peak);
		}
		return 0;
	}
	if (Cmd == "stems" && Argc > 3)
	{
		const int Ix = Book.FindTune(Argv[3]);
		if (Ix < 0)
		{
			std::printf("no tune %s\n", Argv[3]);
			return 1;
		}
		const bool bSoft = Argc > 4 && std::string(Argv[4]) == "soft";
		const FTune& T = Book.Tunes[(size_t)Ix];
		static const char* Names[NumInst] = { "piano", "bass", "reed", "drums" };
		const uint32_t Seed = Hash(HashString(T.Id), 1u) | 1u;
		for (int I = 0; I < NumInst; ++I)
		{
			FPlayer P(32000);
			P.bDry = true;
			for (int J = 0; J < NumInst; ++J)
			{
				P.Solo[J] = J == I;
			}
			std::vector<FEvent> Log;
			P.Log = &Log;
			const std::vector<float> A = RenderTune(P, T, Seed, bSoft);
			WriteWav(std::string(Argv[2]) + "/stem_" + Names[I] + ".wav", A, 32000);
			if (I == 0)
			{
				std::ofstream Csv(std::string(Argv[2]) + "/notes.csv");
				Csv << "time,inst,midi,vel,len\n";
				for (const FEvent& E : Log)
				{
					if (E.Type == 1)
					{
						Csv << E.Time << "," << Names[(int)E.Inst] << "," << E.Midi << "," << E.Vel << "," << E.Len << "\n";
					}
				}
				std::ofstream BarsCsv(std::string(Argv[2]) + "/bars.csv");
				BarsCsv << "start,spb0,spb1,role,formbar,meter,swing\n";
				for (const FBar& B : P.BarPlan())
				{
					BarsCsv << B.Start << "," << B.Spb0 << "," << B.Spb1 << "," << RoleName(B.Role) << "," << B.FormBar << "," << T.Meter << "," << T.Swing << "\n";
				}
			}
		}
		std::printf("stems of %s in %s\n", T.Id.c_str(), Argv[2]);
		return 0;
	}
	if (Cmd == "set" && Argc > 2)
	{
		const int Ix = Book.FindSet(Argv[2]);
		if (Ix < 0)
		{
			std::printf("no set %s\n", Argv[2]);
			return 1;
		}
		const FSet& S = Book.Sets[(size_t)Ix];
		int Last = -1;
		for (double At = 0.0; At < SetCycleSeconds(Book, S); At += 1.0)
		{
			const FSetPos Pos = Locate(Book, S, At);
			if (Pos.Tune >= 0 && Pos.Slot != Last)
			{
				Last = Pos.Slot;
				const FTune& T = Book.Tunes[(size_t)Pos.Tune];
				std::printf("%02d:%02d  %-18s %4.0f s\n", (int)At / 60, (int)At % 60, T.Id.c_str(), S.bSoft ? T.SecondsSoft : T.Seconds);
			}
		}
		return 0;
	}
	std::printf("usage: bandtest test | wav <dir> [tune [soft]] | stems <dir> <tune> [soft] | set <id>\n");
	return 1;
}
