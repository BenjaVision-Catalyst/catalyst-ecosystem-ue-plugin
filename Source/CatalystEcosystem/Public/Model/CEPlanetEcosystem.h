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
    /** Optional close detail rules when canopy is streamed independently. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core") FSoftObjectPath DetailBiomeCoreAsset;
    /** Lighter live distant representation, e.g. canopy without ground scatter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core") FSoftObjectPath DistantBiomeCoreAsset;
    /** Optional live coverage texture parameter used by client ground materials. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance") FName GroundCoverMaskParameter;
};
USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEAuthoredWeatherEvent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Event", meta=(IgnoreForMemberInitializationTest)) FGuid Id=FGuid::NewGuid();
    UPROPERTY(EditAnywhere, Category="Event") FString Name=TEXT("Tornado and parent storm");
    UPROPERTY(EditAnywhere, Category="Event") bool bEnabled=true;
    UPROPERTY(EditAnywhere, Category="Schedule", meta=(ClampMin="0",Units="s")) double StartSeconds=0;
    UPROPERTY(EditAnywhere, Category="Schedule", meta=(ClampMin="60",Units="s")) double DurationSeconds=10800;
    UPROPERTY(EditAnywhere, Category="Storm", meta=(ClampMin="0",ClampMax="150",Units="m/s")) double WindSpeed=50;
    UPROPERTY(EditAnywhere, Category="Storm", meta=(ClampMin="10",Units="m")) double RadiusMeters=100;
    UPROPERTY(EditAnywhere, Category="Storm", meta=(ClampMin="0",ClampMax="1")) double Moisture=.8;
    /** Body directions for a cubic spline, sampled by arc length. Radius is supplied by the designed planet. */
    UPROPERTY(EditAnywhere, Category="Path") TArray<FVector> PathDirections={FVector(1,0,0),FVector(1,.5,.2),FVector(.5,1,.2)};
};
USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEWeatherSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Generation") bool bEnabled=true;
    /** Calm to volatile: increases natural extreme frequency and severity without moving biomes. */
    UPROPERTY(EditAnywhere, Category="Generation", meta=(ClampMin="0",ClampMax="1")) double Character=.5;
    /** Multiplies natural weather travel only. Zero holds trajectories; one is the original rate. */
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="0",ClampMax="100",ToolTip="1 keeps the current travel rate. Below 1 slows it; above 1 speeds it up. Zero holds natural travel. Day/night, wind strength and authored event schedules are unchanged.",DisplayName="Weather travel multiplier")) double TravelMultiplier=2;
    /** Persisted trajectory anchor; the designer adjusts this when the multiplier changes. */
    UPROPERTY() double TravelPhaseOffsetSeconds=0;
    void SetTravelMultiplier(double Value,double SimulationSeconds);
    void SetCharacter(double Value,double SimulationSeconds);
    UPROPERTY(EditAnywhere, Category="Weather") int32 Seed=1729;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="1",ClampMax="10000",DisplayName="Reference pressure (hPa)")) double ReferencePressureHpa=1013.25;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="0.01",ClampMax="5",ToolTip="Wind response to pressure gradients normalized to the planet reference atmosphere.")) double WindResponse=.6;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="0",ClampMax="3")) double RegionalStrength=1;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="0",ClampMax="40",Units="m/s")) double PrevailingWind=6;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="600",ClampMax="604800",Units="s")) double CirculationSeconds=21600;
    UPROPERTY(EditAnywhere, Category="Weather", meta=(ClampMin="60",ClampMax="315576000",Units="s")) double DaySeconds=86400;
    UPROPERTY(EditAnywhere, Category="Authored events") TArray<FCEAuthoredWeatherEvent> Events;
};
USTRUCT(BlueprintType)
struct CATALYSTECOSYSTEM_API FCEPlanetEcosystem
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, Category="Version") int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Climate", meta=(ShowOnlyInnerProperties)) FCEClimateSettings Climate;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weather") FCEWeatherSettings Weather;
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
    /** Packaged authoring example used when a planet has no explicit ecosystem link. */
    static FSoftObjectPath ExampleAssetPath();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ecosystem") FCEPlanetEcosystem Ecosystem;
    /** A configured graph with exactly one bounded native VPP surface sampler. Authoring reference, outside the portable climate payload. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core", meta=(AllowedClasses="/Script/PCG.PCGGraph")) FSoftObjectPath SurfaceSamplerGraph;
    /** Native Biome Core deployment references; no world instance or generated products are authoring inputs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet") FSoftClassPath RegionActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet") FSoftObjectPath RegionDefinition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet") FSoftObjectPath LocalBiomeCache;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet") FSoftObjectPath WindTransformProvider;
    /** Native Biome Core graph configured for runtime hierarchical generation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet") FSoftObjectPath RuntimeGraph;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet", meta=(ClampMin="32",Units="m")) double NearRadiusMeters=150;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Core|Planet", meta=(ClampMin="100",Units="m")) double DistantRadiusMeters=1500;
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
