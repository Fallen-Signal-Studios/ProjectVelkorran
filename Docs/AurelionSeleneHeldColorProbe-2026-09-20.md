# Held live Selene color diagnostic

The intermittent purple/green/gray face remains unresolved. No material, texture,
renderer configuration or saved map was changed by this probe.

`Scripts/Validation/Aurelion/probe_selene_live_color.py` loads the unchanged earned
CP9 banks through the public save API. It checks the restored journal, evidence,
identity, inventory, resources and departure locations before holding a portrait.
It can repeat that load in the same editor process and retain a faulty face for
rendering comparisons. A temporary camera is reframed after the restored head
pose settles; it is removed before travel and cleanup.

The run directory's `color-command.json` accepts `reload`, `isolate`, `ui`, `scene`
or `stop`. The latter capture-mode commands hold the same scene and camera while
choosing full-window UI or scene-only screenshots.
The operator inspects each captured portrait before selecting an action. There
is a three-minute command deadline, a six-portrait limit and a twenty-minute
overall probe deadline. Isolation captures BaseColor, restored lighting, SSS off,
MegaLights disallowed, profile-ID caching off and checkerboarding off. Each change
is followed by restoration and console readback. No override is saved.

Evidence under `Saved/Validation/Aurelion`:

- `SeleneLiveColor-20260920-222258-41aec696`: rejected Python API call before
  capturing any portrait; corrected to use the exposed GameplayStatics API.
- `SeleneHeldColor-20260920-222453-e37dd7f3`: first portrait healthy, second
  incorrectly framed before the head pose settled. The second capture is not
  usable skin evidence. Stopped without renderer overrides.
- `SeleneColorFramed-20260920-222930-126f8d87`: one correctly framed healthy
  portrait, then clean operator stop. No renderer overrides.
- `SeleneColorIsolation-20260920-223358-4f4ae29e`: four correctly framed healthy
  portraits across the first public CP9 load and three additional reloads in one
  process. This does not reproduce or repair the historical failure. The isolation
  sequence is a diagnostic-control check on healthy skin, not repair evidence.

That initial comparison exited zero without timeout. All four public load callbacks succeeded;
both map hashes were unchanged. All ten isolation frames were written, original
console values were restored with readback, and BaseColor, SSS-off and final-restored
frames were inspected. The final lit face remains natural. This validates those
diagnostic controls, not a production rendering correction.

The held portraits record assigned material-instance inheritance and parameter
arrays. They do not expose GPU profile-table contents or prove their correctness.
The character continues idling, so frames are not pixel-identical pose comparisons.
This work does not establish general animation, AAA art, or 90% TDD completion.

## Faulty-state capture and rendering comparisons

`DepartureCofferLive-20260920-225212-eeb4ca21/selene-background.png` exposed a
black face in a wider 80-degree gameplay view. `probe_selene_gameplay_color.py`
holds that exact camera at `(1350, 48200, 170)`, yaw -90. It does not move Selene.

`SeleneGameplayColor-20260920-225533-35c5eda9` rendered natural skin at that pose
and exited cleanly on operator stop. The subsequent
`SeleneCapturePath-20260920-225949-b2bed1b9` captured black skin at the identical
camera. In this second process, all three baseline captures—scene-only, full
editor UI, scene-only again—show the black face while hands remain normal.
Thus this reproduction is not explained by the screenshot UI flag. A healthy
wide-camera run also prevents treating field of view alone as a demonstrated cause.

Exposed LOD readback matches between those healthy and faulty runs: forced face
LOD model 0, minimum LOD model 0, body/torso/hair sync text at 0. The face's actual
predicted LOD property is not exposed in this report, so this is not complete LOD
qualification. Material-instance parameters and profile paths match after stripping
native object addresses from the comparison; unnormalized profile strings naturally
contain different process addresses.

The faulty scene remained live through all ten rendering comparisons:

- BaseColor shows black face pixels, with normal body skin. Restoring lit rendering
  retains the black face.
- Disabling subsurface scattering, disallowing MegaLights, disabling Burley profile-ID
  caching and disabling SSS checkerboarding each leave the face black.
- The intervening restored frames and final restored frame retain the same failure.

BaseColor, restored lit, all four changed rendering paths and final restored images
were inspected. The complete run exited zero without timeout and restored all console
values. These results provide the previously missing **faulty-state** evidence; they
do not justify disabling those features as a repair. The next investigation should
follow the face material's actual color inputs and virtual-texture sampling during
the failure, rather than repeating healthy-launch SSS comparisons. No material or
renderer production change was made.

Validation: full build without SkipBuild, all 726 matching tests, report coverage
and source integrity passed in `Saved/Validation/20260920-230607-4f9b6952`.
The preceding full baseline was `20260920-221732-79c3404f`. Automation passing
does not override the observed black-face failure.

## Reversible material comparisons

`probe_selene_material_binding.py` opts into temporary face-slot comparisons:
VT cache flush, original-material rebind, a dynamic instance without overrides,
and the existing simple `M_SeleneFace` texture sampler. Original assignments are
retained and restored on stop, error or checkpoint travel. The normal portrait
probe does not enable these commands. No material asset is saved by the probe.

`SeleneMaterialBinding-20260920-231337-896cd4f2` showed healthy skin initially,
after VT flush and after a public reload. It eventually reached its operator
command timeout. Its healthy flush does not qualify a faulty-state repair.

`SeleneSamplerBinding-20260920-232341-b033a082` verified all five portraits:
healthy baseline, simple sampler, restored original, dynamic instance, original
rebind. The simple sampler displays the expected tan texture with different
surface shading; the original, restored and dynamic frames display normal skin.
The run exited zero on operator stop, reported no error and verified original
material assignments restored. This qualifies the diagnostic operations and
cleanup, not a production fix or a faulty-state comparison.

`SeleneBindingRepeat-20260920-232840-18d6e6b5` failed before any portrait at the
existing earned-position check. Selene was at X=1341.599 instead of 1350 cm and
moving at -148.932 cm/s on X; Tarrik remained at the expected exit. The check
was not weakened. This separate transient movement sample needs follow-up;
it is neither a skin reproduction nor proof that the final settled position
is wrong. The earlier black-face evidence remains unresolved.

The completed material-probe changes passed the full build and 726-test gate
`20260920-234211-dbea61d3`, including report coverage and source integrity. Later
`DepartureSeatLive-20260920-233909-bee76ae5` passed the same earned-position check
and showed normal skin; it does not erase the earlier transient movement sample.
