#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "CLPlanetInspection.h"
#include "CLPlanet.h"
#include "Model/CEPlanetEcosystem.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWalkingDeploymentGuard,"Catalyst.Ecosystem.Placement.WalkingDeploymentGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWalkingDeploymentGuard::RunTest(const FString&)
{
    TStrongObjectPtr<UCLPlanetRecipeAsset> Source(NewObject<UCLPlanetRecipeAsset>());
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile(NewObject<UCEPlanetEcosystemAsset>());
    auto* Planet=GetMutableDefault<ACLPlanet>();auto* Previous=Planet->RecipeAsset.Get();Planet->RecipeAsset=Source.Get();
    ON_SCOPE_EXIT{Planet->RecipeAsset=Previous;};FString Error;
    TestTrue(TEXT("Unlinked terrain remains usable without Ecosystem art"),FCLPlanetInspection::Prepare(*Planet,Error)==ECLInspectionPreparation::Ready);
    Source->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
    TestTrue(TEXT("A map-only ecosystem needs no invented art deployment"),FCLPlanetInspection::Prepare(*Planet,Error)==ECLInspectionPreparation::Ready);
    Profile->Ecosystem.Biomes[0].BiomeCoreAsset=FSoftObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
    TestTrue(TEXT("Assigned content without deployment must fail closed"),FCLPlanetInspection::Prepare(*Planet,Error)==ECLInspectionPreparation::Failed);
    TestTrue(TEXT("Failure explains missing deployment references"),Error.Contains(TEXT("deployment references")));
    Profile->Ecosystem.Biomes[0].BiomeCoreAsset=FSoftObjectPath(TEXT("/Missing/Biome.Biome"));
    TestTrue(TEXT("Missing assigned art must fail closed"),FCLPlanetInspection::Prepare(*Planet,Error)==ECLInspectionPreparation::Failed);
    TestTrue(TEXT("Missing art identifies the reference"),Error.Contains(TEXT("/Missing/Biome.Biome")));return true;
}
#endif
