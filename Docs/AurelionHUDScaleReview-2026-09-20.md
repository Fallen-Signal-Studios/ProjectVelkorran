# Current HUD scale review

The actual M12 HUD was rendered at UI scales 1.0, 1.5, and 2.0 after the lighter
glass/footer revisions. Run:
`Saved/Validation/Aurelion/HUDCurrentScaleReview-20260920-013348-ff72541f`.
The controlled firearm fixture verified real 32/218 ammunition, one detected
hostile, Blackout suppressing the contact, and restoration afterward. The editor
exited normally; no assets or campaign progress were saved.

The captures expose a real readability defect: the world-projected
`Objective: 7 m` waypoint label crosses the Lyessa subtitle text, particularly
at UI scale 2.0. The holographic plate, ammo housing, radar and footer remain
on screen. This is an embedded 845-by-550-ish editor viewport inside the
1280-by-720 capture, not a full-screen 720p/1080p or ultrawide qualification.
Do not describe the scale report's `passed_requires_visual_review` value as a
clean accessibility pass.

The overlap was initially mistaken for an objective-update notification. Source
inspection identifies the actual owner as native `USovAccessibilityPresentation`:
the waypoint branch of `NativePaint` calls `DrawLabel` after choosing its
world-projected point; subtitle placement uses a different layout calculation.
The label clamp checks screen-safe bounds, not the occupied subtitle rectangle.
Moving an unrelated widget or disabling objective text would not solve that
layout conflict.

The appropriate fix is to reserve active subtitle/caption rectangles when
placing waypoint labels, including edge markers and large text, while preserving
their bearing cue. Validate at 1.0/1.5/2.0 scale, multiple aspect ratios and marker
bearings, and with subtitles absent/present. C++ changes remain pending the
already-requested scope decision under the engineering handoff.
