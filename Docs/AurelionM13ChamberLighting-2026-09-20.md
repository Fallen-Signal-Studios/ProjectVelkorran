# M13 chamber architectural lighting

The chamber approach previously lost most of its ivory wall panels and gold ribs
in shadow. Eight warm architectural uplights now reveal those surfaces around
the perimeter while retaining the existing terminal conversation fill.

Only `L_Aurelion_M13` is saved. The editor script is idempotent and uses named
lights, so rerunning it updates the same eight actors. The lamps use 2,000 lumens,
a 45 m attenuation radius, and a soft 0–60 degree cone. Dynamic shadows remain
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
