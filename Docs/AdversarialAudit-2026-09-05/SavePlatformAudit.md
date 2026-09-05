# Save, platform, and application lifecycle adversarial audit

Reviewed local HEAD `6225c68d50fbea605d7c7dfc8d61d5569805e28f` in `/workspace/scratch/4e000d64b13d/ProjectVelkorran`. Read-only runtime review. The only repository artifact added by this reviewer is `Docs/AdversarialAudit-2026-09-05/CloudClockOrderingEvidence.json`.

Authority: August 2026 TDD v2, especially 11.8–11.9, 15.9, 18.5, 18.9, and approved `Docs/CampaignV2ChangeLog.md`. No engine, SDK, controller, account provider, platform storage, or native automation execution occurred. Source findings describe reachable control flow; platform callbacks and malformed archives still require native adversarial tests. “AAA” here means bounded resource use, recoverable failure, explicit ownership, cancellation correctness, coherent checkpoints, and demonstrable test gates, not a formal certification claim.

## Preserve these foundations

- Native two-bank generation selection, exact readback comparison, preserving previous valid banks, and corrupt-file archiving are appropriate. Do not replace them with another save system.
- Narrative capture preserves its configured `UNarrativeSave` subclass and candidate capture avoids replacing live records until accepted. Phased restore and required/optional record metadata are useful existing contracts.
- Standalone authority admission, stable mission/hero identity, checkpoint admission checks, and deferred failure delivery after world initialization are valuable.
- Platform authorization is now native-only on consoles. Blueprint account selection cannot invent native authorization at entry. Console generic OSS cloud is correctly disabled pending real platform save integration.
- Immutable cloud uploads, byte-for-byte readback verification, and retention of canceled provider ownership prevent delayed callbacks from falsely completing newer same-file requests.
- Persistent GameInstance application events, named pause ownership, input release on interruption, and deliberate resume are the correct architecture. Extend their failure coverage instead of bypassing them.

## Findings

### SP-01: Native storage authorization is checked at entry rather than held through the actual I/O boundary

**Priority:** P1 hardening gate. **Evidence:** missing revalidation is source-proven; event-pumping/serializer witness must run in UE. Do not describe this as an observed cross-account data transfer.

**What exists:** `Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp:116–136` checks native authorization before selecting an account. `ObserveNativePlatformAccount:172–188` revokes `bPlatformStorageOwnerAvailable` while retaining the original namespace. `WriteEnvelope:407–460` checks suspension/availability at entry. This closes calls that begin after a revocation.

**Failure trace A:** `WriteEnvelope:409–414` passes for owner A, then `ReadBest:394–400` performs platform reads and `LoadGameFromMemory`. Loaded envelopes are accepted with `Cast<USovCampaignSaveGame>`, so a valid subclass's `Serialize` may execute code. A native revocation or suspension during a deserialization/platform callback flips availability/suspension while retaining A's namespace. The outer `bBusy` guard blocks selecting another account, but does not block `ObserveNativePlatformAccount` or `SetPlatformSuspended`. `WriteEnvelope` does not check either flag again before `Storage->Write:453` or readback and success at 456–460. `ValidateEnvelope` checks namespace/schema/checksum but not authorization. Thus an operation can perform storage I/O and report success after its original authorization was revoked. The same hole exists after `SaveGameToMemory:432` and target-bank deserialization at 441.

**Failure trace B:** `SelectPlatformUser` calls `PersistPlatformProfileHint:130`, which executes reads/writes at 159–168. There is no transaction guard or post-I/O authorization recheck in selection. A native owner revocation during that call can be overwritten by the unconditional `bPlatformStorageOwnerAvailable = true` at 135 when selection returns. This is a window of false availability under the retained owner, not proof that data was written into a different user's namespace.

**Counter-review qualification:** the current production writer constructs the exact `USovCampaignSaveGame` class at479; no envelope subclass currently exists in the repository. A subclass serialization witness requires deliberately crafted/imported bytes or a test fixture. The configured Narrative payload subclass is serialized before WriteEnvelope, whose entry checks already fence that earlier boundary. Normal automatic profile selection occurs inside `ObserveAccount`'s reentrancy guard, so a nested RefreshPlatformAccount during hint persistence cannot immediately apply revocation there. Failure trace B requires public SelectPlatformUser outside that observer, for example retrying after an initial hint-persistence failure. The finding is a missing operation-wide authorization invariant, not a demonstrated routine save failure or cross-account transfer.

**Related boundaries:** cloud-import archival writes at 269–275 bypass a fresh per-I/O authorization check. `ReadBest` checks authorization once before reading both banks. `LoadSlot:626–635` performs asset loads and configured Narrative deserialization, then travels without rechecking the initiating owner/suspension after callbacks.

**TDD:** platform-account save ownership, no false success, suspend/resume and account-change testing. The console pass already applies owner predicates before/after serialization to mission travel; ordinary save banks should provide an equivalent guarantee.

**Disposition:** extend existing subsystem with one immutable operation owner token containing namespace, local platform user, authorization generation, and suspension generation. Increment it on revocation/selection/suspension. Capture the token before work, and validate after every callback/serializer/required-asset load and immediately before each storage mutation. Revalidate selection after profile-hint persistence before publishing availability. Distinguish a write already issued before revocation from a write whose admission was never valid; do not claim the former could always be rolled back.

**Dependencies/risk/order:** first save/platform hardening slice; medium implementation risk because retry, import, profile selection, and pending-load ownership share this subsystem. Preserve bank naming and existing source data. Additive operation token does not require a new save schema.

**Required validation:** add fake storage callbacks at `Exists`, each read, each write, and readback; trigger `ObserveNativePlatformAccount` via fake adapter plus `RefreshPlatformAccount`, and `SetPlatformSuspended(true)`. Add a test `USovCampaignSaveGame` subclass whose actual `Serialize` revokes ownership during read/write and a configured `UNarrativeSave` subclass for load preflight. Assert no *subsequent* write after revocation, old good bank unchanged, no success notification, and account selection never republishes availability after revocation. Test A→unknown→A and A→B with the namespace held by `bBusy`, so the test proves the real contract rather than assuming an impossible mid-transaction profile switch.

### SP-02: Loss of the original account has no native exit from the blocking recovery screen

**Priority:** P1 progression/recovery defect. **Evidence:** source-proven control-flow dead end when the original account is unavailable; actual controller navigation remains unrun.

**What exists:** account loss pauses and suppresses gameplay; `SovApplicationLifecycleComponent.cpp:180–186` permits resume only when storage ownership is available. `SovSaveSubsystem.cpp:209–212` correctly refuses adopting another account during an active campaign. `SovApplicationInterruptionMenu.cpp:31–35` constructs only a Resume button, and its header's `NativeOnHandleBackAction` consumes Back. The lifecycle component hardcodes this native widget class at 176–177.

**Failure:** owner A signs out and B is present, or A cannot reconnect. Resume cannot succeed. The account change requires returning to the front end, but this modal has no Return to title/front end action or explicit discard-session path. The player must restore A or restart the application. Merely implementing another screen in Blueprint does not supply an escape from the hardcoded blocking widget.

**TDD:** errors must preserve saves and offer actionable recovery; offline account changes and controller disconnect must not leave a hang.

**Disposition:** extend existing interruption menu with a deliberately confirmed Return to title route. It must abandon transient campaign/load/cloud work under an exact request token, preserve durable banks, tear down the outgoing world, and only then permit selecting B. Resume must continue to require A while the old session exists. Do not fix by relaxing `CanManagePlatformSaves` or accepting B into A's campaign.

**Dependencies/risk/order:** second slice, after SP-01. Reuse existing frontend/travel/recovery ownership rather than introducing a second session manager. Medium/high risk because rejected travel, pending load and save-failure pauses may overlap.

**Tests/definition of done:** native widget exposes actionable recovery for unavailable original owner; simulate signout + different user during Idle, failed checkpoint write and pending travel/load; user can return to a safe frontend without writing under B, with zero leaked input/pause leases. Process restart is no longer the only alternative to reconnecting A.

### SP-03: Application interruption does not freeze a travelling source world

**Priority:** P1 suspend/pause correctness. **Evidence:** source-proven broad pause exclusion plus source pawn preserved during travel. Device timing/actual damage must be tested natively.

**What exists:** `SovApplicationLifecycleComponent.cpp:167–172` allows initialization world timers to proceed, acquiring interruption pause only in `Idle` or `Failed`. It releases its pause in *every* other transition state. `ASovPlayerController::TravelToMission`, `SovPlayerController.cpp:625–665`, sets `Travelling`, retains its current source pawn and locks input while waiting for ServerTravel. `SetTransitionInputLock:173–183` only changes move/look input and legacy save admission. It does not stop simulation, hazards, projectiles, damage or existing abilities.

**Failure:** a system overlay/background event arrives after travel acceptance but before source-world replacement, or async travel stalls. The lifecycle layer suppresses input but leaves world simulation running because the transition is `Travelling`. It can also release a previously owned interruption pause when a callback enters a transition. The justification “no ready protagonist” is not true for this state. Active encounters are rejected at `CanTransitionTo:147–150`, but this does not freeze environmental damage, projectiles or unregistered threats. Source progress can change while the application is supposed to be held.

**TDD:** suspend/resume stability, no unsafe protagonist transition, consistent campaign snapshots. Input lock alone is not simulation pause.

**Disposition:** separate transition control work that genuinely requires tick from game simulation. Keep the existing named interruption pause on source worlds with a live ready pawn. Drive necessary orchestration from an explicitly allowed tick/clock or quiesce all relevant simulation under the transition contract. Narrow initialization exemption to a verified pawn/world readiness phase rather than all non-Idle states. Coordinate with campaign audit's missing asynchronous travel-failure recovery finding.

**Dependencies/risk/order:** second slice alongside SP-02 and campaign travel recovery; high regression risk if world timers needed for pawn readiness are paused indiscriminately. Test both sides of the contract: transition initialization must complete, and suspended source gameplay must not simulate.

**Tests/definition of done:** a native viewport test enters `Travelling` with a living pawn and ticking damage hazard, opens an overlay and delays or rejects asynchronous travel; no health, AI, projectile, timed-choice or encounter state advances under the hold. A normal destination initialization still reaches readiness after resume. Verify all transition enum states rather than only Idle.

### SP-04: Cloud revision discovery can hide the most recently published save after clock correction

**Priority:** P2 reliability. **Evidence:** source-proven, with host ordering reproduction. **Scope:** desktop optional generic cloud; console generic cloud is disabled.

**What exists:** `SovOnlinePlatformServicesAdapter.cpp:213` selects one remote filename by lexicographic maximum. Filenames use each publishing client's `FDateTime::UtcNow().GetTicks()` at 219–220, followed by a GUID. The UI subsequently offers only that one cloud revision through `ReadLatest`; readback verification confirms bytes, not global publication order.

**Counterexample:** first upload from a clock ahead produces `SovCloud1_0_0_0639343584000000000_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.sav`. After clock correction, a later acknowledged upload produces `SovCloud1_0_0_0639242496000000000_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.sav`. Both satisfy the source filename shape. Every later discovery chooses the first upload. See `Docs/AdversarialAudit-2026-09-05/CloudClockOrderingEvidence.json`, generated by checking the production selection expression and reproducing only its string ordering in the host runtime. This is not an OSS execution result.

**TDD:** explicit conflict handling, no hidden choice that loses the user's latest progress. Keeping both remote files avoids destructive data loss, but makes the newer one inaccessible through the shipped review path.

**Disposition:** preserve immutable uploads/readback. Expose verified revision history with mission/time/device/publication metadata and explicit selection, or use a real provider-authoritative revision/compare-and-swap mechanism. Do not simply replace local-clock sorting with local save generation: independent devices can produce overlapping generations. Also expose actionable revision management before the existing 32-per-slot cap at 217–218; its current “platform storage tools” message is not a demonstrated management path for each desktop provider.

**Dependencies/risk/order:** third platform slice after native ownership and session recovery. Medium risk, mostly adapter/review model/UI; no need to migrate local banks. Test multi-device clock skew, backwards clock, duplicate generation, concurrent publication and corrupt highest-sorted revision. Definition of done: all retained valid cloud copies remain discoverable and no clock can permanently mask subsequent saves.

### SP-05: Corrupt native save input reaches Unreal deserialization before a bounded integrity preflight

**Priority:** P1 hardening gate, **not a reproduced engine crash**. Missing boundary is source-proven; exact malformed-archive allocation/assert behavior depends on UE5.7 and must be fuzzed.

**What exists:** `ReadBest`, `SovSaveSubsystem.cpp:394–400`, reads an entire local file and calls `UGameplayStatics::LoadGameFromMemory` before any size, schema, checksum or class validation. `ValidatePlatformSnapshot:236–240` limits total cloud envelope bytes to 64 MiB but still constructs/deserializes its save class before checking `HasValidIntegrity`. `USovCampaignSaveGame::HasValidIntegrity`, `SovCampaignSaveGame.cpp:20–22`, validates the payload only after variable-length header fields, asset arrays and payload arrays were deserialized. `DecodeNarrative:554` then deserializes another configured class from that payload.

**Concern:** torn/malformed data is expected under the TDD, and a post-deserialization CRC cannot protect the deserializer's allocation or class-resolution boundary. Existing tests truncate serialized blobs and verify bank fallback, but do not exercise corrupted length fields, unexpected save-class identifiers, large record maps, or malformed nested payloads. No claim is made that UE necessarily crashes on every such input.

**Disposition:** retain the Narrative payload and bank scheme, add a small fixed outer frame containing magic/version/declared lengths/checksum of raw bytes, validate size and framing before loading any UObject class, and allow only the configured envelope/Narrative class family. Add explicit record/component/payload count limits before application. If a new outer format is adopted before first external v2 release, document the version boundary; otherwise provide deterministic migration from existing banks rather than treating real saves as corrupt.

**Dependencies/risk/order:** first save hardening slice with SP-01 or an immediate follow-up, before product save compatibility is committed. High schema/migration risk if deferred until after release. Engine-side bounded archive fuzzing is a prerequisite to calling this closed.

**Tests/definition of done:** fuzz outer bytes and nested count/length fields, truncate at every offset, mutate save-class names and required/optional record metadata, and include max-size envelopes. Loads must finish with a bounded error and preserve both source banks; no assert, OOM, unexpected class loading or partial world application. Maintain immutable golden save files for every supported shipped format.

## Additional provider-contract and scale risks, not counted as reproduced defects

1. **Off-game-thread OSS callbacks are silently discarded.** `SovOnlinePlatformServicesAdapter.cpp:44–57,180` and subsystem callback lambdas return without dispatch when not on the game thread. If a configured provider can deliver its terminal callback on a worker, the request is never retired. Timeout marks it canceled, but `Pending` remains owned until a terminal callback that was discarded; `Begin:185` rejects every subsequent operation. Because the actual provider's delivery contract is unavailable here, this is a conditional integration risk. Establish/assert the provider contract, or copy stable callback values and dispatch to the game thread using a provider generation. Test worker callback, timeout, late completion and provider replacement. Do not fix by releasing undrained request ownership on a generic timer, which reopens same-file callback cross-talk.
2. **No bounded cleanup policy for support/import archives.** `CommitPlatformSnapshot:268–277` creates GUID-suffixed copies for every explicit import; corrupt-bank overwrite similarly creates another support archive at 446–449. This preserves evidence but can steadily consume storage. Add account-scoped inventory, quotas, explicit retention/export/delete UI and no silent deletion of the only good copy. Low-storage behavior must exercise this path, not just the normal two-bank write.
3. **Synchronous serialization/read/write/readback scale is unmeasured.** `CaptureAndWrite` and `WriteEnvelope` run on the game thread; `ListSlots` deserializes up to 28 bank envelopes, and a cloud review copies large byte buffers. The 64 MiB cloud limit is an admission cap, not a memory/frame-time budget. Preserve coherent game-thread capture, move platform-supported I/O safely behind the existing ownership transaction, cache verified header indexes with proper invalidation, and measure representative save sizes/slow storage. No performance numbers can be claimed without engine/content/device execution.
4. **Account settings are still global config.** Forwarded to UI/settings reviewer to avoid duplicate ownership. `USovGameUserSettings::PersistSettings` calls `Super::SaveSettings`; native account selection does not select account-owned settings. Do not assume the portable checkpoint subset is the same as persistent per-account settings. `RestorePortableSettings` has only test callers; whether checkpoint load intentionally preserves current user assists must be decided separately from the verified account-scope gap.

## Existing test coverage and its limits

- Existing native tests exercise synthetic bank writes, denied/torn writes, fallback, captured subclass retention, restore ordering, request tokens, failed initial-world restore, native account revocation before cloud cancellation callbacks, and suspension deadline accounting. These are valuable and should be retained.
- `SovLifecycleRuntimeTests.cpp:90–118` destroys/recreates a controller while asserting the persistent GameInstance latch and Save suspension flag. It expressly runs headless and asserts `CanResumeGameplay` is false. It does not establish that a real replacement controller inherits the actual resume menu, mapper ownership and input/pause leases through CommonUI. `NativeResumeSafeZone:120–127` checks construction and root safe zone only.
- Portable policy suites can establish reason-mask arithmetic and bank-selection math, but cannot prove Unreal deserialization safety, post-callback account fences, Slate navigation, world pausing or platform storage contracts.
- Required first UE validation matrix: non-unity UHT/UBT build; actual native regressions; real-viewport account/lifecycle tests; corrupted archive fuzzing; process-kill and low-storage save tests; multi-device optional cloud conflict tests; licensed console owner and suspend contract verification.

## Recommended implementation sequence

1. **Save operation ownership and bounded decode:** SP-01 and SP-05, extending current `USovSaveSubsystem`, `USovCampaignSaveGame` framing, `ISovSaveStorage` tests and provider observation. Done when every external/serializer boundary has an invariant check, revoked work cannot initiate another write, and corrupt input cannot reach arbitrary unbounded deserialization before framing validation.
2. **Recoverable interrupted session lifecycle:** SP-02 and SP-03, coordinated with campaign async travel failure and cinematic checkpoint-acknowledgment fixes. Done when the original account can resume safely, a replacement can start only from a torn-down frontend, and no gameplay advances during an interruption while required orchestration remains live.
3. **Cloud revision discovery and lifecycle:** SP-04 plus provider callback-thread contract and explicit retention policy. Done when all valid retained revisions are reviewable despite clock skew, late callbacks cannot satisfy later requests, and optional cloud cannot become permanently unusable from one silent callback drop or an unexplained quota cap.
