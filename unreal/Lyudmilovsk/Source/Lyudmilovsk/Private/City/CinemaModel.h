#pragma once

// The Rassvet's newsreel (docs/cinema.md): plain C++, no engine types, so tools/cinemasim checks it offline. Data:
// Content/City/cinema.json (tools/films/films.py writes it from tools/films/catalog.py).
//
// Before a session of the hall's acts (cinema_week, cinema) the projectionist runs the day's real films: public-domain shorts
// of 1895-1908 (the programme by weekday, the festival's on a holiday). Each film's words (the lecturer's lines, as in the
// silent days) and the hall's laughs sit at shares of its length (0..1), so whatever copy fetch_films.py found, the cues fit
// it. Everything here is a pure function of the data and the share played.

#include "../People/MiniJson.h"

#include <string>
#include <vector>

namespace LyudFilm
{
	struct FCue
	{
		float At = 0.f;            // share of the film, 0..1
		std::string Key;           // a string key (Content/Localization/films)
	};

	struct FFilm
	{
		std::string Id;
		int Year = 0;
		std::string Title, Maker;  // string keys
		int MinSeconds = 0, MaxSeconds = 0;
		std::vector<FCue> Lector;
		std::vector<float> Laughs;
	};

	struct FCinema
	{
		std::string Id, Plot;
		std::vector<std::string> Acts;
		double Screen[3] = { 0, 0, 0 };   // world cm, the picture's middle
		double ScreenYaw = 0;             // Unreal degrees: the way the picture faces (into the hall)
		double ScreenW = 0, ScreenH = 0;  // cm
		double Booth[3] = { 0, 0, 0 };    // the projector's porthole
		double Hall[3] = { 0, 0, 0 };     // the hall's middle (its lamps go out round here)
		double HallRadius = 0;
	};

	struct FModel
	{
		std::vector<FFilm> Films;
		std::vector<std::vector<int>> Programme;   // Monday 0 .. Sunday 6: film indices in order
		std::vector<int> Festival;
		std::vector<FCinema> Cinemas;
		std::string LastError;

		bool Load(const std::string& Text)
		{
			LyudJson::FValue Root;
			std::string Error;
			if (!LyudJson::Parse(Text, Root, Error))
			{
				LastError = "cinema.json: " + Error;
				return false;
			}
			Films.clear();
			Programme.clear();
			Festival.clear();
			Cinemas.clear();
			for (const LyudJson::FValue& V : Root["films"].Items)
			{
				FFilm F;
				F.Id = V["id"].S();
				F.Year = V["year"].I();
				F.Title = V["title"].S();
				F.Maker = V["maker"].S();
				F.MinSeconds = V["seconds"][0].I();
				F.MaxSeconds = V["seconds"][1].I();
				for (const LyudJson::FValue& C : V["lector"].Items)
				{
					F.Lector.push_back({ C[0].F(), C[1].S() });
				}
				for (const LyudJson::FValue& L : V["laughs"].Items)
				{
					F.Laughs.push_back(L.F());
				}
				if (F.Id.empty())
				{
					LastError = "cinema.json: a film without an id";
					return false;
				}
				Films.push_back(F);
			}
			for (const LyudJson::FValue& Day : Root["programme"].Items)
			{
				std::vector<int> Ids;
				if (!Resolve(Day, Ids))
				{
					return false;
				}
				Programme.push_back(Ids);
			}
			if (!Resolve(Root["festival"], Festival))
			{
				return false;
			}
			for (const auto& KV : Root["cinemas"].Fields)
			{
				const LyudJson::FValue& V = KV.second;
				FCinema C;
				C.Id = KV.first;
				C.Plot = V["plot"].S();
				for (const LyudJson::FValue& A : V["acts"].Items)
				{
					C.Acts.push_back(A.S());
				}
				for (int I = 0; I < 3; ++I)
				{
					C.Screen[I] = V["screen"]["ue"][(size_t)I].F();
					C.Booth[I] = V["booth"][(size_t)I].F();
					C.Hall[I] = V["hall"]["ue"][(size_t)I].F();
				}
				C.ScreenYaw = V["screen"]["yaw"].F();
				C.ScreenW = V["screen"]["w"].F();
				C.ScreenH = V["screen"]["h"].F();
				C.HallRadius = V["hall"]["radius"].F();
				Cinemas.push_back(C);
			}
			if (Programme.size() != 7)
			{
				LastError = "cinema.json: the programme needs seven weekdays";
				return false;
			}
			return true;
		}

		int FindFilm(const std::string& Id) const
		{
			for (size_t I = 0; I < Films.size(); ++I)
			{
				if (Films[I].Id == Id)
				{
					return (int)I;
				}
			}
			return -1;
		}

		/** The cinema whose hall this act of this plot is a session of; -1 when the act has no newsreel (a quest's reel, a TV). */
		int CinemaFor(const std::string& Plot, const std::string& Act) const
		{
			for (size_t I = 0; I < Cinemas.size(); ++I)
			{
				if (Cinemas[I].Plot != Plot)
				{
					continue;
				}
				for (const std::string& A : Cinemas[I].Acts)
				{
					if (A == Act)
					{
						return (int)I;
					}
				}
			}
			return -1;
		}

		/** The day's newsreel (Monday 0); a holiday shows the festival's. */
		const std::vector<int>& ProgrammeFor(int Weekday, bool bFestival) const
		{
			static const std::vector<int> None;
			if (bFestival && !Festival.empty())
			{
				return Festival;
			}
			return Weekday >= 0 && Weekday < (int)Programme.size() ? Programme[(size_t)Weekday] : None;
		}

		/** The lecturer's line on at this share of the film: the last cue passed, for its hold; -1 before the first and after. */
		static int LectorAt(const FFilm& F, float Share, float HoldShare)
		{
			int On = -1;
			for (size_t I = 0; I < F.Lector.size(); ++I)
			{
				if (F.Lector[I].At <= Share)
				{
					On = (int)I;
				}
			}
			if (On < 0)
			{
				return -1;
			}
			const float Next = (size_t)On + 1 < F.Lector.size() ? F.Lector[(size_t)On + 1].At : 1.f;
			return Share < F.Lector[(size_t)On].At + HoldShare && Share < Next ? On : -1;
		}

		/** How many of the hall's laughs fall in (From, To] of the film: each plays once, however the frames step. */
		static int LaughsIn(const FFilm& F, float From, float To)
		{
			int N = 0;
			for (float L : F.Laughs)
			{
				N += (L > From && L <= To) ? 1 : 0;
			}
			return N;
		}

		/** A share's hold for a line: about Seconds of a film this long, never more than a third of it. */
		static float HoldShare(float FilmSeconds, float Seconds)
		{
			if (FilmSeconds <= 0.f)
			{
				return 0.f;
			}
			const float S = Seconds / FilmSeconds;
			return S < 0.34f ? S : 0.34f;
		}

	private:
		bool Resolve(const LyudJson::FValue& Ids, std::vector<int>& Out)
		{
			for (const LyudJson::FValue& V : Ids.Items)
			{
				const int I = FindFilm(V.S());
				if (I < 0)
				{
					LastError = "cinema.json: the programme names an unknown film " + V.S();
					return false;
				}
				Out.push_back(I);
			}
			return true;
		}
	};
}
