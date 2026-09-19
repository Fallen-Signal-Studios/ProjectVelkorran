# Holographic HUD material pass, 19 September 2026

## Reference housing follow-up

Health/compass validation: fresh editor `HUDHealthVerify-20260919-092013-4b1cbfdd` exited 0, verified the saved health brush references, Mask fill mode, all ten native bindings and active compass offset, with no material compile failures, Python errors or compiler ensures. The unused legacy HUD copy was restored byte-for-byte from its pre-edit backup after editor exit. Full validation `20260919-092119-35559ad7` passed the build invocation, 719 matching automation tests, report coverage and source-integrity check (95 automation warnings; packaged build not run). This is engineering validation, not reference-fidelity or full gameplay acceptance.

Health/compass follow-up: `M_SovHealthBeveledFill` supplies a grayscale energy gradient, bevel highlights and clipped angular ends. Both health brush layers use this shape, with Mask fill mode so depletion clips instead of stretching the ends. HealthBar now binds to `View.Palette.HealthFrom` for the deeper red/teal endpoint. This is not yet a two-endpoint palette gradient or a high-contrast-qualified brush. The existing widget hierarchy and ten native bindings are preserved.

Actual M12 PIE in `HUDHealthReview-20260919-090653-b19a05d6` shows Tarrik's red shaped health fill and, after correcting the active asset, a compass below the plate. `Tarrik-Health-Compass.png` is the `Shot showui` capture. The active frontend creates `/Game/Aurelion/UI/WBP_AurelionGameplayHUD`, confirmed by `live-widget-classes.json`; the controller's legacy GameplayHUDClass points to a different unused project copy. The initial placement attempt on that unused copy was reverted to its original top offset of 20. The active Aurelion compass now has top offset 148, retaining its original width, height, anchor and alignment. No map or C++ changes were made. Additional UI scales, Selene, health depletion in combat and high contrast remain unqualified.

The compass read-only probe initially failed because OverlaySlot does not expose `get_padding` in this engine Python API. It was corrected to read the `padding` property; the subsequent probe completed. This error happened after the health widget save and did not invalidate that saved binding.

Palette follow-up validation: `Saved/Validation/20260919-085830-07151a8b/summary.json` passed the full build invocation, all 719 matching automation tests, report coverage and source-integrity comparison. This qualifies the saved ArcFill/RadarDisc binding changes for this gate; it does not qualify Selene switching, live radar contacts, high contrast or reference fidelity.

Live palette follow-up: ArcFill and RadarDisc now bind Color and Opacity to `View.Palette.Accent` in UMG. Both bindings were compiled and saved in `HUDPalette-20260919-085107-cca6c339`. The actual M12 PIE entry with Tarrik shows warm amber radar/band colors, shield and health filled, and the otherwise open center. `Tarrik-HUD.png` in that run directory was captured using `Shot showui` and includes editor chrome. It is partial visual evidence, not combat or Selene qualification. The existing Narrative compass overlaps the health plate; health currently reads as a flat orange rectangle and needs a beveled gradient closer to the red reference. Ammo was hidden with the weapon stowed. No live radar contacts or ability pips have been authored yet.

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
