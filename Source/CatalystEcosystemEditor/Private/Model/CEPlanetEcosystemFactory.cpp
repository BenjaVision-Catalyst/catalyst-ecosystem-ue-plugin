#include "Model/CEPlanetEcosystemFactory.h"
#include "Model/CEPlanetEcosystem.h"

UCEPlanetEcosystemFactory::UCEPlanetEcosystemFactory()
{
    SupportedClass=UCEPlanetEcosystemAsset::StaticClass();bCreateNew=true;bEditAfterNew=false;
}
FText UCEPlanetEcosystemFactory::GetDisplayName() const
{return NSLOCTEXT("CE","EcosystemAsset","Ecosystem");}
UObject* UCEPlanetEcosystemFactory::FactoryCreateNew(UClass* Class,UObject* Parent,FName Name,EObjectFlags Flags,UObject*,FFeedbackContext*)
{
    auto* Asset=NewObject<UCEPlanetEcosystemAsset>(Parent,Class,Name,Flags|RF_Transactional);
    if(const auto* Example=Cast<UCEPlanetEcosystemAsset>(UCEPlanetEcosystemAsset::ExampleAssetPath().TryLoad()))
    {
        Asset->Ecosystem=Example->Ecosystem;
        Asset->SurfaceSamplerGraph=Example->SurfaceSamplerGraph;
    }
    return Asset;
}

TSoftClassPtr<UObject> UCEPlanetEcosystemAssetDefinition::GetAssetClass() const
{return UCEPlanetEcosystemAsset::StaticClass();}
FText UCEPlanetEcosystemAssetDefinition::GetAssetDisplayName() const
{return NSLOCTEXT("CE","EcosystemAsset","Ecosystem");}
