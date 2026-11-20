#pragma once
#include "Weather/CEWeatherTopology.h"
#include "Weather/CEPlanetWeather.h"
enum class ECEWeatherFrontKind : uint8 { Cold, Warm, Stationary };
struct FCEWeatherFrontPoint
{
    FVector Direction=FVector::ZeroVector;
    /** Tangent pointing from colder to warmer air. */
    FVector Warmward=FVector::ZeroVector;
    double Cloud=0,PrecipitationMmHour=0,CrossWindMetersPerSecond=0;
    /** Continuous 0..1 confidence of a thermal-gradient ridge; queryable by game code. */
    double Strength=0;
};
/** Bounded diagnostic of the sampled field, not an independent moving weather system. */
struct CATALYSTECOSYSTEM_API FCEWeatherFront
{
    ECEWeatherFrontKind Kind=ECEWeatherFrontKind::Stationary;
    TArray<FCEWeatherFrontPoint> Points;
    TArray<double> ArcRadians;
    bool Sample(double Arc,FCEWeatherFrontPoint& Point,FVector& Tangent)const;
};
/** Thermal-gradient ridges on the closed globe; canonical directions, independent of display radius. */
CATALYSTECOSYSTEM_API TArray<FCEWeatherFront> CEDiagnoseWeatherFronts(const FCEWeatherGlobeMesh& Mesh,TConstArrayView<FCEWeatherSample> Samples);
