#pragma once
#include "Climate/CEBiomePlacement.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/ActorComponent.h"
#include "CEPlanetBiomePlacement.generated.h"

UCLASS()
class CATALYSTECOSYSTEMPCG_API UCEBiomePlacementState : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere,Category="Ecosystem") FString GeneratedPlacementKey;
    UPROPERTY(VisibleAnywhere,Category="Ecosystem") bool bDistant=false;
    FString RequestedKey,PendingKey;
};
struct FCLCompiledPlanetRecipe;
CATALYSTECOSYSTEMPCG_API TSharedPtr<const FCEBiomePlacementPlan> CEGetCompiledBiomePlacement(TSharedPtr<const FCLCompiledPlanetRecipe> Plan,UCEPlanetEcosystemAsset* Profile,bool& Pending,FString& Error);
CATALYSTECOSYSTEMPCG_API void CERecordBiomePlacement(UObject* Owner,UObject* Planet,UCEPlanetEcosystemAsset* Profile);
/** Optional terrain adapter; UObject ownership stays on the game thread. */
CATALYSTECOSYSTEMPCG_API TSharedPtr<const FCEBiomePlacementPlan> CEGetPlanetBiomePlacement(UObject* Planet,UCEPlanetEcosystemAsset* Profile,bool& Pending,FString& Error);
CATALYSTECOSYSTEMPCG_API UCEPlanetEcosystemAsset* CEPlanetEcosystemProfile(UObject* Planet);
CATALYSTECOSYSTEMPCG_API void CEStartBiomePlacement();
CATALYSTECOSYSTEMPCG_API void CEStopBiomePlacement();

UCLASS()
class CATALYSTECOSYSTEMPCG_API UCEPlanetBiomePlacementLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Ecosystem|Placement")
    static FString GetSceneryReport(UObject* Planet);
    /** Queries the same accepted planet assignment used by the map and PCG. Retry while Preparing. */
    UFUNCTION(BlueprintCallable,Category="Ecosystem|Placement")
    static bool EvaluatePlanetSurface(UObject* Planet,UCEPlanetEcosystemAsset* Profile,const TArray<FVector>& PositionsMeters,const TArray<FVector>& Normals,TArray<FGuid>& BiomeIds,FString& Error);
    UFUNCTION(BlueprintCallable,Category="Ecosystem|Placement")
    static FString GetPlacementReport(UObject* Planet,UCEPlanetEcosystemAsset* Profile);
};
