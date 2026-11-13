#include "BiomeCore/CEBiomeCoreBridge.h"
#include "Climate/CEClimatePCGSettings.h"
#include "Model/CEPlanetEcosystem.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
namespace
{
FProperty* Field(UStruct* Type,const TCHAR* Prefix)
{
    FProperty* Found=nullptr;
    for(TFieldIterator<FProperty> P(Type);P;++P)if(P->GetName().StartsWith(Prefix)){if(Found)return nullptr;Found=*P;}
    return Found;
}
FStructProperty* GeneratorStruct(UObject* Object){return Object?FindFProperty<FStructProperty>(Object->GetClass(),TEXT("Generator")):nullptr;}
UObject* DuplicateAsset(UObject* Source,const FString& Folder,const FString& Stem)
{
    auto& Tools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();FString Package,Name;
    Tools.CreateUniqueAssetName(Folder/Stem,TEXT(""),Package,Name);
    return Tools.DuplicateAsset(Name,FPackageName::GetLongPackagePath(Package),Source);
}
}
bool UCEBiomeCoreBridge::Build(UCEPlanetEcosystemAsset* Profile,const FString& Folder,const FTransform& Frame,double Radius,int32 Seed,TArray<UObject*>& Assets,FString& Error)
{
    Assets.Reset();
    if(!Profile){Error=TEXT("Select an ecosystem profile.");return false;}
    if(!Profile->Ecosystem.Validate(Error)||!UCEClimatePCGSettings::ValidateFrame(Frame,Radius,Error))return false;
    if(!Folder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(Folder)){Error=TEXT("Choose a valid /Game destination folder.");return false;}
    auto* SamplerGraph=Cast<UPCGGraph>(Profile->SurfaceSamplerGraph.TryLoad());
    if(!SamplerGraph||SamplerGraph->GetNodes().Num()!=1||SamplerGraph->GetNodes()[0]->GetSettings()->GetClass()->GetPathName()!=TEXT("/Script/VoxelPCG.PCGVoxelSamplerSettings"))
    {Error=TEXT("Choose a graph containing exactly one configured native VPP Voxel Sampler.");return false;}
    auto* Sampler=SamplerGraph->GetNodes()[0]->GetSettings();
    auto* Unbounded=FindFProperty<FBoolProperty>(Sampler->GetClass(),TEXT("bUnbounded"));
    if(!Unbounded||Unbounded->GetPropertyValue_InContainer(Sampler)){Error=TEXT("The VPP sampler must use bounded generation.");return false;}
    auto* Template=LoadObject<UObject>(nullptr,TEXT("/PCGBiomeCore/BiomeGenerators/DefaultGenerator.DefaultGenerator"));
    auto* Generator=GeneratorStruct(Template);
    auto* GraphField=Generator?CastField<FObjectPropertyBase>(Field(Generator->Struct,TEXT("GeneratorGraph_"))):nullptr;
    auto* TypeField=Generator?CastField<FNameProperty>(Field(Generator->Struct,TEXT("GeneratorType_"))):nullptr;
    // Some Biome Core versions store the generator type as String instead of Name.
    auto* TypeString=Generator?CastField<FStrProperty>(Field(Generator->Struct,TEXT("GeneratorType_"))):nullptr;
    if(!Generator||!GraphField||(!TypeField&&!TypeString)){Error=TEXT("Biome Core DefaultGenerator schema is unavailable or incompatible. Enable PCGBiomeCore and restart.");return false;}
    struct FSource {const FCEBiomeDefinition* Biome;UObject* Asset;FArrayProperty* Entries;FObjectPropertyBase* Generator;};TArray<FSource> Sources;
    for(const auto& B:Profile->Ecosystem.Biomes)
    {
        if(!B.bEnabled||B.BiomeCoreAsset.IsNull())continue;
        auto* Source=B.BiomeCoreAsset.TryLoad();auto* Entries=Source?FindFProperty<FArrayProperty>(Source->GetClass(),TEXT("BiomeAssets")):nullptr;
        auto* Entry=Entries?CastField<FStructProperty>(Entries->Inner):nullptr;auto* G=Entry?CastField<FObjectPropertyBase>(Field(Entry->Struct,TEXT("Generator_"))):nullptr;
        if(!G){Error=FString::Printf(TEXT("%s: source must be a Biome Core BiomeAsset with generator entries."),*B.Name);return false;}
        FScriptArrayHelper Values(Entries,Entries->ContainerPtrToValuePtr<void>(Source));if(!Values.Num()){Error=TEXT("A source BiomeAsset has no mesh entries.");return false;}
        Sources.Add({&B,Source,Entries,G});
    }
    if(Sources.IsEmpty()){Error=TEXT("Assign a Biome Core asset to at least one enabled biome.");return false;}
    for(const auto& S:Sources)
    {
        const FString Stem=TEXT("CE_")+S.Biome->Id.ToString(EGuidFormats::Digits);
        auto* Graph=Cast<UPCGGraph>(DuplicateAsset(SamplerGraph,Folder,Stem+TEXT("_Graph")));
        auto* Definition=DuplicateAsset(Template,Folder,Stem+TEXT("_Generator"));auto* Asset=DuplicateAsset(S.Asset,Folder,Stem+TEXT("_Assets"));
        if(!Graph||!Definition||!Asset){Error=TEXT("Asset creation failed; completed assets remain in the destination for review.");return false;}
        Graph->Modify();for(auto N:TArray<TObjectPtr<UPCGNode>>(Graph->GetNodes()))Graph->RemoveNode(N);
        UPCGSettings* CopiedSampler=nullptr;auto* VoxelNode=Graph->AddNodeCopy(Sampler,CopiedSampler);
        UCEClimatePCGSettings* Climate=nullptr;auto* ClimateNode=Graph->AddNodeOfType(Climate);
        Climate->Profile=Profile;Climate->PlanetToWorld=Frame;Climate->ReferenceRadiusMeters=Radius;Climate->PlanetSeed=Seed;Climate->TargetBiome=S.Biome->Id;
        if(!Graph->AddEdge(VoxelNode,TEXT("Out"),ClimateNode,TEXT("In"))||!Graph->AddEdge(ClimateNode,TEXT("Out"),Graph->GetOutputNode(),TEXT("Out")))
        {Error=TEXT("PCG pin contract changed. Generated graph requires review.");return false;}
        void* Gen=Generator->ContainerPtrToValuePtr<void>(Definition);GraphField->SetObjectPropertyValue_InContainer(Gen,Graph);
        const FString Type=TEXT("Catalyst_")+FMD5::HashAnsiString(*Profile->GetPathName())+TEXT("_")+S.Biome->Id.ToString(EGuidFormats::Digits);
        if(TypeField)TypeField->SetPropertyValue_InContainer(Gen,FName(*Type));else TypeString->SetPropertyValue_InContainer(Gen,Type);
        void* Values=S.Entries->ContainerPtrToValuePtr<void>(Asset);FScriptArrayHelper Entries(S.Entries,Values);
        for(int32 I=0;I<Entries.Num();++I)S.Generator->SetObjectPropertyValue_InContainer(Entries.GetRawPtr(I),Definition);
        for(UObject* O:{static_cast<UObject*>(Graph),Definition,Asset}){O->PostEditChange();O->MarkPackageDirty();}
        Assets.Add(Asset);
    }
    Error.Reset();return true;
}
