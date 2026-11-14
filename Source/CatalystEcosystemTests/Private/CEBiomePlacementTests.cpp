#include "Misc/AutomationTest.h"
#include "Climate/CEBiomePlacement.h"
#include "Algo/Reverse.h"
namespace
{
TArray<FCEBiomePlacementCell> Sphere(double Radius=500)
{auto Cells=FCEBiomePlacementPlan::MakeLattice();for(auto& C:Cells){C.PositionMeters*=Radius;C.bSupported=true;}return Cells;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEBiomeCoverageGuarantee,"Catalyst.Ecosystem.Placement.EnabledCoverageAndRelaxation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEBiomeCoverageGuarantee::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();M.Climate.EquatorTemperatureC=35;M.Climate.PoleTemperatureC=35;M.Climate.TemperatureVariationC=0;M.Climate.Moisture=0;M.Climate.MoistureVariation=0;
    FString Error;const auto Plan=FCEBiomePlacementPlan::Build(M,Sphere(),500,71,Error);
    if(!TestTrue(*Error,Plan.IsValid()))return false;
    TestTrue(TEXT("Exact cube-sphere areas sum to a sphere"),FMath::Abs(Plan->SupportedSolidAngle-4*FMath::Acos(-1.))<1.e-10);
    TestEqual(TEXT("Every enabled biome receives a region even on a uniform hot dry planet"),Plan->Regions.Num(),M.Biomes.Num());
    TSet<FGuid> Assigned;
    for(int32 I=0;I<Plan->Cells.Num();++I)
    {
        const auto& C=Plan->Cells[I];Assigned.Add(C.BiomeId);
        if(!TestEqual(TEXT("Cell lookup is consistent at every canonical cell centre"),FCEBiomePlacementPlan::CellIndex(C.PositionMeters),I))return false;
        TestEqual(TEXT("Placement query uses the shared assigned cell"),Plan->Evaluate(C.PositionMeters,C.Normal).BiomeId,C.BiomeId);
    }
    TestEqual(TEXT("Actual assignment contains every region"),Assigned.Num(),M.Biomes.Num());
    for(const auto& R:Plan->Regions)
    {
        TestTrue(TEXT("Reserved and final area both meet the one percent minimum"),R.ReservedSolidAngle/Plan->SupportedSolidAngle>=.01-1.e-10&&R.AssignedSolidAngle/Plan->SupportedSolidAngle>=.01-1.e-10);
        TestTrue(TEXT("Meaningful multi-cell patch"),R.ReservedCells>20);
        TestEqual(TEXT("Connected reserve on a complete surface"),R.Components,1);
        if(R.BiomeId==M.Biomes[1].Id)TestTrue(TEXT("Impossible polar habitat relaxes rather than disappearing"),R.MaximumHabitatPenalty>0);
    }
    const auto Climate=Plan->Evaluate(FVector(500,0,0),FVector::XAxisVector);
    TestEqual(TEXT("Relaxation never rewrites factual temperature"),Climate.TemperatureC,35.);
    TestFalse(TEXT("An ocean reservation does not invent water"),Climate.bWater);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEBiomePlacementDeterminism,"Catalyst.Ecosystem.Placement.OrderDisabledAndSurfaceChanges",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEBiomePlacementDeterminism::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();M.Biomes[3].bEnabled=false;FString Error;
    const auto A=FCEBiomePlacementPlan::Build(M,Sphere(),500,1729,Error);auto Reversed=M;Algo::Reverse(Reversed.Biomes);const auto B=FCEBiomePlacementPlan::Build(Reversed,Sphere(),500,1729,Error);
    if(!TestTrue(*Error,A.IsValid()&&B.IsValid()))return false;
    TestEqual(TEXT("Assignment digest is independent of row order"),A->AssignmentKey,B->AssignmentKey);
    for(int32 I=0;I<A->Cells.Num();++I)
    {
        TestEqual(TEXT("Stable GUID tie-breaking ignores row order"),A->Cells[I].BiomeId,B->Cells[I].BiomeId);
        TestNotEqual(TEXT("Disabled biome never claims a cell"),A->Cells[I].BiomeId,M.Biomes[3].Id);
    }
    auto Terrain=Sphere();for(auto& C:Terrain)if(C.PositionMeters.X>0)C.PositionMeters*=1.7;
    M.Biomes[4].AltitudeMeters={100,1000};const auto Changed=FCEBiomePlacementPlan::Build(M,MoveTemp(Terrain),500,1729,Error);
    if(!TestTrue(*Error,Changed.IsValid()))return false;
    const auto* Forest=Changed->Regions.FindByPredicate([&](const auto& R){return R.BiomeId==M.Biomes[4].Id;});
    TestTrue(TEXT("Altitude preference places the seed on actual raised terrain"),Forest&&Changed->Cells[Forest->SeedCell].PositionMeters.Length()>800);
    auto AllOff=M;for(auto& V:AllOff.Biomes)V.bEnabled=false;const auto Off=FCEBiomePlacementPlan::Build(AllOff,Sphere(),500,1729,Error);
    TestTrue(TEXT("All disabled produces no regions"),Off&&Off->Regions.IsEmpty());
    if(Off)for(const auto& C:Off->Cells)TestFalse(TEXT("No disabled fallback"),C.BiomeId.IsValid());
    auto Empty=Sphere();for(auto& C:Empty)C.bSupported=false;
    TestFalse(TEXT("No supported ground refuses an impossible guarantee"),FCEBiomePlacementPlan::Build(M,MoveTemp(Empty),500,1729,Error).IsValid());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEBiomePlacementMaximum,"Catalyst.Ecosystem.Placement.MaximumCompetingBiomes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEBiomePlacementMaximum::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();M.Biomes.Reset();
    for(int32 I=0;I<64;++I){FCEBiomeDefinition B;B.Id=FGuid(1,2,3,I+1);B.Name=FString::FromInt(I);B.Priority=I;M.Biomes.Add(B);}
    FString Error;const auto Plan=FCEBiomePlacementPlan::Build(M,Sphere(),500,72,Error);
    if(!TestTrue(*Error,Plan.IsValid()))return false;
    TestEqual(TEXT("Every competing enabled row represented"),Plan->Regions.Num(),64);
    for(const auto& R:Plan->Regions)TestTrue(TEXT("No priority winner can starve another biome"),R.AssignedSolidAngle/Plan->SupportedSolidAngle>=.01-1.e-10);
    return true;
}
