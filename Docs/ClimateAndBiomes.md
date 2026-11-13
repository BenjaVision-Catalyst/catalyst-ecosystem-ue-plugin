# Planet climate and biome authoring

Ecosystem adds one optional section to Landform's Planet Builder. That section has
**Climate** and **Biomes** sub-tabs, using the same compact label style as Planet.
Optional sections appear before the final Preview tab. Landform owns terrain and its native VPP
preview; Ecosystem owns climate, habitat classification and biome presentation.
Landform remains usable without Ecosystem.

## Saved model and editor session

`UCEPlanetEcosystemAsset` is a Foundation model asset with a versioned
`FCEPlanetEcosystem` payload. Create it explicitly in the Ecosystem section. The
planet retains a soft authoring link under `CatalystEcosystem`; opening a planet
alone does not create or dirty a profile. Saving the Planet Builder also saves its
linked profile. Undo/redo covers saved climate and biome edits.

Climate controls the pole axis, equatorial and polar temperatures, lapse rate,
moisture, subtropical drying and seeded spherical variation. In Planet, sea level
is a read-only **OceanHeight** guide value. The accepted terrain plan supplies its
absolute body radius; Ecosystem converts it to the neutral-surface offset used by
the shared evaluator. Guide edits update the preview without rewriting the stored
climate profile. A planet without OceanHeight is dry. Standalone evaluation outside
Planet retains the explicit profile water datum. Each biome has
a stable ID, description, colour,
generation checkbox, Character, seed, priority, habitat ranges and optional source
Biome Core asset. Character expands the temperature/moisture habitat tolerance by
up to 10 C / 0.2. Reseed changes the stable spatial tie-break between habitats of
equal priority. Higher priority wins first. Row order has no effect.

Eye and whole-planet Solo are session-only. They hide already classified preview
samples; they do not alter classification or PCG generation. Unsolo restores the
eye state from before Solo. Disabled biomes do not generate and their other row
controls are disabled. Biomes use the Landforms row style: disclosure, generation
checkbox, bold name and small description, Character/readout, eye and reseed icons,
and Solo/Unsolo. Expanded habitat/source settings remain owned by Ecosystem.
Up to 64 biomes are admitted; uncovered locations remain
unassigned instead of silently receiving a different habitat.

## Shared evaluator

Both preview and PCG call `FCEClimateEvaluator::Evaluate` with a validated model,
body-space position/normal, neutral reference radius in metres and planet seed.
There is no world-Z climate, global random stream, first-world lookup or live
simulation session. Latitude uses the explicit pole axis. Temperature interpolates
with squared sine of latitude and falls with positive height above sea level.
Moisture combines global moisture, a dry subtropical belt and continuous seeded
3D lattice noise. Ocean eligibility uses radial height below sea level, with a
one-micrometre tolerance to avoid classifying transform round-off as ocean.

The preview samples 2,048 directions on the **accepted Landform terrain** and
estimates surface normals from nearby height samples. One worker runs at a time;
edits coalesce behind it and revision checks discard stale results. Worker inputs
are immutable plain snapshots. Closing joins the worker after cancellation at a
256-query batch boundary. Landform renders the coloured surface markers in its
owned preview scene. These markers show habitat coverage; VPP still supplies the
terrain geometry. This is a bounded diagnostic preview, not a vegetation density
or final species distribution preview.

Repeatability is qualified on the recorded engine/platform. Cross-platform
bit-identical floating-point climate boundaries are not yet a supported promise.
This first climate model does not simulate atmospheric circulation, rain shadows,
distance to rivers/coasts, seasons, erosion or ecological population dynamics.

## VPP → climate → Biome Core

Enable `PCGBiomeCore` in the host to use the native bridge. Ecosystem itself can
load without that optional content plugin. Configure a bounded graph containing
exactly one native VPP **Voxel Sampler**, including its intended layer, sample
density, cell size and seed. Select that graph in Biomes, assign native Biome Core
BiomeAsset sources to the desired biome rows, then choose **Build Biome Core assets**.

The bridge preflights the source schemas and creates unique copies in an adjacent
`EcosystemGenerated` folder. Each enabled, assigned row receives:

1. A graph containing a copy of the configured VPP sampler, followed by
   **Catalyst Planet Climate**, filtered to that row's winning stable biome ID.
2. A native Biome Core generator definition with a type namespaced by profile path
   and biome ID, so separate planet profiles do not share a generator accidentally.
3. A copy of its source BiomeAsset with every mesh entry bound to that definition.

Assign the returned/generated BiomeAssets to the intended Biome Volume. Source
assets, VPP and engine/plugin content are untouched. Multiple generator graphs use
the same sampler settings and disjoint climate filters. The native source entries
retain their mesh, material, variation and spawning settings, including MegaPlants
Instanced Skinned Mesh assets.

`UCEBiomeCoreBridge::Build` also exposes this operation to editor Python/Blueprint.
It takes an explicit planet transform, neutral radius and seed. The Planet Builder
uses the asset's body frame (identity transform), `RadiusMeters + BaseHeightMeters`
and recipe seed. Set the climate node's transform to the actual planet's world
transform when deploying these graphs; positive uniform scale is supported. Rebuild
or update those explicit context values after changing planet radius/seed/frame
or OceanHeight. Planet builds set the PCG node water-datum override from the same
accepted OceanHeight used by the preview. These are generated context snapshots;
guide edits do not regenerate or rebind arbitrary world graphs automatically.
A source graph containing additional processing nodes is rejected rather than
silently discarding them. Generated assets are marked dirty for review/save;
repeated builds create new uniquely named copies, not replacements.

The PCG node emits the following metadata, preserving point transforms, seeds,
bounds, density, tags and inherited metadata:

| Attribute | Type | Meaning |
| --- | --- | --- |
| `CE_TemperatureC` | double | Local temperature |
| `CE_Moisture` | double | Moisture in [0,1] |
| `CE_AltitudeMeters` | double | Height above sea level |
| `CE_LatitudeDegrees` | double | Signed pole-relative latitude |
| `CE_SlopeDegrees` | double | Surface normal versus radial up |
| `CE_Water` | bool | Below sea level |
| `CE_BiomeId` | string | Winning GUID in digits format; empty if unassigned |
| `CE_BiomeWeight` | double | 1 for assigned, 0 for unassigned |

The PCG element executes on the game thread, rejects invalid profiles/frames and
batches over 65,536 points, and disables element caching so forced regeneration
observes profile edits. Climate edits do **not** automatically regenerate arbitrary
worlds: regenerate the explicitly configured Biome Core component after editing.
Streaming/tile budgets and automatic actor-bound invalidation remain a later step.

## Catalyst template and ownership

Runtime model/evaluator, runtime PCG, editor tools and editor automation tests live
in separate modules with CE prefixes and matching Public/Private directories.
Public dependencies follow public headers; editor dependencies stay in editor
modules. VPP/Biome Core headers are absent from the public runtime API. Native
Biome Core content is resolved only by the explicit bridge operation and checked
before use. Dynamic module reload is disabled for reflected layout safety.

Landform's generic `FCLPlanetBuilderExtensions` registry supplies explicit toolkit
context and owns each session. It has no Ecosystem dependency. The editor integration
compiles when the Landform editor module is present; standalone climate/PCG hosts
can omit Landform. Rebuild Ecosystem when adding/removing that compile-time capability.

Authoritative ecological behaviour remains in CatalystEcosystem; future accepted
state rendering will use CatalystSimulator's shared APIs. Local deterministic
planet authoring does not invent a Simulator connection or duplicate its lifecycle.
