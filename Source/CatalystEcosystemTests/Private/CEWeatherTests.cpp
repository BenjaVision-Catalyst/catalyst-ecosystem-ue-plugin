#include "Misc/AutomationTest.h"
#include "Fields/CSFieldProgram.h"
#include "Weather/CEPlanetWeather.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
namespace
{
TArray<int64> Row(const TArray<TSharedPtr<FJsonValue>>& A){TArray<int64> R;for(const auto& V:A)R.Add(int64(V->AsNumber()));return R;}
TArray<TArray<int64>> Rows(const TArray<TSharedPtr<FJsonValue>>& A){TArray<TArray<int64>> R;for(const auto& V:A)R.Add(Row(V->AsArray()));return R;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherVectors,"Catalyst.Ecosystem.Weather.GTNativeParity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherVectors::RunTest(const FString&)
{
    const FString Folder=IPluginManager::Get().FindPlugin(TEXT("CatalystEcosystem"))->GetBaseDir()/TEXT("Resources/Weather/Conformance/planetary-weather-2");FString ProgramJson,Json,Error;
    if(!TestTrue(TEXT("GT generated program exists"),FFileHelper::LoadFileToString(ProgramJson,*(Folder/TEXT("program.json"))))||!TestTrue(TEXT("GT vectors exist"),FFileHelper::LoadFileToString(Json,*(Folder/TEXT("vectors.json")))))return false;
    FCSFieldProgram Invalid;
    TestFalse(TEXT("Missing model arrays reject"),Invalid.Load(TEXT("{\"profile\":\"analytic-field-1\"}"),Error));
    TestFalse(TEXT("Fractional opcode rejects"),Invalid.Load(TEXT("{\"profile\":\"analytic-field-1\",\"nodes\":[[0.5,1]],\"outputs\":[0],\"sine\":[]}"),Error));
    FCSFieldProgram Program;if(!TestTrue(TEXT("GT program validates"),Program.Load(ProgramJson,Error)))return false;
    TSharedPtr<FJsonObject> J;if(!TestTrue(TEXT("GT vectors are canonical JSON"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),J)))return false;
    const auto Inputs=Rows(J->GetArrayField(TEXT("inputs")));FCSFieldContext C;C.Environment=Inputs;
    for(const auto& V:J->GetArrayField(TEXT("curves")))C.Curves.Add(Rows(V->AsArray()));
    TestTrue(TEXT("Cubic arc-length spline agrees exactly with GT"),FCSFieldProgram::BuildCurve(Rows(J->GetArrayField(TEXT("curveControlPoints"))))==C.Curves[0]);
    int32 Count=0;const double Begin=FPlatformTime::Seconds();
    for(const auto& V:J->GetArrayField(TEXT("cases")))
    {
        const auto Case=V->AsObject();C.Seconds=int64(Case->GetNumberField(TEXT("seconds")));C.Seed=uint64(Case->GetNumberField(TEXT("seed")));C.Uniforms=Row(Case->GetArrayField(TEXT("uniforms")));
        TArray<TArray<int64>> Actual;if(!TestTrue(TEXT("Native evaluates bounded GT model"),Program.Evaluate(C,Inputs,Actual,Error))){AddError(Error);return false;}
        TestTrue(FString::Printf(TEXT("Exact GT/native channels at %lld s"),C.Seconds),Actual==Rows(Case->GetArrayField(TEXT("outputs"))));Count+=Actual.Num();
    }
    AddInfo(FString::Printf(TEXT("%d GT/native point cases, %d nodes, %.3f ms"),Count,Program.NodeCount(),(FPlatformTime::Seconds()-Begin)*1000));
    FCSFieldRuntime Clock(3600);TestTrue(TEXT("Retained analytic interval supports history"),Clock.IsHistory(0));TestTrue(TEXT("Forecast lies within next hour"),Clock.IsForecast(7200));TestEqual(TEXT("Forecast query leaves accepted time"),Clock.AcceptedSeconds(),3600LL);TestTrue(TEXT("Advance live state"),Clock.AdvanceTo(7200));TestFalse(TEXT("No implicit rewind"),Clock.AdvanceTo(0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherPlanet,"Catalyst.Ecosystem.Weather.PlanetInputsAndBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherPlanet::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();TArray<FVector> P,N;constexpr int Count=2048;
    for(int I=0;I<Count;++I){const double Z=1-2*(I+.5)/Count,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);P.Add(D*(1000+100*D.X));N.Add(D);}
    FCEPlanetWeather Field;FString Error;if(!TestTrue(TEXT("Explicit planet bake"),Field.Initialize(M,P,N,1000,17,Error))){AddError(Error);return false;}
    TArray<FCEWeatherSample> A,B,Future;const double Start=FPlatformTime::Seconds();if(!TestTrue(TEXT("Now"),Field.Evaluate(3600,A,Error))){AddError(Error);return false;}const double Millis=(FPlatformTime::Seconds()-Start)*1000;
    if(!TestTrue(TEXT("Prediction"),Field.Evaluate(7200,Future,Error))||!TestTrue(TEXT("Now after forecast"),Field.Evaluate(3600,B,Error)))return false;
    double Delta=0;for(int I=0;I<Count;++I){TestEqual(TEXT("Repeatable temperature"),A[I].TemperatureC,B[I].TemperatureC);TestEqual(TEXT("Repeatable pressure"),A[I].PressureHpa,B[I].PressureHpa);TestEqual(TEXT("Repeatable rain"),A[I].RainMmHour,B[I].RainMmHour);TestTrue(TEXT("Repeatable wind"),A[I].Wind==B[I].Wind);TestTrue(TEXT("Nonnegative rain"),A[I].RainMmHour>=0);Delta+=FMath::Abs(A[I].TemperatureC-Future[I].TemperatureC);}
    TestTrue(TEXT("Weather evolves over the hour"),Delta>1);AddInfo(FString::Printf(TEXT("2048-point evaluation %.3f ms; bake %.3f ms"),Millis,Field.LastBakeSeconds*1000));
    M.Weather.Seed++;FCEPlanetWeather Other;TestTrue(TEXT("Other seed initializes"),Other.Initialize(M,P,N,1000,17,Error));TestTrue(TEXT("Other seed evaluates"),Other.Evaluate(3600,B,Error));Delta=0;for(int I=0;I<Count;++I)Delta+=FMath::Abs(A[I].RainMmHour-B[I].RainMmHour);TestTrue(TEXT("Seed changes weather"),Delta>0);
    return true;
}
