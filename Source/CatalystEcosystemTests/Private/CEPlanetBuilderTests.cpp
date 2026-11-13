#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
#include "Model/CEPlanetEcosystem.h"
#include "Model/CEPlanetEcosystemFactory.h"
#include "Engine/Selection.h"
#include "Widgets/IToolTip.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetDefinitionRegistry.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
namespace
{
FString WidgetText(TSharedRef<SWidget> Widget)
{
    FString Text;if(Widget->GetType()==FName(TEXT("STextBlock")))Text=StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)Text+=WidgetText(Children->GetChildAt(I));return Text;
}
TSharedPtr<SCheckBox> Check(TSharedRef<SWidget> W,const FString& Text)
{
    if(W->GetType()==FName(TEXT("SCheckBox"))&&WidgetText(W)==Text)return StaticCastSharedRef<SCheckBox>(W);
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=Check(C->GetChildAt(I),Text))return R;return nullptr;
}
TSharedPtr<SButton> Button(TSharedRef<SWidget> W,const FString& Text,int32& Index)
{
    if(W->GetType()==FName(TEXT("SButton"))&&WidgetText(W)==Text&&Index--==0)return StaticCastSharedRef<SButton>(W);
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=Button(C->GetChildAt(I),Text,Index))return R;return nullptr;
}
struct FToolkitReview : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UCLPlanetRecipeAsset> Planet{NewObject<UCLPlanetRecipeAsset>()};
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile{NewObject<UCEPlanetEcosystemAsset>()};
    struct FPreviewMetrics{int32 Samples=-1;};
    TSharedRef<FPreviewMetrics> Metrics=MakeShared<FPreviewMetrics>();
    TOptional<FCLPlanetBuilderProvider> OriginalProvider;
    TSharedPtr<SWindow> Window;int32 Stage=0;double Start=FPlatformTime::Seconds(),ReadyAt=0;
    explicit FToolkitReview(FAutomationTestBase* T):Test(T)
    {
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.Name=TEXT("Ecosystem globe qualification");Planet->Recipe.RadiusMeters=100;
        Planet->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
        // Observe the real overlay contract, not implementation/debug text in the UI.
        for(const auto& Provider:FCLPlanetBuilderExtensions::Providers())if(Provider.Id==TEXT("CatalystEcosystem"))
        {
            OriginalProvider=Provider;auto Observed=Provider;const auto Create=Provider.Create;const auto View=Metrics;
            Observed.Create=[Create,View](FCLPlanetBuilderContext Context)
            {
                const auto Publish=Context.PublishOverlay;
                Context.PublishOverlay=[Publish,View](auto Overlay){View->Samples=Overlay?Overlay->BodyMeters.Num():-1;Publish(Overlay);};
                return Create(MoveTemp(Context));
            };
            FCLPlanetBuilderExtensions::Register(MoveTemp(Observed));break;
        }
        Test->TestTrue(TEXT("Planet opens through native asset editor"),GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Planet.Get()));
    }
    ~FToolkitReview(){if(OriginalProvider.IsSet())FCLPlanetBuilderExtensions::Register(OriginalProvider.GetValue());}
    bool Close(){GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Planet.Get());return true;}
    void Capture(const TCHAR* Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(Test->TestTrue(TEXT("Actual Planet editor renders"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)))
        {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const auto Folder=FPaths::ProjectSavedDir()/TEXT("EcosystemQualification");IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveArrayToFile(PNG,*(Folder/(FString(Name)+TEXT(".png"))));}
    }
    bool Toggle(const TCHAR* Name)
    {auto C=Check(Window.ToSharedRef(),Name);if(!Test->TestTrue(FString(Name)+TEXT(" sub-tab exists"),C.IsValid()))return false;C->ToggleCheckedState();return true;}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>60){Test->AddError(FString::Printf(TEXT("Timed out waiting for native Ecosystem Planet tab (stage %d)"),Stage));return Close();}
        if(Stage==0)
        {
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
            for(auto W:Windows)if(auto Box=Check(W,TEXT("Ecosystem"))){Window=W;Box->ToggleCheckedState();Stage=1;return false;}
        }
        if(!Window)return false;
        const auto Text=WidgetText(Window.ToSharedRef());
        if(Stage==1)
        {
            if(Metrics->Samples!=2048||!Text.Contains(TEXT("OceanHeight · 40.00 m")))return false;
            if(!ReadyAt){ReadyAt=FPlatformTime::Seconds();return false;}if(FPlatformTime::Seconds()-ReadyAt<3)return false;
            Test->TestTrue(TEXT("Preview is last after optional authoring sections"),Text.Contains(TEXT("PlanetGeologyProvincesEcosystemPreview")));
            Test->TestTrue(TEXT("Both sub-tabs use the actual Planet extension"),Text.Contains(TEXT("ClimateBiomes")));
            Test->TestFalse(TEXT("An independent sea-level editor is absent"),Text.Contains(TEXT("Sea Level Meters")));
            Capture(TEXT("planet-builder"));if(!Toggle(TEXT("Biomes")))return Close();Stage=2;return false;
        }
        if(Stage==2)
        {
            Test->TestFalse(TEXT("Biome authoring hides deferred creation and technical wiring controls"),Text.Contains(TEXT("Add biome"))||Text.Contains(TEXT("Configured VPP surface sampler graph"))||Text.Contains(TEXT("Build Biome Core assets"))||Text.Contains(TEXT("surface samples")));
            Capture(TEXT("biomes"));int32 Index=1;auto Solo=Button(Window.ToSharedRef(),TEXT("Solo"),Index);
            if(!Test->TestTrue(TEXT("Compact polar biome row has a Solo button"),Solo.IsValid()))return Close();
            Solo->SimulateClick();Stage=3;return false;
        }
        if(Stage==3)
        {
            if(Metrics->Samples<0||Metrics->Samples>=2048)return false;
            int32 Index=0;auto Solo=Button(Window.ToSharedRef(),TEXT("Unsolo"),Index);if(!Solo)return false;
            Solo->SimulateClick();Stage=4;return false;
        }
        if(Stage==4)
        {
            if(Metrics->Samples!=2048)return false;
            if(!Toggle(TEXT("Climate")))return Close();
            auto* Ocean=Planet->Recipe.RadialGuides.Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});
            if(!Test->TestNotNull(TEXT("Planet has its OceanHeight guide"),Ocean))return Close();
            Ocean->NormalizedHeight=.9;Planet->OnRecipeChanged.Broadcast();Stage=5;return false;
        }
        if(Stage==5&&Text.Contains(TEXT("OceanHeight · 90.00 m"))&&Metrics->Samples==2048)
        {
            Test->TestEqual(TEXT("Guide changes never rewrite stored climate settings"),Profile->Ecosystem.Climate.SeaLevelMeters,0.);
            Capture(TEXT("ocean-height-guide"));return Close();
        }
        return false;
    }
};
TSharedPtr<SWidget> FindType(TSharedRef<SWidget> W,FName Type)
{
    if(W->GetType()==Type)return W;
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=FindType(C->GetChildAt(I),Type))return R;return nullptr;
}
TSharedPtr<SButton> UseSelected(TSharedRef<SWidget> W)
{
    if(W->GetToolTip().IsValid()&&WidgetText(W->GetToolTip()->AsWidget()).Contains(TEXT("Use Selected Asset")))
        if(auto B=FindType(W,TEXT("SButton")))return StaticCastSharedPtr<SButton>(B);
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=UseSelected(C->GetChildAt(I)))return R;return nullptr;
}
struct FAssetPickerReview : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UCLPlanetRecipeAsset> Planet{NewObject<UCLPlanetRecipeAsset>(GetTransientPackage(),NAME_None,RF_Transactional)};
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Example;
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Created;
    TSharedPtr<SWindow> Window;
    int32 Stage=0;double Start=FPlatformTime::Seconds(),SelectionAt=0;
    explicit FAssetPickerReview(FAutomationTestBase* T):Test(T)
    {
        Example.Reset(Cast<UCEPlanetEcosystemAsset>(UCEPlanetEcosystemAsset::ExampleAssetPath().TryLoad()));
        Test->TestTrue(TEXT("Packaged example loads"),Example.IsValid());
        if(Example.IsValid())
        {
            FString Error;Test->TestTrue(TEXT("Packaged example is valid"),Example->Ecosystem.Validate(Error));
            Test->TestEqual(TEXT("Current nine-biome example is shipped"),Example->Ecosystem.Biomes.Num(),9);
            const auto* Definition=UAssetDefinitionRegistry::Get()->GetAssetDefinitionForAsset(FAssetData(Example.Get()));
            Test->TestTrue(TEXT("Ecosystem uses the Foundation asset definition"),
                Definition&&Definition->IsA<UCEPlanetEcosystemAssetDefinition>());
            Test->TestTrue(TEXT("Ecosystem uses the Foundation model asset"),
                Example->IsA<UCFModelAsset>());
        }
        TStrongObjectPtr<UCEPlanetEcosystemFactory> Factory(NewObject<UCEPlanetEcosystemFactory>());
        Test->TestTrue(TEXT("Native factory is discoverable by the standard picker"),
            PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses({UCEPlanetEcosystemAsset::StaticClass()})
            .ContainsByPredicate([](const UFactory* F){return F->IsA<UCEPlanetEcosystemFactory>();}));
        Created.Reset(Cast<UCEPlanetEcosystemAsset>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().CreateAsset(
            TEXT("DA_PickerEcosystem_")+FGuid::NewGuid().ToString(EGuidFormats::Digits),TEXT("/Game/__EcosystemPickerTest"),
            UCEPlanetEcosystemAsset::StaticClass(),Factory.Get())));
        Test->TestTrue(TEXT("Factory creates an ecosystem"),Created.IsValid());
        if(Example.IsValid()&&Created.IsValid())
        {
            FString A,B;FCEPlanetEcosystem::StaticStruct()->ExportText(A,&Example->Ecosystem,nullptr,nullptr,PPF_None,nullptr);
            FCEPlanetEcosystem::StaticStruct()->ExportText(B,&Created->Ecosystem,nullptr,nullptr,PPF_None,nullptr);
            Test->TestEqual(TEXT("New ecosystem starts from packaged example"),B,A);
        }
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.RadiusMeters=100;
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Planet.Get());
    }
    bool Close(){GEditor->GetSelectedObjects()->Deselect(Created.Get());GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Planet.Get());return true;}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>40){Test->AddError(FString::Printf(TEXT("Asset picker review timed out at stage %d"),Stage));return Close();}
        if(Stage==0)
        {
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
            for(auto W:Windows)if(auto C=Check(W,TEXT("Ecosystem"))){Window=W;C->ToggleCheckedState();Stage=1;return false;}
        }
        if(!Window||!Example.IsValid()||!Created.IsValid())return false;
        const FString Text=WidgetText(Window.ToSharedRef());
        if(Stage==1)
        {
            if(!Text.Contains(TEXT("DA_ExampleEcosystem")))return false;
            Test->TestFalse(TEXT("Opening an unlinked planet does not author a link"),Planet->AuthoringExtensions.Contains(TEXT("CatalystEcosystem")));
            Test->TestTrue(TEXT("Default picker is above the sub-tabs"),Text.Find(TEXT("DA_ExampleEcosystem"))<Text.Find(TEXT("ClimateBiomes")));
            auto Picker=FindType(Window.ToSharedRef(),TEXT("SObjectPropertyEntryBox"));
            if(!Test->TestTrue(TEXT("Uses native UE object picker"),Picker.IsValid()))return Close();
            GEditor->SyncBrowserToObjects(TArray<UObject*>{Created.Get()});SelectionAt=FPlatformTime::Seconds();Stage=10;return false;
        }
        if(Stage==10)
        {
            if(FPlatformTime::Seconds()-SelectionAt<.5)return false;
            auto Picker=FindType(Window.ToSharedRef(),TEXT("SObjectPropertyEntryBox"));
            auto Use=Picker.IsValid()?UseSelected(Picker.ToSharedRef()):nullptr;
            if(!Test->TestTrue(TEXT("Native picker supports existing asset selection"),Use.IsValid()))return Close();
            Use->SimulateClick();Stage=2;return false;
        }
        const auto* Link=Planet->AuthoringExtensions.Find(TEXT("CatalystEcosystem"));
        if(Stage==2)
        {
            if(!Link||*Link!=FSoftObjectPath(Created.Get()))return false;
            Test->TestTrue(TEXT("Selected asset is shown by both panels"),Text.Contains(Created->GetName()));
            GEditor->UndoTransaction();Stage=3;return false;
        }
        if(Stage==3)
        {
            if(Link)return false;
            Test->TestTrue(TEXT("Undo restores the packaged default"),Text.Contains(TEXT("DA_ExampleEcosystem")));
            GEditor->RedoTransaction();Stage=4;return false;
        }
        if(Stage==4&&Link&&*Link==FSoftObjectPath(Created.Get()))
        {Test->TestTrue(TEXT("Redo restores the selected asset"),Text.Contains(Created->GetName()));return Close();}
        return false;
    }
};

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEToolkitTest,"Catalyst.Ecosystem.Editor.NativePlanetBuilder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEToolkitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FToolkitReview(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEAssetPickerTest,"Catalyst.Ecosystem.Editor.AssetPicker",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEAssetPickerTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FAssetPickerReview(this));return true;}
#endif
