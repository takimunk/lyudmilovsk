#include "Terrain/LyudTerrain.h"

#include "TerrainGen.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#endif

namespace
{
	const TCHAR* const MaskPath = TEXT("/Game/Scene01/Terrain/DA_TerrainMask.DA_TerrainMask");
	// cm per landscape height unit step: local height range is +-256 * ZScale cm (+-128 m)
	constexpr float LandscapeZScale = 50.f;
	constexpr int32 MaskStep = 2;          // mask cell = 2 landscape quads
}

ULyudTerrainMask* ULyudTerrainMask::Get()
{
	static TWeakObjectPtr<ULyudTerrainMask> Cached;
	if (!Cached.IsValid())
	{
		Cached = LoadObject<ULyudTerrainMask>(nullptr, MaskPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	return Cached.Get();
}

float ULyudTerrainMask::Sample(const TArray<uint8>& Data, const FVector& WorldLocation) const
{
	if (Width < 2 || Height < 2 || Data.Num() != Width * Height)
	{
		return 0.f;
	}
	const float U = (WorldLocation.X - Origin.X) / CellSize, V = (WorldLocation.Y - Origin.Y) / CellSize;
	if (U < 0.f || V < 0.f || U > Width - 1 || V > Height - 1)
	{
		return 0.f;
	}
	const int32 X = FMath::Min(int32(U), Width - 2), Y = FMath::Min(int32(V), Height - 2);
	const float FX = U - X, FY = V - Y;
	const uint8* Row = &Data[Y * Width + X];
	const float A = FMath::Lerp(float(Row[0]), float(Row[1]), FX);
	const float B = FMath::Lerp(float(Row[Width]), float(Row[Width + 1]), FX);
	return FMath::Lerp(A, B, FY) / 255.f;
}

#if WITH_EDITOR

ULandscapeLayerInfoObject* ULyudTerrainLibrary::CreateLayerInfo(const FString& PackagePath, FName LayerName)
{
	UPackage* Package = CreatePackage(*PackagePath);
	const FName ObjName(*FPackageName::GetShortName(PackagePath));
	ULandscapeLayerInfoObject* Info = NewObject<ULandscapeLayerInfoObject>(Package, ObjName, RF_Public | RF_Standalone | RF_Transactional);
	Info->SetLayerName(LayerName, false);
	Package->MarkPackageDirty();
	return Info;
}

TArray<FName> ULyudTerrainLibrary::GetLayerNames()
{
	TArray<FName> Out;
	for (int32 L = 0; L < LyudTerrain::NumLayers; ++L)
	{
		Out.Add(FName(LyudTerrain::LayerNames[L]));
	}
	return Out;
}

ULyudTerrainMask* ULyudTerrainLibrary::CreateMask(const FString& PackagePath)
{
	UPackage* Package = CreatePackage(*PackagePath);
	ULyudTerrainMask* Mask = NewObject<ULyudTerrainMask>(Package, FName(*FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
	Package->MarkPackageDirty();
	return Mask;
}

ALandscape* ULyudTerrainLibrary::BuildTerrain(UObject* WorldContextObject, const FString& InputDir, FVector Origin, UMaterialInterface* Material,
                                              const TArray<ULandscapeLayerInfoObject*>& Layers, ULyudTerrainMask* Mask, FString& Report)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || Layers.Num() != LyudTerrain::NumLayers)
	{
		Report = TEXT("need a world and one layer info per surface layer");
		return nullptr;
	}
	const double T0 = FPlatformTime::Seconds();
	LyudTerrain::FInputs In;
	const std::string Err = LyudTerrain::ReadInputs(TCHAR_TO_UTF8(*InputDir), In);
	if (!Err.empty())
	{
		Report = UTF8_TO_TCHAR(Err.c_str());
		return nullptr;
	}
	LyudTerrain::FOutputs Out;
	LyudTerrain::Generate(In, Out);
	LyudTerrain::WriteHeights(TCHAR_TO_UTF8(*InputDir), Out);
	Report = FString::Printf(TEXT("%hs\ngenerated in %.1f s"), Out.Report.c_str(), FPlatformTime::Seconds() - T0);

	const int32 NX = In.NX, NY = In.NY;
	const int32 SubsectionQuads = 63, Subsections = 2, ComponentQuads = SubsectionQuads * Subsections;
	if ((NX - 1) % ComponentQuads || (NY - 1) % ComponentQuads)
	{
		Report += TEXT("\ngrid is not a whole number of components");
		return nullptr;
	}

	TArray<uint16> Heights;
	Heights.SetNumUninitialized(NX * NY);
	for (int32 I = 0; I < NX * NY; ++I)
	{
		Heights[I] = LandscapeDataAccess::GetTexHeight(Out.Height[I] * 100.f / LandscapeZScale);
	}
	TArray<FLandscapeImportLayerInfo> LayerData;
	for (int32 L = 0; L < LyudTerrain::NumLayers; ++L)
	{
		FLandscapeImportLayerInfo& Info = LayerData.Emplace_GetRef(Layers[L]->GetLayerName());
		Info.LayerInfo = Layers[L];
		Info.LayerData = TArray<uint8>(Out.Weights[L].data(), NX * NY);
	}
	{
		// the portal hole: the visibility layer cuts the quad whose corner vertices carry it
		FLandscapeImportLayerInfo& Hole = LayerData.Emplace_GetRef(ALandscapeProxy::VisibilityLayer->GetLayerName());
		Hole.LayerInfo = ALandscapeProxy::VisibilityLayer;
		Hole.LayerData = TArray<uint8>(In.Hole.data(), NX * NY);
	}
	TMap<FGuid, TArray<uint16>> HeightPerLayer;
	HeightPerLayer.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayersPerLayer;
	LayersPerLayer.Add(FGuid(), MoveTemp(LayerData));

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->PersistentLevel, ALandscape::StaticClass(), TEXT("Ground"));
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Origin, FRotator::ZeroRotator, Params);
	Landscape->SetActorLabel(TEXT("Ground"));
	Landscape->SetActorRelativeScale3D(FVector(100.f, 100.f, LandscapeZScale));
	Landscape->LandscapeMaterial = Material;
	Landscape->StaticLightingLOD = 2;
	// the outdoor sun lights only channel 1; channel 0 keeps Lumen and the sky lighting it
	Landscape->LightingChannels.bChannel0 = true;
	Landscape->LightingChannels.bChannel1 = true;
	Landscape->Import(FGuid::NewGuid(), 0, 0, NX - 1, NY - 1, Subsections, SubsectionQuads, HeightPerLayer, nullptr, LayersPerLayer,
	                  ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (Info)
	{
		Info->UpdateLayerInfoMap(Landscape);
	}
	for (ULandscapeLayerInfoObject* Layer : Layers)
	{
		Landscape->AddTargetLayer(Layer->GetLayerName(), FLandscapeTargetLayerSettings(Layer));
		if (Info)
		{
			const int32 Index = Info->GetLayerInfoIndex(Layer->GetLayerName());
			if (Info->Layers.IsValidIndex(Index))
			{
				Info->Layers[Index].LayerInfoObj = Layer;
			}
		}
	}

	// the collision's dominant layers (and so its hole) once every layer is known to the landscape
	int32 HoleComponents = 0;
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		for (const FWeightmapLayerAllocationInfo& Alloc : Component->GetWeightmapLayerAllocations())
		{
			if (Alloc.LayerInfo == ALandscapeProxy::VisibilityLayer)
			{
				++HoleComponents;
				Component->UpdateCollisionLayerData();
				if (ULandscapeHeightfieldCollisionComponent* Collision = Component->GetCollisionComponent())
				{
					Collision->RecreateCollision();
				}
			}
		}
	}
	Report += FString::Printf(TEXT("\ncomponents with the portal hole: %d (an editor world traces them whole, the game cuts the hole)"), HoleComponents);

	if (Mask)
	{
		const int32 W = (NX - 1) / MaskStep + 1, H = (NY - 1) / MaskStep + 1;
		Mask->Modify();
		Mask->Origin = FVector2D(Origin.X, Origin.Y);
		Mask->CellSize = 100.f * MaskStep;
		Mask->Width = W;
		Mask->Height = H;
		Mask->Grass.SetNumUninitialized(W * H);
		Mask->DuneGrass.SetNumUninitialized(W * H);
		Mask->Wetness.SetNumUninitialized(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const size_t I = size_t(Y * MaskStep) * NX + X * MaskStep;
				Mask->Grass[Y * W + X] = Out.GrassDensity[I];
				Mask->DuneGrass[Y * W + X] = Out.DuneGrass[I];
				Mask->Wetness[Y * W + X] = Out.Wetness[I];
			}
		}
		Mask->MarkPackageDirty();
	}
	Report += FString::Printf(TEXT("\nlandscape %d x %d vertices, %d components, built in %.1f s"), NX, NY,
	                          ((NX - 1) / ComponentQuads) * ((NY - 1) / ComponentQuads), FPlatformTime::Seconds() - T0);
	return Landscape;
}

#endif
