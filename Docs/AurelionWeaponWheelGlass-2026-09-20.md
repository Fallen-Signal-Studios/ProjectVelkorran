# Protagonist glass weapon wheel and holographic waypoints

The project weapon wheel now uses a translucent optical annulus, restrained highlights,
thin sector seams and a brighter selected rim. Tarrik receives a warm amber, subtly faceted
Dominion lens; Selene receives a continuous cyan Reformation lens. The live owning pawn
supplies the theme, including after a checkpoint load. Existing transparent weapon thumbnails
become bright projected silhouettes rather than disappearing into the dark glass.

The inherited Narrative radial-menu hierarchy, selection graphs, item ordering, hand prompts
and input bindings remain in place. The one graph change replaces the parent material literal
in its existing `CreateDynamicMaterialInstance` node. Changing only the authored image brush
was insufficient: activation recreated the vendor material. The new editor helper refuses
linked or ambiguous material factories and changes no graph connections.

`USovWeaponWheelGlass` is a noninteractive decorative child of the owned radial tree. It feeds
only presentation parameters, styles the live thumbnails and paints the localized faction
identity. It neither equips items nor writes selection state. Its materials are per widget,
not global. No time-driven shimmer or rotation was added. High contrast replaces glass with
opaque dark surfaces and flat white markings; returning to normal restores the protagonist theme.

Objective markers retain their actual projected anchor, distance, localization, edge bearing,
Blackout suppression and navigation accessibility controls. Their glyphs now use layered
holographic strokes: Dominion shield facets and Reformation split diamonds. A small dark glass
label and rim remain readable over the world. Subtitle/caption clearance considers the complete
padded label footprint, while the glyph stays anchored to its target.

Authoring: `Scripts/Editor/author_weapon_wheel_glass.py`. Only the two new HUD materials and
three owned radial-menu Blueprints are saved. The widget-tree helper now registers new widget
GUIDs through Unreal's editor API before compiling. Marketplace assets are not saved.

Final visible evidence is recorded in
`Saved/Validation/Aurelion/WeaponWheelVisibleAcceptance-20260920-135305-f5756695`
and its `weapon-wheel-review.json`. All eight presentation states and both real selections passed.
The checker starts with Tarrik, publicly restores the unchanged
earned Selene checkpoint banks, exercises normal/high-contrast/normal presentation and selects
real inventory weapons through the existing wheel input. It checks the live material parent
as well as parameter values: a MID accepts unknown parameters even if its shader never uses them.
The silhouette bridge waits for `UCommonLazyImage::IsLoading()` to clear before replacing a brush;
setting a material during streaming cancels that load and would capture the placeholder pistol.
The checker also compares the projected texture against each real item's authored thumbnail.

Two-handed items use release-to-confirm; the separate main-hand click path applies to one-handed
items. Earlier checks incorrectly required a main-hand slot for Staccato and timed out waiting
for that slot. Those runs are not counted as selection acceptance.

The final visible run passed Velkorran's one-handed click and Staccato's two-handed release,
verified the actual thumbnail assets in all six wheel captures, and rendered both normal themes,
both high-contrast themes, and both faction waypoints. Normal colours returned after high contrast.
The two stock full-screen menu backdrops are collapsed; the optical annulus supplies its own backing.
Earlier offscreen Tarrik captures looked into a dark camera view. The visible run uses another
valid selection direction and confirms the world remains visible through the glass.

The final production gate `20260920-134916-d42c4461` passed build, 720 matching automation tests,
report coverage and source integrity. Only the review camera direction, comments and this record
changed afterward. Review screenshots are embedded editor viewports, not a packaged-build or
every-resolution certification. At that revision, dialogue subtitles could overlap the lower wheel
and faction inscription in these viewports, especially with the opaque high-contrast wheel.
That initial increment preserved the stock centered wheel placement; it did not claim subtitle/menu
collision avoidance. The waypoint's own padded label continues to avoid the subtitle panel.

The protected M12 map is not saved; its SHA256
remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

## Dialogue clearance follow-up

The owned wheel tree now wraps its existing 600-unit contents in `WheelSafeFrame` (ScaleBox)
and `WheelSafeCanvas`. Native placement prefers the full authored size in the closest free area,
then scales down only if necessary. It reserves the actual subtitle/caption/objective paint bounds,
HUD regions and safe area. A clear position stays fixed until another panel needs the space;
closing dialogue does not move controls during selection. Menu activation resets this preference.
The original sector input, equip graphs and content animation remain intact.

World-label placement reserves the active wheel's bounds. The objective glyph is occluded when
its projected position falls behind the wheel. Weak owning-player references and activation checks
prevent a closed menu from continuing to reserve space.

Authoring: `Scripts/Editor/author_weapon_wheel_clearance_frame.py`, which saves only the owned
radial base and weapon-wheel child. No map was saved. Baseline `20260920-140127-cbe83430`
passed 720 tests. Final full gate `20260920-142155-d82ee9c9` passed the build, 721 matching tests,
report coverage and source integrity. The added test covers dialogue, caption arrival, disappearance
stability, reopening, and impossible/invalid placements.

Visible acceptance: `Saved/Validation/Aurelion/WheelClearanceAcceptance-20260920-142334-7df00cd4`.
All twelve presentation states passed: each protagonist's waypoint, ordinary wheel, high contrast,
restored normal theme, and 150%/200% HUD scale. Ten wheel states measured nonoverlapping live
dialogue and objective-panel bounds inside the safe area. Both real inventory selections passed
(Velkorran one-handed click, Staccato two-handed release). Normal/high-contrast captures and both
200% captures were visually reviewed. No Python errors, material warnings or ensures appeared.
The normal wheel remains full authored size in this viewport and moves to the right of dialogue.
This covers the embedded editor viewport, not every display size or a packaged build. Subtitle
scale remains the user's original value; HUD scale is the setting varied here.

The first geometry probe returned zero for every widget despite the wheel being visibly rendered.
The editor-only `GetWidgetPaintBounds` helper now reads native paint geometry directly and returns
reflected numeric bounds instead of transporting `FGeometry` through Python. Earlier probe failures
are not counted as acceptance. Caption arrival and removal are covered by the layout unit test;
the visible acceptance fixture keeps dialogue active throughout rather than claiming those transitions.

## Windowed viewport and rear-objective clearance

World labels now reserve the rendered vitals, ammo (when present), radar and Echo arc in addition
to dialogue and the weapon wheel. Their complete padded footprint moves clear; the directional
glyph retains its actual bearing. A regression covers rear objectives across four viewport sizes,
100/150/200% UI scale and ranged/melee HUD configurations.

Windowed testing exposed a coordinate-space mismatch in the existing clearance implementation:
`GetCachedGeometry()` returns tick geometry with the desktop window offset, while the panel
measurements and NativePaint use window-relative paint geometry. The safe-area and HUD-region
readers, and the wheel's canvas conversion, now consistently use paint geometry. This fixes both
the wheel/subtitle overlap and incorrect HUD reservations in a moved, compact editor window.

Final full gate `20260920-153419-4667c832` passed the editor build, 722 matching tests, report
coverage and source integrity (95 existing warnings). Visible acceptance is
`Saved/Validation/Aurelion/HolographicUIFinal-20260920-153647-96952e4b`.
All sixteen presentation states and both real inventory selections passed. The compact embedded
viewport measured approximately 844 x 550 physical pixels; ten wheel states stayed clear of
live dialogue and objective panels inside the safe area. Both normal wheels, the high-contrast
fallback, rear objective labels and 200% wheel captures were visually reviewed. The rear marker
camera is rotated only for the fixture, then restored before wheel input. No Python errors,
material warnings or ensures appeared in this final run. These are editor checks, not a packaged
build certification.

Objective glyph strokes also occlude behind the same priority panels, including speech and the
Echo arc. Their anchor/bearing is not shifted; the repositioned distance label remains visible.
This prevents the symbol itself from drawing across subtitle letters at enlarged UI scale.

Earlier `ObjectiveHUDClearance` / `WheelResizeAcceptance` runs exposed the compact-window
overlap and are not acceptance evidence. A diagnostic attempt stopped on an unavailable Python
accessor; the corrected bounds probe confirmed valid slot geometry before the coordinate fix.
No map or gameplay asset was saved in this increment; protected M12's hash is unchanged.
