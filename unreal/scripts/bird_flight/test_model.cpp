// Headless checks of the player bird's flight model (Private/Birds/BirdFlightModel.*), no engine.
// Built and run by unreal/scripts/test_bird_flight.sh; prints one line per check and exits 1 on a failure.
#include "BirdFlightModel.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <functional>
#include <string>
#include <vector>

using namespace BirdFlight;

namespace
{
	struct FSpecies { const char* Id; EStyle Style; double MassG, SpanCm, Hz, Cruise; };
	const FSpecies Species[] = {
		{ "HouseSparrow", EStyle::Bounding, 30, 23, 15, 9 },
		{ "Goldfinch", EStyle::Bounding, 16, 23, 18, 9 },
		{ "Starling", EStyle::Flapping, 75, 38, 12, 17 },
		{ "SongThrush", EStyle::Undulating, 70, 34, 10, 13 },
		{ "RockDove", EStyle::Clapping, 330, 65, 7, 17 },
		{ "Rook", EStyle::Flapping, 450, 90, 3.4, 12 },
		{ "HerringGull", EStyle::Soaring, 1000, 140, 3, 12 },
		{ "CommonSwift", EStyle::Flicker, 40, 42, 8, 20 },
	};
	int Failures = 0;
	void Check(bool Ok, const std::string& What)
	{
		std::printf("%s  %s\n", Ok ? "ok  " : "FAIL", What.c_str());
		if (!Ok) Failures++;
	}
	std::string F(const char* Fmt, ...)
	{
		char Buf[512];
		va_list A; va_start(A, Fmt); std::vsnprintf(Buf, sizeof Buf, Fmt, A); va_end(A);
		return Buf;
	}
	const double Dt = 1.0 / 60.0;

	FState Flying(const FParams& P, double V)
	{
		FState S; S.Pos = V3(0, 0, 100); S.Vel = V3(V, 0, 0); return S;
	}
	void Run(const FParams& P, FState& S, double Seconds, const std::function<void(double, FInput&)>& Pilot, FAir Air = FAir())
	{
		for (double T = 0; T < Seconds; T += Dt)
		{
			FInput In; Pilot(T, In); Step(P, In, Air, S, Dt);
		}
	}
	// the pitch a steady glide at speed V needs: the polar's path angle
	double GlidePitch(const FParams& P, double V) { return -std::asin(GlideSink(P, V) / V); }
}

int main()
{
	for (const FSpecies& Sp : Species)
	{
		const FParams P = MakeParams(Sp.Style, Sp.MassG, Sp.SpanCm, Sp.Hz, Sp.Cruise);
		std::printf("-- %s: best glide %.1f m/s, stall %.1f, glide %.0f:1, %d beats/click, click %.2f m/s %.2f up\n",
			Sp.Id, P.BestGlide, P.Stall, P.GlideRatio, P.BeatsPerClick, P.ClickThrust, P.ClickLift);

		// 1. hands off at the best glide speed, gaze on the polar's glide path: the glide ratio holds
		{
			FState S = Flying(P, P.BestGlide);
			const double Pitch = GlidePitch(P, P.BestGlide);
			Run(P, S, 10, [&](double, FInput& In) { In.AimPitch = Pitch; });
			const double Ratio = S.Pos.Size2D() / std::max(1e-6, 100.0 - S.Pos.Z);
			const double Speed = BirdFlight::Speed(S);
			Check(std::fabs(Ratio - P.GlideRatio) < P.GlideRatio * 0.15 && std::fabs(Speed - P.BestGlide) < 1.5,
				F("glide: %.1f:1 at %.1f m/s (want %.0f:1)", Ratio, Speed, P.GlideRatio));
		}
		// 2. Space held, level gaze: holds height and gets to about cruise speed
		{
			FState S = Flying(P, P.BestGlide);
			double Clock = 0, Tuck = 0;
			// the player holds the height with the gaze
			Run(P, S, 20, [&](double, FInput& In) { const bool C = AutoFlap(P, Dt, Clock, Tuck); In.ClickL = In.ClickR = C; In.TuckL = In.TuckR = Tuck; In.AimPitch = std::min(0.3, std::max(-0.3, (100 - S.Pos.Z) * 0.05)); });
			const double Speed = BirdFlight::Speed(S);
			Check(S.Pos.Z > 95 && Speed > P.Cruise * 0.85 && Speed < P.Cruise * 1.4,
				F("cruise: %.1f m/s, height %+.1f m after 20 s (want ~%.0f m/s)", Speed, S.Pos.Z - 100, P.Cruise));
		}
		// 3. take-off from standing: a hop, then clicking without pause, gaze 25 deg up: off the ground and climbing
		{
			FState S; S.Vel = V3(0.6, 0, 1.8);
			double Next = 0, MinZ = 0;
			Run(P, S, 3, [&](double T, FInput& In) { if (T >= Next) { In.ClickL = In.ClickR = true; Next += P.ClickTime(); } In.AimPitch = 0.45; });
			MinZ = S.Pos.Z;
			Check(MinZ > 1.5 && BirdFlight::Speed(S) > P.Stall, F("take-off: %.1f m up, %.1f m/s after 3 s (stall %.1f)", S.Pos.Z, BirdFlight::Speed(S), P.Stall));
		}
		// 3b. the same with the gaze only a little up (a player looking ahead): flying speed within 2.5 s
		{
			FState S; S.Vel = V3(0.6, 0, 1.8);
			double Next = 0;
			Run(P, S, 2.5, [&](double T, FInput& In) { if (T >= Next) { In.ClickL = In.ClickR = true; Next += P.ClickTime(); } In.AimPitch = 0.2; });
			Check(S.Pos.Z > 0.5 && BirdFlight::Speed(S) > P.Stall, F("take-off, level gaze: %.1f m up, %.1f m/s after 2.5 s", S.Pos.Z, BirdFlight::Speed(S)));
		}
		// 4. wings in, gaze down: speeds up to a fast dive but not past the dive limit
		{
			FState S = Flying(P, P.Cruise);
			Run(P, S, 8, [&](double, FInput& In) { In.TuckL = In.TuckR = 1; In.AimPitch = -1.2; });
			const double Speed = BirdFlight::Speed(S);
			Check(Speed > P.Cruise * 1.6 && Speed < P.MaxSpeed * 1.1, F("dive: %.1f m/s (limit %.0f)", Speed, P.MaxSpeed));
		}
		// 5. stall: gaze steeply up, no beats: speed bleeds off, the nose drops, the bird does not fly backwards
		{
			FState S = Flying(P, P.Cruise);
			double MinSpeed = 1e9; bool Backwards = false;
			for (double T = 0; T < 8; T += Dt)
			{
				FInput In; In.AimPitch = 1.3; Step(P, In, FAir(), S, Dt);
				MinSpeed = std::min(MinSpeed, BirdFlight::Speed(S));
				Backwards |= S.Vel.X < -1.0;
			}
			const double Sink = -S.Vel.Z;
			Check(!Backwards && Sink > -0.5 && BirdFlight::Speed(S) > P.Stall * 0.6, F("stall: lowest %.1f m/s, then sinking %.1f m/s at %.1f m/s", MinSpeed, Sink, BirdFlight::Speed(S)));
		}
		// 6. a 90 degree turn of the gaze: the course follows within the species' time and keeps flying
		{
			FState S = Flying(P, P.Cruise);
			double Clock = 0, Tuck = 0, Done = -1;
			for (double T = 0; T < 6; T += Dt)
			{
				FInput In; In.AimYaw = 1.5708; const bool C = AutoFlap(P, Dt, Clock, Tuck); In.ClickL = In.ClickR = C; In.TuckL = In.TuckR = Tuck;
				Step(P, In, FAir(), S, Dt);
				if (Done < 0 && std::atan2(S.Vel.Y, S.Vel.X) > 1.5708 - 0.1) Done = T;
			}
			const double Want = 1.5708 / P.TurnRate + P.Follow * 3 + 0.4;
			Check(Done > 0 && Done < Want && S.Pos.Z > 90, F("turn 90: %.2f s (within %.2f), height %+.1f m", Done, Want, S.Pos.Z - 100));
		}
		// 7. one-wing clicks turn away from the beating wing
		{
			FState S = Flying(P, P.Cruise);
			int N = 0;
			Run(P, S, 1.2, [&](double T, FInput& In) { In.AimYaw = std::atan2(S.Vel.Y, S.Vel.X); if (T > N * 0.3) { In.ClickL = true; N++; } });
			const double Yaw = std::atan2(S.Vel.Y, S.Vel.X) * 57.3;
			Check(Yaw > 15, F("left oar: course %+.0f deg (right)", Yaw));
		}
		// 8. ridge lift: wings spread, level gaze, rising air the size of the sink: no height lost
		{
			FState S = Flying(P, P.BestGlide);
			FAir Air; Air.Updraft = GlideSink(P, P.BestGlide) * 1.3;
			const double Pitch = GlidePitch(P, P.BestGlide);
			Run(P, S, 15, [&](double, FInput& In) { In.AimPitch = Pitch; }, Air);
			Check(S.Pos.Z >= 100, F("updraft %.2f m/s: height %+.1f m after 15 s without a beat", Air.Updraft, S.Pos.Z - 100));
		}
		// 9. flare from cruise: slows to near stall within a few metres' worth of seconds
		{
			FState S = Flying(P, P.Cruise);
			double T = 0;
			for (; T < 5 && BirdFlight::Speed(S) > P.Stall * 1.1; T += Dt)
			{
				FInput In; In.Flare = 1; In.AimPitch = 0.1; Step(P, In, FAir(), S, Dt);
			}
			Check(T < 2.0, F("flare: %.1f -> %.1f m/s in %.2f s, %+.1f m", P.Cruise, BirdFlight::Speed(S), T, S.Pos.Z - 100));
		}
	}
	std::printf(Failures ? "%d FAILED\n" : "all passed\n", Failures);
	return Failures ? 1 : 0;
}
