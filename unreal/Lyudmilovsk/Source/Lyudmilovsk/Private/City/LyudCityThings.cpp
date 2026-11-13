#include "City/LyudCityThings.h"

#include "CityThingsModel.h"
#include "City/SLyudCityPage.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Doors/LyudDoor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Food/LyudHandUse.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "People/PeopleLife.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudQuests.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#define LYUD_CITY_THINGS_DEV 1
#else
#define LYUD_CITY_THINGS_DEV 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudCity, Log, All);

namespace LyudCityThingsImpl
{
	/** How far the hero reaches a thing (cm, from the eyes), how often the thing in view is re-picked (s), the margin
	 *  round the box of all things within which anything is picked (cm). */
	constexpr double Reach = 260.0;
	constexpr float PickEvery = 0.2f;
	constexpr double TownMargin = 3000.0;
	/** The page shuts when the hero has walked this far from where he opened it (cm). */
	constexpr float PageWalkAway = 250.f;

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

	LyudCity::FWhen When(const UWorld* World)
	{
		const FDateTime Now = UGameCalendar::NowIn(World);
		LyudCity::FWhen W;
		W.Day = (long long)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
		W.Hour = (float)Now.GetHour() + (float)Now.GetMinute() / 60.f;
		W.Weekday = (int)Now.GetDayOfWeek();     // Monday = 0
		return W;
	}

	/** What the rules ask the game: the money, the pocket, who is on shift at a plot (town-life place ids are plot ids). */
	LyudCity::FAsk Ask(const UObject* Context)
	{
		LyudCity::FAsk A;
		ULyudInventory* Inv = ULyudInventory::Get(Context);
		A.Money = Inv ? Inv->Rubles : 0;
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
	Load();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("CityThings"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudCityThings::StartOver);
	}
}

void ULyudCityThings::Deinitialize()
{
	SetPrompted(-1);
	ClosePage();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(GetWorld()))
	{
		Saves->UnregisterSaveObject(TEXT("CityThings"));
		Saves->OnNewGame.RemoveAll(this);
	}
	Super::Deinitialize();
}

void ULyudCityThings::Load()
{
	Model = MakePimpl<LyudCity::FModel>();
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/interiors.json");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudCity, Display, TEXT("CityThings: no City/interiors.json: no things to do in the buildings"));
		return;
	}
	std::string Error;
	if (!Model->Load(std::string(TCHAR_TO_UTF8(*Text)), Error))
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: City/interiors.json unreadable: %s"), *LyudCityThingsImpl::Str(Error));
		Model = MakePimpl<LyudCity::FModel>();
		return;
	}
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
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: %d things, %d acts, %d shops from City/interiors.json (%d unknown items)"),
		(int32)Model->Things.size(), (int32)Model->Acts.size(), (int32)Model->Shops.size(), Unknown);
}

void ULyudCityThings::StartOver()
{
	if (Model)
	{
		Model->ResetState();
	}
	Saved.Reset();
}

void ULyudCityThings::OnGameSaving_Implementation()
{
	Saved.Reset();
	if (!Model)
	{
		return;
	}
	std::map<std::string, std::string> Out;
	Model->SaveState(Out);
	for (const std::pair<const std::string, std::string>& KV : Out)
	{
		Saved.Add(LyudCityThingsImpl::Str(KV.first), LyudCityThingsImpl::Str(KV.second));
	}
}

void ULyudCityThings::OnGameLoaded_Implementation()
{
	if (!Model)
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

void ULyudCityThings::SetPrompted(int32 Thing)
{
	if (Thing == Prompted)
	{
		return;
	}
	Prompted = Thing;
	FText Line;
	if (const LyudCity::FAct* A = Model ? Model->ActOf(Thing) : nullptr)
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

void ULyudCityThings::JumpMinutes(int32 Minutes)
{
	UWorld* World = GetWorld();
	UGameCalendar* Cal = World ? World->GetSubsystem<UGameCalendar>() : nullptr;
	if (Cal && Minutes > 0)
	{
		Cal->SetNow(Cal->Now() + FTimespan::FromMinutes((double)Minutes));
	}
}

void ULyudCityThings::Sit(int32 Thing, AHeroCharacter* Hero, bool bLie)
{
	UWorld* World = GetWorld();
	if (!World || !Hero || !Model || Thing < 0 || Thing >= NumThings() || Hero->IsSeated())
	{
		return;
	}
	if (!SeatRoot)
	{
		// one anchor at the world origin for every seat in town: the seat's frame is given to SitOn in world terms
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("LyudCitySeat"));
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Anchor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		if (!Anchor)
		{
			return;
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
	const float Height = T.PropF("height", bLie ? 0.5f : 0.45f) * 100.f;
	// the floor under it: chairs and stools never block, so a trace from the cushion down meets the floor; a bed or a bench with
	// a blocking box is met on its top, and the floor is a seat's height below that
	double FloorZ = At.Z - Height;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CitySeatFloor), false, Hero);
	if (World->LineTraceSingleByChannel(Hit, At + FVector(0.0, 0.0, 30.0), At - FVector(0.0, 0.0, 300.0), ECC_Visibility, Q))
	{
		FloorZ = Hit.ImpactPoint.Z > At.Z - 0.5 * Height ? Hit.ImpactPoint.Z - Height : Hit.ImpactPoint.Z;
	}
	if (ULyudHandUse* Hand = ULyudHandUse::Get(this))
	{
		Hand->PutAway();
	}
	// the seat's frame: the floor under the cushion's front edge, X the way the sitter faces (the way the thing faces)
	const FRotator Facing(0.0, (double)T.Yaw, 0.0);
	const FVector Front = FVector(At.X, At.Y, FloorZ) + Facing.Vector() * (bLie ? 40.0 : 20.0);
	Hero->SitOn(SeatRoot, FTransform(Facing, Front), Height);
	Hero->SetStandUpOffset(FVector(45.f, 0.f, 0.f));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-8.0, (double)T.Yaw, 0.0));
	}
	// TODO(city): "lie" sits on the bed's edge: the hero's body has no lying pose yet (HeroAnimInstance)
	SeatedOn = Thing;
}

void ULyudCityThings::Run(int32 Thing, bool bFromConsole)
{
	UWorld* World = GetWorld();
	if (!World || !Model || Thing < 0 || Thing >= NumThings())
	{
		return;
	}
	const LyudCity::FOutcome O = Model->Run(Thing, LyudCityThingsImpl::When(World), LyudCityThingsImpl::Ask(this));
	const LyudCity::FThing& T = Model->Things[Thing];
	UE_LOG(LogLyudCity, Display, TEXT("CityThings: do %s (%s, act %s) -> %s line=%s minutes=%d price=%d shop=%s event=%s/%s%s"),
		*LyudCityThingsImpl::Str(T.Id), *LyudCityThingsImpl::Str(T.Kind), O.Act >= 0 ? *LyudCityThingsImpl::Str(Model->Acts[O.Act].Id) : TEXT("-"), UTF8_TO_TCHAR(LyudCity::VerdictName(O.Verdict)),
		*LyudCityThingsImpl::Str(O.Line), O.Minutes, O.Price, *LyudCityThingsImpl::Str(O.Shop), *LyudCityThingsImpl::Str(O.Event), *LyudCityThingsImpl::Str(O.Target), O.bFirstVisitToday ? TEXT(" visit") : TEXT(""));
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
	ULyudQuests::Notify(this, LyudCityThingsImpl::Name(O.Event), LyudCityThingsImpl::Str(O.Target));
	if (O.bFirstVisitToday && !O.Plot.empty())
	{
		ULyudQuests::Notify(this, TEXT("visit"), LyudCityThingsImpl::Str(O.Plot));
	}

	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = Hero ? Cast<APlayerController>(Hero->GetController()) : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
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
	// sitting and lying only when the hero is at the thing (a console run from across town doesn't carry him there)
	const bool bNear = Hero && FVector::Dist(Hero->GetActorLocation(), At) < 400.f;
	if (bNear && (O.Pose == "sit" || O.Pose == "lie"))
	{
		Sit(Thing, Hero, O.Pose == "lie");
	}
	if (O.Minutes > 0)
	{
		if (O.bFade && PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, false, true);
			FadeMinutes = O.Minutes;
			FadeAt = World->GetTimeSeconds() + 0.7;
			bFadingIn = false;
		}
		else
		{
			JumpMinutes(O.Minutes);
		}
	}
	// TODO(city): "screen" (a picture shown while it lasts) shows the act's lines on the page for now
	if (!O.Line.empty())
	{
		PageThing = Thing;
		PageLeft = FMath::Max(0, O.NumLines - 1);
		PageTitle = O.Title.empty() ? FText::GetEmpty() : LyudText::Get(LyudCityThingsImpl::Str(O.Title));
		PageFrom = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
		PageFrame = GFrameCounter;
		ShowPage(PageTitle, LyudText::Get(LyudCityThingsImpl::Str(O.Line)), PageLeft > 0);
		SetPrompted(-1);
	}
	(void)bFromConsole;
}

bool ULyudCityThings::RunThingById(const FString& Id)
{
	const int32 Thing = Model ? Model->FindThing(std::string(TCHAR_TO_UTF8(*Id))) : -1;
	if (Thing < 0)
	{
		UE_LOG(LogLyudCity, Warning, TEXT("CityThings: no thing %s"), *Id);
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
	// a fade round a jump of the clock: black, jump, hold a moment, back
	if (FadeMinutes > 0 && World->GetTimeSeconds() >= FadeAt)
	{
		JumpMinutes(FadeMinutes);
		FadeMinutes = 0;
		bFadingIn = true;
		FadeAt = World->GetTimeSeconds() + 0.4;
	}
	else if (bFadingIn && World->GetTimeSeconds() >= FadeAt)
	{
		bFadingIn = false;
		APlayerController* FadePC = UGameplayStatics::GetPlayerController(World, 0);
		if (FadePC && FadePC->PlayerCameraManager)
		{
			FadePC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.8f, FLinearColor::Black, false, false);
		}
	}
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
	}
	// the thing in view, a few times a second, and nothing at all outside the town's box
	PickClock -= DeltaTime;
	if (PickClock <= 0.f)
	{
		PickClock = LyudCityThingsImpl::PickEvery;
		bOutside = !Hero || !Model->InBounds(LyudCityThingsImpl::Vec(Hero->GetActorLocation()), LyudCityThingsImpl::TownMargin);
		Picked = bOutside ? -1 : PickInView(Hero, PC);
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
		if (PC && FadeMinutes == 0 && !bFadingIn && PC->WasInputKeyJustPressed(EKeys::E) && GFrameCounter != PageFrame)
		{
			if (PageLeft > 0 && PageThing >= 0)
			{
				--PageLeft;
				ShowPage(PageTitle, LyudText::Get(LyudCityThingsImpl::Str(Model->NextLine(PageThing))), PageLeft > 0);
				if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
				{
					Sound->PlayEffect(TEXT("page_turn"), ThingLocation(PageThing));
				}
			}
			else
			{
				ClosePage();
			}
		}
		return;
	}
	// on foot, nothing else on screen, not seated (the restaurant, a train, a chair), not on a bike, not mid-fade
	const bool bFree = PC && UI && UI->CanOpen(true) && !Hero->IsSeated() && !Hero->IsOnBike() && !Hero->IsLeadingBike()
		&& FadeMinutes == 0 && !bFadingIn;
	if (!bFree)
	{
		SetPrompted(-1);
		return;
	}
	if (World->GetTimeSeconds() < RefusedUntil)
	{
		return;
	}
	SetPrompted(Picked);
	if (Picked >= 0 && PC->WasInputKeyJustPressed(EKeys::E))
	{
		Run(Picked, false);
	}
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
#endif

#undef LYUD_CITY_THINGS_DEV
