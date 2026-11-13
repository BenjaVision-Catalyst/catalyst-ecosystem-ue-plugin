#pragma once
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
/** OceanHeight is an absolute body radius; climate uses metres above the neutral surface. */
double CEPlanetSeaLevelMeters(const FCLCompiledPlanetRecipe& Plan);
TSharedRef<ICLPlanetBuilderExtension> CECreatePlanetBuilder(FCLPlanetBuilderContext Context);
#endif
