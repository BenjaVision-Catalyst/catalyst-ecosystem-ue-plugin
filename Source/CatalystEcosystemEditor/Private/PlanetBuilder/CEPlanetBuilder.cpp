#include "PlanetBuilder/CEPlanetBuilder.h"
#if WITH_CE_LANDFORM
#include "Model/CEPlanetEcosystem.h"
#include "Climate/CEPlanetBiomePlacement.h"
#include "PlanetBuilder/CEBiomePreviewState.h"
#include "Weather/CEPlanetWeather.h"
#include "Widgets/Input/SSpinBox.h"
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
    FString Error;
    double Seconds=0;
    int64 ValidTime=0,Offset=0;
    int32 Layer=0;
};
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
        if(Plan!=ObservedPlan){ObservedPlan=Plan;++Revision;bPreviewDirty=true;ResetWeather();Context.PublishOverlay(nullptr);}
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
                if(bActive&&Subtab==2&&Result.Layer==WeatherLayer&&Result.Offset==WeatherOffset){WeatherSamples=MoveTemp(Result.Samples);RenderedWeatherTime=Result.ValidTime;Context.PublishOverlay(Result.Overlay);WeatherStatus=WeatherSummary();}
            }
            else if(WeatherRevision==Revision&&!Result.Error.IsEmpty())WeatherStatus=Result.Error;
        }
        if(bActive&&Subtab==2&&bWeatherPlaying)
        {
            const int64 Target=PlayStartSeconds+FMath::FloorToInt64((FPlatformTime::Seconds()-PlayStarted)*PreviewSpeed);
            if(Target>WeatherClock.AcceptedSeconds()&&WeatherClock.AdvanceTo(Target))bWeatherDirty=true;
        }
        if(bActive&&Subtab==2&&bWeatherDirty&&!WeatherWork.IsValid()&&!Work.IsValid()&&Profile.IsValid()&&Plan)StartWeather(Plan);
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
    TArray<FCEWeatherSample> WeatherSamples;
    FCSFieldRuntime WeatherClock{3600};
    FString WeatherStatus=TEXT("Preparing weather for this planet…");
    uint64 WeatherRevision=0;
    int32 WeatherLayer=0;
    int64 WeatherOffset=0,PlayStartSeconds=3600,RenderedWeatherTime=3600;
    double PreviewSpeed=60,PlayStarted=0,WeatherCost=0;
    bool bWeatherDirty=true,bWeatherPlaying=false;
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
        auto Layer=[this](const TCHAR* Name,int32 Index)->TSharedRef<SWidget>{return SNew(SCheckBox).Style(FAppStyle::Get(),"DetailsView.SectionButton")
            .IsChecked_Lambda([this,Index]{return WeatherLayer==Index?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
            .OnCheckStateChanged_Lambda([this,Index](ECheckBoxState C){if(C==ECheckBoxState::Checked){WeatherLayer=Index;bWeatherDirty=true;}})[Label(Name)];};
        auto TimeButton=[this](const TCHAR* Name,int64 Offset)->TSharedRef<SWidget>{return SNew(SButton).OnClicked_Lambda([this,Offset]{WeatherOffset=Offset;bWeatherDirty=true;return FReply::Handled();})[Label(Name)];};
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                .Text_Lambda([this]{const auto* P=Context.Planet.Get();return FText::FromString(P?P->Recipe.Name:TEXT("Planet unavailable"));})]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[Layer(TEXT("Temperature (°C)"),0)]
                +SHorizontalBox::Slot().FillWidth(1)[Layer(TEXT("Wind (m/s)"),1)]
                +SHorizontalBox::Slot().FillWidth(1)[Layer(TEXT("Rain (mm/h)"),2)]]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont")).AutoWrapText(true)
                .Text_Lambda([this]{return FText::FromString(WeatherStatus);})]
            +SVerticalBox::Slot().AutoHeight().Padding(4,12,4,4)[SNew(STextBlock).Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("Simulation time · %s · %s"),*FormatTime(WeatherClock.AcceptedSeconds()),bWeatherPlaying?TEXT("Running"):TEXT("Paused")));})]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[TimeButton(TEXT("Last hour"),-3600)]
                +SHorizontalBox::Slot().FillWidth(1)[TimeButton(TEXT("Now"),0)]
                +SHorizontalBox::Slot().FillWidth(1)[TimeButton(TEXT("Next hour"),3600)]]
            +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SSlider).Value_Lambda([this]{return float(WeatherOffset+3600)/7200;})
                .OnValueChanged_Lambda([this](float V){WeatherOffset=FMath::RoundToInt64(V*7200)-3600;bWeatherDirty=true;})]
            +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text_Lambda([this]{return FText::FromString(bWeatherPlaying?TEXT("Pause"):TEXT("Play"));})
                    .OnClicked_Lambda([this]{bWeatherPlaying=!bWeatherPlaying;PlayStarted=FPlatformTime::Seconds();PlayStartSeconds=WeatherClock.AcceptedSeconds();return FReply::Handled();})]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).OnClicked_Lambda([this]{WeatherClock.AdvanceTo(WeatherClock.AcceptedSeconds()+3600);bWeatherDirty=true;return FReply::Handled();})[Label(TEXT("Advance hour"))]]
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<double>).MinValue(1).MaxValue(600).Value_Lambda([this]{return PreviewSpeed;})
                    .OnValueChanged_Lambda([this](double V){PreviewSpeed=V;PlayStarted=FPlatformTime::Seconds();PlayStartSeconds=WeatherClock.AcceptedSeconds();}).ToolTipText(FText::FromString(TEXT("Simulation seconds per real second")))]]
            +SVerticalBox::Slot().FillHeight(1).Padding(4)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(WeatherDetailsBox,SVerticalBox)]];
    }
    static FString FormatTime(int64 S){return FString::Printf(TEXT("%02lld:%02lld:%02lld"),S/3600,(S/60)%60,S%60);}
    FString WeatherSummary()const
    {
        double Min=1.e9,Max=-1.e9;for(const auto& S:WeatherSamples){const double V=WeatherLayer==0?S.TemperatureC:(WeatherLayer==1?S.Wind.Length():S.RainMmHour);Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);}
        const TCHAR* Legend=WeatherLayer==0?TEXT("−40 blue → 40 °C red"):(WeatherLayer==1?TEXT("0 blue → 60 m/s yellow"):TEXT("0 clear → 50 mm/h violet"));
        const TCHAR* Kind=WeatherOffset<0?TEXT("History"):(WeatherOffset>0?TEXT("Forecast"):TEXT("Now"));
        return FString::Printf(TEXT("%s at %s · %s\nPlanet range %.1f–%.1f%s"),Kind,*FormatTime(RenderedWeatherTime),Legend,Min,Max,WeatherOffset>0?TEXT(" · current inputs assumed"):TEXT(""));
    }
    void ResetWeather()
    {
        WeatherField.Reset();WeatherPositions.Reset();WeatherSamples.Reset();WeatherClock=FCSFieldRuntime(3600);bWeatherDirty=true;bWeatherPlaying=false;WeatherStatus=TEXT("Preparing weather for this planet…");
    }
    void StartWeather(TSharedPtr<const FCLCompiledPlanetRecipe> Plan)
    {
        FString Error;bool Pending=false;auto Placement=CEGetCompiledBiomePlacement(Plan,Profile.Get(),Pending,Error);
        if(!Placement){WeatherStatus=Pending?TEXT("Preparing the accepted planet surface…"):Error;return;}
        bWeatherDirty=false;WeatherRevision=Revision;
        auto Field=WeatherField;auto Positions=WeatherPositions;auto Model=Profile->Ecosystem;Model.Climate.SeaLevelMeters=CEPlanetSeaLevelMeters(*Plan);
        const auto Offset=WeatherOffset,Time=WeatherClock.AcceptedSeconds()+Offset;const int32 Layer=WeatherLayer,Seed=Context.Planet.IsValid()?Context.Planet->Recipe.Seed:0;
        Stop=MakeShared<TAtomic<bool>,ESPMode::ThreadSafe>(false);auto Cancel=Stop;
        WeatherWork=Async(EAsyncExecution::ThreadPool,[Field,Positions=MoveTemp(Positions),Model,Placement,Plan,Time,Offset,Layer,Seed,Cancel]()mutable
        {
            FCEWeatherView R;R.ValidTime=Time;R.Layer=Layer;R.Offset=Offset;const double Start=FPlatformTime::Seconds();
            if(!Field)
            {
                Field=MakeShared<FCEPlanetWeather,ESPMode::ThreadSafe>();TArray<FVector> Normals;TArray<FGuid> Biomes;
                for(int I=0;I<Placement->Cells.Num();I+=3)if(Placement->Cells[I].bSupported){Positions.Add(Placement->Cells[I].PositionMeters);Normals.Add(Placement->Cells[I].Normal);Biomes.Add(Placement->Cells[I].BiomeId);}
                if(Cancel->Load()||!Field->Initialize(Model,Positions,Normals,Plan->RadiusMeters+Plan->BaseHeightMeters,Seed,R.Error,Biomes))return R;
            }
            if(Cancel->Load())return R;
            if(!Field->Evaluate(Time,R.Samples,R.Error))return R;
            auto Overlay=MakeShared<FCLPlanetBuilderOverlay>();Overlay->PointSizePixels=14;Overlay->BodyMeters=Positions;Overlay->Colors.Reserve(Positions.Num());
            for(const auto& S:R.Samples)
            {
                const double Value=Layer==0?(S.TemperatureC+40)/80.:(Layer==1?S.Wind.Length()/60.:S.RainMmHour/50.);const float V=FMath::Clamp(Value,0.,1.);
                FLinearColor C;if(Layer==0)C=V<.5?FMath::Lerp(FLinearColor(.05,.15,.95),FLinearColor(.1,.8,.4),V*2):FMath::Lerp(FLinearColor(.1,.8,.4),FLinearColor(1,.08,.02),(V-.5)*2);
                else if(Layer==1)C=FMath::Lerp(FLinearColor(.02,.15,.5),FLinearColor(1,.85,.03),V);
                else C=V<.5?FMath::Lerp(FLinearColor(.09,.12,.18),FLinearColor(.02,.65,1),V*2):FMath::Lerp(FLinearColor(.02,.65,1),FLinearColor(.9,.04,.9),(V-.5)*2);
                Overlay->Colors.Add(C);
                if(Layer==1)Overlay->BodyVectorsMeters.Add(S.Wind*Plan->RadiusMeters*.002);
            }
            R.Field=Field;R.Positions=MoveTemp(Positions);R.Overlay=Overlay;R.Seconds=FPlatformTime::Seconds()-Start;return R;
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
        {if(!Profile.IsValid())return;const FScopedTransaction Tx(NSLOCTEXT("CE","WeatherEdit","Edit planet weather"));Profile->Modify();Profile->Ecosystem.Weather=*reinterpret_cast<const FCEWeatherSettings*>(WeatherDraft->GetStructMemory());Profile->PostEditChange();});
        WeatherDetailsBox->AddSlot().AutoHeight()[WeatherView->GetWidget().ToSharedRef()];
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
