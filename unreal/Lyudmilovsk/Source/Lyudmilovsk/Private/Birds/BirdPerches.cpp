#include "BirdPerches.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PhysicsVolume.h"
#include "Plants/GameCalendar.h"
#include "Plants/PlantModel.h"
#include "Plants/PlantSpecies.h"
#include "Plants/ProceduralForest.h"
#include "Plants/ProceduralPlant.h"
#include "Sea/LyudSea.h"
#include "Transport/LyudCableway.h"

namespace
{
	constexpr float Cell = 2000.f;
	FIntPoint CellOf(const FVector& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
	/** A tree's base rounded to 10 cm: a forest tree and the live plant standing in for it share it. */
	FIntVector BaseKey(const FVector& P) { return FIntVector(FMath::RoundToInt(P.X / 10.f), FMath::RoundToInt(P.Y / 10.f), FMath::RoundToInt(P.Z / 10.f)); }

	float DistToSegment(const FVector& P, const FVector& A, const FVector& B, float& OutT)
	{
		const FVector AB = B - A;
		const float L2 = AB.SizeSquared();
		OutT = L2 > 1e-6f ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector::Dist(P, A + AB * OutT);
	}
}

void FBirdPerches::BuildCrowns(UWorld* World)
{
	bBuilt = true;
	Trees.Reset();
	Grid.Reset();
	const float Day = UGameCalendar::NowIn(World).GetDayOfYear();
	TMap<TPair<FName, int32>, FPlantSummary> Cache;
	TSet<FIntVector> Seen;
	auto Add = [&](FName Species, uint32 Seed, float Age, const FTransform& Xf, const FPlantSummary& S)
	{
		const FVector Base = Xf.GetLocation();
		if (S.Height < 2.f || S.CrownRadius < 0.5f || Seen.Contains(BaseKey(Base)))
		{
			return;
		}
		Seen.Add(BaseKey(Base));
		const float Scale = (float)Xf.GetScale3D().Z;
		const float Half = FMath::Max(0.5f, (S.Height - S.CrownBaseHeight) * 0.5f) * Scale * 100.f;
		FTree T;
		T.Species = Species;
		T.Seed = Seed;
		T.Age = Age;
		T.Xf = Xf;
		T.Centre = FVector3f(Base + FVector(0, 0, S.Height * Scale * 100.f - Half));
		T.Radii = FVector2f(S.CrownRadius * Scale * 100.f, Half);
		Grid.FindOrAdd(CellOf(Base)).Add(Trees.Add(T));
	};
	// the live plants first (they are what is drawn near the camera), then the forests' baked trees
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		Add(It->Species, (uint32)It->Seed, It->GetAge(), It->GetActorTransform(), It->GetSummary());
	}
	for (TActorIterator<AProceduralForest> It(World); It; ++It)
	{
		It->ForEachTreeSeeded([&](FName Species, uint32 Seed, const FTransform& Xf, float Age)
		{
			const FPlantSpecies* Sp = PlantSpeciesLibrary::Find(Species);
			if (!Sp) return;
			const TPair<FName, int32> Key(Species, FMath::RoundToInt(Age));
			const FPlantSummary* S = Cache.Find(Key);
			if (!S) S = &Cache.Add(Key, PlantModel::Summary(*Sp, Seed, Age, Day));
			Add(Species, Seed, Age, Xf, *S);
		});
	}
	// the cable car's perches (pylons' crossarms, vestibules' railings and roofs): points on their rails
	for (TActorIterator<ALyudCableway> It(World); It; ++It)
	{
		for (const FVector& P : It->BirdPerches)
		{
			FTree T;
			T.bPoint = true;
			T.Centre = FVector3f(P);
			T.Radii = FVector2f(1.f, 1.f);
			Grid.FindOrAdd(CellOf(P)).Add(Trees.Add(T));
		}
	}
}

const TArray<FBirdPerches::FSeg>& FBirdPerches::Branches(UWorld* World, const FTree& T)
{
	const uint64 Key = HashCombine(HashCombine(GetTypeHash(T.Species), T.Seed), HashCombine(GetTypeHash(BaseKey(T.Xf.GetLocation())), FMath::RoundToInt(T.Age * 20.f)));
	if (const TArray<FSeg>* Found = BranchCache.Find(Key))
	{
		return *Found;
	}
	TArray<FSeg>& Out = BranchCache.Add(Key);
	const FPlantSpecies* Sp = PlantSpeciesLibrary::Find(T.Species);
	if (!Sp)
	{
		return Out;
	}
	// the same skeleton, age and season the tree's wood is drawn from (AProceduralPlant, PlantMeshes::PrepareTreeBake)
	FPlantSkeleton Skeleton;
	Skeleton.Build(*Sp, T.Seed);
	FPlantSkeleton::FInstances Inst;
	Skeleton.Evaluate(T.Age, PlantModel::Season(*Sp, T.Seed, UGameCalendar::NowIn(World).GetDayOfYear()), Inst);
	const float RScale = 100.f * (float)T.Xf.GetScale3D().Z;
	for (const FPlantSkeleton::FTube& Tube : Inst.Tubes)
	{
		const int32 N = FMath::Min(Tube.P.Num(), Tube.R.Num());
		for (int32 I = 0; I + 1 < N; I++)
		{
			FSeg S;
			S.A = FVector3f(T.Xf.TransformPosition(FVector(Tube.P[I]) * 100.f));
			S.B = FVector3f(T.Xf.TransformPosition(FVector(Tube.P[I + 1]) * 100.f));
			S.RA = Tube.R[I] * RScale;
			S.RB = Tube.R[I + 1] * RScale;
			S.Order = (uint8)FMath::Clamp(Tube.Order, 0, 255);
			S.Along = (I + 0.5f) / FMath::Max(1, N - 1);
			Out.Add(S);
		}
	}
	return Out;
}

void FBirdPerches::ForEachBranch(UWorld* World, const FVector& At, float Radius, TFunctionRef<void(const FVector&, const FVector&, float, float)> Fn)
{
	if (!bBuilt)
	{
		BuildCrowns(World);
	}
	for (const FTree& T : Trees)
	{
		if (!T.bPoint && FVector::Dist2D(FVector(T.Centre), At) < Radius + T.Radii.X)
		{
			for (const FSeg& S : Branches(World, T))
			{
				Fn(FVector(S.A), FVector(S.B), S.RA, S.RB);
			}
		}
	}
}

bool FBirdPerches::Find(UWorld* World, const FVector& From, const FVector& Dir, float MaxDist, const AActor* Ignore, FPerch& Out)
{
	if (!World)
	{
		return false;
	}
	if (!bBuilt)
	{
		BuildCrowns(World);
	}
	const FVector Look = Dir.GetSafeNormal();
	float Best = 1e20f;
	// how good a perch is: near, ahead of the bird, and above the ground (a branch or a roof beats the grass)
	auto Score = [&](const FVector& P, bool bHigh)
	{
		const FVector To = P - From;
		const float D = To.Size();
		if (D > MaxDist || D < 30.f) return 1e20f;
		const float Ahead = FVector::DotProduct(To / D, Look);
		if (Ahead < 0.2f) return 1e20f;
		return D * (1.8f - Ahead) + (bHigh ? 0.f : 1500.f);
	};
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BirdPerch), false, Ignore);
	if (ALyudSea* Sea = ALyudSea::Get(World))
	{
		Q.AddIgnoredActor(Sea);
		if (AActor* Vol = Sea->GetWaterVolume()) Q.AddIgnoredActor(Vol);
	}

	// the trees near and ahead: their crowns first (cheap), then the branches of the few best
	const FIntPoint C0 = CellOf(From);
	const int32 R = FMath::CeilToInt(MaxDist / Cell) + 1;
	TArray<TPair<float, int32>> Near;
	for (int32 Y = C0.Y - R; Y <= C0.Y + R; Y++)
	{
		for (int32 X = C0.X - R; X <= C0.X + R; X++)
		{
			const TArray<int32>* List = Grid.Find(FIntPoint(X, Y));
			if (!List) continue;
			for (const int32 Ti : *List)
			{
				const FTree& T = Trees[Ti];
				if (T.bPoint)
				{
					// a rail or a crossarm: only if there is something under the point
					FHitResult Hit;
					const FVector P(T.Centre);
					if (World->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 20.f), P - FVector(0, 0, 40.f), ECC_Visibility, Q) && Hit.ImpactNormal.Z > 0.6f)
					{
						const float S = Score(Hit.ImpactPoint, true);
						if (S < Best)
						{
							Best = S;
							Out = FPerch();
							Out.Point = Hit.ImpactPoint;
						}
					}
					continue;
				}
				// the nearest point of the crown's ellipsoid, roughly
				const FVector Centre(T.Centre);
				FVector D = From - Centre;
				const FVector Edge = Centre + FVector(D.X, D.Y, 0.f).GetClampedToMaxSize(T.Radii.X) + FVector(0, 0, FMath::Clamp((float)D.Z, -T.Radii.Y, T.Radii.Y));
				const float S = Score(Edge, true);
				if (S < 1e19f || FVector::Dist(From, Centre) < T.Radii.X + 300.f)
				{
					Near.Add({ FMath::Min(S, FVector::Dist(From, Edge)), Ti });
				}
			}
		}
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });

	// a branch: a limb (or the inner part of a twig, which hardly sways) the feet can close round, near level, with
	// room above it for the body, not in the trunk's shadow of the bird
	struct FCand { float Score; int32 Tree; int32 Seg; FVector Feet; FVector Axis; FVector Dir; float Radius; };
	TArray<FCand> Cands;
	// a tree near the camera is drawn by a live plant of its own age (it grows with the calendar): perch on that one
	TMap<FIntVector, float> LiveAge;
	for (TActorIterator<AProceduralPlant> It(World); It; ++It)
	{
		LiveAge.Add(BaseKey(It->GetActorLocation()), It->GetAge());
	}
	for (int32 K = 0; K < FMath::Min(Near.Num(), 4); K++)
	{
		FTree& T = Trees[Near[K].Value];
		if (const float* Age = LiveAge.Find(BaseKey(T.Xf.GetLocation())))
		{
			T.Age = *Age;
		}
		const TArray<FSeg>& Segs = Branches(World, T);
		const FVector Base = T.Xf.GetLocation();
		for (int32 Si = 0; Si < Segs.Num(); Si++)
		{
			const FSeg& S = Segs[Si];
			if (S.Order == 0 || (S.Order >= 2 && S.Along > 0.35f)) continue;
			const float Rad = 0.5f * (S.RA + S.RB);
			if (Rad < MinRadius || Rad > MaxRadius) continue;
			const FVector A(S.A), B(S.B);
			const FVector Along = (B - A).GetSafeNormal();
			if (FMath::Abs(Along.Z) > 0.3f) continue;
			const FVector Axis = (A + B) * 0.5f;
			const FVector Up = (FVector::UpVector - Along * Along.Z).GetSafeNormal();
			const FVector Feet = Axis + Up * Rad;
			float Sc = Score(Feet, true);
			if (Sc > 1e19f) continue;
			// the outer branches first: the bird comes in from outside the crown
			const float Out01 = FMath::Clamp(FVector::Dist2D(Feet, Base) / FMath::Max(T.Radii.X, 50.f), 0.f, 1.f);
			Sc += (1.f - Out01) * 600.f;
			Cands.Add({ Sc, Near[K].Value, Si, Feet, Axis, Along, Rad });
		}
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.Score < B.Score; });
	int32 Tested = 0;
	for (const FCand& C : Cands)
	{
		if (C.Score >= Best || ++Tested > 40) break;
		// room: the body (a sphere StandHeight above the feet) and the feet themselves clear of every other piece of wood
		const FVector BodyAt = C.Feet + FVector(0, 0, StandHeight);
		bool bClear = true;
		for (const FSeg& S : Branches(World, Trees[C.Tree]))
		{
			float U = 0.f;
			const float DBody = DistToSegment(BodyAt, FVector(S.A), FVector(S.B), U) - FMath::Lerp(S.RA, S.RB, U);
			if (DBody < BodyRadius * 1.1f)
			{
				bClear = false;
				break;
			}
			const float DFeet = DistToSegment(C.Feet + FVector(0, 0, 1.f), FVector(S.A), FVector(S.B), U) - FMath::Lerp(S.RA, S.RB, U);
			if (DFeet < 0.f)
			{
				bClear = false;
				break;
			}
		}
		if (!bClear) continue;
		Best = C.Score;
		Out = FPerch();
		Out.Point = C.Feet;
		Out.bBranch = true;
		Out.Axis = C.Axis;
		Out.Dir = C.Dir;
		Out.Radius = C.Radius;
		break;
	}

	// roofs, walls, rails, the ground: traces down from a fan of points ahead
	const FVector Fwd = FVector(Look.X, Look.Y, 0.f).GetSafeNormal();
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	for (int32 I = 1; I <= 5; I++)
	{
		for (int32 J = -3; J <= 3; J++)
		{
			const FVector Top = From + Fwd * (MaxDist * I / 6.f) + Right * (MaxDist * J / 9.f) + FVector(0, 0, 800.f);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Top, Top - FVector(0, 0, 6000.f), ECC_Visibility, Q) && Hit.ImpactNormal.Z > 0.75f)
			{
				// "high": well above the ground just beside it (a roof, a wall top)
				FHitResult Low;
				const FVector Beside = Hit.ImpactPoint + Right * 250.f;
				const bool bHigh = World->LineTraceSingleByChannel(Low, Beside + FVector(0, 0, 50.f), Beside - FVector(0, 0, 3000.f), ECC_Visibility, Q)
					&& Hit.ImpactPoint.Z - Low.ImpactPoint.Z > 150.f;
				const float S = Score(Hit.ImpactPoint, bHigh);
				if (S < Best)
				{
					Best = S;
					Out = FPerch();
					Out.Point = Hit.ImpactPoint;
				}
			}
		}
	}
	return Best < 1e19f;
}
