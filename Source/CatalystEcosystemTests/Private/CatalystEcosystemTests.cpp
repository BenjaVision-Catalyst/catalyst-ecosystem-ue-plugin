#include "Modules/ModuleManager.h"
class FCatalystEcosystemTestsModule : public IModuleInterface
{ public: virtual bool SupportsDynamicReloading() override { return false; } };
IMPLEMENT_MODULE(FCatalystEcosystemTestsModule, CatalystEcosystemTests)
