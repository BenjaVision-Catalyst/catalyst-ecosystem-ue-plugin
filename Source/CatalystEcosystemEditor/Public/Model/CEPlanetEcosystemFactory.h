#pragma once
#include "Factories/Factory.h"
#include "AssetDefinitions/CFAssetDefinition_Model.h"
#include "CEPlanetEcosystemFactory.generated.h"

/** Native asset-picker and Content Browser creation for ecosystem profiles. */
UCLASS()
class CATALYSTECOSYSTEMEDITOR_API UCEPlanetEcosystemFactory : public UFactory
{
    GENERATED_BODY()
public:
    UCEPlanetEcosystemFactory();
    virtual FText GetDisplayName() const override;
    virtual UObject* FactoryCreateNew(UClass* Class,UObject* Parent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
};

/** Foundation supplies Catalyst's Content Browser category and model presentation. */
UCLASS()
class CATALYSTECOSYSTEMEDITOR_API UCEPlanetEcosystemAssetDefinition : public UCFAssetDefinition_Model
{
    GENERATED_BODY()
public:
    virtual TSoftClassPtr<UObject> GetAssetClass() const override;
    virtual FText GetAssetDisplayName() const override;
};
