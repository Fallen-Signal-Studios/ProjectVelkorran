# Native narrative speech and interruption

TDD v2 sections 9 and 12 require bounded bark priorities, contextual knowledge gates and critical conversations that survive combat interruption without replaying their events. Narrative already owns dialogue graphs, nodes, choices, timers, audio and task execution. This implementation extends that path and adds one speech arbiter on `ASovPlayerController`.

## Systems and behavior

`USovNarrativeCue` is an authored request containing a stable cue/speaker identity, mission/protagonist/knowledge requirements, timing, priority and either a Narrative dialogue class or captioned bark variants. The native order is lethal warning, objective-critical direction, companion rescue, tactical, relationship and ambient. `USovNarrativeCueComponent` validates the request, limits the queue to 64, rejects duplicates, expires irrelevant ambient requests, cycles variants and increases cooldown with repetition. Dialogue ownership remains in `UTalesComponent`.

Only warning/direction/rescue/tactical barks may play during combat. Higher-priority speech can interrupt a lower-priority bark. A live owned conversation is suspended before another bark starts; unrelated Narrative conversations block the arbiter from speaking over them. Speaker loss stops the bark. Interrupted critical barks remain queued, and an explicitly authored diegetic summary can appear in the record list. No automatic verbatim journal transcript is manufactured.

Barks retain their authored caption when audio cannot load or no audio device is available. They still finish within the bounded caption duration, so an unavailable sound cannot leave critical guidance retrying forever. Failed sound loads produce an authoring warning.

The source closure adds an optional authored `ControllerAudioClass` for bark playback only. It must use Unreal's `ControllerFallbackToSpeaker` output target; controller-only or unconfigured classes retain the ordinary positional route. The existing cue audio component receives its class and separate finite controller-channel gain before playback, without modifying a shared sound asset. Live playback consumes the saved channel gain; muting does not remove the caption or alter cue completion. Native `ControllerAudioFallbackContract` regression source covers accepted/rejected routing and finite/mute behavior. No physical controller output was tested: Epic documents this output target under its legacy sound-class properties, so support by the shipping audio backend/device remains a platform validation gate. See [sound-class output routing](https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-classes-in-unreal-engine) and [component class override](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UAudioComponent).

Critical conversations opt into Narrative's `SetPreserveOnInterruption` contract. Queued conversations must allow free movement and contain no owned control tags, body montages, automatic player alignment, camera shots, cinematic bars, party dialogue or camera shake. This constraint keeps gameplay control available while the conversation is paused. Cinematic campaign beats retain the existing cinematic runner.

`UDialogue::SetPlaybackSuspended` pauses the actual line timer and audio component, blocks choice and skip input and retains the exact graph instance/current node. Resume unpauses those objects; it does not begin another graph or replay node events. Requests made inside a synchronous line-completion transaction are refused until the caller retries, so a half-completed narrative event sequence cannot be frozen and repeated. A start suspended before its first playable line resumes that pending start once. Narrative line completion also removes audio-finish callbacks before stopping audio, rejects recursive/late completion and checks ownership after completion listeners.

`UTalesComponent` fences replacement and exit mutations. Finish listeners may clear or destroy their owner without causing the old call stack to dereference a cleared pointer, deinitialize a new graph or create dialogue during teardown. Cue start/replay state uses a generation counter, including callback-driven loads and immediate graph completion.

## Persistence and authoring

The component implements Narrative's existing savable-component interface. Queued requests, critical bark recovery, repetition counts and approved unheard records use the controller save record. Speaker GUIDs reconnect persistent NPCs after load. Load validates and bounds entries, removes duplicates, and stops old playback without inserting old records into the restored state. Absolute world-clock cooldown timestamps are transient; repetition counts persist and restore the escalation policy on subsequent playback.

Active conversations are not serialized as restarted graphs. Save admission already rejects a live Narrative dialogue; an interrupted critical conversation resumes in the same live instance when combat ends. World travel/save integration must retain that admission gate. Replacing a Tales component ends its old ownership and preserves only any authored unheard summary, rather than replaying previously fired graph events.

Editor work: author localized captions/audio, cue data assets, optional platform-approved sound classes and free-movement Narrative graphs; request cues from the existing encounter/dialogue/companion events. The native frontend now binds `OnCueStarted`/`OnCueEnded` and Tales line events to actual subtitle presentation; an authored replacement must preserve that ownership. `OnDialogueSuspensionChanged` lets the existing dialogue UI hide and restore its current choice/caption state. Complete cinematic graphs should continue through campaign cinematic assets.

## Validation and done criteria

Executed locally: `SovNarrativeCuePolicyTests.cpp` passes 80 production priority/combat/cooldown assertions in the portable C++17 runner with warnings as errors and undefined-behavior sanitizer. The full runner passed 23 suites at integration review.

Authored Unreal automation, not executed here:

- `Narrative.CuePriorityAndCriticalReplay`: real queue replacement, duplicate rejection, critical requeue and authorized summary.
- `Narrative.DialogueSuspensionPreservesNode`: actual timer pause/resume, retained node, blocked choice/skip and no task progression from paused callbacks.
- `Narrative.CriticalSaveAndCallbackIsolation`: one restored critical request, no stale playback/record contamination and no old-request resurrection after callback load.
- `Narrative.DialogueCompletionOwnership`: recursive/late line completion executes once and a finish listener can clear the active pointer while the captured ending instance cleans up.

Native source and tests are present. UE5.7/UHT compilation, these automation tests, voiced conversation playback through combat/choice/resume, level travel admission and authored subtitle UI are required acceptance gates. No Unreal build, audio-device test or in-editor validation is claimed in this environment.
