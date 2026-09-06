# Save-operation ownership hardening

Source baseline: merged PR #34 (`93bf5c2`, tree `a46e58d`). This slice addresses the verified operation-lifetime gap in SP-01 of `AdversarialAudit-2026-09-05/SavePlatformAudit.md`. It preserves the existing Narrative payload, account namespaces, platform-local user routing, two-bank generations, exact readback verification, and durable corrupt/cloud-copy archives. No save-format migration is introduced.

## Contract

Every synchronous save/storage transaction captures its namespace, platform-local user, profile-selection generation, native authorization generation and suspension generation. All actual `ISovSaveStorage` calls go through helpers that check the token both before and after the callback-capable boundary. Deserialization, serialization and required/optional asset preflight recheck the initiating token before continuing.

- Revocation and reauthorization of the same account cannot revive an old operation. Suspend/resume likewise cannot revive an interrupted synchronous write. Repeated observations with no authorization change do not spuriously cancel it.
- Profile-hint persistence is guarded against nested selection. The selected namespace and availability are published only after the original authorization still matches; a callback cannot overwrite a revocation with `available = true`.
- A storage write already issued before revocation may finish. Its target bank or support archive may therefore exist, but it is **unconfirmed**: no subsequent storage call or success report follows the invalidation. The last verified other bank is retained. This is admission and completion safety, not a claim that platform I/O can be rolled back.
- An already decoded pending load retains its initiating account/authorization token, while the existing watchdog still pauses over suspension. Same-owner suspend/resume does not cancel that asynchronous restore. Account authorization loss/ABA instead offers recovery and cannot be turned into success by a stale readiness callback.
- A queued autosave belongs to its initiating token; stale queued work is dropped. Native boundary owners must queue a fresh safe boundary after interruption. A reentrant replacement queue is not silently consumed by the older boundary.
- `RetryFailedWrite` is a deliberate new attempt. It can acquire fresh authorization after the **same retained profile and local user** reconnect/resume; it cannot adopt a different profile selection. A callback acknowledging the old failure cancels further retry I/O. The candidate remains strongly referenced across that callback, and an obsolete retry cannot reacquire the released failure pause.
- Failure acknowledgment clears its owned state before releasing the pause, so release callbacks cannot observe a half-retired decision.
- Irreversible-boundary acknowledgments retain the snapshot's original account token as well as their existing world/mission/hero/boundary identity; authorization ABA cannot authorize a later boundary.

Mission-travel recovery uses the same private owner contracts; its separate retained-origin transaction is documented with that slice. Pending slot-load ownership is not a parallel actor serializer.

## Native regressions

`SovSaveOperationRuntimeTests.cpp` adds ten native registrations:

1. Every actual storage boundary of a third two-bank write, with authorization revocation, authorization ABA, and suspend/resume ABA; assert no later I/O and byte-identical last-good bank.
2. Actual `USaveGame::Serialize` subclass witnesses during both envelope load and save, not mocked serialization.
3. Profile selection reentry/revocation during both hint reads, write and readback.
4. Same-owner suspended pending-load completion versus stale authorization-ABA completion.
5. Rejected unauthorized retry followed by explicit fresh authorization for the same retained owner.
6. Acknowledgment from inside the retry storage callback, proving no later readback or re-created decision.
7. Stale queued-autosave invalidation with zero I/O.
8. Revocation inside cloud-copy archival with no following archive readback or native-bank mutation.
9. Unchanged native account observations across every storage call.
10. Actual configured Narrative subclass decode revocation before record/asset validation.

These tests invoke the native account observer directly from fake-storage/serializer callbacks. They do not claim that any particular licensed console provider delivers callbacks at those exact boundaries. Existing save-bank, profile routing, load-request and world-restore tests remain in place; their staged-load fixtures now explicitly capture the same pending owner token as production.

## Validation and remaining gates

`git diff --check` passed, and `python Scripts/Test-NativePolicies.py` compiled and passed all 39 portable C++ suites. Those host checks do not execute this Unreal subsystem or the new native tests.

The environment has no UE 5.7 engine/toolchain. These native tests are authored but **not compiled or executed here**. Required next validation is a non-unity UHT/UBT build, the `ProjectVelkorran.Campaign.Save` automation group, actual required/optional package-load reentry, and licensed-device account revocation, suspension, readback and low-storage scenarios.

SP-05 remains a distinct hardening gate: local envelopes and nested Narrative bytes still reach Unreal deserialization without a fixed outer integrity frame or general bounded-count archive. The cloud 64 MiB input limit and semantic validation are not protection against arbitrary malformed nested lengths or class resolution. No bounded-deserializer, fuzzing, crash-safety or certification claim is made by this slice. Outer framing requires an explicit compatible-format/migration decision and engine fuzz tests; it was deliberately not mixed into ownership changes.
