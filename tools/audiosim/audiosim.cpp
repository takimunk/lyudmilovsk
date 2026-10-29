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
		// the weather's own voices (AmbienceWeather.cpp)
		S.push_back({ "drizzle_then_rain", 30.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 2.f; P.Leaves = 0.3f; P.Rain = T < 15.f ? 0.25f : 3.f; P.Drizzle = T < 15.f ? 1.f : 0.f; } });
		S.push_back({ "showers", 40.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 7.f; P.WindGustiness = 0.45f; P.Leaves = 0.5f; P.Rain = 5.f; P.Showery = 1.f; } });
		S.push_back({ "umbrella", 30.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 2.5f; P.Rain = 3.f; P.Wet = 1.f; P.Umbrella = T < 15.f ? 1.f : 0.f; } });
		S.push_back({ "hail_roof", 30.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 9.f; P.WindGustiness = 0.45f; P.Rain = 6.f; P.Hail = T < 5.f ? 0.f : 0.9f; P.RainRoof = T < 18.f ? 0.f : 1.f; } });
		S.push_back({ "rain_window_indoors", 30.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 9.f; P.Rain = 3.f; P.Enclosure = 1.f; P.Windows = T < 15.f ? 0.f : 1.f; } });
		S.push_back({ "blizzard", 45.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = Lerp(2.f, 19.f, T / 35.f); P.WindGustiness = 0.45f; P.Rain = 2.5f; P.Snow = 1.f; P.TempC = -8.f; P.SnowDepthCm = 40.f; P.SnowDensity = 90.f; } });
		S.push_back({ "snow_hush", 40.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 3.f; P.Needles = 0.6f; P.SurfHeight = 0.5f; P.SurfPeriod = 5.f; P.SurfDistance = 150.f; P.SurfPan = -0.5f; P.TempC = -3.f;
			P.SnowDepthCm = T < 20.f ? 0.f : 25.f; P.SnowDensity = 80.f; } });
		S.push_back({ "fog_lighthouse", 75.f, [](float, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 1.f; P.Fog = 0.9f; P.Drip = 0.5f; P.SurfHeight = 0.15f; P.SurfPeriod = 3.f; P.SurfDistance = 40.f; P.SurfPan = -0.4f;
			P.FogSignal = 1.f; P.FogSignalDistance = 1500.f; P.FogSignalPan = -0.5f; } });
		S.push_back({ "fog_signal_far", 70.f, [](float, FAmbienceParams& P, FAmbience&) { P.Fog = 0.9f; P.FogSignal = 1.f; P.FogSignalDistance = 6000.f; P.FogSignalPan = 0.3f; } });
		S.push_back({ "after_rain_drips", 30.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 1.5f; P.Leaves = 0.7f; P.Drip = 1.f; P.Wet = 1.f; } });
		S.push_back({ "thaw", 40.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 2.f; P.Melt = 1.f; P.SnowDepthCm = 15.f; P.SnowDensity = 380.f; P.TempC = 4.f; } });
		S.push_back({ "glaze_twigs", 30.f, [](float, FAmbienceParams& P, FAmbience&) { P.WindSpeed = 7.f; P.WindGustiness = 0.4f; P.Glaze = 1.f; P.TempC = -3.f; } });
		S.push_back({ "sea_ice_night", 80.f, [](float T, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 1.f; P.TempC = -14.f; P.SnowDepthCm = 20.f; P.SnowDensity = 200.f; P.SeaIce = 1.f; P.IceCm = 12.f; P.IceDistance = 25.f; P.IcePan = -0.4f;
			P.IceTempRate = T < 40.f ? 0.2f : -1.5f; } });
		S.push_back({ "ice_floes", 40.f, [](float, FAmbienceParams& P, FAmbience&) {
			P.WindSpeed = 8.f; P.TempC = -4.f; P.SeaIce = 0.5f; P.IceCm = 15.f; P.IceDistance = 10.f; P.IcePan = -0.5f; P.IceSwell = 0.5f; P.SurfHeight = 0.2f; P.SurfDistance = 40.f; P.SurfPan = -0.5f; } });
		S.push_back({ "footsteps", 64.f, [](float T, FAmbienceParams& P, FAmbience& A) {
			// eight grounds, 8 s each: dry earth, wet earth, puddles, fresh fluff, snow at -3, cold squeaky snow, slush, ice
			static float Next = 0.f;
			static int Foot = 0;
			if (T < 0.02f) { Next = 0.5f; Foot = 0; }
			const int G = int(T / 8.f);
			P.WindSpeed = 1.5f; P.TempC = 8.f; P.SnowDepthCm = 0.f; P.SnowDensity = 100.f; P.Wet = 0.f; P.Puddles = 0.f; P.Melt = 0.f; P.OnIce = 0.f;
			switch (G)
			{
			case 1: P.Wet = 0.8f; break;
			case 2: P.Wet = 1.f; P.Puddles = 1.f; break;
			case 3: P.TempC = -2.f; P.SnowDepthCm = 15.f; P.SnowDensity = 80.f; break;
			case 4: P.TempC = -3.f; P.SnowDepthCm = 15.f; P.SnowDensity = 200.f; break;
			case 5: P.TempC = -15.f; P.SnowDepthCm = 15.f; P.SnowDensity = 250.f; break;
			case 6: P.TempC = 2.f; P.SnowDepthCm = 8.f; P.SnowDensity = 400.f; P.Melt = 0.8f; break;
			case 7: P.TempC = -6.f; P.OnIce = 1.f; break;
			}
			if (T >= Next) { A.Footstep(1.f, Foot ? 0.12f : -0.12f); Foot ^= 1; Next += 0.55f; } } });
		S.push_back({ "thunder_approach", 90.f, [](float T, FAmbienceParams& P, FAmbience& A) {
			// a storm cell comes in from 12 km, passes 500 m off and moves away
			P.WindSpeed = Lerp(4.f, 11.f, T / 40.f); P.WindGustiness = 0.45f; P.Leaves = 0.5f; P.Rain = T > 30.f && T < 70.f ? 10.f : 0.5f; P.Showery = 1.f;
			static int Fired = 0;
			if (T < 0.02f) Fired = 0;
			const float At[7] = { 1.f, 12.f, 24.f, 34.f, 44.f, 56.f, 70.f }, Dist[7] = { 12000.f, 8000.f, 4500.f, 1800.f, 500.f, 2500.f, 7000.f }, Pan[7] = { 0.8f, 0.7f, 0.5f, 0.3f, 0.f, -0.5f, -0.8f };
			for (int K = Fired; K < 7; K++) if (T >= At[K]) { A.Strike(Dist[K], Pan[K]); Fired = K + 1; } } });
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
			static const char* Names[NumVoices] = { "wind", "leaves", "surf", "rain", "crickets", "thunder", "snow", "drips", "ice", "signal", "steps" };
			std::printf("    voices rms:");
			for (int V = 0; V < NumVoices; V++)
			{
				std::printf(" %s %.3f", Names[V], MeanOf(Res.VoiceRmsPerSecond[V], 0, 999));
			}
			std::printf("\n");
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
		// the weather's own voices
		{
			const FResult R = Run(Find("drizzle_then_rain"));
			const float Drz = MeanOf(R.VoiceRmsPerSecond[Rain], 5, 14), Rn = MeanOf(R.VoiceRmsPerSecond[Rain], 21, 29);
			Check(Drz > 0.001f && Rn > Drz * 2.5f, "drizzle is a soft hiss next to rain", Drz, Rn);
		}
		{
			const FResult R = Run(Find("umbrella"));
			const float Under = MeanOf(R.VoiceRmsPerSecond[Rain], 4, 14), Open = MeanOf(R.VoiceRmsPerSecond[Rain], 20, 29);
			Check(Under > Open * 1.3f, "rain drums on the umbrella over the head", Under, Open);
		}
		{
			const FResult R = Run(Find("hail_roof"));
			const float Plain = MeanOf(R.VoiceRmsPerSecond[Rain], 2, 5), Hail = MeanOf(R.VoiceRmsPerSecond[Rain], 8, 17);
			Check(Hail > Plain * 1.3f, "hail knocks over the rain", Plain, Hail);
		}
		{
			const FResult R = Run(Find("rain_window_indoors"));
			const float Walls = MeanOf(R.RmsPerSecond, 4, 14), Glass = MeanOf(R.RmsPerSecond, 18, 29);
			Check(Glass > Walls * 1.15f, "indoors by a window the rain ticks on the glass", Walls, Glass);
		}
		{
			const FResult R = Run(Find("blizzard"));
			const float Calm = MeanOf(R.VoiceRmsPerSecond[Snow], 1, 6), Storm = MeanOf(R.VoiceRmsPerSecond[Snow], 36, 44);
			Check(Calm < 0.004f && Storm > Calm * 5.f && Storm > 0.008f, "snow falls silently in calm, hisses in a blizzard", Calm, Storm);
		}
		{
			const FResult R = Run(Find("snow_hush"));
			const float Bare = MeanOf(R.RmsPerSecond, 8, 19), Snowy = MeanOf(R.RmsPerSecond, 32, 40);
			Check(Snowy < Bare * 0.85f, "fresh snow hushes the world", Bare, Snowy);
		}
		{
			// two blasts a minute: loud in their seconds, the gap between quiet (the echo dies within 3 s)
			const FResult R = Run(Find("fog_lighthouse"));
			const std::vector<float>& S = R.VoiceRmsPerSecond[Signal];
			float Blast = 0.f, Gap = 0.f;
			for (int I = 0; I < int(S.size()); I++)
			{
				// the signal's clock starts at 50 s of its minute: blasts at 10-12.5 s and 15-17.5 s of the scene, then +60
				const int M = (I + 50) % 60;
				if (M == 0 || M == 1 || M == 5 || M == 6) Blast = std::fmax(Blast, S[I]);
				if (M > 15 && M < 55) Gap = std::fmax(Gap, S[I]);
			}
			Check(Blast > 0.01f && Gap < Blast * 0.05f, "the fog signal: two blasts a minute, silence between", Blast, Gap);
			const FResult F = Run(Find("fog_signal_far"));
			float FarBlast = 0.f;
			for (float V : F.VoiceRmsPerSecond[Signal]) FarBlast = std::fmax(FarBlast, V);
			Check(FarBlast < Blast * 0.5f && FarBlast > 0.001f, "the fog signal fades with distance", Blast, FarBlast);
		}
		{
			const FResult R = Run(Find("after_rain_drips"));
			Check(MeanOf(R.VoiceRmsPerSecond[Drips], 5, 30) > 0.001f, "trees drip after the rain", MeanOf(R.VoiceRmsPerSecond[Drips], 5, 30));
			const FResult T = Run(Find("thaw"));
			Check(MeanOf(T.VoiceRmsPerSecond[Drips], 10, 40) > 0.001f, "eaves drip in a thaw", MeanOf(T.VoiceRmsPerSecond[Drips], 10, 40));
			const FResult G = Run(Find("glaze_twigs"));
			Check(MeanOf(G.VoiceRmsPerSecond[Drips], 5, 30) > 0.0005f, "iced twigs clink in the wind", MeanOf(G.VoiceRmsPerSecond[Drips], 5, 30));
		}
		{
			// the sea ice cracks and sings more as the night cools it
			const FResult R = Run(Find("sea_ice_night"));
			int Warm = 0, Cold = 0;
			for (int I = 5; I < 40; I++) Warm += R.VoiceRmsPerSecond[Ice][I] > 0.002f;
			for (int I = 45; I < 80; I++) Cold += R.VoiceRmsPerSecond[Ice][I] > 0.002f;
			Check(Cold > Warm + 4 && Cold > 8, "sea ice sings and booms as the air cools", float(Warm), float(Cold));
			const FResult F = Run(Find("ice_floes"));
			Check(MeanOf(F.VoiceRmsPerSecond[Ice], 5, 40) > 0.002f, "floes grind in a swell", MeanOf(F.VoiceRmsPerSecond[Ice], 5, 40));
		}
		{
			// each ground is heard underfoot, the snow louder than earth, a cold snow's squeak brighter than a crunch
			const FResult R = Run(Find("footsteps"));
			const char* Grounds[8] = { "dry earth", "wet earth", "puddles", "fresh fluff", "snow", "cold snow", "slush", "ice" };
			float Lv[8];
			for (int G = 0; G < 8; G++)
			{
				Lv[G] = MeanOf(R.VoiceRmsPerSecond[Steps], G * 8 + 1, G * 8 + 8);
				char Line[96];
				std::snprintf(Line, sizeof Line, "footsteps on %s are heard", Grounds[G]);
				Check(Lv[G] > 0.004f && Lv[G] < 0.12f, Line, Lv[G]);
			}
			Check(Lv[4] > Lv[0] * 1.3f, "snow crunches louder than earth pads", Lv[0], Lv[4]);
			FScene Still = Find("footsteps");
			Still.Set = [](float, FAmbienceParams& P, FAmbience&) { P.SnowDepthCm = 15.f; P.TempC = -3.f; P.SnowDensity = 200.f; };
			const FResult S = Run(Still);
			Check(MeanOf(S.VoiceRmsPerSecond[Steps], 0, 60) < 1e-5f, "no steps without walking", MeanOf(S.VoiceRmsPerSecond[Steps], 0, 60));
		}
		{
			// the storm coming: the far strikes only rumble low and late, the near one cracks at once
			const FResult R = Run(Find("thunder_approach"));
			auto One = [&](float Dist)
			{
				FScene S = Find("silence");
				S.Seconds = 50.f;
				S.Set = [Dist](float T, FAmbienceParams&, FAmbience& A) { if (T < 0.01f) A.Strike(Dist, 0.f); };
				float Max = 0.f;
				for (float V : Run(S).VoiceRmsPerSecond[Thunder]) Max = std::fmax(Max, V);
				return Max;
			};
			const float Far = One(8000.f), Near = One(500.f);
			Check(Near > Far * 3.f, "a near strike is much louder than a far rumble", Far, Near);
			Check(MeanOf(R.VoiceRmsPerSecond[Thunder], 2, 36) > 0.001f, "the far storm rumbles before it arrives", MeanOf(R.VoiceRmsPerSecond[Thunder], 2, 36));
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
