// The bird voices' tests (Private/Birds/BirdVoice.cpp, docs/bird_voices.md): the dawn chorus' order by sunrise through a
// year, where and when a bird sings at all, the alarm that spreads through a garden and not across the town, the murmur
// of a flock, the roost's chatter, the gulls' morning, and the voice budget near the camera.
// Part of `tools/birdsim/run.sh test`; BIRD_ONLY=voices runs only these.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdNest.h"
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Birds/BirdSim.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace LyudBirds;

namespace VoiceTest
{
	int Pass = 0, Fail = 0;

	void Check(bool bOk, const std::string& What)
	{
		std::printf("  %s %s\n", bOk ? "PASS" : "FAIL", What.c_str());
		(bOk ? Pass : Fail)++;
	}

	std::string F(const char* Fmt, ...)
	{
		char Buf[512];
		va_list A;
		va_start(A, Fmt);
		std::vsnprintf(Buf, sizeof(Buf), Fmt, A);
		va_end(A);
		return Buf;
	}

	const float Ground = -43.f;

	/** An empty world on a day (sun times roughly for 55 N), no map, no nests, no hawks. */
	void Empty(FWorld& W, float Doy, float Hour)
	{
		W = FWorld();
		W.Opt.bNests = false;
		W.Opt.bPredators = false;
		W.Opt.bCorvids = false;
		W.DayOfYear = Doy;
		const float Half = 6.15f + 2.6f * std::sin((Doy - 80.f) / 365.f * 2.f * 3.14159265f);
		W.Sunrise = 12.3f - Half;
		W.Sunset = 12.3f + Half;
		W.Hour = Hour;
		W.Camera = V3(0.f, 0.f, Ground + 1.7f);
	}

	/** One bird sitting on a twig Up metres over P (its own small tree). */
	int Perched(FWorld& W, const char* Species, const V3& P, float Up = 4.f)
	{
		const int Id = W.AddBird(FindSpecies(Species), V3(P.X, P.Y, Ground + Up));
		if (Id < 0)
		{
			return -1;
		}
		FBird& B = W.Birds[Id];
		FSite& S = W.Sites[B.Site];
		S.Kind = EHabitat::Broadleaf;
		S.Radius = 0.f;
		B.Pos = B.Spot = S.A;
		B.Act = EAct::Perch;
		B.ActLen = 1e9f;
		B.Tier = 0;
		return Id;
	}

	void Step(FWorld& W, float Seconds, float Dt, const std::function<void()>& Each = nullptr)
	{
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			W.Hour += Dt / 3600.f;
			W.Step(Dt);
			if (Each)
			{
				Each();
			}
			W.Events.clear();
		}
	}

	/** The first minute (hours) a perched bird of the species would sing on the world's day; -1 = never before noon. */
	float FirstSong(FWorld& W, int Id)
	{
		const float Keep = W.Hour;
		float At = -1.f;
		for (float H = W.Sunrise - 2.5f; H < 12.f; H += 1.f / 60.f)
		{
			W.Hour = H;
			EVoice V;
			if (W.VoiceNow(W.Birds[Id], V) > 0.f && V == EVoice::Song)
			{
				At = H;
				break;
			}
		}
		W.Hour = Keep;
		return At;
	}

	void Chorus()
	{
		// the order of the dawn chorus, by the day's own sunrise, on days through the breeding months
		const char* Order[] = { "Robin", "Blackbird", "SongThrush", "GreatTit", "BlueTit", "Chaffinch", "HouseSparrow" };
		bool bOrder = true, bBySun = true;
		float Earliest = 1e9f, Latest = -1e9f;
		int Days = 0;
		for (float Doy = 65.f; Doy <= 165.f; Doy += 10.f)
		{
			FWorld W;
			Empty(W, Doy, 3.f);
			float Prev = -1e9f;
			for (const char* Sp : Order)
			{
				const int Id = Perched(W, Sp, V3(10.f, 0.f, 0.f));
				const float At = FirstSong(W, Id);
				bOrder &= At > Prev;
				const float Before = (W.Sunrise - At) * 60.f;
				bBySun &= std::fabs(Before - VoiceTraits(W.Birds[Id].Species).ChorusMin) < 1.5f;
				Prev = At;
			}
			Earliest = std::min(Earliest, W.Sunrise);
			Latest = std::max(Latest, W.Sunrise);
			Days++;
		}
		Check(bOrder, F("dawn chorus: robin, blackbird, song thrush, great tit, blue tit, chaffinch, house sparrow in that order on %d days (sunrise %.1f to %.1f h)", Days, Earliest, Latest));
		Check(bBySun, "dawn chorus: each species starts at its own minutes from that day's sunrise (robin an hour before)");

		// who sings when in the year
		{
			FWorld W;
			Empty(W, 300.f, 3.f);      // late October
			const int Robin = Perched(W, "Robin", V3(10.f, 0.f, 0.f));
			const int Black = Perched(W, "Blackbird", V3(14.f, 0.f, 0.f));
			const int Tit = Perched(W, "GreatTit", V3(18.f, 0.f, 0.f));
			Check(FirstSong(W, Robin) > 0.f && FirstSong(W, Black) < 0.f && FirstSong(W, Tit) < 0.f, "in late October only the robin sings (its autumn song); the blackbird and the great tit are silent");
			Empty(W, 20.f, 3.f);       // 20 January
			const int Tit2 = Perched(W, "GreatTit", V3(10.f, 0.f, 0.f));
			const int Chaff = Perched(W, "Chaffinch", V3(14.f, 0.f, 0.f));
			Check(FirstSong(W, Tit2) > 0.f && FirstSong(W, Chaff) < 0.f, "on 20 January the great tit has started, the chaffinch not yet");
		}
		// where and in what weather
		{
			FWorld W;
			Empty(W, 130.f, 3.f);
			const int Id = Perched(W, "Blackbird", V3(10.f, 0.f, 0.f));
			W.Hour = W.Sunrise - 0.5f;
			EVoice V;
			const float Dawn = W.VoiceNow(W.Birds[Id], V);
			const bool bSong = V == EVoice::Song;
			W.Sites[W.Birds[Id].Site].Kind = EHabitat::Meadow;
			W.VoiceNow(W.Birds[Id], V);
			const bool bGroundSong = V == EVoice::Song;
			W.Sites[W.Birds[Id].Site].Kind = EHabitat::Broadleaf;
			W.Rain = 4.f;
			const float Wet = W.VoiceNow(W.Birds[Id], V);
			const bool bRainSong = V == EVoice::Song;
			W.Rain = 0.f;
			W.Hour = 13.5f;
			W.TempC = 18.f;
			W.Sites[W.Birds[Id].Site].Kind = EHabitat::Meadow;
			const float Mild = W.VoiceNow(W.Birds[Id], V);
			W.TempC = 30.f;
			const float Hot = W.VoiceNow(W.Birds[Id], V);
			W.Sites[W.Birds[Id].Site].Kind = EHabitat::Broadleaf;
			W.VoiceNow(W.Birds[Id], V);
			const bool bHotSong = V == EVoice::Song;
			W.TempC = 15.f;
			W.Hour = W.Sunset - 0.5f;
			const float Dusk = W.VoiceNow(W.Birds[Id], V);
			const bool bDuskSong = V == EVoice::Song;
			Check(bSong && !bGroundSong, F("a blackbird sings from its perch (%.1f songs a minute at dawn), never from the ground", Dawn));
			Check(!bRainSong && Wet < Dawn * 0.3f, F("in the rain it does not sing and calls little (%.2f a minute)", Wet));
			Check(Hot < Mild * 0.5f && !bHotSong, F("in the midday heat it is quiet: %.2f calls a minute at 30 C against %.2f at 18 C, and no song", Hot, Mild));
			Check(bDuskSong && Dusk > 0.f && Dusk < Dawn, F("it sings again at dusk (%.1f a minute), less than at dawn", Dusk));
		}
	}

	void AlarmSpread()
	{
		// a row of gardens: a great tit on a twig every 12 m for 240 m, a cat under the first
		FWorld W;
		Empty(W, 130.f, 9.f);
		W.Opt.MidDist = 400.f;
		W.HearDist = 400.f;            // the ear is not what is tested here
		W.MaxVoices = 16;
		std::vector<int> Tits;
		for (int K = 0; K <= 20; K++)
		{
			Tits.push_back(Perched(W, K % 2 ? "BlueTit" : "GreatTit", V3(12.f * (float)K, 0.f, 0.f), 3.5f));
		}
		const V3 Cat(0.f, 4.f, Ground);
		std::vector<float> First(Tits.size(), -1.f);
		float T = 0.f;
		int Alarms = 0, Others = 0;
		float SumDist = 0.f;
		// a calm minute first: nobody alarms
		Step(W, 20.f, 0.1f, [&]
		{
			for (const FEvent& E : W.Events)
			{
				Others += E.Voice == EVoice::Alarm ? 1 : 0;
			}
		});
		Check(Others == 0, "no cat: no alarm calls in the gardens");
		Step(W, 45.f, 0.1f, [&]
		{
			T += 0.1f;
			W.Beasts.assign(1, FBeast{ Cat, V3(0.3f, 0.f, 0.f), true, false });
			for (size_t K = 0; K < Tits.size(); K++)
			{
				if (First[K] < 0.f && W.Birds[Tits[K]].AlarmT > 0.f)
				{
					First[K] = T;
				}
			}
			for (const FEvent& E : W.Events)
			{
				if (E.Voice == EVoice::Alarm)
				{
					Alarms++;
					SumDist += (E.Pos - Cat).Len2D();
				}
			}
		});
		int Near = 0, NearIn = 0, Far = 0, FarIn = 0;
		float Slowest = 0.f, Reach = 0.f;
		for (size_t K = 0; K < Tits.size(); K++)
		{
			const float D = 12.f * (float)K;
			if (D <= 36.f)
			{
				Near++;
				NearIn += First[K] >= 0.f && First[K] < 8.f ? 1 : 0;
				Slowest = std::max(Slowest, First[K]);
			}
			if (D >= 84.f)
			{
				Far++;
				FarIn += First[K] >= 0.f ? 1 : 0;
			}
			if (First[K] >= 0.f)
			{
				Reach = std::max(Reach, D);
			}
		}
		Check(First[0] >= 0.f && First[0] < 3.f, F("the tit over the cat gives the alarm within %.1f s", First[0]));
		Check(NearIn == Near, F("its neighbours take it up: all %d birds within 36 m alarm within %.1f s", Near, Slowest));
		Check(FarIn == 0 && Reach < 84.f, F("the alarm does not cross the town: it reached %.0f m, none of the %d birds from 84 m on took it up in 45 s", Reach, Far));
		Check(Alarms > 20 && SumDist / std::max(Alarms, 1) < 35.f, F("the cat can be found by ear: %d alarm calls, on average %.0f m from it", Alarms, SumDist / std::max(Alarms, 1)));
		// the cat gone: it dies down
		W.Beasts.clear();
		Step(W, 40.f, 0.1f);
		int Still = 0;
		for (int Id : Tits)
		{
			Still += W.Birds[Id].AlarmT > 0.f ? 1 : 0;
		}
		Check(Still == 0, "the cat gone, the alarm has died down within 40 s");
	}

	void Flocks()
	{
		FWorld W;
		Empty(W, 130.f, 10.f);
		const int Sp = FindSpecies("HouseSparrow");
		const int Lone = W.AddBird(Sp, V3(10.f, 0.f, Ground));
		const int Flock = W.AddFlock(Sp, V3(10.f, 40.f, Ground), 24, 4.f);
		EVoice V;
		const float One = W.VoiceNow(W.Birds[Lone], V);
		const float Many = W.VoiceNow(W.Birds[Flock], V);
		Check(Many > One * 1.3f && Many < One * 2.1f && V == EVoice::Contact, F("contact calls: a sparrow in a flock of 24 calls %.1f times a minute, alone %.1f (the flock together %.0f, 24 lone ones %.0f)", Many, One, Many * 24.f, One * 24.f));

		// the roost: starlings talk as they gather, and not in the middle of the day
		Empty(W, 300.f, 12.f);
		const int St = W.AddFlock(FindSpecies("Starling"), V3(10.f, 0.f, Ground), 40, 5.f);
		W.Groups[W.Birds[St].Group].Plan = 3;
		W.Hour = W.Sunset - 0.5f;
		const float Evening = W.VoiceNow(W.Birds[St], V);
		const bool bChatter = V == EVoice::Chatter;
		W.Groups[W.Birds[St].Group].Plan = 1;
		W.Hour = 12.f;
		W.VoiceNow(W.Birds[St], V);
		Check(bChatter && Evening > 8.f && V != EVoice::Chatter, F("starlings gathering before the roost chatter (%.0f a minute each); at midday on the field they only call", Evening));

		// the gulls' long call on the morning beach
		Empty(W, 130.f, 12.f);
		const int Gull = W.AddFlock(FindSpecies("HerringGull"), V3(10.f, 0.f, Ground), 6, 6.f);
		W.Sites[W.Birds[Gull].Site].Kind = EHabitat::Beach;
		W.Hour = W.Sunrise + 1.f;
		W.VoiceNow(W.Birds[Gull], V);
		const bool bLong = V == EVoice::LongCall;
		W.Hour = 15.f;
		W.VoiceNow(W.Birds[Gull], V);
		Check(bLong && V == EVoice::Contact, "herring gulls give their long call on the beach in the morning, not in the afternoon");
	}

	void Budget()
	{
		// a hundred sparrows round the player and a noisy minute: never more voices at once than the budget, the near
		// ones first
		FWorld W;
		Empty(W, 130.f, 10.f);
		const int Sp = FindSpecies("HouseSparrow");
		MutableSpeciesTable()[Sp].CallRate *= 6.f;
		const int First = W.AddFlock(Sp, V3(0.f, 0.f, Ground), 100, 45.f);
		int Over = 0, Said = 0;
		double SaidDist = 0.0;
		Step(W, 60.f, 1.f / 30.f, [&]
		{
			Over += W.VoicesNow() > W.MaxVoices ? 1 : 0;
			for (const FEvent& E : W.Events)
			{
				if (E.Kind == EEvent::Call || E.Kind == EEvent::Song)
				{
					Said++;
					SaidDist += (E.Pos - W.Camera).Len();
				}
			}
		});
		double AllDist = 0.0;
		for (int K = 0; K < 100; K++)
		{
			AllDist += (W.Birds[First + K].Pos - W.Camera).Len();
		}
		MutableSpeciesTable()[Sp].CallRate /= 6.f;
		Check(Over == 0 && W.VoicesPeak <= W.MaxVoices && W.VoicesPeak >= 3, F("voice budget: at most %d voices at once (the budget is %d) in a minute of 100 noisy sparrows", W.VoicesPeak, W.MaxVoices));
		Check(W.VoicesDropped > 0 && Said == W.VoicesSaid, F("%d voices heard, %d kept back", W.VoicesSaid, W.VoicesDropped));
		Check(SaidDist / std::max(Said, 1) < AllDist / 100.0 * 0.85, F("the near ones first: heard voices are %.0f m away on average, the flock %.0f m", SaidDist / std::max(Said, 1), AllDist / 100.0));

		// out of earshot: nothing at all
		Empty(W, 130.f, 10.f);
		W.AddFlock(Sp, V3(140.f, 0.f, Ground), 60, 10.f);
		int Events = 0;
		Step(W, 60.f, 1.f / 30.f, [&]
		{
			for (const FEvent& E : W.Events)
			{
				Events += (E.Kind == EEvent::Call || E.Kind == EEvent::Song) ? 1 : 0;
			}
		});
		Check(Events == 0 && W.VoicesSaid == 0 && W.VoicesDropped == 0, "a flock 140 m away makes no voices at all (nothing to play, nothing to drop)");

		// the night: sleeping birds are silent
		Empty(W, 130.f, 1.f);
		const int Night = W.AddFlock(Sp, V3(5.f, 0.f, Ground), 30, 5.f);
		for (int K = 0; K < 30; K++)
		{
			W.Birds[Night + K].Act = EAct::Sleep;
			W.Birds[Night + K].ActLen = 1e9f;
		}
		W.Groups[W.Birds[Night].Group].Plan = 2;
		Step(W, 60.f, 1.f / 30.f);
		Check(W.VoicesSaid == 0, "at night the sleeping flock by the player is silent");
	}
}

int VoiceTests(int* Passed)
{
	using namespace VoiceTest;
	Pass = Fail = 0;
	std::printf("voices: the dawn chorus\n");
	Chorus();
	std::printf("voices: the alarm\n");
	AlarmSpread();
	std::printf("voices: flocks, the roost, the beach\n");
	Flocks();
	std::printf("voices: the budget\n");
	Budget();
	std::printf("voices: %d passed, %d failed\n", Pass, Fail);
	if (Passed)
	{
		*Passed = Pass;
	}
	return Fail;
}
