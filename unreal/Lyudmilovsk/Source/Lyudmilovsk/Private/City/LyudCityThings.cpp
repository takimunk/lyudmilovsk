#include "City/LyudCityThings.h"

#include "CityThingsModel.h"
#include "City/SLyudCityBook.h"
#include "City/SLyudCityGame.h"
#include "City/SLyudCityPage.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcDialogue.h"
#include "Sound/SoundBase.h"
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
	/** Room tones: the plot's tone is heard while a thing of it is this near on the hero's floor (cm); the loop fades in and
	 *  out over these seconds; outdoors (ULyudSoundscapeSubsystem::GetOutdoors 1) it keeps this much of its level; a
	 *  one-shot plays this far from the hero (cm). */
	constexpr double ToneRadius = 1000.0;
	constexpr float ToneFadeIn = 1.5f;
	constexpr float ToneFadeOut = 1.0f;
	constexpr float ToneOutdoorKeep = 0.2f;
	constexpr double ToneShotNear = 200.0;
	constexpr double ToneShotFar = 500.0;
	/** A lift trip: the fade to black and back (s), how long the screen stays black after the move (s), the game seconds it
	 *  takes. */
	constexpr float LiftFadeOut = 0.35f;
	constexpr float LiftFadeIn = 0.5f;
	constexpr double LiftHold = 0.35;
	constexpr double LiftSeconds = 20.0;

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
	 *  down meets the floor; a bed or a bench with a blocking box is met on its top, and the floor is its height below; a hit
	 *  far from where the data puts the floor is not believed (LyudCity::FModel::SeatFloor: the feet never go through it). */
	double FloorUnder(UWorld* World, const AActor* Hero, const FVector& At, double HeightCm)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(CitySeatFloor), false, Hero);
		const bool bHit = World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 30.0), At - FVector(0.0, 0.0, 300.0), ECC_Visibility, Q);
		return LyudCity::FModel::SeatFloor(At.Z, HeightCm, bHit, bHit ? (double)Hit.ImpactPoint.Z : 0.0);
	}

	/** The camera manager's own yaw limits (APlayerCameraManager's defaults): no cone. */
	constexpr float FreeYawMin = 0.f;
	constexpr float FreeYawMax = 359.999f;
	/** A bed's window is looked toward when it is this near (cm). */
	constexpr double WindowRadius = 600.0;
	/** Lying down: how long the view settles from the eyes to the pillow (s). */
	constexpr float LieSettle = 1.6f;
	/** A load put the hero this far from where he would stand up from his seat: he is put back where the save had him (cm). */
	constexpr double LoadedFar = 200.0;

	FString HourText(int32 Hour)
	{
		return FString::Printf(TEXT("%02d:00"), ((Hour % 24) + 24) % 24);
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
	CloseBook();
	if (Book.IsValid() && bBookAdded)
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Book.ToSharedRef());
		}
	}
	Book.Reset();
	bBookAdded = false;
	SleepHour = -1;
	bSleeping = false;
	EndTalk();
	EndLiftChoice();
	EndLift();
	if (ToneAudio)
	{
		ToneAudio->Stop();
		ToneAudio->DestroyComponent();
	}
	ToneAudio = nullptr;
	ToneSounds.Reset();
	ToneHeard = -1;
	EndShow();
	EndLie(false);
	UnhookSeatKeys();
	ClearLookCone();
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
	// the room tones' loops, once (a handful of assets; a tone with no asset is silent until its sound exists)
	ToneSounds.Reset();
	ToneSounds.SetNum((int32)Model->Tones.size());
	int32 ToneLoaded = 0;
	for (int32 I = 0; I < ToneSounds.Num(); ++I)
	{
		const std::string& Asset = Model->Tones[(size_t)I].Asset;
		if (!Asset.empty())
		{
			ToneSounds[I] = LoadObject<USoundBase>(nullptr, *LyudCityThingsImpl::Str(Asset), nullptr, LOAD_NoWarn | LOAD_Quiet);
			ToneLoaded += ToneSounds[I] ? 1 : 0;
		}
	}
	ToneHeard = -1;
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %d staff talk topics, %d room tones (%d loops loaded)"), (int32)Model->Topics.size(),
		ToneSounds.Num(), ToneLoaded);
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
	ResetHeroState();
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
	// the pawn is where the save had him by now: whatever he was in the middle of here is let go (never stuck on a seat)
	ResetHeroState();
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
	const FTransform Frame(Facing, Front);
	// where he will stand up to: found now, while his capsule still collides (SitOn turns it off)
	const FVector StandOffset = StandOffsetFor(Frame, FloorZ, Hero, bLie);
	Hero->SitOn(SeatRoot, Frame, (float)Height);
	Hero->SetStandUpOffset(StandOffset);
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-8.0, (double)T.Yaw, 0.0));
		// seated he looks round within a cone about the seat's facing (lying, the pillow's camera has the view)
		if (!bLie)
		{
			SetLookCone(PC, T.Yaw, LyudCity::FModel::LookCone(T));
		}
	}
	SeatedOn = Thing;
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %s on %s (floor z %.0f, seat %.0f cm, stands up to %s): seated=%d"), bLie ? TEXT("lies") : TEXT("sits"),
		*LyudCityThingsImpl::Str(T.Id), FloorZ, Height, *StandOffset.ToCompactString(), Hero->IsSeated() ? 1 : 0);
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
	// toward the room's window when one is near (a little up: the sky over the sill), else the ceiling
	FRotator Look(85.0, (double)LieYaw, 0.0);
	const int32 Window = Model && LyingOn >= 0 ? Model->WindowFrom(LyingOn, LyudCityThingsImpl::WindowRadius) : -1;
	if (Window >= 0)
	{
		const FVector To = ThingLocation(Window) - LieEye;
		if (To.SizeSquared2D() > 400.0)
		{
			Look = To.Rotation();
			Look.Pitch = FMath::Clamp(Look.Pitch + 8.0, 4.0, 40.0);
			Look.Roll = 0.0;
		}
	}
	LieCamera->SetActorLocationAndRotation(LieEye, Look);
	// a slow settle from the eyes down onto the pillow
	PC->SetViewTargetWithBlend(LieCamera.Get(), bCut ? 0.f : LyudCityThingsImpl::LieSettle, VTBlend_EaseInOut, 2.f);
	if (!Hero->IsHidden())
	{
		Hero->SetActorHiddenInGame(true);
		bHeroHiddenForLie = true;
		LieHiddenHero = Hero;
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
		// the one hidden, even when the controller's pawn is another by now (a bird flown from the pillow)
		if (AHeroCharacter* Hidden = LieHiddenHero.Get())
		{
			Hidden->SetActorHiddenInGame(false);
		}
		else if (Hero)
		{
			Hero->SetActorHiddenInGame(false);
		}
		bHeroHiddenForLie = false;
		LieHiddenHero.Reset();
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
	bLieAutoStand = false;
	if (SleepHour >= 0 || bSleeping)
	{
		SleepHour = -1;
		SleepThing = -1;
		bSleeping = false;
		if (Page && PageThing < 0)
		{
			ClosePage();
		}
	}
}

void ULyudCityThings::TickLie(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !PC || !Hero->IsSeated() || SeatedOn != LyingOn)
	{
		EndLie(false);     // he got up (a step, Space) or is not the hero any more
		return;
	}
	if (SleepHour >= 0)
	{
		return;     // the chooser has the keys (TickSleepChoice)
	}
	const bool bSettled = FadeMinutes == 0 && !bFadingIn && JumpLeft <= 0.0;
	if (bSettled && !Page && !bTalk && GFrameCounter != PageFrame && PC->WasInputKeyJustPressed(EKeys::E))
	{
		EndLie(true);
	}
	else if (bSettled && LieUntil > 0.0 && World->GetTimeSeconds() >= LieUntil)
	{
		// woken (a sleep: the bed's line on the page once he is up)
		const bool bWoke = bSleeping;
		const FString Line = SleepLine;
		const FText Title = SleepTitle;
		EndLie(true);
		if (bWoke && !Line.IsEmpty())
		{
			PageThing = -1;
			PageLeft = 0;
			PageTitle = Title;
			PageFrom = Hero->GetActorLocation();
			PageFrame = GFrameCounter;
			ShowPage(PageTitle, LineText(Line), false);
			SetPrompted(-1);
		}
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
	EndTalk();
	// the staff talk is asked before the act runs (a daily act's count or the money spent must not hide it): its topics now
	std::vector<int> TalkOffer;
	if (!bShutWords && Model->HasTopics(Thing))
	{
		Model->TalkReady(Thing, Now, LyudCityThingsImpl::Ask(this), TalkOffer);
	}
	const LyudCity::FOutcome O = Model->Run(Thing, Now, LyudCityThingsImpl::Ask(this));
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: do %s (%s, act %s) -> %s line=%s minutes=%d price=%d shop=%s event=%s/%s%s"),
		*LyudCityThingsImpl::Str(T.Id), *LyudCityThingsImpl::Str(T.Kind), O.Act >= 0 ? *LyudCityThingsImpl::Str(Model->Acts[O.Act].Id) : TEXT("-"), UTF8_TO_TCHAR(LyudCity::VerdictName(O.Verdict)),
		*LyudCityThingsImpl::Str(O.Line), O.Minutes, O.Price, *LyudCityThingsImpl::Str(O.Shop), *LyudCityThingsImpl::Str(O.Event), *LyudCityThingsImpl::Str(O.Target),
		O.bFirstVisitToday ? TEXT(" visit") : (O.bClosed ? TEXT(" closed") : TEXT("")));
	if (O.Verdict != LyudCity::EVerdict::Ok)
	{
		// the staff still talk when the act is refused for the hero's own reasons (done today, no money, no item)
		if (!TalkOffer.empty() && (O.Verdict == LyudCity::EVerdict::DailyDone || O.Verdict == LyudCity::EVerdict::NoMoney
			|| O.Verdict == LyudCity::EVerdict::NeedsItem))
		{
			TArray<int32> RefusedTopics;
			RefusedTopics.Reserve((int32)TalkOffer.size());
			for (const int Topic : TalkOffer)
			{
				RefusedTopics.Add(Topic);
			}
			BeginTalk(Thing, RefusedTopics, TalkSpeaker(Thing), FText::GetEmpty(), FString(), FString(), bFromConsole);
			return;
		}
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
	// a lift or a stair's door: the chooser of its stops, or straight to its one stop (the act's sound, events and stamps came)
	if (Model->IsLift(Thing) && !O.bClosed)
	{
		ClosePage();
		if (T.Floors.size() == 1)
		{
			BeginLift(T.Floors[0].To);
		}
		else
		{
			BeginLiftChoice(Thing);
		}
		return;
	}
	// a counter with topics and its staff on shift: the chooser instead of the shop or the page (the act's line on top, "Buy..."
	// first when it sells); not for games, shows, reading, sitting or lying, or a stretch of time behind a fade
	if (!TalkOffer.empty() && O.Game < 0 && O.Screen.empty() && O.Text < 0 && O.Pose != "sit" && O.Pose != "lie" && !(O.Minutes > 0 && O.bFade))
	{
		TArray<int32> Topics;
		Topics.Reserve((int32)TalkOffer.size());
		for (const int Topic : TalkOffer)
		{
			Topics.Add(Topic);
		}
		if (O.Minutes > 0)
		{
			JumpMinutes(O.Minutes, false);
		}
		BeginTalk(Thing, Topics, TalkSpeaker(Thing), O.Line.empty() ? FText::GetEmpty() : LineText(LyudCityThingsImpl::Str(O.Line)),
			LyudCityThingsImpl::Str(O.Shop), LyudCityThingsImpl::Str(O.Title), bFromConsole);
		return;
	}
	if (O.Game >= 0)
	{
		// a mini-game instead of the page: seated first when the act sits (a chess table: on its chair) and the hero is at it
		int GameSeat = -1;
		if (Hero && O.Pose == "sit" && !Hero->IsSeated() && FVector::Dist(Hero->GetActorLocation(), At) < 400.f)
		{
			GameSeat = Model->SeatFor(Thing, SeatedOn);
			if (GameSeat >= 0 && !Sit(GameSeat, Hero, false))
			{
				GameSeat = -1;
			}
		}
		BeginGame(Thing, O.Minutes, Hero && Hero->IsSeated() && SeatedOn >= 0 && (SeatedOn == Thing || SeatedOn == GameSeat));
		return;
	}
	if (!O.Shop.empty())
	{
		OpenShopOf(LyudCityThingsImpl::Str(O.Shop), LyudCityThingsImpl::Str(O.Title));
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
	if (O.Text >= 0)
	{
		// the reading view instead of the page: seated for it when the act sits (a library's reading pod) and he is at it
		int BookSeat = -1;
		if (bNear && O.Pose == "sit" && !Hero->IsSeated())
		{
			BookSeat = Model->SeatFor(Thing, SeatedOn);
			if (BookSeat >= 0 && !Sit(BookSeat, Hero, false))
			{
				BookSeat = -1;
			}
		}
		OpenBook(O.Text, Thing, Hero && Hero->IsSeated() && SeatedOn >= 0 && (SeatedOn == Thing || SeatedOn == BookSeat));
		return;
	}
	const bool bLie = bNear && O.Pose == "lie" && Sit(Thing, Hero, true);
	if (bNear && O.Pose == "sit")
	{
		// on the thing itself when it is a seat, else on a free seat by it (never in the middle of a table)
		const int SitSeat = Model->SeatFor(Thing, SeatedOn);
		if (SitSeat >= 0)
		{
			Sit(SitSeat, Hero, false);
		}
	}
	if (bLie && O.bSleep)
	{
		// a "sleep" bed: down onto the pillow, then the hour he wakes at is chosen (its line comes when he is up)
		BeginLie(Thing, false, false);
		BeginSleepChoice(Thing, O.Act, O.Line.empty() ? FString() : LyudCityThingsImpl::Str(O.Line), Title);
		return;
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
	// a step on a seat whose keys are ours asked to get up (done here, never inside the hero's own hook)
	if (bStandAsked)
	{
		bStandAsked = false;
		if (Hero && Hero->IsSeated() && SeatedOn >= 0)
		{
			if (LyingOn >= 0)
			{
				EndLie(true);
			}
			else
			{
				Hero->StandUp();
			}
		}
	}
	if (SeatedOn >= 0 && (!Hero || !Hero->IsSeated()))
	{
		// up (a step, Space, StandUp) or the seat went from under him: the cone and the seat's keys go, and he walks again; the
		// keys first, while SeatedOn still says the seat is ours (the controller on another pawn, a bird: he is still seated, and
		// hooks left on him would swallow every step)
		UnhookSeatKeys();
		SeatedOn = -1;
		SeatScreen = -1;
		RecoverFromSeat(Hero);
		ClearLookCone();
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
		TickTone(Hero, LyudCityThingsImpl::PickEvery);
	}
	// E is the talk's while he faces somebody or is talking (one press must not both talk and buy)
	const bool bTalk = TalkDirector.IsValid() && TalkDirector->IsEngaged();
	if (LyingOn >= 0)
	{
		TickLie(Hero, PC, bTalk);
		if (LyingOn < 0)
		{
			// he just got up (E, a step, woken): the press that got him up must not also run the thing in view this frame
			SetPrompted(-1);
			return;
		}
	}
	if (LiftDest >= 0)
	{
		SetPrompted(-1);
		TickLift(Hero, PC);
		return;
	}
	if (LiftThing >= 0)
	{
		SetPrompted(-1);
		TickLiftChoice(Hero, PC, bTalk);
		return;
	}
	if (SleepHour >= 0)
	{
		SetPrompted(-1);
		TickSleepChoice(Hero, PC, bTalk);
		return;
	}
	if (TalkThing >= 0)
	{
		SetPrompted(-1);
		TickTalk(Hero, PC, bTalk);
		return;
	}
	if (BookText >= 0)
	{
		SetPrompted(-1);
		TickBook(Hero, PC, bTalk);
		return;
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
						Sound->PlayEffect(TEXT("diary_page"), ThingLocation(PageThing));
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
	// what was read and where (the Read list), under the collections
	const FString Read = ReadListBody();
	if (!Read.IsEmpty())
	{
		Body += Body.IsEmpty() ? Read : FString(TEXT("\n\n")) + Read;
	}
	UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	// the page needs the tick (it closes on E or walking off), and the tick runs only while there are things
	if (!bPage || !Pawn || NumThings() == 0 || ShowAt >= 0 || BookText >= 0 || SleepHour >= 0)
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

// --- sitting and lying (docs/city_interiors.md "Sitting and lying"): the spot to stand up to, the look cone, never stuck

FVector ULyudCityThings::StandOffsetFor(const FTransform& Frame, double FloorZ, const AHeroCharacter* Hero, bool bLie) const
{
	// in front (a bench, a sofa), beside (a chair pushed under a table), behind (a pew, a desk's chair); a bed: off its side
	static constexpr double SeatSpots[][2] = { { 45.0, 0.0 }, { 10.0, 65.0 }, { 10.0, -65.0 }, { -30.0, 78.0 }, { -30.0, -78.0 }, { -85.0, 0.0 } };
	static constexpr double BedSpots[][2] = { { 50.0, 0.0 }, { 50.0, 60.0 }, { 50.0, -60.0 }, { 20.0, 110.0 }, { 20.0, -110.0 } };
	const FVector Fallback(bLie ? 50.0 : 45.0, 0.0, 0.0);
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Hero ? Hero->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return Fallback;
	}
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CityStandUp), false, Hero);
	const int32 NumSpots = bLie ? (int32)UE_ARRAY_COUNT(BedSpots) : (int32)UE_ARRAY_COUNT(SeatSpots);
	for (int32 I = 0; I < NumSpots; ++I)
	{
		const FVector Spot(bLie ? BedSpots[I][0] : SeatSpots[I][0], bLie ? BedSpots[I][1] : SeatSpots[I][1], 0.0);
		const FVector Feet = Frame.TransformPosition(Spot);
		const FVector Middle(Feet.X, Feet.Y, FloorZ + HalfHeight + 2.0);
		// his capsule fits there (a table, a wall, the next chair's back block it)...
		if (World->OverlapBlockingTestByChannel(Middle, FQuat::Identity, ECC_Pawn, Shape, Q))
		{
			continue;
		}
		// ...and there is floor under it about the seat's (not the stairwell beside a gallery's bench)
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Middle, Middle - FVector(0.0, 0.0, HalfHeight + 60.0), ECC_Visibility, Q)
			|| FMath::Abs(Hit.ImpactPoint.Z - FloorZ) > 40.0)
		{
			continue;
		}
		return Spot;
	}
	return Fallback;
}

void ULyudCityThings::SetLookCone(APlayerController* PC, float Yaw, float Half)
{
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Cam)
	{
		return;
	}
	// APlayerCameraManager::LimitViewYaw clamps round the middle of the two (FMath::ClampAngle): any turns of 360 are fine
	Cam->ViewYawMin = Yaw - Half;
	Cam->ViewYawMax = Yaw + Half;
	ConeBy = PC;
}

void ULyudCityThings::ClearLookCone()
{
	APlayerController* PC = ConeBy.Get();
	if (APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr)
	{
		Cam->ViewYawMin = LyudCityThingsImpl::FreeYawMin;
		Cam->ViewYawMax = LyudCityThingsImpl::FreeYawMax;
	}
	ConeBy.Reset();
}

void ULyudCityThings::RecoverFromSeat(AHeroCharacter* Hero)
{
	if (!Hero || Hero->IsSeated())
	{
		return;
	}
	if (HookedHero.Get() == Hero)
	{
		// not seated: the hooks are nobody's (StandUp clears them; a seat whose anchor went does not)
		Hero->SeatMove = nullptr;
		Hero->SeatLeave = nullptr;
	}
	HookedHero.Reset();
	// StandUp walks him again; a seat that went from under him (its anchor destroyed) left him with no movement and no collision
	UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	if (!Move || Move->MovementMode != MOVE_None)
	{
		return;
	}
	UE_LOG(LogLyudCity, Warning, TEXT("CityThings: the seat went from under the hero: walking again"));
	if (UCapsuleComponent* Capsule = Hero->GetCapsuleComponent(); Capsule && Capsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	Move->SetMovementMode(MOVE_Walking);
}

void ULyudCityThings::HookSeatKeys(AHeroCharacter* Hero)
{
	if (!Hero || !Hero->IsSeated() || (SeatedOn < 0 && LyingOn < 0))
	{
		return;
	}
	HookedHero = Hero;
	TWeakObjectPtr<ULyudCityThings> Self(this);
	// a step: shuts the text (the next one stands him up), nothing while the chooser is up or he sleeps (the chooser polls
	// A/D and W/S), else up (as without hooks). Only asks: the tick does it (a hook must not clear the hooks it runs in)
	Hero->SeatMove = [Self](const FVector2D& Step)
	{
		ULyudCityThings* Things = Self.Get();
		if (!Things || Step.Size() <= 0.5f)
		{
			return;
		}
		if (Things->BookText >= 0)
		{
			Things->bCloseBookAsked = true;
		}
		else if (Things->SleepHour < 0 && !Things->bSleeping)
		{
			Things->bStandAsked = true;
		}
	};
	// Space: shuts the text and he stays seated; the chooser: up (AHeroCharacter stands him up on true); asleep: nothing
	Hero->SeatLeave = [Self]() -> bool
	{
		ULyudCityThings* Things = Self.Get();
		if (!Things)
		{
			return true;
		}
		if (Things->BookText >= 0)
		{
			Things->bCloseBookAsked = true;
			return false;
		}
		return !Things->bSleeping;
	};
}

void ULyudCityThings::UnhookSeatKeys()
{
	AHeroCharacter* Hero = HookedHero.Get();
	HookedHero.Reset();
	// while he is still on a seat or bed of ours (the hooks of another's seat, a boat's, are not ours to clear), or no longer
	// seated at all (our seat's anchor went from under him: hooks left would catch the next seat's steps)
	if (Hero && (!Hero->IsSeated() || SeatedOn >= 0 || LyingOn >= 0))
	{
		Hero->SeatMove = nullptr;
		Hero->SeatLeave = nullptr;
	}
}

int32 ULyudCityThings::NearestSeat(bool bLie) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return -1;
	}
	const FVector From = Pawn->GetActorLocation();
	int32 Best = -1;
	double BestD = 0.0;
	for (int32 I = 0; I < NumThings(); ++I)
	{
		const LyudCity::FThing& T = Model->Things[(size_t)I];
		const bool bBed = T.Kind == "bed";
		if (bBed != bLie || !LyudCity::FModel::IsSeatThing(T) || Model->Holder(I) != 0)
		{
			continue;
		}
		const double D = FVector::DistSquared(ThingLocation(I), From);
		if (Best < 0 || D < BestD)
		{
			Best = I;
			BestD = D;
		}
	}
	return Best;
}

bool ULyudCityThings::SitOnThing(const FString& Id, bool bLie)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	if (!Model || !Hero)
	{
		return false;
	}
	const int32 Thing = Id.IsEmpty() ? NearestSeat(bLie) : Model->FindThing(std::string(TCHAR_TO_UTF8(*Id)));
	if (Thing < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: %s: no such %s"), Id.IsEmpty() ? TEXT("nearest") : *Id, bLie ? TEXT("bed") : TEXT("seat"));
		return false;
	}
	const LyudCity::FThing& T = Model->Things[(size_t)Thing];
	if (!LyudCity::FModel::IsSeatThing(T) || (bLie && T.Kind != "bed") || Model->Holder(Thing) != 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: %s is not a free %s"), *LyudCityThingsImpl::Str(T.Id), bLie ? TEXT("bed") : TEXT("seat"));
		return false;
	}
	CloseBook();
	EndShow();
	EndGame(false);
	ClosePage();
	EndLie(false);
	if (Hero->IsSeated())
	{
		Hero->StandUp();
		SeatedOn = -1;
		SeatScreen = -1;
		ClearLookCone();
	}
	if (!Sit(Thing, Hero, bLie))
	{
		return false;
	}
	if (bLie)
	{
		BeginLie(Thing, false, false);
		const LyudCity::FAct* A = Model->ActOf(Thing);
		if (A && A->bSleep)
		{
			BeginSleepChoice(Thing, T.Act, FString(), FText::GetEmpty());
		}
	}
	return true;
}

void ULyudCityThings::ResetHeroState()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	CloseBook();
	EndTalk();
	EndLiftChoice();
	EndLift();
	SleepHour = -1;
	SleepThing = -1;
	bSleeping = false;
	EndGame(false);
	EndShow();
	ClosePage();
	if (FadeMinutes > 0 || bFadingIn)
	{
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StopCameraFade();
		}
	}
	FadeMinutes = 0;
	bFadingIn = false;
	JumpLeft = 0.0;
	JumpRate = 0.0;
	UnblockMove();
	EndLie(false);
	if (Hero && Hero->IsSeated() && SeatedOn >= 0)
	{
		// a load while seated: up, where he would stand up to when the save had him on the seat, else where the save had him
		const FVector Loaded = Hero->GetActorLocation();
		Hero->StandUp();
		if (FVector::Dist(Hero->GetActorLocation(), Loaded) > LyudCityThingsImpl::LoadedFar)
		{
			Hero->SetActorLocation(Loaded, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	SeatedOn = -1;
	SeatScreen = -1;
	RecoverFromSeat(Hero);
	ClearLookCone();
	bStandAsked = false;
	bCloseBookAsked = false;
}

// --- sleep: the hour chooser on the page while he lies on a "sleep" bed, then the fade and the clock on to it

void ULyudCityThings::BeginSleepChoice(int32 Thing, int32 Act, const FString& LineKey, const FText& Title)
{
	UWorld* World = GetWorld();
	const LyudCity::FAct* A = Model && Act >= 0 && Act < (int32)Model->Acts.size() ? &Model->Acts[(size_t)Act] : nullptr;
	if (!World || !A || LyingOn != Thing)
	{
		return;
	}
	SleepHour = LyudCity::FModel::SleepDefaultHour(*A, LyudCityThingsImpl::When(World, false).Hour);
	SleepThing = Thing;
	SleepLine = LineKey;
	SleepTitle = Title;
	bSleeping = false;
	PageFrame = GFrameCounter;
	HookSeatKeys(Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)));
	SetPrompted(-1);
	ShowSleepChoice();
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: sleep on %s: chooser at %02d:00"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Thing].Id), SleepHour);
}

void ULyudCityThings::ShowSleepChoice()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || SleepHour < 0)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	PageThing = -1;
	PageLeft = 0;
	const int32 Minutes = LyudCity::FModel::MinutesUntil(LyudCityThingsImpl::When(World, false).Hour, SleepHour);
	const FString Hours = Minutes % 60 == 0 ? FString::FromInt(Minutes / 60) : FString::Printf(TEXT("%.1f"), (double)Minutes / 60.0);
	Page->SetScreen(false);
	Page->SetPage(LyudText::Get(TEXT("city.sleep.title")),
		LyudText::Get(TEXT("city.sleep.body"), { FStringFormatArg(LyudCityThingsImpl::HourText(SleepHour)), FStringFormatArg(Hours) }),
		LyudText::Get(TEXT("city.sleep.keys")));
}

void ULyudCityThings::TickSleepChoice(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !PC || !Hero->IsSeated() || LyingOn < 0 || LyingOn != SleepThing)
	{
		EndSleepChoice(false);
		return;
	}
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((UI && UI->IsOpen()) || (Menu && Menu->IsOpen()) || bTalk || GFrameCounter == PageFrame)
	{
		return;     // the keys are another screen's now; the chooser waits
	}
	if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Enter))
	{
		EndSleepChoice(true);
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::S) || PC->WasInputKeyJustPressed(EKeys::Escape)
		|| PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::Down))
	{
		EndSleepChoice(false);
		return;
	}
	const int32 Delta = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right)) ? 1
		: ((PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left)) ? -1 : 0);
	if (Delta != 0)
	{
		SleepHour = LyudCity::FModel::SleepStep(SleepHour, Delta, LyudCityThingsImpl::When(World, false).Hour);
		ShowSleepChoice();
	}
}

void ULyudCityThings::EndSleepChoice(bool bSleep)
{
	if (SleepHour < 0)
	{
		return;
	}
	const int32 Hour = SleepHour;
	SleepHour = -1;
	if (Page && PageThing < 0)
	{
		ClosePage();
	}
	UWorld* World = GetWorld();
	if (!bSleep || !World || LyingOn < 0)
	{
		SleepThing = -1;
		bSleeping = false;
		EndLie(true);
		return;
	}
	int32 Minutes = LyudCity::FModel::MinutesUntil(LyudCityThingsImpl::When(World, false).Hour, Hour);
	if (Minutes >= 24 * 60 - 1)
	{
		Minutes = 1;     // the clock reached the hour while he chose: he only closes his eyes
	}
	bSleeping = true;
	bLieAutoStand = true;
	LieUntil = 0.0;
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: sleeps until %02d:00 (%d min)"), Hour, Minutes);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && PC->PlayerCameraManager)
	{
		// the screen goes black, the clock jumps (Tick: the people's life resettles), back, a moment, up
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.9f, FLinearColor::Black, false, true);
		FadeMinutes = Minutes;
		FadeAt = World->GetTimeSeconds() + 1.0;
		bFadingIn = false;
	}
	else
	{
		JumpMinutes(Minutes, false);
		LieUntil = World->GetTimeSeconds() + LyudCityThingsImpl::JumpSeconds + 2.5;
	}
}

bool ULyudCityThings::SleepUntil(int32 Hour)
{
	if (LyingOn < 0 || Hour < 0 || Hour > 23 || bSleeping)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: sleep %d: not lying on a bed (or already asleep, or no such hour)"), Hour);
		return false;
	}
	if (SleepHour < 0)
	{
		SleepThing = LyingOn;
		SleepLine.Reset();
		SleepTitle = FText::GetEmpty();
		HookSeatKeys(Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)));
	}
	SleepHour = Hour;
	EndSleepChoice(true);
	return true;
}

// --- the reading view (docs/city_interiors.md "The reading page")

void ULyudCityThings::OpenBook(int32 Text, int32 Thing, bool bSeated)
{
	UWorld* World = GetWorld();
	const LyudCity::FReadText* T = Model ? Model->TextAt(Text) : nullptr;
	if (!World || !T)
	{
		return;
	}
	CloseBook();
	EndShow();
	ClosePage();
	std::vector<std::string> Pages;
	Model->TextPages(Text, LyudCityThingsImpl::When(World), Pages);
	if (Pages.empty())
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: text %s has no page to show now"), *LyudCityThingsImpl::Str(T->Id));
		return;
	}
	BookPages.Reset((int32)Pages.size());
	for (const std::string& Key : Pages)
	{
		BookPages.Add(LyudCityThingsImpl::Str(Key));
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	BookText = Text;
	BookThing = Thing;
	bBookSeated = bSeated && Hero && Hero->IsSeated();
	bCloseBookAsked = false;
	BookFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
	BookFrame = GFrameCounter;
	const int32 Where = Thing >= 0 ? Thing : PlotThing;
	BookPlot = Where >= 0 && Where < NumThings() ? LyudCityThingsImpl::Str(Model->Things[(size_t)Where].Plot) : FString();
	// the view: made once, kept in the viewport (collapsed while no text is open)
	if (!Book.IsValid())
	{
		Book = SNew(SLyudCityBook);
	}
	if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && !bBookAdded)
	{
		Viewport->AddViewportWidgetContent(Book.ToSharedRef(), 62);
		bBookAdded = true;
	}
	const FString Kind = LyudCityThingsImpl::Str(T->Kind);
	Book->SetKind(Kind, LyudText::Get(FString(TEXT("city.book.kind.")) + Kind));
	Book->SetVisibility(EVisibility::HitTestInvisible);
	// seated, the seat's keys are the view's (a step must not stand him up mid-page); standing, his steps are held
	if (bBookSeated)
	{
		HookSeatKeys(Hero);
	}
	else if (PC && !BookBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		BookBlockedBy = PC;
	}
	SetPrompted(-1);
	// where it was left (a turn of 0: remembered; a one-page text is read at once)
	const int32 Num = BookPages.Num();
	const LyudCity::FPageTurn Turn = Model->TurnPage(Text, Model->OpenPage(Text, Num), 0, Num, std::string(TCHAR_TO_UTF8(*BookPlot)));
	BookPage = Turn.Page;
	ShowBookPage();
	JumpMinutes(T->Minutes, false);
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("diary_page"), Thing >= 0 ? ThingLocation(Thing) : BookFrom);
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: reads %s (%s, %d pages) at page %d"), *LyudCityThingsImpl::Str(T->Id), *Kind, Num, BookPage + 1);
	BookTurned(Turn);
}

void ULyudCityThings::ShowBookPage()
{
	const LyudCity::FReadText* T = Model ? Model->TextAt(BookText) : nullptr;
	const int32 Num = BookPages.Num();
	if (!Book.IsValid() || !T || Num == 0)
	{
		return;
	}
	BookPage = FMath::Clamp(BookPage, 0, Num - 1);
	const bool bLast = BookPage >= Num - 1;
	Book->SetPage(LyudText::Get(LyudCityThingsImpl::Str(T->Title)), LineText(BookPages[BookPage]),
		LyudText::Get(TEXT("city.book.page"), { FStringFormatArg(BookPage + 1), FStringFormatArg(Num) }),
		LyudText::Get(bLast ? TEXT("city.book.keys_last") : TEXT("city.book.keys")));
}

void ULyudCityThings::BookTurned(const LyudCity::FPageTurn& Turn)
{
	if (!Turn.NewStamps.empty())
	{
		TArray<FString> Fresh;
		for (const std::string& Stamp : Turn.NewStamps)
		{
			Fresh.Add(LyudCityThingsImpl::Str(Stamp));
		}
		AnnounceStamps(Fresh);
	}
	const LyudCity::FReadText* T = Model ? Model->TextAt(BookText) : nullptr;
	if (!Turn.bFinished || !T)
	{
		return;
	}
	const FText Title = LyudText::Get(LyudCityThingsImpl::Str(T->Title));
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: read %s to the end (at %s)"), *LyudCityThingsImpl::Str(T->Id), BookPlot.IsEmpty() ? TEXT("-") : *BookPlot);
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(LyudText::Get(TEXT("city.book.read"), { FStringFormatArg(Title.ToString()) }));
	}
	ULyudQuests::Notify(this, TEXT("read"), LyudCityThingsImpl::Str(T->Id));
}

void ULyudCityThings::TurnBook(int32 Delta)
{
	const LyudCity::FReadText* T = Model ? Model->TextAt(BookText) : nullptr;
	const int32 Num = BookPages.Num();
	if (!T || Num == 0)
	{
		return;
	}
	if (Delta > 0 && BookPage >= Num - 1)
	{
		CloseBook();     // E on the last page puts it down
		return;
	}
	const LyudCity::FPageTurn Turn = Model->TurnPage(BookText, BookPage, Delta > 0 ? 1 : -1, Num, std::string(TCHAR_TO_UTF8(*BookPlot)));
	if (!Turn.bMoved)
	{
		return;
	}
	BookPage = Turn.Page;
	ShowBookPage();
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("diary_page"), BookThing >= 0 ? ThingLocation(BookThing) : BookFrom);
	}
	if (Delta > 0)
	{
		JumpMinutes(T->Minutes, false);     // a page read; leafing back costs nothing
	}
	BookTurned(Turn);
}

void ULyudCityThings::CloseBook()
{
	if (APlayerController* PC = BookBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	BookBlockedBy.Reset();
	bCloseBookAsked = false;
	if (BookText < 0)
	{
		return;
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: text put down at page %d"), BookPage + 1);
	BookText = -1;
	BookThing = -1;
	BookPages.Reset();
	bBookSeated = false;
	if (Book.IsValid())
	{
		Book->SetVisibility(EVisibility::Collapsed);
	}
}

void ULyudCityThings::TickBook(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const ULyudGameMenuSubsystem* Menu = World ? World->GetSubsystem<ULyudGameMenuSubsystem>() : nullptr;
	// shut by a step or Space on the seat, getting up, walking off, another screen, not being the hero
	if (!World || !Hero || !PC || !UI || UI->IsOpen() || (Menu && Menu->IsOpen()) || bCloseBookAsked
		|| (bBookSeated && !Hero->IsSeated())
		|| (!bBookSeated && FVector::Dist(Hero->GetActorLocation(), BookFrom) > LyudCityThingsImpl::PageWalkAway))
	{
		CloseBook();
		return;
	}
	if (bTalk || GFrameCounter == BookFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::S)
		|| PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::Down) || PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		CloseBook();
	}
	else if (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right))
	{
		TurnBook(1);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left))
	{
		TurnBook(-1);
	}
}

bool ULyudCityThings::OpenText(const FString& TextId)
{
	const int32 Text = Model ? Model->FindText(std::string(TCHAR_TO_UTF8(*TextId))) : -1;
	if (Text < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no text %s%s"), *TextId, Loading.IsValid() ? TEXT(" (City/interiors.json is still being read)") : TEXT(""));
		return false;
	}
	const AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	EndGame(false);
	OpenBook(Text, -1, Hero && Hero->IsSeated() && SeatedOn >= 0);
	return IsReading();
}

bool ULyudCityThings::OpenNearestText()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return false;
	}
	const FVector From = Pawn->GetActorLocation();
	int32 Best = -1;
	double BestD = 0.0;
	for (int32 I = 0; I < NumThings(); ++I)
	{
		const LyudCity::FAct* A = Model->ActOf(I);
		const double D = A && !A->Texts.empty() ? FVector::DistSquared(ThingLocation(I), From) : -1.0;
		if (D >= 0.0 && (Best < 0 || D < BestD))
		{
			Best = I;
			BestD = D;
		}
	}
	if (Best < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no thing to read anywhere"));
		return false;
	}
	Run(Best, true);
	return true;
}

FString ULyudCityThings::PlotName(const FString& Plot) const
{
	if (Plot.IsEmpty())
	{
		return LyudText::Get(TEXT("city.read.list.somewhere")).ToString();
	}
	const FString Key = Plot.ToLower().Replace(TEXT("-"), TEXT("_"));
	// LyudText::Get gives the key back when it has no words for it
	const FString NameKey = FString(TEXT("city.plot.")) + Key + TEXT(".name");
	FString Words = LyudText::Get(NameKey).ToString();
	if (!Words.IsEmpty() && Words != NameKey)
	{
		return Words;
	}
	const FString TpKey = FString(TEXT("dev.tp.city.")) + Key;
	Words = LyudText::Get(TpKey).ToString();
	return !Words.IsEmpty() && Words != TpKey ? Words : Plot;
}

FString ULyudCityThings::ReadListBody() const
{
	if (!Model || Model->Texts.empty())
	{
		return FString();
	}
	const int32 Got = Model->CountRead();
	FString Body = LyudText::Get(TEXT("city.read.list.head"), { FStringFormatArg(Got), FStringFormatArg((int32)Model->Texts.size()) }).ToString();
	if (Got == 0)
	{
		return Body + TEXT("\n") + LyudText::Get(TEXT("city.read.list.none")).ToString();
	}
	// per place, the places and titles in the order the texts are declared
	const FString Sep = LyudText::Get(TEXT("city.read.list.sep")).ToString();
	const std::map<std::string, std::string>& ReadAt = Model->GetReadAt();
	TArray<FString> Plots;
	TArray<FString> Titles;
	for (const LyudCity::FReadText& T : Model->Texts)
	{
		const std::map<std::string, std::string>::const_iterator It = ReadAt.find(T.Id);
		if (It == ReadAt.end())
		{
			continue;
		}
		const FString Plot = LyudCityThingsImpl::Str(It->second);
		int32 Row = Plots.Find(Plot);
		if (Row == INDEX_NONE)
		{
			Row = Plots.Add(Plot);
			Titles.AddDefaulted();
		}
		Titles[Row] += (Titles[Row].IsEmpty() ? FString() : Sep) + LyudText::Get(LyudCityThingsImpl::Str(T.Title)).ToString();
	}
	for (int32 Row = 0; Row < Plots.Num(); ++Row)
	{
		Body += TEXT("\n") + LyudText::Get(TEXT("city.read.list.line"), { FStringFormatArg(PlotName(Plots[Row])), FStringFormatArg(Titles[Row]) }).ToString();
	}
	return Body;
}

void ULyudCityThings::ShowReadList(bool bPage)
{
	const FString Body = ReadListBody();
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: read list:\n%s"), Body.IsEmpty() ? TEXT("(no texts)") : *Body);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!bPage || Body.IsEmpty() || !Pawn || NumThings() == 0 || ShowAt >= 0 || BookText >= 0 || SleepHour >= 0 || IsPlayingGame())
	{
		return;
	}
	PageThing = -1;
	PageLeft = 0;
	PageTitle = LyudText::Get(TEXT("city.coll.reading.title"));
	PageFrom = Pawn->GetActorLocation();
	PageFrame = GFrameCounter;
	ShowPage(PageTitle, FText::FromString(Body), false);
	SetPrompted(-1);
}

void ULyudCityThings::SetAllRead(bool bRead)
{
	if (!Model)
	{
		return;
	}
	const std::string Plot = PlotThing >= 0 && PlotThing < NumThings() ? Model->Things[(size_t)PlotThing].Plot : std::string();
	Model->MarkAllRead(bRead, Plot);
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: every text %s (%d of %d read)"), bRead ? TEXT("read") : TEXT("unread"), Model->CountRead(), (int32)Model->Texts.size());
}

bool ULyudCityThings::AreAllRead() const
{
	return Model && Model->AllRead();
}

// --- console

// --- lifts and storey links (docs/city_interiors.md "Lifts and storey links")

bool ULyudCityThings::FindLiftSpot(int32 Dest, const AHeroCharacter* Hero, FVector& OutMiddle, float& OutYaw) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Hero ? Hero->GetCapsuleComponent() : nullptr;
	if (!World || !Model || !Capsule)
	{
		return false;
	}
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CityLift), false, Hero);
	LyudCity::FVec Spots[8];
	float Yaw = 0.f;
	const int32 Num = Model->LiftSpots(Dest, Spots, 8, Yaw);
	for (int32 I = 0; I < Num; ++I)
	{
		const LyudCity::FVec& S = Spots[I];
		// the floor under the spot, about the door's (not the shaft below, not the roof over a gallery)...
		FHitResult Hit;
		const FVector Top(S.X, S.Y, S.Z + LyudCity::FModel::LiftFloorAbove + 10.0);
		const FVector Bottom(S.X, S.Y, S.Z - LyudCity::FModel::LiftFloorBelow - 10.0);
		if (!World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility, Q) || !LyudCity::FModel::LiftFloorOk(S.Z, Hit.ImpactPoint.Z))
		{
			continue;
		}
		// ...and his capsule fits standing on it (never inside a wall, a counter or the lift's core)
		const FVector Middle(S.X, S.Y, Hit.ImpactPoint.Z + HalfHeight + 2.0);
		if (World->OverlapBlockingTestByChannel(Middle, FQuat::Identity, ECC_Pawn, Shape, Q))
		{
			continue;
		}
		OutMiddle = Middle;
		OutYaw = Yaw;
		return true;
	}
	return false;
}

bool ULyudCityThings::BeginLift(int32 Dest)
{
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (!World || !Model || !Hero || !PC || Dest < 0 || Dest >= NumThings() || LiftDest >= 0 || Hero->IsSeated())
	{
		return false;
	}
	FVector Middle;
	float Yaw = 0.f;
	if (!FindLiftSpot(Dest, Hero, Middle, Yaw))
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: lift to %s: no spot fits in front of it"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Dest].Id));
		Refuse(LyudText::Get(TEXT("city.lift.blocked")));
		return false;
	}
	EndLiftChoice();
	EndTalk();
	ClosePage();
	LiftDest = Dest;
	LiftMiddle = Middle;
	LiftYaw = Yaw;
	bLiftMoved = false;
	if (!LiftBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		LiftBlockedBy = PC;
	}
	const double Now = World->GetTimeSeconds();
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, LyudCityThingsImpl::LiftFadeOut, FLinearColor::Black, false, true);
		LiftMoveAt = Now + LyudCityThingsImpl::LiftFadeOut + 0.05;
	}
	else
	{
		LiftMoveAt = Now;
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: lift to %s"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Dest].Id));
	return true;
}

void ULyudCityThings::TickLift(AHeroCharacter* Hero, APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !PC)
	{
		EndLift();
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now < LiftMoveAt)
	{
		return;
	}
	if (!bLiftMoved)
	{
		// the screen is black: there, facing away from the door, standing still; a few game seconds; the plot asked again now
		Hero->SetActorLocationAndRotation(LiftMiddle, FRotator(0.0, (double)LiftYaw, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(FRotator(0.0, (double)LiftYaw, 0.0));
		if (UCharacterMovementComponent* Move = Hero->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
		JumpLeft += LyudCityThingsImpl::LiftSeconds;
		JumpRate = FMath::Max(JumpRate, JumpLeft / LyudCityThingsImpl::JumpSeconds);
		bLiftMoved = true;
		PickClock = 0.f;
		LiftMoveAt = Now + LyudCityThingsImpl::LiftHold;
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: lift arrived at %s (%.0f %.0f %.0f)"), Model && LiftDest >= 0 && LiftDest < NumThings()
			? *LyudCityThingsImpl::Str(Model->Things[(size_t)LiftDest].Id) : TEXT("?"), LiftMiddle.X, LiftMiddle.Y, LiftMiddle.Z);
		return;
	}
	EndLift();
}

void ULyudCityThings::EndLift()
{
	APlayerController* PC = LiftBlockedBy.Get();
	if (PC)
	{
		PC->SetIgnoreMoveInput(false);
	}
	LiftBlockedBy.Reset();
	if (LiftDest < 0)
	{
		return;
	}
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	}
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, LyudCityThingsImpl::LiftFadeIn, FLinearColor::Black, false, false);
	}
	LiftDest = -1;
	bLiftMoved = false;
}

void ULyudCityThings::BeginLiftChoice(int32 Thing)
{
	UWorld* World = GetWorld();
	if (!World || !Model || !Model->IsLift(Thing))
	{
		return;
	}
	LiftThing = Thing;
	LiftSel = 0;
	LiftFrame = GFrameCounter;
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC && !LiftBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		LiftBlockedBy = PC;
	}
	SetPrompted(-1);
	ShowLiftChoice();
}

void ULyudCityThings::ShowLiftChoice()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !Model || !Model->IsLift(LiftThing))
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	PageThing = -1;
	PageLeft = 0;
	const std::vector<LyudCity::FFloor>& Floors = Model->Things[(size_t)LiftThing].Floors;
	FString Body;
	for (int32 I = 0; I < (int32)Floors.size(); ++I)
	{
		const LyudCity::FFloor& F = Floors[(size_t)I];
		const FString Caption = F.Key.empty() ? LyudCityThingsImpl::Str(Model->Things[(size_t)F.To].Id) : LyudText::Get(LyudCityThingsImpl::Str(F.Key)).ToString();
		Body += I == LiftSel ? TEXT("▸ ") : TEXT("   ");
		Body += LyudText::Get(TEXT("city.lift.entry"), { FStringFormatArg(I + 1), FStringFormatArg(Caption) }).ToString();
		Body += TEXT("\n");
	}
	Page->SetScreen(false);
	Page->SetPage(LyudText::Get(TEXT("city.lift.title")), FText::FromString(Body), LyudText::Get(TEXT("city.lift.keys")));
}

void ULyudCityThings::TickLiftChoice(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !Model || !Hero || !PC || !Page || !Model->IsLift(LiftThing) || (UI && UI->IsOpen()))
	{
		EndLiftChoice();
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || bTalk || GFrameCounter == LiftFrame)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::S)
		|| PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		EndLiftChoice();
		return;
	}
	const std::vector<LyudCity::FFloor>& Floors = Model->Things[(size_t)LiftThing].Floors;
	const int32 Num = (int32)Floors.size();
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	int32 Chosen = -1;
	for (int32 I = 0; I < Num && I < (int32)UE_ARRAY_COUNT(Digits); ++I)
	{
		if (PC->WasInputKeyJustPressed(Digits[I]))
		{
			Chosen = I;
		}
	}
	if (Chosen < 0 && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Enter)))
	{
		Chosen = LiftSel;
	}
	if (Chosen >= 0 && Chosen < Num)
	{
		const int32 Dest = Floors[(size_t)Chosen].To;
		EndLiftChoice();
		BeginLift(Dest);
		return;
	}
	const int32 Delta = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::Down)) ? 1
		: ((PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::Up)) ? -1 : 0);
	if (Delta != 0 && Num > 0)
	{
		LiftSel = (LiftSel + Delta + Num) % Num;
		ShowLiftChoice();
	}
}

void ULyudCityThings::EndLiftChoice()
{
	if (LiftThing < 0)
	{
		return;
	}
	LiftThing = -1;
	LiftSel = 0;
	if (LiftDest < 0)
	{
		if (APlayerController* PC = LiftBlockedBy.Get())
		{
			PC->SetIgnoreMoveInput(false);
		}
		LiftBlockedBy.Reset();
	}
	if (Page && PageThing < 0)
	{
		ClosePage();
	}
}

bool ULyudCityThings::GoToThing(const FString& ThingId)
{
	const int32 Thing = Model ? Model->FindThing(std::string(TCHAR_TO_UTF8(*ThingId))) : -1;
	if (Thing < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: go: no thing %s"), *ThingId);
		return false;
	}
	return BeginLift(Thing);
}

bool ULyudCityThings::UseNearestLift()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return false;
	}
	for (const std::pair<int, double>& P : Model->Nearest(LyudCityThingsImpl::Vec(Pawn->GetActorLocation()), 256))
	{
		if (Model->IsLift(P.first))
		{
			const LyudCity::FThing& Door = Model->Things[(size_t)P.first];
			if (Door.Floors.size() == 1)
			{
				return BeginLift(Door.Floors[0].To);
			}
			BeginLiftChoice(P.first);
			return true;
		}
	}
	UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no lift door near"));
	return false;
}

// --- the staff talk (docs/city_interiors.md "Staff talk"): a chooser on the page, the answers one per E

bool ULyudCityThings::OpenShopOf(const FString& Shop, const FString& TitleKey)
{
	const std::vector<LyudCity::FGood>* Stock = Model ? Model->FindShop(std::string(TCHAR_TO_UTF8(*Shop))) : nullptr;
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
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	if (!UI || Goods.Num() == 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: shop %s has nothing to sell"), *Shop);
		return false;
	}
	SetPrompted(-1);
	ClosePage();
	UI->OpenShop(LyudText::Get(TitleKey.IsEmpty() ? FString(TEXT("city.shop.title")) : TitleKey), Goods);
	return true;
}

FText ULyudCityThings::TalkSpeaker(int32 Thing) const
{
	const UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this);
	if (People && Model && Thing >= 0 && Thing < NumThings())
	{
		const std::string Role = Model->TalkRole(Thing);
		const FName Plot = LyudCityThingsImpl::Name(Model->Things[(size_t)Thing].Plot);
		const FVector At = ThingLocation(Thing);
		// the one on shift nearest the counter (somebody not drawn yet counts as far)
		int32 Best = -1;
		double BestDist = TNumericLimits<double>::Max();
		for (const int32 Person : People->StaffOnShift(Plot, Role == "*" ? FName() : LyudCityThingsImpl::Name(Role)))
		{
			FPersonInfo Info;
			const AActor* Body = People->GetPerson(Person, Info) ? Info.Actor.Get() : nullptr;
			const double BodyDist = IsValid(Body) ? FVector::Dist(Body->GetActorLocation(), At) : 1.0e12;
			if (Best < 0 || BodyDist < BestDist)
			{
				Best = Person;
				BestDist = BodyDist;
			}
		}
		const FText Speaker = Best >= 0 ? People->GetDisplayName(Best) : FText::GetEmpty();
		if (!Speaker.IsEmpty())
		{
			return Speaker;
		}
	}
	return LyudText::Get(TEXT("city.talk.speaker"));
}

void ULyudCityThings::BeginTalk(int32 Thing, const TArray<int32>& Topics, const FText& Title, const FText& Opener, const FString& Shop,
	const FString& ShopTitle, bool bAnywhere)
{
	UWorld* World = GetWorld();
	if (!World || !Model || Thing < 0 || Thing >= NumThings() || Topics.Num() == 0)
	{
		return;
	}
	ClosePage();
	TalkThing = Thing;
	TalkTopics = Topics;
	TalkSel = 0;
	// "Buy..." only when the shop has something the item table knows
	TalkShop.Reset();
	TalkShopTitle = ShopTitle;
	if (const std::vector<LyudCity::FGood>* Stock = Shop.IsEmpty() ? nullptr : Model->FindShop(std::string(TCHAR_TO_UTF8(*Shop))))
	{
		for (const LyudCity::FGood& G : *Stock)
		{
			if (ULyudInventory::FindDef(LyudCityThingsImpl::Name(G.Item)))
			{
				TalkShop = Shop;
				break;
			}
		}
	}
	TalkTitle = Title;
	TalkOpener = Opener;
	TalkAnswer.Reset();
	TalkAnswerAt = 0;
	bTalkAnywhere = bAnywhere;
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	TalkFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
	TalkFrame = GFrameCounter;
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	if (PC && !Hero->IsSeated() && !TalkBlockedBy.IsValid())
	{
		PC->SetIgnoreMoveInput(true);
		TalkBlockedBy = PC;
	}
	SetPrompted(-1);
	ShowTalk();
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: talk at %s: %d topics%s, speaker %s"), *LyudCityThingsImpl::Str(Model->Things[(size_t)Thing].Id),
		TalkTopics.Num(), TalkShop.IsEmpty() ? TEXT("") : TEXT(" and the shop"), *TalkTitle.ToString());
}

void ULyudCityThings::ShowTalk()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !Model || TalkThing < 0)
	{
		return;
	}
	if (!Page)
	{
		Page = SNew(SLyudCityPage);
		Viewport->AddViewportWidgetContent(Page.ToSharedRef(), 61);
	}
	PageThing = -1;
	PageLeft = 0;
	const int32 ShopEntries = TalkShop.IsEmpty() ? 0 : 1;
	const int32 Entries = ShopEntries + TalkTopics.Num();
	FString Body = TalkOpener.ToString();
	if (!Body.IsEmpty())
	{
		Body += TEXT("\n\n");
	}
	for (int32 I = 0; I < Entries; ++I)
	{
		const LyudCity::FTopic* T = I < ShopEntries ? nullptr : Model->TopicAt(TalkTopics[I - ShopEntries]);
		const FString EntryName = T ? LyudText::Get(LyudCityThingsImpl::Str(T->Title)).ToString() : LyudText::Get(TEXT("city.talk.buy")).ToString();
		const bool bNew = T && !Model->IsHeard(T->Id);
		Body += I == TalkSel ? TEXT("\u25B8 ") : TEXT("   ");
		Body += LyudText::Get(bNew ? TEXT("city.talk.entry_new") : TEXT("city.talk.entry"), { FStringFormatArg(I + 1), FStringFormatArg(EntryName) }).ToString();
		Body += TEXT("\n");
	}
	Page->SetScreen(false);
	Page->SetPage(TalkTitle, FText::FromString(Body), LyudText::Get(TEXT("city.talk.keys")));
}

void ULyudCityThings::ShowTalkAnswer()
{
	if (!Page || !TalkAnswer.IsValidIndex(TalkAnswerAt))
	{
		return;
	}
	Page->SetScreen(false);
	Page->SetPage(TalkTitle, LineText(TalkAnswer[TalkAnswerAt]),
		LyudText::Get(TalkAnswerAt + 1 < TalkAnswer.Num() ? TEXT("city.talk.keys_answer") : TEXT("city.talk.keys_back")));
}

void ULyudCityThings::ChooseTalk(int32 Entry)
{
	UWorld* World = GetWorld();
	if (!World || !Model || TalkThing < 0)
	{
		return;
	}
	const int32 ShopEntries = TalkShop.IsEmpty() ? 0 : 1;
	if (Entry < ShopEntries)
	{
		const FString Shop = TalkShop;
		const FString Title = TalkShopTitle;
		EndTalk();
		OpenShopOf(Shop, Title);
		return;
	}
	if (!TalkTopics.IsValidIndex(Entry - ShopEntries))
	{
		return;
	}
	TalkSel = Entry;
	const LyudCity::FTopicAnswer A = Model->AskTopic(TalkTopics[Entry - ShopEntries], LyudCityThingsImpl::When(World));
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: asked %s: %d lines%s"), *LyudCityThingsImpl::Str(A.Target), (int32)A.Lines.size(),
		A.bFirst ? TEXT(" (first time)") : TEXT(""));
	if (!A.NewStamps.empty())
	{
		TArray<FString> Fresh;
		for (const std::string& Stamp : A.NewStamps)
		{
			Fresh.Add(LyudCityThingsImpl::Str(Stamp));
		}
		AnnounceStamps(Fresh);
	}
	ULyudQuests::Notify(this, LyudCityThingsImpl::Name(A.Event), LyudCityThingsImpl::Str(A.Target));
	TalkAnswer.Reset();
	for (const std::string& Line : A.Lines)
	{
		TalkAnswer.Add(LyudCityThingsImpl::Str(Line));
	}
	TalkAnswerAt = 0;
	if (TalkAnswer.Num() > 0)
	{
		ShowTalkAnswer();
	}
	else
	{
		ShowTalk();
	}
}

void ULyudCityThings::TickTalk(AHeroCharacter* Hero, APlayerController* PC, bool bTalk)
{
	UWorld* World = GetWorld();
	const ULyudInventoryUI* UI = World ? ULyudInventoryUI::Get(World) : nullptr;
	if (!World || !Model || !Hero || !PC || !Page || (UI && UI->IsOpen())
		|| (!bTalkAnywhere && FVector::Dist(Hero->GetActorLocation(), TalkFrom) > LyudCityThingsImpl::PageWalkAway))
	{
		EndTalk();
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if ((Menu && Menu->IsOpen()) || bTalk || GFrameCounter == TalkFrame)
	{
		return;     // the keys are another screen's now; the talk waits
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::S)
		|| PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		EndTalk();
		return;
	}
	const bool bE = PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Enter);
	if (TalkAnswer.Num() > 0)
	{
		if (!bE)
		{
			return;
		}
		if (++TalkAnswerAt < TalkAnswer.Num())
		{
			ShowTalkAnswer();
			return;
		}
		// the answer is over: back to the chooser with the topics now (a once topic heard is gone, a heard one may open another)
		TalkAnswer.Reset();
		TalkAnswerAt = 0;
		std::vector<int> Offer;
		Model->TopicsNow(TalkThing, LyudCityThingsImpl::When(World), Offer, bOpenAllHours || bTalkAnywhere);
		TalkTopics.Reset();
		for (const int Topic : Offer)
		{
			TalkTopics.Add(Topic);
		}
		const int32 Entries = (TalkShop.IsEmpty() ? 0 : 1) + TalkTopics.Num();
		if (Entries == 0)
		{
			EndTalk();
			return;
		}
		TalkSel = FMath::Clamp(TalkSel, 0, Entries - 1);
		ShowTalk();
		return;
	}
	const int32 Entries = (TalkShop.IsEmpty() ? 0 : 1) + TalkTopics.Num();
	if (Entries <= 0)
	{
		EndTalk();
		return;
	}
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 I = 0; I < Entries && I < (int32)UE_ARRAY_COUNT(Digits); ++I)
	{
		if (PC->WasInputKeyJustPressed(Digits[I]))
		{
			ChooseTalk(I);
			return;
		}
	}
	const int32 Delta = (PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::Down)) ? 1
		: ((PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::Up)) ? -1 : 0);
	if (Delta != 0)
	{
		TalkSel = (TalkSel + Delta + Entries) % Entries;
		ShowTalk();
		return;
	}
	if (bE)
	{
		ChooseTalk(TalkSel);
	}
}

void ULyudCityThings::EndTalk()
{
	if (APlayerController* PC = TalkBlockedBy.Get())
	{
		PC->SetIgnoreMoveInput(false);
	}
	TalkBlockedBy.Reset();
	if (TalkThing < 0)
	{
		return;
	}
	TalkThing = -1;
	TalkTopics.Reset();
	TalkAnswer.Reset();
	TalkAnswerAt = 0;
	TalkSel = 0;
	bTalkAnywhere = false;
	if (Page && PageThing < 0)
	{
		ClosePage();
	}
}

bool ULyudCityThings::AskAt(const FString& ThingId, const FString& TopicId)
{
	UWorld* World = GetWorld();
	const int32 Thing = Model ? Model->FindThing(std::string(TCHAR_TO_UTF8(*ThingId))) : -1;
	if (!World || Thing < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: ask: no thing %s"), *ThingId);
		return false;
	}
	if (BookText >= 0 || SleepHour >= 0 || IsPlayingGame() || ShowAt >= 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: ask: a text, a game, a show or the sleep chooser is up"));
		return false;
	}
	// the console: whatever the hour and whoever is on shift
	std::vector<int> Offer;
	Model->TopicsNow(Thing, LyudCityThingsImpl::When(World), Offer, true);
	if (Offer.empty())
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: ask: %s offers no topics now"), *ThingId);
		return false;
	}
	TArray<int32> Topics;
	for (const int Topic : Offer)
	{
		Topics.Add(Topic);
	}
	const LyudCity::FAct* A = Model->ActOf(Thing);
	EndTalk();
	ClosePage();
	BeginTalk(Thing, Topics, TalkSpeaker(Thing), FText::GetEmpty(), A ? LyudCityThingsImpl::Str(A->Shop) : FString(),
		A ? LyudCityThingsImpl::Str(A->Title) : FString(), true);
	if (TopicId.IsEmpty())
	{
		return true;
	}
	const int32 Topic = Model->FindTopic(std::string(TCHAR_TO_UTF8(*TopicId)));
	const int32 At = Topics.Find(Topic);
	if (Topic < 0 || At == INDEX_NONE)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: ask: %s does not offer %s now"), *ThingId, *TopicId);
		return false;
	}
	ChooseTalk(At + (TalkShop.IsEmpty() ? 0 : 1));
	return true;
}

bool ULyudCityThings::AskNearest()
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Model || !Pawn)
	{
		return false;
	}
	for (const std::pair<int, double>& P : Model->Nearest(LyudCityThingsImpl::Vec(Pawn->GetActorLocation()), 64))
	{
		if (Model->HasTopics(P.first))
		{
			return AskAt(LyudCityThingsImpl::Str(Model->Things[(size_t)P.first].Id), FString());
		}
	}
	UE_LOG(LogLyudCity, Warning, TEXT("CityThings: ask: no thing with topics near"));
	return false;
}

void ULyudCityThings::ShowHeardTopics(bool bPage)
{
	if (!Model)
	{
		return;
	}
	const int32 Heard = (int32)Model->GetHeard().size();
	const int32 All = (int32)Model->Topics.size();
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: topics heard %d of %d"), Heard, All);
	for (const std::string& Id : Model->GetHeard())
	{
		UE_LOG(LogLyudCity, Display, TEXT("  %s"), *LyudCityThingsImpl::Str(Id));
	}
	UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!bPage || !Pawn || NumThings() == 0 || ShowAt >= 0 || BookText >= 0 || SleepHour >= 0 || TalkThing >= 0)
	{
		return;
	}
	PageThing = -1;
	PageLeft = 0;
	PageTitle = LyudText::Get(TEXT("city.talk.heard.title"));
	PageFrom = Pawn->GetActorLocation();
	PageFrame = GFrameCounter;
	ShowPage(PageTitle, LyudText::Get(TEXT("city.talk.heard.body"), { FStringFormatArg(Heard), FStringFormatArg(All) }), false);
	SetPrompted(-1);
}

void ULyudCityThings::ForgetTopics()
{
	if (!Model)
	{
		return;
	}
	Model->ForgetTopics();
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: every topic is new again"));
	Note(LyudText::Get(TEXT("city.talk.heard.reset")));
}

// --- room tones (docs/city_interiors.md "Room tones"): one pooled 2D loop and sparse one-shots by the plot round the hero

float ULyudCityThings::ToneLevel(float Volume) const
{
	const ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	if (!Sound)
	{
		return Volume;
	}
	const float Keep = FMath::Lerp(1.f, LyudCityThingsImpl::ToneOutdoorKeep, FMath::Clamp(Sound->GetOutdoors(), 0.f, 1.f));
	return FMath::Clamp(Volume * Sound->MasterVolume * Sound->AmbienceVolume * Keep, 0.f, 1.f);
}

void ULyudCityThings::SetTone(int32 Tone)
{
	UWorld* World = GetWorld();
	const LyudCity::FTone* T = Model ? Model->ToneAt(Tone) : nullptr;
	USoundBase* Loop = T && ToneSounds.IsValidIndex(Tone) ? ToneSounds[Tone].Get() : nullptr;
	const USoundBase* PlayingLoop = ToneAudio ? ToneAudio->Sound.Get() : nullptr;
	UE_LOG(LogLyudCity, Verbose, TEXT("CityThings: tone %s"), T ? *LyudCityThingsImpl::Str(T->Id) : TEXT("off"));
	ToneHeard = T ? Tone : -1;
	{
		LyudCity::FRng Rng;
		Rng.State = ToneDice;
		ToneShotIn = T ? Model->NextShotIn(Tone, Rng) : -1.0;
		ToneDice = Rng.State;
	}
	if (!Loop)
	{
		if (ToneAudio && ToneAudio->IsPlaying())
		{
			ToneAudio->FadeOut(LyudCityThingsImpl::ToneFadeOut, 0.f);
		}
		ToneLevelSet = -1.f;
		return;
	}
	const float Level = ToneLevel(T->Volume);
	if (!ToneAudio && World)
	{
		// made once and kept (never destroyed by itself): the next tones reuse it
		ToneAudio = UGameplayStatics::CreateSound2D(World, Loop, 1.f, 1.f, 0.f, nullptr, false, false);
	}
	if (!ToneAudio)
	{
		return;
	}
	if (PlayingLoop == Loop && ToneAudio->IsPlaying())
	{
		// the same loop in the next room (a cafe after a cafe): only the level moves
		ToneAudio->AdjustVolume(LyudCityThingsImpl::ToneFadeIn, Level);
	}
	else
	{
		ToneAudio->Stop();
		ToneAudio->SetSound(Loop);
		ToneAudio->FadeIn(LyudCityThingsImpl::ToneFadeIn, Level);
	}
	ToneLevelSet = Level;
}

void ULyudCityThings::TickTone(const AHeroCharacter* Hero, float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !Model || Model->Tones.empty())
	{
		return;
	}
	int32 Want = -1;
	int32 Near = -1;
	if (Hero && !bOutside)
	{
		Near = Model->NearestThing(LyudCityThingsImpl::Vec(Hero->GetActorLocation()), LyudCityThingsImpl::ToneRadius, LyudCityThingsImpl::PlotFloor);
		Want = Near >= 0 ? Model->ToneNow(Near, LyudCityThingsImpl::When(World, false), bOpenAllHours) : -1;
	}
	if (Want != ToneHeard)
	{
		SetTone(Want);
		return;
	}
	const LyudCity::FTone* T = Model->ToneAt(ToneHeard);
	if (!T)
	{
		return;
	}
	if (ToneAudio && ToneSounds.IsValidIndex(ToneHeard) && ToneSounds[ToneHeard])
	{
		// the level follows the volumes and the open air; a loop asset that does not loop by itself is started again
		const float Level = ToneLevel(T->Volume);
		if (!ToneAudio->IsPlaying())
		{
			ToneAudio->FadeIn(0.3f, Level);
			ToneLevelSet = Level;
		}
		else if (FMath::Abs(Level - ToneLevelSet) > 0.02f)
		{
			ToneAudio->AdjustVolume(0.5f, Level);
			ToneLevelSet = Level;
		}
	}
	if (ToneShotIn < 0.0 || !Hero)
	{
		return;
	}
	ToneShotIn -= (double)DeltaTime;
	if (ToneShotIn > 0.0)
	{
		return;
	}
	LyudCity::FRng Rng;
	Rng.State = ToneDice;
	const std::string& Shot = Model->PickShot(ToneHeard, Rng);
	const double Angle = Rng.Unit() * 2.0 * UE_DOUBLE_PI;
	const double ShotDist = LyudCityThingsImpl::ToneShotNear + Rng.Unit() * (LyudCityThingsImpl::ToneShotFar - LyudCityThingsImpl::ToneShotNear);
	ToneShotIn = Model->NextShotIn(ToneHeard, Rng);
	ToneDice = Rng.State;
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	if (Sound && !Shot.empty())
	{
		const float Keep = FMath::Lerp(1.f, LyudCityThingsImpl::ToneOutdoorKeep, FMath::Clamp(Sound->GetOutdoors(), 0.f, 1.f));
		const FVector Where = Hero->GetActorLocation() + FVector(FMath::Cos(Angle) * ShotDist, FMath::Sin(Angle) * ShotDist, 60.0);
		Sound->PlayEffect(LyudCityThingsImpl::Name(Shot), Where, T->ShotVolume * Keep);
	}
}

FString ULyudCityThings::ReportTone() const
{
	const LyudCity::FTone* T = Model ? Model->ToneAt(ToneHeard) : nullptr;
	FString Out;
	if (!T)
	{
		Out = TEXT("room tone: silence");
	}
	else
	{
		const bool bLoaded = ToneSounds.IsValidIndex(ToneHeard) && ToneSounds[ToneHeard] != nullptr;
		Out = FString::Printf(TEXT("room tone %s: loop %s (%s, volume %.2f, level %.2f, %s), %d one-shots every %.0f..%.0f s, next in %.1f s"),
			*LyudCityThingsImpl::Str(T->Id), *LyudCityThingsImpl::Str(T->Loop), bLoaded ? TEXT("loaded") : TEXT("no asset: silent"), T->Volume,
			ToneLevelSet, ToneAudio && ToneAudio->IsPlaying() ? TEXT("playing") : TEXT("stopped"), (int32)T->Shots.size(), T->EveryMin, T->EveryMax,
			ToneShotIn);
	}
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %s"), *Out);
	return Out;
}

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

static FAutoConsoleCommandWithWorldAndArgs GCitySit(TEXT("lyud.City.Sit"),
	TEXT("lyud.City.Sit [thing id]: seats the hero on a seat (the nearest free one without an id), wherever he is: the seated pose, the look cone"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->SitOnThing(Args.Num() > 0 ? Args[0] : FString(), false);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityLie(TEXT("lyud.City.Lie"),
	TEXT("lyud.City.Lie [thing id]: lays the hero on a bed (the nearest free one without an id): the pillow's view; a \"sleep\" bed opens the hour chooser"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->SitOnThing(Args.Num() > 0 ? Args[0] : FString(), true);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCitySleep(TEXT("lyud.City.Sleep"),
	TEXT("lyud.City.Sleep <hour 0..23>: lying on a bed, sleeps until that hour (the fade, the clock on, then up)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (Things && Args.Num() > 0 && Args[0].IsNumeric())
		{
			Things->SleepUntil(FCString::Atoi(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityRead(TEXT("lyud.City.Read"),
	TEXT("lyud.City.Read <text id> | next | prev | close | list | all | clear: opens a text of the reading view wherever the hero is (birds_starlings), turns or shuts it, lists the texts read, marks every text read or none"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (!Things)
		{
			return;
		}
		const FString Verb = Args.Num() > 0 ? Args[0] : FString(TEXT("list"));
		if (Verb == TEXT("next"))
		{
			Things->TurnBook(1);
		}
		else if (Verb == TEXT("prev"))
		{
			Things->TurnBook(-1);
		}
		else if (Verb == TEXT("close"))
		{
			Things->CloseBook();
		}
		else if (Verb == TEXT("list"))
		{
			Things->ShowReadList(true);
		}
		else if (Verb == TEXT("all") || Verb == TEXT("clear"))
		{
			Things->SetAllRead(Verb == TEXT("all"));
			Things->ShowReadList(false);
		}
		else
		{
			Things->OpenText(Verb);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityAsk(TEXT("lyud.City.Ask"),
	TEXT("lyud.City.Ask <thing id> [topic id] | list | reset: opens the staff talk at a thing wherever the hero is (whatever the hour and the staff), or asks one topic of it at once; lists the topics heard; makes every topic new again"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (!Things)
		{
			return;
		}
		const FString Verb = Args.Num() > 0 ? Args[0] : FString(TEXT("list"));
		if (Verb == TEXT("list"))
		{
			Things->ShowHeardTopics(true);
		}
		else if (Verb == TEXT("reset"))
		{
			Things->ForgetTopics();
		}
		else
		{
			Things->AskAt(Verb, Args.Num() > 1 ? Args[1] : FString());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityGo(TEXT("lyud.City.Go"),
	TEXT("lyud.City.Go <thing id>: takes the hero to a thing as a lift would (in front of it, facing away; a fade)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudCityThings* Things = ULyudCityThings::Get(World);
		if (Things && Args.Num() > 0)
		{
			Things->GoToThing(Args[0]);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCityTone(TEXT("lyud.City.Tone"),
	TEXT("lyud.City.Tone: logs the room tone heard now (its loop, level, one-shots)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (const ULyudCityThings* Things = ULyudCityThings::Get(World))
		{
			Things->ReportTone();
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
static LyudDev::FAutoAction GCityDevSit(TEXT("interiors.sit_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.sit_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->SitOnThing(FString(), false);
	}
});
static LyudDev::FAutoAction GCityDevLie(TEXT("interiors.lie_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.lie_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->SitOnThing(FString(), true);
	}
});
static LyudDev::FAutoAction GCityDevReadNear(TEXT("interiors.read_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.read_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->OpenNearestText();
	}
});
static LyudDev::FAutoAction GCityDevReadAll(TEXT("interiors.read_all"), TEXT("interiors"), TEXT("dev.action.interiors.read_all"),
	[](UWorld* W)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(W))
		{
			Things->SetAllRead(!Things->AreAllRead());
			Things->ShowReadList(false);
		}
	},
	[](UWorld* W)
	{
		const ULyudCityThings* Things = ULyudCityThings::Get(W);
		return Things && Things->AreAllRead();
	});
static LyudDev::FAutoAction GCityDevReadList(TEXT("interiors.read_list"), TEXT("interiors"), TEXT("dev.action.interiors.read_list"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ShowReadList(true);
	}
});
static LyudDev::FAutoAction GCityDevTopics(TEXT("interiors.topics"), TEXT("interiors"), TEXT("dev.action.interiors.topics"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ShowHeardTopics(true);
	}
});
static LyudDev::FAutoAction GCityDevTopicsReset(TEXT("interiors.topics_reset"), TEXT("interiors"), TEXT("dev.action.interiors.topics_reset"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ForgetTopics();
	}
});
static LyudDev::FAutoAction GCityDevAskNear(TEXT("interiors.ask_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.ask_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->AskNearest();
	}
});
static LyudDev::FAutoAction GCityDevLift(TEXT("interiors.lift_nearest"), TEXT("interiors"), TEXT("dev.action.interiors.lift_nearest"), [](UWorld* W)
{
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->UseNearestLift();
	}
});
static LyudDev::FAutoAction GCityDevTone(TEXT("interiors.tone"), TEXT("interiors"), TEXT("dev.action.interiors.tone"), [](UWorld* W)
{
	if (const ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->ReportTone();
	}
});
#endif

#undef LYUD_CITY_THINGS_DEV
