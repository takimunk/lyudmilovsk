#include "Plants/TreeBakeCache.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Plants/PlantMeshes.h"
#include "Plants/PlantModel.h"
#include "Tasks/Task.h"

struct UTreeBakeCache::FJob
{
	FTreeBakeKey Key;
	float Age = 0.f;
	float Day = 0.f;
	FPlantSpecies Species;
	PlantMeshes::FTreePrep Prep;
	bool bRebake = false;
};

const FTreeBaked* UTreeBakeCache::Request(FName Species, uint32 Seed, float Age, float Day, float RebakeDays, const FVector& LodDistances)
{
	const FTreeBakeKey Key{ Species, Seed, FMath::RoundToInt(Age * 10.f) };
	FTreeBaked* Have = Baked.Find(Key);
	const bool bStale = Have && FMath::Abs(Day - Have->Day) > RebakeDays;
	if ((!Have || bStale) && !Queued.Contains(Key))
	{
		const FPlantSpecies* S = PlantSpeciesLibrary::Find(Species);
		if (!S) return nullptr;
		TSharedPtr<FJob> Job = MakeShared<FJob>();
		Job->Key = Key;
		Job->Age = FMath::RoundToFloat(Age * 10.f) / 10.f;
		Job->Day = Day;
		Job->Species = *S;
		Job->bRebake = bStale;
		Queued.Add(Key);
		if (!LodsOf.Contains(Key)) LodsOf.Add(Key, LodDistances);
		Wanted.Add(Job);
	}
	return Have;
}

void UTreeBakeCache::Launch(const TSharedPtr<FJob>& Job)
{
	Running++;
	RunningAtomic++;
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Job]()
	{
		Job->Prep = PlantMeshes::PrepareTreeBake(Job->Species, Job->Key.Seed, Job->Age, Job->Day);
		{
			FScopeLock Lock(&FinishedLock);
			Finished.Add(Job);
		}
		RunningAtomic--;
	});
}

void UTreeBakeCache::Finish(FJob& Job)
{
	const double T0 = FPlatformTime::Seconds();
	const FPlantSpecies& S = Job.Species;
	if (!BarkMaterial)
	{
		BarkMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Plants/M_PlantBark.M_PlantBark"));
		LeafMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Plants/M_PlantLeaf.M_PlantLeaf"));
		NeedleMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Plants/M_PlantNeedle.M_PlantNeedle"));
		UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!BarkMaterial) BarkMaterial = Basic;
		if (!LeafMaterial) LeafMaterial = Basic;
		if (!NeedleMaterial) NeedleMaterial = LeafMaterial;
	}
	FSpeciesAssets& A = Assets.FindOrAdd(S.Name);
	const double TexStart = FPlatformTime::Seconds();
	if (!A.Albedo)
	{
		// one bark and one leaf texture per species, whatever the seed
		UTexture2D *Albedo = nullptr, *Normal = nullptr;
		PlantMeshes::BuildBarkTextures(this, S, 1u, Albedo, Normal);
		A.Albedo = Albedo;
		A.Normal = Normal;
		A.Detail = PlantMeshes::BuildLeafDetail(this, S.Leaf);
		// the maps here are not reflected, so the garbage collector cannot see what they hold: keep it alive by hand
		KeepAlive.Add(A.Albedo);
		KeepAlive.Add(A.Normal);
		KeepAlive.Add(A.Detail);
		A.Card = PlantMeshes::BuildFoliageCard(this, S.Leaf);
		KeepAlive.Add(A.Card);
	}
	const double MeshStart = FPlatformTime::Seconds();
	const FVector* L = LodsOf.Find(Job.Key);
	const FVector Lods = L ? *L : FVector(45.0, 120.0, 300.0);
	PlantMeshes::FTreeBake Bake = PlantMeshes::FinishTreeBake(this, S, Job.Key.Seed, MoveTemp(Job.Prep), FVector3f(Lods));
	const double MeshMs = (FPlatformTime::Seconds() - MeshStart) * 1000.0;
	UE_LOG(LogTemp, Verbose, TEXT("TREEBAKE %s: prepare %.0f ms (worker), textures %.0f ms, mesh %.0f ms (%d/%d/%d/%d tris)"), *S.Name.ToString(), Job.Prep.Ms, (MeshStart - TexStart) * 1000.0, MeshMs, Bake.Triangles[0], Bake.Triangles[1], Bake.Triangles[2], Bake.Triangles[3]);
	KeepAlive.Add(Bake.Mesh); // an older bake stays alive while some component still draws it

	FTreeBaked& Out = Baked.FindOrAdd(Job.Key);
	if (!Out.Wood)
	{
		Out.Wood = UMaterialInstanceDynamic::Create(BarkMaterial, this);
		Out.Leaf = UMaterialInstanceDynamic::Create(S.Leaf.Form == EPlantLeafForm::Needle ? NeedleMaterial.Get() : LeafMaterial.Get(), this);
		KeepAlive.Add(Out.Wood);
		KeepAlive.Add(Out.Leaf);
		// from LOD2 on, leaves are cards cut out by their alpha: the needle material with the species' spray
		Out.Card = UMaterialInstanceDynamic::Create(NeedleMaterial, this);
		KeepAlive.Add(Out.Card);
		Out.Card->SetTextureParameterValue(TEXT("LeafDetail"), A.Card);
		Out.Card->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
		Out.Card->SetScalarParameterValue(TEXT("Flutter"), 0.f);
		Out.Wood->SetTextureParameterValue(TEXT("BarkAlbedo"), A.Albedo);
		Out.Wood->SetTextureParameterValue(TEXT("BarkNormal"), A.Normal);
		Out.Wood->SetVectorParameterValue(TEXT("Color"), S.Bark.RidgeColour);
		Out.Wood->SetVectorParameterValue(TEXT("TwigColour"), S.Bark.TwigColour);
		Out.Wood->SetVectorParameterValue(TEXT("BaseRidge"), S.Bark.BaseRidgeColour);
		Out.Wood->SetVectorParameterValue(TEXT("BaseFurrow"), S.Bark.BaseFurrowColour);
		Out.Wood->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
		Out.Leaf->SetTextureParameterValue(TEXT("LeafDetail"), A.Detail);
		Out.Leaf->SetScalarParameterValue(TEXT("SwayAmp"), 0.f);
		Out.Leaf->SetScalarParameterValue(TEXT("Flutter"), 0.f);
	}
	const FPlantSeason Season = PlantModel::Season(S, Job.Key.Seed, Job.Day);
	Out.Wood->SetScalarParameterValue(TEXT("Snow"), Season.Snow);
	Out.Leaf->SetVectorParameterValue(TEXT("Color"), Season.Colour[1]);
	Out.Card->SetVectorParameterValue(TEXT("Color"), Season.Colour[1]);
	Out.Mesh = Bake.Mesh;
	Out.Day = Job.Day;
	Out.Version++;
	for (int32 I = 0; I < 4; I++) Out.Triangles[I] = Bake.Triangles[I];
	Queued.Remove(Job.Key);

	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	FinishMs += Ms;
	PrepareMs += Job.Prep.Ms;
	WorstFinishMs = FMath::Max(WorstFinishMs, Ms);
	Done++;
	for (int32 I = 0; I < 4; I++) TotalTris[I] += Bake.Triangles[I];
	// the next one waits eight times as long as this took
	NextFinishTime = FPlatformTime::Seconds() + Ms * 8.0 * 0.001;
}

void UTreeBakeCache::Deinitialize()
{
	// the workers hold a pointer to this
	while (RunningAtomic.Load() > 0) FPlatformProcess::Sleep(0.002f);
	Super::Deinitialize();
}

void UTreeBakeCache::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Running = RunningAtomic.Load();
	while (Running < MaxWorkers && !Wanted.IsEmpty())
	{
		TSharedPtr<FJob> Job = Wanted[0];
		Wanted.RemoveAt(0);
		Launch(Job);
	}
	if (FPlatformTime::Seconds() < NextFinishTime) return;
	TSharedPtr<FJob> Job;
	{
		FScopeLock Lock(&FinishedLock);
		if (!Finished.IsEmpty())
		{
			Job = Finished[0];
			Finished.RemoveAt(0);
		}
	}
	if (Job) Finish(*Job);
}

void UTreeBakeCache::BakeAllNow()
{
	while (!Wanted.IsEmpty())
	{
		TSharedPtr<FJob> Job = Wanted[0];
		Wanted.RemoveAt(0);
		Job->Prep = PlantMeshes::PrepareTreeBake(Job->Species, Job->Key.Seed, Job->Age, Job->Day);
		Finish(*Job);
	}
	for (;;)
	{
		TSharedPtr<FJob> Job;
		{
			FScopeLock Lock(&FinishedLock);
			if (!Finished.IsEmpty()) { Job = Finished[0]; Finished.RemoveAt(0); }
		}
		if (!Job)
		{
			if (RunningAtomic.Load() == 0) break;
			FPlatformProcess::Sleep(0.002f);
			continue;
		}
		Finish(*Job);
	}
}

FString UTreeBakeCache::DescribeStats() const
{
	return FString::Printf(TEXT("%d kinds baked (%d queued or running); avg %.0f ms on a worker, %.0f ms on the game thread (worst %.0f); triangles in all kinds by LOD %dk/%dk/%dk/%dk"), Done, NumPending(),
		Done ? PrepareMs / Done : 0.0, Done ? FinishMs / Done : 0.0, WorstFinishMs, TotalTris[0] / 1000, TotalTris[1] / 1000, TotalTris[2] / 1000, TotalTris[3] / 1000);
}
