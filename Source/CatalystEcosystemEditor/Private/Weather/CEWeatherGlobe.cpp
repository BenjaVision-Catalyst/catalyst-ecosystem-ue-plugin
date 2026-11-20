#include "Weather/CEWeatherGlobe.h"
#include "Weather/CEWeatherFronts.h"
#if WITH_CE_LANDFORM
bool CEWeatherTerrainInputsMatch(const FCLCompiledPlanetRecipe& A,const FCLCompiledPlanetRecipe& B)
{
    if(!A.SourceRecipe||!B.SourceRecipe)return false;
    auto X=*A.SourceRecipe,Y=*B.SourceRecipe;
    auto Strip=[](FCLPlanetRecipe& R){R.RadialGuides.Guides.RemoveAll([](const auto& G){return G.Name==TEXT("Overlay");});};Strip(X);Strip(Y);
    auto Probe=X;Probe.RenameGuideReferences(TEXT("Overlay"),TEXT("__WeatherOverlayReferenceProbe__"));
    if(!FCLPlanetRecipe::StaticStruct()->CompareScriptStruct(&X,&Probe,0))return false;
    return FCLPlanetRecipe::StaticStruct()->CompareScriptStruct(&X,&Y,0);
}
TSharedPtr<const FCLPlanetBuilderOverlay> CEBuildWeatherGlobe(const FCEWeatherGlobeMesh& M,TConstArrayView<FCEWeatherSample> Samples,double Radius,double Reference,int32 Layer,bool Pressure,bool Wind,bool Heatmap,bool Fronts,TConstArrayView<FVector> GroundPositions)
{
    auto O=MakeShared<FCLPlanetBuilderOverlay>();O->PointSizePixels=0;O->bUseMeshOpacity=true;
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
            else C=V<.5?FMath::Lerp(FLinearColor(.01,.12,1),FLinearColor(.48,.015,.66),V*2):FMath::Lerp(FLinearColor(.48,.015,.66),FLinearColor(1,.5,.01),(V-.5)*2);
            // Keep the tint legible through the transparent shell without a grey midpoint.
            auto HSV=C.LinearRGBToHSV();HSV.G=FMath::Lerp(HSV.G,1.f,.35f);C=HSV.HSVToLinearRGB();
            C.A=.12f;O->MeshBodyMeters.Add(M.Directions[I]*HeatRadius);O->MeshColors.Add(C);
        }
    }
    if(Heatmap&&GroundPositions.Num()==Samples.Num())
    {
        // Reuse the biome preview's unlit ground swatches and its default pixel size.
        O->PointSizePixels=FCLPlanetBuilderOverlay().PointSizePixels;
        for(int32 I=0;I<Samples.Num();++I)
        {
            const double T=FMath::Clamp((Samples[I].TemperatureC+30)/70.,0.,1.);
            FLinearColor C=T<.5?FMath::Lerp(FLinearColor(.025,.10,.85),FLinearColor(.5,.015,.65),float(T*2)):FMath::Lerp(FLinearColor(.5,.015,.65),FLinearColor(1,.16,.015),float((T-.5)*2));C.A=1.f;
            O->BodyMeters.Add(GroundPositions[I]);O->Colors.Add(C);
        }
    }
    if(Fronts)CEAppendWeatherFrontOverlay(M,CEDiagnoseWeatherFronts(M,Samples),Radius,*O);
    if(!Pressure&&!Wind)return O;
    double Min=1.e10,Max=-1.e10;for(const auto& S:Samples){Min=FMath::Min(Min,S.PressureHpa);Max=FMath::Max(Max,S.PressureHpa);}
    const double First=FMath::CeilToDouble(Min/Interval)*Interval;
    int32 EdgeBudget=16384-O->Segments.Num(),HeadBudget=2048;
    for(int32 Level=0;Level<64&&First+Level*Interval<=Max&&EdgeBudget>0;++Level)
    {
        const double P=First+Level*Interval;
        struct FNode{FVector Direction,Wind;TArray<int32> Links;};
        struct FLink{int32 A,B;bool Used=false;};
        TArray<FNode> Nodes;TArray<FLink> Links;TMap<uint64,int32> NodeIds;TSet<uint64> LinkIds;
        for(int32 I=0;I<M.Indices.Num()&&EdgeBudget>0;I+=3)
        {
            int32 Hits[3],Count=0;
            for(int32 E=0;E<3;++E)
            {
                int32 A=M.Indices[I+E],B=M.Indices[I+(E+1)%3];if(A>B)Swap(A,B);const double X=Samples[A].PressureHpa,Y=Samples[B].PressureHpa;
                if(!((X<P&&Y>=P)||(Y<P&&X>=P)))continue;
                const double F=(P-X)/(Y-X);
                // An exact vertex crossing has one identity across all incident triangles.
                const uint64 Key=X==P?(uint64(1)<<63)|uint32(A):(Y==P?(uint64(1)<<63)|uint32(B):(uint64(A)<<32)|uint32(B));
                int32 N;
                if(const auto* Existing=NodeIds.Find(Key))N=*Existing;
                else{N=Nodes.Num();NodeIds.Add(Key,N);FNode Node;Node.Direction=FMath::Lerp(M.Directions[A],M.Directions[B],F).GetSafeNormal();Node.Wind=FMath::Lerp(Samples[A].Wind,Samples[B].Wind,F);Nodes.Add(MoveTemp(Node));}
                bool Duplicate=false;for(int32 H=0;H<Count;++H)Duplicate|=Hits[H]==N;if(!Duplicate)Hits[Count++]=N;
            }
            if(Count!=2||(Nodes[Hits[0]].Direction-Nodes[Hits[1]].Direction).IsNearlyZero(1.e-12))continue;
            const uint64 Key=(uint64(FMath::Min(Hits[0],Hits[1]))<<32)|uint32(FMath::Max(Hits[0],Hits[1]));if(LinkIds.Contains(Key))continue;LinkIds.Add(Key);
            const int32 Link=Links.Add({Hits[0],Hits[1]});Nodes[Hits[0]].Links.Add(Link);Nodes[Hits[1]].Links.Add(Link);--EdgeBudget;
            if(Pressure)O->Segments.Add({Nodes[Hits[0]].Direction*Radius,Nodes[Hits[1]].Direction*Radius,FLinearColor(.92,.96,1),1.25f});
        }
        if(!Wind)continue;
        int32 Loop=0;
        for(int32 Seed=0;Seed<Links.Num()&&O->Flows.Num()<256&&HeadBudget>0;++Seed)
        {
            if(Links[Seed].Used)continue;
            TArray<int32> Path;const int32 Start=Links[Seed].A;Path.Add(Start);int32 Current=Start,Edge=Seed;
            while(Edge!=INDEX_NONE&&!Links[Edge].Used)
            {
                auto& Link=Links[Edge];Link.Used=true;Current=Link.A==Current?Link.B:Link.A;Path.Add(Current);if(Current==Start)break;
                Edge=INDEX_NONE;for(int32 Candidate:Nodes[Current].Links)if(!Links[Candidate].Used){Edge=Candidate;break;}
            }
            if(Path.Num()<4||Path.Last()!=Start)continue;
            FCLPlanetBuilderOverlayFlow Flow;Flow.Family=uint64(FMath::RoundToInt64(P/Reference*1000000));Flow.Key=(Flow.Family<<16)|uint16(Loop++);
            for(int32 N:Path)Flow.BodyMeters.Add(Nodes[N].Direction*Radius);
            if(!Flow.BuildArcLengths())continue;
            double Strength=0,Along=0;
            for(int32 I=1;I<Path.Num();++I)
            {
                const auto& A=Nodes[Path[I-1]];const auto& B=Nodes[Path[I]];const FVector Mid=(A.Direction+B.Direction).GetSafeNormal();
                const FVector W=FVector::VectorPlaneProject((A.Wind+B.Wind)*.5,Mid),T=FVector::VectorPlaneProject(B.Direction-A.Direction,Mid).GetSafeNormal();
                const double Length=Flow.ArcMeters[I]-Flow.ArcMeters[I-1];Strength+=W.Length()*Length;Along+=FVector::DotProduct(W,T)*Length;
            }
            // Representative signed speed; the view owns persistent markers independently of their count.
            Flow.SpeedMetersPerSecond=Radius*.002*Strength/Flow.LengthMeters()*(Along<0?-1:1);
            Flow.ArrowCount=FMath::Min(HeadBudget,FMath::Clamp(FMath::RoundToInt(Flow.LengthMeters()/(Radius*.36)),1,128));HeadBudget-=Flow.ArrowCount;
            O->Flows.Add(MoveTemp(Flow));
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
            Sites.Add(D);O->Labels.Add({D*Radius,FString(C.High?TEXT("H"):TEXT("L")),C.High?FLinearColor(1,.7,.02):FLinearColor(.05,.85,1)});if(O->Labels.Num()==16)break;
        }
    }
    return O;
}
#endif
