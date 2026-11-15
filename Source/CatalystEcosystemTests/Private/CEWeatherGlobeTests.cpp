#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "Weather/CEWeatherGlobe.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherGlobeTest,"Catalyst.Ecosystem.Weather.GlobeTopologyAndGuide",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherGlobeTest::RunTest(const FString&)
{
    const auto M=FCEWeatherGlobeMesh::Make();TestTrue(TEXT("Bounded globe mesh"),M.Directions.Num()<8192&&M.Indices.Num()<=49152);
    TMap<uint64,int32> Edges;
    for(int32 I=0;I<M.Indices.Num();I+=3)
    {
        const auto A=M.Directions[M.Indices[I]],B=M.Directions[M.Indices[I+1]],C=M.Directions[M.Indices[I+2]];
        TestTrue(TEXT("Outward globe faces"),FVector::DotProduct(FVector::CrossProduct(B-A,C-A),A+B+C)>0);
        for(int32 E=0;E<3;++E){const int32 AIndex=M.Indices[I+E],BIndex=M.Indices[I+(E+1)%3];Edges.FindOrAdd(uint64(FMath::Min(AIndex,BIndex))<<32|uint32(FMath::Max(AIndex,BIndex)))++;}
    }
    for(const auto& Pair:Edges)TestEqual(TEXT("Closed topology has no longitude seam or pole boundary"),Pair.Value,2);
    TArray<FCEWeatherSample> Samples;
    for(const auto& D:M.Directions){auto& S=Samples.AddDefaulted_GetRef();S.PressureHpa=1013.25+12*D.Z+9*D.X*D.Y;S.TemperatureC=D.Z*40;S.RainMmHour=(1+D.X)*4;S.Wind=FVector::VectorPlaneProject(FVector(1,2,0),D)*6;}
    const double Begin=FPlatformTime::Seconds();const auto O=CEBuildWeatherGlobe(M,Samples,1500,1013.25,0);
    TestEqual(TEXT("Continuous colour mesh retains shared topology"),O->MeshIndices,M.Indices);
    TestTrue(TEXT("Contours, labels and contour-anchored arrows exist"),O->Segments.Num()>0&&O->Labels.Num()>=2&&O->BodyVectorsMeters.Num()>0);
    TestTrue(TEXT("Bounded overlay work"),O->Segments.Num()<=16384&&O->Labels.Num()<=16&&O->BodyVectorsMeters.Num()<=256);
    for(const auto& P:O->MeshBodyMeters)TestTrue(TEXT("Heatmap is just behind the Overlay shell"),FMath::IsNearlyEqual(P.Length(),1498.5,1.e-6));
    TMap<FIntVector,int32> ContourEnds;
    for(const auto& L:O->Segments)for(const auto& P:{L.A,L.B})
    {
        TestTrue(TEXT("Isobars lie on the guide"),FMath::IsNearlyEqual(P.Length(),1500.,1.e-6));
        ContourEnds.FindOrAdd(FIntVector(FMath::RoundToInt(P.X*10000),FMath::RoundToInt(P.Y*10000),FMath::RoundToInt(P.Z*10000)))++;
    }
    for(const auto& End:ContourEnds)TestEqual(TEXT("Pressure contours join without seam breaks"),End.Value,2);
    for(int32 I=0;I<O->BodyMeters.Num();++I){TestTrue(TEXT("Wind anchors lie on guide"),FMath::IsNearlyEqual(O->BodyMeters[I].Length(),1500.,1.e-6));TestTrue(TEXT("Wind arrow is tangent to shell"),FMath::Abs(FVector::DotProduct(O->BodyMeters[I].GetSafeNormal(),O->BodyVectorsMeters[I]))<1.e-6);}
    const auto Off=CEBuildWeatherGlobe(M,Samples,1500,1013.25,0,false,false,false);TestTrue(TEXT("Layer visibility is presentation only"),Off->MeshIndices.IsEmpty()&&Off->Segments.IsEmpty()&&Off->Labels.IsEmpty()&&Off->BodyVectorsMeters.IsEmpty());
    FString Error;auto Recipe=FCLPlanetCompiler::BasePlanet();Recipe.RadiusMeters=2000;auto A=FCLPlanetCompiler::Compile(Recipe,Error);if(!TestTrue(TEXT("Guide fixture compiles"),A.IsValid()))return false;
    TestEqual(TEXT("Default Overlay matches atmosphere guide"),CLPlanetOverlayRadiusMeters(*A),1800.);
    for(auto& G:Recipe.RadialGuides.Guides)if(G.Name==TEXT("Overlay"))G.NormalizedHeight=.98;
    auto B=FCLPlanetCompiler::Compile(Recipe,Error);if(!TestTrue(TEXT("Moved fixture compiles"),B.IsValid()))return false;
    TestTrue(TEXT("Moving Overlay preserves terrain input identity"),CEWeatherTerrainInputsMatch(*A,*B));TestEqual(TEXT("Moved radius"),CLPlanetOverlayRadiusMeters(*B),1960.);
    Recipe.RadialGuides.Guides.RemoveAll([](const auto& G){return G.Name==TEXT("Overlay");});B=FCLPlanetCompiler::Compile(Recipe,Error);if(!B)return false;
    TestEqual(TEXT("No Overlay uses sky even with AtmosphereMax present"),CLPlanetOverlayRadiusMeters(*B),2000.);TestTrue(TEXT("Removing Overlay preserves terrain identity"),CEWeatherTerrainInputsMatch(*A,*B));
    Recipe.Seed++;B=FCLPlanetCompiler::Compile(Recipe,Error);TestTrue(TEXT("Terrain changes require a new weather input generation"),B&&!CEWeatherTerrainInputsMatch(*A,*B));
    Recipe=FCLPlanetCompiler::BasePlanet();FCLNoiseLayer Bound;Bound.Name=TEXT("Guide-bound terrain");Bound.Minimum.Guide=TEXT("Overlay");Bound.Maximum.Guide=TEXT("Overlay");Recipe.NoiseLayers.Add(Bound);
    A=FCLPlanetCompiler::Compile(Recipe,Error);for(auto& G:Recipe.RadialGuides.Guides)if(G.Name==TEXT("Overlay"))G.NormalizedHeight=.98;B=FCLPlanetCompiler::Compile(Recipe,Error);
    TestTrue(TEXT("An Overlay used by terrain cannot reuse old weather inputs"),A&&B&&!CEWeatherTerrainInputsMatch(*A,*B));
    AddInfo(FString::Printf(TEXT("%d globe vertices, %d contour segments, %d arrows; geometry and guide checks %.3f ms"),M.Directions.Num(),O->Segments.Num(),O->BodyVectorsMeters.Num(),(FPlatformTime::Seconds()-Begin)*1000));return true;
}
#endif
