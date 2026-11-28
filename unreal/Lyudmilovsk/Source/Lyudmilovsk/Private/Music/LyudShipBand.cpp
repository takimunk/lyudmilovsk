// The motor ship's quartet: the sets of the cruise's programme played live by the synthesiser of ShipBandModel.h, as a sound
// placed at the bandstand (docs/ship.md "The band"). The audio is fed as ULyudInstruments feeds its piano: a
// USoundWaveProcedural filled a little ahead of the device from the game thread, nothing allocated while it plays.

#include "Music/LyudShipBand.h"

#include "Audio/LyudSoundscape.h"
#include "Components/AudioComponent.h"
#include "Cruise/LyudCruise.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Music/ShipBandModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudBand, Log, All);

static TAutoConsoleVariable<int32> CVarLyudBand(TEXT("lyud.Band"), 1,
	TEXT("The motor ship's quartet plays at the concerts and the captain's dinner (0: silent). docs/ship.md"));

namespace LyudBandImpl
{
	constexpr int32 Rate = 32000;
	/** How far ahead of the device the band is rendered. Nobody presses a key here, so a fifth of a second: a hitch of the game
	 *  thread that long is not heard. */
	constexpr float AheadSeconds = 0.2f;
	/** The most frames rendered in one tick (half a second: after a long hitch the buffer is refilled over two ticks). */
	constexpr int32 MaxBlock = Rate / 2;
	/** The saloon: how near the bandstand (cm, in the ship's plan) the hero is "in the saloon" for a title to be shown. */
	constexpr float SaloonCm = 1300.f;

	const TCHAR* const SetIds[3] = { TEXT("concert1"), TEXT("captains_dinner"), TEXT("concert2") };

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

ULyudShipBand::ULyudShipBand() = default;
ULyudShipBand::~ULyudShipBand() = default;

ULyudShipBand* ULyudShipBand::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudShipBand>() : nullptr;
}

// ------------------------------------------------------------------------------------------------------------- the life cycle

void ULyudShipBand::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Player = MakePimpl<LyudBand::FPlayer>(LyudBandImpl::Rate);
	Book = MakePimpl<LyudBand::FBook>();
	Pcm.Reserve(LyudBandImpl::MaxBlock * 2);
	Mix.Reserve(LyudBandImpl::MaxBlock * 2);
	LoadBook();
	LoadEventTimes();
}

void ULyudShipBand::Deinitialize()
{
	if (bHeld)
	{
		if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
		{
			Scape->SetMusicHold(this, false);
		}
		bHeld = false;
	}
	if (Voice)
	{
		Voice->Stop();
		Voice = nullptr;
	}
	Wave = nullptr;
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	Super::Deinitialize();
}

void ULyudShipBand::LoadBook()
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Music/band_tunes.txt");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudBand, Warning, TEXT("Band: no %s"), *Path);
		return;
	}
	std::vector<std::string> Errors;
	*Book = LyudBand::Parse(std::string(TCHAR_TO_UTF8(*Text)), Errors);
	for (const std::string& E : Errors)
	{
		UE_LOG(LogLyudBand, Warning, TEXT("Band: band_tunes.txt: %s"), *LyudBandImpl::Str(E));
	}
	UE_LOG(LogLyudBand, Display, TEXT("Band: %d tunes, %d sets"), (int32)Book->Tunes.size(), (int32)Book->Sets.size());
}

void ULyudShipBand::LoadEventTimes()
{
	// when each set begins, in seconds of the voyage: the programme's own numbers (the defaults are the design's)
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Cruise/cruise.json");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), Events) || !Events)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : *Events)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			continue;
		}
		FString Id;
		double T0 = 0.0;
		if ((*Obj)->TryGetStringField(TEXT("id"), Id) && (*Obj)->TryGetNumberField(TEXT("t0"), T0))
		{
			for (int32 I = 0; I < 3; ++I)
			{
				if (Id == LyudBandImpl::SetIds[I])
				{
					SetStart[I] = T0;
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------- what plays

bool ULyudShipBand::IsPlaying() const
{
	return Player.IsValid() && Player->IsPlaying();
}

FString ULyudShipBand::CurrentTuneId() const
{
	const LyudBand::FTune* T = Player.IsValid() ? Player->Playing() : nullptr;
	return T ? LyudBandImpl::Str(T->Id) : FString();
}

FString ULyudShipBand::CurrentTuneKey() const
{
	const FString Id = CurrentTuneId();
	return Id.IsEmpty() ? FString() : FString::Printf(TEXT("band.tune.%s.title"), *Id);
}

int32 ULyudShipBand::NumTunes() const
{
	return Book.IsValid() ? (int32)Book->Tunes.size() : 0;
}

FString ULyudShipBand::TuneId(int32 Index) const
{
	return Book.IsValid() && Index >= 0 && Index < (int32)Book->Tunes.size() ? LyudBandImpl::Str(Book->Tunes[(size_t)Index].Id) : FString();
}

bool ULyudShipBand::PlayTest(const FString& Id, bool bSoft)
{
	if (!Book.IsValid() || !Player.IsValid())
	{
		return false;
	}
	const int32 Ix = Book->FindTune(std::string(TCHAR_TO_UTF8(*Id)));
	if (Ix < 0 || !EnsureVoice())
	{
		return false;
	}
	bTest = true;
	TestTune = Ix;
	TuneNow = Ix;
	SlotNow = -1;
	SeedNow = LyudBand::Hash(LyudBand::HashString(Book->Tunes[(size_t)Ix].Id), (uint32)FMath::Rand()) | 1u;
	Player->Start(Book->Tunes[(size_t)Ix], SeedNow, bSoft, 0.0);
	StartVoice(false, FVector::ZeroVector);
	WantGain = Gain = 1.f;
	WantLowPass = LowPass = 20000.f;
	UE_LOG(LogLyudBand, Display, TEXT("Band: test %s%s, %.0f s"), *Id, bSoft ? TEXT(" (soft)") : TEXT(""), Player->Duration());
	return true;
}

void ULyudShipBand::PlayNext()
{
	const int32 N = NumTunes();
	if (N > 0)
	{
		PlayTest(TuneId((TestTune + 1) % N));
	}
}

void ULyudShipBand::StopTest()
{
	if (bTest && Player.IsValid())
	{
		Player->Stop(false);
	}
	bTest = false;
}

FString ULyudShipBand::Report() const
{
	return FString::Printf(TEXT("Band: set=%s number=%d tune=%s at %.0f/%.0f s test=%d voice=%s gain=%.2f lowpass=%.0f saloon=%d"),
		*SetNow.ToString(), SlotNow, *CurrentTuneId(), Player.IsValid() ? Player->Position() : 0.0, Player.IsValid() ? Player->Duration() : 0.0,
		bTest ? 1 : 0, Voice && Voice->IsPlaying() ? (bVoiceInWorld ? TEXT("bandstand") : TEXT("2D")) : TEXT("off"), Gain, LowPass, bInSaloon ? 1 : 0);
}

// ------------------------------------------------------------------------------------------------------------------ the sound

bool ULyudShipBand::EnsureVoice()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (!Wave)
	{
		Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(LyudBandImpl::Rate);
		// mono, as every spatialised procedural voice of this game (BirdSound, the kite's hum, the fountain): the player renders
		// L/R and Pump sums them. A 2-channel source would be spread by the engine's stereo spatialisation (or, with a
		// spatialisation plugin set, not spatialised at all); one channel is the form known to work, in the world and in 2D.
		Wave->NumChannels = 1;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->SoundGroup = SOUNDGROUP_Music;
		Wave->bLooping = false;
		Wave->bCanProcessAsync = true;
	}
	if (!Holder)
	{
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
		if (!Holder)
		{
			return false;
		}
	}
	if (!Voice)
	{
		Voice = NewObject<UAudioComponent>(Holder, TEXT("ShipBand"));
		Voice->bAutoActivate = false;
		Voice->bAutoDestroy = false;
		Voice->bIsUISound = false;
		Voice->bOverrideAttenuation = true;
		Voice->AttenuationOverrides.bAttenuate = true;
		Voice->AttenuationOverrides.bSpatialize = true;
		// full within 6 m of the bandstand (the saloon), falling away over the next 50 m: the listener's place does the rest
		Voice->AttenuationOverrides.AttenuationShapeExtents = FVector(600.f, 0.f, 0.f);
		Voice->AttenuationOverrides.FalloffDistance = 5000.f;
		Voice->SetSound(Wave);
		Voice->RegisterComponent();
	}
	return Voice != nullptr;
}

void ULyudShipBand::StartVoice(bool bInWorld, const FVector& At)
{
	if (!EnsureVoice())
	{
		return;
	}
	if (Voice->IsPlaying() && bVoiceInWorld != bInWorld)
	{
		Voice->Stop();
	}
	bVoiceInWorld = bInWorld;
	Voice->bAllowSpatialization = bInWorld;
	Voice->AttenuationOverrides.bAttenuate = bInWorld;
	Voice->AttenuationOverrides.bSpatialize = bInWorld;
	if (bInWorld)
	{
		Voice->SetWorldLocation(At);
	}
	if (!Voice->IsPlaying())
	{
		Wave->ResetAudio();
		Voice->Play();
	}
	SilentFor = 0.f;
}

void ULyudShipBand::StopVoice()
{
	if (Voice && Voice->IsPlaying())
	{
		Voice->Stop();
	}
	if (bHeld)
	{
		if (ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
		{
			Scape->SetMusicHold(this, false);
		}
		bHeld = false;
	}
}

void ULyudShipBand::Pump(float DeltaTime)
{
	if (!Wave || !Voice || !Voice->IsPlaying() || !Player.IsValid())
	{
		return;
	}
	// nothing for a while (the set is over, the hero went ashore): the voice is given back until the next tune
	SilentFor = Player->IsPlaying() ? 0.f : SilentFor + DeltaTime;
	if (SilentFor > 1.5f)
	{
		StopVoice();
		return;
	}
	// the wave is mono (EnsureVoice): one int16 a frame; the player's L/R are summed (half each: the limiter's peak stays < 1)
	const int32 BytesPerFrame = (int32)sizeof(int16);
	const int32 Have = Wave->GetAvailableAudioByteCount() / BytesPerFrame;
	const int32 Want = (int32)(LyudBandImpl::Rate * LyudBandImpl::AheadSeconds) - Have;
	if (Want <= 0)
	{
		return;
	}
	const int32 N = FMath::Min(Want + (int32)(FMath::Min(DeltaTime, 0.25f) * LyudBandImpl::Rate), LyudBandImpl::MaxBlock);
	Mix.SetNumUninitialized(N * 2, EAllowShrinking::No);
	Pcm.SetNumUninitialized(N, EAllowShrinking::No);
	Player->Render(Mix.GetData(), N);
	for (int32 I = 0; I < N; ++I)
	{
		const float M = 0.5f * (Mix[2 * I] + Mix[2 * I + 1]);
		Pcm[I] = (int16)FMath::Clamp(M * 32000.f, -32767.f, 32767.f);
	}
	Wave->QueueAudio((const uint8*)Pcm.GetData(), N * BytesPerFrame);
}

// ---------------------------------------------------------------------------------------------------------------- the tick

void ULyudShipBand::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !Player.IsValid())
	{
		return;
	}
	PollClock -= DeltaTime;
	if (PollClock <= 0.f)
	{
		PollClock = 0.25f;
		Decide();
	}
	if (Voice && Voice->IsPlaying())
	{
		// the listener's place: the level and the muffling move to it over about half a second
		const float K = 1.f - FMath::Exp(-DeltaTime / 0.5f);
		Gain += (WantGain - Gain) * K;
		LowPass += (WantLowPass - LowPass) * K;
		const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this);
		// as loud as a record (ULyudMusic::MusicLevel): above the ambient cues, which sit at 0.45 of the music volume
		const float Level = Scape ? Scape->MasterVolume * FMath::Clamp(Scape->MusicVolume * 1.8f, 0.2f, 1.f) : 0.8f;
		// each setter sends a command to the audio thread: only when it has moved enough to be heard
		const float Volume = FMath::Max(0.001f, Level * Gain);
		if (FMath::Abs(Volume - SentVolume) > 0.004f)
		{
			SentVolume = Volume;
			Voice->SetVolumeMultiplier(Volume);
		}
		if (FMath::Abs(LowPass - SentLowPass) > FMath::Max(20.f, 0.02f * LowPass))
		{
			SentLowPass = LowPass;
			Voice->SetLowPassFilterEnabled(LowPass < 15000.f);
			Voice->SetLowPassFilterFrequency(LowPass);
		}
	}
	Pump(DeltaTime);
}

void ULyudShipBand::Decide()
{
	if (CVarLyudBand.GetValueOnGameThread() == 0)
	{
		if (Player->IsPlaying())
		{
			Player->Stop(false);
		}
		bTest = false;
		SetNow = NAME_None;
		return;
	}
	if (bTest)
	{
		// a test plays to its end wherever the hero is
		if (Player->IsPlaying())
		{
			return;
		}
		bTest = false;
	}
	UWorld* World = GetWorld();
	const ULyudCruise* Cruise = ULyudCruise::Get(World);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	int32 SetIx = -1;
	if (Cruise && Cruise->IsRunning() && Cruise->IsHeroAboard() && Pawn && Book.IsValid())
	{
		for (int32 I = 0; I < 3; ++I)
		{
			if (Cruise->IsEventOn(FName(LyudBandImpl::SetIds[I])))
			{
				SetIx = I;
			}
		}
	}
	const int SetBookIx = SetIx >= 0 ? Book->FindSet(std::string(TCHAR_TO_UTF8(LyudBandImpl::SetIds[SetIx]))) : -1;
	if (SetBookIx < 0)
	{
		// no concert, or he went ashore: the band stops (a quick fade, not a cut)
		if (SetNow != NAME_None && Player->IsPlaying())
		{
			Player->Stop(false);
		}
		SetNow = NAME_None;
		SlotNow = -1;
		bInSaloon = false;
		return;
	}
	SetNow = FName(LyudBandImpl::SetIds[SetIx]);
	const LyudBand::FSet& Set = Book->Sets[(size_t)SetBookIx];
	const double Into = FMath::Max(0.0, Cruise->View().VoyageSeconds - SetStart[SetIx]);
	const LyudBand::FSetPos Pos = LyudBand::Locate(*Book, Set, Into);
	Place(*Cruise, Pawn->GetActorLocation(), SetIx == 1);
	if (Pos.Tune < 0)
	{
		// between numbers: the last one has ended by itself (if the clock jumped into a pause, it is faded out)
		if (Player->IsPlaying() && Player->Duration() - Player->Position() > 4.0)
		{
			Player->Stop(false);
		}
		SlotNow = -1;
		return;
	}
	// the tune the clock says, where the clock says: started at its top, or joined (after a sleep, a load, coming aboard)
	const bool bOther = !Player->IsPlaying() || TuneNow != Pos.Tune || SeedNow != Pos.Seed;
	const bool bAdrift = !bOther && FMath::Abs(Player->Position() - Pos.Offset) > 3.0;
	if (bOther || bAdrift)
	{
		const LyudBand::FTune& T = Book->Tunes[(size_t)Pos.Tune];
		if (Pos.Offset > (Set.bSoft ? T.SecondsSoft : T.Seconds) - 6.0 && bOther)
		{
			return;  // only its last chord is left: wait for the next number
		}
		FVector At = FVector::ZeroVector;
		Cruise->GetShipPoint(TEXT("bandstand"), At);
		Player->Start(T, Pos.Seed, Set.bSoft, Pos.Offset < 0.6 ? 0.0 : Pos.Offset);
		TuneNow = Pos.Tune;
		SeedNow = Pos.Seed;
		StartVoice(true, At + FVector(0.f, 0.f, 120.f));
		Place(*Cruise, Pawn->GetActorLocation(), SetIx == 1);
		if (!bAdrift)
		{
			Gain = WantGain;
			LowPass = WantLowPass;
		}
		UE_LOG(LogLyudBand, Display, TEXT("Band: %s no. %d %s at %.0f s"), *SetNow.ToString(), Pos.Slot + 1, *LyudBandImpl::Str(T.Id), Pos.Offset);
	}
	SlotNow = Pos.Slot;
	// the title, once, to whoever sits in the saloon when the number begins
	if (Pos.Offset < 8.0 && Announced != Pos.Seed && bInSaloon)
	{
		Announced = Pos.Seed;
		if (ULyudCruise* Mutable = ULyudCruise::Get(World))
		{
			Mutable->Say(LyudText::Get(TEXT("band.name")), LyudText::Get(CurrentTuneKey()), 6.f);
		}
	}
}

void ULyudShipBand::Place(const ULyudCruise& Cruise, const FVector& Hero, bool bDinner)
{
	FVector Band = FVector::ZeroVector;
	if (!Cruise.GetShipPoint(TEXT("bandstand"), Band))
	{
		WantGain = 1.f;
		WantLowPass = 20000.f;
		return;
	}
	// in the ship's own frame (cm; z up from the waterline): which deck he is on against the saloon's
	const FVector H = Cruise.ToShipFrame(Hero);
	const FVector B = Cruise.ToShipFrame(Band);
	const float Up = (float)(H.Z - B.Z);  // his middle is about 0.9 m over the deck he stands on
	const float Plan = (float)FVector2D(H.X - B.X, H.Y - B.Y).Size();
	const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this);
	const float Outdoors = Scape ? FMath::Clamp(Scape->GetOutdoors(), 0.f, 1.f) : 0.f;
	FVector Source = Band + FVector(0.f, 0.f, 120.f);
	bInSaloon = false;
	if (Up > 230.f)
	{
		// the boat deck and the bridge, over the saloon's roof: faint
		WantGain = 0.2f;
		WantLowPass = 1500.f;
	}
	else if (Up > -60.f)
	{
		// the saloon's own deck: the room, the lobby abaft it (the distance does it), the promenade outside the windows
		bInSaloon = Plan < LyudBandImpl::SaloonCm && Outdoors < 0.5f;
		WantGain = FMath::Lerp(1.f, 0.5f, Outdoors);
		WantLowPass = FMath::Lerp(Plan > 1400.f ? 7000.f : 20000.f, 4500.f, Outdoors);
	}
	else if (Up > -330.f)
	{
		// the main deck, under the saloon's floor
		WantGain = 0.28f;
		WantLowPass = 900.f;
		FVector Dining = FVector::ZeroVector;
		if (bDinner && Cruise.GetShipPoint(TEXT("dining"), Dining))
		{
			// the captain's dinner: the trio is relayed to the dining saloon's loudspeaker
			Source = Dining + FVector(0.f, 0.f, 200.f);
			WantGain = FMath::Lerp(0.6f, 0.3f, Outdoors);
			WantLowPass = 3800.f;
		}
	}
	else
	{
		// the lower deck
		WantGain = 0.1f;
		WantLowPass = 600.f;
	}
	if (Voice && bVoiceInWorld)
	{
		Voice->SetWorldLocation(Source);
	}
	// the town's own music waits while the band is heard
	const bool bHold = Voice && Voice->IsPlaying() && bVoiceInWorld && Player.IsValid() && Player->IsPlaying();
	if (bHold != bHeld)
	{
		if (ULyudSoundscapeSubsystem* MutableScape = ULyudSoundscapeSubsystem::Get(this))
		{
			MutableScape->SetMusicHold(this, bHold);
		}
		bHeld = bHold;
	}
}

// ---------------------------------------------------------------------------------------------- the console and developer mode

static FAutoConsoleCommandWithWorldAndArgs GBandPlayCmd(TEXT("lyud.Band.Play"),
	TEXT("lyud.Band.Play <tune id> [soft] | next | stop: the ship's band plays a tune here and now, not placed in the world; no argument lists the tunes (docs/ship.md)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudShipBand* Band = ULyudShipBand::Get(World);
		if (!Band)
		{
			return;
		}
		const FString Arg = Args.Num() > 0 ? Args[0].ToLower() : FString();
		if (Arg == TEXT("stop"))
		{
			Band->StopTest();
		}
		else if (Arg == TEXT("next"))
		{
			Band->PlayNext();
		}
		else if (!Arg.IsEmpty())
		{
			const bool bSoft = Args.Num() > 1 && Args[1].ToLower() == TEXT("soft");
			UE_LOG(LogLyudBand, Display, TEXT("Band: play %s: %s"), *Arg, Band->PlayTest(Arg, bSoft) ? TEXT("yes") : TEXT("no such tune"));
		}
		else
		{
			for (int32 I = 0; I < Band->NumTunes(); ++I)
			{
				UE_LOG(LogLyudBand, Display, TEXT("  %s"), *Band->TuneId(I));
			}
		}
	}));

static FAutoConsoleCommandWithWorld GBandReportCmd(TEXT("lyud.Band.Report"), TEXT("What the ship's band is doing (docs/ship.md)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudShipBand* Band = ULyudShipBand::Get(World))
		{
			UE_LOG(LogLyudBand, Display, TEXT("%s"), *Band->Report());
		}
	}));

static LyudDev::FAutoAction GDevBandNext(TEXT("band.play_next"), TEXT("cruise"), TEXT("dev.action.band.play_next"), [](UWorld* W)
{
	// the next tune of the band's book, here and now (to hear every arrangement without sailing)
	if (ULyudShipBand* Band = ULyudShipBand::Get(W))
	{
		Band->PlayNext();
	}
});
