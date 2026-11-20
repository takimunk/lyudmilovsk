// Records, the jukebox and the town radio «Волна Людмиловска» (docs/music.md), in plain C++ (no Unreal types) so the same
// rules run in the game (ULyudMusic) and in the cloud (tools/musicsim/run.sh).
//
// Data: Content/City/music.json (tools/music/music.py): the tracks (the game's music cues), the records (an item each, one
// track each), which city acts are jukeboxes, record players, radios or the request box (globs over act ids), and the
// station: a day's schedule of segments (music hours, news, weather and tides, the request show, the shipping forecast,
// sign-on and sign-off), the word lists the announcer reads from.
// What this file decides: what a city act is for the music; the station's air at any game moment (which track plays and from
// which second, the talk-bed under the words, the caption on screen, the chime and the time pips), the same on every radio
// because it is a pure function of the clock; the record a player puts on next; the hero's request (posted on a postcard,
// read in the next evening's request show); what is kept in the save (the radios switched on or off, each player's last
// record, the request, the last record played) as a flat string map.
#pragma once

#include "../People/MiniJson.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace LyudMusic
{
	/** What a city act is for the music. */
	enum class ERole : uint8_t { None, Jukebox, Player, Radio, Request };

	/** What the station is doing in a segment. */
	enum class EKind : uint8_t { OffAir, Music, News, Weather, Shipping, Requests, SignOn, SignOff };

	inline const char* KindName(EKind K)
	{
		switch (K)
		{
		case EKind::Music: return "music";
		case EKind::News: return "news";
		case EKind::Weather: return "weather";
		case EKind::Shipping: return "shipping";
		case EKind::Requests: return "requests";
		case EKind::SignOn: return "sign_on";
		case EKind::SignOff: return "sign_off";
		default: return "off_air";
		}
	}

	inline EKind KindOf(const std::string& S)
	{
		static const EKind All[] = { EKind::Music, EKind::News, EKind::Weather, EKind::Shipping, EKind::Requests, EKind::SignOn, EKind::SignOff };
		for (EKind K : All)
		{
			if (S == KindName(K))
			{
				return K;
			}
		}
		return EKind::OffAir;
	}

	/** Glob with '*' only (act ids). */
	inline bool Glob(const char* P, const char* S)
	{
		if (*P == 0)
		{
			return *S == 0;
		}
		if (*P == '*')
		{
			for (const char* T = S;; ++T)
			{
				if (Glob(P + 1, T))
				{
					return true;
				}
				if (*T == 0)
				{
					return false;
				}
			}
		}
		return *S != 0 && *P == *S && Glob(P + 1, S + 1);
	}

	inline uint32_t Hash(uint32_t A, uint32_t B = 0, uint32_t C = 0)
	{
		uint32_t H = 2166136261u;
		const uint32_t V[3] = { A, B, C };
		for (uint32_t X : V)
		{
			for (int I = 0; I < 4; ++I)
			{
				H = (H ^ ((X >> (I * 8)) & 0xffu)) * 16777619u;
			}
		}
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		H ^= H >> 15;
		return H;
	}

	inline uint32_t HashStr(const std::string& S)
	{
		uint32_t H = 2166136261u;
		for (unsigned char C : S)
		{
			H = (H ^ C) * 16777619u;
		}
		return H;
	}

	struct FTrack
	{
		std::string Cue;      // dawn_1: /Game/Audio/Music/MU_dawn_1
		double Seconds = 120.0;
		std::string Title, Artist;   // string keys
	};

	struct FRecord
	{
		std::string Id;       // the item: record_dawn_1
		int Track = -1;
		std::string Note;     // music.rec.<cue>.note
		std::string JukeboxLine;  // city.jb.<cue>
	};

	struct FSegment
	{
		int AtMin = 0;          // minutes after midnight
		int EndMin = 0;         // the next segment's start (or off air)
		EKind Kind = EKind::Music;
		std::string Pool;
		std::string Show;       // string key of the programme's name
		int DaysMask = 0x7f;    // bit per weekday, 0 = Monday
		int Minutes = 0;        // talk segments: how long the words run (the rest is the bed or music)
	};

	/** A word of a caption: a string key to localise or a literal (a number, a time). */
	struct FArg
	{
		std::string S;
		bool bKey = false;
	};

	struct FCaption
	{
		double Start = 0.0, Dur = 0.0;    // seconds of the day
		std::string Key;
		std::vector<FArg> Args;
		bool bHeroRequest = false;
	};

	struct FPlay
	{
		double Start = 0.0, Dur = 0.0;    // seconds of the day the track starts and how long it plays here
		int Track = -1;
		double From = 0.0;                // the track's own second at Start
		float Volume = 1.f;               // 1, or the talk bed's level
	};

	struct FSfx
	{
		double At = 0.0;
		std::string Sound;                // radio_chime, radio_pips
	};

	/** What the engine knows of the moment that the words need (weather, tide, the paper's front page). */
	struct FFacts
	{
		float TempC = 10.f;
		float WindSpeed = 5.f;
		float WindFromDeg = 235.f;
		float Cloud = 0.5f, Rain = 0.f, Snow = 0.f, Fog = 0.f, Thunder = 0.f;
		bool bTideRising = false;
		std::string TideTime;       // "HH:MM" of the next high (rising) or low (falling) water
		std::string PaperHeadline;  // literal text, empty without the paper
		int Season = 0;             // 0 winter, 1 spring, 2 summer, 3 autumn
	};

	/** One segment laid out: what plays when, what is said when. */
	struct FTimeline
	{
		int Segment = -1;
		EKind Kind = EKind::OffAir;
		std::string Show;
		std::vector<FPlay> Plays;
		std::vector<FCaption> Caps;
		std::vector<FSfx> Sfx;
	};

	/** The hero's request: a track, the dedication, the day it was posted. */
	struct FRequest
	{
		int Track = -1;
		int Dedication = 0;
		long long PostedDay = -1;
		bool Valid() const { return Track >= 0 && PostedDay >= 0; }
	};

	/** The air at a moment. */
	struct FAir
	{
		EKind Kind = EKind::OffAir;
		int Segment = -1;
		std::string Show;
		const FPlay* Play = nullptr;     // into Timeline
		const FCaption* Cap = nullptr;
		FTimeline Timeline;
		FAir() = default;
		FAir(const FAir&) = delete;
		FAir& operator=(const FAir&) = delete;
	};

	class FModel
	{
	public:
		std::vector<FTrack> Tracks;
		std::vector<FRecord> Records;
		std::map<std::string, std::vector<int>> Pools;   // pool -> tracks
		std::vector<FSegment> Schedule;
		std::map<std::string, std::vector<std::string>> Roles;   // jukebox, player, radio, not_radio, request, auto_on, replace
		std::vector<std::string> News, Requests, Dedications, ShipAreas;
		std::vector<std::string> SeasonNews[4];
		std::map<std::string, std::string> WeatherPools;
		double RangeM = 7.0;
		float BedVolume = 0.35f;
		double CaptionSeconds = 6.5;
		int NewsPerBulletin = 3, RequestsPerHour = 4, ShippingAreas = 6;
		int AutoOnFrom = 7 * 60, AutoOnTo = 22 * 60;
		std::string StationName = "music.radio.name";
		std::string RequestCollect;   // "records/{record}" style: a stamp a heard request gives (empty: none)

		// state (saved)
		std::set<std::string> RadiosOn;            // switched on by the hero
		std::set<std::string> RadiosOff;           // auto radios switched off by the hero (until the next day)
		long long RadiosOffDay = -1;
		std::map<std::string, int> PlayerLast;     // thing -> index into the owned list it played last
		FRequest Request;
		int LastRecord = -1;                       // the record last played by the hero (jukebox or player)
		long long RequestsHeard = 0;

		bool Load(const std::string& Text, std::string& Error)
		{
			LyudJson::FValue J;
			if (!LyudJson::Parse(Text, J, Error))
			{
				return false;
			}
			*this = FModel();
			for (const LyudJson::FValue& T : J["tracks"].Items)
			{
				FTrack Tr;
				Tr.Cue = T["cue"].S();
				Tr.Seconds = std::max(10.0, (double)T["seconds"].F(120.f));
				Tr.Title = T["title"].S();
				Tr.Artist = T["artist"].S();
				Tracks.push_back(Tr);
				for (const LyudJson::FValue& P : T["pools"].Items)
				{
					Pools[P.S()].push_back((int)Tracks.size() - 1);
				}
			}
			for (const LyudJson::FValue& R : J["records"].Items)
			{
				FRecord Rec;
				Rec.Id = R["id"].S();
				Rec.Track = FindTrack(R["cue"].S());
				Rec.Note = R["note"].S();
				Rec.JukeboxLine = R["jukebox_line"].S();
				if (Rec.Track < 0)
				{
					Error = "record " + Rec.Id + " has an unknown cue";
					return false;
				}
				Records.push_back(Rec);
				Pools["records"].push_back(Rec.Track);
			}
			for (const auto& F : J["roles"].Fields)
			{
				for (const LyudJson::FValue& G : F.second.Items)
				{
					Roles[F.first].push_back(G.S());
				}
			}
			const LyudJson::FValue& St = J["station"];
			RangeM = St["range_m"].F(7.f);
			BedVolume = St["talk_bed_volume"].F(0.35f);
			CaptionSeconds = std::max(2.0, (double)St["caption_seconds"].F(6.5f));
			NewsPerBulletin = St["news_per_bulletin"].I(3);
			RequestsPerHour = std::max(1, St["requests_per_hour"].I(4));
			ShippingAreas = St["shipping_areas"].I(6);
			StationName = St["name"].S(StationName);
			RequestCollect = St["request_collect"].S();
			if (St["auto_on_hours"].Size() == 2)
			{
				AutoOnFrom = St["auto_on_hours"][0].I(7) * 60;
				AutoOnTo = St["auto_on_hours"][1].I(22) * 60;
			}
			for (const auto& F : St["weather_pools"].Fields)
			{
				WeatherPools[F.first] = F.second.S();
			}
			auto Strings = [](const LyudJson::FValue& A, std::vector<std::string>& Out)
			{
				for (const LyudJson::FValue& V : A.Items)
				{
					Out.push_back(V.S());
				}
			};
			Strings(St["news"], News);
			Strings(St["requests"], Requests);
			Strings(St["dedications"], Dedications);
			Strings(St["ship_areas"], ShipAreas);
			static const char* const SeasonNames[4] = { "winter", "spring", "summer", "autumn" };
			for (int S = 0; S < 4; ++S)
			{
				Strings(St["news_season"][SeasonNames[S]], SeasonNews[S]);
			}
			int OffFrom = -1;
			if (St["off_air"].Size() == 2)
			{
				OffFrom = Minutes(St["off_air"][0].S());
			}
			for (const LyudJson::FValue& Sg : St["schedule"].Items)
			{
				FSegment S;
				S.AtMin = Minutes(Sg["at"].S());
				S.Kind = KindOf(Sg["kind"].S());
				S.Pool = Sg["pool"].S();
				S.Show = Sg["show"].S();
				S.Minutes = Sg["minutes"].I(0);
				S.DaysMask = DaysMask(Sg["days"].S());
				if (S.AtMin < 0 || S.Kind == EKind::OffAir)
				{
					Error = "bad schedule entry at " + Sg["at"].S();
					return false;
				}
				if (S.Kind == EKind::Music || S.Kind == EKind::Requests)
				{
					if (Pools[S.Pool].empty())
					{
						Error = "schedule " + Sg["at"].S() + ": empty pool " + S.Pool;
						return false;
					}
				}
				Schedule.push_back(S);
			}
			std::stable_sort(Schedule.begin(), Schedule.end(), [](const FSegment& A, const FSegment& B) { return A.AtMin < B.AtMin; });
			(void)OffFrom;
			if (Tracks.empty() || Schedule.empty())
			{
				Error = "no tracks or no schedule";
				return false;
			}
			return true;
		}

		static int Minutes(const std::string& HHMM)
		{
			int H = 0, M = 0;
			if (std::sscanf(HHMM.c_str(), "%d:%d", &H, &M) != 2 || H < 0 || H > 24 || M < 0 || M > 59)
			{
				return -1;
			}
			return H * 60 + M;
		}

		static int DaysMask(const std::string& S)
		{
			if (S.empty())
			{
				return 0x7f;
			}
			static const char* const Names[7] = { "mon", "tue", "wed", "thu", "fri", "sat", "sun" };
			int Mask = 0;
			size_t P = 0;
			while (P < S.size())
			{
				size_t E = S.find(',', P);
				if (E == std::string::npos)
				{
					E = S.size();
				}
				const std::string Part = S.substr(P, E - P);
				const size_t Dash = Part.find('-');
				auto Idx = [](const std::string& N) { for (int I = 0; I < 7; ++I) { if (N == Names[I]) { return I; } } return -1; };
				if (Dash != std::string::npos)
				{
					const int A = Idx(Part.substr(0, Dash)), B = Idx(Part.substr(Dash + 1));
					if (A >= 0 && B >= 0)
					{
						for (int I = A;; I = (I + 1) % 7)
						{
							Mask |= 1 << I;
							if (I == B)
							{
								break;
							}
						}
					}
				}
				else if (Idx(Part) >= 0)
				{
					Mask |= 1 << Idx(Part);
				}
				P = E + 1;
			}
			return Mask ? Mask : 0x7f;
		}

		int FindTrack(const std::string& Cue) const
		{
			for (size_t I = 0; I < Tracks.size(); ++I)
			{
				if (Tracks[I].Cue == Cue)
				{
					return (int)I;
				}
			}
			return -1;
		}

		int FindRecord(const std::string& Id) const
		{
			for (size_t I = 0; I < Records.size(); ++I)
			{
				if (Records[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}

		int RecordOfTrack(int Track) const
		{
			for (size_t I = 0; I < Records.size(); ++I)
			{
				if (Records[I].Track == Track)
				{
					return (int)I;
				}
			}
			return -1;
		}

		/** The record a jukebox line plays (city.jb.<cue>), or -1. */
		int RecordOfLine(const std::string& Line) const
		{
			for (size_t I = 0; I < Records.size(); ++I)
			{
				if (!Line.empty() && Records[I].JukeboxLine == Line)
				{
					return (int)I;
				}
			}
			return -1;
		}

		bool Matches(const std::string& Role, const std::string& Act) const
		{
			const auto It = Roles.find(Role);
			if (It == Roles.end())
			{
				return false;
			}
			for (const std::string& G : It->second)
			{
				if (Glob(G.c_str(), Act.c_str()))
				{
					return true;
				}
			}
			return false;
		}

		/** An act id's role ("buy_x@seller": the part before '@'). */
		ERole RoleOf(const std::string& ActIn) const
		{
			const std::string Act = ActIn.substr(0, ActIn.find('@'));
			if (Matches("request", Act))
			{
				return ERole::Request;
			}
			if (Matches("jukebox", Act))
			{
				return ERole::Jukebox;
			}
			if (Matches("player", Act))
			{
				return ERole::Player;
			}
			if (Matches("radio", Act) && !Matches("not_radio", Act))
			{
				return ERole::Radio;
			}
			return ERole::None;
		}

		/** The act's own lines are replaced by the music's words (the generic acts) rather than kept with ours under them. */
		bool Replaces(const std::string& ActIn) const
		{
			return Matches("replace", ActIn.substr(0, ActIn.find('@')));
		}

		bool IsAutoOn(const std::string& ActIn) const
		{
			return Matches("auto_on", ActIn.substr(0, ActIn.find('@')));
		}

		// ------------------------------------------------------------------ the station

		/** The segment on air at a minute of a weekday (-1: off air). Segments that skip the day hand over to the one before. */
		int SegmentAt(int Weekday, int Minute) const
		{
			int Found = -1;
			for (size_t I = 0; I < Schedule.size(); ++I)
			{
				const FSegment& S = Schedule[I];
				if (S.AtMin <= Minute && (S.DaysMask & (1 << Weekday)))
				{
					Found = (int)I;
				}
			}
			if (Found < 0)
			{
				// before the first segment: yesterday's last one runs on past midnight unless it signs off
				const int Prev = (Weekday + 6) % 7;
				for (size_t I = 0; I < Schedule.size(); ++I)
				{
					if (Schedule[I].DaysMask & (1 << Prev))
					{
						Found = (int)I;
					}
				}
				if (Found >= 0 && Schedule[Found].Kind == EKind::SignOff)
				{
					return -1;
				}
				return Found >= 0 ? -2 - Found : -1;    // -2-I: segment I, begun yesterday
			}
			// after sign-off the air is silent until the next segment
			if (Schedule[Found].Kind == EKind::SignOff && Minute >= Schedule[Found].AtMin + std::max(1, Schedule[Found].Minutes))
			{
				return -1;
			}
			return Found;
		}

		/** Start and end (seconds of the day, the end may pass 86400) of segment I on a weekday. */
		void SegmentSpan(int I, int Weekday, bool bYesterday, double& Start, double& End) const
		{
			const FSegment& S = Schedule[I];
			Start = S.AtMin * 60.0 - (bYesterday ? 86400.0 : 0.0);
			const int Day = bYesterday ? (Weekday + 6) % 7 : Weekday;
			End = -1.0;
			for (size_t J = 0; J < Schedule.size(); ++J)
			{
				if ((int)J != I && Schedule[J].AtMin > S.AtMin && (Schedule[J].DaysMask & (1 << Day)))
				{
					End = Schedule[J].AtMin * 60.0 - (bYesterday ? 86400.0 : 0.0);
					break;
				}
			}
			if (End < 0.0)
			{
				// the last of the day: until the first of the next day
				const int Next = (Day + 1) % 7;
				for (size_t J = 0; J < Schedule.size(); ++J)
				{
					if (Schedule[J].DaysMask & (1 << Next))
					{
						End = Schedule[J].AtMin * 60.0 + 86400.0 - (bYesterday ? 86400.0 : 0.0);
						break;
					}
				}
			}
			if (S.Kind == EKind::SignOff)
			{
				End = Start + std::max(1, S.Minutes) * 60.0;
			}
		}

		static int WeatherPoolPick(const FFacts& F)
		{
			if (F.Thunder > 0.3f)
			{
				return 3;
			}
			if (F.Rain > 0.2f && F.Snow >= 0.5f)
			{
				return 2;
			}
			if (F.Rain > 0.2f)
			{
				return 1;
			}
			return 0;
		}

		/** The tracks a music segment plays, from Start until End, deterministic from the day and the segment. */
		void LayMusic(const FSegment& S, long long Day, int SegIndex, double Start, double End, const FFacts& F, std::vector<FPlay>& Out,
			int FirstTrack = -1) const
		{
			const std::vector<int>* Pool = &Pools.at(S.Pool);
			const std::vector<int>* WeatherPool = nullptr;
			const int W = WeatherPoolPick(F);
			static const char* const WNames[4] = { "", "rain", "snow", "storm" };
			if (W > 0 && S.Pool != "electronic" && S.Pool != "records")
			{
				const auto It = WeatherPools.find(WNames[W]);
				const auto P = It != WeatherPools.end() ? Pools.find(It->second) : Pools.end();
				if (P != Pools.end() && !P->second.empty())
				{
					WeatherPool = &P->second;
				}
			}
			double T = Start;
			int Prev = -1, N = 0;
			while (T < End - 1.0 && N < 400)
			{
				int Track;
				if (N == 0 && FirstTrack >= 0)
				{
					Track = FirstTrack;
				}
				else
				{
					const std::vector<int>& P = (WeatherPool && (Hash((uint32_t)Day, (uint32_t)SegIndex, (uint32_t)N + 991u) & 1u)) ? *WeatherPool : *Pool;
					Track = P[Hash((uint32_t)Day, (uint32_t)SegIndex * 131u + 7u, (uint32_t)N) % P.size()];
					if (Track == Prev)
					{
						// never the same record twice running: the next of the segment's own pool
						const std::vector<int>& Q = P.size() > 1 ? P : *Pool;
						const auto Pos = std::find(Q.begin(), Q.end(), Track);
						Track = Q[(size_t)(((Pos - Q.begin()) + 1) % (long)Q.size())];
					}
				}
				FPlay Pl;
				Pl.Start = T;
				Pl.Track = Track;
				Pl.Dur = std::min(Tracks[Track].Seconds, End - T);
				Out.push_back(Pl);
				T += Tracks[Track].Seconds + 4.0;   // the announcer's breath between two records
				Prev = Track;
				++N;
			}
		}

		/** Words for the wind's direction (the way it blows from). */
		static std::string WindKey(float FromDeg)
		{
			static const char* const Dirs[8] = { "music.wind.n", "music.wind.ne", "music.wind.e", "music.wind.se", "music.wind.s", "music.wind.sw", "music.wind.w", "music.wind.nw" };
			float D = std::fmod(FromDeg, 360.f);
			D = D < 0.f ? D + 360.f : D;
			return Dirs[(int)std::floor((D + 22.5f) / 45.f) % 8];
		}

		static std::string SkyKey(const FFacts& F)
		{
			if (F.Thunder > 0.3f) { return "music.sky.storm"; }
			if (F.Rain > 0.2f && F.Snow >= 0.5f) { return "music.sky.snow"; }
			if (F.Rain > 0.2f) { return "music.sky.rain"; }
			if (F.Fog > 0.45f) { return "music.sky.fog"; }
			if (F.Cloud < 0.2f) { return "music.sky.clear"; }
			if (F.Cloud < 0.5f) { return "music.sky.partly"; }
			if (F.Cloud < 0.8f) { return "music.sky.cloudy"; }
			return "music.sky.overcast";
		}

		static std::string Num(double V)
		{
			char B[32];
			std::snprintf(B, sizeof B, "%d", (int)std::lround(V));
			return B;
		}

		static std::string Clock(double SecOfDay)
		{
			double S = std::fmod(SecOfDay, 86400.0);
			S = S < 0.0 ? S + 86400.0 : S;
			const int M = (int)(S / 60.0);
			char B[16];
			std::snprintf(B, sizeof B, "%02d:%02d", M / 60, M % 60);
			return B;
		}

		static FArg Key(const std::string& K) { FArg A; A.S = K; A.bKey = true; return A; }
		static FArg Lit(const std::string& S) { FArg A; A.S = S; return A; }

		/** The weather words: now, the tide, one piece of advice. */
		void WeatherWords(const FFacts& F, std::vector<FCaption>& Out) const
		{
			FCaption C;
			if (F.WindSpeed < 1.f)
			{
				C.Key = "music.radio.weather.calm";
				C.Args = { Key(SkyKey(F)), Lit(Num(F.TempC)) };
			}
			else
			{
				C.Key = "music.radio.weather.now";
				C.Args = { Key(SkyKey(F)), Lit(Num(F.TempC)), Key(WindKey(F.WindFromDeg)), Lit(Num(F.WindSpeed)) };
			}
			Out.push_back(C);
			if (!F.TideTime.empty())
			{
				FCaption T;
				T.Key = F.bTideRising ? "music.radio.tide.rising" : "music.radio.tide.falling";
				T.Args = { Lit(F.TideTime) };
				Out.push_back(T);
			}
			FCaption A;
			if (F.Thunder > 0.3f) { A.Key = "music.radio.weather.advice.storm"; }
			else if (F.Rain > 0.2f && F.Snow >= 0.5f) { A.Key = "music.radio.weather.advice.snow"; }
			else if (F.Rain > 0.2f) { A.Key = "music.radio.weather.advice.rain"; }
			else if (F.Fog > 0.45f) { A.Key = "music.radio.weather.advice.fog"; }
			else if (F.WindSpeed > 11.f) { A.Key = "music.radio.weather.advice.wind"; }
			else if (F.TempC < 2.f) { A.Key = "music.radio.weather.advice.cold"; }
			else if (F.TempC > 22.f) { A.Key = "music.radio.weather.advice.warm"; }
			else { A.Key = "music.radio.weather.advice.clear"; }
			Out.push_back(A);
		}

		/** The day's news items (indices into News or the season's), different from the day before's where it can. */
		void NewsOfDay(long long Day, int Bulletin, int Season, std::vector<std::string>& Out) const
		{
			std::vector<std::string> All = News;
			if (Season >= 0 && Season < 4)
			{
				All.insert(All.end(), SeasonNews[Season].begin(), SeasonNews[Season].end());
			}
			if (All.empty())
			{
				return;
			}
			const int N = std::min<int>(NewsPerBulletin, (int)All.size());
			// a fresh run each day: the day picks a window of the shuffled list, the bulletins of the day share it out
			std::vector<int> Order(All.size());
			for (size_t I = 0; I < Order.size(); ++I)
			{
				Order[I] = (int)I;
			}
			for (size_t I = Order.size(); I > 1; --I)
			{
				std::swap(Order[I - 1], Order[Hash((uint32_t)(Day / 7), 77u, (uint32_t)I) % I]);
			}
			const int PerDay = 3 * N;
			const long long Base = (Day % 7) * PerDay + (long long)Bulletin * N;
			for (int I = 0; I < N; ++I)
			{
				Out.push_back(All[Order[(size_t)((Base + I) % (long long)Order.size())]]);
			}
		}

		/** One segment laid out. Day = days since the game began (for the dice), Weekday 0 = Monday. */
		void Lay(int SegIndex, long long Day, int Weekday, bool bYesterday, const FFacts& F, FTimeline& Out) const
		{
			Out = FTimeline();
			if (SegIndex < 0 || SegIndex >= (int)Schedule.size())
			{
				return;
			}
			const FSegment& S = Schedule[SegIndex];
			const long long SegDay = bYesterday ? Day - 1 : Day;
			const int SegWeekday = bYesterday ? (Weekday + 6) % 7 : Weekday;
			double Start, End;
			SegmentSpan(SegIndex, Weekday, bYesterday, Start, End);
			Out.Segment = SegIndex;
			Out.Kind = S.Kind;
			Out.Show = S.Show;
			std::vector<FCaption> Words;
			double T = Start;
			auto Say = [&](const std::string& K, std::vector<FArg> Args = {})
			{
				FCaption C;
				C.Start = T;
				C.Dur = CaptionSeconds;
				C.Key = K;
				C.Args = std::move(Args);
				Out.Caps.push_back(C);
				T += CaptionSeconds;
			};
			auto Bed = [&](const std::string& PoolName, double From, double To)
			{
				const auto It = Pools.find(PoolName);
				if (It == Pools.end() || It->second.empty() || To <= From)
				{
					return;
				}
				// quiet records back to back under the words, as long as the segment runs
				double At = From;
				for (uint32_t N = 0; At < To - 1.0 && N < 64; ++N)
				{
					FPlay P;
					P.Start = At;
					P.Track = It->second[Hash((uint32_t)SegDay, (uint32_t)SegIndex * 17u + N, 5u) % It->second.size()];
					P.From = 0.0;
					P.Dur = std::min(To - At, Tracks[P.Track].Seconds);
					P.Volume = BedVolume;
					Out.Plays.push_back(P);
					At += P.Dur;
				}
			};
			auto Pips = [&](double At) { FSfx X; X.At = At; X.Sound = "radio_pips"; Out.Sfx.push_back(X); };
			auto Chime = [&](double At) { FSfx X; X.At = At; X.Sound = "radio_chime"; Out.Sfx.push_back(X); };
			const double TalkEnd = std::min(End, Start + std::max(1, S.Minutes) * 60.0);
			switch (S.Kind)
			{
			case EKind::SignOn:
				Chime(Start);
				T = Start + 4.0;
				Say("music.radio.cap.sign_on.1", { Key(StationName) });
				Say("music.radio.cap.sign_on.2");
				Pips(T);
				T += 6.0;
				Say("music.radio.cap.time", { Lit(Clock(T)) });
				Say("music.radio.cap.sign_on.3");
				Bed("morning", T, TalkEnd);
				break;
			case EKind::SignOff:
				Chime(Start);
				T = Start + 4.0;
				Say("music.radio.cap.sign_off.1", { Key(StationName) });
				Say("music.radio.cap.sign_off.2");
				Bed("night", Start, TalkEnd);
				break;
			case EKind::News:
			{
				if (S.AtMin % 60 == 0)
				{
					Pips(Start);
					T = Start + 6.0;
					Say("music.radio.cap.time", { Lit(Clock(Start)) });
				}
				Chime(T);
				T += 3.0;
				Say("music.radio.news.intro");
				const int Bulletin = S.AtMin < 10 * 60 ? 0 : (S.AtMin < 16 * 60 ? 1 : 2);
				if (Bulletin == 0 && !F.PaperHeadline.empty())
				{
					Say("music.radio.news.paper", { Lit(F.PaperHeadline) });
				}
				std::vector<std::string> Items;
				NewsOfDay(SegDay, Bulletin, F.Season, Items);
				for (const std::string& K : Items)
				{
					Say(K);
				}
				Say("music.radio.news.outro");
				Bed("day", Start, TalkEnd);
				break;
			}
			case EKind::Weather:
			{
				if (S.AtMin % 60 == 0)
				{
					Pips(Start);
					T = Start + 6.0;
					Say("music.radio.cap.time", { Lit(Clock(Start)) });
				}
				Say("music.radio.weather.intro");
				std::vector<FCaption> W;
				WeatherWords(F, W);
				for (FCaption& C : W)
				{
					C.Start = T;
					C.Dur = CaptionSeconds;
					Out.Caps.push_back(C);
					T += CaptionSeconds;
				}
				Bed("day", Start, TalkEnd);
				break;
			}
			case EKind::Shipping:
			{
				Chime(Start);
				T = Start + 3.0;
				Say("music.radio.ship.intro");
				const int N = std::min<int>(ShippingAreas, (int)ShipAreas.size());
				for (int I = 0; I < N; ++I)
				{
					// the sea areas off the coast: the town's wind, each a little different (further out stronger)
					const uint32_t H = Hash((uint32_t)SegDay, 313u, (uint32_t)I);
					const float Wind = std::max(0.f, F.WindSpeed * (1.f + 0.12f * (float)I) + (float)((int)(H % 5u) - 2));
					const float Dir = F.WindFromDeg + (float)((int)((H >> 8) % 41u) - 20);
					const int Sea = Wind < 5.f ? 0 : (Wind < 10.f ? 1 : (Wind < 16.f ? 2 : 3));
					const char* Vis = F.Fog > 0.6f ? "music.vis.poor" : ((F.Fog > 0.3f || F.Rain > 2.f) ? "music.vis.moderate" : "music.vis.good");
					Say("music.radio.ship.line", { Key(ShipAreas[I]), Key(WindKey(Dir)), Lit(Num(Wind)), Key("music.sea." + Num(Sea)), Key(Vis) });
				}
				Say("music.radio.ship.outro");
				Bed(F.WindSpeed > 13.f && Pools.count("storm") ? "storm" : "night", Start, TalkEnd);
				break;
			}
			case EKind::Music:
			{
				LayMusic(S, SegDay, SegIndex, Start, End, F, Out.Plays);
				// each record announced as it starts
				for (const FPlay& P : Out.Plays)
				{
					FCaption C;
					C.Start = P.Start;
					C.Dur = CaptionSeconds;
					C.Key = "music.radio.cap.track";
					C.Args = { Key(S.Show), Key(Tracks[P.Track].Title), Key(Tracks[P.Track].Artist) };
					Out.Caps.push_back(C);
				}
				break;
			}
			case EKind::Requests:
			{
				Chime(Start);
				T = Start + 3.0;
				Say("music.radio.req.intro");
				// the hero's request first when it was posted before today
				const bool bHero = Request.Valid() && Request.PostedDay < SegDay;
				int Slot = 0;
				std::vector<FPlay> Plays;
				while (T < End - 30.0 && Slot < 64)
				{
					int Track;
					FCaption C;
					C.Start = T;
					C.Dur = CaptionSeconds + 2.0;
					if (Slot == 0 && bHero)
					{
						Track = Request.Track;
						C.Key = "music.radio.req.hero";
						const std::string Ded = Dedications.empty() ? std::string() : Dedications[(size_t)Request.Dedication % Dedications.size()];
						C.Args = { Key(Tracks[Track].Title), Key(Ded) };
						C.bHeroRequest = true;
					}
					else
					{
						const std::vector<int>& P = Pools.at(S.Pool);
						Track = P[Hash((uint32_t)SegDay, 4242u, (uint32_t)Slot) % P.size()];
						C.Key = Requests.empty() ? "music.radio.req.intro" : Requests[Hash((uint32_t)SegDay, 4243u, (uint32_t)Slot) % Requests.size()];
						C.Args = { Key(Tracks[Track].Title) };
					}
					Out.Caps.push_back(C);
					T += C.Dur;
					FPlay Pl;
					Pl.Start = T;
					Pl.Track = Track;
					Pl.Dur = std::min(Tracks[Track].Seconds, End - T);
					Out.Plays.push_back(Pl);
					T += Tracks[Track].Seconds + 3.0;
					++Slot;
				}
				// the last minute: thanks and goodbye under a quiet record
				T = std::max(T - 3.0, Out.Plays.empty() ? Start : Out.Plays.back().Start + Out.Plays.back().Dur);
				if (T < End)
				{
					FCaption C;
					C.Start = T;
					C.Dur = std::max(CaptionSeconds, End - T);
					C.Key = "music.radio.req.outro";
					Out.Caps.push_back(C);
					Bed("evening", T, End);
				}
				break;
			}
			default:
				break;
			}
			(void)SegWeekday;
		}

		/** The air at a moment: Sec = seconds since midnight of the day Day (days since the game began), Weekday 0 = Monday. */
		void AirAt(long long Day, int Weekday, double Sec, const FFacts& F, FAir& Out) const
		{
			Out.Play = nullptr;
			Out.Cap = nullptr;
			Out.Kind = EKind::OffAir;
			Out.Segment = -1;
			Out.Show.clear();
			const int Seg = SegmentAt(Weekday, (int)(Sec / 60.0));
			if (Seg == -1)
			{
				Out.Timeline = FTimeline();
				return;
			}
			const bool bYesterday = Seg <= -2;
			const int I = bYesterday ? -2 - Seg : Seg;
			Lay(I, Day, Weekday, bYesterday, F, Out.Timeline);
			Out.Kind = Out.Timeline.Kind;
			Out.Segment = I;
			Out.Show = Out.Timeline.Show;
			for (const FPlay& P : Out.Timeline.Plays)
			{
				if (Sec >= P.Start && Sec < P.Start + P.Dur)
				{
					Out.Play = &P;
				}
			}
			for (const FCaption& C : Out.Timeline.Caps)
			{
				if (Sec >= C.Start && Sec < C.Start + C.Dur)
				{
					Out.Cap = &C;
				}
			}
		}

		// ------------------------------------------------------------------ records and players

		/** The next record a player puts on from the hero's own (record ids he has, in the data's order): -1 when he has none. */
		int NextOwned(const std::string& Thing, const std::vector<int>& Owned)
		{
			if (Owned.empty())
			{
				return -1;
			}
			auto It = PlayerLast.find(Thing);
			int Next = 0;
			if (It != PlayerLast.end())
			{
				// the one after the last played (by record index, so a record bought in between slots in its place)
				Next = (int)Owned.size();
				for (size_t I = 0; I < Owned.size(); ++I)
				{
					if (Owned[I] > It->second)
					{
						Next = (int)I;
						break;
					}
				}
				Next %= (int)Owned.size();
			}
			PlayerLast[Thing] = Owned[(size_t)Next];
			return Owned[(size_t)Next];
		}

		/** The record a player holds when the hero has none of his own (the owner's, fixed per thing). */
		int HouseRecord(const std::string& Thing) const
		{
			return Records.empty() ? -1 : (int)(HashStr(Thing) % Records.size());
		}

		/** Posts a request on a postcard: the record last played (else the house favourite). False when one is waiting. */
		bool PostRequest(long long Day, const std::string& Thing)
		{
			if (Request.Valid())
			{
				return false;
			}
			const int Rec = LastRecord >= 0 && LastRecord < (int)Records.size() ? LastRecord : HouseRecord(Thing);
			if (Rec < 0)
			{
				return false;
			}
			Request.Track = Records[(size_t)Rec].Track;
			Request.Dedication = Dedications.empty() ? 0 : (int)(Hash((uint32_t)Day, 9u, (uint32_t)Rec) % Dedications.size());
			Request.PostedDay = Day;
			return true;
		}

		/** The request show of Day is over (or the request was heard): the card is used up. True if it was waiting for that show. */
		bool ConsumeRequest(long long Day)
		{
			if (Request.Valid() && Request.PostedDay < Day)
			{
				Request = FRequest();
				return true;
			}
			return false;
		}

		/** Radios: the hero's own switch wins; an auto radio (a shop's, a café's) plays in its hours unless he turned it off today. */
		bool IsRadioOn(const std::string& Thing, const std::string& Act, long long Day, int Minute) const
		{
			if (RadiosOn.count(Thing))
			{
				return true;
			}
			if (IsAutoOn(Act) && Minute >= AutoOnFrom && Minute < AutoOnTo)
			{
				return !(RadiosOffDay == Day && RadiosOff.count(Thing));
			}
			return false;
		}

		/** E at a radio: flips it. Returns the new state. */
		bool ToggleRadio(const std::string& Thing, const std::string& Act, long long Day, int Minute)
		{
			const bool bOn = IsRadioOn(Thing, Act, Day, Minute);
			if (RadiosOffDay != Day)
			{
				RadiosOff.clear();
				RadiosOffDay = Day;
			}
			if (bOn)
			{
				RadiosOn.erase(Thing);
				if (IsAutoOn(Act))
				{
					RadiosOff.insert(Thing);
				}
				return false;
			}
			RadiosOff.erase(Thing);
			if (!(IsAutoOn(Act) && Minute >= AutoOnFrom && Minute < AutoOnTo))
			{
				RadiosOn.insert(Thing);
			}
			return true;
		}

		// ------------------------------------------------------------------ the save

		void SaveState(std::map<std::string, std::string>& Out) const
		{
			std::string On;
			for (const std::string& S : RadiosOn)
			{
				On += (On.empty() ? "" : "|") + S;
			}
			Out["on"] = On;
			std::string Off;
			for (const std::string& S : RadiosOff)
			{
				Off += (Off.empty() ? "" : "|") + S;
			}
			Out["off"] = Off;
			Out["off_day"] = std::to_string(RadiosOffDay);
			for (const auto& P : PlayerLast)
			{
				if (P.second >= 0 && P.second < (int)Records.size())
				{
					Out["p:" + P.first] = Records[(size_t)P.second].Id;
				}
			}
			if (Request.Valid())
			{
				Out["req"] = Tracks[(size_t)Request.Track].Cue + "|" + std::to_string(Request.Dedication) + "|" + std::to_string(Request.PostedDay);
			}
			if (LastRecord >= 0 && LastRecord < (int)Records.size())
			{
				Out["last"] = Records[(size_t)LastRecord].Id;
			}
			Out["heard"] = std::to_string(RequestsHeard);
		}

		void LoadState(const std::map<std::string, std::string>& In)
		{
			ResetState();
			auto Split = [](const std::string& S, std::set<std::string>& Out)
			{
				size_t P = 0;
				while (P < S.size())
				{
					size_t E = S.find('|', P);
					E = E == std::string::npos ? S.size() : E;
					if (E > P)
					{
						Out.insert(S.substr(P, E - P));
					}
					P = E + 1;
				}
			};
			for (const auto& KV : In)
			{
				if (KV.first == "on")
				{
					Split(KV.second, RadiosOn);
				}
				else if (KV.first == "off")
				{
					Split(KV.second, RadiosOff);
				}
				else if (KV.first == "off_day")
				{
					RadiosOffDay = std::atoll(KV.second.c_str());
				}
				else if (KV.first.compare(0, 2, "p:") == 0)
				{
					const int R = FindRecord(KV.second);
					if (R >= 0)
					{
						PlayerLast[KV.first.substr(2)] = R;
					}
				}
				else if (KV.first == "req")
				{
					const size_t A = KV.second.find('|');
					const size_t B = A == std::string::npos ? A : KV.second.find('|', A + 1);
					if (B != std::string::npos)
					{
						Request.Track = FindTrack(KV.second.substr(0, A));
						Request.Dedication = std::atoi(KV.second.substr(A + 1, B - A - 1).c_str());
						Request.PostedDay = std::atoll(KV.second.substr(B + 1).c_str());
						if (!Request.Valid())
						{
							Request = FRequest();
						}
					}
				}
				else if (KV.first == "last")
				{
					LastRecord = FindRecord(KV.second);
				}
				else if (KV.first == "heard")
				{
					RequestsHeard = std::atoll(KV.second.c_str());
				}
			}
		}

		void ResetState()
		{
			RadiosOn.clear();
			RadiosOff.clear();
			RadiosOffDay = -1;
			PlayerLast.clear();
			Request = FRequest();
			LastRecord = -1;
			RequestsHeard = 0;
		}
	};
}
