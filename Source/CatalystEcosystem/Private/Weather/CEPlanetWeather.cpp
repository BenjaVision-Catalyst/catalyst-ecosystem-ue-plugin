#include "Weather/CEPlanetWeather.h"
#include "Weather/CEWeatherFrontalField.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
namespace {int64 Fixed(double V){return FMath::RoundToInt64(V*10000.);}}
bool FCEPlanetWeather::Initialize(const FCEPlanetEcosystem& M,TConstArrayView<FVector> Positions,TConstArrayView<FVector> Normals,double Radius,int32 Seed,FString& Error,TConstArrayView<FGuid> AssignedBiomes)
{
    const double Begin=FPlatformTime::Seconds();if(!M.Validate(Error))return false;
    if(Positions.IsEmpty()||Positions.Num()>8192||Positions.Num()!=Normals.Num()||Radius<=0||!FMath::IsFinite(Radius)){Error=TEXT("Weather requires bounded accepted terrain samples");return false;}
    auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CatalystEcosystem"));FString Json;
    if(!Plugin||!FFileHelper::LoadFileToString(Json,*(Plugin->GetBaseDir()/TEXT("Resources/Weather/planetary-weather-6.json")))||!Field.Load(Json,Error))return false;
    Context=FCSFieldContext();Context.Seed=uint64(uint32(Seed))<<32|uint32(M.Weather.Seed);Context.Uniforms.SetNumZeroed(52);
    auto& U=Context.Uniforms;U[0]=Fixed(M.Weather.RegionalStrength);U[1]=Fixed(M.Weather.DaySeconds);U[2]=Fixed(M.Weather.CirculationSeconds);U[12]=Fixed(M.Weather.PrevailingWind);U[13]=Fixed(M.Weather.ReferencePressureHpa);U[14]=Fixed(M.Weather.WindResponse);U[15]=Fixed(M.Weather.Character);U[48]=M.Weather.bEnabled?10000:0;U[49]=Fixed(M.Weather.TravelMultiplier);U[50]=Fixed(M.Weather.TravelPhaseOffsetSeconds);U[51]=Fixed(M.Climate.LapseRateCPerKm);
    const FVector Pole=M.Climate.PoleAxis.GetSafeNormal(),Axis=(FMath::Abs(Pole.Z)<.9?FVector::ZAxisVector:FVector::XAxisVector);
    const FVector BasisU=FVector::CrossProduct(Axis,Pole).GetSafeNormal(),BasisV=FVector::CrossProduct(Pole,BasisU);
    for(int I=0;I<3;++I){U[3+I]=Fixed(Pole[I]);U[6+I]=Fixed(BasisU[I]);U[9+I]=Fixed(BasisV[I]);}
    Inputs.Reset();Inputs.Reserve(Positions.Num());TArray<bool> Water;
    for(int I=0;I<Positions.Num();++I)
    {
        const FVector D=Positions[I].GetSafeNormal(),N=Normals[I].GetSafeNormal();const auto Climate=FCEClimateEvaluator::Evaluate(M,Positions[I],N,Radius,Seed);
        double BiomeWet=Climate.Moisture,BiomeTemp=Climate.TemperatureC,Total=0,TempSum=0,WetSum=0;
        for(const auto& B:M.Biomes)if(B.bEnabled&&B.bWater==Climate.bWater)
        {
            const double TD=FMath::Max(0.,FMath::Max(B.TemperatureC.X-Climate.TemperatureC,Climate.TemperatureC-B.TemperatureC.Y));
            const double MD=FMath::Max(0.,FMath::Max(B.Moisture.X-Climate.Moisture,Climate.Moisture-B.Moisture.Y));
            const double Weight=1./FMath::Square(1.+TD/5.+MD*12.);Total+=Weight;TempSum+=Weight*(B.TemperatureC.X+B.TemperatureC.Y)*.5;WetSum+=Weight*(B.Moisture.X+B.Moisture.Y)*.5;
        }
        if(Total>0){BiomeWet=WetSum/Total;BiomeTemp=FMath::Clamp(TempSum/Total,Climate.TemperatureC-10,Climate.TemperatureC+10);}
        if(AssignedBiomes.IsValidIndex(I))if(const auto* B=M.Biomes.FindByPredicate([&](const auto& X){return X.Id==AssignedBiomes[I];}))
        {BiomeWet=FMath::Clamp((B->Moisture.X+B->Moisture.Y)*.5,Climate.Moisture-.25,Climate.Moisture+.25);BiomeTemp=FMath::Clamp((B->TemperatureC.X+B->TemperatureC.Y)*.5,Climate.TemperatureC-5,Climate.TemperatureC+5);}
        TArray<int64> Row={Fixed(D.X),Fixed(D.Y),Fixed(D.Z),Fixed(Climate.TemperatureC),Fixed(Climate.Moisture),Climate.bWater?10000LL:0LL,Fixed(Climate.AltitudeMeters/1000.),Fixed(N.X),Fixed(N.Y),Fixed(N.Z),Fixed(BiomeWet),Fixed(BiomeTemp),0};
        Inputs.Add(MoveTemp(Row));Water.Add(Climate.bWater);
    }
    // Coarse fetch from the actual ocean mask, baked once per accepted revision.
    for(int I=0;I<Inputs.Num();++I){double Closest=-1;const FVector D=Positions[I].GetSafeNormal();for(int J=0;J<Positions.Num();++J)if(Water[J])Closest=FMath::Max(Closest,FVector::DotProduct(D,Positions[J].GetSafeNormal()));Inputs[I][12]=Fixed(FMath::Clamp((Closest-.92)/.08,0.,1.));}
    Context.Environment=Inputs;
    TArray<FCEAuthoredWeatherEvent> Events=M.Weather.Events;Events.Sort([](const auto& A,const auto& B){return A.Id.ToString()<B.Id.ToString();});Context.Curves.SetNum(4);
    for(int I=0;I<Events.Num();++I)
    {
        const auto& E=Events[I];const int B=16+I*8;U[B]=E.bEnabled?10000:0;U[B+1]=Fixed(E.StartSeconds);U[B+2]=Fixed(E.DurationSeconds);U[B+3]=Fixed(E.WindSpeed);
        U[B+4]=FMath::Min<int64>(9999,Fixed(FMath::Cos(FMath::Clamp(E.RadiusMeters/Radius,.01,1.))));U[B+5]=Fixed(E.Moisture);
        TArray<TArray<int64>> Points;for(const auto& V:E.PathDirections){const auto D=V.GetSafeNormal();Points.Add({Fixed(D.X),Fixed(D.Y),Fixed(D.Z)});}
        Context.Curves[I]=FCSFieldProgram::BuildCurve(Points);if(Context.Curves[I].IsEmpty()){Error=TEXT("Weather spline crosses an invalid planet-core direction");return false;}
    }
    TArray<double> ClimateWet,BiomeWet;for(const auto& Row:Inputs){ClimateWet.Add(double(Row[4])/10000.);BiomeWet.Add(double(Row[10])/10000.);}
    Frontal=MakeShared<FCEWeatherFrontalField,ESPMode::ThreadSafe>();if(!Frontal->Initialize(Positions,Normals,ClimateWet,BiomeWet,Context.Uniforms,Error))return false;
    LastBakeSeconds=FPlatformTime::Seconds()-Begin;Error.Reset();return true;
}
bool FCEPlanetWeather::Evaluate(int64 Seconds,TArray<FCEWeatherSample>& Samples,FString& Error)const
{
    FCSFieldContext C=Context;C.Seconds=Seconds;TArray<TArray<int64>> Raw;if(!Field.Evaluate(C,Inputs,Raw,Error))return false;
    TArray<FCEWeatherSample> Background;Background.Reserve(Raw.Num());for(const auto& R:Raw){if(R.Num()!=10){Error=TEXT("Weather channel schema mismatch");return false;}auto& S=Background.AddDefaulted_GetRef();S.TemperatureC=double(R[0])/10000;S.Wind=FVector(R[1],R[2],R[3])/10000.;S.RainMmHour=double(R[4])/10000.;S.Dust=double(R[5])/10000.;S.Cloud=double(R[6])/10000.;S.PressureHpa=double(R[7])/10000.;S.SnowMmHour=double(R[8])/10000.;S.AirMassTemperatureC=double(R[9])/10000.;}
    return Frontal.IsValid()&&Frontal->Evaluate(Background,Samples,Error);
}

FVector CEWeatherSunDirection(FVector Pole,double Period,double Seconds)
{
    if(!FMath::IsFinite(Period)||Period<60||!FMath::IsFinite(Seconds)||Pole.ContainsNaN()||Pole.IsNearlyZero())return FVector::XAxisVector;
    Pole.Normalize();const FVector Axis=FMath::Abs(Pole.Z)<.9?FVector::ZAxisVector:FVector::XAxisVector;
    const FVector U=FVector::CrossProduct(Axis,Pole).GetSafeNormal(),V=FVector::CrossProduct(Pole,U).GetSafeNormal();
    const double Phase=2*UE_DOUBLE_PI*FMath::Fmod(Seconds,Period)/Period;
    return U*FMath::Cos(Phase)+V*FMath::Sin(Phase);
}
