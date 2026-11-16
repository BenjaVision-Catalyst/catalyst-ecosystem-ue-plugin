#pragma once
#include "Weather/CEWeatherGlobe.h"
#if WITH_CE_LANDFORM
enum class ECEWeatherFrontKind : uint8 { Cold, Warm, Stationary };
struct FCEWeatherFrontPoint
{
    FVector Direction=FVector::ZeroVector;
    /** Tangent pointing from colder to warmer air. */
    FVector Warmward=FVector::ZeroVector;
    double Cloud=0,RainMmHour=0,CrossWindMetersPerSecond=0;
};
/** Bounded diagnostic of the sampled field, not an independent moving weather system. */
struct CATALYSTECOSYSTEMEDITOR_API FCEWeatherFront
{
    ECEWeatherFrontKind Kind=ECEWeatherFrontKind::Stationary;
    TArray<FCEWeatherFrontPoint> Points;
    TArray<double> ArcRadians;
    bool Sample(double Arc,FCEWeatherFrontPoint& Point,FVector& Tangent)const;
};
/** Thermal-gradient ridges on the closed globe; canonical directions, independent of display radius. */
CATALYSTECOSYSTEMEDITOR_API TArray<FCEWeatherFront> CEDiagnoseWeatherFronts(const FCEWeatherGlobeMesh& Mesh,TConstArrayView<FCEWeatherSample> Samples);
/** Uses existing transparent heatmap and line primitives; no materials or second simulation. */
CATALYSTECOSYSTEMEDITOR_API void CEAppendWeatherFrontOverlay(const FCEWeatherGlobeMesh& Mesh,TConstArrayView<FCEWeatherFront> Fronts,double RadiusMeters,FCLPlanetBuilderOverlay& Overlay);
#endif
