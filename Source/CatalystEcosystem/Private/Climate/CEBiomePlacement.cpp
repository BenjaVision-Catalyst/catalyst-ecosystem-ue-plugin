#include "Climate/CEBiomePlacement.h"
#include "Algo/Sort.h"
#include "Misc/SecureHash.h"

namespace
{
double Outside(double V,FVector2D R,double Expand=0)
{return FMath::Max3(R.X-Expand-V,0.,V-R.Y-Expand);}
double Penalty(const FCEBiomeDefinition& B,const FCEClimateSample& S,double Radius)
{
    return (B.bWater!=S.bWater?1.e12:0.)+FMath::Min(1.e9,
        FMath::Square(Outside(S.TemperatureC,B.TemperatureC,B.Character*10)/10)+
        FMath::Square(Outside(S.Moisture,B.Moisture,B.Character*.2)/.2)+
        FMath::Square(Outside(S.AltitudeMeters,B.AltitudeMeters)/FMath::Max(50.,Radius*.1))+
        FMath::Square(Outside(S.SlopeDegrees,B.SlopeDegrees)/15));
}
uint32 Rank(int32 Cell,const FCEBiomeDefinition& B,int32 Seed)
{return HashCombineFast(HashCombineFast(GetTypeHash(B.Id),uint32(B.Seed)^uint32(Seed)),uint32(Cell));}
TArray<int32> Neighbors(int32 Cell)
{
    constexpr int32 N=FCEBiomePlacementPlan::Resolution;
    const int32 Face=Cell/(N*N),X=(Cell/N)%N,Y=Cell%N;
    TArray<int32> Out;for(FIntPoint Offset:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
        Out.AddUnique(FCEBiomePlacementPlan::CellIndex(FCEBiomePlacementPlan::Direction(Face,-1+2*(X+Offset.X+.5)/N,-1+2*(Y+Offset.Y+.5)/N)));
    return Out;
}
}
FVector FCEBiomePlacementPlan::Direction(int32 Face,double U,double V)
{
    switch(Face){case 0:return FVector(1,U,V).GetSafeNormal();case 1:return FVector(-1,U,V).GetSafeNormal();
    case 2:return FVector(U,1,V).GetSafeNormal();case 3:return FVector(U,-1,V).GetSafeNormal();
    case 4:return FVector(U,V,1).GetSafeNormal();default:return FVector(U,V,-1).GetSafeNormal();}
}
int32 FCEBiomePlacementPlan::CellIndex(FVector D)
{
    if(D.ContainsNaN()||D.IsNearlyZero())return INDEX_NONE;
    const FVector A=D.GetAbs();int32 Face;double U,V;
    if(A.X>=A.Y&&A.X>=A.Z){Face=D.X>=0?0:1;U=D.Y/A.X;V=D.Z/A.X;}
    else if(A.Y>=A.Z){Face=D.Y>=0?2:3;U=D.X/A.Y;V=D.Z/A.Y;}
    else {Face=D.Z>=0?4:5;U=D.X/A.Z;V=D.Y/A.Z;}
    const int32 X=FMath::Clamp(FMath::FloorToInt((U+1)*.5*Resolution),0,Resolution-1),Y=FMath::Clamp(FMath::FloorToInt((V+1)*.5*Resolution),0,Resolution-1);
    return (Face*Resolution+X)*Resolution+Y;
}
TArray<FCEBiomePlacementCell> FCEBiomePlacementPlan::MakeLattice()
{
    TArray<FCEBiomePlacementCell> Out;Out.Reserve(CellCount);
    auto Area=[](double U,double V){return FMath::Atan2(U*V,FMath::Sqrt(1+U*U+V*V));};
    for(int32 F=0;F<6;++F)for(int32 X=0;X<Resolution;++X)for(int32 Y=0;Y<Resolution;++Y)
    {
        const double U=-1+2.*X/Resolution,V=-1+2.*Y/Resolution,D=2./Resolution;
        auto& C=Out.AddDefaulted_GetRef();C.Normal=Direction(F,U+D*.5,V+D*.5);C.PositionMeters=C.Normal;
        C.SolidAngle=Area(U+D,V+D)-Area(U,V+D)-Area(U+D,V)+Area(U,V);
    }
    return Out;
}
TSharedPtr<const FCEBiomePlacementPlan> FCEBiomePlacementPlan::Build(const FCEPlanetEcosystem& Model,TArray<FCEBiomePlacementCell> Cells,double Radius,int32 Seed,FString& Error)
{
    if(!Model.Validate(Error))return nullptr;
    if(Cells.Num()!=CellCount||!FMath::IsFinite(Radius)||Radius<=0){Error=TEXT("Biome placement needs the complete bounded surface lattice and a positive radius.");return nullptr;}
    auto Plan=MakeShared<FCEBiomePlacementPlan>();Plan->Model=Model;Plan->ReferenceRadiusMeters=Radius;Plan->PlanetSeed=Seed;Plan->Cells=MoveTemp(Cells);
    TArray<const FCEBiomeDefinition*> Enabled;for(const auto& B:Model.Biomes)if(B.bEnabled)Enabled.Add(&B);
    Enabled.Sort([](const auto& A,const auto& B){return A.Id.ToString()<B.Id.ToString();});
    TArray<FCEClimateSample> Samples;Samples.SetNum(CellCount);int32 Supported=0;
    for(int32 I=0;I<CellCount;++I)
    {
        auto& C=Plan->Cells[I];C.BiomeId.Invalidate();
        if(!C.bSupported)continue;
        if(C.PositionMeters.ContainsNaN()||!FMath::IsFinite(C.PositionMeters.SizeSquared())||C.PositionMeters.IsNearlyZero()||C.Normal.ContainsNaN()||C.Normal.IsNearlyZero()||!FMath::IsFinite(C.SolidAngle)||C.SolidAngle<=0)
        {Error=TEXT("Supported biome placement cells require finite ground contacts, normals and positive angular area.");return nullptr;}
        ++Supported;Plan->SupportedSolidAngle+=C.SolidAngle;Samples[I]=FCEClimateEvaluator::Evaluate(Model,C.PositionMeters,C.Normal,Radius,Seed);
        C.BiomeId=Samples[I].BiomeId;
    }
    if(Enabled.IsEmpty()){Error.Reset();return Plan;}
    if(Supported<Enabled.Num()){Error=TEXT("Too little supported ground to represent every enabled biome.");return nullptr;}
    // Constrained habitats reserve first. Stable GUID ordering settles equal costs.
    TMap<FGuid,double> SuitableArea;
    for(const auto* B:Enabled){double Area=0;for(int32 I=0;I<CellCount;++I)if(Plan->Cells[I].bSupported&&Penalty(*B,Samples[I],Radius)==0)Area+=Plan->Cells[I].SolidAngle;SuitableArea.Add(B->Id,Area);}
    Enabled.StableSort([&](const auto& A,const auto& B){return SuitableArea[A.Id]<SuitableArea[B.Id];});
    TBitArray<> Reserved(false,CellCount);
    const double Target=Plan->SupportedSolidAngle*MinimumCoverage;
    for(const auto* B:Enabled)
    {
        auto& R=Plan->Regions.AddDefaulted_GetRef();R.BiomeId=B->Id;
        TBitArray<> Queued(false,CellCount);TArray<int32> Frontier;
        auto Better=[&](int32 A,int32 C)
        {
            const double PA=Penalty(*B,Samples[A],Radius),PC=Penalty(*B,Samples[C],Radius);
            if(PA!=PC)return PA<PC;
            const bool OwnA=Plan->Cells[A].BiomeId==B->Id,OwnC=Plan->Cells[C].BiomeId==B->Id;
            if(OwnA!=OwnC)return OwnA;
            if(R.SeedCell!=INDEX_NONE)
            {
                const FVector Origin=Plan->Cells[R.SeedCell].PositionMeters.GetSafeNormal();
                const double DA=FVector::DotProduct(Origin,Plan->Cells[A].PositionMeters.GetSafeNormal()),DC=FVector::DotProduct(Origin,Plan->Cells[C].PositionMeters.GetSafeNormal());
                if(DA!=DC)return DA>DC;
            }
            return Rank(A,*B,Seed)<Rank(C,*B,Seed)||(Rank(A,*B,Seed)==Rank(C,*B,Seed)&&A<C);
        };
        while(R.ReservedSolidAngle+1.e-12<Target)
        {
            if(Frontier.IsEmpty())
            {
                int32 Best=INDEX_NONE;for(int32 I=0;I<CellCount;++I)if(Plan->Cells[I].bSupported&&!Reserved[I]&&(Best==INDEX_NONE||Better(I,Best)))Best=I;
                if(Best==INDEX_NONE){Error=TEXT("Insufficient unreserved surface for enabled biome coverage.");return nullptr;}
                if(R.SeedCell==INDEX_NONE)R.SeedCell=Best;
                ++R.Components;Frontier.Add(Best);Queued[Best]=true;
            }
            int32 BestIndex=0;for(int32 I=1;I<Frontier.Num();++I)if(Better(Frontier[I],Frontier[BestIndex]))BestIndex=I;
            const int32 Cell=Frontier[BestIndex];Frontier.RemoveAtSwap(BestIndex);
            if(Reserved[Cell])continue;
            Reserved[Cell]=true;Plan->Cells[Cell].BiomeId=B->Id;++R.ReservedCells;R.ReservedSolidAngle+=Plan->Cells[Cell].SolidAngle;
            R.MaximumHabitatPenalty=FMath::Max(R.MaximumHabitatPenalty,Penalty(*B,Samples[Cell],Radius));
            for(int32 Other:Neighbors(Cell))if(Plan->Cells[Other].bSupported&&!Reserved[Other]&&!Queued[Other]){Queued[Other]=true;Frontier.Add(Other);}
        }
    }
    // Natural winners retain the remainder. Gaps use the closest enabled habitat.
    for(int32 I=0;I<CellCount;++I)
    {
        auto& C=Plan->Cells[I];if(!C.bSupported)continue;
        if(!C.BiomeId.IsValid())
        {
            const FCEBiomeDefinition* Best=nullptr;double Cost=MAX_dbl;
            for(const auto* B:Enabled){const double P=Penalty(*B,Samples[I],Radius);if(!Best||P<Cost||(P==Cost&&B->Priority>Best->Priority)||(P==Cost&&B->Priority==Best->Priority&&B->Id.ToString()<Best->Id.ToString())){Best=B;Cost=P;}}
            C.BiomeId=Best->Id;
        }
        for(auto& R:Plan->Regions)if(R.BiomeId==C.BiomeId){R.AssignedSolidAngle+=C.SolidAngle;break;}
    }
    FMD5 Digest;
    for(const auto& C:Plan->Cells)
    {
        Digest.Update(reinterpret_cast<const uint8*>(&C.PositionMeters),sizeof(FVector));
        Digest.Update(reinterpret_cast<const uint8*>(&C.BiomeId),sizeof(FGuid));
    }
    uint8 Bytes[16];Digest.Final(Bytes);Plan->AssignmentKey=BytesToHex(Bytes,16);
    Error.Reset();return Plan;
}
FCEClimateSample FCEBiomePlacementPlan::Evaluate(FVector P,FVector N)const
{
    auto S=FCEClimateEvaluator::Evaluate(Model,P,N,ReferenceRadiusMeters,PlanetSeed);const int32 I=CellIndex(P);
    S.BiomeId=Cells.IsValidIndex(I)?Cells[I].BiomeId:FGuid();S.BiomeWeight=S.BiomeId.IsValid()?1:0;return S;
}
