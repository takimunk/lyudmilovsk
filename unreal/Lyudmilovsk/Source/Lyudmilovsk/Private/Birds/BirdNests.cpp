#include "Birds/BirdNests.h"

#include "BirdNest.h"
#include "BirdPerches.h"
#include "BirdSim.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

using namespace LyudBirds;

namespace
{
	FVector ToUE(const V3& V) { return FVector(V.X, V.Y, V.Z) * 100.0; }
	V3 FromUE(const FVector& V) { return V3((float)V.X * 0.01f, (float)V.Y * 0.01f, (float)V.Z * 0.01f); }

	/** The kits are built facing +X (blender/birds/nests/README.md), as Unreal's forward. */
	constexpr float KitYaw = 0.f;

	uint32 Mix(uint32 H, const FTransform& X)
	{
		const FVector L = X.GetLocation(), S = X.GetScale3D();
		const FRotator R = X.Rotator();
		const int32 V[7] = { FMath::RoundToInt(L.X), FMath::RoundToInt(L.Y), FMath::RoundToInt(L.Z), FMath::RoundToInt(R.Yaw * 10.f),
			FMath::RoundToInt(R.Pitch * 10.f), FMath::RoundToInt(S.X * 100.f), FMath::RoundToInt(S.Z * 100.f) };
		for (int32 I : V)
		{
			H = HashCombineFast(H, (uint32)I);
		}
		return H;
	}

	uint32 Hash32(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352du;
		X ^= X >> 15;
		X *= 0x846ca68bu;
		X ^= X >> 16;
		return X;
	}

	bool IsTreeSite(EHabitat H) { return H == EHabitat::Broadleaf || H == EHabitat::Conifer || H == EHabitat::Garden; }

	/** The mesh of the box or hole a cavity nester uses: boxes in the town's gardens and on houses, holes in the woods. */
	FName CavityMesh(const FSpecies& S, EHabitat Site)
	{
		if (S.Id == "Starling")
		{
			return TEXT("SM_NestBox_Starling");      // the skvorechnik, everywhere
		}
		if (S.Id == "Jackdaw")
		{
			return IsTreeSite(Site) ? FName(TEXT("SM_TreeHole")) : NAME_None;   // in the church tower: out of sight
		}
		if (Site == EHabitat::Broadleaf || Site == EHabitat::Conifer)
		{
			return TEXT("SM_TreeHole");
		}
		return S.Id == "TreeSparrow" ? FName(TEXT("SM_NestBox_Sparrow")) : FName(TEXT("SM_NestBox_Tit"));
	}

	bool Mesh_IsTreeHole(const FBreedKind& K, const FSpecies& S, EHabitat Site)
	{
		return K.Nest == ENest::Cavity && CavityMesh(S, Site) == FName(TEXT("SM_TreeHole"));
	}

	const TCHAR* AgeName(float Age, float NestlingDays)
	{
		const float T = Age / FMath::Max(1.f, NestlingDays);
		return T < 0.3f ? TEXT("Hatch") : (T < 0.7f ? TEXT("Mid") : TEXT("Late"));
	}

	/** How far off a nest is drawn: its size on screen (a tit's box ~35 m, a rook's nest ~150 m, an eagle's 400 m). */
	float NestRange(const FBreedKind& K) { return FMath::Clamp(K.NestRadius * 2.f * 250.f, 30.f, 400.f); }
}

ABirdNestRenderer::ABirdNestRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Perches = MakeShared<FBirdPerches>();
}

ABirdNestRenderer::~ABirdNestRenderer() = default;

ABirdNestRenderer::FDrawSet* ABirdNestRenderer::SetFor(FName Mesh, bool bMover)
{
	if (FDrawSet* S = Sets.Find(Mesh))
	{
		return S;
	}
	if (Missing.Contains(Mesh))
	{
		return nullptr;
	}
	// only ask whether the package is there (the kits may not be imported in a checkout); load the mesh when it is drawn
	const FString Path = Mesh == FName(TEXT("Pole")) ? FString(TEXT("/Engine/BasicShapes/Cylinder")) : FString::Printf(TEXT("/Game/Birds/Nests/%s"), *Mesh.ToString());
	UStaticMesh* SM = FPackageName::DoesPackageExist(Path)
		? LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!SM)
	{
		Missing.Add(Mesh);
		return nullptr;
	}
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("ISM_%s"), *Mesh.ToString()));
	ISM->SetupAttachment(RootComponent);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->bDisableCollision = true;
	ISM->SetCanEverAffectNavigation(false);
	ISM->LightingChannels.bChannel1 = true;          // Scene 01's outdoor sun
	// small things give no bounce light worth having; out of the Lumen scene they cost no card captures when they change
	ISM->bAffectDynamicIndirectLighting = false;
	ISM->bAffectDistanceFieldLighting = false;
	ISM->SetCastShadow(true);
	ISM->SetStaticMesh(SM);
	ISM->RegisterComponent();
	FDrawSet& S = Sets.Add(Mesh);
	S.ISM = ISM;
	S.bMover = bMover;
	return &S;
}

void ABirdNestRenderer::Commit(FDrawSet& S, TArray<FTransform>& Want, bool bForce)
{
	uint32 H = (uint32)Want.Num();
	for (const FTransform& X : Want)
	{
		H = Mix(H, X);
	}
	if (H == S.Hash && !bForce)
	{
		return;
	}
	S.Hash = H;
	UInstancedStaticMeshComponent* ISM = S.ISM.Get();
	if (!ISM)
	{
		S.Xf = Want;
		return;
	}
	if (ISM->GetInstanceCount() == Want.Num() && Want.Num() > 0)
	{
		// the same number: move them in place (no proxy rebuild)
		ISM->BatchUpdateInstancesTransforms(0, Want, true, false, true);
	}
	else
	{
		ISM->ClearInstances();
		if (Want.Num())
		{
			ISM->AddInstances(Want, false, true);
		}
		Rebuilds++;
	}
	S.Xf = Want;
}

void ABirdNestRenderer::OnFeed(int32 Nest)
{
	LastFeed.Add(Nest, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void ABirdNestRenderer::Draw(FWorld& World, const FVector& Camera, float DeltaTime)
{
	SetActorLocation(FVector::ZeroVector);
	if (!World.Nesting())
	{
		return;
	}
	StaticTimer -= DeltaTime;
	if (StaticTimer <= 0.f)
	{
		StaticTimer = 0.5f;
		BuildStatic(World, Camera);
	}
	AnimateBroods(World, Camera, DeltaTime);
	DrawMovers(World, Camera);
}

void ABirdNestRenderer::Place(FWorld& World, int32 NestId)
{
	FNesting& N = *World.Nesting();
	FNest& Nest = N.Nests[NestId];
	if (Nest.bPlaced)
	{
		return;
	}
	Nest.bPlaced = true;
	Placed++;
	UWorld* UW = GetWorld();
	const FBreedKind& K = BreedTable()[Nest.Kind];
	const FSite& Site = World.Sites[Nest.Site];
	const FVector At = ToUE(Nest.Pos);
	const FVector Fwd(FMath::Cos(Nest.Yaw), FMath::Sin(Nest.Yaw), 0.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdNest), false);
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(UW, 0))
	{
		Q.AddIgnoredActor(Pawn);
	}
	FHitResult Hit;
	if (IsTreeSite(Site.Kind))
	{
		// on the tree's own branches (the skeleton its wood is drawn from): a platform in a strong fork near the trunk, a cup
		// out on the thinner side branches, a box or a hole on the trunk itself
		const bool bTrunk = K.Nest == ENest::Cavity;
		const float RMin = bTrunk ? 7.f : (K.Nest == ENest::Cup ? 0.5f : 1.5f);
		const float RMax = bTrunk ? 200.f : (K.Nest == ENest::Cup ? 4.f : 15.f);
		float Best = 1e20f;
		FVector BestP = At, BestDir = Fwd;
		float BestR = 0.f;
		Perches->ForEachBranch(UW, At, 600.f, [&](const FVector& A, const FVector& B, float RA, float RB)
		{
			const float R = FMath::Min(RA, RB);
			if (R < RMin || R > RMax)
			{
				return;
			}
			FVector P;
			if (bTrunk)
			{
				P = FMath::ClosestPointOnSegment(At, A, B);
			}
			else
			{
				P = A;          // a branch's base: a fork
				if (FMath::Abs((B - A).GetSafeNormal().Z) > 0.85f && K.Nest != ENest::Cup)
				{
					return;     // a platform wants a fork, not a point on an upright stem
				}
			}
			const float Score = (float)FVector::Dist(P, At) + FMath::Abs((float)(P.Z - At.Z)) * 0.5f;
			if (Score < Best)
			{
				Best = Score;
				BestP = P;
				BestR = bTrunk ? FMath::Lerp(RA, RB, (float)((P - A).Size() / FMath::Max(1.0, (B - A).Size()))) : R;
				BestDir = (At - P).GetSafeNormal2D();
			}
		});
		if (Best < 1e19f)
		{
			PlacedOn[0]++;
			if (bTrunk)
			{
				const FVector Dir = BestDir.IsNearlyZero() ? Fwd : BestDir;
				Nest.Pos = FromUE(BestP + Dir * BestR);
				Nest.Yaw = FMath::Atan2((float)Dir.Y, (float)Dir.X);
				Nest.DrawScale = FMath::Clamp(BestR / 17.f, 0.45f, 1.2f);     // the tree hole's bark patch is cut for a 17 cm trunk
			}
			else
			{
				Nest.Pos = FromUE(BestP + FVector(0.0, 0.0, BestR * 0.8f));
			}
		}
		else if (bTrunk)
		{
			// no trunk to hang the box on (a yard, a young tree): a pole of its own, the box ~3.5 m up
			FHitResult Ground;
			if (UW->LineTraceSingleByChannel(Ground, At + FVector(0.0, 0.0, 300.0), At - FVector(0.0, 0.0, 1500.0), ECC_Visibility, Q))
			{
				Nest.Pos = FromUE(Ground.ImpactPoint + FVector(0.0, 0.0, 350.0) + Fwd * 5.0);
				Nest.bOnPole = true;
				PlacedOn[2]++;
			}
			else
			{
				PlacedOn[3]++;
			}
		}
		else
		{
			PlacedOn[3]++;
		}
		return;
	}
	const bool bWall = K.Nest == ENest::Eave || K.Nest == ENest::MudCup || K.Nest == ENest::Cavity || K.Nest == ENest::Burrow;
	if (bWall && !IsGroundHabitat(Site.Kind))
	{
		// on the wall or the cliff face: the nearer surface either way along its facing
		float Best = 1e20f;
		for (int32 Side = 0; Side < 2; Side++)
		{
			const FVector D = Side ? -Fwd : Fwd;
			if (UW->LineTraceSingleByChannel(Hit, At + D * 300.0, At - D * 50.0, ECC_Visibility, Q) && Hit.Distance < Best)
			{
				Best = Hit.Distance;
				Nest.Pos = FromUE(Hit.ImpactPoint + Hit.ImpactNormal * (K.Nest == ENest::Cavity ? 0.0 : 1.0));
				Nest.Yaw = FMath::Atan2((float)Hit.ImpactNormal.Y, (float)Hit.ImpactNormal.X);
			}
		}
		PlacedOn[Best < 1e19f ? 1 : 3]++;
		return;
	}
	// on the ground, a ledge or a roof: somewhere level (a scrape on the cliff goes to its top or a ledge, not the face)
	bool bFound = false;
	for (int32 Ring = 0; Ring < 3 && !bFound; Ring++)
	{
		for (int32 Pt = 0; Pt < (Ring ? 8 : 1) && !bFound; Pt++)
		{
			const float Ang = Pt * PI / 4.f + Ring * 0.3f;
			const FVector P = At + FVector(FMath::Cos(Ang), FMath::Sin(Ang), 0.0) * (Ring * 150.0);
			if (UW->LineTraceSingleByChannel(Hit, P + FVector(0.0, 0.0, 300.0), P - FVector(0.0, 0.0, 800.0), ECC_Visibility, Q) && Hit.ImpactNormal.Z > 0.75)
			{
				Nest.Pos = FromUE(Hit.ImpactPoint);
				bFound = true;
			}
		}
	}
	PlacedOn[bFound ? 2 : 3]++;
}

void ABirdNestRenderer::BuildStatic(FWorld& World, const FVector& Camera)
{
	FNesting& N = *World.Nesting();
	TMap<FName, TArray<FTransform>> Want;
	Broods.Reset();
	const V3 Cam = FromUE(Camera);
	int32 Placements = 0;
	auto Add = [&](FName Mesh, const FTransform& X)
	{
		Want.FindOrAdd(Mesh).Add(X);
	};
	static const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	Nearest = FString();
	float NearestD = 1e9f;
	for (FNest& Nest : N.Nests)
	{
		const FBreedKind& K = BreedTable()[Nest.Kind];
		const float Dist = (Nest.Pos - Cam).Len();
		if (Dist < NearestD)
		{
			NearestD = Dist;
			Nearest = FString::Printf(TEXT("nearest nest %d %s %.1f m (range %.0f), stage %s"), Nest.Id, UTF8_TO_TCHAR(K.Id), Dist, NestRange(K),
				UTF8_TO_TCHAR(StageName(Nest.State.Stage)));
		}
		const float Range = NestRange(K);
		if (Dist > Range && Dist > 160.f)
		{
			continue;
		}
		if (!Nest.bPlaced && Placements < 6)
		{
			Place(World, Nest.Id);
			Placements++;
		}
		if (Dist > Range)
		{
			continue;
		}
		const FNestState& St = Nest.State;
		const FSpecies& S = SpeciesTable()[Nest.Species];
		const FName Id(S.Id.c_str());
		const FRotator Rot(0.f, FMath::RadiansToDegrees(Nest.Yaw) + KitYaw, 0.f);
		const FVector At = ToUE(Nest.Pos);
		// (a tree hole's bark patch: only a narrow rim round the hole, so the trunk's own bark shows round it)
		const FTransform NestXf(Rot, At, Mesh_IsTreeHole(K, S, World.Sites[Nest.Site].Kind)
			? FVector(Nest.DrawScale, Nest.DrawScale * 0.45f, Nest.DrawScale * 0.45f) : FVector(Nest.DrawScale));
		const bool bHidden = K.Nest == ENest::Cavity || K.Nest == ENest::Eave || K.Nest == ENest::Burrow;
		// the nest itself
		FName Mesh = NAME_None;
		if (K.Nest == ENest::Cavity)
		{
			Mesh = CavityMesh(S, World.Sites[Nest.Site].Kind);      // boxes and holes are there all year
		}
		else if (K.Nest == ENest::Burrow)
		{
			Mesh = TEXT("SM_Burrow");          // (sunk a little into the face below: only the hole and its lip show)
		}
		else if (K.Nest == ENest::Eave)
		{
			const bool bShows = (St.Stage == EStage::Build && St.Progress > 0.5f) || St.Stage == EStage::Eggs || St.Stage == EStage::Chicks
				|| St.Stage == EStage::Fledged || (St.Stage == EStage::Old && K.bReuse);
			if (bShows && (S.Id == "HouseSparrow" || S.Id == "WhiteWagtail"))
			{
				Mesh = FName(*FString::Printf(TEXT("SM_Nest_%s_Tuft"), *Id.ToString()));
			}
		}
		else
		{
			const TCHAR* Stage = nullptr;
			switch (St.Stage)
			{
			case EStage::Build: Stage = St.Progress < 0.05f ? nullptr : (St.Progress < 0.4f ? TEXT("B1") : (St.Progress < 0.8f ? TEXT("B2") : TEXT("Full"))); break;
			case EStage::Eggs:
			case EStage::Chicks:
			case EStage::Fledged: Stage = TEXT("Full"); break;
			case EStage::Failed: Stage = TEXT("Old"); break;
			case EStage::Old: Stage = K.Nest == ENest::Scrape || St.Progress < 0.35f ? nullptr : TEXT("Old"); break;
			default: break;
			}
			if (Stage)
			{
				Mesh = FName(*FString::Printf(TEXT("SM_Nest_%s_%s"), *Id.ToString(), Stage));
			}
		}
		if (FDrawSet* Set = !Mesh.IsNone() ? SetFor(Mesh, false) : nullptr)
		{
			FTransform X = NestXf;
			if (K.Nest == ENest::Cavity && Mesh != FName(TEXT("SM_TreeHole")))
			{
				// a box: its origin is the hole on its front; its back goes against the trunk or the wall
				const float Depth = -(float)Set->ISM->GetStaticMesh()->GetBoundingBox().Min.X;
				X = FTransform(Rot, At + Rot.Vector() * Depth);
				if (Nest.bOnPole && SetFor(TEXT("Pole"), false))
				{
					// the pole under the box's back (the engine's cylinder: 1 m tall and wide, pivot in the middle)
					const FVector Foot = At - FVector(0.0, 0.0, 350.0);
					Add(TEXT("Pole"), FTransform(FRotator::ZeroRotator, Foot + FVector(0.0, 0.0, 190.0) - Rot.Vector() * 4.0, FVector(0.07, 0.07, 3.8)));
				}
			}
			if (K.Nest == ENest::Burrow)
			{
				X.AddToTranslation(-Rot.Vector() * 1.0);
				X.SetScale3D(FVector(1.0, 0.6, 0.6));      // a smaller clay lip: the cliff's own face round the hole
			}
			Add(Mesh, X);
		}
		if (Dist <= NearestD)
		{
			Nearest += FString::Printf(TEXT(" mesh %s%s at %s"), *Mesh.ToString(), Mesh.IsNone() || Missing.Contains(Mesh) ? TEXT(" (none)") : TEXT(""), *At.ToString());
		}
		// eggs and chicks, close up
		const float Near = FMath::Max(20.f, S.Length * 45.f);
		if (Dist > Near)
		{
			continue;
		}
		uint32 R = Nest.Seed;
		auto Ring = [&](int32 I, int32 Count, float Size, float& Yaw) -> FVector
		{
			const float Ang = (I + 0.5f) / FMath::Max(1, Count) * 2.f * PI + (Hash32(R + I) % 1000) * 0.0006f;
			const float Rr = Count <= 1 ? 0.f : FMath::Min(K.NestRadius * 0.25f, Size * 0.55f * FMath::Sqrt((float)Count)) * 100.f;
			Yaw = FMath::RadiansToDegrees(Ang);
			return FVector(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, 0.0);
		};
		if (St.Stage == EStage::Eggs && !bHidden)
		{
			const FName Egg(*FString::Printf(TEXT("SM_Egg_%s"), *Id.ToString()));
			if (SetFor(Egg, false))
			{
				const float EggLen = FMath::Clamp(S.Length * 0.13f, 0.012f, 0.12f);
				for (int32 I = 0; I < St.Eggs; I++)
				{
					float Yaw;
					const FVector Off = Ring(I, St.Eggs, EggLen * 0.8f, Yaw);
					Add(Egg, FTransform(FRotator(0.f, Yaw + 90.f + (Hash32(R + 7 * I) % 60), 0.f), NestXf.TransformPosition(Off)));
				}
			}
		}
		if (St.Stage == EStage::Chicks && St.Chicks > 0)
		{
			const TCHAR* Age = AgeName(St.Age, (float)K.NestlingDays);
			const FName Rest(*FString::Printf(TEXT("SM_Chick_%s_%s_Rest"), *Id.ToString(), Age));
			const FName Beg(*FString::Printf(TEXT("SM_Chick_%s_%s_Beg"), *Id.ToString(), Age));
			if (!SetFor(Rest, false) || !SetFor(Beg, false))
			{
				continue;
			}
			FBrood Br;
			Br.Nest = Nest.Id;
			Br.Rest = Rest;
			Br.Beg = Beg;
			Br.First = Want.FindOrAdd(Rest).Num();
			const int32 BegFirst = Want.FindOrAdd(Beg).Num();
			check(BegFirst == Br.First || true);
			const float Size = S.Length * (St.Age < K.NestlingDays * 0.3f ? 0.3f : 0.6f);
			if (bHidden)
			{
				// in a box or a burrow: only the oldest come to the hole, one at a time, gaping
				if (FCString::Strcmp(Age, TEXT("Late")) == 0)
				{
					const FTransform X(Rot, At - Rot.Vector() * (S.Length * 30.0));
					Br.Base.Add(X);
					Add(Rest, Hidden);
					Add(Beg, Hidden);
				}
			}
			else
			{
				for (int32 I = 0; I < St.Chicks; I++)
				{
					float Yaw;
					const FVector Off = Ring(I, St.Chicks, Size, Yaw);
					const FTransform X(FRotator(0.f, Yaw + Rot.Yaw, 0.f), NestXf.TransformPosition(Off));
					Br.Base.Add(X);
					Add(Rest, X);
					Add(Beg, Hidden);
				}
			}
			Br.Count = Br.Base.Num();
			// both sets get the brood at the same index only if they had the same count before: keep them in step
			Br.First = Want[Rest].Num() - Br.Count;
			if (Want[Beg].Num() - Br.Count != Br.First)
			{
				Br.Count = 0;      // (can't happen: chicks are always added to both)
			}
			if (Br.Count > 0)
			{
				Br.Phase = (Hash32(R) % 1000) * 0.01f;
				Broods.Add(Br);
			}
		}
	}
	int32 Total = 0;
	for (TPair<FName, FDrawSet>& P : Sets)
	{
		if (P.Value.bMover)
		{
			continue;
		}
		TArray<FTransform>* W = Want.Find(P.Key);
		TArray<FTransform> Empty;
		Commit(P.Value, W ? *W : Empty, false);
		Total += P.Value.Xf.Num();
	}
	InstancesLastUpdate = Total;
}

void ABirdNestRenderer::AnimateBroods(FWorld& World, const FVector& Camera, float DeltaTime)
{
	FNesting& N = *World.Nesting();
	const double Now = GetWorld()->GetTimeSeconds();
	static const FTransform Hidden(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	for (FBrood& Br : Broods)
	{
		const FNest& Nest = N.Nests[Br.Nest];
		if (FVector::Dist(ToUE(Nest.Pos), Camera) > 2500.0)
		{
			continue;
		}
		FDrawSet* Rest = Sets.Find(Br.Rest);
		FDrawSet* Beg = Sets.Find(Br.Beg);
		if (!Rest || !Beg || Rest->Xf.Num() < Br.First + Br.Count || Beg->Xf.Num() < Br.First + Br.Count)
		{
			continue;
		}
		// they gape when a parent lands at the nest (and for a few seconds after a feed), and now and then on their own
		const double* Fed = LastFeed.Find(Br.Nest);
		bool bParent = false;
		for (int32 Id : { Nest.Male, Nest.Female })
		{
			if (Id >= 0 && (World.Birds[Id].Pos - Nest.Pos).Len() < 0.6f)
			{
				bParent = true;
			}
		}
		const bool bAll = bParent || (Fed && Now - *Fed < 5.0);
		Br.Phase += DeltaTime;
		const bool bPeek = Br.Count == 1 && Rest->Xf[Br.First].GetScale3D().IsNearlyZero() && Beg->Xf[Br.First].GetScale3D().IsNearlyZero();
		for (int32 I = 0; I < Br.Count; I++)
		{
			const float Own = FMath::Sin(Br.Phase * 0.35f + I * 2.1f);
			const bool bGape = bAll || Own > 0.97f;
			FTransform B = Br.Base[I];
			if (bGape)
			{
				// the neck up and swaying, quick little bobs
				const float T = Br.Phase * 11.f + I * 1.7f;
				B.AddToTranslation(FVector(0.0, 0.0, FMath::Abs(FMath::Sin(T)) * 0.8));
				B.ConcatenateRotation(FRotator(0.f, FMath::Sin(T * 0.37f) * 12.f, 0.f).Quaternion());
			}
			const int32 Idx = Br.First + I;
			const FTransform RestX = bGape || (bPeek && !bGape) ? Hidden : Br.Base[I];
			const FTransform BegX = bGape ? B : Hidden;
			if (!RestX.Equals(Rest->Xf[Idx], 0.01))
			{
				Rest->ISM->UpdateInstanceTransform(Idx, RestX, true, false, true);
				Rest->Xf[Idx] = RestX;
			}
			if (!BegX.Equals(Beg->Xf[Idx], 0.01))
			{
				Beg->ISM->UpdateInstanceTransform(Idx, BegX, true, false, true);
				Beg->Xf[Idx] = BegX;
			}
		}
	}
	// keep the static hashes as they were built (the animation is in place): the next BuildStatic compares against what it
	// wanted, not what is shown, so a brood's sets are not rebuilt just because a chick gaped
}

void ABirdNestRenderer::DrawMovers(FWorld& World, const FVector& Camera)
{
	FNesting& N = *World.Nesting();
	TMap<FName, TArray<FTransform>> Want;
	const V3 Cam = FromUE(Camera);
	// the mesh names made once (this runs every frame: a Printf and an FName per carrying bird or chick was the cost)
	static const FName MatMeshes[] = { NAME_None, FName(TEXT("SM_NestMat_Twigs")), FName(TEXT("SM_NestMat_Twigs")), FName(TEXT("SM_NestMat_Grass")),
		FName(TEXT("SM_NestMat_Moss")), FName(TEXT("SM_NestMat_Mud")), FName(TEXT("SM_NestMat_Reed")), FName(TEXT("SM_NestMat_Feathers")) };
	static TMap<int32, FName> ChickMeshes;     // by species, age and gape
	for (const FBird& B : World.Birds)
	{
		if (B.bGone || B.Act == EAct::Claimed)
		{
			continue;
		}
		const float Dist = (B.Pos - Cam).Len();
		if (Dist > 80.f)
		{
			continue;
		}
		const FSpecies& S = SpeciesTable()[B.Species];
		// the downy young of swans and gulls, after their mother
		if (N.DownyYoung(B.Id))
		{
			const FNest& Nest = N.Nests[N.YoungOf(B.Id)];
			const FBreedKind& K = BreedTable()[Nest.Kind];
			const float Down = FCStringAnsi::Strcmp(K.Id, "MuteSwan") == 0 ? 70.f : 25.f;
			const int32 AgeIdx = Nest.State.Age < Down * 0.25f ? 0 : (Nest.State.Age < Down * 0.65f ? 1 : 2);
			const bool bBeg = B.Act == EAct::Peck;
			FName& Mesh = ChickMeshes.FindOrAdd(B.Species * 8 + AgeIdx * 2 + (bBeg ? 1 : 0));
			if (Mesh.IsNone())
			{
				static const TCHAR* const Ages[] = { TEXT("Hatch"), TEXT("Mid"), TEXT("Late") };
				Mesh = FName(*FString::Printf(TEXT("SM_Chick_%s_%s_%s"), UTF8_TO_TCHAR(S.Id.c_str()), Ages[AgeIdx], bBeg ? TEXT("Beg") : TEXT("Rest")));
			}
			if (SetFor(Mesh, true))
			{
				Want.FindOrAdd(Mesh).Add(FTransform(FRotator(0.f, FMath::RadiansToDegrees(B.Yaw) + KitYaw, 0.f), ToUE(World.RenderPos(B))));
			}
			continue;
		}
		// a beakful of nest stuff
		const EMaterial M = N.Carrying(B.Id);
		if (M != EMaterial::None && Dist < 60.f)
		{
			const FName Mesh = MatMeshes[(int32)M];
			if (SetFor(Mesh, true))
			{
				const V3 Fwd(FMath::Cos(B.Yaw), FMath::Sin(B.Yaw), 0.f);
				const V3 P = World.RenderPos(B) + Fwd * (S.Length * 0.5f) + V3(0.f, 0.f, IsAirborne(B.Act) ? 0.f : S.Length * 0.3f);
				Want.FindOrAdd(Mesh).Add(FTransform(FRotator(0.f, FMath::RadiansToDegrees(B.Yaw), 0.f), ToUE(P), FVector(S.Length / 0.2f)));
			}
		}
	}
	for (TPair<FName, FDrawSet>& P : Sets)
	{
		if (!P.Value.bMover)
		{
			continue;
		}
		TArray<FTransform>* W = Want.Find(P.Key);
		if (!W && P.Value.Xf.Num() == 0)
		{
			continue;            // nothing to show, nothing shown
		}
		TArray<FTransform> Empty;
		Commit(P.Value, W ? *W : Empty, false);
	}
}

FString ABirdNestRenderer::Describe() const
{
	int32 Meshes = 0;
	for (const TPair<FName, FDrawSet>& P : Sets)
	{
		Meshes += P.Value.Xf.Num() > 0 ? 1 : 0;
	}
	return Nearest + TEXT("; ") + FString::Printf(TEXT("nest drawing: %d instances of %d meshes, %d broods near, %d placed (wood %d, wall %d, ground %d, left as guessed %d), %d rebuilds, %d meshes missing"),
		InstancesLastUpdate, Meshes, Broods.Num(), Placed, PlacedOn[0], PlacedOn[1], PlacedOn[2], PlacedOn[3], Rebuilds, Missing.Num());
}
