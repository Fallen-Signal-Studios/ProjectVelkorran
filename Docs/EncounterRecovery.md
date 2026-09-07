# Native encounter entry recovery

`ASovEncounterDirector` owns one authored encounter and its entry checkpoint. It uses Narrative's existing actor/component records inside the director's ordinary `SaveGame` record. `ASovPlayerState` keeps separate Tarrik and Selene snapshots in its existing Narrative player record. There is no second save slot or replacement quest/inventory system.

## Content setup

1. Place one director with a globally unique, durable `EncounterId`, for example `M01.Courtyard`. Do not rename this ID after shipping saves.
2. Set its `Participants` or call `RegisterParticipant` before capture. Every participant has a unique `ParticipantId`, a project `ASovNPCCharacterBase` subclass, and a required-for-victory flag. Resolve actors through `GetParticipant` after retries: restored NPCs are new instances.
3. Spawn these NPCs directly for the encounter, using their Narrative definitions. Settlement `NPCSpawnComponent` ownership is rejected; its separate respawn loop cannot also own a retry participant. A participant cannot belong to two directors.
4. Finish definitions, appearances, equipment, links, and initial positioning. Call `CaptureEntryCheckpoint(Player, Error)` at the safe entrance before starting combat. Capture requires a ready, living player and NPCs, no active abilities, no timed effects, and no broken/recovery/interaction/cinematic state. Persistent default equipment/perk effects are allowed. All command sources and linked actors must be registered participants. Pure relay actors outside the NPC registry are not supported by this entry slice.
5. A successful capture freezes NPC actions and owns exactly one Busy/Invulnerable contribution while the encounter awaits its start. `BeginEncounter` releases those contributions, starts Echo encounter timing, and creates a fresh attempt ID.
6. Confirmed deaths of every required participant complete the encounter by default. Corpse destruction after a confirmed death does not erase its credit; arbitrary destruction does not count as a kill. Disable automatic completion for an objective encounter and call `CompleteEncounter` from the validated objective owner. Player death calls `FailEncounter`. Either terminal result applies the authored Echo reserve.
7. A retry button calls `RetryEncounter(Error)` on authority. `true` means asynchronous restoration started. The `Active` state event means all participants, the player, and their records finished. `OnEncounterRestoreFailed` supplies the failure reason; the entry record remains available for another retry. A partial restore is never published as active.

`Participants`, state, and attempt ID replicate. Blueprint callbacks should update presentation and authored encounter gates. Do not reimplement inventory/resource/link restoration or award Echo from restore notifications.

## Restored state and boundaries

| State | Native restore policy |
|---|---|
| Player identity | Exact protagonist tag, pawn class, and `UPlayerDefinition` must match. A source pawn record cannot initialize the other protagonist. |
| Health, Shield, Stamina, Poise, Echo | Restore saved absolute currents, clamped to the current definition's maxima. Saved maxima are validation/inspection data. NPC ASC component bytes are excluded from checkpoint loading so authored old Max values cannot replace current tuning. |
| Resource clocks | Health, Shield, and Poise discard prior-attempt recharge/break timers and restart a full authored delay where needed. Echo resets its inactivity clock. The restore suppresses project Shield/Poise break broadcasts and removes only component-owned state counts. |
| Inventory, ammo, equipment | Existing Narrative pawn records restore item quantities, item state, and equipped items. Wield slot pairs are then resolved against the restored inventory. Ammo/gains collected during a failed attempt are rolled back with that inventory. |
| Technique progression | A separate record for the existing SkillTree component avoids recursive PlayerState snapshots. Loading clears previously owned perk grants and replaces the perk list. The project Technique component validates the restored progression policy. |
| NPC lifecycle | Each participant is recreated using its original native/Blueprint class, Narrative definition, spawn overrides, stable GUID, and record. This is required to restore intact physics after severed bodies have been terminated. The replacement uses Narrative's definition/controller/character registry path and defers record application until visual initialization completes. |
| Weak points | Restore the authored zone IDs and remaining consequences without damage, break rewards, or synthetic sever events. Incompatible zone authoring fails restoration. |
| Command links | Resolve all participants by ID, restore link instance/state/last transaction identity, and reconcile owned tags/effects. Restoration does not broadcast `OnCommandLinkSevered`, dispatch a sever gameplay event, or award Echo. |
| Narrative quests/tasks | Restore the entry controller record after its matching pawn exists. Narrative now replaces an empty saved quest list as well as a populated one. Quest content must retain its existing `IsLoading` safeguards for authored side effects. |

An unfinished captured encounter loads as `Failed`, requiring an explicit entry retry, including a captured entrance saved just before `BeginEncounter`. Uncaptured encounters remain inactive and completed encounters remain succeeded. This is an authored-entry checkpoint, not arbitrary midfight rollback. Capture is deliberately rejected during abilities, timed statuses, Poise recovery, interaction, or cinematics. Checkpoint capture happens once per director; progression to the next checkpoint uses the next encounter owner.

Narrative's existing item/component byte serializers do not offer atomic rollback or general schema migration. Renamed/deleted inventory classes or incompatible Blueprint save layouts still require migration/content validation. Restore preflights identities, resource records, NPC classes/definitions, weak points, and link IDs; an incompatible record is not reported as a playable success. Blueprint perks that directly mutate stats must migrate to the project's owned GAS Technique grants; arbitrary instant Blueprint stat writes cannot be reversed by a grant-handle ledger.

## Preventing duplicated rewards and lingering attacks

`ClaimCompletionReward(RewardId)` is authority-only and succeeds once per stable reward key, only after success. The consumed keys are saved. Reward handlers must claim before granting a completion reward; repeatedly receiving presentation/state notifications is not permission to grant it again.

`ClaimAttemptReward(RewardId)` is native-only, requires an active valid attempt, and is consumed across all gates and protagonist instances for that attempt. It resets only when a new attempt successfully starts. Validated Selene bypass gates use this ledger.

The director tracks actors spawned during its attempt whose Owner or Instigator ancestry leads to its player or registered NPCs. Retrying destroys those attributed actors, including persistent attacks and uncollected drops, before restoring resources. The sustain-drop component supplies the killed actor as pickup Owner. Custom delayed spawns must supply Owner/Instigator before spawning, or call `RegisterAttemptActor` after attribution exists. Unrelated world actors are not deleted. Saved pickup GUIDs are retained for cleanup after a save/load; already collected pickups are absent and do not reappear.

Encounter entry content must contain all gameplay threats and mutable objects whose rollback it needs. External projectiles, settlement spawners, unregistered objectives, world-partition unloads, arbitrary Blueprint timers, and unsaved world mutations are not implicitly rewound. For those cases, register an appropriate existing Narrative savable owner in a future slice or use a level checkpoint reload.

## Protagonist snapshots

`ASovPlayerState::CaptureProtagonistSnapshot` creates a temporary snapshot; it does not replace the stored state until `StoreProtagonistSnapshot` succeeds. `PrepareForSave` refreshes the active ready protagonist before Narrative serializes the PlayerState. The snapshot contains a pawn record and an isolated skill component record, never a whole PlayerState record containing the snapshot map itself.

Campaign handoff restores after the matching definition/ASC/visual is ready, before input readiness is published. A newly managed target preserves its startup passive abilities; same-pawn restore cancels its active actions. Each callback boundary verifies that the original pawn still owns the same ASC/avatar. If ownership changes, restoration stops and the origin snapshot remains available to the campaign controller. `SetCampaignFactions` replaces faction sets, including empty sets, before new-protagonist default initialization.

## Validation

Portable validation run in this workspace:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -ISource/ProjectVelkorran/Public Tests/Portable/SovEncounterPolicyTests.cpp -o /tmp/sov_encounter_policy_tests
/tmp/sov_encounter_policy_tests
```

Passed: exhaustive state admission/retry/load/reward cases, nonfinite and invalid values, max-retuning semantics, and 42,000 resource clamp cases. `git diff --check` also passed.

Added Unreal automation tests under `ProjectVelkorran.Campaign.Encounter`:

- Real Narrative ASC resource capture/restore, current-max clamping, repeated restoration, and rejected malformed data.
- Existing Narrative actor-record serialization, preserved encounter identity, completion reward replay rejection, and active-load-to-failed policy.
- Command-link reconstruction with no sever reward/event and preservation of unrelated tag contributions.
- Shield/Poise checkpoint timer reset and preservation of unrelated status ownership.

Unreal/UHT/UBT execution is not available in this workspace. These tests are authored, not reported as executed. Run the project's `Scripts/Validate-Unreal.ps1` validation gate in UE 5.7, then exercise the following content integration cases in PIE:

1. Capture entrance, kill an NPC, collect ammo/Echo, break a zone, sever a limb and a command link, spend resources, then die. Retry restores exact entrance ammo/resources/equipment, intact entry physics, link membership, quest progress, and entry transforms; old pickups and paid attacks are gone.
2. Repeat retry three times. Check no growth in perk ability/effect counts, duplicate drops, sever rewards, or subscriptions.
3. Save during an active attempt; reload, confirm `Failed`, then retry. Save after success; reload and verify completion reward keys cannot be claimed again.
4. Remove a required definition/zone or introduce a duplicate participant ID. Verify a clear failure with no false `Active` transition.
5. Test M01 Tarrik to M02 Selene to Tarrik with distinct ammo, Echo, perks, and factions. Ensure target input is unavailable until its own record is applied.
6. Test a listen server/client retry: owned status counts release once, replacement participant references replicate, and late appearance initialization cannot overwrite the restored loadout.

## Aurelion encounter objectives (7 September 2026)

`ASovCampaignEncounterObjective` binds a director to a campaign beat with
`RequiredEncounterId`. Generic campaign completion and interaction terminals cannot
complete these beats. The objective observes the native Active attempt and defers
victory publication until the director's callbacks have returned. Publication checks
the exact mission, protagonist, possession, ASC/avatar, readiness and actor-info
epochs, campaign transition epoch, director lifecycle generation, and attempt ID.
The campaign journal stores the encounter ID and attempt receipt. Save validation
replays those fields and rejects missing, mismatched or reused receipts.

Author the objective's `EncounterDirector`, `MissionId`, `CompletionBeat` and optional
`StartVolume`/`bStartOnPlayerOverlap`. `StartEncounter` captures the existing director
entry before calling its native begin path, which writes and verifies the ArenaEntry
boundary before releasing combat in a campaign GameInstance. Keep placed NPCs ready
and quiescent behind an occluded approach until entry capture. A failed disk boundary
leaves the captured encounter inactive and frozen so entry can be requested again.

Register protected NPCs in the same director's `Participants`, with
`bRequiredForVictory=false` and `bAllowMassRepresentation=false`, and include their
stable IDs in `ProtectedParticipantIds`. Enemy formation members are ordinary
required-for-victory participants. Protected death or destruction fails the attempt;
confirmed hostile deaths cannot reverse that failure. The frozen entry preserves
survivor membership and restores their existing NPC/resource records on retry.
Protection monitoring stays active during actor/Mass promotion completion. There is
no second NPC save or consequence ledger.

A normal failure uses `RetryEncounter` through the objective's entry function or
volume re-entry. A success whose authority context was retired before publication
requires loading the verified entry checkpoint. It is intentionally not awarded to
a replacement pawn, mission or loaded state. Both the live objective and a retained
director flag hold save admission until the campaign receipt commits, so destroying
the objective actor cannot overwrite the recoverable entry with an orphan victory.
A restored active encounter follows the existing Failed -> Retry policy; a loaded
Succeeded state never manufactures a new receipt.

Native coverage: `ProjectVelkorran.Campaign.EncounterObjective.*` exercises production
entry capture/begin, real delegate consumption of externally supplied combat deaths,
exactly-once publication, survivor death/destruction, callback readiness/possession/
load/destruction retirement, native entry retry and receipt serialization validation.
These tests are authored for Unreal automation and require execution on the full
UE 5.7 checkout. This source-only preparation does not qualify their runtime result.
