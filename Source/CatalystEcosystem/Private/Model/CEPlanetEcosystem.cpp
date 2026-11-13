#include "Model/CEPlanetEcosystem.h"
namespace
{
uint32 Mix(uint32 X) { X ^= X>>16; X*=0x7feb352dU; X^=X>>15; X*=0x846ca68bU; return X^(X>>16); }
double Noise(FVector P,uint32 Seed)
{
    const int32 X=FMath::FloorToInt(P.X),Y=FMath::FloorToInt(P.Y),Z=FMath::FloorToInt(P.Z);
    auto Smooth=[](double V){return V*V*(3-2*V);};const FVector T(Smooth(P.X-X),Smooth(P.Y-Y),Smooth(P.Z-Z));
    auto H=[&](int32 A,int32 B,int32 C){return double(Mix(Seed^Mix(uint32(A))^Mix(uint32(B)+0x9e3779b9U)^Mix(uint32(C)+0x85ebca6bU)))/double(MAX_uint32)*2-1;};
    return FMath::Lerp(FMath::Lerp(FMath::Lerp(H(X,Y,Z),H(X+1,Y,Z),T.X),FMath::Lerp(H(X,Y+1,Z),H(X+1,Y+1,Z),T.X),T.Y),
        FMath::Lerp(FMath::Lerp(H(X,Y,Z+1),H(X+1,Y,Z+1),T.X),FMath::Lerp(H(X,Y+1,Z+1),H(X+1,Y+1,Z+1),T.X),T.Y),T.Z);
}
bool Range(FVector2D R){return FMath::IsFinite(R.X)&&FMath::IsFinite(R.Y)&&R.X<=R.Y;}
bool Inside(double V,FVector2D R,double Expansion=0){return V>=R.X-Expansion&&V<=R.Y+Expansion;}
}
FCEPlanetEcosystem FCEPlanetEcosystem::MakeDefault()
{
    FCEPlanetEcosystem M;
    auto Add=[&](uint32 Id,const TCHAR* Name,FLinearColor Color,bool Water,FVector2D Temp,FVector2D Wet,int32 Priority)
    {FCEBiomeDefinition B;B.Id=FGuid(0x43454249,0,0,Id);B.Name=Name;B.Description=FString::Printf(TEXT("%s habitat"),Name);B.Color=Color;B.bWater=Water;B.TemperatureC=Temp;B.Moisture=Wet;B.Priority=Priority;M.Biomes.Add(B);};
    Add(1,TEXT("Ocean"),FLinearColor(.02,.14,.65),true,{-100,100},{0,1},0);
    Add(2,TEXT("Polar ice"),FLinearColor(.85,.95,1),false,{-100,-12},{0,1},20);
    Add(3,TEXT("Tundra"),FLinearColor(.5,.55,.35),false,{-12,0},{0,1},10);
    Add(4,TEXT("Boreal forest"),FLinearColor(.05,.24,.12),false,{0,10},{.4,1},10);
    Add(5,TEXT("Temperate forest"),FLinearColor(.12,.55,.2),false,{10,23},{.45,1},10);
    Add(6,TEXT("Grassland"),FLinearColor(.55,.7,.18),false,{0,100},{.2,.45},5);
    Add(7,TEXT("Desert"),FLinearColor(.85,.58,.2),false,{0,100},{0,.2},10);
    Add(8,TEXT("Tropical forest"),FLinearColor(.02,.35,.12),false,{23,100},{.45,1},10);
    Add(9,TEXT("Bare ground"),FLinearColor(.35,.3,.28),false,{-100,100},{0,1},-100);
    return M;
}
bool FCEPlanetEcosystem::Validate(FString& Error) const
{
    const auto& C=Climate;
    if(SchemaVersion!=1||C.PoleAxis.ContainsNaN()||C.PoleAxis.IsNearlyZero()||!FMath::IsFinite(C.PoleAxis.SizeSquared())
       ||!Inside(C.EquatorTemperatureC,{-100,100})||!Inside(C.PoleTemperatureC,{-100,100})||!Inside(C.LapseRateCPerKm,{0,20})
       ||!Inside(C.SeaLevelMeters,{-1.e7,1.e7})||!Inside(C.Moisture,{0,1})||!Inside(C.SubtropicalDrying,{0,1})
       ||!Inside(C.TemperatureVariationC,{0,30})||!Inside(C.MoistureVariation,{0,1})||!Inside(C.NoiseFrequency,{1,64})||Biomes.Num()>64)
    {Error=TEXT("Climate values must be finite, within their documented ranges, with a nonzero pole axis and at most 64 biomes.");return false;}
    TSet<FGuid> Seen;
    for(const auto& B:Biomes)
    {
        if(!B.Id.IsValid()||Seen.Contains(B.Id)||B.Name.TrimStartAndEnd().IsEmpty()||!Inside(B.Character,{0,1})
           ||!Range(B.TemperatureC)||!Range(B.Moisture)||!Range(B.AltitudeMeters)||!Range(B.SlopeDegrees)||(!FMath::IsFinite(B.Color.R)||!FMath::IsFinite(B.Color.G)||!FMath::IsFinite(B.Color.B)||!FMath::IsFinite(B.Color.A))
           ||B.Moisture.X<0||B.Moisture.Y>1||B.SlopeDegrees.X<0||B.SlopeDegrees.Y>90)
        {Error=TEXT("Each biome needs a unique stable ID, name, finite ordered habitat ranges and valid Character, moisture and slope values.");return false;}
        Seen.Add(B.Id);
    }
    Error.Reset();return true;
}
FCEClimateSample FCEClimateEvaluator::Evaluate(const FCEPlanetEcosystem& M,FVector P,FVector Normal,double Radius,int32 PlanetSeed)
{
    FCEClimateSample S;const FVector D=P.GetSafeNormal();if(D.IsNearlyZero()||!FMath::IsFinite(Radius)||Radius<=0||P.ContainsNaN()||Normal.ContainsNaN())return S;
    const auto& C=M.Climate;const double Lat=FMath::Asin(FMath::Clamp(FVector::DotProduct(D,C.PoleAxis.GetSafeNormal()),-1.,1.));
    S.LatitudeDegrees=FMath::RadiansToDegrees(Lat);S.AltitudeMeters=P.Length()-Radius-C.SeaLevelMeters;S.bWater=S.AltitudeMeters < -1.e-6;
    S.SlopeDegrees=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(D,Normal.GetSafeNormal()),0.,1.)));
    const uint32 Seed=Mix(uint32(PlanetSeed)^Mix(uint32(C.Seed)));const double N=Noise(D*C.NoiseFrequency,Seed);
    S.TemperatureC=FMath::Lerp(C.EquatorTemperatureC,C.PoleTemperatureC,FMath::Square(FMath::Sin(Lat)))-FMath::Max(0.,S.AltitudeMeters)*C.LapseRateCPerKm/1000+N*C.TemperatureVariationC;
    const double Belt=FMath::Exp(-FMath::Square((FMath::Abs(S.LatitudeDegrees)-28)/14));
    S.Moisture=FMath::Clamp(C.Moisture-C.SubtropicalDrying*Belt+Noise(D*C.NoiseFrequency,Seed^0xa511e9b3U)*C.MoistureVariation,0.,1.);
    int32 Priority=MIN_int32;double Best=-1;FString Key;
    for(const auto& B:M.Biomes)
    {
        if(!B.bEnabled||B.bWater!=S.bWater||!Inside(S.AltitudeMeters,B.AltitudeMeters)||!Inside(S.SlopeDegrees,B.SlopeDegrees)
           ||!Inside(S.TemperatureC,B.TemperatureC,B.Character*10)||!Inside(S.Moisture,B.Moisture,B.Character*.2))continue;
        // Stable seeded tie-break: independent of row order and editor-only visibility.
        const double Score=Noise(D*C.NoiseFrequency,Mix(Seed^uint32(B.Seed)^GetTypeHash(B.Id)));const FString Id=B.Id.ToString();
        if(B.Priority>Priority||(B.Priority==Priority&&(Score>Best||(Score==Best&&(Key.IsEmpty()||Id<Key)))))
        {Priority=B.Priority;Best=Score;Key=Id;S.BiomeId=B.Id;S.BiomeWeight=1;}
    }
    return S;
}
UCEPlanetEcosystemAsset::UCEPlanetEcosystemAsset(){Ecosystem=FCEPlanetEcosystem::MakeDefault();}
#if WITH_EDITOR
void UCEPlanetEcosystemAsset::PostEditChangeProperty(FPropertyChangedEvent& E){Super::PostEditChangeProperty(E);OnEcosystemChanged.Broadcast();}
void UCEPlanetEcosystemAsset::PostEditUndo(){Super::PostEditUndo();OnEcosystemChanged.Broadcast();}
#endif
