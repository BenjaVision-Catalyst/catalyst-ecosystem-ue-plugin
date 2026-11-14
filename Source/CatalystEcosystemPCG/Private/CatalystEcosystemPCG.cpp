#include "Modules/ModuleManager.h"
#include "Climate/CEPlanetBiomePlacement.h"
class FCatalystEcosystemPCGModule : public IModuleInterface
{
public:
    virtual void StartupModule()override{CEStartBiomePlacement();}
    virtual void ShutdownModule()override{CEStopBiomePlacement();}
    virtual bool SupportsDynamicReloading()override{return false;}
};
IMPLEMENT_MODULE(FCatalystEcosystemPCGModule, CatalystEcosystemPCG)
