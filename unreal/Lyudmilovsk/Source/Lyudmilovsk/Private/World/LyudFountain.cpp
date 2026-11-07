#include "World/LyudFountain.h"

#include "../Audio/LyudFountainSynth.h"
#include "../Weather/WeatherModel.h"
#include "../Weather/WindModel.h"
#include "Audio/LyudSoundscape.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "Weather/LyudWeather.h"

static TAutoConsoleVariable<int32> CVarFountain(TEXT("lyud.Fountain"), 1,
	TEXT("The Academgorodok fountain's water and sound (ALyudFountain): 1 when the outdoor map starts, 0 not."));
static TAutoConsoleVariable<int32> CVarFountainForce(TEXT("lyud.Fountain.Force"), -1,
	TEXT("The fountain: -1 runs by the season (1 May .. the first frost), 0 drained, 1 running."));
static TAutoConsoleVariable<int32> CVarFountainDebug(TEXT("lyud.Fountain.Debug"), 0,
	TEXT("1: the fountain logs its state, the season's dates and the sound's level every few seconds."));

namespace
{
	const TCHAR* WaterPath = TEXT("/Game/Academgorodok/Fountain/M_FountainWater.M_FountainWater");
	const TCHAR* StreamPath = TEXT("/Game/Academgorodok/Fountain/M_FountainStream.M_FountainStream");
	const TCHAR* DropsPath = TEXT("/Game/Academgorodok/Fountain/M_FountainDrops.M_FountainDrops");
	const TCHAR* LeavesPath = TEXT("/Game/Academgorodok/Fountain/M_FountainLeaves.M_FountainLeaves");
	constexpr float G = 9.81f;               // m/s2
	constexpr float HearCm = 5000.f;         // the sound plays within this of the camera
	constexpr float FrostC = -0.5f;          // an air frost: the fountain is turned off for the winter

	/** One section of a mesh with up to five UV channels (the materials read flow time, seeds, velocities from them). */
	struct FPartMesh
	{
		static constexpr int32 NumUV = 5;
		TArray<FVector3f> Pos, Nrm, Tan;
		TArray<FVector2f> UV[NumUV];
		TArray<int32> Idx;

		int32 Vert(const FVector& P, const FVector& N, const FVector& T, std::initializer_list<FVector2f> UVs)
		{
			Pos.Add(FVector3f(P));
			Nrm.Add(FVector3f(N.GetSafeNormal()));
			Tan.Add(FVector3f(T.GetSafeNormal()));
			int32 K = 0;
			for (const FVector2f& U : UVs)
			{
				UV[K++].Add(U);
			}
			for (; K < NumUV; K++)
			{
				UV[K].Add(FVector2f::ZeroVector);
			}
			return Pos.Num() - 1;
		}
		/** A B C D counterclockwise seen from the front (as the cable car's mesh kit). */
		void Quad(int32 A, int32 B, int32 C, int32 D) { Idx.Append({ A, B, C, A, C, D }); }

		UStaticMesh* Build(UObject* Outer, FName Name) const
		{
			FMeshDescription MD;
			FStaticMeshAttributes Attr(MD);
			Attr.Register();
			TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> VTan = Attr.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> VSign = Attr.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
			VUV.SetNumChannels(NumUV);
			UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, MakeUniqueObjectName(Outer, UStaticMesh::StaticClass(), Name),
				RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
			const FPolygonGroupID PG = MD.CreatePolygonGroup();
			Attr.GetPolygonGroupMaterialSlotNames()[PG] = Name;
			Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, Name));
			TArray<FVertexInstanceID> Inst;
			Inst.Reserve(Pos.Num());
			for (int32 I = 0; I < Pos.Num(); I++)
			{
				const FVertexID V = MD.CreateVertex();
				VPos[V] = Pos[I];
				const FVertexInstanceID VI = MD.CreateVertexInstance(V);
				VNrm[VI] = Nrm[I];
				VTan[VI] = Tan[I];
				VSign[VI] = -1.f;
				for (int32 K = 0; K < NumUV; K++)
				{
					VUV.Set(VI, K, UV[K][I]);
				}
				Inst.Add(VI);
			}
			for (int32 I = 0; I + 2 < Idx.Num(); I += 3)
			{
				// counterclockwise in a right-handed frame; Unreal's front faces wind the other way
				MD.CreateTriangle(PG, { Inst[Idx[I]], Inst[Idx[I + 2]], Inst[Idx[I + 1]] });
			}
			UStaticMesh::FBuildMeshDescriptionsParams Params;
			Params.bMarkPackageDirty = false;
			Params.bBuildSimpleCollision = false;
			Params.bCommitMeshDescription = false;
			Params.bFastBuild = true;
			Mesh->BuildFromMeshDescriptions({ &MD }, Params);
			return Mesh;
		}
	};

	FVector Radial(float A) { return FVector(FMath::Cos(A), FMath::Sin(A), 0.f); }

	/** A flat ring at height Z (m over the foot) from R0 to R1 (m), UV0 = metres from the axis; normal up. */
	void Disc(FPartMesh& M, float Z, float R0, float R1, int32 NR, int32 NA)
	{
		const int32 Base = M.Pos.Num();
		for (int32 I = 0; I <= NR; I++)
		{
			const float R = FMath::Lerp(R0, R1, float(I) / NR);
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				const FVector P = Radial(A) * R;
				M.Vert(FVector(P.X, P.Y, Z) * 100.f, FVector::UpVector, FVector::ForwardVector, { FVector2f(P.X, P.Y) });
			}
		}
		for (int32 I = 0; I < NR; I++)
		{
			for (int32 J = 0; J < NA; J++)
			{
				const int32 A = Base + I * (NA + 1) + J, B = A + (NA + 1);
				M.Quad(A, B, B + 1, A + 1);
			}
		}
	}

	/**
	 * A sheet of water round the axis: leaving radius R0 at Z0 (m) with radial speed Vr, upward Vz (m/s), falling until
	 * Z1, in NV steps of its flow time. UV0 = (strands round: U * Strands, the flow time s since the nozzle + T0),
	 * UV1 = (0..1 along the sheet, sheet kind). The material moves its streaks with the water: pattern(V - time).
	 */
	void Sheet(FPartMesh& M, float R0, float Z0, float Vr, float Vz, float Z1, float T0, int32 NA, int32 NV, float Strands, float Kind)
	{
		// time to fall from Z0 to Z1: Z0 + Vz t - g t^2 / 2 = Z1
		const float TEnd = (Vz + FMath::Sqrt(Vz * Vz + 2.f * G * (Z0 - Z1))) / G;
		const int32 Base = M.Pos.Num();
		for (int32 I = 0; I <= NV; I++)
		{
			// denser steps near the start where the curve bends most
			const float K = float(I) / NV;
			const float T = TEnd * K;
			const float R = R0 + Vr * T, Z = Z0 + Vz * T - 0.5f * G * T * T;
			const FVector2D Vel(Vr, Vz - G * T);
			// normal of the sheet in the (r, z) plane, to the outside of the fall
			const FVector2D N2(-Vel.Y, Vel.X);
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				const FVector Rd = Radial(A);
				const FVector P = Rd * R + FVector(0.f, 0.f, Z);
				const FVector N = Rd * N2.X + FVector(0.f, 0.f, N2.Y);
				const FVector Tg(-Rd.Y, Rd.X, 0.f);
				M.Vert(P * 100.f, N, Tg, { FVector2f(float(J) / NA * Strands, T0 + T), FVector2f(K, Kind) });
			}
		}
		for (int32 I = 0; I < NV; I++)
		{
			for (int32 J = 0; J < NA; J++)
			{
				const int32 A = Base + I * (NA + 1) + J, B = A + (NA + 1);
				M.Quad(A, B, B + 1, A + 1);
			}
		}
	}

	/** The rising jet: a thin column from the nozzle (Z0, speed V0 up) to its top; the same UVs as Sheet. */
	void Column(FPartMesh& M, float Z0, float V0, float R, int32 NA, int32 NV)
	{
		const float TTop = V0 / G;
		const int32 Base = M.Pos.Num();
		for (int32 I = 0; I <= NV; I++)
		{
			const float K = float(I) / NV;
			const float T = TTop * K;
			const float Z = Z0 + V0 * T - 0.5f * G * T * T;
			// it swells a little as it slows
			const float Rr = R * (1.f + 0.6f * K * K);
			for (int32 J = 0; J <= NA; J++)
			{
				const float A = 2.f * PI * J / NA;
				const FVector Rd = Radial(A);
				M.Vert((Rd * Rr + FVector(0.f, 0.f, Z)) * 100.f, Rd, FVector(-Rd.Y, Rd.X, 0.f), { FVector2f(float(J) / NA * 3.f, T), FVector2f(K, 0.f) });
			}
		}
		for (int32 I = 0; I < NV; I++)
		{
			for (int32 J = 0; J < NA; J++)
			{
				const int32 A = Base + I * (NA + 1) + J, B = A + (NA + 1);
				M.Quad(A, B, B + 1, A + 1);
			}
		}
	}

	/**
	 * Drops thrown from P (m over the foot) with velocity V (m/s), living TEnd s of each Period (staggered by a seed):
	 * a quad whose four corners all sit at P; the material moves it along the parabola and turns it to the camera.
	 * UV0 corner -1..1, UV1 (Vx, Vy), UV2 (Vz, seed), UV3 (period, life), UV4 (radius m).
	 */
	void Drop(FPartMesh& M, const FVector& P, const FVector& V, float TEnd, float Period, float Seed, float Radius)
	{
		const int32 Base = M.Pos.Num();
		const FVector2f C[4] = { { -1.f, -1.f }, { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, 1.f } };
		for (const FVector2f& Cn : C)
		{
			M.Vert(P * 100.f, FVector::UpVector, FVector::ForwardVector,
				{ Cn, FVector2f(V.X, V.Y), FVector2f(V.Z, Seed), FVector2f(Period, TEnd), FVector2f(Radius, 0.f) });
		}
		M.Quad(Base, Base + 1, Base + 2, Base + 3);
	}

	/** Time for a drop at height Z0 going up at Vz to come down to Z1. */
	float FallTime(float Z0, float Vz, float Z1) { return (Vz + FMath::Sqrt(FMath::Max(Vz * Vz + 2.f * G * (Z0 - Z1), 0.f))) / G; }

	double HoursOf(const FDateTime& T)
	{
		return LyudWeather::HoursSince2000(T.GetYear(), T.GetMonth(), T.GetDay(), T.GetTimeOfDay().GetTotalHours());
	}
}

ALyudFountain::ALyudFountain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* ALyudFountain::MakePart(UStaticMesh* Mesh, const TCHAR* Name, const TCHAR* MaterialPath, bool bShadow, float DrawCm,
	TFunctionRef<void(UMaterialInstanceDynamic*)> Setup)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetStaticMesh(Mesh);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		Setup(M);
		C->SetMaterial(0, M);
		Mids.Add(M);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudFountain: no material %s (unreal/scripts/make_fountain_materials.py)"), MaterialPath);
	}
	C->SetMobility(EComponentMobility::Static);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	C->SetCastShadow(bShadow);
	// water that moves in its material: no GI of its own, no distance field (docs/benchmark.md "Live systems and Lumen")
	C->bAffectDynamicIndirectLighting = false;
	C->bAffectDistanceFieldLighting = false;
	C->SetCachedMaxDrawDistance(DrawCm);
	C->LightingChannels.bChannel1 = true;      // the outdoor sun
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	return C;
}

bool ALyudFountain::Build(TSharedPtr<FJsonObject> Doc, bool bAcadem)
{
	if (!Doc)
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Academgorodok/fountain.json");
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
		{
			UE_LOG(LogTemp, Warning, TEXT("LyudFountain: no %s"), *Path);
			return false;
		}
	}
	if (bAcadem)
	{
		Tags.Add(TEXT("Academ"));
	}
	// a bigger fountain throws its water harder: speeds x FS (the shapes grow as FS^2), drops a little bigger and more of them
	const float FS = Doc->HasField(TEXT("flow_scale")) ? (float)Doc->GetNumberField(TEXT("flow_scale")) : 1.f;
	const float DS = FMath::Sqrt(FS);
	const int32 DN = FMath::RoundToInt(FS * FS);
	const TArray<TSharedPtr<FJsonValue>>* AtP = nullptr;
	if (!Doc->TryGetArrayField(TEXT("at"), AtP) || AtP->Num() < 3 || !Doc->HasTypedField<EJson::Object>(TEXT("stone")))
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudFountain: a layout without 'at' [x,y,z] or 'stone'"));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& At = *AtP;
	Foot = FVector(At[0]->AsNumber(), At[1]->AsNumber(), At[2]->AsNumber()) * 100.0;
	// moved once, then static like its parts (a static part can't hang from a movable root)
	RootComponent->SetMobility(EComponentMobility::Movable);
	SetActorLocation(Foot);
	RootComponent->SetMobility(EComponentMobility::Static);
	const TSharedPtr<FJsonObject> S = Doc->GetObjectField(TEXT("stone"));
	auto Num = [&S](const TCHAR* K) { return (float)S->GetNumberField(K); };
	const float LowZ = Num(TEXT("lower_level")), LowR = Num(TEXT("lower_r")), LowFloor = Num(TEXT("lower_floor"));
	const float UpZ = Num(TEXT("upper_level")), UpR = Num(TEXT("upper_r")), UpFloor = Num(TEXT("upper_floor"));
	const float LipR = Num(TEXT("lip_r")), LipZ = Num(TEXT("lip_z")), NozzleZ = Num(TEXT("nozzle_z"));
	const float StemR = Num(TEXT("stem_r")), NeckR = Num(TEXT("neck_r"));

	// the flows (m, m/s): the jet rises ~34 cm over the fleur, opens at its top into a bell falling into the upper
	// bowl clear of the fleur's petals; the bowl spills over its lip in a veil landing ~12 cm out in the lower basin
	const float JetV = 2.58f * FS, TopZ = NozzleZ + JetV * JetV / (2.f * G), BellVr = 0.6f * FS, VeilVr = 0.35f * FS, VeilVz = -0.04f * FS;
	const float BellLandR = 0.012f + BellVr * FallTime(TopZ, 0.f, UpZ);
	const float VeilT = FallTime(LipZ, VeilVz, LowZ);
	const float VeilLandR = LipR + VeilVr * VeilT;

	FPartMesh Lower, Upper, Flow, Spray, Dry;
	Disc(Lower, LowZ, StemR - 0.01f, LowR, FMath::Max(14, FMath::RoundToInt(LowR * 10.f)), FMath::Max(72, FMath::RoundToInt(LowR * 40.f)));
	Disc(Upper, UpZ, NeckR - 0.01f, UpR, 6, 40);
	Column(Flow, NozzleZ - 0.01f, JetV, 0.009f, 8, 10);
	Sheet(Flow, 0.012f, TopZ, BellVr, 0.f, UpZ - 0.005f, JetV / G, 48, 14, 22.f, 1.f);
	Sheet(Flow, LipR, LipZ, VeilVr, VeilVz, LowZ - 0.005f, 0.f, 96, 12, 56.f, 2.f);

	FRandomStream Rng(4113);
	// the veil's landing: a crown of splashes all round, and drops off its broken strands
	for (int32 I = 0; I < 220 * DN; I++)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Rd = Radial(A), Tg(-Rd.Y, Rd.X, 0.f);
		const FVector P = Rd * (VeilLandR + Rng.FRandRange(-0.03f, 0.03f)) + FVector(0.f, 0.f, LowZ);
		const float Vz = Rng.FRandRange(0.35f, 1.15f) * FS;
		const FVector V = (Rd * Rng.FRandRange(-0.25f, 0.45f) + Tg * Rng.FRandRange(-0.15f, 0.15f)) * FS + FVector(0.f, 0.f, Vz);
		const float Life = FallTime(LowZ, Vz, LowZ - 0.01f);
		Drop(Spray, P, V, Life, Life * Rng.FRandRange(1.2f, 2.2f), Rng.FRand(), Rng.FRandRange(0.0018f, 0.0035f) * DS);
	}
	for (int32 I = 0; I < 90 * DN; I++)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Rd = Radial(A);
		const float Z0 = LipZ - Rng.FRandRange(0.05f, 0.35f);
		const float T0 = FallTime(LipZ, VeilVz, Z0);
		const FVector P = Rd * (LipR + VeilVr * T0) + FVector(0.f, 0.f, Z0);
		const float Vz = VeilVz - G * T0;
		const FVector V = Rd * (VeilVr + Rng.FRandRange(0.f, 0.25f) * FS) + FVector(0.f, 0.f, Vz);
		const float Life = FallTime(Z0, Vz, LowZ);
		Drop(Spray, P, V, Life, Life * Rng.FRandRange(1.5f, 3.f), Rng.FRand(), Rng.FRandRange(0.0015f, 0.003f) * DS);
	}
	// the bell's landing in the upper bowl, and the spray off the jet's top
	for (int32 I = 0; I < 80 * DN; I++)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Rd = Radial(A);
		const FVector P = Rd * FMath::Min(BellLandR + Rng.FRandRange(-0.02f, 0.02f), UpR - 0.02f) + FVector(0.f, 0.f, UpZ);
		const float Vz = Rng.FRandRange(0.25f, 0.75f) * FS;
		const FVector V = Rd * Rng.FRandRange(-0.2f, 0.2f) * FS + FVector(0.f, 0.f, Vz);
		const float Life = FallTime(UpZ, Vz, UpZ - 0.01f);
		Drop(Spray, P, V, Life, Life * Rng.FRandRange(1.2f, 2.2f), Rng.FRand(), Rng.FRandRange(0.0015f, 0.003f) * DS);
	}
	for (int32 I = 0; I < 50 * DN; I++)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Rd = Radial(A);
		const float Vh = Rng.FRandRange(0.25f, 0.62f) * FS, Vz = Rng.FRandRange(-0.1f, 0.6f) * FS;
		const FVector P = FVector(0.f, 0.f, TopZ - 0.02f) + Rd * 0.015f;
		const float Life = FallTime(P.Z, Vz, UpZ);
		Drop(Spray, P, Rd * Vh + FVector(0.f, 0.f, Vz), Life, Life * Rng.FRandRange(1.3f, 2.5f), Rng.FRand(), Rng.FRandRange(0.0015f, 0.0028f) * DS);
	}
	// drained: the autumn's leaves on both floors
	Disc(Dry, LowFloor + 0.004f, StemR + 0.005f, FMath::Max(0.6f, LowR - 0.25f), 4, FMath::Max(48, FMath::RoundToInt(LowR * 40.f)));
	Disc(Dry, UpFloor + 0.004f, NeckR + 0.004f, FMath::Max(0.22f, UpR - 0.07f), 2, 24);

	const float Yaw = Doc->HasField(TEXT("yaw")) ? (float)Doc->GetNumberField(TEXT("yaw")) : 0.f;
	LowerWater = MakePart(Lower.Build(this, TEXT("FountainLowerWater")), TEXT("FountainLowerWater"), WaterPath, false, 15000.f,
		[&](UMaterialInstanceDynamic* M) { M->SetVectorParameterValue(TEXT("Ring"), FLinearColor(VeilLandR, 1.f, 1.f, LowR)); });
	UpperWater = MakePart(Upper.Build(this, TEXT("FountainUpperWater")), TEXT("FountainUpperWater"), WaterPath, false, 12000.f,
		[&](UMaterialInstanceDynamic* M) { M->SetVectorParameterValue(TEXT("Ring"), FLinearColor(BellLandR, 0.7f, 0.6f, UpR)); });
	Streams = MakePart(Flow.Build(this, TEXT("FountainStreams")), TEXT("FountainStreams"), StreamPath, false, 9000.f,
		[](UMaterialInstanceDynamic*) {});
	Drops = MakePart(Spray.Build(this, TEXT("FountainDrops")), TEXT("FountainDrops"), DropsPath, false, 5000.f,
		[](UMaterialInstanceDynamic*) {});
	// the drops leave their quads' points by up to ~40 cm in the shader
	Drops->SetBoundsScale(3.f);
	Leaves = MakePart(Dry.Build(this, TEXT("FountainLeaves")), TEXT("FountainLeaves"), LeavesPath, false, 6000.f,
		[Yaw](UMaterialInstanceDynamic* M) { M->SetScalarParameterValue(TEXT("Seed"), Yaw); });

	Sound = NewObject<ULyudFountainSynth>(this, TEXT("FountainSound"));
	Sound->bAutoActivate = false;
	Sound->bOverrideAttenuation = true;
	FSoundAttenuationSettings& A = Sound->AttenuationOverrides;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);    // full level within 1.5 m of the basin's middle
	A.FalloffDistance = HearCm - 150.f;
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	A.dBAttenuationAtMax = -50.f;
	A.NonSpatializedRadiusStart = 120.f;                    // standing at the rim it is all round you
	A.NonSpatializedRadiusEnd = 60.f;
	A.bAttenuateWithLPF = true;                             // the highs go first with distance
	A.LPFRadiusMin = 800.f;
	A.LPFRadiusMax = 4000.f;
	A.LPFFrequencyAtMin = 20000.f;
	A.LPFFrequencyAtMax = 3500.f;
	// no occlusion: the source sits in the basin's middle, inside the stone's collision (the centrepiece), so every trace
	// started there was blocked and the fountain always sounded muffled at -9 dB; a house between you and it is rare here
	A.bEnableOcclusion = false;
	A.OcclusionTraceChannel = ECC_Visibility;
	A.OcclusionLowPassFilterFrequency = 1200.f;
	A.OcclusionVolumeAttenuation = 0.35f;
	A.OcclusionInterpolationTime = 0.4f;
	Sound->SetupAttachment(RootComponent);
	Sound->SetRelativeLocation(FVector(0.f, 0.f, LowZ * 100.f + 20.f));
	Sound->RegisterComponent();
	AddInstanceComponent(Sound);

	UE_LOG(LogTemp, Display, TEXT("LyudFountain: at %s; jet top %.2f m, bell lands at r %.2f, veil at r %.2f (%.2f s); %d + %d + %d + %d triangles, %d drops"),
		*Foot.ToCompactString(), TopZ, BellLandR, VeilLandR, VeilT, Lower.Idx.Num() / 3, Upper.Idx.Num() / 3, Flow.Idx.Num() / 3, Dry.Idx.Num() / 3,
		Spray.Idx.Num() / 6);
	return true;
}

void ALyudFountain::BeginPlay()
{
	Super::BeginPlay();
}

namespace
{
	/** The autumn's first-frost search, shared by every fountain (the weather chain is the world's): per year, scanned
	 *  up to ScannedTo (hours since 2000), found at FrostHours (-1: not yet / none by 1 December). */
	struct FFrostScan { double ScannedTo = 0.0, FrostHours = -1.0; bool bDone = false; };
	TMap<int32, FFrostScan> GFrostScans;
	constexpr int32 FrostHoursPerTick = 120;    // ~2200 hours from 1 September to 1 December: ~18 ticks, never one long frame
}

bool ALyudFountain::ScanFrost(int32 Year, int32 Budget)
{
	FFrostScan* S = GFrostScans.Find(Year);
	if (!S)
	{
		S = &GFrostScans.Add(Year);
		S->ScannedTo = HoursOf(FDateTime(Year, 9, 1));
	}
	if (S->bDone)
	{
		return true;
	}
	// the weather engine's air at 2 m, hour by hour from 1 September (no forced values: the season is the clock's)
	if (ULyudWeatherSubsystem* W = ULyudWeatherSubsystem::Get(this))
	{
		W->GetModelState();     // pushes the climate into the shared chain
	}
	LyudWeather::FChain& Chain = LyudWeather::SharedChain();
	const double End = HoursOf(FDateTime(Year, 12, 1));
	for (int32 N = 0; S->ScannedTo < End && (Budget <= 0 || N < Budget); S->ScannedTo += 1.0, N++)
	{
		if (LyudWeather::Instant(S->ScannedTo, Chain).TempC < FrostC)
		{
			S->FrostHours = S->ScannedTo;
			S->bDone = true;
			return true;
		}
	}
	S->bDone = S->ScannedTo >= End;
	return S->bDone;
}

FDateTime ALyudFountain::FirstFrost(int32 Year)
{
	ScanFrost(Year, 0);
	const FFrostScan& S = GFrostScans.FindChecked(Year);
	return S.FrostHours >= 0.0 ? FDateTime(2000, 1, 1) + FTimespan::FromHours(S.FrostHours) : FDateTime();
}

bool ALyudFountain::InSeason(const FDateTime& When, bool& bKnown)
{
	bKnown = true;
	const int32 Y = When.GetYear();
	if (When < FDateTime(Y, 5, 1) || When >= FDateTime(Y, 12, 1))
	{
		return false;
	}
	if (When < FDateTime(Y, 9, 1))
	{
		return true;
	}
	// autumn: running until the first frost; the search goes on a slice per tick, so an answer may wait a few ticks
	ScanFrost(Y, FrostHoursPerTick);
	const FFrostScan& S = GFrostScans.FindChecked(Y);
	const double H = HoursOf(When);
	if (S.FrostHours >= 0.0)
	{
		return H < S.FrostHours;
	}
	if (S.bDone || S.ScannedTo > H)
	{
		return true;                 // no frost up to now
	}
	bKnown = false;
	return bRunning;
}

void ALyudFountain::SetRunning(bool bOn)
{
	if (bStateKnown && bOn == bRunning)
	{
		return;
	}
	bStateKnown = true;
	bRunning = bOn;
	for (UStaticMeshComponent* C : { LowerWater.Get(), UpperWater.Get(), Streams.Get(), Drops.Get() })
	{
		if (C)
		{
			C->SetVisibility(bOn);
		}
	}
	if (Leaves)
	{
		Leaves->SetVisibility(!bOn);
	}
	UE_LOG(LogTemp, Display, TEXT("LyudFountain: %s (%s)"), bOn ? TEXT("running") : TEXT("drained"), *DebugReport());
}

void ALyudFountain::UpdateSound(float DeltaSeconds)
{
	if (!Sound)
	{
		return;
	}
	static IConsoleVariable* SoundOn = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Sound"));
	float Dist = 1e9f;
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Dist = (float)FVector::Dist(Cam->GetCameraLocation(), Sound->GetComponentLocation());
	}
	const bool bWant = bRunning && !IsHidden() && Dist < HearCm && (!SoundOn || SoundOn->GetInt() > 0);
	bSoundWanted = bWant;
	float Gain = 0.5f;
	if (const ULyudSoundscapeSubsystem* Scape = ULyudSoundscapeSubsystem::Get(this))
	{
		Gain *= Scape->MasterVolume * Scape->AmbienceVolume;
	}
	if (bWant && !bSoundPlaying)
	{
		Sound->Start();
		bSoundPlaying = true;
		SoundFadeLeft = 0.f;
	}
	else if (!bWant && bSoundPlaying)
	{
		// walked out of earshot: stop at once; drained, hidden or the sound switched off: let the flow fade (~1.5 s), then stop
		if (Dist > HearCm + 500.f)
		{
			SoundFadeLeft = 0.f;
		}
		else if (SoundFadeLeft <= 0.f)
		{
			SoundFadeLeft = 1.5f;
		}
		SoundFadeLeft -= DeltaSeconds;
		if (SoundFadeLeft <= 0.f)
		{
			Sound->Stop();
			bSoundPlaying = false;
			SoundFadeLeft = 0.f;
		}
	}
	else if (bWant)
	{
		SoundFadeLeft = 0.f;
	}
	Sound->SetFlow(bWant ? 1.f : 0.f, Gain);
}

FString ALyudFountain::DebugReport()
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const FFrostScan* Scan = GFrostScans.Find(Now.GetYear());
	const FDateTime Frost = Scan && Scan->FrostHours >= 0.0 ? FDateTime(2000, 1, 1) + FTimespan::FromHours(Scan->FrostHours) : FDateTime();
	bool bKnownNow = true;
	return FString::Printf(TEXT("%s, %s; now %s, first frost %s, force %d, sound %s rms %.4f"), bRunning ? TEXT("running") : TEXT("drained"),
		InSeason(Now, bKnownNow) ? TEXT("in season") : TEXT("out of season"), *Now.ToString(TEXT("%Y-%m-%d %H:%M")),
		Frost == FDateTime() ? TEXT("none by December") : *Frost.ToString(TEXT("%Y-%m-%d %H:%M")), CVarFountainForce.GetValueOnGameThread(),
		bSoundPlaying ? TEXT("on") : TEXT("off"), Sound ? Sound->GetRms() : 0.f);
}

float ALyudFountain::GetSoundLevel() const
{
	return Sound && bSoundPlaying && bSoundWanted ? Sound->GetRms() : 0.f;     // a fading voice reports silence
}

void ALyudFountain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f || !bStateKnown)
	{
		CheckTimer = 2.f;
		const int32 Force = CVarFountainForce.GetValueOnGameThread();
		bool bKnown = true;
		const bool bOn = Force >= 0 ? Force > 0 : InSeason(UGameCalendar::NowIn(GetWorld()), bKnown);
		if (bKnown)
		{
			SetRunning(bOn);
		}
		else
		{
			CheckTimer = 0.f;        // the frost search isn't up to today yet: another slice next tick
		}
		if (CVarFountainDebug.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("LyudFountain: %s"), *DebugReport());
		}
	}
	UpdateSound(DeltaSeconds);
}

bool ULyudFountainSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULyudFountainSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarFountain.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	if (ALyudFountain* F = InWorld.SpawnActor<ALyudFountain>(SP))
	{
		if (F->Build())
		{
			Fountain = F;
			All.Add(F);
		}
		else
		{
			F->Destroy();
		}
	}
}

void ULyudFountainSubsystem::Register(ALyudFountain* F)
{
	if (F)
	{
		All.AddUnique(F);
	}
}
