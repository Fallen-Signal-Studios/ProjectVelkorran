# Aurelion alignment estimate — 13 September 2026

Current planning estimate: **about 64% for the M12–M13 Aurelion slice**
(63.75/100, judgment range 60–70%). The 90% goal remains active. This carries
forward the existing 26-requirement, 100-point slice rubric; it does not award
new points merely for builds, repeated partial routes, or cosmetic changes.

The whole-TDD estimate in `TDDAlignment-2026-09-11.md` was approximately 37%.
That includes the full campaign and platform obligations. It is a different
denominator and has not been independently re-audited here.

| Area | Supported points | Weight |
|---|---:|---:|
| Mission progression and consequence | 13 | 20 |
| Protagonist combat | 7.5 | 12 |
| Enemy AI and encounter behavior | 11.25 | 16 |
| HUD, objectives and accessible interaction | 13.5 | 18 |
| Combat VFX, audio meaning and comfort | 6.75 | 10 |
| Environment, animation and story presentation | 6.5 | 10 |
| Save, traversal and reliability | 3.75 | 7 |
| Packaging and measured performance | 1.5 | 7 |

Baseline rubric: creator-workspace
`C:/Users/msnod/Documents/Codex/2026-09-06/go/outputs/Aurelion-TDD-Alignment-Assessment.md`.
This is an evidence update, not a new independent acceptance audit or a test-pass percentage.

## Current evidence and next acceptance work

- The restored-policy run passed E1, E2, shared entry, rescue and E4 entry.
  Axiom confirmation timed out. An earlier separate E4A pass demonstrated actual
  precision Echo and two link-sever receipts, but the immediate E4A→E4B→M13
  route is still unqualified. Thermal payoff, Core follow-up and survivor escape
  remain the highest mission-flow priorities.
- Eclipse roles use compatible Parasites-pack meshes and animation assets.
  Native damage has been observed for Linkbound, Elite and WallRunner; Weaver
  attack acceptance, visible contact/timing and crowded-combat recognition remain open.
- The holographic HUD and word-preserving subtitles/captions built and passed
  focused tests and rendered checks. Alert/caption overlap and all-scale,
  all-background acceptance remain open.
- The current workstation archive cooked with zero errors and 943 warnings.
  Packaged M12 headless startup passed. This supplies no representative GPU,
  audio, input, complete-route or long-session performance acceptance.
- Checkpoint/reload soak, both priority outcomes, player understanding studies,
  canon review, pacing and final audio/cinematic acceptance remain incomplete.

Detailed run IDs, failures and qualification limits are in
`AurelionEclipseAlignment-2026-09-13.md` and `HUDHolographicDirection-2026-09-13.md`.

## Layout-reference finding

The creator's `C:/Users/msnod/Downloads/Aurelion_Level_Layout_Plan.pdf`, pages
12, 13 and 23, requires survivor recesses outside the Elite's charge routes.
Page 23 also calls for the link, exposed joint and strike opportunity to fit one
readable camera composition. The earlier Elite pursuit into the west stretcher
position contradicts this intended staging. Inspect the authored access geometry
and actual navigation before changing enemy authority or enlarging Thermal ranges.

The four concepts on pages 21–24 govern materials, lighting and composition;
the dimensioned plan governs collision and playable dimensions. Mesh replacement
must preserve that distinction. The reference does not authorize its depicted
costumes, heraldry, continuous beam or an additional boss.

The stopped-editor census in
`Saved/Validation/Aurelion/CrucibleRecessAudit-20260913-094129-975a508b/crucible-recesses.json`
verified these current geometry facts:

- West front wall segments end at X=-2900 and X=-2300, leaving a direct 6 m
  entrance at Y=21900. The retained west patient mark is inside at (-2850,22400).
- East front segments leave a direct 5 m entrance, X=2800..3300 at Y=21900.
- Both recess wall families use `/Engine/BasicShapes/Cube`, with query/physics
  collision enabled and 3 m wall height. Threshold marks have no collision.

An offset entrance or baffle should be tested for line-of-sight and charge-path
separation, ordinary player/cast egress, cache access and both priority states.
This is a proposed remediation, not a proven cause or a geometry change. Do not
make survivors invulnerable, alter faction targeting or enlarge Thermal ranges
to compensate for an untested layout.

The census completed successfully. Rendered inspection was interrupted by a
Windows Security prompt for the packaged game; the user must dismiss it before
desktop interaction resumes. No environmental mesh or collision edits were made
by this audit. The captured `crucible-before.png` includes the blocking prompt
and is not a usable scene-fidelity comparison.

While desktop access remained blocked, a Blender wall-panel candidate was authored
under `Art/Source/Aurelion`. Its FBX passed a clean Blender round-trip check for
dimensions, pivot, UVs, material slots and simple UCX collision; its studio preview
was visually inspected. It has not been imported or placed in Unreal and earns
no additional acceptance points. The slice estimate remains 63.75% (about 64%).
