#include "Climate/CEPlanetWeatherInputs.h"

#if WITH_CE_PLACEMENT_LANDFORM
#include "Climate/CEBiomePlacement.h"
#include "Model/CLPlanetRecipe.h"
#include "Weather/CEPlanetWeather.h"
#include "Weather/CEWeatherTopology.h"

double CEPlanetSeaLevelMeters(const FCLCompiledPlanetRecipe& Plan)
{
    const auto* Ocean = Plan.Guides.FindByPredicate([](const auto& Guide) { return Guide.Name == TEXT("OceanHeight"); });
    return Ocean ? Ocean->HeightMeters - Plan.BaseHeightMeters : -(Plan.RadiusMeters + Plan.BaseHeightMeters);
}

bool CEBakePlanetWeatherGlobe(const FCLCompiledPlanetRecipe& Plan, const FCEBiomePlacementPlan& Placement,
    const FCEWeatherGlobeMesh& Mesh, const FCEPlanetEcosystem& Profile, FCEPlanetWeather& Field,
    TArray<FVector>& Positions, FString& Error, const TAtomic<bool>* Cancel)
{
    if (!Plan.SourceRecipe)
    {
        Error = TEXT("Weather requires an accepted planet recipe");
        return false;
    }
    FCEPlanetEcosystem Model = Profile;
    Model.Climate.SeaLevelMeters = CEPlanetSeaLevelMeters(Plan);
    Model.Weather.DaySeconds = Plan.SourceRecipe->RotationPeriodSeconds;

    TArray<double> Heights;
    Plan.SampleHeightsMeters(Mesh.Directions, Heights);
    Positions.Reset(Mesh.Directions.Num());
    TArray<FVector> Normals;
    Normals.SetNumZeroed(Mesh.Directions.Num());
    for (int32 Index = 0; Index < Mesh.Directions.Num(); ++Index)
        Positions.Add(Mesh.Directions[Index] * (Plan.RadiusMeters + Heights[Index]));
    for (int32 Index = 0; Index < Mesh.Indices.Num(); Index += 3)
    {
        const int32 A = Mesh.Indices[Index], B = Mesh.Indices[Index+1], C = Mesh.Indices[Index+2];
        const FVector Normal = FVector::CrossProduct(Positions[B] - Positions[A], Positions[C] - Positions[A]);
        Normals[A] += Normal;
        Normals[B] += Normal;
        Normals[C] += Normal;
    }

    TArray<FVector> CellDirections;
    for (const auto& Cell : Placement.Cells)
        CellDirections.Add(Cell.PositionMeters.GetSafeNormal());
    TArray<FGuid> Biomes;
    Biomes.Reserve(Positions.Num());
    for (int32 Index = 0; Index < Positions.Num(); ++Index)
    {
        if (Cancel && Cancel->Load())
            return false;
        const FVector Direction = Mesh.Directions[Index];
        Normals[Index] = Normals[Index].GetSafeNormal();
        if (FVector::DotProduct(Normals[Index], Direction) < 0)
            Normals[Index] *= -1;
        double Best = -2;
        FGuid Biome;
        for (int32 Cell = 0; Cell < CellDirections.Num(); ++Cell)
        {
            if (!Placement.Cells[Cell].bSupported)
                continue;
            const double Dot = FVector::DotProduct(CellDirections[Cell], Direction);
            if (Dot > Best)
            {
                Best = Dot;
                Biome = Placement.Cells[Cell].BiomeId;
            }
        }
        Biomes.Add(Biome);
    }
    return (!Cancel || !Cancel->Load()) && Field.Initialize(Model, Positions, Normals,
        Plan.RadiusMeters + Plan.BaseHeightMeters, Plan.SourceRecipe->Seed, Error, Biomes);
}
#endif
