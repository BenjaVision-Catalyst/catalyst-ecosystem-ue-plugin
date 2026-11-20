#pragma once
#include "CoreMinimal.h"
#include "Fields/CSFieldProgram.h"
#include "Weather/CEPlanetWeather.h"
/** Canonical spatial inputs for the GT frontal stage. No clock or view-dependent state. */
class CATALYSTECOSYSTEM_API FCEWeatherFrontalField
{
public:
    bool Initialize(TConstArrayView<FVector> Positions,TConstArrayView<FVector> Normals,TConstArrayView<double> ClimateMoisture,TConstArrayView<double> BiomeMoisture,TConstArrayView<int64> Uniforms,FString& Error);
    bool Evaluate(TConstArrayView<FCEWeatherSample> Background,TArray<FCEWeatherSample>& Result,FString& Error)const;
private:
    FCSFieldProgram Program;
    FCSFieldContext Context;
    TArray<FVector> Directions,GroundNormals;
    TArray<double> ClimateWet,BiomeWet,Probes;
    TArray<TArray<int32>> Neighbours,Pools;
    TArray<TArray<FVector>> GradientWeights;
    int32 SmoothPasses=0;
    double ProbeSupport=0;
    void Smooth(TArray<double>& Values)const;
    TArray<FVector> Gradient(const TArray<double>& Values)const;
};
