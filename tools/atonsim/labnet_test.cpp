// Checks of the lab network's model (Private/AtonOS/Lab/LabNet.cpp) and of the data it reads.
//   labnet_test <lab_mail.json> <strings dir: tools/aton/strings>
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/AtonOS/Lab/LabNet.h"

#include <cstdio>
#include <fstream>
#include <sstream>

static int GChecks = 0, GFailed = 0;

static void Expect(bool bOk, const char* What)
{
	++GChecks;
	if (!bOk)
	{
		++GFailed;
		std::printf("FAIL %s\n", What);
	}
}

static std::string ReadFile(const std::string& Path)
{
	std::ifstream In(Path, std::ios::binary);
	std::stringstream S;
	S << In.rdbuf();
	return S.str();
}

int main(int Argc, char** Argv)
{
	using namespace AtonLab;
	// the password
	Expect(PasswordOk("0314"), "0314");
	Expect(PasswordOk(" 03:14 "), "03:14 with spaces");
	Expect(PasswordOk("\xEF\xBC\x90\xEF\xBC\x93\xEF\xBC\x91\xEF\xBC\x94"), "full-width digits");
	Expect(!PasswordOk(""), "empty");
	Expect(!PasswordOk("314"), "314");
	Expect(!PasswordOk("03140"), "03140");
	Expect(!PasswordOk("1403"), "1403");
	Expect(PasswordDigits("a1b\xEF\xBC\x99") == "19", "digits of mixed text");

	// the letters
	std::vector<FLabLetter> Letters;
	std::string Error;
	Expect(!LoadLetters("{", Letters, &Error) && !Error.empty(), "bad json refused");
	Expect(!LoadLetters("{\"letters\":[{\"id\":\"a\",\"box\":\"x\"}]}", Letters, &Error), "bad box refused");
	Expect(!LoadLetters("{\"letters\":[{\"id\":\"a\",\"box\":\"guest\",\"after\":\"b\"}]}", Letters, &Error), "after names nothing");
	Expect(!LoadLetters("{\"letters\":[{\"id\":\"a\",\"box\":\"guest\"},{\"id\":\"a\",\"box\":\"guest\"}]}", Letters, &Error), "repeated id");
	if (Argc > 1)
	{
		const bool bLoaded = LoadLetters(ReadFile(Argv[1]), Letters, &Error);
		Expect(bLoaded, "lab_mail.json loads");
		if (!bLoaded)
		{
			std::printf("  %s\n", Error.c_str());
		}
		FLabState S;
		Expect(!S.Visible(Letters, "guest").empty(), "guest letters show");
		Expect(S.Visible(Letters, "staff").empty(), "staff letters shut before the password");
		bool bPassword = false;
		for (const FLabLetter* L : S.Visible(Letters, "guest"))
		{
			bPassword = bPassword || L->Id == "password";
		}
		Expect(bPassword, "the guest box has the letter about the password");
		S.bStaff = true;
		// reading whatever shows opens the rest: every letter is reached
		size_t Total = 0;
		for (int Pass = 0; Pass < 50; ++Pass)
		{
			for (const char* Box : { "guest", "staff" })
			{
				for (const FLabLetter* L : S.Visible(Letters, Box))
				{
					S.Read.insert(L->Id);
				}
			}
		}
		Total = S.Read.size();
		Expect(Total == Letters.size(), "every letter can be reached");
		Expect(S.Unread(Letters, "staff") == 0 && S.Unread(Letters, "guest") == 0, "nothing unread at the end");
		std::printf("letters: %zu\n", Letters.size());
	}

	// the telescope queue
	int FreeNights2 = 0;
	for (int Night = -3; Night < 400; ++Night)
	{
		const std::vector<FScopeSlot> A = ScopeNight(Night), B = ScopeNight(Night);
		int Free = 0;
		bool bSame = A.size() == (size_t)ScopeSlots, bTargets = true;
		for (size_t I = 0; I < A.size(); ++I)
		{
			bSame = bSame && A[I].Target == B[I].Target && A[I].Owner == B[I].Owner;
			Free += A[I].Owner == -1 ? 1 : 0;
			bTargets = bTargets && A[I].Target >= 0 && A[I].Target < ScopeTargets && A[I].Owner >= -1 && A[I].Owner < ScopeStaff;
			for (size_t J = 0; J < I; ++J)
			{
				bTargets = bTargets && A[I].Target != A[J].Target;
			}
		}
		FreeNights2 += Free == 2 ? 1 : 0;
		if (!bSame || !bTargets || Free < 1 || Free > 2)
		{
			Expect(false, "a night's programme");
		}
	}
	Expect(true, "400 nights' programmes");
	Expect(FreeNights2 > 40 && FreeNights2 < 250, "some nights have two free slots");
	Expect(ScopeSlotHour(0) == 22 && ScopeSlotHour(1) == 0 && ScopeSlotHour(3) == 4, "slot hours");
	Expect(ScopeNightOf(16.0) == 0 && ScopeNightOf(11.9) == -1 && ScopeNightOf(35.9) == 0 && ScopeNightOf(36.0) == 1, "night of an hour");
	Expect(ScopeSlotAt(0, 0) == 22.0 && ScopeSlotAt(0, 1) == 24.0 && ScopeSlotAt(1, 3) == 52.0, "slot times");
	{
		FLabState S;
		int Night = 5, Free = -1, Taken = -1;
		const std::vector<FScopeSlot> P = ScopeNight(Night);
		for (int I = 0; I < ScopeSlots; ++I)
		{
			(P[(size_t)I].Owner == -1 ? Free : Taken) = I;
		}
		const double Now = ScopeSlotAt(Night, 0) - 5.0;
		Expect(!S.Book(Night, Free, Now), "a guest cannot book");
		S.bStaff = true;
		Expect(!S.Book(Night, Taken, Now), "a taken slot is refused");
		Expect(S.Book(Night, Free, Now), "a free slot is taken");
		Expect(!S.Book(Night, Free, Now), "one slot a night");
		Expect(S.Unbook(Night, Now) && S.Book(Night, Free, Now), "given back and taken again");
		Expect(!S.Book(Night - 3, 0, Now) && !S.Book(Night - 3, 1, Now) && !S.Book(Night - 3, 2, Now) && !S.Book(Night - 3, 3, Now), "a past night is refused");
		Expect(S.Resolve(Now, [](double) { return 0.f; }) == 0, "nothing to resolve before the slot");
		Expect(S.Resolve(ScopeSlotAt(Night, Free) + 3.0, [](double) { return 0.9f; }) == 1, "the slot resolves once it has passed");
		Expect(S.Results.size() == 1 && !S.Results[0].bClear && S.Results[0].Target == P[(size_t)Free].Target, "a cloudy night gives no plate");
		Expect(S.Resolve(ScopeSlotAt(Night, Free) + 30.0, [](double) { return 0.f; }) == 0, "and only once");
		Expect(!S.Unbook(Night, ScopeSlotAt(Night, Free) + 3.0), "a passed slot is not given back");
		// the save line
		S.Read.insert("welcome");
		S.Read.insert("flux1");
		S.Endings.insert("dawn");
		S.ChessWon[2] = 1;
		S.ChessLost[0] = 4;
		S.ChessLevel = 3;
		S.FailedLogins = 2;
		S.ChessSave = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1;e2e4,e7e5";
		S.AdvSave = "n=hall;f=a,b;m=12";
		const std::string Line = S.Save();
		FLabState T;
		Expect(T.Restore(Line), "the save line reads back");
		Expect(T.Save() == Line, "and writes the same line");
		Expect(T.bStaff && T.Read.count("flux1") == 1 && T.Endings.count("dawn") == 1 && T.ChessWon[2] == 1 && T.ChessLost[0] == 4 && T.ChessLevel == 3
			&& T.ChessSave == S.ChessSave && T.AdvSave == S.AdvSave && T.Bookings.size() == 1 && T.Results.size() == 1 && T.FailedLogins == 2, "every field");
		Expect(Line.find('\n') == std::string::npos, "one line");
		FLabState U;
		U.bStaff = true;
		Expect(!U.Restore("") && !U.Restore("v2|staff=1") && !U.Restore("v1|read=a b") && !U.Restore("v1|chess=1,2") && !U.Restore("v1|book=1:9") && U.bStaff, "garbage refused, state kept");
		Expect(U.Restore("v1|staff=0|future=1"), "an unknown key is skipped");
		Expect(!U.bStaff, "restored");
	}

	// the weather station
	Expect(Compass8(0.f) == 0 && Compass8(359.f) == 0 && Compass8(44.f) == 1 && Compass8(180.f) == 4 && Compass8(-90.f) == 6 && Compass8(725.f) == 0, "compass");
	Expect(Oktas(0.f) == 0 && Oktas(1.f) == 8 && Oktas(0.03f) == 1 && Oktas(0.97f) == 7 && Oktas(0.5f) == 4 && Oktas(-1.f) == 0 && Oktas(3.f) == 8, "oktas");
	Expect(Beaufort(0.f) == 0 && Beaufort(6.f) == 4 && Beaufort(40.f) == 12, "beaufort");
	Expect(SolarFlux(-5.f, 0.f) == 0.f && SolarFlux(60.f, 0.f) > 800.f && SolarFlux(60.f, 0.f) < 1000.f && SolarFlux(60.f, 1.f) < SolarFlux(60.f, 0.f) * 0.3f
		&& SolarFlux(10.f, 0.f) < SolarFlux(30.f, 0.f), "solar flux");

	std::printf("labnet: %d checks, %d failed\n", GChecks, GFailed);
	return GFailed ? 1 : 0;
}
