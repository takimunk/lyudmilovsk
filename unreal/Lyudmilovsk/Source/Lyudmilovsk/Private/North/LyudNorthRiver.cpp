#include "North/LyudNorthRiver.h"

#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "North/LyudNorthFx.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "StaticMeshAttributes.h"

namespace
{
	const TCHAR* RiverMatPath = TEXT("/Game/North/Materials/M_North_River.M_North_River");
	const TCHAR* RiverMatFallback = TEXT("/Game/North/Materials/M_North_Water.M_North_Water");
	const TCHAR* RiverMatEngine = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* RiverSoundPath = TEXT("/Game/Audio/North/S_north_river_01.S_north_river_01");

	/** Catmull-Rom steps between two samples of north.json (15 m -> ~5 m). */
	constexpr int32 RiverSub = 3;
	/** Points of the smoothed line per mesh part (~320 m): the parts off screen are culled. */
	constexpr int32 RiverPartPoints = 64;
	/** The material's noise repeats every this many V units (import_north.py RIVER_REPEAT): each part's V starts at a
	 *  multiple of it, so the numbers stay small and the joins match. */
	constexpr float RiverRepeatV = 64.f;
	/** The water is not drawn farther than this. */
	constexpr float RiverDrawCm = 150000.f;
	/** The flow loop: full within this of the centre line, then falling off over RiverHearCm. */
	constexpr float RiverInnerCm = 600.f;
	constexpr float RiverHearCm = 7000.f;
	constexpr float RiverLoopBase = 0.7f;

	FVector RiverCatmull(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, double T)
	{
		const double T2 = T * T, T3 = T2 * T;
		return 0.5 * ((2.0 * P1) + (P2 - P0) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T2 + (3.0 * P1 - P0 - 3.0 * P2 + P3) * T3);
	}

	float RiverFast01(float Speed) { return FMath::Clamp((Speed - 0.95f) / 0.7f, 0.f, 1.f); }
}

ALyudNorthRiver::ALyudNorthRiver()
{
	// awake it ticks four times a second (the sound's place); asleep not at all (ALyudNorthThing)
	PrimaryActorTick.TickInterval = 0.25f;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void ALyudNorthRiver::Setup(const TArray<FVector4>& InSamples)
{
	if (bBuilt || InSamples.Num() < 2)
	{
		return;
	}
	bBuilt = true;
	Samples = InSamples;
	BuildPoints();
	if (Points.Num() < 2)
	{
		return;
	}
	SetActorLocation(Points[0].Pos);
	BuildParts();
	UE_LOG(LogTemp, Display, TEXT("NORTH river: %d samples, %d points, %.0f m, %d parts"), Samples.Num(), Points.Num(), LengthM(), Parts.Num());
}

void ALyudNorthRiver::BuildPoints()
{
	const int32 N = Samples.Num();
	auto Plan = [this, N](int32 I)
	{
		const FVector4& S = Samples[FMath::Clamp(I, 0, N - 1)];
		return FVector(S.X, S.Y, 0.0);
	};
	// how fast each sample runs: the fall over its neighbours, quicker where the sheet is narrow (0.6 .. 1.8)
	TArray<float> Speed;
	Speed.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		const int32 A = FMath::Max(I - 1, 0), B = FMath::Min(I + 1, N - 1);
		const double Run = FVector::Dist2D(FVector(Samples[A]), FVector(Samples[B]));
		const double Fall = FMath::Max(0.0, Samples[A].Z - Samples[B].Z);
		const float Slope = Run > 1.0 ? float(Fall / Run) : 0.f;
		const float HalfM = FMath::Max(0.5f, float(Samples[I].W) / 100.f);
		const float Narrow = FMath::Clamp(FMath::Sqrt(5.f / HalfM), 0.75f, 1.3f);
		Speed[I] = FMath::Clamp((0.6f + 9.f * FMath::Sqrt(Slope)) * Narrow, 0.6f, 1.8f);
	}
	// smoothed once ([1 2 1]) so the foam does not come and go sample by sample
	TArray<float> Smooth = Speed;
	for (int32 I = 1; I + 1 < N; ++I)
	{
		Smooth[I] = 0.25f * Speed[I - 1] + 0.5f * Speed[I] + 0.25f * Speed[I + 1];
	}

	Points.Reset();
	auto AddPoint = [this](const FVector& Pos, float HalfCm, float Spd)
	{
		if (Points.Num() && FVector::DistSquared2D(Points.Last().Pos, Pos) < 1.0)
		{
			return;
		}
		FRiverPoint& P = Points.AddDefaulted_GetRef();
		P.Pos = Pos;
		P.Half = FMath::Max(HalfCm, 20.f);
		P.Speed = Spd;
	};
	for (int32 I = 0; I + 1 < N; ++I)
	{
		for (int32 K = 0; K < RiverSub; ++K)
		{
			const double T = double(K) / RiverSub;
			const FVector XY = RiverCatmull(Plan(I - 1), Plan(I), Plan(I + 1), Plan(I + 2), T);
			const double Z = FMath::Lerp(Samples[I].Z, Samples[I + 1].Z, T);
			AddPoint(FVector(XY.X, XY.Y, Z), float(FMath::Lerp(Samples[I].W, Samples[I + 1].W, T)),
				FMath::Lerp(Smooth[I], Smooth[I + 1], float(T)));
		}
	}
	AddPoint(FVector(Samples[N - 1]), float(Samples[N - 1].W), Smooth[N - 1]);

	// the sides (right looking downstream: Up x Forward) and the distance along
	const int32 M = Points.Num();
	TotalLengthCm = 0.f;
	for (int32 I = 0; I < M; ++I)
	{
		const FVector& Prev = Points[FMath::Max(I - 1, 0)].Pos;
		const FVector& Next = Points[FMath::Min(I + 1, M - 1)].Pos;
		FVector Fwd = Next - Prev;
		Fwd.Z = 0.0;
		Fwd = Fwd.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);
		Points[I].Side = FVector(-Fwd.Y, Fwd.X, 0.0);
		if (I > 0)
		{
			TotalLengthCm += float(FVector::Dist(Points[I - 1].Pos, Points[I].Pos));
		}
		Points[I].AlongCm = TotalLengthCm;
	}
}

void ALyudNorthRiver::BuildParts()
{
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, RiverMatPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("NORTH river: no %s (import_north.py materials), using the spring's water"), RiverMatPath);
		Mat = LoadObject<UMaterialInterface>(nullptr, RiverMatFallback, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Mat)
	{
		Mat = LoadObject<UMaterialInterface>(nullptr, RiverMatEngine, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// the edges, once for the whole line (the parts share their end points): on the inside of a tight bend an edge would
	// run backwards and fold the sheet over itself, so there it waits at its last point
	const int32 M = Points.Num();
	TArray<FVector> LeftE, RightE;
	TArray<float> VAt;
	LeftE.SetNum(M);
	RightE.SetNum(M);
	VAt.SetNum(M);
	for (int32 I = 0; I < M; ++I)
	{
		const FRiverPoint& P = Points[I];
		LeftE[I] = P.Pos - P.Side * P.Half;
		RightE[I] = P.Pos + P.Side * P.Half;
		VAt[I] = 0.f;
		if (I > 0)
		{
			const FVector Fwd(P.Side.Y, -P.Side.X, 0.0);
			if (FVector::DotProduct(LeftE[I] - LeftE[I - 1], Fwd) < 0.0)
			{
				LeftE[I] = FVector(LeftE[I - 1].X, LeftE[I - 1].Y, P.Pos.Z);
			}
			if (FVector::DotProduct(RightE[I] - RightE[I - 1], Fwd) < 0.0)
			{
				RightE[I] = FVector(RightE[I - 1].X, RightE[I - 1].Y, P.Pos.Z);
			}
			// V: metres along over the flow speed, so a constant panner runs faster (and streakier) where it is fast
			const float StepM = (P.AlongCm - Points[I - 1].AlongCm) / 100.f;
			VAt[I] = VAt[I - 1] + StepM / (0.5f * (P.Speed + Points[I - 1].Speed));
		}
	}

	const FVector ActorAt = GetActorLocation();
	for (int32 Start = 0; Start + 1 < M; Start += RiverPartPoints)
	{
		const int32 End = FMath::Min(Start + RiverPartPoints, M - 1);
		const FVector Origin = Points[Start].Pos;
		const float VBase = RiverRepeatV * FMath::FloorToFloat(VAt[Start] / RiverRepeatV);

		FMeshDescription MD;
		FStaticMeshAttributes Attr(MD);
		Attr.Register();
		const FPolygonGroupID PG = MD.CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[PG] = TEXT("Water");
		TArray<FVertexInstanceID> LeftI, RightI;
		for (int32 I = Start; I <= End; ++I)
		{
			const FRiverPoint& P = Points[I];
			// G: the sheet fades in at the first point and out at the mouth
			const float Ends = (I == 0 || I == M - 1) ? 1.f : 0.f;
			const FVector4f Colour(RiverFast01(P.Speed), Ends, 0.f, 1.f);
			const FVector3f Tangent = FVector3f(P.Side);
			for (int32 S = 0; S < 2; ++S)
			{
				const FVertexID V = MD.CreateVertex();
				Attr.GetVertexPositions()[V] = FVector3f((S == 0 ? LeftE[I] : RightE[I]) - Origin);
				const FVertexInstanceID VI = MD.CreateVertexInstance(V);
				Attr.GetVertexInstanceNormals()[VI] = FVector3f::UpVector;
				// tangent X along U (across, left -> right), the binormal (Up x Side * -1) downstream along V
				Attr.GetVertexInstanceTangents()[VI] = Tangent;
				Attr.GetVertexInstanceBinormalSigns()[VI] = -1.f;
				Attr.GetVertexInstanceUVs().Set(VI, 0, FVector2f(S == 0 ? 0.f : 1.f, VAt[I] - VBase));
				Attr.GetVertexInstanceColors()[VI] = Colour;
				(S == 0 ? LeftI : RightI).Add(VI);
			}
		}
		for (int32 K = 0; K + 1 < LeftI.Num(); ++K)
		{
			MD.CreateTriangle(PG, { LeftI[K], RightI[K + 1], LeftI[K + 1] });
			MD.CreateTriangle(PG, { LeftI[K], RightI[K], RightI[K + 1] });
		}

		UStaticMesh* Mesh = NewObject<UStaticMesh>(this, MakeUniqueObjectName(this, UStaticMesh::StaticClass(), TEXT("SM_NorthRiver")),
			RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Mat, TEXT("Water")));
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		// V runs to a few hundred: half floats would step it
		UStaticMesh::FBuildMeshDescriptionsLODParams Lod;
		Lod.bUseFullPrecisionUVs = true;
		Params.PerLODOverrides.Add(Lod);
		Mesh->BuildFromMeshDescriptions({ &MD }, Params);
		Meshes.Add(Mesh);

		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Mesh);
		C->SetupAttachment(RootComponent);
		LyudNorthFx::Lightweight(C);
		C->SetRelativeLocation(Origin - ActorAt);
		C->SetCachedMaxDrawDistance(RiverDrawCm);
		C->LDMaxDrawDistance = RiverDrawCm;
		C->RegisterComponent();
		Parts.Add(C);
	}
}

bool ALyudNorthRiver::NearestPoint(const FVector& From, FVector& OutPoint, float& OutHalfCm, float& OutSpeed01) const
{
	if (Points.Num() < 2)
	{
		return false;
	}
	const FVector2D At(From.X, From.Y);
	double Best = TNumericLimits<double>::Max();
	for (int32 I = 0; I + 1 < Points.Num(); ++I)
	{
		const FRiverPoint& A = Points[I];
		const FRiverPoint& B = Points[I + 1];
		const FVector2D A2(A.Pos.X, A.Pos.Y);
		const FVector2D AB = FVector2D(B.Pos.X, B.Pos.Y) - A2;
		const double Len2 = AB.SizeSquared();
		const double T = Len2 > 1.0 ? FMath::Clamp(FVector2D::DotProduct(At - A2, AB) / Len2, 0.0, 1.0) : 0.0;
		const double D2 = FVector2D::DistSquared(At, A2 + AB * T);
		if (D2 < Best)
		{
			Best = D2;
			OutPoint = FMath::Lerp(A.Pos, B.Pos, T);
			OutHalfCm = FMath::Lerp(A.Half, B.Half, float(T));
			OutSpeed01 = RiverFast01(FMath::Lerp(A.Speed, B.Speed, float(T)));
		}
	}
	return true;
}

float ALyudNorthRiver::RangeDistanceCm(const FVector& From) const
{
	FVector P = FVector::ZeroVector;
	float HalfCm = 0.f, Fast = 0.f;
	if (!NearestPoint(From, P, HalfCm, Fast))
	{
		return Super::RangeDistanceCm(From);
	}
	// from the water's edge, not the actor (the river is kilometres long)
	const double Across = FMath::Max(0.0, FVector::Dist2D(From, P) - double(HalfCm));
	const double Up = From.Z - P.Z;
	return float(FMath::Sqrt(Across * Across + Up * Up));
}

bool ALyudNorthRiver::IsInWater(const FVector& WorldLocation, float DepthCm) const
{
	FVector P = FVector::ZeroVector;
	float HalfCm = 0.f, Fast = 0.f;
	return NearestPoint(WorldLocation, P, HalfCm, Fast) && FVector::Dist2D(WorldLocation, P) <= double(HalfCm)
		&& WorldLocation.Z <= P.Z + 50.0 && WorldLocation.Z >= P.Z - double(DepthCm);
}

void ALyudNorthRiver::SetAwake(bool bInAwake, bool bInNear)
{
	Super::SetAwake(bInAwake, bInNear);
	if (bNear)
	{
		StartLoop();
	}
	else
	{
		StopLoop();
	}
}

void ALyudNorthRiver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bNear)
	{
		UpdateLoop();
	}
}

void ALyudNorthRiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Loop)
	{
		Loop->Stop();
		Loop = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ALyudNorthRiver::StartLoop()
{
	if (Loop || Points.Num() < 2)
	{
		return;
	}
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, RiverSoundPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sound)
	{
		if (!bWarnedSound)
		{
			bWarnedSound = true;
			UE_LOG(LogTemp, Warning, TEXT("NORTH river: no %s (tools/north_sounds.py, import_north.sh sounds): silent"), RiverSoundPath);
		}
		return;
	}
	UAudioComponent* A = NewObject<UAudioComponent>(this);
	A->bAutoActivate = false;
	A->bAutoDestroy = false;
	A->bAllowSpatialization = true;
	A->SetSound(Sound);
	A->bOverrideAttenuation = true;
	FSoundAttenuationSettings& S = A->AttenuationOverrides;
	S.bAttenuate = true;
	S.bSpatialize = true;
	S.AttenuationShape = EAttenuationShape::Sphere;
	S.AttenuationShapeExtents = FVector(RiverInnerCm, 0.f, 0.f);
	S.FalloffDistance = RiverHearCm;
	S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	S.dBAttenuationAtMax = -50.f;
	// standing in it or on its bank it is all round you
	S.NonSpatializedRadiusStart = 500.f;
	S.NonSpatializedRadiusEnd = 200.f;
	S.bAttenuateWithLPF = true;
	S.LPFRadiusMin = 1500.f;
	S.LPFRadiusMax = RiverHearCm;
	S.LPFFrequencyAtMin = 20000.f;
	S.LPFFrequencyAtMax = 2500.f;
	S.bEnableOcclusion = false;
	A->SetupAttachment(RootComponent);
	A->SetUsingAbsoluteLocation(true);
	A->RegisterComponent();
	Loop = A;
	LoopLevel = -1.f;
	LoopStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	UpdateLoop();
	Loop->FadeIn(2.f, FMath::Max(LoopLevel, 0.f));
}

void ALyudNorthRiver::StopLoop()
{
	if (Loop)
	{
		// faded out, then gone
		Loop->bAutoDestroy = true;
		Loop->FadeOut(1.5f, 0.f);
		Loop = nullptr;
	}
}

void ALyudNorthRiver::UpdateLoop()
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!Loop || !PC)
	{
		return;
	}
	// the listener is the camera; else the pawn
	FVector Ear = FVector::ZeroVector;
	if (PC->PlayerCameraManager)
	{
		Ear = PC->PlayerCameraManager->GetCameraLocation();
	}
	else if (const APawn* Pawn = PC->GetPawn())
	{
		Ear = Pawn->GetActorLocation();
	}
	else
	{
		return;
	}
	FVector At = FVector::ZeroVector;
	float HalfCm = 0.f, Fast = 0.f;
	if (!NearestPoint(Ear, At, HalfCm, Fast))
	{
		return;
	}
	Loop->SetWorldLocation(At + FVector(0.0, 0.0, 30.0));
	float Volume = RiverLoopBase * FMath::Lerp(0.55f, 1.15f, Fast) * FMath::Clamp(FMath::Sqrt(HalfCm / 500.f), 0.6f, 1.3f);
	if (const ULyudSoundscapeSubsystem* Sc = ULyudSoundscapeSubsystem::Get(this))
	{
		Volume *= Sc->MasterVolume * Sc->EffectsVolume;
	}
	if (LoopLevel < 0.f)
	{
		// the first call: StartLoop fades in to it (later: StartLoop found no listener and faded in to silence)
		LoopLevel = Volume;
		if (World->GetTimeSeconds() > LoopStartTime)
		{
			Loop->AdjustVolume(1.5f, LoopLevel);
		}
		return;
	}
	// a fast reach is louder: eased, and only once the fade-in is over
	if (FMath::Abs(Volume - LoopLevel) > 0.04f && World->GetTimeSeconds() - LoopStartTime > 2.5)
	{
		LoopLevel = Volume;
		Loop->AdjustVolume(0.75f, LoopLevel);
	}
}
