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
	const LyudTransport::FStation& St = Net.Stations[S];
	FLyudBuiltStation& B = Built[S];
	const FVector Centre = W(St.X, St.Y, 0.f);
	StationCentres[S] = Centre;
	StationRadius[S] = IslandLen + 3400.f;
	const float Yaw0 = SimNum(St.Levels) && SimValid(St.AxisYaw, St.Levels[0].Line) ? St.AxisYaw[St.Levels[0].Line] : 0.f;
	FVector CoreW = W(St.CoreX, St.CoreY, St.CoreGround);
	{
		// the floor 15 cm over the landscape as it stands in this level under the tower and the gallery either way
		// (network_built.json's ground is the map's; a landscape built from older inputs can stand a little higher)
		const FTransform F0(FRotator(0.f, Yaw0, 0.f), CoreW);
		float Top = -1e9f;
		for (float Y = -1050.f; Y <= 1050.f; Y += 150.f)
		{
			for (const float X : { -270.f, -180.f, 0.f, 180.f, 270.f })
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
		if (Top + 15.f > CoreW.Z)
		{
			UE_LOG(LogTemp, Log, TEXT("Transport: %s's floor raised %.0f cm over the landscape"), UTF8_TO_TCHAR(St.Id.c_str()), Top + 15.f - CoreW.Z);
			CoreW.Z = Top + 15.f;
		}
	}
	B.Core = FTransform(FRotator(0.f, Yaw0, 0.f), CoreW);

	// the decks' frames: origin on the deck at the station's centre, X along its line
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const float Yaw = SimValid(St.AxisYaw, L) ? St.AxisYaw[L] : 0.f;
		B.Decks.Add(FTransform(FRotator(0.f, Yaw, 0.f), FVector(Centre.X, Centre.Y, St.Levels[K].Z * 100.f)));
		B.DeckLines.Add(L);
	}

	// the gallery's side: toward the vestibule (transport.py puts it square off the first level's axis)
	FVector VestW = B.Core.TransformPosition(FVector(0.f, -(950.f), 0.f));
	float VestZ = St.CoreGround * 100.f;
	if (St.bHasVest)
	{
		VestW = W(St.VestX, St.VestY, St.VestZ);
		VestZ = FMath::Max(St.VestZ * 100.f, CoreW.Z);
	}
	const FVector VestL = B.Core.InverseTransformPosition(VestW);
	const int32 GroundSide = VestL.Y >= 0.f ? 1 : 3;
	const float GalleryLen = FMath::Max(200.f, FMath::Abs(VestL.Y) - TowerOut);
	const float Rise = FMath::Clamp(VestZ - CoreW.Z, 0.f, 260.f);

	// stops: the ground and each deck (over the tower's ground); each deck's door side faces along its platform
	// toward the stop (an end station: toward the wheel)
	TArray<float> Stops = { 0.f };
	TArray<int32> Sides = { GroundSide };
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const int32 End = SimValid(St.Ends, L) ? St.Ends[L] : 0;
		const FVector Toward = B.Core.InverseTransformVectorNoScale(B.Decks[K].GetRotation().Vector()) * (End == 1 ? -1.f : 1.f);
		int32 Best = 0;
		for (int32 E = 1; E < 4; E++)
		{
			if (FVector::DotProduct(SideOut(E), Toward) > FVector::DotProduct(SideOut(Best), Toward))
			{
				Best = E;
			}
		}
		// an end station's tower stands between the stop and the far end: the door toward the stop
		if (End != 0)
		{
			const FVector StopW = B.Decks[K].TransformPosition(FVector(End == 1 ? 250.f : -250.f, 0.f, 0.f));
			const FVector D = B.Core.InverseTransformVectorNoScale(StopW - B.Decks[K].TransformPosition(B.Decks[K].InverseTransformPosition(CoreW) * FVector(1, 1, 0)));
			for (int32 E = 0; E < 4; E++)
			{
				if (FVector::DotProduct(SideOut(E), D) > FVector::DotProduct(SideOut(Best), D))
				{
					Best = E;
				}
			}
		}
		Stops.Add(St.Levels[K].Z * 100.f - CoreW.Z);
		Sides.Add(Best);
	}

	B.Stops = Stops;
	B.StopSides = Sides;
	FLyudMeshKit Kit;
	BuildTower(Kit, Net, S, Stops, Sides);
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		BuildDeck(Kit, Net, S, K);
	}
	BuildGallery(Kit, Net, S, GroundSide, GalleryLen, Rise);
	if (UStaticMeshComponent* C = KitMesh(Kit, B.Core, FName(*FString::Printf(TEXT("SM_Station_%d"), S))))
	{
		C->SetCastShadow(true);
		UE_LOG(LogTemp, Log, TEXT("Transport: station %d mesh %d triangles"), S, Kit.NumTriangles());
	}

	// warm light in the tower at every stop's landing
	for (int32 K = 0; K < Stops.Num(); K++)
	{
		UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
		Lt->SetupAttachment(RootComponent);
		Lt->RegisterComponent();
		Lt->SetWorldLocation(B.Core.TransformPosition(SideOut(Sides[K]) * 187.f + FVector(0, 0, Stops[K] + 250.f)));
		Lt->SetIntensity(3000.f);
		Lt->SetAttenuationRadius(700.f);
		Lt->SetLightColor(FLinearColor(1.f, 0.82f, 0.6f));
		Lt->SetCastShadows(false);
		AddInstanceComponent(Lt);
	}

	// the lift
	if (Stops.Last() > LevelLiftMin)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		B.Lift = GetWorld()->SpawnActor<ALyudStationLift>(ALyudStationLift::StaticClass(), B.Core, SP);
		if (B.Lift)
		{
			B.Lift->Setup(this, Stops, Sides);
		}
	}

	// the gallery's end: the connector, its door, the way in
	const FVector GDir = SideOut(GroundSide);
	B.Connector = FTransform(FRotator(0.f, B.Core.Rotator().Yaw + GroundSide * 90.f + 180.f, 0.f), B.Core.TransformPosition(GDir * (TowerOut + GalleryLen) + FVector(0, 0, Rise)));
	B.Vestibule = FTransform(B.Connector.GetRotation(), B.Core.TransformPosition(GDir * (TowerOut + GalleryLen * 0.5f)));
	B.Entrance = B.Connector.TransformPosition(FVector(-250.f, 0.f, 0.f));
	{
		const FVector2D Hole = ALyudDoor::HoleForClear(150.f, 230.f, true);
		const FTransform DoorT(B.Connector.GetRotation() * FRotator(0.f, 180.f, 0.f).Quaternion(), B.Connector.TransformPosition(FVector(-6.f, 0.f, 0.f)));
		if (ALyudDoor* Door = GetWorld()->SpawnActorDeferred<ALyudDoor>(ALyudDoor::StaticClass(), DoorT, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Door->Size = ELyudDoorSize::FitHole;
			Door->HoleWidth = Hole.X;
			Door->HoleHeight = Hole.Y;
			Door->bDoubleLeaf = true;
			Door->WallThickness = 12.f;
			Door->LeafColor = FLinearColor(0.80f, 0.76f, 0.66f);
			Door->FrameColor = FLinearColor(0.86f, 0.42f, 0.08f);
			Door->FinishSpawning(DoorT);
			B.Door = Door;
		}
	}
	PlaceVestibule(Net, S);

	// the station's name: over the gallery's door outside, on the tower's head both ways across the line, on the
	// tower's sides on each platform facing the tracks; each on its own plate, nothing in front of it
	auto Sign = [&](const FTransform& At, float WidthCm)
	{
		// a widget's face looks along its +X: the sign faces along At's +X
		Widget(S, SNew(SLyudStationSign).Station(S), At, FVector2D(1600.f, 300.f), WidthCm / 1600.f, true);
	};
	Sign(FTransform(B.Connector.GetRotation() * FRotator(0.f, 180.f, 0.f).Quaternion(), B.Connector.TransformPosition(FVector(-14.f, 0.f, 290.f))), 300.f);
	const float Top = Stops.Last();
	for (const int32 E : { 1, 3 })
	{
		const FVector O = SideOut(E);
		Sign(FTransform(FRotator(0.f, B.Core.Rotator().Yaw + E * 90.f, 0.f), B.Core.TransformPosition(O * (TowerOut + 3.f) + FVector(0, 0, Top + HeadOver - 130.f))), 480.f);
		for (int32 K = 1; K < Stops.Num(); K++)
		{
			Sign(FTransform(FRotator(0.f, B.Core.Rotator().Yaw + E * 90.f, 0.f), B.Core.TransformPosition(O * (TowerOut + 3.f) + FVector(0, 0, Stops[K] + 250.f))), 300.f);
		}
	}

	// per level: the call panel at the stop, the board hanging from the canopy over it
	for (int32 K = 0; K < SimNum(St.Levels); K++)
	{
		const int32 L = St.Levels[K].Line;
		const FTransform& F = B.Decks[K];
		const int32 End = SimValid(St.Ends, L) ? St.Ends[L] : 0;
		const float Stop = End == 0 ? 550.f : (End == 1 ? 250.f : -250.f);
		const float CoreX = F.InverseTransformPosition(CoreW).X;
		const float Face = CoreX > Stop ? 0.f : 180.f;
		const FTransform PanelT(FRotator(0.f, Face, 0.f), FVector(Stop, 0.f, 0.f));
		B.Panels.Add(PanelT * F);
		const FTransform BoardAt(FRotator(0.f, Face + 180.f, 0.f), FVector(Stop, 0.f, CanopyLow - 70.f));
		Widget(S, SNew(SLyudStationBoard).Station(S).Line(L), BoardAt * F, FVector2D(900.f, 360.f), 0.27f, true)->SetTwoSided(true);
	}

	// the gallery's furniture: the live route diagram (E: where to go), the clock, a bench, a plant, lamps
	{
		const FVector Mid = B.Core.TransformPosition(GDir * (TowerOut + GalleryLen * 0.5f));
		const FVector Along = B.Core.TransformVectorNoScale(GDir);
		const FVector Across = B.Core.TransformVectorNoScale(SideAlong(GroundSide));
		const float Yaw = Across.Rotation().Yaw + 180.f;     // the face looks back into the gallery
		// the diagram on the solid panel of the +Across wall, facing in
		const FVector DiagramAt = Mid + Across * (GalleryHalf - 4.f) + FVector(0, 0, 165.f);
		Widget(S, SNew(SLyudTransitDiagram).bLive(true).bBoard(false).From(S), FTransform(FRotator(0.f, Yaw, 0.f), DiagramAt), FVector2D(1600.f, 900.f), 0.15f, true);
		VestBoards.Add(FTransform(DiagramAt - Across * 110.f - FVector(0, 0, 65.f)));
		VestBoardStation.Add(S);
		TWeakObjectPtr<UObject> Ctx(this);
		Widget(S, SNew(SLyudClock).WorldContext(Ctx), FTransform(FRotator(0.f, Yaw, 0.f), Mid + Across * (GalleryHalf - 4.f) + Along * 190.f + FVector(0, 0, 250.f)),
			FVector2D(300.f, 300.f), 0.16f, false);
		if (GalleryLen > 500.f)
		{
			const FVector P = Mid - Across * (GalleryHalf - 30.f) - Along * (GalleryLen * 0.5f - 90.f) + FVector(0, 0, 0.f);
			if (AProceduralPlant* Plant = GetWorld()->SpawnActorDeferred<AProceduralPlant>(AProceduralPlant::StaticClass(), FTransform(P + FVector(0, 0, 45.f)), nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Plant->Species = S % 2 ? TEXT("RubberPlant") : TEXT("Monstera");
				Plant->Seed = 700 + S;
				Plant->AgeAtStart = 3.f;
				Plant->bDemoGrowth = false;
				Plant->bSnapToGround = false;
				Plant->FinishSpawning(FTransform(P + FVector(0, 0, 45.f)));
			}
		}
		UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
		Lt->SetupAttachment(RootComponent);
		Lt->RegisterComponent();
		Lt->SetWorldLocation(Mid + FVector(0, 0, GalleryH - 40.f));
		Lt->SetIntensity(5000.f);
		Lt->SetAttenuationRadius(FMath::Max(700.f, GalleryLen));
		Lt->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
		Lt->SetCastShadows(false);
		AddInstanceComponent(Lt);
	}
}

void ALyudCableway::BuildTower(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, const TArray<float>& Stops, const TArray<int32>& Sides)
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

	// the outer wall: glass from the ground to the head, enamel spandrels at the stops, mullions; the ways out
	for (int32 E = 0; E < 4; E++)
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
		K.Wall(TEXT("glass"), WF, TowerHalf * 2.f, 4.f, 0.f, Top + 300.f, Ways);
		// the head: solid enamel over the glass, under the roof
		K.Wall(TEXT("tx_enamel"), FTransform(FRotator(0.f, E * 90.f + 90.f, 0.f), SideOut(E) * TowerHalf - SideAlong(E) * (TowerHalf + 4.f)),
			TowerHalf * 2.f + 8.f, 8.f, Top + 300.f, RoofZ, {});
		// spandrel bands at each stop, broken by the way out; an amber line under each
		for (int32 Sp = 1; Sp < Stops.Num(); Sp++)
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
			Spans.Add(FVector2D(Z, Top + 300.f));
			for (const FVector2D& Sp : Spans)
			{
				if (Sp.Y - Sp.X > 2.f)
				{
					K.BoxIn(TEXT("tx_enamel"), I, SideOut(E) * (TowerHalf + 4.f) + SideAlong(E) * U + FVector(0, 0, (Sp.X + Sp.Y) * 0.5f), FVector(6.f, 7.f, Sp.Y - Sp.X), false, NAME_None, FRotator(0.f, E * 90.f, 0.f));
				}
			}
		}
		K.BoxIn(TEXT("tx_enamel"), I, CornerAt(E, TowerHalf + 2.f, (Top + 300.f) * 0.5f), FVector(14.f, 14.f, Top + 300.f), false);
	}
	// the roof and its amber edge
	K.BoxIn(TEXT("tx_enamel"), I, FVector(0, 0, RoofZ + 15.f), FVector(TowerOut * 2.f + 50.f, TowerOut * 2.f + 50.f, 30.f), true, TEXT("tx_concrete"));
	K.BoxIn(TEXT("tx_amber"), I, FVector(0, 0, RoofZ + 36.f), FVector(TowerOut * 2.f + 54.f, TowerOut * 2.f + 54.f, 12.f), false);
	K.BoxIn(TEXT("glow_red"), I, FVector(0, 0, RoofZ + 60.f), FVector(16.f, 16.f, 16.f), false);
}

void ALyudCableway::BuildDeck(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, int32 Level)
{
	const LyudTransport::FStation& St = Net.Stations[S];
	const FLyudBuiltStation& B = Built[S];
	const int32 L = St.Levels[Level].Line;
	// the deck's frame in the tower's (the kit's) frame
	const FTransform F = B.Decks[Level].GetRelativeTransform(B.Core);
	const int32 End = SimValid(St.Ends, L) ? St.Ends[L] : 0;
	const float A0 = End == 1 ? -60.f : -IslandLen, A1 = End == -1 ? 60.f : IslandLen;
	const float Stop = End == 0 ? 550.f : (End == 1 ? 250.f : -250.f);
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
	const float Bands[] = { -IslandHalf, -IslandHalf + 40.f, -IslandHalf + 80.f, IslandHalf - 80.f, IslandHalf - 40.f, IslandHalf };
	const FName Tops[] = { TEXT("tx_concrete"), TEXT("tx_tactile"), TEXT("tx_terrazzo"), TEXT("tx_tactile"), TEXT("tx_concrete") };
	for (int32 Bd = 0; Bd < 5; Bd++)
	{
		K.SlabWithHole(TEXT("tx_enamel"), F, A0, A1, Bands[Bd], Bands[Bd + 1], Hole, -DeckThick, 0.f, true, Tops[Bd]);
	}
	// the amber belt along the deck's faces and ends
	for (const float Side : { -1.f, 1.f })
	{
		K.BoxIn(TEXT("tx_amber"), F, FVector((A0 + A1) * 0.5f, Side * (IslandHalf + 1.f), -DeckThick + 9.f), FVector(A1 - A0, 2.f, 14.f), false);
	}
	// railings: along the edges, on the hulls' line (HullClear) and short of the stops; across the ends
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
	const float RailY = HullClear - 8.f;
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
		const bool bWheel = (End == 1 && X == A0) || (End == -1 && X == A1);
		if (!bWheel)
		{
			Railing(FVector2D(X - FMath::Sign(X) * 6.f, -RailY), FVector2D(X - FMath::Sign(X) * 6.f, RailY));
		}
		else
		{
			// round the wheel the cabins turn: a railing across, inside the turn
			Railing(FVector2D(X + FMath::Sign(X) * -6.f, -RailY), FVector2D(X + FMath::Sign(X) * -6.f, RailY));
		}
	}

	// columns under the island's ends, in pairs (round concrete, an amber collar under the deck)
	const float Ground = St.Ground * 100.f - 250.f - B.Core.GetLocation().Z;
	for (const float X : { A0 + 220.f, A1 - 220.f })
	{
		if (X > HoleX0 - 150.f && X < HoleX1 + 150.f)
		{
			continue;
		}
		for (const float Y : { -IslandHalf + 90.f, IslandHalf - 90.f })
		{
			const FVector Top = F.TransformPosition(FVector(X, Y, -DeckThick));
			const FVector Bot(Top.X, Top.Y, Ground);
			TArray<FVector2D> Prof = { FVector2D(38.f, Bot.Z), FVector2D(38.f, Top.Z - 60.f), FVector2D(40.f, Top.Z - 60.f), FVector2D(40.f, Top.Z - 30.f), FVector2D(46.f, Top.Z - 30.f), FVector2D(46.f, Top.Z) };
			K.Lathe(FTransform(FVector(Top.X, Top.Y, 0.f)), Prof, { TEXT("tx_concrete"), TEXT("tx_amber"), TEXT("tx_amber"), TEXT("tx_amber"), TEXT("tx_concrete") }, 20, false, true);
		}
	}

	// the canopy: a butterfly roof over the island, low in the middle, on slim columns along the middle; it stops at
	// the tower (which rises through it) and stays inside the hulls' line
	const float CY = HullClear - 10.f;
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

	// the call panel at the stop: a steel pedestal with a glowing screen
	const float CoreX = (HoleX0 + HoleX1) * 0.5f;
	const float Face = CoreX > Stop ? 0.f : 180.f;
	const FTransform PanelT = FTransform(FRotator(0.f, Face, 0.f), FVector(Stop, 0.f, 0.f)) * F;
	K.BoxIn(TEXT("tx_enamel"), PanelT, FVector(0, 0, 65.f), FVector(28.f, 56.f, 130.f), true, TEXT("tx_steel"));
	K.BoxIn(TEXT("glow"), PanelT, FVector(15.f, 0, 100.f), FVector(2.f, 44.f, 28.f), false);
	K.BoxIn(TEXT("tx_amber"), PanelT, FVector(15.f, 0, 72.f), FVector(2.f, 44.f, 6.f), false);
	// the board's hangers from the canopy's gutter
	for (const float Y : { -110.f, 110.f })
	{
		K.Rod(TEXT("tx_steel"), PanelT.TransformPosition(FVector(0, Y, CanopyLow - 20.f)), PanelT.TransformPosition(FVector(0, Y, CanopyLow - 22.f - 20.f)), 1.f, 6, false);
	}

	// an end station: the turn wheel at the rope's height, on a steel mast from the deck at the island's end
	if (End != 0)
	{
		const float Rope = 401.f;
		TArray<FVector2D> Wheel = { FVector2D(80.f, Rope - 30.f), FVector2D(460.f, Rope - 14.f), FVector2D(470.f, Rope), FVector2D(460.f, Rope + 14.f), FVector2D(80.f, Rope + 26.f) };
		K.Lathe(FTransform(F.GetRotation(), F.TransformPosition(FVector(0.f, 0.f, 0.f))), Wheel, { TEXT("tx_steel"), TEXT("tx_dark"), TEXT("tx_dark"), TEXT("tx_steel") }, 48, false, false);
		TArray<FVector2D> Mast = { FVector2D(40.f, -DeckThick), FVector2D(34.f, Rope + 60.f), FVector2D(60.f, Rope + 60.f), FVector2D(60.f, Rope + 90.f) };
		K.Lathe(F, Mast, { TEXT("tx_enamel"), TEXT("tx_amber"), TEXT("tx_amber") }, 20, true, true);
	}
}

void ALyudCableway::BuildGallery(FLyudMeshKit& K, const LyudTransport::FNetwork& Net, int32 S, int32 Side, float Length, float Rise)
{
	// in a frame along the gallery: origin on the floor at the tower's outer face, X out to the connector, Y across
	const FTransform G(FRotator(0.f, Side * 90.f, 0.f), SideOut(Side) * TowerOut);
	const float HalfOut = GalleryHalf + 20.f, RoofTop = GalleryH + 40.f + Rise;
	const float Ground = (Net.Stations[S].bHasVest ? FMath::Min(Net.Stations[S].VestGroundMin, Net.Stations[S].CoreGround) : Net.Stations[S].CoreGround) * 100.f - 200.f - Built[S].Core.GetLocation().Z;
	// the floor on a plinth down into the ground; steps up near the end when the vestibule stands higher
	const float StepsLen = Rise > 2.f ? FMath::CeilToInt(Rise / 17.f) * 30.f : 0.f;
	const float FlatEnd = Length - StepsLen - 60.f;
	K.BoxIn(TEXT("tx_concrete"), G, FVector(Length * 0.5f, 0.f, (Ground - 0.f) * 0.5f), FVector(Length + 2.f, HalfOut * 2.f, -Ground), true, TEXT("tx_terrazzo"));
	if (StepsLen > 0.f)
	{
		const int32 N = FMath::CeilToInt(Rise / 17.f);
		for (int32 I = 0; I < N; I++)
		{
			const float Top = Rise * (I + 1) / N;
			const float X0 = FlatEnd + 30.f * I;
			K.BoxIn(TEXT("tx_terrazzo"), G, FVector((X0 + Length) * 0.5f, 0.f, Top * 0.5f), FVector(Length - X0, GalleryHalf * 2.f, Top), false, TEXT("tx_tread"));
		}
		K.CollideBox(FTransform(FRotator(FMath::RadiansToDegrees(FMath::Atan2(Rise, StepsLen)), 0.f, 0.f), G.TransformPosition(FVector(FlatEnd + StepsLen * 0.5f, 0.f, Rise * 0.5f - 4.f))),
			FVector(FMath::Sqrt(StepsLen * StepsLen + Rise * Rise) * 0.5f, GalleryHalf, 4.f));
		K.BoxIn(TEXT("tx_terrazzo"), G, FVector(Length - 30.f, 0.f, Rise * 0.5f), FVector(60.f, GalleryHalf * 2.f, Rise), true, TEXT("tx_terrazzo"));
	}
	// the side walls: a low enamel dado, glass over it, the solid panel for the diagram on the +Y wall
	for (const float Sg : { -1.f, 1.f })
	{
		const FTransform WF(FRotator(0.f, 0.f, 0.f), FVector(0.f, Sg * (GalleryHalf + 5.f), 0.f));
		const FTransform WG = WF * G;
		const float PanelU0 = Length * 0.5f - 160.f, PanelU1 = Length * 0.5f + 160.f;
		if (Sg > 0.f)
		{
			K.Wall(TEXT("glass"), WG, Length, 3.f, 60.f, GalleryH + Rise, { { PanelU0, PanelU1, 0.f, 1e5f } });
			K.BoxIn(TEXT("tx_mosaic"), WG, FVector((PanelU0 + PanelU1) * 0.5f, -1.f, (GalleryH + Rise) * 0.5f), FVector(PanelU1 - PanelU0, 10.f, GalleryH + Rise), true);
			// the diagram's amber frame
			K.BoxIn(TEXT("tx_amber"), WG, FVector((PanelU0 + PanelU1) * 0.5f, -7.f, 165.f), FVector(252.f, 2.f, 147.f), false);
		}
		else
		{
			K.Wall(TEXT("glass"), WG, Length, 3.f, 60.f, GalleryH + Rise, {});
		}
		K.BoxIn(TEXT("tx_enamel"), WG, FVector(Length * 0.5f, 0.f, 30.f), FVector(Length, 10.f, 60.f), true);
		K.BoxIn(TEXT("tx_amber"), WG, FVector(Length * 0.5f, Sg * 5.f, 61.f), FVector(Length, 2.f, 4.f), false);
		// mullions every ~2 m
		const int32 N = FMath::Max(1, FMath::RoundToInt(Length / 200.f));
		for (int32 I = 0; I <= N; I++)
		{
			const float U = Length * I / N;
			K.BoxIn(TEXT("tx_enamel"), WG, FVector(FMath::Clamp(U, 6.f, Length - 6.f), Sg * 4.f, (GalleryH + Rise) * 0.5f), FVector(10.f, 12.f, GalleryH + Rise), false);
		}
	}
	// the roof (a slab over the walls, amber edges), lamps under it
	K.BoxIn(TEXT("tx_enamel"), G, FVector(Length * 0.5f - 10.f, 0.f, GalleryH + Rise + 20.f), FVector(Length + 20.f, HalfOut * 2.f + 20.f, 40.f), true, TEXT("tx_concrete"));
	for (const float Sg : { -1.f, 1.f })
	{
		K.BoxIn(TEXT("tx_amber"), G, FVector(Length * 0.5f - 10.f, Sg * (HalfOut + 11.f), GalleryH + Rise + 26.f), FVector(Length + 20.f, 2.f, 14.f), false);
	}
	for (int32 I = 0; I < 3; I++)
	{
		K.BoxIn(TEXT("glow"), G, FVector(Length * (I + 0.5f) / 3.f, 0.f, GalleryH + Rise - 1.f), FVector(110.f, 18.f, 2.f), false);
	}
	// the end: a portal frame (the connector, 340 x 340 outside), glass round the double door
	const FTransform E(FRotator(0.f, 0.f, 0.f), FVector(Length, 0.f, Rise));
	const FTransform EG = E * G;
	const FVector2D Hole = ALyudDoor::HoleForClear(150.f, 230.f, true);
	const TArray<FLyudMeshKit::FOpening> DoorHole = { { GalleryHalf - Hole.X * 0.5f, GalleryHalf + Hole.X * 0.5f, 0.f, Hole.Y } };
	K.Wall(TEXT("glass"), FTransform(FRotator(0.f, 90.f, 0.f), FVector(-6.f, -GalleryHalf, 0.f)) * EG, GalleryHalf * 2.f, 3.f, 0.f, GalleryH, DoorHole);
	K.BoxIn(TEXT("tx_steel"), EG, FVector(-6.f, 0.f, Hole.Y + 4.f), FVector(10.f, Hole.X + 12.f, 8.f), false);
	for (const float Sg : { -1.f, 1.f })
	{
		K.BoxIn(TEXT("tx_enamel"), EG, FVector(-10.f, Sg * (GalleryHalf + 10.f), (GalleryH + 40.f) * 0.5f), FVector(20.f, 20.f, GalleryH + 40.f), true);
		K.BoxIn(TEXT("tx_steel"), EG, FVector(-6.f, Sg * (Hole.X * 0.5f + 4.f), Hole.Y * 0.5f), FVector(10.f, 8.f, Hole.Y), false);
	}
	K.BoxIn(TEXT("tx_enamel"), EG, FVector(-10.f, 0.f, GalleryH + 20.f), FVector(20.f, HalfOut * 2.f, 40.f), true);
	// the plate under the station's name (the sign widget sits on it, BuildStation)
	K.BoxIn(TEXT("tx_dark"), EG, FVector(-1.f, 0.f, 290.f), FVector(4.f, 310.f, 64.f), false);
	// inside: a bench along the -Y wall, a bin
	if (Length > 400.f)
	{
		const float Bx = Length * 0.5f;
		K.BoxIn(TEXT("tx_amber"), G, FVector(Bx, -GalleryHalf + 32.f, 44.f), FVector(180.f, 46.f, 6.f), true);
		K.BoxIn(TEXT("tx_amber"), G, FVector(Bx, -GalleryHalf + 10.f, 75.f), FVector(180.f, 5.f, 40.f), false, NAME_None, FRotator(0.f, 0.f, -8.f));
		for (const float D : { -75.f, 75.f })
		{
			K.BoxIn(TEXT("tx_steel"), G, FVector(Bx + D, -GalleryHalf + 30.f, 21.f), FVector(5.f, 40.f, 42.f), false);
		}
		TArray<FVector2D> Bin = { FVector2D(18.f, 0.f), FVector2D(20.f, 70.f), FVector2D(21.f, 74.f) };
		K.Lathe(FTransform(G.TransformPosition(FVector(Bx + 150.f, -GalleryHalf + 26.f, 0.f))), Bin, { TEXT("tx_steel"), TEXT("tx_amber") }, 16, true, true);
	}
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

void ALyudCableway::PlaceVestibule(const LyudTransport::FNetwork& Net, int32 S)
{
	// the hi3d vestibule (blender/transport/vestibules.py) at the connector: its origin there, its +X toward the tower;
	// under it a concrete podium down to the ground, steps down from its street entrances, its door (ALyudDoor), the
	// route diagram, a kiosk and a bench at its spots. Missing (not made yet): a terrace before the gallery's door.
	const LyudTransport::FStation& St = Net.Stations[S];
	const FString Style = St.Style.empty() ? TEXT("saucer") : FString(UTF8_TO_TCHAR(St.Style.c_str()));
	const FString Cap = Style.Left(1).ToUpper() + Style.Mid(1);
	UStaticMesh* Mesh = LoadMesh(TEXT("SM_TR_Vestibule_") + Cap);
	const TSharedPtr<FJsonObject> V = VestibuleData(Style);
	FLyudBuiltStation& B = Built[S];
	const FTransform& C = B.Connector;
	// the ground under a point of the connector frame (the landscape is there by now; the map's number otherwise)
	auto GroundAt = [&](const FVector& Local) -> float
	{
		const FVector P = C.TransformPosition(FVector(Local.X, Local.Y, 0.f));
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("VestGround"), false, this);
		for (TActorIterator<ALyudDoor> It(GetWorld()); It; ++It) { Q.AddIgnoredActor(*It); }
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 3000.f), P - FVector(0, 0, 6000.f), ECC_WorldStatic, Q))
		{
			return Hit.ImpactPoint.Z - C.GetLocation().Z;
		}
		return St.VestGroundMin * 100.f - C.GetLocation().Z;
	};
	FLyudMeshKit K;
	if (Mesh && V.IsValid())
	{
		UStaticMeshComponent* M = Single(Mesh, C, true);
		M->SetCastShadow(true);
		// the podium: the footprint's hull from 60 cm under the floor down past the lowest ground under it
		TArray<FVector2D> Pts;
		const TArray<TSharedPtr<FJsonValue>>* Fp = nullptr;
		if (V->TryGetArrayField(TEXT("footprint"), Fp))
		{
			for (const TSharedPtr<FJsonValue>& P : *Fp)
			{
				const TArray<TSharedPtr<FJsonValue>>& XY = P->AsArray();
				if (XY.Num() >= 2)
				{
					Pts.Add(FVector2D(FMath::Min(XY[0]->AsNumber(), -2.0), XY[1]->AsNumber()));
				}
			}
		}
		TArray<FVector2D> Pod = Hull(Pts);
		float Low = 0.f;
		for (const FVector2D& P : Pod)
		{
			Low = FMath::Min(Low, GroundAt(FVector(P.X, P.Y, 0.f)));
		}
		// inset a little so the podium hides under the vestibule's own edge
		const FVector2D Ctr = [&] { FVector2D A = FVector2D::ZeroVector; for (const FVector2D& P : Pod) { A += P; } return Pod.Num() ? A / Pod.Num() : A; }();
		for (FVector2D& P : Pod)
		{
			P = Ctr + (P - Ctr) * 0.97f;
		}
		if (Pod.Num() >= 3)
		{
			K.Prism(TEXT("tx_concrete"), FTransform::Identity, Pod, Low - 150.f, -58.f, true, TEXT("tx_concrete"));
		}
		// the room's middle (Transport.Goto vestibule) and a warm light under its ceiling
		const TSharedPtr<FJsonObject>* Room = nullptr;
		if (V->TryGetObjectField(TEXT("room"), Room))
		{
			const float RoomH = (*Room)->HasField(TEXT("height")) ? (*Room)->GetNumberField(TEXT("height")) : 330.f;
			const float RoomR = (*Room)->HasField(TEXT("radius")) ? (*Room)->GetNumberField(TEXT("radius")) : 400.f;
			UPointLightComponent* Lt = NewObject<UPointLightComponent>(this);
			Lt->SetupAttachment(RootComponent);
			Lt->RegisterComponent();
			Lt->SetWorldLocation(C.TransformPosition(JsonXY(*Room, TEXT("centre")) + FVector(0, 0, FMath::Min(RoomH - 60.f, 300.f))));
			Lt->SetIntensity(9000.f);
			Lt->SetAttenuationRadius(FMath::Max(800.f, RoomR * 2.2f));
			Lt->SetLightColor(FLinearColor(1.f, 0.8f, 0.55f));
			Lt->SetCastShadows(false);
			AddInstanceComponent(Lt);
			B.Room = FTransform(C.GetRotation(), C.TransformPosition(JsonXY(*Room, TEXT("centre")) + FVector(0, 0, 100.f)));
		}
		// the door into the room
		const TSharedPtr<FJsonObject>* D = nullptr;
		if (V->TryGetObjectField(TEXT("door"), D))
		{
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
		// steps from each street entrance down to the ground
		const TArray<TSharedPtr<FJsonValue>>* En = nullptr;
		if (V->TryGetArrayField(TEXT("entrances"), En))
		{
			for (const TSharedPtr<FJsonValue>& E : *En)
			{
				const TSharedPtr<FJsonObject> O = E->AsObject();
				const FVector At = JsonXY(O, TEXT("at"));
				const FVector Out = JsonXY(O, TEXT("out")).GetSafeNormal2D();
				const float Wd = O->HasField(TEXT("width")) ? O->GetNumberField(TEXT("width")) : 240.f;
				B.Entrance = C.TransformPosition(At + Out * 60.f);
				const float Drop = -GroundAt(At + Out * 200.f);
				const int32 N = FMath::Clamp(FMath::CeilToInt((Drop - 2.f) / 16.f), 0, 40);
				const FRotator R(0.f, Out.Rotation().Yaw, 0.f);
				for (int32 I = 0; I < N; I++)
				{
					const float Top = -Drop * (I + 1) / (N + 0.f) + 0.f;
					const float TopZ = -(Drop * (I + 1)) / N;
					(void)Top;
					K.BoxIn(TEXT("tx_concrete"), FTransform(R, At), FVector(30.f * I + 15.f, 0.f, (TopZ + (Low - 150.f)) * 0.5f), FVector(30.f, Wd + 40.f, TopZ - (Low - 150.f)), false, TEXT("tx_tread"));
				}
				if (N > 0)
				{
					const float Run = 30.f * N;
					K.CollideBox(FTransform(FRotator(-FMath::RadiansToDegrees(FMath::Atan2(Drop, Run)), 0.f, 0.f), FVector(Run * 0.5f, 0.f, -Drop * 0.5f - 4.f)) * FTransform(R, At),
						FVector(FMath::Sqrt(Run * Run + Drop * Drop) * 0.5f, Wd * 0.5f + 20.f, 4.f));
					for (const float Sg : { -1.f, 1.f })
					{
						// the cheek walls
						K.BoxIn(TEXT("tx_enamel"), FTransform(R, At), FVector(Run * 0.5f, Sg * (Wd * 0.5f + 30.f), (-Drop + 90.f + (Low - 150.f)) * 0.5f), FVector(Run, 20.f, -(Low - 150.f) + 90.f - Drop + Drop), true, TEXT("tx_amber"));
					}
				}
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
	else
	{
		// a terrace before the gallery's door
		const float Down = FMath::Max(80.f, -GroundAt(FVector(-220.f, 0.f, 0.f)) + 120.f);
		K.BoxIn(TEXT("tx_concrete"), FTransform::Identity, FVector(-220.f, 0.f, -Down * 0.5f), FVector(400.f, 460.f, Down), true, TEXT("tx_terrazzo"));
	}
	KitMesh(K, C, FName(*FString::Printf(TEXT("SM_Podium_%d"), S)));
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
		// 45 m off across the line on the vestibule's side, a little up, the whole station in view
		const FVector Side = B.Core.TransformVectorNoScale(SideOut(G));
		const FVector P = B.Core.GetLocation() + Side * 4500.f + B.Core.GetRotation().Vector() * 1800.f + FVector(0, 0, 900.f);
		Out = FTransform((B.Core.GetLocation() + FVector(0, 0, B.Stops.Last() * 0.5f) - P).Rotation(), P);
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
		Down(TEXT("way out"), B.Core.TransformPosition(SideOut(Sd) * (TowerOut + 60.f) + FVector(0, 0, B.Stops[K])), G + B.Stops[K], 8.f);
		// three quarters round from this stop's side: on the stair's flight or a corner
		Down(TEXT("corner"), B.Core.TransformPosition(CornerAt(Sd, Mid, 0.f) + FVector(0, 0, B.Stops[K])), G + B.Stops[K], 8.f);
	}
	for (int32 K = 0; K < B.Decks.Num(); K++)
	{
		const FVector Panel = B.Decks[K].InverseTransformPosition(B.Panels.IsValidIndex(K) ? B.Panels[K].GetLocation() : FVector::ZeroVector);
		for (const float X : { -900.f, 0.f, 550.f, 900.f })
		{
			for (const float Y : { -250.f, 0.f, 250.f })
			{
				const FVector P = B.Decks[K].TransformPosition(FVector(X, Y, 0.f));
				// on the island (an end station's runs one way from the wheel), off the tower, the panel and the wheel's mast
				const bool bOff = (Panel.X > 0.f && Panel.X < 400.f && X < 0.f) || (Panel.X < 0.f && Panel.X > -400.f && X > 0.f);
				if (bOff || FVector::Dist2D(P, B.Core.GetLocation()) < TowerOut * 1.45f || FVector::Dist2D(FVector(X, Y, 0.f), Panel) < 80.f ||
					((Panel.X > -400.f && Panel.X < 400.f) && FMath::Abs(X) < 1.f && FMath::Abs(Y) < 1.f))
				{
					continue;
				}
				Down(TEXT("deck"), P, B.Decks[K].GetLocation().Z, 6.f);
			}
		}
	}
	Down(TEXT("gallery"), B.Vestibule.GetLocation(), B.Vestibule.GetLocation().Z, 6.f);
	Down(TEXT("terrace"), B.Connector.TransformPosition(FVector(-150.f, 0.f, 0.f)), B.Connector.GetLocation().Z, 10.f);
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
	const FTransform& F = B.Decks[K];
	const int32 End = SimValid(Net.Stations[S].Ends, L) ? Net.Stations[S].Ends[L] : -1;
	const float Dir = End == 1 ? -1.f : 1.f;           // beyond the wheel
	int32 Count = 0;
	for (const LyudTransport::FCabin& C : Net.Cabins)
	{
		Count += C.Line == L;
	}
	// a hall on columns past the turn wheel: a concrete deck, glass sides on enamel posts, a sawtooth roof with glass
	FLyudMeshKit Kt;
	const FTransform I = FTransform::Identity;
	const float X0 = Dir * 650.f, Len = 500.f * Count + 400.f;
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
	Kt.BoxIn(TEXT("tx_steel"), I, FVector(Xc, 0.f, 401.f), FVector(Len, 14.f, 14.f), false);
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
			bRode = false;
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
			Target = -1;
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
			const float Step = FMath::Clamp(Dist, -Speed * DeltaTime, Speed * DeltaTime);
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
