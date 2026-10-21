using UnrealBuildTool;

public class Lyudmilovsk : ModuleRules
{
	public Lyudmilovsk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "LevelSequence", "MovieScene", "Slate", "SlateCore", "UMG", "WebBrowser", "MeshDescription", "StaticMeshDescription", "Landscape", "RHI", "RenderCore" });
	}
}
