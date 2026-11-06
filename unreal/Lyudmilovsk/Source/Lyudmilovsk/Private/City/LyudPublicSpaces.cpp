#include "City/LyudPublicSpaces.h"

#include "CityKit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sky/LyudSky.h"
#include "World/LyudFountain.h"

static TAutoConsoleVariable<int32> CVarPublicSpaces(TEXT("lyud.City.PublicSpaces"), 1,
	TEXT("The town's public spaces (ALyudPublicSpaces: promenade, square, park, fair, beach furniture): 1 built when the outdoor map starts, 0 not."));
static TAutoConsoleVariable<int32> CVarCityStreetLights(TEXT("lyud.City.StreetLights"), 14,
	TEXT("How many real (shadowless point) lights follow the camera from street lamp to street lamp at night; 0 = the lamps only glow."));
static TAutoConsoleVariable<int32> CVarCityNight(TEXT("lyud.City.Night"), -1,
	TEXT("Street lamps: -1 lit by the sun (under ~1 degree), 0 off, 1 on."));
static TAutoConsoleVariable<int32> CVarCitySummer(TEXT("lyud.City.Summer"), -1,
	TEXT("Beach umbrellas and loungers: -1 out from June to August, 0 never, 1 always."));
static TAutoConsoleVariable<int32> CVarCityShow(TEXT("lyud.City.Show"), 1,
	TEXT("1 the public spaces drawn, 0 all hidden (props, paving, their trees and flowers, the lamps' lights): for A/B frame times in one process."));
static TAutoConsoleVariable<int32> CVarCityIce(TEXT("lyud.City.Ice"), -1,
	TEXT("The park pond: -1 frozen from mid December to mid March, 0 water, 1 ice."));

namespace
{
	const TCHAR* PropMatPath = TEXT("/Game/City/M_CityProp.M_CityProp");
	const TCHAR* WaterMatPath = TEXT("/Game/Academgorodok/Fountain/M_FountainWater.M_FountainWater");
	const TCHAR* StreamMatPath = TEXT("/Game/Academgorodok/Fountain/M_FountainStream.M_FountainStream");
	const TCHAR* LayoutFile = TEXT("City/public_spaces.json");

	/** A vertex of a flowing sheet for M_FountainStream: UV0 = (across in strands, the water's flight time s),
	 *  UV1 = (0..1 along a fall, where it tears; kind 2 = a sheet). */
	void SheetVert(CityKit::FMesh& M, const FVector& P, const FVector& N, float Across, float Time, float Tear)
	{
		CityKit::FSkin S;
		M.Vert(P, N, S, FVector2f(Across * 14.f, Time));
		M.UV1.Last() = FVector2f(Tear, 2.f);
	}
	/** A strip of sheet between two cross-sections (each: left and right point, time, tear). */
	void SheetStrip(CityKit::FMesh& M, const FVector& L0, const FVector& R0, float T0, float A0, const FVector& L1, const FVector& R1,
		float T1, float A1, const FVector& N, float W)
	{
		const int32 B = M.Pos.Num();
		SheetVert(M, L0, N, 0.f, T0, A0);
		SheetVert(M, R0, N, W, T0, A0);
		SheetVert(M, R1, N, W, T1, A1);
		SheetVert(M, L1, N, 0.f, T1, A1);
		M.Tri(B, B + 1, B + 2, N);
		M.Tri(B, B + 2, B + 3, N);
	}

	FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A, double Scale = 100.0)
	{
		return A.Num() >= 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) * Scale : FVector::ZeroVector;
	}
	FVector VecField(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Scale = 100.0)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O->TryGetArrayField(K, A) ? Vec(*A, Scale) : FVector::ZeroVector;
	}
	TSharedPtr<FJsonObject> LoadLayout()
	{
		FString Text;
		TSharedPtr<FJsonObject> Doc;
		const FString Path = FPaths::ProjectContentDir() / LayoutFile;
		if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Doc) || !Doc)
		{
			UE_LOG(LogTemp, Warning, TEXT("LyudPublicSpaces: no %s (python3 tools/public_spaces.py)"), *Path);
			return nullptr;
		}
		return Doc;
	}
}

// --- the spaces -------------------------------------------------------------------------------------------------

ALyudPublicSpaces::ALyudPublicSpaces()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	Tags.Add(TEXT("NoTree"));
	Tags.Add(TEXT("NoGrass"));
	Tags.Add(TEXT("City"));
}

UInstancedStaticMeshComponent* ALyudPublicSpaces::MakeIsm(UStaticMesh* Mesh, const FString& Name, bool bCollide, UMaterialInterface* Mat, float CullCm)
{
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, *Name);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	if (Mat) C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCanEverAffectNavigation(false);
	C->SetCachedMaxDrawDistance(CullCm);
	C->InstanceEndCullDistance = (int32)CullCm;
	C->LightingChannels.bChannel1 = true;
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Comps.Add(C);
	return C;
}

UStaticMeshComponent* ALyudPublicSpaces::MakePart(UStaticMesh* Mesh, const FString& Name, bool bCollide, UMaterialInterface* Mat, float CullCm, const FTransform& Xf)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *Name);
	C->SetRelativeTransform(Xf);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	if (Mat) C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	C->SetCanEverAffectNavigation(false);
	C->SetCachedMaxDrawDistance(CullCm);
	C->LightingChannels.bChannel1 = true;
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Parts.Add(C);
	return C;
}

bool ALyudPublicSpaces::Build(const TSharedPtr<FJsonObject>& Doc)
{
	if (!Doc) return false;
	const double T0 = FPlatformTime::Seconds();
	PropMaterial = LoadObject<UMaterialInterface>(nullptr, PropMatPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!PropMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("LyudPublicSpaces: no %s (unreal/scripts/make_city_materials.py)"), PropMatPath);
	}
	GlowMid = PropMaterial ? UMaterialInstanceDynamic::Create(PropMaterial, this) : nullptr;
	if (GlowMid) GlowMid->SetScalarParameterValue(TEXT("Glow"), 0.f);
	int32 NumBlocks = 0, NumProps = 0, Tris = 0;

	// the blocks: paving, steps, the stair. Drawn as merged meshes per skin and 100 m cell (true sizes, so the slab joints
	// meet the edges); their collision is a hidden instanced unit cube per block
	{
		struct FCell { CityKit::FMesh M; FVector Origin; };
		TMap<FString, FCell> Cells;
		UInstancedStaticMeshComponent* Coll = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Boxes = nullptr;
		if (Doc->TryGetArrayField(TEXT("boxes"), Boxes))
		{
			CityKit::FMesh Cube;
			CityKit::BuildBlock(Cube, TEXT("concrete"));
			UStaticMesh* CubeMesh = Cube.Build(this, TEXT("SM_CityBlockCollision"), true);
			Meshes.Add(CubeMesh);
			Coll = MakeIsm(CubeMesh, TEXT("BlocksCollision"), true, PropMaterial, 1000.f);
			Coll->SetVisibility(false);
			Coll->SetHiddenInGame(true);
			Coll->SetCastShadow(false);
			Coll->bAffectDistanceFieldLighting = false;
			Coll->bAffectDynamicIndirectLighting = false;
			for (const TSharedPtr<FJsonValue>& V : *Boxes)
			{
				const TSharedPtr<FJsonObject> B = V->AsObject();
				const FString Skin = B->GetStringField(TEXT("skin"));
				const FVector C = VecField(B, TEXT("c"), 1.0), S = VecField(B, TEXT("size"), 1.0);
				const float Yaw = B->GetNumberField(TEXT("yaw"));
				const FIntPoint Cell(FMath::FloorToInt(C.X / 100.0), FMath::FloorToInt(C.Y / 100.0));
				FCell& Ce = Cells.FindOrAdd(FString::Printf(TEXT("%s_%d_%d"), *Skin, Cell.X, Cell.Y));
				if (Ce.M.Pos.Num() == 0) Ce.Origin = FVector(Cell.X * 100.0 + 50.0, Cell.Y * 100.0 + 50.0, C.Z);
				CityKit::AddBlock(Ce.M, C - Ce.Origin, Yaw, S, Skin, C.Z);
				Coll->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), C * 100.0, S), true);
				NumBlocks++;
			}
		}
		for (TPair<FString, FCell>& It : Cells)
		{
			UStaticMesh* Mesh = It.Value.M.Build(this, *(TEXT("SM_CityBlocks_") + It.Key.Replace(TEXT("-"), TEXT("m"))), false);
			Meshes.Add(Mesh);
			Tris += It.Value.M.Idx.Num() / 3;
			UStaticMeshComponent* C = MakePart(Mesh, TEXT("Blocks_") + It.Key.Replace(TEXT("-"), TEXT("m")), false, PropMaterial, 80000.f,
				FTransform(It.Value.Origin * 100.0));
			C->bAffectDistanceFieldLighting = false;
			C->SetCastShadow(It.Key.StartsWith(TEXT("granite")) || It.Key.StartsWith(TEXT("boards")));
		}
	}

	// the props: one mesh per kind (and variant / size), instanced; summer-only ones in their own sets
	{
		TMap<FString, int32> GroupOf;
		const TArray<TSharedPtr<FJsonValue>>* Props = nullptr;
		if (Doc->TryGetArrayField(TEXT("props"), Props))
		{
			for (const TSharedPtr<FJsonValue>& V : *Props)
			{
				const TSharedPtr<FJsonObject> P = V->AsObject();
				const FString Kind = P->GetStringField(TEXT("kind"));
				const int32 Variant = P->HasField(TEXT("variant")) ? (int32)P->GetNumberField(TEXT("variant")) : 0;
				const bool bSum = P->HasField(TEXT("season")) && P->GetStringField(TEXT("season")) == TEXT("summer");
				FString Key = FString::Printf(TEXT("%s_%d%s"), *Kind, Variant, bSum ? TEXT("_summer") : TEXT(""));
				if (Kind == TEXT("flowerbed") || Kind == TEXT("pond"))
				{
					FString R;
					const TArray<TSharedPtr<FJsonValue>>* RA = nullptr;
					if (P->TryGetArrayField(TEXT("r"), RA)) for (const auto& X : *RA) R += FString::Printf(TEXT("_%.2f"), X->AsNumber());
					else if (P->HasField(TEXT("r"))) R = FString::Printf(TEXT("_%.2f"), P->GetNumberField(TEXT("r")));
					Key += R;
				}
				int32* Gi = GroupOf.Find(Key);
				if (!Gi)
				{
					CityKit::FMesh M;
					CityKit::BuildProp(M, Kind, Variant, P);
					if (M.Idx.Num() == 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("LyudPublicSpaces: no kit mesh for '%s'"), *Kind);
						GroupOf.Add(Key, INDEX_NONE);
						continue;
					}
					const bool bCollide = CityKit::PropCollides(Kind);
					UStaticMesh* Mesh = M.Build(this, *(TEXT("SM_City_") + Key.Replace(TEXT("."), TEXT("_"))), bCollide);
					Meshes.Add(Mesh);
					Tris += M.Idx.Num() / 3;
					FGroup G;
					G.Kind = Kind;
					G.bSummer = bSum;
					G.bGlow = CityKit::PropGlows(Kind);
					const bool bBig = Kind == TEXT("stage") || Kind == TEXT("carousel") || Kind == TEXT("pond") || Kind.StartsWith(TEXT("lamp"));
					G.Comp = MakeIsm(Mesh, TEXT("Props_") + Key.Replace(TEXT("."), TEXT("_")), bCollide,
						G.bGlow && GlowMid ? static_cast<UMaterialInterface*>(GlowMid.Get()) : PropMaterial.Get(), bBig ? 40000.f : 15000.f);
					// small things cast no shadow beyond the near ones (VSM pages), and no distance fields
					G.Comp->bAffectDistanceFieldLighting = false;
					G.Comp->bAffectDynamicIndirectLighting = bBig;
					Gi = &GroupOf.Add(Key, Groups.Add(G));
				}
				if (*Gi == INDEX_NONE) continue;
				Groups[*Gi].Comp->AddInstance(FTransform(FRotator(0.f, P->GetNumberField(TEXT("yaw")), 0.f), VecField(P, TEXT("at"))), true);
				NumProps++;
			}
		}
	}

	// fountain stones built from their profiles (the water is ALyudFountain's, from the same numbers)
	{
		const TArray<TSharedPtr<FJsonValue>>* Fs = nullptr;
		if (Doc->TryGetArrayField(TEXT("fountains"), Fs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Fs)
			{
				const TSharedPtr<FJsonObject> F = V->AsObject();
				if (!F->HasField(TEXT("build_stone")) || !F->GetBoolField(TEXT("build_stone"))) continue;
				CityKit::FMesh M;
				CityKit::BuildFountainStone(M, F->GetObjectField(TEXT("stone")));
				UStaticMesh* Mesh = M.Build(this, *(TEXT("SM_FountainStone_") + F->GetStringField(TEXT("id"))), true);
				Meshes.Add(Mesh);
				Tris += M.Idx.Num() / 3;
				MakePart(Mesh, TEXT("FountainStone_") + F->GetStringField(TEXT("id")), true, PropMaterial, 60000.f,
					FTransform(FRotator(0.f, F->GetNumberField(TEXT("yaw")), 0.f), VecField(F, TEXT("at"))));
			}
		}
	}

	// still water: the pond (the fountains' water material, no landing ring) and its winter ice
	{
		const TArray<TSharedPtr<FJsonValue>>* Ws = nullptr;
		UMaterialInterface* WaterMat = LoadObject<UMaterialInterface>(nullptr, WaterMatPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Doc->TryGetArrayField(TEXT("waters"), Ws))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ws)
			{
				const TSharedPtr<FJsonObject> W = V->AsObject();
				const TArray<TSharedPtr<FJsonValue>>* RectF = nullptr;
				if (W->TryGetArrayField(TEXT("rect"), RectF))
				{
					// a rectangular basin of the Cascade: ripples from where the channel pours in
					const float Ln = (*RectF)[0]->AsNumber(), Wd = (*RectF)[1]->AsNumber();
					const FVector At = VecField(W, TEXT("at")), Ring = VecField(W, TEXT("ring"));
					const FRotator Rot(0.f, W->GetNumberField(TEXT("yaw")), 0.f);
					CityKit::FMesh Bm;
					CityKit::FSkin S;
					const FVector Rl = Rot.UnrotateVector(Ring - At) / 100.0;           // the pour point in the basin's frame, m
					const int32 NX = 8, NY = 4;
					for (int32 I = 0; I < NX; I++) for (int32 J = 0; J < NY; J++)
					{
						auto Pt = [&](int32 A, int32 B) { return FVector(-Ln * 0.5f + Ln * A / NX, -Wd * 0.5f + Wd * B / NY, 0.f); };
						auto Uv = [&](const FVector& Q) { return FVector2f(Q.X - Rl.X, Q.Y - Rl.Y); };
						const FVector Q0 = Pt(I, J), Q1 = Pt(I + 1, J), Q2 = Pt(I + 1, J + 1), Q3 = Pt(I, J + 1);
						Bm.QuadUV(Q0, Q1, Q2, Q3, Uv(Q0), Uv(Q1), Uv(Q2), Uv(Q3), S);
					}
					UStaticMesh* BMesh = Bm.Build(this, *(TEXT("SM_Basin_") + W->GetStringField(TEXT("id"))), false);
					Meshes.Add(BMesh);
					UMaterialInstanceDynamic* Mid = WaterMat ? UMaterialInstanceDynamic::Create(WaterMat, this) : nullptr;
					const bool bSpring = W->GetStringField(TEXT("kind")) == TEXT("spring");
					if (Mid) Mid->SetVectorParameterValue(TEXT("Ring"), bSpring ? FLinearColor(0.15f, 0.5f, 0.6f, 3.f) : FLinearColor(0.25f, 0.9f, 0.9f, 4.f));
					UStaticMeshComponent* C = MakePart(BMesh, TEXT("Basin_") + W->GetStringField(TEXT("id")), false, Mid, 30000.f, FTransform(Rot, At));
					C->SetCastShadow(false);
					C->bAffectDistanceFieldLighting = false;
					C->bAffectDynamicIndirectLighting = false;
					Cascade.Add(C);
					continue;
				}
				const TArray<TSharedPtr<FJsonValue>>& R = W->GetArrayField(TEXT("r"));
				const float Rx = R[0]->AsNumber(), Ry = R[1]->AsNumber();
				const FVector At = VecField(W, TEXT("at"));
				const FRotator Rot(0.f, W->GetNumberField(TEXT("yaw")), 0.f);
				CityKit::FMesh Wm;
				CityKit::FSkin S;
				for (int32 K = 0; K < 6; K++) Wm.Disc(FVector::ZeroVector, Rx * K / 6.f, Rx * (K + 1) / 6.f, 64, S, Ry * (K + 1) / 6.f);
				UStaticMesh* Mesh = Wm.Build(this, *(TEXT("SM_PondWater_") + W->GetStringField(TEXT("id"))), false);
				Meshes.Add(Mesh);
				UMaterialInstanceDynamic* Mid = WaterMat ? UMaterialInstanceDynamic::Create(WaterMat, this) : nullptr;
				if (Mid) Mid->SetVectorParameterValue(TEXT("Ring"), FLinearColor(0.f, 0.f, 0.f, FMath::Max(Rx, Ry) + 1.f));
				UStaticMeshComponent* C = MakePart(Mesh, TEXT("PondWater_") + W->GetStringField(TEXT("id")), false, Mid, 40000.f, FTransform(Rot, At));
				C->SetCastShadow(false);
				C->bAffectDistanceFieldLighting = false;
				C->bAffectDynamicIndirectLighting = false;
				PondWater.Add(C);
				CityKit::FMesh Im;
				CityKit::FSkin Ice;
				Ice.Color = CityKit::Srgb(196, 212, 222);
				Ice.Rough = 0.12f;
				Im.Disc(FVector(0, 0, 0.01f), 0.f, Rx, 64, Ice, Ry);
				UStaticMesh* IMesh = Im.Build(this, *(TEXT("SM_PondIce_") + W->GetStringField(TEXT("id"))), true);
				Meshes.Add(IMesh);
				UStaticMeshComponent* I = MakePart(IMesh, TEXT("PondIce_") + W->GetStringField(TEXT("id")), true, PropMaterial, 40000.f, FTransform(Rot, At));
				I->SetCastShadow(false);
				PondIce.Add(I);
			}
		}
	}

	// the Cascade: water sheets down the stepped channels and along the rills (one mesh, the stream material), the
	// basins' still water above (with the pond); runs with the town's fountain
	{
		CityKit::FMesh Sheet;
		const TArray<TSharedPtr<FJsonValue>>* Cs = nullptr;
		if (Doc->TryGetArrayField(TEXT("cascades"), Cs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Cs)
			{
				const TSharedPtr<FJsonObject> C = V->AsObject();
				const FVector Foot = VecField(C, TEXT("foot"), 1.0);
				const TArray<TSharedPtr<FJsonValue>>& Dn = C->GetArrayField(TEXT("down"));
				const TArray<TSharedPtr<FJsonValue>>& Ac = C->GetArrayField(TEXT("across"));
				const FVector Down(Dn[0]->AsNumber(), Dn[1]->AsNumber(), 0.0), Across(Ac[0]->AsNumber(), Ac[1]->AsNumber(), 0.0);
				const float W = C->GetNumberField(TEXT("width")) - 0.06f, Rise = C->GetNumberField(TEXT("rise")), Tread = C->GetNumberField(TEXT("tread"));
				const float Bed = C->GetNumberField(TEXT("bed_below"));
				const int32 N = (int32)C->GetNumberField(TEXT("steps"));
				const float Film = 0.018f, Vh = 0.7f;                 // a film 2 cm deep sliding at 0.7 m/s
				const float FallT = FMath::Sqrt(2.f * Rise / 9.81f);
				float T = 0.f;
				// from the head down: step k's tread spans (k+1)*tread .. k*tread from the foot, its bed at foot + (k+1)*rise - bed
				for (int32 K = N - 1; K >= 0; K--)
				{
					const float Z = (K + 1) * Rise - Bed + Film;
					const FVector A = Foot - Down * ((K + 1) * Tread) + FVector(0, 0, Z), B = Foot - Down * (K * Tread) + FVector(0, 0, Z);
					const float T1 = T + Tread / Vh;
					SheetStrip(Sheet, A - Across * W * 0.5f, A + Across * W * 0.5f, T, 0.f, B - Across * W * 0.5f, B + Across * W * 0.5f, T1, 0.02f,
						FVector::UpVector, W);
					T = T1;
					// over the lip: a short ballistic curve down to the next tread (or the basin), a few segments
					const float Zn = K > 0 ? K * Rise - Bed + Film : 0.27f;
					const float H = Z - Zn;
					const float Tf = FMath::Sqrt(2.f * FMath::Max(H, 0.02f) / 9.81f);
					FVector PrevL = B - Across * W * 0.5f, PrevR = B + Across * W * 0.5f;
					float PrevT = T;
					for (int32 J = 1; J <= 4; J++)
					{
						const float Tj = Tf * J / 4.f;
						const FVector P = B + Down * (Vh * Tj) - FVector(0, 0, 4.905f * Tj * Tj);
						const FVector Nn = (Down * (9.81f * Tj) + FVector(0, 0, Vh)).GetSafeNormal();
						SheetStrip(Sheet, PrevL, PrevR, PrevT, (J - 1) / 4.f * 0.6f, P - Across * W * 0.5f, P + Across * W * 0.5f, T + Tj,
							J / 4.f * 0.6f, Nn, W);
						PrevL = P - Across * W * 0.5f;
						PrevR = P + Across * W * 0.5f;
						PrevT = T + Tj;
					}
					T += Tf;
				}
				(void)FallT;
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Rs = nullptr;
		if (Doc->TryGetArrayField(TEXT("rills"), Rs))
		{
			for (const TSharedPtr<FJsonValue>& V : *Rs)
			{
				const TSharedPtr<FJsonObject> R = V->AsObject();
				const TArray<TSharedPtr<FJsonValue>>& Pts = R->GetArrayField(TEXT("points"));
				const float W = R->GetNumberField(TEXT("width")) - 0.05f;
				float T = 0.f;
				for (int32 I = 0; I + 1 < Pts.Num(); I++)
				{
					const FVector A = Vec(Pts[I]->AsArray(), 1.0), B = Vec(Pts[I + 1]->AsArray(), 1.0);
					const FVector D = (B - A).GetSafeNormal2D(), S(-D.Y, D.X, 0.0);
					const int32 Segs = FMath::Max(1, FMath::CeilToInt((B - A).Size2D() / 4.0));
					for (int32 J = 0; J < Segs; J++)
					{
						const FVector P0 = FMath::Lerp(A, B, double(J) / Segs), P1 = FMath::Lerp(A, B, double(J + 1) / Segs);
						const float T1 = T + (P1 - P0).Size() / 0.45f;            // a gentle rill, ~0.45 m/s
						SheetStrip(Sheet, P0 - S * W * 0.5f, P0 + S * W * 0.5f, T, 0.f, P1 - S * W * 0.5f, P1 + S * W * 0.5f, T1, 0.f, FVector::UpVector, W);
						T = T1;
					}
				}
			}
		}
		if (Sheet.Idx.Num() > 0)
		{
			UMaterialInterface* StreamMat = LoadObject<UMaterialInterface>(nullptr, StreamMatPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			UStaticMesh* Mesh = Sheet.Build(this, TEXT("SM_CascadeWater"), false);
			Meshes.Add(Mesh);
			Tris += Sheet.Idx.Num() / 3;
			UStaticMeshComponent* C = MakePart(Mesh, TEXT("CascadeWater"), false, StreamMat, 30000.f, FTransform::Identity);
			C->SetCastShadow(false);
			C->bAffectDistanceFieldLighting = false;
			C->bAffectDynamicIndirectLighting = false;
			Cascade.Add(C);
		}
	}

	// lamp heads for the light pool
	{
		const TArray<TSharedPtr<FJsonValue>>* Ls = nullptr;
		if (Doc->TryGetArrayField(TEXT("lights"), Ls))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ls)
			{
				LampHeads.Add(VecField(V->AsObject(), TEXT("at")));
				LampKinds.Add(FName(*V->AsObject()->GetStringField(TEXT("kind"))));
			}
		}
	}
	// the pool is made once at its largest; lyud.City.StreetLights says how many of it are used (changeable in play)
	const int32 PoolN = FMath::Clamp(FMath::Max(CVarCityStreetLights.GetValueOnGameThread(), 24), 0, 64);
	for (int32 I = 0; I < PoolN; I++)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("LampLight%d"), I));
		L->SetMobility(EComponentMobility::Movable);
		L->SetCastShadows(false);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(1800.f);
		L->SetAttenuationRadius(1500.f);
		L->SetSourceRadius(20.f);
		L->SetLightColor(FLinearColor(1.f, 0.78f, 0.52f));
		L->SetVisibility(false);
		L->bAffectsWorld = true;
		L->SetupAttachment(RootComponent);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Pool.Add(L);
	}

	// trees: one forest of hand-placed trees (shared bakes, live plants only up close)
	{
		const TArray<TSharedPtr<FJsonValue>>* Ts = nullptr;
		if (Doc->TryGetArrayField(TEXT("trees"), Ts) && Ts->Num() > 0)
		{
			FActorSpawnParameters SP;
			SP.ObjectFlags |= RF_Transient;
			SP.bDeferConstruction = true;
			if (AProceduralForest* F = GetWorld()->SpawnActor<AProceduralForest>(AProceduralForest::StaticClass(), FTransform(), SP))
			{
				F->bSnapToGround = false;
				F->Mix.Reset();
				F->Variants = 3;
				for (const TSharedPtr<FJsonValue>& V : *Ts)
				{
					const TSharedPtr<FJsonObject> T = V->AsObject();
					F->AddTree(FName(*T->GetStringField(TEXT("species"))), VecField(T, TEXT("at")), T->GetNumberField(TEXT("age")),
						(int32)T->GetNumberField(TEXT("variant")), T->GetNumberField(TEXT("yaw")), 1.f);
				}
				F->Tags.Add(TEXT("City"));
				F->FinishSpawning(FTransform());
				Forest = F;
			}
		}
	}
	// flowers, shrubs and hedges: single procedural plants
	{
		const TArray<TSharedPtr<FJsonValue>>* Ps = nullptr;
		if (Doc->TryGetArrayField(TEXT("plants"), Ps))
		{
			for (const TSharedPtr<FJsonValue>& V : *Ps)
			{
				const TSharedPtr<FJsonObject> P = V->AsObject();
				const FTransform Xf(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), VecField(P, TEXT("at")));
				if (AProceduralPlant* Pl = GetWorld()->SpawnActorDeferred<AProceduralPlant>(AProceduralPlant::StaticClass(), Xf, nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					Pl->Species = FName(*P->GetStringField(TEXT("species")));
					Pl->Seed = (int32)P->GetNumberField(TEXT("seed"));
					Pl->AgeAtStart = P->GetNumberField(TEXT("age"));
					Pl->bDemoGrowth = false;
					Pl->bSnapToGround = false;
					Pl->Pot = EPlantPot::None;
					Pl->SummaryDistance = 3500.f;
					Pl->Tags.Add(TEXT("City"));
					Pl->FinishSpawning(Xf);
					Plants.Add(Pl);
				}
			}
		}
	}

	UpdateSeason(true);
	UpdateNight(true);
	UE_LOG(LogTemp, Display, TEXT("LyudPublicSpaces: %d blocks, %d props in %d sets (%d kit triangles), %d lamps, %d trees, %d plants, built in %.0f ms"),
		NumBlocks, NumProps, Groups.Num(), Tris, LampHeads.Num(), Forest.IsValid() ? Forest->NumTrees() : 0, Plants.Num(),
		(FPlatformTime::Seconds() - T0) * 1000.0);
	return true;
}

void ALyudPublicSpaces::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AProceduralForest* F = Forest.Get()) F->Destroy();
	for (const TWeakObjectPtr<AProceduralPlant>& P : Plants) if (P.IsValid()) P->Destroy();
	Plants.Reset();
	Super::EndPlay(Reason);
}

void ALyudPublicSpaces::UpdateNight(bool bForce)
{
	bool bOn = bNight;
	const int32 Force = CVarCityNight.GetValueOnGameThread();
	if (Force >= 0)
	{
		bOn = Force > 0;
	}
	else
	{
		TActorIterator<ALyudSky> Sky(GetWorld());
		if (Sky)
		{
			const float Alt = Sky->SunAltitude();
			bOn = bNightKnown && bNight ? Alt < 2.5f : Alt < 0.5f;     // on a little after sunset, off a little after sunrise
		}
	}
	if (bForce || !bNightKnown || bOn != bNight)
	{
		bNight = bOn;
		bNightKnown = true;
		if (!bOn)
		{
			for (UPointLightComponent* L : Pool) L->SetVisibility(false);
		}
		LastPoolCam = FVector(1e9);
	}
}

void ALyudPublicSpaces::UpdateSeason(bool bForce)
{
	const FDateTime Now = UGameCalendar::NowIn(GetWorld());
	const int32 M = Now.GetMonth(), D = Now.GetDay();
	const int32 SF = CVarCitySummer.GetValueOnGameThread();
	const bool bSum = SF >= 0 ? SF > 0 : (M >= 6 && M <= 8);
	if (bForce || !bSummerKnown || bSum != bSummer)
	{
		bSummer = bSum;
		bSummerKnown = true;
		for (FGroup& G : Groups)
		{
			if (G.bSummer && G.Comp)
			{
				G.Comp->SetVisibility(bSum);
				G.Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
	const int32 IF = CVarCityIce.GetValueOnGameThread();
	const bool bFrozen = IF >= 0 ? IF > 0 : ((M == 12 && D >= 15) || M == 1 || M == 2 || (M == 3 && D < 15));
	if (bForce || !bIceKnown || bFrozen != bIce)
	{
		bIce = bFrozen;
		bIceKnown = true;
		for (UStaticMeshComponent* C : PondWater) C->SetVisibility(!bFrozen);
		for (UStaticMeshComponent* C : PondIce)
		{
			C->SetVisibility(bFrozen);
			C->SetCollisionEnabled(bFrozen ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}
}

void ALyudPublicSpaces::UpdatePool()
{
	if (Pool.Num() == 0 || !bNight || Glow < 0.05f) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	static int32 LastUse = -1;
	const int32 UseNow = CVarCityStreetLights.GetValueOnGameThread();
	if (UseNow == LastUse && FVector::DistSquared(Cam, LastPoolCam) < 150.f * 150.f) return;     // moved less than 1.5 m: same lamps
	LastUse = UseNow;
	LastPoolCam = Cam;
	// the nearest lamp heads (a partial selection over ~150: cheap at 4 Hz)
	TArray<TPair<float, int32>> Near;
	Near.Reserve(LampHeads.Num());
	for (int32 I = 0; I < LampHeads.Num(); I++)
	{
		const float D = FVector::DistSquared(LampHeads[I], Cam);
		if (D < 9000.f * 9000.f) Near.Emplace(D, I);
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const int32 Use = FMath::Clamp(CVarCityStreetLights.GetValueOnGameThread(), 0, Pool.Num());
	for (int32 K = 0; K < Pool.Num(); K++)
	{
		UPointLightComponent* L = Pool[K];
		if (K < Near.Num() && K < Use)
		{
			const int32 I = Near[K].Value;
			const FName Kind = LampKinds[I];
			const bool bPark = Kind == TEXT("lamp_park");
			L->SetWorldLocation(LampHeads[I] - FVector(0, 0, bPark ? 8.f : 0.f));
			L->SetIntensity((bPark ? 1100.f : (Kind == TEXT("stage") || Kind == TEXT("carousel") ? 2600.f : 1800.f)) * Glow);
			L->SetAttenuationRadius(bPark ? 1100.f : 1500.f);
			L->SetVisibility(true);
		}
		else
		{
			L->SetVisibility(false);
		}
	}
}

void ALyudPublicSpaces::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 2.f;
		UpdateNight(false);
		UpdateSeason(false);
		// the Cascade runs when the town's fountain does (its season, its switch)
		bool bRun = false;
		if (const ULyudFountainSubsystem* FS = GetWorld()->GetSubsystem<ULyudFountainSubsystem>())
		{
			for (ALyudFountain* F : FS->GetFountains()) if (F && F->ActorHasTag(TEXT("City")) && F->IsRunning()) bRun = true;
		}
		for (UStaticMeshComponent* C : Cascade) if (C) C->SetVisibility(bRun);
		const bool bShow = CVarCityShow.GetValueOnGameThread() != 0;
		if (bShow != bShown)
		{
			bShown = bShow;
			SetActorHiddenInGame(!bShow);
			if (AProceduralForest* F = Forest.Get()) F->SetActorHiddenInGame(!bShow);
			for (const TWeakObjectPtr<AProceduralPlant>& P : Plants) if (P.IsValid()) P->SetActorHiddenInGame(!bShow);
			if (!bShow) for (UPointLightComponent* L : Pool) L->SetVisibility(false);
			LastPoolCam = FVector(1e9);
			UpdateSeason(true);
		}
	}
	if (!bShown) return;
	// the glass fades up over ~4 s (a lamp warming), the pooled lights with it
	const float Target = bNight ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Glow, Target, 0.001f))
	{
		Glow = FMath::FInterpConstantTo(Glow, Target, DeltaSeconds, 0.25f);
		if (GlowMid) GlowMid->SetScalarParameterValue(TEXT("Glow"), Glow);
		LastPoolCam = FVector(1e9);
		if (Glow <= 0.001f) for (UPointLightComponent* L : Pool) L->SetVisibility(false);
	}
	UpdatePool();
}

int32 ALyudPublicSpaces::CountProps(const FString& Kind) const
{
	int32 N = 0;
	for (const FGroup& G : Groups) if (G.Kind == Kind && G.Comp) N += G.Comp->GetInstanceCount();
	return N;
}

FString ALyudPublicSpaces::DebugReport() const
{
	int32 Lit = 0;
	for (const UPointLightComponent* L : Pool) Lit += L->IsVisible() ? 1 : 0;
	int32 Props = 0;
	for (const FGroup& G : Groups) Props += G.Comp ? G.Comp->GetInstanceCount() : 0;
	return FString::Printf(TEXT("night %d glow %.2f, %d pooled lights lit of %d, summer %d, ice %d, %d props, %d trees (%d baked), %d plants"),
		bNight ? 1 : 0, Glow, Lit, Pool.Num(), bSummer ? 1 : 0, bIce ? 1 : 0, Props, Forest.IsValid() ? Forest->NumTrees() : 0,
		Forest.IsValid() ? Forest->NumBaked() : 0, Plants.Num());
}

// --- the spots --------------------------------------------------------------------------------------------------

ULyudPublicSpotsSubsystem* ULyudPublicSpotsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<ULyudPublicSpotsSubsystem>() : nullptr;
}

bool ULyudPublicSpotsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULyudPublicSpotsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Doc = LoadLayout();
	if (!Doc) return;
	const TArray<TSharedPtr<FJsonValue>>* Ss = nullptr;
	if (!Doc->TryGetArrayField(TEXT("spots"), Ss)) return;
	for (const TSharedPtr<FJsonValue>& V : *Ss)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FLyudPublicSpot S;
		S.Id = FName(*O->GetStringField(TEXT("id")));
		S.Kind = FName(*O->GetStringField(TEXT("kind")));
		FString Sub;
		if (O->TryGetStringField(TEXT("sub"), Sub)) S.Sub = FName(*Sub);
		S.Location = VecField(O, TEXT("pos"));
		S.Rotation = FRotator(0.f, O->GetNumberField(TEXT("yaw")), 0.f);
		S.Capacity = FMath::Max(1, (int32)O->GetNumberField(TEXT("seats")));
		FString Season;
		S.bSummerOnly = O->TryGetStringField(TEXT("season"), Season) && Season == TEXT("summer");
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		if (O->TryGetArrayField(TEXT("slots"), Slots))
		{
			for (const TSharedPtr<FJsonValue>& SV : *Slots)
			{
				const TSharedPtr<FJsonObject> SO = SV->AsObject();
				S.Seats.Add(FTransform(FRotator(0.f, SO->GetNumberField(TEXT("yaw")), 0.f), VecField(SO, TEXT("pos"))));
				S.SeatHeights.Add(SO->GetNumberField(TEXT("height")) * 100.f);
			}
			S.Capacity = S.Seats.Num();
		}
		const TArray<TSharedPtr<FJsonValue>>* Path = nullptr;
		if (O->TryGetArrayField(TEXT("path"), Path))
		{
			for (const TSharedPtr<FJsonValue>& PV : *Path) S.Path.Add(Vec(PV->AsArray()));
		}
		S.Taken.SetNum(S.Capacity);
		ById.Add(S.Id, Spots.Add(MoveTemp(S)));
	}
}

void ULyudPublicSpotsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Doc || CVarPublicSpaces.GetValueOnGameThread() == 0 || !InWorld.GetMapName().Contains(TEXT("Lvl_Scene01")))
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.ObjectFlags |= RF_Transient;
	if (ALyudPublicSpaces* A = InWorld.SpawnActor<ALyudPublicSpaces>(SP))
	{
		if (A->Build(Doc)) Spaces = A;
		else A->Destroy();
	}
}

TArray<FLyudPublicSpot> ULyudPublicSpotsSubsystem::GetSpots(FName Kind) const
{
	TArray<FLyudPublicSpot> Out;
	for (const FLyudPublicSpot& S : Spots) if (Kind.IsNone() || S.Kind == Kind) Out.Add(S);
	return Out;
}

bool ULyudPublicSpotsSubsystem::GetSpot(FName Id, FLyudPublicSpot& Out) const
{
	const int32* I = ById.Find(Id);
	if (!I) return false;
	Out = Spots[*I];
	return true;
}

int32 ULyudPublicSpotsSubsystem::FreePlaces(FName Id) const
{
	const int32* I = ById.Find(Id);
	if (!I) return 0;
	int32 N = 0;
	for (const TWeakObjectPtr<UObject>& T : Spots[*I].Taken) N += T.IsValid() ? 0 : 1;
	return N;
}

FName ULyudPublicSpotsSubsystem::FindNearestFree(FName Kind, FVector From, float MaxDistCm, FName Sub) const
{
	const ALyudPublicSpaces* A = Spaces.Get();
	const bool bSummerNow = A ? A->IsSummer() : false;
	float Best = MaxDistCm * MaxDistCm;
	FName Found;
	for (const FLyudPublicSpot& S : Spots)
	{
		if ((!Kind.IsNone() && S.Kind != Kind) || (!Sub.IsNone() && S.Sub != Sub)) continue;
		if (S.bSummerOnly && !bSummerNow) continue;
		const float D = FVector::DistSquared(S.Location, From);
		if (D >= Best) continue;
		bool bFree = false;
		for (const TWeakObjectPtr<UObject>& T : S.Taken) if (!T.IsValid()) { bFree = true; break; }
		if (bFree)
		{
			Best = D;
			Found = S.Id;
		}
	}
	return Found;
}

int32 ULyudPublicSpotsSubsystem::Reserve(FName Id, UObject* Who)
{
	const int32* I = ById.Find(Id);
	if (!I || !Who) return -1;
	FLyudPublicSpot& S = Spots[*I];
	for (int32 K = 0; K < S.Taken.Num(); K++) if (S.Taken[K].Get() == Who) return K;
	for (int32 K = 0; K < S.Taken.Num(); K++)
	{
		if (!S.Taken[K].IsValid())
		{
			S.Taken[K] = Who;
			return K;
		}
	}
	return -1;
}

void ULyudPublicSpotsSubsystem::Release(FName Id, UObject* Who)
{
	const int32* I = ById.Find(Id);
	if (!I) return;
	for (TWeakObjectPtr<UObject>& T : Spots[*I].Taken) if (T.Get() == Who) T.Reset();
}

void ULyudPublicSpotsSubsystem::ReleaseAll(UObject* Who)
{
	for (FLyudPublicSpot& S : Spots) for (TWeakObjectPtr<UObject>& T : S.Taken) if (T.Get() == Who) T.Reset();
}

FTransform ULyudPublicSpotsSubsystem::GetSeat(FName Id, int32 Place) const
{
	const int32* I = ById.Find(Id);
	if (!I) return FTransform::Identity;
	const FLyudPublicSpot& S = Spots[*I];
	if (S.Seats.IsValidIndex(Place)) return S.Seats[Place];
	// seatless: places spread on a sunflower spiral round the spot (crowds), or along its stroll line
	if (S.Path.Num() >= 2 && Place >= 0)
	{
		const int32 Seg = Place % (S.Path.Num() - 1);
		const float T = FMath::Frac(Place * 0.618034f);
		const FVector P = FMath::Lerp(S.Path[Seg], S.Path[Seg + 1], T);
		return FTransform((S.Path[Seg + 1] - S.Path[Seg]).Rotation(), P);
	}
	const float R = 90.f * FMath::Sqrt((float)FMath::Max(0, Place)), A = Place * 2.39996f;
	return FTransform(S.Rotation, S.Location + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f));
}
