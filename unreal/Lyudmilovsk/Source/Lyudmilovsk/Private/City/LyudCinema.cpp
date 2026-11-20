#include "City/LyudCinema.h"

#include "CinemaModel.h"
#include "City/SLyudCityPage.h"
#include "City/LyudCityThings.h"
#include "Audio/LyudSoundscape.h"
#include "Dev/LyudDev.h"
#include "Festival/LyudFestival.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Plants/GameCalendar.h"
#include "UI/LyudGameScreen.h"
#include "World/LyudCityBuilding.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileMediaSource.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudCinema, Log, All);

namespace LyudCinemaImpl
{
	const TCHAR* DataFile = TEXT("City/cinema.json");
	const TCHAR* FilmsDir = TEXT("Films");
	const TCHAR* ScreenMat = TEXT("/Game/Cinema/M_FilmScreen.M_FilmScreen");
	const TCHAR* BeamMat = TEXT("/Game/Cinema/M_FilmBeam.M_FilmBeam");
	const FName SaveKey(TEXT("Cinema"));
	constexpr double CardSeconds = 4.0;       // the title card before each film
	constexpr double OpenTimeout = 10.0;      // a copy that will not start is skipped
	constexpr double StallSeconds = 1.5;      // stopped this long without the end reached: over
	constexpr float LineSeconds = 7.f;        // how long a lecturer's line stays
	constexpr float LampFloorCm = 900.f;      // the hall's lamps: up to the ceiling over the middle
	constexpr float PictureGapCm = 2.f;       // the picture in front of the screen's own card
	constexpr float PortholeCm = 14.f;        // the beam's mouth at the booth
	constexpr float LooseWalkAway = 3000.f;   // a console run ends when he wanders off this far

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	FVector Vec(const double P[3]) { return FVector(P[0], P[1], P[2]); }
}

ULyudCinema::ULyudCinema() = default;
ULyudCinema::~ULyudCinema() = default;

ULyudCinema* ULyudCinema::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudCinema>() : nullptr;
}

void ULyudCinema::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(LyudCinemaImpl::SaveKey, this);
	}
}

void ULyudCinema::Deinitialize()
{
	if (Running >= 0)
	{
		// the world is going: no handing back to the city's things, they go too
		bLooseRun = true;
		Stop(false);
	}
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(this))
	{
		Saves->UnregisterSaveObject(LyudCinemaImpl::SaveKey);
	}
	Super::Deinitialize();
}

bool ULyudCinema::LoadModel()
{
	if (bLoaded)
	{
		return Model.IsValid();
	}
	bLoaded = true;
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / LyudCinemaImpl::DataFile;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudCinema, Display, TEXT("Cinema: no %s: no newsreel"), LyudCinemaImpl::DataFile);
		return false;
	}
	TPimplPtr<LyudFilm::FModel> M = MakePimpl<LyudFilm::FModel>();
	if (!M->Load(std::string(TCHAR_TO_UTF8(*Text))))
	{
		UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: %s"), *LyudCinemaImpl::Str(M->LastError));
		return false;
	}
	Model = MoveTemp(M);
	int32 Have = 0;
	for (int32 I = 0; I < (int32)Model->Films.size(); ++I)
	{
		Have += IFileManager::Get().FileExists(*FilmPath(I)) ? 1 : 0;
	}
	UE_LOG(LogLyudCinema, Display, TEXT("Cinema: %d films (%d fetched into Content/Films), %d cinemas"), (int32)Model->Films.size(), Have,
		(int32)Model->Cinemas.size());
	return true;
}

FString ULyudCinema::FilmPath(int32 Film) const
{
	return Model && Film >= 0 && Film < (int32)Model->Films.size()
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / LyudCinemaImpl::FilmsDir / (LyudCinemaImpl::Str(Model->Films[(size_t)Film].Id) + TEXT(".mp4")))
		: FString();
}

TArray<int32> ULyudCinema::TodaysFilms(int32 Cinema) const
{
	TArray<int32> Out;
	UWorld* World = GetWorld();
	if (!Model || !World || Cinema < 0)
	{
		return Out;
	}
	const int32 Weekday = (int32)UGameCalendar::NowIn(World).GetDayOfWeek();     // Monday 0
	const ULyudFestivalSubsystem* Fest = ULyudFestivalSubsystem::Get(World);
	for (int I : Model->ProgrammeFor(Weekday, Fest && Fest->IsFestivalToday()))
	{
		Out.Add(I);
	}
	return Out;
}

bool ULyudCinema::Begin(FName Plot, FName Act, bool bSeated)
{
	if (Running >= 0 || !LoadModel())
	{
		return false;
	}
	const int32 Cinema = Model->CinemaFor(TCHAR_TO_UTF8(*Plot.ToString()), TCHAR_TO_UTF8(*Act.ToString()));
	if (Cinema < 0)
	{
		return false;
	}
	const TArray<int32> Day = TodaysFilms(Cinema);
	TArray<int32> Have;
	for (int32 I : Day)
	{
		if (IFileManager::Get().FileExists(*FilmPath(I)))
		{
			Have.Add(I);
		}
	}
	if (Have.Num() == 0)
	{
		UE_LOG(LogLyudCinema, Display, TEXT("Cinema: today's %d films are not in Content/Films (tools/films/fetch_films.py): straight to the feature"), Day.Num());
		if (Day.Num() > 0)
		{
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(TEXT("city.film.missing")));
			}
		}
		return false;
	}
	return StartFilms(Cinema, Have, bSeated, false);
}

bool ULyudCinema::PlayLoose(FName FilmId)
{
	if (Running >= 0 || !LoadModel() || Model->Cinemas.empty())
	{
		return false;
	}
	TArray<int32> Films;
	if (!FilmId.IsNone())
	{
		const int32 I = Model->FindFilm(TCHAR_TO_UTF8(*FilmId.ToString()));
		if (I < 0)
		{
			UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: no film %s"), *FilmId.ToString());
			return false;
		}
		Films.Add(I);
	}
	else
	{
		Films = TodaysFilms(0);
	}
	Films.RemoveAll([this](int32 I) { return !IFileManager::Get().FileExists(*FilmPath(I)); });
	if (Films.Num() == 0)
	{
		UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: nothing to play: run tools/films/fetch_films.py"));
		return false;
	}
	return StartFilms(0, Films, false, true);
}

bool ULyudCinema::StartFilms(int32 Cinema, const TArray<int32>& Films, bool bSeated, bool bLoose)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	Running = Cinema;
	Queue = Films;
	QueueAt = 0;
	bSeatedRun = bSeated;
	bLooseRun = bLoose;
	StartFrame = GFrameCounter;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	StartAt = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	const LyudFilm::FCinema& C = Model->Cinemas[(size_t)Cinema];
	// the hall goes dark for the films (back to the hours when they end or he leaves)
	if (ULyudWindowLights* Lights = ULyudWindowLights::Get(World))
	{
		const int32 N = Lights->SwitchLampsNear(LyudCinemaImpl::Vec(C.Hall), (float)C.HallRadius, LyudCinemaImpl::LampFloorCm, false);
		UE_LOG(LogLyudCinema, Display, TEXT("Cinema: %d lamps out in %s"), N, *LyudCinemaImpl::Str(C.Plot));
	}
	if (!Player)
	{
		Player = NewObject<UMediaPlayer>(this, NAME_None, RF_Transient);
		Player->PlayOnOpen = true;
		FilmTexture = NewObject<UMediaTexture>(this, NAME_None, RF_Transient);
		FilmTexture->AutoClear = true;
		FilmTexture->ClearColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.f);
		FilmTexture->SetMediaPlayer(Player);
		FilmTexture->UpdateResource();
		Source = NewObject<UFileMediaSource>(this, NAME_None, RF_Transient);
	}
	BuildRig(Cinema);
	UE_LOG(LogLyudCinema, Display, TEXT("Cinema: the newsreel, %d films%s"), Queue.Num(), bLoose ? TEXT(" (no session)") : TEXT(""));
	StartCard();
	return true;
}

void ULyudCinema::BuildRig(int32 Cinema)
{
	UWorld* World = GetWorld();
	if (!World || Rig)
	{
		return;
	}
	const LyudFilm::FCinema& C = Model->Cinemas[(size_t)Cinema];
	const double Yaw = FMath::DegreesToRadians(C.ScreenYaw);
	const FVector Facing(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);           // into the hall
	const FVector Right(FMath::Sin(Yaw), -FMath::Cos(Yaw), 0.0);           // the right hand of one who looks at the screen
	const FVector Centre = LyudCinemaImpl::Vec(C.Screen) + Facing * LyudCinemaImpl::PictureGapCm;
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Rig = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Centre), P);
	if (!Rig)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Rig, TEXT("Root"));
	Rig->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldLocation(Centre);
	const FVector HalfW = Right * (C.ScreenW * 0.5);
	const FVector HalfH = FVector::UpVector * (C.ScreenH * 0.5);
	// corners relative to the rig: top-left, top-right, bottom-right, bottom-left (the picture's UV 0,0 at the top-left)
	const TArray<FVector> Quad = { -HalfW + HalfH, HalfW + HalfH, HalfW - HalfH, -HalfW - HalfH };
	auto MakeMesh = [this, Root](const TCHAR* Name)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(Rig, Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCastShadow(false);
		M->bAffectDistanceFieldLighting = false;
		M->SetCanEverAffectNavigation(false);
		M->RegisterComponent();
		M->SetRelativeTransform(FTransform::Identity);
		return M;
	};
	// the picture
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, LyudCinemaImpl::ScreenMat, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		ScreenMesh = MakeMesh(TEXT("FilmPicture"));
		const TArray<FVector> N = { Facing, Facing, Facing, Facing };
		const TArray<FVector2D> UV = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) };
		ScreenMesh->CreateMeshSection_LinearColor(0, Quad, { 0, 2, 1, 0, 3, 2 }, N, UV, {}, {}, false);
		ScreenMID = UMaterialInstanceDynamic::Create(Mat, ScreenMesh);
		ScreenMID->SetTextureParameterValue(TEXT("Film"), FilmTexture);
		ScreenMID->SetScalarParameterValue(TEXT("Brightness"), 0.15f);
		ScreenMesh->SetMaterial(0, ScreenMID);
	}
	else
	{
		UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: no %s (unreal/scripts/make_cinema_materials.py): the films run as subtitles only"), LyudCinemaImpl::ScreenMat);
	}
	// the beam: a pyramid from the booth's porthole to the picture's corners, brighter at the booth (UV.y 0) than on the screen (1)
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, LyudCinemaImpl::BeamMat, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		BeamMesh = MakeMesh(TEXT("FilmBeam"));
		const FVector Booth = LyudCinemaImpl::Vec(C.Booth) - Centre;
		const FVector Axis = (FVector::ZeroVector - Booth).GetSafeNormal();
		const FVector BR = FVector::CrossProduct(FVector::UpVector, Axis).GetSafeNormal() * LyudCinemaImpl::PortholeCm * 0.5;
		const FVector BU = FVector::CrossProduct(Axis, BR).GetSafeNormal() * LyudCinemaImpl::PortholeCm * 0.5;
		const TArray<FVector> Mouth = { Booth - BR + BU, Booth + BR + BU, Booth + BR - BU, Booth - BR - BU };
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		for (int32 S = 0; S < 4; ++S)
		{
			const int32 A = S, B = (S + 1) % 4;
			const int32 Base = V.Num();
			V.Append({ Mouth[A], Mouth[B], Quad[B], Quad[A] });
			const FVector Nn = FVector::CrossProduct(Quad[B] - Mouth[A], Quad[A] - Mouth[B]).GetSafeNormal();
			N.Append({ Nn, Nn, Nn, Nn });
			UV.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) });
			T.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}
		BeamMesh->CreateMeshSection_LinearColor(0, V, T, N, UV, {}, {}, false);
		BeamMID = UMaterialInstanceDynamic::Create(Mat, BeamMesh);
		BeamMID->SetScalarParameterValue(TEXT("Intensity"), 0.f);
		BeamMesh->SetMaterial(0, BeamMID);
	}
	// the projector at the booth
	USceneComponent* BoothPoint = NewObject<USceneComponent>(Rig, TEXT("Booth"));
	BoothPoint->SetupAttachment(Root);
	BoothPoint->RegisterComponent();
	BoothPoint->SetWorldLocation(LyudCinemaImpl::Vec(C.Booth));
	if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
	{
		Whirr = Sound->AttachLoop(TEXT("projector_whirr"), BoothPoint, 0.35f);
	}
}

void ULyudCinema::FreeRig()
{
	if (Player)
	{
		Player->Close();
	}
	if (Whirr)
	{
		if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
		{
			Sound->ReleaseLoop(Whirr, 0.6f);
		}
		Whirr = nullptr;
	}
	if (Rig)
	{
		Rig->Destroy();
	}
	Rig = nullptr;
	ScreenMesh = nullptr;
	BeamMesh = nullptr;
	ScreenMID = nullptr;
	BeamMID = nullptr;
}

void ULyudCinema::StartCard()
{
	UWorld* World = GetWorld();
	if (!World || QueueAt < 0 || QueueAt >= Queue.Num())
	{
		Stop(true);
		return;
	}
	const LyudFilm::FFilm& F = Model->Films[(size_t)Queue[QueueAt]];
	Phase = EPhase::Card;
	PhaseStart = World->GetTimeSeconds();
	Caption = -2;
	PlayedShare = 0.f;
	if (ScreenMID)
	{
		ScreenMID->SetScalarParameterValue(TEXT("Brightness"), 0.15f);
	}
	if (BeamMID)
	{
		BeamMID->SetScalarParameterValue(TEXT("Intensity"), 0.35f);
	}
	ShowCaption(LyudText::Get(TEXT("city.film.title")), LyudText::Get(TEXT("city.film.card"), {
		FStringFormatArg(LyudText::Get(LyudCinemaImpl::Str(F.Title)).ToString()),
		FStringFormatArg(LyudText::Get(LyudCinemaImpl::Str(F.Maker)).ToString()),
		FStringFormatArg(FString::FromInt(F.Year)) }));
}

void ULyudCinema::StartFilm()
{
	UWorld* World = GetWorld();
	const FString Path = FilmPath(Queue[QueueAt]);
	Phase = EPhase::Opening;
	PhaseStart = World ? World->GetTimeSeconds() : 0.0;
	LastProgress = PhaseStart;
	HideCaption();
	Caption = -1;
	Source->SetFilePath(Path);
	if (!Player->OpenSource(Source))
	{
		UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: cannot open %s"), *Path);
		EndFilm(false);
		return;
	}
	Player->SetLooping(false);
	if (ScreenMID)
	{
		ScreenMID->SetScalarParameterValue(TEXT("Brightness"), 1.f);
	}
	if (BeamMID)
	{
		BeamMID->SetScalarParameterValue(TEXT("Intensity"), 1.f);
	}
}

void ULyudCinema::EndFilm(bool bWatched)
{
	if (QueueAt >= 0 && QueueAt < Queue.Num() && bWatched)
	{
		const FName Id(*LyudCinemaImpl::Str(Model->Films[(size_t)Queue[QueueAt]].Id));
		if (!SeenFilms.Contains(Id))
		{
			SeenFilms.Add(Id);
			int32 Seen = 0;
			for (const LyudFilm::FFilm& F : Model->Films)
			{
				Seen += SeenFilms.Contains(FName(*LyudCinemaImpl::Str(F.Id))) ? 1 : 0;
			}
			if (ULyudGameScreen* Screen = ULyudGameScreen::Get(this))
			{
				Screen->Toast(LyudText::Get(TEXT("city.film.seen"), { FStringFormatArg(Seen), FStringFormatArg((int32)Model->Films.size()) }));
			}
		}
	}
	if (Player)
	{
		Player->Close();
	}
	++QueueAt;
	if (QueueAt >= Queue.Num())
	{
		Stop(true);
		return;
	}
	StartCard();
}

void ULyudCinema::Stop(bool bFinished)
{
	if (Running < 0)
	{
		return;
	}
	Running = -1;
	Queue.Reset();
	QueueAt = -1;
	HideCaption();
	FreeRig();
	if (ULyudWindowLights* Lights = ULyudWindowLights::Get(this))
	{
		Lights->ClearSwitched();
	}
	UE_LOG(LogLyudCinema, Display, TEXT("Cinema: the newsreel %s"), bFinished ? TEXT("is over") : TEXT("left"));
	if (!bLooseRun)
	{
		if (ULyudCityThings* Things = ULyudCityThings::Get(this))
		{
			Things->ReleaseHeldShow(bFinished);
		}
	}
	bLooseRun = false;
}

void ULyudCinema::ShowCaption(const FText& Title, const FText& Body)
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
		Page->SetScreen(true);
	}
	Page->SetPage(Title, Body, LyudText::Get(TEXT("city.film.leave")));
}

void ULyudCinema::HideCaption()
{
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

void ULyudCinema::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (Running < 0 || !World || !Model)
	{
		return;
	}
	// he leaves: E or Esc, up from the seat, a screen comes up, walks off (a console run)
	AHeroCharacter* Hero = Cast<AHeroCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	const bool bKey = PC && GFrameCounter != StartFrame && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Escape));
	if (!Hero || !PC || bKey || (UI && UI->IsOpen()) || (bSeatedRun && !Hero->IsSeated())
		|| (bLooseRun && FVector::Dist(Hero->GetActorLocation(), StartAt) > LyudCinemaImpl::LooseWalkAway))
	{
		Stop(false);
		return;
	}
	const double Now = World->GetTimeSeconds();
	const LyudFilm::FFilm& F = Model->Films[(size_t)Queue[QueueAt]];
	if (Phase == EPhase::Card)
	{
		if (Now - PhaseStart >= LyudCinemaImpl::CardSeconds)
		{
			StartFilm();
		}
		return;
	}
	const double Duration = Player ? Player->GetDuration().GetTotalSeconds() : 0.0;
	const double Time = Player ? Player->GetTime().GetTotalSeconds() : 0.0;
	if (Phase == EPhase::Opening)
	{
		if (Player && Player->IsPlaying() && Duration > 0.0)
		{
			Phase = EPhase::Playing;
			LastProgress = Now;
			UE_LOG(LogLyudCinema, Display, TEXT("Cinema: %s, %.0f s"), *LyudCinemaImpl::Str(F.Id), Duration);
		}
		else if (Now - PhaseStart > LyudCinemaImpl::OpenTimeout)
		{
			UE_LOG(LogLyudCinema, Warning, TEXT("Cinema: %s did not start in %.0f s: skipped"), *LyudCinemaImpl::Str(F.Id), LyudCinemaImpl::OpenTimeout);
			EndFilm(false);
		}
		return;
	}
	// playing: the share of the film decides the lecturer's line and the laughs, whatever the copy's length
	const float Share = Duration > 0.0 ? (float)FMath::Clamp(Time / Duration, 0.0, 1.0) : 0.f;
	if (Player->IsPlaying())
	{
		LastProgress = Now;
	}
	if (Share > PlayedShare)
	{
		if (LyudFilm::FModel::LaughsIn(F, PlayedShare, Share) > 0)
		{
			if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
			{
				Sound->PlayEffect(TEXT("cinema_laugh"), LyudCinemaImpl::Vec(Model->Cinemas[(size_t)Running].Hall), 0.8f);
			}
		}
		PlayedShare = Share;
	}
	const int32 Line = LyudFilm::FModel::LectorAt(F, Share, LyudFilm::FModel::HoldShare((float)Duration, LyudCinemaImpl::LineSeconds));
	if (Line != Caption)
	{
		Caption = Line;
		if (Line >= 0)
		{
			ShowCaption(LyudText::Get(LyudCinemaImpl::Str(F.Title)), LyudText::Get(TEXT("city.film.lector"),
				{ FStringFormatArg(LyudText::Get(LyudCinemaImpl::Str(F.Lector[(size_t)Line].Key)).ToString()) }));
		}
		else
		{
			HideCaption();
		}
	}
	if ((Duration > 0.0 && Time >= Duration - 0.15) || Now - LastProgress > LyudCinemaImpl::StallSeconds)
	{
		EndFilm(Share > 0.8f || Time >= Duration - 0.15);
	}
}

FString ULyudCinema::Status()
{
	if (!LoadModel())
	{
		return TEXT("Cinema: no model (Content/City/cinema.json)");
	}
	FString S = FString::Printf(TEXT("Cinema: %s, seen %d of %d\n"), Running >= 0 ? TEXT("running") : TEXT("idle"), SeenFilms.Num(), (int32)Model->Films.size());
	for (int32 I = 0; I < (int32)Model->Films.size(); ++I)
	{
		const LyudFilm::FFilm& F = Model->Films[(size_t)I];
		S += FString::Printf(TEXT("  %-20s %d  %s%s\n"), *LyudCinemaImpl::Str(F.Id), F.Year,
			IFileManager::Get().FileExists(*FilmPath(I)) ? TEXT("fetched") : TEXT("MISSING"),
			SeenFilms.Contains(FName(*LyudCinemaImpl::Str(F.Id))) ? TEXT(", seen") : TEXT(""));
	}
	const TArray<int32> Day = TodaysFilms(0);
	S += TEXT("  today:");
	for (int32 I : Day)
	{
		S += TEXT(" ") + LyudCinemaImpl::Str(Model->Films[(size_t)I].Id);
	}
	return S;
}

// --- console and dev mode

static FAutoConsoleCommandWithWorldAndArgs GCinemaStatus(TEXT("lyud.Cinema.Status"),
	TEXT("lyud.Cinema.Status: the Rassvet's films (fetched or missing, seen), today's newsreel"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudCinema* C = ULyudCinema::Get(World))
		{
			UE_LOG(LogLyudCinema, Display, TEXT("%s"), *C->Status());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCinemaPlay(TEXT("lyud.Cinema.Play"),
	TEXT("lyud.Cinema.Play [film id]: today's newsreel (or one film) at the Rassvet now, without a session; E or Esc stops it"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (ULyudCinema* C = ULyudCinema::Get(World))
		{
			const bool bOk = C->PlayLoose(Args.Num() > 0 ? FName(*Args[0]) : NAME_None);
			UE_LOG(LogLyudCinema, Display, TEXT("lyud.Cinema.Play: %s"), bOk ? TEXT("running") : TEXT("nothing to play (lyud.Cinema.Status)"));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCinemaStop(TEXT("lyud.Cinema.Stop"),
	TEXT("lyud.Cinema.Stop: ends the newsreel as if it ran out (a session's feature follows)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (ULyudCinema* C = ULyudCinema::Get(World))
		{
			C->Stop(true);
		}
	}));

static LyudDev::FAutoAction GCinemaDevPlay(TEXT("cinema.play"), TEXT("interiors"), TEXT("dev.action.cinema.play"), [](UWorld* W)
{
	if (ULyudCinema* C = ULyudCinema::Get(W))
	{
		C->PlayLoose(NAME_None);
	}
});
