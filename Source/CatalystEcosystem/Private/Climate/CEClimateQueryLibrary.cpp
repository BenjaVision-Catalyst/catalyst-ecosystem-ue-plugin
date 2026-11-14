#include "Climate/CEClimateQueryLibrary.h"
bool UCEClimateQueryLibrary::EvaluateSurface(UCEPlanetEcosystemAsset* Profile,const TArray<FVector>& Positions,const TArray<FVector>& Normals,
    double Radius,int32 Seed,double SeaLevel,TArray<FGuid>& Samples)
{
    Samples.Reset();FString Error;
    if(!IsValid(Profile)||Positions.Num()!=Normals.Num()||Positions.Num()>65536||!FMath::IsFinite(Radius)||Radius<=0||!FMath::IsFinite(SeaLevel)||!Profile->Ecosystem.Validate(Error))return false;
    for(int32 I=0;I<Positions.Num();++I)if(Positions[I].ContainsNaN()||Normals[I].ContainsNaN()||Positions[I].IsNearlyZero()||Normals[I].IsNearlyZero())return false;
    auto Model=Profile->Ecosystem;Model.Climate.SeaLevelMeters=SeaLevel;
    Samples.Reserve(Positions.Num());for(int32 I=0;I<Positions.Num();++I)Samples.Add(FCEClimateEvaluator::Evaluate(Model,Positions[I],Normals[I],Radius,Seed).BiomeId);
    return true;
}
