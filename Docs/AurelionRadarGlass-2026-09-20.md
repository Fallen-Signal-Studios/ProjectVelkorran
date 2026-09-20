# Radar glass refinement

The live radar retained a nearly opaque black disc after the plate and footer
became translucent. `refine_hud_radar_translucency.py` changes only the owned
`M_SovRadarReticle` material, reducing interior opacity from .96 to .62 while
keeping separate opacity contributions for the rings, axes, ticks and sweep.
Contact brushes, sweep time input, palette bindings and gameplay detection are
unchanged. Authoring run `RadarTranslucency-20260920-001944-307ab41c` backed up
the asset, saved the shader and exited normally without saving a map.

Rendered/functional check: `RadarGlassGameplay-20260920-002121-6dc31e4f` passed.
The actual ammo readout matched 32 / 218; one detected contact was presented,
Blackout hid it, and restoring settings restored the contact. The captured
Tarrik view shows the bright floor and its dark path through the radar glass,
while its rim, central arrow and sweep remain distinct. The Blackout capture was
also reviewed. This is a controlled fixture, not complete mission visual QA.
Full baseline: `20260920-001251-780ccf05`, build and 719 tests passed before this
content edit. Post-change full gate `20260920-002150-b37dd328` passed build without
SkipBuild, all 719 tests, report coverage and source integrity. No tracked edits
occurred during the gate. Both editors exited normally; no mission map was saved.

## Companion investigation retained

Read-only `SwordPoseLayers-20260920-001517-eefa9348` exported the current biped
and sword-layer graphs/defaults. Controller pitch alone does not establish that
the companion's melee pose receives an inappropriate aim offset: the graph has
separate aim/look-at predicates and layers. No animation edit was made on that
assumption. The outstanding companion contact issue remains open; the previous
four-distance controlled player test is not companion acceptance.
