#include "City/LyudCityThings.h"

#include "CityThingsModel.h"
#include "City/SLyudCityGame.h"
#include "City/SLyudCityPage.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Doors/LyudDoor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Tasks/Task.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"
#include "World/LyudCityBuilding.h"
#include <atomic>
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_CITY_THINGS_DEV 1
#else
#define LYUD_CITY_THINGS_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudCity, Log, All);

/** interiors.json parsed on a worker: the model is built there and handed over once (bDone) to the game thread. */
struct FLyudCityLoad
{
	std::atomic<bool> bDone{ false };
	bool bFound = false;
	bool bOk = false;
	std::string Error;
	double Ms = 0.0;
	LyudCity::FModel Model;
};

bool ULyudCityThings::bOpenAllHours = false;

namespace LyudCityThingsImpl
{
	/** How far the hero reaches a thing (cm, from the eyes), how often the thing in view is re-picked (s), the margin
	 *  round the box of all things within which anything is picked (cm). */
	constexpr double Reach = 260.0;
	constexpr float PickEvery = 0.2f;
	constexpr double TownMargin = 3000.0;
	/** The page shuts when the hero has walked this far from where he opened it (cm). */
	constexpr float PageWalkAway = 250.f;
	/** The plot round the hero: the nearest thing within this (cm) on his floor. */
	constexpr double PlotRadius = 400.0;
	constexpr double PlotFloor = 250.0;
	/** How far from a seat a screen is watched, and how far a seat must be for "sit down first" (cm). */
	constexpr double ScreenRadius = 2500.0;
	/** Time passing in steps: how long it takes (real s), the most a frame passes (game s: the people's life steps 1 s at
	 *  a time), and the longest stretch that is stepped through (more: UPeopleLifeSubsystem resettles, cheap, so at once). */
	constexpr double JumpSeconds = 0.6;
	constexpr double JumpMaxPerFrame = 30.0;
	constexpr int32 JumpStepMaxMinutes = 15;
	/** A screen show: at most this many subtitles, each up for a time by its length. */
	constexpr int32 ShowMaxParts = 5;
	/** A light switch reaches the lamps up to this much over it (cm): the ceiling of its room, not the floor above. */
	constexpr float SwitchFloor = 400.f;

	FString Str(const std::string& S)
	{
		return FString(UTF8_TO_TCHAR(S.c_str()));
	}

	FName Name(const std::string& S)
	{
		return S.empty() ? FName() : FName(UTF8_TO_TCHAR(S.c_str()));
	}

	LyudCity::FVec Vec(const FVector& P)
	{
		LyudCity::FVec V;
		V.X = P.X;
		V.Y = P.Y;
		V.Z = P.Z;
		return V;
	}

	/** The game's date for the rules. bFull: also the holidays on now and the weather (an act about to run; it allocates);
	 *  without it only the clock (the prompt, every frame: nothing allocated). */
	LyudCity::FWhen When(const UWorld* World, bool bFull = true)
	{
		const FDateTime Now = UGameCalendar::NowIn(World);
		const UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
		const FDateTime Start = Cal ? Cal->StartDate : FDateTime(2026, 10, 20, 16, 0, 0);
		LyudCity::FWhen W;
		W.Day = (long long)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
		W.Hour = (float)Now.GetHour() + (float)Now.GetMinute() / 60.f;
		W.Weekday = (int)Now.GetDayOfWeek();     // Monday = 0
		W.MonthDay = Now.GetMonth() * 100 + Now.GetDay();
		W.Since = FMath::Max<long long>(0, W.Day - (long long)(Start.GetDate().GetTicks() / ETimespan::TicksPerDay));
		if (!bFull || !World)
		{
			return W;
		}
		if (const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(World))
		{
			W.bFestival = Fest->IsFestivalToday();
			for (const FLyudFestivalNow& F : Fest->ActiveFestivals())
			{
				W.Festivals.push_back(std::string(TCHAR_TO_UTF8(*F.Id.ToString())));
			}
		}
		if (const ULyudWeatherSubsystem* Wx = ULyudWeatherSubsystem::Get(World))
		{
			const FLyudSkyWeather Sky = Wx->GetSky();
			W.Weather = LyudCity::WeatherBits(Sky.Cloud, Sky.Rain, Sky.Snow, Sky.Fog, Sky.TempC, Sky.Wet);
		}
		return W;
	}

	/** A holder's key in the model (never 0). */
	uint64 HoldKey(const UObject* Who)
	{
		return Who ? (uint64)Who->GetUniqueID() + 1 : 0;
	}

	/** How far off the floor asked a townsperson's thing may be (cm). */
	constexpr double FreeFloor = 250.0;

	/** What the rules ask the game: the money, the pocket, who is on shift at a plot (town-life place ids are plot ids). */
	LyudCity::FAsk Ask(const UObject* Context)
	{
		LyudCity::FAsk A;
		ULyudInventory* Inv = ULyudInventory::Get(Context);
		A.Money = Inv ? Inv->Rubles : 0;
		A.bOpenAll = ULyudCityThings::bOpenAllHours;
		TWeakObjectPtr<ULyudInventory> WeakInv(Inv);
		A.ItemCount = [WeakInv](const std::string& Item) -> int
		{
			const ULyudInventory* I = WeakInv.Get();
			return I ? I->Count(Name(Item)) : 0;
		};
		TWeakObjectPtr<const UPeopleLifeSubsystem> WeakPeople(UPeopleLifeSubsystem::Get(Context));
		A.StaffOnShift = [WeakPeople](const std::string& Plot, const std::string& Role) -> bool
		{
			const UPeopleLifeSubsystem* People = WeakPeople.Get();
			if (!People)
			{
				return false;
			}
			return People->StaffOnShift(Name(Plot), Role == "*" ? FName() : Name(Role)).Num() > 0;
		};
		return A;
	}

	/** The floor under a seat or bed at At whose surface is HeightCm over it: chairs never block, so a trace from the cushion
	 *  down meets the floor; a bed or a bench with a blocking box is met on its top, and the floor is its height below. */
	double FloorUnder(UWorld* World, const AActor* Hero, const FVector& At, double HeightCm)
	{
		double FloorZ = At.Z - HeightCm;
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(CitySeatFloor), false, Hero);
		if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 30.0), At - FVector(0.0, 0.0, 300.0), ECC_Visibility, Q))
		{
			FloorZ = Hit.ImpactPoint.Z > At.Z - 0.5 * HeightCm ? Hit.ImpactPoint.Z - HeightCm : Hit.ImpactPoint.Z;
		}
		return FloorZ;
	}
}

ULyudCityThings::ULyudCityThings() = default;
ULyudCityThings::~ULyudCityThings() = default;

ULyudCityThings* ULyudCityThings::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCityThings>() : nullptr;
}

int32 ULyudCityThings::NumThings() const
{
	return Model ? (int32)Model->Things.size() : 0;
}

void ULyudCityThings::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Model = MakePimpl<LyudCity::FModel>();
	StartLoad();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("CityThings"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudCityThings::StartOver);
	}
}

void ULyudCityThings::Deinitialize()
{
	EndGame(false);
	if (GamePanel.IsValid() && bGamePanelAdded)
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(GamePanel.ToSharedRef());
		}
	}
	GamePanel.Reset();
	bGamePanelAdded = false;
	EndShow();
	EndLie(false);
	UnblockMove();
	SetPrompted(-1);
	ClosePage();
	// a parse still under way finishes on its own (it holds its own data) and is dropped
	Loading.Reset();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(GetWorld()))
	{
		Saves->UnregisterSaveObject(TEXT("CityThings"));
		Saves->OnNewGame.RemoveAll(this);
	}
	Super::Deinitialize();
}

void ULyudCityThings::StartLoad()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/interiors.json");
	TSharedPtr<FLyudCityLoad, ESPMode::ThreadSafe> Job = MakeShared<FLyudCityLoad, ESPMode::ThreadSafe>();
	Loading = Job;
	// the bytes go to the parser as they are (UTF-8): no FString round trip, and none of it on the game thread
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [Job, Path]()
	{
		const double T0 = FPlatformTime::Seconds();
		TArray<uint8> Bytes;
		Job->bFound = FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		if (Job->bFound)
		{
			size_t Skip = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF ? 3 : 0;
			const std::string Text((const char*)Bytes.GetData() + Skip, (size_t)Bytes.Num() - Skip);
			Job->bOk = Job->Model.Load(Text, Job->Error);
		}
		Job->Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
		Job->bDone.store(true, std::memory_order_release);
	});
}

void ULyudCityThings::AdoptLoaded()
{
	TSharedPtr<FLyudCityLoad, ESPMode::ThreadSafe> Job = MoveTemp(Loading);
	Loading.Reset();
	if (!Job.IsValid())
	{
		return;
	}
	if (!Job->bFound)
	{
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: no City/interiors.json: no things to do in the buildings"));
		return;
	}
	if (!Job->bOk)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: City/interiors.json unreadable: %s"), *LyudCityThingsImpl::Str(Job->Error));
		return;
	}
	*Model = std::move(Job->Model);
	// items the data names that the item table doesn't know (they are skipped when given or sold)
	int32 Unknown = 0;
	for (const LyudCity::FAct& A : Model->Acts)
	{
		for (const std::pair<std::string, int>& G : A.Give)
		{
			if (!ULyudInventory::FindDef(LyudCityThingsImpl::Name(G.first)))
			{
				UE_LOG(LogLyudCity, Warning, TEXT("CityThings: act %s gives an unknown item %s"), *LyudCityThingsImpl::Str(A.Id), *LyudCityThingsImpl::Str(G.first));
				++Unknown;
			}
		}
	}
	for (const std::pair<const std::string, std::vector<LyudCity::FGood>>& S : Model->Shops)
	{
		for (const LyudCity::FGood& G : S.second)
		{
			if (!ULyudInventory::FindDef(LyudCityThingsImpl::Name(G.Item)))
			{
				UE_LOG(LogLyudCity, Warning, TEXT("CityThings: shop %s sells an unknown item %s"), *LyudCityThingsImpl::Str(S.first), *LyudCityThingsImpl::Str(G.Item));
				++Unknown;
			}
		}
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %d things, %d acts, %d shops from City/interiors.json (%d unknown items, parsed in %.1f ms on a worker)"),
		(int32)Model->Things.size(), (int32)Model->Acts.size(), (int32)Model->Shops.size(), Unknown, Job->Ms);
	// a load restored the save before the file was parsed: its state is applied now
	ApplySaved();
}

void ULyudCityThings::ApplySaved()
{
	if (!Model || Saved.Num() == 0)
	{
		return;
	}
	std::map<std::string, std::string> In;
	for (const TPair<FString, FString>& KV : Saved)
	{
		In[std::string(TCHAR_TO_UTF8(*KV.Key))] = std::string(TCHAR_TO_UTF8(*KV.Value));
	}
	Model->LoadState(In);
}

void ULyudCityThings::StartOver()
{
	if (Model)
	{
		Model->ResetState();
	}
	Saved.Reset();
	GreetedPlot = NAME_None;
	PlotThing = -1;
	EndGame(false);
	if (ULyudWindowLights* Lights = ULyudWindowLights::Get(this))
	{
		Lights->ClearSwitched();
	}
	SwitchedPlot = NAME_None;
}

void ULyudCityThings::OnGameSaving_Implementation()
{
	if (Loading.IsValid() || !Model)
	{
		// still parsing: what was restored (or nothing) is what there is
		return;
	}
	Saved.Reset();
	std::map<std::string, std::string> Out;
	Model->SaveState(Out);
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(LyudCityThingsImpl::Str(KV.first), LyudCityThingsImpl::Str(KV.second));
	}
}

void ULyudCityThings::OnGameLoaded_Implementation()
{
	// while the file is still being parsed, Saved waits for AdoptLoaded
	if (!Loading.IsValid())
	{
		if (Model)
		{
			Model->ResetState();
		}
		ApplySaved();
	}
}

FVector ULyudCityThings::ThingLocation(int32 Thing) const
{
	if (!Model || Thing < 0 || Thing >= NumThings())
	{
		return FVector::ZeroVector;
	}
	const LyudCity::FVec& P = Model->Things[Thing].At;
	return FVector(P.X, P.Y, P.Z);
}

int32 ULyudCityThings::PickInView(const AHeroCharacter* Hero, const APlayerController* PC) const
{
	UWorld* World = GetWorld();
	if (!Model || !Hero || !PC || !World)
	{
		return -1;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	int32 Out[4];
	const int32 Num = Model->Pick(LyudCityThingsImpl::Vec(Eye), LyudCityThingsImpl::Vec(View.Vector()), LyudCityThingsImpl::Reach, Out, 4);
	if (Num <= 0)
	{
		return -1;
	}
	// a door in view takes E (ULyudDoorSubsystem polls it too): no thing then
	if (const ULyudDoorSubsystem* Doors = ULyudDoorSubsystem::Get(World); Doors && Doors->DoorInView())
	{
		return -1;
	}
	// the first of the picks with no wall between (the thing's own blocking box, if it has one, is near its point)
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CityThingSight), false, Hero);
	for (int32 I = 0; I < Num; ++I)
	{
		const FVector At = ThingLocation(Out[I]);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Eye, At, ECC_Visibility, Q) || Hit.Distance >= FVector::Dist(Eye, At) - 90.f)
		{
			return Out[I];
		}
	}
	return -1;
}

void ULyudCityThings::TrackPlot(const AHeroCharacter* Hero)
{
	if (!Model || !Hero)
	{
		return;
	}
	const int32 Near = Model->NearestThing(LyudCityThingsImpl::Vec(Hero->GetActorLocation()), LyudCityThingsImpl::PlotRadius, LyudCityThingsImpl::PlotFloor);
	if (Near < 0)
	{
		return;     // between buildings: the last plot stays (walking out and back in is not a new visit)
	}
	if (PlotThing >= 0 && PlotThing < NumThings() && Model->Things[PlotThing].Plot == Model->Things[Near].Plot)
	{
		return;
	}
	PlotThing = Near;
	GreetedPlot = NAME_None;
	const std::string& Plot = Model->Things[Near].Plot;
	if (!SwitchedPlot.IsNone() && SwitchedPlot != LyudCityThingsImpl::Name(Plot))
	{
		// the lamps he switched in the last building go back to their hours
		if (ULyudWindowLights* Lights = ULyudWindowLights::Get(this))
		{
			Lights->ClearSwitched();
		}
		SwitchedPlot = NAME_None;
	}
	if (!Plot.empty())
	{
		UE_LOG(LogLyudCity, Verbose, TEXT("CityThings: in %s"), *LyudCityThingsImpl::Str(Plot));
		ULyudQuests::Notify(this, TEXT("visit"), LyudCityThingsImpl::Str(Plot));
	}
}

void ULyudCityThings::SetPrompted(int32 Thing)
{
	// the act offered: the thing's own, or the words of its closed act while its plot is shut (only the clock is asked)
	const int32 Act = Model && Thing >= 0 ? Model->ActIndexNow(Thing, LyudCityThingsImpl::When(GetWorld(), false), bOpenAllHours) : -1;
	if (Thing == Prompted && Act == PromptedAct)
	{
		return;
	}
	Prompted = Thing;
	PromptedAct = Act;
	FText Line;
	if (const LyudCity::FAct* A = Model && Act >= 0 ? &Model->Acts[(size_t)Act] : nullptr)
	{
		const FText Base = LyudText::Get(LyudCityThingsImpl::Str(A->Prompt));
		Line = A->Price > 0 ? LyudText::Get(TEXT("city.act.prompt_price"), { FStringFormatArg(Base.ToString()), FStringFormatArg(A->Price) }) : Base;
	}
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, Line);
	}
}

void ULyudCityThings::Refuse(const FText& Why)
{
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: refused: %s"), *Why.ToString());
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!World || !UI)
	{
		return;
	}
	Prompted = -2;     // the next SetPrompted writes the line again
	RefusedUntil = World->GetTimeSeconds() + 2.5;
	UI->SetPrompt(this, Why);
}

void ULyudCityThings::ShowPage(const FText& Title, const FText& Body, bool bMore)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(false);
	Page->SetPage(Title, Body, LyudText::Get(bMore ? TEXT("city.page.next") : TEXT("city.page.close")));
}

void ULyudCityThings::ClosePage()
{
	PageThing = -1;
	PageLeft = 0;
	if (!Page)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page.Reset();
}

void ULyudCityThings::JumpMinutes(int32 Minutes, bool bBlockMove)
{
	UWorld* World = GetWorld();
	UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
	if (!Cal || Minutes <= 0)
	{
		return;
	}
	if (Minutes > LyudCityThingsImpl::JumpStepMaxMinutes)
	{
		// more than 15 minutes: the town's life resettles instead of stepping through it (UPeopleLifeSubsystem), so at once
		Cal->SetNow(Cal->Now() + FTimespan::FromMinutes((double)Minutes));
		return;
	}
	// a few minutes: over the next frames, so the people's life (which steps 1 s at a time) never runs 900 steps in one
	JumpLeft += 60.0 * Minutes;
	JumpRate = FMath::Max(JumpRate, JumpLeft / LyudCityThingsImpl::JumpSeconds);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (bBlockMove && PC && !MoveBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		MoveBlockedBy = PC;
	}
}

void ULyudCityThings::TickJump(float DeltaTime)
{
	if (JumpLeft <= 0.0)
	{
		return;
	}
	UWorld* World = GetWorld();
	UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
	if (!Cal)
	{
		JumpLeft = 0.0;
		UnblockMove();
		return;
	}
	const double Add = FMath::Min(JumpLeft, FMath::Clamp(JumpRate * (double)DeltaTime, 1.0, LyudCityThingsImpl::JumpMaxPerFrame));
	Cal->SetNow(Cal->Now() + FTimespan::FromSeconds(Add));
	JumpLeft -= Add;
	if (JumpLeft <= 0.0)
	{
		JumpLeft = 0.0;
		JumpRate = 0.0;
		UnblockMove();
	}
}

void ULyudCityThings::UnblockMove()
{
	if (APlayerController* PC = MoveBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	MoveBlockedBy.Reset();
}

bool ULyudCityThings::Sit(int32 Thing, AHeroCharacter* Hero, bool bLie)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !Model || Thing < 0 || Thing >= NumThings() || Hero->IsSeated())
	{
		return false;
	}
	// the anchor may have gone with a level's actors (or be pending kill): one is made again
	if (!IsValid(SeatAnchor) || !IsValid(SeatRoot))
	{
		// one anchor at the world origin for every seat in town: the seat's frame is given to SitOn in world terms
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("LyudCitySeat"));
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Anchor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		if (!Anchor)
		{
			return false;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Anchor, TEXT("Seat"));
		Root->SetMobility(EComponentMobility::Static);
		Anchor->SetRootComponent(Root);
		Root->RegisterComponent();
		SeatAnchor = Anchor;
		SeatRoot = Root;
	}
	const LyudCity::FThing& T = Model->Things[Thing];
	const FVector At = ThingLocation(Thing);
	// metres over its own floor; a height measured from the building's base (some exports) would seat him metres under it
	const float HeightM = LyudCity::FModel::SeatHeight(T, bLie);
	if (HeightM != T.PropF("height", HeightM))
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: %s has height %s m: %.2f used"), *LyudCityThingsImpl::Str(T.Id), *LyudCityThingsImpl::Str(T.Prop("height")), HeightM);
	}
	const double Height = (double)HeightM * 100.0;
	const double FloorZ = LyudCityThingsImpl::FloorUnder(World, Hero, At, Height);
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	// the seat's frame: the floor under the cushion's front edge, X the way the sitter faces (the way the thing faces)
	const FRotator Facing(0.0, (double)T.Yaw, 0.0);
	const FVector Front = FVector(At.X, At.Y, FloorZ) + Facing.Vector() * (bLie ? 40.0 : 20.0);
	Hero->SitOn(SeatRoot, FTransform(Facing, Front), (float)Height);
	Hero->SetStandUpOffset(FVector(45.f, 0.f, 0.f));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-8.0, (double)T.Yaw, 0.0));
	}
	SeatedOn = Thing;
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %s on %s (floor z %.0f, seat %.0f cm): seated=%d"), bLie ? TEXT("lies") : TEXT("sits"),
		*LyudCityThingsImpl::Str(T.Id), FloorZ, Height, Hero->IsSeated() ? 1 : 0);
	SeatScreen = bLie ? -1 : Model->ScreenFrom(Thing, LyudCityThingsImpl::ScreenRadius);
	if (bLie)
	{
		// the pillow's eye: over the middle of the bed, a hand over its surface
		LieEye = FVector(At.X, At.Y, FloorZ + Height + 22.0);
		LieYaw = T.Yaw;
	}
	return true;
}

// --- lying down: the hero sits on the bed's edge (the body holds there), the view goes to a camera at the pillow looking at
// the ceiling (the hero hidden: his body would be in the picture); any step, Space or E gets him up

void ULyudCityThings::BeginLie(int32 Thing, bool bFade, bool bTimePasses)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	LyingOn = Thing;
	bLieAutoStand = bTimePasses;
	LieUntil = 0.0;
	PageFrame = GFrameCounter;
	if (!bFade)
	{
		ShowLieView(false);
		if (bTimePasses)
		{
			LieUntil = World->GetTimeSeconds() + LyudCityThingsImpl::JumpSeconds + 2.5;
		}
	}
	// with a fade the view changes once the screen is black (Tick)
}

void ULyudCityThings::ShowLieView(bool bCut)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !Hero || PC->GetViewTarget() != Hero)
	{
		return;     // somebody else has the view (a bird, a cutscene): leave it
	}
	if (!IsValid(LieCamera))
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.Name = MakeUniqueObjectName(World, ACameraActor::StaticClass(), TEXT("LyudCityPillow"));
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LieCamera = World->SpawnActor<ACameraActor>(LieEye, FRotator::ZeroRotator, P);
		if (!LieCamera)
		{
			return;
		}
		LieCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
		LieCamera->GetCameraComponent()->SetFieldOfView(80.f);
	}
	LieCamera->SetActorLocationAndRotation(LieEye, FRotator(85.0, (double)LieYaw, 0.0));
	PC->SetViewTargetWithBlend(LieCamera.Get(), bCut ? 0.f : 0.6f);
	if (!Hero->IsHidden())
	{
		Hero->SetActorHiddenInGame(true);
		bHeroHiddenForLie = true;
	}
}

void ULyudCityThings::EndLie(bool bStandUp)
{
	if (LyingOn < 0 && !bHeroHiddenForLie)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (bHeroHiddenForLie)
	{
		if (Hero)
		{
			Hero->SetActorHiddenInGame(false);
		}
		bHeroHiddenForLie = false;
	}
	if (PC && IsValid(LieCamera) && PC->GetViewTarget() == LieCamera.Get())
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), bStandUp ? 0.5f : 0.f);
	}
	if (bStandUp && Hero && Hero->IsSeated() && SeatedOn == LyingOn)
	{
		Hero->StandUp();
	}
	LyingOn = -1;
	LieUntil = 0.0;
}

void ULyudCityThings::TickLie(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !PC || !Hero->IsSeated() || SeatedOn != LyingOn)
	{
		EndLie(false);     // he got up (a step, Space) or is not the hero any more
		return;
	}
	const bool bSettled = FadeMinutes == 0 && !bFadingIn && JumpLeft <= 0.0;
	if (bSettled && !Page && !bTalk && GFrameCounter != PageFrame && PC->WasInputKeyJustPressed(EKeys::E))
	{
		EndLie(true);
	}
	else if (bSettled && LieUntil > 0.0 && World->GetTimeSeconds() >= LieUntil)
	{
		EndLie(true);
	}
}

// --- a screen show: the act's line cut into subtitles, one after another, the game minutes passing with them

void ULyudCityThings::StartShow(int32 Thing, const FText& Title, const FText& Line, int32 Minutes, bool bSeated)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ShowParts.Reset();
	for (const std::string& Part : LyudCity::Subtitles(std::string(TCHAR_TO_UTF8(*Line.ToString())), LyudCityThingsImpl::ShowMaxParts))
	{
		ShowParts.Add(FText::FromString(LyudCityThingsImpl::Str(Part)));
	}
	if (ShowParts.Num() == 0)
	{
		ShowParts.Add(Line);
	}
	ShowThing = Thing;
	ShowTitle = Title;
	ShowMinutes = FMath::Max(0, Minutes);
	bShowSeated = bSeated;
	ShowAt = -1;
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	PageFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
	PageFrame = GFrameCounter;
	SetPrompted(-1);
	NextShowPart();
}

void ULyudCityThings::NextShowPart()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	++ShowAt;
	if (!World || !Viewport || ShowAt >= ShowParts.Num())
	{
		EndShow();
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	Page->SetScreen(true);
	Page->SetPage(ShowTitle, ShowParts[ShowAt], LyudText::Get(TEXT("city.page.stop_watching")));
	// each subtitle stays by its length; the game minutes pass in equal shares as they come
	const int32 Len = ShowParts[ShowAt].ToString().Len();
	ShowNext = World->GetTimeSeconds() + FMath::Clamp(1.6 + 0.055 * (double)Len, 2.8, 7.0);
	const int32 N = ShowParts.Num();
	const int32 Share = ShowMinutes * (ShowAt + 1) / N - ShowMinutes * ShowAt / N;
	JumpMinutes(Share, false);
}

void ULyudCityThings::EndShow()
{
	if (ShowAt < 0 && ShowParts.Num() == 0)
	{
		return;
	}
	ShowAt = -1;
	ShowThing = -1;
	ShowParts.Reset();
	if (Page)
	{
		Page->SetScreen(false);
	}
	ClosePage();
}

void ULyudCityThings::TickShow(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	// over when he gets up (a seated show), walks off (a standing one), stops being the hero, or a screen comes up
	if (!World || !Hero || !PC || !UI || UI->IsOpen() || (bShowSeated && !Hero->IsSeated())
		|| (!bShowSeated && FVector::Dist(Hero->GetActorLocation(), PageFrom) > LyudCityThingsImpl::PageWalkAway))
	{
		EndShow();
		return;
	}
	if (!bTalk && GFrameCounter != PageFrame && PC->WasInputKeyJustPressed(EKeys::E))
	{
		EndShow();
		return;
	}
	if (World->GetTimeSeconds() >= ShowNext)
	{
		NextShowPart();
	}
}

// --- the staff say hello once a visit when the hero steps up to their counter

void ULyudCityThings::Greet(int32 Thing)
{
	const LyudCity::FAct* A = Model ? Model->ActOf(Thing) : nullptr;
	if (!A || A->NeedsStaff.empty())
	{
		return;
	}
	const LyudCity::FThing& T = Model->Things[Thing];
	const FName Plot = LyudCityThingsImpl::Name(T.Plot);
	if (Plot.IsNone() || Plot == GreetedPlot)
	{
		return;
	}
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	if (!People || !Talk)
	{
		return;
	}
	const FName Role = A->NeedsStaff == "*" ? FName() : LyudCityThingsImpl::Name(A->NeedsStaff);
	const FVector At = ThingLocation(Thing);
	for (const int32 Person : People->StaffOnShift(Plot, Role))
	{
		FPersonInfo Info;
		AActor* Body = People->GetPerson(Person, Info) ? Info.Actor.Get() : nullptr;
		// only somebody drawn near the counter (not one still walking to work across town)
		if (IsValid(Body) && !Body->IsHidden() && FVector::Dist(Body->GetActorLocation(), At) < 600.0)
		{
			GreetedPlot = Plot;
			Talk->SayEvent(Body, TEXT("counter_hello"), Person, Role);
			return;
		}
	}
}

void ULyudCityThings::Run(int32 Thing, bool bFromConsole, bool bFromSeat)
{
	UWorld* World = GetWorld();
	if (!World || !Model || Thing < 0 || Thing >= NumThings())
	{
		return;
	}
	const LyudCity::FWhen Now = LyudCityThingsImpl::When(World);
	const LyudCity::FThing& T = Model->Things[Thing];
	const bool bShutWords = Model->ActIndexNow(Thing, Now, bOpenAllHours) != T.Act;
	// a film, a play or the TV from a sofa: sit down first (not from the console, which runs things from anywhere)
	if (!bFromConsole && !bFromSeat && !bShutWords && Model->NeedsSeatFirst(Thing, LyudCityThingsImpl::ScreenRadius))
	{
		Refuse(LyudText::Get(TEXT("city.act.sit_first")));
		return;
	}
	const LyudCity::FOutcome O = Model->Run(Thing, Now, LyudCityThingsImpl::Ask(this));
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: do %s (%s, act %s) -> %s line=%s minutes=%d price=%d shop=%s event=%s/%s%s"),
		*LyudCityThingsImpl::Str(T.Id), *LyudCityThingsImpl::Str(T.Kind), O.Act >= 0 ? *LyudCityThingsImpl::Str(Model->Acts[O.Act].Id) : TEXT("-"), UTF8_TO_TCHAR(LyudCity::VerdictName(O.Verdict)),
		*LyudCityThingsImpl::Str(O.Line), O.Minutes, O.Price, *LyudCityThingsImpl::Str(O.Shop), *LyudCityThingsImpl::Str(O.Event), *LyudCityThingsImpl::Str(O.Target),
		O.bFirstVisitToday ? TEXT(" visit") : (O.bClosed ? TEXT(" closed") : TEXT("")));
	if (O.Verdict != LyudCity::EVerdict::Ok)
	{
		switch (O.Verdict)
		{
		case LyudCity::EVerdict::NoMoney:
			Refuse(LyudText::Get(TEXT("city.act.no_money"), { FStringFormatArg(O.NeedMoney) }));
			break;
		case LyudCity::EVerdict::NeedsItem:
			Refuse(LyudText::Get(TEXT("city.act.needs_item"), { FStringFormatArg(ULyudInventory::ItemName(LyudCityThingsImpl::Name(O.NeedItem)).ToString()) }));
			break;
		default:
			Refuse(LyudText::Get(LyudCityThingsImpl::Str(LyudCity::RefusalKey(O.Verdict))));
			break;
		}
		return;
	}
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (O.Price > 0 && (!Inv || !Inv->Spend(O.Price)))
	{
		Refuse(LyudText::Get(TEXT("city.act.no_money"), { FStringFormatArg(O.Price) }));
		return;
	}
	for (const std::pair<std::string, int>& G : O.Give)
	{
		const FName Item = LyudCityThingsImpl::Name(G.first);
		if (Inv && ULyudInventory::FindDef(Item))
		{
			Inv->Add(Item, G.second);
		}
	}
	const FVector At = ThingLocation(Thing);
	if (!O.Sound.empty())
	{
		// an id the soundscape has no pool for plays nothing
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->PlayEffect(LyudCityThingsImpl::Name(O.Sound), At);
		}
	}
	if (!O.NewStamps.empty())
	{
		TArray<FString> Fresh;
		for (const std::string& Stamp : O.NewStamps)
		{
			Fresh.Add(LyudCityThingsImpl::Str(Stamp));
		}
		AnnounceStamps(Fresh);
	}
	ULyudQuests::Notify(this, LyudCityThingsImpl::Name(O.Event), LyudCityThingsImpl::Str(O.Target));
	// "visit" at every use (an objective waiting for it must not wait for tomorrow because the plot was used this morning);
	// not for the words of a shut plot's thing
	if (!O.Plot.empty() && !O.bClosed)
	{
		ULyudQuests::Notify(this, TEXT("visit"), LyudCityThingsImpl::Str(O.Plot));
	}
	if (!bFromConsole)
	{
		Greet(Thing);
	}
	if (!O.bClosed && LyudCity::FModel::SwitchRadiusCm(T) > 0.0)
	{
		FlipSwitch(Thing);
	}

	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (O.Game >= 0)
	{
		// a mini-game instead of the page: seated first when the act sits (a chess table) and the hero is at it
		if (Hero && O.Pose == "sit" && !Hero->IsSeated() && FVector::Dist(Hero->GetActorLocation(), At) < 400.f)
		{
			Sit(Thing, Hero, false);
		}
		BeginGame(Thing, O.Minutes, Hero && Hero->IsSeated() && SeatedOn == Thing);
		return;
	}
	if (!O.Shop.empty())
	{
		const std::vector<LyudCity::FGood>* Stock = Model->FindShop(O.Shop);
		TArray<FLyudShopGood> Goods;
		if (Stock)
		{
			for (const LyudCity::FGood& G : *Stock)
			{
				FLyudShopGood Good;
				Good.Item = LyudCityThingsImpl::Name(G.Item);
				Good.Price = G.Price;
				Good.Count = G.Count;
				if (ULyudInventory::FindDef(Good.Item))
				{
					Goods.Add(Good);
				}
			}
		}
		if (UI && Goods.Num() > 0)
		{
			SetPrompted(-1);
			ClosePage();
			UI->OpenShop(LyudText::Get(O.Title.empty() ? FString(TEXT("city.shop.title")) : LyudCityThingsImpl::Str(O.Title)), Goods);
		}
		else
		{
			UE_LOG(LogLyudCity, Warning, TEXT("CityThings: shop %s has nothing to sell"), *LyudCityThingsImpl::Str(O.Shop));
		}
		return;
	}
	const FText Title = O.Title.empty() ? FText::GetEmpty() : LyudText::Get(LyudCityThingsImpl::Str(O.Title));
	// sitting and lying only when the hero is at the thing (a console run from across town doesn't carry him there)
	const bool bNear = Hero && FVector::Dist(Hero->GetActorLocation(), At) < 400.f;
	if (!O.Screen.empty())
	{
		// a show: seated on the thing itself when it is a seat (a cinema's hall seat), else where he is (a sofa, standing)
		bool bSeated = bFromSeat && Hero && Hero->IsSeated();
		if (!bSeated && bNear && O.Pose == "sit" && LyudCity::FModel::IsSeatThing(T))
		{
			bSeated = Sit(Thing, Hero, false);
		}
		ClosePage();
		StartShow(Thing, Title, O.Line.empty() ? FText::GetEmpty() : LineText(LyudCityThingsImpl::Str(O.Line)), O.Minutes, bSeated);
		return;
	}
	const bool bLie = bNear && O.Pose == "lie" && Sit(Thing, Hero, true);
	if (bNear && O.Pose == "sit")
	{
		Sit(Thing, Hero, false);
	}
	const bool bFade = O.Minutes > 0 && O.bFade && PC && PC->PlayerCameraManager;
	if (bLie)
	{
		BeginLie(Thing, bFade, O.Minutes > 0);
	}
	if (O.Minutes > 0)
	{
		if (bFade)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, false, true);
			FadeMinutes = O.Minutes;
			FadeAt = World->GetTimeSeconds() + 0.7;
			bFadingIn = false;
		}
		else
		{
			JumpMinutes(O.Minutes, !bLie);
		}
	}
	if (!O.Line.empty())
	{
		PageThing = Thing;
		PageLeft = FMath::Max(0, O.NumLines - 1);
		PageTitle = Title;
		PageFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
		PageFrame = GFrameCounter;
		ShowPage(PageTitle, LineText(LyudCityThingsImpl::Str(O.Line)), PageLeft > 0);
		SetPrompted(-1);
	}
}

bool ULyudCityThings::RunThingById(const FString& Id)
{
	const int32 Thing = Model ? Model->FindThing(std::string(TCHAR_TO_UTF8(*Id))) : -1;
	if (Thing < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no thing %s%s"), *Id, Loading.IsValid() ? TEXT(" (City/interiors.json is still being read)") : TEXT(""));
		return false;
	}
	Run(Thing, true);
	return true;
}

void ULyudCityThings::LogNearest(int32 N) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %d things, %d acts, %d shops"), NumThings(), Model ? (int32)Model->Acts.size() : 0,
		Model ? (int32)Model->Shops.size() : 0);
	if (!Model || !Pawn)
	{
		return;
	}
	for (const std::pair<int, double>& P : Model->Nearest(LyudCityThingsImpl::Vec(Pawn->GetActorLocation()), N))
	{
		const LyudCity::FThing& T = Model->Things[P.first];
		const LyudCity::FAct* A = Model->ActOf(P.first);
		UE_LOG(LogLyudCity, Display, TEXT("  %s (%s, act %s) %.1f m"), *LyudCityThingsImpl::Str(T.Id), *LyudCityThingsImpl::Str(T.Kind),
			A ? *LyudCityThingsImpl::Str(A->Id) : TEXT("-"), P.second / 100.0);
	}
}

bool ULyudCityThings::RunNearest()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return false;
	}
	const std::vector<std::pair<int, double>> Near = Model->Nearest(LyudCityThingsImpl::Vec(Pawn->GetActorLocation()), 1);
	if (Near.empty())
	{
		return false;
	}
	Run(Near[0].first, true);
	return true;
}

void ULyudCityThings::ResetDailyLimits()
{
	if (Model)
	{
		Model->ResetToday();
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: today's limits reset"));
	}
}

void ULyudCityThings::DrawLabels() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!World || !Pawn || !Model)
	{
		return;
	}
	const FVector From = Pawn->GetActorLocation();
	for (int32 I = 0; I < NumThings(); ++I)
	{
		const FVector At = ThingLocation(I);
		if (FVector::DistSquared(At, From) < FMath::Square(1500.0))
		{
			const LyudCity::FAct* A = Model->ActOf(I);
			const FString Label = FString::Printf(TEXT("%s [%s]"), *LyudCityThingsImpl::Str(Model->Things[I].Id), A ? *LyudCityThingsImpl::Str(A->Id) : TEXT("-"));
			DrawDebugString(World, At, Label, nullptr, I == Prompted ? FColor::Yellow : FColor::White, 0.55f, true, 1.f);
		}
	}
#endif
}

void ULyudCityThings::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !Model)
	{
		return;
	}
	if (Loading.IsValid())
	{
		if (!Loading->bDone.load(std::memory_order_acquire))
		{
			return;
		}
		AdoptLoaded();
	}
	if (Model->Things.empty())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// a fade round a jump of the clock: black, (the pillow's view), the jump, hold a moment, back
	if (FadeMinutes > 0 && Now >= FadeAt)
	{
		if (LyingOn >= 0)
		{
			ShowLieView(true);
		}
		JumpMinutes(FadeMinutes, LyingOn < 0);
		FadeMinutes = 0;
		bFadingIn = true;
		FadeAt = Now + 0.4;
	}
	else if (bFadingIn && JumpLeft <= 0.0 && Now >= FadeAt)
	{
		bFadingIn = false;
		APlayerController* FadePC = UGameplayStatics::GetPlayerController(World, 0);
		if (FadePC && FadePC->PlayerCameraManager)
		{
			FadePC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.8f, FLinearColor::Black, false, false);
		}
		if (LyingOn >= 0 && bLieAutoStand)
		{
			LieUntil = Now + 2.5;     // a moment of the ceiling, then up
		}
	}
	TickJump(DeltaTime);
	if (bShowLabels)
	{
		LabelClock -= DeltaTime;
		if (LabelClock <= 0.f)
		{
			LabelClock = 0.5f;
			DrawLabels();
		}
	}

	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (SeatedOn >= 0 && (!Hero || !Hero->IsSeated()))
	{
		SeatedOn = -1;
		SeatScreen = -1;
	}
	// the thing in view, the plot round the hero and the talk, a few times a second; nothing at all outside the town's box
	PickClock -= DeltaTime;
	if (PickClock <= 0.f)
	{
		PickClock = LyudCityThingsImpl::PickEvery;
		DropDeadHolds();
		bOutside = !Hero || !Model->InBounds(LyudCityThingsImpl::Vec(Hero->GetActorLocation()), LyudCityThingsImpl::TownMargin);
		Picked = bOutside ? -1 : PickInView(Hero, PC);
		if (!bOutside)
		{
			TrackPlot(Hero);
			if (!TalkDirector.IsValid())
			{
				TActorIterator<ANpcTalkDirector> It(World);
				TalkDirector = It ? *It : nullptr;
			}
		}
	}
	// E is the talk's while he faces somebody or is talking (one press must not both talk and buy)
	const bool bTalk = TalkDirector.IsValid() && TalkDirector->IsEngaged();
	if (LyingOn >= 0)
	{
		TickLie(Hero, PC, bTalk);
	}
	if (IsPlayingGame())
	{
		SetPrompted(-1);
		TickGame(Hero, PC, bTalk, DeltaTime);
		return;
	}
	if (ShowAt >= 0)
	{
		SetPrompted(-1);
		TickShow(Hero, PC, bTalk);
		return;
	}
	if (bOutside && !Page)
	{
		SetPrompted(-1);
		return;
	}
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	// a page shuts when the hero walks off, stops being the hero or a screen comes up
	if (Page && (!Hero || !UI || UI->IsOpen() || FVector::Dist(Hero->GetActorLocation(), PageFrom) > LyudCityThingsImpl::PageWalkAway))
	{
		ClosePage();
	}
	// a page up: E turns it (seated too: a bed's or a bench's page), and nothing else is offered meanwhile
	if (Page)
	{
		SetPrompted(-1);
		if (PC && !bTalk && FadeMinutes == 0 && !bFadingIn && PC->WasInputKeyJustPressed(EKeys::E) && GFrameCounter != PageFrame)
		{
			if (PageLeft > 0 && PageThing >= 0)
			{
				--PageLeft;
				std::vector<std::string> NewStamps;
				const std::string Next = Model->NextLine(PageThing, LyudCityThingsImpl::When(World), &NewStamps, bOpenAllHours);
				if (!NewStamps.empty())
				{
					TArray<FString> Fresh;
					for (const std::string& Stamp : NewStamps)
					{
						Fresh.Add(LyudCityThingsImpl::Str(Stamp));
					}
					AnnounceStamps(Fresh);
				}
				if (Next.empty())
				{
					ClosePage();
				}
				else
				{
					ShowPage(PageTitle, LineText(LyudCityThingsImpl::Str(Next)), PageLeft > 0);
					if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
					{
						Sound->PlayEffect(TEXT("page_turn"), ThingLocation(PageThing));
					}
				}
			}
			else
			{
				ClosePage();
			}
		}
		return;
	}
	// nothing else on screen, not talking, the clock not running on, not on a bike
	const bool bCalm = PC && UI && UI->CanOpen(true) && !bTalk && !Hero->IsOnBike() && !Hero->IsLeadingBike()
		&& FadeMinutes == 0 && !bFadingIn && JumpLeft <= 0.0 && LyingOn < 0;
	// seated on a seat of ours with a screen in front (a sofa and the TV): the screen is offered from there
	const int32 Offer = bCalm && Hero->IsSeated() ? (SeatedOn >= 0 ? SeatScreen : -1) : (bCalm ? Picked : -1);
	if (Offer < 0)
	{
		SetPrompted(-1);
		return;
	}
	if (Now < RefusedUntil)
	{
		return;
	}
	SetPrompted(Offer);
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		Run(Offer, false, Hero->IsSeated());
	}
}

// --- collections

void ULyudCityThings::AnnounceStamps(const TArray<FString>& Stamps)
{
	if (!Model)
	{
		return;
	}
	ULyudGameScreen* Screen = ULyudGameScreen::Get(this);
	for (const FString& Stamp : Stamps)
	{
		std::string Set, Piece;
		const LyudCity::FCollection* C = LyudCity::SplitStamp(std::string(TCHAR_TO_UTF8(*Stamp)), Set, Piece) ? Model->FindCollection(Set) : nullptr;
		if (!C)
		{
			continue;
		}
		const int32 Got = Model->CountStamps(Set);
		const int32 All = (int32)C->Pieces.size();
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: stamp %s (%d of %d)"), *Stamp, Got, All);
		if (Screen)
		{
			const FString Title = LyudText::Get(LyudCityThingsImpl::Str(C->Title)).ToString();
			Screen->Toast(LyudText::Get(Got >= All ? TEXT("city.coll.complete") : TEXT("city.coll.got"),
				{ FStringFormatArg(Title), FStringFormatArg(Got), FStringFormatArg(All) }));
		}
	}
}

void ULyudCityThings::ShowCollections(bool bPage)
{
	if (!Model)
	{
		return;
	}
	FString Body;
	for (const LyudCity::FCollection& C : Model->Collections)
	{
		const int32 Got = Model->CountStamps(C.Id);
		const int32 All = (int32)C.Pieces.size();
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: collection %s: %d of %d"), *LyudCityThingsImpl::Str(C.Id), Got, All);
		if (!Body.IsEmpty())
		{
			Body += TEXT("\n");
		}
		Body += LyudText::Get(TEXT("city.coll.line"), { FStringFormatArg(LyudText::Get(LyudCityThingsImpl::Str(C.Title)).ToString()),
			FStringFormatArg(Got), FStringFormatArg(All) }).ToString();
	}
	UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	// the page needs the tick (it closes on E or walking off), and the tick runs only while there are things
	if (!bPage || !Pawn || NumThings() == 0 || ShowAt >= 0)
	{
		return;
	}
	if (Body.IsEmpty())
	{
		Body = LyudText::Get(TEXT("city.coll.none")).ToString();
	}
	PageThing = -1;
	PageLeft = 0;
	PageTitle = LyudText::Get(TEXT("city.coll.page.title"));
	PageFrom = Pawn->GetActorLocation();
	PageFrame = GFrameCounter;
	ShowPage(PageTitle, FText::FromString(Body), false);
	SetPrompted(-1);
}

bool ULyudCityThings::EditCollections(const FString& Verb, const FString& Set)
{
	if (!Model)
	{
		return false;
	}
	const std::string Which(TCHAR_TO_UTF8(*Set));
	if (Verb == TEXT("grant"))
	{
		return Model->GrantSet(Which);
	}
	if (Verb == TEXT("clear") && (Which == "*" || Model->FindCollection(Which)))
	{
		Model->ClearSet(Which);
		return true;
	}
	return false;
}

bool ULyudCityThings::AreCollectionsComplete() const
{
	if (!Model || Model->Collections.empty())
	{
		return false;
	}
	for (const LyudCity::FCollection& C : Model->Collections)
	{
		if (Model->CountStamps(C.Id) < (int)C.Pieces.size())
		{
			return false;
		}
	}
	return true;
}

// --- API for townsfolk

void ULyudCityThings::FillRef(int32 Thing, FCityThingRef& Out) const
{
	const LyudCity::FThing& T = Model->Things[(size_t)Thing];
	Out.Id = LyudCityThingsImpl::Str(T.Id);
	Out.Kind = LyudCityThingsImpl::Name(T.Kind);
	Out.Plot = LyudCityThingsImpl::Str(T.Plot);
	Out.Location = ThingLocation(Thing);
	Out.Yaw = T.Yaw;
	const bool bSeat = T.Kind == "seat" || T.Kind == "bed" || T.Props.count("height") > 0;
	Out.SeatHeightCm = bSeat ? LyudCity::FModel::SeatHeight(T, T.Kind == "bed") * 100.f : 0.f;
	Out.bHeld = Model->Holder(Thing) != 0 || (SeatedOn == Thing && SeatedOn >= 0);
}

bool ULyudCityThings::FindFreeThing(FName Kind, const FVector& Near, float RadiusCm, FCityThingRef& Out) const
{
	if (!Model || Model->Things.empty())
	{
		return false;
	}
	const std::string KindText = Kind.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Kind.ToString()));
	// the seat or bed the hero is on is not free (he holds no claim in the model)
	const int32 Found = Model->FindFree(KindText, LyudCityThingsImpl::Vec(Near), (double)RadiusCm, LyudCityThingsImpl::FreeFloor, SeatedOn);
	if (Found < 0)
	{
		return false;
	}
	FillRef(Found, Out);
	return true;
}

bool ULyudCityThings::ClaimThing(const FString& Id, const UObject* Who)
{
	const int32 Thing = Model && IsValid(Who) ? Model->FindThing(std::string(TCHAR_TO_UTF8(*Id))) : -1;
	if (Thing < 0 || Thing == SeatedOn || Thing == LyingOn)
	{
		return false;
	}
	const uint64 Key = LyudCityThingsImpl::HoldKey(Who);
	if (!Model->Claim(Thing, Key))
	{
		return false;
	}
	FHold& Hold = Holds.FindOrAdd(Thing);
	Hold.Who = Who;
	Hold.Key = Key;
	return true;
}

void ULyudCityThings::ReleaseThing(const FString& Id, const UObject* Who)
{
	const int32 Thing = Model && Who ? Model->FindThing(std::string(TCHAR_TO_UTF8(*Id))) : -1;
	if (Thing < 0)
	{
		return;
	}
	const uint64 Key = LyudCityThingsImpl::HoldKey(Who);
	if (Model->Holder(Thing) == Key)
	{
		Model->Release(Thing, Key);
		Holds.Remove(Thing);
	}
}

void ULyudCityThings::GetThingsOfPlot(const FString& PlotId, FName Kind, TArray<FCityThingRef>& Out) const
{
	const std::vector<int>* Of = Model ? Model->ThingsOfPlot(std::string(TCHAR_TO_UTF8(*PlotId))) : nullptr;
	if (!Of)
	{
		return;
	}
	const std::string KindText = Kind.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Kind.ToString()));
	for (const int I : *Of)
	{
		if (KindText.empty() || Model->Things[(size_t)I].Kind == KindText)
		{
			FillRef(I, Out.AddDefaulted_GetRef());
		}
	}
}

void ULyudCityThings::DropDeadHolds()
{
	for (TMap<int32, FHold>::TIterator It = Holds.CreateIterator(); It; ++It)
	{
		if (!It->Value.Who.IsValid())
		{
			if (Model)
			{
				Model->Release(It->Key, It->Value.Key);
			}
			It.RemoveCurrent();
		}
	}
}

// --- the words of a line, light switches

FText ULyudCityThings::LineText(const FString& Key) const
{
	const FText Text = LyudText::Get(Key);
	const FString& Words = Text.ToString();
	if (!Words.Contains(TEXT("{time}")))
	{
		return Text;
	}
	// a wall clock: the game's time, now
	const LyudCity::FWhen Now = LyudCityThingsImpl::When(GetWorld(), false);
	return FText::FromString(LyudCityThingsImpl::Str(LyudCity::FillTime(std::string(TCHAR_TO_UTF8(*Words)), Now.Hour)));
}

void ULyudCityThings::Note(const FText& What)
{
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: note: %s"), *What.ToString());
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!World || !UI)
	{
		return;
	}
	Prompted = -2;     // the next SetPrompted writes the line again
	RefusedUntil = World->GetTimeSeconds() + 2.0;
	UI->SetPrompt(this, What);
}

void ULyudCityThings::FlipSwitch(int32 Thing)
{
	ULyudWindowLights* Lights = ULyudWindowLights::Get(this);
	if (!Model || !Lights || Thing < 0 || Thing >= NumThings())
	{
		return;
	}
	const LyudCity::FThing& T = Model->Things[(size_t)Thing];
	const float Radius = (float)LyudCity::FModel::SwitchRadiusCm(T);
	const FVector At = ThingLocation(Thing);
	int32 Lamps = 0;
	int32 Lit = 0;
	Lights->CountLampsNear(At, Radius, LyudCityThingsImpl::SwitchFloor, Lamps, Lit);
	if (Lamps == 0)
	{
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: switch %s: no lamps within %.0f m"), *LyudCityThingsImpl::Str(T.Id), Radius / 100.f);
		Note(LyudText::Get(TEXT("city.act.switch.none")));
		return;
	}
	// anything lit: all off; all dark: all on (for this visit: back to the hours when he walks into another plot)
	const bool bOn = Lit == 0;
	const int32 N = Lights->SwitchLampsNear(At, Radius, LyudCityThingsImpl::SwitchFloor, bOn);
	SwitchedPlot = LyudCityThingsImpl::Name(T.Plot);
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: switch %s: %d lamps %s"), *LyudCityThingsImpl::Str(T.Id), N, bOn ? TEXT("on") : TEXT("off"));
	Note(LyudText::Get(bOn ? TEXT("city.act.switch.on") : TEXT("city.act.switch.off")));
}

// --- mini-games (docs/city_interiors.md "Mini-games"): the model plays, this draws the panel, polls the keys and pays out

bool ULyudCityThings::IsPlayingGame() const
{
	return Playing.IsValid() && Playing->Def >= 0;
}

void ULyudCityThings::BeginGame(int32 Thing, int32 Minutes, bool bSeated)
{
	UWorld* World = GetWorld();
	if (!World || !Model || Thing < 0 || Thing >= NumThings())
	{
		return;
	}
	if (!Playing.IsValid())
	{
		Playing = MakePimpl<LyudCity::FGameState>();
	}
	if (!Model->StartGame(Thing, LyudCityThingsImpl::When(World, false), *Playing))
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: %s has no game"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Thing].Id));
		return;
	}
	EndShow();
	ClosePage();
	SetPrompted(-1);
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	GameFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
	bGameSeated = bSeated;
	GameMinutes = FMath::Max(0, Minutes);
	GameFrame = GFrameCounter;
	GameBeatShown = -1;
	const LyudCity::FGameDef* D = Model->GameDef(Playing->Def);
	const LyudCity::FRegular* R = Model->RegularOf(*Playing);
	const FText Title = LyudText::Get(LyudCityThingsImpl::Str(D->Title));
	GameOpponent = R ? LyudText::Get(LyudCityThingsImpl::Str(R->Name)) : FText::GetEmpty();
	GameTitle = R ? LyudText::Get(TEXT("city.game.title_vs"), { FStringFormatArg(Title.ToString()), FStringFormatArg(GameOpponent.ToString()) }) : Title;
	// the panel: made once, kept in the viewport (collapsed between games)
	UGameViewportClient* Viewport = World->GetGameViewport();
	if (!GamePanel.IsValid())
	{
		GamePanel = SNew(SLyudCityGame);
	}
	if (Viewport && !bGamePanelAdded)
	{
		Viewport->AddViewportWidgetContent(GamePanel.ToSharedRef(), 61);
		bGamePanelAdded = true;
	}
	GamePanel->SetVisibility(EVisibility::HitTestInvisible);
	GamePanel->SetMode(D->Type == LyudCity::EGameType::Duel);
	GamePanel->SetTitle(GameTitle);
	GamePanel->SetKeys(LyudText::Get(LyudCityThingsImpl::Str(D->Keys.empty() ? std::string("city.game.title") : D->Keys)));
	UpdateGamePanel(true);
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: game %s on %s against %s (seed %llu, %d min)"),
		D->Type == LyudCity::EGameType::Aim ? TEXT("aim") : D->Type == LyudCity::EGameType::Rally ? TEXT("rally") : TEXT("duel"),
		*LyudCityThingsImpl::Str(Model->Things[(size_t)Thing].Id), R ? *LyudCityThingsImpl::Str(R->Id) : TEXT("nobody"),
		(unsigned long long)Playing->Seed, GameMinutes);
}

void ULyudCityThings::TickGame(AHeroCharacter* Hero, APlayerController* PC, bool bTalk, float DeltaTime)
{
	UWorld* World = GetWorld();
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	// over without a result when he walks off, gets up, stops being the hero or a screen comes up
	if (!World || !Model || !Playing.IsValid() || !Hero || !PC || !UI || UI->IsOpen() || (bGameSeated && !Hero->IsSeated())
		|| (!bGameSeated && FVector::Dist(Hero->GetActorLocation(), GameFrom) > LyudCityThingsImpl::PageWalkAway))
	{
		EndGame(false);
		return;
	}
	LyudCity::FGameState& G = *Playing;
	const LyudCity::FGameDef* D = Model->GameDef(G.Def);
	if (!D)
	{
		EndGame(false);
		return;
	}
	Model->TickGame(G, (double)DeltaTime);
	// the keys are polled, never consumed (the talk keeps E while somebody is in front)
	if (!bTalk && GFrameCounter != GameFrame && !G.bOver)
	{
		if (D->Type == LyudCity::EGameType::Duel)
		{
			const int32 Choice = PC->WasInputKeyJustPressed(EKeys::One) ? 0 : PC->WasInputKeyJustPressed(EKeys::Two) ? 1
				: PC->WasInputKeyJustPressed(EKeys::Three) ? 2 : -1;
			if (Choice >= 0)
			{
				Model->PressGame(G, Choice);
			}
		}
		else if (PC->WasInputKeyJustPressed(EKeys::E))
		{
			Model->PressGame(G, -1);
		}
	}
	const bool bBeat = G.BeatSerial != GameBeatShown;
	if (bBeat && G.Beat != LyudCity::EGameBeat::None)
	{
		const bool bMiss = G.Beat == LyudCity::EGameBeat::Miss || G.Beat == LyudCity::EGameBeat::Early || G.Beat == LyudCity::EGameBeat::Late;
		const std::string& Pool = bMiss ? D->SoundMiss : D->SoundHit;
		ULyudSoundscapeSubsystem* Sound = Pool.empty() ? nullptr : ULyudSoundscapeSubsystem::Get(this);
		if (Sound)
		{
			Sound->PlayEffect(LyudCityThingsImpl::Name(Pool), ThingLocation(G.Thing));
		}
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: game beat %s %d:%d round %d"), UTF8_TO_TCHAR(LyudCity::GameBeatName(G.Beat)), G.Score, G.OppScore, G.Round);
	}
	UpdateGamePanel(bBeat);
	if (G.bOver && G.Clock >= G.Wait)
	{
		FinishGame();
	}
}

void ULyudCityThings::UpdateGamePanel(bool bTexts)
{
	if (!GamePanel.IsValid() || !Playing.IsValid() || !Model)
	{
		return;
	}
	const LyudCity::FGameState& G = *Playing;
	const LyudCity::FGameDef* D = Model->GameDef(G.Def);
	if (!D)
	{
		return;
	}
	float Marker = 0.f, ZoneFrom = 0.f, ZoneTo = 0.f, InnerFrom = 0.f, InnerTo = 0.f;
	if (Model->GameBar(G, Marker, ZoneFrom, ZoneTo, InnerFrom, InnerTo))
	{
		GamePanel->SetBar(Marker, ZoneFrom, ZoneTo, InnerFrom, InnerTo, D->Type == LyudCity::EGameType::Rally);
	}
	if (!bTexts)
	{
		return;
	}
	// the texts only when something happened (a beat, the start): built here, never per frame
	GameBeatShown = G.BeatSerial;
	const LyudCity::FRegular* R = Model->RegularOf(G);
	const bool bDuel = D->Type == LyudCity::EGameType::Duel;
	const bool bRally = D->Type == LyudCity::EGameType::Rally;
	const FString Opp = GameOpponent.ToString();
	const int32 Next = FMath::Min(G.Round + 1, FMath::Max(1, D->Rounds));
	FString Status;
	if (bDuel)
	{
		Status = LyudText::Get(TEXT("city.game.turn"), { FStringFormatArg(Next), FStringFormatArg(D->Rounds) }).ToString();
	}
	else if (!bRally)
	{
		Status = LyudText::Get(TEXT("city.game.throw"), { FStringFormatArg(Next), FStringFormatArg(D->Rounds) }).ToString();
	}
	else if (R)
	{
		Status = LyudText::Get(TEXT("city.game.first_to"), { FStringFormatArg(D->Need) }).ToString();
	}
	const FText Score = R || bDuel
		? LyudText::Get(TEXT("city.game.score_vs"), { FStringFormatArg(G.Score), FStringFormatArg(G.OppScore), FStringFormatArg(Opp) })
		: LyudText::Get(bRally ? TEXT("city.game.score_hits") : TEXT("city.game.score_solo"), { FStringFormatArg(bRally ? G.Hits : G.Score), FStringFormatArg(D->Need) });
	GamePanel->SetStatus(FText::FromString(Status.IsEmpty() ? Score.ToString() : Status + TEXT("   ·   ") + Score.ToString()));
	// what just happened (or the opponent's hello), then in a duel the look that may betray the next move
	auto MoveName = [D](int32 Move) -> FString
	{
		return Move >= 0 && Move < (int32)D->Moves.size() ? LyudText::Get(LyudCityThingsImpl::Str(D->Moves[(size_t)Move].Key)).ToString() : FString();
	};
	FString Hint;
	if (G.Beat != LyudCity::EGameBeat::None)
	{
		Hint = LyudText::Get(FString(TEXT("city.game.beat.")) + UTF8_TO_TCHAR(LyudCity::GameBeatName(G.Beat)),
			{ FStringFormatArg(MoveName(G.HeroMove)), FStringFormatArg(MoveName(G.LastOpp)), FStringFormatArg(Opp) }).ToString();
	}
	else if (R && !R->Hello.empty())
	{
		Hint = LyudText::Get(TEXT("city.game.says"), { FStringFormatArg(Opp), FStringFormatArg(LyudText::Get(LyudCityThingsImpl::Str(R->Hello)).ToString()) }).ToString();
	}
	if (bDuel && !G.bOver && G.Tell >= 0 && G.Tell < (int32)D->Moves.size() && !D->Moves[(size_t)G.Tell].Tell.empty())
	{
		const FString Tell = LyudText::Get(LyudCityThingsImpl::Str(D->Moves[(size_t)G.Tell].Tell), { FStringFormatArg(Opp) }).ToString();
		Hint = Hint.IsEmpty() ? Tell : Hint + TEXT("\n") + Tell;
	}
	GamePanel->SetHint(FText::FromString(Hint));
	if (bDuel)
	{
		for (int32 I = 0; I < 3; ++I)
		{
			const int32 Power = I < (int32)D->Moves.size() ? FMath::Max(1, D->Moves[(size_t)I].Power) : 1;
			GamePanel->SetMove(I, LyudText::Get(TEXT("city.game.move_option"), { FStringFormatArg(I + 1), FStringFormatArg(MoveName(I)), FStringFormatArg(Power) }));
		}
	}
}

void ULyudCityThings::FinishGame()
{
	UWorld* World = GetWorld();
	if (!World || !Model || !Playing.IsValid())
	{
		return;
	}
	const LyudCity::FGameDef* D = Model->GameDef(Playing->Def);
	const int32 Thing = Playing->Thing;
	const int32 Score = Playing->Score, OppScore = Playing->OppScore;
	const LyudCity::FGameEnd End = Model->FinishGame(*Playing, LyudCityThingsImpl::When(World));
	if (!D || End.Result == LyudCity::EGameResult::None)
	{
		EndGame(false);
		return;
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: game over on %s: %s %d:%d line=%s"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Thing].Id),
		UTF8_TO_TCHAR(LyudCity::GameResultName(End.Result)), Score, OppScore, *LyudCityThingsImpl::Str(End.Line));
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		for (const std::pair<std::string, int>& P : End.Prize)
		{
			const FName Item = LyudCityThingsImpl::Name(P.first);
			if (ULyudInventory::FindDef(Item))
			{
				Inv->Add(Item, P.second);
			}
		}
	}
	if (!End.NewStamps.empty())
	{
		TArray<FString> Fresh;
		for (const std::string& Stamp : End.NewStamps)
		{
			Fresh.Add(LyudCityThingsImpl::Str(Stamp));
		}
		AnnounceStamps(Fresh);
	}
	if (End.Result == LyudCity::EGameResult::Win)
	{
		if (!D->SoundEnd.empty())
		{
			if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
			{
				Sound->PlayEffect(LyudCityThingsImpl::Name(D->SoundEnd), ThingLocation(Thing));
			}
		}
		ULyudQuests::Notify(this, TEXT("win"), LyudCityThingsImpl::Str(End.Target));
	}
	const bool bSeated = bGameSeated;
	const int32 Minutes = GameMinutes;
	const FString Opp = GameOpponent.ToString();
	EndGame(true);
	// the result page: the line (its {0} the opponent), then what the opponent says
	FString Body = End.Line.empty() ? FString() : LyudText::Get(LyudCityThingsImpl::Str(End.Line), { FStringFormatArg(Opp) }).ToString();
	if (!End.Says.empty())
	{
		const FString Says = LyudText::Get(TEXT("city.game.says"), { FStringFormatArg(Opp), FStringFormatArg(LyudText::Get(LyudCityThingsImpl::Str(End.Says)).ToString()) }).ToString();
		Body = Body.IsEmpty() ? Says : Body + TEXT("\n\n") + Says;
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	PageThing = -1;
	PageLeft = 0;
	PageTitle = GameTitle;
	PageFrom = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	PageFrame = GFrameCounter;
	if (!Body.IsEmpty())
	{
		ShowPage(PageTitle, FText::FromString(Body), false);
	}
	if (Minutes > 0)
	{
		JumpMinutes(Minutes, !bSeated);
	}
}

void ULyudCityThings::EndGame(bool bFinished)
{
	if (Playing.IsValid())
	{
		if (!bFinished && Playing->Def >= 0)
		{
			UE_LOG(LogLyudCity, Display, TEXT("CityThings: game left unfinished"));
		}
		*Playing = LyudCity::FGameState();
	}
	if (GamePanel.IsValid())
	{
		GamePanel->SetVisibility(EVisibility::Collapsed);
	}
	GameBeatShown = -1;
}

bool ULyudCityThings::StartGameAt(const FString& ThingId)
{
	const int32 Thing = Model ? Model->FindThing(std::string(TCHAR_TO_UTF8(*ThingId))) : -1;
	const LyudCity::FAct* A = Thing >= 0 ? Model->ActOf(Thing) : nullptr;
	if (!A || !Model->GameOfThing(Thing))
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: %s: no such thing, or its act has no game"), *ThingId);
		return false;
	}
	BeginGame(Thing, A->Minutes, false);
	return IsPlayingGame();
}

bool ULyudCityThings::StartNearestGame(const FString& Type)
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return false;
	}
	const LyudCity::EGameType Want = LyudCity::GameTypeOf(std::string(TCHAR_TO_UTF8(*Type)));
	const FVector From = Pawn->GetActorLocation();
	int32 Best = -1;
	double BestD = 0.0;
	// developer mode only: one pass over the things
	for (int32 I = 0; I < NumThings(); ++I)
	{
		const LyudCity::FGameDef* D = Model->GameOfThing(I);
		const double Dist = D && D->Type == Want ? FVector::DistSquared(ThingLocation(I), From) : -1.0;
		if (Dist >= 0.0 && (Best < 0 || Dist < BestD))
		{
			Best = I;
			BestD = Dist;
		}
	}
	if (Best < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no %s game anywhere"), *Type);
		return false;
	}
	return StartGameAt(LyudCityThingsImpl::Str(Model->Things[(size_t)Best].Id));
}

void ULyudCityThings::PressGame(int32 Choice)
{
	if (IsPlayingGame())
	{
		Model->PressGame(*Playing, Choice);
	}
}

void ULyudCityThings::WinGame()
{
	if (IsPlayingGame())
	{
		Model->WinGame(*Playing);
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: game won by the developer"));
	}
}

void ULyudCityThings::QuitGame()
{
	EndGame(false);
}

void ULyudCityThings::ShowRegulars(bool bPage)
{
	if (!Model)
	{
		return;
	}
	const FString Sep = LyudText::Get(TEXT("city.game.list_sep")).ToString();
	FString Body;
	for (const LyudCity::FRegular& R : Model->Regulars)
	{
		FString Plays, Plots;
		for (const std::string& W : R.Plays)
		{
			Plays += (Plays.IsEmpty() ? FString() : Sep) + LyudText::Get(TEXT("city.game.vs.") + LyudCityThingsImpl::Str(W)).ToString();
		}
		for (const std::string& P : R.Plots)
		{
			Plots += (Plots.IsEmpty() ? FString() : FString(TEXT(" "))) + LyudCityThingsImpl::Str(P);
		}
		const FString Name = LyudText::Get(LyudCityThingsImpl::Str(R.Name)).ToString();
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: regular %s (%s): %s, skill %.2f, plays %s, at %s"), *LyudCityThingsImpl::Str(R.Id), *Name,
			*LyudCityThingsImpl::Str(R.Style), R.Skill, *Plays, Plots.IsEmpty() ? TEXT("anywhere") : *Plots);
		Body += (Body.IsEmpty() ? FString() : FString(TEXT("\n"))) + LyudText::Get(TEXT("city.game.regulars.line"), { FStringFormatArg(Name),
			FStringFormatArg(LyudText::Get(TEXT("city.game.style.") + LyudCityThingsImpl::Str(R.Style)).ToString()), FStringFormatArg(Plays) }).ToString();
	}
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!bPage || !Pawn || NumThings() == 0 || ShowAt >= 0 || IsPlayingGame() || Body.IsEmpty())
	{
		return;
	}
	PageThing = -1;
	PageLeft = 0;
	PageTitle = LyudText::Get(TEXT("city.game.regulars.title"));
	PageFrom = Pawn->GetActorLocation();
	PageFrame = GFrameCounter;
	ShowPage(PageTitle, FText::FromString(Body), false);
	SetPrompted(-1);
}

// --- console

static FAutoConsoleCommandWithWorldAndArgs GCityThingsList(TEXT("lyud.City.Things"),
	TEXT("lyud.City.Things: how many things to do there are in the buildings (City/interiors.json) and the five nearest the hero"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->LogNearest(5);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityThingsDo(TEXT("lyud.City.Do"),
	TEXT("lyud.City.Do <thing id>: runs a thing's act as if E was pressed at it, wherever the hero is (P-LIBRARY/shelf_birds)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (Things && Args.Num() > 0)
		{
			Things->RunThingById(Args[0]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityThingsShow(TEXT("lyud.City.Things.Show"),
	TEXT("lyud.City.Things.Show 0|1: labels over the things within 15 m (id and act; the one in view yellow)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->SetShowLabels(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityWeekday(TEXT("lyud.City.Weekday"),
	TEXT("lyud.City.Weekday <mon..sun | 0..6>: moves the game clock on to the next such weekday, same time of day (the quests' dev blocks: a day the place is open)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
		if (!Cal || Args.Num() == 0)
		{
			return;
		}
		static const TCHAR* const Days[7] = { TEXT("mon"), TEXT("tue"), TEXT("wed"), TEXT("thu"), TEXT("fri"), TEXT("sat"), TEXT("sun") };
		int32 Want = Args[0].IsNumeric() ? FCString::Atoi(*Args[0]) : -1;
		for (int32 D = 0; D < 7 && Want < 0; ++D)
		{
			Want = Args[0].StartsWith(Days[D], ESearchCase::IgnoreCase) ? D : -1;
		}
		if (Want < 0 || Want > 6)
		{
			UE_LOG(LogLyudCity, Warning, TEXT("lyud.City.Weekday: %s is not a weekday"), *Args[0]);
			return;
		}
		const int32 Today = (int32)Cal->Now().GetDayOfWeek();
		const int32 Ahead = (Want - Today + 7) % 7;
		if (Ahead > 0)
		{
			Cal->SetNow(Cal->Now() + FTimespan::FromDays((double)Ahead));
		}
		UE_LOG(LogLyudCity, Display, TEXT("lyud.City.Weekday: %s (%d days on)"), *Cal->Now().ToString(), Ahead);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityCollections(TEXT("lyud.City.Collections"),
	TEXT("lyud.City.Collections [grant|clear <set|*>]: the collections of the town's things (stamps: films seen, records heard, kiosks visited...) to the log and the page; grant or clear a set"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (!Things)
		{
			return;
		}
		if (Args.Num() >= 1 && !Things->EditCollections(Args[0], Args.Num() >= 2 ? Args[1] : FString(TEXT("*"))))
		{
			UE_LOG(LogLyudCity, Warning, TEXT("lyud.City.Collections: %s %s: no such verb or set (grant|clear <set|*>)"), *Args[0], Args.Num() >= 2 ? *Args[1] : TEXT("*"));
		}
		Things->ShowCollections(true);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityGame(TEXT("lyud.City.Game"),
	TEXT("lyud.City.Game <thing id> | press [1-3] | win | quit | regulars: starts the mini-game of a thing (darts, chess...) wherever the hero is; press E (or a duel's move 1..3); win it at once; leave it; list the regulars"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (!Things || Args.Num() == 0)
		{
			return;
		}
		if (Args[0] == TEXT("press"))
		{
			Things->PressGame(Args.Num() >= 2 ? FCString::Atoi(*Args[1]) - 1 : -1);
		}
		else if (Args[0] == TEXT("win"))
		{
			Things->WinGame();
		}
		else if (Args[0] == TEXT("quit"))
		{
			Things->QuitGame();
		}
		else if (Args[0] == TEXT("regulars"))
		{
			Things->ShowRegulars(true);
		}
		else
		{
			Things->StartGameAt(Args[0]);
		}
	}));

// --- developer mode (docs/dev_mode.md), when it is in the tree

#if LYUD_CITY_THINGS_DEV
static LyudDev::FAutoAction GCityDevNear(TEXT("interiors.near"), TEXT("interiors"), TEXT("dev.action.interiors.near"), [](UWorld* W)
{
	if (const ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->LogNearest(10);
	}
});
static LyudDev::FAutoAction GCityDevRun(TEXT("interiors.run_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.run_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->RunNearest();
	}
});
static LyudDev::FAutoAction GCityDevReset(TEXT("interiors.reset_daily"), TEXT("interiors"), TEXT("dev.action.interiors.reset_daily"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ResetDailyLimits();
	}
});
static LyudDev::FAutoAction GCityDevMoney(TEXT("interiors.money"), TEXT("interiors"), TEXT("dev.action.interiors.money"), [](UWorld* W)
{
	if (ULyudInventory* Inv = ULyudInventory::Get(W))
	{
		Inv->Earn(500);
	}
});
static LyudDev::FAutoAction GCityDevOpenAll(TEXT("interiors.open_all_hours"), TEXT("interiors"), TEXT("dev.action.interiors.open_all_hours"),
	[](UWorld*) { ULyudCityThings::bOpenAllHours = !ULyudCityThings::bOpenAllHours; },
	[](UWorld*) { return ULyudCityThings::bOpenAllHours; });
static LyudDev::FAutoAction GCityDevLabels(TEXT("interiors.show_things"), TEXT("interiors"), TEXT("dev.action.interiors.show_things"),
	[](UWorld* W)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(W))
		{
			Things->SetShowLabels(!Things->AreLabelsShown());
		}
	},
	[](UWorld* W)
	{
		const ULyudCityThings* Things = ULyudCityThings::Get(W);
		return Things && Things->AreLabelsShown();
	});
static LyudDev::FAutoAction GCityDevCollections(TEXT("interiors.collections"), TEXT("interiors"), TEXT("dev.action.interiors.collections"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ShowCollections(true);
	}
});
static LyudDev::FAutoAction GCityDevCollectAll(TEXT("interiors.collections_all"), TEXT("interiors"), TEXT("dev.action.interiors.collections_all"),
	[](UWorld* W)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(W))
		{
			Things->EditCollections(Things->AreCollectionsComplete() ? TEXT("clear") : TEXT("grant"), TEXT("*"));
			Things->ShowCollections(false);
		}
	},
	[](UWorld* W)
	{
		const ULyudCityThings* Things = ULyudCityThings::Get(W);
		return Things && Things->AreCollectionsComplete();
	});
static LyudDev::FAutoAction GCityDevGameAim(TEXT("interiors.game_aim"), TEXT("interiors"), TEXT("dev.action.interiors.game_aim"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->StartNearestGame(TEXT("aim"));
	}
});
static LyudDev::FAutoAction GCityDevGameRally(TEXT("interiors.game_rally"), TEXT("interiors"), TEXT("dev.action.interiors.game_rally"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->StartNearestGame(TEXT("rally"));
	}
});
static LyudDev::FAutoAction GCityDevGameDuel(TEXT("interiors.game_duel"), TEXT("interiors"), TEXT("dev.action.interiors.game_duel"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->StartNearestGame(TEXT("duel"));
	}
});
static LyudDev::FAutoAction GCityDevGameWin(TEXT("interiors.game_win"), TEXT("interiors"), TEXT("dev.action.interiors.game_win"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->WinGame();
	}
});
static LyudDev::FAutoAction GCityDevRegulars(TEXT("interiors.regulars"), TEXT("interiors"), TEXT("dev.action.interiors.regulars"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ShowRegulars(true);
	}
});
#endif

#undef LYUD_CITY_THINGS_DEV
