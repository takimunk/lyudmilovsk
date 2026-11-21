// The town dressed for a holiday and its summer air (Festival/LyudFestivalTown.h, docs/festivals.md): pennant strings
// across the lanes, kites over the beach, the poplars and their fluff.
#include "Festival/LyudFestivalTown.h"

#include "FestivalParts.h"
#include "../Transport/LyudMeshKit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Festival/LyudFestival.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"
#include "Weather/LyudWind.h"

static TAutoConsoleVariable<int32> CVarFluff(TEXT("lyud.Fluff"), 1,
	TEXT("The poplars' fluff in early summer (ULyudPoplarFluffSubsystem): 1 on, 0 off."));
static TAutoConsoleVariable<float> CVarFluffAmount(TEXT("lyud.Fluff.Amount"), -1.f,
	TEXT("Forces how much poplar fluff is in the air, 0..1 (negative: by the season, the weather and the poplars near)."));
static TAutoConsoleVariable<int32> CVarTownPoplars(TEXT("lyud.Fluff.Poplars"), 1,
	TEXT("1: the town's own black poplars are planted when the map starts (Content/City/poplars.json); 0 not."));


namespace
{
	const TCHAR* TownFluffPath = TEXT("/Game/Festival/M_PoplarFluff.M_PoplarFluff");
	const TCHAR* TownPaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
	// the fluff's box round the camera, cm (its side divides the 4096 m the wind's drift wraps at): 32 m keeps the tufts
	// thick enough to read as a summer's fluff, not a stray speck now and then
	constexpr float FluffBoxXY = 3200.f, FluffBoxZ = 1600.f;
	// m/s: the least wind the kites get on the beach (a sea breeze, onshore when the day is calm)
	constexpr float KiteBreeze = 3.5f;

	float TownRand(uint32& S)
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return (S & 0xFFFFFF) / float(0x1000000);
	}

	FVector TownHang(const FVector& A, const FVector& B, float Sag, float T)
	{
		return A + (B - A) * T - FVector(0.f, 0.f, 4.f * Sag * T * (1.f - T));
	}

	TSharedPtr<FJsonObject> TownJson(const FString& ContentFile)
	{
		FString Text;
		TSharedPtr<FJsonObject> Doc;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / ContentFile)) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc))
		{
			return nullptr;
		}
		return Doc;
	}

	/** A point of the plan's JSON: [x, y, z] numbers, times Scale. */
	bool TownPoint(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, FVector& Out, double Scale = 1.0)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(Field, A) || A->Num() < 2)
		{
			return false;
		}
		Out = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), A->Num() > 2 ? (*A)[2]->AsNumber() : 0.0) * Scale;
		return true;
	}

	/** One string of pennants from A to B into a chunk's cloth and rope. */
	void TownPennantString(FLyudCloth& Cloth, FLyudMeshKit& Rope, const FVector& A, const FVector& B, float SagShare, uint32& Rng, int32& OutPennants)
	{
		const float Len = float(FVector::Dist(A, B));
		if (Len < 150.f)
		{
			return;
		}
		const float Sag = FMath::Min(Len * SagShare, 160.f);
		const int32 N = FMath::Clamp(FMath::CeilToInt(Len / 220.f), 4, 10);
		FVector Prev = A;
		for (int32 I = 1; I <= N; I++)
		{
			const FVector P = TownHang(A, B, Sag, float(I) / N);
			Rope.Rod(TEXT("rope"), Prev, P, 0.7f, 3, false, false);
			Prev = P;
		}
		// a pennant every 46 cm, 26 wide and 36 long, colours in a run that starts anywhere
		const int32 NF = FMath::FloorToInt(Len / 46.f);
		const int32 First = int32(TownRand(Rng) * 6.f);
		for (int32 I = 1; I < NF; I++)
		{
			const float T0 = (I - 0.28f) / NF, T1 = (I + 0.28f) / NF;
			const FVector P0 = TownHang(A, B, Sag, T0), P1 = TownHang(A, B, Sag, T1);
			const FVector Tip = (P0 + P1) * 0.5 - FVector(0.f, 0.f, 36.f);
			Cloth.Pennant(P0, P1, Tip, LyudFestParts::ClothPalette[(First + I) % UE_ARRAY_COUNT(LyudFestParts::ClothPalette)], TownRand(Rng));
			OutPennants++;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// the town's pennant strings

ALyudStreetBunting::ALyudStreetBunting()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALyudStreetBunting::Build(uint32 Seed)
{
	const double T0 = FPlatformTime::Seconds();
	const TSharedPtr<FJsonObject> Plan = TownJson(TEXT("City/city_plan.json"));
	if (!Plan.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Festivals: no city plan, no street bunting"));
		return;
	}
	const double FaceY = Plan->HasField(TEXT("portal_face_y")) ? Plan->GetNumberField(TEXT("portal_face_y")) : 790.0;
	auto ToUE = [FaceY](double X, double Y, double Z) { return FVector(X * 100.0, FaceY + Y * 100.0, Z * 100.0); };
	uint32 Rng = Seed | 1u;

	TMap<FString, double> TerraceZ;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Plan->TryGetArrayField(TEXT("terraces"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (O.IsValid())
			{
				TerraceZ.Add(O->GetStringField(TEXT("id")), O->GetNumberField(TEXT("z")));
			}
		}
	}
	// the walls' faces (map metres, south to north) and the terraces they stand between
	struct FWall { TArray<FVector2D> Line; double Lower = 0.0, Upper = 0.0; };
	TMap<FString, FWall> Walls;
	if (Plan->TryGetArrayField(TEXT("walls"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Line = nullptr;
			if (!O.IsValid() || !O->TryGetArrayField(TEXT("line"), Line))
			{
				continue;
			}
			FWall W;
			W.Lower = TerraceZ.FindRef(O->GetStringField(TEXT("below")));
			W.Upper = TerraceZ.FindRef(O->GetStringField(TEXT("above")));
			for (const TSharedPtr<FJsonValue>& P : *Line)
			{
				const TArray<TSharedPtr<FJsonValue>>& Xy = P->AsArray();
				if (Xy.Num() >= 2)
				{
					W.Line.Add(FVector2D(Xy[0]->AsNumber(), Xy[1]->AsNumber()));
				}
			}
			if (W.Line.Num() >= 2)
			{
				Walls.Add(O->GetStringField(TEXT("id")), MoveTemp(W));
			}
		}
	}
	// the nearest point of a wall's face to a map point
	auto Nearest = [](const FWall& W, const FVector2D& P, double& OutDist) -> FVector2D
	{
		FVector2D Best = W.Line[0];
		OutDist = TNumericLimits<double>::Max();
		for (int32 I = 0; I + 1 < W.Line.Num(); I++)
		{
			const FVector2D Q = FMath::ClosestPointOnSegment2D(P, W.Line[I], W.Line[I + 1]);
			const double D = FVector2D::Distance(P, Q);
			if (D < OutDist)
			{
				OutDist = D;
				Best = Q;
			}
		}
		return Best;
	};

	// chunks along the coast, a cloth and a rope mesh each
	struct FChunk { FLyudCloth Cloth; FLyudMeshKit Rope; };
	TMap<int32, FChunk> Chunks;
	auto ChunkAt = [&Chunks](const FVector& A, const FVector& B, int32 Row) -> FChunk&
	{
		return Chunks.FindOrAdd(Row * 1000 + FMath::FloorToInt32((A.Y + B.Y) * 0.5 / 18000.0));
	};
	auto String = [&](const FVector& A, const FVector& B, float SagShare, int32 Row)
	{
		FChunk& C = ChunkAt(A, B, Row);
		TownPennantString(C.Cloth, C.Rope, A, B, SagShare, Rng, Pennants);
		Strings++;
	};

	// 1. the lanes at the foot of W2..W5: lamp head -> a short mast on the parapet half way to the next lamp -> its head
	struct FLamp { FVector2D Foot; FVector Head; };
	TMap<FString, TArray<FLamp>> Lamps;
	if (Plan->TryGetArrayField(TEXT("lamps"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FVector Foot, Head;
			if (O.IsValid() && TownPoint(O, TEXT("foot"), Foot) && TownPoint(O, TEXT("head_ue"), Head))
			{
				Lamps.FindOrAdd(O->GetStringField(TEXT("wall"))).Add({ FVector2D(Foot.X, Foot.Y), Head });
			}
		}
	}
	int32 Row = 0;
	for (auto& It : Lamps)
	{
		Row++;
		const FWall* W = Walls.Find(It.Key);
		if (!W)
		{
			continue;
		}
		TArray<FLamp>& L = It.Value;
		L.Sort([](const FLamp& A, const FLamp& B) { return A.Foot.Y < B.Foot.Y; });
		for (int32 I = 0; I + 1 < L.Num(); I++)
		{
			const double Gap = FVector2D::Distance(L[I].Foot, L[I + 1].Foot);
			if (Gap > 48.0 || Gap < 6.0)
			{
				continue;       // a grand stair or the Cascade between them
			}
			double Dist = 0.0;
			const FVector2D Face = Nearest(*W, (L[I].Foot + L[I + 1].Foot) * 0.5, Dist);
			if (Dist < 3.0 || Dist > 18.0)
			{
				continue;
			}
			// the mast: 0.8 m of pipe on the parapet's top (1.05 m over the upper terrace)
			const FVector Base = ToUE(Face.X, Face.Y, W->Upper + 1.05);
			const FVector Top = Base + FVector(0.f, 0.f, 80.f);
			ChunkAt(Top, Top, Row).Rope.Rod(TEXT("rope"), Base - FVector(0.f, 0.f, 6.f), Top + FVector(0.f, 0.f, 4.f), 2.2f, 5, true, false);
			// the lamp's head carries the light: the strings tie on a hand under it
			const FVector HeadA = L[I].Head - FVector(0.f, 0.f, 22.f), HeadB = L[I + 1].Head - FVector(0.f, 0.f, 22.f);
			String(HeadA, Top, 0.05f, Row);
			String(Top, HeadB, 0.05f, Row);
		}
	}

	// 2. the promenade's arcade: swags along W1's top rail, bay after bay
	if (const FWall* W1 = Walls.Find(TEXT("W1")))
	{
		Row++;
		for (int32 I = 0; I + 1 < W1->Line.Num(); I++)
		{
			const FVector2D A = W1->Line[I], B = W1->Line[I + 1];
			// the face looks to the sea (+X): hung a hand in front of it, at the rail's height
			const FVector Pa = ToUE(A.X + 0.18, A.Y, W1->Upper + 1.0), Pb = ToUE(B.X + 0.18, B.Y, W1->Upper + 1.0);
			String(Pa, Pb, 0.07f, Row);
		}
	}

	// 3. between neighbouring houses of a terrace: found by looking from half way between two plots for each one's wall
	struct FPlot { FVector At; FString Terrace; };
	TArray<FPlot> Plots;
	if (Plan->TryGetArrayField(TEXT("plots"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FVector At;
			FString Building;
			if (O.IsValid() && O->TryGetStringField(TEXT("building"), Building) && !Building.IsEmpty() && TownPoint(O, TEXT("ue"), At))
			{
				Plots.Add({ At, O->GetStringField(TEXT("terrace")) });
			}
		}
	}
	Row++;
	UWorld* World = GetWorld();
	FCollisionQueryParams Q(TEXT("StreetBunting"), false, this);
	int32 Between = 0;
	for (int32 I = 0; I < Plots.Num() && World; I++)
	{
		for (int32 J = I + 1; J < Plots.Num(); J++)
		{
			const double D = FVector::Dist2D(Plots[I].At, Plots[J].At);
			if (Plots[I].Terrace != Plots[J].Terrace || D > 14000.0 || D < 1500.0)
			{
				continue;
			}
			const double Z = Plots[I].At.Z + 720.0;
			const FVector A(Plots[I].At.X, Plots[I].At.Y, Z), B(Plots[J].At.X, Plots[J].At.Y, Z);
			const FVector Mid = (A + B) * 0.5;
			FHitResult Ha, Hb, Clear;
			if (!World->LineTraceSingleByChannel(Ha, Mid, A, ECC_WorldStatic, Q) || !World->LineTraceSingleByChannel(Hb, Mid, B, ECC_WorldStatic, Q))
			{
				continue;
			}
			// walls only (not a roof's slope or the ground), with open air from one to the other
			if (Ha.bStartPenetrating || Hb.bStartPenetrating || FMath::Abs(Ha.ImpactNormal.Z) > 0.5f || FMath::Abs(Hb.ImpactNormal.Z) > 0.5f)
			{
				continue;
			}
			const FVector Pa = Ha.ImpactPoint + Ha.ImpactNormal * 6.f, Pb = Hb.ImpactPoint + Hb.ImpactNormal * 6.f;
			const double Span = FVector::Dist(Pa, Pb);
			if (Span < 600.0 || Span > 7000.0 || World->LineTraceSingleByChannel(Clear, Pa + (Pb - Pa).GetSafeNormal() * 30.f, Pb, ECC_WorldStatic, Q))
			{
				continue;
			}
			String(Pa, Pb, 0.06f, Row);
			Between++;
		}
	}

	// the meshes
	ClothMat = LyudFestParts::LoadCloth();
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TownPaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	RopeMat = UMaterialInstanceDynamic::Create(Paint ? Paint : LoadObject<UMaterialInterface>(nullptr, LyudFestParts::BasicPath), this);
	RopeMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.05f));
	RopeMat->SetScalarParameterValue(TEXT("Roughness"), 0.7f);
	int32 Tris = 0;
	for (auto& It : Chunks)
	{
		FChunk& C = It.Value;
		if (!C.Cloth.IsEmpty())
		{
			const FName Name(*FString::Printf(TEXT("StreetPennants_%d"), It.Key));
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, Name);
			Comp->SetStaticMesh(C.Cloth.Build(this, Name));
			Comp->SetMaterial(0, ClothMat);
			Comp->SetMobility(EComponentMobility::Static);
			LyudFestParts::ClothComponent(Comp, 45000.f);
			Comp->SetupAttachment(RootComponent);
			Comp->RegisterComponent();
			AddInstanceComponent(Comp);
			Tris += C.Cloth.NumTriangles();
		}
		if (!C.Rope.IsEmpty())
		{
			const FName Name(*FString::Printf(TEXT("StreetRopes_%d"), It.Key));
			TArray<FName> Slots;
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this, Name);
			Comp->SetStaticMesh(C.Rope.Build(this, Name, Slots));
			for (int32 S = 0; S < Slots.Num(); S++)
			{
				Comp->SetMaterial(S, RopeMat);
			}
			Comp->SetMobility(EComponentMobility::Static);
			LyudFestParts::ClothComponent(Comp, 20000.f);
			Comp->SetupAttachment(RootComponent);
			Comp->RegisterComponent();
			AddInstanceComponent(Comp);
			Tris += C.Rope.NumTriangles();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("Festivals: street bunting, %d strings (%d between houses), %d pennants, %d triangles in %d chunks, %.0f ms"),
		Strings, Between, Pennants, Tris, Chunks.Num(), (FPlatformTime::Seconds() - T0) * 1000.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// kites

ALyudKites::ALyudKites()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALyudKites::Setup(const FVector& At, const FVector& InSeaDir, float Radius, double InEndsAt)
{
	EndsAt = InEndsAt;
	SeaDir = InSeaDir.GetSafeNormal2D(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
	ClothMat = LyudFestParts::LoadCloth();
	UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TownPaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	LineMat = UMaterialInstanceDynamic::Create(Paint ? Paint : LoadObject<UMaterialInterface>(nullptr, LyudFestParts::BasicPath), this);
	LineMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.8f, 0.78f));
	{
		// the line: a thin rod one metre long along X, stretched to each kite
		FLyudMeshKit K;
		K.Rod(TEXT("line"), FVector::ZeroVector, FVector(100.f, 0.f, 0.f), 0.5f, 3, false, false);
		TArray<FName> Slots;
		LineMesh = K.Build(this, TEXT("KiteLine"), Slots);
	}
	// three shapes: a diamond with a long tail, a delta with two streamers, a box of two bands. Built facing -X (the
	// kite's face looks down its line), Z up the kite
	for (int32 Shape = 0; Shape < 3; Shape++)
	{
		for (int32 Tint = 0; Tint < 2; Tint++)
		{
			FLyudCloth M;
			const FLinearColor C0 = LyudFestParts::ClothPalette[(Shape * 2 + Tint) % UE_ARRAY_COUNT(LyudFestParts::ClothPalette)];
			const FLinearColor C1 = LyudFestParts::ClothPalette[(Shape * 2 + Tint + 3) % UE_ARRAY_COUNT(LyudFestParts::ClothPalette)];
			const FVector N(-1.f, 0.f, 0.3f);
			auto V = [&M, &N](const FVector& P, const FLinearColor& C, float Flutter)
			{
				return M.Vert(P, N, { FVector2f(0.5f, 0.3f), FVector2f(0.f, Flutter), FVector2f::ZeroVector, FVector2f(C.R, C.G), FVector2f(C.B, 0.f) });
			};
			if (Shape == 0)
			{
				const int32 T = V(FVector(0.f, 0.f, 60.f), C0, 0.f), L = V(FVector(0.f, -45.f, 15.f), C0, 2.f), R = V(FVector(0.f, 45.f, 15.f), C0, 2.f);
				const int32 B = V(FVector(0.f, 0.f, -70.f), C1, 1.f), L2 = V(FVector(0.f, -45.f, 15.f), C1, 2.f), R2 = V(FVector(0.f, 45.f, 15.f), C1, 2.f);
				M.Tri(T, L, R);
				M.Tri(L2, B, R2);
				M.Ribbon(FVector(0.f, 0.f, -70.f), 320.f, 7.f, C0, 0.1f, 8);
			}
			else if (Shape == 1)
			{
				const int32 Nose = V(FVector(0.f, 0.f, 55.f), C0, 0.f), L = V(FVector(8.f, -95.f, -35.f), C0, 5.f), R = V(FVector(8.f, 95.f, -35.f), C0, 5.f);
				const int32 Keel = V(FVector(0.f, 0.f, -30.f), C1, 1.f), Nose2 = V(FVector(0.f, 0.f, 55.f), C1, 0.f);
				M.Tri(Nose, L, Keel);
				M.Tri(Nose2, Keel, R);
				M.Ribbon(FVector(6.f, -80.f, -35.f), 180.f, 5.f, C1, 0.4f, 5);
				M.Ribbon(FVector(6.f, 80.f, -35.f), 180.f, 5.f, C1, 0.7f, 5);
			}
			else
			{
				for (const float Z : { 30.f, -45.f })
				{
					const FLinearColor& C = Z > 0.f ? C0 : C1;
					const FVector P[4] = { FVector(-22.f, -22.f, 0.f), FVector(22.f, -22.f, 0.f), FVector(22.f, 22.f, 0.f), FVector(-22.f, 22.f, 0.f) };
					for (int32 S = 0; S < 4; S++)
					{
						const FVector A = P[S], Bq = P[(S + 1) % 4];
						const int32 A0 = V(A + FVector(0.f, 0.f, Z), C, 0.f), B0 = V(Bq + FVector(0.f, 0.f, Z), C, 0.f);
						const int32 B1 = V(Bq + FVector(0.f, 0.f, Z + 28.f), C, 0.f), A1 = V(A + FVector(0.f, 0.f, Z + 28.f), C, 0.f);
						M.Quad(A0, B0, B1, A1);
					}
				}
				M.Ribbon(FVector(0.f, 0.f, -45.f), 140.f, 6.f, C0, 0.6f, 4);
			}
			Meshes.Add(M.Build(this, *FString::Printf(TEXT("Kite_%d_%d"), Shape, Tint)));
		}
	}
	// the fliers stand along the beach, a little apart; the people's life brings the crowd, the kites fly by themselves
	uint32 Rng = GetTypeHash(At) | 1u;
	const FVector Along = FVector::CrossProduct(FVector::UpVector, SeaDir).GetSafeNormal();
	const int32 Count = 12;
	FCollisionQueryParams Q(TEXT("Kites"), false, this);
	for (int32 I = 0; I < Count; I++)
	{
		FKite K;
		FVector P = At + Along * ((I - (Count - 1) * 0.5f) * FMath::Max(Radius, 1500.f) * 0.22f + (TownRand(Rng) - 0.5f) * 300.f) + SeaDir * (TownRand(Rng) - 0.3f) * 700.f;
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0.f, 0.f, 1500.f), P - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Q))
		{
			P = Hit.ImpactPoint;
		}
		K.Hand = P + FVector(0.f, 0.f, 140.f);
		K.Line = 2600.f + TownRand(Rng) * 3800.f;
		K.Phase = TownRand(Rng) * 6.28f;
		K.At = K.Hand;
		Kites.Add(K);
		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(this);
		Body->SetStaticMesh(Meshes[I % Meshes.Num()]);
		Body->SetMaterial(0, ClothMat);
		Body->SetMobility(EComponentMobility::Movable);
		LyudFestParts::ClothComponent(Body, 80000.f);
		Body->WorldPositionOffsetDisableDistance = 30000;
		Body->SetWorldScale3D(FVector(1.6f));
		Body->SetBoundsScale(3.f);      // its tail streams out of the mesh's own box
		Body->SetupAttachment(RootComponent);
		Body->RegisterComponent();
		Body->SetVisibility(false);
		Bodies.Add(Body);
		UStaticMeshComponent* Ln = NewObject<UStaticMeshComponent>(this);
		Ln->SetStaticMesh(LineMesh);
		Ln->SetMaterial(0, LineMat);
		Ln->SetMobility(EComponentMobility::Movable);
		LyudFestParts::ClothComponent(Ln, 12000.f);
		Ln->SetupAttachment(RootComponent);
		Ln->RegisterComponent();
		Ln->SetVisibility(false);
		Lines.Add(Ln);
	}
}

void ALyudKites::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	Clock += DeltaTime;
	const double Now = (UGameCalendar::NowIn(World) - FDateTime(2000, 1, 1)).GetTotalSeconds();
	const bool bFlying = EndsAt > Now;
	// far from the camera a couple of moves a second are enough
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0); Cam && Kites.Num() > 0)
	{
		const bool bFar = FVector::DistSquared(Cam->GetCameraLocation(), Kites[0].Hand) > FMath::Square(60000.0);
		SetActorTickInterval(bFar ? 0.5f : 0.f);
	}
	const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(World);
	bool bAnyUp = false;
	for (int32 I = 0; I < Kites.Num(); I++)
	{
		FKite& K = Kites[I];
		FVector Air(0.0, 4.0, 0.0);
		float Speed = 4.f, Gust = 1.f;
		if (Wind)
		{
			const FLyudWindSample S = Wind->GetWindAt(K.Hand, 25.f, false);
			Air = S.Velocity;
			Speed = S.Speed;
			Gust = S.Gust;
		}
		// a fair afternoon on the beach has its sea breeze even when the day's wind is calm: the kites need it
		if (Speed < KiteBreeze)
		{
			Air = Speed > 0.5f ? Air.GetSafeNormal() * KiteBreeze : -SeaDir * KiteBreeze;
			Speed = KiteBreeze;
		}
		// under 2 m/s nothing stays up; the fliers pull them in when the hour is over
		const float Want = (bFlying && Speed > 2.f) ? 1.f : 0.f;
		K.Up = FMath::FInterpConstantTo(K.Up, Want, DeltaTime, Want > K.Up ? 0.12f : 0.2f);
		bAnyUp |= K.Up > 0.01f;
		const FVector Down = Air.GetSafeNormal2D(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Down);
		// a kite stands the higher the harder it blows, weaves a slow figure of eight and nods in the gusts
		const float Elev = FMath::DegreesToRadians(FMath::Clamp(28.f + 4.5f * Speed, 30.f, 68.f) + 6.f * (Gust - 1.f) + 4.f * FMath::Sin(Clock * 0.9f + K.Phase * 2.f));
		const float Weave = FMath::Sin(Clock * 0.45f + K.Phase) * 0.16f;
		const float Out = K.Line * FMath::Max(K.Up, 0.02f);
		const FVector Dir = (Down * FMath::Cos(Elev) + Side * Weave + FVector::UpVector * FMath::Sin(Elev) * FMath::Lerp(0.15f, 1.f, K.Up)).GetSafeNormal();
		const FVector Goal = K.Hand + Dir * Out;
		K.At = FMath::VInterpTo(K.At, Goal, DeltaTime, 1.6f);
		const bool bShow = K.Up > 0.01f;
		Bodies[I]->SetVisibility(bShow);
		Lines[I]->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		// its face down the line, leaning with the weave
		const FVector ToHand = (K.Hand - K.At).GetSafeNormal();
		FRotator Rot = FRotationMatrix::MakeFromXZ(-ToHand, FVector::UpVector).Rotator();
		Rot.Roll += Weave * 120.f;
		Bodies[I]->SetWorldLocationAndRotation(K.At, Rot);
		const float Len = float(FVector::Dist(K.Hand, K.At));
		Lines[I]->SetWorldTransform(FTransform((K.At - K.Hand).Rotation(), K.Hand, FVector(Len / 100.f, 1.f, 1.f)));
	}
	if (!bFlying && !bAnyUp && GetLifeSpan() <= 0.f)
	{
		SetLifeSpan(1.f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// poplar fluff

ULyudPoplarFluffSubsystem* ULyudPoplarFluffSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULyudPoplarFluffSubsystem>() : nullptr;
}

bool ULyudPoplarFluffSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId ULyudPoplarFluffSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudPoplarFluffSubsystem, STATGROUP_Tickables);
}

float ULyudPoplarFluffSubsystem::SeasonAt(float DayOfYear)
{
	// the female poplars' seed down: from the last days of May, thick through the first three weeks of June, gone by July
	const float In = FMath::SmoothStep(146.f, 156.f, DayOfYear);
	const float Out = 1.f - FMath::SmoothStep(172.f, 184.f, DayOfYear);
	return In * Out;
}

void ULyudPoplarFluffSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarFluff.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	bRunning = true;
	if (CVarTownPoplars.GetValueOnGameThread() != 0)
	{
		PlantTownPoplars(InWorld);
	}
}

void ULyudPoplarFluffSubsystem::Deinitialize()
{
	bRunning = false;
	if (IsValid(Holder))
	{
		Holder->Destroy();
	}
	if (IsValid(TownPoplars))
	{
		TownPoplars->Destroy();
	}
	Holder = nullptr;
	Tufts = nullptr;
	TuftMat = nullptr;
	TownPoplars = nullptr;
	Super::Deinitialize();
}

void ULyudPoplarFluffSubsystem::PlantTownPoplars(UWorld& World)
{
	// the town's own poplars (tools/town_poplars.py): one forest of hand-placed trees, like the public spaces' (shared
	// bakes, live plants only up close)
	const TSharedPtr<FJsonObject> Doc = TownJson(TEXT("City/poplars.json"));
	const TArray<TSharedPtr<FJsonValue>>* Trees = nullptr;
	if (!Doc.IsValid() || !Doc->TryGetArrayField(TEXT("trees"), Trees) || Trees->Num() == 0)
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	SP.bDeferConstruction = true;
	AProceduralForest* F = World.SpawnActor<AProceduralForest>(AProceduralForest::StaticClass(), FTransform(), SP);
	if (!F)
	{
		return;
	}
	F->bSnapToGround = false;
	F->Mix.Reset();
	F->Variants = 3;
	for (const TSharedPtr<FJsonValue>& V : *Trees)
	{
		const TSharedPtr<FJsonObject> T = V->AsObject();
		FVector At;
		if (T.IsValid() && TownPoint(T, TEXT("at"), At))
		{
			F->AddTree(FName(*T->GetStringField(TEXT("species"))), At, T->GetNumberField(TEXT("age")), (int32)T->GetNumberField(TEXT("variant")),
				T->GetNumberField(TEXT("yaw")), 1.f);
		}
	}
	F->Tags.Add(TEXT("City"));
	F->FinishSpawning(FTransform());
	TownPoplars = F;
	UE_LOG(LogTemp, Display, TEXT("Fluff: %d town poplars planted"), F->NumTrees());
}

void ULyudPoplarFluffSubsystem::FindPoplars()
{
	Poplars.Reset();
	UWorld* World = GetWorld();
	// a species' name is looked at once, not once a tree (the forests hold some hundred thousand)
	TMap<FName, bool> IsPoplar;
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		It->ForEachTree([this, &IsPoplar](FName Species, const FTransform& Xf, float Age)
		{
			if (Age <= 8.f)
			{
				return;
			}
			const bool* Known = IsPoplar.Find(Species);
			if (Known ? *Known : IsPoplar.Add(Species, Species.ToString().Contains(TEXT("Poplar"))))
			{
				Poplars.Add(FVector2D(Xf.GetLocation()));
			}
		});
	}
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		if (It->Species.ToString().Contains(TEXT("Poplar")))
		{
			Poplars.Add(FVector2D(It->GetActorLocation()));
		}
	}
}

void ULyudPoplarFluffSubsystem::MakeMesh()
{
	bMeshTried = true;
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TownFluffPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("Fluff: %s is missing, no poplar fluff (run make_festival_materials.py)"), TownFluffPath);
		return;
	}
	// the tufts: their places in the box are seeds for the material; the vertices themselves stand spread over the box so
	// the component's bounds are the box's
	const FVector Box(FluffBoxXY, FluffBoxXY, FluffBoxZ);
	const int32 Count = 4200;
	FLyudPartMesh M;
	uint32 Rng = 20260613u;
	for (int32 I = 0; I < Count; I++)
	{
		const FVector Seed3(TownRand(Rng), TownRand(Rng), TownRand(Rng));
		// most are small, a few are clumps
		const float Size = TownRand(Rng) < 0.08f ? 5.f + 3.f * TownRand(Rng) : 1.5f + 2.2f * TownRand(Rng);
		const float Phase = TownRand(Rng), Threshold = (I + TownRand(Rng)) / Count;
		const FVector P = (Seed3 - FVector(0.5)) * Box;
		int32 Corner[4];
		const FVector2f Cs[4] = { FVector2f(-1.f, -1.f), FVector2f(1.f, -1.f), FVector2f(1.f, 1.f), FVector2f(-1.f, 1.f) };
		for (int32 K = 0; K < 4; K++)
		{
			Corner[K] = M.Vert(P, FVector::UpVector, { Cs[K], FVector2f(Seed3.X, Seed3.Y), FVector2f(Seed3.Z, Size), FVector2f(Phase, Threshold) });
		}
		M.Quad(Corner[0], Corner[1], Corner[2], Corner[3]);
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
	if (!Holder)
	{
		return;
	}
	Tufts = NewObject<UStaticMeshComponent>(Holder, TEXT("PoplarFluff"));
	Holder->SetRootComponent(Tufts);
	Tufts->SetStaticMesh(M.Build(Holder, TEXT("PoplarFluff")));
	TuftMat = UMaterialInstanceDynamic::Create(Mat, Holder);
	Tufts->SetMaterial(0, TuftMat);
	Tufts->SetMobility(EComponentMobility::Movable);
	LyudFestParts::ClothComponent(Tufts, 0.f);
	Tufts->WorldPositionOffsetDisableDistance = 0;
	Tufts->SetBoundsScale(1.3f);
	Tufts->RegisterComponent();
	Tufts->SetVisibility(false);
}

void ULyudPoplarFluffSubsystem::UpdateAmount()
{
	UWorld* World = GetWorld();
	const float Forced = CVarFluffAmount.GetValueOnGameThread();
	Season = SeasonAt(UGameCalendar::DayOfYear(UGameCalendar::NowIn(World)));
	if (Forced < 0.f && Season <= 0.f)
	{
		Target = 0.f;
		return;
	}
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0);
	const FVector Eye = Cam ? Cam->GetCameraLocation() : FVector::ZeroVector;
	// how near the poplars are: full within 40 m of one, a few tufts a third of a kilometre off, none a kilometre away
	double Best = TNumericLimits<double>::Max();
	const FVector2D Eye2(Eye);
	for (const FVector2D& P : Poplars)
	{
		Best = FMath::Min(Best, FVector2D::DistSquared(P, Eye2));
	}
	const float Dist = Poplars.Num() > 0 ? float(FMath::Sqrt(Best)) : 1e9f;
	Near = FMath::Lerp(1.f, 0.14f, FMath::SmoothStep(4000.f, 35000.f, Dist)) * (1.f - FMath::SmoothStep(60000.f, 100000.f, Dist));
	float Dry = 1.f, Blow = 0.7f;
	if (const ULyudWeatherSubsystem* Weather = ULyudWeatherSubsystem::Get(World))
	{
		const FLyudSkyWeather Sky = Weather->GetSky();
		Dry = (1.f - FMath::Clamp(Sky.Rain / 0.3f, 0.f, 1.f)) * (1.f - 0.6f * FMath::Clamp(Sky.Wet, 0.f, 1.f));
	}
	if (const ULyudWindSubsystem* Wind = ULyudWindSubsystem::Get(World))
	{
		// in still air it lies where it fell; a breeze lifts it
		Blow = 0.3f + 0.7f * FMath::Clamp(Wind->GetMeanSpeed() / 4.f, 0.f, 1.f);
	}
	// indoors there is none (the box would drift through the walls)
	bool bOutdoors = true;
	if (TActorIterator<ALyudSky> It(World); It)
	{
		bOutdoors = It->IsPlayerOutdoors();
	}
	Target = Forced >= 0.f ? FMath::Clamp(Forced, 0.f, 1.f) * (bOutdoors ? 1.f : 0.f) : (bOutdoors ? Season * Near * Dry * Blow : 0.f);
}

void ULyudPoplarFluffSubsystem::Tick(float DeltaTime)
{
	Timer -= DeltaTime;
	if (Timer <= 0.f)
	{
		Timer = 0.5f;
		// the forests lay their trees out over their first seconds: looked at a few times, then kept
		// (only when there can be fluff at all: out of its season the walk over every tree is for nothing)
		if (PoplarLooks < 4 && (CVarFluffAmount.GetValueOnGameThread() >= 0.f || SeasonAt(UGameCalendar::DayOfYear(UGameCalendar::NowIn(GetWorld()))) > 0.f))
		{
			PoplarTimer -= 0.5f;
			if (PoplarTimer <= 0.f)
			{
				PoplarTimer = 10.f;
				PoplarLooks++;
				FindPoplars();
			}
		}
		UpdateAmount();
		if (Target > 0.f && !bMeshTried)
		{
			MakeMesh();
		}
	}
	if (!Tufts)
	{
		Amount = Target;
		return;
	}
	Amount = FMath::FInterpConstantTo(Amount, Target, DeltaTime, 0.12f);
	const bool bShow = Amount > 0.003f;
	if (bShow != Tufts->IsVisible())
	{
		Tufts->SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}
	// the box rides on the camera (only its bounds matter: the material puts every tuft where it belongs)
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Holder->SetActorLocation(Cam->GetCameraLocation());
	}
	TuftMat->SetVectorParameterValue(TEXT("Fluff"), FLinearColor(FluffBoxXY, FluffBoxZ, Amount, 9.f));
}

FString ULyudPoplarFluffSubsystem::Report() const
{
	return FString::Printf(TEXT("FLUFF amount %.2f (target %.2f) season %.2f near %.2f, %d poplars known, %d of the town's planted, mesh %s"),
		Amount, Target, Season, Near, Poplars.Num(), TownPoplars ? TownPoplars->NumTrees() : 0, Tufts ? TEXT("made") : (bMeshTried ? TEXT("no material") : TEXT("not yet")));
}

static FAutoConsoleCommandWithWorld GFluffReport(TEXT("lyud.Fluff.Report"), TEXT("lyud.Fluff.Report: how much poplar fluff is in the air and why"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const ULyudPoplarFluffSubsystem* F = ULyudPoplarFluffSubsystem::Get(World))
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *F->Report());
		}
	}));

// developer mode (F7, Actions): a fluff storm on and off
static LyudDev::FAutoAction GDevFluff(TEXT("weather.fluff"), TEXT("weather"), TEXT("dev.action.weather.fluff"),
	[](UWorld* W)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Fluff.Amount")))
		{
			V->Set(V->GetFloat() >= 0.f ? -1.f : 1.f, ECVF_SetByConsole);
		}
	},
	[](UWorld* W) { return CVarFluffAmount.GetValueOnGameThread() >= 0.f; });
