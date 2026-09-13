# Aurelion holographic HUD refinement

The retained M12/M13 route showed two presentation issues: incoming-fire cards
used a 96% opaque dark fill, and the current objective remained visible across
cinematic letterboxing. These changes address that observed presentation.

Default threat cards now use a 24% dark fill, static thin corner marks and a
one-pixel text shadow. Existing directional chevrons, localized warning words,
source counts, placement/overlap avoidance, expiry and attack-warning ownership
remain intact. High-contrast HUD retains an opaque background; user threat-color
overrides remain respected. No flashing or new animation was introduced.

Objective text and waypoint drawing now defer while the current player's ASC
has the native SequencerControlled tag. The objective cache and review entries
remain available, and the overlay returns when the native cinematic owner
releases that tag. This also covers silent portions of a cinematic; subtitle
presence alone does not determine visibility. Caption/subtitle settings and
their backgrounds are unchanged.

The objective settings regression now checks temporary cinematic hiding,
preserved authorized review entries, and restoration after tag removal.
Build, test and rendered results are recorded below when available. This pass
does not establish the full HUD or overall TDD fidelity target.

UE 5.7 Development Editor rebuilt successfully. All 13 objective tests passed
in `Saved/Validation/HolographicHUD/20260913-140931-7413460d`, including
source/report coverage and unchanged-source checks. The first run crashed in
the new test because its lightweight fixture deliberately had no initialized
ASC; the fixture now explicitly creates one for this test. Production null
handling was already guarded. The failed run is retained separately at
`20260913-140653-a5a771e4`.

All three presentation/threat-cue tests passed against the rebuilt editor in
`Saved/Validation/HolographicHUD/20260913-141047-7f8e07b9` with matching source
coverage. These check direction, safe placement and overlap behavior; they do
not establish rendered readability of the translucent panel.

The subsequent live run `HolographicHUDRoute-20260913-141207-5e409161`
rendered the translucent threat panel in E2 and correctly hid objective text
and waypoints during the rescue scenes, restoring them after the scenes.
Caption backgrounds remain as configured. At the small embedded editor viewport,
the E2 warning and sound-caption text still crowded each other; broad HUD layout
acceptance remains open. This run passed E1, E2, E3 entry/rescue and E4 entry,
then stopped on an E4A pulse without a second sever. It did not reach the M13
camera changes and must not be counted as a complete successful route.

The next run, `CompanionCadenceRoute-20260913-143402-6e41e7f1`, ended in
an actual E1 player defeat. Before closing it, the read-only HUD probe captured
visible caption/objective/vitals widgets with nonzero desired sizes but zero
paint and tick geometry. Threat avoidance discarded those empty rectangles.
`SovWidgetGeometry::FindRenderedBounds` now falls back to the live arranged
Slate widget path when cached geometry is empty, preserving DPI, safe-area and
render transforms. Hidden/collapsed widgets reserve no space. No warning
acknowledgement, lifetime, size or direction rules changed.

UE 5.7 Development Editor rebuilt in 28.85 seconds. All four presentation tests
passed in `Saved/Validation/ThreatPanelGeometry/20260913-143936-b429c2f0`,
including a real native border in a test-owned hidden Slate window with empty
geometry caches. Source/report coverage and unchanged-source checks passed.
Fresh rendered verification remains pending.
