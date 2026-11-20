#include "Weather/CEWeatherFronts.h"
namespace
{
FCEWeatherFrontPoint Blend(const FCEWeatherFrontPoint& A,const FCEWeatherFrontPoint& B,double F)
{
    FCEWeatherFrontPoint P;P.Direction=FMath::Lerp(A.Direction,B.Direction,F).GetSafeNormal();
    P.Warmward=FVector::VectorPlaneProject(FMath::Lerp(A.Warmward,B.Warmward,F),P.Direction).GetSafeNormal();
    P.Strength=FMath::Lerp(A.Strength,B.Strength,F);P.Cloud=FMath::Lerp(A.Cloud,B.Cloud,F);P.RainMmHour=FMath::Lerp(A.RainMmHour,B.RainMmHour,F);
    P.CrossWindMetersPerSecond=FMath::Lerp(A.CrossWindMetersPerSecond,B.CrossWindMetersPerSecond,F);return P;
}
}
FCEWeatherGlobeMesh FCEWeatherGlobeMesh::Make()
{
    FCEWeatherGlobeMesh M;const double T=(1+FMath::Sqrt(5.))*.5;
    M.Directions={{-1,T,0},{1,T,0},{-1,-T,0},{1,-T,0},{0,-1,T},{0,1,T},{0,-1,-T},{0,1,-T},{T,0,-1},{T,0,1},{-T,0,-1},{-T,0,1}};
    for(auto& D:M.Directions)D.Normalize();
    M.Indices={0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11, 1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8, 3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9, 4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1};
    for(int Pass=0;Pass<4;++Pass)
    {
        TMap<uint64,int32> Cache;TArray<int32> Next;Next.Reserve(M.Indices.Num()*4);
        auto Mid=[&](int32 A,int32 B){const uint64 Key=uint64(FMath::Min(A,B))<<32|uint32(FMath::Max(A,B));if(const int32* Found=Cache.Find(Key))return *Found;const int32 I=M.Directions.Add((M.Directions[A]+M.Directions[B]).GetSafeNormal());Cache.Add(Key,I);return I;};
        for(int32 I=0;I<M.Indices.Num();I+=3){const int32 A=M.Indices[I],B=M.Indices[I+1],C=M.Indices[I+2],AB=Mid(A,B),BC=Mid(B,C),CA=Mid(C,A);Next.Append({A,AB,CA,B,BC,AB,C,CA,BC,AB,BC,CA});}
        M.Indices=MoveTemp(Next);
    }
    return M;
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
                const double ThermalStrength=FMath::Lerp(Strength[A],Strength[B],F);
                const double Sharp=-FVector::DotProduct(FMath::Lerp(AcrossGradient[A],AcrossGradient[B],F),P.Warmward);
                auto Ease=[](double V){V=FMath::Clamp(V,0.,1.);return V*V*(3-2*V);};
                P.Strength=Ease((ThermalStrength-8)/8)*Ease((Sharp/FMath::Max(.001,ThermalStrength)-4)/4);
                P.Cloud=FMath::Clamp(FMath::Lerp(Samples[A].Cloud,Samples[B].Cloud,F),0.,1.);P.RainMmHour=FMath::Clamp(FMath::Lerp(Samples[A].RainMmHour,Samples[B].RainMmHour,F),0.,100.);
                Id=Nodes.Add(MoveTemp(Node));Ids.Add(Key,Id);
            }
            bool Duplicate=false;for(int32 H=0;H<Count;++H)Duplicate|=Hits[H]==Id;if(Duplicate)continue;
            Hits[Count++]=Id;G+=FMath::Lerp(Strength[A],Strength[B],F);
            Sharpness-=FVector::DotProduct(FMath::Lerp(AcrossGradient[A],AcrossGradient[B],F),Nodes[Id].Point.Warmward);
        }
        // A ridge, not a thermal minimum or the smooth planetary latitude gradient.
        // Units are degrees C/radian, keeping this diagnostic independent of planet size.
        if(Count!=2||(Nodes[Hits[0]].Point.Strength<.0001&&Nodes[Hits[1]].Point.Strength<.0001))continue;
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
        if(Front.Points.Num()<2)continue;
        const double LengthFade=FMath::Clamp((Front.ArcRadians.Last()-.03)/.15,0.,1.);
        double Peak=0;for(auto& Point:Front.Points){Point.Strength*=LengthFade*LengthFade*(3-2*LengthFade);Peak=FMath::Max(Peak,Point.Strength);}
        if(Peak<.0001)continue;
        PointBudget-=Front.Points.Num();Fronts.Add(MoveTemp(Front));
    }
    return Fronts;
}
