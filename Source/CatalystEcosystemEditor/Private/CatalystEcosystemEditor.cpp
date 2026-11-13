#include "Modules/ModuleManager.h"
#include "PlanetBuilder/CEPlanetBuilder.h"
class FCatalystEcosystemEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
#if WITH_CE_LANDFORM
        FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("CatalystLandformEditor"));
        FCLPlanetBuilderExtensions::Register({TEXT("CatalystEcosystem"),NSLOCTEXT("CE","Tab","Ecosystem"),CECreatePlanetBuilder});
#endif
    }
    virtual void ShutdownModule() override
    {
#if WITH_CE_LANDFORM
        if(FModuleManager::Get().IsModuleLoaded(TEXT("CatalystLandformEditor")))FCLPlanetBuilderExtensions::Unregister(TEXT("CatalystEcosystem"));
#endif
    }
    virtual bool SupportsDynamicReloading() override {return false;}
};
IMPLEMENT_MODULE(FCatalystEcosystemEditorModule,CatalystEcosystemEditor)
