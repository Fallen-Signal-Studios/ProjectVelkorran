# Native controller feedback and HDR output

This closes the source integration portion of TDD v2 §13.9 for controller vibration channels and HDR peak-output calibration. It preserves `USovGameUserSettings`, Narrative's settings/interaction/cinematic events and Unreal's controller/display output owners. It makes no network requests and does not introduce an account or cloud adapter (§15.17).

## Controller feedback

`USovHapticFeedbackComponent` belongs to the existing player controller. `PlayFeedback` returns an exact positive receipt; zero means unavailable, muted, invalid or full. `CancelFeedback` cancels only that receipt. `CancelAllFeedback` retires every owned request and stops only the component's own engine handles. It never calls an unscoped stop that would cancel unrelated third-party feedback.

`FSovHapticSettings` persists a master intensity and five semantic channels: Combat, Interaction, Cinematic, Ambience and UI. Every scale must be finite and within 0–1. Higher priority wins within a channel; equal priority uses the strongest request. Final channel intensity is request × channel × master, bounded to 0–1. Different channels use Unreal's existing motor mixing. These are semantic controls, not a promise that every device has five physical motors.

The production-used portable mixer has sixteen fixed request slots and never reuses a receipt. Each request lasts at most five real seconds. There are at most five active native output handles, one per semantic channel. Output uses short 0.1-second engine leases, periodically renewed while a request remains admitted. An expired handle after a long frame is replaced. This avoids indefinitely running rumble when a component stops ticking. Zeroed channels immediately stop and discard outstanding requests; unmuting cannot resume an old pulse.

Native producers subscribe to existing events:

| Event | Feedback |
| --- | --- |
| Committed damage to the currently possessed avatar | Brief damage pulse; separate stronger patterns for shield/guard break and perfect defense |
| Narrative `OnBeginUseInteractable` | Brief admitted-interaction pulse, rather than vibration merely from aiming at an object or pressing a key |
| Narrative sequence play | Clears outgoing gameplay requests and submits a brief cinematic transition pulse |
| Matching sequence stop/skip | Cancels all cinematic requests attributed to that active sequence |
| Death, readiness epoch change, possession change, campaign transition, deactivation or EndPlay | Cancels owned output |

An authored cinematic `PlayFeedback(Cinematic, …)` request requires the currently active Narrative sequence and is attributed to it. A stale sequence stop cannot cancel its successor. Ambience and UI are available to their actual authored producers; this pass does not fabricate ambient or menu events. The added native getter for Narrative's protected interaction delegate exposes the existing event without a second interaction implementation.

The component observes local-controller ownership, Unreal's force-feedback enable preference, pause, death, controller input suppression and campaign-transition state. It rechecks pawn, ASC avatar and readiness epoch. Device delivery uses Unreal's native [`APlayerController::PlayDynamicForceFeedback`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/APlayerController/PlayDynamicForceFeedback) API. A request receipt confirms local admission, not physical controller presence or hardware acknowledgement. Unsupported devices retain Unreal's no-output behavior. Platform-specific adaptive triggers, controller speakers and proprietary haptic SDKs are outside this implementation.

## HDR preview and persistence

`GetHDROutputStatus` uses Unreal's `SupportsHDRDisplayOutput`, `IsHDREnabled` and `GetCurrentHDRDisplayNits`. `PreviewHDRCalibration` applies through `EnableHDRDisplayOutput`, then reads the engine-selected output level. The requested 400–2000 nit range is validated; Unreal may normalize to a supported output level. The UI must display the returned level and capability result, not infer success from the user's checkbox. These are engine output queries, not a photometric measurement of the physical panel. [Unreal game-user-settings API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameUserSettings).

Preview returns a unique receipt and expires after fifteen real seconds on the core ticker, including while gameplay is paused. Only that active receipt can confirm or revert. Unsupported HDR is rejected before output mutation. Failed apply restores the prior available output. Timeout, a changed/lost output or a stale confirmation reverts; unavailable HDR falls back to SDR. A stale receipt cannot affect a newer preview. Full settings application cancels preview and reapplies HDR after Narrative's monitor move, so capability/output selection uses the final window destination.

The change check compares output availability, HDR support, enable state and peak level. It does not capture a stable physical-display identity. An external OS/window move between HDR displays reporting identical values can therefore retain a preview receipt; explicit display-identity tracking and hotplug/window-move validation remain platform work. The project's own settings-based monitor move cancels preview.

Narrative input/audio setters persist immediately. `USovGameUserSettings::SaveSettings` temporarily supplies the previously confirmed HDR fields while saving an active preview. The live display remains in preview. A save callback cannot confirm while those fields are temporarily substituted. Confirmation persists the actual normalized engine output. No new HDR preference store competes with Unreal's existing `bUseHDRDisplayOutput`/`HDRDisplayOutputNits` fields.

Rollback restores those confirmed config fields independently of physical output capability. If rendering or the display disappears before rollback, the hardware call may be unavailable; a later settings save still writes the confirmed values, never the unconfirmed preview. Save masking remains active throughout physical rollback, with receipt actions blocked by the transaction guard, and the receipt is retired after the confirmed fields are restored. Failed preview application follows the same config guarantee.

Haptic preferences are separate local config values. The portable gameplay payload remains exactly eleven bytes and never imports vibration choices, HDR output or diagnostic consent. Older configs receive bounded channel defaults. Invalid haptic config fails to a muted master instead of enabling malformed output.

## Validation and remaining work

The new portable suite executes the production mixer under C++17 warnings-as-errors and UBSan. It covers priority/preemption, same-priority mixing, per-channel/mute behavior, expiry, stale cancellation after reset, full capacity, nonfinite/out-of-range rejection, and 5,000 mixed requests with bounded output and storage.

Five Unreal automation tests are provided under `ProjectVelkorran.Campaign.PlatformOutput`: HDR preview persistence/reentry/normalization, failure/timeout/device loss, request ownership and mute behavior, portable-settings privacy, and real damage/interaction/sequence/death/readiness delegate routing. Output adapter seams keep those tests from changing the test machine's display or vibrating its devices. These tests have not been compiled or executed here because UE 5.7/UHT/UBT are unavailable.

Before completion on a shipping target:

1. Build/UHT and execute the five engine tests, then test actual controller output at 30/60/unlocked frame rates and with disconnection/reconnection, pause, slow motion, death, retry and cinematic skip.
2. Wire channel controls and calibration imagery to these APIs. Verify zero-intensity accessibility choices across every native and authored producer. Existing third-party direct force-feedback calls must also use the router if they are intended to obey these semantic channel controls.
3. Verify supported HDR displays, window/fullscreen transitions, monitor moves, OS HDR changes and rejected device output in the real UE 5.7 target build. The returned nits are Unreal's selected tone-mapping/output level, not necessarily the display's measured peak. Black-floor, paper-white and separate UI-luminance calibration, system-calibration preferences and platform certification still need the actual shipping renderer/device implementation and authored calibration controls. This pass implements peak-output preview and rollback, not a complete perceptual HDR calibration suite.

Neither this work nor the previous source pass supplies certified account/cloud services, controller audio, platform screen-reader behavior, or performance certification. Those remain explicit engineering/platform integrations in the master audit.
