// The hero's pianos and guitar: free play on the keyboard, the tunes he knows as an easy rhythm game, the people round him
// listening, clapping and (by a bonfire) singing along (docs/instruments.md).

#include "Music/LyudInstruments.h"

#include "Audio/LyudSoundscape.h"
#include "City/LyudCityThings.h"
#include "Components/AudioComponent.h"
#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Music/InstrumentSynth.h"
#include "Music/SLyudInstrument.h"
#include "Music/TunesModel.h"
#include "North/LyudCampfire.h"
#include "People/PeopleLife.h"
#include "People/TownTalk.h"
#include "Sound/SoundWaveProcedural.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudInstr, Log, All);

const FName ULyudInstruments::GuitarItem(TEXT("guitar"));

namespace LyudInstr
{
	/** The tunes of Content/Music/tunes.txt, in its order. */
	struct FTuneBook
	{
		TArray<LyudTunes::FTune> Tunes;
	};
}

namespace LyudInstrImpl
{
	constexpr int32 Rate = 32000;
	/** How far ahead of the device the notes are rendered: short, so a key sounds when it is pressed. */
	constexpr float AheadSeconds = 0.05f;
	constexpr int32 MaxBlock = Rate / 8;

	/** The keyboard: white keys A S D F G H J K L ; ' (C to F above the next C), black W E T Y U O P. */
	struct FKeyNote { FKey Key; int32 Semi; };
	const TArray<FKeyNote>& KeyNotes()
	{
		static const TArray<FKeyNote> K = {
			{EKeys::A, 0}, {EKeys::W, 1}, {EKeys::S, 2}, {EKeys::E, 3}, {EKeys::D, 4}, {EKeys::F, 5}, {EKeys::T, 6},
			{EKeys::G, 7}, {EKeys::Y, 8}, {EKeys::H, 9}, {EKeys::U, 10}, {EKeys::J, 11}, {EKeys::K, 12}, {EKeys::O, 13},
			{EKeys::L, 14}, {EKeys::P, 15}, {EKeys::Semicolon, 16}, {EKeys::Apostrophe, 17},
		};
		return K;
	}

	/** 1..9 (built on first use: InputCore's EKeys are statics of another module, unordered against ours in a packaged build). */
	const FKey& Digit(int32 I)
	{
		static const FKey D[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		return D[FMath::Clamp(I, 0, 8)];
	}

	FString Str(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/** The act id without a staff variant's "@role". */
	FString BareAct(const FString& Act)
	{
		int32 At = INDEX_NONE;
		return Act.FindChar(TEXT('@'), At) ? Act.Left(At) : Act;
	}
}

ULyudInstruments::ULyudInstruments() = default;
ULyudInstruments::~ULyudInstruments() = default;

ULyudInstruments* ULyudInstruments::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudInstruments>() : nullptr;
}

// ------------------------------------------------------------------------------------------------------------- the life cycle

void ULyudInstruments::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Synth = MakePimpl<LyudInstr::FSynth>(LyudInstrImpl::Rate);
	Game = MakePimpl<LyudTunes::FRhythm>();
	Book = MakePimpl<LyudInstr::FTuneBook>();
	Pcm.Reserve(LyudInstrImpl::MaxBlock);
	Mix.Reserve(LyudInstrImpl::MaxBlock);
	NoteOffs.Reserve(64);
	LoadTunes();

	if (ULyudCityThings* Things = Collection.InitializeDependency<ULyudCityThings>())
	{
		OpenHandle = Things->OnOpen.AddUObject(this, &ULyudInstruments::OnCityOpen);
		Things->AddActHook(ULyudCityThings::FActHook::CreateUObject(this, &ULyudInstruments::OnCityAct));
	}
	AfterLoad();
}

void ULyudInstruments::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// learned tunes and the times each was played: kept with the save (as ULyudMusic does, once the world plays)
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("Instruments"), this);
		LoadHandle = Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame* Save)
		{
			if (Save && !Save->Records.Contains(TEXT("Instruments")))
			{
				// a save from before the instruments: only the tunes everybody knows
				Learned.Reset();
				Played.Reset();
			}
			AfterLoad();
		});
		NewGameHandle = Saves->OnNewGame.AddWeakLambda(this, [this] { StartOver(); });
	}
}

void ULyudInstruments::Deinitialize()
{
	Close();
	if (Voice)
	{
		Voice->Stop();
		Voice = nullptr;
	}
	Wave = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (ULyudCityThings* Things = World->GetSubsystem<ULyudCityThings>())
		{
			Things->OnOpen.Remove(OpenHandle);
			Things->RemoveActHooks(this);
		}
		if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(World))
		{
			Saves->OnAfterLoad.Remove(LoadHandle);
			Saves->OnNewGame.Remove(NewGameHandle);
			Saves->UnregisterSaveObject(TEXT("Instruments"));
		}
	}
	Super::Deinitialize();
}

void ULyudInstruments::LoadTunes()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Music/tunes.txt");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogLyudInstr, Warning, TEXT("Instruments: no %s"), *Path);
		return;
	}
	std::vector<std::string> Errors;
	const std::vector<LyudTunes::FTune> Parsed = LyudTunes::Parse(std::string(TCHAR_TO_UTF8(*Text)), Errors);
	for (const std::string& E : Errors)
	{
		UE_LOG(LogLyudInstr, Warning, TEXT("Instruments: tunes.txt: %s"), *LyudInstrImpl::Str(E));
	}
	Book->Tunes.Reset();
	Book->Tunes.Reserve((int32)Parsed.size());
	for (const LyudTunes::FTune& T : Parsed)
	{
		Book->Tunes.Add(T);
	}
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: %d tunes"), Book->Tunes.Num());
}

int32 ULyudInstruments::NumTunes() const { return Book->Tunes.Num(); }

FString ULyudInstruments::TuneId(int32 Index) const
{
	return Book->Tunes.IsValidIndex(Index) ? LyudInstrImpl::Str(Book->Tunes[Index].Id) : FString();
}

int32 ULyudInstruments::FindTune(const FString& Id) const
{
	const std::string Want(TCHAR_TO_UTF8(*Id));
	return Book->Tunes.IndexOfByPredicate([&Want](const LyudTunes::FTune& T) { return T.Id == Want; });
}

void ULyudInstruments::StartOver()
{
	Learned.Reset();
	Played.Reset();
	AfterLoad();
}

void ULyudInstruments::AfterLoad()
{
	// the tunes everybody knows are always his; a tune taken out of tunes.txt is forgotten
	for (const LyudTunes::FTune& T : Book->Tunes)
	{
		if (T.bStart)
		{
			Learned.AddUnique(LyudInstrImpl::Str(T.Id));
		}
	}
	if (Book->Tunes.Num() > 0)
	{
		Learned.RemoveAll([this](const FString& Id) { return FindTune(Id) == INDEX_NONE; });
	}
	RefreshKnown();
}

bool ULyudInstruments::Learn(const FString& Id)
{
	if (FindTune(Id) == INDEX_NONE || Learned.Contains(Id))
	{
		return false;
	}
	Learned.Add(Id);
	RefreshKnown();
	ULyudQuests::Notify(this, TEXT("learn_tune"), Id);
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: learned %s (%d of %d)"), *Id, Learned.Num(), Book->Tunes.Num());
	return true;
}

// ------------------------------------------------------------------------------------------------------------------ the screen

bool ULyudInstruments::Open(bool bGuitar)
{
	UWorld* World = GetWorld();
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(World);
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || IsOpen() || (GameScreen && GameScreen->IsOpen()) || (UI && UI->IsOpen()))
	{
		return false;
	}
	const ULyudInventory* Inv = ULyudInventory::Get(World);
	if (bGuitar && (!Inv || Inv->Count(GuitarItem) <= 0))
	{
		return false;
	}
	bGuitarNow = bGuitar;
	Synth->SetInstrument(bGuitar ? LyudInstr::EInstrument::Guitar : LyudInstr::EInstrument::Piano);
	Synth->SetSustain(false);
	Base = 60;
	Down.Reset();
	NoteOffs.Reset();
	bList = false;
	Cursor = 0;
	HitShown = 0;
	DoneStars = -1;
	DoneTune.Reset();
	DoneIx = -1;
	PlayingFor = 0.f;
	PeopleTimer = 0.f;
	SingTimer = 2.f;
	FVector Fire;
	bByFire = bGuitar && FindBonfire(Fire);

	Screen = SNew(SLyudInstrument).Owner(this);
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 66);
	OpenedFrame = GFrameCounter;
	Lock(true);
	if (ULyudInventoryUI* MutableUI = ULyudInventoryUI::Get(World))
	{
		MutableUI->SetExternalScreen(this);
	}
	// the sound: one procedural voice, rendered a little ahead of the device as he plays
	if (!Wave)
	{
		Wave = NewObject<USoundWaveProcedural>(this);
		Wave->SetSampleRate(LyudInstrImpl::Rate);
		Wave->NumChannels = 1;
		Wave->Duration = INDEFINITELY_LOOPING_DURATION;
		Wave->SoundGroup = SOUNDGROUP_Music;
		Wave->bLooping = false;
		Wave->bCanProcessAsync = true;
	}
	if (!Voice)
	{
		Voice = UGameplayStatics::CreateSound2D(this, Wave, 1.f, 1.f, 0.f, nullptr, false, false);
	}
	if (Voice)
	{
		const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this);
		Voice->SetVolumeMultiplier(Scape ? FMath::Clamp(Scape->MasterVolume, 0.f, 1.f) : 1.f);
		if (!Voice->IsPlaying())
		{
			Wave->ResetAudio();
			Voice->Play();
		}
	}
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: %s open%s, %d tunes known"), bGuitar ? TEXT("guitar") : TEXT("piano"),
		bByFire ? TEXT(" by a bonfire") : TEXT(""), Learned.Num());
	return true;
}

void ULyudInstruments::Close()
{
	if (!Screen.IsValid())
	{
		return;
	}
	StopTune();
	Synth->SetSustain(false);
	for (const int32 M : Down)
	{
		Synth->NoteOff(M);
	}
	Down.Reset();
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->SetExternalScreen(nullptr);
		}
	}
	Screen.Reset();
	bList = false;
	Lock(false);
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: closed"));
}

void ULyudInstruments::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bLocked == bLock)
	{
		return;
	}
	bLocked = bLock;
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	PC->bShowMouseCursor = false;
	if (bLock)
	{
		PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(true).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

bool ULyudInstruments::IsKeyDown(int32 Midi) const { return Down.Contains(Midi); }
bool ULyudInstruments::IsSustain() const { return Synth && Synth->IsSustain(); }
const LyudTunes::FRhythm* ULyudInstruments::Rhythm() const { return GameTune >= 0 ? Game.Get() : nullptr; }

double ULyudInstruments::MusicClock() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

bool ULyudInstruments::CanPlay(int32 Index) const
{
	if (!Book->Tunes.IsValidIndex(Index) || !KnownIx.IsValidIndex(Index) || !KnownIx[Index])
	{
		return false;
	}
	const LyudTunes::FTune& T = Book->Tunes[Index];
	return bGuitarNow ? T.bGuitar : T.bPiano;
}

void ULyudInstruments::PlayableTunes(TArray<int32>& Out) const
{
	Out.Reset();
	for (int32 I = 0; I < Book->Tunes.Num(); ++I)
	{
		if (CanPlay(I))
		{
			Out.Add(I);
		}
	}
}

void ULyudInstruments::RefreshKnown()
{
	KnownIx.SetNum(Book->Tunes.Num());
	for (int32 I = 0; I < Book->Tunes.Num(); ++I)
	{
		KnownIx[I] = Learned.Contains(LyudInstrImpl::Str(Book->Tunes[I].Id));
	}
}

// ---------------------------------------------------------------------------------------------------------------- the tick

void ULyudInstruments::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	Pump(DeltaTime);
	if (!IsOpen())
	{
		return;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (!PC)
	{
		return;
	}
	if (Menu && Menu->IsOpen())
	{
		// the pause menu is over us: a tune can't wait under it (its clock is the real one)
		StopTune();
		return;
	}
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	const ULyudInventory* Inv = ULyudInventory::Get(World);
	if ((GameScreen && GameScreen->IsOpen()) || (bGuitarNow && Inv && Inv->Count(GuitarItem) <= 0))
	{
		Close();
		return;
	}
	if (OpenedFrame != GFrameCounter)
	{
		Keys();
	}
	if (!IsOpen())
	{
		return;
	}
	const double Now = MusicClock();
	TickGame(Now);
	// the held notes of a tune let go at their lengths
	for (int32 I = NoteOffs.Num() - 1; I >= 0; --I)
	{
		if (NoteOffs[I].Key <= Now)
		{
			if (!Down.Contains(NoteOffs[I].Value))
			{
				Synth->NoteOff(NoteOffs[I].Value);
			}
			NoteOffs.RemoveAtSwap(I, EAllowShrinking::No);
		}
	}
	TickPeople(DeltaTime);
}

void ULyudInstruments::Pump(float DeltaTime)
{
	if (!Wave || !Voice || !Voice->IsPlaying())
	{
		return;
	}
	// closed and quiet: the voice stops until the next time he plays
	if (!IsOpen() && Synth->ActiveVoices() == 0)
	{
		Voice->Stop();
		return;
	}
	const int32 Have = Wave->GetAvailableAudioByteCount() / (int32)sizeof(int16);
	const int32 Want = (int32)(LyudInstrImpl::Rate * LyudInstrImpl::AheadSeconds) - Have;
	if (Want <= 0)
	{
		return;
	}
	const int32 N = FMath::Min(Want + (int32)(DeltaTime * LyudInstrImpl::Rate), LyudInstrImpl::MaxBlock);
	Mix.SetNumUninitialized(N, EAllowShrinking::No);
	Pcm.SetNumUninitialized(N, EAllowShrinking::No);
	Synth->Render(Mix.GetData(), N);
	for (int32 I = 0; I < N; ++I)
	{
		Pcm[I] = (int16)FMath::Clamp(Mix[I] * 32000.f, -32767.f, 32767.f);
	}
	Wave->QueueAudio((const uint8*)Pcm.GetData(), N * (int32)sizeof(int16));
}

void ULyudInstruments::Keys()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	auto Pressed = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (Pressed(EKeys::Escape) || Pressed(EKeys::Tab))
	{
		if (bList)
		{
			bList = false;
		}
		else if (GameTune >= 0)
		{
			StopTune();
		}
		else
		{
			Close();
		}
		return;
	}
	// the sustain pedal (the guitar's strings ring by themselves)
	const bool bPedal = !bGuitarNow && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	if (bPedal != Synth->IsSustain())
	{
		Synth->SetSustain(bPedal);
	}

	// the list of tunes he knows
	if (bList)
	{
		TArray<int32>& List = ListBuf;
		PlayableTunes(List);
		if (List.Num() == 0)
		{
			bList = !Pressed(EKeys::Q);
			return;
		}
		Cursor = FMath::Clamp(Cursor, 0, List.Num() - 1);
		if (Pressed(EKeys::W) || Pressed(EKeys::Up))
		{
			Cursor = (Cursor + List.Num() - 1) % List.Num();
		}
		else if (Pressed(EKeys::S) || Pressed(EKeys::Down))
		{
			Cursor = (Cursor + 1) % List.Num();
		}
		int32 Pick = INDEX_NONE;
		for (int32 I = 0; I < 9 && I < List.Num(); ++I)
		{
			if (Pressed(LyudInstrImpl::Digit(I)))
			{
				Pick = I;
			}
		}
		if (Pressed(EKeys::E) || Pressed(EKeys::SpaceBar) || Pressed(EKeys::Enter))
		{
			Pick = Cursor;
		}
		if (Pick != INDEX_NONE)
		{
			bList = false;
			StartTune(TuneId(List[Pick]));
		}
		else if (Pressed(EKeys::Q))
		{
			bList = false;
		}
		return;
	}
	if (Pressed(EKeys::Q))
	{
		StopTune();
		for (const int32 M : Down)
		{
			Synth->NoteOff(M);
		}
		Down.Reset();
		bList = true;
		return;
	}

	// a tune: E or Space plays its next note when it comes to the line
	if (GameTune >= 0)
	{
		if (!bAutoPlay && (Pressed(EKeys::E) || Pressed(EKeys::SpaceBar)))
		{
			Strike(MusicClock());
		}
		return;
	}

	// free play
	if (Pressed(EKeys::Z))
	{
		Base = FMath::Max(36, Base - 12);
	}
	else if (Pressed(EKeys::X))
	{
		Base = FMath::Min(72, Base + 12);
	}
	for (const LyudInstrImpl::FKeyNote& K : LyudInstrImpl::KeyNotes())
	{
		const int32 Midi = Base + K.Semi;
		if (Pressed(K.Key))
		{
			FreeKey(Midi, true);
		}
		else if (Down.Contains(Midi) && !PC->IsInputKeyDown(K.Key))
		{
			FreeKey(Midi, false);
		}
	}
	if (bGuitarNow)
	{
		// 1-8: the chords a boy by a fire knows, strummed down from the low string
		for (int32 I = 0; I < 8; ++I)
		{
			if (Pressed(LyudInstrImpl::Digit(I)))
			{
				const std::vector<int> Notes = LyudTunes::ChordNotes(LyudTunes::FreeChord(I));
				int32 Delay = 0;
				for (const int M : Notes)
				{
					Synth->NoteOn(M, 0.7f, Delay);
					Delay += LyudInstrImpl::Rate / 70;
				}
				PlayingFor = FMath::Max(PlayingFor, 0.01f);
			}
		}
	}
}

void ULyudInstruments::FreeKey(int32 Midi, bool bPressed)
{
	if (bPressed)
	{
		Synth->NoteOn(Midi, 0.72f);
		Down.Add(Midi);
		PlayingFor = FMath::Max(PlayingFor, 0.01f);
	}
	else
	{
		Synth->NoteOff(Midi);
		Down.Remove(Midi);
	}
}

// ------------------------------------------------------------------------------------------------------------ the rhythm game

void ULyudInstruments::Strike(double Now)
{
	LyudTunes::EHit Hit = LyudTunes::EHit::None;
	const int32 Note = Game->Press(Now, Hit);
	HitShown = Hit == LyudTunes::EHit::Perfect ? 1 : (Hit == LyudTunes::EHit::Good ? 2 : 3);
	HitAt = Now;
	if (Note < 0)
	{
		return;
	}
	const LyudTunes::FTune& T = Book->Tunes[GameTune];
	const LyudTunes::FNote& N = T.Notes[(size_t)Note];
	const float Vel = Hit == LyudTunes::EHit::Perfect ? 0.8f : 0.65f;
	int32 Delay = 0;
	for (const int M : N.Midi)
	{
		Synth->NoteOn(M, Vel, Delay);
		NoteOffs.Add(TPair<double, int32>(Now + N.Len * T.SecondsPerBeat() * 0.95, M));
		// a guitar's chord in the melody is strummed, a piano's struck at once
		Delay += bGuitarNow ? LyudInstrImpl::Rate / 90 : 0;
	}
}

bool ULyudInstruments::StartTune(const FString& Id, bool bAuto)
{
	const int32 Ix = FindTune(Id);
	if (!IsOpen() || Ix == INDEX_NONE || !Learned.Contains(Id))
	{
		return false;
	}
	const LyudTunes::FTune& T = Book->Tunes[Ix];
	if (!(bGuitarNow ? T.bGuitar : T.bPiano))
	{
		return false;
	}
	for (const int32 M : Down)
	{
		Synth->NoteOff(M);
	}
	Down.Reset();
	NoteOffs.Reset();
	GameTune = Ix;
	NextChord = 0;
	bAutoPlay = bAuto;
	DoneStars = -1;
	DoneTune.Reset();
	DoneIx = -1;
	HitShown = 0;
	Game->Start(Book->Tunes[Ix], MusicClock());
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: playing %s (%d notes, %.0f bpm)"), *Id, T.NumPlayed(), T.Bpm);
	return true;
}

void ULyudInstruments::StopTune()
{
	if (GameTune < 0)
	{
		return;
	}
	GameTune = -1;
	NextChord = 0;
	*Game = LyudTunes::FRhythm();
}

void ULyudInstruments::TickGame(double Now)
{
	if (GameTune < 0)
	{
		return;
	}
	const LyudTunes::FTune& T = Book->Tunes[GameTune];
	// the auto player strikes each note as it comes to the line
	while (bAutoPlay && GameTune >= 0 && Game->Next < (int32)T.Notes.size() && Game->TimeOf(Game->Next) <= Now)
	{
		Strike(Now);
	}
	if (Game->Update(Now) > 0)
	{
		HitShown = 3;
		HitAt = Now;
	}
	// the accompaniment: the guitar strums the tune's chords, the piano's left hand holds them softly an octave up
	const double Spb = T.SecondsPerBeat();
	while (NextChord < (int32)T.Chords.size() && Game->T0 + T.Chords[(size_t)NextChord].Beat * Spb <= Now)
	{
		const LyudTunes::FChord& C = T.Chords[(size_t)NextChord++];
		if (C.Name == "r")
		{
			continue;
		}
		const std::vector<int> Notes = LyudTunes::ChordNotes(C.Name);
		int32 Delay = 0;
		for (const int M : Notes)
		{
			if (bGuitarNow)
			{
				Synth->NoteOn(M, 0.42f, Delay);
				Delay += LyudInstrImpl::Rate / 80;
			}
			else if (M >= 43)
			{
				// the guitar's voicing without its lowest strings: a soft chord under the melody
				Synth->NoteOn(M, 0.26f);
				NoteOffs.Add(TPair<double, int32>(Now + C.Len * Spb * 0.92, M));
			}
		}
	}
	PlayingFor = FMath::Max(PlayingFor, 0.01f);
	if (Game->bDone)
	{
		FinishTune();
	}
}

void ULyudInstruments::FinishTune()
{
	if (GameTune < 0)
	{
		return;
	}
	const FString Id = TuneId(GameTune);
	const int32 Stars = Game->Stars();
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: %s done: %d perfect, %d good, %d missed, %d stray, score %.2f, %d stars"), *Id,
		Game->Perfect, Game->Good, Game->Miss, Game->Stray, Game->Score(), Stars);
	DoneStars = Stars;
	DoneTune = Id;
	DoneIx = GameTune;
	StopTune();
	const FText Title = LyudText::Get(TEXT("music.tune.") + Id);
	if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
	{
		GS->Toast(LyudText::Get(Stars > 0 ? TEXT("music.toast.played") : TEXT("music.toast.stumbled"),
			{ FStringFormatArg(Title.ToString()), FStringFormatArg(Stars) }));
	}
	if (Stars > 0)
	{
		Played.FindOrAdd(Id)++;
		ULyudQuests::Notify(this, TEXT("play_tune"), Id);
		if (bGuitarNow && bByFire)
		{
			ULyudQuests::Notify(this, TEXT("play_tune"), TEXT("bonfire"));
		}
	}
	Applause(Stars);
}

// ---------------------------------------------------------------------------------------------------------- the people round him

bool ULyudInstruments::FindBonfire(FVector& Out) const
{
	UWorld* World = GetWorld();
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return false;
	}
	const FVector At = Pawn->GetActorLocation();
	constexpr double Reach = 900.0;
	double Best = Reach * Reach;
	bool bFound = false;
	for (TActorIterator<ALyudBonfire> It(World); It; ++It)
	{
		const double D = FVector::DistSquared(It->GetActorLocation(), At);
		if (D < Best)
		{
			Best = D;
			Out = It->GetActorLocation();
			bFound = true;
		}
	}
	for (TActorIterator<ALyudCampfire> It(World); It; ++It)
	{
		const double D = FVector::DistSquared(It->GetActorLocation(), At);
		if (It->IsLit() && D < Best)
		{
			Best = D;
			Out = It->GetActorLocation();
			bFound = true;
		}
	}
	return bFound;
}

void ULyudInstruments::TickPeople(float DeltaTime)
{
	// he is playing while notes sound; a minute's silence and the listeners go back to their own business
	if (Synth->ActiveVoices() > 0 || GameTune >= 0)
	{
		PlayingFor += DeltaTime;
	}
	else
	{
		PlayingFor = FMath::Max(0.f, PlayingFor - DeltaTime * 2.f);
	}
	if ((PeopleTimer -= DeltaTime) > 0.f)
	{
		return;
	}
	PeopleTimer = 1.5f;
	FVector Fire;
	bByFire = bGuitarNow && FindBonfire(Fire);
	if (PlayingFor < 2.f)
	{
		return;  // a note or two is no music yet
	}
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Life || !Pawn)
	{
		return;
	}
	const FVector At = Pawn->GetActorLocation();
	const double Reach = bByFire ? 1100.0 : 1000.0;
	int32 Noticer = INDEX_NONE;
	const int32 N = Life->NumPeople();
	for (int32 Id = 0; Id < N; ++Id)
	{
		int32 Activity = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector Where;
		if (!Life->GetActivityState(Id, Activity, bDoing, Pose, Where) || FVector::DistSquared(Where, At) > Reach * Reach
			|| FMath::Abs(Where.Z - At.Z) > 300.0)
		{
			continue;
		}
		// the ones standing sway a little to it (the warm more readily); somebody may say a word about it
		if (Pose == EPersonPose::Stand && FMath::FRand() < 0.25f + 0.5f * Life->GetTrait(Id, TEXT("warmth")))
		{
			Life->ReactManner(Id, TEXT("sway"), 2.2f);
		}
		if (Noticer == INDEX_NONE || FMath::FRand() < 0.3f)
		{
			Noticer = Id;
		}
	}
	if (Noticer != INDEX_NONE)
	{
		Life->OnPersonEvent.Broadcast(Noticer, TEXT("hero_plays"), -1);
	}
	// by a bonfire with the guitar, the circle sings the tune's words with him (a line every few seconds, one voice at a time)
	if (bByFire && GameTune >= 0 && Book->Tunes[GameTune].bSing && (SingTimer -= 1.5f) <= 0.f)
	{
		SingTimer = 4.5f;
		Sing();
	}
}

void ULyudInstruments::Sing()
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	ULyudTownTalkSubsystem* Talk = ULyudTownTalkSubsystem::Get(this);
	FVector Fire;
	if (!Life || !Talk || GameTune < 0 || !FindBonfire(Fire))
	{
		return;
	}
	const FName Event(*(TEXT("hero_sing_") + TuneId(GameTune)));
	TArray<int32, TInlineAllocator<16>> Round;
	const int32 N = Life->NumPeople();
	for (int32 Id = 0; Id < N && Round.Num() < 16; ++Id)
	{
		int32 Activity = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector Where;
		if (Life->GetActivityState(Id, Activity, bDoing, Pose, Where) && FVector::DistSquared(Where, Fire) < 1200.0 * 1200.0)
		{
			Round.Add(Id);
		}
	}
	for (int32 Tries = 0; Tries < 3 && Round.Num() > 0; ++Tries)
	{
		const int32 Id = Round[FMath::RandRange(0, Round.Num() - 1)];
		FPersonInfo Info;
		AActor* Actor = Life->GetPerson(Id, Info) ? Info.Actor.Get() : nullptr;
		if (IsValid(Actor) && !Actor->IsHidden() && Talk->SayEvent(Actor, Event, Id))
		{
			Life->ReactManner(Id, TEXT("sway"), 3.f);
			return;
		}
	}
}

void ULyudInstruments::Applause(int32 Stars)
{
	UPeopleLifeSubsystem* Life = UPeopleLifeSubsystem::Get(this);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Life || !Pawn || Stars <= 0)
	{
		return;
	}
	const FVector At = Pawn->GetActorLocation();
	int32 Clappers = 0;
	int32 First = INDEX_NONE;
	const int32 N = Life->NumPeople();
	for (int32 Id = 0; Id < N; ++Id)
	{
		int32 Activity = -1;
		bool bDoing = false;
		EPersonPose Pose = EPersonPose::Stand;
		FVector Where;
		if (!Life->GetActivityState(Id, Activity, bDoing, Pose, Where) || FVector::DistSquared(Where, At) > 1200.0 * 1200.0
			|| FMath::Abs(Where.Z - At.Z) > 300.0)
		{
			continue;
		}
		// three stars: everybody near claps; one: the kind ones
		if (FMath::FRand() < 0.35f * Stars + 0.3f * Life->GetTrait(Id, TEXT("warmth")) && Life->ReactManner(Id, TEXT("clap"), 1.5f + Stars))
		{
			++Clappers;
			if (First == INDEX_NONE)
			{
				First = Id;
			}
		}
	}
	if (First != INDEX_NONE)
	{
		Life->OnPersonEvent.Broadcast(First, TEXT("hero_played"), -1);
	}
	UE_LOG(LogLyudInstr, Display, TEXT("Instruments: %d people clap"), Clappers);
}

// ------------------------------------------------------------------------------------------------------------------- the town

void ULyudInstruments::OnCityOpen(FName InScreen, int32)
{
	if (InScreen == TEXT("piano"))
	{
		Open(false);
	}
}

bool ULyudInstruments::OnCityAct(const FCityActInfo& Info, FText& Line)
{
	// sheet music on a music stand, a blackboard, a library shelf: the next tune there he doesn't know yet
	const std::string Act(TCHAR_TO_UTF8(*LyudInstrImpl::BareAct(Info.Act)));
	for (const LyudTunes::FTune& T : Book->Tunes)
	{
		if (std::find(T.Sheets.begin(), T.Sheets.end(), Act) == T.Sheets.end())
		{
			continue;
		}
		const FString Id = LyudInstrImpl::Str(T.Id);
		if (Learned.Contains(Id))
		{
			continue;
		}
		Learn(Id);
		const FText Title = LyudText::Get(TEXT("music.tune.") + Id);
		Line = LyudText::Get(TEXT("music.sheet.learned"), { FStringFormatArg(Title.ToString()) });
		if (ULyudGameScreen* GS = ULyudGameScreen::Get(this))
		{
			GS->Toast(LyudText::Get(TEXT("music.toast.learned"), { FStringFormatArg(Title.ToString()) }));
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------------------------- the console and developer mode

static FAutoConsoleCommandWithWorldAndArgs GInstrCmd(TEXT("lyud.Instr"),
	TEXT("lyud.Instr piano | guitar | close | learn <id|all> | play <id> | auto <id> | list: the hero's instruments (docs/instruments.md)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudInstruments* I = ULyudInstruments::Get(World);
		if (!I)
		{
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0].ToLower() : TEXT("list");
		const FString Arg = Args.Num() > 1 ? Args[1] : FString();
		if (Cmd == TEXT("piano") || Cmd == TEXT("guitar"))
		{
			const bool bGuitar = Cmd == TEXT("guitar");
			if (bGuitar)
			{
				if (ULyudInventory* Inv = ULyudInventory::Get(World); Inv && Inv->Count(ULyudInstruments::GuitarItem) <= 0)
				{
					Inv->Add(ULyudInstruments::GuitarItem);
				}
			}
			I->Close();
			UE_LOG(LogLyudInstr, Display, TEXT("Instruments: open %s: %s"), *Cmd, I->Open(bGuitar) ? TEXT("yes") : TEXT("no"));
		}
		else if (Cmd == TEXT("close"))
		{
			I->Close();
		}
		else if (Cmd == TEXT("learn"))
		{
			for (int32 T = 0; T < I->NumTunes(); ++T)
			{
				if (Arg == TEXT("all") || Arg == I->TuneId(T))
				{
					I->Learn(I->TuneId(T));
				}
			}
		}
		else if (Cmd == TEXT("play") || Cmd == TEXT("auto"))
		{
			// auto: every note struck on time by itself (a recording of a tune, the bench)
			UE_LOG(LogLyudInstr, Display, TEXT("Instruments: %s %s: %s"), *Cmd, *Arg, I->StartTune(Arg, Cmd == TEXT("auto")) ? TEXT("yes") : TEXT("no"));
		}
		else
		{
			for (int32 T = 0; T < I->NumTunes(); ++T)
			{
				const FString Id = I->TuneId(T);
				const int32* N = I->Played.Find(Id);
				UE_LOG(LogLyudInstr, Display, TEXT("  %-16s %s played %d"), *Id, I->Knows(Id) ? TEXT("known  ") : TEXT("unknown"), N ? *N : 0);
			}
		}
	}));

static LyudDev::FAutoAction GDevInstrPiano(TEXT("instruments.piano"), TEXT("instruments"), TEXT("dev.action.instruments.piano"), [](UWorld* W)
{
	if (ULyudInstruments* I = ULyudInstruments::Get(W))
	{
		I->Close();
		I->Open(false);
	}
});

static LyudDev::FAutoAction GDevInstrGuitar(TEXT("instruments.guitar"), TEXT("instruments"), TEXT("dev.action.instruments.guitar"), [](UWorld* W)
{
	ULyudInstruments* I = ULyudInstruments::Get(W);
	ULyudInventory* Inv = ULyudInventory::Get(W);
	if (I && Inv)
	{
		if (Inv->Count(ULyudInstruments::GuitarItem) <= 0)
		{
			Inv->Add(ULyudInstruments::GuitarItem);
		}
		I->Close();
		I->Open(true);
	}
});

static LyudDev::FAutoAction GDevInstrLearn(TEXT("instruments.learn_all"), TEXT("instruments"), TEXT("dev.action.instruments.learn_all"), [](UWorld* W)
{
	if (ULyudInstruments* I = ULyudInstruments::Get(W))
	{
		for (int32 T = 0; T < I->NumTunes(); ++T)
		{
			I->Learn(I->TuneId(T));
		}
	}
});

static LyudDev::FAutoAction GDevHallPiano(TEXT("instruments.hall_piano"), TEXT("instruments"), TEXT("dev.action.instruments.hall_piano"), [](UWorld* W)
{
	// to the stage piano in the hall «Своды», then its keyboard
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->GoToThing(TEXT("P-HALL/stage_piano"));
	}
	if (ULyudInstruments* I = ULyudInstruments::Get(W))
	{
		I->Close();
		I->Open(false);
	}
});

static LyudDev::FAutoAction GDevInstrAuto(TEXT("instruments.auto"), TEXT("instruments"), TEXT("dev.action.instruments.auto"), [](UWorld* W)
{
	// a tune played by itself on the piano here: to hear the synth, to record, to bench (lyud.Instr auto <id> for another)
	if (ULyudInstruments* I = ULyudInstruments::Get(W))
	{
		I->Learn(TEXT("korobeiniki"));
		if (!I->IsOpen())
		{
			I->Open(false);
		}
		I->StartTune(TEXT("korobeiniki"), true);
	}
});
