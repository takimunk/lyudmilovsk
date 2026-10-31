#include "World/LyudNavStrip.h"

#include "AI/Navigation/NavCollisionBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TAutoConsoleVariable<int32> CVarNavStrip(TEXT("lyud.NavStrip"), 1,
		TEXT("1: game worlds free the static meshes' navigation collision (nothing uses Unreal navigation); 0: keep it."));

	void Strip(UStaticMesh* Mesh)
	{
		if (Mesh && Mesh->GetNavCollision())
		{
			Mesh->SetNavCollision(nullptr);
		}
	}
}

bool ULyudNavStripSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

int32 ULyudNavStripSubsystem::StripAll()
{
	int32 N = 0;
	for (TObjectIterator<UStaticMesh> It; It; ++It)
	{
		if (It->GetNavCollision())
		{
			Strip(*It);
			N++;
		}
	}
	return N;
}

void ULyudNavStripSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (CVarNavStrip.GetValueOnGameThread() == 0)
	{
		return;
	}
	const int32 N = StripAll();
	UE_LOG(LogTemp, Log, TEXT("NavStrip: freed the navigation collision of %d static meshes (%s)"), N, *GetWorld()->GetMapName());
	// meshes loaded later (streamed buildings, spawned props) build theirs in PostLoad
	Loaded = FCoreUObjectDelegates::OnAssetLoaded.AddLambda([](UObject* Asset)
	{
		if (CVarNavStrip.GetValueOnGameThread() != 0)
		{
			Strip(Cast<UStaticMesh>(Asset));
		}
	});
}

void ULyudNavStripSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::OnAssetLoaded.Remove(Loaded);
	Super::Deinitialize();
}
