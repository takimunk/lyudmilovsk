// The yacht school on the beach: the boathouse and what stands in and round it (Content/City/sailing_boathouse.json, the
// layout of blender/sailing/boathouse.py), its doors, the slip's boards laid down to today's water, the signal mast's
// flags, the windsock, the lamps and the halyards' rattle. Built once when the hero first comes near, hidden when he is
// far. docs/yacht_school.md "Engine: the school".
#include "Sailing/LyudSailing.h"

#include "LyudDinghyImpl.h"
#include "LyudSailingState.h"
#include "SailSound.h"

#include "../People/MiniJson.h"
#include "../Transport/LyudMeshKit.h"
#include "Audio/LyudSoundscape.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Doors/LyudDoor.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sailing/LyudDinghy.h"
#include "UObject/Package.h"

// The wind-kites branch brings the town's windsocks (ULyudWindTells): until it is merged the school flies its own sock.
#if defined(__has_include)
#if __has_include("Weather/LyudWindsock.h")
#define LYUD_SAIL_HAS_WINDTELLS 1
#endif
#endif
#ifndef LYUD_SAIL_HAS_WINDTELLS
#define LYUD_SAIL_HAS_WINDTELLS 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLyudSailSite, Log, All);

using namespace LyudSail;

namespace LyudSailingSiteImpl
{
	typedef LyudJson::FValue FV;

	/** A point of the building's frame (metres, +X the sea, -Y the north wall) in the site actor's frame, cm. */
	FVector Local(const FV& Pos, float ZIfNone = 0.f)
	{
		return FVector(Pos[(size_t)0].F() * 100.f, -Pos[(size_t)1].F() * 100.f, (Pos.Size() > 2 && Pos[(size_t)2].Type == FV::Number ? Pos[(size_t)2].F() : ZIfNone) * 100.f);
	}

	/** Blender's XYZ Euler angles (degrees, in the building's right-handed frame) as a rotation in the site's frame. */
	FQuat Turned(const FV& Rot)
	{
		const float Rx = FMath::DegreesToRadians(Rot[(size_t)0].F()), Ry = FMath::DegreesToRadians(Rot[(size_t)1].F()), Rz = FMath::DegreesToRadians(Rot[(size_t)2].F());
		return FQuat(FVector::UpVector, -Rz) * FQuat(FVector::RightVector, Ry) * FQuat(FVector::ForwardVector, -Rx);
	}

	/** "SM_Sail_Prop_Desk" -> the kit's asset name "Prop_Desk". */
	FString AssetName(const std::string& Mesh)
	{
		FString Name = LyudSailingImpl::Str(Mesh);
		Name.RemoveFromStart(TEXT("SM_Sail_"));
		return Name;
	}

	/** Small and hung things nobody should bump into. */
	bool Solid(const FString& Name)
	{
		static const TCHAR* const Soft[] = { TEXT("Lamp"), TEXT("Photo"), TEXT("Drawing"), TEXT("Chart"), TEXT("Clock"), TEXT("Barometer"), TEXT("Anemometer"), TEXT("Radio"),
			TEXT("Bell"), TEXT("Paddle"), TEXT("RopeCoil"), TEXT("SailBag"), TEXT("Bailer"), TEXT("Board"), TEXT("LifeRing"), TEXT("PaintTins"), TEXT("BuoyNet"), TEXT("Bracket"),
			TEXT("Buoy"), TEXT("MOB"), TEXT("Mast"), TEXT("Main"), TEXT("Jib"), TEXT("JacketRail"), TEXT("Trolley"), TEXT("Hose") };
		for (const TCHAR* S : Soft)
		{
			if (Name.Contains(S))
			{
				// the signal mast is a mast one does walk into
				return Name.Contains(TEXT("SignalMast"));
			}
		}
		return true;
	}

	UStaticMeshComponent* Prop(AActor* Holder, TArray<TObjectPtr<UObject>>& Keep, UStaticMesh* Mesh, const FTransform& LocalXf, bool bCollide, float CullCm)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Holder, NAME_None, RF_Transient);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCachedMaxDrawDistance(CullCm);
		C->SetupAttachment(Holder->GetRootComponent());
		C->SetRelativeTransform(LocalXf);
		C->RegisterComponent();
		Holder->AddInstanceComponent(C);
		Keep.Add(C);
		return C;
	}

	/** The flag's mesh of the kit by the signal's id ("flag_school", "flag_ap", "storm_cone" ...), as the launch's staff. */
	FString FlagAsset(const std::string& Id)
	{
		FString Name = LyudSailingImpl::Str(Id).ToLower();
		if (Name == TEXT("storm_cone"))
		{
			return TEXT("StormCone");
		}
		Name.RemoveFromStart(TEXT("flag_"));
		static const TMap<FString, FString> Alias = {
			{ TEXT("p"), TEXT("prep") }, { TEXT("blue"), TEXT("prep") }, { TEXT("race"), TEXT("races") }, { TEXT("s"), TEXT("races") },
			{ TEXT("optimist"), TEXT("class_optimist") }, { TEXT("cadet"), TEXT("class_cadet") }, { TEXT("luch"), TEXT("class_luch") },
			{ TEXT("first_sub"), TEXT("recall") }, { TEXT("x"), TEXT("recall") }, { TEXT("ap"), TEXT("shore") }, { TEXT("n"), TEXT("closed") },
			{ TEXT("school"), TEXT("sail_on") } };
		if (const FString* To = Alias.Find(Name))
		{
			Name = *To;
		}
		return FString(TEXT("Flag_")) + Name;
	}

	FLinearColor FlagTint(const std::string& Id)
	{
		if (Id.find("recall") != std::string::npos || Id.find("ap") != std::string::npos)
		{
			return FLinearColor(0.05f, 0.15f, 0.6f);
		}
		if (Id.find("shore") != std::string::npos || Id.find("storm") != std::string::npos)
		{
			return FLinearColor(0.75f, 0.06f, 0.05f);
		}
		if (Id.find("school") != std::string::npos)
		{
			return FLinearColor(0.1f, 0.55f, 0.2f);
		}
		return FLinearColor(0.9f, 0.75f, 0.1f);
	}

	void DressFlag(UStaticMeshComponent* C, const std::string& Id)
	{
		if (!C)
		{
			return;
		}
		if (Id.empty())
		{
			C->SetVisibility(false);
			return;
		}
		C->EmptyOverrideMaterials();
		if (UStaticMesh* Mesh = LyudSailKit::Asset(FlagAsset(Id)))
		{
			C->SetStaticMesh(Mesh);
		}
		else
		{
			const FLinearColor Tint = FlagTint(Id);
			C->SetStaticMesh(LyudSailKit::FlagQuad().Mesh);
			LyudSailKit::PaintBuilt(C, LyudSailKit::FlagQuad(), Tint, Tint);
		}
		C->SetVisibility(true);
	}

	/** The first boards of the slip in the building's frame (blender/sailing/boathouse.py): 1.2 m apart down the sand. */
	constexpr float SlipX0 = 9.1f, SlipY = -9.9f, SlipStep = 1.2f;
	constexpr int32 SlipFixed = 19, SlipMax = 230;
	inline float SandZ(float X) { return -0.0727f * (X + 0.85f); }
}

// ------------------------------------------------------------------ building

void ULyudSailing::BuildSite()
{
	bSiteBuilt = true;
	UWorld* World = GetWorld();
	FLyudSailingState& S = *M;
	if (!World)
	{
		return;
	}
	const FSchoolPoint& At = S.P.Boathouse.At;
	const FTransform House(FRotator(0.f, float(S.P.Yaw), 0.f), FVector(At.X * 100.0, At.Y * 100.0, S.P.FloorZ * 100.0));
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;
	SiteActor = World->SpawnActor<AActor>(AActor::StaticClass(), House, SP);
	if (!SiteActor)
	{
		return;
	}
	USceneComponent* SiteRoot = NewObject<USceneComponent>(SiteActor, TEXT("SailSchoolRoot"));
	SiteRoot->SetMobility(EComponentMobility::Movable);
	SiteActor->SetRootComponent(SiteRoot);
	SiteRoot->RegisterComponent();
	SiteActor->SetActorTransform(House);
#if WITH_EDITOR
	SiteActor->SetActorLabel(TEXT("SailSchool"));
#endif
	AActor* Holder = SiteActor;

	// the layout: where every thing of the kit stands
	LyudJson::FValue J;
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("City/sailing_boathouse.json");
		FString Text;
		std::string Error;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !LyudJson::Parse(LyudSailingImpl::Std(Text), J, Error))
		{
			UE_LOG(LogLyudSailSite, Warning, TEXT("Sailing: no layout %s (%s): the boathouse stands empty"), *Path, *LyudSailingImpl::Str(Error));
		}
	}
	using LyudSailingSiteImpl::FV;

	UStaticMesh* Shell = LyudSailKit::Asset(TEXT("Boathouse"));
	if (Shell)
	{
		LyudSailingSiteImpl::Prop(Holder, Kept, Shell, FTransform::Identity, true, 0.f);
	}
	else
	{
		UE_LOG(LogLyudSailSite, Display, TEXT("Sailing: SM_Sail_Boathouse is not imported yet: a plain shed stands in for it"));
		BuildStandIn(House);
	}
	// the things inside and outside; a kit mesh not imported yet is left out (LyudSailKit::Asset says so once)
	int32 Placed = 0;
	for (const char* List : { "props", "outside", "spars", "hanging_sails" })
	{
		const FV& Arr = J[List];
		for (size_t I = 0; I < Arr.Size(); I++)
		{
			const FV& E = Arr[I];
			const FString Name = LyudSailingSiteImpl::AssetName(E["mesh"].S());
			if (Name.Contains(TEXT("SlipBoard")) || Name.IsEmpty())
			{
				continue;
			}
			UStaticMesh* Mesh = Shell ? LyudSailKit::Asset(Name) : nullptr;
			if (!Mesh)
			{
				continue;
			}
			const FV& Pos = E["pos"];
			const float Sand = LyudSailingSiteImpl::SandZ(Pos[(size_t)0].F());
			const FVector Where = LyudSailingSiteImpl::Local(Pos, Sand);
			const FQuat Rot = E["rot_deg"].Size() == 3 ? LyudSailingSiteImpl::Turned(E["rot_deg"]) : FQuat(FRotator(0.f, -E["yaw"].F(), 0.f));
			LyudSailingSiteImpl::Prop(Holder, Kept, Mesh, FTransform(Rot, Where), LyudSailingSiteImpl::Solid(Name), 30000.f);
			Placed++;
		}
	}

	// the hinged doors (ALyudDoor, by stable names: the save knows them by name)
	{
		const FV& Arr = J["doors"];
		for (size_t I = 0; I < Arr.Size() && Shell; I++)
		{
			const FV& E = Arr[I];
			const FV& Out = E["out"];
			const float YawLocal = FMath::RadiansToDegrees(FMath::Atan2(-Out[(size_t)1].F(), Out[(size_t)0].F()));
			const FTransform DoorT = FTransform(FRotator(0.f, YawLocal, 0.f), LyudSailingSiteImpl::Local(E["at"])) * House;
			FActorSpawnParameters DoorSP;
			DoorSP.Name = FName(*FString::Printf(TEXT("LyudSailDoor_%s"), *LyudSailingImpl::Str(E["name"].S())));
			DoorSP.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			DoorSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			DoorSP.bDeferConstruction = true;
			if (ALyudDoor* Door = World->SpawnActor<ALyudDoor>(ALyudDoor::StaticClass(), DoorT, DoorSP))
			{
				Door->Size = ELyudDoorSize::FitHole;
				Door->HoleWidth = E["hole"][(size_t)0].F(1.2f) * 100.f;
				Door->HoleHeight = E["hole"][(size_t)1].F(2.18f) * 100.f;
				Door->bDoubleLeaf = E["leaves"].I(1) >= 2;
				Door->WallThickness = E["wall"].F(0.2f) * 100.f;
				Door->Hinge = E["hinge"].S() == "right" ? ELyudDoorHinge::Right : ELyudDoorHinge::Left;
				Door->bBuildFrame = E["frame"].F(1.f) > 0.5f;
				Door->bStartOpen = E["open"].F(0.f) > 0.5f;
				Door->bLocked = E["locked"].F(0.f) > 0.5f;
				Door->LeafColor = FLinearColor(0.11f, 0.2f, 0.33f);
				Door->FrameColor = FLinearColor(0.82f, 0.8f, 0.74f);
				Door->FinishSpawning(DoorT);
				SiteDoors.Add(Door);
			}
		}
	}
	// the boat door's two leaves on their rail: slid apart through the school's day
	LeafClosed.Reset();
	LeafOpen.Reset();
	DoorLeaves.Reset();
	{
		const FV& Arr = J["sliding_doors"];
		for (size_t I = 0; I < Arr.Size() && Shell; I++)
		{
			UStaticMesh* Mesh = LyudSailKit::Asset(LyudSailingSiteImpl::AssetName(Arr[I]["mesh"].S()));
			const FV& Leaves = Arr[I]["leaves"];
			for (size_t K = 0; K < Leaves.Size() && Mesh; K++)
			{
				const FVector Closed = LyudSailingSiteImpl::Local(Leaves[K]["closed"]);
				LeafClosed.Add(Closed);
				LeafOpen.Add(Closed + FVector(Leaves[K]["open_dx"].F() * 100.f, 0.f, 0.f));
				DoorLeaves.Add(LyudSailingSiteImpl::Prop(Holder, Kept, Mesh, FTransform(Closed), true, 30000.f));
			}
		}
	}
	bLeavesOpen = false;

	// the lamps (no shadows; lit after dark while somebody is in)
	SiteLamps.Reset();
	{
		const FV& Arr = J["lamps"];
		for (size_t I = 0; I < Arr.Size(); I++)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(Holder, NAME_None, RF_Transient);
			L->SetMobility(EComponentMobility::Movable);
			L->SetCastShadows(false);
			L->SetIntensityUnits(ELightUnits::Lumens);
			L->SetIntensity(Arr[I]["lumens"].F(900.f));
			L->SetAttenuationRadius(650.f);
			L->SetSourceRadius(8.f);
			L->SetLightColor(FLinearColor(1.f, 0.84f, 0.62f));
			L->SetVisibility(false);
			L->SetupAttachment(SiteRoot);
			L->SetRelativeLocation(LyudSailingSiteImpl::Local(Arr[I]["pos"]) - FVector(0.f, 0.f, 12.f));
			L->RegisterComponent();
			Holder->AddInstanceComponent(L);
			SiteLamps.Add(L);
		}
	}
	bLampsOn = false;

	// the slip's boards: one instanced mesh, laid by LaySlip down to the water
	SlipBoards = nullptr;
	if (UStaticMesh* Board = LyudSailKit::Asset(TEXT("Prop_SlipBoard")))
	{
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(Holder, TEXT("SlipBoards"), RF_Transient);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetStaticMesh(Board);
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetCanEverAffectNavigation(false);
		Ism->SetCastShadow(false);
		Ism->bAffectDistanceFieldLighting = false;
		Ism->SetupAttachment(SiteRoot);
		Ism->RegisterComponent();
		Holder->AddInstanceComponent(Ism);
		SlipBoards = Ism;
	}
	S.LpShown = FSchoolV2(-1e9, 0.0);

	// the signal mast's three hoists (top, the yard's two ends), the roof staff's flag and the windsock: quads that
	// stream downwind, dressed by HoistSignals
	MastFlags.Reset();
	const FV& Points = J["flag_points"];
	const char* const Hoists[4] = { "top", "yard_port", "yard_stbd", "staff_top" };
	static const FVector Fallback[4] = { FVector(950.f, 1310.f, 990.f), FVector(950.f, 1160.f, 750.f), FVector(950.f, 1460.f, 750.f), FVector(995.f, 458.f, 1110.f) };
	for (int32 I = 0; I < 4; I++)
	{
		const FV& Pt = Points[Hoists[I]];
		const FVector Where = Pt.Size() >= 3 ? LyudSailingSiteImpl::Local(Pt) : Fallback[I];
		UStaticMeshComponent* Flag = LyudSailingSiteImpl::Prop(Holder, Kept, LyudSailKit::FlagQuad().Mesh, FTransform(Where), false, 40000.f);
		LyudSailKit::NoLumen(Flag);
		Flag->SetCastShadow(false);
		Flag->SetVisibility(false);
		MastFlags.Add(Flag);
		if (I < 3)
		{
			MastPoints[I] = Where;
		}
	}
	{
		// the sock on its pole on the roof's seaward corner (index 4 of MastFlags)
		FVector Pole(1035.f, -475.f, 804.f);
		const FV& Arr = J["outside"];
		for (size_t I = 0; I < Arr.Size(); I++)
		{
			if (Arr[I]["mesh"].S().find("WindsockPole") != std::string::npos)
			{
				Pole = LyudSailingSiteImpl::Local(Arr[I]["pos"]) + FVector(0.f, 0.f, 200.f);
			}
		}
		UStaticMesh* Sock = LyudSailKit::Asset(TEXT("Windsock"));
		UStaticMeshComponent* C = LyudSailingSiteImpl::Prop(Holder, Kept, Sock ? Sock : LyudSailKit::FlagQuad().Mesh, FTransform(Pole), false, 40000.f);
		LyudSailKit::NoLumen(C);
		C->SetCastShadow(false);
		if (!Sock)
		{
			LyudSailKit::PaintBuilt(C, LyudSailKit::FlagQuad(), FLinearColor(0.95f, 0.32f, 0.04f), FLinearColor(0.95f, 0.32f, 0.04f));
		}
		MastFlags.Add(C);
	}
	S.SignalShown = "?";
	bSiteShown = true;
	UE_LOG(LogLyudSailSite, Display, TEXT("SAIL site built at %s: %d things, %d doors, %d lamps%s%s"), *House.GetLocation().ToCompactString(), Placed, SiteDoors.Num(), SiteLamps.Num(),
		Shell ? TEXT("") : TEXT(" (stand-in shed)"), LYUD_SAIL_HAS_WINDTELLS ? TEXT(", the town's windsocks are in the build") : TEXT(""));
}

void ULyudSailing::BuildStandIn(const FTransform& House)
{
	// a boarded shed of the footprint (16.5 x 9 m, walls 3.4 m, a mono-pitch roof rising to the sea) with the north wall's
	// two doorways left open: enough to walk round and into until the kit is imported
	FLyudMeshKit K;
	const float Hx = 825.f, Hy = 450.f, Wall = 340.f, T = 12.f;
	K.Box(TEXT("Floor"), FTransform(FVector(0.f, 0.f, -8.f)), FVector(Hx, Hy, 8.f), true);
	K.Box(TEXT("Wall"), FTransform(FVector(0.f, -Hy, Wall * 0.5f)), FVector(Hx, T, Wall * 0.5f), true);            // the south wall
	K.Box(TEXT("Wall"), FTransform(FVector(-Hx, 0.f, Wall * 0.5f)), FVector(T, Hy, Wall * 0.5f), true);             // landward
	K.Box(TEXT("Wall"), FTransform(FVector(Hx, 0.f, Wall * 0.5f)), FVector(T, Hy, Wall * 0.5f), true);              // seaward
	// the north wall (local +Y) in pieces round the people's door (x -6.35..-5.15) and the boat door (x -2.45..0.75)
	const float Cuts[6] = { -Hx, -635.f, -515.f, -245.f, 75.f, Hx };
	for (int32 I = 0; I < 3; I++)
	{
		const float A = Cuts[I * 2], B = Cuts[I * 2 + 1];
		K.Box(TEXT("Wall"), FTransform(FVector((A + B) * 0.5f, Hy, Wall * 0.5f)), FVector((B - A) * 0.5f, T, Wall * 0.5f), true);
	}
	K.Box(TEXT("Wall"), FTransform(FVector(-575.f, Hy, 218.f + (Wall - 218.f) * 0.5f)), FVector(60.f, T, (Wall - 218.f) * 0.5f), true);
	K.Box(TEXT("Wall"), FTransform(FVector(-85.f, Hy, 295.f + (Wall - 295.f) * 0.5f)), FVector(160.f, T, (Wall - 295.f) * 0.5f), true);
	K.Box(TEXT("Roof"), FTransform(FRotator(7.5f, 0.f, 0.f), FVector(0.f, 0.f, Wall + 110.f)), FVector(Hx + 60.f, Hy + 60.f, 10.f), true);
	TArray<FName> Slots;
	UStaticMesh* Mesh = K.Build(GetTransientPackage(), TEXT("SM_SailBoathouseStandIn"), Slots);
	if (!Mesh || !SiteActor)
	{
		return;
	}
	Kept.Add(Mesh);
	UStaticMeshComponent* C = LyudSailingSiteImpl::Prop(SiteActor, Kept, Mesh, FTransform::Identity, true, 0.f);
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		const FLinearColor Tint = Slots[I] == TEXT("Roof") ? FLinearColor(0.2f, 0.22f, 0.25f) : (Slots[I] == TEXT("Floor") ? FLinearColor(0.45f, 0.42f, 0.38f) : FLinearColor(0.16f, 0.3f, 0.44f));
		if (UMaterialInstanceDynamic* Mat = LyudSailKit::Paint(Tint))
		{
			C->SetMaterial(I, Mat);
		}
	}
	(void)House;
}

void ULyudSailing::ShowSite(bool bShow)
{
	if (!bSiteBuilt || bSiteShown == bShow)
	{
		return;
	}
	bSiteShown = bShow;
	if (SiteActor)
	{
		SiteActor->SetActorHiddenInGame(!bShow);
		SiteActor->SetActorEnableCollision(bShow);
	}
	for (ALyudDoor* Door : SiteDoors)
	{
		if (Door)
		{
			Door->SetActorHiddenInGame(!bShow);
			Door->SetActorEnableCollision(bShow);
		}
	}
	if (!bShow && HalyardLoop)
	{
		ULyudSoundscapeSubsystem::ReleaseLoop(HalyardLoop, 0.5f);
		HalyardLoop = nullptr;
	}
	if (!bShow && bLampsOn)
	{
		bLampsOn = false;
		for (UPointLightComponent* L : SiteLamps)
		{
			if (L)
			{
				L->SetVisibility(false);
			}
		}
	}
	if (M)
	{
		M->SignalShown = "?";
	}
	ParkHulls(false);
}

// ------------------------------------------------------------------ what changes with the tide, the flags and the hour

void ULyudSailing::LaySlip()
{
	FLyudSailingState& S = *M;
	if (!SlipBoards || !SiteActor || !S.Lp.bOk)
	{
		return;
	}
	S.LpShown = S.Lp.At;
	const FTransform House = SiteActor->GetActorTransform();
	// boards from the slip's head to where the trolleys stop today: the first nineteen lie on the built slope, the rest on
	// the flat the ebb has left
	const FVector LpLocal = House.InverseTransformPosition(LyudSailingImpl::CmOf(S.Lp.At, House.GetLocation().Z));
	const int32 Want = FMath::Clamp(FMath::CeilToInt32((LpLocal.X * 0.01f - LyudSailingSiteImpl::SlipX0) / LyudSailingSiteImpl::SlipStep) + 1, LyudSailingSiteImpl::SlipFixed,
		LyudSailingSiteImpl::SlipMax);
	TArray<FTransform>& Xf = S.SlipXf;
	Xf.Reset();
	for (int32 K = 0; K < Want; K++)
	{
		const float X = LyudSailingSiteImpl::SlipX0 + LyudSailingSiteImpl::SlipStep * K;
		FVector Where(X * 100.f, -LyudSailingSiteImpl::SlipY * 100.f, LyudSailingSiteImpl::SandZ(X) * 100.f);
		if (K >= LyudSailingSiteImpl::SlipFixed)
		{
			const FVector World = House.TransformPosition(Where);
			Where.Z = GroundZ(LyudSailingImpl::MetresOf(World)) - House.GetLocation().Z + 2.f;
		}
		Xf.Add(FTransform(Where));
	}
	SlipBoards->ClearInstances();
	SlipBoards->AddInstances(Xf, false, false);
}

void ULyudSailing::HoistSignals()
{
	FLyudSailingState& S = *M;
	S.SignalShown = S.SignalState;
	if (MastFlags.Num() < 5)
	{
		return;
	}
	// "state flag flag ...": the first flag to the masthead, the next two to the yard's ends
	std::string Flags[3];
	size_t At = S.SignalState.find(' ');
	for (int32 I = 0; I < 3 && At != std::string::npos; I++)
	{
		const size_t Next = S.SignalState.find(' ', At + 1);
		Flags[I] = S.SignalState.substr(At + 1, Next == std::string::npos ? std::string::npos : Next - At - 1);
		At = Next;
	}
	for (int32 I = 0; I < 3; I++)
	{
		LyudSailingSiteImpl::DressFlag(MastFlags[I], Flags[I]);
	}
	// the school's own flag on the roof's staff from the fitting-out to the laying-up
	LyudSailingSiteImpl::DressFlag(MastFlags[3], S.Season == ESeasonState::LaidUp ? std::string() : std::string("flag_school"));
	if (ULyudSoundscapeSubsystem* Scape = SiteActor ? ULyudSoundscapeSubsystem::Get(this) : nullptr)
	{
		Scape->PlayEffect(TEXT("sail_flag_squeak"), SiteActor->GetActorTransform().TransformPosition(MastPoints[0]), 1.f);
	}
}

void ULyudSailing::SiteHours()
{
	FLyudSailingState& S = *M;
	if (!SiteActor)
	{
		return;
	}
	// flags and the sock stream downwind; a slack flag hangs, a sock droops
	const float YawWorld = FMath::RadiansToDegrees(float(std::atan2(S.WindTo.Y, S.WindTo.X)));
	const float Lift = float(SchoolClamp(S.W.Wind / 8.0, 0.0, 1.0));
	for (int32 I = 0; I < MastFlags.Num(); I++)
	{
		if (UStaticMeshComponent* C = MastFlags[I])
		{
			C->SetWorldRotation(FRotator(I == 4 ? -75.f * (1.f - Lift) : 0.f, YawWorld, 0.f));
		}
	}
	// the lamps: after dark while the school has somebody in
	const FSchoolDaylight Light = SchoolDaylight(S.P, S.Date);
	const bool bDark = S.Hour > Light.Sunset - 0.4 || S.Hour < Light.Sunrise + 0.4;
	const bool bIn = S.Season != ESeasonState::LaidUp ? (S.Hour >= 8.0 && S.Hour <= 22.0) : S.Now.bOn;
	if (bLampsOn != (bDark && bIn))
	{
		bLampsOn = bDark && bIn;
		for (UPointLightComponent* L : SiteLamps)
		{
			if (L)
			{
				L->SetVisibility(bLampsOn);
			}
		}
	}
	// the boat door stands open through the school's day
	const bool bOpen = S.Season != ESeasonState::LaidUp && S.Hour >= 9.0 && S.Hour <= Light.Sunset && !S.W.bThunder;
	if (bLeavesOpen != bOpen)
	{
		bLeavesOpen = bOpen;
		for (int32 I = 0; I < DoorLeaves.Num() && I < LeafClosed.Num(); I++)
		{
			if (UStaticMeshComponent* C = DoorLeaves[I])
			{
				C->SetRelativeLocation(bOpen ? LeafOpen[I] : LeafClosed[I]);
			}
		}
	}
	// the park's halyards: one emitter for the dozen masts, light or fresh by the wind, silent when the masts are down
	const Sound::FHalyardMix Mix = Sound::MixHalyards(float(S.W.Wind));
	const bool bMasts = S.Season == ESeasonState::Open;
	const int32 Want = !bMasts || Mix.Light + Mix.Fresh < 0.05f ? 0 : (Mix.Fresh > Mix.Light ? 2 : 1);
	if (Want != S.HalyardPool)
	{
		S.HalyardPool = Want;
		if (HalyardLoop)
		{
			ULyudSoundscapeSubsystem::ReleaseLoop(HalyardLoop, 1.5f);
			HalyardLoop = nullptr;
		}
		ULyudSoundscapeSubsystem* Scape = Want != 0 ? ULyudSoundscapeSubsystem::Get(this) : nullptr;
		if (Scape && MastFlags.Num() > 0 && MastFlags[0])
		{
			HalyardLoop = Scape->AttachLoop(Want == 2 ? TEXT("sail_halyard_chorus_fresh") : TEXT("sail_halyard_chorus_light"), MastFlags[0], 1.f);
		}
	}
	if (HalyardLoop)
	{
		HalyardLoop->SetVolumeMultiplier(FMath::Clamp(FMath::Max(Mix.Light, Mix.Fresh), 0.05f, 1.f));
	}
}

void ULyudSailing::TickSite(const FVector& Eye, float Dt)
{
	SiteIn -= Dt;
	if (SiteIn > 0.f)
	{
		return;
	}
	SiteIn = 0.5f;
	FLyudSailingState& S = *M;
	const FVector House = LyudSailingImpl::CmOf(S.P.Boathouse.At.At(), Eye.Z);
	const double DistSq = FVector::DistSquared2D(Eye, House);
	if (!bSiteBuilt)
	{
		if (DistSq > FMath::Square(40000.0))
		{
			return;
		}
		BuildSite();
		MakeFleet();
	}
	// shown within 400 m, hidden beyond 460
	if (bSiteShown ? DistSq > FMath::Square(46000.0) : DistSq < FMath::Square(40000.0))
	{
		ShowSite(!bSiteShown);
		S.HalyardPool = -1;
	}
	if (!bSiteShown)
	{
		return;
	}
	if (S.Lp.bOk && (S.Lp.At - S.LpShown).Len() > 1.0)
	{
		LaySlip();
	}
	if (S.SignalShown != S.SignalState)
	{
		HoistSignals();
	}
	SiteHours();
}
