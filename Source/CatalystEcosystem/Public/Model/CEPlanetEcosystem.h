#pragma once
#include "CoreMinimal.h"
#include "Model/CFModelAsset.h"
#include "CEPlanetEcosystem.generated.h"

USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEClimateSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate") FVector PoleAxis = FVector::ZAxisVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate", meta=(Units="Celsius")) double EquatorTemperatureC = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate", meta=(Units="Celsius")) double PoleTemperatureC = -25;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate", meta=(ClampMin="0",ClampMax="20")) double LapseRateCPerKm = 6.5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water", meta=(ForceUnits="m")) double SeaLevelMeters = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water", meta=(ClampMin="0",ClampMax="1")) double Moisture = .65;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water", meta=(ClampMin="0",ClampMax="1")) double SubtropicalDrying = .5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variation", meta=(ClampMin="0",ClampMax="30",Units="Celsius")) double TemperatureVariationC = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variation", meta=(ClampMin="0",ClampMax="1")) double MoistureVariation = .2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variation", meta=(ClampMin="1",ClampMax="64")) double NoiseFrequency = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Variation") int32 Seed = 1729;
};
USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEBiomeDefinition
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Identity") FGuid Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identity") FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identity", meta=(MultiLine=true)) FString Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Identity") FLinearColor Color = FLinearColor::Green;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation") bool bEnabled = true;
    /** Widens temperature/moisture tolerance by up to 10 C / 0.2 around the curated range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="0",ClampMax="1")) double Character = .5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation") int32 Seed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat") int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat") bool bWater = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat") FVector2D TemperatureC = FVector2D(-100,100);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat") FVector2D Moisture = FVector2D(0,1);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat", meta=(ForceUnits="m")) FVector2D AltitudeMeters = FVector2D(-1.e7,1.e7);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Habitat") FVector2D SlopeDegrees = FVector2D(0,90);
    /** Source UE Biome Core BiomeAsset. Only generated copies are changed by the editor bridge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core") FSoftObjectPath BiomeCoreAsset;
};
USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEPlanetEcosystem
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, Category="Version") int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate", meta=(ShowOnlyInnerProperties)) FCEClimateSettings Climate;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes") TArray<FCEBiomeDefinition> Biomes;
    static FCEPlanetEcosystem MakeDefault();
    bool Validate(FString& Error) const;
};
/** Plain immutable inputs: body coordinates in metres; no world-Z climate or UObject ownership. */
struct CATALYSTECOSYSTEM_API FCEClimateSample
{
    double TemperatureC=0, Moisture=0, AltitudeMeters=0, LatitudeDegrees=0, SlopeDegrees=0;
    bool bWater=false;
    FGuid BiomeId;
    double BiomeWeight=0;
};
struct CATALYSTECOSYSTEM_API FCEClimateEvaluator
{
    static FCEClimateSample Evaluate(const FCEPlanetEcosystem& Model, FVector BodyMeters, FVector BodyNormal, double ReferenceRadiusMeters, int32 PlanetSeed);
};
UCLASS(BlueprintType)
class CATALYSTECOSYSTEM_API UCEPlanetEcosystemAsset : public UCFModelAsset
{
    GENERATED_BODY()
public:
    UCEPlanetEcosystemAsset();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ecosystem") FCEPlanetEcosystem Ecosystem;
    /** A configured graph with exactly one bounded native VPP surface sampler. Authoring reference, outside the portable climate payload. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core", meta=(AllowedClasses="/Script/PCG.PCGGraph")) FSoftObjectPath SurfaceSamplerGraph;
#if WITH_EDITOR
    FSimpleMulticastDelegate OnEcosystemChanged;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    virtual void PostEditUndo() override;
#endif
protected:
    virtual FString GetPluginNameForPaths() const override { return TEXT("CatalystEcosystem"); }
    virtual UScriptStruct* GetPayloadScriptStruct() const override { return FCEPlanetEcosystem::StaticStruct(); }
    virtual void* GetPayloadMemory() override { return &Ecosystem; }
    virtual const void* GetPayloadMemory() const override { return &Ecosystem; }
    virtual bool ValidatePayload(FString& Error) const override { return Ecosystem.Validate(Error); }
};
