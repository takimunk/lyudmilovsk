#include "Transport/LyudFrontDoor.h"

#include "CablewaySim.h"
#include "FrontDoor.h"
#include "SLyudFrontDoorWidgets.h"
#include "StationLife.h"
#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sky/LyudSky.h"
#include "Food/LyudFood.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "People/CounterService.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Plants/GameCalendar.h"
#include "Transport/LyudCableway.h"
#include "Transport/LyudTransport.h"
#include "UI/LyudGameScreen.h"
#if __has_include("Dev/LyudDev.h")
#include "Dev/LyudDev.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudFrontDoor, Log, All);

namespace LyudFrontDoorImpl
{
	FString Str(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }
	std::string Std(const FString& S) { return TCHAR_TO_UTF8(*S); }

	constexpr float ReturnEvery = 0.25f;    // s between two looks for a lost thing's owner near him
	constexpr float ReturnCm = 260.f;       // he hands a thing back within this of its owner
	constexpr float DuskSun = 3.f;          // degrees: the sun under this, the lights are on
	constexpr float DawnSun = 5.f;          // ... and off again over this (no flicker at the edge)
	constexpr float SignNearCm = 30000.f;   // the club's sign is drawn within this

	const TCHAR* const TravelCard = TEXT("travel_card");
}

namespace FrontDoorImpl = LyudFrontDoorImpl;

ULyudFrontDoor::ULyudFrontDoor() = default;
ULyudFrontDoor::~ULyudFrontDoor() = default;

ULyudFrontDoor* ULyudFrontDoor::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudFrontDoor>() : nullptr;
}

void ULyudFrontDoor::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Lost = MakePimpl<LyudFrontDoor::FLostFound>();
	Usual = MakePimpl<LyudFrontDoor::FUsual>();
	Card = MakePimpl<LyudFrontDoor::FTravelCard>();
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(&InWorld))
	{
		UI->OnShopBought.AddUObject(this, &ULyudFrontDoor::OnBought);
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("FrontDoor"), this);
		Saves->OnNewGame.AddWeakLambda(this, [this]()
		{
			Lost->Shelf.clear();
			Lost->Carried.clear();
			Lost->Returned = 0;
			Lost->Day = -1;
			Usual->Bought.clear();
			*Card = LyudFrontDoor::FTravelCard();
			Saved.Reset();
			Day = -1;
		});
	}
}

void ULyudFrontDoor::Deinitialize()
{
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->OnShopBought.RemoveAll(this);
		UI->SetPrompt(this, FText::GetEmpty());
	}
	if (SignActor)
	{
		SignActor->Destroy();
		SignActor = nullptr;
	}
	Super::Deinitialize();
}

void ULyudFrontDoor::OnGameSaving_Implementation()
{
	if (!Lost)
	{
		return;
	}
	Saved.Reset();
	Saved.Add(TEXT("usual"), FrontDoorImpl::Str(Usual->Save()));
	Saved.Add(TEXT("card"), FrontDoorImpl::Str(Card->Save()));
	if (bOwners)
	{
		Saved.Add(TEXT("lost"), FrontDoorImpl::Str(Lost->Save()));
	}
	else if (const FString* Old = Saved.Find(TEXT("lost")))
	{
		Saved.Add(TEXT("lost"), *Old);
	}
}

void ULyudFrontDoor::OnGameLoaded_Implementation()
{
	if (!Lost)
	{
		return;
	}
	if (const FString* S = Saved.Find(TEXT("usual")))
	{
		Usual->Load(FrontDoorImpl::Std(*S));
	}
	if (const FString* S = Saved.Find(TEXT("card")))
	{
		Card->Load(FrontDoorImpl::Std(*S));
	}
	// the lost things name their owners: they wait till the station life is up (EnsureOwners)
	bSavedPending = true;
	bOwners = false;
	Day = -1;
	OfferPid = -1;
}

int32 ULyudFrontDoor::Today() const
{
	const FDateTime N = UGameCalendar::NowIn(GetWorld());
	return (int32)((N.GetDate() - FDateTime(1970, 1, 1)).GetTotalDays());
}

int32 ULyudFrontDoor::FrontStation() const
{
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FNetwork* N = Tr ? Tr->GetNet() : nullptr;
	return N ? N->FindStation("aton") : -1;
}

bool ULyudFrontDoor::EnsureOwners()
{
	if (bOwners)
	{
		return true;
	}
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FStationLife* Life = Tr ? Tr->GetLife() : nullptr;
	const int32 St = FrontStation();
	if (!Life || St < 0)
	{
		return false;
	}
	LyudFrontDoor::OwnersAt(*Life, St, Lost->Owners, Lost->OwnerHour);
	bOwners = true;
	if (bSavedPending)
	{
		bSavedPending = false;
		if (const FString* S = Saved.Find(TEXT("lost")))
		{
			Lost->Load(FrontDoorImpl::Std(*S));
		}
	}
	UE_LOG(LogLyudFrontDoor, Display, TEXT("FrontDoor: %d people could lose things at Aton"), (int32)Lost->Owners.size());
	return true;
}

FText ULyudFrontDoor::ItemName(FName Id) const
{
	return LyudText::Get(FString::Printf(TEXT("item.%s.name"), *Id.ToString()));
}

void ULyudFrontDoor::Toast(const FText& Text) const
{
	if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
	{
		Screen->Toast(Text);
	}
}

void ULyudFrontDoor::Say(AActor* Body, const FString& Prefix, int32 Count, int32 Pid) const
{
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	if (Talk && Body && Count > 0)
	{
		const UWorld* World = GetWorld();
		const int32 N = 1 + (int32)((uint32)(World ? World->GetTimeSeconds() * 3.0 : 0.0) % (uint32)Count);
		Talk->SayLine(Body, FString::Printf(TEXT("%s.%d"), *Prefix, N), Pid);
	}
}

// --- the usual ----------------------------------------------------------------------------------------------------------

void ULyudFrontDoor::OnBought(FName Id, int32 Count)
{
	const ULyudCounterService* Counter = ULyudCounterService::Get(this);
	if (Counter && !Counter->OpenCounter().IsEmpty() && Usual)
	{
		Usual->Add(FrontDoorImpl::Std(Counter->OpenCounter()), FrontDoorImpl::Std(Id.ToString()), FMath::Max(1, Count));
	}
}

FName ULyudFrontDoor::UsualAt(const FString& Counter, int32* OutTimes) const
{
	if (!Usual)
	{
		return NAME_None;
	}
	const std::string Item = Usual->Of(FrontDoorImpl::Std(Counter), OutTimes);
	return Item.empty() ? NAME_None : FName(*FrontDoorImpl::Str(Item));
}

bool ULyudFrontDoor::BuyUsual(const FString& Counter, FName Item, int32 Price, AActor* ServerBody, int32 ServerPid)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || Item.IsNone())
	{
		return false;
	}
	if (Inv->Rubles < Price)
	{
		Toast(LyudText::Get(TEXT("frontdoor.usual.no_money"), { FStringFormatArg(Price) }));
		return false;
	}
	const int32 Purse = Inv->Rubles;
	Inv->Rubles -= Price;
	Inv->Add(Item);
	Usual->Add(FrontDoorImpl::Std(Counter), FrontDoorImpl::Std(Item.ToString()));
	ULyudCounterService* Service = ULyudCounterService::Get(this);
	if (Service)
	{
		Service->AnnounceChange(Price, Purse);
	}
	// the cup goes down on the counter between them and his hand takes it
	if (ULyudFoodSubsystem* Food = ULyudFoodSubsystem::Get(this))
	{
		FVector Where;
		const UWorld* World = GetWorld();
		const AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (!(Service && Service->HandoverPointFrom(ServerBody, 0, Where)) && Hero)
		{
			Where = Hero->EyeLocation() + FRotator(0.f, Hero->GetActorRotation().Yaw, 0.f).RotateVector(FVector(52.f, 4.f, -42.f));
		}
		Food->HandOver(Item, Where);
	}
	if (UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this))
	{
		Life->ReactManner(ServerPid, TEXT("point"), 1.5f);
	}
	Say(ServerBody, TEXT("line.frontdoor.usual_give"), 3, ServerPid);
	UE_LOG(LogLyudFrontDoor, Display, TEXT("FrontDoor: the usual %s at %s for %d"), *Item.ToString(), *Counter, Price);
	return true;
}

// --- the travel card -----------------------------------------------------------------------------------------------------

bool ULyudFrontDoor::HasCard() const
{
	const ULyudInventory* Inv = ULyudInventory::Get(this);
	return Inv && Inv->Count(FrontDoorImpl::TravelCard) > 0;
}

void ULyudFrontDoor::GiveCard(AActor* From, int32 FromPid)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	if (!Inv || HasCard())
	{
		return;
	}
	Inv->Add(FrontDoorImpl::TravelCard);
	Card->bHave = true;
	Say(From, TEXT("line.frontdoor.card_give"), 3, FromPid);
	Toast(LyudText::Get(TEXT("frontdoor.card.got")));
}

bool ULyudFrontDoor::HasStamp(int32 St) const
{
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FNetwork* N = Tr ? Tr->GetNet() : nullptr;
	return N && St >= 0 && St < (int32)N->Stations.size() && Card->Has(N->Stations[(size_t)St].Id);
}

bool ULyudFrontDoor::Punch(int32 St, const FVector& Where)
{
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FNetwork* N = Tr ? Tr->GetNet() : nullptr;
	if (!N || St < 0 || St >= (int32)N->Stations.size())
	{
		return false;
	}
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Sound->PlayEffect(TEXT("ticket_punch"), Where, 0.8f);
	}
	if (!HasCard())
	{
		Toast(LyudText::Get(TEXT("frontdoor.punch.no_card")));
		return false;
	}
	Card->bHave = true;
	const std::string& Id = N->Stations[(size_t)St].Id;
	if (!Card->Punch(Id))
	{
		Toast(LyudText::Get(TEXT("frontdoor.punch.again"), { FStringFormatArg(Tr->StationName(St).ToString()) }));
		return false;
	}
	// the collections' set «Все станции» (its HUD note counts, its last stamp pays the badge)
	if (ULyudCityThings* Things = ULyudCityThings::Get(this))
	{
		Things->GiveStamp(FString::Printf(TEXT("stations/%s"), *FrontDoorImpl::Str(Id)));
	}
	Toast(LyudText::Get(TEXT("frontdoor.punch.done"), { FStringFormatArg(Tr->StationName(St).ToString()), FStringFormatArg((int32)Card->Stamps.size()),
		FStringFormatArg((int32)N->Stations.size()) }));
	return true;
}

// --- the lost and found --------------------------------------------------------------------------------------------------

void ULyudFrontDoor::ShelfItems(TArray<FName>& Out) const
{
	Out.Reset();
	if (!bOwners)
	{
		return;
	}
	for (const LyudFrontDoor::FLost& L : Lost->Shelf)
	{
		Out.Add(FName(LyudFrontDoor::LostKinds[L.Kind]));
	}
}

bool ULyudFrontDoor::TakeFromShelf(AActor* From, int32 FromPid)
{
	ULyudInventory* Inv = ULyudInventory::Get(this);
	LyudFrontDoor::FLost L;
	if (!Inv || !EnsureOwners() || !Lost->Take(0, L))
	{
		Say(From, TEXT("line.frontdoor.shelf_empty"), 2, FromPid);
		return false;
	}
	const FName Item(LyudFrontDoor::LostKinds[L.Kind]);
	Inv->Add(Item);
	const FString Key = FrontDoorImpl::Str(Lost->Owners[(size_t)L.Owner]);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const int32 Pid = Life ? Life->FindPerson(Key) : -1;
	const FText Name = Life && Pid >= 0 ? Life->GetDisplayName(Pid) : FText::FromString(Key);
	const float Hour = Lost->OwnerHour[(size_t)L.Owner];
	Say(From, TEXT("line.frontdoor.shelf_take"), 3, FromPid);
	Toast(LyudText::Get(TEXT("frontdoor.lost.whose"), { FStringFormatArg(ItemName(Item).ToString()), FStringFormatArg(Name.ToString()),
		FStringFormatArg(FrontDoorImpl::Str(LyudFrontDoor::Clock(Hour))) }));
	return true;
}

void ULyudFrontDoor::TickReturn(AHeroCharacter* Hero)
{
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ULyudInventory* Inv = ULyudInventory::Get(this);
	const ULyudCounterService* Service = ULyudCounterService::Get(this);
	const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	int32 Best = -1, BestIndex = -1;
	if (UI && Inv && Service && Life && !UI->IsOpen() && bOwners && !Hero->IsHidden())
	{
		double BestD = (double)FrontDoorImpl::ReturnCm * FrontDoorImpl::ReturnCm;
		for (int32 I = 0; I < (int32)Lost->Carried.size(); I++)
		{
			const LyudFrontDoor::FLost& L = Lost->Carried[(size_t)I];
			if (Inv->Count(FName(LyudFrontDoor::LostKinds[L.Kind])) <= 0)
			{
				continue;
			}
			const FString Key = FrontDoorImpl::Str(Lost->Owners[(size_t)L.Owner]);
			int32* Cached = PidCache.Find(Key);
			const int32 Pid = Cached ? *Cached : PidCache.Add(Key, Life->FindPerson(Key));
			const AActor* Body = Service->BodyOf(Pid);
			const double D = Body ? FVector::DistSquared(Body->GetActorLocation(), Hero->GetActorLocation()) : 1e30;
			if (D < BestD)
			{
				BestD = D;
				Best = Pid;
				BestIndex = I;
			}
		}
	}
	if (Best != OfferPid || BestIndex != OfferIndex)
	{
		OfferPid = Best;
		OfferIndex = BestIndex;
		if (UI)
		{
			UI->SetPrompt(this, Best >= 0 ? LyudText::Get(TEXT("frontdoor.lost.prompt"), { FStringFormatArg(ItemName(FName(LyudFrontDoor::LostKinds[Lost->Carried[(size_t)BestIndex].Kind])).ToString()),
				FStringFormatArg(Life->GetDisplayName(Best).ToString()) }) : FText::GetEmpty());
		}
	}
}

// --- dusk and the club's sign -------------------------------------------------------------------------------------------

void ULyudFrontDoor::BuildSign()
{
	bSignTried = true;
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const ALyudCableway* Way = Tr ? Tr->GetCableway() : nullptr;
	FTransform Board;
	UWorld* World = GetWorld();
	if (!Way || !World || !Way->GetClubBoard(Board))
	{
		return;
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	SignActor = World->SpawnActor<AActor>(AActor::StaticClass(), Board, P);
	if (!SignActor)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(SignActor, TEXT("SignRoot"));
	SignActor->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldTransform(Board);
	// over the route board's canopy (its top at 2.9 m), facing the same way: out of the club's door
	Sign = NewObject<UWidgetComponent>(SignActor, TEXT("ClubSign"));
	Sign->SetupAttachment(Root);
	Sign->SetWidgetSpace(EWidgetSpace::World);
	Sign->SetDrawSize(FVector2D(1400.f, 360.f));
	Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sign->SetBlendMode(EWidgetBlendMode::Opaque);
	Sign->SetCastShadow(false);
	Sign->bAffectDistanceFieldLighting = false;
	Sign->RegisterComponent();
	Sign->SetRelativeLocation(FVector(2.f, 0.f, 345.f));
	Sign->SetRelativeScale3D(FVector(0.26f));
	Sign->SetSlateWidget(SNew(SLyudClubSign));
	Sign->SetRedrawTime(1.f);
	// two warm lamps under the sign light the forecourt at night (shadowless, near the hero only)
	for (const float Y : { -120.f, 120.f })
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(SignActor);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetRelativeLocation(FVector(60.f, Y, 300.f));
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(1400.f);
		L->SetAttenuationRadius(900.f);
		L->SetSourceRadius(8.f);
		L->SetLightColor(FLinearColor(1.f, 0.78f, 0.5f));
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		SignLights.Add(L);
	}
}

void ULyudFrontDoor::TickDusk(float DeltaTime)
{
	DuskClock -= DeltaTime;
	if (DuskClock > 0.f)
	{
		return;
	}
	DuskClock = 2.f;
	if (!Sky.IsValid())
	{
		for (TActorIterator<ALyudSky> It(GetWorld()); It; ++It)
		{
			Sky = *It;
			break;
		}
	}
	// no sky (an indoor level): by the clock, a rough sun
	const FDateTime T = UGameCalendar::NowIn(GetWorld());
	const float Sun = Sky.IsValid() ? Sky->SunAltitude() : 40.f * FMath::Sin((T.GetTimeOfDay().GetTotalHours() - 7.f) / 12.f * PI) - 10.f;
	const bool bWas = bDusk;
	bDusk = DuskForced >= 0 ? DuskForced == 1 : (bDusk ? Sun < FrontDoorImpl::DawnSun : Sun < FrontDoorImpl::DuskSun);
	if (!bSignTried)
	{
		BuildSign();
	}
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const bool bNear = SignActor && PC && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), SignActor->GetActorLocation()) < FrontDoorImpl::SignNearCm;
	if (Sign)
	{
		Sign->SetVisibility(bNear);
		Sign->SetTickMode(bNear ? ETickMode::Enabled : ETickMode::Disabled);
		if (TSharedPtr<SWidget> W = Sign->GetSlateWidget())
		{
			StaticCastSharedPtr<SLyudClubSign>(W)->SetLit(bDusk);
		}
		if (bWas != bDusk)
		{
			Sign->RequestRedraw();
		}
	}
	for (UPointLightComponent* L : SignLights)
	{
		if (L)
		{
			L->SetVisibility(bDusk && bNear && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), L->GetComponentLocation()) < 8000.f);
		}
	}
}

void ULyudFrontDoor::Tick(float DeltaTime)
{
	TickDusk(DeltaTime);
	UWorld* World = GetWorld();
	AHeroCharacter* Hero = World ? Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	if (!Hero || !EnsureOwners())
	{
		return;
	}
	const int32 Now = Today();
	if (Now != Day)
	{
		Day = Now;
		Lost->StartDay(Now);
	}
	ReturnClock -= DeltaTime;
	if (ReturnClock <= 0.f)
	{
		ReturnClock = FrontDoorImpl::ReturnEvery;
		TickReturn(Hero);
	}
	// E by the owner: the thing goes back, a word and a small present (polled, never consumed)
	const APlayerController* PC = Cast<APlayerController>(Hero->GetController());
	if (OfferPid >= 0 && OfferIndex >= 0 && OfferIndex < (int32)Lost->Carried.size() && PC && PC->WasInputKeyJustPressed(EKeys::E))
	{
		ULyudInventory* Inv = ULyudInventory::Get(this);
		const FName Item(LyudFrontDoor::LostKinds[Lost->Carried[(size_t)OfferIndex].Kind]);
		AActor* Body = ULyudCounterService::Get(this) ? ULyudCounterService::Get(this)->BodyOf(OfferPid) : nullptr;
		const UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
		const FText Name = Life ? Life->GetDisplayName(OfferPid) : FText::GetEmpty();
		if (Inv && Inv->Remove(Item))
		{
			const FName Gift(*FrontDoorImpl::Str(Lost->GiveBack(OfferIndex)));
			if (!Gift.IsNone())
			{
				Inv->Add(Gift);
			}
			Say(Body, TEXT("line.frontdoor.thanks"), 4, OfferPid);
			if (UPeopleLifeSubsystem* People = UPeopleLifeSubsystem::Get(this))
			{
				People->ReactManner(OfferPid, TEXT("wave"), 2.f);
			}
			Toast(LyudText::Get(TEXT("frontdoor.lost.returned"), { FStringFormatArg(ItemName(Item).ToString()), FStringFormatArg(Name.ToString()),
				FStringFormatArg(ItemName(Gift).ToString()) }));
			UE_LOG(LogLyudFrontDoor, Display, TEXT("FrontDoor: %s handed back to %d (%d handed back in all)"), *Item.ToString(), OfferPid, Lost->Returned);
		}
		OfferPid = -1;
		OfferIndex = -1;
		ReturnClock = 0.f;
	}
}

// --- console and developer mode ------------------------------------------------------------------------------------------

FString ULyudFrontDoor::Report() const
{
	if (!Lost)
	{
		return TEXT("FrontDoor: not started");
	}
	FString Shelf;
	for (const LyudFrontDoor::FLost& L : Lost->Shelf)
	{
		Shelf += FString::Printf(TEXT(" %s(%s, day %d)"), UTF8_TO_TCHAR(LyudFrontDoor::LostKinds[L.Kind]),
			L.Owner >= 0 && L.Owner < (int32)Lost->Owners.size() ? *FrontDoorImpl::Str(Lost->Owners[(size_t)L.Owner]) : TEXT("?"), L.Day);
	}
	FString Stamps;
	for (const std::string& S : Card->Stamps)
	{
		Stamps += TEXT(" ") + FrontDoorImpl::Str(S);
	}
	int32 Times = 0;
	const FName U = UsualAt(TEXT("hall:") + FString::FromInt(FrontStation()), &Times);
	return FString::Printf(TEXT("FrontDoor: dusk %s (forced %d); card %s, stamps%s; owners %d, shelf %d:%s; carried %d, handed back %d; usual at Aton %s (%d)"),
		bDusk ? TEXT("yes") : TEXT("no"), DuskForced, HasCard() ? TEXT("yes") : TEXT("no"), Stamps.IsEmpty() ? TEXT(" none") : *Stamps,
		(int32)Lost->Owners.size(), (int32)Lost->Shelf.size(), *Shelf, (int32)Lost->Carried.size(), Lost->Returned, *U.ToString(), Times);
}

void ULyudFrontDoor::DevGiveCard()
{
	if (ULyudInventory* Inv = ULyudInventory::Get(this))
	{
		if (!HasCard())
		{
			Inv->Add(FrontDoorImpl::TravelCard);
		}
		Card->bHave = true;
	}
}

void ULyudFrontDoor::DevStampAll()
{
	DevGiveCard();
	const ULyudTransportSubsystem* Tr = ULyudTransportSubsystem::Get(this);
	const LyudTransport::FNetwork* N = Tr ? Tr->GetNet() : nullptr;
	for (int32 S = 0; N && S < (int32)N->Stations.size(); S++)
	{
		if (Card->Punch(N->Stations[(size_t)S].Id))
		{
			if (ULyudCityThings* Things = ULyudCityThings::Get(this))
			{
				Things->GiveStamp(FString::Printf(TEXT("stations/%s"), *FrontDoorImpl::Str(N->Stations[(size_t)S].Id)));
			}
		}
	}
}

void ULyudFrontDoor::DevLost()
{
	if (!EnsureOwners() || Lost->Owners.empty())
	{
		return;
	}
	// a find for now, from somebody who rides through Aton
	LyudFrontDoor::FLost L;
	L.Day = Today();
	L.Owner = (int32)(FPlatformTime::Cycles() % (uint32)Lost->Owners.size());
	L.Kind = (int32)(FPlatformTime::Cycles() / 7u % (uint32)LyudFrontDoor::NumLostKinds);
	if ((int32)Lost->Shelf.size() < LyudFrontDoor::FLostFound::MaxShelf)
	{
		Lost->Shelf.push_back(L);
	}
}

void ULyudFrontDoor::DevUsual()
{
	const FString Counter = TEXT("hall:") + FString::FromInt(FrontStation());
	Usual->Add(FrontDoorImpl::Std(Counter), "coffee", LyudFrontDoor::FUsual::Times);
}

namespace
{
	ULyudFrontDoor* FrontDoorOf(UWorld* W) { return ULyudFrontDoor::Get(W); }

	FAutoConsoleCommandWithWorldAndArgs GFrontDoorCmd(TEXT("lyud.FrontDoor"),
		TEXT("lyud.FrontDoor status | card | stamps | lost | usual | dusk 0|1|-1: Aton as the club's front door (docs/front_door.md)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudFrontDoor* F = FrontDoorOf(W);
			if (!F)
			{
				return;
			}
			const FString Cmd = Args.Num() ? Args[0] : TEXT("status");
			if (Cmd == TEXT("card")) { F->DevGiveCard(); }
			else if (Cmd == TEXT("stamps")) { F->DevStampAll(); }
			else if (Cmd == TEXT("lost")) { F->DevLost(); }
			else if (Cmd == TEXT("usual")) { F->DevUsual(); }
			else if (Cmd == TEXT("dusk")) { F->DevDusk(Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1); }
			UE_LOG(LogLyudFrontDoor, Display, TEXT("%s"), *F->Report());
		}));

#if __has_include("Dev/LyudDev.h")
	LyudDev::FAutoAction GDevCard(TEXT("frontdoor.card"), TEXT("frontdoor"), TEXT("dev.action.frontdoor.card"),
		[](UWorld* W) { if (ULyudFrontDoor* F = FrontDoorOf(W)) { F->DevGiveCard(); } });
	LyudDev::FAutoAction GDevStamps(TEXT("frontdoor.stamps"), TEXT("frontdoor"), TEXT("dev.action.frontdoor.stamps"),
		[](UWorld* W) { if (ULyudFrontDoor* F = FrontDoorOf(W)) { F->DevStampAll(); } });
	LyudDev::FAutoAction GDevLost(TEXT("frontdoor.lost"), TEXT("frontdoor"), TEXT("dev.action.frontdoor.lost"),
		[](UWorld* W) { if (ULyudFrontDoor* F = FrontDoorOf(W)) { F->DevLost(); } });
	LyudDev::FAutoAction GDevUsual(TEXT("frontdoor.usual"), TEXT("frontdoor"), TEXT("dev.action.frontdoor.usual"),
		[](UWorld* W) { if (ULyudFrontDoor* F = FrontDoorOf(W)) { F->DevUsual(); } });
	LyudDev::FAutoAction GDevDusk(TEXT("frontdoor.dusk"), TEXT("frontdoor"), TEXT("dev.action.frontdoor.dusk"),
		[](UWorld* W) { if (ULyudFrontDoor* F = FrontDoorOf(W)) { F->DevDusk(F->IsDusk() ? 0 : 1); } },
		[](UWorld* W) { const ULyudFrontDoor* F = FrontDoorOf(W); return F && F->IsDusk(); });
#endif
}
