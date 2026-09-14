# Enemy combat presentation — September 13, 2026

This pass connects Realistic Blood to native combat health-damage receipts and adds wall-surface presentation. It does not establish a new TDD alignment percentage or qualify the full mission route.

## Blood

`USovBloodFeedbackComponent` is target-owned on Sovereign NPCs and protagonists. It accepts only current native receipts from the bound ASC, consumes each receipt once for this presenter, and emits on applied health damage. Shield-only damage, zero damage, periodic ticks and defended outcomes produce no blood. Edge damage uses the slash effect; overkill or at least 60 applied health damage uses the burst effect; other wounds use the hit effect. No gameplay damage or dismemberment rules change.

Eight owned Niagara systems in `/Game/Aurelion/VFX/Blood` derive from the installed Realistic Blood pack. Four red and four black variants cover hits, slashes, heavy bursts and reduced effects. Black material instances use neutral near-black main/secondary colors and neutral specular tint. The vendor smoke emitter is disabled so its independently authored tint cannot introduce red mist. Vendor assets remain unchanged.

Eclipse Linkbound, WallRunner, Weaver and Elite use black blood. Mechanical drones disable flesh blood. Other Sovereign NPCs and protagonists use red. Rendering loads asynchronously, uses an unreliable cosmetic multicast, respects reduced-effects preferences, and limits each world to 24 simultaneous effects. A watchdog removes effects after eight seconds even if a vendor emitter loops. Existing death-puddle and sever-profile authoring remains a separate system.

## Wall runner and support cast

Traversal continues to use its existing authority-owned movement lease and swept capsule path. The presenter probes the actual wall, aligns the mesh to its normal and travel tangent, places its imported foot plane at the wall, and blends back to the original mesh transform on landing/cancellation. Surface state replicates for clients. The spider's Parasites run clip supplies a looping montage, with root motion disabled and root lock enabled.

Weaver plays a Parasites Alfa fire-cast montage when fresh support links are established. Repeated support polling cannot replay that cue. Existing melee/drone montages remain assigned; the cast audit records grants and defaults rather than replacing valid clips blindly.

The broader definition audit found two additional unassigned casts. Hound Bite now uses an owned FantasyBeast04 attack01 montage. Handler Command now uses a weapon-raise cue retargeted from Narrative's rifle equip animation to the actual Sci-Fi Soldier 05 skeleton. Both clips are root-locked, have their notifies removed, and play over 0.8 seconds; the original native impact/command timers remain unchanged. Hound uses DefaultSlot and Handler uses FullBody. Charge/pounce and gunfire/rocket montages were already assigned. The legacy `/Game/SciFi_Drone_1/Narrative/NPC_ReformationDrone` definition has no ability configuration and no registry referencers; it is not substituted into the active roster.

## Verification

Build and asset verification evidence is recorded under `Saved/Validation/Aurelion`. Tests include blood species/default selection and damage filtering, plus actual physical wall traversal with the installed spider mesh, gait playback, wall-normal alignment and recovery. These isolated fixtures are not live mission proof. Live foot-contact quality, attack timing, crowded-combat gore readability and multiplayer presentation still require qualification.

- Editor build passed after the final source changes (28.08 seconds).
- `Saved/Validation/EnemyBloodWallFinal/20260913-173839-769386d5`: 17/17 enemy-role tests passed, including the actual spider mesh/gait test and the check that presentation cannot acquire a traversal lease or move the capsule.
- `Saved/Validation/BloodFeedback/20260913-173927-06272c40`: blood health-damage/species policy test passed.
- `Saved/Validation/Aurelion/EnemyPresentationFinal-20260913-174027-e5709b7c/blood-casts-review.json`: eight systems ready after reload; every enabled black renderer references an owned near-black material; seven role defaults and four newly assigned montage references verified. Eclipse death-puddle and sever-profile assignments are empty, so no existing red profile conflicts with the new black effects.
- `Saved/Validation/Aurelion/HandlerCastRetarget-20260913-173730-f4dbcfad/missing-enemy-casts.json`: Handler retarget and Hound assignment evidence.

Initial checks caught a material setter returning no value (fixed by checking the actual material value), Python's stripped boolean-property names (corrected), and the old no-tick test contract (replaced with presentation-only ownership assertions). The existing Aura startup diagnostics remain unrelated to the passing automation reports.
