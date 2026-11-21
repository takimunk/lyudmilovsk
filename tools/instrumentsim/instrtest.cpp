// Offline checks of the instruments (docs/instruments.md): bash tools/instrumentsim/run.sh [wav <dir>]
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Music/InstrumentSynth.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Music/TunesModel.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

using namespace LyudInstr;
using namespace LyudTunes;

static int Fails = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Fails; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static std::vector<float> RenderNote(EInstrument I, int Midi, double Seconds, float Vel = 0.8f)
{
	FSynth S(32000);
	S.SetInstrument(I);
	S.NoteOn(Midi, Vel);
	std::vector<float> Out((size_t)(Seconds * 32000));
	S.Render(Out.data(), (int)Out.size());
	return Out;
}

/** The fundamental by normalised autocorrelation near the expected period, refined by a parabola. */
static double Pitch(const std::vector<float>& X, int From, int Len, double Expect)
{
	const double Rate = 32000.0;
	const int Lo = (int)(Rate / (Expect * 1.08)), Hi = (int)(Rate / (Expect / 1.08)) + 1;
	std::vector<double> R((size_t)Hi + 2, 0.0);
	for (int L = Lo - 1; L <= Hi + 1; ++L)
	{
		double S = 0.0, A = 0.0, B = 0.0;
		for (int I = From; I < From + Len; ++I)
		{
			S += (double)X[(size_t)I] * X[(size_t)(I + L)];
			A += (double)X[(size_t)I] * X[(size_t)I];
			B += (double)X[(size_t)(I + L)] * X[(size_t)(I + L)];
		}
		R[(size_t)L] = S / std::sqrt(A * B + 1e-30);
	}
	int Best = Lo;
	for (int L = Lo; L <= Hi; ++L)
	{
		if (R[(size_t)L] > R[(size_t)Best])
		{
			Best = L;
		}
	}
	const double Y0 = R[(size_t)Best - 1], Y1 = R[(size_t)Best], Y2 = R[(size_t)Best + 1];
	const double Den = Y0 - 2 * Y1 + Y2;
	const double Off = std::fabs(Den) > 1e-12 ? 0.5 * (Y0 - Y2) / Den : 0.0;
	return Rate / (Best + Off);
}

static void WriteWav(const std::string& Path, const std::vector<float>& X)
{
	std::ofstream F(Path, std::ios::binary);
	auto U32 = [&F](uint32_t V) { F.write((const char*)&V, 4); };
	auto U16 = [&F](uint16_t V) { F.write((const char*)&V, 2); };
	F.write("RIFF", 4); U32(36 + (uint32_t)X.size() * 2); F.write("WAVEfmt ", 8); U32(16); U16(1); U16(1); U32(32000); U32(64000); U16(2); U16(16);
	F.write("data", 4); U32((uint32_t)X.size() * 2);
	for (float V : X)
	{
		const int16_t S = (int16_t)std::max(-32767.f, std::min(32767.f, V * 32767.f));
		F.write((const char*)&S, 2);
	}
}

/** A tune played perfectly through the synth (the rhythm game's sound, without the game). */
static std::vector<float> RenderTune(const FTune& T, EInstrument I)
{
	FSynth S(32000);
	S.SetInstrument(I);
	const double Spb = T.SecondsPerBeat();
	const int Total = (int)((T.Beats * Spb + 2.5) * 32000);
	std::vector<float> Out((size_t)Total, 0.f);
	int Pos = 0;
	size_t Ni = 0, Ci = 0;
	std::vector<std::pair<int, int>> Offs;  // (sample, midi)
	while (Pos < Total)
	{
		const int Block = 256;
		const double Now = Pos / 32000.0;
		while (Ni < T.Notes.size() && T.Notes[Ni].Beat * Spb <= Now)
		{
			for (int M : T.Notes[Ni].Midi)
			{
				S.NoteOn(M, 0.75f);
				Offs.push_back({ Pos + (int)(T.Notes[Ni].Len * Spb * 0.95 * 32000), M });
			}
			++Ni;
		}
		while (I == EInstrument::Guitar && Ci < T.Chords.size() && T.Chords[Ci].Beat * Spb <= Now)
		{
			const std::vector<int> C = ChordNotes(T.Chords[Ci].Name);
			for (size_t K = 0; K < C.size(); ++K)
			{
				S.NoteOn(C[K], 0.35f, (int)(K * 0.012 * 32000));
			}
			++Ci;
		}
		for (auto& O : Offs)
		{
			if (O.first >= 0 && O.first <= Pos)
			{
				S.NoteOff(O.second);
				O.first = -1;
			}
		}
		const int N = std::min(Block, Total - Pos);
		S.Render(Out.data() + Pos, N);
		Pos += N;
	}
	return Out;
}

int main(int Argc, char** Argv)
{
	const bool bWav = Argc > 2 && std::string(Argv[1]) == "wav";
	// note names
	CHECK(NoteFromName("C4") == 60);
	CHECK(NoteFromName("A4") == 69);
	CHECK(NoteFromName("F#3") == 54);
	CHECK(NoteFromName("Bb5") == 82);
	CHECK(NoteFromName("H4") == -1);
	CHECK(NoteFromName("C") == -1);
	CHECK(NoteFromName("C44") == -1);
	CHECK(std::fabs(MidiHz(69) - 440.0) < 1e-9);

	// every note in tune: the piano within 4 cents (its second string is meant to be a hair sharp), the guitar within 3
	double WorstP = 0.0, WorstG = 0.0;
	int WorstPM = 0, WorstGM = 0;
	for (int M = 36; M <= 89; M += 1)
	{
		const std::vector<float> X = RenderNote(EInstrument::Piano, M, 0.6);
		const double F = MidiHz(M);
		const int Len = std::max(2048, (int)(32000 / F * 12));
		const double Got = Pitch(X, 3200, Len, F);
		const double Cents = 1200.0 * std::log2(Got / F);
		if (std::fabs(Cents) > std::fabs(WorstP))
		{
			WorstP = Cents;
			WorstPM = M;
		}
	}
	for (int M = 40; M <= 84; M += 1)
	{
		const std::vector<float> X = RenderNote(EInstrument::Guitar, M, 0.6);
		const double F = MidiHz(M);
		const int Len = std::max(2048, (int)(32000 / F * 12));
		const double Got = Pitch(X, 3200, Len, F);
		const double Cents = 1200.0 * std::log2(Got / F);
		if (std::fabs(Cents) > std::fabs(WorstG))
		{
			WorstG = Cents;
			WorstGM = M;
		}
	}
	std::printf("pitch: piano worst %+.2f cents (MIDI %d), guitar worst %+.2f cents (MIDI %d)\n", WorstP, WorstPM, WorstG, WorstGM);
	CHECK(std::fabs(WorstP) < 4.0);
	CHECK(std::fabs(WorstG) < 3.0);

	// levels: a note never clips, a ten-note chord neither; the piano's damper stops a let-go key; the guitar rings on
	{
		FSynth S(32000);
		for (int M : { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 })
		{
			S.NoteOn(M, 1.f);
		}
		std::vector<float> X(32000);
		S.Render(X.data(), (int)X.size());
		float Peak = 0.f;
		bool bFinite = true;
		for (float V : X)
		{
			Peak = std::max(Peak, std::fabs(V));
			bFinite = bFinite && std::isfinite(V);
		}
		CHECK(bFinite);
		CHECK(Peak < 1.f);
		CHECK(Peak > 0.1f);
		for (int M : { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 })
		{
			S.NoteOff(M);
		}
		S.Render(X.data(), 16000);
		CHECK(S.ActiveVoices() == 0);
		S.NoteOn(60, 0.8f);
		S.SetSustain(true);
		S.NoteOff(60);
		S.Render(X.data(), 16000);
		CHECK(S.ActiveVoices() == 1);   // the pedal holds it
		S.SetSustain(false);
		S.Render(X.data(), 16000);
		CHECK(S.ActiveVoices() == 0);
		S.SetInstrument(EInstrument::Guitar);
		S.NoteOn(45, 0.8f);
		S.NoteOff(45);
		S.Render(X.data(), 16000);
		CHECK(S.ActiveVoices() == 1);   // a plucked string rings past the key
		S.Mute(45);
		S.Render(X.data(), 32000);
		CHECK(S.ActiveVoices() == 0);
		// a voice for every key: more notes than voices steal the oldest, nothing breaks
		S.SetInstrument(EInstrument::Piano);
		for (int M = 40; M < 80; ++M)
		{
			S.NoteOn(M, 0.5f);
		}
		CHECK(S.ActiveVoices() <= 14);
	}
	// the cost: a second of ten piano notes
	{
		FSynth S(32000);
		for (int M : { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84 })
		{
			S.NoteOn(M, 0.9f);
		}
		std::vector<float> X(512);
		const auto A = std::chrono::steady_clock::now();
		for (int I = 0; I < 62; ++I)
		{
			S.Render(X.data(), 512);
		}
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - A).count();
		std::printf("cost: 1 s of a ten-note piano chord in %.2f ms\n", Ms);
		CHECK(Ms < 60.0);
	}

	// the tunes
	std::ifstream F("../../unreal/Lyudmilovsk/Content/Music/tunes.txt");
	std::stringstream Buf;
	Buf << F.rdbuf();
	std::vector<std::string> Errors;
	const std::vector<FTune> Tunes = Parse(Buf.str(), Errors);
	for (const std::string& E : Errors)
	{
		std::printf("tunes.txt: %s\n", E.c_str());
	}
	CHECK(Errors.empty());
	CHECK(Tunes.size() >= 10);
	std::set<std::string> Ids;
	int Starters = 0, Guitar = 0, Sing = 0;
	for (const FTune& T : Tunes)
	{
		CHECK(Ids.insert(T.Id).second);
		Starters += T.bStart ? 1 : 0;
		Guitar += T.bGuitar ? 1 : 0;
		Sing += T.bSing ? 1 : 0;
		// in the ranges the instruments have, and not too long to play
		for (const FNote& N : T.Notes)
		{
			for (int M : N.Midi)
			{
				CHECK(M >= 48 && M <= 88);
			}
		}
		const double Secs = T.Beats * T.SecondsPerBeat();
		CHECK(Secs > 8.0 && Secs < 90.0);
		// the chords (when there are any) last as long as the melody, within a bar
		if (!T.Chords.empty())
		{
			const double CB = T.Chords.back().Beat + T.Chords.back().Len;
			if (std::fabs(CB - T.Beats) > 4.0)
			{
				std::printf("%s: chords last %.1f beats, the melody %.1f\n", T.Id.c_str(), CB, T.Beats);
			}
			CHECK(std::fabs(CB - T.Beats) <= 4.0);
		}
		// notes never come faster than a player can press (a quarter of a second apart at least... a sixth at the most)
		double Fastest = 1e9;
		double Last = -1e9;
		for (const FNote& N : T.Notes)
		{
			if (N.Midi.empty())
			{
				continue;
			}
			const double At = N.Beat * T.SecondsPerBeat();
			Fastest = std::min(Fastest, At - Last);
			Last = At;
		}
		if (Fastest < 0.15)
		{
			std::printf("%s: two notes %.3f s apart\n", T.Id.c_str(), Fastest);
		}
		CHECK(Fastest >= 0.15);
		std::printf("tune %-15s %5.1f s, %3d notes, fastest %.2f s apart%s%s%s\n", T.Id.c_str(), Secs, T.NumPlayed(), Fastest,
			T.bStart ? ", known from the start" : "", T.bGuitar ? ", guitar" : "", T.bSing ? ", sung" : "");
		if (bWav)
		{
			WriteWav(std::string(Argv[2]) + "/" + T.Id + "_piano.wav", RenderTune(T, EInstrument::Piano));
			if (T.bGuitar)
			{
				WriteWav(std::string(Argv[2]) + "/" + T.Id + "_guitar.wav", RenderTune(T, EInstrument::Guitar));
			}
		}
	}
	CHECK(Starters >= 2);
	CHECK(Guitar >= 6);
	CHECK(Sing >= 4);
	// bad input is told, not swallowed
	{
		std::vector<std::string> E2;
		Parse("[x]\nfor = piano\nstart = yes\nnotes = C4 C4 Q4 C4 C4 C4 C4 C4 C4\nchords = Xm/4\nfoo = 1\n", E2);
		CHECK(E2.size() == 3);
	}

	// the rhythm game
	{
		const FTune* T = &Tunes[0];
		FRhythm R;
		R.Start(*T, 10.0);
		CHECK(R.T0 > 10.0);
		EHit H;
		// a press in the lead-in plays nothing
		CHECK(R.Press(10.1, H) == -1 && H == EHit::Stray);
		// every note dead on: all perfect, three stars
		for (size_t I = 0; I < T->Notes.size(); ++I)
		{
			if (!T->Notes[I].Midi.empty())
			{
				const int Got = R.Press(R.TimeOf((int)I) + 0.01, H);
				CHECK(Got == (int)I && H == EHit::Perfect);
			}
		}
		R.Update(R.EndTime() + 1.0);
		CHECK(R.bDone);
		CHECK(R.Perfect == T->NumPlayed());
		CHECK(R.Stars() == 3);
		// late presses are good, very late ones miss; nothing pressed at all: every note missed, no stars
		FRhythm R2;
		R2.Start(*T, 0.0);
		CHECK(R2.Press(R2.TimeOf(0) + 0.12, H) == 0 && H == EHit::Good);
		CHECK(R2.Press(R2.TimeOf(1) + 0.30, H) == -1);
		R2.Update(R2.EndTime() + 1.0);
		CHECK(R2.bDone);
		CHECK(R2.Good == 1 && R2.Miss == T->NumPlayed() - 1);
		CHECK(R2.Stars() == 0);
		// an early press takes the coming note, not the next but one
		FRhythm R3;
		R3.Start(*T, 0.0);
		CHECK(R3.Press(R3.TimeOf(0) - 0.1, H) == 0 && H == EHit::Good);
		CHECK(R3.Press(R3.TimeOf(1) - 0.03, H) == 1 && H == EHit::Perfect);
	}

	std::printf("%d checks, %d failed\n", Checks, Fails);
	return Fails ? 1 : 0;
}
