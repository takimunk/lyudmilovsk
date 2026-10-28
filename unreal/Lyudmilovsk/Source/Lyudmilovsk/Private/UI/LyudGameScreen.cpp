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
#include "Audio/LyudSoundscape.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HeroReachComponent.h"
#include "Slate/WidgetRenderer.h"
#include "UI/LyudDiaryBook.h"
#include "UI/SLyudDiary.h"
#include "UI/SLyudHud.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHud(TEXT("lyud.Hud"), 1, TEXT("The HUD: the date and time, the followed quest, short notes (0 = hidden)."));

	TAutoConsoleVariable<int32> CVarBinoculars(TEXT("lyud.BinocularsHold"), 0, TEXT("Holds the binoculars up as if the right mouse button were down (headless checks)."));

	const float BinocularsZoom = 5.f;

	TAutoConsoleVariable<float> CVarDiarySpeed(TEXT("lyud.Diary.Speed"), 1.f, TEXT("How fast the hero takes the diary out, opens it and puts it away (2 = twice as fast)."));
	TAutoConsoleVariable<float> CVarDiaryHeight(TEXT("lyud.Diary.Height"), 0.84f, TEXT("How much of the screen's height the open diary's pages take."));
	TAutoConsoleVariable<float> CVarDiaryBright(TEXT("lyud.Diary.PageBright"), 1.f, TEXT("The brightness of the pages on the 3D book (1: as the screen draws them)."));

	/** Seconds of each phase of the diary (EPhase order); with no book (a bird) only the fades. */
	float PhaseLength(ULyudGameScreen::EPhase P, bool bBook)
	{
		using E = ULyudGameScreen::EPhase;
		switch (P)
		{
		case E::TakeOut: return 0.55f;
		case E::Opening: return 0.5f;
		case E::FadeIn: return bBook ? 0.18f : 0.22f;
		case E::FadeOut: return bBook ? 0.14f : 0.2f;
		case E::Closing: return 0.35f;
		case E::PutAway: return 0.45f;
		default: return 0.f;
		}
	}

	float Ease(float T)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f));
	}

	/** The book in the eyes' frame (forward, right, up; cm) at the three keys of its way: at the hip, shut in front, open. */
	struct FBookKey { FVector Pos; FRotator Rot; };
	FBookKey HipKey() { return { FVector(24.f, 34.f, -56.f), FRotator(-10.f, 180.f + 75.f, 30.f) }; }
	FBookKey ShutKey(float Dist) { return { FVector(Dist + 5.f, -6.5f, -4.f), FRotator(-6.f, 180.f + 16.f, 4.f) }; }
	FBookKey OpenKey(float Dist) { return { FVector(Dist, 0.f, 0.f), FRotator(0.f, 180.f, 0.f) }; }

	FBookKey Blend(const FBookKey& A, const FBookKey& B, float T)
	{
		return { FMath::Lerp(A.Pos, B.Pos, T), FQuat::Slerp(A.Rot.Quaternion(), B.Rot.Quaternion(), T).Rotator() };
	}
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
			if (Screen && !Screen->IsTurning()) { Screen->Rebuild(); }
		});
	}
	if (ULyudInventory* Inv = ULyudInventory::Get(&InWorld))
	{
		Inv->OnAdded.AddUObject(this, &ULyudGameScreen::OnItemAdded);
		Inv->OnChanged.AddWeakLambda(this, [this] { if (Screen && !Screen->IsTurning()) { Screen->Rebuild(); } });
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
	AfterClose.Reset();
	Finish();
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
	if (IsOpen())
	{
		if (Screen && Phase == EPhase::Reading)
		{
			Screen->TurnTo(Screen->SpreadFor(Tab));
		}
		else if (Screen && Phase < EPhase::Reading)
		{
			// not readable yet: the book just opens there
			Screen->SetSpread(Screen->SpreadFor(Tab));
		}
		return;
	}
	Screen = SNew(SLyudDiaryScreen).Owner(this);
	Screen->SetSpread(Screen->SpreadFor(Tab));
	Screen->SetAlpha(0.f);
	// over the interactables' own prompts
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 85);
	UI->Lock(true);

	APlayerController* PC = World->GetFirstPlayerController();
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	bWithBook = Hero && !Hero->IsHidden();
	if (bWithBook)
	{
		if (!Book)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			Book = World->SpawnActor<ALyudDiaryBook>(ALyudDiaryBook::StaticClass(), FTransform(Hero->EyeLocation()), Params);
		}
		Book->SetCover(180.f);
		Book->SetPages(nullptr);
		Book->SetPageBrightness(CVarDiaryBright.GetValueOnGameThread());
		Book->SetTabs(Screen->TabsOnLeft());
		// the eyes go down to the book (a look far up or down comes back to a reading angle), and back up after
		PitchBefore = FRotator::NormalizeAxis(PC->GetControlRotation().Pitch);
		PitchRead = FMath::Clamp(PitchBefore, -30.f, -10.f);
		SetPhase(EPhase::TakeOut);
		PlaceBook(0.f);
		PlayDiarySound(TEXT("diary_take"));
	}
	else
	{
		SetPhase(EPhase::FadeIn);
	}
	UE_LOG(LogTemp, Log, TEXT("GameScreen: diary out on tab %d (%s)"), (int32)Tab, bWithBook ? TEXT("in the hand") : TEXT("no hands"));
}

void ULyudGameScreen::Close()
{
	if (!IsOpen() || Phase >= EPhase::FadeOut)
	{
		return;
	}
	if (Book && Screen)
	{
		// the book shows the pages as they are now while it shuts
		Screen->DrawSpread(Screen->CurrentSpread(), PictureTarget(2));
		Book->SetPages(PictureTarget(2));
		Book->SetTabs(Screen->TabsOnLeft());
	}
	SetPhase(EPhase::FadeOut);
}

void ULyudGameScreen::CloseThen(TFunction<void()> Then)
{
	if (!IsOpen())
	{
		Then();
		return;
	}
	AfterClose.Add(MoveTemp(Then));
	Close();
}

void ULyudGameScreen::Finish()
{
	const bool bWas = Phase != EPhase::Closed || Screen.IsValid();
	Phase = EPhase::Closed;
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport(); Viewport && Screen)
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World); UI && bWas)
		{
			UI->Lock(false);
		}
		if (UHeroReachComponent* Reach = UHeroReachComponent::GetForPlayer(World))
		{
			Reach->ClearOverrideHold();
		}
	}
	Screen.Reset();
	if (Book)
	{
		Book->Destroy();
		Book = nullptr;
	}
	TArray<TFunction<void()>> Then = MoveTemp(AfterClose);
	AfterClose.Reset();
	for (TFunction<void()>& F : Then)
	{
		F();
	}
}

void ULyudGameScreen::SetPhase(EPhase InPhase)
{
	Phase = InPhase;
	PhaseTime = 0.f;
	if (Phase == EPhase::Opening && Book && Screen)
	{
		// the pages it falls open on
		Screen->DrawSpread(Screen->CurrentSpread(), PictureTarget(2));
		Book->SetPages(PictureTarget(2));
		Book->SetTabs(Screen->TabsOnLeft());
		PlayDiarySound(TEXT("diary_open"));
	}
	else if (Phase == EPhase::Closing)
	{
		PlayDiarySound(TEXT("diary_close"));
	}
}

UTextureRenderTarget2D* ULyudGameScreen::PictureTarget(int32 Index)
{
	const FIntPoint Size = SLyudDiaryScreen::PictureSize();
	while (Pictures.Num() <= Index)
	{
		Pictures.Add(nullptr);
	}
	UTextureRenderTarget2D* T = Pictures[Index];
	if (!T || T->SizeX != Size.X || T->SizeY != Size.Y)
	{
		T = FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Trilinear, false);
		if (T)
		{
			T->ClearColor = FLinearColor(0.95f, 0.93f, 0.87f, 1.f);
		}
		Pictures[Index] = T;
	}
	return T;
}

void ULyudGameScreen::OnSpreadShown()
{
	if (Book && Screen)
	{
		Book->SetTabs(Screen->TabsOnLeft());
	}
}

void ULyudGameScreen::PlayDiarySound(FName Pool)
{
	ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this);
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Sound && Pawn)
	{
		Sound->PlayEffect(Pool, Book ? Book->GetActorLocation() : Pawn->GetActorLocation(), 0.8f);
	}
}

void ULyudGameScreen::PlaceBook(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AHeroCharacter* Hero = PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	if (!Book || !Hero || !PC->PlayerCameraManager)
	{
		return;
	}
	// the eyes' frame as the hero's camera has it (HeroCharacter::CalcCamera: the eyes, the look)
	const FRotator Look = PC->GetControlRotation();
	const FTransform Eye(FRotator(FRotator::NormalizeAxis(Look.Pitch), Look.Yaw, 0.f), Hero->EyeLocation());
	FVector2D ViewPx(1920.0, 1080.0);
	if (World->GetGameViewport())
	{
		World->GetGameViewport()->GetViewportSize(ViewPx);
	}
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(PC->PlayerCameraManager->GetFOVAngle(), 20.f, 170.f) * 0.5f));
	const float Focal = ViewPx.X * 0.5f / HalfTan;       // pixels per unit of (sideways / forward)
	const float Height = FMath::Clamp(CVarDiaryHeight.GetValueOnGameThread(), 0.3f, 1.f);
	const float Dist = ALyudDiaryBook::PageH * Focal / (Height * ViewPx.Y);

	const float T = PhaseLength(Phase, true) > 0.f ? FMath::Clamp(PhaseTime / PhaseLength(Phase, true), 0.f, 1.f) : 1.f;
	FBookKey K = OpenKey(Dist);
	float Cover = 0.f;
	float Pitch = PitchRead;
	switch (Phase)
	{
	case EPhase::TakeOut:
		K = Blend(HipKey(), ShutKey(Dist), Ease(T));
		Cover = 180.f;
		Pitch = FMath::Lerp(PitchBefore, PitchRead, Ease(T * 1.4f));
		break;
	case EPhase::Opening:
		// it falls open: quick at first, settling flat
		K = Blend(ShutKey(Dist), OpenKey(Dist), Ease(T));
		Cover = 180.f * (1.f - FMath::Sin(FMath::Clamp(T, 0.f, 1.f) * HALF_PI));
		break;
	case EPhase::Closing:
		K = Blend(OpenKey(Dist), ShutKey(Dist), Ease(T));
		Cover = 180.f * Ease(T);
		break;
	case EPhase::PutAway:
		K = Blend(ShutKey(Dist), HipKey(), Ease(T));
		Cover = 180.f;
		Pitch = FMath::Lerp(PitchRead, PitchBefore, Ease(T));
		break;
	default:
		break;
	}
	if (Phase == EPhase::TakeOut || Phase == EPhase::PutAway)
	{
		PC->SetControlRotation(FRotator(Pitch, Look.Yaw, 0.f));
	}
	Book->SetCover(Cover);
	Book->SetActorTransform(FTransform(K.Rot, K.Pos) * Eye);

	// the right hand under it (UHeroReachComponent: the override hold, in the same frame)
	if (UHeroReachComponent* Reach = UHeroReachComponent::Get(Hero))
	{
		FVector Palm, Finger, Normal;
		Book->HoldFrame(Palm, Finger, Normal);
		const FTransform HoldFrame(FRotator(FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch), -45.f, 20.f), Look.Yaw, 0.f), Hero->EyeLocation());
		FHeroHoldPose H;
		H.Pos = HoldFrame.InverseTransformPosition(Palm);
		H.FingerDir = HoldFrame.InverseTransformVectorNoScale(Finger);
		H.PalmNormal = HoldFrame.InverseTransformVectorNoScale(Normal);
		H.PitchFollow = 1.f;
		H.Curl = 0.85f;
		H.BlendTime = 0.25f;
		const FTransform PalmFrame(FRotationMatrix::MakeFromXZ(Finger, Normal).ToQuat(), Palm);
		H.Shape = Book->Shape().Transformed(PalmFrame.Inverse());
		if (Phase == EPhase::PutAway && T > 0.7f)
		{
			Reach->ClearOverrideHold();
		}
		else
		{
			Reach->SetOverrideHold(H);
		}
	}

	// where the open pages are on the screen: the diary screen lies over them
	if (Screen)
	{
		FVector C[4];
		Book->SpreadCorners(C);
		FVector2D Min(1e9, 1e9), Max(-1e9, -1e9);
		bool bAll = true;
		for (const FVector& W : C)
		{
			const FVector L = Eye.InverseTransformPosition(W);
			if (L.X < 1.f)
			{
				bAll = false;
				break;
			}
			const FVector2D S(ViewPx.X * 0.5 + L.Y / L.X * Focal, ViewPx.Y * 0.5 - L.Z / L.X * Focal);
			Min = FVector2D::Min(Min, S);
			Max = FVector2D::Max(Max, S);
		}
		if (bAll)
		{
			Screen->SetPageRect(Min, Max - Min);
		}
	}
}

void ULyudGameScreen::UpdateDiary(float DeltaTime)
{
	if (Phase == EPhase::Closed)
	{
		return;
	}
	PhaseTime += DeltaTime * FMath::Clamp(CVarDiarySpeed.GetValueOnGameThread(), 0.1f, 10.f);
	const float Len = PhaseLength(Phase, bWithBook);
	const float T = Len > 0.f ? FMath::Clamp(PhaseTime / Len, 0.f, 1.f) : 1.f;
	if (bWithBook && !Book)
	{
		// the book went with the level or a seat hid the hero: the pages just fade
		bWithBook = false;
	}
	if (bWithBook)
	{
		PlaceBook(DeltaTime);
	}
	if (Screen)
	{
		Screen->SetAlpha(Phase == EPhase::FadeIn ? Ease(T) : Phase == EPhase::Reading ? 1.f : Phase == EPhase::FadeOut ? 1.f - Ease(T) : 0.f);
	}
	if (T < 1.f || Phase == EPhase::Reading)
	{
		return;
	}
	switch (Phase)
	{
	case EPhase::TakeOut: SetPhase(EPhase::Opening); break;
	case EPhase::Opening: SetPhase(EPhase::FadeIn); break;
	case EPhase::FadeIn: SetPhase(EPhase::Reading); break;
	case EPhase::FadeOut: bWithBook ? SetPhase(EPhase::Closing) : Finish(); break;
	case EPhase::Closing: SetPhase(EPhase::PutAway); break;
	case EPhase::PutAway: Finish(); break;
	default: break;
	}
}

void ULyudGameScreen::ShowOnMap(FName Place)
{
	if (Screen)
	{
		Screen->ShowPlace(Place);
	}
	Open(ELyudScreenTab::Map);
	if (Screen)
	{
		Screen->ShowPlace(Place);
	}
}

void ULyudGameScreen::ShowDiary()
{
	Open(ELyudScreenTab::Diary);
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
	const bool bFree = Hero && !IsOpen() && !(Menu && Menu->IsOpen()) && !UGameplayStatics::IsGamePaused(World);

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
	const bool bShow = CVarHud.GetValueOnGameThread() != 0 && !IsOpen() && !(Menu && Menu->IsOpen()) && !(UI && UI->IsShopOpen());
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

	// the keys: a section's key takes the diary out on it, turns to it, or (on it) puts the diary away
	ELyudScreenTab Want = Tab;
	bool bKey = false;
	if (PC->WasInputKeyJustPressed(EKeys::Tab) || PC->WasInputKeyJustPressed(EKeys::I)) { Want = ELyudScreenTab::Inventory; bKey = true; }
	else if (PC->WasInputKeyJustPressed(EKeys::J)) { Want = ELyudScreenTab::Journal; bKey = true; }
	else if (PC->WasInputKeyJustPressed(EKeys::M)) { Want = ELyudScreenTab::Map; bKey = true; }
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (IsOpen())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape) || (Menu && Menu->IsOpen()))
		{
			Close();
		}
		else if (bKey && Screen)
		{
			// the key of the section that is open shuts the diary; another one turns to its section
			const int32 To = Screen->SpreadFor(Want);
			const bool bHere = Screen->SpreadAt(Screen->CurrentSpread()).Section == Screen->SpreadAt(To).Section;
			if (bHere && Phase == EPhase::Reading && !Screen->IsTurning())
			{
				Close();
			}
			else if (!bHere)
			{
				Open(Want);
			}
		}
		else if (Phase == EPhase::Reading && Screen && !Screen->IsTurning())
		{
			KeysOnDiary(PC);
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
	UpdateDiary(DeltaTime);

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

bool ULyudGameScreen::KeysOnDiary(APlayerController* PC)
{
	// a page forward or back; C finds the hero on the map
	const bool bNext = PC->WasInputKeyJustPressed(EKeys::D) || PC->WasInputKeyJustPressed(EKeys::Right) || PC->WasInputKeyJustPressed(EKeys::PageDown);
	const bool bPrev = PC->WasInputKeyJustPressed(EKeys::A) || PC->WasInputKeyJustPressed(EKeys::Left) || PC->WasInputKeyJustPressed(EKeys::PageUp);
	if (bNext || bPrev)
	{
		const int32 To = Screen->CurrentSpread() + (bNext ? 1 : -1);
		if (Screen->TurnTo(To))
		{
			const LyudDiary::ESection S = Screen->SpreadAt(To).Section;
			Tab = S == LyudDiary::ESection::Things ? ELyudScreenTab::Inventory : S == LyudDiary::ESection::Quests ? ELyudScreenTab::Journal
				: S == LyudDiary::ESection::Diary ? ELyudScreenTab::Diary : ELyudScreenTab::Map;
		}
		return true;
	}
	if (PC->WasInputKeyJustPressed(EKeys::C) && Screen->SpreadAt(Screen->CurrentSpread()).Section == LyudDiary::ESection::Map)
	{
		Screen->CenterMapOnPlayer();
		return true;
	}
	return false;
}

// --- dev commands for headless checks ---
static FAutoConsoleCommandWithWorldAndArgs GScreenOpen(TEXT("Screen.Open"), TEXT("Screen.Open inventory|journal|map|diary|none [place]: takes the diary out on a section (map: centred on a place), or puts it away"),
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
			S->Open(Page == TEXT("map") ? ELyudScreenTab::Map : Page == TEXT("journal") ? ELyudScreenTab::Journal : Page == TEXT("diary") ? ELyudScreenTab::Diary
				: ELyudScreenTab::Inventory);
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
