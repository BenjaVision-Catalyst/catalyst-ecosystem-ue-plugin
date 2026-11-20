#pragma once
#include "CoreMinimal.h"
#include "Model/CEPlanetEcosystem.h"
#include "Fields/CSFieldProgram.h"
struct FCEWeatherSample
{
    double TemperatureC=0;
    /** Liquid rain and snowfall water equivalent, both in mm/hour. */
    double RainMmHour=0,SnowMmHour=0;
    double Dust=0,Cloud=0,PressureHpa=0;FVector Wind=FVector::ZeroVector;
    double PrecipitationMmHour()const{return RainMmHour+SnowMmHour;}
};
/** Immutable baked planet inputs + GT-authored program; no domain solver or clock in this adapter. */
class CATALYSTECOSYSTEM_API FCEPlanetWeather
{
public:
    bool Initialize(const FCEPlanetEcosystem& Model,TConstArrayView<FVector> Positions,TConstArrayView<FVector> Normals,double ReferenceRadiusMeters,int32 PlanetSeed,FString& Error,TConstArrayView<FGuid> AssignedBiomes={});
    bool Evaluate(int64 Seconds,TArray<FCEWeatherSample>& Samples,FString& Error)const;
    const FCSFieldProgram& Program()const{return Field;}
    double LastBakeSeconds=0;
private:
    FCSFieldProgram Field;
    FCSFieldContext Context;
    TArray<TArray<int64>> Inputs;
};

/** Planet body-frame solar direction; identical basis/phase to the GT daily thermal forcing. */
CATALYSTECOSYSTEM_API FVector CEWeatherSunDirection(FVector Pole,double RotationPeriodSeconds,double SimulationSeconds);
