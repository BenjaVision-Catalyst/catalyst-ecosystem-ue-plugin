using UnrealBuildTool;
public class CatalystEcosystem : ModuleRules
{
    public CatalystEcosystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CatalystFoundation", "CatalystSimulator" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "Json" });
        RuntimeDependencies.Add("$(PluginDir)/Resources/Weather/planetary-weather-6.json");
        RuntimeDependencies.Add("$(PluginDir)/Resources/Weather/planetary-weather-frontal-1.json");
    }
}
