# Planetary weather preview

Open a planet recipe, choose **Ecosystem → Weather**. The existing designed globe
shows Temperature (°C), Wind (m/s with direction arrows), or Rain (mm/hour).
Legends stay fixed while scrubbing. Last hour/Now/Next hour select views without
changing simulation time. Play advances the preview at the chosen rate; Advance
hour moves its frontier. Authoring edits reset this preview and invalidate old
work. Viewing weather does not dirty or save the planet/ecosystem assets.

This is the first local analytic simulation backend. Its preview epoch is zero;
it starts paused at 3600 seconds with the model-defined interval 0–3600 available.
That interval is analytic preview warm-up, not observed gameplay history. Forecast
means the same model at future simulation time **conditional on unchanged inputs**.
No dense time-stepped grid or frame archive is needed. There is one coalescing
worker per panel and bounded immutable terrain samples; teardown cancels and joins.

## Model and ownership

Portable `CEPlanetWeatherModel` in CatalystEcosystem authors the equations through
Simulator's `CSFieldBuilder`. `Resources/Weather/planetary-weather-1.json` is its
GT-generated immutable `analytic-field-1` program. The Simulator plugin executes
that generic profile. The native Ecosystem adapter supplies frozen terrain,
climate/biome inputs, seed, event definitions and channel/presentation mapping;
it contains no duplicate weather evolution equations.

Eight overlapping regional systems share latitude-dependent planetary circulation.
Seeded smooth temporal forcing varies them over hours. Upstream geography supplies
moisture/dust, oceans moderate heating, nearby ocean fetch adds moisture, terrain
normals alter windward rain/lee drying, and biome expectations gently bias the
baseline. Outputs are temperature, canonical wind, rain, dust and cloud fraction.
Rain washes dust out; dry supply cannot sustain rain. This is a deliberately
cheap coherent approximation, not a conserved fluid/moisture transport solver.
No claim of quantitative meteorological accuracy or qualified natural tornado
frequency is made. Further realism should follow controlled experience tests.

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

The globe samples roughly 2048 accepted terrain cells (hard ceiling 8192), one
immutable bake and one current overlay per panel. Generic DAG limit is 4096 nodes;
the current model has 1917. Wind arrows are bounded to 512 and back-face culled.
Climate/biome views regain their own overlay when leaving Weather. The installed
planets, VPP streaming, Shaki and curvature presentation are not rewritten.
Weather values remain in canonical planet coordinates.

Production Runtime/GemStone acceptance, durable input generations/history,
multiplayer replication and near-player precipitation/cloud/lightning rendering
are subsequent integrations. This editor preview is not a second gameplay
weather authority. Quantitative fixtures, source/export parity and native checks
are recorded in [Weather qualification](WeatherQualification.md).
