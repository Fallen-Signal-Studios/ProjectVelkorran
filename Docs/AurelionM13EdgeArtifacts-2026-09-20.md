# M13 chamber edge-artifact investigation

Current disposition: the stable earned-checkpoint PIE view did not reproduce
the editor artifacts. See the live-frame evidence below before changing assets.

The saved Crownmark preview shows broken dark edges around wall mouldings and
small floor details. This pass did not fix that artifact. It tested proposed
lighting causes without saving a map, light, material or renderer configuration.

Evidence under `Saved/Validation/Aurelion`:

- `M13ShadowComparison-20260920-044510-a01e295d`: fixed-camera baseline, local
  virtual-shadow resolution bias -2, then ShadowQuality 0. Increasing resolution
  did not visibly resolve the broken edges. Disabling shadows removed some
  shading but left the wall-edge artifact. The light census records the original
  settings, including VSM enabled, local bias 0, TSR method 4, 100% resolution,
  and zero-radius chamber spot sources.
- `M13CaptureHistory-20260920-044837-4ad2fab3`: fresh-process 64-frame screenshot
  preparation followed by 4. The artifact remains visible in the first prepared
  image; it is not resolved by screenshot preparation alone.
- `M13SoftSourcePreview-20260920-045049-ec9e1bc4`: eight uplights temporarily use
  30 cm source radius at unchanged intensity and shadow casting. The chamber and
  close wall captures did not establish sufficient improvement to keep this
  lighting change. No map was saved.
- `M13WallOcclusion-20260920-045339-8969451e`: close wall baseline versus disabled
  Lumen short-range AO, diffuse-indirect SSAO and ambient occlusion levels. The
  broken edges remain. This does not rule out every indirect-lighting subsystem.

The reusable M13 capture driver now follows the existing documented 64-frame
preparation policy and restores the original value after completion or callback
failure. The soft-source preview exercised that driver: its receipt records
64 frames, restoration to 4, removal of the temporary camera and 1,315 actors
after cleanup. This is a capture-workflow correction, not a runtime quality fix.

Next inspect the mesh-rendering path (including an isolated Nanite/full-fallback
comparison) and material/normal response. Do not apply a global shadow-resolution
increase or claim final visual acceptance based on these images. Static editor
captures do not establish moving-camera quality or performance.

Full gate `20260920-045436-3fb743c4` passed the build, 719 matching automation tests,
coverage and source integrity without SkipBuild. M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The prechange gate was `20260920-044133-a90ec85f`. Both map assets remain unchanged
by this investigation. No packaged build was performed.
# Live-frame disposition, 20 September

The broken wall/floor edges from the editor previews were **not reproduced in
the stable restored M13 game view**. `M13CaptureComparison-20260920-055639-6eca186f`
loaded the unmodified earned CP9 banks through the public save owner, confirmed
the success callback, and captured ordinary / high-resolution / ordinary frames
from one camera at the same 1,696 x 862 resolution. All three were inspected:
wall frames, the new supports and ring bands, and floor seams remain substantially
cleaner than the editor captures. The high-resolution sample used the same
64-frame preparation as the editor preview, so the high-resolution API alone
does not explain the earlier difference. The original delay of four was restored.

Live settings were TSR (`r.AntiAliasingMethod=4`), screen percentage 100,
shadow quality 5 and Nanite enabled. Camera position was (0,33600,-1570), yaw 90,
FOV approximately 80. The review temporarily moved only the PIE copy of the
existing GrammarPropagation camera; no map, mesh, light or renderer setting was
saved. Normal window inspection in the preceding successful run
`M13EarnedFrameReview-20260920-055226-82b593c3` also showed Tarrik's live HUD.

This does not prove every moving view is artifact-free, nor identify the exact
editor/game difference. It changes the next action: do not continue modifying
assets or global rendering to address this editor-only observation without a
corresponding game-view reproduction. Use the live-frame review for visual
acceptance and editor captures for placement. The earlier experiments below
remain historical evidence, not a demonstrated live gameplay rendering defect.

Harness: `Scripts/Validation/Aurelion/review_m13_live_frame.py`. Run visibly
through `run-editor-script.ps1` on M12; direct M13 login intentionally rejects a
missing inactive protagonist kit/anchor. The harness copies only the two real
checkpoint banks into its isolated profile and verifies their hashes. It does
not manufacture journal receipts or teleport the pawn. The first direct-entry
attempt was stopped after that native rejection; the first checkpoint attempt
restored successfully but failed on an unavailable Python spawn API. The final
harness uses an existing cinematic camera and ends PIE before editor shutdown.

Full gate `20260920-055955-7f4511a0` passed the build, all 719 matching automation
tests, report coverage and source integrity without SkipBuild. M13 was unchanged
on disk; M12 retains its protected SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
The newly present untracked `Plugins/GASPALS` directory was not modified or staged.
