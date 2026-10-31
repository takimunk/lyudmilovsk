#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudNavStrip.generated.h"

/**
 * Frees the navigation collision every static mesh builds on load (~0.28 GB in the game, ~1 GB in the editor with
 * Lvl_Scene01 open). Nothing in the game uses Unreal's navigation: people, animals and birds walk their own maps.
 * Editor, game and PIE worlds; the meshes keep bHasNavigationData and the collision is transient, so nothing is saved
 * differently. lyud.NavStrip 0 keeps it (A/B; takes effect for worlds started after).
 */
UCLASS()
class LYUDMILOVSK_API ULyudNavStripSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Drops the nav collision of every loaded static mesh; returns how many it dropped. */
	static int32 StripAll();

private:
	FDelegateHandle Loaded;
};
