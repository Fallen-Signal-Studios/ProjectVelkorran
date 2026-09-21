# Custom departure lounge and dock paving

The departure lounge and both scenic docks now use custom fitted basalt paving,
ivory perimeter courses and flush gold seams. The lounge has a large central
inlaid frame; the docks have paired longitudinal bands. This replaces 130 visible
stock tiles with one lounge mesh and two instances of a dock mesh. The original
gold route inlay remains. No canopy, shuttle, wall, seating or light was edited.

`audit_m13_departure_envelope.py` captured complete saved geometry in
`DepartureEnvelope-20260920-234813-7821ef26`. The lounge's native floor is
42 × 24 m with its top at world Z=0. Both dock bodies are 28 × 18 m, also topped
at Z=0, but their components deliberately use NoCollision and the scenic
graybox material. They are not walkable floor. The existing canopy covers only
the south portion of the open bay; this pass preserves that roof layout.

The full census is preserved with the editable blend, generator, FBX exports,
studio render and manifest in `Art/Source/Aurelion/Z12DeparturePaving`.
Run `build_z12_departure_paving.py` using Blender 4.5 with `--background
--factory-startup`. The lounge mesh has 61,112 triangles and the dock mesh
27,648. Both have two UV channels, Nanite at precision 10, full fallback geometry
and no collision. All surface geometry fits inside the native outlines and
original 4.624 cm decorative thickness, with paving tops at Z=0. Gold is
recessed 1 mm; tile joints are 8 mm. Insets use disjoint faces, not overlays.
Materials are the existing owned PavingBasalt, PavingIvory, Gold and Reveal.

`fit_m13_departure_paving.py` validates the original visible batches separately
from the pre-existing hidden 112-instance floor batch. It preserves the hidden
batch and other components exactly, every actor transform/collision setting,
the physical lounge floor and the non-blocking dock behavior. Nine lounge traces
must hit Z12_Floor at Z=0; eight dock traces must have no blocking hit.

The first preview `DeparturePavingPreview-20260920-235544-d788e7f6` stopped before
import because its verifier incorrectly expected a physical dock floor. The saved
native component census established that these are scenic NoCollision docks;
the corrected check explicitly verifies the expected non-hit instead.

`DeparturePavingFit-20260920-235838-0f332019` passed the corrected checks. The
lounge overview, paving detail and Selene background were inspected. Its dock
camera was too close to the shuttle for useful floor review. A higher view in
`DeparturePavingOverview-20260921-000135-48b29240` showed the dock outline and
paired bands around the shuttle. Both runs exited zero. No level was saved by
the previews.

`save_m13_departure_paving.py` requires the latter reviewed map hashes, mesh
hashes and poses. `DeparturePavingSaved-20260921-000452-0b013301` backed up M13,
saved only M13 and reloaded the level. It verified all three paving placements,
unchanged actor/collision state, unchanged hidden components, matching physical
and non-blocking traces and the protected M12 hash.

The initial live attempt `DeparturePavingLive-20260921-000718-92eefc29` stopped
before visual captures at the unchanged earned-position check. Selene was at
X=1319.791 cm rather than 1350, with zero sampled velocity, Y=48000 and Z=90.15.
This extends the pre-existing restoration movement evidence from
`SeleneBindingRepeat-20260920-232840-18d6e6b5`, captured before paving changes.
It remains a failed run; the tolerance was not loosened. No geometry or character
state was changed to obtain a subsequent attempt.

Fresh run `DeparturePavingLiveRepeat-20260921-001009-925618be` passed the unchanged
earned CP9 checks, including journal/evidence, identities, inventory/ammunition,
resources, settled positions and HUD/input state. The paving verifier also passed
exact mesh/pose readback and all 17 collision/non-hit checks in PIE. Its chained
wall and seating verifiers passed. All five frames were inspected: player,
lounge overview, paving detail, dock overview and Selene background. Selene's
skin was normal in this run. The temporary camera was removed, maps remained
unchanged and the run exited zero. Its `passed_requires_visual_review` status
is supplemented by this recorded review, not rewritten as an artistic-quality
or reliable-companion-restoration pass.

Validation baseline `20260920-234211-dbea61d3` passed before this work. Final
gate `20260921-001410-e2a4e17f` passed the full build without SkipBuild, all 726
matching automation tests (99 warnings), exact report coverage and unchanged
source integrity. Six changed Python scripts also parsed successfully. No
packaged build or GPU benchmark was run.

This is a local architecture improvement, not a claim that the full level is at
AAA quality or 90% TDD alignment. Canopy/shuttle refinement, moving-camera
quality and GPU cost remain separate work, as do the existing companion and
intermittent Selene shading issues.
