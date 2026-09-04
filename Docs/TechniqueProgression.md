# Native Technique progression policy

This implements the campaign policy from TDD v2 section 10 over Narrative's existing `USkillTreeComponent`, `UTreeSkill` and `UTreePerk`. It does not add XP, character levels, a second progression graph, random rewards or an equipment economy.

`ASovPlayerState` now creates `USovTechniqueComponent` under the original `SkillTreeComponent` subobject name. Existing protagonist snapshots therefore serialize the same component boundary. Default budgets are 20 earned points for Tarrik and 20 for Selene; authoring accepts 18–22, matching the TDD. A configured active tree must contain three uniquely named branch classes and 24–30 total purchasable ranks. Empty progression records can still save/load before tree assets are authored; purchases remain unavailable until the tree is valid.

## Authoring

1. Create data-only Blueprint branches derived from `USovTechniqueSkill`, with a stable `BranchId` and Tarrik or Selene identity. Configure the existing `Perks`/`LinkedTo` graph. Use unique branch/perk classes across both protagonists; the existing Narrative serializer identifies them by class. Assign all six branch instances to the inherited `Skill Tree Skills` array on the PlayerState's Technique component.
2. Create data-only perk children of `USovTechniquePerk`. Existing display name, description, preview video, linked nodes and rank count remain Narrative fields. `Required Branch Investment` and `Incompatible Perks` declare tier/capstone policy. All incoming graph links must be satisfied. Cycles, duplicate classes and links to the other protagonist are rejected.
3. Author gameplay through `Persistent Effects` and `Granted Abilities`. Effects must be infinite, nonperiodic, nonstacking Gameplay Effects with no executions; their spec level is the purchased rank. Abilities receive the same rank and the perk as their source object. Upgrading replaces this perk's prior native grants. Use these effects/abilities to change timing, resource conversions and move behavior. Do not use instant/direct attribute mutations or override the Blueprint `Set Perk Level` event. Such overrides fail policy validation, and direct calls cannot bypass the component's purchase/restore transaction.
4. Place `ASovTechniqueSafePoint` at a quiet-space workbench/reflection area. Give it a stable `Safe Point Id` and size `Safe Bounds`. Modification requires the actual pawn to be inside these native bounds, idle/alive, with no Active/Restoring encounter and no nearby living hostile. A caller-provided “safe” boolean is never trusted. Use `Can Modify Techniques` for UI availability and `Respec At Safe Point` for free respec.
5. Place a `USovTechniqueRewardSource` on an authored reward/reflection actor. Give it a globally unique stable `RewardId`, correct protagonist, 1–5 points and a completion proof: mission success, completed campaign beat, or succeeded encounter. Mission/beat proof reads `USovCampaignStateComponent` on the current controller. Encounter proof reads the native director and atomically consumes its completion-reward key. The player must be within the authored claim distance. Invoke `Claim Reward` with that component after the authored event.

The inherited raw `Give Skill Points` entry point is disabled even when called through a base-class pointer. Reward IDs are recorded before feedback delegates and cannot be claimed again after respec. Rewards exceeding the remaining lifetime budget are rejected as a whole; no partial award or farmable overflow is created.

Native perk purchase validates the requested owning branch, active protagonist, prerequisites, branch investment, symmetric incompatibilities, rank limit and point balance. The existing Narrative purchase path still creates the perk and owns its grants. A failed native grant restores the previous perk save state and point balance. If restoration itself fails, the policy becomes invalid and blocks further mutation until a valid save/protagonist restore.

## Save, retry and protagonist handoff

The component saves the current protagonist identity, unique reward ledger, available points and inherited purchased-perk data. Load checks identity, budget, earned/spent balance, allowed classes/ranks, prerequisites, investment and incompatibilities before applying perks. Repeated restore removes prior owned grants before applying the saved ranks. Invalid state removes owned grants and blocks purchases/respec rather than minting points.

For a first-ever protagonist with no saved snapshot, native handoff calls:

```cpp
Cast<USovTechniqueComponent>(PlayerState->GetSkillTreeComponent())
    ->InitializeNewProtagonist(TargetIdentity);
```

For a previously visited protagonist, restore its existing Narrative component record and check `IsTechniqueStateValid()` before committing the handoff. Encounter retry uses the existing entry snapshot. Free respec refunds exactly earned points and keeps all reward claims. No API modifies an inactive protagonist during a mission.

First-entry initialization requires the target identity to be active on the shared ASC. It rejects a populated profile for that same protagonist; it cannot serve as a point/reward reset. If an outgoing-effect removal callback replaces the avatar, initialization stops, retains the prior ledger, marks its partial live state invalid and requires an explicit restore.

The existing generic perk cleanup method is now native-only so Blueprint content cannot bypass the safe-point policy. Authored abilities/Gameplay Effects still need acceptance testing for their individual behavior; native bookkeeping cannot certify those assets' design quality.

Native perks report their exact ability/effect handles through Narrative's `ISovOwnedPerkGrants` hook. Unrelated grants from synchronous GAS callbacks are never attributed to the perk. Grant/removal callbacks are fenced by the original PlayerState, pawn, ASC, protagonist identity and purchased-perk membership; an invalidated transaction stops granting and cannot replay the old profile onto a replacement pawn. Campaign handoff must reject `IsTechniqueMutationInProgress()` before clearing state. A complete inherited perk snapshot is published before `OnTechniquesChanged`, so saves initiated by that notification are consistent. Save serialization during an unfinished grant/removal, or with a known-invalid ledger, fails. Native snapshots and Narrative's save path propagate archive errors and preserve the caller's last good output record.

## Files

- `Source/ProjectVelkorran/Public/Private/Progression/SovTechniqueComponent.*`: policy, ledger, authoritative purchase/respec and validated load.
- `Source/ProjectVelkorran/Public/Private/Progression/SovTechniqueTypes.*`: identity metadata on existing branches and reversible native perk grants.
- `Source/ProjectVelkorran/Public/Private/Progression/SovTechniqueRewardSource.*`: authored reward/completion proof.
- `Source/ProjectVelkorran/Public/Private/Progression/SovTechniqueSafePoint.*`: physical safe point and encounter/hostile checks.
- `Source/ProjectVelkorran/Private/Progression/SovTechniquePolicy.h`: production budget/ledger arithmetic.
- `Source/ProjectVelkorran/Private/Framework/SovPlayerState.cpp`: original subobject replaced with the policy subclass.
- `Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTestFixtures.*`, `SovTechniqueRuntimeTests.cpp`: World/GAS/Narrative-save integration tests.
- `Tests/Portable/SovTechniquePolicyTests.cpp`: compiled production policy checks.

## Validation

Compiled and ran the exact production budget/ledger helper with GCC C++17 and `-Wall -Wextra -Werror -pedantic`: 3,500 checks passed. Coverage includes the 18–22 range, claim deduplication, cap rejection, negative/overflow inputs, point conservation and free-respec arithmetic.

Added six Unreal automation registrations under `ProjectVelkorran.Campaign.Techniques`:

- `RewardsPurchaseRespecAndRestore`: native PlayerState subobject, encounter proof, blocked raw point minting, rank replacement, direct perk-call rejection, repeated save restoration, physical safe-point restrictions, refunds and claim preservation.
- `InvalidProofAndLedger`: wrong/incomplete proof rejection, inconsistent saved balances and refusal to overwrite a checkpoint with a known-invalid ledger.
- `CommittedNotificationSnapshot`: capture inside the actual `OnTechniquesChanged` delegate, then respec/restore to verify coherent purchased ranks and spent points.
- `GrantCallbackSnapshotAndOwnership`: capture inside an actual GAS grant callback; both component capture and Narrative `CreateActorRecord` reject unfinished state and preserve their previous output. Unrelated ability and effect grants from that callback survive respec.
- `AvatarChangedDuringGrant`: a GAS callback changes the shared ASC avatar; remaining grants stop and the returned effect handle is cleaned without taking ownership away from the replacement.
- `ProfileInitializationOwnership`: first-entry reset rejection for an existing/inactive profile and avatar replacement during outgoing-effect removal, preserving the prior ledger and requiring explicit recovery.

Unreal 5.7/UHT and these runtime tests could not be executed in this environment. Run `Scripts/Validate-Unreal.ps1 -EngineRoot <UE_5.7> -TestFilter ProjectVelkorran.Campaign.Techniques` after installing the required project plugins. Remaining editor gates are actual branch assets/point distribution, each perk's behavior and preview, first-entry/return handoffs, retry after rewards, save before/after respec, and paused/menu presentation. The TDD's full authored Technique content remains a Blueprint/data acceptance task.
