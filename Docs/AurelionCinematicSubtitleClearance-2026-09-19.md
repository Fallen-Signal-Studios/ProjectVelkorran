# Cinematic subtitle clearance and M13 lift follow-up

## Saved UI correction

The live M12 GroundLyric and refuge scenes showed lower dialogue lines clipped
by decorative black bars. The owned WBP_AurelionGameplayHUD embeds the vendor
WBP_CinematicOverlay, whose visual tree contains only two black-bar images.
Its root render opacity is now zero. The widget remains instantiated so its
skip/pause bindings and animation lifecycle remain available. Camera aspect
ratios, sequences and mission data were not edited.

`Scripts/Editor/clear_cinematic_subtitle_occlusion.py` saved only the owned HUD,
backed it up and verified the creator's M12 map hash remained unchanged.
Fresh PIE with `review_cinematic_subtitle_clearance.py` exercised the actual
Drop Black Bars function and native cinematic speech at UI scales 1, 1.5 and 2.
All three captures were visually reviewed: the complete Lyessa passage remained
readable with clearance above the footer. This is controlled presentation proof,
not replay of an actual cinematic scene or skip/pause input qualification.

Evidence: `Saved/Validation/Aurelion/CameraPerspectiveReplay-20260919-204937-08687aad/`
contains `CinematicSubtitleRepair/repair.json`, `cinematic-subtitle-clearance.json`
and the three `cinematic-subtitle-*.png` captures.

## Open lift failure

The same fresh route completed M12 and native M13 travel, then accepted the lift
hold but returned to AtOrigin without moving. The failed route and read-only
rider snapshot are preserved. Player and companion capsule radii were both
34 cm and centers were about 54.86 cm apart horizontally.

A possible cause is the native passenger clearance sweep: it ignores the current
rider and lift but not other riders, whereas the body sweep ignores every rider.
Close co-riders could therefore block passenger clearance. This is an inference;
no blocking collider or OnTransitChanged error message was captured. Geometry
or another cancellation condition has not been excluded. Do not disable lift
collision, bypass progression, or claim this resolved. Diagnose the native
cancellation reason on a focused retry. Source changes are outside the engine
handoff's content-only scope.

A temporary video-memory-over-budget message appeared during M13 travel; no
performance fix or full visual-quality acceptance is claimed.

Post-change full gate `20260919-211249-bd0f8c59` passed build invocation without
SkipBuild, 719 matching automation tests, coverage and source integrity.
