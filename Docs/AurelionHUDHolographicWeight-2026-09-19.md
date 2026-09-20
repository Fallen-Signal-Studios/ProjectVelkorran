# Holographic HUD visual weight — 19 September 2026

The owned plate and energy-arc materials now expose more of the scene behind
their backings. The footer half-width falls from .145 to .112 in its existing
UV rectangle (23% thinner); its energy inset and glint follow the reduced width.
Empty footer backing opacity falls from .97 to .38. Filled cells and luminous
edges have independent opacity contributions, preserving their legibility.
Plate backing falls from .97 to .62 with separately strengthened edge opacity.
Both perimeter halos are quieter. Layout, protagonist palettes, resource values,
widget bindings and native presentation logic are unchanged.

Authoring: `HUDHolographicWeightRetry-20260919-230221-ee76e300` saved both materials.
The first attempt failed to save because the completed campaign editor retained
the asset file; no content changes landed in that attempt. After that editor
closed, the fresh authoring session saved successfully. Both runs retain their
logs and the original material backups. No mission maps were saved.

Baseline full gate: `20260919-225515-17c21d1a` (build and 719 tests).
Fresh gameplay fixture `HUDHolographicGameplay-20260919-230329-bc4eb9eb` passed:
actual Cinderline ammo 32/218, one native radar contact, zero contacts under
Blackout and one after restoration. Legacy readout/minimap retirement survived
the fixture's cinematic visibility stress case. The rendered live-contact PNG
was inspected: the footer is thinner and the floor remains visible through empty
cells; filled segments and ammo remain legible against the bright arrival hall.
This is a controlled fixture, not new full-route, gamepad, or all-palette proof.
The earlier full campaign and reload evidence remains documented separately.

The creator-owned M12 map retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
No C++ or creator grenade assets changed. Overall AAA visual/gameplay completion
remains unproven; the goal is active.

Post-change full gate `20260919-230556-cc9b5224` passed: build invocation without
SkipBuild, all 719 matching automation tests, report coverage and source integrity.
No tracked files changed during the gate. Python syntax and whitespace checks passed.
