// The cable-car metro's structures, its lifts and its cabins (LyudCableway.h), built in code from the network.
#include "Transport/LyudCableway.h"

#include "CablewaySim.h"
#include "TransportStd.h"
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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SLyudTransitWidgets.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Transport/LyudTransport.h"
#include "Audio/LyudSoundscape.h"

namespace
{
	// sizes in cm, the network's (tools/transport.py) and the kit's (blender/transport/optimize.py)
	const float IslandHalf = 330.f;        // the platform between the two tracks (they are track_gap = 900 apart)
	const float IslandLen = 1350.f;        // from the station's centre to each end
	const float DeckThick = 40.f;
	const float CrownOver = 460.f;         // the crown's slab over the deck (the rope runs at 365)
	const float ShaftHalf = 120.f;         // the lift shaft
	const float StairW = 120.f;            // the stair round it
	const float RingHalf = ShaftHalf + StairW;
	const float RisePerSide = 125.f;
	const int32 StepsPerSide = 8;
	const float LevelLiftMin = 500.f;      // no lift under 5 m (Aton: a stair only)

	struct FKit
	{
		float CabinGrip = 386.3f, CabinFloor = 22.f;
		float PylonFeet = 398.5f, PylonShaft = 2207.3f, PylonHeadSaddle = 86.9f;
		float CornerSaddle = 0.8f;
		bool bLoaded = false;
	};

	FKit& Kit()
	{
		static FKit K;
		if (!K.bLoaded)
		{
			K.bLoaded = true;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/kit.json"))))
			{
				TSharedPtr<FJsonObject> J;
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())
				{
					if (const TSharedPtr<FJsonObject>* C; J->TryGetObjectField(TEXT("cabin"), C))
					{
						K.CabinGrip = (*C)->GetNumberField(TEXT("grip")) * 100.f;
						K.CabinFloor = (*C)->GetNumberField(TEXT("floor")) * 100.f;
					}
					if (const TSharedPtr<FJsonObject>* P; J->TryGetObjectField(TEXT("pylon_line"), P))
					{
						K.PylonFeet = (*P)->GetNumberField(TEXT("feet")) * 100.f;
						K.PylonShaft = (*P)->GetNumberField(TEXT("shaft")) * 100.f;
						K.PylonHeadSaddle = (*P)->GetNumberField(TEXT("saddle_over_head_base")) * 100.f;
					}
					if (const TSharedPtr<FJsonObject>* P; J->TryGetObjectField(TEXT("pylon_corner"), P))
					{
						K.CornerSaddle = (*P)->GetNumberField(TEXT("saddle"));
					}
				}
			}
		}
		return K;
	}

	UStaticMesh* LoadMesh(const FString& Name)
	{
		// Interchange names the asset after the glTF mesh; some versions prefix it again
		for (const FString& N : { Name, TEXT("SM_") + Name })
		{
			const FString Path = FString::Printf(TEXT("/Game/Transport/Meshes/transport_kit/StaticMeshes/%s.%s"), *N, *N);
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return M;
			}
		}
		return nullptr;
	}

	UStaticMesh* BasicShape(FName Kind)
	{
		const TCHAR* Path = Kind == TEXT("cyl") ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube");
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	struct FPaint { FLinearColor Color; float Rough; float Metal; };
	const TMap<FName, FPaint>& Palette()
	{
		static const TMap<FName, FPaint> P = {
			{ TEXT("ivory"), { FLinearColor(0.80f, 0.76f, 0.66f), 0.45f, 0.f } },
			{ TEXT("amber"), { FLinearColor(0.86f, 0.42f, 0.08f), 0.35f, 0.f } },
			{ TEXT("concrete"), { FLinearColor(0.50f, 0.48f, 0.45f), 0.9f, 0.f } },
			{ TEXT("terrazzo"), { FLinearColor(0.60f, 0.56f, 0.50f), 0.45f, 0.f } },
			{ TEXT("steel"), { FLinearColor(0.32f, 0.33f, 0.35f), 0.4f, 0.8f } },
			{ TEXT("chrome"), { FLinearColor(0.80f, 0.80f, 0.82f), 0.15f, 1.f } },
			{ TEXT("rope"), { FLinearColor(0.10f, 0.10f, 0.11f), 0.5f, 0.7f } },
			{ TEXT("dark"), { FLinearColor(0.05f, 0.045f, 0.04f), 0.7f, 0.f } },
			{ TEXT("seat"), { FLinearColor(0.62f, 0.26f, 0.06f), 0.5f, 0.f } },
			{ TEXT("mosaic"), { FLinearColor(0.15f, 0.36f, 0.42f), 0.5f, 0.f } },
		};
		return P;
	}
}

// --- the static structure ----------------------------------------------------------------------------------

ALyudCableway::ALyudCableway()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
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
	AddInstanceComponent(C);
	Singles.Add(C);
	return C;
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
	int32 N = 0;
	for (const auto& P : Ism)
	{
		N += P.Value->GetInstanceCount();
	}
	UE_LOG(LogTemp, Log, TEXT("Transport: built %d stations, %d lines, %d instances in %d groups, %d meshes"), SimNum(Net.Stations), SimNum(Net.Lines), N, Ism.Num(), Singles.Num());
}

void ALyudCableway::BuildStation(const LyudTransport::FNetwork& Net, int32 S)
{
	const LyudTransport::FStation& St = Net.Stations[S];
	FLyudBuiltStation& B = Built[S];
	const FVector Centre = W(St.X, St.Y, 0.f);
	StationCentres[S] = Centre;
	StationRadius[S] = IslandLen + 600.f;
	const FVector CoreW = W(St.CoreX, St.CoreY, St.CoreGround);
	const float TopDeck = SimNum(St.Levels) ? St.Levels.back().Z * 100.f : St.Deck * 100.f;
	const float CoreGround = St.CoreGround * 100.f;
	B.Entrance = CoreW;

	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const float Yaw = SimValid(St.AxisYaw, L) ? St.AxisYaw[L] : 0.f;
		const float DeckZ = St.Levels[K].Z * 100.f;
		const FTransform F(FRotator(0.f, Yaw, 0.f), FVector(Centre.X, Centre.Y, DeckZ));
		B.Decks.Add(F);
		B.DeckLines.Add(L);
		const int32 End = SimValid(St.Ends, L) ? St.Ends[L] : 0;
		const float A0 = End == 1 ? 0.f : -IslandLen, A1 = End == -1 ? 0.f : IslandLen;
		const float Stop = End == 0 ? 550.f : (End == 1 ? 250.f : -250.f);
		// the core (stair ring and lift) in this deck's frame
		const FVector CoreL = F.InverseTransformPosition(FVector(CoreW.X, CoreW.Y, DeckZ));
		const float C0 = CoreL.X - RingHalf, C1 = CoreL.X + RingHalf;
		const bool bTop = K == SimNum(St.Levels) - 1;

		// the island round the core's hole
		auto Slab = [&](float X0, float X1, float Y0, float Y1)
		{
			if (X1 - X0 > 1.f && Y1 - Y0 > 1.f)
			{
				Box(F, FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, -DeckThick * 0.5f), FVector(X1 - X0, Y1 - Y0, DeckThick), TEXT("terrazzo"));
			}
		};
		Slab(A0, C0, -IslandHalf, IslandHalf);
		Slab(C1, A1, -IslandHalf, IslandHalf);
		Slab(C0, C1, RingHalf, IslandHalf);
		Slab(C0, C1, -IslandHalf, -RingHalf);
		// an ivory edge band with the amber stripe under it (the station's belt, as on the cabins)
		for (const float Side : { -1.f, 1.f })
		{
			Box(F, FVector((A0 + A1) * 0.5f, Side * (IslandHalf - 5.f), -DeckThick - 25.f), FVector(A1 - A0, 10.f, 50.f), TEXT("ivory"));
			Box(F, FVector((A0 + A1) * 0.5f, Side * (IslandHalf - 2.f), -DeckThick + 2.f), FVector(A1 - A0, 6.f, 14.f), TEXT("amber"), false);
			// the safety line
			Box(F, FVector((A0 + A1) * 0.5f, Side * (IslandHalf - 40.f), 0.5f), FVector(A1 - A0, 12.f, 1.f), TEXT("amber"), false);
			// railings along the edge but where the cabins stop
			for (const FVector2D& Run : { FVector2D(A0, Stop - 260.f), FVector2D(Stop + 260.f, A1) })
			{
				if (Run.Y - Run.X < 50.f)
				{
					continue;
				}
				Box(F, FVector((Run.X + Run.Y) * 0.5f, Side * (IslandHalf - 6.f), 55.f), FVector(Run.Y - Run.X, 4.f, 110.f), TEXT("glass"));
				Box(F, FVector((Run.X + Run.Y) * 0.5f, Side * (IslandHalf - 6.f), 110.f), FVector(Run.Y - Run.X, 8.f, 6.f), TEXT("chrome"), false);
			}
		}
		// the ends of the island: railings
		for (const float X : { A0, A1 })
		{
			if (FMath::Abs(X) > 1.f)
			{
				Box(F, FVector(X - FMath::Sign(X) * 6.f, 0.f, 55.f), FVector(4.f, IslandHalf * 2.f, 110.f), TEXT("glass"));
			}
		}
		// the hole round the stair: railings, open at the corner where the stair comes up to this deck
		{
			const float Rise = (DeckZ - CoreGround);
			const int32 Corner = FMath::RoundToInt(Rise / RisePerSide) % 4;
			// the ring's corners in this deck's frame (the ring is built in the first level's frame)
			const FTransform CoreF = FTransform(FRotator(0.f, SimValid(St.AxisYaw, St.Levels[0].Line) ? St.AxisYaw[St.Levels[0].Line] : 0.f, 0.f), CoreW);
			const FVector Open = F.InverseTransformPosition(CoreF.TransformPosition(
				FVector(Corner == 1 || Corner == 2 ? -1.f : 1.f, Corner >= 2 ? -1.f : 1.f, 0.f) * (ShaftHalf + StairW * 0.5f)) + FVector(0, 0, DeckZ - CoreGround));
			for (int32 E = 0; E < 4; E++)
			{
				const float Ang = E * 90.f;
				const FVector Dir = FRotator(0.f, Ang, 0.f).Vector();
				const FVector Side = FRotator(0.f, Ang + 90.f, 0.f).Vector();
				for (int32 Half = -1; Half <= 1; Half += 2)
				{
					const FVector Mid = FVector(CoreL.X, CoreL.Y, 0.f) + Dir * RingHalf + Side * (Half * RingHalf * 0.5f);
					if (FVector::Dist2D(Mid, FVector(Open.X, Open.Y, 0.f)) < RingHalf * 0.8f)
					{
						continue;
					}
					Box(F, Mid + FVector(0, 0, 55.f), FVector(4.f, RingHalf, 110.f), TEXT("glass"), true, FRotator(0.f, Ang, 0.f));
				}
			}
		}
		// columns from the ground under the island's ends
		for (const float X : { A0 + 200.f, A1 - 200.f })
		{
			if (FMath::Abs(X - CoreL.X) < RingHalf + 150.f)
			{
				continue;
			}
			const FVector P = F.TransformPosition(FVector(X, 0.f, 0.f));
			const float G = St.Ground * 100.f - 200.f;
			Cyl(FTransform::Identity, FVector(P.X, P.Y, G), 70.f, P.Z - DeckThick - G, TEXT("ivory"));
			Cyl(FTransform::Identity, FVector(P.X, P.Y, P.Z - DeckThick - 120.f), 74.f, 30.f, TEXT("amber"), false);
		}
		// the roof: the crown over the top level, on four columns; a plain canopy under another level
		if (bTop)
		{
			for (const FVector2D& C : { FVector2D(-600.f, 260.f), FVector2D(-600.f, -260.f), FVector2D(600.f, 260.f), FVector2D(600.f, -260.f) })
			{
				if (C.X < A0 || C.X > A1)
				{
					continue;
				}
				Cyl(F, FVector(C.X, C.Y, 0.f), 22.f, CrownOver, TEXT("ivory"));
			}
			const FString Style = St.Style.empty() ? TEXT("Saucer") : FString(UTF8_TO_TCHAR(St.Style.c_str()));
			const FString MeshName = TEXT("SM_TR_Top_") + Style.Left(1).ToUpper() + Style.Mid(1);
			const FTransform CrownT = FTransform(FVector((A0 + A1) * 0.5f, 0.f, CrownOver)) * F;
			if (UStaticMesh* Crown = LoadMesh(MeshName))
			{
				UStaticMeshComponent* C = Single(Crown, CrownT, false);
				C->SetCastShadow(true);
			}
			else
			{
				Cyl(CrownT, FVector::ZeroVector, 1000.f, 40.f, TEXT("ivory"), false);
				Cyl(CrownT, FVector(0, 0, 40.f), 1010.f, 12.f, TEXT("amber"), false);
			}
		}
		// end station: the turn wheel at rope height
		if (End != 0)
		{
			Cyl(F, FVector(0.f, 0.f, 350.f), 470.f, 22.f, TEXT("steel"), false);
			Cyl(F, FVector(0.f, 0.f, 372.f), 120.f, CrownOver - 372.f, TEXT("steel"), false);
		}
		// the call panel in the middle of the platform at the stop, its face toward the stair; the board over it
		const float Face = CoreL.X > Stop ? 0.f : 180.f;
		const FTransform PanelT(FRotator(0.f, Face, 0.f), FVector(Stop, 0.f, 0.f));
		B.Panels.Add(PanelT * F);
		Box(F, PanelT.GetLocation() + FVector(0, 0, 70.f), FVector(30.f, 60.f, 140.f), TEXT("ivory"));
		Box(F, PanelT.TransformPosition(FVector(16.f, 0, 110.f)), FVector(2.f, 50.f, 30.f), TEXT("glow"), false, PanelT.Rotator());
		Cyl(F, PanelT.GetLocation() + FVector(0, 0, 140.f), 6.f, 160.f, TEXT("chrome"), false);
		UWidgetComponent* Board = NewObject<UWidgetComponent>(this);
		Board->SetupAttachment(RootComponent);
		Board->SetWidgetSpace(EWidgetSpace::World);
		Board->SetDrawSize(FVector2D(900.f, 360.f));
		Board->SetPivot(FVector2D(0.5f, 0.5f));
		Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Board->SetTwoSided(true);
		Board->SetBlendMode(EWidgetBlendMode::Opaque);
		Board->RegisterComponent();
		// over the panel, the size of a station clock (2.4 x 0.95 m), facing back along the platform
		const FVector BoardAt = F.TransformPosition(PanelT.GetLocation() + FVector(0, 0, 290.f));
		Board->SetWorldLocationAndRotation(BoardAt, (F.GetRotation() * PanelT.GetRotation()).Rotator());
		Board->SetWorldScale3D(FVector(0.27f));
		Board->SetSlateWidget(SNew(SLyudStationBoard).Station(S).Line(L));
		AddInstanceComponent(Board);
		B.Boards.Add(Board);
	}

	// the stair and the lift from the ground to the top deck (Aton: only a stair up to its platform on the hill)
	const float Height = TopDeck - CoreGround;
	const float CoreYaw = SimNum(St.Levels) && SimValid(St.AxisYaw, St.Levels[0].Line) ? St.AxisYaw[St.Levels[0].Line] : 0.f;
	const FTransform Foot(FRotator(0.f, CoreYaw, 0.f), FVector(CoreW.X, CoreW.Y, CoreGround));
	BuildStair(Foot, Height, S);
	// the shaft: glass on four sides with ivory corner posts, open at the ground on the stair's third side and at the top
	// toward the stop; the cab is an ALyudStationLift
	if (Height > LevelLiftMin)
	{
		const int32 GroundSide = 2;
		const FVector TopDir = Foot.InverseTransformVectorNoScale(B.Decks.Last().GetRotation().Vector()) *
			((SimValid(St.Ends, St.Levels.back().Line) && St.Ends[St.Levels.back().Line] == 1) ? -1.f : 1.f);
		int32 TopSide = 0;
		float Best = -2.f;
		for (int32 E = 0; E < 4; E++)
		{
			const FVector D = FRotator(0.f, E * 90.f, 0.f).Vector();
			if (FVector::DotProduct(D, TopDir) > Best)
			{
				Best = FVector::DotProduct(D, TopDir);
				TopSide = E;
			}
		}
		for (int32 E = 0; E < 4; E++)
		{
			const FVector D = FRotator(0.f, E * 90.f, 0.f).Vector();
			const float Z0 = E == GroundSide ? 240.f : 0.f;
			const float Z1 = E == TopSide ? Height : Height + 240.f;
			Box(Foot, D * ShaftHalf + FVector(0, 0, (Z0 + Z1) * 0.5f), FVector(5.f, ShaftHalf * 2.f, Z1 - Z0), TEXT("glass"), true, FRotator(0.f, E * 90.f, 0.f));
			if (E == TopSide)
			{
				Box(Foot, D * ShaftHalf + FVector(0, 0, Height + 230.f), FVector(8.f, ShaftHalf * 2.f, 20.f), TEXT("ivory"), true, FRotator(0.f, E * 90.f, 0.f));
			}
		}
		for (int32 E = 0; E < 4; E++)
		{
			const FVector C = FRotator(0.f, 45.f + E * 90.f, 0.f).Vector() * ShaftHalf * 1.414f;
			Box(Foot, C + FVector(0, 0, (Height + 250.f) * 0.5f), FVector(14.f, 14.f, Height + 250.f), TEXT("ivory"));
		}
		Box(Foot, FVector(0, 0, Height + 255.f), FVector(ShaftHalf * 2.f + 20.f, ShaftHalf * 2.f + 20.f, 12.f), TEXT("ivory"));
		Box(Foot, FVector(0, 0, -5.f), FVector(ShaftHalf * 2.f, ShaftHalf * 2.f, 10.f), TEXT("concrete"));
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		B.Lift = GetWorld()->SpawnActor<ALyudStationLift>(ALyudStationLift::StaticClass(), Foot, SP);
		if (B.Lift)
		{
			TArray<float> Stops = { 0.f, Height };
			B.Lift->Setup(Stops, Mat(TEXT("terrazzo")), Mat(TEXT("glass")), Mat(TEXT("chrome")));
			B.Lift->DoorSides = { GroundSide, TopSide };
		}
		B.Entrance = Foot.TransformPosition(FRotator(0.f, GroundSide * 90.f, 0.f).Vector() * (RingHalf + 150.f));
	}
	else
	{
		B.Entrance = Foot.TransformPosition(FVector(RingHalf + 150.f, -RingHalf, 0.f));
	}

	// the station's name over the ground entrance, on both sides
	for (int32 Side = 0; Side < 2; Side++)
	{
		UWidgetComponent* Sign = NewObject<UWidgetComponent>(this);
		Sign->SetupAttachment(RootComponent);
		Sign->SetWidgetSpace(EWidgetSpace::World);
		Sign->SetDrawSize(FVector2D(1000.f, 260.f));
		Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sign->SetBlendMode(EWidgetBlendMode::Transparent);
		Sign->RegisterComponent();
		const float Ang = (Side == 0 ? 0.f : 180.f) + 90.f;
		const FVector At = Foot.TransformPosition(FRotator(0.f, Ang, 0.f).Vector() * (RingHalf + 3.f) + FVector(0, 0, 330.f));
		Sign->SetWorldLocationAndRotation(At, (Foot.GetRotation() * FRotator(0.f, Ang, 0.f).Quaternion()).Rotator());
		Sign->SetWorldScale3D(FVector(0.4f));
		Sign->SetSlateWidget(SNew(SLyudStationSign).Station(S));
		AddInstanceComponent(Sign);
		B.Boards.Add(Sign);
	}
}

void ALyudCableway::BuildStair(const FTransform& Foot, float Height, int32 Station)
{
	if (Height < 30.f)
	{
		return;
	}
	// a square spiral round the shaft: a flight along each side, a landing at each corner; the rise per side is
	// RisePerSide or a little less so the last landing is exactly at the top deck
	const int32 Sides = FMath::Max(1, FMath::CeilToInt(Height / RisePerSide));
	const float Rise = Height / Sides;
	const float Mid = ShaftHalf + StairW * 0.5f;
	auto Corner = [&](int32 K) -> FVector
	{
		const int32 C = K % 4;
		return FVector(C == 1 || C == 2 ? -Mid : Mid, C >= 2 ? -Mid : Mid, 0.f);
	};
	// corner 0 at (+,+), 1 (-,+), 2 (-,-), 3 (+,-): counterclockwise seen from above
	for (int32 K = 0; K <= Sides; K++)
	{
		const FVector P = Corner(K) + FVector(0, 0, K * Rise);
		Box(Foot, P - FVector(0, 0, 12.5f), FVector(StairW, StairW, 25.f), TEXT("concrete"));
		if (K == Sides)
		{
			break;
		}
		const FVector Q = Corner(K + 1) + FVector(0, 0, (K + 1) * Rise);
		const FVector Along = (FVector(Q.X, Q.Y, 0.f) - FVector(P.X, P.Y, 0.f)).GetSafeNormal();
		const FVector Out = FVector(P.X + Q.X, P.Y + Q.Y, 0.f).GetSafeNormal();
		const float Yaw = Along.Rotation().Yaw;
		const float Run = (2.f * ShaftHalf) / StepsPerSide;
		for (int32 I = 0; I < StepsPerSide; I++)
		{
			const float Top = P.Z + Rise * (I + 1) / (StepsPerSide + 1);
			const FVector C = FVector(P.X, P.Y, 0.f) + Along * (StairW * 0.5f + Run * (I + 0.5f)) + FVector(0, 0, Top - 12.5f);
			Box(Foot, C, FVector(Run + 1.f, StairW, 25.f), I % 2 ? TEXT("terrazzo") : TEXT("concrete"), true, FRotator(0.f, Yaw, 0.f));
		}
		// the outer railing: a glass panel along the flight and a chrome rail on top
		const FVector A = FVector(P.X, P.Y, 0.f) + Out * (StairW * 0.5f - 4.f) * 0.f + Out * 0.f;
		const FVector OutEdge = Out * 0.f;
		const FVector S0 = FVector(P.X, P.Y, P.Z) + Along * StairW * 0.5f + (FVector(P.X, P.Y, 0.f).GetSafeNormal2D() * 0.f);
		(void)A; (void)OutEdge; (void)S0;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Along) * -1.f;
		const FVector OutSide = FVector::DotProduct(Side, Out) > 0.f ? Side : -Side;
		const FVector R0 = FVector(P.X, P.Y, P.Z) + OutSide * (StairW * 0.5f - 3.f) + Along * StairW * 0.5f;
		const FVector R1 = FVector(Q.X, Q.Y, Q.Z) + OutSide * (StairW * 0.5f - 3.f) - Along * StairW * 0.5f;
		const FVector M = (R0 + R1) * 0.5f + FVector(0, 0, 50.f);
		const float Len = FVector::Dist(R0, R1);
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(R1.Z - R0.Z, FVector::Dist2D(R0, R1)));
		Box(Foot, M, FVector(Len, 4.f, 100.f), TEXT("glass"), true, FRotator(Pitch, Yaw, 0.f));
		Box(Foot, M + FVector(0, 0, 52.f), FVector(Len, 7.f, 6.f), TEXT("chrome"), false, FRotator(Pitch, Yaw, 0.f));
		// the landing's own two outer edges
		const FVector L0 = Corner(K + 1);
		for (const FVector& E : { FVector(FMath::Sign(L0.X), 0.f, 0.f), FVector(0.f, FMath::Sign(L0.Y), 0.f) })
		{
			if (FVector::DotProduct(E, Along) > 0.5f)
			{
				const FVector C = FVector(L0.X, L0.Y, Q.Z) + E * (StairW * 0.5f - 3.f) + FVector(0, 0, 55.f);
				Box(Foot, C, FVector(4.f, StairW, 110.f), TEXT("glass"), true, FRotator(0.f, E.Rotation().Yaw, 0.f));
			}
		}
	}
	// the ivory core walls under the shaft's glass where there is no lift (Aton)
	if (Height <= LevelLiftMin)
	{
		Box(Foot, FVector(0, 0, Height * 0.5f), FVector(ShaftHalf * 2.f, ShaftHalf * 2.f, Height), TEXT("ivory"));
	}
}

void ALyudCableway::BuildPylons(const LyudTransport::FNetwork& Net, int32 L)
{
	const FKit& K = Kit();
	UStaticMesh* Feet = LoadMesh(TEXT("SM_TR_PylonLine_Feet"));
	UStaticMesh* Shaft = LoadMesh(TEXT("SM_TR_PylonLine_Shaft"));
	UStaticMesh* Head = LoadMesh(TEXT("SM_TR_PylonLine_Head"));
	UStaticMesh* Corner = LoadMesh(TEXT("SM_TR_PylonCorner"));
	for (const LyudTransport::FPylon& P : Net.Lines[L].Pylons)
	{
		const FVector G = W(P.X, P.Y, P.Ground);
		const float Top = P.Top * 100.f;
		const float H = Top - G.Z;
		if (P.bCorner && Corner)
		{
			// the lattice tower scaled whole: its sheave at the rope
			const float S = H / (100.f * K.CornerSaddle);
			Single(Corner, FTransform(FRotator(0.f, P.Yaw, 0.f), G - FVector(0, 0, 30.f), FVector(S)), true);
			continue;
		}
		if (Feet && Shaft && Head)
		{
			const FRotator R(0.f, P.Yaw - 90.f, 0.f);       // the crossarm across the line
			const float HeadBase = Top - K.PylonHeadSaddle;
			const float ShaftLen = FMath::Max(10.f, HeadBase - (G.Z + K.PylonFeet));
			Single(Feet, FTransform(R, G - FVector(0, 0, 20.f)), true);
			Single(Shaft, FTransform(R, G + FVector(0, 0, K.PylonFeet - 20.f), FVector(1.f, 1.f, (ShaftLen + 20.f) / 100.f)), true);
			Single(Head, FTransform(R, FVector(G.X, G.Y, HeadBase)), false);
			continue;
		}
		// no kit: a plain mast and crossarm
		const FTransform F(FRotator(0.f, P.Yaw, 0.f), G);
		Cyl(F, FVector(0, 0, -50.f), 60.f, H + 50.f, TEXT("ivory"));
		Box(F, FVector(0, 0, H + 20.f), FVector(80.f, 1000.f, 40.f), TEXT("steel"), false);
	}
}

void ALyudCableway::BuildRopes(const LyudTransport::FNetwork& Net, int32 L)
{
	const LyudTransport::FLine& Ln = Net.Lines[L];
	for (int32 I = 0; I + 1 < SimNum(Ln.Loop); I++)
	{
		const LyudTransport::V3& A = Ln.Loop[I];
		const LyudTransport::V3& B = Ln.Loop[I + 1];
		// thicker than a real 5 cm rope so the line reads from afar
		Rod(W(A.X, A.Y, A.Z), W(B.X, B.Y, B.Z), 4.5f, TEXT("rope"), false);
	}
}

void ALyudCableway::BuildDepot(const LyudTransport::FNetwork& Net, int32 L)
{
	const LyudTransport::FLine& Ln = Net.Lines[L];
	const int32 S = Ln.Depot;
	if (!Built.IsValidIndex(S))
	{
		return;
	}
	const FLyudBuiltStation& B = Built[S];
	const int32 K = B.DeckLines.Find(L);
	if (K == INDEX_NONE)
	{
		return;
	}
	const FTransform& F = B.Decks[K];
	const int32 End = SimValid(Net.Stations[S].Ends, L) ? Net.Stations[S].Ends[L] : -1;
	const float Dir = End == 1 ? -1.f : 1.f;           // beyond the wheel
	int32 Count = 0;
	for (const LyudTransport::FCabin& C : Net.Cabins)
	{
		Count += C.Line == L;
	}
	// a hall on columns past the turn wheel: a storage rail with the cabins in a row, a sawtooth roof
	const float X0 = Dir * 650.f, Len = 500.f * Count + 400.f;
	const float Xc = X0 + Dir * Len * 0.5f;
	Box(F, FVector(Xc, 0.f, -DeckThick * 0.5f), FVector(Len, 700.f, DeckThick), TEXT("concrete"));
	for (const float Y : { -340.f, 340.f })
	{
		Box(F, FVector(Xc, Y, 240.f), FVector(Len, 20.f, 480.f), TEXT("glass"));
		Box(F, FVector(Xc, Y, 480.f), FVector(Len, 30.f, 30.f), TEXT("ivory"));
	}
	Box(F, FVector(X0 + Dir * Len, 0.f, 240.f), FVector(30.f, 700.f, 480.f), TEXT("ivory"));
	for (int32 I = 0; I < FMath::Max(2, Count); I++)
	{
		const float X = X0 + Dir * (Len * (I + 0.5f) / FMath::Max(2, Count));
		Box(F, FVector(X, 0.f, 560.f), FVector(Len / FMath::Max(2, Count) + 2.f, 720.f, 20.f), TEXT("ivory"), false, FRotator(14.f, 0.f, 0.f));
		Box(F, FVector(X, 0.f, 520.f), FVector(8.f, 700.f, 80.f), TEXT("glass"), false);
	}
	Box(F, FVector(Xc, 0.f, 365.f), FVector(Len, 12.f, 12.f), TEXT("steel"), false);
	for (const float X : { X0 + Dir * 150.f, X0 + Dir * (Len - 150.f) })
	{
		const FVector P = F.TransformPosition(FVector(X, 0.f, 0.f));
		const float G = Net.Stations[S].Ground * 100.f - 200.f;
		Cyl(FTransform::Identity, FVector(P.X, P.Y, G), 80.f, P.Z - DeckThick - G, TEXT("ivory"));
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
	const FVector P = F.TransformPosition(FVector(X, Side * (IslandHalf - 75.f), 0.f));
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

ALyudStationLift::ALyudStationLift()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Cab = CreateDefaultSubobject<USceneComponent>(TEXT("Cab"));
	Cab->SetupAttachment(Root);
	Cab->SetMobility(EComponentMobility::Movable);
	Inside = CreateDefaultSubobject<UBoxComponent>(TEXT("Inside"));
	Inside->SetupAttachment(Cab);
	Inside->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	Inside->SetRelativeLocation(FVector(0, 0, 100.f));
	Inside->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ALyudStationLift::Setup(const TArray<float>& InStops, UMaterialInterface* Floor, UMaterialInterface* Glass, UMaterialInterface* Frame)
{
	Stops = InStops;
	Z = Stops.Num() ? Stops[0] : 0.f;
	UStaticMesh* Cube = BasicShape(TEXT("box"));
	auto Part = [&](const FVector& C, const FVector& Size, UMaterialInterface* M, bool bCollide)
	{
		UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
		P->SetMobility(EComponentMobility::Movable);
		P->SetStaticMesh(Cube);
		P->SetMaterial(0, M);
		P->SetupAttachment(Cab);
		P->SetRelativeLocation(C);
		P->SetRelativeScale3D(Size / 100.f);
		P->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		P->RegisterComponent();
		AddInstanceComponent(P);
		return P;
	};
	Part(FVector(0, 0, -6.f), FVector(228.f, 228.f, 12.f), Floor, true);
	Part(FVector(0, 0, 232.f), FVector(228.f, 228.f, 8.f), Frame, false);
	// a handrail on the two sides without doors
	Part(FVector(0, 108.f, 95.f), FVector(200.f, 4.f, 4.f), Frame, false);
	Part(FVector(0, -108.f, 95.f), FVector(200.f, 4.f, 4.f), Frame, false);
	// door blockers at the stops the cab is not at (see Tick)
	for (int32 I = 0; I < 2; I++)
	{
		UStaticMeshComponent* G = NewObject<UStaticMeshComponent>(this);
		G->SetMobility(EComponentMobility::Movable);
		G->SetStaticMesh(Cube);
		G->SetMaterial(0, Glass);
		G->SetupAttachment(Root);
		G->SetRelativeScale3D(FVector(0.06f, 2.3f, 2.3f));
		G->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		G->RegisterComponent();
		AddInstanceComponent(G);
		Gates.Add(G);
	}
	Cab->SetRelativeLocation(FVector(0, 0, Z));
}

int32 ALyudStationLift::StopNear(float LocalZ) const
{
	int32 Best = 0;
	for (int32 I = 1; I < Stops.Num(); I++)
	{
		if (FMath::Abs(Stops[I] - LocalZ) < FMath::Abs(Stops[Best] - LocalZ))
		{
			Best = I;
		}
	}
	return Best;
}

void ALyudStationLift::CallTo(int32 Stop)
{
	if (Stops.IsValidIndex(Stop))
	{
		Target = Stop;
	}
}

void ALyudStationLift::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Stops.Num() < 2)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector Local = Pawn ? GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()) : FVector(1e6f);
	const bool bIn = FMath::Abs(Local.X) < 115.f && FMath::Abs(Local.Y) < 115.f && Local.Z > Z - 20.f && Local.Z < Z + 260.f;
	if (Target < 0)
	{
		if (bIn)
		{
			// standing in the cab a moment sends it to the other end
			Stand += DeltaTime;
			if (Stand > 1.2f)
			{
				Target = StopNear(Z) == 0 ? Stops.Num() - 1 : 0;
				Stand = 0.f;
				if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
				{
					Sound->PlayEffect(TEXT("lift_start"), Cab->GetComponentLocation() + FVector(0, 0, 120.f), 0.8f);
				}
			}
		}
		else
		{
			Stand = 0.f;
			// somebody waiting at a door the cab is not at: it comes
			const int32 Near = StopNear(Local.Z);
			if (Pawn && Near != StopNear(Z) && FMath::Abs(Local.Z - Stops[Near]) < 150.f && FVector2D(Local.X, Local.Y).Size() < 420.f)
			{
				Target = Near;
			}
		}
	}
	if (Target >= 0)
	{
		const float Goal = Stops[Target];
		const float Dist = Goal - Z;
		const float Speed = FMath::Min(250.f, FMath::Sqrt(2.f * 120.f * FMath::Abs(Dist)) + 20.f);
		Z += FMath::Clamp(Dist, -Speed * DeltaTime, Speed * DeltaTime);
		if (FMath::IsNearlyEqual(Z, Goal, 0.5f))
		{
			Z = Goal;
			Target = -1;
			if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
			{
				Sound->PlayEffect(TEXT("lift_ding"), Cab->GetComponentLocation() + FVector(0, 0, 200.f), 0.7f);
			}
		}
		Cab->SetRelativeLocation(FVector(0, 0, Z));
	}
	// the gates: shut at every stop but where the cab stands
	for (int32 I = 0; I < Gates.Num() && I < Stops.Num() && I < DoorSides.Num(); I++)
	{
		const bool bHere = Target < 0 && FMath::IsNearlyEqual(Z, Stops[I], 1.f);
		const FVector D = FRotator(0.f, DoorSides[I] * 90.f, 0.f).Vector();
		Gates[I]->SetRelativeLocation(D * 122.f + FVector(0, 0, Stops[I] + 115.f));
		Gates[I]->SetRelativeRotation(FRotator(0.f, DoorSides[I] * 90.f, 0.f));
		Gates[I]->SetCollisionEnabled(bHere ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		Gates[I]->SetVisibility(!bHere);
	}
}

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
	Lamp->SetIntensity(1200.f);
	Lamp->SetAttenuationRadius(380.f);
	Lamp->SetLightColor(FLinearColor(1.f, 0.78f, 0.5f));
	Lamp->SetCastShadows(false);
	Board = CreateDefaultSubobject<UWidgetComponent>(TEXT("Board"));
	Board->SetupAttachment(Root);
}

void ALyudCabin::Setup(ALyudCableway* Way, int32 InIndex, int32 Number, const FText& Name, const FLinearColor& LineColor)
{
	Index = InIndex;
	const FKit& K = Kit();
	UStaticMesh* Cube = BasicShape(TEXT("box"));
	if (UStaticMesh* M = LoadMesh(TEXT("SM_TR_Cabin")))
	{
		Shell->SetStaticMesh(M);
		Shell->SetRelativeLocation(FVector(0, 0, -K.CabinFloor));
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
	Drive(GetActorLocation() + FVector(0, 0, Hanger), FVector::ForwardVector, 0.f, 0.f);

	// the board over the door inside: where it goes
	Board->SetWidgetSpace(EWidgetSpace::World);
	Board->SetDrawSize(FVector2D(600.f, 150.f));
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Board->SetBlendMode(EWidgetBlendMode::Opaque);
	Board->SetRelativeLocation(FVector(0.f, -HW + 6.f, Top - 25.f));
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
		P->SetRelativeLocation(FVector(S > 0 ? 0.f : 110.f, S * (Width * 0.5f + 4.f), 48.f));
		P->SetRelativeRotation(FRotator(0.f, S > 0 ? 90.f : -90.f, 0.f));
		P->SetRelativeScale3D(FVector(0.16f));
		P->SetSlateWidget(SNew(SLyudCabinPlate).Number(Number).Name(Name).Color(LineColor));
		AddInstanceComponent(P);
		Plates.Add(P);
	}
}

void ALyudCabin::Drive(const FVector& Grip, const FVector& Dir, float InDoor, float DeltaTime)
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
	SetActorLocationAndRotation(Grip - FVector(0, 0, Over), FRotator(0.f, Yaw, 0.f));
	Door = InDoor;
	const float HW = Width * 0.5f - 20.f;
	DoorLeaf->SetRelativeLocation(FVector(Door * 92.f, -HW - 6.f, 102.5f));
}

void ALyudCabin::SetBoard(const FText& Line1, const FText& Line2)
{
	BoardA = Line1;
	BoardB = Line2;
}

bool ALyudCabin::Contains(const FVector& P) const
{
	const FVector L = GetActorTransform().InverseTransformPosition(P);
	return FMath::Abs(L.X) < Length * 0.5f - 20.f && FMath::Abs(L.Y) < Width * 0.5f - 10.f && L.Z > -40.f && L.Z < 240.f;
}

FTransform ALyudCabin::Seat(int32 K) const
{
	const float HL = Length * 0.5f - 30.f;
	const float S = K < 2 ? 1.f : -1.f;
	const float Y = (K % 2 ? 1.f : -1.f) * 45.f;
	// the floor under the seat; facing the other bench
	return FTransform(FRotator(0.f, S > 0 ? 180.f : 0.f, 0.f), FVector(S * (HL - 28.f), Y, 0.f)) * GetActorTransform();
}

FVector ALyudCabin::DoorPoint() const
{
	return GetActorTransform().TransformPosition(FVector(0.f, -Width * 0.5f, 0.f));
}
