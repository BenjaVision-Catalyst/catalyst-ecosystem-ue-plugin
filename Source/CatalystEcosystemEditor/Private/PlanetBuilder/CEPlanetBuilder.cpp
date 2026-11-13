#include "PlanetBuilder/CEPlanetBuilder.h"
#if WITH_CE_LANDFORM
#include "Model/CEPlanetEcosystem.h"
#include "PlanetBuilder/CEBiomePreviewState.h"
#include "BiomeCore/CEBiomeCoreBridge.h"
#include "Async/Async.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "UObject/StructOnScope.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "PropertyCustomizationHelpers.h"
#include "PCGGraph.h"
#include "Engine/Selection.h"
namespace
{
const FName EcosystemKey(TEXT("CatalystEcosystem"));
class SCEPlanetBuilder : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCEPlanetBuilder){} SLATE_END_ARGS()
    void Construct(const FArguments&,FCLPlanetBuilderContext InContext)
    {
        Context=MoveTemp(InContext);
        ChildSlot[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==0?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)Subtab=0;})[SNew(STextBlock).Text(FText::FromString(TEXT("Climate")))]]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==1?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)Subtab=1;})[SNew(STextBlock).Text(FText::FromString(TEXT("Biomes")))]]]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Create ecosystem for this planet")))
                .Visibility_Lambda([this]{return Profile.IsValid()?EVisibility::Collapsed:EVisibility::Visible;}).OnClicked_Lambda([this]{CreateProfile();return FReply::Handled();})]
            +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Pages,SWidgetSwitcher).WidgetIndex_Lambda([this]{return Subtab;})
                +SWidgetSwitcher::Slot()[SAssignNew(ClimateBox,SVerticalBox)]
                +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(BiomeBox,SVerticalBox)]]]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(Status);})]];
        SyncProfile();
    }
    virtual ~SCEPlanetBuilder() override
    {if(Dragging)Dragging.Reset();if(Profile.IsValid())Profile->OnEcosystemChanged.RemoveAll(this);if(Stop)Stop->Store(true);if(Work.IsValid())Work.Wait();}
    void SetActive(bool Value)
    {bActive=Value;++Revision;if(bActive)bPreviewDirty=true;else Context.PublishOverlay(nullptr);}
    void Saveables(TArray<UObject*>& Objects)const{if(Profile.IsValid())Objects.AddUnique(Profile.Get());}
    virtual void Tick(const FGeometry& G,double T,float D) override
    {
        SCompoundWidget::Tick(G,T,D);SyncProfile();
        if(bRebuild&&!Dragging){bRebuild=false;Rebuild();}
        const auto Plan=Context.GetPlan();
        if(Plan!=ObservedPlan){ObservedPlan=Plan;++Revision;bPreviewDirty=true;Context.PublishOverlay(nullptr);}
        if(Work.IsValid()&&Work.IsReady())
        {
            auto Result=Work.Get();Work=TFuture<TSharedPtr<const FCLPlanetBuilderOverlay>>();if(bActive&&WorkRevision==Revision&&Result){Context.PublishOverlay(Result);Status=FString::Printf(TEXT("Biome preview: %d surface samples. Eye and Solo affect preview only."),Result->BodyMeters.Num());}
        }
        if(bActive&&bPreviewDirty&&!Work.IsValid()&&Profile.IsValid()&&Plan)StartPreview(Plan);
    }
private:
    FCLPlanetBuilderContext Context;
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile;
    FSoftObjectPath ObservedProfileLink;
    bool bProfileResolved=false;
    TSharedPtr<SVerticalBox> ClimateBox,BiomeBox;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<IStructureDetailsView> ClimateView;
    TArray<TSharedPtr<IStructureDetailsView>> BiomeViews;
    TSharedPtr<FStructOnScope> ClimateDraft;
    TSharedPtr<const FCLCompiledPlanetRecipe> ObservedPlan;
    TFuture<TSharedPtr<const FCLPlanetBuilderOverlay>> Work;
    TSharedPtr<TAtomic<bool>,ESPMode::ThreadSafe> Stop;
    uint64 Revision=0,WorkRevision=0;
    bool bActive=false,bPreviewDirty=true,bRebuild=false;
    int32 Subtab=0;
    FString Status=TEXT("Create a profile to author climate and preview biome placement.");
    FCEBiomePreviewState PreviewState;
    TUniquePtr<FScopedTransaction> Dragging;
    void Changed(){++Revision;bPreviewDirty=true;bRebuild=true;Context.PublishOverlay(nullptr);}
    void SyncProfile()
    {
        auto* Planet=Context.Planet.Get();const FSoftObjectPath* Link=Planet?Planet->AuthoringExtensions.Find(EcosystemKey):nullptr;
        const FSoftObjectPath Wanted=Link?*Link:FSoftObjectPath();
        if(bProfileResolved&&Wanted==ObservedProfileLink)return;
        ObservedProfileLink=Wanted;bProfileResolved=true;
        if(Profile.IsValid())Profile->OnEcosystemChanged.RemoveAll(this);
        Profile.Reset(Link?Cast<UCEPlanetEcosystemAsset>(Link->TryLoad()):nullptr);
        if(Profile.IsValid())Profile->OnEcosystemChanged.AddSP(this,&SCEPlanetBuilder::Changed);
        PreviewState=FCEBiomePreviewState();Changed();
        if(Link&&!Profile.IsValid())Status=TEXT("The linked ecosystem profile could not be loaded. Repair the link or create a replacement profile.");
    }
    void CreateProfile()
    {
        auto* Planet=Context.Planet.Get();if(!Planet||Profile.IsValid())return;
        FString Package,Name;FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateUniqueAssetName(Planet->GetOutermost()->GetName()+TEXT("_Ecosystem"),TEXT(""),Package,Name);
        const FScopedTransaction Tx(NSLOCTEXT("CE","Create","Create planet ecosystem"));Planet->Modify();
        auto* Asset=NewObject<UCEPlanetEcosystemAsset>(CreatePackage(*Package),*Name,RF_Public|RF_Standalone|RF_Transactional);
        Asset->Modify();FAssetRegistryModule::AssetCreated(Asset);Asset->MarkPackageDirty();
        Planet->AuthoringExtensions.Add(EcosystemKey,FSoftObjectPath(Asset));Planet->MarkPackageDirty();SyncProfile();
    }
    void Edit(FGuid Id,FText Label,TFunction<void(FCEBiomeDefinition&)> Action)
    {
        if(!Profile.IsValid())return;
        auto* B=Profile->Ecosystem.Biomes.FindByPredicate([Id](const auto& V){return V.Id==Id;});if(!B)return;
        TUniquePtr<FScopedTransaction> Tx;if(!Dragging)Tx=MakeUnique<FScopedTransaction>(Label);
        Profile->Modify();Action(*B);Profile->PostEditChange();
    }
    TSharedRef<SWidget> Label(const TCHAR* Text){return SNew(STextBlock).Text(FText::FromString(Text));}
    void Rebuild()
    {
        ClimateBox->ClearChildren();BiomeBox->ClearChildren();BiomeViews.Reset();ClimateView.Reset();ClimateDraft.Reset();
        if(!Profile.IsValid())return;
        FDetailsViewArgs Args;Args.bUpdatesFromSelection=false;Args.bHideSelectionTip=true;Args.NameAreaSettings=FDetailsViewArgs::HideNameArea;
        FStructureDetailsViewArgs StructArgs;
        ClimateDraft=MakeShared<FStructOnScope>(FCEClimateSettings::StaticStruct());
        *reinterpret_cast<FCEClimateSettings*>(ClimateDraft->GetStructMemory())=Profile->Ecosystem.Climate;
        ClimateView=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateStructureDetailView(Args,StructArgs,ClimateDraft);
        ClimateView->GetOnFinishedChangingPropertiesDelegate().AddLambda([this](const FPropertyChangedEvent&)
        {if(!Profile.IsValid())return;const FScopedTransaction Tx(NSLOCTEXT("CE","ClimateEdit","Edit planet climate"));Profile->Modify();Profile->Ecosystem.Climate=*reinterpret_cast<const FCEClimateSettings*>(ClimateDraft->GetStructMemory());Profile->PostEditChange();});
        ClimateBox->AddSlot().FillHeight(1)[ClimateView->GetWidget().ToSharedRef()];
        BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Generation · Character · Eye · Reseed · Solo. Expand a biome to edit its habitat and Biome Core asset.")))];
        for(const auto& B:Profile->Ecosystem.Biomes)
        {
            const FGuid Id=B.Id;
            auto Enabled=[this,Id]{const auto* V=Profile.IsValid()?Profile->Ecosystem.Biomes.FindByPredicate([Id](const auto& X){return X.Id==Id;}):nullptr;return V&&V->bEnabled;};
            auto Header=SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(SInlineEditableTextBlock).IsEnabled_Lambda(Enabled).Text(FText::FromString(B.Name))
                    .OnTextCommitted_Lambda([this,Id](const FText& N,ETextCommit::Type){Edit(Id,NSLOCTEXT("CE","Rename","Rename biome"),[&](auto& V){V.Name=N.ToString();});})]
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(B.Description))]
                +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked(B.bEnabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked)
                        .OnCheckStateChanged_Lambda([this,Id](ECheckBoxState V){Edit(Id,NSLOCTEXT("CE","Enable","Enable biome"),[V](auto& X){X.bEnabled=V==ECheckBoxState::Checked;});})[Label(TEXT("Generate"))]]
                    +SHorizontalBox::Slot().FillWidth(1).Padding(5,0)[SNew(SSlider).IsEnabled_Lambda(Enabled).Value(B.Character).ToolTipText(FText::FromString(TEXT("Character widens habitat tolerance")))
                        .OnMouseCaptureBegin_Lambda([this]{Dragging=MakeUnique<FScopedTransaction>(NSLOCTEXT("CE","Character","Change biome character"));})
                        .OnMouseCaptureEnd_Lambda([this]{Dragging.Reset();})
                        .OnValueChanged_Lambda([this,Id](float V){Edit(Id,NSLOCTEXT("CE","Character","Change biome character"),[V](auto& X){X.Character=V;});})]
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsEnabled_Lambda(Enabled).IsChecked_Lambda([this,Id]{return PreviewState.Hidden.Contains(Id)?ECheckBoxState::Unchecked:ECheckBoxState::Checked;})
                        .OnCheckStateChanged_Lambda([this,Id](ECheckBoxState C){PreviewState.SetEye(Id,C==ECheckBoxState::Checked);++Revision;bPreviewDirty=true;})[Label(TEXT("Eye"))]]
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsEnabled_Lambda(Enabled).Text(FText::FromString(TEXT("Reseed")))
                        .OnClicked_Lambda([this,Id]{Edit(Id,NSLOCTEXT("CE","Reseed","Reseed biome"),[](auto& X){X.Seed=int32(uint32(X.Seed)+1U);});return FReply::Handled();})]
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsEnabled_Lambda(Enabled).IsChecked_Lambda([this,Id]{return PreviewState.Solo==Id?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                        .OnCheckStateChanged_Lambda([this,Id](ECheckBoxState C){PreviewState.SetSolo(C==ECheckBoxState::Checked?Id:FGuid());++Revision;bPreviewDirty=true;})[Label(TEXT("Solo"))]]];
            auto Draft=MakeShared<FStructOnScope>(FCEBiomeDefinition::StaticStruct());*reinterpret_cast<FCEBiomeDefinition*>(Draft->GetStructMemory())=B;
            auto Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateStructureDetailView(Args,StructArgs,Draft);
            Details->GetOnFinishedChangingPropertiesDelegate().AddLambda([this,Id,Draft](const FPropertyChangedEvent&){Edit(Id,NSLOCTEXT("CE","Habitat","Edit biome habitat"),[&](auto& X){X=*reinterpret_cast<const FCEBiomeDefinition*>(Draft->GetStructMemory());X.Id=Id;});});BiomeViews.Add(Details);
            BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[Header].BodyContent()[SNew(SBox).IsEnabled_Lambda(Enabled)[Details->GetWidget().ToSharedRef()]]];
        }
        BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Add biome"))).IsEnabled(Profile->Ecosystem.Biomes.Num()<64)
            .OnClicked_Lambda([this]{const FScopedTransaction Tx(NSLOCTEXT("CE","Add","Add biome"));Profile->Modify();auto& B=Profile->Ecosystem.Biomes.AddDefaulted_GetRef();B.Id=FGuid::NewGuid();B.Name=TEXT("New biome");Profile->PostEditChange();return FReply::Handled();})];
        BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).Text(FText::FromString(TEXT("Configured VPP surface sampler graph")))];
        BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(SObjectPropertyEntryBox).AllowedClass(UPCGGraph::StaticClass())
            .ObjectPath_Lambda([this]{return Profile.IsValid()?Profile->SurfaceSamplerGraph.ToString():FString();})
            .OnObjectChanged_Lambda([this](const FAssetData& A){const FScopedTransaction Tx(NSLOCTEXT("CE","Sampler","Set ecosystem surface sampler"));Profile->Modify();Profile->SurfaceSamplerGraph=A.GetSoftObjectPath();Profile->PostEditChange();})];
        BiomeBox->AddSlot().AutoHeight().Padding(4)[SNew(SButton).Text(FText::FromString(TEXT("Build Biome Core assets")))
            .OnClicked_Lambda([this]{TArray<UObject*> Assets;FString Error;auto* P=Context.Planet.Get();auto Plan=Context.GetPlan();
                if(!P||!Plan){Status=TEXT("Wait for an accepted terrain preview.");return FReply::Handled();}
                const FString Folder=FPackageName::GetLongPackagePath(P->GetOutermost()->GetName())+TEXT("/EcosystemGenerated");
                if(UCEBiomeCoreBridge::Build(Profile.Get(),Folder,FTransform::Identity,Plan->RadiusMeters+Plan->BaseHeightMeters,P->Recipe.Seed,Assets,Error))Status=FString::Printf(TEXT("Created %d Biome Core assets. Assign these to the intended Biome Volume. Set the climate node planet transform to match that volume's planet."),Assets.Num());else Status=Error;
                return FReply::Handled();})];
    }
    void StartPreview(TSharedPtr<const FCLCompiledPlanetRecipe> Plan)
    {
        bPreviewDirty=false;FString Error;if(!Profile->Ecosystem.Validate(Error)){Status=Error;Context.PublishOverlay(nullptr);return;}
        const auto Model=Profile->Ecosystem;const auto Visibility=PreviewState;const int32 Seed=Context.Planet.IsValid()?Context.Planet->Recipe.Seed:0;
        WorkRevision=Revision;Status=TEXT("Evaluating climate on the accepted planet surface…");
        Stop=MakeShared<TAtomic<bool>,ESPMode::ThreadSafe>(false);const auto Cancel=Stop;
        Work=Async(EAsyncExecution::ThreadPool,[Model,Plan,Visibility,Seed,Cancel]()->TSharedPtr<const FCLPlanetBuilderOverlay>
        {
            auto Overlay=MakeShared<FCLPlanetBuilderOverlay>();constexpr int32 N=2048;TArray<FVector> Directions;Directions.Reserve(N*3);
            for(int32 I=0;I<N;++I)
            {
                const double Z=1-2*(I+.5)/N,A=I*2.399963229728653;const FVector D(FMath::Sqrt(1-Z*Z)*FMath::Cos(A),FMath::Sqrt(1-Z*Z)*FMath::Sin(A),Z);
                const FVector T=FVector::CrossProduct(D,FMath::Abs(Z)<.9?FVector::ZAxisVector:FVector::XAxisVector).GetSafeNormal();const FVector B=FVector::CrossProduct(D,T);
                Directions.Add(D);Directions.Add((D+T*.001).GetSafeNormal());Directions.Add((D+B*.001).GetSafeNormal());
            }
            TArray<double> Heights;Heights.Reserve(Directions.Num());
            for(int32 Start=0;Start<Directions.Num();Start+=256){if(Cancel->Load())return nullptr;TArray<double> Batch;Plan->SampleHeightsMeters(MakeArrayView(Directions).Slice(Start,FMath::Min(256,Directions.Num()-Start)),Batch);Heights.Append(Batch);}
            for(int32 I=0;I<N;++I)
            {
                const int32 J=I*3;const FVector P=Directions[J]*(Plan->RadiusMeters+Heights[J]);const FVector X=Directions[J+1]*(Plan->RadiusMeters+Heights[J+1]),Y=Directions[J+2]*(Plan->RadiusMeters+Heights[J+2]);
                const auto S=FCEClimateEvaluator::Evaluate(Model,P,FVector::CrossProduct(X-P,Y-P).GetSafeNormal(),Plan->RadiusMeters+Plan->BaseHeightMeters,Seed);
                if(!Visibility.Visible(S.BiomeId))continue;
                const auto* Biome=Model.Biomes.FindByPredicate([&](const auto& B){return B.Id==S.BiomeId;});if(!Biome)continue;
                Overlay->BodyMeters.Add(P);Overlay->Colors.Add(Biome->Color);
            }
            return Overlay;
        });
    }
};
class FCEPlanetBuilderSession : public ICLPlanetBuilderExtension
{
    TSharedRef<SCEPlanetBuilder> Panel;
public:
    explicit FCEPlanetBuilderSession(FCLPlanetBuilderContext Context):Panel(SNew(SCEPlanetBuilder,MoveTemp(Context))){}
    virtual TSharedRef<SWidget> Widget() override{return Panel;}
    virtual void SetActive(bool Active) override{Panel->SetActive(Active);}
    virtual void GetSaveableObjects(TArray<UObject*>& Objects)const override{Panel->Saveables(Objects);}
};
}
TSharedRef<ICLPlanetBuilderExtension> CECreatePlanetBuilder(FCLPlanetBuilderContext Context){return MakeShared<FCEPlanetBuilderSession>(MoveTemp(Context));}
#endif
