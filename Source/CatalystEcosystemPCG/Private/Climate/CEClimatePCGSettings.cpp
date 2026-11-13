#include "Climate/CEClimatePCGSettings.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
DEFINE_LOG_CATEGORY_STATIC(LogCEClimate,Log,All);
namespace
{
class FCEClimateElement : public IPCGElement
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
    // External asset contents can change without a node CRC change. Explicit Regenerate always reevaluates.
    virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const auto* S=Context->GetInputSettings<UCEClimatePCGSettings>();FString Error;
        if(!S||!S->Profile||!S->Profile->Ecosystem.Validate(Error)||!UCEClimatePCGSettings::ValidateFrame(S->PlanetToWorld,S->ReferenceRadiusMeters,Error))
        {UE_LOG(LogCEClimate,Error,TEXT("Climate input rejected: %s"),Error.IsEmpty()?TEXT("Missing ecosystem profile"):*Error);return true;}
        const FCEPlanetEcosystem Model=S->Profile->Ecosystem;
        const auto Inputs=Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
        int64 Count=0;TArray<const UPCGPointData*> PointInputs;
        for(const auto& Input:Inputs)
        {
            const auto* Spatial=Cast<UPCGSpatialData>(Input.Data);const auto* Points=Spatial?Spatial->ToPointData(Context):nullptr;
            if(!Points||(Count+=Points->GetPoints().Num())>65536){UE_LOG(LogCEClimate,Error,TEXT("Climate requires at most 65536 surface points per execution."));return true;}
            PointInputs.Add(Points);
        }
        // Validate the complete batch before publishing anything.
        for(const auto* Points:PointInputs)for(const auto& P:Points->GetPoints())
        {
            const FVector Body=S->PlanetToWorld.InverseTransformPosition(P.Transform.GetLocation())*.01;
            if(Body.ContainsNaN()||!FMath::IsFinite(Body.SizeSquared())||Body.IsNearlyZero()||!P.Transform.GetRotation().IsNormalized())
            {UE_LOG(LogCEClimate,Error,TEXT("Climate surface points require finite positions away from the planet centre and finite rotations."));return true;}
        }
        for(int32 I=0;I<Inputs.Num();++I)
        {
            auto* Out=NewObject<UPCGPointData>();Out->InitializeFromData(PointInputs[I]);
            auto* M=Out->Metadata.Get();
            auto* Temp=M->CreateAttribute<double>(TEXT("CE_TemperatureC"),0,true,true);
            auto* Wet=M->CreateAttribute<double>(TEXT("CE_Moisture"),0,true,true);
            auto* Alt=M->CreateAttribute<double>(TEXT("CE_AltitudeMeters"),0,true,true);
            auto* Lat=M->CreateAttribute<double>(TEXT("CE_LatitudeDegrees"),0,true,true);
            auto* Slope=M->CreateAttribute<double>(TEXT("CE_SlopeDegrees"),0,true,true);
            auto* Water=M->CreateAttribute<bool>(TEXT("CE_Water"),false,false,true);
            auto* Id=M->CreateAttribute<FString>(TEXT("CE_BiomeId"),TEXT(""),false,true);
            auto* Weight=M->CreateAttribute<double>(TEXT("CE_BiomeWeight"),0,true,true);
            if(!Temp||!Wet||!Alt||!Lat||!Slope||!Water||!Id||!Weight){Context->OutputData.TaggedData.Reset();return true;}
            auto& Dest=Out->GetMutablePoints();
            for(const auto& Point:PointInputs[I]->GetPoints())
            {
                const FVector Body=S->PlanetToWorld.InverseTransformPosition(Point.Transform.GetLocation())*.01;
                const FVector Normal=S->PlanetToWorld.InverseTransformVectorNoScale(Point.Transform.GetRotation().GetAxisZ());
                const auto Sample=FCEClimateEvaluator::Evaluate(Model,Body,Normal,S->ReferenceRadiusMeters,S->PlanetSeed);
                if(S->TargetBiome.IsValid()&&Sample.BiomeId!=S->TargetBiome)continue;
                auto& P=Dest.Add_GetRef(Point);P.MetadataEntry=M->AddEntry(Point.MetadataEntry);
                Temp->SetValue(P.MetadataEntry,Sample.TemperatureC);Wet->SetValue(P.MetadataEntry,Sample.Moisture);Alt->SetValue(P.MetadataEntry,Sample.AltitudeMeters);
                Lat->SetValue(P.MetadataEntry,Sample.LatitudeDegrees);Slope->SetValue(P.MetadataEntry,Sample.SlopeDegrees);Water->SetValue(P.MetadataEntry,Sample.bWater);
                Id->SetValue(P.MetadataEntry,Sample.BiomeId.IsValid()?Sample.BiomeId.ToString(EGuidFormats::Digits):FString());Weight->SetValue(P.MetadataEntry,Sample.BiomeWeight);
            }
            auto& Tagged=Context->OutputData.TaggedData.Add_GetRef(Inputs[I]);Tagged.Data=Out;Tagged.Pin=PCGPinConstants::DefaultOutputLabel;
        }
        return true;
    }
};
}
bool UCEClimatePCGSettings::ValidateFrame(const FTransform& F,double R,FString& Error)
{
    const auto S=F.GetScale3D();if(F.ContainsNaN()||!F.GetRotation().IsNormalized()||!FMath::IsFinite(R)||R<=0||S.X<=0||!S.Equals(FVector(S.X),1.e-6))
    {Error=TEXT("Planet climate requires a finite rigid transform, positive uniform scale and positive reference radius in metres.");return false;}
    return true;
}
TArray<FPCGPinProperties> UCEClimatePCGSettings::InputPinProperties() const{return {FPCGPinProperties(PCGPinConstants::DefaultInputLabel,EPCGDataType::Point)};}
TArray<FPCGPinProperties> UCEClimatePCGSettings::OutputPinProperties() const{return {FPCGPinProperties(PCGPinConstants::DefaultOutputLabel,EPCGDataType::Point)};}
FPCGElementPtr UCEClimatePCGSettings::CreateElement() const{return MakeShared<FCEClimateElement>();}
