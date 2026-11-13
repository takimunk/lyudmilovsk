#include "Sea/LyudEcosystem.h"

#include "EcoModel.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FishModel.h"
#include "HAL/IConsoleManager.h"
#include "Plants/GameCalendar.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSeabed.h"
#include "Sky/LyudSky.h"

namespace
{
	/** The web is stepped in whole game hours; a jump of more than this is lived through at once (a load, a long sleep). */
	constexpr double EcoMaxCatchUpDays = 3.0 * 365.0;
	/** A fresh game starts from the base, lived through this many years of seasons first (the base has no seasons). */
	constexpr int32 EcoWarmYears = 6;
	/** ... a slice of it at each sync (every two seconds of play, ~1 ms each), never all in one frame. */
	constexpr float EcoWarmSliceDays = 60.f;

	FAutoConsoleCommandWithWorldAndArgs CmdEcoReport(TEXT("lyud.Eco.Report"), TEXT("Prints the coastal food web: every group's biomass against its base, here or away, what eats most of it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudEcosystem* E = ULyudEcosystem::Get(World))
			{
				E->Sync(World);
				TArray<FString> Lines;
				E->GetReport().ParseIntoArrayLines(Lines);
				for (const FString& L : Lines) UE_LOG(LogTemp, Display, TEXT("LyudEco: %s"), *L);
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdEcoDays(TEXT("lyud.Eco.Days"), TEXT("<days>: the food web lives that many days at once (from today's day of the year)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudEcosystem* E = ULyudEcosystem::Get(World); E && Args.Num()) E->LiveDays(FCString::Atof(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdEcoReset(TEXT("lyud.Eco.Reset"), TEXT("Back to the food web's base (warmed through a few years of seasons)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ULyudEcosystem* E = ULyudEcosystem::Get(World)) E->ResetWeb(World);
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdEcoScale(TEXT("lyud.Eco.Scale"), TEXT("<Id> <factor>: multiplies a group's biomass (lyud.Eco.Scale Cod 0.1: the cod fished out)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ULyudEcosystem* E = ULyudEcosystem::Get(World); E && Args.Num() >= 2)
			{
				if (!E->Scale(FName(*Args[0]), FCString::Atof(*Args[1]))) UE_LOG(LogTemp, Warning, TEXT("LyudEco: no group %s"), *Args[0]);
			}
		}));

	float EcoDayOfYear(const FDateTime& T) { return float(T.GetDayOfYear() - 1) + float(T.GetTimeOfDay().GetTotalHours() / 24.0); }
}

ULyudEcosystem::ULyudEcosystem() = default;
ULyudEcosystem::~ULyudEcosystem() = default;

ULyudEcosystem* ULyudEcosystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudEcosystem>() : nullptr;
}

void ULyudEcosystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Model = MakePimpl<LyudEco::FModel>();
	FishNode.SetNum(LyudFish::NumSpecies());
	for (int32 S = 0; S < LyudFish::NumSpecies(); S++) FishNode[S] = LyudEco::Find(LyudFish::Species(S).Id);
	Refresh();
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Ecosystem"), this);
		// a New Game, or a save made before the web existed (no record: OnGameLoaded is never called), starts it over
		Saves->OnNewGame.AddUObject(this, &ULyudEcosystem::StartOver);
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame* Save)
		{
			if (Save && !Save->Records.Contains(TEXT("Ecosystem"))) StartOver();
		});
	}
}

void ULyudEcosystem::Warm(const FDateTime& Now)
{
	// the base is balanced but has no seasons: live a few years up to today so the plankton, the migrants and the
	// young of the year are where the season puts them. Sliced over the next syncs (a minute or so of play) so a new
	// game never hitches; until then the fish see the base, which is close.
	WarmLeft = float(EcoWarmYears * 365);
	Model->Reset(EcoDayOfYear(Now) - WarmLeft);
	SteppedTo = Now;
	Refresh();
}

const ALyudSky* ULyudEcosystem::FindSky(const UWorld* World) const
{
	if (!World) return nullptr;
	ALyudSky* Sky = SkyRef.Get();
	if (!Sky || Sky->GetWorld() != World)
	{
		Sky = nullptr;
		if (TActorIterator<ALyudSky> It(const_cast<UWorld*>(World)); It) Sky = *It;
		SkyRef = Sky;
	}
	return Sky;
}

void ULyudEcosystem::Sync(const UWorld* World)
{
	if (!World) return;
	FDateTime Now = UGameCalendar::NowIn(World);
	if (const ALyudSky* Sky = FindSky(World)) Now = Sky->ShownTime();
	if (SyncedWorld.Get() != World)
	{
		// a new level: a New Game (the calendar starts again, earlier than the web) or a save without the web starts it
		// fresh; a load with the web has already restored it (OnGameLoaded sets SyncedWorld); a trip to another level
		// with the clock running on keeps it
		SyncedWorld = World;
		if (SteppedTo.GetTicks() != 0 && Now < SteppedTo)
		{
			SteppedTo = FDateTime(0);
			WarmLeft = 0.f;
		}
	}
	Sync(Now);
}

void ULyudEcosystem::Sync(const FDateTime& Now)
{
	if (!Model) return;
	if (SteppedTo.GetTicks() == 0)
	{
		Warm(Now);
		return;
	}
	if (WarmLeft > 0.f)
	{
		const float Slice = FMath::Min(WarmLeft, EcoWarmSliceDays);
		Model->Step(EcoDayOfYear(SteppedTo) - WarmLeft, Slice);
		WarmLeft -= Slice;
		// the fish keep the base's numbers until the warm-up is through (no swings while it runs)
		if (WarmLeft <= 0.f) Refresh();
		return;
	}
	const double Days = (Now - SteppedTo).GetTotalDays();
	if (Days < 1.0 / 24.0)
	{
		// the clock went back (a load of an older save restores SteppedTo with it): just follow it
		if (Days < 0.0) SteppedTo = Now;
		return;
	}
	Model->Step(EcoDayOfYear(SteppedTo), float(FMath::Min(Days, EcoMaxCatchUpDays)));
	SteppedTo = Now;
	Refresh();
}

void ULyudEcosystem::Refresh()
{
	FishNow.SetNum(FishNode.Num());
	for (int32 S = 0; S < FishNode.Num(); S++)
	{
		// the fish model has the seasons itself (its months): it only needs how many there are against the usual
		FishNow[S] = FishNode[S] >= 0 ? FMath::Clamp(Model->Relative(FishNode[S]), 0.05f, 4.f) : 1.f;
	}
}

int32 ULyudEcosystem::NodeOf(FName Id) const
{
	return LyudEco::Find(TCHAR_TO_ANSI(*Id.ToString()));
}

float ULyudEcosystem::GetAbundance(FName Id) const
{
	const int32 N = NodeOf(Id);
	return Model && N >= 0 ? Model->Relative(N) * Model->Here(N) : 0.f;
}

float ULyudEcosystem::GetLocalAbundance(FName Id, const FVector& Location) const
{
	const int32 N = NodeOf(Id);
	return Model && N >= 0 ? Model->Relative(N) * Model->Here(N) * Model->Local(N, Location.X * 0.01f, Location.Y * 0.01f) : 0.f;
}

float ULyudEcosystem::GetBiomass(FName Id) const
{
	const int32 N = NodeOf(Id);
	return Model && N >= 0 ? Model->Biomass(N) : 0.f;
}

float ULyudEcosystem::GetDietShare(FName Predator, FName Prey) const
{
	const int32 J = NodeOf(Predator), I = NodeOf(Prey);
	return Model && I >= 0 && J >= 0 ? Model->DietShare(J, I) : 0.f;
}

float ULyudEcosystem::FishAbundance(int32 FishSpecies, float XMetres, float YMetres) const
{
	if (!Model || !FishNow.IsValidIndex(FishSpecies)) return 1.f;
	const int32 N = FishNode[FishSpecies];
	return FishNow[FishSpecies] * (N >= 0 ? Model->Local(N, XMetres, YMetres) : 1.f);
}

void ULyudEcosystem::ForEachFishDiet(TFunctionRef<void(int32, int32, float)> Fn) const
{
	if (!Model) return;
	for (int32 S = 0; S < FishNode.Num(); S++)
		for (int32 P = 0; P < FishNode.Num(); P++)
			if (FishNode[S] >= 0 && FishNode[P] >= 0) Fn(S, P, Model->DietShare(FishNode[S], FishNode[P]));
}

TMap<FName, float> ULyudEcosystem::GetBiteWeights(const FVector& Location) const
{
	TMap<FName, float> Out;
	if (!Model) return Out;
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const ALyudSea* Sea = World ? ALyudSea::Get(World) : nullptr;
	const float Depth = Sea ? Sea->DepthAt(Location) * 0.01f : 3.f;
	float Sand = 1.f, Stone = 0.f, Weed = 0.f;
	ULyudSeabedComponent::HabitatAt(Location.X, Location.Y, Depth, Sand, Stone, Weed);
	LyudFish::FClock Clock;
	Clock.Month = SteppedTo.GetTicks() ? SteppedTo.GetMonth() : 10;
	Clock.Daylight = 1.f;
	if (World)
	{
		if (const ALyudSky* Sky = FindSky(World)) Clock.Daylight = FMath::SmoothStep(-6.f, 6.f, Sky->SunAltitude());
	}
	for (int32 S = 0; S < FishNode.Num(); S++)
	{
		const LyudFish::FSpecies& Sp = LyudFish::Species(S);
		// only fish take a bait (no shrimp, mysids or seals on the hook)
		if (Sp.Way == LyudFish::EWay::Crawler || Sp.Way == LyudFish::EWay::Swarm || Sp.bBreathes) continue;
		const float W = FishAbundance(S, Location.X * 0.01f, Location.Y * 0.01f) * LyudFish::Suitability(S, Sand, Stone, Weed, Depth)
			* LyudFish::Activity(S, Clock) * Sp.Abundance;
		if (W > 0.f) Out.Add(FName(ANSI_TO_TCHAR(Sp.Id)), W);
	}
	return Out;
}

void ULyudEcosystem::ReportCatch(FName Species, const FVector& Location, float Grams)
{
	const int32 N = NodeOf(Species);
	if (!Model || N < 0 || !FMath::IsFinite(Grams) || Location.ContainsNaN()) return;
	const int32 S = LyudFish::FindSpecies(TCHAR_TO_ANSI(*Species.ToString()));
	if (Grams < 0.f)
	{
		// a typical one: weight ~ 10 g at 10 cm, ~ length cubed
		const float L = S >= 0 ? LyudFish::Species(S).Length : 0.2f;
		Grams = 10.f * FMath::Pow(L / 0.1f, 3.f);
	}
	Model->Remove(N, Grams);
	// the spot: a bottom fish was one of a few in its 40 m, a roamer one of many
	const bool bSettled = S >= 0 && LyudFish::IsSettled(LyudFish::Species(S).Way);
	Model->TakeLocal(N, Location.X * 0.01f, Location.Y * 0.01f, bSettled ? 0.25f : 0.08f);
	Refresh();
}

float ULyudEcosystem::GetBloom() const
{
	// the diatoms' spring bloom and the blue-greens' August bloom, together against their usual
	const int32 P = LyudEco::Find("Phyto"), C = LyudEco::Find("Cyano");
	if (!Model || P < 0 || C < 0) return 1.f;
	const float Now = Model->Biomass(P) + Model->Biomass(C);
	const float Usual = LyudEco::Node(P).B + LyudEco::Node(C).B;
	return Usual > 0.f ? Now / Usual : 1.f;
}

FString ULyudEcosystem::GetReport() const
{
	if (!Model) return FString();
	return FString::Printf(TEXT("food web stepped to %s\n%s"), *SteppedTo.ToString(), ANSI_TO_TCHAR(Model->Report().c_str()));
}

void ULyudEcosystem::LiveDays(float Days)
{
	if (!Model || Days <= 0.f) return;
	if (WarmLeft > 0.f)
	{
		// finish the warm-up first (a console command, not play)
		Model->Step(EcoDayOfYear(SteppedTo) - WarmLeft, WarmLeft);
		WarmLeft = 0.f;
	}
	Model->Step(EcoDayOfYear(SteppedTo), FMath::Min(Days, float(EcoMaxCatchUpDays)));
	Refresh();
}

void ULyudEcosystem::ResetWeb(const UWorld* World)
{
	if (!Model) return;
	SteppedTo = FDateTime(0);
	Sync(World);
}

bool ULyudEcosystem::Scale(FName Id, float Factor)
{
	const int32 N = NodeOf(Id);
	if (!Model || N < 0 || !FMath::IsFinite(Factor)) return false;
	Model->SetBiomass(N, Model->Biomass(N) * FMath::Max(Factor, 0.f));
	Refresh();
	return true;
}

void ULyudEcosystem::StartOver()
{
	SteppedTo = FDateTime(0);
	WarmLeft = 0.f;
	SyncedWorld = nullptr;
}

void ULyudEcosystem::OnGameSaving_Implementation()
{
	SavedKeys.Reset();
	SavedValues.Reset();
	if (Model && WarmLeft > 0.f)
	{
		// saved during a fresh game's warm-up: finish it now (WarmLeft is not saved), so the save holds a whole web
		Model->Step(EcoDayOfYear(SteppedTo) - WarmLeft, WarmLeft);
		WarmLeft = 0.f;
		Refresh();
	}
	if (!Model) return;
	std::vector<std::string> K;
	std::vector<float> V;
	Model->Save(K, V);
	for (size_t I = 0; I < K.size(); I++)
	{
		SavedKeys.Add(ANSI_TO_TCHAR(K[I].c_str()));
		SavedValues.Add(V[I]);
	}
}

void ULyudEcosystem::OnGameLoaded_Implementation()
{
	if (!Model) return;
	if (SavedKeys.Num() == 0 || SteppedTo.GetTicks() == 0)
	{
		// an older save without the web: start it fresh at the save's time on the next sync
		SteppedTo = FDateTime(0);
		return;
	}
	std::vector<std::string> K;
	std::vector<float> V;
	for (int32 I = 0; I < SavedKeys.Num() && I < SavedValues.Num(); I++)
	{
		K.emplace_back(TCHAR_TO_ANSI(*SavedKeys[I]));
		V.push_back(SavedValues[I]);
	}
	WarmLeft = 0.f;
	if (const UGameInstance* GI = GetGameInstance()) SyncedWorld = GI->GetWorld();
	Model->Reset(EcoDayOfYear(SteppedTo));
	Model->Load(K, V, EcoDayOfYear(SteppedTo));
	Refresh();
}
