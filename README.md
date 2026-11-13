# Catalyst Ecosystem Plugin

Catalyst Ecosystem adds deterministic planetary climate and biome authoring to
Unreal Engine. Its optional Landform Planet Builder section has **Climate** and
**Biomes** sub-tabs, with a standard Unreal ecosystem asset picker above them.
The plugin includes `Examples/DA_ExampleEcosystem` as the default for unlinked
planets. Choose an existing asset or create a new ecosystem from the picker.
A shared evaluator drives the biome coverage preview and a
native PCG node; the editor bridge builds climate-filtered Biome Core generators.

See [Climate and Biomes](Docs/ClimateAndBiomes.md) for model, integration and limits,
and [qualification](Docs/Qualification.md) for the exact engine/platform coverage.
The workspace checkout is `Ecosystems/catalyst-ecosystem-ue-plugin`.

Install alongside CatalystFoundation and PCG. Add CatalystLandform for the Planet
Builder integration, and enable PCGBiomeCore in the host for the generator bridge.
Restart the editor after compiling reflected native changes. The project must use
plugins built against its engine version; Landform's pinned 5.7 host and VPPTest58
are separate qualification targets.

## Planned simulation presentation responsibility

- Own ecosystem prototype catalogs and every ecosystem presentation choice:
  meshes, textures, materials, skeletons, animation, appearance variants and
  ecosystem-specific visual/LOD policy.
- Interpret accepted species, life stage, injury, season and semantic action data.
- Provide ecosystem presentation adapters, foliage/weather integration, inspection
  and interaction UI over CatalystSimulatorPlugin's shared APIs.
- Own asynchronous content selection/loading and fallback policy, with teardown
  safe across world travel, provider removal and pooled-Actor reuse.

The future simulation presentation adapter will depend on
[CatalystSimulatorPlugin](https://github.com/BenjaVision-Catalyst/catalyst-simulator-ue-plugin)
for the connection binding, accepted-state cache, stable identity, subscriptions,
Mass/instance machinery, interpolation, pooling and global budgets. It does not
duplicate those facilities or independently simulate authoritative animal behaviour.
That behaviour belongs to
[CatalystEcosystem](https://github.com/BenjaVision-Catalyst/catalyst-ecosystem).

## Experimental source mining

Existing GT and DevKit ecosystem work is experimental source material. Review
useful model/presentation ideas against the new component boundary. Do not
silently replace old plugins, copy experimental schedulers into UE, or externally
rewrite GT-managed Smalltalk. The [GT inventory](https://github.com/BenjaVisionPro/catalyst-workspace/blob/main/Simulation/GT-Prototype-Mining-Inventory.md)
records the initial live-image review; the
[UE research](https://github.com/BenjaVisionPro/catalyst-workspace/blob/main/Simulation/UE-Projection-Research-and-Design.md)
defines the Mass/instanced-rendering qualification path.

## License

[MIT](LICENSE).
