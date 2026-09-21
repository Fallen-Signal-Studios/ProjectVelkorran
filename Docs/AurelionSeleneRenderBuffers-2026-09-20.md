# Selene runtime skin investigation — 20 September 2026

## Scope and result

Added `review_selene_face_streaming.py`, a controlled portrait diagnostic using
the public load of earned CP9 banks in an isolated profile. Only the PIE copy of
the completed GrammarPropagation camera is repositioned; Selene, resources,
materials, lighting and campaign proof are not edited. The camera is discarded
with PIE. Diagnostic render settings are restored before ending the session.

This check **does not reproduce the green face under FifthWitness chamber
lighting**, and does not fix it. The CP9 portrait is much darker. It must not be
used to claim that the green rendering has the same cause, or that campaign
gameplay passed. The next useful comparison is under the affected chamber
lighting with the same buffer instrumentation.

## Evidence

Reports and original frames are under `Saved/Validation/Aurelion/`:

- `SeleneFaceStreaming-20260920-175511-179ac2fe`: successful CP9 load, dark
  blue/purple lit face before and after `r.VT.Flush`; no visible correction.
- `SeleneFaceBuffers-20260920-175741-81f9c0f3`: `viewmode buffervisualization`
  was rejected. The files named buffer captures from this run are **lit images**
  and are not valid buffer evidence.
- `SeleneFaceBufferFlags-20260920-180436-09308f55`: corrected the diagnostic
  to `ShowFlag.VisualizeBuffer 1` with `r.BufferVisualizationTarget`. Actual
  base-color output has normal pink/tan skin, roughness and specular retain
  authored detail, and world normals are coherent. Lit output remains dark.
- `SeleneSubsurfaceIsolation-20260920-180734-cbf97c83`: successful capture of
  all seven buffers plus lit frames with SSS scale 0 and restored scale 1.
  Face metallic output is black (nonmetallic). Shading-model visualization is
  consistent with SubsurfaceProfile, using the local engine's color mapping.
  Disabling/restoring the SSS scale does not visibly correct this dark view.
  SubsurfaceColor is profile-encoded for this shading model and must not be
  interpreted as a conventional face albedo or tint.

All four portrait processes exited 0 without timeout; all completed reports
record unchanged M12/M13 file hashes. Their result is rendering evidence only.

## Inherited material audit

Extended `inspect_selene_face_materials.py` to record resolved scalar, vector,
texture and static-switch values in addition to local overrides. It also exports
the instance descriptions and asserts that baked textures exist before reading.

`SeleneInheritedMaterials-20260920-180235-25707a2d` wrote its complete JSON and
instance exports, then crashed in Python during editor shutdown (exit 3). The
report exists, but this is not a clean process pass. Resolved skin multipliers
are neutral; baked material and virtual textures are enabled, and the expected
baked textures are selected. No obvious inherited tint override was found.
This does not establish shader correctness in the affected chamber shot.

## Validation

Full `Validate-Unreal.ps1` without SkipBuild completed in
`Saved/Validation/20260920-181139-c4cd333e`: build, 722 matching automation tests,
coverage and source integrity passed. No native or production content changed.
Existing cinematic startup C++ approval remains pending; this rendering work
does not bypass that restriction or resolve the startup failure.

Map hashes remain:

- M12: `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`
- M13: `CCAF63F21731315B8A19644D455F439222D3AE4F2577A826FB0D1C807CCE52CB`
