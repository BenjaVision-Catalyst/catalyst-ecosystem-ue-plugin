# Climate and biome qualification — 13 November 2026

Platform: Mac arm64. This is the first local authoring/PCG implementation, not a
qualification of the future Simulator-backed ecological presentation adapter.

| Check | Result |
| --- | --- |
| UE 5.7.4 (CL 51494982), LandformExamples editor build | Passed |
| UE 5.7.4 native Ecosystem automation | 6 passed |
| Existing Landform authoring boundaries and inspector/variation undo | 2 passed |
| Native Planet Builder integration | Ecosystem is the sixth section; Climate/Biomes sub-tabs and 2,048 coloured globe coverage samples rendered |
| Whole-planet Solo | Polar-only coverage, Unsolo restores all 2,048 samples; leaving the section clears the overlay |
| Foundation model | Validation, stable IDs and JSON round-trip passed |
| UE 5.8.4 (CL 58902052), standalone editor/game targets | Passed |
| UE 5.8.4 standalone automation, without Landform installed | 4 passed |
| PCG and preview parity | Identical climate/classification for explicit translated, rotated and scaled planet frames; point seeds/density retained; profile edits observed on regeneration |
| Spherical climate | Arbitrary poles, radial altitude/lapse rate, water level, seam continuity, repeatability and biome row-order independence passed |
| Native VPP → climate → Biome Core → MegaPlants | 280 Beech in synthetic cold habitat; 726 Birch in synthetic warm habitat; all 8 mesh variants |
| Climate edit/restore | Warm poles produce 1,006 Birch; restore reproduces every original species/placement digest |

The vegetation fixture uses stock VPP development revision
`58909934d4b680910cf1edc5114eee3183ccd263`, the native Biome Core generator
interface and the locally acquired Beech/Birch Instanced Skinned Mesh assets. No
VPP source patches, vendor content changes or staging-library modifications were
made. Habitat thresholds are deliberately synthetic plumbing tests, not botanical
recommendations.

The native editor tests retain screenshots under
`Landform/Projects/LandformExamples/Saved/EcosystemQualification/` including
`planet-builder.png`, `climate.png`, `biomes.png` and `polar-solo.png`.
The machine-readable vegetation evidence is in the workspace's
`VPPTest58/conformance/ecosystem-mac-arm64.json`; detailed measurements remain in
`VPPTest58/Saved/EcosystemQualification/`. Licensed and derived fixture binaries
stay local.

## Reproduction

Stage the optional provider into a closed Landform host with the workspace's
`python3 Landform/Tools/stage-plugins.py --with-ecosystem`, then enable
CatalystEcosystem in that project and build using the host's pinned engine. Staging
replaces managed plugin copies and requires the existing pinned Connect native
artifacts, as before. Do not overwrite plugins in a running editor.

Run `Module Load CatalystEcosystemTests`, followed by
`Automation RunTests Catalyst.Ecosystem` in the editor console. The dedicated
editor test module loads only when requested. For automation, use
`-ExecCmds="Module Load CatalystEcosystemTests,Automation RunTests Catalyst.Ecosystem"`
and `-TestExit="Automation Test Queue Empty"`. The native toolkit test requires
rendering; standalone climate/PCG tests can use `-NullRHI`. Start automation on
`/Engine/Maps/Entry` to avoid unrelated showcase generation.

The workspace VPPTest58 README documents the native vegetation fixture setup and
collection scripts. A subsequent installation of the consolidated GitHub Landform
plugin now qualifies the current **Planet** editor on UE 5.8.4 as well. The native
Ecosystem panel, 2,048-sample globe overlay, climate/PCG parity and profile preview
undo checks pass in that host. Ecosystem registers after Planet's four built-in
sections; with Ecosystem disabled, only Planet, Geology, Landforms and Preview
appear. Landform has no Ecosystem dependency. The packaged Landform installation
check uses its own catalogue, guides and materials without private Lab assets.

This combined-editor check does not requalify the earlier saved vegetation
fixture as a scene authored in the current Planet editor. Its Beech/Birch counts
remain evidence for the VPP/Biome Core fixture described above. Landform's
`Docs/LandformDeposits.md` defines the optional future biome-cover handoff; the
current classifier/PCG adapter does not implement that material stage.

## Remaining limits

- Coverage is a bounded coloured sample overlay, not a continuous biome material
  or final vegetation-density preview. The underlying native terrain is retained.
- Profile edits require explicit regeneration of the configured Biome Core
  components. Radius, seed and frame are explicit graph context; update them after
  changing the planet context. Generated assets are reviewable copies.
- Climate sea level is an explicit offset from the neutral surface; synchronization
  with Landform's OceanHeight guide is manual.
- Generator types are namespaced by profile path and biome ID. Full multi-planet
  Biome Core world behavior and streamed tile/global budgets remain unqualified.
- Packaged runtime generation, seasonal/climate simulation, planetary wind and
  migration of the packs' deprecated UE5.7 PVE preset loaders remain unqualified.
- The existing VPP UE5.8 Nanite `ExistingShadingPipeline.ComputeShader` ensure still
  occurs on fresh rendering. It is not repaired or suppressed by Ecosystem.

During validation, UE5.7 UBA compiler startup stalled in `libUbaDetours` rather than
compiling C++; the successful build used `-NoUBA` and the local parallel executor.
Builds for different engine versions must be serialized because these local
installations share a Marketplace rules-assembly cache.
