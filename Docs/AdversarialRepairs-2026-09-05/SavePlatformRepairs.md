# Save, platform and interrupted-session repairs

These changes implement SP-01 through SP-05 from the adversarial source audit and coordinate campaign travel recovery with the existing GameInstance save owner. They extend the current Narrative serializer, bank layout, platform adapter and named pause owners. No second save or session system was introduced.

## Implemented

| Finding | Source change | Result |
|---|---|---|
| SP-01: authorization expires inside callbacks | `FSovStorageOwnerToken`, authorization generation, post-callback checks in `USovSaveSubsystem`; native provider events fence storage even when another observation is already processing | Revoke/re-authorize cycles cannot reuse an earlier save receipt. Profile selection cannot publish availability after hint persistence lost its authorization. Capture, import, archive, bank I/O, readback, slot enumeration and load preflight recheck the initiating owner. Cloud review callbacks retain the exact generation as well as the request and namespace. |
| SP-02: account-loss modal has no exit | Native interruption menu adds a two-step Return to title action; `AbandonSessionForTitle` retires transient load/write/acknowledgement/cloud state and disables underlying Narrative saving | A replacement account cannot inherit the outgoing campaign. Only the actual frontend permits profile ownership to reopen. The controller validates the configured separate title map before abandoning the session. Asynchronous title-travel failure reports a retryable failed state while retaining the title pause and storage/input fences. |
| SP-03: travelling world not paused | Application lifecycle retains its named simulation pause in `Travelling`; only `Initializing`, `Switching` and `Recovering` preserve their existing readiness timer exemption | The ready source world no longer advances under an overlay merely because map travel was accepted. Actual packaged pause/travel interaction remains an engine test gate. |
| SP-04: client clock chooses a hidden cloud revision | Adapter enumerates immutable revision IDs; service reads/validates history sequentially and exposes `FSovCloudRevision`; multiple valid copies require explicit selection and exact reread | A clock-corrected upload remains discoverable. Only one selected remote payload is retained. Compatibility `ReadLatest` refuses an ambiguous multi-revision slot. |
| SP-05: raw bytes deserialize before integrity preflight | New fixed `SVF2` outer frame with declared size/version/raw CRC; bounded genuine GVAS header inspection; exact configured class admission; known-object decode and archive/record budgets | Damaged framed data is rejected before UObject construction. Current unframed UE5 campaign banks remain readable and migrate only into the alternate bank on a successful subsequent write. |
| Provider worker callbacks discarded | Thread-safe adapter strong/weak ownership, copied immutable callback values, game-thread dispatch and provider generation checks | Worker callbacks are consumed on the game thread; callbacks from stopped provider bindings cannot mutate replacements. |
| Missing provider terminal callback | Canceled request remains owned; after a bounded drain interval cloud availability is quarantined with an actionable restart/provider-completion message | No timer releases an ambiguous same-user SDK operation into a successor request. Local campaign saves remain independent. A real provider completion can restore availability, requiring fresh opt-in. |
| Unbounded archival growth | Two rolling import archive generations; four preserved corrupt archives per bank with frontend list/delete APIs and verified-bank prerequisite; explicit cloud revision deletion | New archival growth is bounded. Cleanup cannot name another slot/account or delete a native bank. Old GUID-named archives are left untouched rather than silently deleted during migration. |
| Accepted mission travel can stall | Existing save GameInstance owner records destination and original owner, listens to travel failure, enforces watchdog, and makes one checkpoint recovery attempt | Persistent load/travel receipts survive a clean suspend/resume only; native revoke/re-authorize cycles cannot revive them. Failed initialization/travel retires controller state before retry. A missing destination controller takes one validated title-map fallback so a new frontend controller can offer explicit load. Missing title assets fail visibly without inventing a map or retrying forever. |
| Settings owner integration | Native save observer publishes verified opaque namespace/index and suspension to the existing game settings class | Per-account preferences are fenced before cancellation callbacks. Unregistered native test subsystems cannot write the editor user's real preferences. |

Central save capture, decode, verified-bank reads and write/readback paths now emit CPU trace scopes for performance captures.

## Wire format and compatibility

`SVF2` wraps the unchanged engine `SaveGameToMemory` envelope in a 16-byte little-endian header: magic, outer version, payload byte count and CRC32. The total frame limit is 64 MiB. The production-used CRC uses a constexpr 256-entry table, with the standard `123456789` test vector.

The compatibility reader recognizes real GVAS bytes from the previous current-product format. It admits the UE5 format-3 header with optimized custom versions, bounds string/custom-version counts, and verifies the exact class path before asking the engine to strip its header. The engine's returned body offset must agree with the preflight. Unsupported layouts are rejected while retaining original source bytes. The campaign envelope class is fixed; the nested Narrative class is the currently configured `USaveSystemDeveloperSettings::SaveGameClass`, preserving the existing configured subclass contract without loading arbitrary classes named by the file.

Migration is covered by an authored native regression that first calls the actual engine `SaveGameToMemory` implementation to create a genuine pre-frame bank. It does not create an invented legacy fixture. The old bank remains byte-for-byte unchanged; the next alternate-bank write records `Envelope.RawUnframedToSVF2.v1` in migration history. A schema/product mismatch still fails existing validation. Old December prototype saves are not newly supported.

The header-stripping API is public engine functionality: [UGameplayStatics](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UGameplayStatics). Its current public page may show a later engine version; the native UE5.7 round-trip and compilation gates remain authoritative.

## Validation

Executed here:

- The production-used `SovSaveFramePolicy` compiled with strict C++17 warnings and UBSan and passed CRC32 known-vector, every truncation, every byte mutation, oversized/trailing-length, and genuine GVAS recognition cases. Log: `SaveFramePolicyTest.txt`.
- `git diff --check` passed at the local review checkpoint.

Authored native regressions, not executed here:

- `ProjectVelkorran.Campaign.PlatformServices.OwnerReceiptAcrossNativeIOAndSelection`: actual native writer plus injected storage callback revokes and re-authorizes the same owner during bank read; no later write occurs. A failed automatic profile-hint write followed by direct public selection retry cannot publish availability after callback revocation. Persistent mission receipts survive clean suspension but reject account ABA during suspension; same-namespace cloud callbacks cannot revive an expired owner generation.
- `ProjectVelkorran.Campaign.PlatformServices.ExplicitRevisionHistoryAndWorkerCompletion`: both ahead-clock and corrected-clock revisions are visible; Use Cloud cannot choose silently; explicit selection reads the exact filename; a worker completion reaches the native service; cleanup targets only the confirmed revision.
- `ProjectVelkorran.Campaign.Save.RawFrameAndCurrentUnframedMigration`: genuine engine-generated pre-frame bytes remain compatible; alternate-bank framed successor preserves its source and records migration; corrupt raw frame, wrong class and unsupported header are rejected.
- `ProjectVelkorran.Campaign.Save.ExactAcknowledgedBoundaryAndAbandonment`: actual possessed campaign state validates exact one-use boundary acknowledgement and expiry; mission travel timeout reports one recovery result, retires controller travel state, and does not retry forever; title abandonment clears receipts and storage availability.
- Existing bank/archive regression now covers discoverable bounded corrupt-bank preservation and explicit verified cleanup.

These fixtures are in `Source/ProjectVelkorranTests/Private/Tests/`, the editor-only test module. Runtime contracts remain in the game module; private engine automation fields are not a shipping capability.

## Boundaries still requiring engine/provider validation

These are not reported as completed console certification, engine compilation or gameplay execution:

- UE5.7 UHT/UBT and native automation must validate the real GVAS header, configured Narrative subclass serialization, archive limits, input/controller behavior, and travel delegates.
- New-format raw integrity is checked before UObject decoding. Genuine unframed compatibility necessarily enters engine property deserialization after bounded header/class admission. Deep reflected array/map allocation behavior requires malformed-archive fuzzing in UE5.7; the source checks are not a formal proof against arbitrary hostile payloads.
- Generic `LoadDataFromSlot` obtains complete bytes before project code can enforce its 64 MiB admission cap. Platform backend/file-size admission and allocation behavior must be tested with the licensed engine implementation.
- Native framed decode has archive and record budgets, but actual maximum save size, memory peaks and slow-storage latency must be measured. CPU trace scopes provide capture points; no performance figures are invented.
- A provider that never issues its terminal callback is explicitly quarantined. The engine/provider session or application restart is the safe recovery boundary; the game does not guess a proprietary SDK reset or unlock overlapping ambiguous callbacks on a timer.
- Real SDK identity changes, suspend notification timing, storage process-kill/low-space behavior and controller/TV presentation remain hardware gates. Existing Narrative console module filters, missing ZenDyn and licensed engine/SDK access remain separate build prerequisites.
- Title recovery requires a separate valid cooked `GameDefaultMap` whose frontend supports account selection. Source validates that dependency; authoring/cooking the map remains editor/content work.
- Cleanup requires a verified native bank. If both source banks are corrupt and all preserved archive positions are occupied, source-preserving writes remain blocked rather than silently dropping unique recovery bytes; this exceptional recovery requires a verified bank/platform recovery workflow.
- Pre-policy GUID-named archives are retained. The new bounded archive IDs and UI do not pretend to enumerate unknown provider storage files using a nonexistent generic SDK capability.

## Recommended first engine run

1. Compile non-unity Development Editor and the test module, then run the four new native registrations and existing bank/serializer/lifecycle tests.
2. Fuzz framed and genuine compatibility blobs, including count/length mutations with recomputed raw checksums, under engine memory diagnostics. Exercise configured Narrative subclasses and each supported saved engine format.
3. In a real viewport, overlay during accepted travel, delay/fail destination initialization, lose the owning account, choose Return to title, and verify that no source gameplay advances and no new account inherits the abandoned session.
4. On configured desktop cloud providers, test two devices, clock correction, late worker callbacks, timeout/quarantine, explicit revision selection/deletion and quota recovery. Then perform the licensed console account/storage/suspend matrix separately.
