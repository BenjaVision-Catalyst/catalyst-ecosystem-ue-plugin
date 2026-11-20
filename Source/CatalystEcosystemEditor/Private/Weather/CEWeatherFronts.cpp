#include "Weather/CEWeatherGlobe.h"
#if WITH_CE_LANDFORM
namespace
{
FLinearColor FrontColor(ECEWeatherFrontKind K)
{return K==ECEWeatherFrontKind::Cold?FLinearColor(.01,.035,.38):(K==ECEWeatherFrontKind::Warm?FLinearColor(.48,.012,.02):FLinearColor(.22,.012,.32));}
}
void CEAppendWeatherFrontOverlay(const FCEWeatherGlobeMesh& M,TConstArrayView<FCEWeatherFront> Fronts,double Radius,FCLPlanetBuilderOverlay& O)
{
    if(Radius<=0||!FMath::IsFinite(Radius))return;
    int32 Symbols=0;
    auto Line=[&](FVector A,FVector B,FLinearColor C,float Width=2.5f)
    {if(O.Segments.Num()<16384)O.Segments.Add({A.GetSafeNormal()*Radius,B.GetSafeNormal()*Radius,C,Width});};
    auto Fill=[&](TArray<FVector> Polygon,FLinearColor C)
    {
        if(C.A<=.0001||O.MeshBodyMeters.Num()+1+Polygon.Num()*2>16384||O.MeshIndices.Num()+Polygon.Num()*9>98304)return;
        O.bUseMeshOpacity=true;FVector Centre=FVector::ZeroVector;for(auto P:Polygon)Centre+=P;Centre/=Polygon.Num();
        const int32 Base=O.MeshBodyMeters.Num();O.MeshBodyMeters.Add(Centre.GetSafeNormal()*Radius);O.MeshColors.Add(C);
        for(const auto& P:Polygon)
        {
            O.MeshBodyMeters.Add(P.GetSafeNormal()*Radius);O.MeshColors.Add(C);
            O.MeshBodyMeters.Add((P+(P-Centre).GetSafeNormal()*.001).GetSafeNormal()*Radius);auto Clear=C;Clear.A=0;O.MeshColors.Add(Clear);
        }
        auto Tri=[&](int32 A,int32 B,int32 D){if(FVector::DotProduct(FVector::CrossProduct(O.MeshBodyMeters[B]-O.MeshBodyMeters[A],O.MeshBodyMeters[D]-O.MeshBodyMeters[A]),Centre)<0)Swap(B,D);O.MeshIndices.Append({A,B,D});};
        for(int32 I=0;I<Polygon.Num();++I){const int32 A=Base+1+I*2,B=Base+1+((I+1)%Polygon.Num())*2;Tri(Base,A,B);Tri(A,A+1,B);Tri(B,A+1,B+1);}
    };
    for(const auto& F:Fronts)
    {
        if(F.Points.Num()<2||F.ArcRadians.Num()!=F.Points.Num())continue;
        auto Color=FrontColor(F.Kind);
        for(int32 I=1;I<F.Points.Num();++I){Color.A=(F.Points[I-1].Strength+F.Points[I].Strength)*.5;Line(F.Points[I-1].Direction,F.Points[I].Direction,Color);}
        const int32 Count=FMath::Min(512-Symbols,FMath::Clamp(FMath::FloorToInt(F.ArcRadians.Last()/.14),1,128));
        for(int32 I=0;I<Count;++I,++Symbols)
        {
            FCEWeatherFrontPoint P;FVector T;if(!F.Sample((I+.5)*F.ArcRadians.Last()/Count,P,T))continue;
            const bool Cold=F.Kind==ECEWeatherFrontKind::Cold||(F.Kind==ECEWeatherFrontKind::Stationary&&I%2==0);
            const FVector Advance=P.Warmward*(Cold?1:-1);auto C=FrontColor(Cold?ECEWeatherFrontKind::Cold:ECEWeatherFrontKind::Warm);C.A=P.Strength;const double Size=.014;
            if(Cold)
            {
                const FVector A=P.Direction-T*Size,B=P.Direction+T*Size,Tip=P.Direction+Advance*Size*1.7;
                Fill({A,Tip,B},C);
            }
            else
            {
                TArray<FVector> Polygon;
                for(int32 J=0;J<=12;++J){const double A=J*UE_DOUBLE_PI/12;Polygon.Add(P.Direction+T*(FMath::Cos(A)*Size)+Advance*(FMath::Sin(A)*Size));}
                Fill(Polygon,C);
            }
        }
    }
}
void FCEWeatherFrontPresentation::SetFronts(TArray<FCEWeatherFront> Fronts)
{
    auto Separation=[](const FCEWeatherFront& A,const FCEWeatherFront& B)
    {
        if(A.Points.IsEmpty()||B.Points.IsEmpty())return 1.e100;
        double Sum=0;const int32 Count=FMath::Min(5,A.Points.Num());
        for(int32 I=0;I<Count;++I){const auto D=A.Points[I*(A.Points.Num()-1)/FMath::Max(1,Count-1)].Direction;double Near=1.e100;for(const auto& P:B.Points)Near=FMath::Min(Near,(D-P.Direction).SizeSquared());Sum+=Near;}
        return Sum/Count;
    };
    TArray<FEntry> Next;TSet<int32> Used;
    for(auto& F:Fronts)
    {
        int32 Best=INDEX_NONE;double Score=.04;
        for(int32 I=0;I<Entries.Num();++I)if(!Used.Contains(I)&&Entries[I].Front.Kind==F.Kind)
        {
            const auto& Old=Entries[I].Front;const double Distance=(Separation(F,Old)+Separation(Old,F))*.5;
            if(Distance<Score){Best=I;Score=Distance;}
        }
        FEntry Entry;if(Best!=INDEX_NONE){Used.Add(Best);Entry.Opacity=Entries[Best].Opacity;}
        Entry.Front=MoveTemp(F);Next.Add(MoveTemp(Entry));if(Next.Num()==64)break;
    }
    for(int32 I=0;I<Entries.Num()&&Next.Num()<128;++I)if(!Used.Contains(I)){auto Entry=MoveTemp(Entries[I]);Entry.Retiring=true;Next.Add(MoveTemp(Entry));}
    Entries=MoveTemp(Next);
}
void FCEWeatherFrontPresentation::Tick(double Seconds)
{
    if(!FMath::IsFinite(Seconds)||Seconds<0)return;
    for(auto& E:Entries)E.Opacity=FMath::Clamp(E.Opacity+float(Seconds/.8)*(E.Retiring?-1:1),0.f,1.f);
    Entries.RemoveAll([](const auto& E){return E.Retiring&&E.Opacity<=0;});
}
bool FCEWeatherFrontPresentation::IsAnimating()const
{for(const auto& E:Entries)if(E.Retiring||E.Opacity<1)return true;return false;}
void FCEWeatherFrontPresentation::Append(const FCEWeatherGlobeMesh& Mesh,double Radius,FCLPlanetBuilderOverlay& O)const
{
    TArray<FCEWeatherFront> Visible;
    for(const auto& E:Entries)if(E.Opacity>0){auto F=E.Front;for(auto& P:F.Points)P.Strength*=E.Opacity;Visible.Add(MoveTemp(F));}
    CEAppendWeatherFrontOverlay(Mesh,Visible,Radius,O);
}
#endif
