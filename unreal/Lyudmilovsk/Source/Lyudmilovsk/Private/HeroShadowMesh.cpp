#include "HeroShadowMesh.h"

#include "Components/SkeletalMeshComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarHeroShadowHead(TEXT("lyud.HeroShadowHead"), 1,
		TEXT("1: the hero's shadow has his head (an invisible full copy casts it), 0: the headless body casts its own"));
}

UHeroShadowMesh::UHeroShadowMesh()
{
	PrimaryComponentTick.bCanEverTick = true;
	// after the body's animation (pre-physics) and whatever moves its bones after it
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetRenderInMainPass(false);
	SetRenderInDepthPass(false);
	SetCastShadow(true);
	bCastDynamicShadow = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bReceivesDecals = false;
	SetCanEverAffectNavigation(false);
}

void UHeroShadowMesh::Follow(USkeletalMeshComponent* Source)
{
	Body = Source;
	if (!Source || !Source->GetSkinnedAsset())
	{
		return;
	}
	SetSkinnedAssetAndUpdate(Source->GetSkinnedAsset());
	PrimaryComponentTick.AddPrerequisite(Source, Source->PrimaryComponentTick);
	CopyPoseFromSkeletalComponent(Source);
}

void UHeroShadowMesh::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	USkeletalMeshComponent* Source = Body.Get();
	const bool bOn = Source && CVarHeroShadowHead.GetValueOnGameThread() != 0 && Source->IsVisible() && Source->GetSkinnedAsset() == GetSkinnedAsset();
	// copy before the tick: it only marks the pose dirty, and Super's tick turns it into bone transforms this frame
	if (bOn)
	{
		CopyPoseFromSkeletalComponent(Source);
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Source)
	{
		return;
	}
	if (IsVisible() != bOn)
	{
		SetVisibility(bOn);
	}
	// exactly one of the two casts the shadow
	if (Source->CastShadow == bOn)
	{
		Source->SetCastShadow(!bOn);
	}
}
