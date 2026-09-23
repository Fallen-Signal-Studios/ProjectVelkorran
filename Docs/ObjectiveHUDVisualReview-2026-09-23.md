# M12 objective HUD visual review, 23 September 2026

The world waypoint label and the upper-left objective summary now use a lighter
normal-mode presentation. The waypoint retains its protagonist-colored glyph,
text, bearing and distance, but its label uses a 28%-opacity glass backing,
fine angular strokes and a faint colored halo. The objective summary backing
uses 30% opacity and 18-point type at UI scale 1. High-contrast mode retains
an opaque backing and 20-point objective type. Both surfaces still honor UI
scale, text-panel avoidance, objective visibility and cinematic suppression.

`Saved/Validation/20260923-085833-c1018a43/AutomationReport/index.json`
passed all 52 `ProjectVelkorran.UI` tests. The objective settings test verifies
27-point normal and 30-point high-contrast text at UI scale 1.5. A fresh M12
visible PIE capture passed at
`Saved/Validation/Aurelion/ObjectiveHUDCompactPIE-20260923-085953-33b65765/player-viewport-capture.json`;
`m12-player-viewport-with-ui.png` in the same directory is the real local
player Slate viewport. It shows the objective label and summary legible against
the bright departure hall while occupying less visual weight than the previous
capture at `Saved/Validation/Aurelion/PlayerSlateCapture-20260923-045653-c57eabf6`.

This is a CP0 HUD visual check, not a full M12/M13 lighting or accessibility
sign-off. In particular, a waypoint directly over the player silhouette or a
dark enemy still needs a separate moving-camera review.
