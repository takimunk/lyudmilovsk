#include "UI/LyudGameScreen.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SpotLightComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "UI/SLyudGameScreen.h"
#include "UI/SLyudHud.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHud(TEXT("lyud.Hud"), 1, TEXT("The HUD: the date and time, the followed quest, short notes (0 = hidden)."));

	TAutoConsoleVariable<int32> CVarBinoculars(TEXT("lyud.BinocularsHold"), 0, TEXT("Holds the binoculars up as if the right mouse button were down (headless checks)."));

	const float BinocularsZoom = 5.f;
}

ULyudGameScreen* ULyudGameScreen::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudGameScreen>() : nullptr;
}

void ULyudGameScreen::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ULyudQuests::ReloadData();
	if (ULyudQuests* Q = ULyudQuests::Get(&InWorld))
	{
		// every level start is a new game unless a load restores the quests right after (like the pocket)
		Q->Reset();
		Q->OnNews.AddUObject(this, &ULyudGameScreen::OnNews);
		Q->OnChanged.AddWeakLambda(this, [this]
		{
			if (Hud) { Hud->RefreshQuest(); }
			if (Screen) { Screen->Rebuild(); }
		});
	}
	if (ULyudInventory* Inv = ULyudInventory::Get(&InWorld))
	{
		Inv->OnAdded.AddUObject(this, &ULyudGameScreen::OnItemAdded);
		Inv->OnChanged.AddWeakLambda(this, [this] { if (Screen) { Screen->Rebuild(); } });
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame*) { bAnnounced = true; });
	}
	if (UGameViewportClient* Viewport = InWorld.GetGameViewport())
	{
		Hud = SNew(SLyudHud).Owner(this);
		Viewport->AddViewportWidgetContent(Hud.ToSharedRef(), 40);
	}
	QuietFor = 2.f;
	bAnnounced = false;
	bWalked = false;
	bWalkFromSet = false;
}

void ULyudGameScreen::Deinitialize()
{
	Close();
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Hud)
		{
			Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		}
		if (ULyudQuests* Q = ULyudQuests::Get(World))
		{
			Q->OnNews.RemoveAll(this);
			Q->OnChanged.RemoveAll(this);
		}
		if (ULyudInventory* Inv = ULyudInventory::Get(World))
		{
			Inv->OnAdded.RemoveAll(this);
			Inv->OnChanged.RemoveAll(this);
		}
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			Saves->OnAfterLoad.RemoveAll(this);
		}
	}
	Hud.Reset();
	Super::Deinitialize();
}

void ULyudGameScreen::Open(ELyudScreenTab InTab)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	if (!Viewport || !UI)
	{
		return;
	}
	Tab = InTab;
	if (bOpen)
	{
		Screen->SetTab(Tab);
		return;
	}
	Screen = SNew(SLyudGameScreenWidget).Owner(this);
	Screen->SetTab(Tab);
	// over the interactables' own prompts
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 85);
	bOpen = true;
	UI->Lock(true);
	UE_LOG(LogTemp, Log, TEXT("GameScreen: open on tab %d"), (int32)Tab);
}

void ULyudGameScreen::Close()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Screen)
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->Lock(false);
		}
	}
	Screen.Reset();
}

void ULyudGameScreen::ShowOnMap(FName Place)
{
	Open(ELyudScreenTab::Map);
	if (Screen)
	{
		Screen->ShowPlace(Place);
	}
}

void ULyudGameScreen::ShowDiary()
{
	Open(ELyudScreenTab::Journal);
	if (Screen)
	{
		Screen->ShowDiary();
	}
}

void ULyudGameScreen::Toast(const FText& Text, bool bQuest)
{
	if (Hud)
	{
		Hud->Toast(Text, bQuest);
	}
}

void ULyudGameScreen::OnNews(ELyudQuestNews What, FName Id)
{
	const FString Title = ULyudQuests::QuestTitle(Id).ToString();
	switch (What)
	{
	case ELyudQuestNews::Started:
		if (QuietFor <= 0.f)
		{
			Toast(LyudText::Get(TEXT("ui.toast.quest_new"), { FStringFormatArg(Title) }), true);
		}
		break;
	case ELyudQuestNews::Stage:
		Toast(LyudText::Get(TEXT("ui.toast.quest_stage"), { FStringFormatArg(Title) }), true);
		break;
	case ELyudQuestNews::Done:
		Toast(LyudText::Get(TEXT("ui.toast.quest_done"), { FStringFormatArg(Title) }), true);
		break;
	case ELyudQuestNews::Place:
		Toast(LyudText::Get(TEXT("ui.toast.place"), { FStringFormatArg(ULyudQuests::PlaceName(Id).ToString()) }));
		break;
	default:
		break;
	}
}

void ULyudGameScreen::OnItemAdded(FName Id, int32 Count)
{
	if (QuietFor > 0.f)
	{
		return;
	}
	const FString Name = ULyudInventory::ItemName(Id).ToString();
	Toast(Count > 1 ? LyudText::Get(TEXT("ui.toast.items"), { FStringFormatArg(Name), FStringFormatArg(Count) })
	                : LyudText::Get(TEXT("ui.toast.item"), { FStringFormatArg(Name) }));
}

UTexture2D* ULyudGameScreen::MapTexture()
{
	if (!MapTex)
	{
		MapTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_WorldMap.T_WorldMap"));
	}
	return MapTex;
}

UTexture2D* ULyudGameScreen::BinocularsMask()
{
	if (!MaskTex)
	{
		MaskTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_Binoculars.T_Binoculars"));
	}
	return MaskTex;
}

bool ULyudGameScreen::PlayerOnMap(FVector2D& At, FVector2D& Facing) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return false;
	}
	At = ULyudQuests::Frame().FromWorld(Pawn->GetActorLocation());
	const FVector Dir = FRotator(0.f, PC->GetControlRotation().Yaw, 0.f).Vector();
	// the map frame's axes are Unreal's X and Y
	Facing = FVector2D(Dir.X, Dir.Y).GetSafeNormal();
	return true;
}

bool ULyudGameScreen::IsHeadLampOn() const
{
	return Lamp && Lamp->IsVisible();
}

void ULyudGameScreen::UpdateWear(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	const ULyudInventory* Inv = ULyudInventory::Get(World);
	APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Inv || !Cam)
	{
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	const bool bFree = Hero && !bOpen && !(Menu && Menu->IsOpen()) && !UGameplayStatics::IsGamePaused(World);

	// the head lamp: a warm narrow beam from the forehead along the view
	const bool bLampWorn = Hero && Inv->IsWorn(TEXT("head_lamp"));
	if (bLampWorn && bFree && PC->WasInputKeyJustPressed(EKeys::L))
	{
		bLampOn = !bLampOn;
	}
	if (bLampWorn && (!Lamp || Lamp->GetOwner() != Hero))
	{
		Lamp = NewObject<USpotLightComponent>(Hero, TEXT("HeadLamp"));
		Lamp->SetIntensityUnits(ELightUnits::Lumens);
		Lamp->SetIntensity(180.f);
		Lamp->SetUseTemperature(true);
		Lamp->SetTemperature(4300.f);
		Lamp->SetAttenuationRadius(2200.f);
		Lamp->SetInnerConeAngle(10.f);
		Lamp->SetOuterConeAngle(26.f);
		Lamp->SetSourceRadius(1.f);
		Lamp->SetCastShadows(true);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetupAttachment(Hero->GetRootComponent());
		Lamp->RegisterComponent();
	}
	if (Lamp)
	{
		const bool bOn = bLampWorn && bLampOn && Lamp->GetOwner() == Hero;
		Lamp->SetVisibility(bOn);
		if (bOn)
		{
			const FRotator R = Cam->GetCameraRotation();
			Lamp->SetWorldLocationAndRotation(Cam->GetCameraLocation() + R.RotateVector(FVector(8.f, 0.f, 9.f)), R);
		}
	}

	// the binoculars: held to the eyes while the right mouse button is down
	const bool bLook = bFree && Inv->IsWorn(TEXT("binoculars")) && (PC->IsInputKeyDown(EKeys::RightMouseButton) || CVarBinoculars.GetValueOnGameThread() != 0);
	const float Before = Binoculars;
	Binoculars = FMath::FInterpConstantTo(Binoculars, bLook ? 1.f : 0.f, DeltaTime, 3.5f);
	if (Binoculars > 0.f)
	{
		if (Before <= 0.f || NormalFov <= 0.f)
		{
			Cam->UnlockFOV();
			NormalFov = Cam->GetFOVAngle();
		}
		const float K = FMath::SmoothStep(0.f, 1.f, Binoculars);
		Cam->SetFOV(FMath::Lerp(NormalFov, NormalFov / BinocularsZoom, K));
	}
	else if (Before > 0.f)
	{
		Cam->UnlockFOV();
		NormalFov = 0.f;
	}
}

void ULyudGameScreen::UpdateHud()
{
	if (!Hud)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const ULyudGameMenuSubsystem* Menu = World ? World->GetSubsystem<ULyudGameMenuSubsystem>() : nullptr;
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const bool bShow = CVarHud.GetValueOnGameThread() != 0 && !bOpen && !(Menu && Menu->IsOpen()) && !(UI && UI->IsShopOpen());
	Hud->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
}

void ULyudGameScreen::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (QuietFor > 0.f && (QuietFor -= DeltaTime) <= 0.f && !bAnnounced)
	{
		// a new game: name the quest it begins with
		bAnnounced = true;
		if (const ULyudQuests* Q = ULyudQuests::Get(World); Q && !Q->ShownQuest().IsNone())
		{
			Toast(LyudText::Get(TEXT("ui.toast.quest_new"), { FStringFormatArg(ULyudQuests::QuestTitle(Q->ShownQuest()).ToString()) }), true);
		}
	}

	// the keys: a page's key opens it, switches to it, or (on it) closes the screen
	ELyudScreenTab Want = Tab;
	bool bKey = false;
	if (PC->WasInputKeyJustPressed(EKeys::Tab) || PC->WasInputKeyJustPressed(EKeys::I)) { Want = ELyudScreenTab::Inventory; bKey = true; }
	else if (PC->WasInputKeyJustPressed(EKeys::J)) { Want = ELyudScreenTab::Journal; bKey = true; }
	else if (PC->WasInputKeyJustPressed(EKeys::M)) { Want = ELyudScreenTab::Map; bKey = true; }
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (bOpen)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape) || (Menu && Menu->IsOpen()) || (bKey && Want == Tab))
		{
			Close();
		}
		else if (bKey)
		{
			Open(Want);
		}
		else if (Tab == ELyudScreenTab::Map && PC->WasInputKeyJustPressed(EKeys::C) && Screen)
		{
			Screen->CenterMapOnPlayer();
		}
	}
	else if (bKey)
	{
		// the journal and the map open as a bird too; the inventory needs the hero's hands
		const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
		if (UI && UI->CanOpen(Want == ELyudScreenTab::Inventory))
		{
			Open(Want);
		}
	}

	// where the player is: places found, reach objectives, becoming a bird
	if (APawn* Pawn = PC->GetPawn())
	{
		const bool bBird = !Pawn->IsA<AHeroCharacter>();
		if (bBird && !bWasBird)
		{
			ULyudQuests::Notify(World, TEXT("bird"));
		}
		bWasBird = bBird;
		// the first few steps on foot (the game starts standing in the club; sitting down hides the hero)
		if (!bWalked && !Pawn->IsHidden())
		{
			if (!bWalkFromSet)
			{
				WalkFrom = Pawn->GetActorLocation();
				bWalkFromSet = true;
			}
			else if (FVector::Dist2D(WalkFrom, Pawn->GetActorLocation()) > 250.f)
			{
				bWalked = true;
				ULyudQuests::Notify(World, TEXT("walk"));
			}
		}
		if ((PlaceTimer -= DeltaTime) <= 0.f)
		{
			PlaceTimer = 0.25f;
			if (ULyudQuests* Q = ULyudQuests::Get(World))
			{
				Q->UpdatePosition(World, Pawn->GetActorLocation());
			}
		}
	}
	UpdateWear(DeltaTime);
	UpdateHud();
}

// --- dev commands for headless checks ---
static FAutoConsoleCommandWithWorldAndArgs GScreenOpen(TEXT("Screen.Open"), TEXT("Screen.Open inventory|journal|map|diary|none [place]: opens a page of the game screen (map: centred on a place)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudGameScreen* S = ULyudGameScreen::Get(World);
		if (!S)
		{
			return;
		}
		const FString Page = Args.Num() ? Args[0] : TEXT("inventory");
		if (Page == TEXT("none"))
		{
			S->Close();
		}
		else if (Page == TEXT("map") && Args.Num() > 1)
		{
			S->ShowOnMap(*Args[1]);
		}
		else
		{
			S->Open(Page == TEXT("map") ? ELyudScreenTab::Map : Page == TEXT("journal") || Page == TEXT("diary") ? ELyudScreenTab::Journal : ELyudScreenTab::Inventory);
			if (Page == TEXT("diary"))
			{
				S->ShowDiary();
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GScreenToast(TEXT("Screen.Toast"), TEXT("Screen.Toast <key>: shows a string key's text as a HUD note"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudGameScreen* S = ULyudGameScreen::Get(World); S && Args.Num())
		{
			S->Toast(LyudText::Get(Args[0]), false);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GWear(TEXT("Inventory.Wear"), TEXT("Inventory.Wear <item id> [n]: gives n (default 1) and puts it on; Inventory.Wear -<slot number> takes a slot off"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudInventory* Inv = ULyudInventory::Get(World);
		if (!Inv || !Args.Num())
		{
			return;
		}
		if (Args[0].StartsWith(TEXT("-")))
		{
			Inv->TakeOff(static_cast<ELyudEquipSlot>(FCString::Atoi(*Args[0].Mid(1))));
			return;
		}
		if (Inv->Count(*Args[0]) < 1)
		{
			Inv->Add(*Args[0], Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1);
		}
		Inv->Wear(*Args[0]);
	}));
