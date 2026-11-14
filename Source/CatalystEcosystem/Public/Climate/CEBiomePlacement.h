#pragma once
#include "Model/CEPlanetEcosystem.h"

/** Immutable planetary assignment. Climate is factual; habitat ranges are placement preferences. */
struct CATALYSTECOSYSTEM_API FCEBiomePlacementCell
{
    FVector PositionMeters=FVector::ZeroVector,Normal=FVector::ZAxisVector;
    bool bSupported=false;
    double SolidAngle=0;
    FGuid BiomeId;
};
struct CATALYSTECOSYSTEM_API FCEBiomePlacementRegion
{
    FGuid BiomeId;
    int32 SeedCell=INDEX_NONE,ReservedCells=0,Components=0;
    double ReservedSolidAngle=0,AssignedSolidAngle=0,MaximumHabitatPenalty=0;
};
struct CATALYSTECOSYSTEM_API FCEBiomePlacementPlan
{
    static constexpr int32 Resolution=32;
    static constexpr int32 CellCount=6*Resolution*Resolution;
    static constexpr int32 Profile=1;
    static constexpr double MinimumCoverage=.01;
    FCEPlanetEcosystem Model;
    double ReferenceRadiusMeters=0,SupportedSolidAngle=0;
    int32 PlanetSeed=0;
    FString AssignmentKey;
    TArray<FCEBiomePlacementCell> Cells;
    TArray<FCEBiomePlacementRegion> Regions;
    static FVector Direction(int32 Face,double U,double V);
    static int32 CellIndex(FVector Direction);
    static TArray<FCEBiomePlacementCell> MakeLattice();
    static TSharedPtr<const FCEBiomePlacementPlan> Build(const FCEPlanetEcosystem&,TArray<FCEBiomePlacementCell>,double Radius,int32 Seed,FString& Error);
    FCEClimateSample Evaluate(FVector BodyMeters,FVector BodyNormal)const;
};
