using UnrealBuildTool;
public class CatalystEcosystem : ModuleRules
{
    public CatalystEcosystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CatalystFoundation" });
        PrivateDependencyModuleNames.AddRange(new string[] {  });
    }
}
