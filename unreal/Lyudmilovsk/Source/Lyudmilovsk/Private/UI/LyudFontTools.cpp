#include "UI/LyudFontTools.h"

#include "Engine/FontFace.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

UFontFace* ULyudFontTools::MakeFontFace(const FString& FontFile, const FString& PackagePath)
{
#if WITH_EDITORONLY_DATA
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *FontFile) || Data.Num() == 0)
	{
		return nullptr;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	Package->FullyLoad();
	const FName Name(*FPackageName::GetShortName(PackagePath));
	UFontFace* Face = FindObject<UFontFace>(Package, *Name.ToString());
	if (!Face)
	{
		Face = NewObject<UFontFace>(Package, Name, RF_Public | RF_Standalone);
	}
	else if (Face->GetFontFaceData()->HasData())
	{
		// imported before: a face takes its data only once (InitializeFromBulkData asserts on a second run)
		return Face;
	}
	Face->LoadingPolicy = EFontLoadingPolicy::Inline;
	static_cast<IFontFaceInterface*>(Face)->InitializeFromBulkData(FontFile, EFontHinting::Default, Data.GetData(), Data.Num());
	Face->CacheSubFaces();
	Package->MarkPackageDirty();
	return Face;
#else
	return nullptr;
#endif
}
