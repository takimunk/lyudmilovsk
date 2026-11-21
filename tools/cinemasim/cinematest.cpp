// Offline checks of the Rassvet's newsreel (Private/City/CinemaModel.h) on the real Content/City/cinema.json and its strings.
#include "City/CinemaModel.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

static int Fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++Fails; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string Read(const char* Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

int main()
{
	LyudFilm::FModel M;
	const std::string Text = Read("unreal/Lyudmilovsk/Content/City/cinema.json");
	CHECK(!Text.empty(), "no Content/City/cinema.json (run tools/films/films.py from the repo root)");
	CHECK(M.Load(Text), "load: %s", M.LastError.c_str());
	CHECK(M.Films.size() >= 2, "the done-line wants two films at least, got %zu", M.Films.size());

	// every key the engine shows is in all three tables
	for (const char* Lang : { "ru", "en", "ja" })
	{
		LyudJson::FValue T;
		std::string Err;
		const std::string Path = std::string("unreal/Lyudmilovsk/Content/Localization/films/strings_") + Lang + ".json";
		CHECK(LyudJson::Parse(Read(Path.c_str()), T, Err), "%s: %s", Path.c_str(), Err.c_str());
		std::set<std::string> Keys;
		for (const auto& KV : T.Fields) Keys.insert(KV.first);
		for (const LyudFilm::FFilm& F : M.Films)
		{
			CHECK(Keys.count(F.Title) && Keys.count(F.Maker), "%s: %s has no title/maker", Lang, F.Id.c_str());
			for (const LyudFilm::FCue& C : F.Lector) CHECK(Keys.count(C.Key), "%s: no %s", Lang, C.Key.c_str());
		}
		for (const char* K : { "city.film.card", "city.film.lector", "city.film.leave", "city.film.missing", "city.film.seen",
		                       "city.film.title", "film.programme.mon", "film.programme.sun", "film.programme.fest" })
		{
			CHECK(Keys.count(K), "%s: no %s", Lang, K);
		}
	}

	// the programme: every weekday plays something, every film is on some day, a holiday plays the festival's
	std::set<int> Shown;
	for (int D = 0; D < 7; ++D)
	{
		CHECK(!M.ProgrammeFor(D, false).empty(), "weekday %d shows nothing", D);
		for (int I : M.ProgrammeFor(D, false)) Shown.insert(I);
	}
	CHECK(Shown.size() == M.Films.size(), "%zu of %zu films are on no day", M.Films.size() - Shown.size(), M.Films.size());
	CHECK(M.ProgrammeFor(2, true) == M.Festival, "a holiday must show the festival programme");
	{
		// a holiday's own programme (docs/festivals.md "The new toys")
		const std::vector<std::string> NewYear = { "weekend_market", "new_year" }, Plain = { "lantern_night" };
		CHECK(M.FestivalOf.count("new_year") && M.FestivalOf.count("sea_festival") && M.FestivalOf.count("children_day"), "New Year, the Sea Festival and Children's Day have programmes of their own (%d festivals do)", (int)M.FestivalOf.size());
		CHECK(M.FestivalOf.count("new_year") && M.ProgrammeFor(2, true, &NewYear) == M.FestivalOf.at("new_year") && M.ProgrammeFor(2, true, &NewYear) != M.Festival, "New Year's Eve shows its own films, not the common festival ones");
		CHECK(M.ProgrammeFor(2, true, &Plain) == M.Festival && M.ProgrammeFor(2, false, &Plain) == M.Programme[2], "a holiday without a programme of its own shows the common one; a plain day the weekday's");
		bool bCartoon = false;
		for (int I : M.FestivalOf.count("new_year") ? M.FestivalOf.at("new_year") : std::vector<int>()) bCartoon = bCartoon || M.Films[(size_t)I].Id == "cohl_fantasmagorie";
		CHECK(bCartoon, "New Year's programme has the cartoon");
	}
	CHECK(M.ProgrammeFor(9, false).empty(), "an out-of-range weekday shows nothing");

	// the Rassvet: its two session acts have the newsreel, the quest's reel and other plots do not
	const int R = M.CinemaFor("P-CLUB2", "cinema_week");
	CHECK(R >= 0 && M.CinemaFor("P-CLUB2", "cinema") == R, "the Rassvet's sessions have no newsreel");
	CHECK(M.CinemaFor("P-CLUB2", "cinema_reel") < 0, "the quest's reel must stay as it is");
	CHECK(M.CinemaFor("P-CLUB3", "cinema_week") < 0, "another plot's act must not start the Rassvet's newsreel");
	if (R >= 0)
	{
		const LyudFilm::FCinema& C = M.Cinemas[(size_t)R];
		CHECK(C.ScreenW > 300 && C.ScreenH > 200 && C.ScreenW < 1300, "screen %gx%g cm", C.ScreenW, C.ScreenH);
		// the booth is behind the hall's middle, the screen in front: the beam crosses the hall
		const double Bx = C.Booth[0] - C.Hall[0], By = C.Booth[1] - C.Hall[1];
		const double Sx = C.Screen[0] - C.Hall[0], Sy = C.Screen[1] - C.Hall[1];
		CHECK(Bx * Sx + By * Sy < 0, "booth and screen on the same side of the hall");
		CHECK(C.Booth[2] > C.Hall[2], "the booth's porthole is above the hall's middle");
	}

	// cues: lines show in order and go after their hold; laughs fire once however the frames step
	for (const LyudFilm::FFilm& F : M.Films)
	{
		CHECK(F.MinSeconds > 0 && F.MinSeconds < F.MaxSeconds, "%s: seconds", F.Id.c_str());
		CHECK(F.Year < 1929, "%s: %d is not before 1929", F.Id.c_str(), F.Year);
		const float Len = (float)(F.MinSeconds + F.MaxSeconds) * 0.5f;
		const float Hold = LyudFilm::FModel::HoldShare(Len, 7.f);
		int Last = -1, Seen = 0;
		for (int Step = 0; Step <= 1000; ++Step)
		{
			const int On = LyudFilm::FModel::LectorAt(F, Step / 1000.f, Hold);
			if (On >= 0 && On != Last)
			{
				CHECK(On > Last, "%s: line %d after %d", F.Id.c_str(), On, Last);
				++Seen;
				Last = On;
			}
		}
		CHECK(Seen == (int)F.Lector.size(), "%s: %d of %zu lines shown", F.Id.c_str(), Seen, F.Lector.size());
		CHECK(LyudFilm::FModel::LectorAt(F, 0.999f, Hold) < 0 || F.Lector.back().At + Hold > 0.999f, "%s: last line never goes", F.Id.c_str());
		for (float Dt : { 0.0007f, 0.013f, 0.11f })
		{
			int N = 0;
			for (float S = 0.f; S < 1.f; S += Dt) N += LyudFilm::FModel::LaughsIn(F, S, S + Dt);
			CHECK(N == (int)F.Laughs.size(), "%s: %d laughs at step %g, wants %zu", F.Id.c_str(), N, Dt, F.Laughs.size());
		}
	}
	CHECK(LyudFilm::FModel::HoldShare(0.f, 7.f) == 0.f && LyudFilm::FModel::HoldShare(10.f, 7.f) < 0.35f, "hold share");

	// a broken file says why
	LyudFilm::FModel Bad;
	CHECK(!Bad.Load("{\"films\":[{\"id\":\"a\"}],\"programme\":[[\"b\"]]}") && !Bad.LastError.empty(), "unknown film accepted");
	CHECK(!Bad.Load("{\"films\":[{\"id\":\"a\"}],\"programme\":[[\"a\"]]}"), "a programme of one day accepted");

	std::printf("%s: %zu films, %zu cinemas, %d failures\n", Fails ? "FAIL" : "ok", M.Films.size(), M.Cinemas.size(), Fails);
	return Fails ? 1 : 0;
}
