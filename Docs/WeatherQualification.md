# Weather qualification — 16 November 2026

Initial target: UE 5.8.4, macOS arm64, Development editor. Native Metal rendering,
not a null-RHI UI inspection. The same immutable model is interpreted in live GT
and native Unreal. All shared source was compiled/exported through GT MCP into
independent clean checkouts, then each class/method definition was read back with
TonelReader and compared to the live image (five packages, 61 definitions).
Clean-image loading and GemStone execution have not been qualified in this slice.

- Three GT tests pass: seed/replay/forecast equivalence, dry/moist supply, and
  authored parent-storm/vortex lifecycle.
- 72 native point cases match all seven GT channels exactly, including seed high
  bits, history/next-hour/24-hour times and authored event envelopes.
- Native cubic arc-length samples match the GT reference exactly.
- Fifteen Ecosystem automation tests pass (14 clean, one expected warning from
  the existing missing-art deployment-guard test). No failures.
- The real Planet editor test changes Temperature/Wind/Rain, Last hour/Now/Next
  hour, checks Now after scrubbing, advances the live frontier to compare against
  its earlier forecast, and plays/pauses simulation time. Preview preserves asset
  dirty flags, restores biome overlays, and closes its worker-owning toolkit.
- A 2048-point field evaluation took 15.624 ms; one terrain/climate bake took
  6.305 ms on this host. These are one measured synthetic workload, not a universal
  frame-time guarantee. Eight regional systems and four event slots compile to
  1917 scalar nodes. Queries evaluate requested time directly rather than stepping
  an hour. The UI runs this work asynchronously, never every render frame while
  paused. Bounded coarse ocean fetch is baked once per input generation.
- Invalid missing-array/fractional-op programs reject. Unused curve slots are
  valid zeros; authored invalid/core-crossing paths reject. Native input budgets
  and monotonic logical time are checked.

GT/native runs used `CCNativeMcpLease` with one independent analytic runtime owner.
The controller waited outside GT for `gt-mcp-gate.json` with released state and
restored original registries before joining. The live GT image stayed open.

Portable source revisions: Simulator `3b9b306`, Ecosystem `9d3772d`. Model bytes
and reference vectors are retained in `Resources/Weather/Conformance/`; the main
weather resource is byte-identical to the conformance program.

Local retained evidence in ThatGame58: `Saved/WeatherSimulationQualification/`,
`Saved/WeatherVisualQualification/`, `Saved/EcosystemQualification/weather-*.png`,
and the corresponding `Saved/Logs/` logs. Workspace recovery/export parity and
lease markers are under `recovery/weather-20261116/` (not source artifacts).

## Open experience and production gates

Quantitative windward/lee tests under reversed wind, conserved long-distance
moisture/dust transport, seam/pole continuity tolerances, 24-hour experience
review and calibrated rare natural extremes remain open. Numerical repetition
and a changing globe do not establish those meteorological claims. See the
workspace planetary design for full acceptance criteria.

Production Runtime/GemStone durability, history across terrain edits, multiplayer,
near-player weather and curvature-rendered precipitation/lightning, point probes,
and detailed tornado effects/commands remain unqualified. The globe currently
shows sampled colour markers and arrows, not a continuous weather material.
