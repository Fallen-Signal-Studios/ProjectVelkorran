# M12 star ray rendering repair

Recent M12 loads repeatedly reported `MID_MI_Rays_0` using additive blending on
the Nanite mesh `Meshy_AI_splitting_star_game_r_0909162327_texture`. The engine
explicitly rejects this material/renderer combination. The ownership audit located
it on `BP_Star` / `BP_Star1`, component `SM_Rays`, rather than a weapon or character.

Set `disallow_nanite=True` only on `BP_Star1`'s `SM_Rays_GEN_VARIABLE` template.
The solid `SM_Star` remains Nanite-enabled; mesh assets, material assignments,
transforms, shadows and collision settings are preserved. This uses the supported
raster path for the additive effect. It is a compatibility repair, not a star redesign
or a claim of a large visible improvement.

The local asset is
`/Game/Space_Creator_Pro/Star_Creator/StarCreator_Update_1/Blueprints/BP_Star1`.
Marketplace content stays untracked under the project handoff. The reproducible
authoring script is `Scripts/Editor/fix_aurelion_star_rays_raster.py`; run it with
`Scripts/Validation/Aurelion/run-editor-script.ps1`, the local UE 5.7 engine root,
`-DisableAura` and `-UseFileSystemCache`. It backs up the original package, checks
all static-mesh component presentation/collision properties, saves only this
Blueprint and asserts both map file hashes are unchanged. It deliberately refuses
to reapply an already-fixed asset. Use `verify_star_rays_raster.py` for readback.

Evidence under `Saved/Validation/Aurelion`:

- `AdditiveMeshOwnership-20260920-192223-f91d4682`: read-only ownership report;
  process exit 0, no timeout.
- `StarRaysPreview-20260920-192456-3957b51e`: before/after editor captures from a
  fixed camera. Both inspected; the authored star and glow appearance are retained.
  The animated particles differ across capture times. Temporary component setting
  and screenshot delay restored; camera removed; no map saves. Exit 0, no timeout.
- `StarRaysSave-20260920-192725-d0e37f49`: exact component change saved, all other
  recorded properties preserved, both maps unchanged. Original package backup is
  `BP_Star1.before.uasset` in this directory. Exit 0, no timeout.
- `StarRaysReadback-20260920-192854-b4dacf22`: fresh editor and PIE both inherit
  rays `disallow_nanite=True`, body `False`, with the original mesh and materials.
  Report passed, exit 0, no timeout, maps unchanged. The entire fresh log contains
  zero `Invalid material [MID_MI_Rays` warnings.

Saved asset SHA256: `549CF982ED38AF55EC773343B901F0B3C6D3006B84169A71C032F04BA89323B7`.
This does not resolve Selene's intermittent skin tint or qualify the full campaign.

Full post-change gate `Saved/Validation/20260920-193027-01279e57` passed the build
without SkipBuild, all 722 matching automation tests, coverage and source integrity.
Baseline gate: `20260920-191806-e81db91e`. All four new scripts passed Python syntax
parsing. No packaged-build or GPU-performance qualification is claimed.
