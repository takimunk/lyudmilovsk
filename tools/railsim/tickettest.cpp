// Offline checks of riding the sea railway as a passenger (Private/Railway/RailTicketModel.h) on Content/Railway/passenger.json
// and its strings (Content/Localization/rail). Run: tools/railsim/tickets.sh
#include "Railway/RailTicketModel.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

static int Fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++Fails; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::string Read(const std::string& Path)
{
	std::ifstream F(Path, std::ios::binary);
	std::stringstream S;
	S << F.rdbuf();
	return S.str();
}

int main()
{
	using namespace LyudTicket;
	FModel M;
	CHECK(M.Load(Read("unreal/Lyudmilovsk/Content/Railway/passenger.json")), "load: %s", M.LastError.c_str());
	CHECK(M.Fare > 0 && M.FineRoubles > M.Fare, "the fine (%d) must cost more than the fare (%d)", M.FineRoubles, M.Fare);

	for (const char* Lang : { "ru", "en", "ja" })
	{
		LyudJson::FValue T;
		std::string Err;
		CHECK(LyudJson::Parse(Read(std::string("unreal/Lyudmilovsk/Content/Localization/rail/strings_") + Lang + ".json"), T, Err), "%s: %s", Lang, Err.c_str());
		std::set<std::string> K;
		for (const auto& KV : T.Fields) K.insert(KV.first);
		for (const FConductor& C : M.Conductors)
		{
			for (const std::string* S : { &C.Name, &C.Ask, &C.Punch, &C.Again, &C.Warn, &C.Fine, &C.Broke })
			{
				CHECK(K.count(*S), "%s: no %s", Lang, S->c_str());
			}
		}
		for (const char* S : { "rail.ticket.prompt", "rail.ticket.bought", "rail.ticket.no_money", "rail.fine.paid", "rail.conductor.says",
		                       "rail.announce.next", "rail.announce.arrive", "rail.announce.doors", "rail.announce.last",
		                       "item.rail_ticket.name", "item.rail_ticket_punched.name" })
		{
			CHECK(K.count(S), "%s: no %s", Lang, S);
		}
	}

	// every conductor works some days, the same one all day
	std::vector<int> Days(M.Conductors.size(), 0);
	for (long long D = 739000; D < 739000 + 365; ++D)
	{
		const int C = M.ConductorOn(D);
		CHECK(C >= 0 && C < (int)M.Conductors.size(), "day %lld: conductor %d", D, C);
		if (C >= 0) ++Days[(size_t)C];
		CHECK(C == M.ConductorOn(D), "day %lld changes its conductor", D);
	}
	for (size_t I = 0; I < Days.size(); ++I)
	{
		CHECK(Days[I] > 365 / (int)Days.size() / 3, "%s works only %d days of a year", M.Conductors[I].Id.c_str(), Days[I]);
	}

	// the checks: a ticket is punched and taken; punched already: waved on; none: warned Warns times, then fined, or put off
	for (int C = 0; C < (int)M.Conductors.size(); ++C)
	{
		const FConductor& Co = M.Conductors[(size_t)C];
		FCheck R = M.Check(C, 2, false, 5, 0);
		CHECK(R.Verdict == EVerdict::Punch && R.bTakeTicket && R.Fine == 0 && R.Line == Co.Punch, "%s: a ticket must be punched", Co.Id.c_str());
		R = M.Check(C, 0, true, 0, 0);
		CHECK(R.Verdict == EVerdict::Again && !R.bTakeTicket, "%s: punched this ride must pass", Co.Id.c_str());
		R = M.Check(C, 1, true, 0, 0);
		CHECK(R.Verdict == EVerdict::Again && !R.bTakeTicket, "%s: a second ticket must not be punched on the same ride", Co.Id.c_str());
		int Offences = 0, Money = 100, Warned = 0, Fined = 0;
		for (int Ride = 0; Ride < 6; ++Ride)
		{
			R = M.Check(C, 0, false, Offences, Money);
			CHECK(FModel::IsOffence(R.Verdict) && !R.bTakeTicket, "%s: no ticket is an offence", Co.Id.c_str());
			Warned += R.Verdict == EVerdict::Warn;
			Fined += R.Verdict == EVerdict::Fine;
			Money -= R.Fine;
			++Offences;
		}
		CHECK(Warned == Co.Warns, "%s: %d warnings, wants %d", Co.Id.c_str(), Warned, Co.Warns);
		CHECK(Fined + Warned + (Money < M.FineRoubles ? 1 : 0) >= 6 - 1, "%s: fines %d", Co.Id.c_str(), Fined);
		R = M.Check(C, 0, false, 99, M.FineRoubles - 1);
		CHECK(R.Verdict == EVerdict::Broke && R.Fine == 0, "%s: no money for the fine must not fine", Co.Id.c_str());
	}
	CHECK(M.Check(-1, 1, false, 0, 0).Line.empty(), "no conductor says nothing");

	// the walk: from the end farther from the hero, stopping short of him, never past the car's end
	float From = 0, To = 0;
	FModel::Walk(-1500.f, 1500.f, 400.f, 90.f, From, To);
	CHECK(From == -1500.f && To == 310.f, "walk from the far end: %g -> %g", From, To);
	FModel::Walk(-1500.f, 1500.f, -700.f, 90.f, From, To);
	CHECK(From == 1500.f && To == -610.f, "walk from the other end: %g -> %g", From, To);
	FModel::Walk(-100.f, 100.f, 99.f, 300.f, From, To);
	CHECK(To == From, "a hero at the very end: no walk past it (%g -> %g)", From, To);

	LyudTicket::FModel Bad;
	CHECK(!Bad.Load("{\"fare\":5,\"fine\":20,\"conductors\":[]}"), "no conductors accepted");

	std::printf("%s: %zu conductors, fare %d, fine %d, %d failures\n", Fails ? "FAIL" : "ok", M.Conductors.size(), M.Fare, M.FineRoubles, Fails);
	return Fails ? 1 : 0;
}
