#include "CoreMinimal.h"
#if WITH_CE_LANDFORM && WITH_DEV_AUTOMATION_TESTS
#include "CLPlanetInspection.h"
#include "Model/CLPlanetRecipe.h"
#include "Model/CEPlanetEcosystem.h"
#include "Climate/CEPlanetBiomePlacement.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FString Text(TSharedRef<SWidget> W)
{
    FString S;if(W->GetType()==FName(TEXT("STextBlock")))S=StaticCastSharedRef<STextBlock>(W)->GetText().ToString();
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)S+=Text(C->GetChildAt(I));return S;
}
TSharedPtr<SWidget> Find(TSharedRef<SWidget> W,FName Type,const FString& Value,int32& Index)
{
    if(W->GetType()==Type&&Text(W)==Value&&Index--==0)return W;
    auto* C=W->GetChildren();for(int32 I=0;I<C->Num();++I)if(auto R=Find(C->GetChildAt(I),Type,Value,Index))return R;return nullptr;
}
struct FReopenClick
{
    TStrongObjectPtr<UCLPlanetRecipeAsset> Asset;
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile;
    TSharedPtr<const FCLCompiledPlanetRecipe> Terrain;
    FGuid Biome;int32 Stage=0;double Start=FPlatformTime::Seconds();TSharedPtr<SWindow> Window;
    bool Tick(float)
    {
        FString Error;
        if(FPlatformTime::Seconds()-Start>120){UE_LOG(LogTemp,Error,TEXT("ECOSYSTEM_REOPEN_UI_FAILED: stage %d"),Stage);return false;}
        if(Stage==0)
        {
            TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
            for(auto W:Windows){int32 I=0;if(auto C=Find(W,TEXT("SCheckBox"),TEXT("Ecosystem"),I))
            {Window=W;auto Box=StaticCastSharedPtr<SCheckBox>(C);if(!Box->IsChecked())Box->ToggleCheckedState();Stage=1;return true;}}return true;
        }
        if(Stage==1)
        {int32 I=0;auto C=Find(Window.ToSharedRef(),TEXT("SCheckBox"),TEXT("Biomes"),I);if(!C)return true;auto B=StaticCastSharedPtr<SCheckBox>(C);if(!B->IsChecked())B->ToggleCheckedState();Stage=2;return true;}
        if(Stage==2)
        {
            int32 Index=0;for(const auto& B:Profile->Ecosystem.Biomes){if(B.Id==Biome)break;++Index;}
            auto B=Find(Window.ToSharedRef(),TEXT("SButton"),TEXT("Solo"),Index);if(!B)return true;
            StaticCastSharedPtr<SButton>(B)->SimulateClick();Stage=3;
            UE_LOG(LogTemp,Display,TEXT("ECOSYSTEM_REOPEN_BIOME_SELECTED: %s | native Biomes Solo button"),*Biome.ToString());return true;
        }
        bool Pending=false;const auto Plan=CEGetCompiledBiomePlacement(Terrain,Profile.Get(),Pending,Error);if(Pending)return true;
        if(!Plan){UE_LOG(LogTemp,Error,TEXT("ECOSYSTEM_REOPEN_UI_FAILED: %s"),*Error);return false;}
        const FCEBiomePlacementCell* Best=nullptr;double BestDot=-2;
        for(const auto& C:Plan->Cells)if(C.bSupported&&C.BiomeId==Biome)
        {const double Dot=FVector::DotProduct(C.PositionMeters.GetSafeNormal(),C.Normal);if(Dot>BestDot){Best=&C;BestDot=Dot;}}
        if(!Best){UE_LOG(LogTemp,Error,TEXT("ECOSYSTEM_REOPEN_UI_FAILED: selected biome has no supported cells"));return false;}
        if(!CLVerifyPlanetPlayerMarker(Best->PositionMeters.GetSafeNormal(),Error))return true;
        UE_LOG(LogTemp,Display,TEXT("ECOSYSTEM_REOPEN_MARKER_CLICKED: %s | %s | native routed down/up; no deployment performed by qualification"),*Asset->GetPathName(),*Best->PositionMeters.GetSafeNormal().ToString());return false;
    }
};
FAutoConsoleCommand VerifyReopen(TEXT("ecosystem.VerifyReopenWalk"),TEXT("Qualification: <existing Planet asset> <biome GUID>; opens native UI, selects biome and clicks actual player marker."),FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
    if(Args.Num()!=2)return;auto R=MakeShared<FReopenClick>();R->Asset.Reset(LoadObject<UCLPlanetRecipeAsset>(nullptr,*Args[0]));if(!R->Asset||!FGuid::Parse(Args[1],R->Biome))return;
    const auto* Link=R->Asset->AuthoringExtensions.Find(TEXT("CatalystEcosystem"));R->Profile.Reset(Link?Cast<UCEPlanetEcosystemAsset>(Link->TryLoad()):nullptr);FString Error;
    if(!R->Profile||!(R->Terrain=FCLPlanetCompiler::Compile(R->Asset->Recipe,Error))||!GEditor)return;
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(R->Asset.Get());
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([R](float D){return R->Tick(D);}),.25f);
}));
}
#endif
