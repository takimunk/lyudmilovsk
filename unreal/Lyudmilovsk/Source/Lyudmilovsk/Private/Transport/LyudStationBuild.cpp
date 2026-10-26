// The stations' procedural part (docs/transport.md "Station layout and the vestibule connector"): per station one mesh
// with metre UVs and simple collision (FLyudMeshKit), built in the tower's frame:
//   - the tower on the island: a glass lift shaft in the middle, a square stair round it inside a glass curtain wall;
//     at every stop (the ground, each deck) the stair comes to a level landing along one whole side, and the lift's door
//     and the way out are on that side, so the lift always opens onto a landing, never onto a flight;
//   - each level's platform (terrazzo, a tactile strip, glass railings where the cabins only pass, open where they
//     stop), its butterfly canopy on columns along the middle, columns under the island's ends;
//   - the ground gallery from the tower under the track to the vestibule's connector, with its door, the diagram,
//     the clock, a bench, a plant; the hi3d vestibule placed at the connector on a podium.
// The pylons' shafts are lathes here too (the hi3d shaft streaked and tore when stretched).
#include "Transport/LyudCableway.h"

#include "CablewaySim.h"
#include "LyudMeshKit.h"
#include "SLyudTransitWidgets.h"
#include "TransportKit.h"
#include "TransportStd.h"
#include "Audio/LyudSoundscape.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Doors/LyudDoor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Plants/ProceduralPlant.h"
#include "Plants/ProceduralGrass.h"
#include "Plants/ProceduralForest.h"
#include "Transport/LyudTransport.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace LyudTransportKit;

namespace
{
	/** Side K of the tower: its outward direction and its counterclockwise tangent (core frame). */
	FVector SideOut(int32 K) { return FRotator(0.f, (K & 3) * 90.f, 0.f).Vector(); }
	FVector SideAlong(int32 K) { return FRotator(0.f, (K & 3) * 90.f + 90.f, 0.f).Vector(); }

	/** One piece of the stair: a level landing along a whole side, or a flight up along it. */
	struct FStairPiece { int32 Side; bool bFlat; float Z0, Z1; };

	/**
	 * The stair from the ground to the top stop: at each stop a level side (its door side), between two stops flights
	 * on the sides in between, counterclockwise, as many turns as the height needs (no flight rises over MaxFlightRise).
	 */
	TArray<FStairPiece> PlanStair(const TArray<float>& Stops, const TArray<int32>& Sides)
	{
		TArray<FStairPiece> Out;
		for (int32 K = 0; K < Stops.Num(); K++)
		{
			Out.Add({ Sides[K] & 3, true, Stops[K], Stops[K] });
			if (K + 1 >= Stops.Num())
			{
				break;
			}
			const float Rise = Stops[K + 1] - Stops[K];
			int32 N = (((Sides[K + 1] - Sides[K] - 1) % 4) + 4) % 4;
			while (N < 1 || Rise / N > MaxFlightRise)
			{
				N += 4;
			}
			for (int32 J = 1; J <= N; J++)
			{
				Out.Add({ (Sides[K] + J) & 3, false, Stops[K] + Rise * (J - 1) / N, Stops[K] + Rise * J / N });
			}
		}
		return Out;
	}

	/** A point on side K's lane: U along it (-LaneIn..LaneIn), R out from the middle. */
	FVector Lane(int32 K, float U, float R, float Z) { return SideOut(K) * R + SideAlong(K) * U + FVector(0, 0, Z); }

	/** The corner between side K and side K+1 (counterclockwise), at distance R on both axes. */
	FVector CornerAt(int32 K, float R, float Z) { return (SideOut(K) + SideOut(K + 1)) * R + FVector(0, 0, Z); }
}

namespace
{
	/** Content/Transport/vestibules.json (blender/transport/vestibules.py): per style, UE cm in the connector frame. */
	TSharedPtr<FJsonObject> VestibuleData(const FString& Style)
	{
		static TSharedPtr<FJsonObject> All;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Transport/vestibules.json"))))
			{
				FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), All);
			}
		}
		const TSharedPtr<FJsonObject>* V = nullptr;
		return All.IsValid() && All->TryGetObjectField(Style.ToLower(), V) ? *V : nullptr;
	}

	/** vestibules.json v2 tower.decks[]: the deck on the vestibule's tower that serves line Line (Zenit's Sun line). */
	TSharedPtr<FJsonObject> TowerDeck(const TSharedPtr<FJsonObject>& V, const FString& Line)
	{
		const TSharedPtr<FJsonObject>* T = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr;
		if (V.IsValid() && V->TryGetObjectField(TEXT("tower"), T) && (*T)->TryGetArrayField(TEXT("decks"), Ds))
		{
			for (const TSharedPtr<FJsonValue>& D : *Ds)
			{
				if (D->AsObject()->GetStringField(TEXT("line")) == Line)
				{
					return D->AsObject();
				}
			}
		}
		return nullptr;
	}

	FVector JsonXY(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, float Z = 0.f)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, A) && A->Num() >= 2)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : Z);
		}
		return FVector(0, 0, Z);
	}

	/** The convex hull of 2D points (counterclockwise). */
	TArray<FVector2D> Hull(TArray<FVector2D> P)
	{
		P.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X || (A.X == B.X && A.Y < B.Y); });
		TArray<FVector2D> H;
		auto Cross = [](const FVector2D& O, const FVector2D& A, const FVector2D& B) { return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X); };
		for (int32 Pass = 0; Pass < 2; Pass++)
		{
			const int32 Start = H.Num();
			for (int32 I = 0; I < P.Num(); I++)
			{
				const FVector2D& Q = Pass ? P[P.Num() - 1 - I] : P[I];
				while (H.Num() >= Start + 2 && Cross(H[H.Num() - 2], H.Last(), Q) <= 0.f)
				{
					H.Pop();
				}
				H.Add(Q);
			}
			H.Pop();
		}
		return H;
	}
}

UStaticMeshComponent* ALyudCableway::KitMesh(const FLyudMeshKit& K, const FTransform& At, FName Name, bool bMovable, USceneComponent* Parent)
{
	if (K.IsEmpty())
	{
		return nullptr;
	}
	AActor* OwnerActor = Parent ? Parent->GetOwner() : this;
	TArray<FName> Slots;
	UStaticMesh* Mesh = K.Build(OwnerActor, Name, Slots);
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(OwnerActor);
	// Movable: a static component registered at play with a mesh built at play is never drawn
	C->SetMobility(EComponentMobility::Movable);
	(void)bMovable;
	C->SetStaticMesh(Mesh);
	for (int32 I = 0; I < Slots.Num(); I++)
	{
		C->SetMaterial(I, Mat(Slots[I]));
	}
	C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	C->SetupAttachment(Parent ? Parent : RootComponent.Get());
	C->RegisterComponent();
	if (Parent)
	{
		C->SetRelativeTransform(At);
	}
	else
	{
		C->SetWorldTransform(At);
	}
	OwnerActor->AddInstanceComponent(C);
	if (Mesh->GetBodySetup() && !C->GetBodyInstance()->IsValidBodyInstance())
	{
		UE_LOG(LogTemp, Warning, TEXT("Transport: %s has %d collision pieces but no physics body"), *Name.ToString(), Mesh->GetBodySetup()->AggGeom.GetElementCount());
	}
	if (!Parent)
	{
		Singles.Add(C);
	}
	return C;
}

UWidgetComponent* ALyudCableway::Widget(int32 S, const TSharedRef<SWidget>& Slate, const FTransform& At, const FVector2D& Draw, float Scale, bool bOpaque)
{
	UWidgetComponent* Wc = NewObject<UWidgetComponent>(this);
	Wc->SetupAttachment(RootComponent);
	Wc->SetWidgetSpace(EWidgetSpace::World);
	Wc->SetDrawSize(Draw);
	Wc->SetPivot(FVector2D(0.5f, 0.5f));
	Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wc->SetBlendMode(bOpaque ? EWidgetBlendMode::Opaque : EWidgetBlendMode::Transparent);
	Wc->RegisterComponent();
	Wc->SetWorldLocationAndRotation(At.GetLocation(), At.Rotator());
	Wc->SetWorldScale3D(FVector(Scale));
	Wc->SetSlateWidget(Slate);
	AddInstanceComponent(Wc);
	if (Built.IsValidIndex(S))
	{
		Built[S].Boards.Add(Wc);
	}
	return Wc;
}

void ALyudCableway::BuildStation(const LyudTransport::FNetwork& Net, int32 S)
{
	// docs/transport.md "Station layout v2": the vestibule at deck height between the spread tracks (its two edges the
	// boarding platforms), on a tower with the lift and the stair from a street door at its foot
	const LyudTransport::FStation& St = Net.Stations[S];
	FLyudBuiltStation& B = Built[S];
	const FVector Centre = W(St.X, St.Y, 0.f);
	StationCentres[S] = Centre;
	StationRadius[S] = 4500.f;
	// the tower and the vestibule turn with the top level's line (the vestibule's deck; a lower deck on the tower is
	// turned to its own line)
	const float Yaw0 = SimNum(St.Levels) && SimValid(St.AxisYaw, St.Levels.back().Line) ? St.AxisYaw[St.Levels.back().Line] : 0.f;
	FVector CoreW = W(St.CoreX, St.CoreY, St.CoreGround);
	const FString Style = St.Style.empty() ? TEXT("saucer") : FString(UTF8_TO_TCHAR(St.Style.c_str()));
	const TSharedPtr<FJsonObject> V = VestibuleData(Style);
	const bool bV2 = V.IsValid() && V->HasField(TEXT("track_gap"));
	// the tower kit's podium (tower.base): the tower's floor on it, it on the highest ground under the tower
	float BaseH = 0.f;
	{
		const TSharedPtr<FJsonObject>* T = nullptr;
		const TSharedPtr<FJsonObject>* Bs = nullptr;
		if (bV2 && V->TryGetObjectField(TEXT("tower"), T) && (*T)->TryGetObjectField(TEXT("base"), Bs) && LoadMesh((*Bs)->GetStringField(TEXT("mesh"))))
		{
			BaseH = (*Bs)->GetNumberField(TEXT("h"));
			CoreW.Z += BaseH - 15.f;
		}
	}
	{
		// the tower's floor 15 cm over the landscape as it stands in this level (a landscape built from older inputs
		// can stand a little higher than network_built.json's ground)
		const FTransform F0(FRotator(0.f, Yaw0, 0.f), CoreW);
		float Top = -1e9f;
		for (const float Y : { -270.f, -135.f, 0.f, 135.f, 270.f })
		{
			for (const float X : { -270.f, -135.f, 0.f, 135.f, 270.f })
			{
				const FVector P = F0.TransformPosition(FVector(X, Y, 0.f));
				FHitResult Hit;
				FCollisionQueryParams Q(TEXT("StationFloor"), false, this);
				if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 2000.f), P - FVector(0, 0, 3000.f), ECC_WorldStatic, Q) &&
					Hit.GetComponent() && Hit.GetComponent()->IsA<ULandscapeHeightfieldCollisionComponent>())
				{
					Top = FMath::Max(Top, (float)Hit.ImpactPoint.Z);
				}
			}
		}
		const float Over = BaseH > 0.f ? BaseH : 15.f;
		if (Top + Over > CoreW.Z)
		{
			CoreW.Z = Top + Over;
		}
	}
	B.Core = FTransform(FRotator(0.f, Yaw0, 0.f), CoreW);

	// the decks' frames: origin on the deck at the station's centre, X along its line's way out
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const float Yaw = SimValid(St.AxisYaw, L) ? St.AxisYaw[L] : 0.f;
		const LyudTransport::FStationGeo* Geo = SimValid(St.Geo, L) && St.Geo[L].bValid ? &St.Geo[L] : nullptr;
		// the line's own middle (a deck on the tower: the tower's axis)
		const FVector Mid = Geo ? W(Geo->CX, Geo->CY, 0.f) : Centre;
		B.Decks.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Mid.X, Mid.Y, St.Levels[K].Z * 100.f)));
		B.DeckLines.Add(L);
		// the depot's rail: past the turn wheel, along the line away from the station
		if (Geo && Geo->bWheel)
		{
			const FVector Wh = W(Geo->Wheel.X, Geo->Wheel.Y, St.Levels[K].Z);
			const FVector Away = (Wh - FVector(Centre.X, Centre.Y, Wh.Z)).GetSafeNormal2D();
			B.DepotAt.Add(FTransform(Away.Rotation(), Wh + Away * 650.f));
		}
		else
		{
			B.DepotAt.Add(FTransform::Identity);
		}
	}
	// the vestibule's frame: the top deck's, at the station's centre
	B.Connector = B.Decks.Num() ? FTransform(B.Decks.Last().GetRotation(), FVector(Centre.X, Centre.Y, B.Decks.Last().GetLocation().Z)) : FTransform(Centre);

	// the vestibule's numbers (vestibules.json v2): the sides of the tower's street door and of its exit into the room
	auto SideOf = [](const FString& Str, int32 Default)
	{
		return Str == TEXT("+x") ? 0 : Str == TEXT("+y") ? 1 : Str == TEXT("-x") ? 2 : Str == TEXT("-y") ? 3 : Default;
	};
	int32 GroundSide = 2, ExitSide = 0;
	if (bV2)
	{
		ExitSide = SideOf(V->GetStringField(TEXT("core_exit")), 0);
		const TSharedPtr<FJsonObject>* T = nullptr;
		const TSharedPtr<FJsonObject>* D = nullptr;
		if (V->TryGetObjectField(TEXT("tower"), T) && (*T)->TryGetObjectField(TEXT("door"), D))
		{
			GroundSide = SideOf((*D)->GetStringField(TEXT("side")), 2);
		}
	}
	TArray<float> Stops = { 0.f };
	TArray<int32> Sides = { GroundSide };
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		Stops.Add(St.Levels[K].Z * 100.f - CoreW.Z);
		int32 Side = ExitSide;
		// a lower deck on the tower: its own way out (tower.decks[].core_exit_dir, in the station's frame)
		if (const TSharedPtr<FJsonObject> D = TowerDeck(V, UTF8_TO_TCHAR(Net.Lines[St.Levels[K].Line].Id.c_str())); D.IsValid() && K + 1 < SimNum(St.Levels))
		{
			const FVector Dir = JsonXY(D, TEXT("core_exit_dir"));
			for (int32 E = 1, Best = 0; E <= 4; E++)
			{
				if (E == 4)
				{
					Side = Best;
					break;
				}
				if (FVector::DotProduct(SideOut(E), Dir) > FVector::DotProduct(SideOut(Best), Dir))
				{
					Best = E;
				}
			}
		}
		Sides.Add(Side);
	}
	B.Stops = Stops;
	B.StopSides = Sides;
	const float Top = Stops.Last();

	// the tower's own shell, unless the vestibule's tower kit stacks one round it
	const bool bShell = bV2 && StackTowerKit(S, Style, Top);
	FLyudMeshKit Kit;
	BuildTower(Kit, Net, S, Stops, Sides, !bShell);
	// no vestibule (yet): a plain boarding deck between the spread tracks
	if (!bV2)
	{
		for (int32 K = 0; K < SimNum(St.Levels); K++)
		{
			BuildDeck(Kit, Net, S, K);
		}
	}
	BuildRopeGear(Kit, Net, S);
	if (UStaticMeshComponent* C = KitMesh(Kit, B.Core, FName(*FString::Printf(TEXT("SM_Station_%d"), S))))
	{
		C->SetCastShadow(true);
		UE_LOG(LogTemp, Log, TEXT("Transport: station %d mesh %d triangles%s"), S, Kit.NumTriangles(), bShell ? TEXT(" (in its tower kit)") : TEXT(""));
	}
	// bare ground: under the vestibule (and its ropes' spread) and round the tower
	BareBoxes.Add({ FTransform(B.Core.GetRotation(), FVector(Centre.X, Centre.Y, (CoreW.Z + Top + CoreW.Z) * 0.5f)),
		FVector(1500.f, 1500.f, Top * 0.5f + 2500.f) });

	// warm light in the tower at every stop's landing
	for (int32 K = 0; K < Stops.Num(); K++)
	{
		UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
		Lt->SetupAttachment(RootComponent);
		Lt->RegisterComponent();
		Lt->SetWorldLocation(B.Core.TransformPosition(SideOut(Sides[K]) * 187.f + FVector(0, 0, Stops[K] + (K + 1 == Stops.Num() ? -60.f : 250.f))));
		Lt->SetIntensity(3000.f);
		Lt->SetAttenuationRadius(700.f);
		Lt->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
		Lt->SetCastShadows(false);
		AddInstanceComponent(Lt);
	}

	// the lift
	if (Top > LevelLiftMin)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		B.Lift = GetWorld()->SpawnActor<ALyudStationLift>(ALyudStationLift::StaticClass(), B.Core, SP);
		if (B.Lift)
		{
			B.Lift->Setup(this, Stops, Sides);
		}
	}

	// the street door at the tower's foot, steps down from it to the ground, the way in
	const FVector GDir = B.Core.TransformVectorNoScale(SideOut(GroundSide));
	{
		const FVector2D Hole = ALyudDoor::HoleForClear(150.f, 230.f, true);
		// in the kit's foot: in its wall just outside the clear square (tower.door.at: its distance from the axis)
		float DoorR = TowerHalf;
		if (bShell)
		{
			DoorR = 268.f;
			const TSharedPtr<FJsonObject>* T = nullptr;
			const TSharedPtr<FJsonObject>* D = nullptr;
			if (V->TryGetObjectField(TEXT("tower"), T) && (*T)->TryGetObjectField(TEXT("door"), D) && (*D)->HasTypedField<EJson::Number>(TEXT("at")))
			{
				DoorR = (*D)->GetNumberField(TEXT("at"));
			}
		}
		const FTransform DoorT(FRotator(0.f, Yaw0 + GroundSide * 90.f, 0.f), B.Core.TransformPosition(SideOut(GroundSide) * DoorR));
		if (ALyudDoor* Door = GetWorld()->SpawnActorDeferred<ALyudDoor>(ALyudDoor::StaticClass(), DoorT, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Door->Size = ELyudDoorSize::FitHole;
			Door->HoleWidth = FMath::Min(Hole.X, 200.f);
			Door->HoleHeight = FMath::Min(Hole.Y, 230.f);
			Door->bDoubleLeaf = true;
			Door->WallThickness = 8.f;
			Door->LeafColor = FLinearColor(0.80f, 0.76f, 0.66f);
			Door->FrameColor = FLinearColor(0.86f, 0.42f, 0.08f);
			Door->FinishSpawning(DoorT);
			B.Door = Door;
		}
		FLyudMeshKit Steps;
		const FTransform OutF(FRotator(0.f, GroundSide * 90.f, 0.f), SideOut(GroundSide) * TowerOut);
		// a landing before the door, then steps down to the ground found there
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("StationSteps"), false, this);
		const FVector Probe = B.Core.TransformPosition(SideOut(GroundSide) * (TowerOut + 260.f));
		float Drop = 0.f;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Probe + FVector(0, 0, 300.f), Probe - FVector(0, 0, 1500.f), ECC_WorldStatic, Q))
		{
			Drop = FMath::Max(0.f, CoreW.Z - (float)Hit.ImpactPoint.Z);
		}
		if (BaseH > 0.f)
		{
			// the kit's podium has its own steps all round
			Drop = 0.f;
		}
		else
		{
			Steps.BoxIn(TEXT("tx_concrete"), OutF, FVector(75.f, 0.f, -(Drop + 60.f) * 0.5f), FVector(150.f, 300.f, Drop + 60.f), true, TEXT("tx_terrazzo"));
		}
		const int32 N = FMath::Clamp(FMath::CeilToInt((Drop - 3.f) / 16.f), 0, 30);
		for (int32 I = 0; I < N; I++)
		{
			const float TopZ = -Drop * (I + 1) / N;
			Steps.BoxIn(TEXT("tx_concrete"), OutF, FVector(150.f + 30.f * I + 15.f, 0.f, (TopZ - Drop - 40.f) * 0.5f), FVector(30.f, 300.f, TopZ + Drop + 40.f), false, TEXT("tx_tread"));
		}
		if (N > 0)
		{
			const float Run = 30.f * N;
			Steps.CollideBox(FTransform(FRotator(-FMath::RadiansToDegrees(FMath::Atan2(Drop, Run)), 0.f, 0.f), FVector(150.f + Run * 0.5f, 0.f, -Drop * 0.5f - 4.f)) * OutF,
				FVector(FMath::Sqrt(Run * Run + Drop * Drop) * 0.5f, 150.f, 4.f));
		}
		KitMesh(Steps, B.Core, FName(*FString::Printf(TEXT("SM_StationSteps_%d"), S)));
		B.Entrance = B.Core.TransformPosition(SideOut(GroundSide) * (TowerOut + 150.f + 30.f * N + 120.f) - FVector(0, 0, Drop));
		B.EntranceOut = GDir;
		B.Vestibule = FTransform(FRotator(0.f, Yaw0 + GroundSide * 90.f + 180.f, 0.f), B.Core.TransformPosition(SideOut(GroundSide) * (TowerOut + 100.f)));
	}
	PlaceVestibule(Net, S);

	// the station's name over the street door, and on the tower's two faces across the line
	auto Sign = [&](const FTransform& At, float WidthCm)
	{
		// a widget's face looks along its +X: the sign faces along At's +X
		Widget(S, SNew(SLyudStationSign).Station(S), At, FVector2D(1600.f, 300.f), WidthCm / 1600.f, true);
	};
	Sign(FTransform(FRotator(0.f, Yaw0 + GroundSide * 90.f, 0.f), B.Core.TransformPosition(SideOut(GroundSide) * (TowerOut + 3.f) + FVector(0, 0, 285.f))), 300.f);
	for (const int32 E : { (GroundSide + 1) & 3, (GroundSide + 3) & 3 })
	{
		Sign(FTransform(FRotator(0.f, Yaw0 + E * 90.f, 0.f), B.Core.TransformPosition(SideOut(E) * (TowerOut + 3.f) + FVector(0, 0, FMath::Min(450.f, Top * 0.5f)))), 420.f);
	}

	// per level: by each edge the call panel at the stop mark (vestibules.json "panels" or the fallback deck's) with
	// its wait board over it
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const FTransform& F = B.Decks[K];
		const LyudTransport::FStationGeo* Geo = SimValid(St.Geo, L) && St.Geo[L].bValid ? &St.Geo[L] : nullptr;
		const float Edge = Geo ? Geo->Edge * 100.f : IslandHalf;
		const float Stop = Geo ? Geo->StopX * 100.f : 0.f;
		TArray<FTransform> Spots;
		const TArray<TSharedPtr<FJsonValue>>* Pn = nullptr;
		// the panels: the vestibule's on its (top) deck, a lower deck's own in its frame (the tower's axis, its line)
		const TSharedPtr<FJsonObject> TD = bV2 ? TowerDeck(V, UTF8_TO_TCHAR(Net.Lines[L].Id.c_str())) : nullptr;
		if (TD.IsValid() && TD->TryGetArrayField(TEXT("panels"), Pn))
		{
			const FTransform DF(FRotator(0.f, Yaw0 + TD->GetNumberField(TEXT("axis_yaw")), 0.f), FVector(CoreW.X, CoreW.Y, F.GetLocation().Z));
			for (const TSharedPtr<FJsonValue>& P : *Pn)
			{
				const TSharedPtr<FJsonObject> O = P->AsObject();
				const TArray<TSharedPtr<FJsonValue>>& At = O->GetArrayField(TEXT("at"));
				Spots.Add(FTransform(FRotator(0.f, O->GetNumberField(TEXT("yaw")) - TD->GetNumberField(TEXT("axis_yaw")), 0.f), FVector(At[0]->AsNumber(), At[1]->AsNumber(), 0.f)) * DF * F.Inverse());
			}
		}
		else if (bV2 && K + 1 == SimNum(St.Levels) && V->TryGetArrayField(TEXT("panels"), Pn))
		{
			for (const TSharedPtr<FJsonValue>& P : *Pn)
			{
				const TSharedPtr<FJsonObject> O = P->AsObject();
				const TArray<TSharedPtr<FJsonValue>>& At = O->GetArrayField(TEXT("at"));
				Spots.Add(FTransform(FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f), FVector(At[0]->AsNumber(), At[1]->AsNumber(), 0.f)) * B.Connector * F.Inverse());
			}
		}
		if (!Spots.Num())
		{
			for (const float Side : { -1.f, 1.f })
			{
				Spots.Add(FTransform(FRotator(0.f, Side * 90.f, 0.f), FVector(Stop, Side * (Edge - 170.f), 0.f)));
			}
		}
		for (const FTransform& Sp : Spots)
		{
			B.Panels.Add(Sp * F);
			const FTransform BoardAt(FRotator(0.f, 180.f, 0.f), FVector(0.f, 0.f, 290.f));
			Widget(S, SNew(SLyudStationBoard).Station(S).Line(L), BoardAt * Sp * F, FVector2D(900.f, 360.f), 0.27f, true)->SetTwoSided(true);
		}
	}
}

void ALyudCableway::BuildTower(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, const TArray<float>& Stops, const TArray<int32>& Sides, bool bOwnShell)
{
	const LyudTransport::FStation& St = Net.Stations[S];
	const FTransform I = FTransform::Identity;
	const float Top = Stops.Last();
	const float RoofZ = Top + HeadOver;
	const bool bLift = Top > LevelLiftMin;
	const float Mid = (LaneIn + LaneOut) * 0.5f, LaneW = LaneOut - LaneIn;

	// the ground floor and its plinth down into the ground (the tower's ground is the middle of its footprint)
	K.BoxIn(TEXT("tx_concrete"), I, FVector(0, 0, -150.f), FVector(TowerOut * 2.f + 40.f, TowerOut * 2.f + 40.f, 300.f), true, TEXT("tx_terrazzo"));
	K.BoxIn(TEXT("tx_terrazzo"), I, FVector(0, 0, -1.f), FVector(TowerOut * 2.f + 40.f, TowerOut * 2.f + 40.f, 2.f), false);

	// the stair
	const TArray<FStairPiece> Plan = PlanStair(Stops, Sides);
	TArray<FVector> RailOut, RailIn;          // the two handrails' polylines (a break where a point is NaN-free but flagged)
	TArray<bool> OutBreak, InBreak;
	auto AddRail = [](TArray<FVector>& R, TArray<bool>& Br, const FVector& P, bool bBreakBefore) { R.Add(P); Br.Add(bBreakBefore); };
	for (int32 N = 0; N < Plan.Num(); N++)
	{
		const FStairPiece& P = Plan[N];
		const int32 Sd = P.Side;
		const FVector Out = SideOut(Sd), Along = SideAlong(Sd);
		const FRotator SideRot(0.f, Sd * 90.f + 90.f, 0.f);     // X along the lane
		if (P.bFlat)
		{
			if (P.Z0 > 1.f)
			{
				K.BoxIn(TEXT("tx_concrete"), I, Lane(Sd, 0.f, Mid, P.Z0 - 12.5f), FVector(LaneIn * 2.f, LaneW, 25.f), true, TEXT("tx_terrazzo"), SideRot);
			}
		}
		else
		{
			// a flight: risers of at most MaxRiser, treads filling the lane between the corner landings; a sloped soffit
			// under it; collision is a ramp along the nosings (no stepping jitter)
			const float Rise = P.Z1 - P.Z0;
			const int32 Risers = FMath::Max(1, FMath::CeilToInt(Rise / MaxRiser));
			const float Riser = Rise / Risers;
			const int32 Treads = Risers - 1;
			const float Run = Treads > 0 ? (LaneIn * 2.f) / Treads : LaneIn * 2.f;
			for (int32 T = 0; T < Treads; T++)
			{
				const float TopZ = P.Z0 + Riser * (T + 1);
				const float U0 = -LaneIn + Run * T, U1 = U0 + Run;
				const float Bottom = P.Z0 + (Rise * (U0 + LaneIn) / (LaneIn * 2.f)) - 18.f;
				K.BoxIn(TEXT("tx_concrete"), I, Lane(Sd, (U0 + U1) * 0.5f + 0.5f, Mid, (TopZ + Bottom) * 0.5f), FVector(Run + 1.f, LaneW, TopZ - Bottom), false, TEXT("tx_tread"), SideRot);
			}
			const float Len = FMath::Sqrt(FMath::Square(LaneIn * 2.f) + FMath::Square(Rise));
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, LaneIn * 2.f));
			const FRotator Slope(Pitch, Sd * 90.f + 90.f, 0.f);
			// the soffit, 16 cm under the line from the lower landing's edge to the upper one's
			K.Box(TEXT("tx_concrete"), FTransform(Slope, Lane(Sd, 0.f, Mid, (P.Z0 + P.Z1) * 0.5f - 18.f - 8.f)), FVector(Len * 0.5f, LaneW * 0.5f, 8.f), false);
			K.CollideBox(FTransform(Slope, Lane(Sd, 0.f, Mid, (P.Z0 + P.Z1) * 0.5f - 5.f)), FVector(Len * 0.5f + 2.f, LaneW * 0.5f, 5.f));
		}
		// the corner landing after this piece (at its top), and the very first one before the ground side
		const float Zc = P.Z1;
		if (Zc > 1.f)
		{
			K.BoxIn(TEXT("tx_concrete"), I, CornerAt(Sd, Mid, Zc - 12.5f), FVector(LaneW, LaneW, 25.f), true, TEXT("tx_terrazzo"));
		}
		// handrails, 90 cm over the pitch line, 6 cm off the walls; open where a door or the way out is
		const float ROut = LaneOut - 6.f, RIn = LaneIn + 6.f;
		const float H = 90.f;
		if (P.bFlat)
		{
			// the way out on this side (U -100..100) and the lift's door (U -62..62): the rails stop short of them
			AddRail(RailOut, OutBreak, Lane(Sd, -LaneIn, ROut, P.Z0 + H), false);
			AddRail(RailOut, OutBreak, Lane(Sd, -112.f, ROut, P.Z0 + H), false);
			AddRail(RailOut, OutBreak, Lane(Sd, 112.f, ROut, P.Z0 + H), true);
			AddRail(RailOut, OutBreak, Lane(Sd, LaneIn, ROut, P.Z0 + H), false);
			AddRail(RailIn, InBreak, Lane(Sd, -LaneIn, RIn, P.Z0 + H), false);
			AddRail(RailIn, InBreak, Lane(Sd, bLift ? -74.f : -LaneIn + 1.f, RIn, P.Z0 + H), false);
			AddRail(RailIn, InBreak, Lane(Sd, bLift ? 74.f : LaneIn - 1.f, RIn, P.Z0 + H), bLift);
			AddRail(RailIn, InBreak, Lane(Sd, LaneIn, RIn, P.Z0 + H), false);
		}
		else
		{
			AddRail(RailOut, OutBreak, Lane(Sd, -LaneIn, ROut, P.Z0 + H), false);
			AddRail(RailOut, OutBreak, Lane(Sd, LaneIn, ROut, P.Z1 + H), false);
			AddRail(RailIn, InBreak, Lane(Sd, -LaneIn, RIn, P.Z0 + H), false);
			AddRail(RailIn, InBreak, Lane(Sd, LaneIn, RIn, P.Z1 + H), false);
		}
		if (N + 1 < Plan.Num())
		{
			AddRail(RailOut, OutBreak, CornerAt(Sd, ROut, Zc + H), false);
		}
	}
	auto Rails = [&](const TArray<FVector>& R, const TArray<bool>& Br, float Off)
	{
		for (int32 N = 0; N + 1 < R.Num(); N++)
		{
			if (Br[N + 1])
			{
				continue;
			}
			K.Rod(TEXT("tx_steel"), R[N], R[N + 1], 2.4f, 8, false);
			// a bracket to the wall every point
			const FVector W2 = R[N] * FVector(Off / FMath::Max(1.f, R[N].Size2D()), Off / FMath::Max(1.f, R[N].Size2D()), 1.f);
			K.Rod(TEXT("tx_steel"), R[N], FVector(W2.X, W2.Y, R[N].Z - 6.f), 1.2f, 6, false);
		}
		if (R.Num())
		{
			K.Rod(TEXT("tx_steel"), R.Last() - FVector(0, 0, 0.f), R.Last() - FVector(0, 0, 8.f), 2.4f, 8, true);
		}
	};
	Rails(RailOut, OutBreak, LaneOut + 2.f);
	Rails(RailIn, InBreak, LaneIn - 2.f);

	// the lift shaft: glass on four sides between steel corner posts, its door holes on the stops' sides
	if (bLift)
	{
		for (int32 E = 0; E < 4; E++)
		{
			TArray<FLyudMeshKit::FOpening> Holes;
			for (int32 Sp = 0; Sp < Stops.Num(); Sp++)
			{
				if ((Sides[Sp] & 3) == E)
				{
					Holes.Add({ ShaftHalf - 62.f, ShaftHalf + 62.f, Stops[Sp], Stops[Sp] + 222.f });
				}
			}
			const FTransform WF(FRotator(0.f, E * 90.f + 90.f, 0.f), SideOut(E) * ShaftHalf - SideAlong(E) * ShaftHalf);
			K.Wall(TEXT("glass"), WF, ShaftHalf * 2.f, 3.f, 0.f, Top + 280.f, Holes);
			// a steel frame round each door hole on the landing's face
			for (const FLyudMeshKit::FOpening& O : Holes)
			{
				const FVector C = SideOut(E) * (ShaftHalf + 3.f);
				for (const float U : { -65.f, 65.f })
				{
					K.BoxIn(TEXT("tx_steel"), I, C + SideAlong(E) * U + FVector(0, 0, O.Z0 + 112.f), FVector(6.f, 6.f, 224.f), false, NAME_None, FRotator(0.f, E * 90.f, 0.f));
				}
				K.BoxIn(TEXT("tx_steel"), I, C + FVector(0, 0, O.Z1 + 3.f), FVector(6.f, 136.f, 6.f), false, NAME_None, FRotator(0.f, E * 90.f, 0.f));
				K.BoxIn(TEXT("glow"), I, C + FVector(0, 0, O.Z1 + 18.f), FVector(2.f, 30.f, 10.f), false, NAME_None, FRotator(0.f, E * 90.f, 0.f));
			}
		}
		for (int32 E = 0; E < 4; E++)
		{
			K.BoxIn(TEXT("tx_steel"), I, CornerAt(E, ShaftHalf, (Top + 280.f) * 0.5f), FVector(9.f, 9.f, Top + 280.f), false);
		}
		K.BoxIn(TEXT("tx_concrete"), I, FVector(0, 0, Top + 300.f), FVector(ShaftHalf * 2.f + 10.f, ShaftHalf * 2.f + 10.f, 40.f), true);
		K.BoxIn(TEXT("tx_concrete"), I, FVector(0, 0, -6.f), FVector(ShaftHalf * 2.f - 6.f, ShaftHalf * 2.f - 6.f, 12.f), false, TEXT("tx_steel"));
	}
	else
	{
		// no lift: a solid enamel core the stair winds round
		K.BoxIn(TEXT("tx_enamel"), I, FVector(0, 0, (Top + 280.f) * 0.5f), FVector(ShaftHalf * 2.f, ShaftHalf * 2.f, Top + 280.f), true);
	}

	// the outer wall (when the vestibule's tower kit does not give one): glass from the ground to the underside of the
	// deck the vestibule stands on, enamel spandrels at the stops under it, mullions; the ways out at the lower stops
	const float WallTop = Top - DeckThick;
	for (int32 E = 0; E < 4 && bOwnShell; E++)
	{
		TArray<FLyudMeshKit::FOpening> Ways;
		for (int32 Sp = 0; Sp < Stops.Num(); Sp++)
		{
			if ((Sides[Sp] & 3) == E)
			{
				Ways.Add({ TowerHalf - 100.f, TowerHalf + 100.f, Stops[Sp], Stops[Sp] + 230.f });
			}
		}
		const FTransform WF(FRotator(0.f, E * 90.f + 90.f, 0.f), SideOut(E) * TowerHalf - SideAlong(E) * TowerHalf);
		K.Wall(TEXT("glass"), WF, TowerHalf * 2.f, 4.f, 0.f, WallTop, Ways);
		// spandrel bands at each stop, broken by the way out; an amber line under each
		for (int32 Sp = 1; Sp + 1 < Stops.Num(); Sp++)
		{
			const float Z = Stops[Sp];
			TArray<FLyudMeshKit::FOpening> Gap;
			if ((Sides[Sp] & 3) == E)
			{
				Gap.Add({ TowerHalf - 100.f, TowerHalf + 100.f, Z - 60.f, Z + 1.f });
			}
			const FTransform SF(FRotator(0.f, E * 90.f + 90.f, 0.f), SideOut(E) * (TowerHalf + 4.f) - SideAlong(E) * (TowerHalf + 4.f));
			K.Wall(TEXT("tx_enamel"), SF, TowerHalf * 2.f + 8.f, 4.f, Z - 52.f, Z, Gap, false);
			K.Wall(TEXT("tx_amber"), FTransform(SF.GetRotation(), SF.GetLocation() + SideOut(E) * 2.f), TowerHalf * 2.f + 8.f, 2.f, Z - 62.f, Z - 52.f, Gap, false);
		}
		// mullions: at the corners and at the thirds of each side
		for (const float U : { -TowerHalf * 0.34f, TowerHalf * 0.34f })
		{
			bool bInWay = false;
			for (const FLyudMeshKit::FOpening& O : Ways)
			{
				bInWay |= (U + TowerHalf > O.U0 - 6.f && U + TowerHalf < O.U1 + 6.f);
			}
			// over and under each way the mullion still runs; it stops at the openings
			float Z = 0.f;
			TArray<FVector2D> Spans;
			if (bInWay)
			{
				for (const FLyudMeshKit::FOpening& O : Ways)
				{
					Spans.Add(FVector2D(Z, O.Z0));
					Z = O.Z1;
				}
			}
			Spans.Add(FVector2D(Z, WallTop));
			for (const FVector2D& Sp : Spans)
			{
				if (Sp.Y - Sp.X > 2.f)
				{
					K.BoxIn(TEXT("tx_enamel"), I, SideOut(E) * (TowerHalf + 4.f) + SideAlong(E) * U + FVector(0, 0, (Sp.X + Sp.Y) * 0.5f), FVector(6.f, 7.f, Sp.Y - Sp.X), false, NAME_None, FRotator(0.f, E * 90.f, 0.f));
				}
			}
		}
		K.BoxIn(TEXT("tx_enamel"), I, CornerAt(E, TowerHalf + 2.f, WallTop * 0.5f), FVector(14.f, 14.f, WallTop), false);
	}
	// round the stair's well at the top (the floor's opening over the core): glass on a steel shoe with a round top
	// rail, open in the middle of the side the stair's last landing and the lift's door face
	{
		const int32 Exit = Sides.Last() & 3;
		for (int32 E = 0; E < 4; E++)
		{
			const FVector O = SideOut(E), A = SideAlong(E);
			TArray<FVector2D> Runs;
			if (E == Exit)
			{
				Runs = { FVector2D(-TowerHalf, -115.f), FVector2D(115.f, TowerHalf) };
			}
			else
			{
				Runs = { FVector2D(-TowerHalf, TowerHalf) };
			}
			for (const FVector2D& R : Runs)
			{
				const FVector P0 = O * (TowerHalf + 5.f) + A * R.X + FVector(0, 0, Top), P1 = O * (TowerHalf + 5.f) + A * R.Y + FVector(0, 0, Top);
				const float Len = FVector::Dist(P0, P1);
				const FRotator Rr(0.f, E * 90.f + 90.f, 0.f);
				K.BoxIn(TEXT("glass"), I, (P0 + P1) * 0.5f + FVector(0, 0, 58.f), FVector(Len - 4.f, 2.f, 96.f), true, NAME_None, Rr);
				K.BoxIn(TEXT("tx_steel"), I, (P0 + P1) * 0.5f + FVector(0, 0, 5.f), FVector(Len, 7.f, 10.f), false, NAME_None, Rr);
				K.Rod(TEXT("tx_steel"), P0 + FVector(0, 0, 110.f), P1 + FVector(0, 0, 110.f), 3.2f, 10, true);
				for (const FVector& P : { P0, P1 })
				{
					K.Rod(TEXT("tx_steel"), P, P + FVector(0, 0, 107.f), 2.5f, 8, false);
				}
			}
		}
	}
	(void)RoofZ;
}



void ALyudCableway::BuildDeck(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, int32 Level)
{
	const LyudTransport::FStation& St = Net.Stations[S];
	const FLyudBuiltStation& B = Built[S];
	const int32 L = St.Levels[Level].Line;
	// the deck's frame in the tower's (the kit's) frame
	const FTransform F = B.Decks[Level].GetRelativeTransform(B.Core);
	// a boarding deck between the spread tracks (when the vestibule is not there yet: the vestibule is the deck),
	// its edges G/2 - 1.05 m off the axis, as long as the vestibule's edges
	const LyudTransport::FStationGeo* Geo = SimValid(St.Geo, L) && St.Geo[L].bValid ? &St.Geo[L] : nullptr;
	const float Half = Geo ? Geo->Edge * 100.f : IslandHalf;
	const float Clear = Half - 32.f;       // nothing over the deck farther out (the hulls bulge to 1.24 m off the track)
	const float HalfLen = Geo ? Geo->Length * 50.f : IslandLen;
	const float A0 = -HalfLen, A1 = HalfLen;
	const float Stop = Geo ? Geo->StopX * 100.f : 0.f;
	const int32 End = SimValid(St.Ends, L) ? St.Ends[L] : 0;
	const bool bTop = Level == SimNum(St.Levels) - 1;

	// the tower's square in this deck's frame (the hole)
	TArray<FVector2D> Hole;
	float HoleX0 = 1e9f, HoleX1 = -1e9f;
	for (int32 E = 0; E < 4; E++)
	{
		const FVector P = F.InverseTransformPosition(CornerAt(E, TowerOut, 0.f) + FVector(0, 0, F.GetLocation().Z));
		Hole.Add(FVector2D(P.X, P.Y));
		HoleX0 = FMath::Min(HoleX0, P.X);
		HoleX1 = FMath::Max(HoleX1, P.X);
	}
	// the slab in bands across: the edge stone, the tactile strip, the floor, ...; each minus the tower
	const float Bands[] = { -Half, -Half + 40.f, -Half + 80.f, Half - 80.f, Half - 40.f, Half };
	const FName Tops[] = { TEXT("tx_concrete"), TEXT("tx_tactile"), TEXT("tx_terrazzo"), TEXT("tx_tactile"), TEXT("tx_concrete") };
	for (int32 Bd = 0; Bd < 5; Bd++)
	{
		K.SlabWithHole(TEXT("tx_enamel"), F, A0, A1, Bands[Bd], Bands[Bd + 1], Hole, -DeckThick, 0.f, true, Tops[Bd]);
	}
	// the amber belt along the deck's faces and ends
	for (const float Side : { -1.f, 1.f })
	{
		K.BoxIn(TEXT("tx_amber"), F, FVector((A0 + A1) * 0.5f, Side * (Half + 1.f), -DeckThick + 9.f), FVector(A1 - A0, 2.f, 14.f), false);
	}
	// railings: along the edges, on the hulls' line (Clear) and short of the stops; across the ends
	auto Railing = [&](const FVector2D& P0, const FVector2D& P1)
	{
		const FVector A(P0.X, P0.Y, 0.f), Bv(P1.X, P1.Y, 0.f);
		const float Len = FVector::Dist(A, Bv);
		if (Len < 40.f)
		{
			return;
		}
		const FVector D = (Bv - A) / Len;
		const float Yaw = D.Rotation().Yaw;
		// glass in a steel shoe, a round steel top rail, posts at most 1.5 m apart (no overlaps: one panel per run)
		K.BoxIn(TEXT("glass"), F, (A + Bv) * 0.5f + FVector(0, 0, 58.f), FVector(Len - 4.f, 2.f, 96.f), true, NAME_None, FRotator(0.f, Yaw, 0.f));
		K.BoxIn(TEXT("tx_steel"), F, (A + Bv) * 0.5f + FVector(0, 0, 5.f), FVector(Len, 7.f, 10.f), false, NAME_None, FRotator(0.f, Yaw, 0.f));
		K.Rod(TEXT("tx_steel"), F.TransformPosition(A + FVector(0, 0, 110.f)), F.TransformPosition(Bv + FVector(0, 0, 110.f)), 3.2f, 10, true);
		const int32 Posts = FMath::Max(1, FMath::CeilToInt(Len / 150.f));
		for (int32 Pi = 0; Pi <= Posts; Pi++)
		{
			const FVector P = A + (Bv - A) * (float(Pi) / Posts);
			K.Rod(TEXT("tx_steel"), F.TransformPosition(P), F.TransformPosition(P + FVector(0, 0, 107.f)), 2.5f, 8, false);
		}
	};
	const float RailY = Clear - 8.f;
	for (const float Side : { -1.f, 1.f })
	{
		// the cabins' doors stop at Stop +- 0.47 m (either way of travel): open from Stop - 330 to Stop + 330
		const float G0 = Stop - 330.f, G1 = Stop + 330.f;
		auto Run = [&](float X0, float X1)
		{
			// not through the tower
			if (X1 <= X0)
			{
				return;
			}
			const float T0 = HoleX0 - 10.f, T1 = HoleX1 + 10.f;
			const float TowerY = TowerOut;
			if (RailY > TowerY + 5.f || X1 < T0 || X0 > T1)
			{
				Railing(FVector2D(X0, Side * RailY), FVector2D(X1, Side * RailY));
			}
			else
			{
				Railing(FVector2D(X0, Side * RailY), FVector2D(FMath::Min(X1, T0), Side * RailY));
				Railing(FVector2D(FMath::Max(X0, T1), Side * RailY), FVector2D(X1, Side * RailY));
			}
		};
		Run(A0 + 6.f, FMath::Min(G0, A1 - 6.f));
		Run(FMath::Max(G1, A0 + 6.f), A1 - 6.f);
	}
	for (const float X : { A0, A1 })
	{
		Railing(FVector2D(X - FMath::Sign(X) * 6.f, -RailY), FVector2D(X - FMath::Sign(X) * 6.f, RailY));
	}

	// columns under the island's ends, in pairs (round concrete, an amber collar under the deck)
	const float Ground = St.Ground * 100.f - 250.f - B.Core.GetLocation().Z;
	for (const float X : { A0 + 220.f, A1 - 220.f })
	{
		if (X > HoleX0 - 150.f && X < HoleX1 + 150.f)
		{
			continue;
		}
		for (const float Y : { -Half + 90.f, Half - 90.f })
		{
			const FVector Top = F.TransformPosition(FVector(X, Y, -DeckThick));
			const FVector Bot(Top.X, Top.Y, Ground);
			TArray<FVector2D> Prof = { FVector2D(38.f, Bot.Z), FVector2D(38.f, Top.Z - 60.f), FVector2D(40.f, Top.Z - 60.f), FVector2D(40.f, Top.Z - 30.f), FVector2D(46.f, Top.Z - 30.f), FVector2D(46.f, Top.Z) };
			K.Lathe(FTransform(FVector(Top.X, Top.Y, 0.f)), Prof, { TEXT("tx_concrete"), TEXT("tx_amber"), TEXT("tx_amber"), TEXT("tx_amber"), TEXT("tx_concrete") }, 20, false, true);
		}
	}

	// the canopy: a butterfly roof over the island, low in the middle, on slim columns along the middle; it stops at
	// the tower (which rises through it) and stays inside the hulls' line
	const float CY = Clear - 10.f;
	auto Canopy = [&](float X0, float X1)
	{
		if (X1 - X0 < 100.f)
		{
			return;
		}
		const float Xm = (X0 + X1) * 0.5f, Len = X1 - X0;
		const float Tilt = FMath::RadiansToDegrees(FMath::Atan2(CanopyHigh - CanopyLow, CY));
		const float Half = FMath::Sqrt(FMath::Square(CY) + FMath::Square(CanopyHigh - CanopyLow)) * 0.5f;
		for (const float Side : { -1.f, 1.f })
		{
			const FVector C(Xm, Side * CY * 0.5f, (CanopyLow + CanopyHigh) * 0.5f + 9.f);
			K.Box(TEXT("tx_enamel"), FTransform(FRotator(0.f, 0.f, -Side * Tilt), C) * F, FVector(Len * 0.5f, Half, 9.f), true, TEXT("tx_concrete"));
			// the amber fascia at the outer edge
			K.BoxIn(TEXT("tx_amber"), F, FVector(Xm, Side * (CY + 2.f), CanopyHigh + 4.f), FVector(Len, 4.f, 30.f), false);
		}
		// the gutter beam along the valley
		K.BoxIn(TEXT("tx_steel"), F, FVector(Xm, 0.f, CanopyLow - 4.f), FVector(Len, 30.f, 16.f), false);
		// columns: every ~6 m, never by the stop's call panel
		const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 600.f));
		for (int32 C = 0; C < N; C++)
		{
			const float X = X0 + Len * (C + 0.5f) / N;
			if (FMath::Abs(X - Stop) < 120.f)
			{
				continue;
			}
			const FVector Bot = F.TransformPosition(FVector(X, 0.f, 0.f));
			TArray<FVector2D> Prof = { FVector2D(16.f, Bot.Z), FVector2D(16.f, Bot.Z + 14.f), FVector2D(11.f, Bot.Z + 20.f), FVector2D(11.f, Bot.Z + CanopyLow - 12.f) };
			K.Lathe(FTransform(FVector(Bot.X, Bot.Y, 0.f)), Prof, { TEXT("tx_steel"), TEXT("tx_steel"), TEXT("tx_enamel") }, 14, false, true);
		}
		// lamps under the canopy
		for (int32 C = 0; C < N; C++)
		{
			const float X = X0 + Len * (C + 0.5f) / N;
			for (const float Side : { -1.f, 1.f })
			{
				K.BoxIn(TEXT("glow"), F, FVector(X, Side * 150.f, CanopyLow + (CanopyHigh - CanopyLow) * 150.f / CY - 2.f), FVector(90.f, 14.f, 3.f), false);
			}
		}
	};
	const float CA0 = A0 + 20.f, CA1 = A1 - 20.f;
	// any deck over this one (Zenith): the tower takes care of the middle; the canopy runs on both sides of it
	Canopy(CA0, FMath::Min(CA1, HoleX0 - 2.f));
	Canopy(FMath::Max(CA0, HoleX1 + 2.f), CA1);
	(void)bTop;

	// the call panels: one by each edge at the stop mark, facing its track; the board's hangers from the canopy
	for (const float Side : { -1.f, 1.f })
	{
		const FTransform PanelT = FTransform(FRotator(0.f, Side * 90.f, 0.f), FVector(Stop, Side * (Half - 170.f), 0.f)) * F;
		K.BoxIn(TEXT("tx_enamel"), PanelT, FVector(0, 0, 65.f), FVector(28.f, 56.f, 130.f), true, TEXT("tx_steel"));
		K.BoxIn(TEXT("glow"), PanelT, FVector(15.f, 0, 100.f), FVector(2.f, 44.f, 28.f), false);
		K.BoxIn(TEXT("tx_amber"), PanelT, FVector(15.f, 0, 72.f), FVector(2.f, 44.f, 6.f), false);
	}

	(void)End;
}

void ALyudCableway::PlaceVestibule(const LyudTransport::FNetwork& Net, int32 S)
{
	// the hi3d vestibule (blender/transport/vestibules.py, vestibules.json v2) in the station's frame: on the first
	// deck at the station's centre, X along the line's way out, its edges by the spread tracks; its doors (ALyudDoor),
	// lamps, the route diagram, a kiosk and a bench at its spots. Missing (not made yet): the fallback boarding deck.
	const LyudTransport::FStation& St = Net.Stations[S];
	const FString Style = St.Style.empty() ? TEXT("saucer") : FString(UTF8_TO_TCHAR(St.Style.c_str()));
	const FString Cap = Style.Left(1).ToUpper() + Style.Mid(1);
	UStaticMesh* Mesh = LoadMesh(TEXT("SM_TR_Vestibule_") + Cap);
	const TSharedPtr<FJsonObject> V = VestibuleData(Style);
	FLyudBuiltStation& B = Built[S];
	const FTransform& C = B.Connector;
	FLyudMeshKit K;
	if (Mesh && V.IsValid() && V->HasField(TEXT("track_gap")))
	{
		UStaticMeshComponent* M = Single(Mesh, C, true);
		M->SetCastShadow(true);
		{
			// bare ground under it and its terrace, a metre and a half round
			const FBox Bb = Mesh->GetBoundingBox();
			BareBoxes.Add({ FTransform(C.GetRotation(), C.TransformPosition(Bb.GetCenter())), Bb.GetExtent() + FVector(150.f, 150.f, 2000.f) });
		}
		// the room's middle (Transport.Goto vestibule) and a warm light under its ceiling
		const TSharedPtr<FJsonObject>* Room = nullptr;
		if (V->TryGetObjectField(TEXT("room"), Room))
		{
			// warm lights under the ceiling, one per ~3 m of the room's box, brighter and higher in a tall room
			const float RoomH = (*Room)->HasField(TEXT("height")) ? (*Room)->GetNumberField(TEXT("height")) : 330.f;
			FVector B0 = JsonXY(*Room, TEXT("centre")) - FVector(300.f, 300.f, 0.f), B1 = JsonXY(*Room, TEXT("centre")) + FVector(300.f, 300.f, 0.f);
			const TArray<TSharedPtr<FJsonValue>>* RB = nullptr;
			if ((*Room)->TryGetArrayField(TEXT("box"), RB) && RB->Num() == 2)
			{
				const TArray<TSharedPtr<FJsonValue>>& L0 = (*RB)[0]->AsArray();
				const TArray<TSharedPtr<FJsonValue>>& L1 = (*RB)[1]->AsArray();
				B0 = FVector(FMath::Min(L0[0]->AsNumber(), L1[0]->AsNumber()), FMath::Min(L0[1]->AsNumber(), L1[1]->AsNumber()), 0.f);
				B1 = FVector(FMath::Max(L0[0]->AsNumber(), L1[0]->AsNumber()), FMath::Max(L0[1]->AsNumber(), L1[1]->AsNumber()), 0.f);
			}
			const int32 Nx = FMath::Clamp(FMath::RoundToInt((B1.X - B0.X) / 300.f), 1, 4), Ny = FMath::Clamp(FMath::RoundToInt((B1.Y - B0.Y) / 300.f), 1, 4);
			const float LightZ = FMath::Clamp(RoomH - 60.f, 250.f, 450.f);
			for (int32 Ix = 0; Ix < Nx; Ix++)
			{
				for (int32 Iy = 0; Iy < Ny; Iy++)
				{
					const FVector At(B0.X + (B1.X - B0.X) * (Ix + 0.5f) / Nx, B0.Y + (B1.Y - B0.Y) * (Iy + 0.5f) / Ny, LightZ);
					UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
					Lt->SetupAttachment(RootComponent);
					Lt->RegisterComponent();
					Lt->SetWorldLocation(C.TransformPosition(At));
					Lt->SetIntensity(RoomH > 500.f ? 9000.f : 6000.f);
					Lt->SetAttenuationRadius(700.f);
					Lt->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
					Lt->SetCastShadows(false);
					AddInstanceComponent(Lt);
				}
			}
			if (RoomH > 500.f)
			{
				// a lamp high in a tall hall (the dome, the lantern)
				UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
				Lt->SetupAttachment(RootComponent);
				Lt->RegisterComponent();
				Lt->SetWorldLocation(C.TransformPosition(JsonXY(*Room, TEXT("centre")) + FVector(0, 0, RoomH * 0.7f)));
				Lt->SetIntensity(20000.f);
				Lt->SetAttenuationRadius(RoomH * 1.6f);
				Lt->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
				Lt->SetCastShadows(false);
				AddInstanceComponent(Lt);
			}
			B.Room = FTransform(C.GetRotation(), C.TransformPosition(JsonXY(*Room, TEXT("centre")) + FVector(0, 0, 100.f)));
			if (V->HasField(TEXT("core_in_room")) && V->GetBoolField(TEXT("core_in_room")) && B.StopSides.Num())
			{
				// the core stands in the middle: 2.5 m out of its way out, looking back at it
				const int32 E = B.StopSides.Last();
				const FVector At = B.Core.TransformPosition(SideOut(E) * (TowerHalf + 250.f) + FVector(0, 0, B.Stops.Last() + 100.f));
				B.Room = FTransform((B.Core.GetLocation() - At).GetSafeNormal2D().Rotation(), At);
			}
		}
		// the doors into the room ("doors", or v1's one "door")
		TArray<TSharedPtr<FJsonObject>> DoorList;
		const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr;
		if (V->TryGetArrayField(TEXT("doors"), Ds))
		{
			for (const TSharedPtr<FJsonValue>& Dv : *Ds)
			{
				DoorList.Add(Dv->AsObject());
			}
		}
		else if (const TSharedPtr<FJsonObject>* D1 = nullptr; V->TryGetObjectField(TEXT("door"), D1))
		{
			DoorList.Add(*D1);
		}
		for (const TSharedPtr<FJsonObject>& Dd : DoorList)
		{
			const TSharedPtr<FJsonObject>* D = &Dd;
			const FVector At = JsonXY(*D, TEXT("at"));
			const FVector Hole = JsonXY(*D, TEXT("hole"));
			const float DYaw = (*D)->GetNumberField(TEXT("yaw"));
			const FTransform DoorT = FTransform(FRotator(0.f, DYaw, 0.f), At) * C;
			if (ALyudDoor* Door = GetWorld()->SpawnActorDeferred<ALyudDoor>(ALyudDoor::StaticClass(), DoorT, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Door->Size = ELyudDoorSize::FitHole;
				Door->HoleWidth = Hole.X > 1.f ? Hole.X : 164.f;
				Door->HoleHeight = Hole.Y > 1.f ? Hole.Y : 233.f;
				Door->bDoubleLeaf = true;
				Door->WallThickness = 16.f;
				Door->LeafColor = FLinearColor(0.80f, 0.76f, 0.66f);
				Door->FrameColor = FLinearColor(0.86f, 0.42f, 0.08f);
				Door->FinishSpawning(DoorT);
			}
		}
		// the furniture at its spots
		const TSharedPtr<FJsonObject>* Sp = nullptr;
		if (V->TryGetObjectField(TEXT("spots"), Sp))
		{
			auto Spot = [&](const TCHAR* Name, FTransform& Out) -> bool
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (!(*Sp)->TryGetObjectField(Name, O))
				{
					return false;
				}
				Out = FTransform(FRotator(0.f, (*O)->GetNumberField(TEXT("yaw")), 0.f), JsonXY(*O, TEXT("at")));
				return true;
			};
			FTransform T;
			if (Spot(TEXT("kiosk"), T))
			{
				// an ivory counter, glass over it, the glowing label (the spot faces the room: +X toward the visitor)
				K.BoxIn(TEXT("tx_enamel"), T, FVector(-40.f, 0.f, 55.f), FVector(110.f, 170.f, 110.f), true, TEXT("tx_amber"));
				K.BoxIn(TEXT("glass"), T, FVector(-40.f, 0.f, 170.f), FVector(100.f, 160.f, 110.f), true);
				K.BoxIn(TEXT("tx_enamel"), T, FVector(-40.f, 0.f, 230.f), FVector(116.f, 176.f, 10.f), false);
				Widget(S, SNew(SLyudLabel).Key(TEXT("transport.vestibule.kiosk")), FTransform(FRotator::ZeroRotator, FVector(20.f, 0.f, 255.f)) * T * C,
					FVector2D(600.f, 140.f), 0.25f, true);
			}
			if (Spot(TEXT("bench"), T))
			{
				K.BoxIn(TEXT("tx_amber"), T, FVector(0.f, 0.f, 44.f), FVector(46.f, 180.f, 6.f), true);
				K.BoxIn(TEXT("tx_amber"), T, FVector(-24.f, 0.f, 75.f), FVector(5.f, 180.f, 40.f), false);
				for (const float D2 : { -75.f, 75.f })
				{
					K.BoxIn(TEXT("tx_steel"), T, FVector(0.f, D2, 21.f), FVector(40.f, 5.f, 42.f), false);
				}
			}
			if (Spot(TEXT("diagram"), T))
			{
				// a free-standing enamel stele with the live diagram on its face
				K.BoxIn(TEXT("tx_enamel"), T, FVector(-10.f, 0.f, 120.f), FVector(16.f, 270.f, 240.f), true, TEXT("tx_amber"));
				const FTransform At = FTransform(FRotator::ZeroRotator, FVector(-1.f, 0.f, 150.f)) * T * C;
				Widget(S, SNew(SLyudTransitDiagram).bLive(true).bBoard(false).From(S), At,
					FVector2D(1600.f, 900.f), 0.15f, true);
				VestBoards.Add(FTransform(C.TransformPosition(T.TransformPosition(FVector(110.f, 0.f, 100.f)))));
				VestBoardStation.Add(S);
			}
		}
	}
	KitMesh(K, C, FName(*FString::Printf(TEXT("SM_VestibuleFurniture_%d"), S)));
}

bool ALyudCableway::StackTowerKit(int32 S, const FString& Style, float Height)
{
	// vestibules.json v2 "tower": {"foot": {mesh, h}, "shafts": [{mesh, h}, ...], "neck": {mesh, h}, "neck_top",
	// "decks": [{mesh, floor_z, axis_yaw, ...}]}; every module's origin on the tower's axis at its own bottom (a deck's:
	// at its floor); stacked from the ground to the vestibule's floor, the tallest shaft modules first, the short ones to
	// make up the rest, a deck at its floor_z under the vestibule's floor; nothing stretched
	const TSharedPtr<FJsonObject> V = VestibuleData(Style);
	const TSharedPtr<FJsonObject>* T = nullptr;
	if (!V.IsValid() || !V->TryGetObjectField(TEXT("tower"), T))
	{
		return false;
	}
	auto Module = [&](const TSharedPtr<FJsonObject>& O, UStaticMesh*& Mesh, float& H)
	{
		Mesh = LoadMesh(O->GetStringField(TEXT("mesh")));
		H = O->GetNumberField(TEXT("h"));
		return Mesh && H > 1.f;
	};
	UStaticMesh* Foot = nullptr; UStaticMesh* Neck = nullptr;
	float FootH = 0.f, NeckH = 0.f;
	const TSharedPtr<FJsonObject>* Fo = nullptr;
	const TSharedPtr<FJsonObject>* Ne = nullptr;
	if (!(*T)->TryGetObjectField(TEXT("foot"), Fo) || !(*T)->TryGetObjectField(TEXT("neck"), Ne) || !Module(*Fo, Foot, FootH) || !Module(*Ne, Neck, NeckH))
	{
		return false;
	}
	TArray<TPair<UStaticMesh*, float>> Shafts;
	for (const TSharedPtr<FJsonValue>& Sv : (*T)->GetArrayField(TEXT("shafts")))
	{
		UStaticMesh* M = nullptr;
		float H = 0.f;
		if (Module(Sv->AsObject(), M, H))
		{
			Shafts.Add({ M, H });
		}
	}
	if (!Shafts.Num())
	{
		return false;
	}
	Shafts.Sort([](const TPair<UStaticMesh*, float>& A, const TPair<UStaticMesh*, float>& Bm) { return A.Value > Bm.Value; });
	const float NeckTop = Height + ((*T)->HasField(TEXT("neck_top")) ? (*T)->GetNumberField(TEXT("neck_top")) : -DeckThick);
	const FTransform& Core = Built[S].Core;
	// the pieces between Z0 and Z1: whole modules, the last short one sunk into the piece over it where they do not add up
	auto Fill = [&](float Z0, float Z1)
	{
		float Z = Z0, Left = Z1 - Z0;
		for (const TPair<UStaticMesh*, float>& Sh : Shafts)
		{
			while (Left >= Sh.Value - 0.5f)
			{
				Single(Sh.Key, FTransform(FVector(0, 0, Z)) * Core, true);
				Z += Sh.Value;
				Left -= Sh.Value;
			}
		}
		if (Left > 0.5f)
		{
			Single(Shafts.Last().Key, FTransform(FVector(0, 0, Z1 - Shafts.Last().Value)) * Core, true);
		}
	};
	const TSharedPtr<FJsonObject>* Bs = nullptr;
	if ((*T)->TryGetObjectField(TEXT("base"), Bs))
	{
		if (UStaticMesh* BaseM = LoadMesh((*Bs)->GetStringField(TEXT("mesh"))))
		{
			Single(BaseM, FTransform(FVector(0, 0, -(*Bs)->GetNumberField(TEXT("h")))) * Core, true);
		}
	}
	Single(Foot, Core, true);
	float Z = FootH;
	// the decks on the tower, from the lowest
	TArray<TSharedPtr<FJsonObject>> Decks;
	const TArray<TSharedPtr<FJsonValue>>* Ds = nullptr;
	if ((*T)->TryGetArrayField(TEXT("decks"), Ds))
	{
		for (const TSharedPtr<FJsonValue>& D : *Ds)
		{
			Decks.Add(D->AsObject());
		}
	}
	Decks.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& Bd) { return A->GetNumberField(TEXT("floor_z")) < Bd->GetNumberField(TEXT("floor_z")); });
	for (const TSharedPtr<FJsonObject>& D : Decks)
	{
		UStaticMesh* M = LoadMesh(D->GetStringField(TEXT("mesh")));
		if (!M)
		{
			continue;
		}
		const float Floor = Height + D->GetNumberField(TEXT("floor_z"));
		const FBox Bb = M->GetBoundingBox();
		Fill(Z, Floor + Bb.Min.Z);
		// (the mesh is already turned to its line inside: the tower's own yaw)
		Single(M, FTransform(FVector(0, 0, Floor)) * Core, true);
		Z = Floor + Bb.Max.Z;
	}
	if (NeckTop - NeckH < Z - 1.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Transport: %s's tower kit does not fit %.0f cm (foot %.0f, neck %.0f)"), *Style, Height, FootH, NeckH);
	}
	Fill(Z, NeckTop - NeckH);
	Single(Neck, FTransform(FVector(0, 0, NeckTop - NeckH)) * Core, true);
	return true;
}

void ALyudCableway::BuildRopeGear(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S)
{
	// in the tower's frame (K's): per line through the station, a sheave post at each bend of a rope at the vestibule's
	// ends (the convergence pylons are line pylons, BuildPylons), and at an end station the turn wheel on its mast
	const LyudTransport::FStation& St = Net.Stations[S];
	const FTransform& Core = Built[S].Core;
	auto GroundAt = [&](const FVector& Wp) -> float
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("GearGround"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Wp + FVector(0, 0, 500.f), Wp - FVector(0, 0, 8000.f), ECC_WorldStatic, Q))
		{
			return (float)Hit.ImpactPoint.Z;
		}
		return St.Ground * 100.f;
	};
	for (int32 L = 0; L < SimNum(St.Geo); L++)
	{
		const LyudTransport::FStationGeo& G = St.Geo[L];
		if (!G.bValid)
		{
			continue;
		}
		for (const LyudTransport::FSheave& Sh : G.Sheaves)
		{
			if (Sh.bConverge)
			{
				continue;
			}
			// the sheave a little under the rope, its post outside the track (never under the passing cabins), an arm
			const FVector At = W(Sh.X, Sh.Y, Sh.Z);
			const FVector Out = FRotator(0.f, Sh.Yaw + (Sh.Side > 0 ? 90.f : -90.f), 0.f).Vector();
			const FVector PostTop = At + Out * 190.f + FVector(0, 0, 60.f);
			const float G0 = GroundAt(At + Out * 190.f) - 60.f;
			const FVector Lp = Core.InverseTransformPosition(PostTop), Lb = Core.InverseTransformPosition(FVector(PostTop.X, PostTop.Y, G0));
			TArray<FVector2D> Prof = { FVector2D(30.f, Lb.Z), FVector2D(24.f, Lp.Z - 40.f), FVector2D(30.f, Lp.Z - 40.f), FVector2D(30.f, Lp.Z) };
			K.Lathe(FTransform(FVector(Lp.X, Lp.Y, 0.f)), Prof, { TEXT("tx_plate"), TEXT("tx_amber"), TEXT("tx_amber") }, 16, true, true);
			const FVector La = Core.InverseTransformPosition(At + FVector(0, 0, 60.f));
			K.Rod(TEXT("tx_steel"), Lp - FVector(0, 0, 15.f), La, 9.f, 10, true);
			// the sheave: a dark disc in the rope's plane, a steel hub
			const FTransform Disc(FRotator(0.f, Sh.Yaw - Core.Rotator().Yaw, 90.f), Core.InverseTransformPosition(At + FVector(0, 0, 30.f)));
			TArray<FVector2D> Wheel = { FVector2D(8.f, -6.f), FVector2D(40.f, -5.f), FVector2D(45.f, 0.f), FVector2D(40.f, 5.f), FVector2D(8.f, 6.f) };
			K.Lathe(Disc, Wheel, { TEXT("tx_dark"), TEXT("tx_dark"), TEXT("tx_dark"), TEXT("tx_steel") }, 20, false, false);
		}
		if (G.bWheel)
		{
			// the turn wheel at the rope's height on a mast from the ground; the cabins' grips ride its rim
			const FVector Wh = W(G.Wheel.X, G.Wheel.Y, G.Wheel.Z);
			const FVector Lw = Core.InverseTransformPosition(Wh);
			const float Base = Core.InverseTransformPosition(FVector(Wh.X, Wh.Y, GroundAt(Wh) - 60.f)).Z;
			const float R = 450.f;
			TArray<FVector2D> Rim = { FVector2D(70.f, Lw.Z + 20.f), FVector2D(R - 20.f, Lw.Z + 12.f), FVector2D(R + 6.f, Lw.Z + 4.f), FVector2D(R + 6.f, Lw.Z - 4.f),
				FVector2D(R - 20.f, Lw.Z - 12.f), FVector2D(70.f, Lw.Z - 22.f) };
			K.Lathe(FTransform(FVector(Lw.X, Lw.Y, 0.f)), Rim, { TEXT("tx_steel"), TEXT("tx_dark"), TEXT("tx_dark"), TEXT("tx_dark"), TEXT("tx_steel") }, 48, false, false);
			for (int32 Sp = 0; Sp < 6; Sp++)
			{
				const FVector D = FRotator(0.f, Sp * 60.f, 0.f).Vector();
				K.Rod(TEXT("tx_steel"), FVector(Lw.X, Lw.Y, Lw.Z) + D * 70.f, FVector(Lw.X, Lw.Y, Lw.Z) + D * (R - 20.f), 6.f, 8, false);
			}
			// the mast comes up from under (the grips pass over the rim, the hangers outside it)
			TArray<FVector2D> Mast = { FVector2D(90.f, Base), FVector2D(70.f, Lw.Z - 60.f), FVector2D(90.f, Lw.Z - 60.f), FVector2D(90.f, Lw.Z - 22.f) };
			K.Lathe(FTransform(FVector(Lw.X, Lw.Y, 0.f)), Mast, { TEXT("tx_plate"), TEXT("tx_amber"), TEXT("tx_amber") }, 20, false, true);
		}
	}
}

void ALyudCableway::ClearGround()
{
	// temporary blockers that only answer Visibility traces (the ones grass and forests scatter with): the scatter's
	// trace meets them first, sees this actor's NoGrass / NoTree tags and leaves the spot bare
	TArray<UBoxComponent*> Blockers;
	for (const TPair<FTransform, FVector>& Bb : BareBoxes)
	{
		UBoxComponent* Bx = NewObject<UBoxComponent>(this);
		Bx->SetupAttachment(RootComponent);
		Bx->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Bx->SetCollisionResponseToAllChannels(ECR_Ignore);
		Bx->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Bx->SetBoxExtent(Bb.Value);
		Bx->RegisterComponent();
		Bx->SetWorldTransform(Bb.Key);
		Blockers.Add(Bx);
	}
	auto Overlaps = [&](const FTransform& At, const FVector2D& Half)
	{
		for (const TPair<FTransform, FVector>& Bb : BareBoxes)
		{
			const FVector L = At.InverseTransformPosition(Bb.Key.GetLocation());
			const float R = FVector2D(Bb.Value.X, Bb.Value.Y).Size();
			if (FMath::Abs(L.X) < Half.X + R && FMath::Abs(L.Y) < Half.Y + R)
			{
				return true;
			}
		}
		return false;
	};
	int32 NGrass = 0, NForest = 0;
	for (TActorIterator<AProceduralGrass> It(GetWorld()); It; ++It)
	{
		if (Overlaps(It->GetActorTransform(), It->HalfExtent))
		{
			It->Rebuild();
			NGrass++;
		}
	}
	for (TActorIterator<AProceduralForest> It(GetWorld()); It; ++It)
	{
		if (Overlaps(It->GetActorTransform(), It->HalfExtent))
		{
			It->Rebuild();
			NForest++;
		}
	}
	for (UBoxComponent* Bx : Blockers)
	{
		Bx->DestroyComponent();
	}
	UE_LOG(LogTemp, Log, TEXT("Transport: ground cleared under %d boxes: %d grass patches, %d forests scattered again"), BareBoxes.Num(), NGrass, NForest);
}

bool ALyudCableway::View(int32 S, const FString& Name, FTransform& Out) const
{
	if (!Built.IsValidIndex(S) || !Built[S].Stops.Num())
	{
		return false;
	}
	const FLyudBuiltStation& B = Built[S];
	const float Mid = (LaneIn + LaneOut) * 0.5f;
	const int32 G = B.StopSides[0];
	if (Name == TEXT("lift"))
	{
		// on the ground landing by the lift's door, looking at it
		Out = FTransform(FRotator(0.f, B.Core.Rotator().Yaw + G * 90.f + 180.f, 0.f), B.Core.TransformPosition(SideOut(G) * (Mid + 20.f) + FVector(0, 0, 100.f)));
		return true;
	}
	if (Name == TEXT("liftin"))
	{
		Out = FTransform(FRotator(0.f, B.Core.Rotator().Yaw + G * 90.f, 0.f), B.Core.TransformPosition(FVector(0, 0, 100.f)));
		return true;
	}
	if (Name == TEXT("top"))
	{
		const int32 T = B.StopSides.Last();
		Out = FTransform(FRotator(-4.f, B.Core.Rotator().Yaw + T * 90.f, 0.f), B.Core.TransformPosition(SideOut(T) * Mid + FVector(0, 0, B.Stops.Last() + 100.f)));
		return true;
	}
	if (Name == TEXT("stair"))
	{
		// on the first corner landing up, looking up the stair
		const int32 T = (G + 1) & 3;
		Out = FTransform(FRotator(18.f, B.Core.Rotator().Yaw + T * 90.f + 90.f, 0.f), B.Core.TransformPosition(CornerAt(G, Mid, 0.f) + FVector(0, 0, 100.f)));
		return true;
	}
	if (Name == TEXT("tower"))
	{
		// 40 m off across the line, a little up: the vestibule on its tower between the spread ropes
		const FVector Across = B.Core.TransformVectorNoScale(FVector(0.f, 1.f, 0.f));
		const FVector P = B.Core.GetLocation() + Across * 4000.f + B.Core.GetRotation().Vector() * 1500.f + FVector(0, 0, B.Stops.Last() * 0.6f + 300.f);
		Out = FTransform((B.Core.GetLocation() + FVector(0, 0, B.Stops.Last() * 0.6f) - P).Rotation(), P);
		return true;
	}
	if (Name == TEXT("edge"))
	{
		// on the first deck's +Y edge by the stop mark, looking along it
		if (!B.Decks.Num())
		{
			return false;
		}
		const LyudTransport::FNetwork* Net = Transport ? Transport->GetNet() : nullptr;
		const int32 L = B.DeckLines[0];
		const LyudTransport::FStationGeo* Geo = Net && SimValid(Net->Stations[S].Geo, L) && Net->Stations[S].Geo[L].bValid ? &Net->Stations[S].Geo[L] : nullptr;
		const float Edge = Geo ? Geo->Edge * 100.f : IslandHalf;
		Out = FTransform(FRotator(-5.f, B.Decks[0].Rotator().Yaw + 160.f, 0.f), B.Decks[0].TransformPosition(FVector(800.f, Edge - 120.f, 170.f)));
		return true;
	}
	return false;
}

int32 ALyudCableway::Probe(int32 S) const
{
	if (!Built.IsValidIndex(S) || !Built[S].Stops.Num())
	{
		return 0;
	}
	const FLyudBuiltStation& B = Built[S];
	const float Mid = (LaneIn + LaneOut) * 0.5f;
	int32 Fails = 0;
	auto Down = [&](const TCHAR* What, const FVector& P, float WantZ, float Tol)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("StationProbe"), false);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 150.f), P - FVector(0, 0, 400.f), ECC_Pawn, Q);
		const bool bOk = bHit && FMath::Abs(Hit.ImpactPoint.Z - WantZ) < Tol;
		Fails += !bOk;
		UE_LOG(LogTemp, Display, TEXT("STATIONPROBE %d %s %s: %s want %.0f got %s"), S, bOk ? TEXT("ok") : TEXT("FAIL"), What, *P.ToCompactString(), WantZ,
			bHit ? *FString::Printf(TEXT("%.0f (%s)"), Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetComponent())) : TEXT("nothing"));
	};
	const float G = B.Core.GetLocation().Z;
	for (int32 K = 0; K < B.Stops.Num(); K++)
	{
		const int32 Sd = B.StopSides[K];
		Down(TEXT("landing"), B.Core.TransformPosition(SideOut(Sd) * Mid + FVector(0, 0, B.Stops[K])), G + B.Stops[K], 8.f);
		if (K > 0)
		{
			Down(TEXT("way out"), B.Core.TransformPosition(SideOut(Sd) * (TowerOut + 60.f) + FVector(0, 0, B.Stops[K])), G + B.Stops[K], 8.f);
		}
		// three quarters round from this stop's side: on the stair's flight or a corner
		Down(TEXT("corner"), B.Core.TransformPosition(CornerAt(Sd, Mid, 0.f) + FVector(0, 0, B.Stops[K])), G + B.Stops[K], 8.f);
	}
	// the boarding strips along each edge round the stop mark (where people wait and step in)
	const LyudTransport::FNetwork* Net = Transport ? Transport->GetNet() : nullptr;
	for (int32 K = 0; K < B.Decks.Num() && Net; K++)
	{
		const LyudTransport::FStation& St = Net->Stations[S];
		const int32 L = B.DeckLines[K];
		if (!SimValid(St.Geo, L) || !St.Geo[L].bValid)
		{
			continue;
		}
		const LyudTransport::FStationGeo& Ge = St.Geo[L];
		for (const float X : { -250.f, 0.f, 250.f })
		{
			for (const float Side : { -1.f, 1.f })
			{
				Down(TEXT("edge"), B.Decks[K].TransformPosition(FVector(Ge.StopX * 100.f + X, Side * (Ge.Edge * 100.f - 60.f), 0.f)), B.Decks[K].GetLocation().Z, 8.f);
			}
		}
	}
	// the street door's landing outside
	// (on a kit's podium: its steps go down from the door, up to its 45 cm)
	Down(TEXT("door landing"), B.Core.TransformPosition(SideOut(B.StopSides[0]) * (TowerOut + 70.f)), G - 25.f, 30.f);
	return Fails;
}

// --- the pylons -------------------------------------------------------------------------------------------------

void ALyudCableway::BuildPylons(const LyudTransport::FNetwork& Net, int32 L)
{
	const FKit& Kt = Kit();
	UStaticMesh* Feet = LoadMesh(TEXT("SM_TR_PylonLine_Feet"));
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
			const float Sc = H / (100.f * Kt.CornerSaddle);
			Single(Corner, FTransform(FRotator(0.f, P.Yaw, 0.f), G - FVector(0, 0, 30.f), FVector(Sc)), true);
			continue;
		}
		const FRotator R(0.f, P.Yaw - 90.f, 0.f);       // the crossarm across the line
		const float HeadBase = Top - Kt.PylonHeadSaddle;
		// the shaft: a clean tapered round of painted steel plate from inside the feet' sleeve to inside the head's
		// collar, the amber bands at fixed heights (never stretched), a little swell (entasis)
		const float Z0 = Kt.PylonFeet - 120.f, Z1 = HeadBase - G.Z - 25.f;
		const float R0 = 136.f, R1 = 46.f;
		auto Rad = [&](float Z) { const float T = FMath::Clamp((Z - Z0) / FMath::Max(1.f, Z1 - Z0), 0.f, 1.f); return FMath::Lerp(R0, R1, T) + 6.f * FMath::Sin(T * PI); };
		TArray<FVector2D> Prof;
		TArray<FName> BandMats;
		TArray<FVector2D> Bands = { FVector2D(Kt.PylonFeet + 90.f, Kt.PylonFeet + 160.f), FVector2D(Kt.PylonFeet + 250.f, Kt.PylonFeet + 320.f), FVector2D(Z1 - 260.f, Z1 - 200.f) };
		TArray<float> Zs;
		const int32 Seg = FMath::Clamp(FMath::CeilToInt((Z1 - Z0) / 300.f), 4, 16);
		for (int32 I = 0; I <= Seg; I++)
		{
			Zs.Add(FMath::Lerp(Z0, Z1, float(I) / Seg));
		}
		for (const FVector2D& Bd : Bands)
		{
			if (Bd.X > Z0 + 20.f && Bd.Y < Z1 - 20.f)
			{
				Zs.Append({ Bd.X, Bd.Y });
			}
		}
		Zs.Sort();
		for (int32 I = 0; I < Zs.Num(); I++)
		{
			if (I > 0 && Zs[I] - Zs[I - 1] < 1.f)
			{
				continue;
			}
			bool bBand = false;
			for (const FVector2D& Bd : Bands)
			{
				bBand |= Zs[I] >= Bd.X - 0.5f && Zs[I] < Bd.Y - 0.5f;
			}
			if (Prof.Num())
			{
				BandMats.Add(bBand ? TEXT("tx_amber") : TEXT("tx_plate"));
			}
			Prof.Add(FVector2D(Rad(Zs[I]) + 0.f, Zs[I]));
		}
		// a band's material is its segment's; the band stands a hair proud (1.5 cm) so it reads as a ring
		for (int32 I = 0; I + 1 < Prof.Num(); I++)
		{
			if (BandMats[I] == TEXT("tx_amber"))
			{
				Prof[I].X += 1.5f;
				Prof[I + 1].X += 1.5f;
			}
		}
		FLyudMeshKit K;
		K.Lathe(FTransform::Identity, Prof, BandMats, 40, true, true);
		KitMesh(K, FTransform(R, G - FVector(0, 0, 20.f)), TEXT("SM_PylonShaft"));
		if (Feet && Head)
		{
			Single(Feet, FTransform(R, G - FVector(0, 0, 20.f)), true);
			Single(Head, FTransform(R, FVector(G.X, G.Y, HeadBase)), false);
		}
	}
}

// --- the depot ------------------------------------------------------------------------------------------------

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
	// past the turn wheel, along the line away from the station (DepotAt: origin 6.5 m past the wheel, on the deck)
	if (!B.DepotAt.IsValidIndex(K) || B.DepotAt[K].Equals(FTransform::Identity))
	{
		return;
	}
	const FTransform& F = B.DepotAt[K];
	const float Dir = 1.f;
	int32 Count = 0;
	for (const LyudTransport::FCabin& C : Net.Cabins)
	{
		Count += C.Line == L;
	}
	// a hall on columns past the turn wheel: a concrete deck, glass sides on enamel posts, a sawtooth roof with glass
	FLyudMeshKit Kt;
	const FTransform I = FTransform::Identity;
	const float X0 = 0.f, Len = 560.f * Count + 400.f;
	const float Xc = X0 + Dir * Len * 0.5f;
	Kt.BoxIn(TEXT("tx_enamel"), I, FVector(Xc, 0.f, -DeckThick * 0.5f), FVector(Len, 760.f, DeckThick), true, TEXT("tx_concrete"));
	for (const float Y : { -370.f, 370.f })
	{
		Kt.BoxIn(TEXT("glass"), I, FVector(Xc, Y, 260.f), FVector(Len, 4.f, 520.f), true);
		const int32 N = FMath::Max(2, FMath::RoundToInt(Len / 300.f));
		for (int32 P = 0; P <= N; P++)
		{
			Kt.BoxIn(TEXT("tx_enamel"), I, FVector(X0 + Dir * Len * P / N, Y, 270.f), FVector(16.f, 16.f, 540.f), false);
		}
		Kt.BoxIn(TEXT("tx_amber"), I, FVector(Xc, Y, 540.f), FVector(Len, 20.f, 30.f), false);
	}
	Kt.BoxIn(TEXT("tx_enamel"), I, FVector(X0 + Dir * Len, 0.f, 270.f), FVector(20.f, 760.f, 540.f), true);
	const int32 Teeth = FMath::Max(2, Count);
	for (int32 T = 0; T < Teeth; T++)
	{
		const float X = X0 + Dir * (Len * (T + 0.5f) / Teeth);
		Kt.BoxIn(TEXT("tx_enamel"), I, FVector(X, 0.f, 600.f), FVector(Len / Teeth + 4.f, 780.f, 16.f), false, TEXT("tx_concrete"), FRotator(Dir * 14.f, 0.f, 0.f));
		Kt.BoxIn(TEXT("glass"), I, FVector(X + Dir * Len / Teeth * 0.5f, 0.f, 590.f), FVector(4.f, 760.f, 90.f), false);
	}
	Kt.BoxIn(TEXT("tx_steel"), I, FVector(Xc, 0.f, ALyudCabin::GripOverFloor()), FVector(Len, 14.f, 14.f), false);
	// the rail on to it from the wheel
	Kt.BoxIn(TEXT("tx_steel"), I, FVector(-325.f, 0.f, ALyudCabin::GripOverFloor()), FVector(650.f, 14.f, 14.f), false);
	const float G = Net.Stations[S].Ground * 100.f - 200.f - F.GetLocation().Z;
	for (const float X : { X0 + Dir * 150.f, X0 + Dir * (Len - 150.f) })
	{
		for (const float Y : { -260.f, 260.f })
		{
			TArray<FVector2D> Prof = { FVector2D(50.f, G), FVector2D(50.f, -DeckThick) };
			Kt.Lathe(FTransform(FVector(X, Y, 0.f)), Prof, { TEXT("tx_concrete") }, 20, false, true);
		}
	}
	KitMesh(Kt, F, TEXT("SM_Depot"));
}

static TAutoConsoleVariable<int32> CVarLiftLog(TEXT("lyud.Transport.LiftLog"), 0, TEXT("1: the station lifts log their cab and the player once a second."));

// --- the lift -------------------------------------------------------------------------------------------------

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

void ALyudStationLift::Setup(ALyudCableway* Way, const TArray<float>& InStops, const TArray<int32>& InSides)
{
	Stops = InStops;
	DoorSides = InSides;
	Z = Stops.Num() ? Stops[0] : 0.f;
	// the cab: a terrazzo floor, a steel ceiling with a lamp, glass walls but on its door sides, a handrail
	const float H = ShaftHalf - 8.f;
	FLyudMeshKit K;
	K.BoxIn(TEXT("tx_steel"), FTransform::Identity, FVector(0, 0, -6.f), FVector(H * 2.f, H * 2.f, 12.f), true, TEXT("tx_terrazzo"));
	K.BoxIn(TEXT("tx_steel"), FTransform::Identity, FVector(0, 0, 236.f), FVector(H * 2.f, H * 2.f, 8.f), false);
	K.BoxIn(TEXT("glow"), FTransform::Identity, FVector(0, 0, 231.f), FVector(80.f, 80.f, 2.f), false);
	TSet<int32> Doors;
	for (const int32 D : DoorSides)
	{
		Doors.Add(D & 3);
	}
	for (int32 E = 0; E < 4; E++)
	{
		const FRotator R(0.f, E * 90.f, 0.f);
		const FVector O = SideOut(E);
		if (!Doors.Contains(E))
		{
			K.BoxIn(TEXT("glass"), FTransform::Identity, O * (H - 2.f) + FVector(0, 0, 116.f), FVector(3.f, H * 2.f, 232.f), true, NAME_None, R);
			K.Rod(TEXT("tx_steel"), O * (H - 8.f) - SideAlong(E) * (H - 20.f) + FVector(0, 0, 92.f), O * (H - 8.f) + SideAlong(E) * (H - 20.f) + FVector(0, 0, 92.f), 2.2f, 8, true);
		}
		else
		{
			// the cab's own door posts
			for (const float U : { -H + 15.f, H - 15.f })
			{
				K.BoxIn(TEXT("tx_steel"), FTransform::Identity, O * (H - 4.f) + SideAlong(E) * (U > 0 ? U + 7.f : U - 7.f) * 0.f + SideAlong(E) * U + FVector(0, 0, 116.f), FVector(6.f, FMath::Max(2.f, H - 62.f - 2.f) * 0.f + 30.f, 232.f), false, NAME_None, R);
			}
		}
	}
	for (int32 E = 0; E < 4; E++)
	{
		K.BoxIn(TEXT("tx_steel"), FTransform::Identity, CornerAt(E, H - 4.f, 116.f), FVector(8.f, 8.f, 232.f), false);
	}
	Way->KitMesh(K, FTransform::Identity, TEXT("SM_LiftCab"), true, Cab);
	// the landing doors: two steel leaves at each stop, sliding apart
	FLyudMeshKit Lf;
	Lf.BoxIn(TEXT("tx_steel"), FTransform::Identity, FVector(0, 0, 111.f), FVector(4.f, 62.f, 222.f), true);
	for (int32 I = 0; I < Stops.Num(); I++)
	{
		for (int32 Lr = 0; Lr < 2; Lr++)
		{
			Leaves.Add(Way->KitMesh(Lf, FTransform::Identity, TEXT("SM_LiftLeaf"), true, Root));
		}
	}
	Open.Init(0.f, Stops.Num());
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
	const float HalfIn = ShaftHalf - 15.f;
	const bool bIn = FMath::Abs(Local.X) < HalfIn && FMath::Abs(Local.Y) < HalfIn && Local.Z > Z - 20.f && Local.Z < Z + 260.f;
	const int32 Here = StopNear(Z);
	if (bIn)
	{
		OutTime = 0.f;
	}
	const bool bDoorsShut = Open.IsValidIndex(Here) ? Open[Here] < 0.02f : true;
	if (Target < 0)
	{
		if (bIn && bRode)
		{
			// just brought here: it waits until the rider has stepped out (and in again)
		}
		else if (bIn)
		{
			// standing in the cab a moment: the doors close and it goes to the next stop up (from the top: the ground)
			Stand += DeltaTime;
			if (Stand > 1.5f)
			{
				const int32 Next = Here == Stops.Num() - 1 ? 0 : (Here == 0 ? Stops.Num() - 1 : Here + 1);
				Target = Next;
				Stand = 0.f;
				Dwell = 0.f;
			}
		}
		else
		{
			Stand = 0.f;
			// stepped out for good (not a bump as the cab stops): it may take them again
			OutTime += DeltaTime;
			bRode = bRode && OutTime < 0.6f;
			// somebody waiting by a landing door the cab is not at: it comes
			const int32 Near = StopNear(Local.Z);
			if (Pawn && Near != Here && FMath::Abs(Local.Z - Stops[Near]) < 150.f && FVector2D(Local.X, Local.Y).Size() < 420.f)
			{
				Target = Near;
				Dwell = 0.f;
			}
		}
	}
	if (CVarLiftLog.GetValueOnGameThread() && Pawn)
	{
		LogT += DeltaTime;
		if (LogT > 1.f)
		{
			LogT = 0.f;
			UE_LOG(LogTemp, Display, TEXT("Lift %s: cab %.0f target %d here %d open %.2f rider local %s in %d"), *GetName(), Z, Target, Here, Open.IsValidIndex(Here) ? Open[Here] : -1.f, *Local.ToCompactString(), bIn ? 1 : 0);
		}
	}
	// the doors at the cab's stop open while it stands there; they shut before it moves
	const bool bLeaving = Target >= 0 && Target != Here;
	for (int32 I = 0; I < Stops.Num(); I++)
	{
		const bool bWant = I == Here && FMath::IsNearlyEqual(Z, Stops[I], 1.f) && !bLeaving;
		Open[I] = FMath::FInterpConstantTo(Open[I], bWant ? 1.f : 0.f, DeltaTime, 1.6f);
	}
	if (Target >= 0)
	{
		if (Target == Here && FMath::IsNearlyEqual(Z, Stops[Target], 1.f))
		{
			// arrived (within a centimetre: the last step may stop short of the half-centimetre test below)
			Z = Stops[Target];
			Target = -1;
			bRode = bRode || (bIn && Dwell > 0.f);
			Dwell = 0.f;
		}
		else if (bDoorsShut)
		{
			if (FMath::IsNearlyEqual(Z, Stops[Here], 1.f) && Dwell == 0.f)
			{
				Dwell = 1.f;
				if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
				{
					Sound->PlayEffect(TEXT("lift_start"), Cab->GetComponentLocation() + FVector(0, 0, 120.f), 0.8f);
				}
			}
			const float Goal = Stops[Target];
			const float Dist = Goal - Z;
			const float Speed = FMath::Min(250.f, FMath::Sqrt(2.f * 120.f * FMath::Abs(Dist)) + 20.f);
			float Step = FMath::Clamp(Dist, -Speed * DeltaTime, Speed * DeltaTime);
			// never down onto somebody in the shaft under the cab (only a teleport or a fall puts them there)
			const bool bUnder = Pawn && FMath::Abs(Local.X) < HalfIn + 20.f && FMath::Abs(Local.Y) < HalfIn + 20.f && Local.Z < Z - 20.f && Local.Z > Z - 320.f;
			if (Step < 0.f && bUnder)
			{
				Step = 0.f;
			}
			Z += Step;
			// the rider goes with the cab (CharacterMovement does not ride a floor moved without a sweep)
			if (bIn && Pawn)
			{
				const_cast<APawn*>(Pawn)->AddActorWorldOffset(FVector(0, 0, Step), false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (FMath::IsNearlyEqual(Z, Goal, 0.5f))
			{
				Z = Goal;
				Target = -1;
				Dwell = 0.f;
				bRode = bIn;
				if (ULyudSoundscapeSubsystem* Sound = ULyudSoundscapeSubsystem::Get(this))
				{
					Sound->PlayEffect(TEXT("lift_ding"), Cab->GetComponentLocation() + FVector(0, 0, 200.f), 0.7f);
				}
			}
			Cab->SetRelativeLocation(FVector(0, 0, Z));
		}
	}
	// the leaves: in the shaft's door hole at each stop, sliding apart along the wall as they open
	for (int32 I = 0; I < Stops.Num() && I < DoorSides.Num(); I++)
	{
		const int32 E = DoorSides[I] & 3;
		const float Ease = Open[I] * Open[I] * (3.f - 2.f * Open[I]);
		for (int32 Lr = 0; Lr < 2; Lr++)
		{
			UStaticMeshComponent* Lf = Leaves.IsValidIndex(I * 2 + Lr) ? Leaves[I * 2 + Lr].Get() : nullptr;
			if (!Lf)
			{
				continue;
			}
			const float Sg = Lr ? 1.f : -1.f;
			const float U = Sg * (31.f + Ease * 60.f);
			Lf->SetRelativeLocationAndRotation(SideOut(E) * (ShaftHalf - 5.f) + SideAlong(E) * U + FVector(0, 0, Stops[I]), FRotator(0.f, E * 90.f, 0.f));
			Lf->SetCollisionEnabled(Ease > 0.8f ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		}
	}
}
