#pragma once
#include "CoreMinimal.h"
/** Live, player-driven derived scenery. No generated world or baked planet input. */
CATALYSTECOSYSTEMPCG_API bool CEValidatePlanetScenery(UObject* Planet,FString& Error);
CATALYSTECOSYSTEMPCG_API void CEStartPlanetScenery();
CATALYSTECOSYSTEMPCG_API void CEStopPlanetScenery();

CATALYSTECOSYSTEMPCG_API FString CEPlanetSceneryReport(UObject* Planet);
