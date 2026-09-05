# Source closure validation and review record

5 September 2026. Baseline: `0378febff90b2cb04ac7887812ba4efe1b87072d` (merged PR #28 source). Branch: `codex/source-engineering-closure`. This is a source-only delivery for the next adversarial audit; UE5.7 compilation is intentionally deferred to Monday.

## Executed here

| Check | Observed result | What it establishes |
| --- | --- | --- |
| `python3 Scripts/Test-NativePolicies.py` | **37 suites passed** | Actual production-used portable C++ policies compile and execute under C++17, `-Wall -Wextra -Werror -pedantic`, UBSan and no sanitizer recovery. See the retained [log](SourceEngineeringClosurePortableTests.txt). It does not compile any Unreal-dependent class. |
| `python3 -m unittest discover -s Scripts/Tests -p 'Test*.py' -v` | **17 tests passed** | The production report checker rejects missing source registrations, stale/incomplete reports, duplicates, counter tampering and hidden errors. Synthetic checker inputs are not Unreal runtime evidence. |
| Production checker source inventory | **212 unique selected native registrations**, up from 174 | The full `ProjectVelkorran` filter now expects all registered project/plugin cases. The 38 added engine cases are authored, not executed. |
| Changed reflected-header inspection | **50 headers passed generated-include checks** | Reflected declarations have a generated include, included last. This is a textual check, not UHT. |
| Project descriptor inspection | JSON valid; enabled plugin names unique | Explicit OnlineSubsystem/Utils and Win64 TextToSpeech declarations are present. Actual module/plugin availability requires the UE build. |
| `git diff --check` | Passed | No patch whitespace errors. It is not a compiler or behavior check. |

No project `.uasset`/`.umap`, engine build tools or UE5.7 installation are present in this checkout. No UHT, UBT, Unreal automation, Blueprint compilation, cook, packaged mission, process-kill recovery, physical output, live cloud transaction or performance measurement was run.

## Newly authored engine coverage

| Area | Added registrations |
| --- | ---: |
| Cinematic inventory/commit/rollback | 11 |
| Campaign Mass and existing plugin participant receipts | 4 |
| Native dialogue/narration and frontend producer/HUD integration | 7 |
| Accessibility settings, presentation, evidence and record-view re-entry | 5 |
| Platform/cloud/account/actual stored-byte restart | 6 |
| Existing Narrative audio settings/mix consumers | 2 |
| Full HDR identity and production isolated-CVar adapter | 2 |
| Controller cue fallback configuration | 1 |
| Total | **38** |

Fixtures exercise production owners with hardware/provider seams only where actual devices/services must not be driven. The HDR CVar fixture specifically executes the real calibration adapter, routing its variable lookups to uniquely named test-owned CVars. It does not replace that adapter with arithmetic mocks or write to the machine's display CVars. Narration/provider seams do not prove a real platform has emitted speech or stored data remotely. The Mass round-trip fixture does not prove a production encounter meets its budget.

## Independent review findings fixed during integration

- **Inventory removal callback race:** final receipt validation now leads to native detachment without a second overridable permission callback; busy/active writes publish revisions before callbacks.
- **Mass promotion and cleanup:** require successful suspension release and unchanged NPC/controller/ASC ownership; release a rejected proxy using the authoritative actor fragment; preserve item-owned effect/grant handles rather than recreating them with an NPC source.
- **Dialogue/frontend re-entry:** guard backend creation, speech failure/completion and same-instance replacement; dispatch completion safely; retire old subtitle scenes before queueing a replacement; preserve successor record/menu state after cancellation callbacks.
- **First boot before opening:** paused world time was insufficient because managed cinematic loading/watchdogs tick while paused. An accepted loading request now waits on the actual local first-boot prerequisite, excludes that interval from its load timeout and rechecks before playback.
- **Offline restart/account boundary:** a durable validated hash-only profile hint retains the existing local namespace when network identity is unknown. A newly discovered online identity does not steal an explicitly offline campaign; a known different account still fences outgoing ownership.
- **Cloud phase listener re-entry:** cancellation/deinitialization before transport dispatch cannot launch obsolete requests; terminal cleanup precedes notification; listeners may start successor reviews without old completion clearing their bytes.
- **HDR adapter ownership:** the actual menu can preview/confirm SDR. Compositor/reference changes invalidate full calibration. Independent CVar rollback preserves external overrides while restoring other fields, including the raw UI gain when its base reference changed.
- **Native host lifetime:** create the native WidgetTree if absent; gameplay presentation renders beneath the existing HUD menu/modal layers; settings reach the first real world audio device and are not repeatedly applied per frame.

These are concrete internal review fixes, not a claim that the requested independent adversarial audit has already found every defect.

The final weapon-presentation attachment fix checks the actual mesh parent/socket/offset, rather than cached drawn/holstered flags, and adds `PhysicalAttachmentNotCachedFlags`. That last patch received local source/API review; it was not independently re-reviewed or engine-compiled before handoff. The final registration count includes this late case.

## Required next gates

1. Adversarially review this source delta and add failing regressions for concrete findings.
2. On Monday, run `Scripts/Validate-Unreal.ps1` with the full UE5.7 project/plugins; fix actual UHT/UBT/link failures; execute all 212 registered cases and the shipping mission validator. The report checker must reject any omitted registration.
3. In the complete content project, validate opening/convergence save/retry/transition sequences, real Mass participants, authored cinematic inventory/weapon visuals, target narration/HDR/audio/cloud devices, process-kill recovery and measured performance. Unsupported adapters remain safely unavailable or keep actors in A/B; they are not silently certified.

The build script's `-SkipBuild` path must not be used to present stale binaries as evidence for these source changes. Source registration coverage alone cannot detect a stale implementation under an unchanged test name.
