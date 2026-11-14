#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Model/CEPlanetEcosystem.h"
#include "CEClimateQueryLibrary.generated.h"
/** Shared classification query for tools, coverage materials and placement verification.
 * Positions/normals are canonical body-space metres. Sea level is the accepted guide datum.
 */
UCLASS()
class CATALYSTECOSYSTEM_API UCEClimateQueryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Ecosystem|Climate")
    static bool EvaluateSurface(UCEPlanetEcosystemAsset* Profile,const TArray<FVector>& PositionsMeters,const TArray<FVector>& Normals,
        double ReferenceRadiusMeters,int32 PlanetSeed,double SeaLevelMeters,TArray<FGuid>& BiomeIds);
};
