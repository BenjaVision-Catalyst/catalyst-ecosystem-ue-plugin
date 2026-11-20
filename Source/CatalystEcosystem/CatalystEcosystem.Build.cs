using UnrealBuildTool;
public class CatalystEcosystem : ModuleRules
{
    public CatalystEcosystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CatalystFoundation", "CatalystSimulator" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Projects" });
        RuntimeDependencies.Add("$(PluginDir)/Resources/Weather/planetary-weather-4.json");
    }
}
