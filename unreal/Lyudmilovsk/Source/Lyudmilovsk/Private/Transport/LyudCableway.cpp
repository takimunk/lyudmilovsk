// The cable-car metro's structures, its lifts and its cabins (LyudCableway.h), built in code from the network.
#include "Transport/LyudCableway.h"

#include "CablewaySim.h"
#include "TransportStd.h"
#include "TransportKit.h"
#include "LyudMeshKit.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LyudText.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SLyudTransitWidgets.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Transport/LyudTransport.h"
#include "Audio/LyudSoundscape.h"
#include "Doors/LyudDoor.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/ProceduralGrass.h"
#include "EngineUtils.h"

using namespace LyudTransportKit;

// --- the static structure ----------------------------------------------------------------------------------

ALyudCableway::ALyudCableway()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	// AProceduralGrass grows nothing where its ground trace meets this actor (floors, roofs, podiums)
	Tags.Add(TEXT("NoGrass"));
}

UMaterialInterface* ALyudCableway::Mat(FName Name)
{
	if (TObjectPtr<UMaterialInterface>* Found = Mats.Find(Name))
	{
		return *Found;
	}
	UMaterialInterface* Out = nullptr;
	static const TCHAR* PaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
	static const TCHAR* GlassPath = TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass");
	static const TCHAR* GlowPath = TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow");
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Name == TEXT("glass"))
	{
		UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(G ? G : Basic, this);
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.62f, 0.66f));
		Out = M;
	}
	else if (Name.ToString().StartsWith(TEXT("glow")))
	{
		UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(G ? G : Basic, this);
		M->SetVectorParameterValue(TEXT("Color"), Name == TEXT("glow_red") ? FLinearColor(1.f, 0.05f, 0.02f) : FLinearColor(1.f, 0.72f, 0.38f));
		M->SetScalarParameterValue(TEXT("Intensity"), Name == TEXT("glow_red") ? 20.f : 4.f);
		Out = M;
	}
	else if (Name.ToString().StartsWith(TEXT("tx_")))
	{
		// a textured surface (tools/transport_textures.py -> /Game/Transport/Textures, M_TR_Surface): the mesh's UVs are
		// metres, the material repeats the texture every Tile metres; no textures yet: the flat paint of its colour
		const FString Key = Name.ToString().Mid(3);
		struct FSurf { const TCHAR* Tex; FLinearColor Tint; float Tile; float Metal; float Rough; FName Paint; };
		static const TMap<FString, FSurf> Surfs = {
			{ TEXT("terrazzo"), { TEXT("terrazzo"), FLinearColor::White, 1.2f, 0.f, 1.f, TEXT("terrazzo") } },
			{ TEXT("concrete"), { TEXT("concrete"), FLinearColor::White, 2.4f, 0.f, 1.f, TEXT("concrete") } },
			{ TEXT("enamel"), { TEXT("enamel"), FLinearColor::White, 2.4f, 0.f, 1.f, TEXT("ivory") } },
			{ TEXT("plaster"), { TEXT("enamel"), FLinearColor(0.95f, 0.93f, 0.9f), 6.f, 0.f, 1.3f, TEXT("plaster") } },
			{ TEXT("amber"), { TEXT("amber"), FLinearColor::White, 1.f, 0.f, 1.f, TEXT("amber") } },
			{ TEXT("steel"), { TEXT("steel"), FLinearColor::White, 1.f, 0.9f, 0.8f, TEXT("chrome") } },
			{ TEXT("dark"), { TEXT("steel"), FLinearColor(0.18f, 0.19f, 0.21f), 1.f, 0.6f, 1.4f, TEXT("steel") } },
			{ TEXT("plate"), { TEXT("plate"), FLinearColor::White, 2.f, 0.f, 1.f, TEXT("ivory") } },
			{ TEXT("tread"), { TEXT("tread"), FLinearColor::White, 1.2f, 0.f, 1.f, TEXT("dark") } },
			{ TEXT("mosaic"), { TEXT("mosaic"), FLinearColor::White, 0.8f, 0.f, 1.f, TEXT("mosaic") } },
			{ TEXT("tactile"), { TEXT("tactile"), FLinearColor::White, 0.6f, 0.f, 1.f, TEXT("amber") } },
		};
		const FSurf* Sf = Surfs.Find(Key);
		static const TCHAR* SurfacePath = TEXT("/Game/Transport/Materials/M_TR_Surface.M_TR_Surface");
		UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, SurfacePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		auto Tex = [](const FString& T, const TCHAR* Suffix)
		{
			const FString N = FString::Printf(TEXT("T_TR_%s_%s"), *T, Suffix);
			return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Transport/Textures/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		};
		UTexture* C = Sf ? Tex(Sf->Tex, TEXT("C")) : nullptr;
		if (Master && C)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, this);
			M->SetTextureParameterValue(TEXT("BaseTex"), C);
			if (UTexture* N = Tex(Sf->Tex, TEXT("N")))
			{
				M->SetTextureParameterValue(TEXT("NormalTex"), N);
			}
			if (UTexture* R = Tex(Sf->Tex, TEXT("R")))
			{
				M->SetTextureParameterValue(TEXT("RoughTex"), R);
			}
			M->SetVectorParameterValue(TEXT("Tint"), Sf->Tint);
			M->SetScalarParameterValue(TEXT("Tile"), Sf->Tile);
			M->SetScalarParameterValue(TEXT("Metallic"), Sf->Metal);
			M->SetScalarParameterValue(TEXT("RoughMul"), Sf->Rough);
			Out = M;
		}
		else
		{
			Out = Mat(Sf ? Sf->Paint : FName(TEXT("ivory")));
		}
	}
	else
	{
		UMaterialInterface* P = LoadObject<UMaterialInterface>(nullptr, PaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(P ? P : Basic, this);
		FPaint Paint = { FLinearColor(0.7f, 0.7f, 0.7f), 0.5f, 0.f };
		if (const FPaint* Found = Palette().Find(Name))
		{
			Paint = *Found;
		}
		else if (Name.ToString().StartsWith(TEXT("#")))
		{
			Paint.Color = FLinearColor(FColor::FromHex(Name.ToString()));
			Paint.Rough = 0.4f;
		}
		M->SetVectorParameterValue(TEXT("Color"), Paint.Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Paint.Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), Paint.Metal);
		Out = M;
	}
	Mats.Add(Name, Out);
	return Out;
}

UInstancedStaticMeshComponent* ALyudCableway::IsmFor(FName Kind, FName MatName, bool bCollide)
{
	const FName Key(*FString::Printf(TEXT("%s|%s|%d"), *Kind.ToString(), *MatName.ToString(), bCollide ? 1 : 0));
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Ism.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(BasicShape(Kind));
	C->SetMaterial(0, Mat(MatName));
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCastShadow(MatName != TEXT("glass"));
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Ism.Add(Key, C);
	return C;
}

void ALyudCableway::Add(FName Kind, FName MatName, const FTransform& T, bool bCollide)
{
	IsmFor(Kind, MatName, bCollide)->AddInstance(T, true);
}

UStaticMeshComponent* ALyudCableway::Single(UStaticMesh* Mesh, const FTransform& T, bool bCollide, FName MatName)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	C->SetWorldTransform(T);
	if (!MatName.IsNone())
	{
		C->SetMaterial(0, Mat(MatName));
	}
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	C->SetWorldTransform(T);
	if (MatName.IsNone())
	{
		KitMaterials(C);
	}
	AddInstanceComponent(C);
	Singles.Add(C);
	return C;
}

void ALyudCableway::KitMaterials(UStaticMeshComponent* C)
{
	// the hi3d meshes' windows (TR_Glass*) and the rooms inside them (TR_Interior*, optimize.py): real glass, plaster
	const TArray<FName> Slots = C ? C->GetMaterialSlotNames() : TArray<FName>();
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		const FString N = Slots[I].ToString();
		if (N.StartsWith(TEXT("TR_Glass")))
		{
			C->SetMaterial(I, Mat(TEXT("glass")));
		}
		else if (N.StartsWith(TEXT("TR_Interior")))
		{
			C->SetMaterial(I, Mat(TEXT("plaster")));
		}
		else if (N.StartsWith(TEXT("TR_")))
		{
			// the cabin's inside (cabin.py): TR_Terrazzo, TR_Seat, TR_Chrome, TR_Glow -> the palette's own
			const FName Key(*N.Mid(3).ToLower());
			if (Key == TEXT("glow") || Palette().Contains(Key))
			{
				C->SetMaterial(I, Mat(Key));
			}
		}
	}
}

void ALyudCableway::Box(const FTransform& F, const FVector& C, const FVector& Size, FName MatName, bool bCollide, const FRotator& R)
{
	const FTransform Local(R, C, Size / 100.f);
	Add(TEXT("box"), MatName, Local * F, bCollide);
}

void ALyudCableway::Cyl(const FTransform& F, const FVector& Bottom, float Radius, float Height, FName MatName, bool bCollide)
{
	if (Height <= 1.f)
	{
		return;
	}
	const FTransform Local(FRotator::ZeroRotator, Bottom + FVector(0, 0, Height * 0.5f), FVector(Radius / 50.f, Radius / 50.f, Height / 100.f));
	Add(TEXT("cyl"), MatName, Local * F, bCollide);
}

void ALyudCableway::Rod(const FVector& A, const FVector& B, float Radius, FName MatName, bool bCollide)
{
	const FVector D = B - A;
	const float L = D.Size();
	if (L < 1.f)
	{
		return;
	}
	const FQuat Q = FRotationMatrix::MakeFromZ(D / L).ToQuat();
	Add(TEXT("cyl"), MatName, FTransform(Q, (A + B) * 0.5f, FVector(Radius / 50.f, Radius / 50.f, L / 100.f)), bCollide);
}

FVector ALyudCableway::W(float X, float Y, float Z) const
{
	return Transport ? Transport->MapToWorld(X, Y, Z) : FVector(X * 100.f, Y * 100.f, Z * 100.f);
}

void ALyudCableway::Build(ULyudTransportSubsystem* InOwner, const LyudTransport::FNetwork& Net)
{
	Transport = InOwner;
	Built.SetNum(SimNum(Net.Stations));
	StationCentres.SetNum(SimNum(Net.Stations));
	StationRadius.SetNum(SimNum(Net.Stations));
	for (int32 S = 0; S < SimNum(Net.Stations); S++)
	{
		BuildStation(Net, S);
	}
	for (int32 L = 0; L < SimNum(Net.Lines); L++)
	{
		BuildPylons(Net, L);
		BuildRopes(Net, L);
		BuildDepot(Net, L);
	}
	BuildClubBoard(Net);
	// grass scattered before the stations stood: again, so none grows through their floors (NoGrass tag)
	for (TActorIterator<AProceduralGrass> It(GetWorld()); It; ++It)
	{
		for (const FVector& C : StationCentres)
		{
			if (FVector::Dist2D(It->GetActorLocation(), C) < 15000.f)
			{
				It->Rebuild();
				break;
			}
		}
	}
	int32 N = 0;
	for (const auto& P : Ism)
	{
		N += P.Value->GetInstanceCount();
	}
	UE_LOG(LogTemp, Log, TEXT("Transport: built %d stations, %d lines, %d instances in %d groups, %d meshes"), SimNum(Net.Stations), SimNum(Net.Lines), N, Ism.Num(), Singles.Num());
}

namespace
{
	FTransform RodTransform(const FVector& A, const FVector& B, float Radius)
	{
		const FVector D = B - A;
		const float L = FMath::Max(1.f, D.Size());
		return FTransform(FRotationMatrix::MakeFromZ(D / L).ToQuat(), (A + B) * 0.5f, FVector(Radius / 50.f, Radius / 50.f, L / 100.f));
	}
}

void ALyudCableway::BuildRopes(const LyudTransport::FNetwork& Net, int32 L)
{
	const LyudTransport::FLine& Ln = Net.Lines[L];
	// its own movable instances: the spans near the viewer move with the network's rope motion
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(BasicShape(TEXT("cyl")));
	C->SetMaterial(0, Mat(TEXT("rope")));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	if (RopeIsm.Num() <= L)
	{
		RopeIsm.SetNum(L + 1);
		RopeMoved.SetNum(L + 1);
	}
	RopeIsm[L] = C;
	TArray<FTransform> Ts;
	for (int32 I = 0; I + 1 < SimNum(Ln.Loop); I++)
	{
		const LyudTransport::V3& A = Ln.Loop[I];
		const LyudTransport::V3& B = Ln.Loop[I + 1];
		// thicker than a real 5 cm rope so the line reads from afar
		Ts.Add(RodTransform(W(A.X, A.Y, A.Z), W(B.X, B.Y, B.Z), 4.5f));
	}
	C->AddInstances(Ts, false, true);
	RopeMoved[L].Init(false, Ts.Num());
}

void ALyudCableway::UpdateRopes(const LyudTransport::FNetwork& Net, const FVector& Viewer)
{
	for (int32 L = 0; L < RopeIsm.Num() && L < SimNum(Net.Lines); L++)
	{
		UInstancedStaticMeshComponent* C = RopeIsm[L];
		const LyudTransport::FLine& Ln = Net.Lines[L];
		if (!C)
		{
			continue;
		}
		bool bAny = false;
		auto Point = [&](int32 I, bool bMove)
		{
			LyudTransport::V3 P = Ln.Loop[I];
			if (bMove)
			{
				P = P + Net.RopeOffset(L, Ln.LoopS[I]);
			}
			return W(P.X, P.Y, P.Z);
		};
		for (int32 I = 0; I + 1 < SimNum(Ln.Loop); I++)
		{
			const FVector A0 = W(Ln.Loop[I].X, Ln.Loop[I].Y, Ln.Loop[I].Z);
			// only within ~900 m: farther the motion is under a pixel
			const bool bNear = FVector::DistSquared(A0, Viewer) < FMath::Square(90000.f);
			if (!bNear && !RopeMoved[L][I])
			{
				continue;
			}
			C->UpdateInstanceTransform(I, RodTransform(Point(I, bNear), Point(I + 1, bNear), 4.5f), true, false, true);
			RopeMoved[L][I] = bNear;
			bAny = true;
		}
		if (bAny)
		{
			C->MarkRenderStateDirty();
		}
	}
}

void ALyudCableway::BuildClubBoard(const LyudTransport::FNetwork& Net)
{
	if (!Net.bHasClubBoard)
	{
		return;
	}
	const FVector At = W(Net.ClubBoard.X, Net.ClubBoard.Y, Net.ClubBoard.Z);
	const FTransform F(FRotator(0.f, Net.ClubBoardYaw, 0.f), At);
	// two ivory posts, an amber-framed board under a little canopy
	for (const float Y : { -170.f, 170.f })
	{
		Cyl(F, FVector(0.f, Y, -30.f), 7.f, 300.f, TEXT("ivory"));
	}
	Box(F, FVector(-4.f, 0.f, 175.f), FVector(6.f, 336.f, 196.f), TEXT("amber"));
	Box(F, FVector(0.f, 0.f, 285.f), FVector(60.f, 380.f, 8.f), TEXT("ivory"), false, FRotator(-8.f, 0.f, 0.f));
	UWidgetComponent* Board = NewObject<UWidgetComponent>(this);
	Board->SetupAttachment(RootComponent);
	Board->SetWidgetSpace(EWidgetSpace::World);
	Board->SetDrawSize(FVector2D(1600.f, 900.f));
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Board->SetBlendMode(EWidgetBlendMode::Opaque);
	Board->RegisterComponent();
	Board->SetWorldLocationAndRotation(F.TransformPosition(FVector(0.5f, 0.f, 175.f)), F.Rotator());
	Board->SetWorldScale3D(FVector(0.2f));
	Board->SetSlateWidget(SNew(SLyudTransitDiagram).bLive(true).bBoard(true));
	AddInstanceComponent(Board);
	ClubBoard = Board;
	ClubBoardAt = F;
}

void ALyudCableway::UpdateBoards(const LyudTransport::FNetwork& Net, const FVector& Viewer, float DeltaTime)
{
	for (int32 S = 0; S < Built.Num(); S++)
	{
		FLyudBuiltStation& B = Built[S];
		const bool bNear = FVector::Dist(Viewer, StationCentres[S]) < 25000.f;
		B.BoardTimer -= DeltaTime;
		for (UWidgetComponent* W : B.Boards)
		{
			if (!W)
			{
				continue;
			}
			W->SetVisibility(bNear);
			W->SetTickMode(bNear ? ETickMode::Enabled : ETickMode::Disabled);
		}
	}
	if (ClubBoard)
	{
		const bool bNear = FVector::Dist(Viewer, ClubBoardAt.GetLocation()) < 6000.f;
		ClubBoard->SetVisibility(bNear);
		ClubBoard->SetTickMode(bNear ? ETickMode::Enabled : ETickMode::Disabled);
	}
}

FTransform ALyudCableway::WaitSpot(int32 Station, int32 Line, int32 K) const
{
	if (!Built.IsValidIndex(Station))
	{
		return FTransform::Identity;
	}
	const FLyudBuiltStation& B = Built[Station];
	int32 Lv = B.DeckLines.Find(Line);
	Lv = Lv == INDEX_NONE ? 0 : Lv;
	if (!B.Decks.IsValidIndex(Lv) || !B.Panels.IsValidIndex(Lv))
	{
		return FTransform::Identity;
	}
	const FTransform& F = B.Decks[Lv];
	const FVector Panel = F.InverseTransformPosition(B.Panels[Lv].GetLocation());
	// along the platform's two edges by the stop, facing the tracks
	const float Side = (K % 2) ? 1.f : -1.f;
	const float X = Panel.X + ((K / 2) % 2 ? -1.f : 1.f) * (90.f + 70.f * (K / 4));
	const FVector P = F.TransformPosition(FVector(X, Side * (IslandHalf - 110.f), 0.f));
	return FTransform(FRotator(0.f, F.Rotator().Yaw + Side * 90.f, 0.f), P);
}

int32 ALyudCableway::PanelNear(int32 Station, const FVector& P, float R) const
{
	if (!Built.IsValidIndex(Station))
	{
		return -1;
	}
	for (int32 I = 0; I < Built[Station].Panels.Num(); I++)
	{
		if (FVector::Dist(Built[Station].Panels[I].GetLocation() + FVector(0, 0, 100.f), P) < R)
		{
			return I;
		}
	}
	return -1;
}

int32 ALyudCableway::StationAt(const FVector& P) const
{
	for (int32 S = 0; S < StationCentres.Num(); S++)
	{
		if (FVector::Dist2D(P, StationCentres[S]) < StationRadius[S])
		{
			return S;
		}
	}
	return -1;
}

// --- the lift ----------------------------------------------------------------------------------------------

// --- the cabin ---------------------------------------------------------------------------------------------

ALyudCabin::ALyudCabin()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Root);
	Shell->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DoorLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
	DoorLeaf->SetupAttachment(Root);
	Inside = CreateDefaultSubobject<UBoxComponent>(TEXT("Inside"));
	Inside->SetupAttachment(Root);
	Inside->SetBoxExtent(FVector(Length * 0.5f - 25.f, Width * 0.5f - 25.f, 105.f));
	Inside->SetRelativeLocation(FVector(0, 0, 105.f));
	Inside->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Lamp = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Root);
	Lamp->SetRelativeLocation(FVector(0, 0, 195.f));
	Lamp->SetIntensity(5000.f);
	Lamp->SetAttenuationRadius(450.f);
	Lamp->SetLightColor(FLinearColor(1.f, 0.78f, 0.5f));
	Lamp->SetCastShadows(false);
	Board = CreateDefaultSubobject<UWidgetComponent>(TEXT("Board"));
	Board->SetupAttachment(Root);
}

void ALyudCabin::Setup(ALyudCableway* Way, int32 InIndex, int32 Number, const FText& Name, const FLinearColor& LineColor)
{
	Index = InIndex;
	const FCabinKit& R = Kit().Cabin;
	UStaticMesh* ShellMesh = LoadMesh(TEXT("SM_TR_Cabin"));
	UStaticMesh* InsideMesh = R.bRoom ? LoadMesh(TEXT("SM_TR_CabinInside")) : nullptr;
	bRoom = ShellMesh && InsideMesh;
	if (!bRoom)
	{
		SetupBoxes(Way, InIndex, Number, Name, LineColor);
	}
	else
	{
		// the hollow cabin (blender/transport/cabin.py): every part has its origin on the floor's centre
		Shell->SetStaticMesh(ShellMesh);
		Shell->SetRelativeLocation(FVector::ZeroVector);
		Way->KitMaterials(Shell);
		auto Mesh = [&](const TCHAR* MeshName)
		{
			UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
			P->SetMobility(EComponentMobility::Movable);
			P->SetStaticMesh(LoadMesh(MeshName));
			P->SetupAttachment(Root);
			P->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			P->RegisterComponent();
			AddInstanceComponent(P);
			Way->KitMaterials(P);
			return P;
		};
		InsideParts = Mesh(TEXT("SM_TR_CabinInside"));
		InsideParts->SetCastShadow(false);
		Hanger = Mesh(TEXT("SM_TR_CabinHanger"));
		DoorLeaf->SetStaticMesh(LoadMesh(TEXT("SM_TR_CabinDoor")));
		DoorLeaf->SetRelativeScale3D(FVector::OneVector);
		DoorLeaf->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		DoorLeaf->SetMobility(EComponentMobility::Movable);
		Way->KitMaterials(DoorLeaf);
		// what the player and people bump into: the floor, the walls round the room, the benches (kit.json collision)
		for (const TPair<FVector, FVector>& B : R.Collision)
		{
			UBoxComponent* Wall = NewObject<UBoxComponent>(this);
			Wall->SetupAttachment(Root);
			Wall->SetRelativeLocation(B.Key);
			Wall->SetBoxExtent(B.Value * 0.5f);
			Wall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Wall->SetCanEverAffectNavigation(false);
			Wall->RegisterComponent();
			AddInstanceComponent(Wall);
		}
		// the shut door: one box over the opening, out of the way once it's open
		Gate = NewObject<UBoxComponent>(this);
		Gate->SetupAttachment(Root);
		Gate->SetRelativeLocation(FVector(R.DoorX, -R.WallY - 5.f, R.DoorH * 0.5f));
		Gate->SetBoxExtent(FVector(R.DoorW * 0.5f, 5.f, R.DoorH * 0.5f));
		Gate->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Gate->SetCanEverAffectNavigation(false);
		Gate->RegisterComponent();
		AddInstanceComponent(Gate);
		Lamp->SetRelativeLocation(R.Lamp);
		Inside->SetBoxExtent(FVector(R.EndX, R.WallY, 105.f));
	}
	Drive(GetActorLocation() + FVector(0, 0, Kit().CabinGrip - Kit().CabinFloor), FVector::ForwardVector, 0.f, 0.f);

	// the board over the door inside: where it goes
	const float HW = Width * 0.5f - 20.f, Top = 205.f;
	Board->SetWidgetSpace(EWidgetSpace::World);
	Board->SetDrawSize(FVector2D(600.f, 150.f));
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Board->SetBlendMode(EWidgetBlendMode::Opaque);
	Board->SetRelativeLocation(bRoom ? R.Board + FVector(0, 6.f, 0) : FVector(0.f, -HW + 6.f, Top - 25.f));
	Board->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Board->SetRelativeScale3D(FVector(0.13f));
	Board->SetSlateWidget(SNew(SLyudCabinBoard).Cabin(InIndex));
	// the number and the name, painted on both sides under the windows
	for (const float S : { -1.f, 1.f })
	{
		UWidgetComponent* P = NewObject<UWidgetComponent>(this);
		P->SetupAttachment(Root);
		P->SetWidgetSpace(EWidgetSpace::World);
		P->SetDrawSize(FVector2D(560.f, 160.f));
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetBlendMode(EWidgetBlendMode::Transparent);
		P->RegisterComponent();
		const FVector At = bRoom ? FVector(S > 0 ? -R.Plate.X : R.Plate.X, S * FMath::Abs(R.Plate.Y), R.Plate.Z)
			: FVector(S > 0 ? 0.f : 110.f, S * (Width * 0.5f + 4.f), 48.f);
		P->SetRelativeLocation(At);
		P->SetRelativeRotation(FRotator(0.f, S > 0 ? 90.f : -90.f, 0.f));
		P->SetRelativeScale3D(FVector(0.16f));
		P->SetSlateWidget(SNew(SLyudCabinPlate).Number(Number).Name(Name).Color(LineColor));
		AddInstanceComponent(P);
		Plates.Add(P);
	}
}

void ALyudCabin::SetupBoxes(ALyudCableway* Way, int32 InIndex, int32 Number, const FText& Name, const FLinearColor& LineColor)
{
	// no hollow cabin in the kit (an old kit.json): the hi3d shell round a room of boxes
	const FKit& K = Kit();
	UStaticMesh* Cube = BasicShape(TEXT("box"));
	if (UStaticMesh* M = LoadMesh(TEXT("SM_TR_Cabin")))
	{
		Shell->SetStaticMesh(M);
		Shell->SetRelativeLocation(FVector(0, 0, -K.CabinFloor));
		Way->KitMaterials(Shell);
	}
	else
	{
		// no kit: an ivory capsule of boxes
		Shell->SetStaticMesh(Cube);
		Shell->SetRelativeScale3D(FVector(Length, Width, 230.f) / 100.f);
		Shell->SetRelativeLocation(FVector(0, 0, 105.f));
		Shell->SetMaterial(0, Way->Mat(TEXT("ivory")));
		Shell->SetVisibility(false);
	}
	auto Part = [&](const FVector& C, const FVector& Size, FName Mat, bool bCollide, bool bVisible = true)
	{
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
		P->SetMobility(EComponentMobility::Movable);
		P->SetStaticMesh(Cube);
		P->SetMaterial(0, Way->Mat(Mat));
		P->SetupAttachment(Root);
		P->SetRelativeLocation(C);
		P->SetRelativeScale3D(Size / 100.f);
		P->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		P->SetVisibility(bVisible);
		P->SetCastShadow(false);
		P->RegisterComponent();
		AddInstanceComponent(P);
		return P;
	};
	const float HL = Length * 0.5f - 30.f, HW = Width * 0.5f - 20.f, Top = 205.f;
	// the floor the player stands on, the walls they can't pass (the shell shows them), the roof
	Part(FVector(0, 0, -5.f), FVector(HL * 2.f, HW * 2.f, 10.f), TEXT("terrazzo"), true);
	Part(FVector(HL, 0, Top * 0.5f), FVector(8.f, HW * 2.f, Top), TEXT("ivory"), true, false);
	Part(FVector(-HL, 0, Top * 0.5f), FVector(8.f, HW * 2.f, Top), TEXT("ivory"), true, false);
	Part(FVector(0, HW, Top * 0.5f), FVector(HL * 2.f, 8.f, Top), TEXT("ivory"), true, false);
	for (const float S : { -1.f, 1.f })
	{
		Part(FVector(S * (HL + 50.f) * 0.5f, -HW, Top * 0.5f), FVector(HL - 50.f, 8.f, Top), TEXT("ivory"), true, false);
	}
	Part(FVector(0, 0, Top + 4.f), FVector(HL * 2.f, HW * 2.f, 8.f), TEXT("ivory"), true);
	// inside: the lower walls under the glass band, two benches facing each other, corner posts, a lamp
	Part(FVector(HL - 2.f, 0, 45.f), FVector(4.f, HW * 2.f, 90.f), TEXT("ivory"), false);
	Part(FVector(-HL + 2.f, 0, 45.f), FVector(4.f, HW * 2.f, 90.f), TEXT("ivory"), false);
	Part(FVector(0, HW - 2.f, 45.f), FVector(HL * 2.f, 4.f, 90.f), TEXT("ivory"), false);
	Part(FVector(0, HW - 4.f, 92.f), FVector(HL * 2.f, 6.f, 6.f), TEXT("amber"), false);
	for (const float S : { -1.f, 1.f })
	{
		Part(FVector(S * (HL - 28.f), 0, 22.f), FVector(52.f, HW * 2.f - 10.f, 44.f), TEXT("seat"), true);
		Part(FVector(S * (HL - 5.f), 0, 70.f), FVector(10.f, HW * 2.f - 10.f, 55.f), TEXT("seat"), false);
		for (const float T : { -1.f, 1.f })
		{
			Part(FVector(S * (HL - 3.f), T * (HW - 3.f), Top * 0.5f), FVector(5.f, 5.f, Top), TEXT("chrome"), false);
		}
	}
	Part(FVector(0, 0, Top - 3.f), FVector(60.f, 20.f, 4.f), TEXT("glow"), false);
	// the sliding door: shut over the opening on the left (-Y), slides along the side when open
	DoorLeaf->SetStaticMesh(Cube);
	DoorLeaf->SetMaterial(0, Way->Mat(TEXT("ivory")));
	DoorLeaf->SetRelativeScale3D(FVector(0.86f, 0.05f, Top / 100.f));
	DoorLeaf->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	DoorLeaf->SetMobility(EComponentMobility::Movable);
}

void ALyudCabin::Drive(const FVector& Grip, const FVector& Dir, float InDoor, float DeltaTime, float PitchDeg, float RollDeg)
{
	const FKit& K = Kit();
	const float Over = K.CabinGrip - K.CabinFloor;
	const float TargetYaw = FVector(Dir.X, Dir.Y, 0.f).IsNearlyZero() ? GetActorRotation().Yaw : Dir.Rotation().Yaw;
	float Yaw = TargetYaw;
	if (DeltaTime > 0.f)
	{
		// round the corners smoothly (the grip turns on the sheaves, the cabin swings after it)
		Yaw = FMath::FixedTurn(GetActorRotation().Yaw, TargetYaw, 120.f * DeltaTime);
	}
	// hanging from the grip: swung by the pitch (along) and the roll (across) round it
	const FRotator R(PitchDeg, Yaw, RollDeg);
	SetActorLocationAndRotation(Grip - R.RotateVector(FVector(0, 0, Over)), R);
	Door = InDoor;
	if (bRoom)
	{
		// the plug door: out first, then along the side
		const FCabinKit& Rk = Kit().Cabin;
		const float OutT = FMath::Clamp(Door / 0.25f, 0.f, 1.f), SlideT = FMath::Clamp((Door - 0.2f) / 0.8f, 0.f, 1.f);
		DoorLeaf->SetRelativeLocation(FVector(Rk.Slide * FMath::SmoothStep(0.f, 1.f, SlideT), -Rk.Out * OutT, 0.f));
		Gate->SetCollisionEnabled(Door > 0.85f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		return;
	}
	const float HW = Width * 0.5f - 20.f;
	DoorLeaf->SetRelativeLocation(FVector(Door * 92.f, -HW - 6.f, 102.5f));
}

void ALyudCabin::SetBoard(const FText& Line1, const FText& Line2)
{
	BoardA = Line1;
	BoardB = Line2;
}

float ALyudCabin::GripOverFloor()
{
	return Kit().CabinGrip - Kit().CabinFloor;
}

bool ALyudCabin::Contains(const FVector& P) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(P);
	if (bRoom)
	{
		const FCabinKit& R = Kit().Cabin;
		return FMath::Abs(L.X) < R.EndX && FMath::Abs(L.Y) < R.WallY + 5.f && L.Z > -40.f && L.Z < 240.f;
	}
	return FMath::Abs(L.X) < Length * 0.5f - 20.f && FMath::Abs(L.Y) < Width * 0.5f - 10.f && L.Z > -40.f && L.Z < 240.f;
}

FTransform ALyudCabin::Seat(int32 K) const
{
	const FCabinKit& R = Kit().Cabin;
	if (bRoom && R.Seats.Num() > 0)
	{
		return R.Seats[K % R.Seats.Num()] * GetActorTransform();
	}
	const float HL = Length * 0.5f - 30.f;
	const float S = K < 2 ? 1.f : -1.f;
	const float Y = (K % 2 ? 1.f : -1.f) * 45.f;
	// the floor under the seat; facing the other bench
	return FTransform(FRotator(0.f, S > 0 ? 180.f : 0.f, 0.f), FVector(S * (HL - 28.f), Y, 0.f)) * GetActorTransform();
}

FVector ALyudCabin::DoorPoint() const
{
	if (bRoom)
	{
		const FCabinKit& R = Kit().Cabin;
		return GetActorTransform().TransformPosition(FVector(R.DoorX, -R.SillY, 0.f));
	}
	return GetActorTransform().TransformPosition(FVector(0.f, -Width * 0.5f, 0.f));
}
