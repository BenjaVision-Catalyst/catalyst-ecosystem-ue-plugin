#include "Climate/CEPlanetScenery.h"
#include "Climate/CEPlanetBiomePlacement.h"
#if WITH_CE_PLACEMENT_LANDFORM
#include "CLPlanet.h"
#include "PCG/CLPlanetSurfacePCG.h"
#include "Components/BoxComponent.h"
#include "Animation/TransformProviderData.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "PerQualityLevelProperties.h"
#include "Helpers/PCGGraphParametersHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Containers/Ticker.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
#include "Engine/Texture2D.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FTSTicker::FDelegateHandle Handle;
FDelegateHandle CoverReady,CoverRetire;
struct FState
{
    TArray<TWeakObjectPtr<AActor>> Sources;FString Error,Configuration;
    TSet<TWeakObjectPtr<UPCGComponent>> Products;
    bool CoverDirty=true;double LastCover=0;FString CoverAssignment;
    TMap<FName,TStrongObjectPtr<UTexture2D>> CoverTextures;
};
TMap<TWeakObjectPtr<ACLPlanet>,FState> States;
struct FInputs
{
    UClass* Class=nullptr;UObject *Definition=nullptr,*Cache=nullptr;UTransformProviderData* Wind=nullptr;
    TArray<UObject*> Near,Far;UPCGGraph* Graph=nullptr;FString Key;
};
bool Inputs(ACLPlanet& Planet,FInputs& Out,FString& Error)
{
    Error.Reset();if(!Planet.RecipeAsset||!Planet.RecipeAsset->AuthoringExtensions.Contains(TEXT("CatalystEcosystem")))return true;
    auto* P=CEPlanetEcosystemProfile(&Planet);if(!P||!P->Ecosystem.Validate(Error)){if(Error.IsEmpty())Error=TEXT("The linked ecosystem is unavailable.");return false;}
    for(const auto& B:P->Ecosystem.Biomes)if(B.bEnabled&&!B.BiomeCoreAsset.IsNull())
    {
        auto* A=(B.DetailBiomeCoreAsset.IsNull()?B.BiomeCoreAsset:B.DetailBiomeCoreAsset).TryLoad();if(!A){Error=TEXT("Missing biome content: ")+B.BiomeCoreAsset.ToString();return false;}Out.Near.Add(A);
        if(!B.DistantBiomeCoreAsset.IsNull())
        {auto* D=B.DistantBiomeCoreAsset.TryLoad();if(!D){Error=TEXT("Missing distant biome content: ")+B.DistantBiomeCoreAsset.ToString();return false;}Out.Far.Add(D);}
    }
    if(Out.Near.IsEmpty())return true;
    Out.Graph=Cast<UPCGGraph>(P->RuntimeGraph.TryLoad());
    Out.Class=P->RegionActorClass.TryLoadClass<AActor>();Out.Definition=P->RegionDefinition.TryLoad();Out.Cache=P->LocalBiomeCache.TryLoad();
    Out.Wind=P->WindTransformProvider.IsNull()?nullptr:Cast<UTransformProviderData>(P->WindTransformProvider.TryLoad());
    if(!Out.Graph||!Out.Class||!Out.Definition||!Out.Cache||(!P->WindTransformProvider.IsNull()&&!Out.Wind)||
        !FMath::IsFinite(P->NearRadiusMeters)||P->NearRadiusMeters<32||
        !FMath::IsFinite(P->DistantRadiusMeters)||P->DistantRadiusMeters<P->NearRadiusMeters)
    {Error=TEXT("The ecosystem has content but its live deployment references or distances are incomplete.");return false;}
    Out.Key=TEXT("NativePCG=1|")+Out.Graph->GetPathName()+Out.Class->GetPathName()+Out.Definition->GetPathName()+Out.Cache->GetPathName()+P->WindTransformProvider.ToString();
    for(auto* A:Out.Near)Out.Key+=TEXT("|N:")+A->GetPathName();for(auto* A:Out.Far)Out.Key+=TEXT("|F:")+A->GetPathName();Out.Key+=FString::Printf(TEXT("|%g|%g"),P->NearRadiusMeters,P->DistantRadiusMeters);Out.Key=FMD5::HashAnsiString(*Out.Key);return true;
}
bool Bind(AActor& A,const FInputs& I,bool Distant,FString& Error)
{
    auto* Definition=FindFProperty<FObjectPropertyBase>(A.GetClass(),TEXT("Definition"));auto* Assets=FindFProperty<FArrayProperty>(A.GetClass(),TEXT("Assets"));
    auto* Inner=Assets?CastField<FObjectPropertyBase>(Assets->Inner):nullptr;const auto& Values=Distant?I.Far:I.Near;
    if(!Definition||!Inner||!I.Definition->IsA(Definition->PropertyClass)){Error=TEXT("Incompatible native Biome Core region schema.");return false;}
    for(auto* V:Values)if(!V->IsA(Inner->PropertyClass)){Error=TEXT("Incompatible biome content reference.");return false;}
    Definition->SetObjectPropertyValue_InContainer(&A,I.Definition);FScriptArrayHelper List(Assets,Assets->ContainerPtrToValuePtr<void>(&A));List.Resize(Values.Num());
    for(int32 N=0;N<Values.Num();++N)Inner->SetObjectPropertyValue(List.GetRawPtr(N),Values[N]);
    auto* C=A.FindComponentByClass<UPCGComponent>();if(!C||!C->GetGraphInstance()){Error=TEXT("Native biome region has no PCG graph.");return false;}
    C->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateOnDemand;
#if WITH_EDITOR
    C->bRegenerateInEditor=false;C->bForceGenerateOnBPAddedToWorld=false;C->bGenerateOnDropWhenTriggerOnDemand=false;
#endif
    if(C->IsGenerating())C->CancelGeneration();
    UPCGGraphParametersHelpers::SetObjectParameter(C->GetGraphInstance(),TEXT("LocalBiomeCache"),I.Cache);
    UPCGGraphParametersHelpers::SetBoolParameter(C->GetGraphInstance(),TEXT("LocalPreview"),false);
    for(const TCHAR* Name:{TEXT("BlendingRange"),TEXT("BlendingNoise"),TEXT("BlendingDensity")})
        if(auto* P=FindFProperty<FNumericProperty>(A.GetClass(),Name))P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(&A),0.);
    return true;
}
void UpdateCover(ACLPlanet& Planet,UCEPlanetEcosystemAsset& Profile,const FCEBiomePlacementPlan& Plan,FState& State)
{
    const double Now=FPlatformTime::Seconds();
    if(State.CoverAssignment!=Plan.AssignmentKey){State.CoverAssignment=Plan.AssignmentKey;State.CoverDirty=true;}
    if(!State.CoverDirty||Now-State.LastCover<.5)return;State.LastCover=Now;State.CoverDirty=false;
    constexpr int32 Width=1024,Height=512;double Radius=0;
    for(const auto& C:Plan.Cells)if(C.bSupported)Radius=FMath::Max(Radius,C.PositionMeters.Length());if(Radius<=0)return;
    TSet<UObject*> CrownMeshes;
    for(const auto& B:Profile.Ecosystem.Biomes)if(B.bEnabled)if(auto* Asset=B.DistantBiomeCoreAsset.TryLoad())
    {
        auto* P=FindFProperty<FArrayProperty>(Asset->GetClass(),TEXT("BiomeAssets"));auto* Entry=P?CastField<FStructProperty>(P->Inner):nullptr;if(!Entry)continue;
        FScriptArrayHelper Entries(P,P->ContainerPtrToValuePtr<void>(Asset));
        for(TFieldIterator<FObjectPropertyBase> F(Entry->Struct);F;++F)if(F->GetName().StartsWith(TEXT("Mesh_")))
            for(int32 I=0;I<Entries.Num();++I)if(auto* Mesh=F->GetObjectPropertyValue_InContainer(Entries.GetRawPtr(I)))CrownMeshes.Add(Mesh);
    }
    TArray<float> Density;Density.SetNumZeroed(Width*Height);
    auto Crown=[&](FVector World)
    {
        const auto D=Planet.GetActorTransform().InverseTransformPosition(World).GetSafeNormal();
        const double Lat=FMath::Asin(FMath::Clamp(D.Z,-1.,1.)),Lon=FMath::Atan2(D.Y,D.X);
        const double SX=FMath::Max(.6,5.5/(Radius*FMath::Max(.08,FMath::Cos(Lat)))*Width/(2*UE_PI)),SY=FMath::Max(.6,5.5/Radius*Height/UE_PI);
        const double CX=(Lon/(2*UE_PI)+.5)*Width-.5,CY=(.5-Lat/UE_PI)*Height-.5;
        for(int32 Y=FMath::Max(0,FMath::FloorToInt(CY-3*SY));Y<=FMath::Min(Height-1,FMath::CeilToInt(CY+3*SY));++Y)
            for(int32 X=FMath::FloorToInt(CX-3*SX);X<=FMath::CeilToInt(CX+3*SX);++X)
                Density[Y*Width+(X%Width+Width)%Width]+=float(FMath::Exp(-.5*FMath::Square((X-CX)/SX)-.5*FMath::Square((Y-CY)/SY)));
    };
    for(const auto& Weak:State.Products)if(auto* PCG=Weak.Get();PCG&&PCG->bGenerated&&!PCG->IsGenerating())
    {
        auto* A=PCG->GetOwner();
        TInlineComponentArray<UInstancedSkinnedMeshComponent*> Skin(A);for(auto* C:Skin)if(CrownMeshes.Contains(C->GetSkinnedAsset()))
            for(int32 I=0;I<C->GetInstanceCount();++I){FTransform T;if(C->GetInstanceTransform(C->GetInstanceId(I),T,true))Crown(T.GetLocation());}
        TInlineComponentArray<UInstancedStaticMeshComponent*> Static(A);for(auto* C:Static)if(CrownMeshes.Contains(C->GetStaticMesh()))
            for(int32 I=0;I<C->GetInstanceCount();++I){FTransform T;if(C->GetInstanceTransform(I,T,true))Crown(T.GetLocation());}
    }
    for(const auto& B:Profile.Ecosystem.Biomes)if(!B.GroundCoverMaskParameter.IsNone())
    {
        auto* Found=State.CoverTextures.Find(B.GroundCoverMaskParameter);
        if(!Found)
        {
            auto* T=UTexture2D::CreateTransient(Width,Height,PF_B8G8R8A8);T->SRGB=false;T->NeverStream=true;T->AddressX=TA_Wrap;T->AddressY=TA_Clamp;T->UpdateResource();
            Found=&State.CoverTextures.Add(B.GroundCoverMaskParameter,TStrongObjectPtr<UTexture2D>(T));Planet.PresentationTextures.Add(B.GroundCoverMaskParameter,T);
        }
        auto* Bytes=static_cast<uint8*>(FMemory::Malloc(Width*Height*sizeof(FColor)));auto* Pixels=reinterpret_cast<FColor*>(Bytes);
        for(int32 Y=0;Y<Height;++Y)for(int32 X=0;X<Width;++X)
        {
            const double Lat=(.5-(Y+.5)/Height)*UE_PI,Lon=((X+.5)/Width-.5)*2*UE_PI;const FVector D(FMath::Cos(Lat)*FMath::Cos(Lon),FMath::Cos(Lat)*FMath::Sin(Lon),FMath::Sin(Lat));
            const int32 I=Y*Width+X;const int32 Cell=Plan.CellIndex(D);const bool Biome=B.bEnabled&&Plan.Cells.IsValidIndex(Cell)&&Plan.Cells[Cell].BiomeId==B.Id;
            Pixels[I]=FColor(Biome?255:0,uint8(FMath::RoundToInt(255*(1-FMath::Exp(-1.3*Density[I])))),0,255);
        }
        auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Width,Height);
        Found->Get()->UpdateTextureRegions(0,1,Region,Width*sizeof(FColor),sizeof(FColor),Bytes,[](uint8* Data,const FUpdateTextureRegion2D* R){FMemory::Free(Data);delete R;});
    }
}
void Clear(ACLPlanet& Planet,FState& S)
{
    S.Products.Reset();
    for(auto& Weak:S.Sources)if(auto* A=Weak.Get()){CLPlanetSceneryRetiring().Broadcast(&Planet,A);A->Destroy();}
    S.Sources.Reset();for(const auto& Pair:S.CoverTextures)Planet.PresentationTextures.Remove(Pair.Key);
    S.CoverTextures.Reset();S.CoverAssignment.Reset();S.CoverDirty=true;
}
void SetRadii(UPCGComponent& C,double Radius)
{
    C.bOverrideGenerationRadii=true;
    for(TFieldIterator<FStructProperty> F(FPCGRuntimeGenerationRadii::StaticStruct());F;++F)
        if(F->GetName().StartsWith(TEXT("Radius")))
        {
            auto* V=F->ContainerPtrToValuePtr<FPerQualityLevelFloat>(&C.GenerationRadii);V->Default=Radius;
        }
    C.GenerationRadii.ComputeHash();
}
void Update(ACLPlanet& Planet,FState& S)
{
    FInputs I;FString Error;if(!Inputs(Planet,I,Error)){if(S.Error!=Error){S.Error=Error;UE_LOG(LogTemp,Warning,TEXT("ECOSYSTEM_NATIVE_SCENERY: %s"),*Error);}return;}
    if(I.Near.IsEmpty()){Clear(Planet,S);return;}
    auto* Profile=CEPlanetEcosystemProfile(&Planet);bool Pending=false;auto Plan=CEGetPlanetBiomePlacement(&Planet,Profile,Pending,Error);if(!Plan)return;
    if(S.Configuration!=I.Key||S.Sources.IsEmpty())
    {
        Clear(Planet,S);S.Configuration=I.Key;
        auto* Subsystem=UPCGSubsystem::GetInstance(Planet.GetWorld());
        if(!Subsystem->GetPCGWorldActor())
        {FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;Planet.GetWorld()->SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(),FTransform::Identity,Spawn);}
        for(bool Far:{false,true})
        {
            if(Far&&I.Far.IsEmpty())continue;
            FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
            auto* A=Planet.GetWorld()->SpawnActor<AActor>(I.Class,FTransform(Planet.GetActorLocation()),Spawn);if(!A)continue;
            if(auto* C=A->FindComponentByClass<UPCGComponent>())
            {
                C->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateOnDemand;
                auto* Graph=DuplicateObject<UPCGGraph>(I.Graph,A);FindFProperty<FBoolProperty>(UPCGGraph::StaticClass(),TEXT("bUseHierarchicalGeneration"))->SetPropertyValue_InContainer(Graph,true);
                FindFProperty<FBoolProperty>(UPCGGraph::StaticClass(),TEXT("bUse2DGrid"))->SetPropertyValue_InContainer(Graph,false);
                auto* Grid=FindFProperty<FEnumProperty>(UPCGGraph::StaticClass(),TEXT("HiGenGridSize"));
                Grid->GetUnderlyingProperty()->SetIntPropertyValue(Grid->ContainerPtrToValuePtr<void>(Graph),int64(Far?EPCGHiGenGrid::Grid512:EPCGHiGenGrid::Grid64));C->SetGraph(Graph);
            }
            if(auto* Box=A->FindComponentByClass<UBoxComponent>())
            {Box->SetBoxExtent(FVector(Planet.RadiusMeters*100),false);Box->SetVisibility(false);}
            auto* Region=UCLPlanetPCGLibrary::BindSource(A,&Planet);Region->TransformProvider=I.Wind;
            auto* State=NewObject<UCEBiomePlacementState>(A);A->AddInstanceComponent(State);State->RegisterComponent();State->bDistant=Far;
            if(!Bind(*A,I,Far,Error)){A->Destroy();S.Error=Error;continue;}
            auto* C=A->FindComponentByClass<UPCGComponent>();SetRadii(*C,(Far?Profile->DistantRadiusMeters:Profile->NearRadiusMeters)*100);
            C->GenerationTrigger=EPCGComponentGenerationTrigger::GenerateAtRuntime;C->SetSchedulingPolicyClass(C->SchedulingPolicyClass);C->SetIsPartitioned(true);S.Sources.Add(A);
#if WITH_EDITOR
            A->SetActorLabel(Far?TEXT("Ecosystem | Native distant canopy"):TEXT("Ecosystem | Native local detail"));
#endif
            C->GetSubsystem()->RegisterOrUpdateExecutionSource(C,true);
        }
    }
    S.Error.Reset();
    // Observe native products for presentation/coverage only. The PCG runtime
    // scheduler owns cell creation, generation, cancellation, pooling and cleanup.
    for(TActorIterator<AActor> It(Planet.GetWorld());It;++It)
    {
        TInlineComponentArray<UPCGComponent*> Components(*It);
        for(auto* C:Components)
        {
            auto* Original=C->GetOriginalComponent();if(C==Original||!Original||!S.Sources.Contains(Original->GetOwner()))continue;
            S.Products.Add(C);
        }
    }
    for(auto It=S.Products.CreateIterator();It;++It)
        if(auto* C=It->Get();!C||!C->GetOriginalComponent()||!S.Sources.Contains(C->GetOriginalComponent()->GetOwner()))It.RemoveCurrent();
    UpdateCover(Planet,*Profile,*Plan,S);
}
bool Tick(float)
{
    if(!GEngine)return true;
    for(const auto& Context:GEngine->GetWorldContexts())if(Context.WorldType==EWorldType::Game||Context.WorldType==EWorldType::PIE)
        if(auto* W=Context.World();W&&!W->bIsTearingDown)for(TActorIterator<ACLPlanet> It(W);It;++It)
            if(It->RecipeAsset&&It->RecipeAsset->AuthoringExtensions.Contains(TEXT("CatalystEcosystem")))Update(**It,States.FindOrAdd(*It));
    for(auto It=States.CreateIterator();It;++It)
    {
        auto* P=It.Key().Get();
        if(!P){It.RemoveCurrent();continue;}
        if(!P->RecipeAsset||!P->RecipeAsset->AuthoringExtensions.Contains(TEXT("CatalystEcosystem")))
        {Clear(*P,It.Value());It.RemoveCurrent();}
    }
    return true;
}
}
FString CEPlanetSceneryReport(UObject* Object)
{
    auto* P=Cast<ACLPlanet>(Object);auto* S=P?States.Find(P):nullptr;auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Cells,Sources;
    if(S)
    {
        for(auto& W:S->Sources)if(auto* A=W.Get())
        {
            auto* C=A->FindComponentByClass<UPCGComponent>();auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("actor"),A->GetPathName());
            V->SetBoolField(TEXT("runtimeManaged"),C&&C->IsManagedByRuntimeGenSystem());V->SetBoolField(TEXT("partitioned"),C&&C->IsPartitioned());Sources.Add(MakeShared<FJsonValueObject>(V));
        }
        for(auto& Weak:S->Products)if(auto* C=Weak.Get())
        {
            auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("component"),C->GetPathName());V->SetStringField(TEXT("actor"),C->GetOwner()->GetPathName());
            V->SetBoolField(TEXT("generated"),C->bGenerated);V->SetBoolField(TEXT("generating"),C->IsGenerating());V->SetBoolField(TEXT("runtimeManaged"),C->IsManagedByRuntimeGenSystem());
            V->SetNumberField(TEXT("gridCm"),C->GetGenerationGridSize());Cells.Add(MakeShared<FJsonValueObject>(V));
        }
        J->SetNumberField(TEXT("coverageTextures"),S->CoverTextures.Num());J->SetStringField(TEXT("error"),S->Error);
    }
    J->SetArrayField(TEXT("sources"),Sources);J->SetArrayField(TEXT("cells"),Cells);J->SetStringField(TEXT("scheduler"),TEXT("Unreal PCG Runtime Generation"));
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(J,Writer);return Text;
}
bool CEValidatePlanetScenery(UObject* Object,FString& Error)
{auto* P=Cast<ACLPlanet>(Object);if(!P){Error=TEXT("Expected an explicit Planet.");return false;}FInputs I;return Inputs(*P,I,Error);}
void CEStartPlanetScenery()
{
    if(!Handle.IsValid())Handle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),.25f);
    auto Changed=[](ACLPlanet* P,AActor*){if(auto* S=States.Find(P))S->CoverDirty=true;};
    if(!CoverReady.IsValid())CoverReady=CLPlanetSceneryReady().AddLambda(Changed);if(!CoverRetire.IsValid())CoverRetire=CLPlanetSceneryRetiring().AddLambda(Changed);
    FCLSceneryReadiness::Register(TEXT("CatalystEcosystem"),[](const ACLPlanet& P,FVector At,FString& Status)
    {
        FInputs InputsValue;FString Error;if(!Inputs(const_cast<ACLPlanet&>(P),InputsValue,Error)){Status=Error;return false;}if(InputsValue.Near.IsEmpty())return true;
        auto* S=States.Find(const_cast<ACLPlanet*>(&P));if(!S||S->Sources.IsEmpty()){Status=TEXT("Preparing native runtime PCG.");return false;}
        bool NearReady=false,FarReady=InputsValue.Far.IsEmpty();
        for(const auto& Weak:S->Products)if(auto* C=Weak.Get();C&&C->bGenerated&&!C->IsGenerating())
            if(C->GetGridBounds().ComputeSquaredDistanceToPoint(At)<=1)
                if(auto* Original=C->GetOriginalComponent())if(auto* State=Original->GetOwner()->FindComponentByClass<UCEBiomePlacementState>())
                {if(State->bDistant)FarReady=true;else NearReady=true;}
        if(NearReady&&FarReady)return true;
        Status=TEXT("Generating the local native PCG cell.");return false;
    });
}
void CEStopPlanetScenery(){FTSTicker::RemoveTicker(Handle);Handle.Reset();FCLSceneryReadiness::Unregister(TEXT("CatalystEcosystem"));CLPlanetSceneryReady().Remove(CoverReady);CLPlanetSceneryRetiring().Remove(CoverRetire);CoverReady.Reset();CoverRetire.Reset();States.Reset();}
#else
FString CEPlanetSceneryReport(UObject*){return TEXT("{}");}
bool CEValidatePlanetScenery(UObject*,FString& Error){Error=TEXT("Live planetary scenery requires Landform.");return false;}
void CEStartPlanetScenery(){}
void CEStopPlanetScenery(){}
#endif
