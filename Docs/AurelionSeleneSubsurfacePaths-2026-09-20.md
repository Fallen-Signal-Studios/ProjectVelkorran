# Selene render-path comparison

The purple saved-map portrait from `DepartureFillSavedPlayback-20260920-185910-b5855260`
remains an unresolved failure. No character material, renderer configuration or map
was changed in this investigation.

`review_selene_subsurface_paths.py` restores earned CP9 through the public checkpoint
loader and captures one fixed portrait while independently overriding subsurface
scattering, MegaLights permission, SSS resolution, checkerboarding and profile-ID cache.
It restores the original values between comparisons, verifies console readback, and
waits for screenshot rendering before each change. The character's idle continues;
these are not pixel-identical pose comparisons.

Evidence under `Saved/Validation/Aurelion`:

- `SeleneLitDepartureBuffers-20260920-190642-85f4588f`: clean exit 0, no timeout,
  unchanged map hashes. The inspected baseline and restored lit portraits both have
  natural skin. Thus a fresh launch can render the saved departure lights correctly.
- `SeleneSubsurfacePaths-20260920-190956-eec135be`: clean exit 0, no timeout,
  unchanged maps and verified restoration. All seven portraits were visually reviewed;
  none reproduced purple/green skin. Scattering-disabled changes fine surface appearance
  but is **not** a demonstrated repair. The full-resolution step repeats the baseline
  value, so it is a control, not a change in rendering resolution.
- `SeleneSubsurfacePathsRepeat-20260920-191353-54940160`: second fresh process,
  clean exit 0, no timeout, unchanged maps and verified restoration. Inspected baseline
  and final restored frames both have natural skin. This repeat also failed to capture
  the discolored baseline; it is not repair evidence.

Runtime baseline: `r.SubsurfaceScattering=1`, `r.SSS.HalfRes=0`,
`r.SSS.Checkerboard=2`, `r.SSS.Burley.EnableProfileIdCache=1`,
`r.MegaLights.EnableForProject=0`, `r.MegaLights.Allowed=1`, `r.SceneColorFormat=4`.
The project setting does not alone exclude a post-process override of MegaLights.

Epic support confirmed a 5.7.3 MegaLights/subsurface-profile color issue in
[this report](https://forums.unrealengine.com/t/megalights-turns-metahumans-yellow/2708790).
It is a lead only: this project defaults MegaLights off, and a healthy baseline cannot
show whether any override repairs the intermittent defect. The linked engine fix is
not publicly accessible through the available browser. No engine modification or
quality-reducing workaround is justified by these results.

Next useful evidence must capture the **faulty** lit portrait and its buffers/settings
in the same live process. Do not count additional healthy launches as a fix or continue
retinting the authored skin based on them. The wider visual/gameplay goal remains active.

Validation: full build and 722 matching automation tests passed without SkipBuild in
`Saved/Validation/20260920-191806-e81db91e`; coverage passed and native sources stayed
unchanged during the gate. Prior gate was `20260920-190144-49587240`.
The new script also passed Python syntax parsing. These checks do not qualify the
intermittent rendering defect as fixed.
