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
    const FString Folder=IPluginManager::Get().FindPlugin(TEXT("CatalystEcosystem"))->GetBaseDir()/TEXT("Resources/Weather/Conformance/planetary-weather-5");FString ProgramJson,Json,Error;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherCharacter,"Catalyst.Ecosystem.Weather.CharacterDistribution",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherCharacter::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();M.Climate.Moisture=.9;M.Climate.MoistureVariation=0;M.Climate.SubtropicalDrying=0;
    TArray<FVector> P,N;constexpr int Count=128;
    for(int I=0;I<Count;++I){const double Z=1-2*(I+.5)/Count,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);P.Add(D*1000);N.Add(D);}
    double Wind[3]={},Rain[3]={};int32 Strong[3]={},Heavy[3]={};FString Error;
    for(int C=0;C<3;++C)
    {
        M.Weather.Character=C*.5;FCEPlanetWeather Field;if(!TestTrue(TEXT("Character bake"),Field.Initialize(M,P,N,1000,17,Error))){AddError(Error);return false;}
        for(int T=0;T<96;++T)
        {
            TArray<FCEWeatherSample> Rows;if(!TestTrue(TEXT("Character window evaluates"),Field.Evaluate(T*900,Rows,Error)))return false;
            for(const auto& S:Rows){const double Speed=S.Wind.Length();Wind[C]+=Speed;Rain[C]+=S.PrecipitationMmHour();Strong[C]+=Speed>=25;Heavy[C]+=S.PrecipitationMmHour()>=10;TestTrue(TEXT("Weather fractions remain bounded"),S.Cloud>=0&&S.Cloud<=1&&S.Dust>=0&&S.Dust<=1);}
        }
    }
    TestTrue(TEXT("Character increases average wind"),Wind[0]<Wind[1]&&Wind[1]<Wind[2]);
    TestTrue(TEXT("Character increases strong-wind frequency"),Strong[0]<Strong[1]&&Strong[1]<Strong[2]);
    TestTrue(TEXT("Character increases rain severity and heavy-rain frequency"),Rain[0]<Rain[1]&&Rain[1]<Rain[2]&&Heavy[0]<Heavy[1]&&Heavy[1]<Heavy[2]);
    AddInfo(FString::Printf(TEXT("Calm/moderate/volatile: mean wind %.3f/%.3f/%.3f m/s; strong-wind samples %d/%d/%d; heavy-rain samples %d/%d/%d of %d"),Wind[0]/(96*Count),Wind[1]/(96*Count),Wind[2]/(96*Count),Strong[0],Strong[1],Strong[2],Heavy[0],Heavy[1],Heavy[2],96*Count));
    M.Weather.bEnabled=false;FCEPlanetWeather Off;TestTrue(TEXT("Disabled field remains queryable"),Off.Initialize(M,P,N,1000,17,Error));TArray<FCEWeatherSample> Rows;TestTrue(TEXT("Disabled field evaluates"),Off.Evaluate(7200,Rows,Error));
    for(int I=0;I<Rows.Num();++I){const auto& S=Rows[I];TestTrue(TEXT("Disabled weather is static climate with no storm"),S.Wind.IsZero()&&S.RainMmHour==0&&S.SnowMmHour==0&&S.Cloud==0&&S.Dust==0&&S.PressureHpa==M.Weather.ReferencePressureHpa);TestTrue(TEXT("Disabled temperature retains terrain climate"),FMath::IsNearlyEqual(S.TemperatureC,FCEClimateEvaluator::Evaluate(M,P[I],N[I],1000,17).TemperatureC,.000051));}
    M.Weather.Character=1.1;TestFalse(TEXT("Out-of-range Character rejects"),M.Validate(Error));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEWeatherTravel,"Catalyst.Ecosystem.Weather.TravelMultiplier",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEWeatherTravel::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();TArray<FVector> P,N;
    for(int I=0;I<64;++I){const double Z=1-2*(I+.5)/64,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);P.Add(D*1000);N.Add(D);}
    FString Error;FCEPlanetWeather Base;if(!TestTrue(TEXT("Original travel bakes"),Base.Initialize(M,P,N,1000,17,Error)))return false;
    TArray<FCEWeatherSample> Before;Base.Evaluate(7200,Before,Error);
    const auto Original=M.Weather;
    for(double Rate:{0.,.25,.5,1.,2.,4.})
    {
        M.Weather=Original;M.Weather.SetTravelMultiplier(Rate,7200);
        TestEqual(TEXT("Continuous travel multiplier"),M.Weather.TravelMultiplier,Rate);
        TestEqual(TEXT("Position anchored at edit time"),7200*Rate+M.Weather.TravelPhaseOffsetSeconds,7200.*Original.TravelMultiplier);
        TestEqual(TEXT("Day length unchanged"),M.Weather.DaySeconds,Original.DaySeconds);
        TestEqual(TEXT("Wind strength unchanged"),M.Weather.PrevailingWind,Original.PrevailingWind);
        TestEqual(TEXT("Character unchanged"),M.Weather.Character,Original.Character);
        FCEPlanetWeather Changed;if(!TestTrue(TEXT("Changed travel bakes"),Changed.Initialize(M,P,N,1000,17,Error)))return false;
        TArray<FCEWeatherSample> Now,Future,Replay;Changed.Evaluate(7200,Now,Error);Changed.Evaluate(10800,Future,Error);Changed.Evaluate(3600,Replay,Error);Changed.Evaluate(10800,Replay,Error);
        for(int I=0;I<P.Num();++I)
        {
            TestTrue(TEXT("Changing travel never teleports weather or changes strength at edit time"),Now[I].TemperatureC==Before[I].TemperatureC&&Now[I].PressureHpa==Before[I].PressureHpa&&Now[I].Wind==Before[I].Wind&&Now[I].RainMmHour==Before[I].RainMmHour&&Now[I].SnowMmHour==Before[I].SnowMmHour&&Now[I].Dust==Before[I].Dust&&Now[I].Cloud==Before[I].Cloud);
            TestTrue(TEXT("Travel forecast survives out-of-order queries"),Future[I].TemperatureC==Replay[I].TemperatureC&&Future[I].PressureHpa==Replay[I].PressureHpa&&Future[I].Wind==Replay[I].Wind&&Future[I].RainMmHour==Replay[I].RainMmHour&&Future[I].SnowMmHour==Replay[I].SnowMmHour);
        }
    }
    M.Weather=Original;M.Weather.SetCharacter(0,7200);TestEqual(TEXT("Calm Character sets original travel"),M.Weather.TravelMultiplier,1.);
    M.Weather.SetCharacter(.5,7200);TestEqual(TEXT("Midpoint Character sets double travel"),M.Weather.TravelMultiplier,2.);
    M.Weather.SetCharacter(1,7200);TestEqual(TEXT("Volatile Character sets faster travel"),M.Weather.TravelMultiplier,4.);
    TestEqual(TEXT("Character movement preserves trajectory anchor"),7200*M.Weather.TravelMultiplier+M.Weather.TravelPhaseOffsetSeconds,7200.*Original.TravelMultiplier);
    M.Weather.SetTravelMultiplier(.7,7200);TestEqual(TEXT("Advanced override is independent of Character"),M.Weather.Character,1.);TestEqual(TEXT("Advanced accepts arbitrary fractional multiplier"),M.Weather.TravelMultiplier,.7);
    auto* Exported=NewObject<UCEPlanetEcosystemAsset>();auto* Imported=NewObject<UCEPlanetEcosystemAsset>();Exported->Ecosystem=M;
    FString Json;TestTrue(TEXT("Weather travel exports"),Exported->ExportJsonString(Json,Error));TestTrue(TEXT("Weather travel imports"),Imported->ApplyJsonString(Json,Error));
    TestEqual(TEXT("Advanced travel survives model serialization"),Imported->Ecosystem.Weather.TravelMultiplier,M.Weather.TravelMultiplier);
    TestEqual(TEXT("Trajectory anchor survives model serialization"),Imported->Ecosystem.Weather.TravelPhaseOffsetSeconds,M.Weather.TravelPhaseOffsetSeconds);
    M.Weather.TravelMultiplier=-1;TestFalse(TEXT("Negative travel rejects"),M.Validate(Error));M.Weather.TravelMultiplier=101;TestFalse(TEXT("Unbounded travel rejects"),M.Validate(Error));
    M.Weather=Original;M.Weather.TravelPhaseOffsetSeconds=3.e11;TestFalse(TEXT("Unbounded anchor rejects"),M.Validate(Error));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPrecipitationPhaseTest,"Catalyst.Ecosystem.Weather.PrecipitationPhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPrecipitationPhaseTest::RunTest(const FString&)
{
    auto M=FCEPlanetEcosystem::MakeDefault();M.Climate.Moisture=.9;M.Climate.MoistureVariation=0;M.Climate.SubtropicalDrying=0;M.Climate.TemperatureVariationC=0;
    TArray<FVector> P,N;for(int I=0;I<64;++I){const double Z=1-2*(I+.5)/64,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);P.Add(D*1000);N.Add(D);}
    FString Error;
    for(bool Cold:{false,true})
    {
        M.Climate.EquatorTemperatureC=M.Climate.PoleTemperatureC=Cold?-15:30;FCEPlanetWeather Field;
        if(!TestTrue(TEXT("Phase fixture initializes"),Field.Initialize(M,P,N,1000,17,Error))){AddError(Error);return false;}
        TArray<FCEWeatherSample> Now,Future,Replay;Field.Evaluate(3600,Now,Error);Field.Evaluate(7200,Future,Error);Field.Evaluate(3600,Replay,Error);double Total=0;
        for(int32 I=0;I<Now.Num();++I)
        {
            const auto& S=Now[I];Total+=S.PrecipitationMmHour();TestTrue(TEXT("Nonnegative water-equivalent precipitation"),S.RainMmHour>=0&&S.SnowMmHour>=0&&S.PrecipitationMmHour()<=100.00001);
            TestEqual(TEXT("Rain survives forecast queries"),S.RainMmHour,Replay[I].RainMmHour);TestEqual(TEXT("Snow survives forecast queries"),S.SnowMmHour,Replay[I].SnowMmHour);
            TestEqual(TEXT("Warm/cold phase follows GT temperature"),Cold?S.RainMmHour:S.SnowMmHour,0.);
        }
        TestTrue(TEXT("Both warm and cold fixtures produce real precipitation"),Total>0);
    }
    return true;
}
