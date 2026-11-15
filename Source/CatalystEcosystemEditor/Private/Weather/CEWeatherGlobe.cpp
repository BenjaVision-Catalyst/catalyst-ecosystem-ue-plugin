#include "Weather/CEWeatherGlobe.h"
#if WITH_CE_LANDFORM
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
bool CEWeatherTerrainInputsMatch(const FCLCompiledPlanetRecipe& A,const FCLCompiledPlanetRecipe& B)
{
    if(!A.SourceRecipe||!B.SourceRecipe)return false;
    auto X=*A.SourceRecipe,Y=*B.SourceRecipe;
    auto Strip=[](FCLPlanetRecipe& R){R.RadialGuides.Guides.RemoveAll([](const auto& G){return G.Name==TEXT("Overlay");});};Strip(X);Strip(Y);
    auto Probe=X;Probe.RenameGuideReferences(TEXT("Overlay"),TEXT("__WeatherOverlayReferenceProbe__"));
    if(!FCLPlanetRecipe::StaticStruct()->CompareScriptStruct(&X,&Probe,0))return false;
    return FCLPlanetRecipe::StaticStruct()->CompareScriptStruct(&X,&Y,0);
}
TSharedPtr<const FCLPlanetBuilderOverlay> CEBuildWeatherGlobe(const FCEWeatherGlobeMesh& M,TConstArrayView<FCEWeatherSample> Samples,double Radius,double Reference,int32 Layer,bool Pressure,bool Wind,bool Heatmap)
{
    auto O=MakeShared<FCLPlanetBuilderOverlay>();O->PointSizePixels=0;
    if(Samples.Num()!=M.Directions.Num()||Samples.IsEmpty()||Radius<=0||!FMath::IsFinite(Radius)||Reference<=0)return O;
    const double HeatRadius=Radius-FMath::Max(Radius*.001,.001),Interval=4*Reference/1013.25;
    if(Heatmap)
    {
        O->MeshIndices=M.Indices;
        for(int32 I=0;I<Samples.Num();++I)
        {
            const auto& S=Samples[I];const double Value=Layer==0?(S.TemperatureC+40)/80.:(Layer==1?S.Wind.Length()/60.:(Layer==2?S.RainMmHour/50.:(S.PressureHpa/Reference-1)*1013.25/64.+.5));const float V=FMath::Clamp(Value,0.,1.);
            FLinearColor C;
            if(Layer==0)C=V<.5?FMath::Lerp(FLinearColor(.05,.15,.95),FLinearColor(.1,.8,.4),V*2):FMath::Lerp(FLinearColor(.1,.8,.4),FLinearColor(1,.08,.02),(V-.5)*2);
            else if(Layer==1)C=FMath::Lerp(FLinearColor(.02,.15,.5),FLinearColor(1,.85,.03),V);
            else if(Layer==2)C=V<.5?FMath::Lerp(FLinearColor(.09,.12,.18),FLinearColor(.02,.65,1),V*2):FMath::Lerp(FLinearColor(.02,.65,1),FLinearColor(.9,.04,.9),(V-.5)*2);
            else C=FMath::Lerp(FLinearColor(.03,.35,.9),FLinearColor(1,.65,.03),V);
            O->MeshBodyMeters.Add(M.Directions[I]*HeatRadius);O->MeshColors.Add(C);
        }
    }
    if(!Pressure&&!Wind)return O;
    double Min=1.e10,Max=-1.e10;for(const auto& S:Samples){Min=FMath::Min(Min,S.PressureHpa);Max=FMath::Max(Max,S.PressureHpa);}
    const double First=FMath::CeilToDouble(Min/Interval)*Interval;
    TArray<FVector> ArrowSites;
    for(int32 Level=0;Level<64&&First+Level*Interval<=Max;++Level)
    {
        const double P=First+Level*Interval;
        for(int32 I=0;I<M.Indices.Num();I+=3)
        {
            FVector Hits[2],Winds[2];int32 Count=0;
            for(int32 E=0;E<3;++E)
            {
                int32 A=M.Indices[I+E],B=M.Indices[I+(E+1)%3];if(A>B)Swap(A,B);const double X=Samples[A].PressureHpa,Y=Samples[B].PressureHpa;
                if((X<P&&Y>=P)||(Y<P&&X>=P))
                {
                    const double F=(P-X)/(Y-X);if(Count<2){Hits[Count]=FMath::Lerp(M.Directions[A],M.Directions[B],F).GetSafeNormal();Winds[Count]=FMath::Lerp(Samples[A].Wind,Samples[B].Wind,F);++Count;}
                }
            }
            if(Count!=2||(Hits[0]-Hits[1]).IsNearlyZero(1.e-12))continue;
            if(Pressure&&O->Segments.Num()<16384)O->Segments.Add({Hits[0]*Radius,Hits[1]*Radius,FLinearColor(.92,.96,1),1.25f});
            if(Wind&&O->BodyMeters.Num()<256&&I%12==0)
            {
                const FVector D=(Hits[0]+Hits[1]).GetSafeNormal();const FVector W=FVector::VectorPlaneProject((Winds[0]+Winds[1])*.5,D);if(W.Length()<.25)continue;
                bool Close=false;for(const auto& Site:ArrowSites)if(FVector::DotProduct(D,Site)>.985){Close=true;break;}if(Close)continue;
                ArrowSites.Add(D);O->BodyMeters.Add(D*Radius);O->Colors.Add(FLinearColor::White);O->BodyVectorsMeters.Add(W.GetSafeNormal()*Radius*FMath::Clamp(W.Length()*.002,.018,.06));
            }
        }
    }
    if(Pressure)
    {
        TArray<TArray<int32>> Neighbours;Neighbours.SetNum(Samples.Num());
        for(int32 I=0;I<M.Indices.Num();I+=3)for(int32 E=0;E<3;++E){const int32 A=M.Indices[I+E],B=M.Indices[I+(E+1)%3];Neighbours[A].AddUnique(B);Neighbours[B].AddUnique(A);}
        struct FExtremum{int32 Index;bool High;double Strength;};TArray<FExtremum> Candidates;
        for(int32 I=0;I<Samples.Num();++I)
        {
            bool High=true,Low=true;double Contrast=0;
            for(int32 J:Neighbours[I]){const double D=Samples[I].PressureHpa-Samples[J].PressureHpa;High&=D>=0;Low&=D<=0;Contrast=FMath::Max(Contrast,FMath::Abs(D));}
            if((High||Low)&&Contrast>Interval*.005)Candidates.Add({I,High,FMath::Abs(Samples[I].PressureHpa-(Min+Max)*.5)});
        }
        Candidates.Sort([](const auto& A,const auto& B){return A.Strength==B.Strength?A.Index<B.Index:A.Strength>B.Strength;});TArray<FVector> Sites;
        for(const auto& C:Candidates)
        {
            const FVector D=M.Directions[C.Index];bool Close=false;for(const auto& Site:Sites)if(FVector::DotProduct(D,Site)>.9){Close=true;break;}if(Close)continue;
            Sites.Add(D);O->Labels.Add({D*Radius,FString::Printf(TEXT("%s %.0f"),C.High?TEXT("H"):TEXT("L"),Samples[C.Index].PressureHpa),C.High?FLinearColor(1,.7,.02):FLinearColor(.05,.85,1)});if(O->Labels.Num()==16)break;
        }
    }
    return O;
}
#endif
