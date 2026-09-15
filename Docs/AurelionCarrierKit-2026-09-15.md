# Custom Aurelion rescue carrier

The custom Blender kit contains a faceted hull, radiator-lined nacelle and glazed
bridge. Ceramic armour, narrow gold conductors, docking shutters and thrust irises
replace the tiled-floor construction of the former carrier. This is an environment
art pass, not a final AAA quality or campaign completion claim.

Source: `Art/Source/Aurelion/CarrierKit/Aurelion-Rescue-Carrier.blend` and
`build_carrier_kit.py`. Three FBX meshes have centred pivots, metre dimensions,
two UV channels, three existing Aurelion material slots and no collision hulls.
The round-trip report records 71,212 hull, 64,004 nacelle and 4,868 bridge triangles.
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
