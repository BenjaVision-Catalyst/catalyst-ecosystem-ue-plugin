#include "Model/CEPlanetEcosystem.h"
#if WITH_CE_LANDFORM
#include "CLPlanetInspection.h"
#include "CLPlanet.h"
#include "Climate/CEPlanetScenery.h"
#include "Climate/CEPlanetBiomePlacement.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "UObject/Package.h"
// Validate inputs only. Scenery is generated from the running world's state.
ECLInspectionPreparation CEPreparePlanetScenery(ACLPlanet& Planet,FString& Error)
{return CEValidatePlanetScenery(&Planet,Error)?ECLInspectionPreparation::Ready:ECLInspectionPreparation::Failed;}
void CECollectSceneryMaterials(ACLPlanet& Planet,TArray<UMaterialInterface*>& Materials)
{
    auto* P=CEPlanetEcosystemProfile(&Planet);if(!P)return;TArray<UObject*> Pending;TSet<UObject*> Visited;
    for(const auto& B:P->Ecosystem.Biomes)if(B.bEnabled)
    {if(auto* A=B.BiomeCoreAsset.TryLoad())Pending.Add(A);if(auto* A=B.DistantBiomeCoreAsset.TryLoad())Pending.Add(A);}
    while(!Pending.IsEmpty())
    {
        auto* A=Pending.Pop(EAllowShrinking::No);if(!A||Visited.Contains(A))continue;Visited.Add(A);
        if(auto* M=Cast<UMaterialInterface>(A)){Materials.AddUnique(M);continue;}
        if(A->IsA<UClass>()||A->IsA<UPackage>()||A->IsA<UWorld>()||A->IsA<UTexture>()||A->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn))continue;
        TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(A);
        Pending.Append(Refs);
    }
}
#endif
