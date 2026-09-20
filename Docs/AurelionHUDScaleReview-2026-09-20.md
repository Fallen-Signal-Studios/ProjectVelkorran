# Current HUD scale review

Update: the waypoint/subtitle overlap below is fixed in the reproduced framing;
see [HUD native refinement](AurelionHUDNativeRefinement-2026-09-20.md) for the
new rendered evidence and remaining coverage limits.

## Immersive review, 20 September

`HUDImmersiveReview-20260920-061644-7d50a450` now provides UI-inclusive images
without editor chrome, with **1286 x 726 actual gameplay pixels**. All three
images (UI scale 1.0, 1.5, 2.0) were inspected directly. The resource fills stay
inside the plate, split 32 / 218 ammunition stays legible, and radar and lower
arc remain on screen. The controlled native ammo/radar/Blackout fixture passed
before the scale captures. This is Tarrik at the M12 entrance, not Selene, a
complete campaign run, 1080p, or ultrawide acceptance.

This camera framing leaves the projected objective above the subtitle rectangle
at all three scales. It does **not** invalidate the earlier overlapping framing
below. At 2.0 the top-left objective consumes substantially more scene area;
neither this check nor the existing scale setting qualifies long localization
strings. Synthetic speech/caption text was presented to inspect occupied space;
it is not evidence of story dialogue triggering correctly.

`review_hud_accessibility_scales.py` now records actual viewport dimensions,
viewport DPI scale, and both visible ammo fields instead of the desired size of
the collapsed compatibility AmmoText. It has a bounded optional operator gate
(`SOV_HUD_REQUIRE_IMMERSIVE=1`, then run-directory `viewport-ready.txt`) so that
the viewport can be expanded before capture. Use the runner's visible retained
ExecCmds mode for interactive viewport controls. The ExecutePythonScript mode
holds a UI-blocking progress task while alive.

The preceding `HUDImmersiveScale-20260920-061505-6b03711d` attempt is explicitly
**embedded**: its 2576 x 1048 image contains a 1696 x 862 gameplay viewport.
Do not infer coverage from either run's label or image dimensions. The successful
immersive run restores settings and ends PIE; its isolated editor was then
closed. No content assets, maps, or native source were changed by these reviews.

## Health luminosity refinement

The clean baseline showed a subdued health fill compared with the supplied
reference. `refine_hud_health_luminance.py` increases the existing directional
energy gradient, raises its dark vertical falloff, and adds a restrained center
highlight and stronger bevel. It changes only `M_SovHealthBeveledFill`'s grayscale
brightness. The material silhouette/opacity, progress mask, native color binding,
health value and widget layout are preserved. The canonical health-fill authoring
script now generates the same shader.

Saved by `HUDHealthLuminance-20260920-062208-a854c27f`, with original package and
before/after shader backups. Fresh `HUDHealthImmersive-20260920-062309-e6c094c1`
passed the actual ammo/radar/Blackout fixture and captured the same 1286 x 726
viewport at all three scales. Direct review of all three images confirms a
brighter red fill, unchanged mask boundaries, and retained readout legibility.
The material is shared with Selene's tint, but this increment does not constitute
a fresh Selene gameplay or depleted-health acceptance pass.

Baseline full gate `20260920-062013-9b636589`: build without SkipBuild, 719 matching
automation tests, coverage and source integrity passed. Post-change full gate
`20260920-062623-43147590` also passed the build without SkipBuild, all 719 matching
automation tests, coverage and source integrity. Python syntax and whitespace
checks passed. No tracked files changed during that gate.
The creator's M12 map retains the hash documented below; no map was saved.

## Earlier overlapping framing (still open)

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
