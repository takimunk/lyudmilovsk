#include "Europa/LyudEuropa.h"

#include "Audio/LyudSoundscape.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HeroCharacter.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LyudInventory.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sea/LyudSea.h"
#include "Sea/LyudSwim.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudRareSky.h"
#include "Sky/LyudSky.h"
#include "Sound/SoundBase.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudUIStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/LyudLadder.h"
#include "World/LyudPrefetch.h"
#include "Sky/LyudSunGlare.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEuropa(TEXT("lyud.Europa"), 1, TEXT("The hatch to Europa on the sea bed (0: not spawned; read at play start)."));
	TAutoConsoleVariable<int32> CVarEuropaFast(TEXT("lyud.Europa.Fast"), 0, TEXT("1: the hatch's climbs, lids and water run five times faster (tests)."));

	const TCHAR* PlanetLine = TEXT("line.hero.europa_planet.1");
	const TCHAR* MusicPath = TEXT("/Game/Audio/Music/MU_europa_1.MU_europa_1");
	const TCHAR* Meshes = TEXT("/Game/Europa/Meshes/");
	/** The ground's meshes (unreal/scripts/import_moon.py). */
	const TCHAR* MoonMeshes = TEXT("/Game/Europa/Moon/");

	/** The kit's measures (Content/Europa/kit.json, blender/europa/europa_kit.py), cm in the hatch frame. */
	struct FKitData
	{
		FVector SeaHinge = FVector(-70, 0, 5), InnerHinge = FVector(-250, 0, -325);
		FVector EuropaShaft = FVector(-180, 0, 0), RoomCenter = FVector(-90, 0, 0);
		float EuropaTop = -90.f, ShaftBBottom = -320.f, Floor = -580.f, Ceiling = -320.f, RoomRadius = 200.f;
		float OpenRadius = 60.f, SiteZ = -35.f, LadderOffset = 42.f;
		FVector ConsoleAt = FVector(-90, 194, -580);
		float ConsoleYaw = -90.f;
		FVector Screen = FVector(-90, 160, -480);
		TArray<FVector> Lamps;
		bool bLoaded = false;

		/** A shaft's ladder as europa_kit.py's ladder() makes it: the rung line LadderOffset toward +X of the shaft's
		 *  axis, rungs every 30 cm from 25 cm over its foot while under the rails' top less 5 cm. Foot: the rung line
		 *  at the shaft's floor, kit frame; the climber faces +X. */
		void SetupLadder(ULyudLadderComponent* L, const FVector& Foot, float RailTop) const
		{
			L->SetRelativeLocationAndRotation(Foot, FRotator(0.f, 180.f, 0.f));
			L->RungSpacing = 30.f;
			L->FirstRungZ = 25.f;
			L->RungCount = FMath::Max(1, FMath::CeilToInt((RailTop - 5.f - (Foot.Z + 25.f)) / 30.f));
			L->HalfWidth = 22.5f;
			L->RungRadius = 1.6f;
			L->BodyStandoff = 26.f;
			L->bInteractive = false;
		}
	};

	TSharedPtr<FJsonObject> ReadJson(const FString& Rel)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / Rel)))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
		}
		return Root;
	}

	FVector Vec(const TSharedPtr<FJsonObject>& O, const FString& Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O && O->TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return Default;
	}

	const FKitData& KitData()
	{
		static FKitData K;
		if (K.bLoaded)
		{
			return K;
		}
		K.bLoaded = true;
		const TSharedPtr<FJsonObject> J = ReadJson(TEXT("Europa/kit.json"));
		if (!J)
		{
			UE_LOG(LogTemp, Warning, TEXT("Europa: no Content/Europa/kit.json, using built-in measures"));
			return K;
		}
		K.SeaHinge = Vec(J, TEXT("sea_hinge"), K.SeaHinge);
		K.InnerHinge = Vec(J, TEXT("inner_hinge"), K.InnerHinge);
		K.EuropaShaft = Vec(J, TEXT("europa_shaft"), K.EuropaShaft);
		K.RoomCenter = Vec(J, TEXT("room_center"), K.RoomCenter);
		K.EuropaTop = J->GetNumberField(TEXT("europa_top"));
		K.ShaftBBottom = J->GetNumberField(TEXT("shaft_b_bottom"));
		K.Floor = J->GetNumberField(TEXT("floor"));
		K.Ceiling = J->GetNumberField(TEXT("ceiling"));
		K.RoomRadius = J->GetNumberField(TEXT("room_radius"));
		K.OpenRadius = J->GetNumberField(TEXT("open_radius"));
		K.SiteZ = J->GetNumberField(TEXT("site_z"));
		K.LadderOffset = J->GetNumberField(TEXT("ladder_offset"));
		if (const TSharedPtr<FJsonObject>* C = nullptr; J->TryGetObjectField(TEXT("console"), C))
		{
			K.ConsoleAt = Vec(*C, TEXT("at"), K.ConsoleAt);
			K.ConsoleYaw = (*C)->GetNumberField(TEXT("yaw"));
			// the screen's centre is in the console's own frame (metres x 100, Blender's Y): turn it into the hatch frame
			const FVector S = Vec(*C, TEXT("screen"), FVector(30, 0, 100));
			K.Screen = K.ConsoleAt + FRotator(0.f, K.ConsoleYaw, 0.f).RotateVector(FVector(S.X, -S.Y, S.Z));
		}
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (J->TryGetArrayField(TEXT("lamps"), L))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
				K.Lamps.Add(FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber()));
			}
		}
		return K;
	}

	UStaticMesh* Mesh(const TCHAR* Name, const TCHAR* Folder = Meshes)
	{
		const FString Path = FString(Folder) + Name + TEXT(".") + Name;
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("Europa: missing %s (unreal/scripts/import_europa.sh)"), *Path);
		}
		return M;
	}

	UStaticMeshComponent* MakeMesh(AActor* Owner, const TCHAR* Name, USceneComponent* Parent)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->RegisterComponent();
		return C;
	}

	/** Moves, never sweeps; small lights for the rooms, none of them in the distance field or the GI. */
	void Quiet(UPrimitiveComponent* C)
	{
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
	}

	float Smooth01(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	ACharacter* PlayerHero(const UWorld* World, APlayerController** OutPC = nullptr)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (OutPC)
		{
			*OutPC = PC;
		}
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	void Toast(const UObject* Ctx, const TCHAR* Key)
	{
		if (ULyudGameScreen* S = ULyudGameScreen::Get(Ctx))
		{
			S->Toast(LyudText::Get(Key));
		}
	}

	ULyudEuropaSubsystem* EuropaOf(UWorld* World)
	{
		return World ? World->GetSubsystem<ULyudEuropaSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorld GoHatchCmd(TEXT("lyud.Europa.GoHatch"), TEXT("To the sea hatch (in the water over it)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudEuropaSubsystem* E = EuropaOf(W)) { E->GoHatch(); } }));
	FAutoConsoleCommandWithWorld GoRoomCmd(TEXT("lyud.Europa.GoRoom"), TEXT("Into the airlock, by its computer."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudEuropaSubsystem* E = EuropaOf(W)) { E->GoRoom(); } }));
	FAutoConsoleCommandWithWorld GoEuropaCmd(TEXT("lyud.Europa.GoEuropa"), TEXT("Onto Europa, by its hatch."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudEuropaSubsystem* E = EuropaOf(W)) { E->GoEuropa(); } }));
	FAutoConsoleCommandWithWorld UseCmd(TEXT("lyud.Europa.Use"), TEXT("What E does at the hatch now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudEuropaSubsystem* E = EuropaOf(W)) { E->Use(); } }));
	/** The hour offset (lyud.SkyHourOffset) within the next Days when the town faces Europa most squarely, by day (Day 1)
	 *  or by night (Day 0): the ephemeris worked forward, nothing moved (tests and screenshots). */
	FAutoConsoleCommandWithWorldAndArgs FindIslandCmd(TEXT("lyud.Europa.FindIsland"), TEXT("lyud.Europa.FindIsland [days] [day 1|night 0]: logs EUROPA island hour <offset>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
		{
			ULyudEuropaSubsystem* E = EuropaOf(W);
			TActorIterator<ALyudSky> Sky(W);
			if (!E || !E->GetSite() || !Sky)
			{
				return;
			}
			const int32 Days = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 30;
			const bool bDay = Args.Num() > 1 ? FCString::Atoi(*Args[1]) != 0 : true;
			const FDateTime Base = Sky->ShownUtc() - FTimespan::FromHours(IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.SkyHourOffset"))->GetFloat());
			float Best = -2.f;
			int32 BestH = 0;
			for (int32 H = 0; H < Days * 24; ++H)
			{
				const ALyudEuropaSite::FSky S = E->GetSite()->SkyAt(Base + FTimespan::FromHours(H));
				const float Lit = bDay ? S.TownSun : -S.TownSun;
				const float Score = S.IslandUp + (Lit > 0.25f ? 0.f : -2.f);
				if (Score > Best)
				{
					Best = Score;
					BestH = H;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("EUROPA island hour %d (%s, score %.2f)"), BestH, bDay ? TEXT("day") : TEXT("night"), Best);
			IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.SkyHourOffset"))->Set(*FString::FromInt(BestH), ECVF_SetByConsole);
		}));
	FAutoConsoleCommandWithWorld ReportCmd(TEXT("lyud.Europa.Report"), TEXT("Logs the hatch's state (EUROPA ...)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W) { if (ULyudEuropaSubsystem* E = EuropaOf(W)) { UE_LOG(LogTemp, Display, TEXT("%s"), *E->Report()); } }));
}

// ------------------------------------------------------------------ the subtitle of the line

class SLyudEuropaSubtitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudEuropaSubtitle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULyudEuropaSubsystem>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace LyudUI;
		TWeakObjectPtr<ULyudEuropaSubsystem> W = InArgs._Owner;
		SetVisibility(EVisibility::HitTestInvisible);
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 132))
			[
				SNew(SBox).WidthOverride(760.f)
				[
					SNew(STextBlock).Font(Font(18)).Justification(ETextJustify::Center).AutoWrapText(true)
					.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.7f))
					.Text_Lambda([]() { return LyudText::Get(PlanetLine); })
					.ColorAndOpacity_Lambda([W]() { return FSlateColor(Bright.CopyWithNewOpacity(W.IsValid() ? W->SubtitleAlpha : 0.f)); })
				]
			]
		];
	}
};

// ------------------------------------------------------------------ lids

bool FLyudHatchLid::Update(float DeltaTime, float Seconds)
{
	const float Target = bOpen ? 1.f : 0.f;
	if (Openness == Target || !Pivot)
	{
		return false;
	}
	Openness = FMath::FInterpConstantTo(Openness, Target, DeltaTime, 1.f / FMath::Max(Seconds, 0.05f));
	Pivot->SetRelativeRotation(FRotator(OpenPitch * Smooth01(Openness), 0.f, 0.f));
	return true;
}

void FLyudHatchLid::Snap(bool bInOpen)
{
	bOpen = bInOpen;
	Openness = bInOpen ? 1.f : 0.f;
	if (Pivot)
	{
		Pivot->SetRelativeRotation(FRotator(OpenPitch * Openness, 0.f, 0.f));
	}
}

// ------------------------------------------------------------------ the airlock

ALyudAirlock::ALyudAirlock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Underwater = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Underwater"));
	Underwater->SetupAttachment(RootComponent);
	Underwater->bUnbound = true;
	SeaLadder = CreateDefaultSubobject<ULyudLadderComponent>(TEXT("SeaLadder"));
	SeaLadder->SetupAttachment(RootComponent);
	EuropaLadder = CreateDefaultSubobject<ULyudLadderComponent>(TEXT("EuropaLadder"));
	EuropaLadder->SetupAttachment(RootComponent);
	Underwater->Priority = 1100.f;
	Underwater->BlendWeight = 0.f;
	FPostProcessSettings& S = Underwater->Settings;
	S.bOverride_SceneColorTint = true;
	S.SceneColorTint = FLinearColor(0.42f, 0.78f, 0.74f);
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.85f;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 1.2f;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.75f, 0.85f, 0.85f, 1.f);
}

void ALyudAirlock::Build()
{
	const FKitData& K = KitData();
	USceneComponent* Root = GetRootComponent();
	Collar = MakeMesh(this, TEXT("Collar"), Root);
	Collar->SetStaticMesh(Mesh(TEXT("SM_EU_Collar")));
	// outdoors on the bed: the sun's channel too
	Collar->SetLightingChannels(true, true, false);
	Room = MakeMesh(this, TEXT("Room"), Root);
	Room->SetStaticMesh(Mesh(TEXT("SM_EU_Airlock")));
	Quiet(Room);

	auto MakeLid = [this, Root](FLyudHatchLid& Lid, const TCHAR* Name, const FVector& Hinge, bool bFlip)
	{
		Lid.Pivot = NewObject<USceneComponent>(this, *(FString(Name) + TEXT("Pivot")));
		Lid.Pivot->SetupAttachment(Root);
		Lid.Pivot->SetRelativeLocation(Hinge);
		Lid.Pivot->RegisterComponent();
		Lid.Mesh = MakeMesh(this, Name, Lid.Pivot);
		Lid.Mesh->SetStaticMesh(Mesh(TEXT("SM_EU_Lid")));
		if (bFlip)
		{
			// the inner lid hangs under the ceiling, top down, and swings down to open
			Lid.Mesh->SetRelativeRotation(FRotator(0.f, 0.f, 180.f));
			Lid.OpenPitch = -92.f;
		}
		Quiet(Lid.Mesh);
	};
	K.SetupLadder(SeaLadder, FVector(K.LadderOffset, 0.f, K.Floor), -12.f);
	K.SetupLadder(EuropaLadder, FVector(K.EuropaShaft.X + K.LadderOffset, 0.f, K.Floor), K.EuropaTop - 10.f);
	MakeLid(SeaLid, TEXT("SeaLid"), K.SeaHinge, false);
	SeaLid.Mesh->SetLightingChannels(true, true, false);
	MakeLid(InnerLid, TEXT("InnerLid"), K.InnerHinge, true);

	Console = MakeMesh(this, TEXT("Console"), Root);
	Console->SetStaticMesh(Mesh(TEXT("SM_EU_Console")));
	Console->SetRelativeLocationAndRotation(K.ConsoleAt, FRotator(0.f, K.ConsoleYaw, 0.f));
	Quiet(Console);
	for (int32 i = 0; i < Console->GetNumMaterials(); ++i)
	{
		if (Console->GetMaterialSlotNames().IsValidIndex(i) && Console->GetMaterialSlotNames()[i].ToString().Contains(TEXT("screen")))
		{
			ScreenMID = Console->CreateDynamicMaterialInstance(i);
		}
	}

	Water = MakeMesh(this, TEXT("Water"), Root);
	Water->SetStaticMesh(Mesh(TEXT("SM_EU_Water")));
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCastShadow(false);
	Quiet(Water);

	// the room's lamps and a dim one in each shaft (channel 0: they light the room, not the sea)
	TArray<FVector> At = K.Lamps;
	At.Add(FVector(0, 0, -180));
	At.Add(K.EuropaShaft + FVector(0, 0, -200));
	for (int32 i = 0; i < At.Num(); ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("Lamp%d"), i));
		L->SetupAttachment(Root);
		L->SetRelativeLocation(At[i]);
		const bool bShaft = i >= K.Lamps.Num();
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(bShaft ? 12.f : 160.f);
		L->SetAttenuationRadius(bShaft ? 260.f : 700.f);
		L->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
		L->SetSourceRadius(6.f);
		L->SetCastShadows(!bShaft);
		L->RegisterComponent();
		Lights.Add(L);
	}
}

void ALyudAirlock::BeginPlay()
{
	Super::BeginPlay();
	Build();
	SeaLid.Snap(false);
	InnerLid.Snap(false);
	ShownLevel = -1.f;
	UpdateLook();
	UpdateDryZone();
}

void ALyudAirlock::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Sea->SetDryZone(this, FBox(ForceInit));
	}
	Super::EndPlay(Reason);
}

void ALyudAirlock::OnGameLoaded_Implementation()
{
	// a load lands between cycles: a running one finishes at once
	if (State == ELyudAirlockState::Draining)
	{
		State = ELyudAirlockState::Dry;
	}
	else if (State == ELyudAirlockState::Filling)
	{
		State = ELyudAirlockState::Flooded;
	}
	Water01 = State == ELyudAirlockState::Dry ? 0.f : 1.f;
	SeaLid.Snap(false);
	InnerLid.Snap(false);
	if (CycleLoop)
	{
		CycleLoop->Stop();
		CycleLoop = nullptr;
	}
	ShownLevel = -1.f;
	UpdateLook();
	UpdateDryZone();
}

float ALyudAirlock::GetWaterZ() const
{
	const FKitData& K = KitData();
	return float(GetActorLocation().Z) + FMath::Lerp(K.Floor, K.Ceiling, Water01);
}

bool ALyudAirlock::IsInside(const FVector& P) const
{
	const FKitData& K = KitData();
	const FVector L = GetActorTransform().InverseTransformPosition(P);
	if (L.Z < K.Floor - 20.f || L.Z > -40.f)
	{
		return false;
	}
	const FVector2D L2(L);
	if (L.Z <= K.Ceiling + 5.f && FVector2D::Distance(L2, FVector2D(K.RoomCenter)) < K.RoomRadius + 10.f)
	{
		return true;
	}
	return FVector2D::Distance(L2, FVector2D::ZeroVector) < K.OpenRadius + 10.f
		|| (FVector2D::Distance(L2, FVector2D(K.EuropaShaft)) < K.OpenRadius + 10.f && L.Z < K.EuropaTop);
}

FVector ALyudAirlock::ScreenLocation() const
{
	return Kit(KitData().Screen);
}

void ALyudAirlock::SetSeaLidOpen(bool bOpen)
{
	if (SeaLid.bOpen == bOpen)
	{
		return;
	}
	SeaLid.bOpen = bOpen;
	Sound(bOpen ? TEXT("hatch_open") : TEXT("hatch_close"), Kit(FVector(0, 0, 10)));
}

void ALyudAirlock::SetInnerLidOpen(bool bOpen, bool bSnap)
{
	if (bSnap)
	{
		InnerLid.Snap(bOpen);
		return;
	}
	if (InnerLid.bOpen == bOpen)
	{
		return;
	}
	InnerLid.bOpen = bOpen;
	Sound(bOpen ? TEXT("hatch_open") : TEXT("hatch_close"), Kit(KitData().InnerHinge));
}

bool ALyudAirlock::PressButton(FString& OutRefusal)
{
	Sound(TEXT("airlock_beep"), ScreenLocation(), 0.8f);
	const ALyudEuropaSite* SiteNow = ULyudEuropaSubsystem::Get(this) ? ULyudEuropaSubsystem::Get(this)->GetSite() : nullptr;
	switch (State)
	{
	case ELyudAirlockState::Flooded:
		if (!SeaLid.IsShut())
		{
			OutRefusal = TEXT("ui.europa.shut_sea");
			return false;
		}
		State = ELyudAirlockState::Draining;
		break;
	case ELyudAirlockState::Dry:
		if (!InnerLid.IsShut() || (SiteNow && !SiteNow->Lid.IsShut()))
		{
			OutRefusal = TEXT("ui.europa.shut_inner");
			return false;
		}
		State = ELyudAirlockState::Filling;
		break;
	default:
		OutRefusal = TEXT("ui.europa.busy");
		return false;
	}
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		CycleLoop = S->AttachLoop(State == ELyudAirlockState::Draining ? TEXT("airlock_drain") : TEXT("airlock_fill"), Room, 1.f);
		if (CycleLoop)
		{
			CycleLoop->SetWorldLocation(Kit(KitData().RoomCenter + FVector(0, 0, KitData().Floor + 40.f)));
			CycleLoop->FadeIn(0.6f, 1.f);
		}
	}
	UpdateDryZone();
	UE_LOG(LogTemp, Display, TEXT("EUROPA airlock %s"), State == ELyudAirlockState::Draining ? TEXT("draining") : TEXT("filling"));
	return true;
}

void ALyudAirlock::UpdateDryZone()
{
	ALyudSea* Sea = ALyudSea::Get(this);
	if (!Sea)
	{
		return;
	}
	// full, the room is the sea's (swimming, its underwater look); otherwise the room and shafts are dry ground
	if (State == ELyudAirlockState::Flooded)
	{
		Sea->SetDryZone(this, FBox(ForceInit));
		return;
	}
	const FKitData& K = KitData();
	const FVector C = K.RoomCenter;
	const FBox Local(FVector(FMath::Min(C.X, K.EuropaShaft.X) - K.RoomRadius - 40.f, C.Y - K.RoomRadius - 40.f, K.Floor - 60.f),
		FVector(FMath::Max(C.X + K.RoomRadius, K.OpenRadius) + 40.f, C.Y + K.RoomRadius + 40.f, -45.f));
	Sea->SetDryZone(this, Local.TransformBy(GetActorTransform()));
}

void ALyudAirlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Fast = CVarEuropaFast.GetValueOnGameThread() > 0 ? 5.f : 1.f;
	const float Dt = DeltaSeconds * Fast;
	SeaLid.Update(Dt, LidSeconds);
	InnerLid.Update(Dt, LidSeconds);
	if (State == ELyudAirlockState::Draining || State == ELyudAirlockState::Filling)
	{
		const bool bDrain = State == ELyudAirlockState::Draining;
		Water01 = FMath::Clamp(Water01 + (bDrain ? -1.f : 1.f) * Dt / FMath::Max(CycleSeconds, 0.1f), 0.f, 1.f);
		if (Water01 <= 0.f || Water01 >= 1.f)
		{
			State = bDrain ? ELyudAirlockState::Dry : ELyudAirlockState::Flooded;
			if (CycleLoop)
			{
				CycleLoop->FadeOut(0.8f, 0.f);
				CycleLoop = nullptr;
			}
			Sound(TEXT("airlock_beep"), ScreenLocation(), 1.f);
			UpdateDryZone();
			UE_LOG(LogTemp, Display, TEXT("EUROPA airlock %s"), bDrain ? TEXT("dry") : TEXT("flooded"));
		}
	}
	UpdateLook();
}

void ALyudAirlock::UpdateLook()
{
	const FKitData& K = KitData();
	// the water's surface: a disc in the room while there is water under the ceiling
	const bool bShowWater = Water01 > 0.002f && Water01 < 0.999f;
	if (Water && Water->IsVisible() != bShowWater)
	{
		Water->SetVisibility(bShowWater);
	}
	if (Water && bShowWater && !FMath::IsNearlyEqual(ShownLevel, Water01, 1e-4f))
	{
		Water->SetRelativeLocation(FVector(K.RoomCenter.X, K.RoomCenter.Y, FMath::Lerp(K.Floor, K.Ceiling, Water01)));
	}
	// the screen: the level, and what it is doing (0 full, 1 draining, 2 dry, 3 filling)
	const int32 Mode = int32(State);
	if (ScreenMID && (Mode != ShownMode || !FMath::IsNearlyEqual(ShownLevel, Water01, 1e-3f)))
	{
		ScreenMID->SetScalarParameterValue(TEXT("Level"), Water01);
		ScreenMID->SetScalarParameterValue(TEXT("Mode"), float(Mode));
	}
	ShownLevel = Water01;
	ShownMode = Mode;
	// under the water's surface while it goes down or comes up (full, the sea's own look takes over)
	float Weight = 0.f;
	if (State != ELyudAirlockState::Flooded && Water01 > 0.f)
	{
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
		{
			const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
			if (IsInside(Cam) && Cam.Z < GetWaterZ())
			{
				Weight = 1.f;
			}
		}
	}
	if (Underwater->BlendWeight != Weight)
	{
		Underwater->BlendWeight = Weight;
	}
}

void ALyudAirlock::Sound(FName Pool, const FVector& Where, float Volume) const
{
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->PlayEffect(Pool, Where, Volume);
	}
}

// ------------------------------------------------------------------ Europa

namespace
{
	/** The town on the planet (ALyudSky's place), degrees. */
	constexpr double TownLat = 54.96;
	constexpr double TownLon = 20.40;
	const TCHAR* SeaMaterial = TEXT("/Game/Sea/M_LyudSea.M_LyudSea");

	/** Europa's water: a few slow ripples under 0.13 g (deep-water dispersion with Europa's gravity), the sea's own
	 *  material reading them from a table like ALyudSea's (dir x, dir y, k, amplitude m; omega, phase, wavelength). */
	UTexture2D* MakeLeadWaves()
	{
		constexpr int32 W = 128;
		UTexture2D* T = UTexture2D::CreateTransient(W, 2, PF_A32B32G32R32F, TEXT("T_EU_LeadWaves"));
		T->SRGB = false;
		T->Filter = TF_Nearest;
		T->NeverStream = true;
		T->AddressX = TA_Clamp;
		T->AddressY = TA_Clamp;
		T->UpdateResource();
		FLinearColor* Data = new FLinearColor[W * 2];
		FMemory::Memzero(Data, sizeof(FLinearColor) * W * 2);
		const float G = 9.81f * 0.13f;
		const float Lambda[] = { 4.1f, 2.6f, 1.7f, 1.15f, 0.8f, 0.55f };
		const float Amp[] = { 0.006f, 0.004f, 0.003f, 0.002f, 0.0014f, 0.001f };
		const float Dir[] = { 0.3f, 1.4f, 2.6f, 0.9f, 3.7f, 5.1f };
		for (int32 I = 0; I < 6; ++I)
		{
			const float K = 2.f * PI / Lambda[I];
			Data[I] = FLinearColor(FMath::Cos(Dir[I]), FMath::Sin(Dir[I]), K, Amp[I]);
			Data[W + I] = FLinearColor(FMath::Sqrt(G * K), 1.7f * I, Lambda[I], 0.f);
		}
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, 2);
		T->UpdateTextureRegions(0, 1, Region, W * sizeof(FLinearColor), sizeof(FLinearColor), (uint8*)Data,
			[](uint8* Src, const FUpdateTextureRegion2D* Regions) { delete[] (FLinearColor*)Src; delete Regions; });
		return T;
	}

	/** The sea's water material on a mesh, for still water away from the sea (no depth map: everywhere OpenDepth
	 *  deep), with these ripples. */
	UMaterialInstanceDynamic* StillWater(UObject* Outer, UTexture2D* Waves, float DepthM)
	{
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, SeaMaterial);
		if (!M)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Outer);
		MID->SetTextureParameterValue(TEXT("WaveTable"), Waves);
		MID->SetVectorParameterValue(TEXT("SeaParams"), FLinearColor(0.2f, 0.02f, 6.f, 0.f));
		MID->SetVectorParameterValue(TEXT("DepthRect"), FLinearColor(0.f, 0.f, 0.f, 0.f));
		MID->SetVectorParameterValue(TEXT("SeaClock"), FLinearColor(0.f, 128.f, 0.25f, DepthM));
		MID->SetScalarParameterValue(TEXT("Tide"), 0.f);
		MID->SetScalarParameterValue(TEXT("LandSideX"), -1.0e7f);
		MID->SetVectorParameterValue(TEXT("Glitter"), FLinearColor(0.035f, 0.015f, 0.f, 0.f));
		return MID;
	}

	/** An equatorial direction (right-handed: x the equinox, z the pole) in the site's world: east is -X, north +Y. */
	struct FSiteFrame
	{
		FVector E, N, U;
		FVector World(const FVector& D) const
		{
			return FVector(-FVector::DotProduct(D, E), FVector::DotProduct(D, N), FVector::DotProduct(D, U));
		}
		/** A rotation taking a mesh made in a right-handed frame (Blender's: its Y mirrored by the import) with axes
		 *  Ax, Ay, Az (equatorial) onto the world. */
		FQuat Turn(const FVector& Ax, const FVector& Ay, const FVector& Az) const
		{
			return FMatrix(World(Ax).GetSafeNormal(), -World(Ay).GetSafeNormal(), World(Az).GetSafeNormal(), FVector::ZeroVector).ToQuat().GetNormalized();
		}
	};
}

ALyudEuropaSite::ALyudEuropaSite()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PlanetShine = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PlanetShine"));
	PlanetShine->SetupAttachment(RootComponent);
	PlanetShine->SetMobility(EComponentMobility::Movable);
	PlanetShine->LightingChannels.bChannel0 = false;
	PlanetShine->LightingChannels.bChannel2 = true;
	PlanetShine->SetIntensity(0.6f);
	PlanetShine->SetLightColor(FLinearColor(0.55f, 0.72f, 1.f));
	PlanetShine->SetCastShadows(false);
	PlanetShine->bAffectsWorld = true;
	PlanetShine->SetAtmosphereSunLight(false);
	ShaftLadder = CreateDefaultSubobject<ULyudLadderComponent>(TEXT("ShaftLadder"));
	ShaftLadder->SetupAttachment(RootComponent);
	ShaftLadder->bInteractive = false;
}

ALyudEuropaSite::FSky ALyudEuropaSite::SkyAt(const FDateTime& Utc) const
{
	FSky Out;
	// Europa where the ephemeris has the moon: the planet from there is straight back down that line
	const FVector M = LyudEphemeris::MoonEquatorial(Utc);
	const FVector M2 = LyudEphemeris::MoonEquatorial(Utc + FTimespan::FromHours(1.0));
	const FVector S = LyudEphemeris::SunEquatorial(Utc);
	// its body frame, tidally locked: x toward the planet, z along its orbit's pole
	const FVector Xb = -M;
	FVector Zb = FVector::CrossProduct(M, M2).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZAxisVector);
	Zb = (Zb - Xb * FVector::DotProduct(Zb, Xb)).GetSafeNormal();
	const FVector Yb = FVector::CrossProduct(Zb, Xb);
	// the hatch's place on it: up, east, north (right-handed)
	const double La = FMath::DegreesToRadians(SiteLatDeg), Lo = FMath::DegreesToRadians(SiteLonDeg);
	FSiteFrame F;
	F.U = (Xb * (FMath::Cos(La) * FMath::Cos(Lo)) + Yb * (FMath::Cos(La) * FMath::Sin(Lo)) + Zb * FMath::Sin(La)).GetSafeNormal();
	F.E = (Xb * -FMath::Sin(Lo) + Yb * FMath::Cos(Lo)).GetSafeNormal();
	F.N = FVector::CrossProduct(F.U, F.E);
	Out.Sun = F.World(S).GetSafeNormal();
	Out.Planet = F.World(Xb).GetSafeNormal();
	// the planet turned with the day: its mesh's longitudes are the Earth's (Greenwich at the texture's middle)
	const double Th = FMath::DegreesToRadians(LyudEphemeris::SiderealDeg(Utc, 0.0));
	const FVector Ex(FMath::Cos(Th), FMath::Sin(Th), 0.0), Ey(-FMath::Sin(Th), FMath::Cos(Th), 0.0);
	Out.PlanetTurn = F.Turn(Ex, Ey, FVector::ZAxisVector);
	Out.StarTurn = F.Turn(FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector);
	// the town faces Europa when Europa stands over its horizon
	const double TLa = FMath::DegreesToRadians(TownLat), TLo = Th + FMath::DegreesToRadians(TownLon);
	const FVector Town(FMath::Cos(TLa) * FMath::Cos(TLo), FMath::Cos(TLa) * FMath::Sin(TLo), FMath::Sin(TLa));
	Out.IslandUp = float(FVector::DotProduct(Town, M));
	Out.TownSun = float(FVector::DotProduct(Town, S));
	Out.bIslandFacing = Out.IslandUp > 0.05f;
	return Out;
}

void ALyudEuropaSite::BeginPlay()
{
	Super::BeginPlay();
	const FKitData& K = KitData();
	USceneComponent* Root = GetRootComponent();
	// the ground (~130 MB of Nanite meshes) is made when it is first needed (EnsureGround): the subsystem loads it in
	// the background while the player is in the airlock
	Collar = MakeMesh(this, TEXT("Collar"), Root);
	Collar->SetStaticMesh(Mesh(TEXT("SM_EU_Collar")));
	// the sea's collar, its concrete and crust iced over
	if (UMaterialInterface* Crust = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/MI_EU_ice_crust.MI_EU_ice_crust")))
	{
		const TArray<FName> Slots = Collar->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (Slots[i].ToString().StartsWith(TEXT("tx_algae")) || Slots[i].ToString().StartsWith(TEXT("tx_concrete")))
			{
				Collar->SetMaterial(i, Crust);
			}
		}
	}
	Collar->SetLightingChannels(false, true, true);
	Shaft = MakeMesh(this, TEXT("Shaft"), Root);
	Shaft->SetStaticMesh(Mesh(TEXT("SM_EU_ShaftB")));
	Quiet(Shaft);
	// the shaft's ladder (the same as the kit's: rungs every 30 cm from 25 cm over its floor, on its +X side)
	K.SetupLadder(ShaftLadder, FVector(K.LadderOffset, 0.f, K.ShaftBBottom), -12.f);
	ShaftLadder->GripVolume = 0.3f;
	Lid.Pivot = NewObject<USceneComponent>(this, TEXT("LidPivot"));
	Lid.Pivot->SetupAttachment(Root);
	Lid.Pivot->SetRelativeLocation(K.SeaHinge);
	Lid.Pivot->RegisterComponent();
	Lid.Mesh = MakeMesh(this, TEXT("Lid"), Lid.Pivot);
	Lid.Mesh->SetStaticMesh(Mesh(TEXT("SM_EU_Lid")));
	Lid.Mesh->SetLightingChannels(false, true, true);
	Lid.Snap(false);
	// the same dim lamp as in the room's Europa shaft, at the same place under the top (the two shafts look alike)
	ShaftLight = NewObject<UPointLightComponent>(this, TEXT("ShaftLight"));
	ShaftLight->SetupAttachment(Root);
	ShaftLight->SetRelativeLocation(FVector(0, 0, -200.f - K.EuropaTop));
	ShaftLight->SetIntensityUnits(ELightUnits::Candelas);
	ShaftLight->SetIntensity(12.f);
	ShaftLight->SetAttenuationRadius(260.f);
	ShaftLight->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
	ShaftLight->SetCastShadows(false);
	ShaftLight->RegisterComponent();

	// the planet, its air, and the stars round the whole site
	Planet = MakeMesh(this, TEXT("Planet"), Root);
	Planet->SetStaticMesh(Mesh(TEXT("SM_EU_Planet")));
	Planet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Planet->SetCastShadow(false);
	Quiet(Planet);
	Planet->bNeverDistanceCull = true;
	PlanetMID = Planet->CreateDynamicMaterialInstance(0);
	PlanetAir = MakeMesh(this, TEXT("PlanetAir"), Root);
	PlanetAir->SetStaticMesh(Mesh(TEXT("SM_EU_Planet")));
	PlanetAir->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlanetAir->SetCastShadow(false);
	Quiet(PlanetAir);
	PlanetAir->bNeverDistanceCull = true;
	if (UMaterialInterface* Air = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/M_EU_PlanetAir.M_EU_PlanetAir")))
	{
		AirMID = UMaterialInstanceDynamic::Create(Air, this);
		PlanetAir->SetMaterial(0, AirMID);
	}
	else
	{
		PlanetAir->SetVisibility(false);
	}
	Stars = MakeMesh(this, TEXT("Stars"), Root);
	Stars->SetStaticMesh(Mesh(TEXT("SM_EU_Planet")));
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Quiet(Stars);
	Stars->bNeverDistanceCull = true;
	// 100 km round the hatch: past the planet (60 km), inside the coast's sky dome; the mesh is a 1 m sphere
	Stars->SetRelativeScale3D(FVector(1.0e5f));
	if (UMaterialInterface* Space = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/M_EU_Space.M_EU_Space")))
	{
		StarMID = UMaterialInstanceDynamic::Create(Space, this);
		Stars->SetMaterial(0, StarMID);
	}
	else
	{
		Stars->SetVisibility(false);
	}
	Place(PlanetDir, PlanetDistance, PlanetDiameterDeg);
	SetShown(false);
}

void ALyudEuropaSite::EnsureGround()
{
	if (!bGroundBuilt)
	{
		bGroundBuilt = true;
		BuildGround();
		SetActorHiddenInGame(!bShown);
	}
}

void ALyudEuropaSite::BuildGround()
{
	USceneComponent* Root = GetRootComponent();
	const TSharedPtr<FJsonObject> J = ReadJson(TEXT("Europa/moon.json"));
	auto Outdoor = [](UStaticMeshComponent* C)
	{
		// the star's light (channel 1) and the planet's (channel 2)
		C->SetLightingChannels(false, true, true);
	};
	if (!J)
	{
		// the old ice disc and blocks
		UE_LOG(LogTemp, Warning, TEXT("Europa: no Content/Europa/moon.json (tools/europa/moon_terrain.py), the old ice disc"));
		Ground = MakeMesh(this, TEXT("Ground"), Root);
		Ground->SetStaticMesh(Mesh(TEXT("SM_EU_Ground")));
		Outdoor(Ground);
		Quiet(Ground);
		Blocks = MakeMesh(this, TEXT("Blocks"), Root);
		Blocks->SetStaticMesh(Mesh(TEXT("SM_EU_IceBlocks")));
		Outdoor(Blocks);
		Quiet(Blocks);
		return;
	}
	if (const TSharedPtr<FJsonObject>* Site = nullptr; J->TryGetObjectField(TEXT("site"), Site))
	{
		SiteLatDeg = (*Site)->GetNumberField(TEXT("lat_deg"));
		SiteLonDeg = (*Site)->GetNumberField(TEXT("lon_deg"));
	}
	TArray<FString> Names;
	const TArray<TSharedPtr<FJsonValue>>* Tiles = nullptr;
	if (J->TryGetArrayField(TEXT("tiles"), Tiles))
	{
		for (const TSharedPtr<FJsonValue>& V : *Tiles)
		{
			Names.Add(V->AsObject()->GetStringField(TEXT("name")));
		}
	}
	Names.Add(J->GetStringField(TEXT("horizon")));
	Names.Add(J->GetStringField(TEXT("boulders")));
	for (const FString& N : Names)
	{
		UStaticMesh* SM = Mesh(*N, MoonMeshes);
		if (!SM)
		{
			continue;
		}
		UStaticMeshComponent* C = MakeMesh(this, *N, Root);
		C->SetStaticMesh(SM);
		// the ground stands still: the sun's shadows and Lumen's distance fields keep it (only far skyline casts none)
		C->SetMobility(EComponentMobility::Movable);
		Outdoor(C);
		if (N.Contains(TEXT("Horizon")))
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->bNeverDistanceCull = true;
		}
		Terrain.Add(C);
	}
	const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
	if (J->TryGetArrayField(TEXT("water"), W))
	{
		for (const TSharedPtr<FJsonValue>& V : *W)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FLead L;
			const FVector C = Vec(O, TEXT("centre_cm"), FVector::ZeroVector);
			const TArray<TSharedPtr<FJsonValue>>& St = O->GetArrayField(TEXT("strike"));
			L.Centre = FVector2D(C.X, C.Y);
			L.Strike = FVector2D(St[0]->AsNumber(), St[1]->AsNumber());
			L.Length = O->GetNumberField(TEXT("length_cm"));
			L.HalfWidth = O->GetNumberField(TEXT("half_width_cm"));
			L.Z = O->GetNumberField(TEXT("z_cm"));
			Leads.Add(L);
			if (UStaticMesh* SM = Mesh(*O->GetStringField(TEXT("name")), MoonMeshes))
			{
				UStaticMeshComponent* M = MakeMesh(this, *O->GetStringField(TEXT("name")), Root);
				M->SetStaticMesh(SM);
				M->SetCastShadow(false);
				Quiet(M);
				// the water's skin holds you up (no swimming here): you stand at its edge, not in it
				M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				M->SetCollisionResponseToAllChannels(ECR_Block);
				Waters.Add(M);
			}
		}
	}
	BuildWater();
	UE_LOG(LogTemp, Display, TEXT("EUROPA ground: %d meshes, %d leads (seed %d, heights %s)"), Terrain.Num(), Waters.Num(),
		int32(J->GetNumberField(TEXT("seed"))), *J->GetStringField(TEXT("heights_sha1")));
}

void ALyudEuropaSite::BuildWater()
{
	if (Waters.Num() == 0)
	{
		return;
	}
	if (!WaveTable)
	{
		WaveTable = MakeLeadWaves();
	}
	// the leads' own water (M_EU_LeadWater: the sea's absorption, Fresnel reflections of this sky, ripples, the shore's
	// ice rim); the sea's single-layer material drew grey foam over the whole lead away from its depth map
	if (UMaterialInterface* Lead = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Europa/Materials/M_EU_LeadWater.M_EU_LeadWater")))
	{
		SheenMID = UMaterialInstanceDynamic::Create(Lead, this);
	}
	else
	{
		WaterMID = StillWater(this, WaveTable, 12.f);
	}
	for (UStaticMeshComponent* W : Waters)
	{
		if (SheenMID || WaterMID)
		{
			W->SetMaterial(0, SheenMID ? SheenMID.Get() : WaterMID.Get());
		}
	}
}

bool ALyudEuropaSite::WaterAt(const FVector& World, float& OutZ) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(World);
	for (const FLead& Lead : Leads)
	{
		const FVector2D D = FVector2D(L.X, L.Y) - Lead.Centre;
		const float Along = float(FVector2D::DotProduct(D, Lead.Strike));
		const float Across = float(FVector2D::CrossProduct(Lead.Strike, D));
		if (FMath::Abs(Along) < Lead.Length * 0.5f && FMath::Abs(Across) < Lead.HalfWidth)
		{
			OutZ = float(GetActorLocation().Z) + Lead.Z;
			return true;
		}
	}
	return false;
}

void ALyudEuropaSite::Place(const FVector& InPlanetDir, float DistanceCm, float DiameterDeg)
{
	PlanetDir = InPlanetDir.GetSafeNormal();
	PlanetDistance = DistanceCm;
	PlanetDiameterDeg = DiameterDeg;
	if (!Planet)
	{
		return;
	}
	// the mesh is a sphere of 1 m (100 cm) radius
	const float R = DistanceCm * FMath::Tan(FMath::DegreesToRadians(DiameterDeg * 0.5f));
	const FVector At = GetActorLocation() + PlanetDir * DistanceCm;
	Planet->SetWorldLocationAndRotation(At, LastSky.PlanetTurn);
	Planet->SetWorldScale3D(FVector(R / 100.f));
	if (PlanetAir)
	{
		PlanetAir->SetWorldLocationAndRotation(At, LastSky.PlanetTurn);
		PlanetAir->SetWorldScale3D(FVector(R * AirRadius / 100.f));
	}
	if (Stars)
	{
		Stars->SetWorldRotation(LastSky.StarTurn);
	}
	PlanetShine->SetWorldRotation((-PlanetDir).Rotation());
}

void ALyudEuropaSite::SetShown(bool bInShown)
{
	if (bShown == bInShown)
	{
		return;
	}
	bShown = bInShown;
	if (bShown)
	{
		EnsureGround();
	}
	Lid.Snap(Lid.bOpen);
	SunTimer = 0.f;
	SetActorHiddenInGame(!bShown);
	PlanetShine->SetVisibility(bShown);
	SetActorTickEnabled(bShown);
	if (bShown)
	{
		UpdateLook();
	}
}

void ALyudEuropaSite::UpdateLook()
{
	TActorIterator<ALyudSky> It(GetWorld());
	if (!It)
	{
		return;
	}
	LastSky = SkyAt(It->ShownUtc());
	const FVector S = LastSky.Sun;
	// the planet where Europa's orbit puts it, turned with the day; the stars with Europa's month
	Place(LastSky.Planet, PlanetDistance, PlanetDiameterDeg);
	const FLinearColor Light = It->SunColor * (It->SunIlluminance / PI);
	// the eye opens up in Europa's night (ALyudSky's night exposure follows its sun): the planet and its air are drawn
	// that much dimmer, so a gibbous planet over a dark plain is bright, not a white blot; the stars a little
	const float SunAlt = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(S.Z), -1.f, 1.f)));
	const float NightEV = It->NightExposureEV * (1.f - FMath::SmoothStep(-8.f, 8.f, SunAlt));
	const float Dim = FMath::Pow(2.f, -0.85f * NightEV);
	if (PlanetMID)
	{
		PlanetMID->SetScalarParameterValue(TEXT("Gain"), 2.2f * Dim);
		PlanetMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(S.X), float(S.Y), float(S.Z), 0.f));
		PlanetMID->SetVectorParameterValue(TEXT("SunLight"), Light);
	}
	if (AirMID)
	{
		AirMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(S.X), float(S.Y), float(S.Z), 0.f));
		AirMID->SetVectorParameterValue(TEXT("SunLight"), Light);
		AirMID->SetVectorParameterValue(TEXT("Air"), FLinearColor(AirRadius, AirStrength, 0.f, 0.f));
		AirMID->SetScalarParameterValue(TEXT("Gain"), 2.2f * Dim);
	}
	if (StarMID)
	{
		StarMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(S.X), float(S.Y), float(S.Z), FMath::DegreesToRadians(It->SunAngularDiameterDeg * 0.5f)));
		StarMID->SetVectorParameterValue(TEXT("Look"), FLinearColor(StarGain * FMath::Pow(2.f, -0.5f * NightEV), StarTwinkle, 0.f, 0.f));
	}
	// the planet's light on the ice: its lit share as seen from here (a full planet when the star is behind us)
	const float Lit = 0.5f * (1.f + float(FVector::DotProduct(S, -LastSky.Planet)));
	if (SheenMID)
	{
		const FVector P = LastSky.Planet;
		const float PlanetCos = FMath::Cos(FMath::DegreesToRadians(PlanetDiameterDeg * 0.5f));
		const float SunCos = FMath::Cos(FMath::DegreesToRadians(It->SunAngularDiameterDeg * 0.5f));
		SheenMID->SetVectorParameterValue(TEXT("PlanetDir"), FLinearColor(float(P.X), float(P.Y), float(P.Z), PlanetCos));
		SheenMID->SetVectorParameterValue(TEXT("PlanetLight"), Light * (1.8f * Dim * Lit));
		SheenMID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(float(S.X), float(S.Y), float(S.Z), SunCos));
		SheenMID->SetVectorParameterValue(TEXT("SunLight"), It->SunColor * (It->SunIlluminance * It->SunDiskGain));
		SheenMID->SetScalarParameterValue(TEXT("SunLit"), FMath::SmoothStep(-5.f, 5.f, SunAlt));
	}
	PlanetShine->SetIntensity(0.05f + 0.9f * FMath::Pow(Lit, 1.3f));
}

void ALyudEuropaSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Fast = CVarEuropaFast.GetValueOnGameThread() > 0 ? 5.f : 1.f;
	Lid.Update(DeltaSeconds * Fast, 1.6f);
	if (WaterMID)
	{
		// the ripples' clock (wrapped as the sea's)
		const float T = float(FMath::Fmod(GetWorld()->GetTimeSeconds(), 1200.0));
		WaterMID->SetVectorParameterValue(TEXT("SeaClock"), FLinearColor(T, 128.f, 0.25f, 12.f));
	}
	SunTimer -= DeltaSeconds;
	if (SunTimer > 0.f)
	{
		return;
	}
	SunTimer = 0.5f;
	UpdateLook();
}

// ------------------------------------------------------------------ the subsystem

ULyudEuropaSubsystem* ULyudEuropaSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudEuropaSubsystem>() : nullptr;
}

bool ULyudEuropaSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULyudEuropaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarEuropa.GetValueOnGameThread() > 0)
	{
		TrySpawn();
	}
}

void ULyudEuropaSubsystem::Deinitialize()
{
	ShowSubtitle(false);
	// the rain's cvar outlives the world: leaving it while on Europa must not leave rain off in every later one
	if (bOnEuropa && !SavedRainFx.IsEmpty())
	{
		if (IConsoleVariable* Rain = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Rain.Fx")))
		{
			Rain->Set(*SavedRainFx, ECVF_SetByCode);
		}
		bOnEuropa = false;
	}
	Super::Deinitialize();
}

bool ULyudEuropaSubsystem::TrySpawn()
{
	UWorld* World = GetWorld();
	// the sea's world (Lvl_Scene01 and its test levels): a sky and a sea
	if (bSpawned || !World || !ALyudSea::Get(World) || !TActorIterator<ALyudSky>(World))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> J = ReadJson(TEXT("Europa/europa.json"));
	if (!J)
	{
		UE_LOG(LogTemp, Warning, TEXT("Europa: no Content/Europa/europa.json"));
		return false;
	}
	bSpawned = true;
	FVector Hatch = Vec(J, TEXT("hatch_cm"), FVector::ZeroVector);
	SiteOrigin = Vec(J, TEXT("europa_origin_cm"), FVector(0, -8.0e6, 5.0e5));
	GravityScale = J->GetNumberField(TEXT("gravity"));
	SpaceRadius = J->GetNumberField(TEXT("space_radius_cm"));
	WalkRadius = J->GetNumberField(TEXT("walk_radius_cm"));

	// the bed under the hatch as the game has it (round the collar: the landscape has a hole under the shaft)
	float Sum = 0.f;
	int32 N = 0;
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector At = Hatch + FRotator(0.f, 60.f * i, 0.f).RotateVector(FVector(220.f, 0.f, 0.f));
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, FVector(At.X, At.Y, 0.0), FVector(At.X, At.Y, -20000.0), FCollisionObjectQueryParams(ECC_WorldStatic));
		for (const FHitResult& H : Hits)
		{
			if (H.GetActor() && H.GetActor()->IsA<ALandscapeProxy>())
			{
				Sum += float(H.ImpactPoint.Z);
				++N;
				break;
			}
		}
	}
	const float SiteZ = KitData().SiteZ;
	if (N > 0)
	{
		const float Traced = Sum / N - SiteZ;
		UE_LOG(LogTemp, Display, TEXT("EUROPA hatch: bed traced %.0f cm, europa.json %.0f cm"), Sum / N, Hatch.Z + SiteZ);
		Hatch.Z = Traced;
	}
	FActorSpawnParameters P;
	P.Name = TEXT("EuropaAirlock");
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Airlock = World->SpawnActor<ALyudAirlock>(Hatch, FRotator::ZeroRotator, P);
	P.Name = TEXT("EuropaSite");
	Site = World->SpawnActorDeferred<ALyudEuropaSite>(ALyudEuropaSite::StaticClass(), FTransform(SiteOrigin), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Site)
	{
		const TSharedPtr<FJsonObject>* PO = nullptr;
		if (J->TryGetObjectField(TEXT("planet"), PO))
		{
			Site->PlanetDistance = (*PO)->GetNumberField(TEXT("distance_cm"));
			Site->PlanetDiameterDeg = (*PO)->GetNumberField(TEXT("diameter_deg"));
		}
		UGameplayStatics::FinishSpawningActor(Site, FTransform(SiteOrigin));
		// the sky seen from the ice: Europa's own (the sun and the planet where its orbit has them)
		TWeakObjectPtr<ALyudEuropaSite> WeakSite = Site;
		for (TActorIterator<ALyudSky> It(World); It; ++It)
		{
			It->SetSpaceObserver([WeakSite](const FDateTime& Utc, FVector& OutSun, FVector& OutPlanet)
			{
				if (const ALyudEuropaSite* S = WeakSite.Get())
				{
					const ALyudEuropaSite::FSky Sky = S->SkyAt(Utc);
					OutSun = Sky.Sun;
					OutPlanet = Sky.Planet;
				}
			});
		}
	}
	UE_LOG(LogTemp, Display, TEXT("EUROPA spawned: hatch %s, Europa %s"), *Hatch.ToString(), *SiteOrigin.ToString());
	return true;
}

float ULyudEuropaSubsystem::Speed() const
{
	return CVarEuropaFast.GetValueOnGameThread() > 0 ? 5.f : 1.f;
}

bool ULyudEuropaSubsystem::HasSeenPlanet() const
{
	return Airlock && Airlock->bPlanetSeen;
}

FVector ULyudEuropaSubsystem::PlanetDirection() const
{
	return Site ? Site->PlanetDir : FVector::UpVector;
}

bool ULyudEuropaSubsystem::IsIslandFacing() const
{
	return Site && Site->IsIslandFacing();
}

bool ULyudEuropaSubsystem::IsMusicPlaying() const
{
	return Music && Music->IsPlaying();
}

void ULyudEuropaSubsystem::Tick(float DeltaTime)
{
	APlayerController* PC = nullptr;
	ACharacter* Hero = PlayerHero(GetWorld(), &PC);
	if (!Airlock || !Site || !Hero || !PC)
	{
		return;
	}
	if (Climb.Num() > 0)
	{
		TickClimb(*Hero, DeltaTime * Speed());
	}
	// Europa's ground loads in the background while the player is in the airlock (built at once if it isn't by the time)
	if (!Site->HasGround() && Airlock->IsInside(Hero->GetActorLocation()) && ULyudPrefetch::FolderReady(this, TEXT("/Game/Europa/Moon")))
	{
		Site->EnsureGround();
	}
	// on Europa: anywhere near the far site (a climb jumps there and back; a load may land there too)
	const bool bThere = FVector::Dist(Hero->GetActorLocation(), SiteOrigin) < SpaceRadius;
	if (bThere != bOnEuropa)
	{
		SetOnEuropa(bThere);
	}
	if (bOnEuropa)
	{
		TickEuropa(*Hero, *PC, DeltaTime);
	}
	UpdateAction(*Hero, *PC);
	const ULyudInventoryUI* ScreenUI = ULyudInventoryUI::Get(this);
	if (PC->WasInputKeyJustPressed(EKeys::E) && !Action.IsEmpty() && !(ScreenUI && ScreenUI->IsOpen()))
	{
		// polled, never consumed: E belongs to several things at once
		Use();
	}
	if (SubtitleFor > 0.f || SubtitleAlpha > 0.f)
	{
		SubtitleFor -= DeltaTime;
		SubtitleAlpha = FMath::FInterpConstantTo(SubtitleAlpha, SubtitleFor > 0.f ? 1.f : 0.f, DeltaTime, 3.f);
		if (SubtitleFor <= 0.f && SubtitleAlpha <= 0.f)
		{
			ShowSubtitle(false);
		}
	}
}

void ULyudEuropaSubsystem::UpdateAction(ACharacter& Hero, APlayerController& PC)
{
	FString Now;
	if (Climb.Num() == 0)
	{
		const AHeroCharacter* H = Cast<AHeroCharacter>(&Hero);
		const FVector Eye = H ? H->EyeLocation() : Hero.GetPawnViewLocation();
		const FVector Dir = PC.GetControlRotation().Vector();
		auto Looks = [&Eye, &Dir](const FVector& At, float Range, float MinDot)
		{
			const FVector D = At - Eye;
			const float Dist = float(D.Size());
			return Dist < Range && (Dist < 90.f || FVector::DotProduct(Dir, D / Dist) > MinDot);
		};
		const FKitData& K = KitData();
		if (bOnEuropa)
		{
			const FVector LidAt = Site->Kit(FVector(0, 0, 10));
			if (Eye.Z > LidAt.Z - 30.f && Looks(LidAt, 280.f, 0.55f))
			{
				Now = Site->Lid.IsShut() ? TEXT("open_europa") : Site->Lid.IsFullyOpen() ? TEXT("enter_europa") : TEXT("");
			}
		}
		else if (Airlock->IsInside(Eye))
		{
			const FVector L = Airlock->GetActorTransform().InverseTransformPosition(Hero.GetActorLocation());
			if (Looks(Airlock->ScreenLocation(), 190.f, 0.6f))
			{
				Now = TEXT("console");
			}
			else if (FVector2D::Distance(FVector2D(L), FVector2D::ZeroVector) < 95.f)
			{
				Now = TEXT("climb_sea");
			}
			else if (FVector2D::Distance(FVector2D(L), FVector2D(K.EuropaShaft)) < 95.f)
			{
				Now = TEXT("climb_europa");
			}
		}
		else
		{
			const FVector LidAt = Airlock->Kit(FVector(0, 0, 10));
			if (Eye.Z > LidAt.Z - 30.f && Looks(LidAt, 330.f, 0.5f))
			{
				Now = Airlock->SeaLid.IsShut() ? TEXT("open_sea") : Airlock->SeaLid.IsFullyOpen() ? TEXT("enter_sea") : TEXT("");
			}
		}
	}
	Action = Now;
	if (Action != ShownPrompt)
	{
		ShownPrompt = Action;
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(this))
		{
			UI->SetPrompt(this, Action.IsEmpty() ? FText::GetEmpty() : LyudText::Get(TEXT("ui.europa.") + Action));
		}
	}
}

void ULyudEuropaSubsystem::Use()
{
	if (!Airlock || !Site || Climb.Num() > 0)
	{
		return;
	}
	const ELyudAirlockState S = Airlock->GetState();
	UE_LOG(LogTemp, Display, TEXT("EUROPA use %s (airlock %d)"), *Action, int32(S));
	if (Action == TEXT("open_sea"))
	{
		// the sea hatch gives only with the sea on both sides of it
		if (S == ELyudAirlockState::Flooded)
		{
			Airlock->SetSeaLidOpen(true);
		}
		else
		{
			Toast(this, TEXT("ui.europa.stuck"));
		}
	}
	else if (Action == TEXT("enter_sea"))
	{
		ClimbDownFromSea();
	}
	else if (Action == TEXT("climb_sea"))
	{
		if (S == ELyudAirlockState::Flooded)
		{
			ClimbUpToSea();
		}
		else
		{
			Toast(this, TEXT("ui.europa.need_water"));
		}
	}
	else if (Action == TEXT("climb_europa"))
	{
		if (S == ELyudAirlockState::Dry)
		{
			ClimbUpToEuropa();
		}
		else
		{
			Toast(this, TEXT("ui.europa.need_dry"));
		}
	}
	else if (Action == TEXT("console"))
	{
		FString Why;
		if (!Airlock->PressButton(Why))
		{
			Toast(this, *Why);
		}
	}
	else if (Action == TEXT("open_europa"))
	{
		if (S == ELyudAirlockState::Dry)
		{
			Site->Lid.bOpen = true;
			if (ULyudSoundscapeSubsystem* SS = ULyudSoundscapeSubsystem::Get(this))
			{
				// no air to carry it: felt through the hands, not heard
				SS->PlayEffect(TEXT("hatch_open"), Site->Kit(FVector(0, 0, 10)), 0.25f);
			}
		}
		else
		{
			Toast(this, TEXT("ui.europa.stuck"));
		}
	}
	else if (Action == TEXT("enter_europa"))
	{
		ClimbDownFromEuropa();
	}
}

// ------------------------------------------------------------------ climbing

namespace
{
	/** The room's Europa shaft and the one under Europa's hatch are alike from the top, but their rungs sit 10 cm apart
	 *  on that measure: the jump between them shifts the feet by this (not the shafts' tops' 90 cm), so hands and feet
	 *  land on the same rungs and nothing pops; the tops differ by 10 cm, out of sight up the shaft. */
	constexpr float ShaftJump = 80.f;
}

FVector ULyudEuropaSubsystem::FeetOf(const ACharacter& Hero) const
{
	return Hero.GetActorLocation() - FVector(0, 0, Hero.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void ULyudEuropaSubsystem::SetFeet(ACharacter& Hero, const FVector& Feet) const
{
	Hero.SetActorLocation(Feet + FVector(0, 0, Hero.GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
}

void ULyudEuropaSubsystem::Rungs(TArray<FKey>& Keys, ULyudLadderComponent* Ladder, const FVector& From, const FVector& To) const
{
	FKey& K = Keys.AddDefaulted_GetRef();
	// on the ladder the body hangs out from its rungs
	K.Feet = Ladder ? Ladder->ToWorld(FVector(Ladder->BodyStandoff, 0.f, Ladder->FromWorld(To).Z)) : To;
	K.Ladder = Ladder;
	// a rung (30 cm) every 0.42 s: hand over hand, each limb two rungs a cycle
	K.Seconds = FMath::Max(0.3f, float(FMath::Abs(To.Z - From.Z)) / 30.f * 0.42f);
}

void ULyudEuropaSubsystem::Hold(TArray<FKey>& Keys, ULyudLadderComponent* Ladder, float Seconds) const
{
	const FVector At = Keys.Num() > 0 ? Keys.Last().Feet : FVector::ZeroVector;
	FKey& K = Keys.AddDefaulted_GetRef();
	K.Feet = At;
	K.Ladder = Ladder;
	K.Seconds = Seconds;
}

void ULyudEuropaSubsystem::StartClimb(TArray<FKey>&& Keys, TFunction<void()> Done)
{
	APlayerController* PC = nullptr;
	ACharacter* Hero = PlayerHero(GetWorld(), &PC);
	if (!Hero || !PC || Keys.Num() == 0)
	{
		return;
	}
	Climb = MoveTemp(Keys);
	ClimbIndex = 0;
	ClimbTime = 0.f;
	ClimbFrom = FeetOf(*Hero);
	ClimbDone = MoveTemp(Done);
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Sea->SetPlayerScripted(this, true);
	}
	Hero->SetActorEnableCollision(false);
	UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_None);
	PC->SetIgnoreMoveInput(true);
	// face the ladder (+X of the kit: the rungs are on that side of every shaft)
	PC->SetControlRotation(FRotator(PC->GetControlRotation().Pitch, 0.f, 0.f));
	UE_LOG(LogTemp, Display, TEXT("EUROPA climb start (%d keys)"), Climb.Num());
}

void ULyudEuropaSubsystem::TickClimb(ACharacter& Hero, float DeltaTime)
{
	FKey& K = Climb[ClimbIndex];
	ULyudLadderClimber* Climber = ULyudLadderClimber::Get(&Hero);
	ULyudLadderComponent* L = K.Ladder.Get();
	if (K.bJump)
	{
		SetFeet(Hero, K.Feet);
		if (L && Climber)
		{
			// into the other shaft, on the same rungs (ShaftJump)
			Climber->Attach(L, float(L->FromWorld(K.Feet).Z), 0.f);
		}
	}
	else
	{
		ClimbTime += DeltaTime;
		const float T = FMath::Clamp(ClimbTime / FMath::Max(K.Seconds, 1e-3f), 0.f, 1.f);
		if (L && Climber)
		{
			// hand over hand while the hands still find rungs above; over the top the body goes on by itself
			const float Z0 = float(L->FromWorld(ClimbFrom).Z), Z1 = float(L->FromWorld(K.Feet).Z);
			const float Z = FMath::Lerp(Z0, Z1, T);
			const float Reach = L->TopRungZ() - Climber->HandAboveFeet + L->RungSpacing;
			if (Z <= Reach)
			{
				if (Climber->GetLadder() != L || !Climber->IsOnLadder())
				{
					Climber->Attach(L, Z, 0.35f);
				}
				Climber->SetFeetZ(Z);
			}
			else
			{
				if (Climber->IsOnLadder())
				{
					Climber->Release(0.5f);
				}
				SetFeet(Hero, L->ToWorld(FVector(L->BodyStandoff, 0.f, Z)));
			}
		}
		else
		{
			if (Climber && Climber->IsOnLadder())
			{
				Climber->Release(0.4f);
			}
			SetFeet(Hero, FMath::Lerp(ClimbFrom, K.Feet, Smooth01(T)));
		}
		if (T < 1.f)
		{
			return;
		}
	}
	ClimbFrom = K.Feet;
	ClimbTime = 0.f;
	TFunction<void()> Reach = K.OnReach;
	++ClimbIndex;
	if (Reach)
	{
		Reach();
	}
	if (ClimbIndex >= Climb.Num())
	{
		EndClimb(Hero);
	}
}

void ULyudEuropaSubsystem::EndClimb(ACharacter& Hero)
{
	Climb.Reset();
	ClimbIndex = 0;
	if (ULyudLadderClimber* Climber = ULyudLadderClimber::Get(&Hero, false))
	{
		Climber->Release(0.4f);
	}
	Hero.SetActorEnableCollision(true);
	Hero.GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (APlayerController* PC = Cast<APlayerController>(Hero.GetController()))
	{
		PC->SetIgnoreMoveInput(false);
	}
	if (ALyudSea* Sea = ALyudSea::Get(this))
	{
		Sea->SetPlayerScripted(this, false);
	}
	TFunction<void()> Done = MoveTemp(ClimbDone);
	ClimbDone = nullptr;
	UE_LOG(LogTemp, Display, TEXT("EUROPA climb done at %s"), *Hero.GetActorLocation().ToString());
	if (Done)
	{
		Done();
	}
}

void ULyudEuropaSubsystem::ClimbDownFromSea()
{
	const FKitData& K = KitData();
	ALyudAirlock* A = Airlock;
	TArray<FKey> Keys;
	const FVector Above = A->Kit(FVector(0, 0, 30));
	FKey& K0 = Keys.AddDefaulted_GetRef();
	K0.Feet = Above;
	K0.Seconds = 1.0f;
	const FVector Shut = A->Kit(FVector(0, 0, -230));
	Rungs(Keys, A->SeaLadder, Above, Shut);
	// the head under the rim: pull the lid shut over it
	Keys.Last().OnReach = [A]() { A->SetSeaLidOpen(false); };
	const FVector Floor = A->Kit(FVector(0, 0, K.Floor + 2.f));
	Rungs(Keys, A->SeaLadder, Shut, Floor);
	FKey& Off = Keys.AddDefaulted_GetRef();
	Off.Feet = A->Kit(FVector(-55, 25, K.Floor + 2.f));
	Off.Seconds = 0.8f;
	StartClimb(MoveTemp(Keys), [this, A]()
	{
		// a full lock is the sea's: swim in it, under
		if (A->GetState() == ELyudAirlockState::Flooded)
		{
			if (ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this))
			{
				Swim->EnterUnder();
			}
		}
	});
}

void ULyudEuropaSubsystem::ClimbUpToSea()
{
	const FKitData& K = KitData();
	ALyudAirlock* A = Airlock;
	TArray<FKey> Keys;
	FKey& K0 = Keys.AddDefaulted_GetRef();
	K0.Feet = A->Kit(FVector(0, 0, K.Floor + 2.f));
	K0.Seconds = 0.6f;
	const FVector Under = A->Kit(FVector(0, 0, -260));
	Rungs(Keys, A->SeaLadder, K0.Feet, Under);
	Keys.Last().OnReach = [A]() { A->SetSeaLidOpen(true); };
	Hold(Keys, A->SeaLadder, A->LidSeconds + 0.2f);
	Rungs(Keys, A->SeaLadder, Under, A->Kit(FVector(0, 0, 20)));
	// out over the collar onto the bed beside it
	FKey& Out = Keys.AddDefaulted_GetRef();
	Out.Feet = A->Kit(FVector(200, 0, K.SiteZ + 10.f));
	Out.Seconds = 1.2f;
	Out.OnReach = [A]() { A->SetSeaLidOpen(false); };
	StartClimb(MoveTemp(Keys), [this]()
	{
		// back in the sea, under it: the swimming takes over
		if (ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this))
		{
			Swim->EnterUnder();
		}
	});
}

void ULyudEuropaSubsystem::ClimbUpToEuropa()
{
	const FKitData& K = KitData();
	ALyudAirlock* A = Airlock;
	ALyudEuropaSite* S = Site;
	TArray<FKey> Keys;
	FKey& K0 = Keys.AddDefaulted_GetRef();
	K0.Feet = A->Kit(K.EuropaShaft + FVector(0, 0, K.Floor + 2.f));
	K0.Seconds = 0.6f;
	K0.OnReach = [A]() { A->SetInnerLidOpen(true); };
	FKey& Wait = Keys.AddDefaulted_GetRef();
	Wait.Feet = K0.Feet;
	Wait.Seconds = A->LidSeconds + 0.2f;
	// up into the shaft until the eyes are well inside it, then the same place in Europa's shaft (on the same rungs:
	// ShaftJump)
	const float JumpFeet = K.Ceiling - 15.f;
	const FVector InA = A->Kit(K.EuropaShaft + FVector(0, 0, JumpFeet));
	Rungs(Keys, A->EuropaLadder, K0.Feet, InA);
	FKey& Jump = Keys.AddDefaulted_GetRef();
	Jump.bJump = true;
	Jump.Ladder = S->ShaftLadder;
	Jump.Feet = S->ShaftLadder->ToWorld(FVector(S->ShaftLadder->BodyStandoff, 0.f, S->ShaftLadder->FromWorld(S->Kit(FVector(0, 0, JumpFeet + ShaftJump))).Z));
	Jump.OnReach = [this, A]()
	{
		A->SetInnerLidOpen(false, true);
		SetOnEuropa(true);
	};
	const FVector Under = S->Kit(FVector(0, 0, -150));
	Rungs(Keys, S->ShaftLadder, Jump.Feet, Under);
	Keys.Last().OnReach = [S]() { S->Lid.bOpen = true; };
	Hold(Keys, S->ShaftLadder, 1.8f);
	Rungs(Keys, S->ShaftLadder, Under, S->Kit(FVector(0, 0, 20)));
	FKey& Out = Keys.AddDefaulted_GetRef();
	Out.Feet = S->Kit(FVector(200, 0, K.SiteZ + 5.f));
	Out.Seconds = 1.2f;
	// out of the dark shaft into the bare star's light: blinded the first time (the sun-glare thread's effect)
	Keys.Last().OnReach = [this, S]()
	{
		S->Lid.bOpen = false;
		if (ULyudSunGlare* Glare = ULyudSunGlare::Get(this))
		{
			Glare->FlashBlindOnce(TEXT("EuropaHatch"), 1.f);
		}
	};
	StartClimb(MoveTemp(Keys), nullptr);
}

void ULyudEuropaSubsystem::ClimbDownFromEuropa()
{
	const FKitData& K = KitData();
	ALyudAirlock* A = Airlock;
	ALyudEuropaSite* S = Site;
	TArray<FKey> Keys;
	FKey& K0 = Keys.AddDefaulted_GetRef();
	K0.Feet = S->Kit(FVector(0, 0, 30));
	K0.Seconds = 1.0f;
	const FVector Shut = S->Kit(FVector(0, 0, -230));
	Rungs(Keys, S->ShaftLadder, K0.Feet, Shut);
	Keys.Last().OnReach = [S]() { S->Lid.bOpen = false; };
	const float JumpFeet = K.Ceiling - 15.f;
	const FVector InB = S->Kit(FVector(0, 0, JumpFeet + ShaftJump));
	Rungs(Keys, S->ShaftLadder, Shut, InB);
	FKey& Jump = Keys.AddDefaulted_GetRef();
	Jump.bJump = true;
	Jump.Ladder = A->EuropaLadder;
	Jump.Feet = A->EuropaLadder->ToWorld(FVector(A->EuropaLadder->BodyStandoff, 0.f, A->EuropaLadder->FromWorld(A->Kit(K.EuropaShaft + FVector(0, 0, JumpFeet))).Z));
	Jump.OnReach = [this, A]()
	{
		A->SetInnerLidOpen(true, true);
		SetOnEuropa(false);
	};
	const FVector Floor = A->Kit(K.EuropaShaft + FVector(0, 0, K.Floor + 2.f));
	Rungs(Keys, A->EuropaLadder, Jump.Feet, Floor);
	FKey& Off = Keys.AddDefaulted_GetRef();
	Off.Feet = A->Kit(K.EuropaShaft + FVector(75, -30, K.Floor + 2.f));
	Off.Seconds = 0.8f;
	Off.OnReach = [A]() { A->SetInnerLidOpen(false); };
	StartClimb(MoveTemp(Keys), nullptr);
}

// ------------------------------------------------------------------ on Europa

void ULyudEuropaSubsystem::SetOnEuropa(bool bOn)
{
	if (bOnEuropa == bOn)
	{
		return;
	}
	bOnEuropa = bOn;
	UWorld* World = GetWorld();
	for (TActorIterator<ALyudSky> It(World); It; ++It)
	{
		It->SetSpaceView(bOn);
	}
	for (TActorIterator<ALyudRareSky> It(World); It; ++It)
	{
		It->SetActorHiddenInGame(bOn);
	}
	if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
	{
		S->SetVacuum(bOn);
	}
	if (Site)
	{
		Site->SetShown(bOn);
	}
	if (IConsoleVariable* Rain = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Rain.Fx")))
	{
		if (bOn)
		{
			SavedRainFx = Rain->GetString();
			Rain->Set(TEXT("0"), ECVF_SetByCode);
		}
		else if (!SavedRainFx.IsEmpty())
		{
			Rain->Set(*SavedRainFx, ECVF_SetByCode);
		}
	}
	APlayerController* PC = nullptr;
	if (ACharacter* Hero = PlayerHero(World, &PC))
	{
		UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
		if (bOn)
		{
			SavedGravity = Move->GravityScale;
			SavedAirControl = Move->AirControl;
			Move->GravityScale = GravityScale;
			Move->AirControl = FMath::Max(SavedAirControl, 0.5f);
		}
		else
		{
			Move->GravityScale = SavedGravity;
			Move->AirControl = SavedAirControl;
		}
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
		{
			if (bOn && !Breath)
			{
				Breath = S->AttachLoop(TEXT("europa_breath"), Hero->GetRootComponent(), 0.5f);
				if (Breath)
				{
					Breath->FadeIn(2.f, 0.5f);
				}
			}
			else if (!bOn && Breath)
			{
				Breath->FadeOut(1.f, 0.f);
				Breath = nullptr;
			}
		}
	}
	if (!bOn && Music)
	{
		Music->FadeOut(3.f, 0.f);
		Music = nullptr;
	}
	UE_LOG(LogTemp, Display, TEXT("EUROPA %s"), bOn ? TEXT("arrived on Europa") : TEXT("left Europa"));
}

void ULyudEuropaSubsystem::TickEuropa(ACharacter& Hero, APlayerController& PC, float DeltaTime)
{
	// the fence: a soft stop 3.5 km out (the ice ends at 4)
	const FVector Off = Hero.GetActorLocation() - SiteOrigin;
	if (Off.Size2D() > WalkRadius && Climb.Num() == 0)
	{
		const FVector2D Back = FVector2D(Off).GetSafeNormal() * WalkRadius;
		Hero.SetActorLocation(FVector(SiteOrigin.X + Back.X, SiteOrigin.Y + Back.Y, Hero.GetActorLocation().Z));
	}
	// own steps, through the body (no air to carry the rest)
	const UCharacterMovementComponent* Move = Hero.GetCharacterMovement();
	if (Move->IsMovingOnGround() && Move->Velocity.Size2D() > 40.f)
	{
		StrideLeft -= float(Move->Velocity.Size2D()) * 0.01f * DeltaTime;
		if (StrideLeft <= 0.f)
		{
			StrideLeft = 0.8f;
			if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
			{
				S->PlayEffect(TEXT("europa_step"), FeetOf(Hero), 0.6f);
			}
		}
	}
	// the planet: the first good look at it from the ice (not from the shaft), with the island on its face toward us
	// ("only the sea... and our island"); while the town is turned away it is all ocean and cloud, and he says nothing
	if (!Airlock->bPlanetSeen && Climb.Num() == 0 && Site && Site->IsIslandFacing())
	{
		const FVector Eye = PC.PlayerCameraManager ? PC.PlayerCameraManager->GetCameraLocation() : Hero.GetPawnViewLocation();
		const FVector Fwd = PC.GetControlRotation().Vector();
		const float Cos = float(FVector::DotProduct(Fwd, Site->PlanetDir));
		if (Eye.Z > SiteOrigin.Z && Cos > FMath::Cos(FMath::DegreesToRadians(24.f)))
		{
			Airlock->bPlanetSeen = true;
			LineIn = 0.7f;
			UE_LOG(LogTemp, Display, TEXT("EUROPA planet seen"));
		}
	}
	if (LineIn > 0.f)
	{
		LineIn -= DeltaTime;
		if (LineIn <= 0.f)
		{
			SayPlanetLine();
		}
	}
}

void ULyudEuropaSubsystem::SayPlanetLine()
{
	USoundBase* Voice = LyudText::LoadVoice(PlanetLine);
	APlayerController* PC = nullptr;
	ACharacter* Hero = PlayerHero(GetWorld(), &PC);
	if (Voice && Hero)
	{
		// his own voice, inside his head: the effects volume, not the (silent) air
		if (ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this))
		{
			S->PlayVoiceAt(Voice, Hero->GetPawnViewLocation(), 1.f);
		}
	}
	SubtitleFor = Voice ? FMath::Max(3.5f, Voice->GetDuration() + 1.f) : 6.f;
	ShowSubtitle(true);
	// the music starts under the line
	if (USoundBase* Track = LoadObject<USoundBase>(nullptr, MusicPath))
	{
		const ULyudSoundscapeSubsystem* S = ULyudSoundscapeSubsystem::Get(this);
		const float Volume = S ? S->MasterVolume * S->MusicVolume * 1.6f : 0.7f;
		Music = UGameplayStatics::CreateSound2D(this, Track, 1.f, 1.f, 0.f, nullptr, false, false);
		if (Music)
		{
			Music->FadeIn(2.5f, FMath::Min(Volume, 1.f));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("EUROPA line %s (voice %d, music %d)"), PlanetLine, Voice ? 1 : 0, Music ? 1 : 0);
}

void ULyudEuropaSubsystem::ShowSubtitle(bool bShow)
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (bShow && !SubtitleWidget)
	{
		SubtitleWidget = SNew(SLyudEuropaSubtitle).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(SubtitleWidget.ToSharedRef(), 58);
	}
	else if (!bShow && SubtitleWidget)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SubtitleWidget.ToSharedRef());
		SubtitleWidget.Reset();
	}
}

// ------------------------------------------------------------------ tests

void ULyudEuropaSubsystem::GoHatch()
{
	ACharacter* Hero = PlayerHero(GetWorld());
	if (!Airlock || !Hero)
	{
		return;
	}
	// in the water a little off the lid, looking at it
	SetFeet(*Hero, Airlock->Kit(FVector(-150, 0, 40)));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation(FRotator(-35.f, 0.f, 0.f));
	}
	if (ULyudSwimComponent* Swim = ULyudSwimComponent::Get(this))
	{
		Swim->EnterUnder();
	}
}

void ULyudEuropaSubsystem::GoRoom()
{
	ACharacter* Hero = PlayerHero(GetWorld());
	if (!Airlock || !Hero)
	{
		return;
	}
	const FKitData& K = KitData();
	SetFeet(*Hero, Airlock->Kit(K.RoomCenter + FVector(0, -40, K.Floor + 2.f)));
	if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
	{
		PC->SetControlRotation((Airlock->ScreenLocation() - Hero->GetPawnViewLocation()).Rotation());
	}
}

void ULyudEuropaSubsystem::GoEuropa()
{
	ACharacter* Hero = PlayerHero(GetWorld());
	if (!Site || !Hero)
	{
		return;
	}
	SetFeet(*Hero, Site->Kit(FVector(220, 0, KitData().SiteZ + 5.f)));
	SetOnEuropa(true);
}

FString ULyudEuropaSubsystem::Report() const
{
	const ACharacter* Hero = PlayerHero(GetWorld());
	const UCharacterMovementComponent* Move = Hero ? Hero->GetCharacterMovement() : nullptr;
	return FString::Printf(TEXT("EUROPA report: airlock %d water %.2f sealid %.2f innerlid %.2f europalid %.2f | on europa %d climbing %d action '%s' | gravity %.2f mode %d | planet seen %d music %d | hero %s"),
		Airlock ? int32(Airlock->GetState()) : -1, Airlock ? Airlock->GetWater01() : -1.f, Airlock ? Airlock->SeaLid.Openness : -1.f,
		Airlock ? Airlock->InnerLid.Openness : -1.f, Site ? Site->Lid.Openness : -1.f, bOnEuropa ? 1 : 0, Climb.Num() > 0 ? 1 : 0, *Action,
		Move ? Move->GravityScale : 0.f, Move ? int32(Move->MovementMode) : -1, HasSeenPlanet() ? 1 : 0, Music && Music->IsPlaying() ? 1 : 0,
		Hero ? *Hero->GetActorLocation().ToString() : TEXT("none"))
		+ FString::Printf(TEXT(" | island facing %d up %.2f townsun %.2f"), Site && Site->IsIslandFacing() ? 1 : 0, Site ? Site->GetSky().IslandUp : 0.f, Site ? Site->GetSky().TownSun : 0.f)
		+ FString::Printf(TEXT(" | planet dir %s at %s scale %.0f shown %d | sun %s | swim %d"),
		Site ? *Site->PlanetDir.ToString() : TEXT("-"), Site && Site->Planet ? *Site->Planet->GetComponentLocation().ToString() : TEXT("-"),
		Site && Site->Planet ? Site->Planet->GetComponentScale().X : 0.f, Site ? int32(Site->IsShown()) : -1,
		TActorIterator<ALyudSky>(GetWorld()) ? *(*TActorIterator<ALyudSky>(GetWorld()))->SunDirection().ToString() : TEXT("-"),
		ULyudSwimComponent::Get(GetWorld()) ? int32(ULyudSwimComponent::Get(GetWorld())->GetState()) : -1);
}
