# Restored Tarrik is missing his permitted sword attack

`RestoredGrantReadback-20260920-105229-61e5e254/restored-grants.json` records the
actual companion after a public load of the genuine generation-18 E4A checkpoint
and its authored retry. The restored curated list contains exactly:

- `GA_Tarrik_Guard`
- `GA_Melee_Punch_Unarmed`

The currently loaded mission profile additionally permits
`/Game/Aurelion/Characters/Melee/GA_Tarrik_MeleeLight`. The actual equipped
`WI_Velkorran` grants that light attack and its heavy variant. The missing light
attack in the restored companion list explains why native candidate checks can
report the light attack available while `TickContextCommand` skips it: command
dispatch requires exact membership in CuratedAbilities.

This checkpoint predates `CompanionPrimaryRepair-20260919-183329-94f445f0`.
`PrepareProxyFromSnapshot` copies Snapshot.Grants into CuratedAbilities; it validates
each saved entry against current curation but does not fill missing entries.
The old list is therefore still a permitted subset and loads successfully,
while omitting the primary sword attack required for useful combat.

The initial readback `RestoredCompanionGrants-20260920-104646-0efd6ed3` failed
when attempting to reflect private CopiedGrants. Its error remains in Editor.log.
The corrected readback uses the public component list. It stopped its observer
immediately after capture, ended PIE and exited normally. It supplied no damage,
grants, transforms or altered save bytes. No gameplay or content was edited.

## Proposed native repair

Reconcile restored companion permissions with the current mission's curated
kit using validated evidence that the inactive protagonist already unlocked
the ability **or owns the weapon supplying it**. The runtime follow-up below
rules out using `GrantedAbilities` alone as complete weapon-kit evidence.
Do not blindly grant all current abilities or silently increase
ability levels. Apply the same rule to full campaign load and encounter retry;
preserve actor identity, resources and the campaign journal. Missing unlocked-kit
evidence must produce a clear diagnostic rather than invented progression.

Required verification: this original checkpoint gains its proven permitted
primary attack; excluded and locked abilities remain unavailable; duplicate and
invalid grants still fail validation; restored ability levels retain proven
limits; retry and full load agree; the golden save stays unchanged; a fresh
campaign remains valid; live Tarrik actually attacks and deals native damage.

The runtime defect is established for this older checkpoint. It is not proof
that current fresh-companion dispatch is broken. The native repair has not been
implemented: the engineering handoff restricts C++ changes outside the existing
HUD and Selene-animation exceptions.

## Repository and preservation check

Fetched origin during this review. The tracked content branch had no upstream
commits missing locally (113 local commits ahead). Main had one unmatched
commit, `532b42ab`, consisting of 112 binary asset paths and no restoration
source changes. Blob comparison found 104 already identical to HEAD and eight
different existing assets: Selene light melee, both mission definitions, Elite
ability configuration, Elite and Enforcer Blueprints, SecurityCrossfire behavior
tree, and ReceiverStatus material. None was missing. No merge or binary
replacement was performed; in particular the behavior tree was not opened.

This turn changed documentation only. The existing full native gate
`20260920-104320-f1997d15` passed build, 720 tests, coverage and source integrity.
The original M12 hash is unchanged:
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Creator grenade assets and GASPALS remain untouched. The readback script is
retained at `Saved/Validation/inspect_restored_companion_grants.py` and its
successful JSON at the evidence directory above. The proposed native repair
has been explicitly submitted for user approval; no repair is claimed yet.

## Follow-up: saved weapon ownership is required

`RestoredUnlockEvidence-20260920-112823-b1336333` repeated the public load of
the original generation-18 checkpoint and its authored E4A retry. It used the
engine's read-only `GETALL` command, restricted by exact player-state name and
world outer, to export `ProtagonistSnapshots`. This property is reflected but
not Python-exposed; no native API or gameplay data was changed to inspect it.
The observer stopped immediately after the readback, ended PIE and exited.

The loaded Tarrik snapshot has ten granted abilities: unarmed punch, crouch,
jump, wield, reload, death, sprint, evade, cover and Cinder Sticky Grenade.
Neither native light/heavy melee nor the superseded sword attack appears.
Selene's eleven entries similarly contain no Verity attack. Thus neither a
current-class intersection nor an old-sword-class alias would repair this save.

The snapshots' serialized `InventoryComponent` records contain these weapon
class paths:

| Protagonist | Weapon classes in saved inventory bytes |
|---|---|
| Tarrik | WI_Cinderline, WI_Velkorran |
| Selene | WI_Axiom, WI_Staccato, WI_Verity |

The 2,141-byte Tarrik and 3,320-byte Selene inventory payloads were extracted
from the native property export. This is evidence in a natively loaded snapshot,
not a separate inventory restore or an independently validated byte decoder.
The raw export and concise `saved-unlocks-summary.json` remain in the run.
The read-only diagnostic is `Saved/Validation/inspect_restored_companion_unlocks.py`.

Source explains the discrepancy: `UWeaponItem::HandleUnWield_Implementation`
calls `RemoveWeaponAbilities`, whereas `ASovPlayerState::CaptureProtagonistSnapshot`
enumerates currently activatable ASC specs. A holstered weapon can therefore
be owned without appearing in `GrantedAbilities`; that list is not a complete
weapon-unlock history.

`ASovProtagonistCompanionCharacter::PrepareProxy` already accounts for this in
the live outgoing-player path: it checks real inventory weapon grants against
the explicit mission allowlist, uses an existing spec's level when available,
otherwise level 1, and records weapon provenance. Restoration should follow
that ownership rule using validated saved evidence. Merely adding everything
on the NPC's default weapon would not establish the player's ownership.

`USovConvergenceCompanionState::StageInitialCompanion` currently intersects
only `Kit.GrantedAbilities`. Its handling of a legitimately holstered saved kit
must be included in the repair and tests, alongside full load and encounter
retry. This source path is a demonstrated coverage gap; the current follow-up
did not run a fresh initial-convergence comparison.

Required additional cases: owned/holstered versus wielded weapon parity,
unowned weapon exclusion, malformed/missing inventory evidence, weapon-grant
provenance and no duplicate grants. Preserve saved levels for non-weapon grants
and use the existing level-1 weapon fallback only where ownership validates it.
Do not deserialize untrusted saved bytes into the active player's inventory
merely to query them, or overwrite the golden save to make validation pass.

No native repair is implemented or approved by this follow-up. It corrects the
earlier proposal before implementation; native companion approval is still
pending. The unchanged-source full gate remains `20260920-112236-2f605927`,
with build, 720 tests, coverage and source integrity passed.
