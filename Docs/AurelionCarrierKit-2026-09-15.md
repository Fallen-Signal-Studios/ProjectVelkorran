# Custom Aurelion rescue carrier

The custom Blender kit contains a faceted hull, radiator-lined nacelle and glazed
bridge. Ceramic armour, narrow gold conductors, docking shutters and thrust irises
replace the tiled-floor construction of the former carrier. This is an environment
art pass, not a final AAA quality or campaign completion claim.

Source: `Art/Source/Aurelion/CarrierKit/Aurelion-Rescue-Carrier.blend` and
`build_carrier_kit.py`. Three FBX meshes have centred pivots, metre dimensions,
two UV channels, three existing Aurelion material slots and no collision hulls.
The first round-trip report recorded 71,212 hull, 64,004 nacelle and 4,868 bridge triangles.
These are source geometry counts, not measured runtime costs. Same-facing
axis-aligned face audits report zero overlaps; that audit does not cover every
possible oblique or inter-object intersection.

## Placement repair

The eight existing carrier actors retain their identities, transforms, attachments
and journal-controlled visibility/collision flags. Their hidden native cube
components remain unchanged. Each art HISM receives one unit-scale custom mesh,
replacing 2,528 former tiles across the two presentations. Visual components use
NoCollision and cannot contribute to navigation.

The suspended hull was banked while its engines and bridge retained level world
poses. The new art poses compose each part's local offset with the hull's bank,
so the visual assembly stays together. The actor targets used by the rescue
cinematic are preserved. Checks compare each mesh to its original nominal size
at the corrected pose and require the entire visual footprint to remain east of
X=7,800 cm. The corrected bridge necessarily extends below its old unbanked world
box; both envelopes are recorded, rather than claiming unchanged world bounds.

The noncolliding vendor building `Aurelion_Radiance_AtriumEastRelatedForm` sat
immediately above the rescued hull and floated above the suspended hull. Its
component is hidden as redundant scenery. Its actor, mesh reference and transform
are retained. The unrelated surrounding pillars and sky assets are preserved.

## Review evidence and limits

- `CarrierArtAudit-20260915-022447-18c7055f`: fresh map inventory and three baseline
  views; eight actors and 3,140 total map actors.
- `CarrierKitPreview-20260915-023034-4761d33e`: initial static fit passed. Both
  presentations appeared together in the editor, so these images cannot qualify
  either gameplay state individually.
- `CarrierKitPhases-20260915-023434-22734a61`: isolated editor presentations exposed
  the disconnected banked parts and the legacy spire. Temporary component visibility
  was restored after capture; no journal receipt was supplied.
- `CarrierKitBanked-20260915-023846-647a5e27`: stopped on the old unbanked-envelope
  assumption for the bridge. No map saved. The checker was corrected to use the
  authored banked pose and original nominal dimensions.
- `CarrierKitBankedFit-20260915-024119-debb821c`: corrected art fit passed; four
  isolated phase images inspected, no Python errors and exit 0. The preview
  restored its temporary visibility and screenshot settings with 3,140 actors.
- `CarrierKitSaved-20260915-024559-3c8b34d2`: backed up and saved M12, repeated
  the fit checks, captured both poses and reviewed saved-version wide/forward
  images; no Python errors and exit 0. Actor count remains 3,140.
- `CarrierKitFreshPIE-20260915-024951-57eb1671`: all 87 saved-map architecture
  reports passed. Fresh native PIE passed pre-meeting carrier visibility and
  corrected art-pose checks for all eight parts, three complete navigation paths
  and six unobstructed actual-player-capsule sweeps. The probe left the player
  position, journal and map file unchanged. Exit 0, no Python errors.

Exterior lighting still crushes much of the carrier's side detail into shadow.
The material finish, breakup of the broad roof surfaces, rescue transition in
live play and performance need further qualification. Structural carrier scenery
is not a candidate for the separately requested selective cover destruction.

`validate_carrier_kit_pie.py` runs the preceding architecture checks and a fresh
native PIE boot, then inspects real pre-meeting visibility, art poses, navigation
paths and player-capsule clearance. It does not move the player or grant campaign
events. The recorded pass qualifies only the pre-meeting state, not the rescued
transition or full route. No alignment percentage increase is claimed for this pass.

## Visual refinement

The next hull revision adds inset service lids, intake slots and a low tapered
dorsal machinery house. The first uniform grid of lids was rejected as too
repetitive; the retained layout uses edge access panels around a distinct central
housing. The hull is 44 x 105 x 13.975 m with 141,844 source triangles, two UV
channels and no collision hulls. Its authored crown envelope now reaches 7.9 m
above the pivot. This intentionally changes the former flat visual envelope;
width, length, native collision ownership and the X=7,800 cm corridor boundary
remain requirements. The bridge art is level relative to its hull rather than
retaining the graybox's sideways cant. Cinematic actor transforms are preserved.

`CarrierLightingComparison-20260915-025731-265b58a7` compared the original lighting
against west/front fills of 100,000/40,000 and 500,000/200,000 lumens. Both were too
bright and blue, especially on the walkway, and were rejected. The candidate uses
10,000/4,000 lumens with a more neutral color. It adds two shadow-casting RectLight
components to the retained scenery owner whose obsolete mesh is hidden. No global
sun or exposure changes are part of this work.

`CarrierRefinement-20260915-030240-bc7f476f` and
`CarrierRefinementState-20260915-030447-c8853e7e` stopped before map saving on the
preservation assertion. The diagnostic run identified exactly two new NoCollision
editor billboard icons on the light owner, with no existing light changes. The
assertion now permits only those expected editor icons and still checks all
original actor transforms and collision state.

### Rendering diagnosis and retained fill

The retained fill uses 400 x 300 cm emitters at 10,000 and 4,000 lumens. The
original 4,000 x 3,000 cm emitters produced visibly grainy shadows on the sides.
The smaller emitters improve those areas without disabling cast shadows. The
roof's fine dark patches remain unresolved; this is an incremental art pass,
not a final fidelity qualification.

- `CarrierShadowComparison-20260915-031759-ab18705e` stopped during startup and
  produced no captures. Its process was confirmed absent before retrying.
- `CarrierShadowRetry-20260915-075143-229781c5` completed with exit 0 and no
  Python errors. Disabling fill shadows removed side grain but did not remove
  the roof patches. Removing carrier cast shadows also left the roof patches.
- `CarrierSurfaceInputs-20260915-075658-9dc69e7c` completed with exit 0 and no
  Python errors. Carrier distance-field participation and stone normal-map
  isolation did not remove the artifact. All light contact-shadow lengths were
  already zero, so the contact-shadow view added no independent evidence.
- `CarrierAOComparison-20260915-075958-f7a38bf5` completed, but its attempted
  unlit viewport stayed lit and is excluded as unlit evidence. The short-range
  AO switch did not visibly resolve the artifact.
- `CarrierBaseColor-20260915-080322-08192bf0` reused stale scene-capture contents;
  its images are excluded. `CarrierExplicitCapture-20260915-080532-91513cb8`
  corrected this by explicitly requesting each capture. Its base-color image
  contains dark patches in fine roof geometry, pointing away from lighting alone.
- `CarrierRendererComparison-20260915-080927-20788398` completed with exit 0 and
  no Python errors. It verified the global `r.Nanite` switch, which visibly
  changed other scene details but did not remove the roof artifact. The earlier
  component-only comparison was therefore not used alone to rule out Nanite.

All diagnostic renderer, material, visibility and light changes were unsaved.
No global lighting, exposure, normal-map or Nanite switch is part of the retained
refinement. Investigating the fine surface geometry and its instance transforms
remains necessary before this carrier is visually complete.

### Saved refinement validation

`CarrierRefinementSaved-20260915-081207-616c09d6` saved the refined map after
backing it up and preserving all 113 existing lights, actor transforms and
collision state. The suspended route and stable forward screenshots were
reviewed. The smaller fill improves side readability; roof artifacts remain.

`CarrierRefinementFreshPIE-20260915-081501-cea96b7f` reloaded the saved map and
passed all 88 architecture verification reports. Fresh native PIE confirmed the
eight carrier part transforms and pre-meeting visibility, checked the local
navigation/clearance, and left the map hash unchanged. Both runs ended with exit
0 and no Python errors. This does not qualify the rescued transition, a full
campaign route, destruction or performance. The alignment score is unchanged.

Review images and the saved-state, PIE and clearance reports are retained in
`Docs/Validation/AurelionCarrierRefinement-2026-09-15/`.
