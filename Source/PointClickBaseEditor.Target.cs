// xixgames - juaxix - 2024

using UnrealBuildTool;
using System.Collections.Generic;

public class PointClickBaseEditorTarget : TargetRules
{
	public PointClickBaseEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange( new string[] { "PointClickBase" } );
	}
}
