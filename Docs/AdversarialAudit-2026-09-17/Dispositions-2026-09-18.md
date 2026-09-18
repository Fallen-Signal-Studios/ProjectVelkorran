# Audit dispositions, 18 September 2026

The audit beside this file is a record of what was true on 17 September. This file records what is
true now. **The audit is not edited** — it stays as the reviewer wrote it.

## How to read this

Three levels of confidence, and the difference matters:

- **Verified closed / Verified open** — I read the current code and checked the finding's own claim.
- **Commit claims it** — a commit names the ID in its body, but I have not re-read the code. The two
  do not track each other: `UX2-08` is named by no commit although work landed for it today, and
  `PC2-01` was named by one commit while being closed by content changes that named none.
- **Not swept** — nobody has looked since the audit.

Where a finding turned out to be partly done or to rest on a wrong premise, that is said plainly,
because "still open" and "open for a different reason than written" lead to different work.

## P1 — all five were already closed

| ID | Disposition | Evidence |
|---|---|---|
| AR2-01 | **Verified closed** | `e6729a02` (17 Sep). `LoadCampaignEnvelope` no longer passes untrusted bytes to `LoadGameFromMemory`: raw frame CRC and length first, bounded reader, 64 MiB cap, class-name length limit, and only an already-loaded subclass of the envelope class is accepted. The nested Narrative payload still goes through `LoadGameFromMemory`, but only after the envelope's integrity check has passed. |
| AR2-02 | **Verified mostly closed** | `7e82c9cb`. `GameDefaultMap` is M12 and `GlobalDefaultGameMode` is `SovCampaignGameMode`, with a config comment explaining why. **Residual:** `GameInstanceClass` is still `/NarrativePro/.../BP_NarrativeGameInstance`. |
| AR2-03 | **Verified substantially closed** | `7e82c9cb`. `Validate-Campaign.py --release` builds Test and Shipping, packages Shipping, runs `-ShippingValidation` and fails on staged Narrative demo content; a Shipping binary was built 17 Sep. **Residuals:** `Validate-Unreal.ps1` still counts `succeededWithWarnings` as passing and hard-codes `packagedBuild = 'not run'`. Whether the release gate has ever passed is unknown. |
| EA2-01 | **Verified closed** | `cc30b7c8` (17 Sep) retires attempt combatants on completion and failure. `4ac0b0a7` (18 Sep) added the transfer regression the audit named as missing, and closed the second half: the phase boundary now consults live attempt combatants instead of owned participants alone. |
| PC2-01 | **Verified closed** | `WI_Velkorran` grants `GA_Tarrik_MeleeLight` and `GA_Tarrik_MeleeHeavy`; `WI_Verity` grants `GA_Selene_MeleeLight` and `GA_Selene_MeleeHeavy`. All four derive from `SovGameplayAbility_Melee` and have matching `SovMeleeAttackDefinition` data. Eight assets now use the framework, not four. **Residual:** the legacy `GA_Attack_Melee_Sword_1H_Tarrik` and `GA_SovVerityTwinAttack` still exist and are still referenced by `DA_M12_FireAndFrost`, `DA_M13_ContraryWitness` and `NWI_Velkorran` — worth checking whether those are vestigial or a second live grant path. |

## P2 — verified open

Each of these I read the current code for, and the finding's claim still holds.

| ID | What is still true |
|---|---|
| AR2-07 | Capture judges p95 with 5% of frames allowed over budget; not the §18.8 gate. |
| AR2-09 | `SovApplicationInterruptionMenu` constructs only `ResumeButton`. No exit after account loss. |
| AR2-10 | Cloud revision discovery still takes the lexicographic maximum filename (`SovOnlinePlatformServicesAdapter.cpp:213`), not a clock- or generation-ordered pick. |
| AR2-12 | Still one runtime module; `Source/` holds only the runtime, editor and test modules. |
| AR2-13 | `LoadSynchronous` on the elite summon path, in combat. |
| EA2-04 | `DefeatedParticipants` is filled only by `HandleDeath`; losing a participant without a death receipt still stalls victory. |
| EA2-08 | `SetDecisionTier` and `SetParticipantRepresentation` have no production caller outside their own definitions. |
| EA2-09 | Nothing in the campaign directors references `SovDominionPackCoordinator`. |
| PC2-09 | No grant of the finisher or resonance abilities in character or framework source. Content may grant them; not checked. |
| PC2-13 | `State_Poise_SuperArmor` is read in five places and added in none. There is no producer. |
| PC2-14 | `FSovMeleeAttackNode` still has one `NextNode`, one `FollowUpInput`, one `DefensiveInput`. |
| CN2-06 | `SovCampaignNarrativeAdapters.cpp:91` still calls `CompleteBeat` and discards the result. |
| CN2-09 | `ScanTarget` and `RequestCompanionAnalysis` have no caller outside their own library. |
| UX2-07 | No remapping or key-binding UI in the project's menus. |
| UX2-10 | No hit stop anywhere: no `TimeDilation` or hit-stop code in project or plugin source. |
| UX2-14 | `USovFrontendComponent::TickComponent` calls `RefreshFrontend()` every frame; the accessibility presentation ticks too. |
| UX2-16 | No HUD consumer for the reduced-corruption preference. |
| UX2-17 | `SovNarrativeCueComponent.cpp:154-155` still loads bark sound and audio class synchronously. |

## Re-rate or re-word

| ID | Why |
|---|---|
| AR2-19 | **Premise partly wrong.** `InventoryComponent` is load-bearing — cinematic inventory transactions, ammo pickups, companions all use it. The XP attribute is already annotated in source as legacy and unused. Only `VendorInventoryComponent` looks genuinely dead. "Remove the December systems" is not the right instruction; "remove the vendor component and decide whether XP should be deleted or left annotated" is. |
| AR2-04 | **Partly addressed.** The validator now dispatches dialogue, cues, evidence, melee definitions and corruption profiles, and runs a dependency closure with cook roots and a shipping-validation mode. The audit's specific gaps — map actors, GUIDs, role budgets, ability cleanup, cinematic skip — were not individually checked. |
| UX2-08 | **Partly done, 18 Sep.** Plural forms, culture-aware dates, grapheme-safe letterspacing and a gather config are in, and the gather runs clean over 897 entries. Still open: save and settings errors reaching the UI as raw `FString`, the 27px-per-character width heuristics, story cues without string-table keys, and pseudo-localization, whose mechanism I got wrong once already — a culture entry crashes the compile step. |

## Commit claims it — not re-verified

`AR2-06`, `AR2-08`, `AR2-17`, `CN2-01`, `CN2-04`, `CN2-05`, `CN2-10`, `EA2-05`, `EA2-06`, `EA2-07`,
`EA2-10`, `PC2-02`, `PC2-03`, `PC2-05`, `PC2-06`, `PC2-07`, `PC2-08`, `PC2-11`, `UX2-01`, `UX2-02`,
`UX2-04`, `UX2-06`, `UX2-15`.

Most of these were done in this session with a regression each, so the confidence is high — but the
line above is what I actually checked, which is that a commit names them.

## Not swept

`EA2-02`, `EA2-03`, `CN2-02`, `CN2-03`, `CN2-07`, `CN2-08`, `UX2-03`, `UX2-05`, `UX2-11`, `UX2-13`,
`UX2-18`, and every P3.

## What this says about the audit

Every one of the five P1s was closed before anyone worked from this list. The audit was written the
same day several of those fixes landed, so it was partly stale when it was filed rather than becoming
so since.

**PC2-01 is the cautionary one.** The first version of this file called it verified open, because I
read the source, found the native framework used only by enemy abilities, and accepted the audit's
content claim about what the protagonists' weapons grant. The weapons had been switched over. A claim
about content has to be checked against content, and a second trap sat behind that: searching package
dependencies for a C++ class name returns zero for every native parent, which looks exactly like a
finding. Parentage comes from the registry's `ParentClass` and `NativeParentClass` tags, or from
`isinstance` on a default object.

Verify before implementing — it is minutes against an hour, and on this audit it has now changed the
right answer more often than it has confirmed it.
