#pragma once
#include "CoreMinimal.h"
/** Shared, immutable presentation topology. Weather stays in canonical surface coordinates. */
struct CATALYSTECOSYSTEM_API FCEWeatherGlobeMesh
{
    TArray<FVector> Directions;
    TArray<int32> Indices;
    static FCEWeatherGlobeMesh Make();
};

/** A reusable spherical lookup into the complete canonical weather evaluation. */
struct FCEWeatherGlobeStencil
{
    FIntVector Vertices;
    FVector Weights;
};

CATALYSTECOSYSTEM_API bool CELocateWeatherGlobeTriangle(const FCEWeatherGlobeMesh& Mesh,
    const FVector& BodyDirection, int32& TriangleHint, FCEWeatherGlobeStencil& Stencil);
