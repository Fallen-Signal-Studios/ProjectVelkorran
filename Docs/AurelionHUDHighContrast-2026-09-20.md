# Widget HUD high-contrast repair

The Selene checkpoint review found that high-contrast settings reached the HUD
palette, but the authored materials retained translucent glass, health gradients
and the moving radar sector. White tint alone did not provide the opaque,
flat presentation required by FSovHolographicHUDPalette's contract.

Baseline `SeleneHUDAccessibility-20260920-123653-9c525faa` successfully loaded the
earned CP2 and recorded normal and high-contrast settings at UI scales 1, 1.5 and
2. Staccato stayed at its real 4/32 ammunition. Its JSON state checks passed,
but inspected images expose the material defect; this is not visual acceptance.

USovHolographicHUDSurface now creates per-widget material instances at construction
and sends `HighContrast` when the published palette changes. Image and border
instances remain available to their existing UMG updates; material-based progress
bar brushes receive separate transient instances. The shared content materials,
live resource values, weapon selection and widget layout are not mutated by the
runtime setting. The HUD remains drawn by the authored UMG widgets.

`author_hud_high_contrast_materials.py` adds a default-zero scalar and an explicit
high-contrast branch to the plate, ammo housing, footer, radar, health fill and
ability-pip materials. The original normal-mode shader bodies are preserved
verbatim and backed up with SHA256 records. High contrast uses opaque shaped
backings, flat white fills and retained symbols/cell state, with no radar sector,
optical halos or energy gradients. Existing contact brushes are unchanged.

The initial native compile rejected `auto*` iteration over TObjectPtr entries;
the corrected reference iteration compiled and full gate
`20260920-124538-1845f374` passed all 720 matching tests, coverage and source
integrity. Baseline full gate was `20260920-122926-e5761b5b`.
Author run `HUDHighContrastMaterials-20260920-124727-074e2ac1` saved six materials.
It emitted temporary missing-HighContrast-input warnings while constructing each
input before connecting it. Fresh-process rendering and material parameter checks
are required to establish the saved graph's validity.

The expanded checkpoint checker exercises three scales in both modes and returns
to normal afterward. It reads seven actual dynamic material parameters without
creating or repairing them. It also verifies the live bars, palette, weapon ammo
and visible split text, and restores the original settings on cleanup. Speech and
sound-caption text are synthetic layout fixtures, not mission dialogue evidence.

## Runtime result

Fresh `SeleneHUDContrastFixed-20260920-124830-02fe89bb` passed all eight states
and all 56 per-material parameter checks, including return to normal. The saved
materials produced no compile failures in that fresh process. Images at all
three scales in normal/high-contrast mode were inspected, including the return
to normal: high contrast has opaque black backings and flat white fills without
a sweep; normal retains its cyan glass and gradients. Actual Staccato ammo and
visible fields remained 4/32. Both mode/scale checks use an embedded 844 x 550
viewport, not fullscreen or ultrawide acceptance. Caption duration expired in
some captures; simultaneous captions are visible in the high-contrast 1/2 cases.

`HUDContrastRegression-20260920-125131-ee1b977e` passed the existing Tarrik
weapon/radar/Blackout fixture and three normal-mode scales. Actual ammunition
remained 32/218, contact suppression and restoration passed, and the 1/2-scale
images were inspected for retained gold/red presentation and text clearance.
The fixture spawns a controlled hostile; it is not a campaign completion claim.

The final native cleanup reuses this surface's existing progress-bar instances
on reconstruction and refreshes its ArcMaterial cache, avoiding dynamic-parent
chains when the same widget is re-added. The screenshot runs precede that
lifetime-only cleanup; repeated widget reconstruction has not received separate
rendered qualification. Final full gate `20260920-125408-6778e84a` passed build
without SkipBuild, all 720 matching tests, coverage and source integrity.
Python syntax and whitespace checks passed. No map, widget Blueprint, weapon or
gameplay state asset was saved. The six material assets and HUD native support
are the production change. M12 retains its protected hash recorded in the
Selene holster evidence document; existing user work is preserved.
