using UnrealBuildTool;

public class LyudmilovskEditorTarget : TargetRules
{
	public LyudmilovskEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Lyudmilovsk");
	}
}
