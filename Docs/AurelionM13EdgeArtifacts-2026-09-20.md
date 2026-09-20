# M13 chamber edge-artifact investigation

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
