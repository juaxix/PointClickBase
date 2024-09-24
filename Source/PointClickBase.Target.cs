// xixgames - juaxix - 2024

using UnrealBuildTool;
using System.Collections.Generic;

public class PointClickBaseTarget : TargetRules
{
	public PointClickBaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange( new string[] { "PointClickBase" } );
	}
}
