#include "Climate/CEPlanetBiomePlacement.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"
#if WITH_CE_PLACEMENT_LANDFORM
#include "CLPlanet.h"
#include "CLPlanetStamp.h"
#include "PCG/CLPlanetSurfacePCG.h"
#include "Scene/CLGroundSurface.h"
#include "PCGComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#endif

namespace
{
FString ModelKey(const UCEPlanetEcosystemAsset& P)
{
    FString Text=FString::FromInt(FCEBiomePlacementPlan::Profile)+TEXT("|")+P.GetPathName();
    FCEPlanetEcosystem::StaticStruct()->ExportText(Text,&P.Ecosystem,nullptr,nullptr,PPF_None,nullptr);
    return FMD5::HashAnsiString(*Text);
}
#if WITH_CE_PLACEMENT_LANDFORM
struct FBuildResult {TSharedPtr<const FCEBiomePlacementPlan> Plan;FString Error;};
struct FEntry
{
    TWeakObjectPtr<UCEPlanetEcosystemAsset> Profile;
    FString Key;TSharedPtr<const FCEBiomePlacementPlan> Plan;TFuture<FBuildResult> Work;FString Error;
};
TArray<TUniquePtr<FEntry>> Cache;
FTSTicker::FDelegateHandle Handle;
#if WITH_EDITOR
struct FProfileWatch {TWeakObjectPtr<UCEPlanetEcosystemAsset> Profile;FDelegateHandle Handle;};
TArray<FProfileWatch> Watches;
#endif
FString PlacementKey(const ACLPlanet& Planet,const UCEPlanetEcosystemAsset& Profile)
{return Planet.GetSurfaceGenerationKey()+TEXT("|")+ModelKey(Profile);}
UCEBiomePlacementState* State(AActor& Owner)
{
    auto* S=Owner.FindComponentByClass<UCEBiomePlacementState>();
    if(!S){S=NewObject<UCEBiomePlacementState>(&Owner);Owner.AddInstanceComponent(S);S->RegisterComponent();}return S;
}
bool Tick(float)
{
    if(!GEngine)return true;
    for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return true;
    TMap<ACLPlanet*,TPair<UCEPlanetEcosystemAsset*,FString>> Profiles;
    for(const auto& Context:GEngine->GetWorldContexts())
    {
        auto* W=Context.World();if(Context.WorldType!=EWorldType::Editor||!W||W->bIsTearingDown)continue;
        for(TActorIterator<AActor> It(W);It;++It)
        {
            auto* R=It->FindComponentByClass<UCLPlanetSurfaceRegion>();if(!R||!IsValid(R->Planet)||R->Planet->GetSurfaceGenerationKey().IsEmpty())continue;
            // A default preview profile alone does not make Ecosystems the owner of
            // unrelated generic surface graphs. An explicit link or a prior
            // climate-node publication establishes that consumer binding.
            if(!It->FindComponentByClass<UCEBiomePlacementState>()&&(!R->Planet->RecipeAsset||!R->Planet->RecipeAsset->AuthoringExtensions.Contains(TEXT("CatalystEcosystem"))))continue;
            auto* Pair=Profiles.Find(R->Planet);
            if(!Pair){auto* P=CEPlanetEcosystemProfile(R->Planet);if(!P)continue;Pair=&Profiles.Add(R->Planet,{P,PlacementKey(*R->Planet,*P)});
#if WITH_EDITOR
                if(!Watches.ContainsByPredicate([&](const auto& V){return V.Profile==P;}))
                    Watches.Add({P,P->OnEcosystemChanged.AddStatic([]{Tick(0);})});
#endif
            }
            auto* S=State(**It);const auto& Key=Pair->Value;
            if(S->RequestedKey!=Key)
            {
                S->RequestedKey=Key;S->PendingKey.Reset();
                if(S->GeneratedPlacementKey!=Key)CLInvalidatePlanetSurfaceRegion(R);
            }
            auto* C=It->FindComponentByClass<UPCGComponent>();
            if(C&&!C->IsGenerating()&&!C->IsCleaningUp()&&C->bGenerated&&R->GeneratedSurfaceKey==R->Planet->GetSurfaceGenerationKey()&&S->PendingKey==Key&&S->GeneratedPlacementKey!=Key)
            {S->Modify();S->GeneratedPlacementKey=Key;}
        }
    }
    for(int32 I=Cache.Num()-1;I>=0;--I)if(!Cache[I]->Profile.IsValid())
    {if(Cache[I]->Work.IsValid())Cache[I]->Work.Wait();Cache.RemoveAt(I);}
    return true;
}
#endif
}
UCEPlanetEcosystemAsset* CEPlanetEcosystemProfile(UObject* Object)
{
#if WITH_CE_PLACEMENT_LANDFORM
    const auto* Planet=Cast<ACLPlanet>(Object);if(!Planet||!Planet->RecipeAsset)return nullptr;
    const auto* Link=Planet->RecipeAsset->AuthoringExtensions.Find(TEXT("CatalystEcosystem"));
    return Cast<UCEPlanetEcosystemAsset>((Link?*Link:UCEPlanetEcosystemAsset::ExampleAssetPath()).TryLoad());
#else
    return nullptr;
#endif
}
TSharedPtr<const FCEBiomePlacementPlan> CEGetPlanetBiomePlacement(UObject* Object,UCEPlanetEcosystemAsset* Profile,bool& Pending,FString& Error)
{
    check(IsInGameThread());Pending=false;Error.Reset();
#if WITH_CE_PLACEMENT_LANDFORM
    auto* Planet=Cast<ACLPlanet>(Object);
    if(!IsValid(Planet)||!IsValid(Profile)||!Profile->Ecosystem.Validate(Error)){if(Error.IsEmpty())Error=TEXT("Placement requires an explicit accepted Planet and ecosystem.");return nullptr;}
    const auto Runtime=Planet->GetSamplingRuntime();const auto Plan=Runtime?Runtime->RecipePlan:nullptr;
    if(!Plan||Planet->GetSurfaceGenerationKey().IsEmpty()){Error=TEXT("Waiting for the accepted planet surface.");Pending=true;return nullptr;}
    return CEGetCompiledBiomePlacement(Plan,Profile,Pending,Error);
#else
    Error=TEXT("Planetary biome placement requires the optional Landform integration.");return nullptr;
#endif
}
TSharedPtr<const FCEBiomePlacementPlan> CEGetCompiledBiomePlacement(TSharedPtr<const FCLCompiledPlanetRecipe> Plan,UCEPlanetEcosystemAsset* Profile,bool& Pending,FString& Error)
{
    check(IsInGameThread());Pending=false;Error.Reset();
#if WITH_CE_PLACEMENT_LANDFORM
    if(!Plan||!IsValid(Profile)||!Plan->SourceRecipe||!Profile->Ecosystem.Validate(Error)){if(Error.IsEmpty())Error=TEXT("Placement requires an accepted immutable surface recipe.");return nullptr;}
    FString Source;FCLPlanetRecipe::StaticStruct()->ExportText(Source,Plan->SourceRecipe.Get(),nullptr,nullptr,PPF_None,nullptr);
    const FString Key=FMD5::HashAnsiString(*Source)+TEXT("|")+ModelKey(*Profile);FEntry* Entry=nullptr;
    for(auto& E:Cache)if(E->Key==Key){Entry=E.Get();break;}
    if(!Entry)
    {
        if(Cache.Num()>=32)
        {
            int32 Old=INDEX_NONE;for(int32 I=0;I<Cache.Num();++I)if(!Cache[I]->Work.IsValid()||Cache[I]->Work.IsReady()){Old=I;break;}
            if(Old==INDEX_NONE){Pending=true;return nullptr;}Cache.RemoveAt(Old);
        }
        Entry=Cache.Add_GetRef(MakeUnique<FEntry>()).Get();Entry->Profile=Profile;
    }
    if(Entry->Work.IsValid())
    {
        if(!Entry->Work.IsReady()){Pending=true;return nullptr;}
        auto Result=Entry->Work.Get();Entry->Work=TFuture<FBuildResult>();Entry->Plan=Result.Plan;Entry->Error=Result.Error;
    }
    if(Entry->Key!=Key)
    {
        Entry->Key=Key;Entry->Plan.Reset();Entry->Error.Reset();auto Model=Profile->Ecosystem;
        const double Reference=Plan->RadiusMeters+Plan->BaseHeightMeters;
        const auto* Ocean=Plan->Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});Model.Climate.SeaLevelMeters=Ocean?Ocean->HeightMeters-Plan->BaseHeightMeters:-Reference;
        const int32 Seed=Plan->SourceRecipe?Plan->SourceRecipe->Seed:0;
        Entry->Work=Async(EAsyncExecution::ThreadPool,[Plan,Model,Reference,Seed]
        {
            FBuildResult Result;auto Ground=FCLGroundSurfaceSampler::Make(Plan,Result.Error);if(!Ground)return Result;
            auto Cells=FCEBiomePlacementPlan::MakeLattice();TArray<FString> Errors;Errors.SetNum((Cells.Num()+63)/64);
            ParallelFor(Errors.Num(),[&](int32 Batch)
            {
                const int32 Begin=Batch*64,End=FMath::Min(Begin+64,Cells.Num());TArray<FVector> D;for(int32 I=Begin;I<End;++I)D.Add(Cells[I].PositionMeters);
                TArray<FCLGroundSurfaceSample> Samples;if(!Ground->Sample(D,Samples,Errors[Batch]))return;
                for(int32 I=Begin;I<End;++I){auto& C=Cells[I];const auto& S=Samples[I-Begin];C.bSupported=S.bSupported;C.PositionMeters=S.PositionMeters;C.Normal=S.Normal;}
            });
            for(const auto& E:Errors)if(!E.IsEmpty()){Result.Error=E;return Result;}
            Result.Plan=FCEBiomePlacementPlan::Build(Model,MoveTemp(Cells),Reference,Seed,Result.Error);return Result;
        });
        Pending=true;return nullptr;
    }
    Error=Entry->Error;return Entry->Plan;
#else
    Error=TEXT("Planetary biome placement requires the optional Landform integration.");return nullptr;
#endif
}
bool UCEPlanetBiomePlacementLibrary::EvaluatePlanetSurface(UObject* Planet,UCEPlanetEcosystemAsset* Profile,const TArray<FVector>& Positions,const TArray<FVector>& Normals,TArray<FGuid>& Ids,FString& Error)
{
    Ids.Reset();if(Positions.Num()!=Normals.Num()||Positions.Num()>65536){Error=TEXT("Placement queries require matching batches of at most 65536 ground points.");return false;}
    bool Pending;const auto Plan=CEGetPlanetBiomePlacement(Planet,Profile,Pending,Error);if(!Plan){if(Pending)Error=TEXT("Preparing planetary biome placement.");return false;}
    for(int32 I=0;I<Positions.Num();++I)if(Positions[I].ContainsNaN()||Positions[I].IsNearlyZero()||Normals[I].ContainsNaN()||Normals[I].IsNearlyZero()){Error=TEXT("Placement queries require finite body positions and ground normals.");return false;}
    for(int32 I=0;I<Positions.Num();++I)Ids.Add(Plan->Evaluate(Positions[I],Normals[I]).BiomeId);return true;
}
FString UCEPlanetBiomePlacementLibrary::GetPlacementReport(UObject* Planet,UCEPlanetEcosystemAsset* Profile)
{
    bool Pending;FString Error;const auto Plan=CEGetPlanetBiomePlacement(Planet,Profile,Pending,Error);auto Json=MakeShared<FJsonObject>();Json->SetBoolField(TEXT("pending"),Pending);Json->SetStringField(TEXT("error"),Error);
    if(Plan)
    {
        Json->SetStringField(TEXT("assignmentKey"),Plan->AssignmentKey);Json->SetNumberField(TEXT("profile"),FCEBiomePlacementPlan::Profile);Json->SetNumberField(TEXT("minimumCoverage"),FCEBiomePlacementPlan::MinimumCoverage);Json->SetNumberField(TEXT("cells"),Plan->Cells.Num());Json->SetNumberField(TEXT("supportedSolidAngle"),Plan->SupportedSolidAngle);
#if WITH_CE_PLACEMENT_LANDFORM
        Json->SetStringField(TEXT("placementKey"),PlacementKey(*CastChecked<ACLPlanet>(Planet),*Profile));
#endif
        TArray<TSharedPtr<FJsonValue>> Regions;
        for(const auto& R:Plan->Regions)
        {
            auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("id"),R.BiomeId.ToString(EGuidFormats::Digits));
            const auto* B=Plan->Model.Biomes.FindByPredicate([&](const auto& V){return V.Id==R.BiomeId;});O->SetStringField(TEXT("name"),B?B->Name:TEXT(""));O->SetNumberField(TEXT("reservedFraction"),R.ReservedSolidAngle/Plan->SupportedSolidAngle);O->SetNumberField(TEXT("assignedFraction"),R.AssignedSolidAngle/Plan->SupportedSolidAngle);O->SetNumberField(TEXT("reservedCells"),R.ReservedCells);O->SetNumberField(TEXT("components"),R.Components);O->SetNumberField(TEXT("maximumHabitatPenalty"),R.MaximumHabitatPenalty);Regions.Add(MakeShared<FJsonValueObject>(O));
        }
        Json->SetArrayField(TEXT("regions"),Regions);
    }
    FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(Json,Writer);return Text;
}
void CERecordBiomePlacement(UObject* Object,UObject* PlanetObject,UCEPlanetEcosystemAsset* Profile)
{
#if WITH_CE_PLACEMENT_LANDFORM
    auto* Owner=Cast<AActor>(Object);auto* Planet=Cast<ACLPlanet>(PlanetObject);
    if(Owner&&Planet&&Profile)
    {
        auto* S=State(*Owner);const FString Key=PlacementKey(*Planet,*Profile);
        if(S->RequestedKey.IsEmpty())S->RequestedKey=Key;
        S->PendingKey=Key;
    }
#endif
}
void CEStartBiomePlacement()
{
#if WITH_CE_PLACEMENT_LANDFORM && WITH_EDITOR
    if(!Handle.IsValid())Handle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),.25f);
#endif
}
void CEStopBiomePlacement()
{
#if WITH_CE_PLACEMENT_LANDFORM
    FTSTicker::RemoveTicker(Handle);Handle.Reset();
#if WITH_EDITOR
    for(auto& W:Watches)if(auto* P=W.Profile.Get())P->OnEcosystemChanged.Remove(W.Handle);Watches.Reset();
#endif
    for(auto& E:Cache)if(E->Work.IsValid())E->Work.Wait();Cache.Reset();
#endif
}
