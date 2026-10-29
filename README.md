# Catalyst Ecosystem Plugin

CatalystEcosystemPlugin presents ecosystem simulations in Unreal Engine. The
workspace checkout is `Ecosystems/catalyst-ecosystem-ue-plugin`. Implementation is
planned; this initial repository is not a working or qualified UE plugin yet.

## Responsibility

- Own ecosystem prototype catalogs and every ecosystem presentation choice:
  meshes, textures, materials, skeletons, animation, appearance variants and
  ecosystem-specific visual/LOD policy.
- Interpret accepted species, life stage, injury, season and semantic action data.
- Provide ecosystem presentation adapters, foliage/weather integration, inspection
  and interaction UI over CatalystSimulatorPlugin's shared APIs.
- Own asynchronous content selection/loading and fallback policy, with teardown
  safe across world travel, provider removal and pooled-Actor reuse.

The plugin depends on
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
