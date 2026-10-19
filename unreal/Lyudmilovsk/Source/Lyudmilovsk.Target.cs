using UnrealBuildTool;

public class LyudmilovskTarget : TargetRules
{
	public LyudmilovskTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Lyudmilovsk");
	}
}
