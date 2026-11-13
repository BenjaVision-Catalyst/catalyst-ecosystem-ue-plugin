#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CEBiomeCoreBridge.generated.h"
class UCEPlanetEcosystemAsset;
/** Editor-only conversion to native Biome Core assets; originals and engine content remain untouched. */
UCLASS()
class CATALYSTECOSYSTEMEDITOR_API UCEBiomeCoreBridge : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Catalyst|Ecosystem")
    static bool Build(UCEPlanetEcosystemAsset* Profile,const FString& DestinationFolder,const FTransform& PlanetToWorld,double ReferenceRadiusMeters,int32 PlanetSeed,TArray<UObject*>& BiomeAssets,FString& Error);
};
