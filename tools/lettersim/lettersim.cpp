// The town's post outside the engine (docs/letters.md): the plain C++ model of Private/Letters/LettersModel.* run on
// the real data (Content/Letters/letters.json) over weeks of game days. From the repo root: tools/lettersim/run.sh
//   run.sh            the checks (exit code = number of failures)
//   run.sh story [ru|en|ja]   prints the whole correspondence of a simulated autumn in one language
#include "Letters/LettersModel.h"
#include "People/MiniJson.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace LyudLetters;

namespace
{
	int Failures = 0;
	void Check(bool bOk, const std::string& What)
	{
		if (!bOk)
		{
			++Failures;
			std::printf("FAIL %s\n", What.c_str());
		}
	}

	std::string Read(const std::string& Path)
	{
		std::ifstream F(Path, std::ios::binary);
		std::stringstream S;
		S << F.rdbuf();
		return S.str();
	}

	std::map<std::string, std::string> Table(const std::string& Lang)
	{
		std::map<std::string, std::string> Out;
		for (const std::string& File : { std::string("unreal/Lyudmilovsk/Content/Localization/strings_") + Lang + ".json",
				 std::string("unreal/Lyudmilovsk/Content/Localization/letters/strings_") + Lang + ".json",
				 std::string("unreal/Lyudmilovsk/Content/Localization/town/strings_") + Lang + ".json" })
		{
			LyudJson::FValue Root;
			std::string Error;
			if (LyudJson::Parse(Read(File), Root, Error))
			{
				for (const auto& F : Root.Fields)
				{
					Out.emplace(F.first, F.second.S());
				}
			}
		}
		return Out;
	}

	const char* DayName(int Day)
	{
		static const char* Names[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
		return Names[Weekday(Day)];
	}

	std::string Date(int Day)
	{
		int Y, M, D;
		CivilFromDays(Day, Y, M, D);
		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "%04d-%02d-%02d %s", Y, M, D, DayName(Day));
		return Buf;
	}

	/** Fills {0}, {1} with the args (an arg starting with @ is a key). */
	std::string Fill(const std::map<std::string, std::string>& T, const std::string& Key, const std::vector<std::string>& Args)
	{
		const auto It = T.find(Key);
		std::string S = It == T.end() ? "<" + Key + ">" : It->second;
		for (size_t I = 0; I < Args.size(); ++I)
		{
			std::string V = Args[I];
			if (!V.empty() && V[0] == '@')
			{
				const auto A = T.find(V.substr(1));
				V = A == T.end() ? "<" + V.substr(1) + ">" : A->second;
			}
			const std::string Mark = "{" + std::to_string(I) + "}";
			for (size_t At = S.find(Mark); At != std::string::npos; At = S.find(Mark, At + V.size()))
			{
				S.replace(At, Mark.size(), V);
			}
		}
		return S;
	}

	bool Has(const std::map<std::string, std::string>& T, const std::string& Key)
	{
		return T.count(Key) > 0;
	}

	/** One simulated autumn: the hero's moves by day, the facts growing. bAnswer: he answers Tonya each time. */
	struct FRun
	{
		FModel M;
		FFacts Facts;
		std::vector<std::string> Log;
		std::map<std::string, int> ArrivedOnWeekday;
		int Start = 0;
		int Delivered = 0;
	};

	void Simulate(FRun& R, int Days, bool bAnswer, bool bCards, int SaveLoadAtDay = -1)
	{
		R.Start = DaysFromCivil(2026, 10, 20);
		R.Facts.Festivals = { { DaysFromCivil(2026, 10, 24), "sea_festival" }, { DaysFromCivil(2026, 10, 31), "halloween" },
			{ DaysFromCivil(2026, 11, 1), "dia_de_muertos" }, { DaysFromCivil(2026, 11, 14), "lantern_night" },
			{ DaysFromCivil(2026, 12, 18), "winter_fair" }, { DaysFromCivil(2026, 12, 31), "new_year" } };
		const std::vector<std::string> Birds = { "bird.great_tit.name", "bird.house_sparrow.name", "bird.magpie.name", "bird.rook.name",
			"bird.jackdaw.name" };
		const std::vector<std::string> Fish = { "fish.herring.name", "fish.sprat.name" };
		const std::vector<std::string> Places = { "place.alley.name", "place.club_door.name" };
		for (int Min = 16 * 60; Min < Days * 1440; Min += 15)
		{
			const FTime Now{ R.Start + Min / 1440, Min % 1440 };
			const int D = Now.Day - R.Start;
			// what he does, by day
			if (Now.Minute == 12 * 60)
			{
				if (D >= 1 && D - 1 < (int)Birds.size()) R.Facts.Birds.push_back(Birds[(size_t)(D - 1)]);
				if (D == 6) R.Facts.Fish.push_back(Fish[0]);
				if (D == 15) R.Facts.Fish.push_back(Fish[1]);
				if (D == 3) R.Facts.Places.push_back(Places[0]);
				if (D == 12) R.Facts.Places.push_back(Places[1]);
				if (D == 9) R.Facts.Friends.push_back("bird.great_tit.name");
				if (D == 4) R.Facts.QuestsDone.insert("waxwing");
				if (D == 8) R.Facts.QuestsDone.insert("painter_views");
				if (bCards && (D == 2 || D == 9))
				{
					Check(R.M.PostCard(Now, "h_holm", "lighthouse", "hello", "weather"), "post a card to Holm");
				}
				if (bCards && D >= 1 && D <= 10)
				{
					Check(R.M.PostCard(Now, "t_resident_" + std::to_string(D % 4), "starlings", "sea", "day", AgeGroup(10 + 20 * (D % 4))),
						"post a card to a townsperson");
				}
			}
			if (SaveLoadAtDay >= 0 && D == SaveLoadAtDay && Now.Minute == 3 * 60)
			{
				std::map<std::string, std::string> Saved;
				R.M.Save(Saved);
				FModel Fresh;
				Fresh.Data = R.M.Data;
				Fresh.LoadState(Saved);
				std::map<std::string, std::string> Again;
				Fresh.Save(Again);
				Check(Again == Saved, "save, load and save again give the same record");
				R.M = Fresh;
			}
			const FEvents E = R.M.Step(Now, R.Facts);
			R.Delivered += (int)E.Delivered.size();
			for (const int I : E.Arrived)
			{
				const FLetter& L = R.M.Letters[(size_t)I];
				R.ArrivedOnWeekday[DayName(Now.Day)]++;
				Check(IsPostDay(Now.Day), "a letter arrives on a post day (" + L.Id + " on " + Date(Now.Day) + ")");
				Check(Now.Minute >= R.M.Data.Post.DeliverHero, "a letter arrives after the morning round (" + L.Id + ")");
				R.Log.push_back(Date(Now.Day) + " " + L.Id);
			}
			// he reads his mail at 18:00 and answers Tonya the next day at noon
			if (Now.Minute == 18 * 60)
			{
				for (size_t I = 0; I < R.M.Letters.size(); ++I)
				{
					if (R.M.Letters[I].bArrived && !R.M.Letters[I].bRead)
					{
						R.M.MarkRead((int)I, Now.Day);
					}
				}
			}
			if (bAnswer && Now.Minute == 12 * 60 && R.M.PenWaiting())
			{
				const std::vector<FAnswer> Options = R.M.PenOptions(R.Facts);
				Check(!Options.empty() && Options.back().Kind == "none", "the answers end with 'nothing special'");
				Check(Options.size() <= 4, "at most three answers and 'nothing special'");
				Check(R.M.PostPenAnswer(Now, Options.front()), "post an answer to Tonya");
				Check(!R.M.PenWaiting(), "nothing waits once answered");
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	const std::string Mode = Argc > 1 ? Argv[1] : "test";
	FData Data;
	std::string Error;
	if (!Data.Load(Read("unreal/Lyudmilovsk/Content/Letters/letters.json"), Error))
	{
		std::printf("FAIL letters.json: %s\n", Error.c_str());
		return 1;
	}

	if (Mode == "story")
	{
		const std::string Lang = Argc > 2 ? Argv[2] : "ru";
		const auto T = Table(Lang);
		FRun R;
		R.M.Data = Data;
		Simulate(R, 75, true, true);
		for (const FLetter& L : R.M.Letters)
		{
			std::string From = L.From;
			if (From.rfind("t_", 0) == 0) From = "npc." + From + ".name";
			std::printf("==== %s  [%s]  %s\n", Date(L.Arrive).c_str(), L.Id.c_str(), Fill(T, From, {}).c_str());
			if (!L.Ack.empty()) std::printf("%s\n\n", Fill(T, L.Ack, L.AckArgs).c_str());
			std::printf("%s\n\n", Fill(T, L.Body, L.Args).c_str());
		}
		return 0;
	}

	// --- the calendar
	const int Tue = DaysFromCivil(2026, 10, 20);
	Check(Weekday(Tue) == 1, "2026-10-20 is a Tuesday");
	for (int D = Tue - 800; D < Tue + 800; D += 37)
	{
		int Y, M, Dd;
		CivilFromDays(D, Y, M, Dd);
		Check(DaysFromCivil(Y, M, Dd) == D, "civil date round trip");
	}
	Check(NextPostDay(Tue + 4) == Tue + 6, "Saturday's next post day is Monday");

	// --- delivery by the calendar
	{
		FModel M;
		M.Data = Data;
		Check(M.DeliveryDay({ Tue, 10 * 60 }) == Tue + 1, "posted Tuesday morning: delivered Wednesday");
		Check(M.DeliveryDay({ Tue, 18 * 60 }) == Tue + 2, "posted Tuesday after the collection: delivered Thursday");
		Check(M.DeliveryDay({ Tue + 4, 16 * 60 }) == Tue + 6, "posted Saturday before 17:00: delivered Monday");
		Check(M.DeliveryDay({ Tue + 4, 18 * 60 }) == Tue + 7, "posted Saturday evening: collected Monday, delivered Tuesday");
		Check(M.DeliveryDay({ Tue + 5, 9 * 60 }) == Tue + 7, "posted on Sunday: collected Monday, delivered Tuesday");
		Check(M.PenDeliveryDay({ Tue, 10 * 60 }) == Tue + 3, "to the north: three post days");
		Check(M.PenDeliveryDay({ Tue + 2, 10 * 60 }) == Tue + 6, "to the north over a Sunday: Thursday to Monday");
		Check(!M.PostCard({ Tue, 600 }, "h_holm", "nope", "hello", "day"), "an unknown picture is refused");
		Check(!M.PostCard({ Tue, 600 }, "", "lighthouse", "hello", "day"), "nobody to send to is refused");
	}

	// --- the data names words that exist in every language
	const char* Langs[] = { "ru", "en", "ja" };
	for (const char* Lang : Langs)
	{
		const auto T = Table(Lang);
		std::vector<std::string> Keys = { Data.InviteGeneric, Data.PenNudge, Data.PenWho, Data.PenPlace, Data.PenAnswerNone };
		for (const auto& P : Data.Pictures) Keys.push_back("letters.pic." + P);
		for (const auto& P : Data.Openings) Keys.push_back("letters.phrase." + P);
		for (const auto& P : Data.Wishes) Keys.push_back("letters.phrase." + P);
		for (const auto& R : Data.Replies) for (const auto& K : R.second) Keys.push_back(K);
		for (const auto& C : Data.Correspondents) { Keys.push_back(C.Name); Keys.push_back(C.Invite); Keys.push_back(C.Shelfnote); for (const auto& K : C.Replies) Keys.push_back(K); }
		for (const auto& Th : Data.Thanks) { Keys.push_back(Th.From); Keys.push_back(Th.Body); }
		for (const auto& F : Data.InviteFestival) { Keys.push_back(F.second); Keys.push_back("festival." + F.first + ".name"); }
		for (const auto& P : Data.Pen) { Keys.push_back(P.Body); Keys.push_back(P.Diary); }
		for (const auto& A : Data.PenAcks) for (const auto& K : A.second) Keys.push_back(K);
		for (const auto& A : Data.PenAnswers) Keys.push_back(A.second);
		for (const auto& H : Data.Homes) Keys.push_back("letters.street." + H.second[0]);
		std::set<std::string> Missing;
		for (const auto& K : Keys) if (!Has(T, K)) Missing.insert(K);
		for (const auto& K : Missing) Check(false, std::string(Lang) + ": no words for " + K);
	}
	Check(Data.Pen.size() >= 6, "the pen friend writes at least six letters");
	Check(Data.Correspondents.size() >= 5, "five or more correspondents in the open homes");

	// --- an autumn of post: the hero sends cards and answers every letter
	{
		FRun R;
		R.M.Data = Data;
		Simulate(R, 75, true, true);
		Check(R.M.PenSent == (int)Data.Pen.size(), "all of Tonya's letters came within 75 days (" + std::to_string(R.M.PenSent) + ")");
		Check(R.Delivered == 12, "every card was carried (" + std::to_string(R.Delivered) + " of 12)");
		Check(R.ArrivedOnWeekday["Sun"] == 0, "nothing arrives on a Sunday");
		std::map<std::string, int> Kinds;
		std::set<std::string> AckFacts;
		for (const FLetter& L : R.M.Letters)
		{
			Kinds[L.Kind]++;
			Check(L.bArrived, "every letter has come by the end (" + L.Id + ")");
			if (L.Kind == "pen" && L.Id != "pen:1")
			{
				Check(!L.Ack.empty(), "her letters after the first say what he told her (" + L.Id + ")");
				for (const auto& A : L.AckArgs) AckFacts.insert(A);
			}
		}
		Check(Kinds["home"] == 2, "Holm answered both cards (" + std::to_string(Kinds["home"]) + ")");
		Check(Kinds["thanks"] == 2, "two quests, two thank-you letters");
		Check(Kinds["reply"] >= 3 && Kinds["reply"] <= 10, "some townsfolk answer, not all (" + std::to_string(Kinds["reply"]) + " of 10)");
		Check(Kinds["invite"] >= 5, "invitations: Holm's and the holidays' (" + std::to_string(Kinds["invite"]) + ")");
		Check(AckFacts.size() >= 4, "her letters name what he really did, different things (" + std::to_string(AckFacts.size()) + ")");
		// a holiday's invitation comes before the holiday
		for (const FLetter& L : R.M.Letters)
		{
			if (L.Id.rfind("invite:", 0) == 0)
			{
				const int On = std::stoi(L.Id.substr(L.Id.find('@') + 1));
				Check(L.Arrive <= On && L.Arrive >= On - Data.Post.FestivalNotice - 1, "an invitation comes just before its holiday (" + L.Id + ")");
			}
			if (L.Id == "home:h_holm:1")
			{
				Check(L.Arrive == R.M.TownReplyArrive(R.M.DeliveryDay({ R.Start + 2, 12 * 60 })), "Holm's answer comes two post days after his card");
			}
			if (L.Id == "thanks:waxwing")
			{
				const int Due = IsPostDay(R.Start + 5) ? R.Start + 5 : NextPostDay(R.Start + 5);
				Check(L.Arrive == Due, "the waxwing thanks come the post day after the quest (" + Date(L.Arrive) + ")");
			}
		}
		// the thank-you in the street: recipients of recent cards, once each
		const int Last = R.Start + 10;
		FModel& M = R.M;
		(void)Last;
		std::vector<std::string> Want = M.WantsToThank(R.Start + 3);
		Check(Want.empty(), "nobody thanks for cards long past (" + std::to_string(Want.size()) + ")");
		// the same autumn with a save and load in the middle comes out the same
		FRun R2;
		R2.M.Data = Data;
		Simulate(R2, 75, true, true, 23);
		std::map<std::string, std::string> A, B;
		R.M.Save(A);
		R2.M.Save(B);
		Check(A == B, "a save and load on day 23 changes nothing");
	}

	// --- the thanks in the street, step by step
	{
		FModel M;
		M.Data = Data;
		FFacts Facts;
		M.PostCard({ Tue, 9 * 60 }, "t_somebody", "fountain", "morning", "care", "elder");
		M.Step({ Tue + 1, 10 * 60 }, Facts);
		Check(M.WantsToThank(Tue + 1).empty(), "not before the card is carried (11:00)");
		M.Step({ Tue + 1, 12 * 60 }, Facts);
		Check(M.WantsToThank(Tue + 1).size() == 1, "the day it is carried: wants to thank");
		M.NoteThanked("t_somebody");
		Check(M.WantsToThank(Tue + 2).empty(), "thanked once");
		M.PostCard({ Tue + 2, 9 * 60 }, "t_somebody", "fountain", "morning", "care", "elder");
		M.Step({ Tue + 3, 12 * 60 }, Facts);
		Check(M.WantsToThank(Tue + 3).size() == 1, "a second card: thanks again");
		Check(M.WantsToThank(Tue + 3 + Data.Post.ThanksWindow + 1).empty(), "forgotten after the window");
	}

	// --- he never answers Tonya: one nudge, ten days after he read her letter
	{
		FRun R;
		R.M.Data = Data;
		Simulate(R, 40, false, false);
		int Nudges = 0;
		int ReadDay = -1, NudgeArrive = -1;
		for (const FLetter& L : R.M.Letters)
		{
			if (L.Id == "pen:1") ReadDay = L.ReadDay;
			if (L.Id.find(":nudge") != std::string::npos) { ++Nudges; NudgeArrive = L.Arrive; }
		}
		Check(R.M.PenSent == 1, "without answers she writes no further letters");
		Check(Nudges == 1, "one nudge, not more (" + std::to_string(Nudges) + ")");
		Check(NudgeArrive - ReadDay >= Data.Post.PenNudgeDays && NudgeArrive - ReadDay <= Data.Post.PenNudgeDays + 2,
			"the nudge comes about ten days after her letter was read");
	}

	// --- answers come from what he did, newest first, nothing repeated while something new is left
	{
		FModel M;
		M.Data = Data;
		FFacts Facts;
		Facts.Birds = { "bird.a.name", "bird.b.name", "bird.c.name", "bird.d.name" };
		M.Step({ Tue, 600 }, Facts);
		M.Step({ Tue + Data.Post.PenStartDay, 9 * 60 }, Facts);
		M.Step({ Tue + Data.Post.PenStartDay, 11 * 60 }, Facts);
		Check(M.NumUnread() == 1, "her first letter came on the third day with a bird met");
		Check(!M.PenWaiting(), "nothing to answer before it is read");
		M.MarkRead(0, Tue + 2);
		const std::vector<FAnswer> O = M.PenOptions(Facts);
		Check(O.size() == 4 && O[0].Fact == "bird.d.name" && O[1].Fact == "bird.c.name" && O[2].Fact == "bird.b.name", "the three newest birds first");
		Check(M.PenAsk() == "bird", "her first letter asks about birds");
	}
	// a quick start without birds: she still writes, later
	{
		FModel M;
		M.Data = Data;
		FFacts Facts;
		M.Step({ Tue, 600 }, Facts);
		M.Step({ Tue + Data.Post.PenStartDay + 1, 12 * 60 }, Facts);
		Check(M.PenSent == 0, "no bird met: no letter on the third day");
		M.Step({ Tue + Data.Post.PenStartLate + 2, 12 * 60 }, Facts);
		Check(M.PenSent == 1, "no bird met: she writes anyway after a few days");
	}
	// a jump of a month at once (sleeping, a load): everything due arrives, nothing twice
	{
		FModel M;
		M.Data = Data;
		FFacts Facts;
		M.Step({ Tue, 17 * 60 }, Facts);
		Facts.QuestsDone = { "waxwing", "painter_views", "autumn_count" };
		M.Step({ Tue + 30, 12 * 60 }, Facts);
		if (M.Letters.size() != 4 || M.NumOnTheWay() != 4) for (const FLetter& L : M.Letters) std::printf("  %s arrive %s arrived %d\n", L.Id.c_str(), Date(L.Arrive).c_str(), (int)L.bArrived);
		Check(M.Letters.size() == 4 && M.NumOnTheWay() == 4, "a jump of a month: the thanks and her first letter are written, on their way");
		const FEvents E = M.Step({ Tue + 33, 12 * 60 }, Facts);
		Check(E.Arrived.size() == M.Letters.size(), "a few days on: every letter has come at once");
		const FEvents E2 = M.Step({ Tue + 33, 12 * 60 + 15 }, Facts);
		Check(E2.Arrived.empty(), "and none comes twice");
	}

	std::printf("lettersim: %s (%d failures)\n", Failures ? "FAIL" : "ok", Failures);
	return Failures;
}
