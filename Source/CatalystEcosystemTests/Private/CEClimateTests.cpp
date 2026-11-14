#include <limits>
#include "Misc/AutomationTest.h"
#include "Model/CEPlanetEcosystem.h"
#include "Climate/CEClimatePCGSettings.h"
#include "Climate/CEClimateQueryLibrary.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "UObject/StrongObjectPtr.h"
#include "Algo/Reverse.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEClimateTest,"Catalyst.Ecosystem.Climate.DeterminismAndSphericalFrame",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEClimateTest::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();FString Error;TestTrue(TEXT("Default model validates"),M.Validate(Error));
    auto Reverse=M;Algo::Reverse(Reverse.Biomes);TSet<FGuid> Found;
    for(int32 I=0;I<2048;++I)
    {
        const double Z=1-2*(I+.5)/2048,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);
        const auto S=FCEClimateEvaluator::Evaluate(M,D*100,D,100,1729),R=FCEClimateEvaluator::Evaluate(Reverse,D*100,D,100,1729),Again=FCEClimateEvaluator::Evaluate(M,D*100,D,100,1729);
        TestEqual(TEXT("Stable point classification"),S.BiomeId,Again.BiomeId);TestEqual(TEXT("Row order does not affect biome"),S.BiomeId,R.BiomeId);TestEqual(TEXT("Climate repeats exactly"),S.TemperatureC,Again.TemperatureC);
        TestTrue(TEXT("Every default land point has a biome"),S.BiomeId.IsValid());TestTrue(TEXT("Radial normals have zero slope"),S.SlopeDegrees<1.e-5);Found.Add(S.BiomeId);
    }
    TestTrue(TEXT("Preview has multiple habitats"),Found.Num()>=5);
    M.Climate.TemperatureVariationC=0;M.Climate.MoistureVariation=0;
    auto Equator=FCEClimateEvaluator::Evaluate(M,FVector(100,0,0),FVector(1,0,0),100,1);
    auto Pole=FCEClimateEvaluator::Evaluate(M,FVector(0,0,100),FVector(0,0,1),100,1);
    TestEqual(TEXT("Equator temperature"),Equator.TemperatureC,30.);TestEqual(TEXT("Pole temperature"),Pole.TemperatureC,-25.);
    M.Climate.PoleAxis=FVector::XAxisVector;auto Tilt=FCEClimateEvaluator::Evaluate(M,FVector(100,0,0),FVector(1,0,0),100,1);TestEqual(TEXT("Climate follows chosen pole"),Tilt.TemperatureC,Pole.TemperatureC);
    M.Climate.PoleAxis=FVector::ZAxisVector;
    auto High=FCEClimateEvaluator::Evaluate(M,FVector(1100,0,0),FVector(1,0,0),100,1);TestEqual(TEXT("1000 metres applies lapse rate"),High.TemperatureC,23.5);TestEqual(TEXT("Altitude uses planet radius"),High.AltitudeMeters,1000.);
    M.Climate.SeaLevelMeters=2;auto Sea=FCEClimateEvaluator::Evaluate(M,FVector(100,0,0),FVector(1,0,0),100,1);TestTrue(TEXT("Water uses sea-level offset"),Sea.bWater);TestEqual(TEXT("Ocean selected"),Sea.BiomeId,M.Biomes[0].Id);
    auto SeamA=FCEClimateEvaluator::Evaluate(M,FVector(-100,1.e-7,0),FVector(-1,0,0),100,17),SeamB=FCEClimateEvaluator::Evaluate(M,FVector(-100,-1.e-7,0),FVector(-1,0,0),100,17);
    TestTrue(TEXT("No longitude seam"),FMath::Abs(SeamA.TemperatureC-SeamB.TemperatureC)<1.e-6&&FMath::Abs(SeamA.Moisture-SeamB.Moisture)<1.e-6);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCECoverageQueryTest,"Catalyst.Ecosystem.Climate.CoverageQueryMatchesPreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCECoverageQueryTest::RunTest(const FString&)
{
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile(NewObject<UCEPlanetEcosystemAsset>());
    TArray<FVector> P,N;TArray<FGuid> Ids;
    for(int32 I=0;I<2048;++I)
    {
        const double Z=1-2*(I+.5)/2048,A=I*2.399963229728653;
        const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);
        P.Add(D*(100+I%30));N.Add(D);
    }
    TestTrue(TEXT("Body-space query accepts a complete globe"),UCEClimateQueryLibrary::EvaluateSurface(Profile.Get(),P,N,100,1729,5,Ids));
    auto Model=Profile->Ecosystem;Model.Climate.SeaLevelMeters=5;
    if(!TestEqual(TEXT("Every position classified"),Ids.Num(),P.Num()))return false;
    for(int32 I=0;I<P.Num();++I)TestEqual(TEXT("Coverage material query uses the preview evaluator"),Ids[I],FCEClimateEvaluator::Evaluate(Model,P[I],N[I],100,1729).BiomeId);
    N.Pop();TestFalse(TEXT("Mismatched normals rejected atomically"),UCEClimateQueryLibrary::EvaluateSurface(Profile.Get(),P,N,100,1729,5,Ids));
    TestTrue(TEXT("Invalid batch publishes no partial coverage"),Ids.IsEmpty());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEValidationTest,"Catalyst.Ecosystem.Climate.ValidationAndModelRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEValidationTest::RunTest(const FString&)
{
    FString Error;auto M=FCEPlanetEcosystem::MakeDefault();M.Climate.PoleAxis=FVector::ZeroVector;TestFalse(TEXT("Reject zero pole"),M.Validate(Error));
    M=FCEPlanetEcosystem::MakeDefault();M.Biomes[1].Id=M.Biomes[0].Id;TestFalse(TEXT("Reject duplicate IDs"),M.Validate(Error));
    M=FCEPlanetEcosystem::MakeDefault();M.Biomes[1].Moisture=FVector2D(.8,.2);TestFalse(TEXT("Reject reversed range"),M.Validate(Error));
    M=FCEPlanetEcosystem::MakeDefault();M.Climate.Moisture=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("Reject NaN"),M.Validate(Error));
    TStrongObjectPtr<UCEPlanetEcosystemAsset> A(NewObject<UCEPlanetEcosystemAsset>()),B(NewObject<UCEPlanetEcosystemAsset>());A->Ecosystem.Climate.Seed=1907;
    FString Json;TestTrue(TEXT("Foundation export"),A->ExportJsonString(Json,Error));TestTrue(TEXT("Foundation import"),B->ApplyJsonString(Json,Error));TestEqual(TEXT("Climate seed retained"),B->Ecosystem.Climate.Seed,1907);TestEqual(TEXT("Biome identities retained"),B->Ecosystem.Biomes[4].Id,A->Ecosystem.Biomes[4].Id);
    TestFalse(TEXT("Reject nonuniform scale"),UCEClimatePCGSettings::ValidateFrame(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(1,2,1)),100,Error));
    TestFalse(TEXT("Reject negative radius"),UCEClimatePCGSettings::ValidateFrame(FTransform::Identity,-1,Error));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPCGTest,"Catalyst.Ecosystem.PCG.FieldsFilterAndPlanetTransform",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPCGTest::RunTest(const FString&)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>());UCEClimatePCGSettings* Settings=nullptr;auto* Node=Graph->AddNodeOfType(Settings);
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile(NewObject<UCEPlanetEcosystemAsset>());Settings->Profile=Profile.Get();Settings->ReferenceRadiusMeters=100;
    const FTransform Frame(FQuat(FVector::XAxisVector,.7),FVector(12345,-6789,4567),FVector(2));Settings->PlanetToWorld=Frame;
    TStrongObjectPtr<UPCGPointData> Data(NewObject<UPCGPointData>());auto& Points=Data->GetMutablePoints();
    for(FVector D:{FVector::XAxisVector,FVector::YAxisVector,FVector::ZAxisVector,-FVector::ZAxisVector})
    {FPCGPoint P;P.Transform=FTransform(Frame.GetRotation()*FQuat::FindBetweenNormals(FVector::ZAxisVector,D),Frame.TransformPosition(D*10000));P.Seed=Points.Num()+51;P.Density=.7;Points.Add(P);}
    auto Run=[&]()->TUniquePtr<FPCGContext>
    {
        FPCGDataCollection Input;auto& Tagged=Input.TaggedData.AddDefaulted_GetRef();Tagged.Data=Data.Get();Tagged.Pin=TEXT("In");
        auto Element=Settings->GetElement();TUniquePtr<FPCGContext> C(Element->Initialize(FPCGInitializeElementParams(&Input,{},Node)));C->bIsPaused=false;C->AsyncState.NumAvailableTasks=1;
        int32 Steps=0;while(!Element->Execute(C.Get())&&++Steps<100){}return C;
    };
    auto Context=Run();if(!TestEqual(TEXT("One output data"),Context->OutputData.TaggedData.Num(),1))return false;
    const auto* Out=Cast<UPCGPointData>(Context->OutputData.TaggedData[0].Data);if(!TestNotNull(TEXT("Point output"),Out))return false;
    TestEqual(TEXT("Every point preserved"),Out->GetPoints().Num(),4);
    auto* Temp=Out->Metadata->GetConstTypedAttribute<double>(TEXT("CE_TemperatureC"));auto* Id=Out->Metadata->GetConstTypedAttribute<FString>(TEXT("CE_BiomeId"));
    if(!TestNotNull(TEXT("Temperature field"),Temp)||!TestNotNull(TEXT("Biome field"),Id))return false;
    for(int32 I=0;I<4;++I)
    {
        const auto& P=Out->GetPoints()[I];const FVector Body=Frame.InverseTransformPosition(P.Transform.GetLocation())*.01;
        auto S=FCEClimateEvaluator::Evaluate(Profile->Ecosystem,Body,Frame.InverseTransformVectorNoScale(P.Transform.GetRotation().GetAxisZ()),100,1729);
        TestEqual(TEXT("PCG temperature equals preview evaluator"),Temp->GetValueFromItemKey(P.MetadataEntry),S.TemperatureC);TestEqual(TEXT("PCG biome equals preview evaluator"),Id->GetValueFromItemKey(P.MetadataEntry),S.BiomeId.ToString(EGuidFormats::Digits));
        TestEqual(TEXT("Point seeds preserved"),P.Seed,Points[I].Seed);TestEqual(TEXT("Point density preserved"),P.Density,Points[I].Density);
    }
    Settings->TargetBiome=FGuid(0x43454249,0,0,2);Context=Run();const auto* Filtered=Cast<UPCGPointData>(Context->OutputData.TaggedData[0].Data);TestEqual(TEXT("Biome filter keeps both poles"),Filtered->GetPoints().Num(),2);
    Profile->Ecosystem.Climate.PoleTemperatureC=30;Context=Run();Filtered=Cast<UPCGPointData>(Context->OutputData.TaggedData[0].Data);TestEqual(TEXT("Regenerate sees edited profile"),Filtered->GetPoints().Num(),0);
    Settings->TargetBiome.Invalidate();Profile->Ecosystem.Climate.SeaLevelMeters=-10;
    Settings->bOverrideSeaLevel=true;Settings->SeaLevelMeters=5;Context=Run();
    Out=Cast<UPCGPointData>(Context->OutputData.TaggedData[0].Data);
    auto* Water=Out->Metadata->GetConstTypedAttribute<bool>(TEXT("CE_Water"));
    for(const auto& P:Out->GetPoints())TestTrue(TEXT("Generated OceanHeight context overrides the profile water datum"),Water->GetValueFromItemKey(P.MetadataEntry));
    TestEqual(TEXT("Guide-derived execution does not rewrite the saved profile"),Profile->Ecosystem.Climate.SeaLevelMeters,-10.);
    Settings->bOverrideSeaLevel=false;Context=Run();Out=Cast<UPCGPointData>(Context->OutputData.TaggedData[0].Data);
    Water=Out->Metadata->GetConstTypedAttribute<bool>(TEXT("CE_Water"));
    for(const auto& P:Out->GetPoints())TestFalse(TEXT("Standalone PCG retains its explicit profile datum"),Water->GetValueFromItemKey(P.MetadataEntry));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPCGBatchesTest,"Catalyst.Ecosystem.PCG.PlanetarySurfaceBatches",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPCGBatchesTest::RunTest(const FString&)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>());UCEClimatePCGSettings* S=nullptr;auto* Node=Graph->AddNodeOfType(S);
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile(NewObject<UCEPlanetEcosystemAsset>());S->Profile=Profile.Get();S->ReferenceRadiusMeters=100;
    TStrongObjectPtr<UPCGPointData> A(NewObject<UPCGPointData>()),B(NewObject<UPCGPointData>());
    for(auto* Data:{A.Get(),B.Get()})
    {
        auto& Points=Data->GetMutablePoints();Points.SetNum(40000);
        const FVector D=Data==A.Get()?FVector::ZAxisVector:FVector::XAxisVector;
        for(int32 I=0;I<Points.Num();++I){Points[I].Transform=FTransform(FQuat::FindBetweenNormals(FVector::ZAxisVector,D),D*10000);Points[I].Seed=I;}
    }
    FPCGDataCollection In;for(auto* Data:{A.Get(),B.Get()}){auto& T=In.TaggedData.AddDefaulted_GetRef();T.Data=Data;T.Pin=TEXT("In");}
    auto Element=S->GetElement();TUniquePtr<FPCGContext> C(Element->Initialize(FPCGInitializeElementParams(&In,{},Node)));C->AsyncState.NumAvailableTasks=1;
    int32 Steps=0;while(!Element->Execute(C.Get())&&++Steps<100){}
    if(!TestEqual(TEXT("A planetary tile accepts multiple bounded batches"),C->OutputData.TaggedData.Num(),2))return false;
    for(int32 I=0;I<2;++I)
    {
        const auto* Out=Cast<UPCGPointData>(C->OutputData.TaggedData[I].Data);if(!TestNotNull(TEXT("Native point data output"),Out))return false;
        TestEqual(TEXT("Every batch retains all of its points"),Out->GetPoints().Num(),40000);
        const FVector D=I==0?FVector::ZAxisVector:FVector::XAxisVector;
        const FString Expected=FCEClimateEvaluator::Evaluate(Profile->Ecosystem,D*100,D,100,1729).BiomeId.ToString(EGuidFormats::Digits);
        const auto* Id=Out->Metadata->GetConstTypedAttribute<FString>(TEXT("CE_BiomeId"));if(!TestNotNull(TEXT("Every batch has classification metadata"),Id))return false;
        TestEqual(TEXT("Last point classification agrees with preview"),Id->GetValueFromItemKey(Out->GetPoints().Last().MetadataEntry),Expected);
    }
    return true;
}
