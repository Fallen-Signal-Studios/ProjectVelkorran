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
