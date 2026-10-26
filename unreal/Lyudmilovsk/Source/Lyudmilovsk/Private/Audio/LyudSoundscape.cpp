#include "Audio/LyudSoundscape.h"

#include "Animals/AnimalLife.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Birds/BirdPawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Animation/SkeletalMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "LyudAmbienceSynth.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sea/LyudSea.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSound(TEXT("lyud.Sound"), 1, TEXT("The world's sound and music (ULyudSoundscapeSubsystem): 1 on, 0 silent (for A/B)."));
	TAutoConsoleVariable<int32> CVarMusic(TEXT("lyud.Music"), 1, TEXT("1: ambient music between silences; 0: none."));
	TAutoConsoleVariable<float> CVarMusicGap(TEXT("lyud.Music.Gap"), 1.f, TEXT("Scale on the silences between music cues (0.1 = almost back to back)."));
	TAutoConsoleVariable<FString> CVarMusicMood(TEXT("lyud.Music.Mood"), TEXT(""), TEXT("Force the music's mood (club, dawn, day, dusk, night, rain, storm, snow, sea, flight); empty = by the moment."));
	TAutoConsoleVariable<int32> CVarDebug(TEXT("lyud.Sound.Debug"), 0, TEXT("1: print what the soundscape hears on screen."));
	TAutoConsoleVariable<int32> CVarBed(TEXT("lyud.Sound.Bed"), 1, TEXT("1: the procedural outdoor bed (wind, leaves, surf, rain, crickets, thunder); 0: off."));

	FAutoConsoleCommandWithWorld NextCommand(TEXT("lyud.Music.Next"), TEXT("Play a music cue for the current mood now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(World))
			{
				S->PlayNextCue();
			}
		}));

	// recording the game's own mix (tests, unreal/scripts/test_audio.py): what the player would hear, to WAV
	FAutoConsoleCommandWithWorld RecordCommand(TEXT("lyud.Sound.Record"), TEXT("Start recording the game's audio output."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UAudioMixerBlueprintLibrary::StartRecordingOutput(World, 0.f);
		}));
	FAutoConsoleCommandWithWorldAndArgs SaveCommand(TEXT("lyud.Sound.SaveRecording"),
		TEXT("Stop recording and write it: lyud.Sound.SaveRecording <name> <absolute dir> -> <dir>/<name>.wav"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Name = Args.Num() > 0 ? Args[0] : TEXT("recording");
			const FString Dir = Args.Num() > 1 ? Args[1] : FPaths::ProjectSavedDir() / TEXT("AudioTest");
			UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, Name, Dir);
		}));

	const TCHAR* Moods[] = { TEXT("club"), TEXT("dawn"), TEXT("day"), TEXT("dusk"), TEXT("night"), TEXT("rain"), TEXT("storm"), TEXT("snow"), TEXT("sea"), TEXT("flight") };

	/** Where a mood borrows cues from when its own have all just played (or it has none). */
	TArray<FName> MoodFallbacks(FName M)
	{
		static const TMap<FName, TArray<FName>> Map = {
			{ TEXT("dawn"), { TEXT("day"), TEXT("sea") } },
			{ TEXT("day"), { TEXT("dawn"), TEXT("sea") } },
			{ TEXT("sea"), { TEXT("day"), TEXT("dawn") } },
			{ TEXT("dusk"), { TEXT("night"), TEXT("day") } },
			{ TEXT("night"), { TEXT("dusk"), TEXT("snow") } },
			{ TEXT("snow"), { TEXT("night") } },
			{ TEXT("rain"), { TEXT("night"), TEXT("dusk") } },
			{ TEXT("storm"), { TEXT("rain") } },
			{ TEXT("club"), { TEXT("night") } },
			{ TEXT("flight"), { TEXT("day") } },
		};
		const TArray<FName>* F = Map.Find(M);
		return F ? *F : TArray<FName>();
	}

	float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }

	const TCHAR* Conifers[] = { TEXT("ScotsPine"), TEXT("DunePine"), TEXT("NorwaySpruce"), TEXT("Juniper") };
	bool IsConifer(FName Species)
	{
		for (const TCHAR* C : Conifers)
		{
			if (Species == FName(C))
			{
				return true;
			}
		}
		return false;
	}
	/** Aspens and poplars tremble in the least air. */
	float LeafLoudness(FName Species)
	{
		const FString S = Species.ToString();
		return S.Contains(TEXT("Aspen")) || S.Contains(TEXT("Poplar")) ? 1.6f : 1.f;
	}

	constexpr float TreeCell = 2000.f;   // cm

	ALyudSky* FindSky(UWorld* World)
	{
		TActorIterator<ALyudSky> It(World);
		return It ? *It : nullptr;
	}
}

ULyudSoundscapeSubsystem* ULyudSoundscapeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudSoundscapeSubsystem>() : nullptr;
}

bool ULyudSoundscapeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId ULyudSoundscapeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudSoundscapeSubsystem, STATGROUP_Tickables);
}

void ULyudSoundscapeSubsystem::SetVolumes(float Master, float Ambience, float Music, float Effects)
{
	MasterVolume = FMath::Clamp(Master, 0.f, 1.f);
	AmbienceVolume = FMath::Clamp(Ambience, 0.f, 1.f);
	MusicVolume = FMath::Clamp(Music, 0.f, 1.f);
	EffectsVolume = FMath::Clamp(Effects, 0.f, 1.f);
}

void ULyudSoundscapeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld() && InWorld.GetNetMode() != NM_DedicatedServer)
	{
		Start(InWorld);
	}
}

void ULyudSoundscapeSubsystem::Deinitialize()
{
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	Super::Deinitialize();
}

void ULyudSoundscapeSubsystem::Start(UWorld& World)
{
	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(&World, AActor::StaticClass(), TEXT("LyudSoundscape"));
	Params.ObjectFlags |= RF_Transient;
	Holder = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	Bed = NewObject<ULyudAmbienceSynth>(Holder, TEXT("Bed"));
	Bed->SetupAttachment(Root);
	Bed->RegisterComponent();
	Bed->Start();

	auto Make2D = [this, Root](const TCHAR* Name)
	{
		UAudioComponent* A = NewObject<UAudioComponent>(Holder, Name);
		A->SetupAttachment(Root);
		A->bAutoActivate = false;
		A->bAllowSpatialization = false;
		A->bIsUISound = false;
		A->RegisterComponent();
		return A;
	};
	MusicA = Make2D(TEXT("MusicA"));
	MusicB = Make2D(TEXT("MusicB"));
	TownLoop = Make2D(TEXT("TownLoop"));
	RoomTone = Make2D(TEXT("RoomTone"));

	// a close sound (a bark, a meow): full up to 3 m, natural fall-off to ~80 m, the top fades with distance
	NearAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = NearAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(300.f, 0.f, 0.f);
		A.FalloffDistance = 8000.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = 1000.f;
		A.LPFRadiusMax = 8000.f;
	}
	// a far sound placed a few hundred metres off in its direction: no fall-off of its own (it was made distant)
	FarAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = FarAttenuation->Attenuation;
		A.bAttenuate = false;
		A.bSpatialize = true;
		A.NonSpatializedRadiusStart = 0.f;
		A.NonSpatializedRadiusEnd = 0.f;
	}
	// a thing in a room (a keyboard, a console): heard across the room, gone two rooms away
	RoomAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = RoomAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
		A.FalloffDistance = 1600.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	}

	LoadSounds();

	if (UAnimalLifeSubsystem* Animals = World.GetSubsystem<UAnimalLifeSubsystem>())
	{
		Animals->OnAnimalEvent.AddDynamic(this, &ULyudSoundscapeSubsystem::OnAnimalEvent);
	}
	if (ULyudWeatherSubsystem* Weather = World.GetSubsystem<ULyudWeatherSubsystem>())
	{
		Weather->OnLightning.AddDynamic(this, &ULyudSoundscapeSubsystem::OnLightning);
	}
}

void ULyudSoundscapeSubsystem::LoadSounds()
{
	auto Load = [this](const FString& Path) -> USoundBase*
	{
		USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (S)
		{
			CueRefs.Add(S);
		}
		return S;
	};
	// pools: /Game/Audio/<group>/S_<id>_<nn>, the ids of audio/sounds.json
	static const TCHAR* Groups[][2] = {
		{ TEXT("Animals"), TEXT("dog_bark_big") }, { TEXT("Animals"), TEXT("dog_bark_medium") }, { TEXT("Animals"), TEXT("dog_bark_laika") },
		{ TEXT("Animals"), TEXT("dog_growl") }, { TEXT("Animals"), TEXT("dog_whine") }, { TEXT("Animals"), TEXT("dog_bark_far") },
		{ TEXT("Animals"), TEXT("cat_meow") }, { TEXT("Animals"), TEXT("cat_purr") }, { TEXT("Animals"), TEXT("cat_hiss") },
		{ TEXT("Far"), TEXT("ship_horn_far") }, { TEXT("Far"), TEXT("foghorn_far") }, { TEXT("Far"), TEXT("buoy_bell") }, { TEXT("Far"), TEXT("town_far") },
		{ TEXT("Club"), TEXT("club_room_tone") }, { TEXT("Club"), TEXT("keyboard_typing") }, { TEXT("Club"), TEXT("mouse_clicks") },
		{ TEXT("Club"), TEXT("console_game_far") }, { TEXT("Club"), TEXT("vending_hum") }, { TEXT("Club"), TEXT("vending_drop") },
		// the cable car (ULyudTransportSubsystem plays them through PlayEffect / AttachLoop)
		{ TEXT("Transport"), TEXT("cabin_motor") }, { TEXT("Transport"), TEXT("cabin_sheave") }, { TEXT("Transport"), TEXT("cabin_door") },
		{ TEXT("Transport"), TEXT("station_chime") }, { TEXT("Transport"), TEXT("station_wheel") }, { TEXT("Transport"), TEXT("lift_start") },
		{ TEXT("Transport"), TEXT("lift_ding") }, { TEXT("Transport"), TEXT("lift_hum") }, { TEXT("Transport"), TEXT("lift_doors") },
		{ TEXT("Transport"), TEXT("station_hall") },
	};
	for (const auto& G : Groups)
	{
		FPool& P = Pools.FindOrAdd(FName(G[1]));
		for (int32 N = 1; N <= 12; N++)
		{
			USoundBase* S = Load(FString::Printf(TEXT("/Game/Audio/%s/S_%s_%02d.S_%s_%02d"), G[0], G[1], N, G[1], N));
			if (!S)
			{
				break;
			}
			P.Sounds.Add(S);
		}
	}
	// music: /Game/Audio/Music/MU_<mood>_<n>
	for (const TCHAR* M : Moods)
	{
		for (int32 N = 1; N <= 9; N++)
		{
			const FString Name = FString::Printf(TEXT("MU_%s_%d"), M, N);
			if (USoundBase* S = Load(FString::Printf(TEXT("/Game/Audio/Music/%s.%s"), *Name, *Name)))
			{
				FCue& C = Cues.AddDefaulted_GetRef();
				C.Sound = S;
				C.Name = FName(*Name);
				C.Moods = { FName(M) };
				C.Seconds = S->GetDuration();
			}
		}
	}
	if (TownLoop)
	{
		if (USoundBase* S = Pick(TEXT("town_far")))
		{
			TownLoop->SetSound(S);
		}
	}
	if (RoomTone)
	{
		if (USoundBase* S = Pick(TEXT("club_room_tone")))
		{
			RoomTone->SetSound(S);
		}
	}
}

USoundBase* ULyudSoundscapeSubsystem::Pick(FName Pool)
{
	FPool* P = Pools.Find(Pool);
	if (!P || P->Sounds.Num() == 0)
	{
		return nullptr;
	}
	// never the same take twice in a row
	int32 I = FMath::RandRange(0, P->Sounds.Num() - 1);
	if (P->Sounds.Num() > 1 && I == P->Last)
	{
		I = (I + 1) % P->Sounds.Num();
	}
	P->Last = I;
	return P->Sounds[I];
}

void ULyudSoundscapeSubsystem::PlayAt(FName Pool, const FVector& Where, float Volume, USoundAttenuation* Attenuation, float PitchJitter)
{
	if (USoundBase* S = Pick(Pool))
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, Where, FRotator::ZeroRotator, Volume * MasterVolume * EffectsVolume,
			FMath::FRandRange(1.f - PitchJitter, 1.f + PitchJitter), 0.f, Attenuation);
	}
}

void ULyudSoundscapeSubsystem::PlayEffect(FName Pool, const FVector& Where, float Volume)
{
	PlayAt(Pool, Where, Volume, NearAttenuation, 0.03f);
}

UAudioComponent* ULyudSoundscapeSubsystem::AttachLoop(FName Pool, USceneComponent* Parent, float Volume)
{
	USoundBase* S = Pick(Pool);
	if (!S || !Parent)
	{
		return nullptr;
	}
	UAudioComponent* A = UGameplayStatics::SpawnSoundAttached(S, Parent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		true, Volume * MasterVolume * EffectsVolume, 1.f, 0.f, NearAttenuation, nullptr, false);
	return A;
}

void ULyudSoundscapeSubsystem::PlayVoiceAt(USoundBase* Voice, const FVector& Where, float Volume)
{
	if (Voice)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Voice, Where, FRotator::ZeroRotator, Volume * MasterVolume * EffectsVolume, 1.f, 0.f, NearAttenuation);
	}
}

float ULyudSoundscapeSubsystem::PanOf(const FVector& Where) const
{
	const FVector D = (Where - Listener).GetSafeNormal2D();
	return FMath::Clamp(float(FVector::DotProduct(D, ListenerRight.GetSafeNormal2D())), -1.f, 1.f);
}

float ULyudSoundscapeSubsystem::SunAltitude() const
{
	if (const ALyudSky* Sky = FindSky(GetWorld()))
	{
		return Sky->SunAltitude();
	}
	// no sky: by the clock (a rough sun for indoor levels)
	const FDateTime T = UGameCalendar::NowIn(GetWorld());
	return 40.f * FMath::Sin((T.GetTimeOfDay().GetTotalHours() - 7.f) / 12.f * PI) - 10.f;
}

float ULyudSoundscapeSubsystem::DayOfYear() const
{
	return UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()));
}

void ULyudSoundscapeSubsystem::UpdateListener()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		Listener = PC->PlayerCameraManager->GetCameraLocation();
		ListenerRight = PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector);
	}
	bBirdFlying = false;
	if (const ABirdPawn* Bird = PC ? Cast<ABirdPawn>(PC->GetPawn()) : nullptr)
	{
		// the bird's own wind takes over, except while the camera flies in or out (its voice fades there)
		bBirdFlying = Bird->GetMode() == EBirdMode::Air || Bird->GetMode() == EBirdMode::Ground;
	}
}

void ULyudSoundscapeSubsystem::BuildTreeGrid()
{
	bTreeGridBuilt = true;
	TreeGrid.Reset();
	auto Add = [this](FName Species, const FVector& L, float Weight)
	{
		FTreeCell& C = TreeGrid.FindOrAdd(FIntPoint(FMath::FloorToInt(L.X / TreeCell), FMath::FloorToInt(L.Y / TreeCell)));
		if (IsConifer(Species))
		{
			C.Conifer += Weight;
		}
		else
		{
			C.Leafy += Weight * LeafLoudness(Species);
		}
	};
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		It->ForEachTree([&](FName Species, const FTransform& Xf, float Age)
		{
			Add(Species, Xf.GetLocation(), FMath::Clamp(Age / 20.f, 0.2f, 1.5f));
		});
	}
	for (TActorIterator<AProceduralPlant> It(GetWorld()); It; ++It)
	{
		Add(It->Species, It->GetActorLocation(), FMath::Clamp(It->AgeAtStart / 20.f, 0.1f, 1.5f));
	}
}

void ULyudSoundscapeSubsystem::UpdateTrees()
{
	if (!bTreeGridBuilt)
	{
		BuildTreeGrid();
	}
	// crowns within ~35 m, the near ones weigh most
	float L = 0.f, C = 0.f;
	const FIntPoint Me(FMath::FloorToInt(Listener.X / TreeCell), FMath::FloorToInt(Listener.Y / TreeCell));
	for (int32 DY = -2; DY <= 2; DY++)
	{
		for (int32 DX = -2; DX <= 2; DX++)
		{
			if (const FTreeCell* Cell = TreeGrid.Find(Me + FIntPoint(DX, DY)))
			{
				const FVector Centre((Me.X + DX + 0.5f) * TreeCell, (Me.Y + DY + 0.5f) * TreeCell, Listener.Z);
				const float D = float(FVector::Dist2D(Centre, Listener)) / 100.f;
				const float W = 1.f / (1.f + FMath::Square(D / 15.f));
				L += Cell->Leafy * W;
				C += Cell->Conifer * W;
			}
		}
	}
	// a handful of near broadleaf crowns makes the full rustle
	Leafy = FMath::Clamp(L / 5.f, 0.f, 1.f);
	Conifer = FMath::Clamp(C / 6.f, 0.f, 1.f);
	// the season: leaves open in May, dry from September, mostly down by mid-November
	const float Doy = DayOfYear();
	LeafSeason = Smooth(120.f, 145.f, Doy) * (1.f - 0.85f * Smooth(295.f, 320.f, Doy));
	LeafDry = Smooth(240.f, 300.f, Doy);
}

void ULyudSoundscapeSubsystem::UpdateSurf()
{
	ALyudSea* Sea = ALyudSea::Get(GetWorld());
	if (!Sea)
	{
		bHaveShore = false;
		SurfHeight = 0.f;
		return;
	}
	// the nearest water deeper than a hand: rings of points out to 2.5 km
	static const float Rings[] = { 3.f, 6.f, 12.f, 24.f, 48.f, 96.f, 192.f, 384.f, 768.f, 1536.f, 2560.f };
	float Best = 1e9f;
	FVector BestAt = FVector::ZeroVector;
	for (float R : Rings)
	{
		if (R * 100.f > Best)
		{
			break;
		}
		const int32 N = R < 50.f ? 16 : 32;
		for (int32 K = 0; K < N; K++)
		{
			const float A = 2.f * PI * K / N;
			const FVector P = Listener + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * 100.f;
			if (Sea->DepthAt(P) > 20.f)
			{
				const float D = float(FVector::Dist2D(P, Listener));
				if (D < Best)
				{
					Best = D;
					BestAt = P;
				}
			}
		}
	}
	bHaveShore = Best < 1e8f;
	if (bHaveShore)
	{
		// the waves break a little further out than where the water starts
		Shore = FVector(BestAt.X, BestAt.Y, Sea->SeaLevel());
	}
	// breakers on a gently shelving beach are about the open sea's significant height; the period grows with it
	const float Hs = Sea->WaveHeight() / 100.f;
	SurfHeight = Hs * 0.8f;
	SurfPeriod = FMath::Clamp(2.f + 3.6f * FMath::Sqrt(Hs), 2.5f, 10.f);
}

void ULyudSoundscapeSubsystem::UpdateBed()
{
	UWorld* World = GetWorld();
	LyudAudio::FAmbienceParams P;
	const bool bOn = CVarSound.GetValueOnGameThread() > 0 && CVarBed.GetValueOnGameThread() > 0;
	P.Master = bOn ? MasterVolume * AmbienceVolume : 0.f;

	// indoors: the sky's own test (the pawn under open sky), eased; without a sky there is no outside to hear
	bool bOutdoorsNow = false;
	bool bHaveSky = false;
	if (const ALyudSky* Sky = FindSky(World))
	{
		bHaveSky = true;
		bOutdoorsNow = Sky->IsPlayerOutdoors();
	}
	if (bBirdFlying)
	{
		bOutdoorsNow = bHaveSky;
	}
	Outdoors = FMath::FInterpTo(Outdoors, bOutdoorsNow ? 1.f : 0.f, 0.1f, 1.2f);
	P.Enclosure = 1.f - Outdoors;
	ALyudSea* Sea = ALyudSea::Get(World);
	if (Sea && Sea->IsCameraUnderwater())
	{
		P.Enclosure = 0.97f;
	}

	// wind at the ear (2 m over the ground), a little sheltered among trees
	if (const ULyudWindSubsystem* Wind = World->GetSubsystem<ULyudWindSubsystem>())
	{
		const FLyudWindSample S = Wind->GetWindAt(Listener, 2.f, false);
		const float Shelter = 1.f - 0.35f * FMath::Clamp(Leafy * LeafSeason + Conifer, 0.f, 1.f);
		P.WindSpeed = S.Speed * Shelter;
		P.WindGustiness = Wind->GetGustiness();
		// pan toward where it comes from
		P.WindPan = -float(FVector::DotProduct(S.Velocity.GetSafeNormal2D(), ListenerRight.GetSafeNormal2D()));
		LastWind = S.Speed;
		LastGust = S.Gust;
	}
	if (bBirdFlying)
	{
		P.Gain[LyudAudio::Wind] = 0.f;
	}
	// leaves through walls are lost; the wind's rumble still comes
	const float Through = 0.15f + 0.85f * Outdoors;
	P.Leaves = Leafy * LeafSeason * Through;
	P.LeafDry = LeafDry;
	P.Needles = Conifer * Through;

	if (bHaveShore)
	{
		P.SurfHeight = SurfHeight;
		P.SurfPeriod = SurfPeriod;
		P.SurfDistance = float(FVector::Dist(Shore, Listener)) / 100.f;
		P.SurfPan = PanOf(Shore);
	}

	if (const ULyudWeatherSubsystem* Weather = World->GetSubsystem<ULyudWeatherSubsystem>())
	{
		const FLyudSkyWeather W = Weather->GetSky();
		P.Rain = W.Rain;
		P.Snow = W.Snow;
		P.TempC = W.TempC;
		// a roof or canopy overhead drums only out there (indoors the walls already carry it, muffled)
		P.RainRoof = Roof * Outdoors;
	}
	// field crickets: summer nights (and late evenings) in the warm
	const float Doy = DayOfYear();
	const float Sun = SunAltitude();
	P.Crickets = Smooth(155.f, 175.f, Doy) * (1.f - Smooth(245.f, 265.f, Doy)) * Smooth(2.f, -6.f, Sun) * (1.f - FMath::Clamp(P.Rain, 0.f, 1.f));

	if (Bed)
	{
		Bed->SetParams(P);
	}
}

void ULyudSoundscapeSubsystem::OnLightning(FVector Location, float DistanceFromViewM)
{
	if (Bed && CVarSound.GetValueOnGameThread() > 0)
	{
		Bed->Strike(DistanceFromViewM, PanOf(Location));
	}
}

void ULyudSoundscapeSubsystem::OnAnimalEvent(int32 AnimalId, FName Event)
{
	if (CVarSound.GetValueOnGameThread() <= 0)
	{
		return;
	}
	const UAnimalLifeSubsystem* Animals = UAnimalLifeSubsystem::Get(this);
	FAnimalInfo A;
	if (!Animals || !Animals->GetAnimal(AnimalId, A))
	{
		return;
	}
	// a far animal isn't heard at all; and never a din
	const float Dist = float(FVector::Dist(A.Location, Listener)) / 100.f;
	if (Dist > 120.f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - AnimalSecond > 1.0)
	{
		AnimalSecond = Now;
		AnimalSoundsThisSecond = 0;
	}
	if (++AnimalSoundsThisSecond > 4)
	{
		return;
	}
	const FVector Mouth = A.Location + FVector(0, 0, A.Kind == EAnimalKind::Cat ? 20.f : 45.f);
	const float Muffle = 0.35f + 0.65f * Outdoors;
	FName Pool;
	float Volume = 1.f;
	if (A.Kind == EAnimalKind::Dog)
	{
		if (Event == TEXT("Bark"))
		{
			Pool = A.Breed == TEXT("Shepherd") ? TEXT("dog_bark_big") : (A.Breed == TEXT("Laika") ? TEXT("dog_bark_laika") : TEXT("dog_bark_medium"));
		}
		else if (Event == TEXT("Growl"))
		{
			Pool = TEXT("dog_growl");
			Volume = 0.6f;
		}
		else if (Event == TEXT("Whine"))
		{
			Pool = TEXT("dog_whine");
			Volume = 0.5f;
		}
	}
	else
	{
		if (Event == TEXT("Meow"))
		{
			Pool = TEXT("cat_meow");
			Volume = 0.55f;
		}
		else if (Event == TEXT("Hiss"))
		{
			Pool = TEXT("cat_hiss");
			Volume = 0.6f;
		}
		else if (Event == TEXT("Purr"))
		{
			Pool = TEXT("cat_purr");
			Volume = 0.3f;
		}
	}
	if (!Pool.IsNone())
	{
		// a puppy or a kitten is higher
		const float Pitch = A.bYoung ? 0.18f : 0.f;
		if (USoundBase* S = Pick(Pool))
		{
			UGameplayStatics::PlaySoundAtLocation(this, S, Mouth, FRotator::ZeroRotator, Volume * Muffle * MasterVolume * EffectsVolume,
				FMath::FRandRange(0.95f, 1.05f) + Pitch, 0.f, NearAttenuation);
		}
	}
}

void ULyudSoundscapeSubsystem::UpdateFar(float DeltaTime)
{
	FarTimer -= DeltaTime;
	if (FarTimer > 0.f || CVarSound.GetValueOnGameThread() <= 0)
	{
		return;
	}
	FarTimer = 1.f;
	const UWorld* World = GetWorld();
	const ULyudWeatherSubsystem* Weather = World->GetSubsystem<ULyudWeatherSubsystem>();
	const FLyudSkyWeather W = Weather ? Weather->GetSky() : FLyudSkyWeather();
	const float Sun = SunAltitude();
	const float SeaDist = bHaveShore ? float(FVector::Dist2D(Shore, Listener)) / 100.f : 1e9f;
	const FVector SeaDir = bHaveShore ? (Shore - Listener).GetSafeNormal2D() : FVector::ForwardVector;

	// a Poisson clock per kind: Rate = expected per minute now
	auto Due = [this](FName Kind, float PerMinute)
	{
		if (PerMinute <= 0.f)
		{
			NextFar.Remove(Kind);
			return false;
		}
		double* Next = NextFar.Find(Kind);
		if (!Next)
		{
			NextFar.Add(Kind, PlayTime + 60.0 / PerMinute * -FMath::Loge(FMath::Max(FMath::FRand(), 0.05f)));
			return false;
		}
		if (PlayTime < *Next)
		{
			return false;
		}
		*Next = PlayTime + 60.0 / PerMinute * -FMath::Loge(FMath::Max(FMath::FRand(), 0.05f));
		return true;
	};
	const float Out = Outdoors;
	const float Vol = 0.25f + 0.75f * Out;
	// the far ones are placed 300 m off in their direction, swung a little, so they come from somewhere
	auto FarPoint = [this](const FVector& Dir, float SwingDeg)
	{
		return Listener + Dir.RotateAngleAxis(FMath::FRandRange(-SwingDeg, SwingDeg), FVector::UpVector) * 30000.f + FVector(0, 0, 500.f);
	};
	if (bHaveShore && SeaDist < 2500.f && Due(TEXT("ship_horn_far"), 0.12f * Smooth(2500.f, 600.f, SeaDist)))
	{
		PlayAt(TEXT("ship_horn_far"), FarPoint(SeaDir, 40.f), 0.55f * Vol, FarAttenuation, 0.03f);
	}
	if (bHaveShore && SeaDist < 3000.f && Due(TEXT("foghorn_far"), 0.5f * Smooth(0.4f, 0.8f, W.Fog)))
	{
		PlayAt(TEXT("foghorn_far"), FarPoint(SeaDir, 20.f), 0.6f * Vol, FarAttenuation, 0.f);
	}
	if (bHaveShore && SeaDist < 400.f && Due(TEXT("buoy_bell"), 0.8f * Smooth(0.15f, 0.6f, SurfHeight) * Smooth(400.f, 80.f, SeaDist)))
	{
		PlayAt(TEXT("buoy_bell"), FarPoint(SeaDir, 30.f), 0.35f * Vol, FarAttenuation, 0.02f);
	}
	// dogs answering each other across the valley at night, away from the sea
	const FVector Inland = bHaveShore ? -SeaDir : FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
	if (Due(TEXT("dog_bark_far"), 0.7f * Smooth(0.f, -8.f, Sun) * Out * (1.f - Smooth(1.f, 5.f, W.Rain))))
	{
		PlayAt(TEXT("dog_bark_far"), FarPoint(Inland, 70.f), 0.4f, FarAttenuation, 0.06f);
	}
	// the town's murmur by day, outdoors, away from the surf
	if (TownLoop && TownLoop->GetSound())
	{
		const float Day = Smooth(-4.f, 10.f, Sun);
		const float Target = MasterVolume * EffectsVolume * 0.18f * Day * Out * Smooth(80.f, 400.f, SeaDist) * (CVarSound.GetValueOnGameThread() > 0 ? 1.f : 0.f);
		if (Target > 0.01f && !TownLoop->IsPlaying())
		{
			TownLoop->FadeIn(4.f, Target);
		}
		else if (Target <= 0.01f && TownLoop->IsPlaying())
		{
			TownLoop->FadeOut(4.f, 0.f);
		}
		else if (TownLoop->IsPlaying())
		{
			TownLoop->AdjustVolume(1.f, Target);
		}
	}
	// a roof or a canopy right overhead (rain drums on it)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(LyudSoundRoof), false);
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		Q.AddIgnoredActor(PC->GetPawn());
	}
	Roof = World->LineTraceSingleByChannel(Hit, Listener, Listener + FVector(0, 0, 1000.f), ECC_Visibility, Q) ? 1.f : 0.f;
}

void ULyudSoundscapeSubsystem::UpdateClub()
{
	// the computer club: its people typing, clicking and playing, found once (people's meshes are SK_<Id> humans)
	UWorld* World = GetWorld();
	if (ClubLoops.Num() == 0)
	{
		int32 K = 0;
		for (TActorIterator<ASkeletalMeshActor> It(World); It; ++It)
		{
			const USkeletalMeshComponent* Mesh = It->GetSkeletalMeshComponent();
			const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			if (!Asset || !Asset->GetPathName().Contains(TEXT("/Characters/Humans/")))
			{
				continue;
			}
			// people sit at their computers: the keyboard is in front of the chest
			const FName Pool = (K % 4 == 3) ? FName(TEXT("console_game_far")) : (K % 4 == 1 ? FName(TEXT("mouse_clicks")) : FName(TEXT("keyboard_typing")));
			K++;
			USoundBase* S = Pick(Pool);
			if (!S)
			{
				continue;
			}
			UAudioComponent* A = NewObject<UAudioComponent>(Holder);
			A->SetSound(S);
			A->bAutoActivate = false;
			A->bAllowSpatialization = true;
			A->AttenuationSettings = RoomAttenuation;
			A->SetWorldLocation(It->GetActorLocation() + It->GetActorForwardVector() * 40.f + FVector(0, 0, 80.f));
			A->RegisterComponent();
			ClubLoops.Add(A);
		}
		if (ClubLoops.Num() == 0)
		{
			// nobody in this level: don't look again
			ClubLoops.Add(nullptr);
		}
	}
	const float Inside = 1.f - Outdoors;
	const bool bOn = CVarSound.GetValueOnGameThread() > 0;
	// people's sounds are short takes played now and then, not loops: one starts when its last one ended
	float Nearest = 1e9f;
	for (UAudioComponent* A : ClubLoops)
	{
		if (!A)
		{
			continue;
		}
		const float D = float(FVector::Dist(A->GetComponentLocation(), Listener)) / 100.f;
		Nearest = FMath::Min(Nearest, D);
		if (bOn && D < 25.f && !A->IsPlaying() && FMath::FRand() < 0.15f)
		{
			A->SetVolumeMultiplier(0.5f * MasterVolume * EffectsVolume);
			A->SetPitchMultiplier(FMath::FRandRange(0.96f, 1.04f));
			A->Play(FMath::FRandRange(0.f, 2.f));
		}
	}
	// the club's room tone: fans and monitors, where its people are
	if (RoomTone && RoomTone->GetSound())
	{
		const float Target = bOn ? MasterVolume * AmbienceVolume * 0.5f * Inside * Smooth(60.f, 15.f, Nearest) : 0.f;
		if (Target > 0.01f && !RoomTone->IsPlaying())
		{
			RoomTone->FadeIn(2.f, Target);
		}
		else if (Target <= 0.01f && RoomTone->IsPlaying())
		{
			RoomTone->FadeOut(2.f, 0.f);
		}
		else if (RoomTone->IsPlaying())
		{
			RoomTone->AdjustVolume(0.5f, Target);
		}
	}
}

FName ULyudSoundscapeSubsystem::ComputeMood() const
{
	const FString Forced = CVarMusicMood.GetValueOnGameThread();
	if (!Forced.IsEmpty())
	{
		return FName(*Forced);
	}
	if (bBirdFlying)
	{
		return TEXT("flight");
	}
	if (Outdoors < 0.4f)
	{
		return TEXT("club");
	}
	const UWorld* World = GetWorld();
	const ULyudWeatherSubsystem* Weather = World->GetSubsystem<ULyudWeatherSubsystem>();
	const FLyudSkyWeather W = Weather ? Weather->GetSky() : FLyudSkyWeather();
	if (W.Thunder > 0.15f || LastWind > 14.f)
	{
		return TEXT("storm");
	}
	if (W.Rain > 0.3f)
	{
		return W.Snow > 0.5f ? TEXT("snow") : TEXT("rain");
	}
	const float Sun = SunAltitude();
	const float Hour = float(UGameCalendar::NowIn(World).GetTimeOfDay().GetTotalHours());
	if (Sun < -6.f)
	{
		return DayOfYear() < 75.f || DayOfYear() > 335.f ? TEXT("snow") : TEXT("night");
	}
	if (Sun < 10.f)
	{
		return Hour < 12.f ? TEXT("dawn") : TEXT("dusk");
	}
	if (bHaveShore && FVector::Dist2D(Shore, Listener) < 6000.f)
	{
		return TEXT("sea");
	}
	return TEXT("day");
}

void ULyudSoundscapeSubsystem::PlayNextCue()
{
	SilenceUntil = 0.0;
	CueEnds = 0.0;
	// start a fresh cue now: the old one fades on the other component
	UAudioComponent* Old = bMusicOnA ? MusicA : MusicB;
	if (Old && Old->IsPlaying())
	{
		Old->FadeOut(4.f, 0.f);
	}
	CurrentCue = NAME_None;
	FadingOutUntil = 0.0;
	UpdateMusic();
}

void ULyudSoundscapeSubsystem::UpdateMusic()
{
	if (!MusicA || !MusicB)
	{
		return;
	}
	const bool bOn = CVarSound.GetValueOnGameThread() > 0 && CVarMusic.GetValueOnGameThread() > 0 && Cues.Num() > 0;
	UAudioComponent* Cur = bMusicOnA ? MusicA : MusicB;
	const FName NewMood = ComputeMood();
	const bool bMoodChanged = NewMood != Mood;
	Mood = NewMood;
	if (!bOn)
	{
		if (Cur->IsPlaying())
		{
			Cur->FadeOut(2.f, 0.f);
		}
		CurrentCue = NAME_None;
		return;
	}
	const float Volume = MasterVolume * MusicVolume;
	if (!CurrentCue.IsNone())
	{
		Cur->AdjustVolume(1.f, Volume);
		const FCue* Playing = Cues.FindByPredicate([this](const FCue& C) { return C.Name == CurrentCue; });
		// a big change of place or state (into the club, into the air, a storm) ends the cue: the next one fits
		const bool bClash = bMoodChanged && Playing && !Playing->Moods.Contains(Mood)
			&& (Mood == TEXT("club") || Mood == TEXT("flight") || Mood == TEXT("storm") || (Playing->Moods.Contains(TEXT("club")) || Playing->Moods.Contains(TEXT("flight"))));
		if (bClash)
		{
			Cur->FadeOut(6.f, 0.f);
			CurrentCue = NAME_None;
			// flight and the club get their music at once; the rest after a breath
			SilenceUntil = PlayTime + ((Mood == TEXT("flight") || Mood == TEXT("club")) ? 5.0 : 20.0);
			return;
		}
		if (PlayTime >= CueEnds)
		{
			CurrentCue = NAME_None;
			// a silence: a minute or three, shorter in the air
			const double Gap = (Mood == TEXT("flight") ? FMath::FRandRange(15.f, 40.f) : FMath::FRandRange(50.f, 160.f)) * CVarMusicGap.GetValueOnGameThread();
			SilenceUntil = PlayTime + Gap;
		}
		return;
	}
	if (PlayTime < SilenceUntil)
	{
		return;
	}
	// the cue: of this mood, not one of the last few; else of a neighbouring mood
	auto Candidates = [this](FName M)
	{
		TArray<const FCue*> Out;
		for (const FCue& C : Cues)
		{
			if (C.Moods.Contains(M) && !RecentCues.Contains(C.Name))
			{
				Out.Add(&C);
			}
		}
		return Out;
	};
	TArray<const FCue*> Pick = Candidates(Mood);
	for (FName F : MoodFallbacks(Mood))
	{
		if (Pick.Num() > 0)
		{
			break;
		}
		Pick = Candidates(F);
	}
	if (Pick.Num() == 0)
	{
		RecentCues.Reset();
		Pick = Candidates(Mood);
	}
	if (Pick.Num() == 0)
	{
		SilenceUntil = PlayTime + 30.0;
		return;
	}
	const FCue* C = Pick[FMath::RandRange(0, Pick.Num() - 1)];
	bMusicOnA = !bMusicOnA;
	UAudioComponent* Next = bMusicOnA ? MusicA : MusicB;
	Next->SetSound(C->Sound);
	Next->FadeIn(4.f, Volume);
	CurrentCue = C->Name;
	CueEnds = PlayTime + FMath::Max(C->Seconds, 10.f);
	RecentCues.Add(C->Name);
	if (RecentCues.Num() > 4)
	{
		RecentCues.RemoveAt(0);
	}
}

void ULyudSoundscapeSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !Holder)
	{
		return;
	}
	PlayTime += DeltaTime;
	UpdateListener();
	TreeTimer -= DeltaTime;
	if (TreeTimer <= 0.f)
	{
		TreeTimer = 2.f;
		UpdateTrees();
	}
	SurfTimer -= DeltaTime;
	if (SurfTimer <= 0.f)
	{
		SurfTimer = 1.f;
		UpdateSurf();
	}
	ParamTimer -= DeltaTime;
	if (ParamTimer <= 0.f)
	{
		ParamTimer = 0.1f;
		UpdateBed();
	}
	UpdateFar(DeltaTime);
	ClubTimer -= DeltaTime;
	if (ClubTimer <= 0.f)
	{
		ClubTimer = 0.5f;
		UpdateClub();
		UpdateMusic();
	}
	if (CVarDebug.GetValueOnGameThread() > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0x50D5C), 0.f, FColor::Emerald, DebugReport());
	}
}

FString ULyudSoundscapeSubsystem::DebugReport() const
{
	float Rms[LyudAudio::NumVoices] = {};
	if (Bed)
	{
		Bed->GetVoiceRms(Rms);
	}
	return FString::Printf(TEXT("SOUND outdoors %.2f wind %.1f m/s gust %.2f leaves %.2f (season %.2f dry %.2f) needles %.2f surf %s %.0f m h %.2f T %.1f roof %.0f bird %d | ")
		TEXT("rms wind %.3f leaves %.3f surf %.3f rain %.3f crickets %.3f thunder %.3f | music mood %s cue %s silence %.0f s | sounds %d cues %d"),
		Outdoors, LastWind, LastGust, Leafy, LeafSeason, LeafDry, Conifer, bHaveShore ? TEXT("yes") : TEXT("no"),
		bHaveShore ? FVector::Dist(Shore, Listener) / 100.f : -1.f, SurfHeight, SurfPeriod, Roof, bBirdFlying ? 1 : 0,
		Rms[0], Rms[1], Rms[2], Rms[3], Rms[4], Rms[5], *Mood.ToString(), *CurrentCue.ToString(),
		FMath::Max(SilenceUntil - PlayTime, 0.0), CueRefs.Num(), Cues.Num());
}
