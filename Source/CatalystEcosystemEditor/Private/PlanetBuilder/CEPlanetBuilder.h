#pragma once
#if WITH_CE_LANDFORM
#include "PlanetBuilder/CLPlanetBuilderExtensions.h"
TSharedRef<ICLPlanetBuilderExtension> CECreatePlanetBuilder(FCLPlanetBuilderContext Context);
#endif
