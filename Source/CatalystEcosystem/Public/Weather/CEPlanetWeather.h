#pragma once
#include "CoreMinimal.h"
#include "Model/CEPlanetEcosystem.h"
#include "Fields/CSFieldProgram.h"
struct FCEWeatherSample { double TemperatureC=0,RainMmHour=0,Dust=0,Cloud=0;FVector Wind=FVector::ZeroVector; };
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
