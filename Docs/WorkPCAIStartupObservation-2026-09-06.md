# Selene encounter startup observation, 6 September 2026

One fresh Selene PIE run remained in `BT_ReturnToSpawn` for 60 game seconds, despite two Hounds currently seeing the living player and the native threat service allowing direct targeting. A fresh retry in the same editor, with no native or AI asset changes, engaged and killed Selene normally. Normal Respawn then supported a complete 24-second defensive-input observation: 37 mapped pulses, native deflection activation, a natural perfect deflection, and the typed +10 Echo reward (25 to 35). The probe did not restore resources, grant equipment, force damage, or select hostile targets.

The stalled run's goal generators had the correct current PIE controller/component outers and no goals. ReturnToSpawn was the fallback activity; the evidence does not establish a navigation fault or an old-world pointer leak. See `live-attack-goal-readonly.json` and `attack-goal-readonly-exports/`; these preserve the stalled snapshot independently of the later successful encounter report.

## Confirmed graph and source path

- Stock `GoalGenerator_Attack` initially refreshes currently perceived Sight actors and listens to the controller's custom perception delegate and GameState's `OnFactionAttitudeChanged`. Its attack predicate uses current Narrative `GetAttitude` and `IsAlive`; it does not use `GetPerceivedHostileActors`.
- Stock `BP_NarrativeNPCController` immediately relays its perception events to the custom delegate. Its faction-affiliation callback functions are empty, and no player `OnFactionUpdated` or readiness subscription appears in this graph.
- `ANarrativePlayerCharacter::OnCharacterVisualInitialized` applies default factions through `InitNewCharacter` after visual initialization. `ANarrativePlayerState::OnRep_Faction` broadcasts the player's `OnFactionUpdated`, not the GameState's attitude-change event.
- UE5.7 `UAIPerceptionComponent::ProcessStimuli` suppresses same-state Sight notifications. Updating stored stimulus freshness does not make `ConditionallyStoreSuccessfulStimulus` request a new event.
- Initial generator setup also requires the current GameState cast to succeed. Its failure branch has no retry, though the captured stalled snapshot already had the expected GameState; no evidence proves this branch failed at initialization.

These paths permit a missed initial event if the player is first seen before its factions are available. The stalled run did not capture pre-readiness event order, so this remains a candidate cause, not a proven runtime diagnosis. Empty engine cached-hostile lists are not sufficient evidence for a cache repair. No speculative native change or shared Blueprint edit was made.

## Bounded follow-up

Run a cold fresh Selene encounter with a timeline observer installed before requesting PIE. Record the first player/ASC/faction readiness, real perception callbacks, generator appearance, current attitude, current Sight actors, and goal count. Record attachment times so missed pre-attachment events cannot be mistaken for absent engine events. If the stall repeats, compare the first successful Sight callback against faction publication and goal-generator setup. A narrow fix should follow the proven missing publication/retry edge and preserve existing LOS, hostility, death, sensor-deactivation, and ownership gates. A blanket perception reset or forced attack goal would not establish that repair.

All graph exports were read-only and preserved the source Blueprint persistent-property fingerprints. The observed retry and Respawn support playable combat, but repeatable cold-start engagement remains unqualified.
