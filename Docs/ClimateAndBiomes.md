# Planet climate and biome authoring

Ecosystem adds one optional section to Landform's Planet Builder. That section has
**Climate**, **Biomes** and **Weather** sub-tabs, using the same compact label style as Planet.
Optional sections appear before the final Preview tab. Landform owns terrain and its native VPP
preview; Ecosystem owns climate, habitat classification and biome presentation.
Landform remains usable without Ecosystem.

## Saved model and editor session

`UCEPlanetEcosystemAsset` is a Foundation model asset with a versioned
`FCEPlanetEcosystem` payload. Its native asset definition derives from Foundation's
`UCFAssetDefinition_Model`, providing the Catalyst Content Browser category and
model appearance, just like the Planet asset. A native factory supports creating
a reusable ecosystem in Content Browser or the picker. A standard Unreal asset selector sits above Climate
and Biomes. It lists existing ecosystem assets and offers **Create New Asset →
Ecosystem**. New ecosystems start with the packaged example's climate, nine
biomes and sampler reference; creating one selects it in the planet. The selector
also supports Content Browser selection, browsing and drag/drop.

A planet without an explicit link displays
`/CatalystEcosystem/Examples/DA_ExampleEcosystem`, the example shipped in this
plugin. Opening a planet alone does not create or dirty an asset. An existing
link is preserved, including an unresolved link so it can be repaired through the
picker. Explicit selection records a soft authoring link under
`CatalystEcosystem`, with undo/redo support. Both sub-tabs edit the selected asset;
a shared ecosystem is shared data. Create a new ecosystem to author a separate
variant. Saving the Planet Builder saves its resolved ecosystem as well as the
planet. Climate and biome edits retain their own undo/redo transactions.

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
controls are disabled. Biomes use the Provinces > Types row style: disclosure, generation
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
density, cell size and seed. This is currently a developer API workflow through
`UCEBiomeCoreBridge::Build`, not a Planet Builder action. The Biomes page contains
the existing biome rows and their editing/preview controls. Custom biome creation,
the sampler selector, asset-build button and sample-count footer are deferred or
removed from this authoring page; progress and errors remain visible when needed.
The diagnostic point preview continues to evaluate automatically.

For a developer build, set the ecosystem's `SurfaceSamplerGraph` reference,
assign native Biome Core BiomeAsset sources to the desired biome definitions,
and invoke the bridge with explicit context.

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
It takes an explicit planet transform, neutral radius and seed. Supply the actual
planet's world transform, `RadiusMeters + BaseHeightMeters`, recipe seed and the
accepted OceanHeight override. Positive uniform scale is supported. Rebuild
or update those explicit context values after changing planet radius/seed/frame
or OceanHeight. Developer builds must supply the PCG node water-datum override from the same
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
observes profile edits. Bound native runtime execution sources request refresh through
Unreal PCG after ecosystem edits. VPP tracks sampled terrain dependencies.
Ecosystems does not schedule or regenerate partition cells itself. Standalone PCG
worlds require an explicit runtime source/binding for the same automatic refresh.

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


## Planetary coverage queries

`UCEClimateQueryLibrary::EvaluateSurface` exposes local climate/habitat suitability,
without a whole-planet terrain context. Planet-bound maps and placement use
`UCEPlanetBiomePlacementLibrary::EvaluatePlanetSurface` instead. Supply body-space metre positions, surface normals, the
accepted reference radius, Planet seed and OceanHeight datum. It returns winning
biome IDs without modifying the profile; batches are bounded to 65,536 points.
Use the planet-bound assignment query for appearance-mask baking and placement
checks; the local suitability query cannot guarantee representation. Physical
ground-cover/deposit generation is still a separate contract.

The PCG filter accepts multiple data sets, each bounded to 65,536 points, with an
aggregate two-million-point execution limit. This permits tiled planetary surface
samplers without removing the memory guard. It validates the complete input
before publishing results. Five native climate/PCG tests pass, including native
query/preview parity and an 80,000-point two-batch execution. The explicit planetary adapter below supplies shared assignment and invalidation.


## Accepted surface context in PCG

Surface point batches may provide `Planet_ToWorld` (Transform),
`Planet_ReferenceRadiusMeters` (double), `Planet_Seed` (int32) and
`Planet_SeaLevelMeters` (double) as typed default metadata. The climate node uses
this complete, consistent accepted context instead of its graph-authoring
snapshot. This keeps reference radius, frame, seed and OceanHeight current after
a terrain provider regenerates its regions. Invalid, partial or mixed contexts
are rejected. Standalone inputs without this metadata retain explicit graph
settings. Standalone metadata consumption rewrites neither profile nor graph. The optional
planetary adapter compiles when Landform is present; it resolves the region's
explicit Planet and the linked ecosystem, including after selecting another
profile. Native PCG owns generation and VPP tracks terrain dependencies.


## Enabled biome representation

An enabled biome receives at least **1% of supported spherical surface footprint**,
measured by exact cube-sphere solid angles. This is a minimum, not a cap. Existing
natural winners retain the remainder. Temperature, moisture, altitude, slope and
water are preferences; the planner grows a connected reserve from the best
available habitat, reserving the most constrained habitats first. It widens
placement through increasing habitat penalty when necessary. Water preference
is relaxed last. Climate, sea level, terrain and habitat authoring values remain
unchanged. Disabled rows receive no cells. Stable biome GUIDs and seeds settle
ties independently of row order or preview eye/Solo state.

The bounded assignment has 6 × 32 × 32 cells. Supported cell centres and normals
come from Landform's actual immutable cave-aware ground sampler. Connected growth
crosses cube-face seams. If unreserved support is fragmented into islands, a reserve may
need multiple connected components; the report records this explicitly. No
supported surface, invalid inputs or insufficient sampled ground produce an
error rather than a false placement guarantee. The footprint is angular area,
not a surveyed measure of additional cave-wall or cliff area.

One immutable `FCEBiomePlacementPlan` supplies the Biomes overlay, planet-bound
PCG filter, appearance query and placement checks. It carries an assignment digest;
PCG outputs `CE_PlacementKey` alongside the existing factual climate fields.
`GetPlacementReport` reports reserved/final fractions, connected-component counts
and habitat relaxation. The editor and runtime adapters key plans by the accepted
compiled source recipe and ecosystem payload, not asset names alone. Pure ground
queries/assignment run off-thread; UObject access/publication stays on the main
thread. The cache holds at most 32 snapshots and joins outstanding work on shutdown.

Ecosystems requests native runtime PCG refresh when its linked profile or model
contents change. VPP independently tracks terrain-query dependencies. The runtime
PCG filter resolves the original source's explicit Planet and uses the same
assignment as the Biomes map. Eye/Solo only controls visualization.

The guarantee is biome assignment. A row needs a Biome Core asset to render its
own foliage. Unconfigured biomes retain their map regions and cannot invent art
assets. This does not add a terrain material provider or automatic forest-floor
texture baking; consumers of baked masks must use the new assignment query.

Region ownership is explicit: a linked ecosystem or a previous climate-node
publication admits the consumer. Merely having the packaged default available for
preview does not attach Ecosystem state to unlinked generic surface graphs.

## Native runtime deployment and reopening

An ecosystem persists native deployment references on the same asset:
`RegionActorClass`, `RegionDefinition`, `LocalBiomeCache`, `RuntimeGraph` and
optional `WindTransformProvider`. Rows reference `BiomeCoreAsset`, optional
`DetailBiomeCoreAsset` and `DistantBiomeCoreAsset`. The detail rule asset omits
canopy when it is supplied by the distant rule asset; these are reusable asset
rules, never generated transforms or a saved vegetation world.

The inspection provider validates these references and collects material inputs.
It does not generate editor regions. In the running world, Ecosystems installs
native partitioned PCG execution sources and a native PCGWorldActor if required.
The configured runtime graph reuses Epic Biome Core's generators and spawners;
its preview-only spawning gate is removed. Local Preview is false. The native
PCG runtime scheduler owns cells, radii, generation, cleanup and pooling. Default
near detail uses 64 m cells within 150 m; canopy uses 512 m cells within 1,500 m.
No custom spherical tile queue or player-distance scheduling loop remains.

Generator graphs use VPP's native volume sampler with layer-change tracking.
Classifier context resolves through native GetOriginalComponent, so partition
actors use the accepted Planet frame, reference radius, seed, OceanHeight and
linked ecosystem rather than stale graph-authoring values. VPP normals determine
support slopes; inverted/steep faces cannot anchor upright surface vegetation.

Optional `GroundCoverMaskParameter` binds a transient runtime coverage texture.
Its assignment channel uses the shared map; its density channel follows currently
generated canopy. Native product completion/cleanup invalidates that derived
appearance. Textures and generated instances are not saved. This is a render
appearance input, not a physical deposit or finite ground resource.

A map-only ecosystem requires no invented art. Configured content with missing
references fails explicitly. Durable graphs, definitions, rule assets and
Planet/profile references must be saved in the private project. Generated output,
a recovery map and Python setup scripts are not reopening dependencies.

`GetSceneryReport` exposes native sources and runtime-managed partition components
for qualification. Rendered acceptance must cover fresh reopening, native player
marker input, generation during movement, distant canopy and live terrain edits.
Historical 96-region snapshot results do not qualify this runtime implementation.

## Weather tab

Weather belongs to the same Ecosystem section and the globe is the planet currently
open in Planet Designer. It uses that toolkit's explicit planet context and its
existing terrain preview; no second planet, world lookup or preview scene is
created. Selecting Weather clears the biome overlay and fences/cancels pending
biome work. Returning to Climate or Biomes restores its normal preview.

The current Weather page identifies the planet and shows Temperature (°C), Wind
(m/s), Rain (mm/h) and Last hour / Now / Next hour in simulation time. These controls
are disabled with an explicit unavailable-data message until the deterministic
weather model and its data binding are implemented. This UI shell does not
fabricate weather, advance a clock or modify saved planet/ecosystem values.
