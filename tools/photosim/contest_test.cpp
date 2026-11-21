// The photo contest's checks outside the engine (docs/photo.md "The contest"): themes by the month, the jury's marks on
// synthetic shots, the rivals and places, what the hero entered, the save line.
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/Photo/PhotoContestModel.h"

#include <cstdio>
#include <string>
#include <vector>

static int GChecks = 0, GFailed = 0;

static void Expect(bool bOk, const char* What)
{
	++GChecks;
	if (!bOk)
	{
		++GFailed;
		std::printf("FAILED: %s\n", What);
	}
}

struct FTagIn
{
	const char* Tag;
	float Sharp;
};

static PhotoModel::FShot Shot(float Sharp, std::vector<FTagIn> Tags, const char* Place = "")
{
	PhotoModel::FShot S;
	S.Roll = 1;
	S.Frame = 1;
	S.Sharp = Sharp;
	S.Place = Place;
	for (const FTagIn& T : Tags)
	{
		PhotoModel::FSubject Sub;
		Sub.Tag = T.Tag;
		Sub.Sharp = T.Sharp;
		S.Subjects.push_back(Sub);
	}
	return S;
}

static float Sum(const PhotoModel::FShot& S, int Theme)
{
	return PhotoContest::Judge(S, Theme).Sum();
}

int main()
{
	using namespace PhotoContest;
	// themes
	Expect(std::string(ThemeId(0)) == "sea" && std::string(ThemeId(1)) == "birds" && std::string(ThemeId(2)) == "work"
		&& std::string(ThemeId(3)) == "festival", "theme ids");
	Expect(std::string(ThemeId(-3)) == "sea" && std::string(ThemeId(99)) == "festival", "theme id clamped");
	Expect(ThemeOf(1) == 0 && ThemeOf(10) == 1 && ThemeOf(11) == 2 && ThemeOf(12) == 3 && ThemeOf(5) == 0, "theme of a month");
	for (int Y = 2026; Y < 2030; ++Y)
	{
		for (int M = 1; M <= 12; ++M)
		{
			const int C = ContestOf(Y, M);
			Expect(YearOf(C) == Y && MonthOf(C) == M, "contest number and back");
			Expect(M == 1 || ThemeOf(M) != ThemeOf(M - 1), "two months running differ in theme");
		}
	}
	Expect(ContestOf(2026, 12) + 1 == ContestOf(2027, 1), "contests follow over the new year");

	// the jury: the theme's subject matters most
	const PhotoModel::FShot Gull = Shot(0.9f, { { "bird:HerringGull", 0.9f }, { "sun:8", 1.f } });
	const PhotoModel::FShot TwoBirds = Shot(0.9f, { { "bird:HerringGull", 0.9f }, { "bird:Rook", 0.8f }, { "sun:8", 1.f } });
	const PhotoModel::FShot BlurGull = Shot(0.2f, { { "bird:HerringGull", 0.2f }, { "sun:8", 1.f } });
	const PhotoModel::FShot Empty = Shot(0.9f, { { "sun:8", 1.f } });
	Expect(Sum(Gull, 1) > Sum(Empty, 1) + 30.f, "a bird beats an empty frame on the birds theme");
	Expect(Sum(TwoBirds, 1) > Sum(Gull, 1), "two species beat one");
	Expect(Sum(Gull, 1) > Sum(BlurGull, 1) + 15.f, "a sharp bird beats a blurred one");
	Expect(Sum(Gull, 0) < Sum(Gull, 1) - 25.f, "a bird is off the sea theme");
	Expect(Judge(Empty, 1).Subject == 5.f, "off the theme: 5 for the subject");
	Expect(Judge(Gull, 1).Sharp > 22.f && Judge(BlurGull, 1).Sharp < 6.f, "sharpness is the subject's own");
	const PhotoModel::FShot SeaDay = Shot(1.f, { { "sea", 1.f }, { "sun:50", 1.f } });
	const PhotoModel::FShot SeaDusk = Shot(1.f, { { "sea", 1.f }, { "sun:4", 1.f }, { "sunset:high", 1.f } });
	const PhotoModel::FShot SeaNight = Shot(1.f, { { "sea", 1.f }, { "sun:-30", 1.f } });
	const PhotoModel::FShot SeaAurora = Shot(1.f, { { "sea", 1.f }, { "sun:-30", 1.f }, { "aurora", 1.f } });
	Expect(Sum(SeaDusk, 0) > Sum(SeaDay, 0) + 10.f, "the low sun and a sunset beat the midday sea");
	Expect(Sum(SeaDay, 0) > Sum(SeaNight, 0), "midday beats the night");
	Expect(Sum(SeaAurora, 0) > Sum(SeaDay, 0), "the aurora lights the night");
	Expect(Judge(SeaDusk, 0).Light == 25.f, "light is held to 25");
	Expect(Judge(Shot(1.f, { { "sea", 1.f } }), 0).Light == 12.f, "no sun tag: a middling light");
	Expect(Judge(Shot(1.f, { { "sea", 1.f }, { "sun:x", 1.f } }), 0).Light == 12.f, "a broken sun tag is no tag");
	const PhotoModel::FShot Worker = Shot(0.8f, { { "work", 0.8f }, { "person:t_a", 0.8f }, { "sun:20", 1.f } });
	const PhotoModel::FShot Crew = Shot(0.8f, { { "work", 0.8f }, { "person:t_a", 0.8f }, { "person:t_b", 0.8f }, { "person:t_c", 0.7f }, { "sun:20", 1.f } });
	const PhotoModel::FShot Idle = Shot(0.8f, { { "person:t_a", 0.8f }, { "sun:20", 1.f } });
	Expect(Sum(Worker, 2) > Sum(Idle, 2) + 25.f, "people at work beat people standing");
	Expect(Sum(Crew, 2) == Sum(Worker, 2) + 10.f, "more workers, up to 10 more");
	const PhotoModel::FShot Fair = Shot(0.7f, { { "festival:sun_day", 0.7f }, { "sun:30", 1.f } });
	const PhotoModel::FShot FairPeople = Shot(0.7f, { { "festival:sun_day", 0.7f }, { "person:t_a", 0.9f }, { "sun:30", 1.f } });
	Expect(Sum(Fair, 3) > Sum(Idle, 3) + 20.f && Sum(FairPeople, 3) == Sum(Fair, 3) + 5.f, "the festival theme");
	Expect(Judge(Shot(0.7f, { { "festival:", 0.7f } }), 3).Subject == 5.f, "an empty festival id is nothing");
	// every score is 0..100, nonsense in or not
	const float Bad[] = { -5.f, 0.f, 0.5f, 1.f, 7.f, 0.f / 1.f };
	for (int T = -1; T <= Themes; ++T)
	{
		for (const float B : Bad)
		{
			const FMarks M = Judge(Shot(B, { { "sea", B }, { "bird:X", B }, { "work", B }, { "festival:f", B }, { "sun:-200", 1.f }, { "sun:999", 1.f } }), T);
			Expect(M.Subject >= 0.f && M.Subject <= 50.f && M.Light >= 0.f && M.Light <= 25.f && M.Sharp >= 0.f && M.Sharp <= 25.f, "marks in range");
		}
	}
	float Nan = 0.f;
	Nan = Nan / Nan;
	Expect(Sum(Shot(Nan, { { "sea", Nan } }), 0) == Sum(Shot(0.f, { { "sea", 0.f } }), 0), "NaN sharpness counts as 0");

	// rivals and places
	bool bSpread = false;
	for (int C = ContestOf(2026, 10); C < ContestOf(2030, 1); ++C)
	{
		float Lo = 100.f, Hi = 0.f;
		for (int I = 0; I < Rivals; ++I)
		{
			const float R = RivalScore(C, I);
			Expect(R >= 40.f && R <= 85.01f && R == RivalScore(C, I), "a rival's score is 40..85 and the same each time");
			Lo = R < Lo ? R : Lo;
			Hi = R > Hi ? R : Hi;
		}
		bSpread = bSpread || Hi - Lo > 15.f;
		Expect(PlaceOf(C, 100.f) == 1 && PlaceOf(C, 0.f) == Rivals + 1, "the best score wins, the worst is last");
		Expect(PlaceOf(C, Hi) == 1, "a tie goes to the hero");
		int Last = 1;
		for (float S = 100.f; S >= 0.f; S -= 2.5f)
		{
			const int P = PlaceOf(C, S);
			Expect(P >= Last && P >= 1 && P <= Rivals + 1, "a lower score never takes a better place");
			Last = P;
		}
	}
	Expect(bSpread, "the rivals are not all alike");
	Expect(PrizeOf(1) > PrizeOf(2) && PrizeOf(2) > PrizeOf(3) && PrizeOf(3) > 0 && PrizeOf(4) == 0 && PrizeOf(0) == 0, "prizes");
	// a perfect print on the theme wins any month; an empty frame never takes a prize
	for (int C = ContestOf(2026, 10); C < ContestOf(2030, 1); ++C)
	{
		Expect(PlaceOf(C, Sum(Shot(1.f, { { "bird:A", 1.f }, { "bird:B", 1.f }, { "sun:5", 1.f } }), 1)) == 1, "a perfect print wins");
		Expect(PlaceOf(C, Sum(Shot(0.3f, { { "sun:50", 1.f } }), 1)) > PrizePlaces, "an empty blurred frame wins nothing");
	}

	// the hero's entries
	FState S;
	Expect(S.CanEnter(2026, 10, 20) && S.CanEnter(2026, 10, 25) && !S.CanEnter(2026, 10, 26) && !S.CanEnter(2026, 10, 0), "entries by the 25th");
	PhotoModel::FShot Mine = Shot(0.9f, { { "bird:HerringGull", 0.9f }, { "sun:8", 1.f } }, "beach");
	Mine.Roll = 2;
	Mine.Frame = 17;
	Expect(!S.Enter(2026, 10, 26, Mine) && S.Entries().empty(), "too late is refused");
	Expect(S.Enter(2026, 10, 21, Mine) && S.Entries().size() == 1, "a print handed in");
	Expect(!S.CanEnter(2026, 10, 22) && !S.Enter(2026, 10, 22, Mine), "one print a month");
	const FEntry* E = S.Find(ContestOf(2026, 10));
	Expect(E && E->Roll == 2 && E->Frame == 17 && E->Place == "beach" && !E->bTold, "the entry keeps the print");
	Expect(E && E->Score > 80.f && E->Score == Sum(Mine, 1), "scored on the month's theme");
	Expect(S.Find(ContestOf(2026, 11)) == nullptr, "no entry in another month");
	Expect(S.Untold(2026, 10, 25) == nullptr && S.Wall(2026, 10, 25).empty(), "nothing before the jury");
	Expect(S.Untold(2026, 10, 26) == E && S.Untold(2026, 11, 3) == E && S.Untold(2027, 1, 1) == E, "judged from the 26th on");
	const int Place = E ? PlaceOf(E->Contest, E->Score) : 99;
	Expect((S.Wall(2026, 10, 26).size() == 1) == (Place <= PrizePlaces), "a prize print hangs on the wall");
	S.Told(ContestOf(2026, 10));
	Expect(S.Untold(2026, 11, 3) == nullptr && S.Find(ContestOf(2026, 10))->bTold, "heard once");
	PhotoModel::FShot Odd = Shot(0.1f, {}, "a place|with:odd=chars");
	Expect(S.Enter(2026, 11, 2, Odd) && S.Entries().back().Place.empty(), "an odd place name is dropped");
	Expect(S.Entries().back().Score < 40.f && S.Wall(2026, 12, 1).size() == (Place <= PrizePlaces ? 1u : 0u), "a poor print wins nothing");

	// the save line
	const std::string Line = S.Save();
	FState Back;
	Expect(Back.Restore(Line) && Back.Save() == Line && Back.Entries().size() == 2, "save and back");
	Expect(Back.Entries()[0].Score == S.Entries()[0].Score && Back.Entries()[0].bTold && !Back.Entries()[1].bTold
		&& Back.Entries()[0].Place == "beach", "the entries come back whole");
	Expect(PlaceOf(Back.Entries()[0].Contest, Back.Entries()[0].Score) == Place, "the place survives a save");
	FState None;
	Expect(None.Save() == "c1" && Back.Restore("c1") && Back.Entries().empty(), "an empty state");
	Expect(Back.Restore("c1|later=1:2|e=24321:1:1:500:0:") && Back.Entries().size() == 1, "an unknown key is skipped");
	const char* Garbage[] = { "", "c2", "v1", "c1|e", "c1|e=", "c1|e=1:2:3", "c1|e=a:1:1:500:0:", "c1|e=24321:0:1:500:0:", "c1|e=24321:1:37:500:0:",
		"c1|e=24321:1:1:1001:0:", "c1|e=24321:1:1:-1:0:", "c1|e=24321:1:1:500:2:", "c1|e=24321:1:1:500:0:a b", "c1|e=24321:1:1:500:0:x|e=24321:1:2:500:0:x",
		"c1|e=24322:1:1:500:0:x|e=24321:1:2:500:0:x", "c1|e=24321:1:1:500:0:x:y", "c1|e=99999999999:1:1:500:0:", "c1|e=-1:1:1:500:0:" };
	for (const char* G : Garbage)
	{
		FState Bad2;
		Bad2.Enter(2026, 10, 1, Mine);
		Expect(!Bad2.Restore(G) && Bad2.Entries().empty(), G);
	}
	// a year of contests, a print each month
	FState Year;
	int Wins = 0;
	for (int M = 0; M < 24; ++M)
	{
		const int Y = 2027 + M / 12, Mo = M % 12 + 1;
		const char* Tags[] = { "sea", "bird:Rook", "work", "festival:sun_day" };
		const PhotoModel::FShot Fit = Shot(0.6f + 0.02f * float(M % 10), { { Tags[ThemeOf(Mo)], 0.6f + 0.02f * float(M % 10) }, { "sun:15", 1.f } });
		Expect(Year.Enter(Y, Mo, 1 + M, Fit) == (1 + M <= LastEntryDay), "a print each month while the day allows");
		FState Copy;
		Expect(Copy.Restore(Year.Save()) && Copy.Save() == Year.Save(), "the save is a fixed point");
		Wins += int(Year.Wall(Y, Mo, 28).size() > Year.Wall(Y, Mo, 25).size());
	}
	Expect(Wins >= 6 && Wins <= 24, "a fair print on the theme often takes a prize");

	std::printf("contest: %d checks, %d failed\n", GChecks, GFailed);
	return GFailed == 0 ? 0 : 1;
}
