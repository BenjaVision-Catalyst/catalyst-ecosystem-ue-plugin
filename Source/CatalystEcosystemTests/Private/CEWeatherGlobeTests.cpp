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
    for(const auto& D:M.Directions){auto& S=Samples.AddDefaulted_GetRef();S.PressureHpa=1013.25+12*D.Z+9*D.X*D.Y;S.TemperatureC=D.Z*40;S.RainMmHour=(1+D.X)*4;S.Wind=FVector::CrossProduct(FVector::ZAxisVector,D)*6;}
    const double Begin=FPlatformTime::Seconds();const auto O=CEBuildWeatherGlobe(M,Samples,1500,1013.25,0,true,true,true,false);
    TestEqual(TEXT("Continuous colour mesh retains shared topology"),O->MeshIndices,M.Indices);
    TestTrue(TEXT("Contours, labels and contour-anchored arrows exist"),O->Segments.Num()>0&&O->Labels.Num()>=2&&O->Flows.Num()>0);
    TestTrue(TEXT("Bounded overlay work"),O->Segments.Num()<=16384&&O->Labels.Num()<=16&&O->Flows.Num()<=256);
    for(const auto& P:O->MeshBodyMeters)TestTrue(TEXT("Heatmap is just behind the Overlay shell"),FMath::IsNearlyEqual(P.Length(),1498.5,1.e-6));
    TMap<FIntVector,int32> ContourEnds;
    for(const auto& L:O->Segments)for(const auto& P:{L.A,L.B})
    {
        TestTrue(TEXT("Isobars lie on the guide"),FMath::IsNearlyEqual(P.Length(),1500.,1.e-6));
        ContourEnds.FindOrAdd(FIntVector(FMath::RoundToInt(P.X*10000),FMath::RoundToInt(P.Y*10000),FMath::RoundToInt(P.Z*10000)))++;
    }
    for(const auto& End:ContourEnds)TestEqual(TEXT("Pressure contours join without seam breaks"),End.Value,2);
    int32 Heads=0,EdgesInFlows=0;
    for(const auto& Flow:O->Flows)
    {
        TestTrue(TEXT("Each marching contour is closed"),Flow.BodyMeters[0].Equals(Flow.BodyMeters.Last(),1.e-7));
        Heads+=Flow.ArrowCount;EdgesInFlows+=Flow.BodyMeters.Num()-1;
        for(int32 I=0;I<Flow.ArrowCount;++I)
        {
            FVector Point,Tangent;TestTrue(TEXT("Marching samples exist"),Flow.Sample((I+.37)*Flow.LengthMeters()/Flow.ArrowCount,Point,Tangent));
            TestTrue(TEXT("Arrowheads remain on the Overlay guide"),FMath::IsNearlyEqual(Point.Length(),1500.,1.e-6));
            TestTrue(TEXT("Arrowhead follows the contour tangent"),FMath::Abs(FVector::DotProduct(Point.GetSafeNormal(),Tangent))<1.e-8);
        }
    }
    TestEqual(TEXT("Every topology segment participates in a marching loop"),EdgesInFlows,O->Segments.Num());
    TestTrue(TEXT("Arrowhead work is bounded"),Heads>0&&Heads<=2048);
    for(const auto& Label:O->Labels)TestTrue(TEXT("Pressure labels have no numeric values"),Label.Text==TEXT("H")||Label.Text==TEXT("L"));
    const auto Off=CEBuildWeatherGlobe(M,Samples,1500,1013.25,0,false,false,false,false);TestTrue(TEXT("Layer visibility is presentation only"),Off->MeshIndices.IsEmpty()&&Off->Segments.IsEmpty()&&Off->Labels.IsEmpty()&&Off->Flows.IsEmpty());
    auto Faster=Samples;for(auto& S:Faster)S.Wind*=2;const auto Fast=CEBuildWeatherGlobe(M,Faster,1500,1013.25,0,true,true,true,false);
    auto Reverse=Samples;for(auto& S:Reverse)S.Wind*=-1;const auto Back=CEBuildWeatherGlobe(M,Reverse,1500,1013.25,0,true,true,true,false);
    TestEqual(TEXT("Speed changes preserve the contour count"),Fast->Flows.Num(),O->Flows.Num());
    for(int32 I=0;I<O->Flows.Num();++I)
    {
        TestTrue(TEXT("Double wind doubles marching speed"),FMath::IsNearlyEqual(Fast->Flows[I].SpeedMetersPerSecond,O->Flows[I].SpeedMetersPerSecond*2,1.e-8));
        TestTrue(TEXT("Reversed wind reverses the marching direction"),FMath::IsNearlyEqual(Back->Flows[I].SpeedMetersPerSecond,-O->Flows[I].SpeedMetersPerSecond,1.e-8));
    }
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
    AddInfo(FString::Printf(TEXT("%d globe vertices, %d contour segments, %d arrows; geometry and guide checks %.3f ms"),M.Directions.Num(),O->Segments.Num(),Heads,(FPlatformTime::Seconds()-Begin)*1000));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEContourMotionTest,"Catalyst.Ecosystem.Weather.ContourMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEContourMotionTest::RunTest(const FString&)
{
    FCLPlanetBuilderOverlayFlow Flow;const double Radius=1000;
    for(int32 I=0;I<8;++I){const double A=I*UE_DOUBLE_PI/4;Flow.BodyMeters.Add(FVector(FMath::Cos(A),FMath::Sin(A),0)*Radius);}Flow.BodyMeters.Add(FVector(Flow.BodyMeters[0]));
    TestTrue(TEXT("Closed circle builds"),Flow.BuildArcLengths());Flow.ArrowCount=16;Flow.SpeedMetersPerSecond=10;
    TestTrue(TEXT("Arc length is a full circumference"),FMath::IsNearlyEqual(Flow.LengthMeters(),2*UE_DOUBLE_PI*Radius,1.e-7));
    const double Step=Flow.LengthMeters()/Flow.ArrowCount;
    for(int32 I=0;I<Flow.ArrowCount;++I)
    {
        FVector P,T,Q,U;Flow.Sample((I+.27)*Step,P,T);Flow.Sample((I+1+.27)*Step,Q,U);
        TestTrue(TEXT("Arrowheads have equal spherical arc spacing across the seam"),FMath::IsNearlyEqual(FMath::Acos(FVector::DotProduct(P,Q)/(Radius*Radius))*Radius,Step,1.e-7));
        TestTrue(TEXT("Flow points forward around the circle"),FVector::DotProduct(T,FVector(-P.Y,P.X,0))>0);
    }
    const double Phase=Flow.AdvancePhase(.9,20);TestTrue(TEXT("Phase depends on elapsed time, not frame count"),FMath::IsNearlyEqual(Phase,Flow.AdvancePhase(Flow.AdvancePhase(.9,7),13),1.e-12));
    TestTrue(TEXT("Stationary display time leaves the phase unchanged"),FMath::IsNearlyEqual(Flow.AdvancePhase(.3,0),.3));
    Flow.SpeedMetersPerSecond=-10;TestTrue(TEXT("Reverse motion wraps without a jump"),FMath::IsNearlyEqual(Flow.AdvancePhase(.1,Step/10),.1,1.e-12));
    Flow.SpeedMetersPerSecond=0;TestEqual(TEXT("Calm wind is stationary"),Flow.AdvancePhase(.42,3600),.42);
    Flow.BodyMeters.Pop();TestFalse(TEXT("Open paths cannot introduce a seam jump"),Flow.BuildArcLengths());return true;
}

#endif
