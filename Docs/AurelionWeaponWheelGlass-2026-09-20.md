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
every-resolution certification. Remaining QoL: dialogue subtitles can overlap the lower wheel
and faction inscription in these viewports, especially with the opaque high-contrast wheel.
This increment preserves the stock centered wheel placement; it does not claim subtitle/menu
collision avoidance. The waypoint's own padded label continues to avoid the subtitle panel.

The protected M12 map is not saved; its SHA256
remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
