// The look model's checks (docs/look.md): haircuts that grow out, who says what once, clothes against the weather, and that
// every cut and garment has its words, its item and its icon.
#include "LookModel.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

using namespace LyudLook;

static int Checks = 0, Failed = 0;
#define CHECK(Cond, ...) do { ++Checks; if (!(Cond)) { ++Failed; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string ReadAll(const std::string& Path)
{
	std::ifstream In(Path);
	std::stringstream S;
	S << In.rdbuf();
	return S.str();
}

static int Test(const std::string& Root)
{
	const std::string Loc = Root + "/unreal/Lyudmilovsk/Content/Localization/";
	const std::string Lang[3] = { ReadAll(Loc + "city/strings_ru.json") + ReadAll(Loc + "strings_ru.json"), ReadAll(Loc + "city/strings_en.json") + ReadAll(Loc + "strings_en.json"),
		ReadAll(Loc + "city/strings_ja.json") + ReadAll(Loc + "strings_ja.json") };
	const std::string Items = ReadAll(Root + "/unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/LyudInventory.cpp");
	const std::string Assets = ReadAll(Root + "/localization/assets.json");
	const std::string Acts = ReadAll(Root + "/tools/city/content/activities.json");
	const std::string Interiors = ReadAll(Root + "/unreal/Lyudmilovsk/Content/City/interiors.json");
	CHECK(!Lang[0].empty() && !Items.empty() && !Assets.empty() && !Interiors.empty(), "the tables are read");
	auto Has = [&Lang](const std::string& Key) { const std::string K = "\"" + Key + "\""; return Lang[0].find(K) != std::string::npos && Lang[1].find(K) != std::string::npos && Lang[2].find(K) != std::string::npos; };

	// the cuts
	CHECK(Cuts().size() == 4 && Offered().size() == 3 && FindCut("shaggy") == 0 && FindCut("mohawk") == -1, "four cuts, three offered");
	for (const FCut& C : Cuts())
	{
		CHECK(Has("look.cut." + C.Id), "look.cut.%s is missing", C.Id.c_str());
		if (C.LastsDays > 0)
		{
			CHECK(C.LastsDays >= 7 && C.LastsDays <= 45, "%s lasts %d days", C.Id.c_str(), C.LastsDays);
			for (int L = 1; L <= 3; ++L)
			{
				CHECK(Has("line.look.cut." + C.Id + "." + std::to_string(L)), "line.look.cut.%s.%d is missing", C.Id.c_str(), L);
			}
		}
	}
	for (const char* F : { "wet", "cold", "hot" })
	{
		for (int L = 1; L <= 2; ++L)
		{
			CHECK(Has(std::string("line.look.") + F + "." + std::to_string(L)), "line.look.%s.%d is missing", F, L);
		}
	}
	// the three places open ULyudLook's pages
	CHECK(Acts.find("\"sound\": \"scissors\", \"open\": \"barber\"") != std::string::npos, "the barber's chair opens the page");
	CHECK(Interiors.find("\"open\": \"barber\"") != std::string::npos && Interiors.find("\"open\": \"clothes\"") != std::string::npos
		&& Interiors.find("\"open\": \"fitting\"") != std::string::npos, "interiors.json has the three open acts");

	// the clothes
	int Sold = 0, Rain = 0;
	for (const FWear& W : Clothes())
	{
		CHECK(Items.find("Wear(TEXT(\"" + W.Item + "\")") != std::string::npos, "%s is not a Wear item of LyudInventory.cpp", W.Item.c_str());
		CHECK(Items.find("Wear(TEXT(\"" + W.Item + "\"), ELyudEquipSlot::" + (W.Slot == "outer" ? "Outer" : "Head") + ")") != std::string::npos, "%s: slot %s", W.Item.c_str(), W.Slot.c_str());
		CHECK(Has("item." + W.Item + ".name") && Has("item." + W.Item + ".desc"), "item.%s words", W.Item.c_str());
		CHECK(Assets.find("\"icon_" + W.Item + "\"") != std::string::npos, "icon_%s", W.Item.c_str());
		CHECK(W.Slot == "outer" || W.Slot == "head", "%s: slot", W.Item.c_str());
		CHECK(W.Rough > 0.05f && W.Rough <= 1.f && W.Tint[0] >= 0.f && W.Tint[0] <= 1.f && W.Tint[1] <= 1.f && W.Tint[2] <= 1.f, "%s: cloth", W.Item.c_str());
		CHECK((W.Price == 0) == W.bOwnCloth, "%s: only what he came in costs nothing", W.Item.c_str());
		Sold += W.Price > 0;
		Rain += W.bRain;
		CHECK(FindWear(W.Item) == &W, "FindWear %s", W.Item.c_str());
	}
	CHECK(Sold == 4 && Rain == 1 && FindWear("hoodie") == nullptr, "four things sold, one for rain");

	// the weather
	CHECK(Fit("overshirt", false, 15.0, false) == EFit::Fine, "a shirt on a mild dry day");
	CHECK(Fit("overshirt", false, 15.0, true) == EFit::Wet && Fit("winter_coat", true, -5.0, true) == EFit::Wet, "rain without a raincoat is wet whatever the warmth");
	CHECK(Fit("raincoat", false, 12.0, true) == EFit::Fine, "the raincoat in rain");
	CHECK(Fit("overshirt", false, 5.0, false) == EFit::Cold && Fit("", false, 11.0, false) == EFit::Cold, "a shirt at five degrees");
	CHECK(Fit("wind_jacket", false, 7.0, false) == EFit::Fine && Fit("wind_jacket", false, 2.0, false) == EFit::Cold, "the windbreaker down to six");
	CHECK(Fit("wind_jacket", true, 4.5, false) == EFit::Fine, "the cap is two degrees");
	CHECK(Fit("winter_coat", false, -3.0, false) == EFit::Fine && Fit("winter_coat", false, -10.0, false) == EFit::Cold, "the coat in frost");
	CHECK(Fit("winter_coat", false, 18.0, false) == EFit::Hot && Fit("wind_jacket", false, 25.0, false) == EFit::Fine, "only the coat is too hot");
	CHECK(std::string(FitId(EFit::Wet)) == "wet" && std::string(FitId(EFit::Fine)).empty(), "FitId");
	// the game begins on 20 October (about 8 C by the sea): what he came in is a little cold, the windbreaker is right
	CHECK(Fit("overshirt", false, 8.0, false) == EFit::Cold && Fit("wind_jacket", false, 8.0, false) == EFit::Fine, "October");

	// a haircut and its talk
	FState S;
	CHECK(CutNow(S, 0.0) == "shaggy" && !WillComment(S, 7, 0.0, 0.0), "nothing to say of his own hair");
	CHECK(!GetCut(S, "shaggy", 1.0) && !GetCut(S, "mohawk", 1.0) && S.Serial == 0, "no such cuts at the barber's");
	CHECK(GetCut(S, "short", 10.0) && S.Serial == 1 && CutNow(S, 10.5) == "short", "a haircut");
	double Hour = 10.0 * 24.0 + 1.0;
	CHECK(WillComment(S, 7, 10.1, Hour), "the first resident speaks");
	NoteComment(S, 7, Hour);
	CHECK(!WillComment(S, 7, 10.2, Hour + 5.0), "the same resident not twice");
	CHECK(!WillComment(S, 8, 10.1, Hour + 0.2) && WillComment(S, 8, 10.1, Hour + 0.6), "not two in a row within half an hour");
	int Said = 1;
	for (int P = 8; P < 40; ++P)
	{
		Hour += 1.0;
		if (WillComment(S, P, 10.0 + (Hour - 240.0) / 24.0, Hour))
		{
			NoteComment(S, P, Hour);
			++Said;
		}
	}
	CHECK(Said == MaxComments, "%d residents speak of one haircut", Said);
	// a trim: the same cut again starts no new talk; another cut does
	CHECK(GetCut(S, "short", 11.0) && S.Serial == 1 && !WillComment(S, 99, 11.1, 11.0 * 24.0 + 3.0), "a trim is not news");
	CHECK(GetCut(S, "crop", 12.0) && S.Serial == 2 && S.Told.empty() && WillComment(S, 7, 12.1, 12.0 * 24.0 + 3.0), "a new cut is");
	CHECK(!WillComment(S, 7, 12.0 + FreshDays + 0.1, (12.0 + FreshDays + 0.1) * 24.0), "after three days it is old news");
	CHECK(CutNow(S, 12.0 + 29.9) == "crop" && CutNow(S, 12.0 + 30.0) == "shaggy", "the crop grows out in thirty days");
	// grown out and cut the same way again: news again
	CHECK(GetCut(S, "crop", 50.0) && S.Serial == 3, "the same cut after it grew out is a new haircut");
	std::set<int> Lines;
	for (int P = 0; P < 60; ++P)
	{
		const int L = CommentLine(P, 2, 3);
		CHECK(L >= 1 && L <= 3 && L == CommentLine(P, 2, 3), "CommentLine in range and steady");
		Lines.insert(L);
	}
	CHECK(Lines.size() == 3 && CommentLine(5, 1, 1) == 1, "all three lines are said by somebody");

	// remarks about the clothes: one a day
	FState R;
	CHECK(!WillRemark(R, EFit::Fine, 3.2, 80.0) && WillRemark(R, EFit::Wet, 3.2, 80.0), "a remark when wet");
	NoteRemark(R, 3.2, 80.0);
	CHECK(!WillRemark(R, EFit::Cold, 3.9, 95.0) && WillRemark(R, EFit::Cold, 4.1, 99.0), "one a day");
	// a remark and a haircut comment keep the same half hour apart
	FState G;
	GetCut(G, "parted", 4.0);
	NoteRemark(G, 4.0, 96.0);
	CHECK(!WillComment(G, 3, 4.0, 96.2) && WillComment(G, 3, 4.05, 96.6), "the gap is shared");

	// save
	FState A;
	GetCut(A, "parted", 33.25);
	NoteComment(A, 12, 800.5);
	NoteComment(A, 407, 803.0);
	NoteRemark(A, 33.4, 803.75);
	FState B;
	CHECK(Load(B, Save(A)) && B.Cut == "parted" && B.CutDay == 33.25 && B.Serial == 1 && B.Told == A.Told && B.LastToldHour == 803.75 && B.RemarkDay == 33,
		"save and load: %s", Save(A).c_str());
	FState Fresh;
	CHECK(Load(B, Save(Fresh)) && B.Cut == "shaggy" && B.Told.empty() && B.Serial == 0 && !WillComment(B, 1, 0.0, 0.0), "a fresh state round-trips");
	CHECK(!Load(B, "") && !Load(B, "mohawk|1|1|1|1|") && !Load(B, "crop|1"), "bad saves are refused");

	std::printf("looksim: %d checks, %d failed\n", Checks, Failed);
	return Failed ? 1 : 0;
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	const char* Env = std::getenv("LYUD_ROOT");
	if (Mode == "weather")
	{
		std::printf("%-12s", "temp C");
		for (const FWear& W : Clothes()) if (W.Slot == "outer") std::printf(" %-12s", W.Item.c_str());
		std::printf("\n");
		for (int T = -15; T <= 25; T += 5)
		{
			std::printf("%-12d", T);
			for (const FWear& W : Clothes()) if (W.Slot == "outer") { const char* F = FitId(Fit(W.Item, false, T, false)); std::printf(" %-12s", *F ? F : "fine"); }
			std::printf("\n");
		}
		return 0;
	}
	return Test(Env ? Env : ".");
}
