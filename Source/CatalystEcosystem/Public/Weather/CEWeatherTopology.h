#pragma once
#include "CoreMinimal.h"
/** Shared, immutable presentation topology. Weather stays in canonical surface coordinates. */
struct CATALYSTECOSYSTEM_API FCEWeatherGlobeMesh
{
    TArray<FVector> Directions;
    TArray<int32> Indices;
    static FCEWeatherGlobeMesh Make();
};
