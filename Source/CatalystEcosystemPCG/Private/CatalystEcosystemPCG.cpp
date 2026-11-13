#include "Modules/ModuleManager.h"
class FCatalystEcosystemPCGModule : public IModuleInterface
{ public: virtual bool SupportsDynamicReloading() override { return false; } };
IMPLEMENT_MODULE(FCatalystEcosystemPCGModule, CatalystEcosystemPCG)
