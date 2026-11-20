#include "PlanetBuilder/CEPlanetBuilder.h"
#if WITH_CE_LANDFORM
#include "Model/CEPlanetEcosystem.h"
#include "Climate/CEPlanetBiomePlacement.h"
#include "PlanetBuilder/CEBiomePreviewState.h"
#include "Weather/CEPlanetWeather.h"
#include "Weather/CEWeatherGlobe.h"
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
#include "AssetRegistry/AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "PropertyCustomizationHelpers.h"
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
struct FCEWeatherView
{
    TSharedPtr<FCEPlanetWeather,ESPMode::ThreadSafe> Field;
    TArray<FVector> Positions;
    TSharedPtr<const FCLPlanetBuilderOverlay> Overlay;
    TArray<FCEWeatherSample> Samples;
    TArray<FCEWeatherFront> Fronts;
    FString Error;
    double Seconds=0;
    int64 ValidTime=0;
};
class SCEPlanetBuilder : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCEPlanetBuilder){} SLATE_END_ARGS()
    void Construct(const FArguments&,FCLPlanetBuilderContext InContext)
    {
        Context=MoveTemp(InContext);WeatherMesh=MakeShared<FCEWeatherGlobeMesh,ESPMode::ThreadSafe>(FCEWeatherGlobeMesh::Make());
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
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)SelectSubtab(0);})[Label(TEXT("Climate"))]]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==1?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)SelectSubtab(1);})[Label(TEXT("Biomes"))]]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
                    .IsChecked_Lambda([this]{return Subtab==2?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState C){if(C==ECheckBoxState::Checked)SelectSubtab(2);})[Label(TEXT("Weather"))]]]
            +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Pages,SWidgetSwitcher).WidgetIndex_Lambda([this]{return Subtab;})
                +SWidgetSwitcher::Slot()[SAssignNew(ClimateBox,SVerticalBox)]
                +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(BiomeBox,SVerticalBox)]]
                +SWidgetSwitcher::Slot()[WeatherPage()]]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Visibility_Lambda([this]{return Subtab==2||Status.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;}).AutoWrapText(true).Text_Lambda([this]{return FText::FromString(Status);})]];
        SyncProfile();
    }
    virtual ~SCEPlanetBuilder() override
    {if(Dragging)Dragging.Reset();if(Profile.IsValid())Profile->OnEcosystemChanged.RemoveAll(this);if(Stop)Stop->Store(true);if(Work.IsValid())Work.Wait();if(WeatherWork.IsValid())WeatherWork.Wait();}
    void SetActive(bool Value)
    {bActive=Value;++Revision;if(bActive)bPreviewDirty=true;else Context.PublishOverlay(nullptr);}
    void Saveables(TArray<UObject*>& Objects)const{if(Profile.IsValid())Objects.AddUnique(Profile.Get());}
    virtual void Tick(const FGeometry& G,double T,float D) override
    {
        SCompoundWidget::Tick(G,T,D);SyncProfile();
        if(bRebuild&&!Dragging){bRebuild=false;Rebuild();}
        const auto Plan=Context.GetPlan();
        if(Plan!=ObservedPlan)
        {
            const bool PresentationOnly=Plan&&ObservedPlan&&WeatherField&&CEWeatherTerrainInputsMatch(*ObservedPlan,*Plan);
            ObservedPlan=Plan;++Revision;bPreviewDirty=true;if(PresentationOnly)bWeatherDirty=true;else ResetWeather();Context.PublishOverlay(nullptr);
        }
        if(Work.IsValid()&&Work.IsReady())
        {
            auto Result=Work.Get();Work=TFuture<TSharedPtr<const FCLPlanetBuilderOverlay>>();if(bActive&&Subtab!=2&&WorkRevision==Revision&&Result){Context.PublishOverlay(Result);Status.Reset();}
        }
        if(WeatherWork.IsValid()&&WeatherWork.IsReady())
        {
            auto Result=WeatherWork.Get();WeatherWork=TFuture<FCEWeatherView>();
            if(WeatherRevision==Revision&&Result.Error.IsEmpty()&&Result.Field.IsValid()&&Result.Overlay.IsValid())
            {
                WeatherField=Result.Field;WeatherPositions=MoveTemp(Result.Positions);WeatherCost=Result.Seconds;
                if(bActive&&Subtab==2&&WeatherVisible()){WeatherSamples=MoveTemp(Result.Samples);RenderedWeatherTime=Result.ValidTime;WeatherBaseOverlay=Result.Overlay;WeatherFrontPresentation.SetFronts(MoveTemp(Result.Fronts));bFrontPresentationDirty=true;WeatherError.Reset();}
            }
            else if(WeatherRevision==Revision&&!Result.Error.IsEmpty())WeatherError=Result.Error;
        }
        if(bActive&&Subtab==2&&Profile.IsValid()&&!Profile->Ecosystem.Weather.bEnabled)WeatherError.Reset();
        if(bActive&&Subtab==2&&WeatherField)
        {
            WeatherSeconds+=FMath::Max(0.f,D)*60.;
            const int64 Target=FMath::FloorToInt64(WeatherSeconds);
            if(Target>WeatherClock.AcceptedSeconds()&&WeatherClock.AdvanceTo(Target))bWeatherDirty=true;
        }
        bFrontPresentationDirty|=WeatherFrontPresentation.IsAnimating();WeatherFrontPresentation.Tick(D);
        if(bActive&&Subtab==2&&WeatherVisible()&&WeatherBaseOverlay&&Plan&&(bFrontPresentationDirty||WeatherFrontPresentation.IsAnimating())&&FPlatformTime::Seconds()>=NextFrontViewAt)
        {
            auto O=MakeShared<FCLPlanetBuilderOverlay>(*WeatherBaseOverlay);
            WeatherFrontPresentation.Append(*WeatherMesh,CLPlanetOverlayRadiusMeters(*Plan),*O);Context.PublishOverlay(O);
            NextFrontViewAt=FPlatformTime::Seconds()+.05;bFrontPresentationDirty=false;
        }
        if(bActive&&Subtab==2&&WeatherVisible()&&bWeatherDirty&&!WeatherWork.IsValid()&&!Work.IsValid()&&Profile.IsValid()&&Plan)StartWeather(Plan);
        if(bActive&&Subtab!=2&&bPreviewDirty&&!Dragging&&!Work.IsValid()&&!WeatherWork.IsValid()&&Profile.IsValid()&&Plan)StartPreview(Plan);
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
    TSharedPtr<SVerticalBox> WeatherDetailsBox;
    TSharedPtr<IStructureDetailsView> WeatherView;
    TSharedPtr<FStructOnScope> WeatherDraft;
    TFuture<FCEWeatherView> WeatherWork;
    TSharedPtr<FCEPlanetWeather,ESPMode::ThreadSafe> WeatherField;
    TArray<FVector> WeatherPositions;
    TSharedPtr<const FCEWeatherGlobeMesh,ESPMode::ThreadSafe> WeatherMesh;
    double NextWeatherViewAt=0,NextFrontViewAt=0;
    bool bFrontPresentationDirty=false;
    FCEWeatherFrontPresentation WeatherFrontPresentation;
    TSharedPtr<const FCLPlanetBuilderOverlay> WeatherBaseOverlay;
    TArray<FCEWeatherSample> WeatherSamples;
    FCSFieldRuntime WeatherClock{3600};
    FString WeatherError;
    uint64 WeatherRevision=0;
    int64 RenderedWeatherTime=3600;
    double WeatherSeconds=3600,WeatherCost=0;
    bool bWeatherDirty=true,bWeatherEye=true,bWeatherSolo=false,bWeatherExpanded=false;
    bool WeatherVisible()const{return Profile.IsValid()&&Profile->Ecosystem.Weather.bEnabled&&(bWeatherEye||bWeatherSolo);}
    void RefreshWeatherEye(){Context.PublishOverlay(nullptr);bWeatherDirty=true;}
    FCEBiomePreviewState PreviewState;
    TSet<FGuid> Expanded;
    TUniquePtr<FScopedTransaction> Dragging;
    void SelectSubtab(int32 Value)
    {
        if(Subtab==Value)return;
        Subtab=Value;++Revision;bPreviewDirty=true;bWeatherDirty=true;
        if(Stop)Stop->Store(true);
        Context.PublishOverlay(nullptr);
    }
    TSharedRef<SWidget> WeatherPage()
    {
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true)
                .Visibility_Lambda([this]{return WeatherError.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})
                .Text_Lambda([this]{return FText::FromString(WeatherError);})]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SAssignNew(WeatherDetailsBox,SVerticalBox)];
    }
    void ResetWeather()
    {
        WeatherBaseOverlay.Reset();WeatherFrontPresentation.Entries.Reset();WeatherField.Reset();WeatherPositions.Reset();WeatherSamples.Reset();WeatherClock=FCSFieldRuntime(3600);WeatherSeconds=3600;NextWeatherViewAt=0;bWeatherDirty=true;WeatherError.Reset();
    }
    void StartWeather(TSharedPtr<const FCLCompiledPlanetRecipe> Plan)
    {
        if(FPlatformTime::Seconds()<NextWeatherViewAt)return;
        FString Error;bool Pending=false;TSharedPtr<const FCEBiomePlacementPlan> Placement;
        if(!WeatherField)
        {
            Placement=CEGetCompiledBiomePlacement(Plan,Profile.Get(),Pending,Error);
            if(!Placement){WeatherError=Pending?FString():Error;return;}
        }
        bWeatherDirty=false;WeatherRevision=Revision;NextWeatherViewAt=FPlatformTime::Seconds()+.2;
        auto Field=WeatherField;auto Positions=WeatherPositions;auto Model=Profile->Ecosystem;Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(*Plan);Model.Weather.DaySeconds=Plan->SourceRecipe->RotationPeriodSeconds;
        const auto Time=WeatherClock.AcceptedSeconds();const int32 Seed=Context.Planet.IsValid()?Context.Planet->Recipe.Seed:0;
        const auto Mesh=WeatherMesh;
        auto Cached=RenderedWeatherTime==Time&&Field?WeatherSamples:TArray<FCEWeatherSample>();
        Stop=MakeShared<TAtomic<bool>,ESPMode::ThreadSafe>(false);auto Cancel=Stop;
        WeatherWork=Async(EAsyncExecution::ThreadPool,[Field,Positions=MoveTemp(Positions),Cached=MoveTemp(Cached),Model,Placement,Plan,Time,Seed,Cancel,Mesh]()mutable
        {
            FCEWeatherView R;R.ValidTime=Time;const double Start=FPlatformTime::Seconds();
            if(!Field)
            {
                Field=MakeShared<FCEPlanetWeather,ESPMode::ThreadSafe>();TArray<double> Heights;Plan->SampleHeightsMeters(Mesh->Directions,Heights);
                TArray<FVector> Normals;Normals.SetNumZeroed(Mesh->Directions.Num());TArray<FGuid> Biomes;Positions.Reserve(Mesh->Directions.Num());
                for(int32 I=0;I<Mesh->Directions.Num();++I)Positions.Add(Mesh->Directions[I]*(Plan->RadiusMeters+Heights[I]));
                for(int32 I=0;I<Mesh->Indices.Num();I+=3)
                {
                    const int32 A=Mesh->Indices[I],B=Mesh->Indices[I+1],C=Mesh->Indices[I+2];const FVector N=FVector::CrossProduct(Positions[B]-Positions[A],Positions[C]-Positions[A]);Normals[A]+=N;Normals[B]+=N;Normals[C]+=N;
                }
                TArray<FVector> CellDirections;for(const auto& C:Placement->Cells)CellDirections.Add(C.PositionMeters.GetSafeNormal());
                for(int32 I=0;I<Positions.Num();++I)
                {
                    if(Cancel->Load())return R;Normals[I]=Normals[I].GetSafeNormal();if(FVector::DotProduct(Normals[I],Mesh->Directions[I])<0)Normals[I]*=-1;
                    double Best=-2;FGuid Id;for(int32 J=0;J<CellDirections.Num();++J)if(Placement->Cells[J].bSupported){const double Dot=FVector::DotProduct(CellDirections[J],Mesh->Directions[I]);if(Dot>Best){Best=Dot;Id=Placement->Cells[J].BiomeId;}}Biomes.Add(Id);
                }
                if(Cancel->Load()||!Field->Initialize(Model,Positions,Normals,Plan->RadiusMeters+Plan->BaseHeightMeters,Seed,R.Error,Biomes))return R;
            }
            if(Cancel->Load())return R;
            if(Cached.Num()==Mesh->Directions.Num())R.Samples=MoveTemp(Cached);else if(!Field->Evaluate(Time,R.Samples,R.Error))return R;
            auto Overlay=ConstCastSharedPtr<FCLPlanetBuilderOverlay>(CEBuildWeatherGlobe(*Mesh,R.Samples,CLPlanetOverlayRadiusMeters(*Plan),Model.Weather.ReferencePressureHpa,3,true,true,true,false,Positions));
            R.Fronts=CEDiagnoseWeatherFronts(*Mesh,R.Samples);Overlay->SunDirection=CEWeatherSunDirection(Model.Climate.PoleAxis,Model.Weather.DaySeconds,Time);R.Overlay=Overlay;
            R.Field=Field;R.Positions=MoveTemp(Positions);R.Seconds=FPlatformTime::Seconds()-Start;return R;
        });
    }
    void Changed(){++Revision;bPreviewDirty=true;bRebuild=true;ResetWeather();if(Stop)Stop->Store(true);Context.PublishOverlay(nullptr);}
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
    void EditWeather(FText Label,TFunction<void(FCEWeatherSettings&)> Action)
    {
        if(!Profile.IsValid())return;
        TUniquePtr<FScopedTransaction> Tx;if(!Dragging)Tx=MakeUnique<FScopedTransaction>(Label);
        Profile->SetFlags(RF_Transactional);Profile->Modify();Action(Profile->Ecosystem.Weather);Profile->PostEditChange();
    }
    TSharedRef<SWidget> WeatherRow(TSharedRef<SWidget> Details)
    {
        auto Enabled=[this]{return Profile.IsValid()&&Profile->Ecosystem.Weather.bEnabled;};
        const auto Advanced=SNew(SBox).MinDesiredHeight(280).MaxDesiredHeight(420).IsEnabled_Lambda(Enabled)
            .Visibility_Lambda([this]{return bWeatherExpanded?EVisibility::Visible:EVisibility::Collapsed;})[Details];
        return SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.055,.055,.055)).Padding(6,7)
            [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SNew(SButton).IsEnabled_Lambda(Enabled)
                    .ButtonStyle(FAppStyle::Get(),"NoBorder").ContentPadding(0).ToolTipText(FText::FromString(TEXT("Edit underlying weather values")))
                    .OnClicked_Lambda([this]{bWeatherExpanded=!bWeatherExpanded;return FReply::Handled();})
                    [SNew(SImage).DesiredSizeOverride(FVector2D(10,10)).Image_Lambda([this]{return FAppStyle::GetBrush(bWeatherExpanded?TEXT("TreeArrow_Expanded"):TEXT("TreeArrow_Collapsed"));})]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SCheckBox)
                    .ToolTipText(FText::FromString(TEXT("Include weather in planet generation")))
                    .IsChecked_Lambda([Enabled]{return Enabled()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState V){EditWeather(NSLOCTEXT("CE","WeatherEnable","Enable weather"),[V](auto& W){W.bEnabled=V==ECheckBoxState::Checked;});})]
                +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).IsEnabled_Lambda(Enabled).Text(FText::FromString(TEXT("Weather"))).Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("Calm days to volatile fronts, strong winds and extreme storms.")))
                        .Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(FLinearColor(.65,.65,.65))]]
                +SHorizontalBox::Slot().FillWidth(.55).VAlign(VAlign_Center)[SNew(SSlider).IsEnabled_Lambda(Enabled).StepSize(.01)
                    .Value_Lambda([this]{return Profile.IsValid()?float(Profile->Ecosystem.Weather.Character):.5f;})
                    .ToolTipText(FText::FromString(TEXT("Weather character: Calm to Volatile. Higher values bring more frequent and stronger extremes. Expand for underlying values.")))
                    .OnMouseCaptureBegin_Lambda([this]{Dragging=MakeUnique<FScopedTransaction>(NSLOCTEXT("CE","WeatherCharacter","Change weather character"));})
                    .OnMouseCaptureEnd_Lambda([this]{Dragging.Reset();})
                    .OnValueChanged_Lambda([this](float V){EditWeather(NSLOCTEXT("CE","WeatherCharacter","Change weather character"),[V](auto& W){W.Character=V;});})]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0)[SNew(SBox).WidthOverride(30)[SNew(STextBlock).IsEnabled_Lambda(Enabled)
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont")).Text_Lambda([this]{FNumberFormattingOptions F;F.SetMaximumFractionalDigits(1);return FText::AsNumber(Profile.IsValid()?Profile->Ecosystem.Weather.Character*100:0,&F);})]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).IsEnabled_Lambda(Enabled).ButtonStyle(FAppStyle::Get(),"SimpleButton").ContentPadding(3)
                    .ToolTipText(FText::FromString(TEXT("Include weather in preview"))).OnClicked_Lambda([this]{if(bWeatherSolo)bWeatherSolo=false;bWeatherEye=!bWeatherEye;RefreshWeatherEye();return FReply::Handled();})
                    [SNew(SImage).DesiredSizeOverride(FVector2D(16,16)).Image_Lambda([this]{return FAppStyle::GetBrush(WeatherVisible()?TEXT("Level.VisibleIcon16x"):TEXT("Level.NotVisibleIcon16x"));})]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).IsEnabled_Lambda(Enabled).ButtonStyle(FAppStyle::Get(),"SimpleButton").ContentPadding(3)
                    .ToolTipText(FText::FromString(TEXT("Reseed weather"))).OnClicked_Lambda([this]{EditWeather(NSLOCTEXT("CE","WeatherReseed","Reseed weather"),[](auto& W){W.Seed=int32(uint32(W.Seed)+1U);});return FReply::Handled();})
                    [SNew(SImage).DesiredSizeOverride(FVector2D(16,16)).Image(FAppStyle::GetBrush("Icons.Refresh"))]]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0,0,0)[SNew(SBox).WidthOverride(50)[SNew(SButton).IsEnabled_Lambda(Enabled).ContentPadding(FMargin(3,2))
                    .ToolTipText(FText::FromString(TEXT("Preview weather on its own, restoring its previous visibility when leaving Solo.")))
                    .OnClicked_Lambda([this]{bWeatherSolo=!bWeatherSolo;RefreshWeatherEye();return FReply::Handled();})
                    [SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont")).Text_Lambda([this]{return FText::FromString(bWeatherSolo?TEXT("Unsolo"):TEXT("Solo"));})]]]]
            +SVerticalBox::Slot().AutoHeight().Padding(16,8,0,0)[Advanced]];
    }
    TSharedRef<SWidget> Label(const TCHAR* Text){return SNew(STextBlock).Text(FText::FromString(Text)).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));}
    void Rebuild()
    {
        ClimateBox->ClearChildren();BiomeBox->ClearChildren();WeatherDetailsBox->ClearChildren();WeatherView.Reset();WeatherDraft.Reset();BiomeViews.Reset();ClimateView.Reset();ClimateDraft.Reset();
        if(!Profile.IsValid())return;
        FDetailsViewArgs Args;Args.bUpdatesFromSelection=false;Args.bHideSelectionTip=true;Args.NameAreaSettings=FDetailsViewArgs::HideNameArea;
        FStructureDetailsViewArgs StructArgs;
        WeatherDraft=MakeShared<FStructOnScope>(FCEWeatherSettings::StaticStruct());
        *reinterpret_cast<FCEWeatherSettings*>(WeatherDraft->GetStructMemory())=Profile->Ecosystem.Weather;
        WeatherView=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateStructureDetailView(Args,StructArgs,WeatherDraft);
        WeatherView->GetOnFinishedChangingPropertiesDelegate().AddLambda([this](const FPropertyChangedEvent&)
        {EditWeather(NSLOCTEXT("CE","WeatherEdit","Edit planet weather"),[this](auto& W){W=*reinterpret_cast<const FCEWeatherSettings*>(WeatherDraft->GetStructMemory());});});
        WeatherView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P){return P.Property.GetOwnerStruct()!=FCEWeatherSettings::StaticStruct()||(P.Property.GetFName()!=TEXT("bEnabled")&&P.Property.GetFName()!=TEXT("Character")&&P.Property.GetFName()!=TEXT("DaySeconds"));}));
        WeatherView->GetDetailsView()->ForceRefresh();
        WeatherDetailsBox->AddSlot().AutoHeight().Padding(0,1)[WeatherRow(WeatherView->GetWidget().ToSharedRef())];
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

    }
    void StartPreview(TSharedPtr<const FCLCompiledPlanetRecipe> Plan)
    {
        bPreviewDirty=false;FString Error;if(!Profile->Ecosystem.Validate(Error)){Status=Error;Context.PublishOverlay(nullptr);return;}
        bool Pending;const auto Placement=CEGetCompiledBiomePlacement(Plan,Profile.Get(),Pending,Error);
        if(!Placement){Status=Pending?TEXT("Preparing planetary biome regions…"):Error;bPreviewDirty=Pending;Context.PublishOverlay(nullptr);return;}
        const auto Visibility=PreviewState;
        WorkRevision=Revision;Status=TEXT("Evaluating climate on the accepted planet surface…");
        Stop=MakeShared<TAtomic<bool>,ESPMode::ThreadSafe>(false);const auto Cancel=Stop;
        Work=Async(EAsyncExecution::ThreadPool,[Placement,Visibility,Cancel]()->TSharedPtr<const FCLPlanetBuilderOverlay>
        {
            auto Overlay=MakeShared<FCLPlanetBuilderOverlay>();
            for(const auto& C:Placement->Cells)
            {
                if(Cancel->Load())return nullptr;
                if(!C.bSupported||!Visibility.Visible(C.BiomeId))continue;
                const auto* B=Placement->Model.Biomes.FindByPredicate([&](const auto& V){return V.Id==C.BiomeId;});if(!B)continue;
                Overlay->BodyMeters.Add(C.PositionMeters);Overlay->Colors.Add(B->Color);
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
