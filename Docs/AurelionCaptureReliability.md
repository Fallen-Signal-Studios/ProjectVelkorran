# Aurelion editor capture reliability

The Crucible cargo review exposed different lighting in early and late captures from the same camera. This investigation isolates screenshot preparation before using those images to justify more asset or lighting changes. It does not qualify runtime rendering, final art or performance.

## Engine behavior and scope

UE 5.7.4 source `AutomationBlueprintFunctionLibrary.cpp:1223` shows that `TakeHighResScreenshot` finishes pending loading and requests a high-resolution screenshot. `UnrealClient.cpp:1503` creates a temporary viewport; at line 1551 it reads `r.HighResScreenshotDelay`, with a default of four frames, then renders that many frames. Waiting seconds before the request is distinct from rendering those preparation frames.

The observed editor settings were screenshot delay 4, screen percentage 100, anti-aliasing method 4, Nanite enabled, Nanite maximum pixels per edge 1, streaming pool 512 MB and virtual shadows enabled. This investigation changes only the screenshot delay and restores it afterward. Realtime viewport control was inspected in the engine API but was not changed or established as a cause.

## Controlled comparisons

`Z08CaptureWarmup-20260914-154520-8768fb80` produced six 1600 × 900 images: entry at 4, 32 and 64 preparation frames, a return to 4, then cargo detail at 4 and 64. The early entry was substantially darker. Returning to 4 after the longer captures retained the brighter lighting, so a simple sequential 4-versus-64 comparison alone would confound frame count with accumulated scene settling.

`Z08CaptureWarmupReverse-20260914-155004-d22f35c4` began a fresh editor session with 64 frames, followed by 4 at the same entry camera, then detail at 64 followed by 4. The first entry capture already showed the brighter, more settled presentation. Both runs completed without Python errors, preserved all 3,140 actor/collision states, destroyed the temporary camera and confirmed unchanged map SHA-256 `b90a6bfbd2d77e90f6a7abc2d1393abf3198ed4f81fab155117162fe8dd5f5cb`.

Raw RGB differences measured with `Scripts/Validation/Aurelion/measure-capture-drift.py`:

| Same-camera pair | Mean absolute channel difference, 0–255 | Pixels with any channel difference > 8 |
| --- | ---: | ---: |
| Original first 4 → later 4 | 41.34 | 78.02% |
| Original 64 → later 4 | 1.86 | 4.68% |
| Fresh reverse-order first 64 → following 4 | 3.45 | 14.58% |
| Fresh reverse-order detail 64 → following 4 | 2.36 | 5.02% |

These are observations, not acceptance thresholds. The reverse-order control supports using 64 preparation frames for art captures. Differences remain, and pixel drift does not identify every rendering subsystem involved or prove live stability.

## Shared review change

`review_eclipse_wall_scars.py`, also reused by the current cargo, guardrail and decking/light review scripts, now selects 64 screenshot preparation frames. It records the chosen and restored settings in `review-capture-settings.json`, restores the original console value on completion or capture failure, and removes its temporary camera after successful captures. No project renderer setting, material, light or map asset is changed by this policy.

Older captures remain historical evidence with their original limitations. Fine-edge aliasing, seams, material finish, room lighting design, furnishings and live traversal/combat still require independent review. This correction does not raise the supported 63.75% TDD estimate or complete the full environment goal.

## Revised-driver check

`Z08CapturePolicyReview-20260914-155358-8f0d7db6` completed without Python errors and produced four inspected images through the actual shared driver. Its receipt confirms 64 preparation frames, restoration from 64 to the original 4, and 3,140 actors after camera cleanup. The map SHA-256 remained unchanged. First-to-return entry mean absolute channel difference was 4.88/255, with 26.91% of pixels exceeding an eight-level difference in at least one channel. The large dark-to-bright shift is reduced, but the remaining difference prevents a claim of deterministic or fully settled rendering. No architecture regression count is added for this workflow-only change; the unchanged map retains the preceding 63-check receipt.
