#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
#include "Model/CEPlanetEcosystem.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SCheckBox.h"
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
TSharedPtr<SCheckBox> Check(TSharedRef<SWidget> W,const FString& Text,int32& Index)
{
    if(W->GetType()==FName(TEXT("SCheckBox"))&&WidgetText(W)==Text){if(Index--==0)return StaticCastSharedRef<SCheckBox>(W);}
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=Check(C->GetChildAt(I),Text,Index))return R;return nullptr;
}
struct FPanelReview : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UCLPlanetRecipeAsset> Planet{NewObject<UCLPlanetRecipeAsset>()};
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile{NewObject<UCEPlanetEcosystemAsset>()};
    TSharedPtr<ICLPlanetBuilderExtension> Session;
    TSharedPtr<SWindow> Window;
    TSharedPtr<const FCLPlanetBuilderOverlay> Overlay;
    int32 Stage=0,Publications=0;double Start=FPlatformTime::Seconds();
    explicit FPanelReview(FAutomationTestBase* T):Test(T)
    {
        Planet->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
        const auto Providers=FCLPlanetBuilderExtensions::Providers();auto* Provider=Providers.FindByPredicate([](const auto& P){return P.Id==TEXT("CatalystEcosystem");});
        if(!Test->TestNotNull(TEXT("Ecosystem provider registered in Landform"),Provider))return;
        const auto Plan=MakeShared<FCLCompiledPlanetRecipe>();Plan->RadiusMeters=100;
        FCLPlanetBuilderContext C;C.Planet=Planet.Get();C.GetPlan=[Plan]{return Plan;};
        C.PublishOverlay=[this](TSharedPtr<const FCLPlanetBuilderOverlay> O){Overlay=MoveTemp(O);++Publications;};
        Session=Provider->Create(MoveTemp(C));Window=SNew(SWindow).Title(FText::FromString(TEXT("Catalyst Ecosystem qualification"))).ClientSize(FVector2D(700,950))[Session->Widget()];
        FSlateApplication::Get().AddWindow(Window.ToSharedRef());Session->SetActive(true);
    }
    void Capture(const TCHAR* Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(FSlateApplication::Get().TakeScreenshot(Session->Widget(),Pixels,Size))
        {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const auto Folder=FPaths::ProjectSavedDir()/TEXT("EcosystemQualification");IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveArrayToFile(PNG,*(Folder/(FString(Name)+TEXT(".png"))));}
    }
    bool Toggle(const TCHAR* Text,int32 Index=0){auto Box=Check(Session->Widget(),Text,Index);if(!Test->TestTrue(FString::Printf(TEXT("%s control present"),Text),Box.IsValid()))return false;Box->ToggleCheckedState();return true;}
    virtual bool Update() override
    {
        if(!Session)return true;
        if(FPlatformTime::Seconds()-Start>30){Test->AddError(TEXT("Timed out waiting for ecosystem overlay"));return Close();}
        if(Stage==0&&Overlay)
        {
            Test->TestEqual(TEXT("Climate preview covers complete sphere"),Overlay->BodyMeters.Num(),2048);
            Test->TestTrue(TEXT("Both sub-tabs present"),WidgetText(Session->Widget()).Contains(TEXT("ClimateBiomes")));Capture(TEXT("climate"));
            if(!Toggle(TEXT("Biomes")))return Close();Stage=1;return false;
        }
        if(Stage==1)
        {
            Capture(TEXT("biomes"));if(!Toggle(TEXT("Solo"),1))return Close();Stage=2;Overlay.Reset();return false;
        }
        if(Stage==2&&Overlay)
        {
            Test->TestTrue(TEXT("Whole-planet Solo shows only the polar biome"),Overlay->BodyMeters.Num()>0&&Overlay->BodyMeters.Num()<2048);
            Capture(TEXT("polar-solo"));if(!Toggle(TEXT("Solo"),1))return Close();Stage=3;Overlay.Reset();return false;
        }
        if(Stage==3&&Overlay)
        {
            Test->TestEqual(TEXT("Unsolo restores all surface markers"),Overlay->BodyMeters.Num(),2048);
            TArray<UObject*> Save;Session->GetSaveableObjects(Save);Test->TestTrue(TEXT("Planet save includes profile"),Save.Contains(Profile.Get()));
            Session->SetActive(false);Test->TestFalse(TEXT("Leaving tab clears overlay"),Overlay.IsValid());return Close();
        }
        return false;
    }
    bool Close(){if(Session)Session->SetActive(false);if(Window)Window->SetContent(SNullWidget::NullWidget);Session.Reset();if(Window)Window->RequestDestroyWindow();Window.Reset();return true;}
};
}

namespace
{
struct FToolkitReview : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UCLPlanetRecipeAsset> Planet{NewObject<UCLPlanetRecipeAsset>()};
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile{NewObject<UCEPlanetEcosystemAsset>()};
    TSharedPtr<SWindow> Window;int32 Stage=0;double Start=FPlatformTime::Seconds(),ReadyAt=0;
    explicit FToolkitReview(FAutomationTestBase* T):Test(T)
    {
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.Name=TEXT("Ecosystem globe qualification");Planet->Recipe.RadiusMeters=100;
        Planet->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
        Test->TestTrue(TEXT("Planet opens through native asset editor"),GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Planet.Get()));
    }
    bool Close(){GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Planet.Get());return true;}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>45){Test->AddError(FString::Printf(TEXT("Timed out waiting for native Ecosystem planet tab (stage %d)"),Stage));
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);FString Text;for(auto W:Windows)Text+=WidgetText(W);FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("EcosystemQualification/toolkit-timeout.txt")));return Close();}
        if(Stage==0)
        {
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
            for(auto W:Windows)
            {
                int32 Index=0;if(auto Box=Check(W,TEXT("Ecosystem"),Index)){Window=W;Box->ToggleCheckedState();Stage=1;return false;}
            }
        }
        if(Stage==1&&Window)
        {
            const auto Text=WidgetText(Window.ToSharedRef());if(!Text.Contains(TEXT("Biome preview: 2048")))return false;
            // Give the independent native VPP preview time to publish the accepted plan.
            if(!ReadyAt){ReadyAt=FPlatformTime::Seconds();return false;}if(FPlatformTime::Seconds()-ReadyAt<3)return false;
            Test->TestTrue(TEXT("Ecosystem is a native Planet Builder section"),Text.Contains(TEXT("ClimateBiomes")));
            TArray<FColor> Pixels;FIntVector Size;if(Test->TestTrue(TEXT("Full Planet Builder renders"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)))
            {TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const auto Folder=FPaths::ProjectSavedDir()/TEXT("EcosystemQualification");IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveArrayToFile(PNG,*(Folder/TEXT("planet-builder.png")));}
            return Close();
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEToolkitTest,"Catalyst.Ecosystem.Editor.NativePlanetBuilder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEToolkitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FToolkitReview(this));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPlanetBuilderTest,"Catalyst.Ecosystem.Editor.LandformClimateAndBiomeTabs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPlanetBuilderTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FPanelReview(this));return true;}
#endif
