# Console build compatibility pass, 5 September 2026

Scope: Xbox Series X|S and the PlayStation 5 family, including PS5 Pro. This is source engineering preparation against UE 5.7, not evidence of a successful console build, hardware performance or platform certification. There is no UE source tree, console SDK, console platform extension or devkit in this workspace.

## Changes

| Finding | Source change | Preserved behavior and validation |
| --- | --- | --- |
| `IncomingCollisionProcessor.cpp` declared `MassDebuggerSubsystem` only under `WITH_MASSGAMEPLAY_DEBUG` but referenced it under the independent `ENABLE_VISUAL_LOG` flag. | Guard the debugger header and selected-entity visualization with the same debugger feature condition. | Collision avoidance and ordinary visual logs remain available. The actual file's preprocessor matrix is checked for all four combinations of the two flags. |
| `NarrativeArsenal.Build.cs` linked `SequencerScripting` with no corresponding runtime source use. | Remove that unused runtime dependency. | Native cinematic playback still links `LevelSequence`, `MovieScene` and `MovieSceneTracks`; authored sequence behavior is unchanged. |
| Narrative's descriptor repeated the identical `HairStrands` dependency. | Retain one enabled declaration. | Groom support remains enabled; descriptor validation now verifies unique dependency names. |
| Desktop MetaHuman authoring, animation capture and calibration plugins were explicitly enabled for every target. | Scope the five authoring/capture project references to `Editor`. Explicitly enable `RigLogic` for assembled MetaHuman runtime deformation. | Editor authoring remains available. This does not remove HairStrands, runtime animation or assembled skeletal meshes. Cooked asset references and ZenDyn's undisclosed dependencies still need the complete project's cook verification. |
| Global renderer config imposed a 3000 MB texture streaming pool on all hardware. | Move the existing value into Windows and Linux config. | Existing supported desktop defaults remain. Consoles inherit engine/platform defaults until measured device profiles are authored; no Series S or PS5 memory budget is invented. |
| Project settings enabled split screen despite the single-player, sequential protagonist design. | Disable split-screen layout in `DefaultEngine.ini`. | Hero switching remains inside the existing local player's campaign. This setting alone does not implement user/controller ownership; that is handled by the platform/input pass. |
| Exported lifecycle header uses ApplicationCore input device types. | Move the game's existing `ApplicationCore` dependency from private to public. | The dependency is unchanged; downstream modules now receive the include visibility required by the public header. |

## Travel account boundaries

Campaign travel uses the existing Narrative player-only slot path with the current opaque account namespace and explicit resolved platform user index. The native save/read functions accept an optional ownership predicate while preserving legacy call signatures. Capturing actors runs Blueprint `PrepareForSave` callbacks and custom serialization, so a controller-level check after the save is insufficient: ownership is now checked after each capture, before reading the existing slot, after deserializing it, and after serializing the replacement into memory immediately before `SaveDataToSlot`. This keeps Unreal's platform save system and the existing save subclass metadata; it does not add a parallel save backend.

Destination `InitNewPlayer` retains the expected namespace/user and rejects changes across reading and staging player records. Failed reads clear output records rather than leaving another user's previous data available to a caller. Native regression cases revoke the ownership predicate from the actual controller `PrepareForSave` callback and from the save subclass's `Serialize` callback, then verify the existing slot bytes remain identical. A deserialization callback also revokes ownership during a read, which must expose no records. These native tests require UE compilation and execution. The legacy display-name/default-user-0 multiplayer APIs still exist for Narrative compatibility and must not be used by console campaign menus.

## Hard blockers retained honestly

1. **Narrative module platform filtering.** The vendored `NarrativePro.uplugin` explicitly allows only `Win64`, `Android` and `Linux` for each required runtime module: `NarrativePro`, `NarrativeArsenal`, `NarrativeCommonUI` and `NarrativeSaveSystem`. Their source has been inspected for obvious desktop APIs; Windows DXGI code and system libraries are guarded. That inspection is not vendor console support or a licensed-target compile. These lists have deliberately not been erased. Obtain the vendor-supported UE 5.7 console source/platform extensions, reconcile them with this modified plugin, then verify all four modules in both Development and Shipping.
2. **ZenDyn is enabled but unavailable.** There is no ZenDyn descriptor, source or binary in this checkout. Its console support, licenses, ABI, physics threading and cooked asset dependencies cannot be inferred. It remains required rather than silently disabled.
3. **Engine and SDK dependencies are external.** CommonUI, Mass, HairStrands, animation and the other enabled engine plugins must resolve in the licensed console UE 5.7 tree. A Windows editor build is insufficient evidence for either console. Console SDK setup, target registration, packaging, signing and deployment use the restricted platform documentation supplied to the licensed developer.
4. **Content and device profiles are absent.** The checkout has no `.uasset`/`.umap` content. Startup map references currently lead into Narrative content; cook reachability, campaign map routing, animation/mesh compatibility, runtime RigLogic dependencies, shader permutations and memory residency remain unverified. Measure Series S separately from Series X, and PS5 separately from PS5 Pro. Do not infer a frame-rate target from this pass.

## Repeatable preflight

Run `python -B Scripts/Check-ConsoleBuild.py` in this source checkout. It produces JSON and returns **1** while required plugin descriptors or Narrative target support are unresolved. That failure is expected here and must not be relabeled a test pass. Missing engine plugins are reported because this checkout does not include the engine.

On the licensed build machine, pass `--platform-token` using the exact registered UBT platform identifier, and repeat `--plugin-root` for the installed engine and vendor plugin roots. The checker never guesses restricted target tokens. It ignores nested plugin templates and editor-only project references, traverses enabled plugin dependencies, reports missing nonoptional plugins and checks every required Narrative runtime module. Exit 0 means only that these descriptor checks found no blocker. It does not run UBT, merge platform extensions, validate binaries, cook assets or certify the game; UBT's resolved build graph is authoritative.

Run `python -B Scripts/Tests/TestConsoleBuild.py` for the checker failure cases and Mass preprocessor matrix. These are host-side checks. The preprocessor test intentionally omits unavailable UE includes and does **not** compile Unreal C++.

## Licensed build and device acceptance

1. Integrate supported Narrative and ZenDyn console implementations; retain the project's gameplay modifications. Resolve the complete engine/plugin dependency graph in UBT and record exact engine, SDK and plugin revisions.
2. Build Development and Shipping Game targets for both platform families with unity disabled at least once. Compile the native automation target and execute the project's tests on supported test targets. Confirm no editor module or Windows-only import enters the cooked executable.
3. Cook and stage the complete content; fail on missing assets, unsupported plugins, unresolved class redirects or startup map errors. Verify RigLogic face deformation, grooms, cinematics, Mass promotion/demotion, severing and physics on each device tier.
4. Record CPU/GPU frame times, peak memory, texture residency, shader/PSO stalls and streaming hitches for Level 1 and Level 2. Apply measured platform/device profiles through the licensed configuration hierarchy. Inspect Series S under prolonged combat, migration and save/load churn.
5. Complete the separate account/storage, suspend/resume, controller, safe-area, HDR and accessibility acceptance steps in the console pass documentation before adversarial device testing.

## Public reference basis

- Epic states console development and packaging need a source engine build and platform tooling: [Packaging Unreal Engine projects](https://dev.epicgames.com/documentation/unreal-engine/packaging-your-project).
- Descriptor platform/target filtering and explicit platform extension semantics: [FModuleDescriptor](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Projects/FModuleDescriptor) and [FPluginReferenceDescriptor](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Projects/FPluginReferenceDescriptor).
- MetaHuman Creator is an authoring/editor tool; Core Tech, Animator and capture plugins have separate roles: [MetaHuman plugins overview](https://dev.epicgames.com/documentation/metahuman/metahuman-plugins-overview-in-unreal-engine).
- Sequencer scripting is an editor/Python utility plugin; runtime playback keeps its existing cinematic modules: [Sequencer Scripting](https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/SequencerScripting).
- Runtime facial animation remains an explicit dependency: [RigLogic plugin](https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/RigLogic).
- UE's in-memory serialization and platform-slot byte APIs: [Saving and loading your game](https://dev.epicgames.com/documentation/unreal-engine/saving-and-loading-your-game-in-unreal-engine).

The public documentation is rolling. The licensed UE 5.7 source, vendor platform packages and actual UBT/cook results remain authoritative for exact APIs and compatibility.
