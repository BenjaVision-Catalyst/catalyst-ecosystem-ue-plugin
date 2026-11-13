#include "Misc/AutomationTest.h"
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
#include "Model/CEPlanetEcosystem.h"
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
    TSharedPtr<SWindow> Window;int32 Stage=0;double Start=FPlatformTime::Seconds(),ReadyAt=0;
    explicit FToolkitReview(FAutomationTestBase* T):Test(T)
    {
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.Name=TEXT("Ecosystem globe qualification");Planet->Recipe.RadiusMeters=100;
        Planet->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
        Test->TestTrue(TEXT("Planet opens through native asset editor"),GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Planet.Get()));
    }
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
            if(!Text.Contains(TEXT("Biome preview: 2048"))||!Text.Contains(TEXT("OceanHeight · 40.00 m")))return false;
            if(!ReadyAt){ReadyAt=FPlatformTime::Seconds();return false;}if(FPlatformTime::Seconds()-ReadyAt<3)return false;
            Test->TestTrue(TEXT("Preview is last after optional authoring sections"),Text.Contains(TEXT("PlanetGeologyLandformsEcosystemPreview")));
            Test->TestTrue(TEXT("Both sub-tabs use the actual Planet extension"),Text.Contains(TEXT("ClimateBiomes")));
            Test->TestFalse(TEXT("An independent sea-level editor is absent"),Text.Contains(TEXT("Sea Level Meters")));
            Capture(TEXT("planet-builder"));if(!Toggle(TEXT("Biomes")))return Close();Stage=2;return false;
        }
        if(Stage==2)
        {
            Capture(TEXT("biomes"));int32 Index=1;auto Solo=Button(Window.ToSharedRef(),TEXT("Solo"),Index);
            if(!Test->TestTrue(TEXT("Compact polar biome row has a Solo button"),Solo.IsValid()))return Close();
            Solo->SimulateClick();Stage=3;return false;
        }
        if(Stage==3)
        {
            if(Text.Contains(TEXT("Biome preview: 2048"))||!Text.Contains(TEXT("Biome preview:")))return false;
            int32 Index=0;auto Solo=Button(Window.ToSharedRef(),TEXT("Unsolo"),Index);if(!Solo)return false;
            Solo->SimulateClick();Stage=4;return false;
        }
        if(Stage==4)
        {
            if(!Text.Contains(TEXT("Biome preview: 2048")))return false;
            if(!Toggle(TEXT("Climate")))return Close();
            auto* Ocean=Planet->Recipe.RadialGuides.Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});
            if(!Test->TestNotNull(TEXT("Planet has its OceanHeight guide"),Ocean))return Close();
            Ocean->NormalizedHeight=.9;Planet->OnRecipeChanged.Broadcast();Stage=5;return false;
        }
        if(Stage==5&&Text.Contains(TEXT("OceanHeight · 90.00 m"))&&Text.Contains(TEXT("Biome preview: 2048")))
        {
            Test->TestEqual(TEXT("Guide changes never rewrite stored climate settings"),Profile->Ecosystem.Climate.SeaLevelMeters,0.);
            Capture(TEXT("ocean-height-guide"));return Close();
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEToolkitTest,"Catalyst.Ecosystem.Editor.NativePlanetBuilder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEToolkitTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FToolkitReview(this));return true;}
#endif
