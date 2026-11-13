#include "Misc/AutomationTest.h"
#include "PlanetBuilder/CEBiomePreviewState.h"
#include "Model/CEPlanetEcosystem.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCEPreviewStateTest,"Catalyst.Ecosystem.Editor.PreviewVisibilityAndUndo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCEPreviewStateTest::RunTest(const FString&)
{
    FCEBiomePreviewState HiddenSolo;const FGuid HiddenId(8,7,6,5);HiddenSolo.SetEye(HiddenId,false);HiddenSolo.SetSolo(HiddenId);TestTrue(TEXT("Solo temporarily reveals a hidden biome"),HiddenSolo.Visible(HiddenId));HiddenSolo.SetSolo(FGuid());TestFalse(TEXT("Unsolo restores the hidden biome"),HiddenSolo.Visible(HiddenId));
    FCEBiomePreviewState State;const FGuid A(1,2,3,4),B(2,3,4,5);
    State.SetEye(A,false);State.SetSolo(B);TestFalse(TEXT("Solo hides other biomes"),State.Visible(A));TestTrue(TEXT("Solo biome remains visible"),State.Visible(B));
    State.SetEye(A,true);State.SetSolo(A);State.SetSolo(FGuid());TestFalse(TEXT("Unsolo restores prior eye state"),State.Visible(A));TestTrue(TEXT("Other original eye restored"),State.Visible(B));
    TStrongObjectPtr<UCEPlanetEcosystemAsset> Profile(NewObject<UCEPlanetEcosystemAsset>(GetTransientPackage(),NAME_None,RF_Transactional));
    const auto Before=Profile->Ecosystem.Climate.Seed;
    {const FScopedTransaction Tx(NSLOCTEXT("CE","TestUndo","Ecosystem test edit"));Profile->Modify();Profile->Ecosystem.Climate.Seed=22;Profile->PostEditChange();}
    TestEqual(TEXT("Climate edit accepted"),Profile->Ecosystem.Climate.Seed,22);GEditor->UndoTransaction();TestEqual(TEXT("Undo restores climate"),Profile->Ecosystem.Climate.Seed,Before);GEditor->RedoTransaction();TestEqual(TEXT("Redo restores climate edit"),Profile->Ecosystem.Climate.Seed,22);
    return true;
}
