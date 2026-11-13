#pragma once
#include "PCGSettings.h"
#include "Model/CEPlanetEcosystem.h"
#include "CEClimatePCGSettings.generated.h"
/** Feed VPP surface samples here, before Biome Core's generator output. Frame is explicit. */
UCLASS(BlueprintType, ClassGroup=(Procedural))
class CATALYSTECOSYSTEMPCG_API UCEClimatePCGSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate") TObjectPtr<UCEPlanetEcosystemAsset> Profile;
    /** Planet transform in world centimetres; only positive uniform scale is supported. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planet") FTransform PlanetToWorld = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planet", meta=(ClampMin="0.001",Units="m")) double ReferenceRadiusMeters = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planet") int32 PlanetSeed = 1729;
    /** Explicit generated-surface context, supplied from OceanHeight by the Planet editor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planet", meta=(InlineEditConditionToggle)) bool bOverrideSeaLevel = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Planet", meta=(EditCondition="bOverrideSeaLevel",ForceUnits="m")) double SeaLevelMeters = 0;
    /** Empty returns every classified point, including unassigned points. Valid GUID filters to its winning biome. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes") FGuid TargetBiome;
    static bool ValidateFrame(const FTransform& Frame, double Radius, FString& Error);
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CatalystPlanetClimate"); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("CE","ClimateNode","Catalyst Planet Climate"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};
