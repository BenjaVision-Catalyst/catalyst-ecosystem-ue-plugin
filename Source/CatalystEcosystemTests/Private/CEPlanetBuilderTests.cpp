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
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableText.h"
#include "InputCoreTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/StrongObjectPtr.h"
#include "ImageUtils.h"
#include "ShaderCompiler.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Components/DynamicMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
namespace
{
FString WidgetText(TSharedRef<SWidget> Widget)
{
    FString Text;if(Widget->GetType()==FName(TEXT("STextBlock")))Text=StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)Text+=WidgetText(Children->GetChildAt(I));return Text;
}
bool VisibleText(TSharedRef<SWidget> W,const FString& Text)
{
    if(!W->GetVisibility().IsVisible()||W->GetCachedGeometry().GetLocalSize().Y<1)return false;
    if(W->GetType()==FName(TEXT("STextBlock"))&&StaticCastSharedRef<STextBlock>(W)->GetText().ToString()==Text)return true;
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(VisibleText(C->GetChildAt(I),Text))return true;return false;
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
TSharedPtr<SEditableText> TextEntry(TSharedRef<SWidget> W)
{
    if(W->GetType()==TEXT("SEditableText"))return StaticCastSharedRef<SEditableText>(W);
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto V=TextEntry(C->GetChildAt(I)))return V;return nullptr;
}
TSharedPtr<SWidget> SpinEntry(TSharedRef<SWidget> W)
{
    if(W->GetTypeAsString().StartsWith(TEXT("SSpinBox")))return W;
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto V=SpinEntry(C->GetChildAt(I)))return V;return nullptr;
}
TSharedPtr<SWidget> PropertyEntry(TSharedRef<SWidget> W,const FString& Label)
{
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto V=PropertyEntry(C->GetChildAt(I),Label))return V;
    return WidgetText(W).Contains(Label)?SpinEntry(W):nullptr;
}
TSharedPtr<SEditorViewport> PlanetViewport(TSharedRef<SWidget> W)
{
    if(W->GetType()==FName(TEXT("SCLPlanetPreview")))return StaticCastSharedRef<SEditorViewport>(W);
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto V=PlanetViewport(C->GetChildAt(I)))return V;return nullptr;
}
TSharedPtr<SWidget> WeatherControl(TSharedRef<SWidget> W,FName Type,const FString& Tip)
{
    if(W->GetType()==Type&&W->GetToolTip().IsValid()&&WidgetText(W->GetToolTip()->AsWidget()).Contains(Tip))return W;
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto Found=WeatherControl(C->GetChildAt(I),Type,Tip))return Found;return nullptr;
}
struct FToolkitReview : IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UCLPlanetRecipeAsset> Planet{NewObject<UCLPlanetRecipeAsset>()};
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile{NewObject<UCEPlanetEcosystemAsset>()};
    struct FPreviewMetrics{int32 Samples=-1,FlowPaths=0,Arrowheads=0,Segments=0,Labels=0;double MeshRadius=0;bool PlainLabels=true,FreezeWeather=false;TArray<FLinearColor> Colors;TSharedPtr<const FCLPlanetBuilderOverlay> LastOverlay,FrozenOverlay;};
    TSharedRef<FPreviewMetrics> Metrics=MakeShared<FPreviewMetrics>();
    TOptional<FCLPlanetBuilderProvider> OriginalProvider;
    TSharedPtr<SWindow> Window;int32 Stage=0,FullSamples=0;double Start=FPlatformTime::Seconds(),ReadyAt=0,CaptureReady=0;
    bool PlanetDirtyBeforeWeather=false,ProfileDirtyBeforeWeather=false;
    TArray<FLinearColor> WeatherNowColors;
    FCEWeatherSettings WeatherBeforePreview;
    double TravelMultiplierBefore=0;FVector TravelSunBefore;
    TArray<FColor> FrozenFlowPixels;FIntPoint FrozenFlowSize;
    explicit FToolkitReview(FAutomationTestBase* T):Test(T)
    {
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.Name=TEXT("Ecosystem globe qualification");Planet->Recipe.RadiusMeters=100;for(auto& G:Planet->Recipe.RadialGuides.Guides)if(G.Name==TEXT("Overlay"))G.NormalizedHeight=.96;
        Planet->AuthoringExtensions.Add(TEXT("CatalystEcosystem"),FSoftObjectPath(Profile.Get()));
        // Observe the real overlay contract, not implementation/debug text in the UI.
        for(const auto& Provider:FCLPlanetBuilderExtensions::Providers())if(Provider.Id==TEXT("CatalystEcosystem"))
        {
            OriginalProvider=Provider;auto Observed=Provider;const auto Create=Provider.Create;const auto View=Metrics;
            Observed.Create=[Create,View](FCLPlanetBuilderContext Context)
            {
                const auto Publish=Context.PublishOverlay;
                Context.PublishOverlay=[Publish,View](auto Overlay)
                {
                    if(View->FreezeWeather&&Overlay&&!Overlay->Flows.IsEmpty()){Publish(View->FrozenOverlay);return;}
                    View->Samples=Overlay?(Overlay->MeshBodyMeters.IsEmpty()?Overlay->BodyMeters.Num():Overlay->MeshBodyMeters.Num()):-1;
                    View->FlowPaths=Overlay?Overlay->Flows.Num():0;View->Arrowheads=0;View->PlainLabels=true;
                    if(Overlay){for(const auto& F:Overlay->Flows)View->Arrowheads+=F.ArrowCount;for(const auto& L:Overlay->Labels)View->PlainLabels&=L.Text==TEXT("H")||L.Text==TEXT("L");}
                    View->Segments=Overlay?Overlay->Segments.Num():0;View->Labels=Overlay?Overlay->Labels.Num():0;
                    View->MeshRadius=Overlay&&!Overlay->MeshBodyMeters.IsEmpty()?Overlay->MeshBodyMeters[0].Length():0;
                    View->Colors=Overlay?(Overlay->MeshColors.IsEmpty()?Overlay->Colors:Overlay->MeshColors):TArray<FLinearColor>();View->LastOverlay=Overlay;Publish(Overlay);
                };
                return Create(MoveTemp(Context));
            };
            FCLPlanetBuilderExtensions::Register(MoveTemp(Observed));break;
        }
        Test->TestTrue(TEXT("Planet opens through native asset editor"),GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Planet.Get()));
    }
    ~FToolkitReview(){if(OriginalProvider.IsSet())FCLPlanetBuilderExtensions::Register(OriginalProvider.GetValue());}
    bool Close(){GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Planet.Get());return true;}
    bool Settled()
    {
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
        if(Metrics->MeshRadius>0)
        {
            // Payload publication can precede VPP's accepted surface. Wait for the
            // actual shell component to become visible before checking its pixels.
            auto V=PlanetViewport(Window.ToSharedRef());bool Visible=false;
            if(V&&V->GetViewportClient())for(TObjectIterator<UDynamicMeshComponent> C;C;++C)
                if(C->GetWorld()==V->GetViewportClient()->GetWorld()&&C->IsVisible()&&C->GetMaterial(0)&&C->GetMaterial(0)->GetOuter()==*C){Visible=true;break;}
            if(!Visible){CaptureReady=0;return false;}
        }
        if(!CaptureReady){CaptureReady=FPlatformTime::Seconds();return false;}if(FPlatformTime::Seconds()-CaptureReady<.4)return false;CaptureReady=0;return true;
    }
    void Capture(const TCHAR* Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(Test->TestTrue(TEXT("Actual Planet editor renders"),FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)))
        {
            if(FString(Name)==TEXT("weather-flow-a")){FrozenFlowPixels=Pixels;FrozenFlowSize=FIntPoint(Size.X,Size.Y);}
            if(FString(Name)==TEXT("weather-flow-b"))
            {
                int32 Changed=0;if(FrozenFlowPixels.Num()==Pixels.Num()&&FrozenFlowSize==FIntPoint(Size.X,Size.Y))
                    for(int32 Y=Size.Y/10;Y<Size.Y*9/10;++Y)for(int32 X=Size.X/2;X<Size.X;++X)
                    {
                        const int32 I=Y*Size.X+X;const auto A=FrozenFlowPixels[I],B=Pixels[I];
                        const bool WhiteA=A.R>210&&A.G>210&&A.B>210,WhiteB=B.R>210&&B.G>210&&B.B>210;
                        if(WhiteA!=WhiteB&&FMath::Abs(int32(A.R)-B.R)+FMath::Abs(int32(A.G)-B.G)+FMath::Abs(int32(A.B)-B.B)>40)++Changed;
                    }
                Test->TestTrue(TEXT("Rendered arrowheads march with the weather payload held fixed"),Changed>50);
                Test->AddInfo(FString::Printf(TEXT("Marching heads change %d white globe pixels with fixed fields"),Changed));FrozenFlowPixels.Reset();
            }
            TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);const auto Folder=FPaths::ProjectSavedDir()/TEXT("EcosystemQualification");IFileManager::Get().MakeDirectory(*Folder,true);FFileHelper::SaveArrayToFile(PNG,*(Folder/(FString(Name)+TEXT(".png"))));}
    }
    bool Toggle(const TCHAR* Name)
    {auto C=Check(Window.ToSharedRef(),Name);if(!Test->TestTrue(FString(Name)+TEXT(" sub-tab exists"),C.IsValid()))return false;C->ToggleCheckedState();return true;}
    virtual bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>90){Test->AddError(FString::Printf(TEXT("Timed out waiting for native Ecosystem Planet tab (stage %d)"),Stage));return Close();}
        if(Stage==0)
        {
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
            for(auto W:Windows)if(auto Box=Check(W,TEXT("Ecosystem"))){Window=W;Box->ToggleCheckedState();Stage=1;return false;}
        }
        if(!Window)return false;
        const auto Text=WidgetText(Window.ToSharedRef());
        if(Stage==1)
        {
            if(Metrics->Samples<128||Metrics->Samples>8192||!Text.Contains(TEXT("OceanHeight · 40.00 m")))return false;
            FullSamples=Metrics->Samples;
            for(const auto& B:Profile->Ecosystem.Biomes)if(B.bEnabled)
                Test->TestTrue(TEXT("Every enabled biome has a meaningful visible map region"),Metrics->Colors.FilterByPredicate([&](const FLinearColor& C){return C.Equals(B.Color);}).Num()>=16);
            if(!ReadyAt){ReadyAt=FPlatformTime::Seconds();return false;}if(FPlatformTime::Seconds()-ReadyAt<3)return false;
            Test->TestTrue(TEXT("Preview is last after optional authoring sections"),Text.Contains(TEXT("PlanetGeologyProvincesEcosystemPreview")));
            Test->TestTrue(TEXT("All three sub-tabs use the actual Planet extension"),Text.Contains(TEXT("ClimateBiomesWeather")));
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
            if(Metrics->Samples<=0||Metrics->Samples>=FullSamples)return false;
            for(const auto& C:Metrics->Colors)Test->TestTrue(TEXT("Solo filters the assignment without moving biomes"),C.Equals(Profile->Ecosystem.Biomes[1].Color));
            int32 Index=0;auto Solo=Button(Window.ToSharedRef(),TEXT("Unsolo"),Index);if(!Solo)return false;
            Solo->SimulateClick();Stage=4;return false;
        }
        if(Stage==4)
        {
            if(Metrics->Samples!=FullSamples)return false;
            if(!Toggle(TEXT("Climate")))return Close();
            auto* Ocean=Planet->Recipe.RadialGuides.Guides.FindByPredicate([](const auto& G){return G.Name==TEXT("OceanHeight");});
            if(!Test->TestNotNull(TEXT("Planet has its OceanHeight guide"),Ocean))return Close();
            Ocean->NormalizedHeight=.9;Planet->OnRecipeChanged.Broadcast();Stage=5;return false;
        }
        if(Stage==5&&Text.Contains(TEXT("OceanHeight · 90.00 m"))&&Metrics->Samples==FullSamples)
        {
            Test->TestEqual(TEXT("Guide changes never rewrite stored climate settings"),Profile->Ecosystem.Climate.SeaLevelMeters,0.);
            Capture(TEXT("ocean-height-guide"));
            PlanetDirtyBeforeWeather=Planet->GetOutermost()->IsDirty();ProfileDirtyBeforeWeather=Profile->GetOutermost()->IsDirty();
            if(!Toggle(TEXT("Weather")))return Close();ReadyAt=FPlatformTime::Seconds();Stage=6;return false;
        }
        if(Stage==6)
        {
            if(Metrics->Samples<=0||Metrics->FlowPaths<=0)return false;
            Test->TestFalse(TEXT("Weather removes layer buttons, toggles, readouts and timeline controls"),
                Check(Window.ToSharedRef(),TEXT("Temperature (°C)")).IsValid()||Check(Window.ToSharedRef(),TEXT("Pressure (hPa)")).IsValid()||
                Check(Window.ToSharedRef(),TEXT("Isobars")).IsValid()||Check(Window.ToSharedRef(),TEXT("Wind arrows")).IsValid()||Check(Window.ToSharedRef(),TEXT("Heatmap")).IsValid()||
                Text.Contains(TEXT("Simulation time"))||Text.Contains(TEXT("Planet range"))||Text.Contains(TEXT("Last hour"))||Text.Contains(TEXT("Advance hour")));
            Test->TestTrue(TEXT("Plain H/L labels and complete contour flow are published"),Metrics->PlainLabels&&Metrics->Labels>0&&Metrics->Segments>0&&Metrics->Arrowheads>0);
            Test->TestEqual(TEXT("Weather preview preserves planet authoring"),Planet->GetOutermost()->IsDirty(),PlanetDirtyBeforeWeather);
            Test->TestEqual(TEXT("Weather preview preserves ecosystem authoring"),Profile->GetOutermost()->IsDirty(),ProfileDirtyBeforeWeather);
            if(!Settled())return false;WeatherNowColors=Metrics->Colors;ReadyAt=FPlatformTime::Seconds();Capture(TEXT("weather-running"));Stage=7;return false;
        }
        if(Stage==7)
        {
            if(FPlatformTime::Seconds()-ReadyAt<1||Metrics->Colors==WeatherNowColors)return false;
            Test->TestFalse(TEXT("Weather underlying values are collapsed by default"),VisibleText(Window.ToSharedRef(),TEXT("Reference pressure (hPa)")));
            Test->TestFalse(TEXT("Travel multiplier is advanced, absent from compact row"),VisibleText(Window.ToSharedRef(),TEXT("Weather travel multiplier")));
            Test->TestTrue(TEXT("Weather has the compact Character row"),WeatherControl(Window.ToSharedRef(),TEXT("SSlider"),TEXT("Weather character:")).IsValid());
            Test->TestTrue(TEXT("Weather automatically advances without a Play action"),Metrics->Colors!=WeatherNowColors);
            Capture(TEXT("weather-running-later"));Metrics->FrozenOverlay=Metrics->LastOverlay;Metrics->FreezeWeather=true;ReadyAt=FPlatformTime::Seconds();Stage=8;return false;
        }
        if(Stage==8)
        {
            if(FPlatformTime::Seconds()-ReadyAt<.5||!Settled())return false;
            Capture(TEXT("weather-flow-a"));ReadyAt=FPlatformTime::Seconds();Stage=9;return false;
        }
        if(Stage==9)
        {
            if(FPlatformTime::Seconds()-ReadyAt<.6)return false;
            Capture(TEXT("weather-flow-b"));Metrics->FreezeWeather=false;Metrics->FrozenOverlay.Reset();
            for(auto& G:Planet->Recipe.RadialGuides.Guides)if(G.Name==TEXT("Overlay"))G.NormalizedHeight=.99;
            Planet->OnRecipeChanged.Broadcast();Stage=10;return false;
        }
        if(Stage==10)
        {
            if(!FMath::IsNearlyEqual(Metrics->MeshRadius,98.901,1.e-5)||!Settled())return false;
            Capture(TEXT("weather-overlay-moved"));Planet->Recipe.RadialGuides.Guides.RemoveAll([](const auto& G){return G.Name==TEXT("Overlay");});Planet->OnRecipeChanged.Broadcast();Stage=11;return false;
        }
        if(Stage==11)
        {
            if(!FMath::IsNearlyEqual(Metrics->MeshRadius,99.9,1.e-5)||!Settled())return false;
            Test->TestTrue(TEXT("Moving/removing Overlay retains contour animation and plain labels"),Metrics->FlowPaths>0&&Metrics->PlainLabels);
            Capture(TEXT("weather-sky-fallback"));Stage=13;return false;
        }
        if(Stage==13)
        {
            auto Expand=StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Edit underlying weather values")));
            if(!Test->TestTrue(TEXT("Weather has an advanced disclosure"),Expand.IsValid()))return Close();Expand->SimulateClick();Stage=14;return false;
        }
        if(Stage==14)
        {
            if(!VisibleText(Window.ToSharedRef(),TEXT("Reference pressure (hPa)")))return false;
            auto Travel=PropertyEntry(Window.ToSharedRef(),TEXT("Weather travel multiplier"));
            if(!Test->TestTrue(TEXT("Travel multiplier is editable in advanced settings"),Travel.IsValid())){Test->AddInfo(WidgetText(Window.ToSharedRef()));Capture(TEXT("weather-travel-missing"));return Close();}
            TravelMultiplierBefore=Profile->Ecosystem.Weather.TravelMultiplier;TravelSunBefore=Metrics->LastOverlay->SunDirection.GetValue();
            Travel->OnKeyDown(Travel->GetCachedGeometry(),FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            auto TravelText=TextEntry(Travel.ToSharedRef());if(!Test->TestTrue(TEXT("Travel enters native text edit mode"),TravelText.IsValid()))return Close();
            FSlateApplication::Get().SetKeyboardFocus(TravelText,EFocusCause::SetDirectly);TravelText->SetEditableText(FText::FromString(TEXT("0.5")));
            StaticCastSharedPtr<SWidget>(TravelText)->OnKeyDown(TravelText->GetCachedGeometry(),FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            Test->TestEqual(TEXT("Advanced travel commit edits fractional multiplier"),Profile->Ecosystem.Weather.TravelMultiplier,.5);
            Test->TestTrue(TEXT("Advanced travel commit anchors the existing simulation time"),Profile->Ecosystem.Weather.TravelPhaseOffsetSeconds>1800);
            FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
            Stage=141;ReadyAt=FPlatformTime::Seconds();return false;
        }
        if(Stage==141)
        {
            if(!Metrics->LastOverlay||!Settled()||FPlatformTime::Seconds()-ReadyAt<.5)return false;
            const FVector AfterSun=Metrics->LastOverlay->SunDirection.GetValue();
            Test->TestTrue(TEXT("Travel edit preserves the advancing day/night preview clock"),FVector::DotProduct(FVector::CrossProduct(TravelSunBefore,AfterSun),Profile->Ecosystem.Climate.PoleAxis.GetSafeNormal())>0&&FVector::DotProduct(TravelSunBefore,AfterSun)>.99);
            GEditor->UndoTransaction();Test->TestEqual(TEXT("Travel supports Undo"),Profile->Ecosystem.Weather.TravelMultiplier,TravelMultiplierBefore);
            GEditor->RedoTransaction();Test->TestEqual(TEXT("Travel supports Redo"),Profile->Ecosystem.Weather.TravelMultiplier,.5);GEditor->UndoTransaction();
            Capture(TEXT("weather-character-advanced"));
            StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Edit underlying weather values")))->SimulateClick();
            Stage=142;return false;
        }
        if(Stage==142)
        {
            auto Slider=StaticCastSharedPtr<SSlider>(WeatherControl(Window.ToSharedRef(),TEXT("SSlider"),TEXT("Weather character:")));if(!Test->TestTrue(TEXT("Character slider is usable"),Slider.IsValid()&&Slider->IsEnabled()))return Close();
            const double Before=Profile->Ecosystem.Weather.Character;
            const auto Geometry=Slider->GetCachedGeometry();if(Geometry.GetLocalSize().X<1)return false;
            const FVector2D Position=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*FVector2D(.75,.5));
            const FPointerEvent Press(0,Position,Position,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
            Slider->OnMouseButtonDown(Geometry,Press);Slider->OnMouseCaptureLost(FCaptureLostEvent(0,0));
            Test->TestTrue(TEXT("Character edits the authored weather"),Profile->Ecosystem.Weather.Character>Before);
            Test->TestTrue(TEXT("Character knob increases travel towards the right"),Profile->Ecosystem.Weather.TravelMultiplier>TravelMultiplierBefore);
            GEditor->UndoTransaction();Test->TestEqual(TEXT("Weather Character supports Undo"),Profile->Ecosystem.Weather.Character,Before);
            GEditor->RedoTransaction();Test->TestTrue(TEXT("Weather Character supports Redo"),Profile->Ecosystem.Weather.Character>Before);GEditor->UndoTransaction();Stage=15;return false;
        }
        if(Stage==15)
        {
            const int32 Seed=Profile->Ecosystem.Weather.Seed;auto Reseed=StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Reseed weather")));
            if(!Test->TestTrue(TEXT("Weather has a reseed action"),Reseed.IsValid()))return Close();Reseed->SimulateClick();Test->TestTrue(TEXT("Reseed changes the weather seed"),Profile->Ecosystem.Weather.Seed!=Seed);GEditor->UndoTransaction();Test->TestEqual(TEXT("Reseed undo restores the seed"),Profile->Ecosystem.Weather.Seed,Seed);
            WeatherBeforePreview=Profile->Ecosystem.Weather;
            StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Include weather in preview")))->SimulateClick();Stage=16;return false;
        }
        if(Stage==16)
        {
            if(Metrics->Samples!=-1)return false;
            StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Preview weather on its own")))->SimulateClick();Stage=17;return false;
        }
        if(Stage==17)
        {
            if(Metrics->Samples<=0)return false;
            Test->TestTrue(TEXT("Eye and Solo never rewrite weather authoring"),FCEWeatherSettings::StaticStruct()->CompareScriptStruct(&WeatherBeforePreview,&Profile->Ecosystem.Weather,0));
            StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Preview weather on its own")))->SimulateClick();Stage=18;return false;
        }
        if(Stage==18)
        {
            if(Metrics->Samples!=-1)return false;
            StaticCastSharedPtr<SButton>(WeatherControl(Window.ToSharedRef(),TEXT("SButton"),TEXT("Include weather in preview")))->SimulateClick();
            StaticCastSharedPtr<SCheckBox>(WeatherControl(Window.ToSharedRef(),TEXT("SCheckBox"),TEXT("Include weather in planet generation")))->ToggleCheckedState();ReadyAt=FPlatformTime::Seconds();Stage=19;return false;
        }
        if(Stage==19)
        {
            if(Metrics->Samples!=-1||FPlatformTime::Seconds()-ReadyAt<.2)return false;
            auto Slider=WeatherControl(Window.ToSharedRef(),TEXT("SSlider"),TEXT("Weather character:"));Test->TestTrue(TEXT("Unchecked weather disables Character"),!Profile->Ecosystem.Weather.bEnabled&&Slider.IsValid()&&!Slider->IsEnabled());
            StaticCastSharedPtr<SCheckBox>(WeatherControl(Window.ToSharedRef(),TEXT("SCheckBox"),TEXT("Include weather in planet generation")))->ToggleCheckedState();Stage=20;return false;
        }
        if(Stage==20)
        {
            if(Metrics->Samples<=0||!Settled())return false;
            Test->TestTrue(TEXT("Re-enabled weather restores the animated overlay"),Profile->Ecosystem.Weather.bEnabled&&Metrics->FlowPaths>0);Capture(TEXT("weather-character-row"));
            if(!Toggle(TEXT("Climate")))return Close();Stage=12;return false;
        }
        if(Stage==12&&Metrics->Samples==FullSamples)return Close();
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
        Planet->Recipe=FCLPlanetCompiler::BasePlanet();Planet->Recipe.RadiusMeters=100;for(auto& G:Planet->Recipe.RadialGuides.Guides)if(G.Name==TEXT("Overlay"))G.NormalizedHeight=.96;
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
