#include "Weather/CEWeatherFronts.h"
#if WITH_CE_LANDFORM
namespace
{
FCEWeatherFrontPoint Blend(const FCEWeatherFrontPoint& A,const FCEWeatherFrontPoint& B,double F)
{
    FCEWeatherFrontPoint P;P.Direction=FMath::Lerp(A.Direction,B.Direction,F).GetSafeNormal();
    P.Warmward=FVector::VectorPlaneProject(FMath::Lerp(A.Warmward,B.Warmward,F),P.Direction).GetSafeNormal();
    P.Cloud=FMath::Lerp(A.Cloud,B.Cloud,F);P.RainMmHour=FMath::Lerp(A.RainMmHour,B.RainMmHour,F);
    P.CrossWindMetersPerSecond=FMath::Lerp(A.CrossWindMetersPerSecond,B.CrossWindMetersPerSecond,F);return P;
}
FLinearColor FrontColor(ECEWeatherFrontKind K)
{return K==ECEWeatherFrontKind::Cold?FLinearColor(.015,.32,1):(K==ECEWeatherFrontKind::Warm?FLinearColor(1,.1,.035):FLinearColor(.7,.2,.9));}
}
bool FCEWeatherFront::Sample(double Arc,FCEWeatherFrontPoint& P,FVector& Tangent)const
{
    if(Points.Num()<2||ArcRadians.Num()!=Points.Num())return false;
    Arc=FMath::Clamp(Arc,0.,ArcRadians.Last());int32 I=1;while(I<ArcRadians.Num()-1&&ArcRadians[I]<Arc)++I;
    const double Span=ArcRadians[I]-ArcRadians[I-1];if(Span<=0)return false;
    P=Blend(Points[I-1],Points[I],(Arc-ArcRadians[I-1])/Span);
    Tangent=FVector::VectorPlaneProject(Points[I].Direction-Points[I-1].Direction,P.Direction).GetSafeNormal();return !Tangent.IsNearlyZero();
}
TArray<FCEWeatherFront> CEDiagnoseWeatherFronts(const FCEWeatherGlobeMesh& M,TConstArrayView<FCEWeatherSample> Samples)
{
    TArray<FCEWeatherFront> Fronts;const int32 N=M.Directions.Num();
    if(N<3||N>8192||Samples.Num()!=N||M.Indices.Num()%3||M.Indices.Num()>49152)return Fronts;
    for(int32 I:M.Indices)if(I<0||I>=N)return Fronts;
    for(int32 I=0;I<N;++I)if(M.Directions[I].ContainsNaN()||!FMath::IsFinite(Samples[I].TemperatureC)||Samples[I].Wind.ContainsNaN()||!FMath::IsFinite(Samples[I].Cloud)||!FMath::IsFinite(Samples[I].RainMmHour))return Fronts;
    TArray<TArray<int32>> Neighbours;Neighbours.SetNum(N);
    for(int32 I=0;I<M.Indices.Num();I+=3)for(int32 E=0;E<3;++E)
    {const int32 A=M.Indices[I+E],B=M.Indices[I+(E+1)%3];Neighbours[A].AddUnique(B);Neighbours[B].AddUnique(A);}
    TArray<double> Temperature;for(const auto& S:Samples)Temperature.Add(S.TemperatureC);
    // Derivatives amplify small stencil differences at the icosphere's valence-five
    // vertices. Smooth each derivative stage as well as the original temperature.
    auto Smooth=[&](TArray<double>& Values)
    {
        for(int32 Pass=0;Pass<3;++Pass)
        {
            auto Next=Values;
            for(int32 I=0;I<N;++I)if(!Neighbours[I].IsEmpty())
            {double Sum=0;for(int32 J:Neighbours[I])Sum+=Values[J];Next[I]=.5*(Values[I]+Sum/Neighbours[I].Num());}
            Values=MoveTemp(Next);
        }
    };
    Smooth(Temperature);
    auto Gradient=[&](const TArray<double>& Values)
    {
        TArray<FVector> G;G.SetNumZeroed(N);
        for(int32 I=0;I<N;++I)
        {
            FVector X,Y;M.Directions[I].FindBestAxisVectors(X,Y);double XX=0,XY=0,YY=0,BX=0,BY=0;
            for(int32 J:Neighbours[I])
            {
                const FVector Offset=FVector::VectorPlaneProject(M.Directions[J]-M.Directions[I],M.Directions[I]);
                const double U=FVector::DotProduct(Offset,X),V=FVector::DotProduct(Offset,Y),W=1/FMath::Max(1.e-12,U*U+V*V),D=Values[J]-Values[I];
                XX+=U*U*W;XY+=U*V*W;YY+=V*V*W;BX+=D*U*W;BY+=D*V*W;
            }
            const double Det=XX*YY-XY*XY;if(Det>1.e-10)G[I]=X*((BX*YY-BY*XY)/Det)+Y*((BY*XX-BX*XY)/Det);
        }
        return G;
    };
    const auto Thermal=Gradient(Temperature);TArray<double> Strength;for(const auto& G:Thermal)Strength.Add(G.Length());Smooth(Strength);
    const auto StrengthGradient=Gradient(Strength);TArray<double> Across;
    for(int32 I=0;I<N;++I)Across.Add(FVector::DotProduct(StrengthGradient[I],Thermal[I].GetSafeNormal()));
    Smooth(Across);const auto AcrossGradient=Gradient(Across);
    struct FNode{FCEWeatherFrontPoint Point;TArray<int32> Links;};
    struct FLink{int32 A,B;ECEWeatherFrontKind Kind;bool Used=false;};
    TArray<FNode> Nodes;TArray<FLink> Links;TMap<uint64,int32> Ids;TSet<uint64> Edges;
    for(int32 I=0;I<M.Indices.Num()&&Links.Num()<4096;I+=3)
    {
        int32 Hits[3],Count=0;double G=0,Sharpness=0;
        for(int32 E=0;E<3;++E)
        {
            int32 A=M.Indices[I+E],B=M.Indices[I+(E+1)%3];if(A>B)Swap(A,B);const double X=Across[A],Y=Across[B];
            if(!((X<0&&Y>=0)||(Y<0&&X>=0)))continue;
            const double F=-X/(Y-X);const uint64 Key=X==0?(uint64(1)<<63)|uint32(A):(Y==0?(uint64(1)<<63)|uint32(B):(uint64(A)<<32)|uint32(B));
            int32 Id;
            if(const int32* Found=Ids.Find(Key))Id=*Found;
            else
            {
                FNode Node;auto& P=Node.Point;P.Direction=FMath::Lerp(M.Directions[A],M.Directions[B],F).GetSafeNormal();
                P.Warmward=FVector::VectorPlaneProject(FMath::Lerp(Thermal[A],Thermal[B],F),P.Direction).GetSafeNormal();
                P.CrossWindMetersPerSecond=FVector::DotProduct(FMath::Lerp(Samples[A].Wind,Samples[B].Wind,F),P.Warmward);
                P.Cloud=FMath::Clamp(FMath::Lerp(Samples[A].Cloud,Samples[B].Cloud,F),0.,1.);P.RainMmHour=FMath::Clamp(FMath::Lerp(Samples[A].RainMmHour,Samples[B].RainMmHour,F),0.,100.);
                Id=Nodes.Add(MoveTemp(Node));Ids.Add(Key,Id);
            }
            bool Duplicate=false;for(int32 H=0;H<Count;++H)Duplicate|=Hits[H]==Id;if(Duplicate)continue;
            Hits[Count++]=Id;G+=FMath::Lerp(Strength[A],Strength[B],F);
            Sharpness-=FVector::DotProduct(FMath::Lerp(AcrossGradient[A],AcrossGradient[B],F),Nodes[Id].Point.Warmward);
        }
        // A ridge, not a thermal minimum or the smooth planetary latitude gradient.
        // Units are degrees C/radian, keeping this diagnostic independent of planet size.
        if(Count!=2||G<16||Sharpness<4*G)continue;
        const uint64 Key=(uint64(FMath::Min(Hits[0],Hits[1]))<<32)|uint32(FMath::Max(Hits[0],Hits[1]));if(Edges.Contains(Key))continue;Edges.Add(Key);
        const double Cross=(Nodes[Hits[0]].Point.CrossWindMetersPerSecond+Nodes[Hits[1]].Point.CrossWindMetersPerSecond)*.5;
        const auto Kind=Cross>.5?ECEWeatherFrontKind::Cold:(Cross<-.5?ECEWeatherFrontKind::Warm:ECEWeatherFrontKind::Stationary);
        const int32 Link=Links.Add({Hits[0],Hits[1],Kind});Nodes[Hits[0]].Links.Add(Link);Nodes[Hits[1]].Links.Add(Link);
    }
    auto Degree=[&](int32 Node,ECEWeatherFrontKind Kind){int32 D=0;for(int32 L:Nodes[Node].Links)D+=Links[L].Kind==Kind;return D;};
    int32 PointBudget=1024;
    // Open chains first, then closed chains. Shared edge identities join across every
    // former longitude seam and at the poles. Kind changes terminate a chain.
    for(int32 Pass=0;Pass<2;++Pass)for(int32 Seed=0;Seed<Links.Num()&&Fronts.Num()<64&&PointBudget>3;++Seed)
    {
        if(Links[Seed].Used)continue;const auto Kind=Links[Seed].Kind;
        int32 Start=Links[Seed].A;if(Pass==0){if(Degree(Start,Kind)==2)Start=Links[Seed].B;if(Degree(Start,Kind)==2)continue;}
        FCEWeatherFront Front;Front.Kind=Kind;Front.Points.Add(Nodes[Start].Point);Front.ArcRadians.Add(0);
        int32 Current=Start,Edge=Seed;
        while(Edge!=INDEX_NONE&&!Links[Edge].Used&&Front.Points.Num()<PointBudget)
        {
            auto& L=Links[Edge];L.Used=true;Current=L.A==Current?L.B:L.A;
            const auto& P=Nodes[Current].Point;const double Arc=FMath::Acos(FMath::Clamp(FVector::DotProduct(Front.Points.Last().Direction,P.Direction),-1.,1.));
            if(Arc>1.e-8){Front.Points.Add(P);Front.ArcRadians.Add(Front.ArcRadians.Last()+Arc);}
            if(Current==Start||Degree(Current,Kind)!=2)break;
            Edge=INDEX_NONE;for(int32 Candidate:Nodes[Current].Links)if(!Links[Candidate].Used&&Links[Candidate].Kind==Kind){Edge=Candidate;break;}
        }
        if(Front.Points.Num()<4||Front.ArcRadians.Last()<.18)continue;
        PointBudget-=Front.Points.Num();Fronts.Add(MoveTemp(Front));
    }
    return Fronts;
}
void CEAppendWeatherFrontOverlay(const FCEWeatherGlobeMesh& M,TConstArrayView<FCEWeatherFront> Fronts,double Radius,FCLPlanetBuilderOverlay& O)
{
    if(Radius<=0||!FMath::IsFinite(Radius))return;
    int32 Symbols=0;
    auto Line=[&](FVector A,FVector B,FLinearColor C,float Width=2.5f)
    {if(O.Segments.Num()<16384)O.Segments.Add({A.GetSafeNormal()*Radius,B.GetSafeNormal()*Radius,C,Width});};
    for(const auto& F:Fronts)
    {
        if(F.Points.Num()<2||F.ArcRadians.Num()!=F.Points.Num())continue;
        const auto Color=FrontColor(F.Kind);
        for(int32 I=1;I<F.Points.Num();++I)Line(F.Points[I-1].Direction,F.Points[I].Direction,Color);
        const int32 Count=FMath::Min(512-Symbols,FMath::Clamp(FMath::FloorToInt(F.ArcRadians.Last()/.14),1,128));
        for(int32 I=0;I<Count;++I,++Symbols)
        {
            FCEWeatherFrontPoint P;FVector T;if(!F.Sample((I+.5)*F.ArcRadians.Last()/Count,P,T))continue;
            const bool Cold=F.Kind==ECEWeatherFrontKind::Cold||(F.Kind==ECEWeatherFrontKind::Stationary&&I%2==0);
            const FVector Advance=P.Warmward*(Cold?1:-1);const auto C=FrontColor(Cold?ECEWeatherFrontKind::Cold:ECEWeatherFrontKind::Warm);const double Size=.014;
            if(Cold)
            {
                const FVector A=P.Direction-T*Size,B=P.Direction+T*Size,Tip=P.Direction+Advance*Size*1.7;
                Line(A,Tip,C);Line(Tip,B,C);Line(B,A,C);
            }
            else for(int32 J=0;J<8;++J)
            {
                auto Arc=[&](int32 K){const double A=K*UE_DOUBLE_PI/8;return P.Direction+T*(FMath::Cos(A)*Size)+Advance*(FMath::Sin(A)*Size);};
                Line(Arc(J),Arc(J+1),C);
            }
        }
    }
    // Tint the existing transparent shell: width follows cloud, cyan intensity
    // follows actual rain. There is no opaque second globe or per-frame mesh build.
    if(O.MeshColors.Num()!=M.Directions.Num())return;
    struct FBand{FVector A,Delta,Mid;double InverseLength2,Width2,Bound2,Opacity;FLinearColor Tint;};TArray<FBand> Bands;
    for(const auto& F:Fronts)for(int32 J=1;J<F.Points.Num();++J)
    {
        const auto& A=F.Points[J-1];const auto& B=F.Points[J];
        const double Cloud=(A.Cloud+B.Cloud)*.5,Rain=FMath::Clamp((A.RainMmHour+B.RainMmHour)/40.,0.,1.),Width=.025+.045*Cloud+.025*Rain;
        const FVector Delta=B.Direction-A.Direction;
        Bands.Add({A.Direction,Delta,(A.Direction+B.Direction).GetSafeNormal(),1/FMath::Max(1.e-12,Delta.SizeSquared()),Width*Width,FMath::Square(Width+Delta.Length()),.35+.25*Cloud+.25*Rain,FMath::Lerp(FrontColor(F.Kind),FLinearColor(.02,.85,1),float(Rain*.7))});
    }
    for(int32 I=0;I<M.Directions.Num();++I)
    {
        double Best=0;FLinearColor Tint;
        for(const auto& B:Bands)
        {
            if((M.Directions[I]-B.Mid).SizeSquared()>B.Bound2)continue;
            const double Along=FMath::Clamp(FVector::DotProduct(M.Directions[I]-B.A,B.Delta)*B.InverseLength2,0.,1.);
            const FVector Near=(B.A+B.Delta*Along).GetSafeNormal();const double Distance2=(M.Directions[I]-Near).SizeSquared();
            if(Distance2>=B.Width2)continue;
            const double W=FMath::Square(1-Distance2/B.Width2)*B.Opacity;
            if(W>Best){Best=W;Tint=B.Tint;}
        }
        if(Best>0)O.MeshColors[I]=FMath::Lerp(O.MeshColors[I],Tint,float(Best));
    }
}
#endif
