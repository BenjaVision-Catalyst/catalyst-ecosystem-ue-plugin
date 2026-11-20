#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "Weather/CEWeatherGlobe.h"
#include "Algo/Reverse.h"
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

#if WITH_CE_LANDFORM
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPersistentArrowTest,"Catalyst.Ecosystem.Weather.PersistentArrows",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPersistentArrowTest::RunTest(const FString&)
{
    FCLPlanetBuilderOverlayFlow F;F.ArrowCount=8;F.SpeedMetersPerSecond=10;
    for(int32 I=0;I<=64;++I){const double A=I*2*UE_DOUBLE_PI/64;F.BodyMeters.Add(FVector(FMath::Cos(A),FMath::Sin(A),0)*1000);}F.BodyMeters.Last()=F.BodyMeters[0];F.BuildArcLengths();
    FCLPlanetBuilderFlowAnimation A;A.SetFlow(F);A.Tick(1);const auto Before=A.Heads;F.ArrowCount=9;A.SetFlow(F);
    TestEqual(TEXT("One extra site is admitted"),A.Heads.Num(),9);TestEqual(TEXT("New arrow starts transparent"),A.Heads.Last().Opacity,0.f);
    for(int32 I=0;I<Before.Num();++I){TestEqual(TEXT("Existing arrow identity survives count change"),A.Heads[I].Id,Before[I].Id);TestTrue(TEXT("Count change does not redistribute existing arrows"),FMath::IsNearlyEqual(A.Heads[I].DistanceMeters,Before[I].DistanceMeters,1.e-6));}
    A.Tick(.3);TestTrue(TEXT("Birth fades through partial opacity"),A.Heads.Last().Opacity>0&&A.Heads.Last().Opacity<1);F.ArrowCount=8;const auto Grown=A.Heads;A.SetFlow(F);int32 Retiring=0;
    for(int32 I=0;I<A.Heads.Num();++I){Retiring+=A.Heads[I].Retiring;TestTrue(TEXT("Contraction preserves every current position"),FMath::IsNearlyEqual(A.Heads[I].DistanceMeters,Grown[I].DistanceMeters,1.e-6));}TestEqual(TEXT("Only one arrow retires"),Retiring,1);A.Tick(1);TestEqual(TEXT("Retirement completes"),A.Heads.Num(),8);
    // Extraction can rotate its seam or reverse traversal without moving a site.
    TArray<FVector> Sites;for(const auto& H:A.Heads){FVector P,T;A.Flow.Sample(H.DistanceMeters,P,T);Sites.Add(P);}
    Algo::Reverse(F.BodyMeters);F.BuildArcLengths();F.SpeedMetersPerSecond=-10;A.SetFlow(F);
    for(int32 I=0;I<A.Heads.Num();++I){FVector P,T;A.Flow.Sample(A.Heads[I].DistanceMeters,P,T);TestTrue(TEXT("Reversed extraction preserves physical marker positions"),P.Equals(Sites[I],1.e-6));}
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEDayNightTest,"Catalyst.Ecosystem.Weather.DayNightAndSurfaceHeatmap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEDayNightTest::RunTest(const FString&)
{
    const double Period=7200;const FVector Pole=FVector::ZAxisVector,Sun=CEWeatherSunDirection(Pole,Period,0);
    TestTrue(TEXT("Sun makes one complete turn per configured period"),Sun.Equals(CEWeatherSunDirection(Pole,Period,Period),1.e-10));TestTrue(TEXT("Night is the opposite solar direction"),Sun.Equals(-CEWeatherSunDirection(Pole,Period,Period*.5),1.e-10));
    auto Model=FCEPlanetEcosystem::MakeDefault();Model.Weather.DaySeconds=Period;Model.Weather.RegionalStrength=0;Model.Weather.PrevailingWind=0;Model.Weather.Events.Reset();TArray<FVector> Positions={Sun*1000,-Sun*1000},Normals={Sun,-Sun};FCEPlanetWeather Field;FString Error;
    if(!TestTrue(TEXT("Planet period binds to the existing native weather program"),Field.Initialize(Model,Positions,Normals,1000,17,Error))){AddError(Error);return false;}TArray<FCEWeatherSample> Day,Night,Repeat;Field.Evaluate(0,Day,Error);Field.Evaluate(3600,Night,Error);Field.Evaluate(0,Repeat,Error);
    TestTrue(TEXT("Sunlit surface is warmer than the opposite night side"),Day[0].TemperatureC>Day[1].TemperatureC+5);TestTrue(TEXT("Half a revolution reverses day/night heating"),Night[0].TemperatureC<Night[1].TemperatureC-5);TestEqual(TEXT("Forecast leaves historical temperature repeatable"),Day[0].TemperatureC,Repeat[0].TemperatureC);
    auto Mesh=FCEWeatherGlobeMesh::Make();Positions.Reset();TArray<FCEWeatherSample> Samples;
    for(auto D:Mesh.Directions){Positions.Add(D*(800+50*D.X));auto& S=Samples.AddDefaulted_GetRef();S.TemperatureC=20*D.X;S.PressureHpa=1013.25+30*D.Z;}
    auto O=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,false,false,true,false,Positions);
    TestEqual(TEXT("Only pressure uses a triangle heatmap shell"),O->MeshBodyMeters.Num(),Positions.Num());
    TestEqual(TEXT("Temperature uses one swatch per ground sample"),O->BodyMeters.Num(),Positions.Num());
    TestEqual(TEXT("Temperature matches the biome swatch size"),O->PointSizePixels,FCLPlanetBuilderOverlay().PointSizePixels);
    for(int32 I=0;I<Positions.Num();++I)
    {
        const auto C=O->MeshColors[I];TestTrue(TEXT("Pressure palette never has a green dominant midpoint"),C.G<FMath::Max(C.R,C.B));
        TestTrue(TEXT("Temperature swatches use exact sampled ground positions"),O->BodyMeters[I]==Positions[I]);
        TestTrue(TEXT("Temperature colours are opaque data values"),O->Colors[I].A==1.f);
    }
    const auto Hidden=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,false,false,false,false,Positions);
    TestTrue(TEXT("Hiding the heatmap removes temperature swatches"),Hidden->BodyMeters.IsEmpty()&&Hidden->MeshBodyMeters.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPrecipitationOverlayTest,"Catalyst.Ecosystem.Weather.PrecipitationOverlay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPrecipitationOverlayTest::RunTest(const FString&)
{
    TestEqual(TEXT("Dry precipitation is transparent"),CEWeatherPrecipitationColor(0,0).A,0.f);
    TestTrue(TEXT("Drizzle fades in smoothly from zero"),CEWeatherPrecipitationColor(.00001,0).A<.0001&&CEWeatherPrecipitationColor(.1,0).A<CEWeatherPrecipitationColor(1,0).A);
    const auto Blue=CEWeatherPrecipitationColor(1,0),Purple=CEWeatherPrecipitationColor(5,0),Yellow=CEWeatherPrecipitationColor(40,0),Snow=CEWeatherPrecipitationColor(0,40);
    TestTrue(TEXT("Reference rain palette progresses blue, purple, yellow"),Blue.B>.9&&Blue.R<.1&&Purple.R>.4&&Purple.B>.7&&Purple.G<.1&&Yellow.R>.9&&Yellow.G>.9&&Yellow.B<.1);
    TestTrue(TEXT("Heavy snowfall is icy white, distinct from yellow rain"),Snow.R>.9&&Snow.G>.9&&Snow.B>.9&&!Snow.Equals(Yellow,.1));
    for(double Rate:{.1,1.,5.,15.,40.})TestTrue(TEXT("Intensity colour and opacity have no threshold jumps"),CEWeatherPrecipitationColor(Rate-.00001,0).Equals(CEWeatherPrecipitationColor(Rate+.00001,0),.001));
    const auto Mixed=CEWeatherPrecipitationColor(3,3),Liquid=CEWeatherPrecipitationColor(6,0),Solid=CEWeatherPrecipitationColor(0,6);
    TestTrue(TEXT("Mixed phase blends colour without increasing intensity"),Mixed.Equals(FMath::Lerp(Liquid,Solid,.5f),.00001));
    const auto Mesh=FCEWeatherGlobeMesh::Make();TArray<FCEWeatherSample> Samples;TArray<FVector> Ground;
    for(const auto& D:Mesh.Directions){auto& S=Samples.AddDefaulted_GetRef();S.PressureHpa=1013.25+12*D.Z;S.Wind=FVector::CrossProduct(FVector::ZAxisVector,D)*6;Ground.Add(D*(800+50*D.Z));}
    const auto Dry=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,true,true,true,false,Ground);
    for(int32 I=0;I<Samples.Num();++I){if(Mesh.Directions[I].X>.3)Samples[I].RainMmHour=40;else if(Mesh.Directions[I].X<-.3)Samples[I].SnowMmHour=40;}
    const auto Wet=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,true,true,true,false,Ground);
    TestEqual(TEXT("Precipitation reuses the existing heatmap shell vertices"),Wet->MeshBodyMeters,Dry->MeshBodyMeters);TestEqual(TEXT("Precipitation adds no triangles"),Wet->MeshIndices,Dry->MeshIndices);
    TestEqual(TEXT("Ground temperature layer retains its anchors"),Wet->BodyMeters,Dry->BodyMeters);TestEqual(TEXT("Ground temperature data is untouched"),Wet->Colors,Dry->Colors);
    TestEqual(TEXT("Isobars remain above the combined heatmap"),Wet->Segments.Num(),Dry->Segments.Num());
    for(int32 I=0;I<Samples.Num();++I)
    {
        if(Samples[I].PrecipitationMmHour()==0)TestTrue(TEXT("Dry cells retain the transparent pressure tint"),Wet->MeshColors[I]==Dry->MeshColors[I]);
        else TestTrue(TEXT("Heavy precipitation is legible over the pressure tint"),Wet->MeshColors[I].A>.85&&Wet->MeshColors[I].A<=1);
    }
    const auto Moved=CEBuildWeatherGlobe(Mesh,Samples,3000,1013.25,3,true,true,true,false,Ground);
    TestEqual(TEXT("Moving Overlay preserves intensity colours"),Moved->MeshColors,Wet->MeshColors);
    for(int32 I=0;I<Wet->MeshBodyMeters.Num();++I)TestTrue(TEXT("Precipitation follows the movable guide"),Moved->MeshBodyMeters[I].Equals(Wet->MeshBodyMeters[I]*2,1.e-6));
    return true;
}
#endif
