// The street soda-water automats (docs/soda_machines.md) on the real data: a coin, the glass rinsed or not (a passer-by minds),
// filled to the line and drunk in sips; the syrup of the day as a stamp; the coin swallowed now and then and freed by a thump
// on the side (at most once a day a machine), lost when the thump is missed; the machine shut out of its months and on its
// broken days, which are the days its building's "broken" sign shows (DayDie == DressHash01). Included by citytest.cpp (its
// CHECK, ReadFile, AtDate).
#include "../../unreal/Lyudmilovsk/Source/Lyudmilovsk/Private/City/CityDressModel.h"

namespace SodaChecks
{
	enum class EHand { Good, Clumsy, NoThump };

	struct FPlayed
	{
		FProcEnd End;
		std::vector<std::string> Lines;
		bool bJam = false;
		bool bOver = false;
	};

	// Pick: 0 rinse, 1 as it is. Good: pours to the middle of the mark, sips on every tick, thumps three times; Clumsy: holds the
	// button over the brim and gulps; NoThump: like Good but never thumps.
	static FPlayed Play(FModel& M, int Thing, const FWhen& When, const FAsk& Ask, int Pick, EHand How)
	{
		FPlayed P;
		FProcState G;
		if (!M.StartProc(Thing, When, {}, G))
		{
			return P;
		}
		const FProcDef* D = M.ProcDef(G.Def);
		const double Dt = 1.0 / 60.0;
		int LastStep = -1;
		for (int Frame = 0; Frame < 60 * 200 && !G.bOver; ++Frame)
		{
			const FProcStep& S = D->Steps[(size_t)G.Step];
			if (G.Step != LastStep)
			{
				LastStep = G.Step;
				P.Lines.push_back(G.Line);
				P.bJam |= S.Do == EProcStep::Tap;
			}
			const double T = G.Clock - G.StepFrom;
			bool bDown = false;
			if (!G.bSettled)
			{
				switch (S.Do)
				{
				case EProcStep::Pick:
					M.PressProc(G, Pick);
					break;
				case EProcStep::Pour:
					bDown = How == EHand::Clumsy ? true : G.Level < (S.MarkLo + S.MarkHi) * 0.5;
					break;
				case EProcStep::Sip:
				{
					const double K = (T - FModel::ProcLead) / S.Every;
					if (How != EHand::Clumsy && K > -0.01 && std::fabs(K - std::floor(K + 0.5)) * S.Every < Dt * 0.6)
					{
						M.PressProc(G, -1);
					}
					break;
				}
				case EProcStep::Tap:
					if (How != EHand::NoThump && Frame % 8 == 0)
					{
						M.PressProc(G, -1);
					}
					break;
				default:
					if (T > 1.0)
					{
						M.PressProc(G, -1);
					}
					break;
				}
			}
			M.TickProc(G, Dt, bDown);
		}
		P.bOver = G.bOver;
		if (G.bOver)
		{
			P.End = M.FinishProc(G, When, Ask);
		}
		return P;
	}

	static bool Has(const std::vector<std::string>& Lines, const std::string& Prefix)
	{
		for (const std::string& L : Lines)
		{
			if (L.rfind(Prefix, 0) == 0)
			{
				return true;
			}
		}
		return false;
	}
}

static void SodaMachineChecks(const std::string& Path)
{
	using namespace SodaChecks;
	std::string Text, Error;
	if (!ReadFile(Path, Text))
	{
		return;
	}
	std::printf("the street soda-water automats (real data)\n");
	FModel M;
	CHECK(M.Load(Text, Error), "it loads (%s)", Error.c_str());
	const int Plain = M.FindThing("SM-SODA1/plain"), Syrup = M.FindThing("SM-SODA1/syrup"), Park = M.FindThing("SM-SODA6/syrup");
	if (Plain < 0 || Syrup < 0 || Park < 0)
	{
		CHECK(false, "the six machines' things are there (SM-SODA1/plain, SM-SODA1/syrup, SM-SODA6/syrup)");
		return;
	}
	int Machines = 0;
	for (int K = 1; K <= 6; ++K)
	{
		Machines += M.FindThing("SM-SODA" + std::to_string(K) + "/plain") >= 0 && M.FindThing("SM-SODA" + std::to_string(K) + "/syrup") >= 0;
	}
	CHECK(Machines == 6, "six machines, each with its plain and its syrup button (%d)", Machines);
	FAsk Ask;
	Ask.Money = 1000;
	// a July day (day number D, the game's day Since = D - 1000); weather "warm" (0x10) so the hot lines may show
	auto July = [](long long Since, float Hour) { return AtDate((int)(Since % 7), Hour, 1000 + Since, 715, Since); };
	auto Find = [&](bool bBroken, long long From) {
		for (long long S = From; S < From + 400; ++S)
		{
			if (M.IsBroken(Plain, July(S, 12.f)) == bBroken && M.IsBroken(Syrup, July(S, 12.f)) == bBroken)
			{
				return S;
			}
		}
		return (long long)-1;
	};
	const long long Fine = Find(false, 10), Bad = Find(true, 10);
	CHECK(Fine >= 0 && Bad >= 0, "there are working days and broken days (%lld, %lld)", Fine, Bad);

	// the price on the buttons, the scene on both
	CHECK(M.Run(Plain, July(Fine, 12.f), FAsk()).Verdict == EVerdict::NoMoney, "no coin, no water");
	CHECK(M.ProcNow(Plain, July(Fine, 12.f), Ask) >= 0 && M.ProcNow(Syrup, July(Fine, 12.f), Ask) >= 0, "both buttons play the scene");

	// plain, rinsed, a careful hand: the good words, a coin of 1, no stamp
	const FPlayed A = Play(M, Plain, July(Fine, 12.f), Ask, 0, EHand::Good);
	CHECK(A.bOver && A.End.Result == 0 && A.End.Line.rfind("city.soda.p.good.plain.", 0) == 0, "rinsed, filled to the line, sipped in time: good (%d %s)",
		A.End.Result, A.End.Line.c_str());
	CHECK(A.End.Run.Verdict == EVerdict::Ok && A.End.Run.NeedMoney == 1, "the act runs after: one coin (%d)", A.End.Run.NeedMoney);
	CHECK(Has(A.Lines, "city.soda.p.rinsed.yes.") && !Has(A.Lines, "city.soda.p.rinsed.no.") && Has(A.Lines, "city.soda.p.back."),
		"rinsed: the washer's fountain, and the glass goes back on it");
	CHECK(!Has(A.Lines, "city.soda.p.syrup."), "no syrup from the plain button");

	// not rinsed: somebody behind minds
	const FPlayed B = Play(M, Plain, July(Fine, 13.f), Ask, 1, EHand::Good);
	CHECK(Has(B.Lines, "city.soda.p.rinsed.no.") && !Has(B.Lines, "city.soda.p.rinsed.yes."), "not rinsed: a passer-by's remark");

	// the syrup of the day: six flavours over six days, each drunk is a stamp
	static const char* Flavours[] = { "cherry", "duchesse", "cream_soda", "tarragon", "lemon", "apple" };
	std::set<std::string> Poured;
	int Days = 0;
	for (long long S = Fine; Poured.size() < 6 && S < Fine + 60; ++S)
	{
		if (M.IsBroken(Syrup, July(S, 12.f)))
		{
			continue;
		}
		const FPlayed C = Play(M, Syrup, July(S, 11.f), Ask, 0, EHand::Good);
		const std::string Fl = Flavours[(int)(S % 6)];
		CHECK(Has(C.Lines, "city.soda.p.syrup." + Fl) && C.End.Result == 0 && M.HasStamp("soda_syrups/" + Fl),
			"day %lld: the syrup is %s, a good glass stamps it (%s)", S, Fl.c_str(), C.End.Line.c_str());
		CHECK(C.End.Run.NeedMoney == 3, "with syrup: three coins (%d)", C.End.Run.NeedMoney);
		Poured.insert(Fl);
		++Days;
	}
	CHECK(Poured.size() == 6 && M.CountStamps("soda_syrups") == 6, "all six syrups in %d good days (%d)", Days, (int)Poured.size());

	// a clumsy hand: poor words, no stamp
	FModel M2;
	M2.Load(Text, Error);
	const FPlayed Clumsy = Play(M2, Syrup, July(Fine, 12.f), Ask, 0, EHand::Clumsy);
	CHECK(Clumsy.End.Result == 2 && Clumsy.End.Line.rfind("city.soda.p.poor.", 0) == 0 && M2.CountStamps("soda_syrups") == 0,
		"over the brim and gulped: poor, no stamp (%d %s)", Clumsy.End.Result, Clumsy.End.Line.c_str());

	// the swallowed coin: at most once a day a machine, by the scene's die; over a season it does happen
	int Jams = 0, Scenes = 0;
	bool bTwice = false;
	FModel M3;
	M3.Load(Text, Error);
	for (long long S = 0; S < 120; ++S)
	{
		int Today = 0;
		for (int K = 0; K < 4; ++K)
		{
			if (M3.IsBroken(Plain, July(S, 10.f)))
			{
				break;
			}
			const FPlayed J = Play(M3, Plain, July(S, 10.f + K), Ask, 0, EHand::Good);
			Scenes += J.bOver ? 1 : 0;
			Today += J.bJam ? 1 : 0;
		}
		Jams += Today;
		bTwice |= Today > 1;
	}
	CHECK(!bTwice && Jams > 5 && Jams < Scenes / 2, "the coin sticks now and then, never twice a day at one machine (%d in %d scenes)", Jams, Scenes);
	// with the dev flag the coin always sticks: thumped, the scene goes on to the glass; not thumped, it ends with nothing
	GDevChanceSteps = true;
	FModel M4;
	M4.Load(Text, Error);
	const FPlayed Thumped = Play(M4, Plain, July(Fine, 12.f), Ask, 0, EHand::Good);
	const FPlayed Lost = Play(M4, Syrup, July(Fine, 12.f), Ask, 0, EHand::NoThump);
	GDevChanceSteps = false;
	CHECK(Thumped.bJam && Has(Thumped.Lines, "city.soda.p.jam") && Has(Thumped.Lines, "city.soda.p.back.") && Thumped.End.Result == 0,
		"a thump on the side: the water comes after all (%d)", Thumped.End.Result);
	CHECK(Lost.bJam && Lost.bOver && !Has(Lost.Lines, "city.soda.p.rinse.") && Lost.End.Result == 2 && M4.CountStamps("soda_syrups") == 0
		&& Lost.End.Run.NeedMoney == 3, "no thump: the scene ends there, the coin is gone, no syrup stamp (%d)", Lost.End.Result);

	// shut out of season and on broken days: the closed words, nothing taken
	const FWhen Winter = AtDate(2, 12.f, 1000 + Fine, 115, Fine);
	const FOutcome W = M.Run(Plain, Winter, Ask);
	CHECK(M.IsShut(Plain, Winter, false) && W.bClosed && W.Line.rfind("city.soda.closed.winter.", 0) == 0 && M.ProcNow(Plain, Winter, Ask) < 0,
		"January: under its tarpaulin, the winter words (%s)", W.Line.c_str());
	const FWhen Nov = AtDate(2, 12.f, 1000 + Fine, 1105, Fine), May = AtDate(2, 12.f, 1000 + Fine, 501, Fine);
	CHECK(M.IsShut(Plain, Nov, false) && !M.IsShut(Plain, May, false), "it works from May, not in November");
	const FOutcome Br = M.Run(Syrup, July(Bad, 12.f), Ask);
	CHECK(Br.bClosed && Br.Line.rfind("city.soda.closed.broken.", 0) == 0, "a broken day: the cardboard sign's words (%s)", Br.Line.c_str());
	CHECK(!M.IsShut(Plain, July(Bad, 12.f), true), "open all hours (developer mode) opens it anyway");

	// the broken days are the days the building's sign shows, about 12% of them
	int Broken = 0, Agree = 0;
	for (long long D = 0; D < 2000; ++D)
	{
		const FWhen At2 = AtDate(0, 12.f, D, 715, D);
		const bool bB = M.IsBroken(Park, At2);
		Broken += bB;
		Agree += bB == (LyudCity::DressHash01("SM-SODA6", "broken", D) < 0.12f);
	}
	CHECK(Agree == 2000, "the thing's broken days are the dressing set's sign days (%d of 2000)", Agree);
	CHECK(Broken > 170 && Broken < 320, "broken on about 12%% of the days (%d of 2000)", Broken);
	CHECK(LyudCity::DayDie(LyudCity::Fnv32("P-X/y@"), -3) == LyudCity::DressHash01("P-X", "y", -3), "the die agrees on a negative day too");
}
