# Weather qualification

## Rain/snow intensity — 21 November 2026

UE 5.8.4, macOS arm64, native Metal: 16 focused tests pass, with no automation
warnings, failures or skipped cases. All 14 GT weather tests pass. The v5 program
has 2087 nodes (seven added) and nine outputs. All 240 GT/native point cases match
exactly, including 115 cases with rain and 68 with snow. Against the previous v4
vectors, rain + snow conserves every fixed-point precipitation unit and all seven
other channels retain their exact values.

The phase fixture checks positive precipitation on an entirely warm and an
entirely cold planet, nonnegative rates and repeatable forecasts. GT checks
conservation and smooth phase progression through freezing, dry output, disable
and replay. The surface-temperature split is an inexpensive game approximation;
wet-bulb/vertical column physics and snow depth/accumulation remain outside scope.

The editor fixture publishes synthetic dry and rain/snow fields on the same
native shell, camera and lighting, with foreground isobars retained. Metal
readback identifies 284964 changed yellow rain pixels and 534606 changed icy-white
snow pixels. Palette tests retain dry transparency, continuous thresholds,
mixed-phase intensity, guide repositioning, mesh bounds and unchanged ground
swatches. Front regression compares matching precipitation inputs and confirms
fronts themselves introduce no cloud bands. The default Weather tab has a small
static Rain/Snow legend and continues to animate automatically.

GT source was compiled/exported through MCP into an independent checkout; both
class definitions and all 29 methods match the live image. The portable component
is `afff257`. Native runs retained one independent Runtime owner through lease
release, restored original registries and positively joined cleanup. The GT image
remains open. Saved user planet/profile/map hashes are unchanged by qualification.

Evidence: `Saved/WeatherPrecipitationQualification/index.json`, its matching log,
`Saved/EcosystemQualification/weather-precipitation-{dry,intensity}.png`,
`Saved/EcosystemQualification/weather-character-row.png`, and workspace
`recovery/weather-precip-20261121/native-2/`.

## Travel multiplier — 21 November 2026

UE 5.8.4, macOS arm64, native Metal: 14 focused tests pass with no automation
warnings, failures or skipped cases; 12 GT weather tests pass. The v4 program
contains 2080 nodes (five added), with 240 exact GT/native point cases across all
eight channels. All original 132 v3 reference cases retain their exact values at
travel multiplier 1. Existing analytic evaluation owns execution; no extra time
steps, clock or transport solver were added.

The advanced field accepts a fractional multiplier (0.5 is exercised through the
native numeric widget), preserves the travel position at the edit time, retains
the advancing rendered sun/day clock, and supports Undo/Redo. The compact row
contains no multiplier field. A native mouse interaction with Character increases
both Character and the travel multiplier, with Undo/Redo retained. Unit checks
verify the 1/2/4 Character curve, arbitrary advanced overrides, serialized anchors,
slower/stopped/faster trajectories and unchanged strength/thermal/event timing.
History/forecast queries remain reproducible for the current input revision.

GT model and test methods were compiled and exported through GT MCP, and every
exported method was compared with the live image in an independent checkout.
The portable component is published as `5576330`. Native validation used one
independent Runtime owner through MCP lease release and positive join; original
registries were restored. The live image remains open. User planet/profile/map
hashes match the saved pre-build authoring baseline.

Evidence: `Saved/WeatherTravelQualification/index.json`,
`Saved/Logs/WeatherTravelQualification.log`,
`Saved/EcosystemQualification/weather-character-advanced.png`,
`Saved/EcosystemQualification/weather-character-row.png`, and workspace
`recovery/weather-travel-20261121/native-6/`.

## Thermal front overlay

UE 5.8.4, macOS arm64, native Metal. Seven focused tests pass, zero warnings,
failures or not-run cases: all six Weather tests plus the actual Planet Designer
interaction test. The unchanged `planetary-weather-3` model still matches all
132 exported GT/native point cases. This slice changes only native diagnostics
and presentation; no GT source or model resource was changed or re-exported.

Fixtures verify uniform air and broad latitude climate produce no fronts, sharp
thermal boundaries connect, reversing wind swaps Cold/Warm, calm boundaries stay
Stationary, repeated queries preserve chain ordering and geometry, and a moved
thermal boundary is diagnosed at its new position. Front samples/tangents remain
canonical, all marker vertices follow the Overlay radius, cloud/rain broaden the
transparent band, invalid samples reject, and combined geometry respects bounds.
The pressure-only topology test separately retains its closed-contour assertions.

One default-model globe (2562 points, seed 17) gives these cold/warm/stationary
chain counts: 6/10/1 at 0 seconds, 4/7/4 at 3600, and 5/8/4 at 7200. A standalone
diagnosis followed by the complete overlay build (which diagnoses again) averaged
3.258 ms over those three queries on this host. This is one declared workload,
not a universal time guarantee. All added work runs on the existing coalescing
worker, with no added per-frame weather evaluation.

FirstPlanet was visually inspected with cold triangles, warm semicircles and
coloured transition bands alongside the H/L pressure overlay. Two captures three
real seconds apart show evolving fronts. The user's authored Character value and
1534.9 m Overlay guide were preserved; on-disk planet/profile/guide hashes were
unchanged by viewing. These are field diagnostics, not calibrated atmospheric
fronts or acceptance of near-player sky rendering.

Evidence: ThatGame58 `Saved/WeatherFrontQualification/`,
`Saved/Logs/WeatherFrontQualification.log`,
`Saved/EcosystemQualification/weather-fronts.txt`,
`Saved/EcosystemQualification/firstplanet-weather-fronts{,-later}.png` and workspace
`recovery/weather-fronts-20261116/native-2/`. The native lease reports released,
original registries restored; its independent Runtime owner was joined through
cleanup. The live GT image stayed open.

## Weather Character row

Current profile `planetary-weather-3`, using the unchanged generic
`analytic-field-1` interpreter. Nine GT tests pass. Verified GT exports contain
26 class/method definitions across three packages, all compared against the live
image through TonelReader. The graph has 2075 nodes; 132 native point cases match
all eight channels exactly, including Character endpoints and global disable.

The focused native suite passes 24 tests (23 clean, one expected existing missing-
art warning), zero failed/not run. It exercises the actual compact Weather row,
collapsed/expanded values, Character edit/Undo/Redo, reseed/Undo, generation disable,
disabled slider, preview eye, Solo restoring a hidden eye, re-enable and running
synoptic animation. Preview actions preserve authored weather values.

A controlled moist-planet scenario samples 128 directions at 96 quarter-hour
steps over one day. Calm/Moderate/Volatile mean winds are 3.881/11.120/26.898 m/s;
samples at or above 25 m/s number 0/990/5011, and rain at or above 10 mm/hour
0/614/2555, each out of 12288 samples. This demonstrates increasing frequency and
severity for the declared fixture, not calibrated meteorological probabilities.
GT extreme-envelope checks also compare four seeds over three days. Replay,
zero-moisture rainfall constraints and disabled static-climate outputs pass.

Evidence: ThatGame58 `Saved/WeatherCharacterQualification/`, its matching log,
`Saved/EcosystemQualification/weather-character-{row,advanced}.png`, and workspace
`recovery/weather-character-20261116/` (GT source parity and released/restored
native gate). Source was exported only through GT into an independent checkout.
The live image remains open. The existing legacy-import exclusion below remains.

## Marching contours and automatic view

UE 5.8.4, macOS arm64, native Metal. The focused suite passes 23 checks: 22 clean
and one with the expected existing missing-art warning, zero failed/not run.
Checks cover complete closed contour connectivity, every segment participating
in a flow loop, spherical arc spacing across closure, tangent sampling, doubled
wind doubling speed, reversed wind reversing motion, frame partition equivalence,
and stationary calm wind. Pressure labels contain only H or L.

The real Planet editor test confirms the removed controls/readouts are absent,
weather advances without clicking Play, and authoring dirty flags are preserved.
It then holds the complete rendered weather payload fixed while taking two native
screenshots: changing white pixels proves that arrowheads themselves move. Guide
movement, sky fallback and returning to the biome/climate overlay also pass.
Evidence is under ThatGame58 `Saved/MarchingWeatherQualification/`, its matching
log, and `Saved/EcosystemQualification/weather-flow-{a,b}.png` / `weather-running*.png`.
The unchanged eight-channel model still passes all 72 GT/native conformance cases.
Native work used one isolated owner and `CCNativeMcpLease`; the controller waited
outside GT for released state and restored registries, then joined the owner.

The existing legacy schema-1 / algorithm-1 import exclusion below is unchanged.
No new GT source or numeric weather profile was introduced by this presentation
change. Earlier layer/history/playback UI qualifications below describe the prior
interface; current backend history and forecast semantics remain as documented.

## Pressure and movable overlay shell (prior UI)

UE 5.8.4, macOS arm64, native Metal Development editor. The current
`planetary-weather-2` model adds common-datum pressure and derives bulk tangent
wind from its analytic pressure gradient, with hemisphere turning and surface
inflow/outflow. Authored local vortices remain additive. Six GT tests pass, and
72 native point cases match all eight channels exactly (1934 scalar nodes).
Three GT-exported packages were read back with TonelReader: all 22 class/method
definitions match the live image. Clean-image loading remains unqualified.

The closed 2562-vertex globe has no longitude/pole seam. Checks cover closed
isobars, tangent arrows, H/L labels, heatmap placement, and guide-bound terrain
invalidation. Real Slate controls exercise all four layers, history/forecast,
Play/Pause, guide movement without clock/value changes, and removal falling
back to the outer sky boundary. Screenshot comparison verifies rendered heatmap
pixels, supplemented by visual inspection of the native rendering.

The focused native run includes all 16 Ecosystem tests and six supported radial
guide tests: 21 clean passes, one pass with warning, zero failures/skips.
A 2048-point model evaluation took 15.603 ms and its bake 5.899 ms on this host;
the globe geometry/guide checks took 0.755 ms (one synthetic workload, not a
frame-time guarantee). Heatmap on/off changed 1,569,869 globe pixels; the front
hemisphere also passes coverage and colour checks. One expected warning belongs to the existing missing-art guard.
The broader `RadialGuides.AuthoringAndResize` test has a separate existing
schema-1 / algorithm-1 import rejection in the unchanged compiler/importer. Its
fixture and compatibility contract remain intact; that test is excluded from
this focused passing run. See Landform `Docs/PlanetBuilderOverlays.md`.

The heatmap is a retained native Dynamic Mesh, with no per-frame mesh rebuild.
Animated weather requests coalesce to at most 5 Hz; paused views reuse samples.
Moving an unbound Overlay guide changes presentation only; terrain or weather
input changes reset the session. Physical pressure remains a reference-datum
field, independent of display altitude. This is not a vertical atmosphere solver.

Evidence: ThatGame58 `Saved/PressureOverlayQualification/`,
`Saved/EcosystemQualification/weather-*.png`, and
`Saved/Logs/PressureOverlayQualification.log`; workspace source parity and
released/restored native lease markers under
`recovery/pressure-overlay-20261116/`. The live GT image stayed open.

## Initial weather slice (historical)

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
and detailed tornado effects/commands remain unqualified. The initial marker display is superseded by the continuous movable weather
shell above. Near-player weather rendering remains a separate production gate.
