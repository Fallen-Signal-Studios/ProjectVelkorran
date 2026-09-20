# M13 chamber architectural lighting

The chamber approach previously lost most of its ivory wall panels and gold ribs
in shadow. Eight warm architectural uplights now reveal those surfaces around
the perimeter while retaining the existing terminal conversation fill.

Only `L_Aurelion_M13` is saved. The editor script is idempotent and uses named
lights, so rerunning it updates the same eight actors. The lamps use 2,000 lumens,
a 45 m attenuation radius, and a soft 0–60 degree cone in the initial pass described below. Dynamic shadows remain
enabled to keep the light behind the chamber geometry.

The 20,000-lumen trial was rejected after rendering because it washed out the
wall panels. The accepted preview is
`Saved/Validation/Aurelion/M13ChamberSoftUplights-20260920-011047-e2e05dcd/chamber.png`.
The earlier baseline is `M13RoutePresentation-20260920-010415-23e50e45/chamber.png`
under the same validation root. That first capture run timed out following a
reentrant screenshot callback; only its chamber image is usable evidence.

`M13ChamberLightingSaved-20260920-011225-fb1f8204/saved-lighting.json` confirms the
saved map reloaded with all eight lights, preserved existing actor names/classes,
and left the protected M12 file unchanged. Its backup contains the prechange M13
map. The mission definition and journal semantics were not edited, so this
presentation-only change does not raise `ContentRevision` or require migration.

The route cameras are editor game-view captures, not a runtime campaign pass.
The saved chamber render confirms the accepted lighting. The gallery view was
also inspected. The lift camera at Y=37500 faces the closed core gate at close
range and is obstructed; that image does not qualify lift-route readability.
GPU cost, hardware-controller play, companion contact reliability, and complete
M12/M13 visual acceptance remain open. This does not establish 90% TDD alignment.

Authoring: `Scripts/Editor/refine_m13_chamber_lighting.py`.
Scoped save/readback: `Scripts/Editor/save_m13_chamber_lighting.py`.
Reusable capture: `Scripts/Editor/preview_m13_route.py` (temporary camera, no save).

Full validation `20260920-011429-49cbcfaf` passed the build, 719 matching automation
tests, report coverage and source-integrity checks. The prechange gate was
`20260920-005530-aa7a2a8f`. Neither run used `-SkipBuild`.

## Balance after the custom wall, ring and rib replacement

The current authoring defaults supersede the initial settings above: eight
existing uplights now aim upward at 82 degrees with 3,600 lumens, a 0–24 degree
cone and a 15 cm source radius. Their positions, warm color, 45 m reach and
shadow casting remain intact. The three existing terminal conversation rect
lights drop from 3,000 to 2,100 lumens. No new lights, global renderer settings,
directional lights, geometry, collision or mission semantics change.

The first 9,000-lumen/18-degree trial in
`ChamberLightBalance-20260920-053201-667395d3` was too bright and was not saved.
The accepted preview `ChamberLightRefined-20260920-053343-12381f88` uses the
settings above. Chamber, terminal-stage and upward views were inspected. The
light reaches more of the upper panels and braces instead of pooling primarily
near the lower wall. The existing wall-edge artifacts persist; this is a light
distribution improvement, not a claim that the rendering defect is resolved.

`preview_m13_chamber_light_balance.py` previews the eight canonical uplights
plus the three stage-light changes. `save_m13_chamber_light_balance.py` backs up
and saves M13 only, reloads and verifies every changed light's pose/properties,
other actors' transforms/collision states and the protected M12 hash. The
canonical uplight authoring helper and its original save verifier have also
been updated so rerunning them does not restore the superseded low-wall aim.

Prechange full gate: `20260920-052847-e6bba01c`, 719 tests passed.

Saved readback `ChamberLightSaved-20260920-053543-c0906434` passed all 11
light checks, other actor state preservation and M12 hash preservation; its
chamber render was inspected. Postchange full gate
`20260920-053747-5f63db4f` passed the editor build, all 719 matching automation
tests, report coverage and source integrity, without SkipBuild. No packaged
build, runtime GPU profile or full campaign replay is claimed by this pass.
