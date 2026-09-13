# Crucible refuge geometry and cache access

The previous uninterrupted WestStretchers route failed when the Elite acquired
Tharne and entered the west refuge. Static traces reproduced the entry sightline
from (-750,19263,-1020) to Tharne and the western patient.

Seven visible, colliding stone panels now form staggered entrances: four 2m panels
across the west refuge and three half-width panels across the east. The existing
refuge walls and seven cinematic exit marks stay in place. Complete native paths
around the panels remain available; these walls provide occlusion, not immunity.

The original medical cabinet faced a cramped gap behind the original south wall.
Its four cabinet pieces, cache and existing west-priority gate moved 490cm north.
The cache is now at (-3050,22600,-1140), with its gate at (-3050,22475,-1060).
The content authoring recipe matches those saved placements. Medical use still
requires native WestStretchers eligibility, proximity, visibility and missing
health; the once-only transaction and 250cm interaction range are unchanged.

## Stopped-editor evidence

`Saved/Validation/Aurelion/RecessBaffles-20260913/admission.json` records:

- All 28 sampled court-to-refuge sightlines blocked; the two previously open
  western rays now hit the new panels.
- Seven representative 42cm-radius, 88cm-half-height exit capsules clear.
- Complete paths to both refuges and the medical cabinet's front approach.
- Four M12 handoff destinations and all eight scene-request surfaces passing
  their existing native geometry validators.
- Five reachable cache approaches for each actual hero CDO capsule, about
  218–222cm from the cache. Visibility passes with the native priority gate
  excluded from the static trace; the corresponding closed-gate trace hits that
  exact support actor. No gate state or campaign state was changed for this check.
- Map Check: zero errors, **51 material override warnings**. These warnings remain
  open; this pass does not claim a warning-free map.

After saving, M12 was unloaded by opening M13, then reloaded from disk. The exact
seven panel placements and collision modes, cache/gate coordinates, sightlines,
paths, handoffs and scene-request checks passed again in
`admission-after-reload.json`. This is a refuge-specific persistence check; it
does not waive the previously recorded procedural asteroid/star reload drift.

`West-baffle-editor.png` is an actual Unreal Game View capture of the first saved
candidate. Two dark, noncolliding REFUGE / SIDE ACCESS signs were added after
the runtime attempt to clarify the side passages. The first, wider east-panel
candidate failed navigation and was narrowed before saving.

## Runtime qualification

A fresh visible M12 route was launched in
`Saved/Validation/Aurelion/RefugeBafflesValidated-20260913-121838-976a04a5`.
Fresh entry passed in 34.828s, E1 in 256.204s, E2 in 51.765s and E3 entry in
93.750s. E3 rescue failed at 4.687s when the existing Cinderline ammunition ran
out. Tarrik entered with nine rounds and no reserve. The driver stopped without
granting ammunition, replacing weapons or changing mission state. A subsequent
read-only live census found no ammunition pickups in the world. PIE was stopped
after preserving the failed report and `E3-ammo-exhausted.png`.

This attempt never reached E4, so it cannot qualify the refuge changes in combat.
Static occlusion does not prove that threat memory or subsequent pursuit cannot
lead the Elite around a panel. Survivor protection,
Thermal/Core victory, both priorities and earned M13 travel remain required.
The test uses the existing ordinary-input drivers, with audio disabled; it cannot
qualify sound or the packaged GPU build. TDD alignment remains about 64%.

The next route needs a normal visible melee/ability fallback or justified authored
ammunition recovery. Do not patch this failure with runtime grants, synthetic
pickups or checkpoint resets. The E1 live capture also records the remaining HUD
issue: the high-opacity caption and incoming-fire panels visually dominate the
thin bars. Subtitle/accessibility settings and threat semantics must be preserved
when refining their presentation.
