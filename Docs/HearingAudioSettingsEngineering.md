# Hearing accessibility: existing audio-settings consumer completion

TDD v2 §13.9's ambience, tinnitus-like tone and dynamic-range settings now extend Narrative's existing `UNarrativeGameUserSettings::ApplySoundSettings` consumer. No parallel audio manager, simulated compressor, new save snapshot or editor-authored audio asset is introduced.

## Source path

`SetAmbienceAudioVolume` and `SetTinnitusAudioVolume` follow the existing dialogue/music/SFX/UI/master setters: finite bounded local configuration, immediate `SaveSettings`, then immediate application through the existing settings path. All seven configured volumes normalize before device output, including values loaded directly from config. Ambience/tinnitus default to 1.0 to preserve existing content behavior; either may be independently set to zero. These local hearing preferences are not added to the portable gameplay save subset.

`UArsenalSettings` exposes `AmbienceSoundClass` and `TinnitusSoundClass` next to its existing five sound-class paths. The runtime resolves actual `USoundClass` assets and applies each value through `FAudioDevice::SetSoundMixClassOverride` on the existing default base mix. Missing/wrong-class optional paths are skipped. Duplicate class aliases are rejected so an accidentally Master-routed tinnitus slider cannot overwrite the Master gain. Audio output/device absence produces no bus calls and does not fail the campaign.

The frontend owner reapplies settings once the playable world's audio device becomes available, once per world/device identity; each explicit setter already reapplies immediately. This closes the startup interval in which GameUserSettings can load before any playable audio device exists.

## Authored dynamic-range presets

`ENarrativeAudioDynamicRange` has Full, Reduced and Night. Full uses the existing mix without an extra modifier. Reduced and Night use `ReducedDynamicRangeSoundMix` / `NightDynamicRangeSoundMix` in `UArsenalSettings`. The selected saved preference is distinct from `GetAppliedAudioDynamicRange`: unavailable optional assets visibly fall back to Full without falsely claiming that a requested preset is active. `IsAudioDynamicRangeAvailable` supports truthful menu admission.

The selected range asset is a separate authored persistent modifier. It is not used as the default slider mix, because overwriting its class gains with ordinary slider values would erase authored range balance. The settings owner uses Unreal's actual [`PushSoundMixModifier`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FAudioDevice/PushSoundMixModifier) and [`PopSoundMixModifier`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FAudioDevice/PopSoundMixModifier) APIs, tracks its own modifier and exact audio device, avoids stacking duplicate pushes on reapply, and releases only its own modifier on range/device change or destruction. It never clears unrelated gameplay mixes.

Range assets must have a finite negative duration (persistent) and must not be the existing default base mix. Missing assets, the wrong class, a timed mix, or a duplicate base-mix assignment use Full. Unknown persisted enum values normalize to Full. The implementation does not claim that SoundMix selection magically creates compression: the project's sound designers must author appropriate class balance/EQ and any approved downstream dynamics processing in the actual audio graph, then verify the perceptual result. Source selection is complete; those assets and listening validation remain editor/device work.

## Required asset wiring

Author ambience and tinnitus classes as independent siblings of SFX/dialogue/music beneath Master. Route relevant sounds to them; a new empty class does not reroute existing content automatically. Tinnitus-like tones must not be the only carrier of gameplay-critical information. Keep captions and non-audio cues for the underlying events. Assign approved persistent Reduced/Night SoundMix assets in Project Settings / Narrative Pro Sounds. Do not populate speculative asset paths in source defaults.

Existing controller-audio gain and caption/visual cue production are separate outputs owned by their existing project components; these changes do not route tinnitus-like sound into a controller speaker or force any hardware playback during tests.

## Validation

Executed here: `NarrativeAudioSettingsPolicyTests.cpp` compiled as C++17 with warnings as errors, pedantic mode and undefined-behavior sanitization, then passed. Coverage includes normal bounds, NaN/infinities, fallback, and all 256 persisted range byte values.

Two Unreal automation tests under `ProjectVelkorran.Campaign.Audio` use real transient `UArsenalSettings`, `USoundClass` and `USoundMix` objects through the actual settings consumer, with the final device calls captured by deterministic output seams. They cover seven independent bus values, exact zero tinnitus mute, immediate persistence, duplicate-alias rejection, config normalization, authored range identity, persistent-versus-timed preset admission, missing/base-mix fallback, no-device behavior and reapplication after recovery. These tests have not been compiled or executed in UE5.7 here.

Monday/target validation must confirm UHT/UBT, automation, actual mix push/pop ownership on device/world changes, authored sound routing and hierarchy, audible independent slider control, full/reduced/night loudness and clarity, headphones/TV/console output, and complete caption/visual alternatives when tones are muted. No live audio device was driven in this source-only pass.
