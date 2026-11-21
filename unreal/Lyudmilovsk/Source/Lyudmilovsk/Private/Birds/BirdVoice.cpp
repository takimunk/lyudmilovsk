// Bird voices that mean something (docs/bird_voices.md). Plain C++, part of the bird life model (BirdSim.h): what a bird
// says follows what it is doing. Songs come from singing perches in the breeding months, at dawn in the species' real
// order; flocks keep up a murmur of contact calls; an alarm at a cat or a hawk is taken up by the neighbours for some
// tens of metres, so the danger can be found by ear; starlings chatter at the roost, gulls give their long calls on the
// morning beach. Rain and the midday heat quiet them. Only birds near the camera have a voice, and only a few of those
// sound at once (FWorld::MaxVoices): the nearest and the loudest.
#include "BirdNest.h"
#include "BirdSim.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace LyudBirds
{
	// (named, not anonymous, and always spelled out: in a unity build the other bird files' helpers share one translation
	// unit with these)
	namespace VoiceDetail
	{
		float Clamp(float V, float A, float B) { return V < A ? A : (V > B ? B : V); }

		struct FRow
		{
			const char* Id;
			FVoiceTraits T;
		};

		float Day(int Month, int D)
		{
			static const int Before[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
			return (float)(Before[Month - 1] + D);
		}

		FVoiceTraits Singer(float ChorusMin, float From, float To, bool bEvening, bool bScolds, float Loud, bool bAutumn = false)
		{
			FVoiceTraits T;
			T.bSings = true;
			T.ChorusMin = ChorusMin;
			T.SongFrom = From;
			T.SongTo = To;
			T.bEvening = bEvening;
			T.bScolds = bScolds;
			T.Loud = Loud;
			T.bAutumn = bAutumn;
			return T;
		}

		FVoiceTraits Caller(bool bScolds, float Loud, bool bChatters = false, bool bLongCalls = false)
		{
			FVoiceTraits T;
			T.bScolds = bScolds;
			T.Loud = Loud;
			T.bChatters = bChatters;
			T.bLongCalls = bLongCalls;
			return T;
		}

		// The dawn chorus' order is the one field guides give for northern Europe: the big-eyed birds of the undergrowth
		// first, in the dark (robin, blackbird, song thrush), then the woodpigeon and the tits, then the finches at first
		// light, the sparrows and the starlings on the roofs only when the sun is nearly up.
		const std::vector<FRow>& Rows()
		{
			static const std::vector<FRow> R = {
				//                         before sunrise  from          to           evening scolds loud
				{ "Robin",        Singer(60.f, Day(1, 15), Day(7, 10), true,  true,  0.75f, true) },
				{ "Blackbird",    Singer(50.f, Day(2, 20), Day(7, 20), true,  true,  1.0f) },
				{ "SongThrush",   Singer(40.f, Day(2, 10), Day(7, 10), true,  false, 1.0f) },
				{ "WoodPigeon",   Singer(30.f, Day(3, 1),  Day(8, 31), false, false, 0.9f) },
				{ "GreatTit",     Singer(25.f, Day(1, 15), Day(6, 15), false, true,  0.85f) },
				{ "BlueTit",      Singer(20.f, Day(2, 1),  Day(6, 15), false, true,  0.7f) },
				{ "BarnSwallow",  Singer(15.f, Day(4, 20), Day(7, 31), false, true,  0.6f) },
				{ "Chaffinch",    Singer(10.f, Day(3, 1),  Day(7, 10), false, true,  0.85f) },
				{ "Goldfinch",    Singer(5.f,  Day(3, 15), Day(7, 31), false, false, 0.65f) },
				{ "Bullfinch",    Singer(5.f,  Day(3, 1),  Day(6, 30), false, false, 0.5f) },
				{ "WhiteWagtail", Singer(0.f,  Day(4, 1),  Day(7, 15), false, true,  0.6f) },
				{ "Starling",     Singer(0.f,  Day(2, 15), Day(6, 15), false, true,  0.75f) },
				{ "HouseSparrow", Singer(-5.f, Day(3, 1),  Day(8, 31), false, true,  0.7f) },
				{ "TreeSparrow",  Singer(-5.f, Day(3, 1),  Day(8, 31), false, true,  0.65f) },
				{ "FeralPigeon",  Singer(-10.f, 1.f,       365.f,      false, false, 0.6f) },
				{ "Magpie",       Caller(true, 0.9f) },
				{ "Jay",          Caller(true, 1.0f) },
				{ "Jackdaw",      Caller(true, 0.9f, true) },
				{ "HoodedCrow",   Caller(true, 1.0f) },
				{ "CarrionCrow",  Caller(true, 1.0f) },
				{ "Rook",         Caller(false, 1.0f, true) },
				{ "Raven",        Caller(false, 1.0f) },
				{ "Nutcracker",   Caller(true, 0.9f) },
				{ "Fieldfare",    Caller(true, 0.85f) },
				{ "Waxwing",      Caller(false, 0.5f) },
				{ "HerringGull",  Caller(false, 1.0f, false, true) },
				{ "CommonGull",   Caller(false, 0.95f, false, true) },
				{ "BlackHeadedGull", Caller(false, 0.9f, false, true) },
				{ "CommonSwift",  Caller(false, 0.9f) },
				{ "SandMartin",   Caller(false, 0.5f, true) },
			};
			return R;
		}

		// the starling both sings and chatters at the roost
		FVoiceTraits Make(const std::string& Id)
		{
			for (const FRow& R : Rows())
			{
				if (Id == R.Id)
				{
					FVoiceTraits T = R.T;
					if (Id == "Starling" || Id == "HouseSparrow" || Id == "TreeSparrow")
					{
						T.bChatters = true;
					}
					return T;
				}
			}
			return FVoiceTraits();
		}

		float Smooth01(float X)
		{
			X = VoiceDetail::Clamp(X, 0.f, 1.f);
			return X * X * (3.f - 2.f * X);
		}

		bool InDays(float Doy, float From, float To)
		{
			return From <= To ? (Doy >= From && Doy <= To) : (Doy >= From || Doy <= To);
		}

		float Lasts(EVoice V)
		{
			switch (V)
			{
			case EVoice::Song: return 2.4f;
			case EVoice::Alarm: return 0.5f;
			case EVoice::Beg: return 0.5f;
			case EVoice::Chatter: return 1.4f;
			case EVoice::LongCall: return 3.f;
			default: return 0.6f;
			}
		}
	}

	const char* VoiceName(EVoice V)
	{
		static const char* Names[] = { "contact", "song", "alarm", "beg", "chatter", "long_call" };
		return (int)V < (int)EVoice::Num ? Names[(int)V] : "?";
	}

	const FVoiceTraits& VoiceTraits(int Species)
	{
		static std::vector<FVoiceTraits> Table;
		const std::vector<FSpecies>& S = SpeciesTable();
		if (Table.size() != S.size())
		{
			Table.clear();
			for (const FSpecies& Sp : S)
			{
				Table.push_back(VoiceDetail::Make(Sp.Id));
			}
		}
		static const FVoiceTraits None;
		return Species >= 0 && Species < (int)Table.size() ? Table[Species] : None;
	}

	int FWorld::VoicesNow() const
	{
		int N = 0;
		for (int I = 0; I < MaxVoices && I < 16; I++)
		{
			N += VoiceSlots[I].Until > Time ? 1 : 0;
		}
		return N;
	}

	bool FWorld::Say(const FBird& B, EVoice V)
	{
		const FVoiceTraits& T = VoiceTraits(B.Species);
		const float Dist = (B.Pos - Camera).Len();
		// an alarm and a long call carry; young beg thinly
		const float Loud = VoiceDetail::Clamp(T.Loud * (V == EVoice::Alarm ? 1.25f : V == EVoice::LongCall ? 1.2f : V == EVoice::Beg ? 0.6f : 1.f), 0.2f, 1.f);
		if (Dist > HearDist * (0.5f + 0.5f * Loud))
		{
			return false;                 // out of earshot: not made at all
		}
		const int Max = std::min(MaxVoices, 16);
		int Free = -1, Busy = 0;
		for (int I = 0; I < Max; I++)
		{
			if (VoiceSlots[I].Until > Time)
			{
				Busy++;
			}
			else if (Free < 0)
			{
				Free = I;
			}
		}
		// the more voices are sounding, the nearer (or louder) the next must be: the last place is for a bird a few steps
		// away, the first for anything in earshot
		const float Room = 1.f - (float)Busy / (float)std::max(Max, 1);
		const float Reach = HearDist * (0.5f + 0.5f * Loud) * std::pow(std::max(Room, 0.f), 0.8f);
		if (Free < 0 || Dist > Reach)
		{
			VoicesDropped++;
			return false;
		}
		VoiceSlots[Free].Until = Time + VoiceDetail::Lasts(V);
		VoicesSaid++;
		VoicesPeak = std::max(VoicesPeak, Busy + 1);
		Events.push_back({ V == EVoice::Song ? EEvent::Song : EEvent::Call, B.Id, B.Pos, V });
		return true;
	}

	void FWorld::RaiseAlarm(FBird& B, const V3& Danger)
	{
		if (B.AlarmT <= 0.f)
		{
			B.AlarmHop = 0;
			B.CallTimer = std::min(B.CallTimer, 0.2f + Rand(B.Rng) * 0.6f);      // it speaks up at once
		}
		B.AlarmT = std::max(B.AlarmT, 3.f);
		B.AlarmWait = 0.f;
		B.AlarmFrom = Danger;
	}

	void FWorld::SpreadAlarm(const FBird& From)
	{
		// taken up from bird to bird, but only a few birds deep and only so far from the danger itself: the garden knows,
		// the next street does not
		if (From.AlarmHop >= 3)
		{
			return;
		}
		ForNear(From.Pos, 24.f, [&](int I)
		{
			FBird& B = Birds[I];
			if (I == From.Id || B.bGone || B.Tier > 1 || B.Act == EAct::Sleep || B.Act == EAct::Claimed || IsAirborne(B.Act))
			{
				return;
			}
			if (B.AlarmT > 0.f || B.AlarmWait > 0.f || B.AlarmCool > 0.f || !VoiceTraits(B.Species).bScolds || SpeciesTable()[B.Species].bPredator)
			{
				return;
			}
			if ((B.Pos - From.AlarmFrom).Len() > 60.f)
			{
				return;
			}
			B.AlarmWait = 0.4f + Rand(B.Rng) * 1.2f;
			B.AlarmHop = (uint8_t)(From.AlarmHop + 1);
			B.AlarmFrom = From.AlarmFrom;
			B.Alert = std::min(std::max(B.Alert, 0.3f), 0.7f);     // uneasy, looking that way; it does not fly for a call
			if (!B.bLook)
			{
				B.Look = From.AlarmFrom + V3(0.f, 0.f, 0.2f);
				B.bLook = true;
			}
		});
	}

	float FWorld::VoiceNow(const FBird& B, EVoice& V) const
	{
		const FSpecies& S = SpeciesTable()[B.Species];
		const FVoiceTraits& T = VoiceTraits(B.Species);
		const bool bAir = IsAirborne(B.Act);
		V = EVoice::Contact;
		if (B.AlarmT > 0.f && !bAir)
		{
			V = EVoice::Alarm;
			return S.Length < 0.3f ? 45.f : 25.f;
		}
		// rain and the midday heat quiet them (a drizzle a little, a downpour nearly all)
		float Quiet = 1.f - 0.85f * VoiceDetail::Smooth01(Rain / 2.5f);
		if (Hour > 11.5f && Hour < 16.f)
		{
			Quiet *= 1.f - 0.7f * VoiceDetail::Smooth01((TempC - 24.f) / 6.f);
		}
		// how many of its flock are round it, awake: a bigger flock keeps up a denser murmur
		int Round = 0;
		if (B.Group >= 0 && B.Group < (int)Groups.size())
		{
			for (int Id : Groups[B.Group].Members)
			{
				const FBird& O = Birds[Id];
				Round += (Id != B.Id && !O.bGone && O.Act != EAct::Sleep && (O.Pos - B.Pos).Len2D() < 25.f) ? 1 : 0;
			}
		}
		// (each bird a little more talkative in company: with the flock's numbers that is a much denser murmur)
		const float Density = std::min(2.f, 1.f + 0.2f * std::log(1.f + (float)Round));
		const FSite* Site = B.Site >= 0 && B.Site < (int)Sites.size() ? &Sites[B.Site] : nullptr;
		const bool bGroundSite = Site && IsGroundHabitat(Site->Kind);
		if (!bAir)
		{
			// the song: from a perch off the ground, in the months it breeds, at dawn in its turn (and less through the
			// morning, a little all day, again at dusk for the thrushes and the robin)
			const bool bNestSong = Nest && Nest->DutyOf(B.Id) != EDuty::None;        // the nests' males have their own song posts
			if (T.bSings && !bNestSong && B.Act == EAct::Perch && Site && !bGroundSite && Quiet > 0.5f)
			{
				const bool bSeason = VoiceDetail::InDays(DayOfYear, T.SongFrom, T.SongTo);
				const bool bAutumn = T.bAutumn && (DayOfYear >= 244.f || DayOfYear < T.SongFrom);
				if (bSeason || bAutumn)
				{
					const float Start = Sunrise - T.ChorusMin / 60.f;
					float K = 0.f;
					if (Hour >= Start && Hour < Start + 1.75f)
					{
						K = 1.f;
					}
					else if (Hour >= Start + 1.75f && Hour < Sunrise + 4.f)
					{
						K = 0.25f;
					}
					else if (T.bEvening && Hour > Sunset - 1.2f && Hour < Sunset + 0.5f)
					{
						K = 0.6f;
					}
					else if (Hour >= Sunrise + 4.f && Hour < Sunset - 1.2f)
					{
						K = 0.08f;
					}
					K *= bSeason ? 1.f : 0.4f;
					if (K > 0.f)
					{
						V = EVoice::Song;
						return std::max(S.DawnSong, 3.f) * K * Quiet;
					}
				}
			}
			// the roost: starlings, sparrows, jackdaws and rooks talk as they gather and before they leave
			if (T.bChatters && B.Group >= 0 && Round >= 3)
			{
				const int Plan = Groups[B.Group].Plan;
				const bool bEvening = (Plan == 2 || Plan == 3) && Hour > Sunset - 1.2f && Hour < Sunset + 0.4f;
				const bool bMorning = Plan == 2 && Hour > Sunrise - 0.5f && Hour < Sunrise + 0.4f;
				if (bEvening || bMorning)
				{
					V = EVoice::Chatter;
					return 6.f * Density * Quiet;
				}
			}
			// the gulls' long call: on the beach and the roofs in the first hours of the day
			if (T.bLongCalls && Site && (Site->Kind == EHabitat::Beach || Site->Kind == EHabitat::Roof) && Hour > Sunrise - 0.2f && Hour < Sunrise + 3.f)
			{
				V = EVoice::LongCall;
				return 2.f * Quiet;
			}
		}
		// (awake in the dark, it keeps its voice down)
		const float Night = (Hour < Sunrise - 1.2f || Hour > Sunset + 0.8f) ? 0.1f : 1.f;
		return S.CallRate * Density * Quiet * Night * (bAir ? 0.6f : 1.f);
	}

	void FWorld::ThinkVoice(FBird& B, float Dt)
	{
		B.AlarmCool = std::max(0.f, B.AlarmCool - Dt);
		if (B.AlarmWait > 0.f)
		{
			B.AlarmWait -= Dt;
			if (B.AlarmWait <= 0.f)
			{
				B.AlarmT = 4.f + Rand(B.Rng) * 3.f;
				B.CallTimer = 0.f;
			}
		}
		if (B.AlarmT > 0.f)
		{
			B.AlarmT -= Dt;
			if (B.AlarmT <= 0.f)
			{
				B.AlarmCool = 20.f;       // it has said its piece: the same alarm does not come back round to it
			}
		}
		B.CallTimer -= Dt;
		if (B.CallTimer > 0.f)
		{
			return;
		}
		// nothing more for a bird out of earshot: it is asked again in a while
		if ((B.Pos - Camera).Len() > HearDist * 1.3f)
		{
			B.CallTimer = 4.f + Rand(B.Rng) * 6.f;
			return;
		}
		const FVoiceTraits& T = VoiceTraits(B.Species);
		// a hawk in sight of a perched bird: the same alarm as for a cat
		if (T.bScolds && B.AlarmT <= 0.f && !IsAirborne(B.Act))
		{
			ForNear(B.Pos, 30.f, [&](int I)
			{
				const FBird& H = Birds[I];
				if (!H.bGone && SpeciesTable()[H.Species].bPredator && H.Act != EAct::Sleep && (!Sight || Sight(B.Pos, H.Pos)))
				{
					RaiseAlarm(B, H.Pos);
				}
			});
		}
		EVoice V = EVoice::Contact;
		const float Rate = VoiceNow(B, V);
		if (Rate <= 0.02f)
		{
			B.CallTimer = 8.f + Rand(B.Rng) * 12.f;
			return;
		}
		B.CallTimer = 60.f / Rate * (0.4f + 1.2f * Rand(B.Rng));
		Say(B, V);
		if (V == EVoice::Alarm)
		{
			SpreadAlarm(B);              // heard by its neighbours whether or not the player's ear had room for it
		}
	}
}
