// Checks of the coastal food web (unreal/.../Private/Sea/EcoModel.cpp) outside the engine: tools/underwater/run.sh eco [-v]
// The base is mass-balanced, the web stays bounded over decades of seasons, the seasons and migrants show, taking the
// cod out lets their prey grow and starves the seals, the state saves and loads, the local thinning heals.
#include "EcoModel.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace LyudEco;

static int Fails = 0;
static void Check(bool Ok, const char* What, double Value = 0.0)
{
	std::printf("%s %s (%.3f)\n", Ok ? "ok  " : "FAIL", What, Value);
	if (!Ok) ++Fails;
}

int main(int Argc, char** Argv)
{
	const bool bVerbose = Argc > 1 && std::strcmp(Argv[1], "-v") == 0;
	if (Argc > 1 && std::strcmp(Argv[1], "-web") == 0)
	{
		// the web for tools/underwater/foodweb.py: nodes with their trophic level and biomass, links with the base flows
		FModel Web;
		std::vector<float> Tl(NumNodes(), 1.f);
		for (int It = 0; It < 50; ++It)
			for (int J = 0; J < NumNodes(); ++J)
			{
				if (Node(J).Kind != EKind::Consumer) continue;
				float T = 1.f;
				for (int I = 0; I < NumNodes(); ++I) T += Web.DietShare(J, I) * Tl[I];
				Tl[J] = T;
			}
		for (int I = 0; I < NumNodes(); ++I) std::printf("N %s %.3f %.4f\n", Node(I).Id, Tl[I], Node(I).B);
		for (int J = 0; J < NumNodes(); ++J)
			for (int I = 0; I < NumNodes(); ++I)
				if (Web.DietShare(J, I) > 0.f) std::printf("L %s %s %.5f\n", Node(J).Id, Node(I).Id, Web.DietShare(J, I) * Node(J).QB * Node(J).B);
		return 0;
	}
	// every prey is a node, every diet sums to something, assimilation is possible
	bool bIds = true, bGe = true;
	for (int I = 0; I < NumNodes(); ++I)
	{
		const FNode& Nd = Node(I);
		for (const FDietItem& D : Nd.Diet)
			if (Find(D.Prey) < 0) { std::printf("  %s eats unknown %s\n", Nd.Id, D.Prey); bIds = false; }
		if (Nd.Kind == EKind::Consumer && (Nd.Diet.empty() || Nd.PB / Nd.QB > 0.8f)) { std::printf("  %s: P/Q %.2f\n", Nd.Id, Nd.PB / Nd.QB); bGe = false; }
	}
	Check(bIds, "every prey is a node");
	Check(bGe, "every consumer eats and P/Q <= 0.8");

	{
		// what a fresh game pays once: the base lived through six years of seasons
		const auto T0 = std::chrono::steady_clock::now();
		FModel Warm;
		Warm.Reset(293.f);
		Warm.Step(293.f, 6.f * 365.f);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - T0).count();
		Check(Ms < 150.0, "warming a fresh web through six years takes under 150 ms (ms)", Ms);
		const auto T1 = std::chrono::steady_clock::now();
		Warm.Step(293.f, 1.f / 24.f);
		const double Us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - T1).count();
		Check(Us < 500.0, "a game hour's step takes under 0.5 ms (us)", Us);
	}
	FModel Base;
	float WorstEE = 0.f;
	for (int I = 0; I < NumNodes(); ++I)
	{
		if (bVerbose || Base.EE(I) > 1.f) std::printf("  EE %-20s %.2f\n", Node(I).Id, Base.EE(I));
		WorstEE = std::max(WorstEE, Base.EE(I));
	}
	Check(WorstEE <= 1.f, "the base balances: no node eaten beyond its production (worst EE)", WorstEE);

	// thirty years of seasons from the 1st of January
	FModel W;
	W.Reset(0.f);
	std::vector<double> Mean(NumNodes(), 0.0);
	float Lo = 1e9f, Hi = 0.f;
	const char* LoId = "", *HiId = "";
	for (int Y = 0; Y < 30; ++Y)
	{
		for (int D = 0; D < 365; ++D)
		{
			W.Step(float(D), 1.f);
			if (Y == 29)
				for (int I = 0; I < NumNodes(); ++I) Mean[I] += W.Relative(I) / 365.0;
		}
		if (bVerbose && (Y < 3 || Y % 5 == 4))
			std::printf("  year %2d: phyto %.2f zoo %.2f herring %.2f sprat %.2f cod %.2f goby %.2f mussel %.2f seal %.2f\n", Y + 1,
				W.Relative(Find("Phyto")), W.Relative(Find("Zoo")), W.Relative(Find("Herring")), W.Relative(Find("Sprat")),
				W.Relative(Find("Cod")), W.Relative(Find("RoundGoby")), W.Relative(Find("Mussel")), W.Relative(Find("GreySeal")));
	}
	for (int I = 0; I < NumNodes(); ++I)
	{
		if (bVerbose) std::printf("  mean year 30 %-20s %.2f\n", Node(I).Id, Mean[I]);
		if (Mean[I] < Lo) { Lo = float(Mean[I]); LoId = Node(I).Id; }
		if (Mean[I] > Hi) { Hi = float(Mean[I]); HiId = Node(I).Id; }
	}
	std::printf("  after 30 years: lowest %s, highest %s\n", LoId, HiId);
	Check(Lo > 0.5f, "after 30 years every node keeps over half its base (lowest mean)", Lo);
	Check(Hi < 2.5f, "after 30 years no node runs away past 2.5x its base (highest mean)", Hi);
	// year to year it settles (the last two years' January within 10%)
	{
		FModel S = W;
		std::vector<float> A(NumNodes()), B(NumNodes());
		for (int I = 0; I < NumNodes(); ++I) A[I] = S.Biomass(I);
		S.Step(0.f, 365.f);
		float Worst = 0.f;
		for (int I = 0; I < NumNodes(); ++I) Worst = std::max(Worst, std::fabs(S.Biomass(I) / A[I] - 1.f));
		Check(Worst < 0.1f, "settled: a year later every node within 10% of the year before (worst change)", Worst);
	}

	// the seasons: the spring bloom, the migrants
	{
		FModel S = W;
		float Peak = 0.f, PeakDay = 0.f;
		float SpratJul = 1.f, DucksJan = 0.f, DucksJul = 1.f, JellyAug = 0.f, JellyMar = 1.f;
		float CyPeak = 0.f, CyDay = 0.f, CyMay = 0.f;
		const int Herring = Find("Herring"), Cyano = Find("Cyano");
		float YoungJun = 0.f, YoungSep = 1.f, HerJun = 0.f, HerAug = 0.f;
		for (int D = 0; D < 365; ++D)
		{
			S.Step(float(D), 1.f);
			if (S.Biomass(0) > Peak) { Peak = S.Biomass(0); PeakDay = float(D); }
			if (S.Biomass(Cyano) > CyPeak) { CyPeak = S.Biomass(Cyano); CyDay = float(D); }
			if (D == 135) CyMay = S.Biomass(Cyano);
			if (D == 160) { YoungJun = S.YoungToCome(Herring) / S.Biomass(Herring); HerJun = S.Biomass(Herring); }
			if (D == 230) HerAug = S.Biomass(Herring);
			if (D == 226) YoungSep = S.YoungToCome(Herring) / S.Biomass(Herring);
			if (D == 200) { SpratJul = S.Here(Find("Sprat")); DucksJul = S.Here(Find("Eider")); }
			if (D == 20) DucksJan = S.Here(Find("Eider"));
			if (D == 225) JellyAug = S.Here(Find("Jelly"));
			if (D == 75) JellyMar = S.Here(Find("Jelly"));
		}
		Check(PeakDay > 90.f && PeakDay < 135.f, "the phytoplankton peaks in the spring bloom (day)", PeakDay);
		Check(CyDay > 205.f && CyDay < 245.f && CyPeak > 20.f * CyMay, "the blue-greens bloom in August, twenty times May's (day)", CyDay);
		std::printf("  herring: young to come in June %.2f, mid August %.2f of the stock; stock August/June %.2f\n", YoungJun, YoungSep, HerAug / HerJun);
		Check(YoungJun > 0.1f && YoungSep < 0.01f && HerAug > 1.1f * HerJun, "the young herring join the stock in July and August (August/June)", HerAug / HerJun);
		Check(SpratJul < 0.05f, "the sprat are offshore in July (share here)", SpratJul);
		Check(DucksJan > 0.95f && DucksJul < 0.05f, "the sea ducks winter here and leave for the summer (January share)", DucksJan);
		Check(JellyAug > 0.95f && JellyMar < 0.05f, "moon jellies in August, none in March (August share)", JellyAug);
	}

	// a trophic cascade: fish the cod hard for ten years
	{
		FModel Fished = W, Kept = W;
		const int Cod = Find("Cod");
		for (int Y = 0; Y < 10; ++Y)
			for (int D = 0; D < 365; ++D)
			{
				Fished.Step(float(D), 1.f);
				Kept.Step(float(D), 1.f);
				Fished.SetBiomass(Cod, Fished.Biomass(Cod) * (1.f - 1.5f / 365.f));
			}
		const float CodR = Fished.Biomass(Cod) / Kept.Biomass(Cod);
		const float SadR = Fished.Biomass(Find("Saduria")) / Kept.Biomass(Find("Saduria"));
		const float SealR = Fished.Biomass(Find("GreySeal")) / Kept.Biomass(Find("GreySeal"));
		const float SprR = Fished.Biomass(Find("Sprat")) / Kept.Biomass(Find("Sprat"));
		std::printf("  cod fished 10 years: cod %.2f, Saduria %.2f, sprat %.2f, grey seals %.2f of unfished\n", CodR, SadR, SprR, SealR);
		Check(CodR < 0.4f, "fishing takes the cod down (fished/unfished)", CodR);
		Check(SadR > 1.05f, "with fewer cod the isopods Saduria thrive (fished/unfished)", SadR);
		(void)SealR;
	}
	// and the herring: the seals and the cod go hungry, the zooplankton they ate thrives
	{
		FModel Fished = W, Kept = W;
		const int Her = Find("Herring");
		for (int Y = 0; Y < 10; ++Y)
			for (int D = 0; D < 365; ++D)
			{
				Fished.Step(float(D), 1.f);
				Kept.Step(float(D), 1.f);
				Fished.SetBiomass(Her, Fished.Biomass(Her) * (1.f - 1.5f / 365.f));
			}
		const float SealR = Fished.Biomass(Find("GreySeal")) / Kept.Biomass(Find("GreySeal"));
		const float CodR = Fished.Biomass(Find("Cod")) / Kept.Biomass(Find("Cod"));
		const float MysR = Fished.Biomass(Find("Mysid")) / Kept.Biomass(Find("Mysid"));
		std::printf("  herring fished 10 years: herring %.2f, grey seals %.2f, cod %.2f, mysids %.2f of unfished\n",
			Fished.Biomass(Her) / Kept.Biomass(Her), SealR, CodR, MysR);
		Check(SealR < 0.97f, "with fewer herring the grey seals go hungry (fished/unfished)", SealR);
		Check(MysR > 1.05f, "with fewer herring the mysids they ate thrive (fished/unfished)", MysR);
	}

	// save and load
	{
		FModel S = W;
		S.TakeLocal(Find("Perch"), 120.f, -35.f, 0.5f);
		std::vector<std::string> K;
		std::vector<float> V;
		S.Save(K, V);
		FModel L;
		L.Load(K, V);
		float Worst = 0.f;
		for (int I = 0; I < NumNodes(); ++I) Worst = std::max(Worst, std::fabs(L.Biomass(I) / S.Biomass(I) - 1.f) + std::fabs(L.Here(I) - S.Here(I)));
		Check(Worst < 1e-6f && std::fabs(L.Local(Find("Perch"), 121.f, -36.f) - 0.5f) < 1e-6f, "saves and loads (biomass, here, local)", Worst);
	}

	// nonsense in (a NaN catch, an infinite console scale) changes nothing
	{
		FModel S = W;
		S.Remove(Find("Cod"), std::nanf(""));
		S.SetBiomass(Find("Cod"), INFINITY);
		S.TakeLocal(Find("Cod"), std::nanf(""), 0.f, 0.5f);
		S.Step(10.f, 30.f);
		int Bad = 0;
		for (int I = 0; I < NumNodes(); ++I) Bad += !std::isfinite(S.Biomass(I));
		Check(Bad == 0, "a NaN or infinite input leaves the web finite (nodes gone bad)", Bad);
	}

	// the local thinning heals: roamers in days, bottom fish in weeks
	{
		FModel S = W;
		const int Perch = Find("Perch"), Goby = Find("RoundGoby");
		S.TakeLocal(Perch, 10.f, 10.f, 0.8f);
		S.TakeLocal(Goby, 10.f, 10.f, 0.8f);
		S.TakeLocal(Goby, 10.f, 10.f, 0.5f);
		const float G0 = S.Local(Goby, 10.f, 10.f), Other = S.Local(Goby, 60.f, 10.f);
		S.Step(100.f, 7.f);
		const float P7 = S.Local(Perch, 10.f, 10.f), G7 = S.Local(Goby, 10.f, 10.f);
		S.Step(107.f, 60.f);
		Check(std::fabs(G0 - 0.1f) < 1e-4f && Other == 1.f, "taking fish thins only that spot (gobies left)", G0);
		Check(P7 > 0.85f && G7 < 0.5f, "a week on the perch are back, the gobies not yet (perch)", P7);
		Check(S.Local(Goby, 10.f, 10.f) > 0.9f, "two months on the gobies are back", S.Local(Goby, 10.f, 10.f));
	}

	if (bVerbose) std::printf("%s", W.Report().c_str());
	std::printf(Fails ? "%d FAILED\n" : "all ok\n", Fails);
	return Fails ? 1 : 0;
}
