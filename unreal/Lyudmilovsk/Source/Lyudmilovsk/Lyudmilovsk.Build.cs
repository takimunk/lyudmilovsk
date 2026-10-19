using UnrealBuildTool;

public class Lyudmilovsk : ModuleRules
{
	public Lyudmilovsk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "LevelSequence", "MovieScene" });
	}
}
