using UnrealBuildTool;
public class CatalystEcosystemPCG : ModuleRules
{
    public CatalystEcosystemPCG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "PCG", "CatalystEcosystem" });
        PrivateDependencyModuleNames.AddRange(new string[] {  });
    }
}
