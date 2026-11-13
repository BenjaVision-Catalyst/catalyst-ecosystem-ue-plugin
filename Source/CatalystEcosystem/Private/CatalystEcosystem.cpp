#include "Modules/ModuleManager.h"
class FCatalystEcosystemModule : public IModuleInterface
{ public: virtual bool SupportsDynamicReloading() override { return false; } };
IMPLEMENT_MODULE(FCatalystEcosystemModule, CatalystEcosystem)
