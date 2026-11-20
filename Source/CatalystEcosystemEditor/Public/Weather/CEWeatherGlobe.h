#pragma once
#include "CoreMinimal.h"
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
#include "Weather/CEPlanetWeather.h"
#include "Weather/CEWeatherTopology.h"
#include "Weather/CEWeatherFronts.h"
/** Presentation-only birth/retirement; physical weather and diagnostic strength remain queryable without this cache. */
struct CATALYSTECOSYSTEMEDITOR_API FCEWeatherFrontPresentation
{
    struct FEntry{FCEWeatherFront Front;float Opacity=0;bool Retiring=false;};
    TArray<FEntry> Entries;
    void SetFronts(TArray<FCEWeatherFront> Fronts);
    void Tick(double Seconds);
    bool IsAnimating()const;
    void Append(const FCEWeatherGlobeMesh&,double Radius,FCLPlanetBuilderOverlay&)const;
};
/** Unlit radar palette. Dry/non-finite inputs are transparent; rates are water-equivalent mm/hour. */
CATALYSTECOSYSTEMEDITOR_API FLinearColor CEWeatherPrecipitationColor(double RainMmHour,double SnowMmHour);
/** Changes to Overlay alone alter presentation, preserving the model input generation and clock. */
CATALYSTECOSYSTEMEDITOR_API bool CEWeatherTerrainInputsMatch(const FCLCompiledPlanetRecipe& A,const FCLCompiledPlanetRecipe& B);
CATALYSTECOSYSTEMEDITOR_API TSharedPtr<const FCLPlanetBuilderOverlay> CEBuildWeatherGlobe(const FCEWeatherGlobeMesh& Mesh,TConstArrayView<FCEWeatherSample> Samples,double RadiusMeters,double ReferencePressureHpa,int32 Layer,bool Pressure=true,bool Wind=true,bool Heatmap=true,bool Fronts=true,TConstArrayView<FVector> GroundPositions={});
CATALYSTECOSYSTEMEDITOR_API void CEAppendWeatherFrontOverlay(const FCEWeatherGlobeMesh&,TConstArrayView<FCEWeatherFront>,double,FCLPlanetBuilderOverlay&);
#endif
