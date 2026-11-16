#pragma once
#include "CoreMinimal.h"
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
#include "Weather/CEPlanetWeather.h"
/** Shared, immutable presentation topology. Weather stays in canonical surface coordinates. */
struct CATALYSTECOSYSTEMEDITOR_API FCEWeatherGlobeMesh
{
    TArray<FVector> Directions;
    TArray<int32> Indices;
    static FCEWeatherGlobeMesh Make();
};
/** Changes to Overlay alone alter presentation, preserving the model input generation and clock. */
CATALYSTECOSYSTEMEDITOR_API bool CEWeatherTerrainInputsMatch(const FCLCompiledPlanetRecipe& A,const FCLCompiledPlanetRecipe& B);
CATALYSTECOSYSTEMEDITOR_API TSharedPtr<const FCLPlanetBuilderOverlay> CEBuildWeatherGlobe(const FCEWeatherGlobeMesh& Mesh,TConstArrayView<FCEWeatherSample> Samples,double RadiusMeters,double ReferencePressureHpa,int32 Layer,bool Pressure=true,bool Wind=true,bool Heatmap=true,bool Fronts=true);
#endif
