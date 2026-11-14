using UnrealBuildTool;
public class CatalystEcosystemPCG : ModuleRules
{
    public CatalystEcosystemPCG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "PCG", "CatalystEcosystem" });
        bool HasLandform=false;
        try { HasLandform=System.IO.Directory.Exists(GetModuleDirectory("CatalystLandform")); } catch (BuildException) { }
        PublicDefinitions.Add("WITH_CE_PLACEMENT_LANDFORM="+(HasLandform?"1":"0"));
        if(HasLandform) PrivateDependencyModuleNames.Add("CatalystLandform");
        PrivateDependencyModuleNames.Add("Json");
    }
}
