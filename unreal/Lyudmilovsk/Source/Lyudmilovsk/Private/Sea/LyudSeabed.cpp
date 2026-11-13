#include "Sea/LyudSeabed.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantHerb.h"
#include "Plants/ProceduralPlant.h"
#include "Sea/LyudSea.h"
#include "SeaLifeMeshes.h"
#include "Sky/LyudSky.h"
#include "Terrain/LyudTerrain.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<int32> CVarSeabed(TEXT("lyud.Seabed"), 1, TEXT("0: no seabed life."));
	TAutoConsoleVariable<float> CVarSeabedRadius(TEXT("lyud.Seabed.Radius"), -1.f, TEXT("Seabed life radius around the camera, m (negative: by lyud.Sea.Quality)."));

	uint32 TileHash(int32 X, int32 Y, uint32 K)
	{
		uint32 H = uint32(X) * 0x8da6b343u ^ uint32(Y) * 0xd8163841u ^ K * 0xcb1ab31fu;
		H = (H ^ (H >> 15)) * 0x2c1b3c6du;
		H = (H ^ (H >> 12)) * 0x297a2d39u;
		return H ^ (H >> 15);
	}

	struct FRng
	{
		uint32 S;
		float F() { S = S * 1664525u + 1013904223u; return float(S >> 8) / 16777216.f; }
		float R(float A, float B) { return A + (B - A) * F(); }
	};

	/** Smooth 0..1 noise over the ground (patches of meadow, the stones where the terrain has no mask). */
	float Patch(double X, double Y, double Cell, uint32 Seed)
	{
		const double U = X / Cell, V = Y / Cell;
		const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
		const float FX = FMath::SmoothStep(0.f, 1.f, float(U - IX)), FY = FMath::SmoothStep(0.f, 1.f, float(V - IY));
		auto H = [&](int32 A, int32 B) { return float(TileHash(A, B, Seed) >> 8) / 16777216.f; };
		return FMath::Lerp(FMath::Lerp(H(IX, IY), H(IX + 1, IY), FX), FMath::Lerp(H(IX, IY + 1), H(IX + 1, IY + 1), FX), FY);
	}

	const ULyudTerrainMask* Mask()
	{
		// looked up once a minute at most (a missing mask must not be looked for every call)
		static TWeakObjectPtr<ULyudTerrainMask> Cached;
		static double Tried = -1e9;
		if (!Cached.IsValid() && FPlatformTime::Seconds() - Tried > 60.0)
		{
			Tried = FPlatformTime::Seconds();
			ULyudTerrainMask* M = ULyudTerrainMask::Get();
			Cached = M && M->HasSeaStone() ? M : nullptr;
		}
		return Cached.Get();
	}

	struct FKindLook
	{
		const TCHAR* Name;
		/** The plant species it grows (none: a rock or shell). */
		const TCHAR* Species;
		FLinearColor Tint;
		float Roughness;
		bool bShadow;
		/** Gone into the murk beyond this, cm. */
		float Cull;
		/** How far the current bends it (the alga material's Flex). */
		float Flex;
	};
	const FKindLook Looks[] = {
		{ TEXT("Eelgrass"), TEXT("Eelgrass"), FLinearColor::White, 0.45f, false, 2200.f, 1.3f },
		{ TEXT("Bladderwrack"), TEXT("Bladderwrack"), FLinearColor::White, 0.4f, false, 3200.f, 0.6f },
		{ TEXT("Furcellaria"), TEXT("Furcellaria"), FLinearColor::White, 0.55f, false, 2600.f, 0.35f },
		{ TEXT("Boulder"), nullptr, FLinearColor(0.2f, 0.19f, 0.18f), 0.85f, true, 4500.f, 0.f },
		{ TEXT("Mussels"), nullptr, FLinearColor(0.03f, 0.035f, 0.05f), 0.3f, false, 1800.f, 0.f },
		{ TEXT("Shells"), nullptr, FLinearColor(0.75f, 0.68f, 0.58f), 0.6f, false, 1500.f, 0.f },
		{ TEXT("Cladophora"), TEXT("Cladophora"), FLinearColor::White, 0.6f, false, 2000.f, 1.f },
		{ TEXT("Jellyfish"), nullptr, FLinearColor(0.75f, 0.8f, 0.9f), 0.2f, false, 2500.f, 0.f },
		{ TEXT("SeaLettuce"), TEXT("SeaLettuce"), FLinearColor::White, 0.4f, false, 2000.f, 1.f },
		{ TEXT("Oarweed"), TEXT("Oarweed"), FLinearColor::White, 0.45f, false, 3500.f, 0.45f },
		{ TEXT("ReefRock"), nullptr, FLinearColor(0.21f, 0.2f, 0.19f), 0.85f, true, 4500.f, 0.f },
		{ TEXT("Cobbles"), nullptr, FLinearColor(0.26f, 0.24f, 0.21f), 0.75f, false, 2600.f, 0.f },
	};

	float DayNow(const UWorld* World, TWeakObjectPtr<ALyudSky>* Cache = nullptr)
	{
		ALyudSky* Sky = Cache ? Cache->Get() : nullptr;
		if (!Sky)
		{
			if (TActorIterator<ALyudSky> It(World); It)
			{
				Sky = *It;
				if (Cache) *Cache = Sky;
			}
		}
		if (Sky)
		{
			return UGameCalendar::DayOfYear(Sky->ShownTime());
		}
		return UGameCalendar::DayOfYear(UGameCalendar::NowIn(World));
	}
}

ULyudSeabedComponent::ULyudSeabedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bTickInEditor = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Alga(TEXT("/Game/Sea/Underwater/M_LyudSeaAlga.M_LyudSeaAlga"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rock(TEXT("/Game/Sea/M_SeaRock.M_SeaRock"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Jelly(TEXT("/Game/Sea/M_SeaJelly.M_SeaJelly"));
	AlgaMaterial = Alga.Succeeded() ? Alga.Object : nullptr;
	RockMaterial = Rock.Succeeded() ? Rock.Object : nullptr;
	JellyMaterial = Jelly.Succeeded() ? Jelly.Object : nullptr;
}

namespace
{
	/** The Europa hatch on the bed (world cm; world_map.EUROPA_HATCH, Content/Europa/europa.json): nothing grows, lies or
	 * settles within KeepClear of it. */
	const FVector2D Hatch(57300.0, 10790.0);
	constexpr double KeepClear = 400.0;
	/** The traced ground kept (ULyudSeabedComponent::BedCache): pruned to round the eye over the first, never over the second
	 *  (~28 bytes an entry: about 1 MB and 3 MB). */
	constexpr int32 BedCacheMax = 40000;
	constexpr int32 BedCacheHardMax = 120000;
}

void ULyudSeabedComponent::HabitatAt(double X, double Y, float Depth, float& Sand, float& Stone, float& Weed)
{
	if (FVector2D::DistSquared(FVector2D(X, Y), Hatch) < KeepClear * KeepClear)
	{
		Sand = Stone = Weed = 0.f;
		return;
	}
	const ULyudTerrainMask* M = Mask();
	// stones: the terrain's reefs and boulder fields; without a mask (an old build) patches of noise as before
	Stone = M ? M->GetSeaStone(FVector(X, Y, 0.0)) : FMath::Clamp((Patch(X * 0.01, Y * 0.01, 24.0, 5) - 0.6f) * 4.f, 0.f, 1.f) * FMath::SmoothStep(1.f, 3.f, Depth);
	Sand = 1.f - Stone;
	// eelgrass meadows on sand 1.5-7.5 m; wrack on the stones in the first metres
	const float Meadow = FMath::SmoothStep(0.42f, 0.55f, Patch(X * 0.01, Y * 0.01, 17.0, 9)) * FMath::SmoothStep(1.5f, 2.2f, Depth) * (1.f - FMath::SmoothStep(6.5f, 7.5f, Depth));
	Weed = FMath::Max(Meadow * Sand, Stone * (1.f - FMath::SmoothStep(4.f, 7.f, Depth)) * 0.8f);
}

void ULyudSeabedComponent::EnsureComponents()
{
	if (Kinds.Num() == NumKinds * Variants)
	{
		return;
	}
	AActor* Owner = GetOwner();
	PlantKeys.Init(0xFFFFFFFFu, NumKinds * Variants);
	PlantMeshes.SetNum(NumKinds * Variants);
	PlantMIDs.SetNum(NumKinds * Variants * PlantHerbs::NumSlots);
	for (int32 K = 0; K < NumKinds; K++)
	{
		for (int32 V = 0; V < Variants; V++)
		{
			UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Owner, *FString::Printf(TEXT("Seabed_%s_%d"), Looks[K].Name, V), RF_Transient);
			C->SetupAttachment(this);
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(Looks[K].bShadow);
			C->SetGenerateOverlapEvents(false);
			C->bAffectDistanceFieldLighting = false;
			// the jellies move every frame and the weed's material sways it: out of the Lumen scene
			C->bAffectDynamicIndirectLighting = Looks[K].bShadow;
			C->SetCullDistances(0, int32(Looks[K].Cull));
			UStaticMesh* Mesh = nullptr;
			if (K == Boulder) Mesh = LyudSeaLife::Boulder();
			else if (K == ReefRock) Mesh = LyudSeaLife::ReefRock(V);
			else if (K == Cobbles) Mesh = LyudSeaLife::Cobbles(V);
			else if (K == Mussels) Mesh = LyudSeaLife::Mussels();
			else if (K == Shells) Mesh = LyudSeaLife::Shells();
			else if (K == Jelly) Mesh = LyudSeaLife::Jellyfish();
			if (Mesh)
			{
				C->SetStaticMesh(Mesh);
				UMaterialInterface* Base = K == Jelly ? JellyMaterial.Get() : RockMaterial.Get();
				if (Base)
				{
					UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, C);
					M->SetVectorParameterValue(TEXT("Tint"), Looks[K].Tint);
					M->SetScalarParameterValue(TEXT("Roughness"), Looks[K].Roughness);
					C->SetMaterial(0, M);
				}
			}
			C->RegisterComponent();
			Kinds.Add(C);
		}
	}
	UpdatePlants(true);
}

void ULyudSeabedComponent::UpdatePlants(bool bForce)
{
	const float Day = DayNow(GetWorld());
	for (int32 K = 0; K < NumKinds; K++)
	{
		const FPlantSpecies* S = Looks[K].Species ? PlantSpeciesLibrary::Find(FName(Looks[K].Species)) : nullptr;
		if (!S || !AlgaMaterial)
		{
			continue;
		}
		for (int32 V = 0; V < Variants; V++)
		{
			const int32 I = K * Variants + V;
			const uint32 Seed = 0x5EAu + uint32(K) * 131u + uint32(V) * 7919u;
			const PlantHerbs::FState St = PlantHerbs::State(*S, Seed, 3.f + V, Day);
			UInstancedStaticMeshComponent* C = Kinds[I];
			if (!bForce && St.Key == PlantKeys[I])
			{
				continue;
			}
			PlantKeys[I] = St.Key;
			PlantHerbs::FHerbGeometry G;
			float H = 0.f, R = 0.f;
			PlantHerbs::Build(*S, Seed, St, G, H, R);
			if (G.Pos.Num() == 0)
			{
				// gone for the season (Cladophora in winter): nothing drawn
				C->SetStaticMesh(nullptr);
				continue;
			}
			// regrown in place (the old mesh is reused, not leaked)
			PlantMeshes[I] = PlantHerbs::BuildMesh(this, PlantMeshes[I], FName(*FString::Printf(TEXT("SM_Alga_%s_%d"), Looks[K].Name, V)), G);
			C->SetStaticMesh(PlantMeshes[I]);
			for (int32 Slot = 0; Slot < PlantHerbs::NumSlots; Slot++)
			{
				TObjectPtr<UMaterialInstanceDynamic>& M = PlantMIDs[I * PlantHerbs::NumSlots + Slot];
				if (!M) M = UMaterialInstanceDynamic::Create(AlgaMaterial, C);
				const FLinearColor Col = Slot == PlantHerbs::Leaves ? St.LeafColour : Slot == PlantHerbs::Stems ? S->Herb.StemColour : S->Herb.CentreColour;
				M->SetVectorParameterValue(TEXT("Color"), Col);
				M->SetVectorParameterValue(TEXT("TipColour"), S->AutumnLeafColour);
				M->SetScalarParameterValue(TEXT("TipBrown"), Slot == PlantHerbs::Leaves ? St.Wither : 0.f);
				M->SetScalarParameterValue(TEXT("Translucent"), S->Herb.Alga.Translucent);
				M->SetScalarParameterValue(TEXT("HeightRef"), FMath::Max(H * 100.f, 5.f));
				M->SetScalarParameterValue(TEXT("Flex"), Looks[K].Flex * (Slot == PlantHerbs::Stems ? 0.3f : 1.f));
				M->SetScalarParameterValue(TEXT("Roughness"), Looks[K].Roughness);
				C->SetMaterial(Slot, M);
			}
		}
	}
}

float ULyudSeabedComponent::GroundZ(double X, double Y, bool& bOk) const
{
	// the same point gives the same ground: traced once (a complex multi-trace), then remembered
	const FVector2D Key(X, Y);
	if (const float* Known = BedCache.Find(Key))
	{
		bOk = true;
		return *Known;
	}
	const ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	bOk = false;
	const float Z = Sea ? Sea->GroundZ(X, Y, bOk) : 0.f;
	if (bOk)
	{
		// (never more than this even if no rebuild prunes it)
		if (BedCache.Num() >= BedCacheHardMax) BedCache.Reset();
		BedCache.Add(Key, Z);
	}
	return Z;
}

void ULyudSeabedComponent::Clear()
{
	for (UInstancedStaticMeshComponent* C : Kinds)
	{
		if (C && C->GetInstanceCount()) C->ClearInstances();
	}
	bBuilt = false;
	bJelliesPlaced = false;
	BuiltTile = FIntPoint(INT32_MAX, INT32_MAX);
}

void ULyudSeabedComponent::Rebuild(const FVector& Eye)
{
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	if (!Sea)
	{
		return;
	}
	// the bed's life stays where the water covers it even at the lowest spring tide (the flat above it dries)
	const float SeaZ = Sea->LowWaterZ();
	TArray<FTransform> Out[NumKinds][Variants];
	const int32 TX = FMath::FloorToInt(Eye.X / TileSize), TY = FMath::FloorToInt(Eye.Y / TileSize);
	const int32 Reach = FMath::CeilToInt(AppliedRadius / TileSize);
	const float Dens = Density;
	// the season thins what grows (the plant state's share of fronds)
	auto Present = [&](int32 K)
	{
		const FPlantSpecies* S = Looks[K].Species ? PlantSpeciesLibrary::Find(FName(Looks[K].Species)) : nullptr;
		return S ? PlantHerbs::State(*S, 1u, 5.f, DayNow(GetWorld())).Present : 1.f;
	};
	const float CladoNow = Present(Cladophora), LettuceNow = Present(SeaLettuce), EelNow = Present(Eelgrass);
	for (int32 Y = TY - Reach; Y <= TY + Reach; Y++)
	{
		for (int32 X = TX - Reach; X <= TX + Reach; X++)
		{
			const FVector Center((X + 0.5) * TileSize, (Y + 0.5) * TileSize, 0.0);
			const float TileDist = FVector::Dist2D(Center, Eye);
			if (TileDist > AppliedRadius + TileSize * 0.71f)
			{
				continue;
			}
			// only where there is water over some of the tile
			float MaxDepth = -1e9f;
			for (int32 K = 0; K < 4; K++)
			{
				MaxDepth = FMath::Max(MaxDepth, Sea->DepthAtLevel(Center + FVector((K & 1 ? 0.5 : -0.5) * TileSize, (K & 2 ? 0.5 : -0.5) * TileSize, 0.0), SeaZ));
			}
			if (MaxDepth < 10.f)
			{
				continue;
			}
			FRng R{ TileHash(X, Y, 77) };
			auto Place = [&](double PX, double PY, float& OutZ, float& OutDepth) -> bool
			{
				if (FVector2D::DistSquared(FVector2D(PX, PY), Hatch) < FMath::Square(KeepClear + 150.0)) return false;
				bool bOk;
				OutZ = GroundZ(PX, PY, bOk);
				OutDepth = (SeaZ - OutZ) * 0.01f;
				return bOk && OutDepth > 0.15f;
			};
			const double X0 = X * TileSize, Y0 = Y * TileSize;
			const bool bNearEye = TileDist < FMath::Min(AppliedRadius, 2200.f) + TileSize * 0.71f;
			// the tile's stones: sampled at a few points (a reef's edge runs through tiles)
			float Sand, Stone, Weed;
			HabitatAt(Center.X, Center.Y, FMath::Max(MaxDepth * 0.01f, 0.f), Sand, Stone, Weed);
			// rocks: a reef or boulder field is dense with blocks, the open sand has a stray erratic now and then
			const int32 NB = int32((Stone * R.R(5.f, 11.f) + (R.F() < 0.1f ? 1.f : 0.f)) * Dens);
			for (int32 B = 0; B < NB; B++)
			{
				const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
				float Z, D;
				const float Big = R.F();
				const int32 V = int32(R.F() * Variants) % Variants;
				const float Yaw = R.R(0.f, 360.f), Tilt = R.R(-12.f, 12.f);
				float S0, St, W0;
				HabitatAt(PX, PY, 1.f, S0, St, W0);
				if (St < 0.25f && Stone > 0.f && B > 0) continue;
				if (!Place(PX, PY, Z, D))
				{
					continue;
				}
				// on a reef the blocks are big (0.6-2.5 m); in a field of cobbles mostly cobble patches, some boulders
				const bool bReef = St > 0.6f;
				const float S = bReef ? FMath::Lerp(0.6f, 2.4f, Big * Big) : FMath::Lerp(0.35f, 1.3f, Big * Big);
				const int32 Kind = bReef ? (Big < 0.35f ? int32(Cobbles) : int32(ReefRock)) : (Big < 0.6f ? int32(Cobbles) : int32(Boulder));
				const FVector At(PX, PY, Z - (Kind == Cobbles ? 2.f : 12.f * S));
				const FVector Scale = Kind == Cobbles ? FVector(R.R(0.8f, 1.3f)) : FVector(S * R.R(0.85f, 1.15f), S * R.R(0.85f, 1.15f), S * R.R(0.65f, 1.f));
				Out[Kind][Kind == Boulder ? 0 : V].Add(FTransform(FRotator(Tilt, Yaw, R.R(-10.f, 10.f)), At, Scale));
				if (Kind == Cobbles)
				{
					continue;
				}
				// weed on its top by the depth: wrack (with green thread weed and sea lettuce in the first metres), the red
				// Furcellaria from 4 m, a kelp on the deepest big blocks
				const int32 NW = int32(R.R(2.f, 6.f) * S * Dens);
				for (int32 W = 0; W < NW; W++)
				{
					const float A = R.F() * 2.f * PI, Rr = R.F() * 38.f * S;
					const FVector P = At + FVector(Rr * FMath::Cos(A), Rr * FMath::Sin(A), (Kind == ReefRock ? 50.f : 52.f) * S * (1.f - 0.65f * (Rr / (38.f * S)) * (Rr / (38.f * S))));
					if (P.Z > SeaZ - 25.f) continue;
					const float Wd = (SeaZ - P.Z) * 0.01f;
					int32 Grow;
					const float Pick = R.F();
					if (Wd > 9.f && S > 1.3f && Pick < 0.12f) Grow = Oarweed;
					else if (Wd < 2.5f && Pick < 0.25f * LettuceNow) Grow = SeaLettuce;
					else if (Wd < 3.5f && Pick < 0.45f * CladoNow) Grow = Cladophora;
					else if (Wd < FMath::Lerp(3.5f, 6.f, R.F())) Grow = Bladderwrack;
					else Grow = Furcellaria;
					Out[Grow][int32(R.F() * Variants) % Variants].Add(FTransform(FRotator(R.R(-15.f, 15.f), R.R(0.f, 360.f), R.R(-15.f, 15.f)), P, FVector(R.R(0.7f, 1.3f))));
				}
				// blue mussels in the crevices at its foot (Mytilus trossulus covers the Baltic's stones)
				const int32 NM = int32(R.R(0.f, 2.6f) * Dens);
				for (int32 Mi = 0; Mi < NM; Mi++)
				{
					const float A = R.F() * 2.f * PI;
					Out[Mussels][0].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX + FMath::Cos(A) * 48.f * S, PY + FMath::Sin(A) * 48.f * S, Z - 1.f), FVector(R.R(0.7f, 1.5f))));
				}
			}
			// eelgrass meadows on sand from 1.5 m down to ~7 m (dense, so only near the eye: the water hides it beyond ~20 m)
			if (bNearEye && Weed > 0.05f && Stone < 0.5f)
			{
				// thick round the eye, thinner where the murk takes it anyway
				const float Near = 1.f + 1.5f * (1.f - FMath::SmoothStep(800.f, 1600.f, TileDist));
				const int32 NE = int32(Weed * 150.f * Near * Dens * (0.4f + 0.6f * EelNow));
				for (int32 E = 0; E < NE; E++)
				{
					const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
					float Z, D;
					float S0, St, W0;
					if (!Place(PX, PY, Z, D)) continue;
					HabitatAt(PX, PY, D, S0, St, W0);
					if (W0 * S0 < 0.12f + 0.25f * R.F() * (1.f - W0)) continue;
					// shorter where the surf reaches down
					const float H = FMath::Min(R.R(0.75f, 1.2f), D * 0.4f);
					Out[Eelgrass][int32(R.F() * Variants) % Variants].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX, PY, Z - 2.f), FVector(1.f, 1.f, H)));
				}
			}
			// shells on the sand everywhere but the deepest; drifting loose mats of Furcellaria on the deep sand
			if (R.F() < 0.7f * Dens)
			{
				const double PX = X0 + R.F() * TileSize, PY = Y0 + R.F() * TileSize;
				float Z, D;
				if (Place(PX, PY, Z, D) && D < 11.f)
				{
					Out[Shells][0].Add(FTransform(FRotator(0.f, R.R(0.f, 360.f), 0.f), FVector(PX, PY, Z - 0.5f), FVector(R.R(0.8f, 1.3f))));
				}
			}
			if (MaxDepth > 700.f && R.F() < 0.5f * Dens)
			{
				const int32 NL = int32(R.R(1.f, 5.f));
				const double CX = X0 + R.F() * TileSize, CY = Y0 + R.F() * TileSize;
				for (int32 L = 0; L < NL; L++)
				{
					const double PX = CX + R.R(-120.f, 120.f), PY = CY + R.R(-120.f, 120.f);
					float Z, D;
					if (Place(PX, PY, Z, D) && D > 6.f)
					{
						// lying over: the loose form tumbles along the bed
						Out[Furcellaria][int32(R.F() * Variants) % Variants].Add(FTransform(FRotator(R.R(50.f, 80.f), R.R(0.f, 360.f), 0.f), FVector(PX, PY, Z + 4.f), FVector(R.R(0.6f, 1.f))));
					}
				}
			}
		}
	}
	for (int32 K = 0; K < NumKinds; K++)
	{
		if (K == Jelly)
		{
			continue;
		}
		for (int32 V = 0; V < Variants; V++)
		{
			UInstancedStaticMeshComponent* C = Mesh(K, V);
			C->ClearInstances();
			if (Out[K][V].Num() && C->GetStaticMesh())
			{
				C->AddInstances(Out[K][V], false, true, false);
			}
		}
	}
	bBuilt = true;
	// the remembered ground: only what is round the eye is kept once it has grown (the points further off are traced
	// again if the eye comes back)
	if (BedCache.Num() > BedCacheMax)
	{
		const double Keep = FMath::Square(double(AppliedRadius) + 2.0 * TileSize);
		const FVector2D Eye2(Eye.X, Eye.Y);
		for (auto It = BedCache.CreateIterator(); It; ++It)
		{
			if (FVector2D::DistSquared(It.Key(), Eye2) > Keep) It.RemoveCurrent();
		}
	}
}

void ULyudSeabedComponent::UpdateJellies(const FVector& Eye, float Dt)
{
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	UInstancedStaticMeshComponent* J = Mesh(Jelly, 0);
	// moon jellies come with the summer's warmth and stay into the autumn's storms
	const float Day = DayNow(GetWorld(), &SkyRef);
	const int32 Want = (Day > 165.f && Day < 320.f) ? FMath::Max(Jellyfish, 0) : 0;
	const float SeaZ = Sea->SeaLevel();
	const float Box = FMath::Min(AppliedRadius, 1100.f);
	FRng R{ uint32(Clock * 1000.0) + 13u };
	auto Column = [&](const FVector2D& XY, float& Lo, float& Hi) -> bool
	{
		const float D = Sea->DepthAt(FVector(XY, 0.0));
		Lo = SeaZ - D + 40.f;
		Hi = SeaZ - 30.f;
		return D > 80.f;
	};
	if (!bJelliesPlaced || Jellies.Num() != Want)
	{
		bJelliesPlaced = true;
		Jellies.SetNum(Want);
		for (FSwimmer& S : Jellies)
		{
			float Lo, Hi;
			const FVector2D XY(Eye.X + R.R(-Box, Box), Eye.Y + R.R(-Box, Box));
			S.Pos = FVector(XY, Column(XY, Lo, Hi) ? R.R(Lo, Hi) : SeaZ - 9e5f);
			S.Phase = R.R(0.f, 6.28f);
			S.Scale = R.R(0.6f, 1.4f);
			S.Seed = R.F();
		}
	}
	// pulse up a little, sink back, carried by the waves' orbits; recycled ahead of the camera
	TArray<FTransform>& JX = JellyXf;
	JX.Reset();
	JX.Reserve(Jellies.Num());
	for (FSwimmer& S : Jellies)
	{
		float Lo, Hi;
		if (FVector::Dist2D(S.Pos, Eye) > Box * 1.3f || S.Pos.Z < SeaZ - 5e5f)
		{
			const FVector2D Away = FVector2D(Eye - S.Pos).GetSafeNormal();
			const FVector2D XY = FVector2D(Eye) + Away * Box * R.R(0.7f, 1.f) + FVector2D(-Away.Y, Away.X) * R.R(-Box, Box) * 0.6f;
			S.Pos = FVector(XY, Column(XY, Lo, Hi) ? R.R(Lo, Hi) : SeaZ - 9e5f);
		}
		S.Phase += Dt * (1.4f + 0.4f * S.Seed);
		const FLyudSeaSample W = Sea->SampleSurface(S.Pos);
		const float Deep = FMath::Exp(-FMath::Max(W.SurfaceZ - S.Pos.Z, 0.f) / 400.f);
		const float Pulse = FMath::Max(0.f, FMath::Sin(S.Phase));
		S.Pos += (W.Velocity * Deep * 0.8f + FVector(0.f, 0.f, 6.f * Pulse - 2.5f)) * Dt;
		if (Column(FVector2D(S.Pos), Lo, Hi))
		{
			S.Pos.Z = FMath::Clamp(S.Pos.Z, Lo, Hi);
		}
		const FRotator Tilt(8.f * FMath::Sin(S.Phase * 0.31f + S.Seed * 6.f), S.Seed * 360.f, 8.f * FMath::Cos(S.Phase * 0.27f));
		// the bell's beat: it narrows and deepens, then relaxes slowly
		const float Beat = FMath::Pow(Pulse, 3.f);
		JX.Add(FTransform(Tilt, S.Pos, FVector(S.Scale * (1.f - 0.14f * Beat), S.Scale * (1.f - 0.14f * Beat), S.Scale * (1.f + 0.2f * Beat))));
	}
	if (J->GetInstanceCount() != JX.Num())
	{
		J->ClearInstances();
		if (JX.Num()) J->AddInstances(JX, false, true, false);
	}
	else if (JX.Num())
	{
		J->BatchUpdateInstancesTransforms(0, JX, true, false);
	}
}

void ULyudSeabedComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ALyudSea* Sea = Cast<ALyudSea>(GetOwner());
	UWorld* World = GetWorld();
	if (!Sea || !World || !World->IsGameWorld())
	{
		return;
	}
	const bool bOn = CVarSeabed.GetValueOnGameThread() != 0 && IsActive();
	EnsureComponents();
	for (UInstancedStaticMeshComponent* C : Kinds)
	{
		C->SetVisibility(bOn && !bAwayHidden);
	}
	if (!bOn || !Sea->IsDepthReady())
	{
		return;
	}
	Clock += DeltaTime;
	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	// only round an eye in the water or a few metres over it (from the cliff or the town nothing of it shows)
	// against the still level (a passing wave crest moved the edge), and leaving a little further out than coming in
	const FLyudSeaSample Here = Sea->SampleSurface(Eye);
	const bool bShown = bBuilt && !bAwayHidden;
	const float Level = Sea->SeaLevel();
	const bool bNear = Here.bIsSea && (bShown ? Here.Depth > 25.f && Eye.Z < Level + 1100.f : Here.Depth > 50.f && Eye.Z < Level + 800.f);
	if (!bNear)
	{
		if (bBuilt)
		{
			// hide now, free after half a minute away (coming back to the same tile then costs nothing)
			if (!bAwayHidden)
			{
				bAwayHidden = true;
				AwayTime = 0.f;
				for (UInstancedStaticMeshComponent* C : Kinds)
				{
					C->SetVisibility(false);
				}
			}
			AwayTime += DeltaTime;
			if (AwayTime > 30.f)
			{
				Clear();
				bAwayHidden = false;
			}
		}
		return;
	}
	if (bAwayHidden)
	{
		bAwayHidden = false;
		for (UInstancedStaticMeshComponent* C : Kinds)
		{
			C->SetVisibility(true);
		}
	}
	// the plants grow with the calendar: check their state now and then (a regrow only when it changed)
	if ((PlantClock -= DeltaTime) <= 0.f)
	{
		PlantClock = 10.f;
		UpdatePlants(false);
	}
	static IConsoleVariable* Quality = IConsoleManager::Get().FindConsoleVariable(TEXT("lyud.Sea.Quality"));
	int32 Q = Quality ? Quality->GetInt() : -1;
	if (Q < 0)
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		Q = 0;
#else
		Q = 2;
#endif
	}
	const float Forced = CVarSeabedRadius.GetValueOnGameThread();
	const float Rad = Forced >= 0.f ? Forced * 100.f : (Q == 0 ? 1500.f : Q == 1 ? 2500.f : Radius);
	const FIntPoint Tile(FMath::FloorToInt(Eye.X / TileSize), FMath::FloorToInt(Eye.Y / TileSize));
	if (Tile != BuiltTile || Q != BuiltQuality || Rad != AppliedRadius || !bBuilt)
	{
		BuiltTile = Tile;
		BuiltQuality = Q;
		AppliedRadius = Rad;
		for (int32 K = 0; K < NumKinds; K++)
		{
			for (int32 V = 0; V < Variants; V++)
			{
				Mesh(K, V)->SetCullDistances(0, int32(FMath::Min(Looks[K].Cull, Rad * 1.2f)));
			}
		}
		Rebuild(Eye);
	}
	UpdateJellies(Eye, DeltaTime);
}

FVector ULyudSeabedComponent::FindNearest(int32 Kind, const FVector& From) const
{
	FVector Best = From;
	if (bAwayHidden) return Best;  // parked away from the water: nothing on the bed, as before it was kept
	double BestD = DBL_MAX;
	for (int32 V = 0; V < Variants; V++)
	{
		const UInstancedStaticMeshComponent* C = Mesh(Kind, V);
		if (!C) continue;
		for (int32 I = 0; I < C->GetInstanceCount(); I++)
		{
			FTransform T;
			C->GetInstanceTransform(I, T, true);
			const double D = FVector::DistSquared(T.GetLocation(), From);
			if (D < BestD)
			{
				BestD = D;
				Best = T.GetLocation();
			}
		}
	}
	return Best;
}

int32 ULyudSeabedComponent::GetInstanceCount() const
{
	int32 N = 0;
	for (const UInstancedStaticMeshComponent* C : Kinds)
	{
		N += C ? C->GetInstanceCount() : 0;
	}
	return N;
}

FString ULyudSeabedComponent::GetDebugReport() const
{
	FString S = TEXT("seabed:");
	for (int32 K = 0; K < NumKinds; K++)
	{
		int32 N = 0;
		for (int32 V = 0; V < Variants; V++) N += Mesh(K, V) ? Mesh(K, V)->GetInstanceCount() : 0;
		S += FString::Printf(TEXT(" %s %d"), Looks[K].Name, N);
	}
	return S;
}
