using UnrealBuildTool;
public class CatalystEcosystemEditor : ModuleRules
{
    public CatalystEcosystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "CatalystFoundationEditor", "AssetDefinition" });
        PrivateDependencyModuleNames.AddRange(new string[] { "CatalystEcosystem", "CatalystEcosystemPCG", "CatalystFoundation", "Slate", "SlateCore", "PropertyEditor", "AssetTools", "AssetRegistry", "PCG", "Projects" });
        bool HasLandform = false;
        try { HasLandform = System.IO.Directory.Exists(GetModuleDirectory("CatalystLandformEditor")); } catch (BuildException) { }
        PublicDefinitions.Add("WITH_CE_LANDFORM=" + (HasLandform ? "1" : "0"));
        if (HasLandform) PrivateDependencyModuleNames.AddRange(new[] { "CatalystLandform", "CatalystLandformEditor" });

    }
}
