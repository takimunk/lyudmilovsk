#pragma once

// The instruments the hero plays (docs/instruments.md): an upright piano and a six-string guitar, synthesised as they play
// (no recordings, nothing paid). Plain C++ (no engine types) so tools/instrumentsim can render notes and check their pitch.
//
//   LyudInstr::FSynth S(32000);
//   S.SetInstrument(LyudInstr::EInstrument::Piano);
//   S.NoteOn(60, 0.8f);            // middle C
//   S.Render(Buffer, 512);         // mono floats, about -1..1
//   S.NoteOff(60);                 // the damper falls (unless the sustain pedal is down)
//
// Piano: a hammer's thump, then up to 14 partials per note, slightly stretched (string stiffness), each dying away faster the
// higher it is, the first three doubled by a second string a hair out of tune (the slow beat of a real piano), in two stages
// (the prompt sound and the long aftersound). Each partial is a rotating phasor (no sin per sample).
// Guitar: Karplus-Strong, a delay line one period long fed by a pluck of shaped noise, its fractional length tuned with a
// first-order allpass (so every fret is in tune), a soft loop filter, and a little body resonance.

#include <cmath>
#include <cstdint>
#include <vector>

namespace LyudInstr
{
	enum class EInstrument : uint8_t { Piano, Guitar };

	/** Equal temperament, A4 = 440 Hz. */
	inline double MidiHz(int Midi) { return 440.0 * std::pow(2.0, (Midi - 69) / 12.0); }

	/** "C4", "F#3", "Bb5" -> MIDI number (C4 = 60); -1 if it is not a note. */
	inline int NoteFromName(const char* S)
	{
		if (!S || !*S)
		{
			return -1;
		}
		static const int Base[7] = { 9, 11, 0, 2, 4, 5, 7 };  // A B C D E F G
		char C = *S;
		if (C >= 'a' && C <= 'g')
		{
			C = (char)(C - 'a' + 'A');
		}
		if (C < 'A' || C > 'G')
		{
			return -1;
		}
		int N = Base[C - 'A'];
		++S;
		if (*S == '#')
		{
			++N;
			++S;
		}
		else if (*S == 'b')
		{
			--N;
			++S;
		}
		if (*S < '0' || *S > '8')
		{
			return -1;
		}
		const int Oct = *S - '0';
		++S;
		if (*S != 0)
		{
			return -1;
		}
		return 12 * (Oct + 1) + N;
	}

	class FSynth
	{
	public:
		explicit FSynth(int SampleRate = 32000) : Rate(SampleRate > 8000 ? SampleRate : 8000) { Voices.resize(MaxVoices); }

		void SetInstrument(EInstrument I)
		{
			if (I != Instrument)
			{
				AllOff(true);
				Instrument = I;
			}
		}
		EInstrument GetInstrument() const { return Instrument; }
		int SampleRate() const { return Rate; }

		/** Velocity 0..1. Delay: samples from the next Render (a strum's strings come a few ms apart). */
		void NoteOn(int Midi, float Velocity, int Delay = 0)
		{
			if (Midi < 21 || Midi > 108)
			{
				return;
			}
			FVoice& V = Steal(Midi);
			V = FVoice();
			V.bOn = true;
			V.bHeld = true;
			V.Midi = Midi;
			V.Vel = Velocity < 0.05f ? 0.05f : (Velocity > 1.f ? 1.f : Velocity);
			V.Wait = Delay > 0 ? Delay : 0;
			V.Order = ++Counter;
			if (Instrument == EInstrument::Piano)
			{
				StartPiano(V);
			}
			else
			{
				StartGuitar(V);
			}
		}

		/** The key let go: the piano's damper falls (unless the pedal holds it); a guitar string rings on (Mute stops it). */
		void NoteOff(int Midi)
		{
			if (Instrument == EInstrument::Guitar)
			{
				return;
			}
			Mute(Midi);
		}

		/** A finger on the string / the damper now, whatever the instrument. */
		void Mute(int Midi)
		{
			for (FVoice& V : Voices)
			{
				if (V.bOn && V.bHeld && V.Midi == Midi)
				{
					V.bHeld = false;
				}
			}
		}

		/** The piano's right pedal: while down, keys let go keep sounding. */
		void SetSustain(bool bDown) { bSustain = bDown; }
		bool IsSustain() const { return bSustain; }

		/** Everything stops (bNow) or is let go. */
		void AllOff(bool bNow)
		{
			for (FVoice& V : Voices)
			{
				V.bHeld = false;
				if (bNow)
				{
					V.bOn = false;
				}
			}
			if (bNow)
			{
				Body1 = Body2 = Body3 = Body4 = 0.f;
			}
		}

		int ActiveVoices() const
		{
			int N = 0;
			for (const FVoice& V : Voices)
			{
				N += V.bOn ? 1 : 0;
			}
			return N;
		}

		/** Adds nothing: writes Count mono samples. */
		void Render(float* Out, int Count)
		{
			for (int I = 0; I < Count; ++I)
			{
				Out[I] = 0.f;
			}
			for (FVoice& V : Voices)
			{
				if (!V.bOn)
				{
					continue;
				}
				if (Instrument == EInstrument::Piano)
				{
					RenderPiano(V, Out, Count);
				}
				else
				{
					RenderGuitar(V, Out, Count);
				}
			}
			if (Instrument == EInstrument::Guitar)
			{
				// the box: two soft resonances (the air and the top) under the strings
				for (int I = 0; I < Count; ++I)
				{
					const float X = Out[I];
					Body1 += BodyK1 * (X - Body1 - BodyQ1 * Body2);
					Body2 += BodyK1 * Body1;
					Body3 += BodyK2 * (X - Body3 - BodyQ2 * Body4);
					Body4 += BodyK2 * Body3;
					Out[I] = X + 0.12f * Body1 + 0.06f * Body3;
				}
			}
			for (int I = 0; I < Count; ++I)
			{
				// a soft limiter: chords never clip
				const float X = Out[I] * Gain;
				Out[I] = X / (1.f + std::fabs(X));
			}
		}

		float Gain = 0.8f;

	private:
		static constexpr int MaxVoices = 14;
		static constexpr int MaxPartials = 14;

		struct FPartial
		{
			float Re = 1.f, Im = 0.f;    // the phasor
			float Cr = 1.f, Ci = 0.f;    // one sample's turn
			float Amp = 0.f;
			float Fast = 0.f, Slow = 0.f;  // the two stages' levels
			float FastK = 1.f, SlowK = 1.f; // their per-sample decay
		};

		struct FVoice
		{
			bool bOn = false, bHeld = false;
			int Midi = 60;
			float Vel = 0.7f;
			int Wait = 0;
			unsigned Order = 0;
			float Release = 1.f;   // the damper's level: 1 open, falls once the key is let go
			float Age = 0.f;       // samples sounded
			// piano
			int NumPartials = 0;
			FPartial P[MaxPartials + 3];
			float Thump = 0.f, ThumpLp = 0.f;
			uint32_t Seed = 1;
			// guitar
			std::vector<float> Line;
			int Pos = 0;
			float Ap = 0.f, ApPrev = 0.f, ApC = 0.f;
			float LoopPrev = 0.f, Loss = 0.996f, Bright = 0.5f;
			float Level = 0.f;
		};

		int Rate;
		EInstrument Instrument = EInstrument::Piano;
		std::vector<FVoice> Voices;
		unsigned Counter = 0;
		bool bSustain = false;
		float Body1 = 0.f, Body2 = 0.f, Body3 = 0.f, Body4 = 0.f;
		float BodyK1 = 0.02f, BodyQ1 = 0.06f, BodyK2 = 0.04f, BodyQ2 = 0.08f;

		static float Noise(uint32_t& S)
		{
			S = S * 1664525u + 1013904223u;
			return (float)((S >> 9) & 0x7fff) / 16383.5f - 1.f;
		}

		FVoice& Steal(int Midi)
		{
			// the same key struck again takes its own voice (a piano string is struck again, a guitar string re-plucked)
			for (FVoice& V : Voices)
			{
				if (V.bOn && V.Midi == Midi)
				{
					return V;
				}
			}
			for (FVoice& V : Voices)
			{
				if (!V.bOn)
				{
					return V;
				}
			}
			// else the quietest of the oldest half
			FVoice* Pick = &Voices[0];
			for (FVoice& V : Voices)
			{
				if (V.Order < Pick->Order)
				{
					Pick = &V;
				}
			}
			return *Pick;
		}

		void SetPhasor(FPartial& P, double Hz)
		{
			const double W = 2.0 * 3.14159265358979323846 * Hz / Rate;
			P.Cr = (float)std::cos(W);
			P.Ci = (float)std::sin(W);
			P.Re = 1.f;
			P.Im = 0.f;
		}

		void StartPiano(FVoice& V)
		{
			const double F = MidiHz(V.Midi);
			// stiffer, shorter strings up the keyboard stretch their partials more
			const double B = 0.00008 * std::pow(F / 261.6, 1.0);
			// how long the note rings: bass strings for many seconds, the top only a moment
			const double Ring = std::fmin(9.0, std::fmax(0.9, 7.0 * std::pow(261.6 / F, 0.65)));
			const double Bright = 0.55 + 0.45 * V.Vel;  // a hard blow brings out the upper partials
			V.Seed = 0x9E3779B9u ^ (uint32_t)(V.Midi * 7919);
			int N = 0;
			for (int K = 1; K <= MaxPartials; ++K)
			{
				const double Hz = K * F * std::sqrt(1.0 + B * K * K);
				if (Hz > Rate * 0.42)
				{
					break;
				}
				FPartial& P = V.P[N++];
				SetPhasor(P, Hz);
				// the partials' spectrum: falling with K, struck nearer the end of the string the brighter
				P.Amp = (float)(std::pow(Bright, K - 1) / std::pow((double)K, 0.9) * (K == 1 ? 1.0 : 0.8));
				const double TFast = Ring * 0.12 / (1.0 + 0.5 * (K - 1));
				const double TSlow = Ring / (1.0 + 0.35 * (K - 1));
				P.Fast = 0.65f;
				P.Slow = 0.35f;
				P.FastK = (float)std::exp(-1.0 / (TFast * Rate));
				P.SlowK = (float)std::exp(-1.0 / (TSlow * Rate));
			}
			// the second string of the first partials, a hair sharp: the beat
			for (int K = 1; K <= 3 && K <= N; ++K)
			{
				FPartial& P = V.P[N + K - 1];
				P = V.P[K - 1];
				const double Hz = K * F * std::sqrt(1.0 + B * K * K) * std::pow(2.0, (0.9 + 0.3 * K) / 1200.0);
				SetPhasor(P, Hz);
				P.Amp *= 0.7f;
			}
			V.NumPartials = N;
			V.Thump = 1.f;
		}

		void RenderPiano(FVoice& V, float* Out, int Count)
		{
			const int Total = V.NumPartials + (V.NumPartials >= 3 ? 3 : V.NumPartials);
			const float Level = 0.22f * V.Vel;
			const float Damp = (float)std::exp(-1.0 / (0.05 * Rate));      // the felt damper: gone in a few tenths
			const float ThumpK = (float)std::exp(-1.0 / (0.006 * Rate));
			const float LpK = (float)std::fmin(0.9, 2.0 * 3.14159 * (300.0 + 900.0 * V.Vel) / Rate);
			for (int I = 0; I < Count; ++I)
			{
				if (V.Wait > 0)
				{
					--V.Wait;
					continue;
				}
				float S = 0.f;
				for (int J = 0; J < Total; ++J)
				{
					FPartial& P = V.P[J];
					const float Re = P.Re * P.Cr - P.Im * P.Ci;
					const float Im = P.Re * P.Ci + P.Im * P.Cr;
					P.Re = Re;
					P.Im = Im;
					P.Fast *= P.FastK;
					P.Slow *= P.SlowK;
					S += Im * P.Amp * (P.Fast + P.Slow);
				}
				// the hammer: a short dull knock
				if (V.Thump > 0.001f)
				{
					V.ThumpLp += LpK * (Noise(V.Seed) - V.ThumpLp);
					S += V.ThumpLp * V.Thump * 0.6f;
					V.Thump *= ThumpK;
				}
				if (!V.bHeld && !bSustain)
				{
					V.Release *= Damp;
				}
				Out[I] += S * Level * V.Release;
				V.Age += 1.f;
			}
			// keep the phasors on the unit circle (float drift over seconds)
			for (int J = 0; J < Total; ++J)
			{
				FPartial& P = V.P[J];
				const float M = 1.f / std::sqrt(P.Re * P.Re + P.Im * P.Im);
				P.Re *= M;
				P.Im *= M;
			}
			const float Left = V.P[0].Fast + V.P[0].Slow;
			if (V.Release < 0.002f || Left < 0.0004f)
			{
				V.bOn = false;
			}
		}

		void StartGuitar(FVoice& V)
		{
			const double F = MidiHz(V.Midi);
			// the loop: N whole samples, the loop filter's delay and an allpass for the rest, all measured at this
			// note's own frequency (the brighter the pluck the less the loop filter delays), so every fret is in tune
			const double Period = Rate / F;
			const double W = 2.0 * 3.14159265358979323846 * F / Rate;
			V.Bright = 0.35f + 0.4f * V.Vel;
			const double A = 0.5 + 0.5 * (V.Bright - 0.5);  // the loop filter: A x[n] + (1 - A) x[n-1]
			const double LoopDelay = -std::atan2(-(1.0 - A) * std::sin(W), A + (1.0 - A) * std::cos(W)) / W;
			int N = (int)std::floor(Period - LoopDelay - 0.15);
			if (N < 2)
			{
				N = 2;
			}
			const double Want = Period - LoopDelay - N;  // the allpass's phase delay at W
			auto ApDelay = [W](double C)
			{
				// H(z) = (C + z^-1) / (1 + C z^-1): its phase delay at W
				const double Ph = std::atan2(-std::sin(W), C + std::cos(W)) - std::atan2(-C * std::sin(W), 1.0 + C * std::cos(W));
				return -Ph / W;
			};
			double Lo = -0.95, Hi = 0.95;  // the delay falls as C rises
			for (int It = 0; It < 50; ++It)
			{
				const double Mid = 0.5 * (Lo + Hi);
				if (ApDelay(Mid) > Want)
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid;
				}
			}
			V.ApC = (float)(0.5 * (Lo + Hi));
			V.Line.assign((size_t)N, 0.f);
			V.Pos = 0;
			V.Ap = V.ApPrev = 0.f;
			V.LoopPrev = 0.f;
			// how long it rings (the low strings longest) and how bright the pluck is (harder: brighter)
			const double Ring = std::fmin(6.0, std::fmax(1.2, 4.5 * std::pow(110.0 / F, 0.5)));
			V.Loss = (float)std::pow(0.001, 1.0 / (Ring * F));  // -60 dB after Ring seconds
			// the pluck: noise, low-passed by how hard, and a comb for where the pick struck (a fifth of the way along)
			V.Seed = 0x2545F491u ^ (uint32_t)(V.Midi * 104729);
			float Lp = 0.f;
			std::vector<float> Exc((size_t)N);
			for (int I = 0; I < N; ++I)
			{
				Lp += (0.25f + 0.6f * V.Vel) * (Noise(V.Seed) - Lp);
				Exc[(size_t)I] = Lp;
			}
			const int Pick = N / 5 > 0 ? N / 5 : 1;
			float Mean = 0.f;
			for (int I = 0; I < N; ++I)
			{
				const float X = Exc[(size_t)I] - (I >= Pick ? Exc[(size_t)(I - Pick)] : 0.f);
				V.Line[(size_t)I] = X;
				Mean += X;
			}
			Mean /= (float)N;
			float Peak = 1e-6f;
			for (float& X : V.Line)
			{
				X -= Mean;
				Peak = std::fmax(Peak, std::fabs(X));
			}
			for (float& X : V.Line)
			{
				X /= Peak;
			}
			V.Level = 0.34f * V.Vel;
		}

		void RenderGuitar(FVoice& V, float* Out, int Count)
		{
			const int N = (int)V.Line.size();
			if (N < 2)
			{
				V.bOn = false;
				return;
			}
			// a finger laid on the string: it dies in a few tenths
			const float Mute = (float)std::exp(-1.0 / (0.06 * Rate));
			const float B = V.Bright;
			float Energy = 0.f;
			for (int I = 0; I < Count; ++I)
			{
				if (V.Wait > 0)
				{
					--V.Wait;
					continue;
				}
				const float X = V.Line[(size_t)V.Pos];
				// the loop filter: the averaging of two samples (half a sample late), leaning to the newer one when bright
				const float Avg = (0.5f + 0.5f * (B - 0.5f)) * X + (0.5f - 0.5f * (B - 0.5f)) * V.LoopPrev;  // as StartGuitar's A
				V.LoopPrev = X;
				// the allpass: the fraction of a sample the loop still needs
				const float Y = V.ApC * Avg + V.ApPrev - V.ApC * V.Ap;
				V.ApPrev = Avg;
				V.Ap = Y;
				V.Line[(size_t)V.Pos] = Y * V.Loss;
				V.Pos = V.Pos + 1 < N ? V.Pos + 1 : 0;
				if (!V.bHeld)
				{
					V.Release *= Mute;
				}
				Out[I] += X * V.Level * V.Release;
				Energy += X * X;
				V.Age += 1.f;
			}
			if (V.Age > Rate * 0.05f && (Energy / (float)(Count > 0 ? Count : 1) < 1e-8f || V.Release < 0.002f))
			{
				V.bOn = false;
			}
		}
	};
}
