#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyudAnimTools.generated.h"

class USkeleton;

/**
 * Small animation helpers for editor Python (unreal.LyudAnimTools). The Skeleton's bone tree is
 * read-only from Python, so the translation retargeting mode of a bone is set here; used by
 * unreal/scripts/life_retarget.py so the shared A_Human_* clips fit every human body.
 */
UCLASS()
class LYUDMILOVSK_API ULyudAnimTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Mode: 0 Animation, 1 Skeleton, 2 AnimationScaled, 3 AnimationRelative, 4 OrientAndScale.
	 *  Returns false when the bone is not in the skeleton. Marks the skeleton dirty (save it after). */
	UFUNCTION(BlueprintCallable, Category = "Lyudmilovsk|Animation")
	static bool SetBoneTranslationRetargeting(USkeleton* Skeleton, FName Bone, int32 Mode, bool bChildrenToo);

	/** The bone's current mode (as above), or -1 when the bone is missing. */
	UFUNCTION(BlueprintCallable, Category = "Lyudmilovsk|Animation")
	static int32 GetBoneTranslationRetargeting(USkeleton* Skeleton, FName Bone);
};
