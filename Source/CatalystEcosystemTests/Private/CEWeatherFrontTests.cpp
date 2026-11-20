#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_CE_LANDFORM
#include "Weather/CEWeatherFronts.h"
#include "Weather/CEWeatherGlobe.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherFrontTest,"Catalyst.Ecosystem.Weather.FrontDiagnostics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherFrontTest::RunTest(const FString&)
{
    const auto Mesh=FCEWeatherGlobeMesh::Make();TArray<FCEWeatherSample> Samples;Samples.SetNum(Mesh.Directions.Num());
    for(auto& S:Samples){S.TemperatureC=12;S.PressureHpa=1013.25;S.Wind=FVector(5,0,0);}
    TestTrue(TEXT("Uniform air has no manufactured front"),CEDiagnoseWeatherFronts(Mesh,Samples).IsEmpty());
    for(int32 I=0;I<Samples.Num();++I)Samples[I].TemperatureC=30*Mesh.Directions[I].Z;
    TestTrue(TEXT("Smooth latitude climate alone is not a front"),CEDiagnoseWeatherFronts(Mesh,Samples).IsEmpty());
    auto Fill=[&](FVector Axis,double Wind,double Centre,bool Wet)
    {
        for(int32 I=0;I<Samples.Num();++I)
        {
            const auto D=Mesh.Directions[I];const double Q=FVector::DotProduct(D,Axis)-Centre;
            auto& S=Samples[I];S.TemperatureC=12*(2/(1+FMath::Exp(-2*Q/.12))-1);
            S.Wind=FVector::VectorPlaneProject(Axis,D)*Wind;S.Cloud=Wet?.9:0;S.RainMmHour=Wet?20:0;
        }
    };
    Fill(FVector::XAxisVector,12,.1,true);const auto Cold=CEDiagnoseWeatherFronts(Mesh,Samples);
    if(!TestTrue(TEXT("A sharp air-mass boundary produces connected fronts"),!Cold.IsEmpty()))return false;
    int32 Points=0;for(const auto& F:Cold)
    {
        TestTrue(TEXT("Cold air advancing toward warmth is a cold front"),F.Kind==ECEWeatherFrontKind::Cold);
        Points+=F.Points.Num();
        for(const auto& P:F.Points){TestTrue(TEXT("Thermal ridge is at the actual air-mass boundary"),FMath::Abs(P.Direction.X-.1)<.055);TestTrue(TEXT("Cloud/rain are sampled, not invented"),FMath::IsNearlyEqual(P.Cloud,.9)&&FMath::IsNearlyEqual(P.RainMmHour,20.));}
        FCEWeatherFrontPoint P;FVector T;TestTrue(TEXT("Front can be sampled at spherical arc distance"),F.Sample(F.ArcRadians.Last()*.37,P,T));
        TestTrue(TEXT("Front samples and tangent remain canonical"),P.Direction.IsUnit()&&FMath::Abs(FVector::DotProduct(P.Direction,T))<1.e-8);
    }
    TestTrue(TEXT("Front work stays bounded"),Cold.Num()<=64&&Points<=1024);
    const auto Again=CEDiagnoseWeatherFronts(Mesh,Samples);TestEqual(TEXT("Repeat queries preserve chain ordering"),Again.Num(),Cold.Num());
    for(int32 I=0;I<Cold.Num();++I){TestEqual(TEXT("Repeat query preserves node count"),Again[I].Points.Num(),Cold[I].Points.Num());for(int32 J=0;J<Cold[I].Points.Num();++J)TestTrue(TEXT("Repeat query preserves canonical geometry"),Again[I].Points[J].Direction==Cold[I].Points[J].Direction);}
    FCEWeatherFrontPresentation Presentation;Presentation.SetFronts(Cold);TestEqual(TEXT("Newly diagnosed fronts start transparent"),Presentation.Entries[0].Opacity,0.f);
    Presentation.Tick(.4);TestTrue(TEXT("Front birth fades continuously"),Presentation.Entries[0].Opacity>0&&Presentation.Entries[0].Opacity<1);const auto Opacity=Presentation.Entries[0].Opacity;Presentation.SetFronts(Cold);TestEqual(TEXT("Repeated samples do not restart front birth"),Presentation.Entries[0].Opacity,Opacity);
    Presentation.SetFronts({});TestTrue(TEXT("Missing topology retires instead of vanishing"),!Presentation.Entries.IsEmpty()&&Presentation.Entries[0].Retiring);Presentation.Tick(.1);TestTrue(TEXT("Retiring front remains partially visible"),Presentation.Entries[0].Opacity>0&&Presentation.Entries[0].Opacity<Opacity);Presentation.Tick(1);TestTrue(TEXT("Retirement releases presentation geometry"),Presentation.Entries.IsEmpty());
    const auto Overlay=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,false,false,true,true);
    TestTrue(TEXT("Front symbols render without pressure lines"),!Overlay->Segments.IsEmpty());
    for(const auto& L:Overlay->Segments)TestTrue(TEXT("Lines and all marker points use the radial guide"),FMath::IsNearlyEqual(L.A.Length(),1500.,1.e-6)&&FMath::IsNearlyEqual(L.B.Length(),1500.,1.e-6));
    Fill(FVector::XAxisVector,-12,.1,true);const auto Warm=CEDiagnoseWeatherFronts(Mesh,Samples);TestEqual(TEXT("Reversed advection preserves the boundary"),Warm.Num(),Cold.Num());
    for(const auto& F:Warm)TestTrue(TEXT("Warm air advancing toward cold is a warm front"),F.Kind==ECEWeatherFrontKind::Warm);
    Fill(FVector::XAxisVector,0,.1,false);const auto Still=CEDiagnoseWeatherFronts(Mesh,Samples);TestTrue(TEXT("A stationary thermal boundary remains visible"),!Still.IsEmpty());for(const auto& F:Still)TestTrue(TEXT("Calm boundaries are not falsely labelled advancing"),F.Kind==ECEWeatherFrontKind::Stationary);
    const auto Dry=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,false,false,true,true);
    const auto Plain=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3,false,false,true,false);
    for(int32 I=0;I<Samples.Num();++I)TestTrue(TEXT("Fronts never tint cloud bands into the heatmap"),Overlay->MeshColors[I]==Plain->MeshColors[I]&&Dry->MeshColors[I]==Plain->MeshColors[I]);
    TestTrue(TEXT("Front triangles and semicircles have filled geometry"),Overlay->MeshIndices.Num()>Plain->MeshIndices.Num()&&Dry->MeshIndices.Num()>Plain->MeshIndices.Num());
    TestEqual(TEXT("Only front symbols occupy the foreground triangle range"),Overlay->ForegroundTriangleStart,Plain->MeshIndices.Num()/3);
    TestEqual(TEXT("All filled front symbols are above ground swatches"),Overlay->ForegroundTriangleCount,(Overlay->MeshIndices.Num()-Plain->MeshIndices.Num())/3);
    for(const auto& F:Cold)for(const auto& P:F.Points)TestTrue(TEXT("Game-queryable front strength is bounded and nonzero"),P.Strength>0&&P.Strength<=1);
    Fill(FVector::ZAxisVector,12,.4,true);const auto Moved=CEDiagnoseWeatherFronts(Mesh,Samples);TestTrue(TEXT("A moving boundary follows the field across the globe"),!Moved.IsEmpty());for(const auto& F:Moved)for(const auto& P:F.Points)TestTrue(TEXT("Front is in its new location, not a drifting decoration"),FMath::Abs(P.Direction.Z-.4)<.055);
    Samples[0].TemperatureC=std::numeric_limits<double>::quiet_NaN();TestTrue(TEXT("Invalid samples reject without broken geometry"),CEDiagnoseWeatherFronts(Mesh,Samples).IsEmpty());
    // The unchanged domain model must supply visible, repeatable evolving fronts too.
    auto Model=FCEPlanetEcosystem::MakeDefault();TArray<FVector> Positions,Normals;
    for(const auto& D:Mesh.Directions){Positions.Add(D*1000);Normals.Add(D);}FCEPlanetWeather Field;FString Error;
    if(!TestTrue(TEXT("Native field initializes for front diagnostics"),Field.Initialize(Model,Positions,Normals,1000,17,Error))){AddError(Error);return false;}
    int32 Natural=0;double TimeMs=0;FString Evidence;
    for(int32 Time:{0,3600,7200})
    {
        if(!TestTrue(TEXT("Fronts query the existing simulation timeline"),Field.Evaluate(Time,Samples,Error)))return false;
        const double Begin=FPlatformTime::Seconds();const auto Fronts=CEDiagnoseWeatherFronts(Mesh,Samples);const auto Globe=CEBuildWeatherGlobe(Mesh,Samples,1500,1013.25,3);TimeMs+=(FPlatformTime::Seconds()-Begin)*1000;
        int32 C=0,W=0,S=0;for(const auto& F:Fronts){C+=F.Kind==ECEWeatherFrontKind::Cold;W+=F.Kind==ECEWeatherFrontKind::Warm;S+=F.Kind==ECEWeatherFrontKind::Stationary;}
        Natural+=Fronts.Num();Evidence+=FString::Printf(TEXT("time=%d cold=%d warm=%d stationary=%d segments=%d\n"),Time,C,W,S,Globe->Segments.Num());
        TestTrue(TEXT("Combined overlay respects native rendering bounds"),Globe->Segments.Num()<=16384&&Globe->MeshBodyMeters.Num()<=16384&&Globe->MeshIndices.Num()<=98304);
    }
    TestTrue(TEXT("Natural model exposes fronts on the globe"),Natural>0);Evidence+=FString::Printf(TEXT("diagnostic plus combined overlay average %.3f ms\n"),TimeMs/3);AddInfo(Evidence);
    FFileHelper::SaveStringToFile(Evidence,*(FPaths::ProjectSavedDir()/TEXT("EcosystemQualification/weather-fronts.txt")));return true;
}
#endif
