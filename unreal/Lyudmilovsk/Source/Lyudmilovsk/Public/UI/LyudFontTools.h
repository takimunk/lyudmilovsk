#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyudFontTools.generated.h"

class UFontFace;

/**
 * Font import for editor Python (unreal.LyudFontTools). The .ttf factory ends in UFontFace::PostEditChangeProperty, which
 * flushes Slate's font cache and so asserts in a commandlet; this fills a new font face from the file without it.
 * Used by unreal/scripts/import_diary_assets.py.
 */
UCLASS()
class LYUDMILOVSK_API ULyudFontTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A font face at PackagePath (/Game/.../FF_Name) holding the font file inline (replaces one already there); null when
	 *  the file can't be read. The package is left dirty: save it after. */
	UFUNCTION(BlueprintCallable, Category = "Lyudmilovsk|UI")
	static UFontFace* MakeFontFace(const FString& FontFile, const FString& PackagePath);
};
