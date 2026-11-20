#pragma once

#include "CoreMinimal.h"

class FCEPlanetWeather;
struct FCEBiomePlacementPlan;
struct FCEPlanetEcosystem;
struct FCEWeatherGlobeMesh;
struct FCLCompiledPlanetRecipe;

#if WITH_CE_PLACEMENT_LANDFORM
CATALYSTECOSYSTEMPCG_API double CEPlanetSeaLevelMeters(const FCLCompiledPlanetRecipe& Plan);

/** Bake the complete canonical context from immutable inputs, suitable for a worker. */
CATALYSTECOSYSTEMPCG_API bool CEBakePlanetWeatherGlobe(
    const FCLCompiledPlanetRecipe& Plan, const FCEBiomePlacementPlan& Placement,
    const FCEWeatherGlobeMesh& Mesh, const FCEPlanetEcosystem& Profile,
    FCEPlanetWeather& Field, TArray<FVector>& Positions, FString& Error,
    const TAtomic<bool>* Cancel = nullptr);
#endif
