#include "AtonLab.h"

#include "AtonOS/AtonApps.h"
#include "AtonOS/Lab/LabNet.h"
#include "Camera/CameraComponent.h"
#include "City/CityKit.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/LyudQuests.h"
#include "Weather/LyudWeather.h"

namespace LyudAtonLabDetail
{
	const TCHAR* PropMatPath = TEXT("/Game/City/M_CityProp.M_CityProp");
	const TCHAR* TerminalsFile = TEXT("AtonOS/terminals.json");

	// the desk's own frame, metres: +X out of the screen (toward the hero), origin on the floor under the middle of the desk's front edge
	constexpr float DeskDepth = 0.62f, DeskHalfWidth = 0.60f, DeskTop = 0.755f;
	constexpr float ScreenWidthCm = 34.f;
	/** The monitor's front face and the middle of its screen. */
	constexpr float MonitorFrontX = -0.20f, ScreenZ = 0.985f;
	/** The view sits this far in front of the screen, cm. */
	constexpr float ViewDistanceCm = 50.f;
	/** How near the hero must stand (to the desk's front edge) and how straight he must look at the screen for the prompt. */
	constexpr float UseReachCm = 150.f, UseDot = 0.5f;

	/** The terminal: a plain office desk with a drawer block, a beige tube monitor on its foot, a keyboard, a stool pushed under. */
	void BuildTerminal(CityKit::FMesh& M)
	{
		const CityKit::FSkin Wood = CityKit::Paint(133, 94, 60, 0.5f);
		const CityKit::FSkin WoodDark = CityKit::Paint(96, 66, 42, 0.55f);
		const CityKit::FSkin Beige = CityKit::Paint(206, 197, 172, 0.6f);
		const CityKit::FSkin BeigeDark = CityKit::Paint(171, 162, 139, 0.65f);
		const CityKit::FSkin Keys = CityKit::Paint(226, 220, 200, 0.55f);
		const CityKit::FSkin Dark = CityKit::Paint(34, 36, 38, 0.35f);
		const CityKit::FSkin Steel = CityKit::Iron();
		const float Mid = -DeskDepth * 0.5f;

		// the desk: top, two side panels, a modesty panel, a drawer block on the right
		M.Box(FVector(Mid, 0, DeskTop - 0.015f), FVector(DeskDepth * 0.5f, DeskHalfWidth, 0.015f), Wood, FRotator::ZeroRotator, true);
		for (const float Side : { -1.f, 1.f })
		{
			M.Box(FVector(Mid, Side * (DeskHalfWidth - 0.03f), (DeskTop - 0.03f) * 0.5f), FVector(DeskDepth * 0.5f - 0.02f, 0.012f, (DeskTop - 0.03f) * 0.5f), WoodDark);
		}
		M.Box(FVector(-DeskDepth + 0.04f, 0, 0.47f), FVector(0.01f, DeskHalfWidth - 0.04f, 0.24f), WoodDark);
		M.Box(FVector(Mid, 0.38f, 0.585f), FVector(DeskDepth * 0.5f - 0.03f, 0.17f, 0.14f), Wood, FRotator::ZeroRotator, true);
		for (const float Z : { 0.65f, 0.52f })
		{
			// drawer fronts and their pulls
			M.Box(FVector(-0.03f, 0.38f, Z), FVector(0.006f, 0.15f, 0.055f), WoodDark);
			M.Box(FVector(-0.02f, 0.38f, Z), FVector(0.008f, 0.04f, 0.008f), Steel);
		}

		// the monitor: a foot, a neck, the tube's case with a darker bezel round the screen (the screen itself is the OS's widget)
		const float CaseHalfW = 0.215f, CaseHalfH = 0.175f, CaseDepth = 0.36f;
		M.Box(FVector(MonitorFrontX - 0.17f, 0, DeskTop + 0.012f), FVector(0.12f, 0.14f, 0.012f), BeigeDark);
		M.Box(FVector(MonitorFrontX - 0.17f, 0, DeskTop + 0.04f), FVector(0.06f, 0.07f, 0.02f), BeigeDark);
		M.Box(FVector(MonitorFrontX - CaseDepth * 0.5f, 0, ScreenZ), FVector(CaseDepth * 0.5f, CaseHalfW, CaseHalfH), Beige, FRotator::ZeroRotator, true);
		// the tube's narrower back
		M.Box(FVector(MonitorFrontX - CaseDepth - 0.03f, 0, ScreenZ - 0.01f), FVector(0.03f, CaseHalfW - 0.06f, CaseHalfH - 0.05f), BeigeDark);
		// the bezel: four bars standing 6 mm proud of the case round the 34 x 25.5 cm screen
		const float SW = ScreenWidthCm * 0.005f, SH = SW * 0.75f;
		M.Box(FVector(MonitorFrontX + 0.003f, 0, ScreenZ + SH + 0.012f), FVector(0.003f, SW + 0.024f, 0.012f), BeigeDark);
		M.Box(FVector(MonitorFrontX + 0.003f, 0, ScreenZ - SH - 0.012f), FVector(0.003f, SW + 0.024f, 0.012f), BeigeDark);
		for (const float Side : { -1.f, 1.f })
		{
			M.Box(FVector(MonitorFrontX + 0.003f, Side * (SW + 0.012f), ScreenZ), FVector(0.003f, 0.012f, SH), BeigeDark);
		}
		// the dark glass behind the widget (seen at the edges and before the OS draws)
		M.Box(FVector(MonitorFrontX + 0.001f, 0, ScreenZ), FVector(0.001f, SW, SH), Dark);
		// a power lamp and the two knobs under the screen
		M.Box(FVector(MonitorFrontX + 0.004f, CaseHalfW - 0.035f, ScreenZ - CaseHalfH + 0.012f), FVector(0.003f, 0.006f, 0.004f), CityKit::Paint(120, 220, 120, 0.3f));
		for (const float Y : { -0.02f, 0.02f })
		{
			M.Box(FVector(MonitorFrontX + 0.006f, CaseHalfW - 0.09f + Y, ScreenZ - CaseHalfH + 0.012f), FVector(0.006f, 0.008f, 0.005f), BeigeDark);
		}

		// the keyboard, tilted a little toward the hands
		const FRotator Tilt(-5.f, 0.f, 0.f);
		M.Box(FVector(-0.105f, 0, DeskTop + 0.016f), FVector(0.08f, 0.225f, 0.014f), Beige, Tilt);
		M.Box(FVector(-0.105f, -0.03f, DeskTop + 0.032f), FVector(0.06f, 0.17f, 0.004f), Keys, Tilt);
		M.Box(FVector(-0.105f, 0.185f, DeskTop + 0.032f), FVector(0.06f, 0.03f, 0.004f), Keys, Tilt);

		// a stool pushed under the desk on the left
		const FVector Stool(-0.27f, -0.24f, 0.f);
		M.Tube(Stool + FVector(0, 0, 0.42f), Stool + FVector(0, 0, 0.46f), 0.16f, 0.16f, 14, WoodDark, true, true);
		M.Tube(Stool + FVector(0, 0, 0.03f), Stool + FVector(0, 0, 0.42f), 0.02f, 0.02f, 8, Steel, false, false);
		M.Tube(Stool, Stool + FVector(0, 0, 0.03f), 0.13f, 0.03f, 12, Steel, false, false);

		M.Collide(FVector(Mid, 0, DeskTop * 0.5f), FVector(DeskDepth * 0.5f, DeskHalfWidth, DeskTop * 0.5f));
		M.Collide(FVector(MonitorFrontX - CaseDepth * 0.5f, 0, ScreenZ), FVector(CaseDepth * 0.5f, CaseHalfW, CaseHalfH));
	}

	AHeroCharacter* PlayerHero(const UWorld* World, APlayerController** OutPC = nullptr)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (OutPC)
		{
			*OutPC = PC;
		}
		return PC ? Cast<AHeroCharacter>(PC->GetPawn()) : nullptr;
	}

	void SetAllProfiles(UWorld* World, FName Profile)
	{
		for (TActorIterator<AAtonComputer> It(World); It; ++It)
		{
			// the lab's own terminals stay what they are
			if (It->bUseSeat)
			{
				It->SetProfile(Profile);
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GProfileCmd(TEXT("lyud.Aton.Profile"),
		TEXT("club | lab: the hero's computer runs its own programs or the lab network's (to try the terminals' programs in the club)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			SetAllProfiles(World, Args.Num() > 0 && Args[0] == TEXT("lab") ? AtonApps::ProfileLab : AtonApps::ProfileClub);
		}));

	FAutoConsoleCommandWithWorldAndArgs GLabCmd(TEXT("lyud.Aton.Lab"),
		TEXT("login | logout | reset | state: the lab network's session and saved state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ULyudAtonLab* Lab = ULyudAtonLab::Get(World);
			if (!Lab)
			{
				return;
			}
			const FString What = Args.Num() > 0 ? Args[0] : TEXT("state");
			if (What == TEXT("login"))
			{
				Lab->State().bStaff = true;
			}
			else if (What == TEXT("logout"))
			{
				Lab->State().bStaff = false;
			}
			else if (What == TEXT("reset"))
			{
				Lab->State() = AtonLab::FLabState();
			}
			UE_LOG(LogTemp, Display, TEXT("ATONLAB state %s"), *FString(UTF8_TO_TCHAR(Lab->State().Save().c_str())));
		}));

	FAutoConsoleCommandWithWorldAndArgs GUseCmd(TEXT("lyud.Aton.Use"),
		TEXT("The hero sits down to the nearest lab terminal (as E in front of it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			APlayerController* PC = nullptr;
			const AHeroCharacter* Hero = PlayerHero(World, &PC);
			ULyudAtonLab* Lab = ULyudAtonLab::Get(World);
			ALyudLabTerminal* T = Lab && PC ? Lab->NearestTerminal(Hero ? Hero->GetActorLocation() : FVector::ZeroVector) : nullptr;
			UE_LOG(LogTemp, Display, TEXT("ATONLAB use %s"), T ? *T->TerminalId.ToString() : TEXT("(no terminal)"));
			if (T)
			{
				T->Use(PC);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GLeaveCmd(TEXT("lyud.Aton.Leave"),
		TEXT("The hero gets up from the lab terminal he is at."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (const ULyudAtonLab* Lab = ULyudAtonLab::Get(World))
			{
				for (ALyudLabTerminal* T : Lab->GetTerminals())
				{
					if (T && T->IsActive())
					{
						T->StopUsing();
					}
				}
			}
		}));

	// developer mode (docs/dev_mode.md): the lab's programs on the club computer, the duty shift's session, a clean state
	LyudDev::FAutoAction GDevProfile(TEXT("aton.lab_profile"), TEXT("aton"), TEXT("dev.action.aton.lab_profile"),
		[](UWorld* W)
		{
			bool bLab = false;
			for (TActorIterator<AAtonComputer> It(W); It; ++It)
			{
				bLab = bLab || (It->bUseSeat && It->Profile == AtonApps::ProfileLab);
			}
			SetAllProfiles(W, bLab ? AtonApps::ProfileClub : AtonApps::ProfileLab);
		},
		[](UWorld* W)
		{
			for (TActorIterator<AAtonComputer> It(W); It; ++It)
			{
				if (It->bUseSeat && It->Profile == AtonApps::ProfileLab)
				{
					return true;
				}
			}
			return false;
		});
	LyudDev::FAutoAction GDevLogin(TEXT("aton.lab_login"), TEXT("aton"), TEXT("dev.action.aton.lab_login"),
		[](UWorld* W)
		{
			if (ULyudAtonLab* Lab = ULyudAtonLab::Get(W))
			{
				Lab->State().bStaff = !Lab->State().bStaff;
			}
		},
		[](UWorld* W)
		{
			ULyudAtonLab* Lab = ULyudAtonLab::Get(W);
			return Lab && Lab->State().bStaff;
		});
	LyudDev::FAutoAction GDevReset(TEXT("aton.lab_reset"), TEXT("aton"), TEXT("dev.action.aton.lab_reset"),
		[](UWorld* W)
		{
			if (ULyudAtonLab* Lab = ULyudAtonLab::Get(W))
			{
				Lab->State() = AtonLab::FLabState();
			}
		});
}

// ---------------------------------------------------------------- the terminal

ALyudLabTerminal::ALyudLabTerminal()
{
	Profile = AtonApps::ProfileLab;
	bUseSeat = false;
	ScreenWidth = LyudAtonLabDetail::ScreenWidthCm;
	Brightness = 4.f;

	Desk = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Desk"));
	Desk->SetupAttachment(RootComponent);
	Desk->SetCanEverAffectNavigation(false);

	// 4 mm in front of the dark glass
	Screen->SetRelativeLocation(FVector((LyudAtonLabDetail::MonitorFrontX + 0.005f) * 100.f, 0.f, LyudAtonLabDetail::ScreenZ * 100.f));

	Eye = CreateDefaultSubobject<UCameraComponent>(TEXT("Eye"));
	Eye->SetupAttachment(RootComponent);
	Eye->SetRelativeLocation(FVector(LyudAtonLabDetail::MonitorFrontX * 100.f + LyudAtonLabDetail::ViewDistanceCm, 0.f, LyudAtonLabDetail::ScreenZ * 100.f));
	Eye->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	// the 4:3 screen fills most of a wide view's height
	Eye->SetFieldOfView(54.f);
	Eye->bConstrainAspectRatio = false;

	Tags.Add(TEXT("Academ"));
	ApplyScreenSize();
}

void ALyudLabTerminal::BeginPlay()
{
	CityKit::FMesh M;
	LyudAtonLabDetail::BuildTerminal(M);
	if (UStaticMesh* Mesh = M.Build(this, TEXT("SM_LabTerminal"), true))
	{
		Desk->SetStaticMesh(Mesh);
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, LyudAtonLabDetail::PropMatPath))
		{
			Desk->SetMaterial(0, Mat);
		}
		Desk->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Desk->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// a small thing indoors: it takes the room's light, it does not add to the global illumination's scene
		Desk->bAffectDynamicIndirectLighting = false;
	}
	Super::BeginPlay();
	OnLeave.AddDynamic(this, &ALyudLabTerminal::OnLeft);
}

FVector ALyudLabTerminal::ViewPoint() const
{
	return Eye->GetComponentLocation();
}

FVector ALyudLabTerminal::ScreenPoint() const
{
	return Screen->GetComponentLocation();
}

void ALyudLabTerminal::Use(APlayerController* PC)
{
	if (!PC || IsActive())
	{
		return;
	}
	PC->SetViewTargetWithBlend(this, 0.6f, VTBlend_Cubic);
	Activate(PC);
	UE_LOG(LogTemp, Display, TEXT("ATONLAB terminal %s in use"), *TerminalId.ToString());
}

void ALyudLabTerminal::StopUsing()
{
	if (!IsActive())
	{
		return;
	}
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	Deactivate();
	OnLeft(PC);
}

void ALyudLabTerminal::OnLeft(APlayerController* Player)
{
	if (Player && Player->GetPawn())
	{
		Player->SetViewTargetWithBlend(Player->GetPawn(), 0.45f, VTBlend_Cubic);
	}
	UE_LOG(LogTemp, Display, TEXT("ATONLAB terminal %s left"), *TerminalId.ToString());
}

// ---------------------------------------------------------------- the lab network in a world

ULyudAtonLab::ULyudAtonLab()
{
}

ULyudAtonLab::~ULyudAtonLab()
{
}

ULyudAtonLab* ULyudAtonLab::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudAtonLab>() : nullptr;
}

AtonLab::FLabState& ULyudAtonLab::State()
{
	if (!Lab.IsValid())
	{
		Lab = MakePimpl<AtonLab::FLabState>();
	}
	return *Lab;
}

FDateTime ULyudAtonLab::DayDate(int32 Days) const
{
	const UGameCalendar* Calendar = GetWorld() ? GetWorld()->GetSubsystem<UGameCalendar>() : nullptr;
	const FDateTime Start = Calendar ? Calendar->StartDate : FDateTime(2026, 10, 20, 16, 0, 0);
	return Start.GetDate() + FTimespan::FromDays(double(Days));
}

double ULyudAtonLab::NowHours() const
{
	return (UGameCalendar::NowIn(GetWorld()) - DayDate(0)).GetTotalHours();
}

float ULyudAtonLab::CloudAt(double Hours) const
{
	const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(this);
	return Weather ? Weather->GetStateAt(DayDate(0) + FTimespan::FromHours(Hours)).CloudCover : 0.f;
}

void ULyudAtonLab::ResolveScope()
{
	AtonLab::FLabState& S = State();
	if (S.Bookings.empty())
	{
		return;
	}
	const int32 Added = S.Resolve(NowHours(), [this](double Hours) { return CloudAt(Hours); });
	if (Added > 0)
	{
		Report(TEXT("scope"));
	}
}

void ULyudAtonLab::Report(const TCHAR* What)
{
	ULyudQuests::Notify(this, TEXT("do"), FString(TEXT("aton_")) + What);
	UE_LOG(LogTemp, Display, TEXT("ATONLAB %s"), What);
}

ALyudLabTerminal* ULyudAtonLab::NearestTerminal(const FVector& To) const
{
	ALyudLabTerminal* Best = nullptr;
	double BestD = 0.0;
	for (ALyudLabTerminal* T : Terminals)
	{
		const double D = T ? FVector::DistSquared(T->GetActorLocation(), To) : 0.0;
		if (T && (!Best || D < BestD))
		{
			Best = T;
			BestD = D;
		}
	}
	return Best;
}

void ULyudAtonLab::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	State();
	if (ULyudSaveSubsystem* Saves = ULyudSaveSubsystem::Get(&InWorld))
	{
		Saves->RegisterSaveObject(TEXT("AtonLab"), this);
		Saves->OnNewGame.AddUObject(this, &ULyudAtonLab::StartOver);
	}
	if (InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		SpawnTerminals(InWorld);
	}
}

void ULyudAtonLab::StartOver()
{
	State() = AtonLab::FLabState();
}

void ULyudAtonLab::OnGameSaving_Implementation()
{
	Saved = UTF8_TO_TCHAR(State().Save().c_str());
}

void ULyudAtonLab::OnGameLoaded_Implementation()
{
	AtonLab::FLabState Loaded;
	if (!Saved.IsEmpty() && Loaded.Restore(TCHAR_TO_UTF8(*Saved)))
	{
		State() = Loaded;
	}
	else
	{
		State() = AtonLab::FLabState();
	}
}

void ULyudAtonLab::SpawnTerminals(UWorld& World)
{
	FString Text;
	TSharedPtr<FJsonObject> Doc;
	const FString Path = FPaths::ProjectContentDir() / LyudAtonLabDetail::TerminalsFile;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATONLAB no terminals: %s"), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Doc->TryGetArrayField(TEXT("terminals"), List))
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Item : *List)
	{
		const TSharedPtr<FJsonObject>* T = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
		if (!Item->TryGetObject(T) || !(*T)->TryGetArrayField(TEXT("at"), At) || At->Num() < 3)
		{
			continue;
		}
		// Unreal metres in the file (X as the map, Y = portal face + map Y, Z up), like fountain.json
		FVector Where((*At)[0]->AsNumber() * 100.0, (*At)[1]->AsNumber() * 100.0, (*At)[2]->AsNumber() * 100.0);
		const float Yaw = float((*T)->GetNumberField(TEXT("yaw")));
		// onto the floor that is really there: the room's floor within 60 cm of where the file says it is
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AtonLabFloor), true);
		if (World.LineTraceSingleByChannel(Hit, Where + FVector(0, 0, 60.f), Where - FVector(0, 0, 60.f), ECC_WorldStatic, Params))
		{
			Where.Z = Hit.ImpactPoint.Z;
		}
		FActorSpawnParameters SP;
		SP.ObjectFlags |= RF_Transient;
		if (ALyudLabTerminal* Terminal = World.SpawnActor<ALyudLabTerminal>(Where, FRotator(0.f, Yaw, 0.f), SP))
		{
			Terminal->TerminalId = FName(*(*T)->GetStringField(TEXT("id")));
			Terminals.Add(Terminal);
			UE_LOG(LogTemp, Display, TEXT("ATONLAB terminal %s at %s yaw %.1f"), *Terminal->TerminalId.ToString(), *Where.ToCompactString(), Yaw);
		}
	}
}

void ULyudAtonLab::SetPrompt(bool bShow)
{
	if (bShow == bPrompt)
	{
		return;
	}
	bPrompt = bShow;
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
	{
		UI->SetPrompt(this, bShow ? LyudText::Get(TEXT("ui.aton.lab.prompt")) : FText::GetEmpty());
	}
}

void ULyudAtonLab::Tick(float DeltaTime)
{
	// the telescope's slots pass whether or not anyone looks at the queue
	ScopeTimer -= DeltaTime;
	if (ScopeTimer <= 0.f)
	{
		ScopeTimer = 5.f;
		ResolveScope();
	}

	APlayerController* PC = nullptr;
	const AHeroCharacter* Hero = LyudAtonLabDetail::PlayerHero(GetWorld(), &PC);
	const ULyudInventoryUI* UI = ULyudInventoryUI::Get(this);
	ALyudLabTerminal* Now = nullptr;
	if (Hero && PC && !(UI && UI->IsOpen()))
	{
		const FVector Eye = Hero->EyeLocation();
		const FVector Look = PC->GetControlRotation().Vector();
		for (ALyudLabTerminal* T : Terminals)
		{
			if (!T || T->IsHidden() || T->IsActive())
			{
				continue;
			}
			// in front of the desk, near it, and looking at the screen
			const FVector Local = T->GetActorTransform().InverseTransformPosition(Hero->GetActorLocation());
			const FVector ToScreen = T->ScreenPoint() - Eye;
			const double Dist = ToScreen.Size();
			if (Local.X > 5.0 && Local.X < LyudAtonLabDetail::UseReachCm && FMath::Abs(Local.Y) < 110.0 && FMath::Abs(Local.Z) < 150.0
				&& Dist > 1.0 && FVector::DotProduct(Look, ToScreen / Dist) > LyudAtonLabDetail::UseDot)
			{
				Now = T;
				break;
			}
		}
	}
	Aimed = Now;
	SetPrompt(Now != nullptr);
	// polled, never consumed: E belongs to several things at once
	if (Now && PC->WasInputKeyJustPressed(EKeys::E))
	{
		SetPrompt(false);
		Now->Use(PC);
	}
}
