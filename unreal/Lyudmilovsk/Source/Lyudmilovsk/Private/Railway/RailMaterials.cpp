#include "RailMaterials.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace LyudRailMat
{
namespace
{
const TCHAR* PaintPath = TEXT("/Game/Transport/Materials/M_TR_Paint.M_TR_Paint");
const TCHAR* GlassPath = TEXT("/Game/Transport/Materials/M_TR_Glass.M_TR_Glass");
const TCHAR* GlowPath = TEXT("/Game/Transport/Materials/M_TR_Glow.M_TR_Glow");
const TCHAR* SurfacePath = TEXT("/Game/Transport/Materials/M_TR_Surface.M_TR_Surface");

struct FPaint { FLinearColor Color; float Rough; float Metal; };

const TMap<FName, FPaint>& Palette()
{
	static const TMap<FName, FPaint> P = {
		{ TEXT("ballast"), { FLinearColor(0.19f, 0.18f, 0.165f), 0.95f, 0.f } },
		{ TEXT("stone"), { FLinearColor(0.16f, 0.15f, 0.14f), 0.9f, 0.f } },
		{ TEXT("weed"), { FLinearColor(0.07f, 0.09f, 0.04f), 0.7f, 0.f } },
		{ TEXT("rail"), { FLinearColor(0.32f, 0.30f, 0.29f), 0.35f, 0.9f } },
		{ TEXT("rust"), { FLinearColor(0.20f, 0.09f, 0.05f), 0.8f, 0.2f } },
		{ TEXT("wire"), { FLinearColor(0.12f, 0.10f, 0.08f), 0.5f, 0.7f } },
		{ TEXT("wood"), { FLinearColor(0.28f, 0.17f, 0.09f), 0.85f, 0.f } },
		{ TEXT("wood_grey"), { FLinearColor(0.30f, 0.28f, 0.25f), 0.9f, 0.f } },
		{ TEXT("pile"), { FLinearColor(0.12f, 0.10f, 0.08f), 0.8f, 0.f } },
		{ TEXT("white"), { FLinearColor(0.85f, 0.85f, 0.82f), 0.6f, 0.f } },
		{ TEXT("yellow"), { FLinearColor(0.85f, 0.62f, 0.02f), 0.6f, 0.f } },
		{ TEXT("black"), { FLinearColor(0.015f, 0.015f, 0.015f), 0.7f, 0.f } },
		{ TEXT("rubber"), { FLinearColor(0.03f, 0.03f, 0.03f), 0.9f, 0.f } },
		{ TEXT("dark"), { FLinearColor(0.006f, 0.006f, 0.006f), 1.f, 0.f } },
		{ TEXT("cream"), { FLinearColor(0.80f, 0.72f, 0.52f), 0.5f, 0.f } },
		{ TEXT("green"), { FLinearColor(0.02f, 0.08f, 0.04f), 0.5f, 0.f } },
		{ TEXT("concrete_old"), { FLinearColor(0.36f, 0.35f, 0.32f), 0.95f, 0.f } },
	};
	return P;
}
}

UMaterialInstanceDynamic* Glow(UObject* Owner, const FLinearColor& Color, float Intensity)
{
	UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlowPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!G)
	{
		G = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(G, Owner);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	return M;
}

UMaterialInterface* Get(UObject* Owner, FName Name, TMap<FName, TObjectPtr<UMaterialInterface>>& Cache)
{
	if (TObjectPtr<UMaterialInterface>* Found = Cache.Find(Name))
	{
		return *Found;
	}
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInterface* Out = nullptr;
	const FString S = Name.ToString();
	if (S == TEXT("glass"))
	{
		UMaterialInterface* G = LoadObject<UMaterialInterface>(nullptr, GlassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(G ? G : Basic, Owner);
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.62f, 0.66f));
		Out = M;
	}
	else if (S.StartsWith(TEXT("glow")))
	{
		const FLinearColor C = S == TEXT("glow_red") ? FLinearColor(1.f, 0.05f, 0.02f)
			: (S == TEXT("glow_white") ? FLinearColor(1.f, 0.95f, 0.85f) : FLinearColor(1.f, 0.72f, 0.38f));
		Out = Glow(Owner, C, S == TEXT("glow_red") ? 20.f : 4.f);
	}
	else if (S.StartsWith(TEXT("tx_")))
	{
		const FString Key = S.Mid(3);
		UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, SurfacePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		// the railway's own surfaces (tools/railway_textures.py -> /Game/Railway/Textures, metres per tile), else the
		// cable car's (/Game/Transport/Textures)
		static const TMap<FString, float> RailTiles = {
			{ TEXT("ballast"), 0.8f }, { TEXT("armour"), 2.4f }, { TEXT("planks"), 1.6f }, { TEXT("planks_dark"), 1.6f },
			{ TEXT("rubber"), 1.2f }, { TEXT("tunnel"), 3.f }, { TEXT("wetsand"), 1.5f }, { TEXT("turf"), 1.8f }, { TEXT("lichen"), 2.f },
		};
		const float* RailTile = RailTiles.Find(Key);
		const FString TexBase = Key == TEXT("tunnel") && !RailTile ? TEXT("concrete") : Key;
		auto Tex = [&](const TCHAR* Suffix) -> UTexture*
		{
			if (RailTile)
			{
				const FString N = FString::Printf(TEXT("T_RL_%s_%s"), *Key, Suffix);
				if (UTexture* T = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Railway/Textures/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					return T;
				}
			}
			const FString N = FString::Printf(TEXT("T_TR_%s_%s"), *(Key == TEXT("tunnel") ? FString(TEXT("concrete")) : TexBase), Suffix);
			return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Transport/Textures/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		};
		UTexture* C = Tex(TEXT("C"));
		if (Master && C)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Master, Owner);
			M->SetTextureParameterValue(TEXT("BaseTex"), C);
			if (UTexture* N = Tex(TEXT("N")))
			{
				M->SetTextureParameterValue(TEXT("NormalTex"), N);
			}
			if (UTexture* R = Tex(TEXT("R")))
			{
				M->SetTextureParameterValue(TEXT("RoughTex"), R);
			}
			// weathered sea-side concrete: greyer and darker than the stations', the tunnel's inside sooty
			const bool bOwn = RailTile && C->GetPathName().StartsWith(TEXT("/Game/Railway/"));
			const FLinearColor Tint = Key == TEXT("tunnel") ? (bOwn ? FLinearColor(0.55f, 0.53f, 0.5f) : FLinearColor(0.25f, 0.24f, 0.23f))
				: (Key == TEXT("concrete") ? FLinearColor(0.78f, 0.76f, 0.72f) : FLinearColor::White);
			M->SetVectorParameterValue(TEXT("Tint"), Tint);
			M->SetScalarParameterValue(TEXT("Tile"), bOwn ? *RailTile : (Key == TEXT("steel") ? 1.f : 2.4f));
			M->SetScalarParameterValue(TEXT("Metallic"), Key == TEXT("steel") ? 0.8f : 0.f);
			M->SetScalarParameterValue(TEXT("RoughMul"), 1.1f);
			Out = M;
		}
		else
		{
			return Get(Owner, TEXT("concrete_old"), Cache);
		}
	}
	else
	{
		UMaterialInterface* P = LoadObject<UMaterialInterface>(nullptr, PaintPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(P ? P : Basic, Owner);
		const FPaint* Found = Palette().Find(Name);
		const FPaint Pt = Found ? *Found : FPaint{ FLinearColor(0.5f, 0.5f, 0.5f), 0.6f, 0.f };
		M->SetVectorParameterValue(TEXT("Color"), Pt.Color);
		M->SetScalarParameterValue(TEXT("Roughness"), Pt.Rough);
		M->SetScalarParameterValue(TEXT("Metallic"), Pt.Metal);
		Out = M;
	}
	Cache.Add(Name, Out);
	return Out;
}

UStaticMesh* KitMesh(const TCHAR* Name)
{
	// Interchange puts a glTF scene's meshes in <folder>/<file>/StaticMeshes (or straight in the folder)
	for (const TCHAR* Dir : { TEXT("/Game/Railway/Meshes/rail_kit/StaticMeshes"), TEXT("/Game/Railway/Meshes/rail_kit") })
	{
		const FString P = FString::Printf(TEXT("%s/%s.%s"), Dir, Name, Name);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return M;
		}
	}
	return nullptr;
}
}
