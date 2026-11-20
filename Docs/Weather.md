# Planetary weather preview

Open a planet recipe, choose **Ecosystem → Weather**. Its designed globe starts
an animated pressure heatmap and synoptic overlay automatically. The panel keeps
one compact Weather row; layer buttons, overlay toggles, numeric readouts, timeline,
Play/Pause and speed controls are removed. The preview advances at 60 simulation
seconds per real second only while this page is active. Input edits restart its
local session; moving an unbound Overlay guide preserves it.

Pressure centres are labelled simply **H** or **L**. Every closed pressure contour
carries equally spaced arrowheads, following its curve without vector shafts.
One arc-length-weighted mean wind strength sets each loop's marching speed; the
mean wind projected along that loop selects its circulation direction. Calm wind
is stationary. This is a general flow indicator, not a pointwise wind vector.
The visual rate is radius × 0.002 × wind metres/second, independent of the weather
clock. A shared phase preserves spacing and is integrated across payload updates.
Arrowheads can therefore keep moving when an unchanged field is displayed.

The continuous heatmap sits just behind the contours. Its shell uses 18% opacity
with saturated colour ramps; pressure runs blue through green to amber without a
grey midpoint. No copied artwork or persistent material assets are needed.

## Fronts on the overlay

Weather also displays connected thermal fronts: blue triangles for advancing cold
air, red semicircles for advancing warm air, and alternating symbols on opposite
sides for nearly stationary boundaries. Symbols point across the boundary in the
advancing air's direction. These use the [NWS surface-front convention](https://www.wpc.ncep.noaa.gov/html/fntcodes2.shtml).
Pressure contours and their marching wind arrows remain visible alongside them.

The editor diagnoses ridges of temperature-gradient strength on the same closed
globe used by the field. Three neighbour-smoothing passes at each derivative stage suppress mesh noise;
least-squares tangent gradients locate the ridge, and shared edge identities join
its zero crossings into chains across poles and seams. Minimum thermal contrast,
ridge sharpness and chain length exclude uniform air, broad latitude climate and
short isolated fragments. The component of wind from cold toward warm determines
Cold/Warm (or Stationary within ±0.5 m/s). No random decoration, independent drift,
new simulation clock or domain evolution equation is added.

Front bands tint the existing transparent shell. Their width increases with
sampled cloud and rain, with stronger rain adding cyan. The same simulation-time
query produces the same geometry; current field updates move the fronts, while
history/forecast queries diagnose the boundary at the requested time. Moving
Overlay changes only their presentation radius. Disabling or hiding Weather
removes fronts with the rest of the overlay.

This is a surface-field diagnostic, not a calibrated synoptic analysis or a new
prognostic air-mass solver. It can include terrain-driven thermal boundaries; it
does not identify occlusions or independently transport moisture. The structured
front points retain cloud/rain and cross-front wind for sky-planning work, but
near-player cloud and precipitation renderers are not connected by this change.
At most 64 chains / 1024 points / 512 symbols are emitted; all lines share the
existing 16,384 segment budget. Bands reuse the heatmap topology and opacity.

This remains the local analytic simulation backend, with epoch zero and a warm-up
interval 0–3600. Backend history/forecast queries remain deterministic functions of
simulation time and unchanged inputs; their earlier controls are no longer on
this page. There is one coalescing worker per panel, with cancellation/join on
retirement. Previewing weather does not dirty or save authoring assets.

## Weather Character

Weather follows the same compact row as Biomes: enable checkbox, name/description,
Character (0–100), preview eye, reseed, Solo and an advanced disclosure. Underlying
weather/event settings are collapsed initially. Recalculation is silent; only actual
preview errors appear above the row. Dragging Character forms one Undo
transaction. Preview eye and Solo never alter the model or saved generation flag;
Solo temporarily overrides the eye and restores its previous visibility on exit.

Character runs from Calm (0) through Moderate (50, the default) to Volatile (100).
The GT model scales natural background strength by `0.35 + 1.3 × Character` and adds
smooth, independently seeded regional extreme windows. Their roll threshold falls
as Character rises (`1 − (0.01 + 0.34 × Character²)`), and their additional strength
rises to `4 × Character²`. The threshold controls relative likelihood, not a
calibrated percentage of real weather time. The noise roll changes smoothly over
three-hour intervals; it is independent of queries and viewer/frame count.

Increasing Character therefore increases both extreme-window frequency and
severity: stronger pressure gradients/wind, heavier moisture-supported rain/cloud,
and stronger dry-region dust. Terrain, ocean supply and biome climate remain the
same inputs. Character never creates rain on a dry planet. Natural background
wind retains the 80 m/s bound and rain 100 mm/hour; cloud/dust fractions are 0–1.
Advanced parameters remain authored multipliers, rather than being overwritten by
the knob. Explicit mission-authored events retain their authored intensity/path.

Unchecking Weather disables the row and its preview work. Domain queries still
return the static terrain climate, reference pressure and zero wind/rain/dust/
cloud, including authored weather. Character zero alone requests gentle natural
weather; it does not disable the system. Reseeding changes the deterministic
weather seed without changing the planet/biome seed.

## Model and ownership

Portable `CEPlanetWeatherModel` in CatalystEcosystem authors the equations through
Simulator's `CSFieldBuilder`. `Resources/Weather/planetary-weather-6.json` is its
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

Add or move the **Overlay** radial guide in Planet. Pressure heatmap, contours, front symbols, labels and wind anchors use that
core-relative radius. Temperature swatches follow the sampled composed ground, including height
variation, with the biome view's small display clearance. With no Overlay guide they use the
outer **sky boundary**, even if AtmosphereMax exists. New guide presets include
Overlay at AtmosphereMax, or sky for airless presets. Existing custom styles are
not silently rewritten. The heatmap is inset by 0.1% of shell radius to prevent
fighting with the pressure lines. Its altitude is presentation only: it shows
surface/reference-datum weather, not atmospheric conditions at that height.

The model has 2075 scalar nodes (generic cap 4096), eight background systems and
four authored slots. At most 16,384 combined pressure/front segments, 256 closed flow paths,
2048 arrowheads (at most 128 per loop) and 16 H/L labels are generated. Paths join
through shared edge/vertex identities and use spherical arc length, including the
closing edge. Atmospheric graphics share a payload bounded to 16,384 vertices /
98,304 indices. Ground temperature uses the same unlit square swatches as the
biome preview: exact sampled composed-surface positions, the same default pixel
size and hemisphere/depth culling. Swatch colours represent temperature directly;
sunlight, night fill and cast shadows do not alter them. The terrain beneath keeps
its standard day/night lighting and shadows. Shared camera-facing swatches use an fully opaque-alpha unlit translucent scene component, before
atmospheric translucency; camera changes rebuild at most 8192 squares. No
temperature-specific terrain material is generated. Pressure uses a blue–purple–orange palette, with no green midpoint.
Isobars use 1.875-pixel cores and marching heads use 2.7-pixel cores (1.5 times
their initial widths), each with a 0.5-pixel black rim per side. Analytic edge
coverage applies to both the core and rim and shares each arrow's fade. All
heavier black backing strokes are batched before any coloured isobar or arrow
cores. The outline surrounds their combined silhouette, with no black seams at
segment or arrow joins. Isobars,
arrows and filled front symbols use a depth-independent scene ink layer above unlit
ground swatches; the pressure heatmap stays in the scene behind these graphics.
Longitude, latitude, equator, pole markers/stems and N/S text use the background
geographic scene layer (sort priority -30), behind ground swatches (-20),
atmospheric heatmaps (0) and weather ink. N/S text is scene text, never a late
Canvas label. All these scene materials use the same pre-DOF translucency pass.
Ink uses standard world translucency with depth testing disabled. The native
translucent foreground depth-priority group did not draw these view meshes in
the Mac qualification; ordinary editor overlay points would cover scene ink.
Fronts use dark blue/red/purple strokes and filled triangles/semicircles with
feathered edges; cloud/rain bands do not tint the heatmap.
Weather evaluations coalesce to at most five per real second. Per-frame work advances persistent visual arrow sites and front opacity; it
does not evaluate weather. Arrows are approximately twice as widely spaced as
the original marching view. Replacing a contour projects existing sites onto its
new arc, independent of seam/traversal and arrow count. New sites enter the
largest available gaps at zero opacity; redundant sites retire over 0.6 seconds.
No count change globally redistributes sites. Same-pressure loops match spatially
rather than by extraction order. Front birth/retirement fades over 0.8 real seconds;
front presentation may republish at 20 Hz while fading, otherwise only on new data.
These are display transitions, not changes to simulation time or physical fields.

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

## Planet clock and fronts

Planet owns `RotationPeriodSeconds` (60–315,576,000 simulation seconds). The Planet
inspector's Rotation Speed (degrees/second) and Revolution time (hours per axial
turn) edit this single value, so they cannot drift. Existing recipes default to
24 hours. The embedded Weather inspector hides its standalone `DaySeconds` input:
the Planet period supplies that input to the unchanged GT-authored model.
The existing solar basis and phase drive both daily temperature forcing and the
preview's world-fixed standard directional sunlight. An opposed weaker night
light and ambient sky illumination keep the shadow hemisphere readable. Orbiting
the camera does not move this solar terminator. Other designer tabs retain the
studio rig. This first clock has no orbital seasons, axial obliquity controls or
thermal inertia integration; daily forcing is analytic and ocean-moderated.

Front diagnostics and their shared topology now belong to the runtime module:
`CEDiagnoseWeatherFronts` returns canonical connected boundaries, sampled weather
and continuous 0–1 ridge confidence in `FCEWeatherFrontPoint::Strength`. Strength
ramps across thermal-gradient and ridge-sharpness ranges rather than making the
old hard pass/fail cut. Short chains also ease from zero. Geometry and front-kind
labels are categorical diagnoses and can split/merge; game effects should query
the underlying temperature, wind, rain and strength, not treat a newly extracted
polyline as an instantaneous physical storm. Editor birth/death presentation is
separate and never feeds weather back into the simulation.

Real frontogenesis strengthens an existing air-mass temperature contrast; frontolysis
weakens it. A local frontal passage can be abrupt, but this does not imply a whole
front suddenly materializes. See the [NWS frontogenesis/frontolysis guide](https://www.weather.gov/media/zhu/ZHU_Training_Page/Miscellaneous/fronts/fronts.pdf).

## Travel multiplier

The advanced Weather settings expose **Weather travel multiplier**. It is a
continuous value: 1 retains the original natural travel, 0.5 halves it and 2
doubles it; 0 holds trajectories. The finite supported range is 0–100, represented
at four decimal places by the existing fixed-point interpreter. The Character
knob sets travel from 1 at Calm, through 2 at its midpoint, to 4 at Volatile.
Editing the advanced multiplier overrides that value until Character moves again.
New settings default to Character 0.5 and travel 2.

Only the natural regional trajectories and their wandering use scaled travel
time. Day/night, strength/pressure response, extreme forcing and authored mission
spline schedules keep simulation time. Moving to different terrain can naturally
change moisture, rain and local temperatures; the multiplier does not amplify
the weather channels. Marching wind arrowheads still represent local wind speed,
independently of the regional system's travel multiplier.

Designer edits anchor `travelTime = simulationTime * multiplier + phaseOffset`
at the accepted simulation second, retaining the position at that moment. The
anchor is persisted with the settings, including Undo/Redo and export. Character
and multiplier edits retain the preview clock. Predictions/history are deterministic
queries for the current input revision; they do not journal earlier authoring edits.
The runtime adapter maps the multiplier and anchor to uniforms 49/50 of the
GT-authored weather model; no second clock or weather solver is introduced.

## Rain and snow intensity

The default Weather view composites precipitation over the faint pressure tint,
on the same Overlay shell and behind isobars/front symbols. Dry precipitation is
transparent. Rain follows the reference radar colours: blue through purple and
pink to bright yellow at the strongest rates. Snow uses cyan, pale violet and
icy white; mixed precipitation blends both palettes. The small static legend
identifies the two colour scales. Presentation stops are 0.1, 1, 5, 15 and 40+
mm/hour of water equivalent, with continuous logarithmic colour interpolation
and a smooth opacity fade near zero. These are game display stops, not official
meteorological categories. No colour noise or precipitation is invented by the UI.

The v5 GT model partitions the existing total precipitation into liquid rain and
snowfall water equivalent. A smooth surface-temperature blend runs from all snow
at -1 C to all rain at +1 C. This is a lightweight phase approximation; vertical
air temperatures, wet-bulb temperature, melting/refreezing layers and snow depth
or accumulation are not modelled. Snow is not silently treated as snow-depth
mm/hour. Computing snow first and subtracting it from total precipitation retains
exact fixed-point water conservation. Temperature, pressure, wind, cloud, dust,
travel and event/solar timing retain their previous output values.

The nine output channels retain the earlier ordering and append `snowMmHour`;
`rainMmHour` now means liquid rain. The native adapter exposes both rates and
`PrecipitationMmHour()` for their sum. Front diagnostics use the total. The phase
split adds seven graph nodes and no timesteps. The overlay uses the existing
single triangle shell/material, with no extra draw layer, texture, mesh or render
asset. Guide movement, AA ink and ground-temperature layering stay on the shared
Planet Designer path.

## Frontal precipitation and the shared boundary field

Previously the native overlay diagnosed fronts after querying weather; the rain
model did not use those boundaries. The current pipeline couples both. The GT
background definition (`planetary-weather-6-background`) emits the previous nine
weather channels plus air-mass temperature. It removes the positive-land altitude
lapse for boundary detection; surface temperature still includes altitude cooling
and continues to determine rain/snow. Subsea bathymetry is not inverted into an
air-mass cooling term.

`CEWeatherFrontalField` supplies canonical spatial inputs: a baked six-neighbour
sample graph, tangent least-squares gradients and smoothed ridge derivatives.
This is view-independent scalar geometry, reusing the earlier diagnostic's
stencil work. The GT `frontalDefinition` owns front confidence thresholds,
moisture gating, cold/warm/stationary lifting, intensity, extra cloud/cooling,
precipitation cap and rain/snow split. The native adapter has no second set of
weather equations. Both programs execute through the unchanged generic Simulator.

Cold-front rain is stronger and concentrated near the thermal ridge. Warm-front
rain extends onto the colder side using a warmward probe. Stationary moist fronts
can sustain precipitation; dry fronts remain visible without manufactured rain.
Local/biome water supply and existing storm precipitation gate moisture. Terrain
normals still affect upslope enhancement. Existing regional/mission rainfall is
retained and total precipitation stays capped at 100 mm/h water equivalent.
These are game coefficients, not a calibrated atmospheric column/moisture solver.
They follow the broad lifting behaviour described by
[NWS front education](https://www.weather.gov/jkl/education) and
[NWS air masses/fronts](https://www.weather.gov/lmk/basic-fronts).

The two spatial probes (+/-0.12 radians) use fixed cached neighbourhoods and
continuous compact-support interpolation, avoiding changing-neighbour-rank jumps.
Their strength fades with the local thermal gradient. A front's diagnostic
confidence and its precipitation come from the same inputs; the displayed line
does not act as a rainfall emitter. Air-mass diagnostics are retained before the
additional precipitation cooling, preventing a self-reinforcing cooling/front loop.
Cloud fronts remain sharp symbols; no artificial cloud bands were reintroduced.

The background stage has 2091 nodes and the frontal stage 225. No fluid grid,
extra timesteps, clock or RNG stream was added. Uniform 51 is the climate lapse
rate; the current input vector contains 52 uniforms. The frontal program takes
30 spatial inputs and emits the nine final weather channels plus confidence and
frontal precipitation contribution. `FCEWeatherSample` and front points expose
the contribution for game queries. Forecast/history use the same immutable full
canonical sampling context; a camera-dependent subset is not a substitute for
that context. Arbitrary isolated point probes remain a separate production gate.
