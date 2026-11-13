using UnrealBuildTool;
public class CatalystEcosystemTests : ModuleRules
{
    public CatalystEcosystemTests(ReadOnlyTargetRules Target) : base(Target)
    {
        try { if (System.IO.Directory.Exists(GetModuleDirectory("CatalystLandformEditor"))) PrivateDependencyModuleNames.AddRange(new[] {"CatalystLandform", "CatalystLandformEditor", "Slate", "SlateCore", "ImageWrapper"}); } catch (BuildException) { }
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { });
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UnrealEd", "PCG", "CatalystEcosystem", "CatalystEcosystemPCG", "CatalystEcosystemEditor", "CatalystFoundation" });
    }
}
