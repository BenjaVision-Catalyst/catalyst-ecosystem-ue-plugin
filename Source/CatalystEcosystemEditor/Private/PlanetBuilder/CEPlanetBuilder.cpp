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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
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
double CEPlanetSeaLevelMeters(const FCLCompiledPlanetRecipe& Plan)
{
    const auto* Ocean=Plan.Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});
    // No ocean guide means a dry planet, never a second independently authored sea.
    return Ocean?Ocean->HeightMeters-Plan.BaseHeightMeters:-(Plan.RadiusMeters+Plan.BaseHeightMeters);
}
namespace
{
class FCEClimateDetails : public IDetailCustomization
{
    TFunction<FText()> OceanText;
public:
    explicit FCEClimateDetails(TFunction<FText()> Text):OceanText(MoveTemp(Text)){}
    void CustomizeDetails(IDetailLayoutBuilder& Builder) override
    {
        Builder.EditCategory(TEXT("Climate")).SetSortOrder(0);
        Builder.EditCategory(TEXT("Variation")).SetSortOrder(2);
        auto& Water=Builder.EditCategory(TEXT("Water"));Water.SetSortOrder(1);
        Water.AddCustomRow(FText::FromString(TEXT("Sea level OceanHeight")))
            .NameContent()[SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont")).Text(FText::FromString(TEXT("Sea level")))]
            .ValueContent()[SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont")).Text_Lambda([Text=OceanText]{return Text();})
                .ToolTipText(FText::FromString(TEXT("Uses the planet's OceanHeight radial guide. Edit that guide in Planet.")))];
    }
};
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
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[Label(TEXT("Ecosystem"))]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SObjectPropertyEntryBox)
                    .AllowedClass(UCEPlanetEcosystemAsset::StaticClass()).AllowClear(false).AllowCreate(true)
                    .ObjectPath_Lambda([this]{return Profile.IsValid()?Profile->GetPathName():ObservedProfileLink.ToString();})
                    .OnObjectChanged_Lambda([this](const FAssetData& Asset){SelectProfile(Cast<UCEPlanetEcosystemAsset>(Asset.GetAsset()));})]]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==0?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)Subtab=0;})[Label(TEXT("Climate"))]]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==1?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)Subtab=1;})[Label(TEXT("Biomes"))]]]
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
    FString Status;
    FCEBiomePreviewState PreviewState;
    TSet<FGuid> Expanded;
    TUniquePtr<FScopedTransaction> Dragging;
    void Changed(){++Revision;bPreviewDirty=true;bRebuild=true;Context.PublishOverlay(nullptr);}
    void SyncProfile()
    {
        auto* Planet=Context.Planet.Get();const FSoftObjectPath* Link=Planet?Planet->AuthoringExtensions.Find(EcosystemKey):nullptr;
        const FSoftObjectPath Wanted=Link?*Link:UCEPlanetEcosystemAsset::ExampleAssetPath();
        if(bProfileResolved&&Wanted==ObservedProfileLink)return;
        ObservedProfileLink=Wanted;bProfileResolved=true;
        if(Profile.IsValid())Profile->OnEcosystemChanged.RemoveAll(this);
        Profile.Reset(Cast<UCEPlanetEcosystemAsset>(Wanted.TryLoad()));
        if(Profile.IsValid())Profile->OnEcosystemChanged.AddSP(this,&SCEPlanetBuilder::Changed);
        PreviewState=FCEBiomePreviewState();Changed();
        if(!Profile.IsValid())Status=TEXT("The ecosystem could not be loaded. Select an existing ecosystem or create one from the asset picker.");
    }
    void SelectProfile(UCEPlanetEcosystemAsset* Asset)
    {
        auto* Planet=Context.Planet.Get();if(!Planet||!Asset)return;
        const FSoftObjectPath Link(Asset);
        if(const auto* Existing=Planet->AuthoringExtensions.Find(EcosystemKey);Existing&&*Existing==Link)return;
        const FScopedTransaction Tx(NSLOCTEXT("CE","Select","Select planet ecosystem"));
        Planet->Modify();Planet->AuthoringExtensions.Add(EcosystemKey,Link);Planet->MarkPackageDirty();SyncProfile();
    }
    void Edit(FGuid Id,FText Label,TFunction<void(FCEBiomeDefinition&)> Action)
    {
        if(!Profile.IsValid())return;
        auto* B=Profile->Ecosystem.Biomes.FindByPredicate([Id](const auto& V){return V.Id==Id;});if(!B)return;
        TUniquePtr<FScopedTransaction> Tx;if(!Dragging)Tx=MakeUnique<FScopedTransaction>(Label);
        Profile->Modify();Action(*B);Profile->PostEditChange();
    }
    TSharedRef<SWidget> Label(const TCHAR* Text){return SNew(STextBlock).Text(FText::FromString(Text)).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));}
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
        const auto View=ClimateView->GetDetailsView();
        View->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P){return P.Property.GetFName()!=GET_MEMBER_NAME_CHECKED(FCEClimateSettings,SeaLevelMeters);}));
        View->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateLambda([this]
        {
            return MakeShared<FCEClimateDetails>([this]
            {
                const auto Plan=Context.GetPlan();if(!Plan)return FText::FromString(TEXT("Waiting for planet preview"));
                const auto* Ocean=Plan->Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});
                return FText::FromString(Ocean?FString::Printf(TEXT("OceanHeight · %.2f m"),Plan->RadiusMeters+Ocean->HeightMeters):TEXT("No OceanHeight guide · dry planet"));
            });
        }));
        View->ForceRefresh();
        ClimateBox->AddSlot().FillHeight(1)[ClimateView->GetWidget().ToSharedRef()];
        for(const auto& B:Profile->Ecosystem.Biomes)
        {
            const FGuid Id=B.Id;
            auto Entry=[this,Id]()->const FCEBiomeDefinition*{return Profile.IsValid()?Profile->Ecosystem.Biomes.FindByPredicate([Id](const auto& X){return X.Id==Id;}):nullptr;};
            auto Enabled=[Entry]{const auto* V=Entry();return V&&V->bEnabled;};
            FString Description=B.Description;int32 End;if(Description.FindChar('.',End))Description=Description.Left(End+1);if(Description.Len()>95)Description=Description.Left(92)+TEXT("…");
            const auto DetailsBox=SNew(SBox).MaxDesiredHeight(420).IsEnabled_Lambda(Enabled)
                .Visibility_Lambda([this,Id]{return Expanded.Contains(Id)?EVisibility::Visible:EVisibility::Collapsed;});
            BiomeBox->AddSlot().AutoHeight().Padding(4,1)[SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.055,.055,.055)).Padding(6,7)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SNew(SButton).IsEnabled_Lambda(Enabled)
                        .ButtonStyle(FAppStyle::Get(),"NoBorder").ContentPadding(0).ToolTipText(FText::FromString(TEXT("Edit underlying values")))
                        .OnClicked_Lambda([this,Id]{if(!Expanded.Remove(Id))Expanded.Add(Id);return FReply::Handled();})
                        [SNew(SImage).DesiredSizeOverride(FVector2D(10,10)).Image_Lambda([this,Id]{return FAppStyle::GetBrush(Expanded.Contains(Id)?TEXT("TreeArrow_Expanded"):TEXT("TreeArrow_Collapsed"));})]]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SCheckBox)
                        .ToolTipText(FText::FromString(TEXT("Include in planet generation")))
                        .IsChecked_Lambda([Enabled]{return Enabled()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                        .OnCheckStateChanged_Lambda([this,Id](ECheckBoxState V){Edit(Id,NSLOCTEXT("CE","Enable","Enable biome"),[V](auto& X){X.bEnabled=V==ECheckBoxState::Checked;});})]
                    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(B.Name)).Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))]
                        +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Description))
                            .ToolTipText(FText::FromString(B.Description)).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(.65,.65,.65))]]
                    +SHorizontalBox::Slot().FillWidth(.55).VAlign(VAlign_Center)[SNew(SSlider).IsEnabled_Lambda(Enabled)
                        .Value_Lambda([Entry]{const auto* V=Entry();return V?float(V->Character):0.f;}).ToolTipText(FText::FromString(TEXT("Character. Expand the row to edit habitat values.")))
                        .OnMouseCaptureBegin_Lambda([this]{Dragging=MakeUnique<FScopedTransaction>(NSLOCTEXT("CE","Character","Change biome character"));})
                        .OnMouseCaptureEnd_Lambda([this]{Dragging.Reset();})
                        .OnValueChanged_Lambda([this,Id](float V){Edit(Id,NSLOCTEXT("CE","Character","Change biome character"),[V](auto& X){X.Character=V;});})]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0)[SNew(SBox).WidthOverride(30)[SNew(STextBlock)
                        .IsEnabled_Lambda(Enabled).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                        .Text_Lambda([Entry]{const auto* V=Entry();FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(1);return FText::AsNumber(V?V->Character*100:0,&Format);})]]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).IsEnabled_Lambda(Enabled).ButtonStyle(FAppStyle::Get(),"SimpleButton")
                        .ContentPadding(3).ToolTipText(FText::FromString(TEXT("Include in preview")))
                        .OnClicked_Lambda([this,Id]{const bool Visible=PreviewState.Visible(Id);if(PreviewState.Solo==Id)PreviewState.SetSolo(FGuid());PreviewState.SetEye(Id,!Visible);++Revision;bPreviewDirty=true;return FReply::Handled();})
                        [SNew(SImage).DesiredSizeOverride(FVector2D(16,16)).Image_Lambda([this,Id]{return FAppStyle::GetBrush(PreviewState.Visible(Id)?TEXT("Level.VisibleIcon16x"):TEXT("Level.NotVisibleIcon16x"));})]]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).IsEnabled_Lambda(Enabled).ButtonStyle(FAppStyle::Get(),"SimpleButton")
                        .ContentPadding(3).ToolTipText(FText::FromString(TEXT("Reseed")))
                        .OnClicked_Lambda([this,Id]{Edit(Id,NSLOCTEXT("CE","Reseed","Reseed biome"),[](auto& X){X.Seed=int32(uint32(X.Seed)+1U);});return FReply::Handled();})
                        [SNew(SImage).DesiredSizeOverride(FVector2D(16,16)).Image(FAppStyle::GetBrush("Icons.Refresh"))]]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0,0,0)[SNew(SBox).WidthOverride(50)[SNew(SButton).IsEnabled_Lambda(Enabled)
                        .ContentPadding(FMargin(3,2)).ToolTipText(FText::FromString(TEXT("Preview this biome across the whole planet")))
                        .OnClicked_Lambda([this,Id]{PreviewState.SetSolo(PreviewState.Solo==Id?FGuid():Id);++Revision;bPreviewDirty=true;return FReply::Handled();})
                        [SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                            .Text_Lambda([this,Id]{return FText::FromString(PreviewState.Solo==Id?TEXT("Unsolo"):TEXT("Solo"));})]]]]
                +SVerticalBox::Slot().AutoHeight().Padding(16,8,0,0)[DetailsBox]]];
            auto Draft=MakeShared<FStructOnScope>(FCEBiomeDefinition::StaticStruct());*reinterpret_cast<FCEBiomeDefinition*>(Draft->GetStructMemory())=B;
            auto Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateStructureDetailView(Args,StructArgs,Draft);
            Details->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
                {const FName N=P.Property.GetFName();return N!=TEXT("Id")&&N!=TEXT("bEnabled")&&N!=TEXT("Character");}));
            Details->GetDetailsView()->ForceRefresh();
            Details->GetOnFinishedChangingPropertiesDelegate().AddLambda([this,Id,Draft](const FPropertyChangedEvent&){Edit(Id,NSLOCTEXT("CE","Habitat","Edit biome habitat"),[&](auto& X){X=*reinterpret_cast<const FCEBiomeDefinition*>(Draft->GetStructMemory());X.Id=Id;});});
            BiomeViews.Add(Details);DetailsBox->SetContent(Details->GetWidget().ToSharedRef());
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
                if(UCEBiomeCoreBridge::Build(Profile.Get(),Folder,FTransform::Identity,Plan->RadiusMeters+Plan->BaseHeightMeters,P->Recipe.Seed,Assets,Error,CEPlanetSeaLevelMeters(*Plan),true))Status=FString::Printf(TEXT("Created %d Biome Core assets. Assign these to the intended Biome Volume. Set the climate node planet transform to match that volume's planet."),Assets.Num());else Status=Error;
                return FReply::Handled();})];
    }
    void StartPreview(TSharedPtr<const FCLCompiledPlanetRecipe> Plan)
    {
        bPreviewDirty=false;FString Error;if(!Profile->Ecosystem.Validate(Error)){Status=Error;Context.PublishOverlay(nullptr);return;}
        auto Model=Profile->Ecosystem;Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(*Plan);const auto Visibility=PreviewState;const int32 Seed=Context.Planet.IsValid()?Context.Planet->Recipe.Seed:0;
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

#if WITH_CE_LANDFORM && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEOceanGuideTest,"Catalyst.Ecosystem.Editor.OceanHeightDatum",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEOceanGuideTest::RunTest(const FString&)
{
    FCLCompiledPlanetRecipe Plan;Plan.RadiusMeters=1000;Plan.BaseHeightMeters=-250;
    FCLHeightGuide Ocean;Ocean.Name=TEXT("OceanHeight");Ocean.HeightMeters=-400;Plan.Guides.Add(Ocean);
    auto Model=FCEPlanetEcosystem::MakeDefault();Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(Plan);
    TestEqual(TEXT("OceanHeight converts from outer-radius offset to neutral-surface offset"),Model.Climate.SeaLevelMeters,-150.);
    for(FVector Up:{FVector::XAxisVector,FVector::ZAxisVector,-FVector::ZAxisVector})
    {
        TestTrue(TEXT("Below the OceanHeight guide is water at equator and poles"),FCEClimateEvaluator::Evaluate(Model,Up*590,Up,750,1).bWater);
        TestFalse(TEXT("Above the OceanHeight guide is land"),FCEClimateEvaluator::Evaluate(Model,Up*610,Up,750,1).bWater);
    }
    Plan.Guides[0].HeightMeters=-300;Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(Plan);
    TestTrue(TEXT("Moving OceanHeight moves the classification boundary"),FCEClimateEvaluator::Evaluate(Model,FVector(610,0,0),FVector::XAxisVector,750,1).bWater);
    Plan.Guides.Reset();Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(Plan);
    TestFalse(TEXT("A planet with no ocean guide is dry"),FCEClimateEvaluator::Evaluate(Model,FVector(590,0,0),FVector::XAxisVector,750,1).bWater);
    return true;
}
#endif
