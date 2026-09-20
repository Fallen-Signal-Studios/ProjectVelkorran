# HUD magazine and text-clearance fixes

The user explicitly authorized native HUD changes on 20 September, extending
the handoff's content-only scope for these two verified defects.

## Magazine visibility

The HUD snapshot now queries `UWeaponItem::UsesMagazine`: configured ammo class
plus positive magazine capacity. It does not query inventory availability to
decide whether the panel exists. Ammo-free melee and non-magazine weapons omit
the panel; an exhausted firearm still shows its empty magazine and reserve.
The existing widget bindings, palettes and magazine counts are retained.

The snapshot automation now exercises wielded equipment, exhausted firearms,
switching to ammo-free melee, and ammo use without a magazine. The equipment map
is a controlled fixture; production weapon lookup and snapshot code are used.

`SeleneHUDMagazineFix-20260920-090620-4896962c` restored the unchanged earned CP2
banks through the public save subsystem and checked four states in Unreal:

| State | Ammo panel | Visible ammunition |
|---|---|---|
| Restored, unarmed | Hidden | None |
| Verity | Hidden | None |
| Staccato | Visible | 4 / 32 |
| Verity after Staccato | Hidden | None |

The report passed, with no presentation errors. Both Verity captures and the
Staccato capture were visually inspected. Actual gameplay viewport dimensions
were 1696 x 862 within the editor. This is controlled wield-API coverage, not
physical weapon-wheel input or a new campaign playthrough. The checker now
includes the return switch and keeps separate numbered screenshots.

## World labels around speech

`USovAccessibilityPresentation` now measures the actual arranged paint geometry
of visible subtitle, caption and objective panels. The shared world-label draw
path finds the nearest complete safe-area placement with an eight-unit gap,
including the text shadow. It does not move the world target or edge-bearing
glyph. Hidden panels reserve no space. If no complete placement exists, only
the secondary label is omitted; its target/direction glyph remains visible.

The new geometry regression covers embedded, 720p, 1080p and ultrawide sizes;
UI scales 1/1.5/2; wider labels; nonzero safe-area origins; corners and center
bearings; simultaneous speech/caption panels; their removal; and no-fit fallback.
Those are geometry tests, not rendered acceptance at every aspect ratio.

`HUDTextClearance-20260920-090842-afb89c91` reproduced the original problematic
entrance framing at an actual 844 x 550 gameplay viewport. All three captured
scales were inspected: the `Objective / 7 m` label sits below the subtitle with
clear separation, while the target glyph stays anchored. Simultaneous captions
remain readable. The real Cinderline 32/218 readout, hostile radar contact,
Blackout suppression and contact restoration fixture also passed. Speech and
captions were deliberately presented for layout inspection; this is not dialogue
trigger or complete accessibility acceptance. Fullscreen edge-marker motion,
long localization and controller testing remain outside this rendered check.

## Validation and preservation

Baseline full build and 719 tests: `20260920-085915-42564d55`.
The first implementation compile rejected direct access to Narrative's protected
ammo configuration. A read-only weapon query replaced that access; the corrected
full build, all 720 automation tests, coverage and source integrity passed in
`20260920-090350-292c7f4c` without SkipBuild.
After the rendered checks and checker update, final full gate
`20260920-091203-4ac32097` again passed build, all 720 tests, coverage and source
integrity. Unreal remains open in edit mode after the review ended PIE.

No gameplay assets, maps, animation packages or mission revisions were changed.
The creator's grenade and M12 edits and local GASPALS plugin remain untouched.
M12 SHA256 remains
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
This fixes two HUD defects; it does not complete the Aurelion visual/gameplay goal.
