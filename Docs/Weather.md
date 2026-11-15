# Planetary weather preview

Open a planet recipe, choose **Ecosystem → Weather**. The existing designed globe
shows Temperature (°C), Wind (m/s), Rain (mm/hour) or Pressure (hPa).
A continuous translucent heatmap sits just behind pressure contours, H/L labels
and wind arrows anchored along the contours. Isobars, arrows and heatmap can be
shown independently.
Legends stay fixed while scrubbing. Last hour/Now/Next hour select views without
changing simulation time. Play advances the preview at the chosen rate; Advance
hour moves its frontier. Weather/terrain edits reset this preview and invalidate
old work. Moving the unbound Overlay guide preserves its weather and clock. Viewing weather does not dirty or save the planet/ecosystem assets.

This is the first local analytic simulation backend. Its preview epoch is zero;
it starts paused at 3600 seconds with the model-defined interval 0–3600 available.
That interval is analytic preview warm-up, not observed gameplay history. Forecast
means the same model at future simulation time **conditional on unchanged inputs**.
No dense time-stepped grid or frame archive is needed. There is one coalescing
worker per panel and bounded immutable terrain samples; teardown cancels and joins.

## Model and ownership

Portable `CEPlanetWeatherModel` in CatalystEcosystem authors the equations through
Simulator's `CSFieldBuilder`. `Resources/Weather/planetary-weather-2.json` is its
GT-generated immutable `analytic-field-1` program. The Simulator plugin executes
that generic profile. The native Ecosystem adapter supplies frozen terrain,
climate/biome inputs, seed, event definitions and channel/presentation mapping;
it contains no duplicate weather evolution equations.

Eight overlapping high/low pressure systems and latitude-dependent pressure bands
supply a shared pressure field and its analytic tangent gradient. Wind responds to
that gradient through continuous hemisphere-dependent turning and surface inflow /
outflow, capped at 80 m/s before explicit authored local vortex contributions.
There is no singular division by Coriolis strength at the equator. High-pressure
regions suppress broad rain/cloud contributions; lows favour them when moisture
is available. The reference pressure defaults to 1013.25 hPa and scales the
pressure anomalies; wind response uses gradients normalized to that reference.
These are game-oriented analytic approximations, not a calibrated balance solver.

Seeded smooth temporal forcing varies systems over hours. Upstream geography
supplies moisture/dust, oceans moderate heating, nearby ocean fetch adds moisture,
terrain normals alter windward rain/lee drying, and biome expectations gently
bias the baseline. Outputs are temperature, canonical wind, rain, dust, cloud and
pressure at a common reference datum. Pressure is not inferred from rendered wind
or changed by the overlay's display altitude. No claim of conserved fluid/moisture
transport, quantitative meteorological accuracy or calibrated natural tornado
frequency is made.

Reproduction requires the exact program bytes, fixed-point profile, baked input
rows and ordering, combined planet/weather seed, uniform/event data and time.
Terrain preprocessing uses Unreal doubles then freezes Q10000 values; identical
raw assets across unqualified platforms are not yet guaranteed identical bakes.
Queries and viewer count never consume mutable RNG or alter the model timeline.

## Authored events

Expand Weather Settings → Events (up to four). Each event has stable GUID,
start/duration in simulation seconds, speed, radius, moisture and 2–16 canonical
body-space path directions. GUID sorting fixes event-slot assignment. Directions
compile to a cubic arc-length path, with eased travel and ten-percent build-up /
twenty-percent decay. A broad parent rain/cloud/inflow envelope surrounds the
vortex/updraft. A multi-hour duration is supported. Disabled/expired events have
zero additional contribution. Editing an event explicitly resets the preview.

This initial data interface supports scheduled field events. It does not yet
provide spline gizmos, arbitrary authored speed curves/pauses, accepted gameplay
start/cancel commands, a funnel/debris/hazard renderer or separate parent IDs.
Those remain in the planetary design contract; they are not claimed by the globe.
Q10000 globe directions imply a minimum event support near 0.014 radians of
planet radius. Metre-scale tornado hazards on Earth-sized planets need a local
event-coordinate refinement before gameplay qualification.

## Bounds and next integration

The globe uses a closed 2562-vertex / 5120-triangle icosphere, with actual composed
terrain heights and area-weighted normals baked at its directions. The accepted
biome placement supplies nearest biome identities. The model and renderer share
these sample indices; no longitude seam or duplicated pole is introduced. Terrain
baking is bounded and cached per input generation.

Add or move the **Overlay** radial guide in Planet. Heatmap, contours, labels and
wind anchors use that core-relative radius. With no Overlay guide they use the
outer **sky boundary**, even if AtmosphereMax exists. New guide presets include
Overlay at AtmosphereMax, or sky for airless presets. Existing custom styles are
not silently rewritten. The heatmap is inset by 0.1% of shell radius to prevent
fighting with the pressure lines. Its altitude is presentation only: it shows
surface/reference-datum weather, not atmospheric conditions at that height.

The model has 1934 scalar nodes (generic cap 4096), eight background systems and
four authored slots. At most 16,384 contour segments, 256 contour-anchored wind
arrows and 16 H/L labels are generated. The native scene retains one mesh, updated
on data changes; it is not uploaded per render frame. Animated views coalesce to
at most five evaluations per real second; paused views do no simulation work.
Changing the selected layer reuses samples when the valid time is unchanged.

Only Overlay changes preserve the input generation. Other recipe changes rebuild
the bake. If terrain explicitly references Overlay, moving it also rebuilds;
the canonical Landform reference visitor detects that dependency. Climate/biome
views regain their own overlay when leaving Weather. VPP streaming, Shaki and
curvature presentation are not rewritten. All weather remains canonical.

Production Runtime/GemStone acceptance, durable input generations/history,
multiplayer replication and near-player precipitation/cloud/lightning rendering
are subsequent integrations. This editor preview is not a second gameplay
weather authority. Quantitative fixtures, source/export parity and native checks
are recorded in [Weather qualification](WeatherQualification.md).
