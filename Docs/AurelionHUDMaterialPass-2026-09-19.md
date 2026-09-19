# Holographic HUD material pass, 19 September 2026

## Reference housing follow-up

The subsequent `author_hud_reference_housings.py` pass replaces the 56-tick arc with eight broad inset cells, a translucent dark body, bevel lines and a central split with chevrons. Radar now has a dark circular backing, three rings, cardinal ticks and a forward-facing player arrow. New `M_SovPlateHousing` and `M_SovAmmoHousing` UI materials supply angular backings without changing the widget tree. Plate padding was adjusted to fit the shield and health bars into those housings.

ShieldBar and HealthBar now have UMG property bindings to `View.Palette.ShieldTo` and `View.Palette.HealthTo`. Both compiled in the designer and were explicitly saved through EditorAssetLibrary; initial toolbar save attempts had not written the asset. Other colors are still fixed and need palette bindings. These two bindings are not yet proven through protagonist switching in PIE.

The normal editor pass is recorded in `Saved/Validation/Aurelion/HUDDesigner-20260919-082919-f568ce0a/ReferenceHousings/result.json`; all ten native bindings still match. Designer inspection confirms the broader arc, backed radar and angular housings render. This is not a gameplay capture or reference-match qualification. The first component-mask connection attempt failed before saving; changing its input to the default unnamed pin resolved it. Intermediate graph reconstruction produced missing-input shader warnings, so a fresh-process load must verify the completed graphs separately.

Remaining reference work includes the named cap, health/shield glyphs, six functional ability pips, actual radar contacts and sweep, full palette/high-contrast handling, frame-edge detail, and proportions/overlap checks at runtime. In the designer, the arc still passes beneath the right side of the radar and the plate needs further contour refinement. The historical pass below describes the earlier state, superseded by this follow-up where noted.

Follow-up validation: fresh editor `HUDHousingVerify-20260919-084805-db8414d8` exited 0, verified all four material references and ten native bindings, and logged no material compile failures, Python errors or compiler ensures. Full validation `20260919-084931-06ec192b` passed the build invocation, all 719 tests and source-integrity check. The preceding `20260919-084615-a04738e3` run passed build/tests but was rejected because verification scripts changed during the run; it is not qualifying evidence. These checks still do not prove live palette switching or visual/gameplay acceptance.

This is an incomplete presentation pass on handoff task 2. It does not qualify the reference match or the 90% visual/gameplay goal.

`WBP_SovHolographicHUD` now uses a procedural UI material for the segmented Echo sweep, driven by the existing native `ArcFill` scalar `Fill`. The opaque rectangular Echo overlay is collapsed. The radar background is a transparent concentric reticle rather than a solid square. The survival bars have thinner shield/stamina strokes around a heavier health stroke, with transparent region backings. Their existing native value bindings and automatic region placement remain intact.

Two self-contained engine materials were added under `/Game/Aurelion/UI/HUD`: `M_SovEchoSegmentedArc` and `M_SovRadarReticle`. Neither references raster artwork or marketplace assets. The radar material contains no contacts; contacts must still be authored from the actual view array.

The first attempted tree rearrangement failed the binding check and was not saved. `AddWidgetToTree` recompiles/reinstances the existing Blueprint; retaining references across these operations produced `TRASH_` widgets and missing bindings. Its log contains compiler GUID ensures. The successful pass changes existing widget properties only. It does not modify C++ or either mission map.

Evidence:

- Baseline full build and all 719 tests: `Saved/Validation/20260919-081402-cbd6b357/summary.json`.
- Authoring and all ten native name/type bindings: `Saved/Validation/Aurelion/HUDMaterialPass-20260919-082402-37739a5e/hud-refinement.json`. Exit 0, no Python errors, material compile failures or compiler ensures in this successful run.
- Fresh editor reload: `Saved/Validation/Aurelion/HUDMaterialVerify-20260919-082505-70a718b4/hud-material-verification.json` passed, confirming both saved UI-material references and all ten native bindings.
- Post-edit validation: `Saved/Validation/20260919-082610-03178650/summary.json` passed the full build invocation (target up to date), all 719 matching automation tests and source-integrity comparison. This does not replace visual or live-data qualification.
- Backups of the original widget are inside each authoring run's `Backups/Content/Aurelion/UI/HUD` directory.

Next: implement the named identity plate, resource labels, ability pips, dynamic protagonist/accessibility palettes and live/memory radar contacts in UMG. Python reflection does not expose the editor binding structures. Use the Blueprint editor for that pass; do not add another runtime HUD owner or substitute decorative contacts. The current fixed colors are temporary, not evidence of either protagonist palette working. Verify both protagonists, Blackout, high contrast and UI scaling in actual PIE with `Shot showui` captures. The transparent backings are not yet suitable for high-contrast mode; its opaque palette binding is required before acceptance.

The lethal-floor cue remains the next handoff priority after HUD completion.

## Supplied visual references

The creator reattached the actual references after the material pass. They are retained as design references at `Art/References/Aurelion/HUD/Tarrik.jpg` and `Selene.jpg`; do not import them as flat HUD textures. Both were visually inspected in the conversation.

The current material pass is too skeletal. The references show dark translucent housings, a substantial beveled health plate, a thin separate shield trough, six inset pip cells, a named top cap, angular ammo housing, a backed circular radar with a directional sweep, and a substantial bottom band split by central chevrons. The arc has broad insets rather than the current 56 fine ticks. Match these silhouettes, stroke weights and proportions before calling this a reference-aligned pass. Tarrik's health is red inside warm gold trim; Selene's health is teal inside pale cyan trim. Preserve the large unobstructed center. Faint framing follows the screen edge; noisy plasma must not overwhelm those clean housings.
