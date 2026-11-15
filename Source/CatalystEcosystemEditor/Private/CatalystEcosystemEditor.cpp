#include "Modules/ModuleManager.h"
#include "PlanetBuilder/CEPlanetBuilder.h"
#if WITH_CE_LANDFORM
#include "CLPlanetInspection.h"
ECLInspectionPreparation CEPreparePlanetScenery(ACLPlanet&,FString&);
void CECollectSceneryMaterials(ACLPlanet&,TArray<UMaterialInterface*>&);
#endif
class FCatalystEcosystemEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
#if WITH_CE_LANDFORM
        FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("CatalystLandformEditor"));
        FCLPlanetBuilderExtensions::Register({TEXT("CatalystEcosystem"),NSLOCTEXT("CE","Tab","Ecosystem"),CECreatePlanetBuilder});
        FCLPlanetInspection::RegisterPreparation(TEXT("CatalystEcosystem"),CEPreparePlanetScenery);
        FCLPlanetInspection::RegisterMaterialSources(TEXT("CatalystEcosystem"),CECollectSceneryMaterials);
#endif
    }
    virtual void ShutdownModule() override
    {
#if WITH_CE_LANDFORM
        if(FModuleManager::Get().IsModuleLoaded(TEXT("CatalystLandformEditor")))
        {FCLPlanetInspection::UnregisterPreparation(TEXT("CatalystEcosystem"));FCLPlanetInspection::UnregisterMaterialSources(TEXT("CatalystEcosystem"));FCLPlanetBuilderExtensions::Unregister(TEXT("CatalystEcosystem"));}
#endif
    }
    virtual bool SupportsDynamicReloading() override {return false;}
};
IMPLEMENT_MODULE(FCatalystEcosystemEditorModule,CatalystEcosystemEditor)
