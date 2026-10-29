#include "AmbienceDSP.h"

#include <algorithm>
#include <cmath>

namespace LyudAudio
{
	namespace
	{
		float Clamp(float V, float A, float B) { return std::min(std::max(V, A), B); }
		float SmoothStep(float A, float B, float X)
		{
			const float T = Clamp((X - A) / (B - A), 0.f, 1.f);
			return T * T * (3.f - 2.f * T);
		}
		/** Coefficients are refreshed every this many samples (filter sweeps are slow next to it). */
		constexpr int Block = 32;
	}

	float FPink::Process(float W)
	{
		B[0] = 0.99886f * B[0] + W * 0.0555179f;
		B[1] = 0.99332f * B[1] + W * 0.0750759f;
		B[2] = 0.96900f * B[2] + W * 0.1538520f;
		B[3] = 0.86650f * B[3] + W * 0.3104856f;
		B[4] = 0.55000f * B[4] + W * 0.5329522f;
		B[5] = -0.7616f * B[5] - W * 0.0168980f;
		const float Out = B[0] + B[1] + B[2] + B[3] + B[4] + B[5] + B[6] + W * 0.5362f;
		B[6] = W * 0.115926f;
		return Out * 0.11f;
	}

	void FSvf::Set(float CutoffHz, float Q, float SampleRate)
	{
		const float F = Clamp(CutoffHz, 10.f, SampleRate * 0.45f);
		G = std::tan(Pi * F / SampleRate);
		K = 1.f / std::max(Q, 0.05f);
		A1 = 1.f / (1.f + G * (G + K));
		A2 = G * A1;
		A3 = G * A2;
	}

	void FSvf::Process(float X)
	{
		const float V3 = X - Ic2;
		const float V1 = A1 * Ic1 + A2 * V3;
		const float V2 = Ic2 + A2 * Ic1 + A3 * V3;
		Ic1 = 2.f * V1 - Ic1;
		Ic2 = 2.f * V2 - Ic2;
		Low = V2;
		Band = V1;
		High = X - K * V1 - V2;
	}

	void FSmooth::Init(float Seconds, float SampleRate, float Start)
	{
		Coef = 1.f - std::exp(-1.f / std::max(Seconds * SampleRate, 1.f));
		Y = Start;
	}

	float FDrift::Step(float RateHz, float SampleRate)
	{
		T += RateHz / SampleRate;
		if (T >= 1.f)
		{
			T -= 1.f;
			A = B; B = C; C = D; D = R.White();
		}
		// Catmull-Rom through the last four points: smooth, no overshoot worth minding
		const float T2 = T * T, T3 = T2 * T;
		return Clamp(0.5f * (2.f * B + (-A + C) * T + (2.f * A - 5.f * B + 4.f * C - D) * T2 + (-A + 3.f * B - 3.f * C + D) * T3), -1.f, 1.f);
	}

	// ---------------------------------------------------------------------------------------------- wind

	void FWindVoice::Init(float SampleRate, uint32_t Seed)
	{
		Rate = SampleRate;
		NL = FRng(Seed * 7 + 1); NR = FRng(Seed * 7 + 2); NW = FRng(Seed * 7 + 3);
		Turb1 = FDrift(Seed * 7 + 4); Turb2 = FDrift(Seed * 7 + 5); Turb3 = FDrift(Seed * 7 + 6);
		WhistleDrift = FDrift(Seed * 7 + 7); Buffet = FDrift(Seed * 7 + 8);
		// a block's step is Block samples, so the smoothing runs on the block clock
		SSpeed.Init(1.5f, SampleRate / Block);
		SGust.Init(1.0f, SampleRate / Block, 0.25f);
		SPan.Init(0.5f, SampleRate / Block);
		SAir.Init(0.25f, SampleRate / Block);
		Counter = 0;
	}

	void FWindVoice::Render(float* L, float* R, int Num, float Speed, float Gustiness, float Pan, float AirSpeed, float Gain)
	{
		float Level = LastLevel, Whistle = 0.f, Rush = 0.f, Hiss = 0.f, PanL = 1.f, PanR = 1.f, Turb = 1.f;
		for (int I = 0; I < Num; I++)
		{
			if (Counter-- <= 0)
			{
				Counter = Block - 1;
				const float S = SSpeed.Step(std::max(Speed, 0.f));
				const float G = SGust.Step(Clamp(Gustiness, 0.f, 0.6f));
				const float P = SPan.Step(Clamp(Pan, -1.f, 1.f));
				const float A = SAir.Step(std::max(AirSpeed, 0.f));
				const float BlockRate = Rate / Block;
				// the gusts at the ear: slow swells, quicker eddies, a flutter
				Turb = 1.f + 1.5f * G * (0.6f * Turb1.Step(0.11f, BlockRate) + 0.4f * Turb2.Step(0.45f, BlockRate)) + 0.25f * G * Turb3.Step(2.2f, BlockRate);
				const float Se = std::max(S * Turb, 0.f);
				Level = 0.085f * std::pow(Clamp((Se - 0.4f) / 11.f, 0.f, 1.8f), 1.4f);
				BodyL.Set(140.f + 26.f * Se, 0.9f, Rate); BodyR.Set(150.f + 27.f * Se, 0.9f, Rate);
				AirL.Set(650.f + 115.f * Se, 0.7f, Rate); AirR.Set(690.f + 112.f * Se, 0.7f, Rate);
				RumbleL.Set(70.f + 7.f * Se, 0.7f, Rate); RumbleR.Set(72.f + 7.f * Se, 0.7f, Rate);
				// aeolian tones of wires (~5 mm) and edges (~2 cm): f = 0.2 U / d, singing in a strong wind
				const float Wd = WhistleDrift.Step(0.07f, BlockRate);
				Whistle1.Set(std::max(40.f * Se * (1.f + 0.04f * Wd), 60.f), 45.f, Rate);
				Whistle2.Set(std::max(10.f * Se * (1.f - 0.05f * Wd) * 6.3f, 60.f), 30.f, Rate);
				Whistle = SmoothStep(8.f, 16.f, Se) * (0.55f + 0.45f * Wd);
				// flight: buffeting round the head, hiss of the air past the feathers
				Rush = std::min(std::pow(A / 16.f, 2.f), 2.5f) * (0.65f + 0.35f * Buffet.Step(9.f, BlockRate));
				Hiss = std::min(std::pow(A / 22.f, 2.f), 2.f);
				RushLow.Set(120.f + 6.f * A, 0.8f, Rate);
				RushHigh.Set(2200.f + 60.f * A, 0.6f, Rate);
				// the ear facing the wind hears more of it
				PanL = 1.f - 0.35f * P;
				PanR = 1.f + 0.35f * P;
			}
			const float WL = NL.White(), WR = NR.White();
			const float Pl = PL.Process(WL), Pr = PR.Process(WR);
			const float Bl = BL.Process(WL), Br = BR.Process(WR);
			BodyL.Process(Pl); BodyR.Process(Pr);
			AirL.Process(WL); AirR.Process(WR);
			RumbleL.Process(Bl); RumbleR.Process(Br);
			float OutL = Level * (2.2f * RumbleL.Low + 1.6f * BodyL.Band + 0.10f * AirL.Band);
			float OutR = Level * (2.2f * RumbleR.Low + 1.6f * BodyR.Band + 0.10f * AirR.Band);
			if (Whistle > 1e-4f)
			{
				const float Wn = NW.White();
				Whistle1.Process(Wn);
				Whistle2.Process(Wn);
				const float Wv = Whistle * (0.05f * Whistle1.Band + 0.03f * Whistle2.Band);
				OutL += Wv * (1.f - 0.3f * SPan.Y);
				OutR += Wv * (1.f + 0.3f * SPan.Y);
			}
			if (Rush > 1e-4f || Hiss > 1e-4f)
			{
				const float Wn = NW.White();
				RushLow.Process(BRush.Process(Wn));
				RushHigh.Process(Wn);
				const float V = 0.2f * Rush * RushLow.Low + 0.012f * Hiss * RushHigh.High;
				OutL += V;
				OutR += V * 0.92f;
			}
			L[I] += Gain * OutL * PanL;
			R[I] += Gain * OutR * PanR;
		}
		LastLevel = Level / 0.085f + Rush;
	}

	// ---------------------------------------------------------------------------------------------- the bed

	void FAmbience::Init(float SampleRate, uint32_t Seed)
	{
		Rate = SampleRate;
		R = FRng(Seed);
		WindV.Init(SampleRate, Seed + 100);
		LeafHp.Set(1900.f, 0.6f, Rate);
		LeafHp2.Set(9000.f, 0.5f, Rate);
		NeedleBp.Set(4200.f, 0.5f, Rate);
		SlapF.Set(900.f, 1.2f, Rate);
		RoofL.Set(900.f, 0.6f, Rate); RoofR.Set(950.f, 0.6f, Rate);
		RainBedL.Set(2600.f, 0.45f, Rate); RainBedR.Set(2800.f, 0.45f, Rate);
		RainLowL.Set(500.f, 0.6f, Rate); RainLowR.Set(520.f, 0.6f, Rate);
		SEnc.Init(0.6f, SampleRate / Block);
		SRain.Init(4.f, SampleRate / Block);
		SLeaves.Init(2.f, SampleRate / Block);
		SCrick.Init(3.f, SampleRate / Block);
		SSurfH.Init(3.f, SampleRate / Block);
		SSurfD.Init(0.6f, SampleRate / Block, 1000.f);
		SSurfPan.Init(0.4f, SampleRate / Block);
		Grains.reserve(256);
		Breaks.resize(6);
		Strikes.resize(4);
		Crick.resize(10);
		for (size_t I = 0; I < Crick.size(); I++)
		{
			FCricket& C = Crick[I];
			C.Pan = R.White() * 0.9f;
			C.Freq = 4300.f + 700.f * R.Uniform();
			C.Clock = R.Uniform();
			C.RateJit = 0.9f + 0.2f * R.Uniform();
			C.Gain = 0.25f + 0.75f * R.Uniform() * R.Uniform();
		}
	}

	void FAmbience::AddGrain(int Kind, float Amp, float Pan, float Freq, float DecayS, float Q, int Wait)
	{
		if (Grains.size() >= 240)
		{
			return;
		}
		FGrain G;
		G.Kind = Kind;
		// a grain starts at a random sample of the block it was born in, so they never tick in step
		G.Wait = Wait + int(R.Uniform() * Block);
		G.Amp = Amp;
		G.Pan = Pan;
		G.Freq = Freq;
		G.Decay = std::exp(-1.f / std::max(DecayS * Rate, 1.f));
		if (Kind != 2)
		{
			G.F.Set(Freq, Q, Rate);
		}
		Grains.push_back(G);
	}

	void FAmbience::Strike(float DistanceM, float Pan)
	{
		for (FStrike& S : Strikes)
		{
			if (!S.bLive)
			{
				S = FStrike();
				S.bLive = true;
				S.Dist = std::max(DistanceM, 50.f);
				S.Delay = S.Dist / 343.f;
				S.Pan = Pan;
				S.T = 0.f;
				S.Roll = FDrift(R.Next());
				return;
			}
		}
	}

	void FAmbience::RenderLeaves(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		const float Wind = std::max(P.WindSpeed, 0.f);
		const float Target = Clamp(P.Leaves, 0.f, 1.f);
		const float BlockRate = Rate / Block;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (Block - 1)) == 0)
			{
				const float Lv = SLeaves.Step(Target);
				const float Swell = 0.6f + 0.4f * LeafSwell.Step(0.25f, BlockRate);
				// flutters per second in the crowns: a few in still air, thousands in a gale
				const float W = Wind * Swell;
				const float PerSec = Lv * (6.f + 9.f * W * W);
				const float Expected = PerSec * Block / Rate;
				int N = int(Expected);
				if (R.Uniform() < Expected - N)
				{
					N++;
				}
				const float Dry = Clamp(P.LeafDry, 0.f, 1.f);
				for (int K = 0; K < N; K++)
				{
					// loudness: most flutters faint, a few close and clear
					const float U = R.Uniform();
					const float Amp = 0.006f * (0.3f + std::pow(U, 6.f) * 3.f) * (1.f + 0.02f * W);
					AddGrain(0, Amp, R.White() * 0.95f, 2500.f + 4500.f * R.Uniform() + 2000.f * Dry, 0.006f + 0.02f * R.Uniform() * (1.f - 0.6f * Dry), 1.2f, I);
				}
				LeafHp.Set(1800.f + 700.f * Dry, 0.6f, Rate);
			}
		}
		(void)L; (void)R2;
	}

	void FAmbience::RenderSurf(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		const float BlockRate = Rate / Block;
		float H = SSurfH.Y, D = SSurfD.Y, Pan = SSurfPan.Y, Near = 1.f, Roar = 0.f, Single = 1.f;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (Block - 1)) == 0)
			{
				H = SSurfH.Step(Clamp(P.SurfHeight, 0.f, 4.f));
				D = SSurfD.Step(Clamp(P.SurfDistance, 1.f, 20000.f));
				Pan = SSurfPan.Step(Clamp(P.SurfPan, -1.f, 1.f));
				// a surf line is a long source: it fades as 1/sqrt(distance), the air takes its top first
				Near = std::sqrt(25.f / (25.f + D));
				// past a couple of hundred metres single waves melt into the roar of the whole shore
				Single = 1.f / (1.f + D / 150.f);
				const float Cut = 16000.f / (1.f + D / 120.f) + 250.f;
				RoarL.Set(std::min(Cut, 420.f + 180.f * H), 0.6f, Rate);
				RoarR.Set(std::min(Cut, 440.f + 170.f * H), 0.6f, Rate);
				Roar = std::pow(H, 0.9f) * std::sqrt(80.f / (80.f + D)) * (0.75f + 0.25f * RoarDrift.Step(1.f / std::max(P.SurfPeriod, 2.f), BlockRate));
				// the next wave breaks
				NextBreak -= Block / Rate;
				if (NextBreak <= 0.f && H > 0.03f)
				{
					const float Period = Clamp(P.SurfPeriod, 2.f, 14.f);
					NextBreak = Period * (0.8f + 0.4f * R.Uniform());
					SetPhase += 1.f / 7.f;
					const float Size = Clamp(0.65f + 0.35f * std::sin(2.f * Pi * SetPhase) + 0.2f * R.White(), 0.25f, 1.3f);
					for (FBreak& B : Breaks)
					{
						if (!B.bLive)
						{
							B = FBreak();
							B.bLive = true;
							B.Size = Size * std::pow(H, 0.8f);
							B.Crash = 0.25f + 0.5f * H;
							B.Wash = 2.f + 1.5f * std::min(H, 1.5f) + 0.8f * R.Uniform();
							break;
						}
					}
				}
				// close to calm water: little slaps on the stones and sand
				if (H < 0.3f && D < 60.f && R.Uniform() < (1.8f * Block / Rate) * (1.f - H / 0.3f))
				{
					SlapEnv = (0.4f + 0.6f * R.Uniform()) * (1.f - D / 60.f);
					SlapPan = Pan + 0.4f * R.White();
				}
				const float Cutoff = 12000.f / (1.f + D / 40.f) + 250.f;
				for (FBreak& B : Breaks)
				{
					if (B.bLive)
					{
						// the crash darkens as it rolls; the wash is bright and thins as it sinks into the sand
						const float Ct = std::min(Cutoff, 2800.f * std::exp(-B.T / 1.4f) + 450.f);
						B.F.Set(Ct, 0.7f, Rate);
						const float Wt = std::min(Cutoff, 5200.f - 900.f * std::min(B.T / B.Wash, 1.5f));
						B.Fw.Set(Wt, 0.5f, Rate);
					}
				}
			}
			float OutL = 0.f, OutR = 0.f;
			// the roar of all the breakers along the coast
			if (Roar > 1e-4f)
			{
				RoarL.Process(RoarPL.Process(R.White()));
				RoarR.Process(RoarPR.Process(R.White()));
				OutL += 0.55f * Roar * RoarL.Low;
				OutR += 0.55f * Roar * RoarR.Low;
			}
			float Mono = 0.f, Side = 0.f;
			for (FBreak& B : Breaks)
			{
				if (!B.bLive)
				{
					continue;
				}
				const float T = B.T;
				const float CrashEnv = (T < B.Crash ? std::pow(T / B.Crash, 2.f) : std::exp(-(T - B.Crash) / 0.9f));
				const float WashT = T - 0.6f;
				const float WashEnv = WashT <= 0.f ? 0.f : (WashT < 1.f ? WashT : std::exp(-(WashT - 1.f) / (0.45f * B.Wash)));
				const float W = R.White();
				B.F.Process(W);
				B.Fw.Process(R.White());
				Mono += Single * B.Size * (0.55f * CrashEnv * B.F.Low);
				Side += Single * B.Size * 0.22f * WashEnv * B.Fw.Band * (0.7f + 0.3f * R.Uniform());
				B.T += 1.f / Rate;
				if (T > B.Crash + B.Wash * 3.f + 2.f)
				{
					B.bLive = false;
				}
			}
			if (SlapEnv > 1e-4f)
			{
				SlapF.Process(R.White());
				const float S = 0.25f * SlapEnv * SlapF.Band;
				PanTo(S, SlapPan, 0.7f, OutL, OutR);
				SlapEnv *= 0.9993f;
			}
			// the breaking waves spread along the beach (wide), the wash is wider still
			PanTo(Mono, Pan, 0.55f, OutL, OutR);
			const float Dec = R.White() * 0.3f;
			PanTo(Side * (1.f + Dec), Pan - 0.3f, 0.5f, OutL, OutR);
			PanTo(Side * (1.f - Dec), Pan + 0.3f, 0.5f, OutL, OutR);
			L[I] += Near * OutL * P.Gain[Surf];
			R2[I] += Near * OutR * P.Gain[Surf];
		}
	}

	void FAmbience::RenderRain(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		float Rn = SRain.Y, Bed = 0.f, Low = 0.f, Roof = Clamp(P.RainRoof, 0.f, 1.f), Snow = Clamp(P.Snow, 0.f, 1.f);
		const float OnWater = Clamp(P.RainWater, 0.f, 1.f) * (1.f - Roof);
		for (int I = 0; I < Num; I++)
		{
			if ((I & (Block - 1)) == 0)
			{
				Rn = SRain.Step(std::max(P.Rain, 0.f));
				const float Water = Rn * (1.f - 0.92f * Snow);
				// rain on the sea hisses brighter and fuller than on the ground
				Bed = 0.15f * std::sqrt(Water) * (1.f + 0.45f * OnWater);
				Low = 0.10f * std::sqrt(std::max(Water - 1.f, 0.f));
				// single drops near the ears: ticks on the ground, patters on leaves, plips into puddles
				const float PerSec = std::min(40.f * Water, 900.f);
				const float Expected = PerSec * Block / Rate;
				int N = int(Expected);
				if (R.Uniform() < Expected - N)
				{
					N++;
				}
				for (int K = 0; K < N; K++)
				{
					const float U = R.Uniform();
					const float Amp = 0.03f * (0.2f + std::pow(R.Uniform(), 5.f) * 2.5f) * (1.f - 0.7f * Roof);
					if (U < 0.18f + 0.62f * OnWater)
					{
						// a small drop into water: the bubble rings (Minnaert) and rises
						AddGrain(2, Amp * 0.7f, R.White(), 1300.f + 2500.f * R.Uniform(), 0.012f + 0.02f * R.Uniform(), 0.f, I);
					}
					else if (U < 0.18f + 0.5f * P.Leaves)
					{
						AddGrain(1, Amp * 0.8f, R.White(), 1800.f + 2500.f * R.Uniform(), 0.008f + 0.01f * R.Uniform(), 1.5f, I);
					}
					else
					{
						AddGrain(1, Amp, R.White(), 3000.f + 4000.f * R.Uniform(), 0.002f + 0.004f * R.Uniform(), 0.9f, I);
					}
				}
			}
			if (Bed < 1e-5f)
			{
				continue;
			}
			const float WL = RainPL.Process(R.White()), WR = RainPR.Process(R.White());
			RainBedL.Process(WL); RainBedR.Process(WR);
			RainLowL.Process(WL); RainLowR.Process(WR);
			float OutL = Bed * RainBedL.Band + Low * RainLowL.Low;
			float OutR = Bed * RainBedR.Band + Low * RainLowR.Low;
			if (Roof > 0.f)
			{
				// on a roof overhead: the patter turns into a muffled drumming
				RoofL.Process(OutL * 1.5f + Bed * WL * 0.3f);
				RoofR.Process(OutR * 1.5f + Bed * WR * 0.3f);
				OutL = OutL * (1.f - Roof) + Roof * RoofL.Low;
				OutR = OutR * (1.f - Roof) + Roof * RoofR.Low;
			}
			L[I] += OutL * P.Gain[Rain];
			R2[I] += OutR * P.Gain[Rain];
		}
	}

	void FAmbience::RenderCrickets(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		const float Target = Clamp(P.Crickets, 0.f, 1.f) * SmoothStep(11.f, 15.f, P.TempC);
		// Dolbear: chirps per minute ~ 7 T - 30 (field crickets), a few degrees either way per singer
		const float PerMin = std::max(7.f * P.TempC - 30.f, 30.f);
		float Act = SCrick.Y;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (Block - 1)) == 0)
			{
				Act = SCrick.Step(Target);
			}
			if (Act < 1e-3f)
			{
				continue;
			}
			float OutL = 0.f, OutR = 0.f;
			const int Singers = int(std::ceil(Act * Crick.size()));
			for (int K = 0; K < Singers; K++)
			{
				FCricket& C = Crick[K];
				const float Fade = Clamp(Act * Crick.size() - K, 0.f, 1.f);
				const float Period = 60.f / (PerMin * C.RateJit);
				C.Clock += 1.f / Rate;
				if (C.Clock >= Period)
				{
					C.Clock -= Period;
				}
				// a chirp = 4 pulses of 15 ms, 32 ms apart, each a clean tone from the file and scraper
				const float Tp = C.Clock / 0.032f;
				const int Pulse = int(Tp);
				if (Pulse < 4)
				{
					const float X = (Tp - Pulse) * (0.032f / 0.015f);
					if (X < 1.f)
					{
						const float Env = std::sin(Pi * X);
						C.Phase += C.Freq / Rate;
						C.Phase -= std::floor(C.Phase);
						const float V = 0.05f * C.Gain * Fade * Env * Env * std::sin(2.f * Pi * C.Phase);
						PanTo(V, C.Pan, 1.f, OutL, OutR);
					}
				}
			}
			L[I] += OutL * P.Gain[Crickets];
			R2[I] += OutR * P.Gain[Crickets];
		}
	}

	void FAmbience::RenderThunder(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		for (FStrike& S : Strikes)
		{
			if (!S.bLive)
			{
				continue;
			}
			const float Length = 3.f + S.Dist / 500.f;
			const float Loud = 1.f / (1.f + S.Dist / 1200.f);
			for (int I = 0; I < Num; I++)
			{
				if ((I & (Block - 1)) == 0)
				{
					const float Since = S.T - S.Delay;
					S.F.Set(1500.f / (1.f + S.Dist / 600.f) + 70.f - std::max(Since, 0.f) * 30.f, 0.7f, Rate);
					S.Fc.Set(1200.f, 0.6f, Rate);
				}
				const float T = S.T - S.Delay;
				S.T += 1.f / Rate;
				if (T < 0.f)
				{
					continue;
				}
				// rumble: rolling swells (echoes off clouds and ground) inside an envelope that rises and fades
				const float Env = (T < 0.4f ? T / 0.4f : std::exp(-(T - 0.4f) / (0.35f * Length)));
				const float Roll = 0.4f + 0.6f * std::fabs(S.Roll.Step(1.6f, Rate));
				S.F.Process(ThunderB.Process(R.White()));
				float V = 2.0f * Loud * Env * Roll * S.F.Low;
				// the crack of a near strike
				if (S.Dist < 1500.f && T < 0.6f)
				{
					S.Fc.Process(R.White());
					V += 0.3f * (1.f - S.Dist / 1500.f) * std::exp(-T / 0.12f) * S.Fc.High;
				}
				float OutL = 0.f, OutR = 0.f;
				PanTo(V, S.Pan, 0.6f, OutL, OutR);
				L[I] += OutL * P.Gain[Thunder];
				R2[I] += OutR * P.Gain[Thunder];
				if (T > Length * 3.f)
				{
					S.bLive = false;
					break;
				}
			}
		}
	}

	void FAmbience::Render(float* L, float* R2, int Num, const FAmbienceParams& P)
	{
		// scratch kept between calls: no allocation on the audio thread once it has grown
		if (Tmp.size() < size_t(Num) * 2)
		{
			Tmp.resize(size_t(Num) * 2);
		}
		std::fill(Tmp.begin(), Tmp.begin() + size_t(Num) * 2, 0.f);
		float* TL = Tmp.data();
		float* TR = Tmp.data() + Num;
		float Before[NumVoices + 1] = {};
		auto Energy = [&]() { double E = 0; for (int I = 0; I < Num; I++) E += double(TL[I]) * TL[I] + double(TR[I]) * TR[I]; return float(E); };

		WindV.Render(TL, TR, Num, P.WindSpeed, P.WindGustiness, P.WindPan, P.AirSpeed, P.Gain[Wind]);
		Before[1] = Energy();
		RenderLeaves(TL, TR, Num, P);
		// grains: the leaves' flutters and the rain's drops (the leaves' share also gets a bed of far rustle)
		{
			const float Lv = SLeaves.Y;
			const float W = std::max(P.WindSpeed, 0.f);
			const float Bed = Lv * 0.0045f * std::pow(W, 0.8f);
			const float Needle = Clamp(P.Needles, 0.f, 1.f) * 0.004f * std::pow(W, 1.3f);
			for (int I = 0; I < Num; I++)
			{
				float GL = 0.f, GR = 0.f, LeafL = 0.f, LeafR = 0.f;
				for (size_t K = 0; K < Grains.size();)
				{
					FGrain& G = Grains[K];
					if (G.Wait > 0)
					{
						G.Wait--;
						K++;
						continue;
					}
					float V;
					if (G.Kind == 2)
					{
						// a bubble: sine whose pitch rises as it shrinks
						G.Phase += G.Freq / Rate;
						G.Freq *= 1.00003f;
						V = G.Amp * std::sin(2.f * Pi * G.Phase);
					}
					else
					{
						G.F.Process(R.White());
						V = G.Amp * G.F.Band;
					}
					if (G.Kind == 0)
					{
						PanTo(V, G.Pan, 1.f, LeafL, LeafR);
					}
					else
					{
						PanTo(V, G.Pan, 1.f, GL, GR);
					}
					G.Amp *= G.Decay;
					if (G.Amp < 1e-5f)
					{
						G = Grains.back();
						Grains.pop_back();
						continue;
					}
					K++;
				}
				const float Wn = R.White();
				LeafHp.Process(Wn);
				NeedleBp.Process(R.White());
				LeafL += Bed * LeafHp.High + Needle * NeedleBp.Band;
				LeafR += Bed * LeafHp.High * 0.9f + Needle * NeedleBp.Band * 0.95f;
				TL[I] += LeafL * P.Gain[Leaves] * 6.f + GL * P.Gain[Rain];
				TR[I] += LeafR * P.Gain[Leaves] * 6.f + GR * P.Gain[Rain];
			}
		}
		Before[2] = Energy();
		RenderSurf(TL, TR, Num, P);
		Before[3] = Energy();
		RenderRain(TL, TR, Num, P);
		Before[4] = Energy();
		RenderCrickets(TL, TR, Num, P);
		Before[5] = Energy();
		RenderThunder(TL, TR, Num, P);
		Before[6] = Energy();
		for (int V = 0; V < NumVoices; V++)
		{
			VoiceRms[V] = std::sqrt(std::max(Before[V + 1] - Before[V], 0.f) / (2.f * Num));
		}

		// through the walls: muffled and quieter the deeper inside
		float E = SEnc.Y;
		for (int I = 0; I < Num; I++)
		{
			if ((I & (Block - 1)) == 0)
			{
				E = SEnc.Step(Clamp(P.Enclosure, 0.f, 1.f));
				const float Cut = 18000.f * (1.f - E) * (1.f - E) + 280.f;
				WallL.Set(Cut, 0.6f, Rate);
				WallR.Set(Cut, 0.6f, Rate);
			}
			WallL.Process(TL[I]);
			WallR.Process(TR[I]);
			const float G = P.Master * (1.f - 0.9f * E);
			L[I] += G * WallL.Low;
			R2[I] += G * WallR.Low;
		}
		Time += Num / Rate;
	}
}
