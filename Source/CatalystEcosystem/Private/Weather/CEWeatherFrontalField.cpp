#include "Weather/CEWeatherFrontalField.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
namespace {int64 Q(double Value){return FMath::RoundToInt64(Value*10000.);}}
bool FCEWeatherFrontalField::Initialize(TConstArrayView<FVector> Positions,TConstArrayView<FVector> Normals,TConstArrayView<double> Moisture,TConstArrayView<double> Biomes,TConstArrayView<int64> Uniforms,FString& Error)
{
    const int32 N=Positions.Num();
    if(N<1||N>8192||Normals.Num()!=N||Moisture.Num()!=N||Biomes.Num()!=N||Uniforms.Num()!=52){Error=TEXT("Frontal stage needs canonical bounded sample inputs");return false;}
    FString Json;auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CatalystEcosystem"));
    if(!Plugin||!FFileHelper::LoadFileToString(Json,*(Plugin->GetBaseDir()/TEXT("Resources/Weather/planetary-weather-frontal-1.json")))||!Program.Load(Json,Error))return false;
    TSharedPtr<FJsonObject> Document;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Document)){Error=TEXT("Invalid frontal sampling profile");return false;}
    const auto Profile=Document->GetObjectField(TEXT("spatialSampling"));
    const int32 K=Profile->GetIntegerField(TEXT("neighbours")),Rings=Profile->GetIntegerField(TEXT("searchRings"));
    SmoothPasses=Profile->GetIntegerField(TEXT("smoothPasses"));ProbeSupport=Profile->GetNumberField(TEXT("probeSupportRadiansQ10000"))/10000.;
    Probes.Reset();for(const auto& V:Profile->GetArrayField(TEXT("probeRadiansQ10000")))Probes.Add(V->AsNumber()/10000.);
    if(K<2||K>12||Rings<1||Rings>3||SmoothPasses<1||SmoothPasses>4||!FMath::IsFinite(ProbeSupport)||ProbeSupport<=0||ProbeSupport>.25||Probes.Num()!=2){Error=TEXT("Frontal spatial sampling exceeds bounds");return false;}
    for(double P:Probes)if(!FMath::IsFinite(P)||FMath::Abs(P)>.25){Error=TEXT("Invalid frontal probe angle");return false;}
    Directions.Reset();GroundNormals.Reset();ClimateWet.Reset();BiomeWet.Reset();Context=FCSFieldContext();Context.Uniforms.Append(Uniforms.GetData(),Uniforms.Num());
    for(int32 I=0;I<N;++I)
    {
        if(Positions[I].ContainsNaN()||Positions[I].IsNearlyZero()||Normals[I].ContainsNaN()||!FMath::IsFinite(Moisture[I])||!FMath::IsFinite(Biomes[I])){Error=TEXT("Nonfinite frontal spatial input");return false;}
        Directions.Add(Positions[I].GetSafeNormal());GroundNormals.Add(Normals[I].GetSafeNormal());ClimateWet.Add(Moisture[I]);BiomeWet.Add(Biomes[I]);
    }
    Neighbours.SetNum(N);Pools.SetNum(N);GradientWeights.SetNum(N);
    for(int32 I=0;I<N;++I)
    {
        auto& Near=Neighbours[I];Near.Reset();TArray<double> Scores;
        for(int32 J=0;J<N;++J)if(J!=I)
        {
            const double Score=FVector::DotProduct(Directions[I],Directions[J]);int32 At=0;while(At<Scores.Num()&&Scores[At]>=Score)++At;
            if(At<K){Scores.Insert(Score,At);Near.Insert(J,At);if(Near.Num()>K){Near.Pop(EAllowShrinking::No);Scores.Pop(EAllowShrinking::No);}}
        }
        FVector X,Y;Directions[I].FindBestAxisVectors(X,Y);double XX=0,XY=0,YY=0;TArray<FVector2D> Offsets;TArray<double> Weights;
        for(int32 J:Near)
        {
            const auto D=FVector::VectorPlaneProject(Directions[J]-Directions[I],Directions[I]);const double U=FVector::DotProduct(D,X),V=FVector::DotProduct(D,Y),W=1/FMath::Max(1.e-12,U*U+V*V);
            XX+=U*U*W;XY+=U*V*W;YY+=V*V*W;Offsets.Add({U,V});Weights.Add(W);
        }
        const double Det=XX*YY-XY*XY;auto& Coefficients=GradientWeights[I];Coefficients.Reset();
        for(int32 E=0;E<Near.Num();++E){const auto D=Offsets[E];Coefficients.Add(Det>1.e-10?(X*(D.X*YY-D.Y*XY)+Y*(D.Y*XX-D.X*XY))*(Weights[E]/Det):FVector::ZeroVector);}
    }
    for(int32 I=0;I<N;++I)
    {
        TSet<int32> Seen;Seen.Add(I);TArray<int32> Frontier={I};
        for(int32 Ring=0;Ring<Rings;++Ring){TArray<int32> Next;for(int32 J:Frontier)for(int32 V:Neighbours[J])if(!Seen.Contains(V)){Seen.Add(V);Next.Add(V);}Frontier=MoveTemp(Next);}
        Pools[I]=Seen.Array();Pools[I].Sort();
    }
    Error.Reset();return true;
}
void FCEWeatherFrontalField::Smooth(TArray<double>& Values)const
{
    for(int32 Pass=0;Pass<SmoothPasses;++Pass)
    {
        auto Next=Values;for(int32 I=0;I<Values.Num();++I)if(!Neighbours[I].IsEmpty()){double Sum=0;for(int32 J:Neighbours[I])Sum+=Values[J];Next[I]=.5*(Values[I]+Sum/Neighbours[I].Num());}Values=MoveTemp(Next);
    }
}
TArray<FVector> FCEWeatherFrontalField::Gradient(const TArray<double>& Values)const
{
    TArray<FVector> Result;Result.SetNumZeroed(Values.Num());
    for(int32 I=0;I<Values.Num();++I)for(int32 E=0;E<Neighbours[I].Num();++E)Result[I]+=GradientWeights[I][E]*(Values[Neighbours[I][E]]-Values[I]);
    return Result;
}
bool FCEWeatherFrontalField::Evaluate(TConstArrayView<FCEWeatherSample> Background,TArray<FCEWeatherSample>& Result,FString& Error)const
{
    const int32 N=Directions.Num();if(N==0||Background.Num()!=N){Error=TEXT("Frontal sample generation mismatch");return false;}
    TArray<double> Temperature;for(const auto& S:Background)
    {
        if(!FMath::IsFinite(S.AirMassTemperatureC)||!FMath::IsFinite(S.TemperatureC)||S.Wind.ContainsNaN()||!FMath::IsFinite(S.PrecipitationMmHour())){Error=TEXT("Nonfinite background weather");return false;}Temperature.Add(S.AirMassTemperatureC);
    }
    Smooth(Temperature);const auto Thermal=Gradient(Temperature);TArray<double> Strength;for(const auto& G:Thermal)Strength.Add(G.Length());Smooth(Strength);
    const auto StrengthGradient=Gradient(Strength);TArray<double> Across;
    for(int32 I=0;I<N;++I)Across.Add(FVector::DotProduct(StrengthGradient[I],Thermal[I].GetSafeNormal()));
    Smooth(Across);const auto AcrossGradient=Gradient(Across);
    TArray<TArray<double>> Features;
    for(int32 I=0;I<N;++I)
    {
        const auto Warmward=Thermal[I].GetSafeNormal();Features.Add({Strength[I],-FVector::DotProduct(AcrossGradient[I],Warmward),Across[I],FVector::DotProduct(Background[I].Wind,Warmward),ClimateWet[I],BiomeWet[I]});
    }
    TArray<TArray<int64>> Inputs;Inputs.Reserve(N);
    for(int32 I=0;I<N;++I)
    {
        const auto& S=Background[I];const auto Normal=GroundNormals[I],Warmward=Thermal[I].GetSafeNormal();
        TArray<int64> Row={Q(S.TemperatureC),Q(S.Wind.X),Q(S.Wind.Y),Q(S.Wind.Z),Q(S.RainMmHour),Q(S.Dust),Q(S.Cloud),Q(S.PressureHpa),Q(S.SnowMmHour),Q(Normal.X),Q(Normal.Y),Q(Normal.Z)};
        for(double V:Features[I])Row.Add(Q(V));
        for(double Angle:Probes)
        {
            const auto Target=(Directions[I]*FMath::Cos(Angle)+Warmward*FMath::Sin(Angle)).GetSafeNormal();const double Support=1-FMath::Cos(ProbeSupport);
            double Sum[6]={},Weight=0;int32 Exact=INDEX_NONE;
            for(int32 J:Pools[I])
            {
                const double Distance=FMath::Max(0.,1-FVector::DotProduct(Target,Directions[J]));
                if(Distance<1.e-10){Exact=J;break;}if(Distance>=Support)continue;
                // Fixed stencil membership and zero-slope support edges avoid neighbour-rank jumps.
                const double W=FMath::Square(1-Distance/Support)/Distance;Weight+=W;
                for(int32 C=0;C<6;++C)Sum[C]+=W*Features[J][C];
            }
            for(int32 C=0;C<6;++C)Row.Add(Q(Exact!=INDEX_NONE?Features[Exact][C]:(Weight>0?Sum[C]/Weight:Features[I][C])));
        }
        Inputs.Add(MoveTemp(Row));
    }
    TArray<TArray<int64>> Raw;if(!Program.Evaluate(Context,Inputs,Raw,Error))return false;
    Result.Reset();Result.Reserve(N);
    for(int32 I=0;I<N;++I)
    {
        const auto& R=Raw[I];if(R.Num()!=11){Error=TEXT("Frontal weather channel schema mismatch");return false;}
        auto S=Background[I];S.TemperatureC=double(R[0])/10000.;S.Wind=FVector(R[1],R[2],R[3])/10000.;S.RainMmHour=double(R[4])/10000.;S.Dust=double(R[5])/10000.;S.Cloud=double(R[6])/10000.;S.PressureHpa=double(R[7])/10000.;S.SnowMmHour=double(R[8])/10000.;S.FrontStrength=double(R[9])/10000.;S.FrontalPrecipitationMmHour=double(R[10])/10000.;S.FrontWarmward=Thermal[I].GetSafeNormal();S.FrontAcross=double(Inputs[I][14])/10000.;S.FrontCrossWind=double(Inputs[I][15])/10000.;Result.Add(S);
    }
    return true;
}
