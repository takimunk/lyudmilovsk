// Renders the procedural sound (Private/Audio/AmbienceDSP) to WAV files and checks it, outside the engine.
//   tools/audiosim/run.sh render [scene ...]   Saved/AudioSim/<scene>.wav (all scenes without arguments)
//   tools/audiosim/run.sh test                 levels, silence, monotony, timing; exit code 1 on a failure
//   tools/audiosim/run.sh weather              the weather model's climate (weather.cpp)
// Then tools/audiosim/spectrogram.py Saved/AudioSim/<scene>.wav draws what it sounds like.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Audio/AmbienceDSP.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace LyudAudio;

int WeatherMain(int Argc, char** Argv);

namespace
{
	constexpr float SR = 48000.f;
	constexpr int Buf = 512;

	bool WriteWav(const std::string& Path, const std::vector<float>& L, const std::vector<float>& R)
	{
		FILE* F = std::fopen(Path.c_str(), "wb");
		if (!F)
		{
			return false;
		}
		const uint32_t N = uint32_t(L.size()), Data = N * 4, Rate = uint32_t(SR), Bytes = Rate * 4;
		const uint16_t Fmt = 1, Ch = 2, Align = 4, Bits = 16;
		const uint32_t Riff = 36 + Data, FmtLen = 16;
		std::fwrite("RIFF", 1, 4, F); std::fwrite(&Riff, 4, 1, F); std::fwrite("WAVEfmt ", 1, 8, F);
		std::fwrite(&FmtLen, 4, 1, F); std::fwrite(&Fmt, 2, 1, F); std::fwrite(&Ch, 2, 1, F); std::fwrite(&Rate, 4, 1, F);
		std::fwrite(&Bytes, 4, 1, F); std::fwrite(&Align, 2, 1, F); std::fwrite(&Bits, 2, 1, F);
		std::fwrite("data", 1, 4, F); std::fwrite(&Data, 4, 1, F);
		for (uint32_t I = 0; I < N; I++)
		{
			const int16_t A = int16_t(std::fmax(-1.f, std::fmin(1.f, L[I])) * 32767.f);
			const int16_t B = int16_t(std::fmax(-1.f, std::fmin(1.f, R[I])) * 32767.f);
			std::fwrite(&A, 2, 1, F);
			std::fwrite(&B, 2, 1, F);
		}
		std::fclose(F);
		return true;
	}

	/** A scene: Seconds long, Set(t, params) called every buffer; Event(t) may fire thunder. */
	struct FScene
	{
		const char* Name;
		float Seconds;
		std::function<void(float, FAmbienceParams&, FAmbience&)> Set;
	};

	struct FResult
	{
		std::vector<float> L, R;
		std::vector<float> VoiceRmsPerSecond[NumVoices];
		std::vector<float> RmsPerSecond;
		float Peak = 0.f;
		bool bFinite = true;
	};

	FResult Run(const FScene& S)
	{
		FResult Out;
		FAmbience A;
		A.Init(SR, 7);
		FAmbienceParams P;
		const int Total = int(S.Seconds * SR);
		Out.L.assign(Total, 0.f);
		Out.R.assign(Total, 0.f);
		double SecE = 0, SecV[NumVoices] = {};
		int SecN = 0;
		for (int I = 0; I < Total; I += Buf)
		{
			const int N = std::min(Buf, Total - I);
			S.Set(I / SR, P, A);
			A.Render(&Out.L[I], &Out.R[I], N, P);
			for (int K = 0; K < N; K++)
			{
				const float L = Out.L[I + K], R = Out.R[I + K];
				if (!std::isfinite(L) || !std::isfinite(R))
				{
					Out.bFinite = false;
				}
				Out.Peak = std::fmax(Out.Peak, std::fmax(std::fabs(L), std::fabs(R)));
				SecE += 0.5 * (double(L) * L + double(R) * R);
			}
			for (int V = 0; V < NumVoices; V++)
			{
				SecV[V] += double(A.VoiceRms[V]) * A.VoiceRms[V] * N;
			}
			SecN += N;
			if (SecN >= int(SR))
			{
				Out.RmsPerSecond.push_back(float(std::sqrt(SecE / SecN)));
				for (int V = 0; V < NumVoices; V++)
				{
					Out.VoiceRmsPerSecond[V].push_back(float(std::sqrt(SecV[V] / SecN)));
					SecV[V] = 0;
				}
				SecE = 0;
				SecN = 0;
			}
		}
		return Out;
	}

	float Lerp(float A, float B, float T) { return A + (B - A) * std::fmin(std::fmax(T, 0.f), 1.f); }

	std::vector<FScene> Scenes()
	{
		std::vector<FScene> S;
		S.push_back({ "wind_ramp", 40.f, [](float T, FAmbienceParams& P, FAmbience&) { P.WindSpeed = Lerp(0.f, 18.f, T / 36.f); P.WindGustiness = 0.3f; P.WindPan = 0.5f; } });
		S.push_back({ "breeze_autumn_trees", 25.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 3.5f; P.Leaves = 0.9f; P.LeafDry = 0.7f; P.Needles = 0.2f; } });
		S.push_back({ "gale_forest", 25.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 13.f; P.WindGustiness = 0.4f; P.Leaves = 0.8f; P.LeafDry = 0.3f; P.Needles = 0.6f; } });
		S.push_back({ "pines_light", 20.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 5.f; P.Needles = 1.f; } });
		S.push_back({ "beach_walk_away", 60.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 6.f; P.SurfHeight = 0.7f; P.SurfPeriod = 5.5f; P.SurfDistance = Lerp(6.f, 600.f, (T - 20.f) / 35.f); P.SurfPan = -0.6f; } });
		S.push_back({ "calm_sea_shore", 30.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 1.f; P.SurfHeight = 0.12f; P.SurfPeriod = 3.f; P.SurfDistance = 4.f; P.SurfPan = -0.3f; } });
		S.push_back({ "storm_sea", 40.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 16.f; P.WindGustiness = 0.45f; P.SurfHeight = 2.2f; P.SurfPeriod = 7.f; P.SurfDistance = 30.f; P.SurfPan = -0.5f; } });
		S.push_back({ "rain_steps", 45.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 2.f; P.Leaves = 0.4f; P.Rain = T < 15.f ? 0.4f : (T < 30.f ? 3.f : 12.f); } });
		S.push_back({ "rain_roof", 20.f, [](float, FAmbienceParams& P, FAmbience&) { P.Rain = 4.f; P.RainRoof = 1.f; } });
		S.push_back({ "summer_night", 30.f, [](float, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 1.2f; P.Leaves = 0.5f; P.Crickets = 1.f; P.TempC = 19.f; P.SurfHeight = 0.3f; P.SurfDistance = 400.f; P.SurfPeriod = 4.f; } });
		S.push_back({ "thunderstorm", 45.f, [](float T, FAmbienceParams& P, FAmbience& A) {
			P.WindSpeed = 8.f; P.WindGustiness = 0.4f; P.Leaves = 0.6f; P.Rain = 9.f;
			static int Fired = 0;
			if (T < 0.02f) Fired = 0;
			const float At[3] = { 1.f, 15.f, 27.f }, Dist[3] = { 4000.f, 1200.f, 350.f }, Pan[3] = { -0.7f, 0.4f, 0.f };
			for (int K = Fired; K < 3; K++) if (T >= At[K]) { A.Strike(Dist[K], Pan[K]); Fired = K + 1; } } });
		S.push_back({ "flight", 30.f, [](float T, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 6.f; P.AirSpeed = Lerp(0.f, 24.f, T / 25.f); } });
		S.push_back({ "walk_indoors", 30.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 7.f; P.Leaves = 0.6f; P.Rain = 2.f; P.Enclosure = Lerp(0.f, 1.f, (T - 10.f) / 10.f); } });
		S.push_back({ "silence", 10.f, [](float, FAmbienceParams&, FAmbience&) {} });
		return S;
	}

	float MeanOf(const std::vector<float>& V, int From, int To)
	{
		float S = 0.f;
		int N = 0;
		for (int I = From; I < To && I < int(V.size()); I++, N++) S += V[I];
		return N ? S / N : 0.f;
	}

	int Render(int Argc, char** Argv)
	{
		int Done = 0;
		for (const FScene& S : Scenes())
		{
			bool bWant = Argc <= 2;
			for (int K = 2; K < Argc; K++)
			{
				bWant |= std::strcmp(Argv[K], S.Name) == 0;
			}
			if (!bWant)
			{
				continue;
			}
			const FResult Res = Run(S);
			const std::string Path = std::string("Saved/AudioSim/") + S.Name + ".wav";
			WriteWav(Path, Res.L, Res.R);
			float Mean = 0.f;
			for (float V : Res.RmsPerSecond) Mean += V;
			Mean /= std::max<size_t>(Res.RmsPerSecond.size(), 1);
			std::printf("%-22s %5.1f s  peak %.2f  rms %.3f (%.1f dBFS)  -> %s\n", S.Name, S.Seconds, Res.Peak, Mean, 20.f * std::log10(std::fmax(Mean, 1e-6f)), Path.c_str());
			std::printf("    voices rms: wind %.3f leaves %.3f surf %.3f rain %.3f crickets %.3f thunder %.3f\n",
				MeanOf(Res.VoiceRmsPerSecond[Wind], 0, 999), MeanOf(Res.VoiceRmsPerSecond[Leaves], 0, 999), MeanOf(Res.VoiceRmsPerSecond[Surf], 0, 999),
				MeanOf(Res.VoiceRmsPerSecond[Rain], 0, 999), MeanOf(Res.VoiceRmsPerSecond[Crickets], 0, 999), MeanOf(Res.VoiceRmsPerSecond[Thunder], 0, 999));
			Done++;
		}
		return Done ? 0 : 1;
	}

	int Failures = 0;
	void Check(bool bOk, const char* What, float A = 0.f, float B = 0.f)
	{
		std::printf("%s  %s  (%.4f, %.4f)\n", bOk ? "ok  " : "FAIL", What, A, B);
		Failures += bOk ? 0 : 1;
	}

	int Test()
	{
		std::vector<FScene> All = Scenes();
		auto Find = [&](const char* Name) -> const FScene& { for (const FScene& S : All) if (!std::strcmp(S.Name, Name)) return S; return All[0]; };
		for (const FScene& S : All)
		{
			const FResult R = Run(S);
			char Line[160];
			std::snprintf(Line, sizeof Line, "%s: finite, no clipping (peak < 0.98)", S.Name);
			Check(R.bFinite && R.Peak < 0.98f, Line, R.Peak);
		}
		{
			const FResult R = Run(Find("silence"));
			Check(R.Peak < 1e-4f, "nothing in the world: silence", R.Peak);
		}
		{
			const FResult R = Run(Find("wind_ramp"));
			const float Calm = MeanOf(R.RmsPerSecond, 1, 3), Breeze = MeanOf(R.RmsPerSecond, 13, 15), Gale = MeanOf(R.RmsPerSecond, 33, 36);
			Check(Calm < 0.01f, "wind under 1 m/s is almost silent", Calm);
			Check(Breeze > Calm * 3.f && Gale > Breeze * 2.f, "wind grows with its speed", Breeze, Gale);
			Check(Gale > 0.05f && Gale < 0.35f, "a gale is loud but leaves headroom", Gale);
		}
		{
			const FResult R = Run(Find("beach_walk_away"));
			const float Near = MeanOf(R.VoiceRmsPerSecond[Surf], 5, 18), Far = MeanOf(R.VoiceRmsPerSecond[Surf], 56, 60);
			Check(Near > 0.03f, "surf at the waterline is clearly heard", Near);
			Check(Far < Near * 0.35f, "surf fades walking away 600 m", Near, Far);
			// waves break with the period: the near surf's loudness swings
			float Min = 1e9f, Max = 0.f;
			for (int I = 3; I < 18; I++) { Min = std::fmin(Min, R.VoiceRmsPerSecond[Surf][I]); Max = std::fmax(Max, R.VoiceRmsPerSecond[Surf][I]); }
			Check(Max > Min * 1.5f, "the near surf comes in waves, not a steady hiss", Min, Max);
		}
		{
			const FResult R = Run(Find("rain_steps"));
			const float A = MeanOf(R.VoiceRmsPerSecond[Rain], 5, 14), B = MeanOf(R.VoiceRmsPerSecond[Rain], 20, 29), C = MeanOf(R.VoiceRmsPerSecond[Rain], 36, 44);
			Check(A > 0.003f && B > A * 1.5f && C > B * 1.3f, "drizzle < rain < downpour", A, C);
		}
		{
			const FResult R = Run(Find("summer_night"));
			Check(MeanOf(R.VoiceRmsPerSecond[Crickets], 5, 30) > 0.004f, "crickets sing on a warm night", MeanOf(R.VoiceRmsPerSecond[Crickets], 5, 30));
			FScene Cold = Find("summer_night");
			Cold.Set = [](float, FAmbienceParams& P, FAmbience&) { P.Crickets = 1.f; P.TempC = 8.f; };
			const FResult C = Run(Cold);
			Check(MeanOf(C.VoiceRmsPerSecond[Crickets], 2, 30) < 1e-4f, "no crickets at 8 C", MeanOf(C.VoiceRmsPerSecond[Crickets], 2, 30));
		}
		{
			const FResult R = Run(Find("thunderstorm"));
			// the 4 km strike is heard ~11.7 s after the flash (at 1 s), not before
			const float Before = MeanOf(R.VoiceRmsPerSecond[Thunder], 2, 11), After = MeanOf(R.VoiceRmsPerSecond[Thunder], 13, 15);
			Check(Before < 1e-4f && After > 0.005f, "thunder arrives after the sound's travel time", Before, After);
		}
		{
			const FResult R = Run(Find("walk_indoors"));
			const float Out = MeanOf(R.RmsPerSecond, 2, 9), In = MeanOf(R.RmsPerSecond, 24, 30);
			Check(In < Out * 0.25f, "indoors the outside is muffled and quiet", Out, In);
		}
		{
			const FResult R = Run(Find("flight"));
			Check(MeanOf(R.RmsPerSecond, 24, 30) > 2.f * MeanOf(R.RmsPerSecond, 1, 3), "the air rushes louder the faster the flight", MeanOf(R.RmsPerSecond, 1, 3), MeanOf(R.RmsPerSecond, 24, 30));
		}
		std::printf("%s (%d failed)\n", Failures ? "FAILED" : "all passed", Failures);
		return Failures ? 1 : 0;
	}
}

int main(int Argc, char** Argv)
{
	const char* Cmd = Argc > 1 ? Argv[1] : "test";
	if (!std::strcmp(Cmd, "weather")) return WeatherMain(Argc, Argv);
	if (!std::strcmp(Cmd, "render")) return Render(Argc, Argv);
	return Test();
}
