#include "LyudAnimTools.h"

#include "Animation/Skeleton.h"

bool ULyudAnimTools::SetBoneTranslationRetargeting(USkeleton* Skeleton, FName Bone, int32 Mode, bool bChildrenToo)
{
	if (!Skeleton || Mode < 0 || Mode > EBoneTranslationRetargetingMode::OrientAndScale)
	{
		return false;
	}
	const int32 Index = Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Skeleton->Modify();
	Skeleton->SetBoneTranslationRetargetingMode(Index, static_cast<EBoneTranslationRetargetingMode::Type>(Mode), bChildrenToo);
	return true;
}

int32 ULyudAnimTools::GetBoneTranslationRetargeting(USkeleton* Skeleton, FName Bone)
{
	if (!Skeleton)
	{
		return -1;
	}
	const int32 Index = Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone);
	return Index == INDEX_NONE ? -1 : static_cast<int32>(Skeleton->GetBoneTranslationRetargetingMode(Index));
}
