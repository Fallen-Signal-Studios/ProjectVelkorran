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

The subsequent source closure adds physical-output ownership: the real native window must overlap one uniquely identified monitor most strongly; ties, missing IDs and a missing/minimized window reject admission. A transient hash of selected monitor ID and display topology accompanies the receipt. Polling catches window movement even between equal-capability displays; the native display-metrics subscription invalidates unplug/replug with otherwise identical metadata. Identities are not saved or exported.

Narrative input/audio setters persist immediately. `USovGameUserSettings::SaveSettings` temporarily supplies the previously confirmed HDR fields while saving an active preview. The live display remains in preview. A save callback cannot confirm while those fields are temporarily substituted. Confirmation persists the actual normalized engine output. No new HDR preference store competes with Unreal's existing `bUseHDRDisplayOutput`/`HDRDisplayOutputNits` fields.

Rollback restores those confirmed config fields independently of physical output capability. If rendering or the display disappears before rollback, the hardware call may be unavailable; a later settings save still writes the confirmed values, never the unconfirmed preview. Save masking remains active throughout physical rollback, with receipt actions blocked by the transaction guard, and the receipt is retired after the confirmed fields are restored. Failed preview application follows the same config guarantee.

Haptic preferences are separate local config values. The portable gameplay payload remains exactly eleven bytes and never imports vibration choices, HDR output or diagnostic consent. Older configs receive bounded channel defaults. Invalid haptic config fails to a muted master instead of enabling malformed output.

## Full calibration source closure

`PreviewHDRDisplay` extends the same transaction with `FSovHDRCalibration`. Black floor is bounded to 0.000001–1 nit, paper white to 80–500 nits and UI white to 80–500 nits; nonfinite values are rejected. The renderer adapter maps black floor to `r.HDR.Display.MinLuminanceLog10`, paper white to 18% gray via `r.HDR.Display.MidLuminance`, and UI white to `r.HDR.UI.Level` relative to `r.HDR.UI.Luminance`. The defaults preserve the renderer's original 15-nit gray reference and 300-nit UI reference; no new calibration is applied until explicitly confirmed.

All three controls must exist and accept game-setting priority; the compatible `r.HDR.UI.CompositeMode=1` path must already be enabled. Missing/locked/unsupported controls reject full calibration instead of reporting an inert success. A failed partial write restores still-owned fields, including a later field that became immutable during an earlier setter callback. Revert preserves a later different field override; confirmed config is restored even when no hardware write is possible. The native frontend provides actual adjustment/preview/confirm/revert controls. Perceptual reference imagery, engine mapping and panel luminance still require target-build review.

Full calibration rechecks compositor capability and its UI-luminance reference before confirmation and on the preview ticker. Rollback restores each still-owned CVar independently even if another field was overridden or the compositor is no longer compatible. The receipt captures raw before/applied UI gain, so changing the base luminance cannot strand the preview gain. Switching the native menu to SDR uses the output-only preview instead of incorrectly requiring an HDR calibration payload. `ProductionCVarOwnership` exercises the actual adapter with isolated registered CVars (not the workstation's renderer variables), covering priority conflicts, compositor loss, reference changes and synchronous setter callbacks. That UE regression source is not an executed engine result.

Primary references: [Epic HDR output](https://dev.epicgames.com/documentation/en-us/unreal-engine/high-dynamic-range-display-output-in-unreal-engine?application_version=5.7), [renderer CVar reference](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-console-variables-reference?lang=en-US), [native window state](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/ApplicationCore/FGenericWindow), [monitor identity and geometry](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/ApplicationCore/FMonitorInfo). Rolling documentation may describe a newer renderer; actual UE5.7 headers and live CVar availability are authoritative.

## Validation and remaining work

The new portable suite executes the production mixer under C++17 warnings-as-errors and UBSan. It covers priority/preemption, same-priority mixing, per-channel/mute behavior, expiry, stale cancellation after reset, full capacity, nonfinite/out-of-range rejection, and 5,000 mixed requests with bounded output and storage.

Unreal automation under `ProjectVelkorran.Campaign.PlatformOutput` covers HDR preview persistence/reentry/normalization, failure/timeout/device loss, request ownership and mute behavior, portable-settings privacy, real damage/interaction/sequence/death/readiness delegate routing, plus `FullCalibrationAndDisplayIdentity`. The latter covers complete calibration, save masking, equal-capability monitor changes, display-metrics invalidation, external field overrides, unavailable output, missing renderer and nonfinite inputs. Output adapter seams do not change the test machine's display or vibrate its devices. These tests have not been compiled or executed here because UE 5.7/UHT/UBT are unavailable. Production `SovDisplayPolicyTests.cpp` passed 10,064 finite/calibration/viewport overlap checks with strict C++17 and UBSan.

Before completion on a shipping target:

1. Build/UHT and execute all engine tests, then test actual controller output at 30/60/unlocked frame rates and with disconnection/reconnection, pause, slow motion, death, retry and cinematic skip.
2. Exercise the native controls and author/review perceptual calibration imagery. Verify zero-intensity accessibility choices across every native and authored producer. Existing third-party direct force-feedback calls must also use the router if they are intended to obey these semantic channel controls.
3. Verify supported HDR displays, window/fullscreen transitions, equal-capability monitor moves, OS HDR changes, UI compositor mapping and rejected device output in the real UE 5.7 target build. The returned nits are Unreal's selected tone-mapping/output level, not the panel's measured peak. Platform system-calibration preferences and certification require the approved target integration.

The [subsequent source closure](SourceEngineeringClosure-2026-09-05.md) adds configured account/cloud adapters, in-game narration and optional engine controller-audio routing. It does not certify a provider, external OS screen reader, proprietary controller SDK or measured performance.
