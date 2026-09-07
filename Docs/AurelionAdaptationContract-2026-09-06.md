# Aurelion adaptation contract

Authority: creator requests of 6–7 September 2026, including implementation of `Aurelion_Level_Layout_Plan.pdf` dated 7 September; `sovereign-call-origins-TRADE.pdf` uploaded 3 September; August 14 TDD v2.0 §§3.3–3.4 and 17.1–17.5; approved exceptions in `CampaignV2ChangeLog.md`. Source baseline: `8b1551e862018ec3fc8e4ed6ff90e2ad51de6c04`.

This is a source preparation package for the lower-Aurelion slice, now aligned to the adopted [full layout contract](AurelionLayoutContract-2026-09-07.md). Native contracts, tools and regression fixtures are not authored mission completion. Existing review missions remain technical fixtures, with their original evidence and restrictions intact.

## Source map

Page numbers below are the manuscript's printed pages, not PDF viewer page indices.

| Chapter | Printed pages | TDD mission | Adaptation use |
|---|---|---|---|
| 23, Last Throne, Last Wound | 397–433 | I11 | Prior preparation, independently authenticated evidence and approach context; not a requirement to build the fleet-preparation interstitial for this slice. |
| 24, Fire and Frost | 437–473 | M12 | Opposed arrivals; confrontation; rescue and first cooperation; contaminated capture platforms; isolated targeting interoperability; quarantine; contrary-witness recognition. |
| 25, Contrary Witness | 474–503 | M13 | Independent assent; Meridian completion; Fifth Witness; recovered containment; release withheld; voluntary decision to talk. |
| 26, Friends Not Soon Forgotten | 504–528 | M13 continuation | Aftermath, reciprocal evidence custody, containment pact and eventual separate departures. |

The TDD selects a 50–60-minute production excerpt, not a literal recreation of every event in these chapters. The adopted layout explicitly changes the TDD outline's phase order: separate approaches meet at the carrier-rescue scene, shared Eclipse combat and the local priority occur before quarantine, then recognition, independent assent, Witness, unavoidable grammar propagation and quiet aftermath follow. This preserves manuscript causality. The target is 50 mandatory minutes plus eight optional minutes, 58 total. Full fleet simulation, the later final battle and complete progression trees are outside the slice.

## Fixed outcomes

| Contract | Required distinction | Forbidden shortcut |
|---|---|---|
| Meridian | Completion stabilizes the boundary and enables terminal authority. | Treating completion, authority availability and prison opening as the same boolean. |
| Containment | The boundary remains closed and terminal release is withheld during this material; grammar propagates despite the attempted interruption and Crownmark Five stays integrated. | A debug terminal, local choice or ordinary encounter victory opening the prison. |
| Contrary assent | Tarrik and Selene are independent authorities; neither commands the other's assent. | Treating companion arrival, a remote command, recording or one protagonist's input as the other's assent. |
| Fifth Witness | They observe history; the King contained the Eclipse and entered the contained side as the reference. | Letting a combat callback award the witnessed knowledge or allowing the player to alter the historical outcome. |
| Lyric | She survives the immediate event; the acute response quiets, but corruption is not reversed. | Awarding a generic cure or complete corruption clearance. |
| Cooperation | Shared threat tracks/firing corridors retain isolated command authority; governments remain opposed. | Globally changing Dominion/Reformation relations to permanent allies. |
| Voluntary stay | They choose to speak after quarantine ceases to compel their proximity. | Claiming a still-locked door proves the voluntary relationship outcome. |
| Reciprocal custody | Selene receives the Cauldron recorder; Tarrik receives Record 7283. | Reversing recipients, granting an unspecified shared inventory item, or equating knowledge with physical custody. |
| Pact | No release without direct contrary concurrence; no proxy/recording, controlled aperture, bypass research or assent from a compromised bearer. | Turning the local slice choice into authorization to violate the pact. |

These facts should use the existing campaign journal, evidence custody and cinematic/handoff mechanisms. The package does not introduce a second save system or a new universal terminal authority. Placeholder scene identifiers are dependency contracts, not proof that a scene exists or has played.

## Gameplay adaptation decisions

The layout fixes thirteen zones, four physical encounters and one local priority. Z01's E1 is six Reformation drones with four active at once. It is not the earlier technical mixed-survivor preparation encounter. Z04's E2 combines four Dominion Enforcers and two contaminated drones; both physical receivers must be disabled through actual interactions. Z06's E3 is the shared-breach rescue. Z08's E4 has two native phases in the same physical arena, preserving one elite across the Selene-to-Tarrik handoff. See the full layout manifest for exact counts, gate conditions and checkpoint bindings.

The existing `M12_AurelionTarrikPreparation` remains a separate three-beat technical harness. It demonstrates registered combat, protected participants and recovery. Its arbitrary proxy roster does not qualify canonical E1, E3, command coordination, Eclipse presentation or the full layout. Its setup script remains deliberately separate from full mission authoring.

The local decision is now the adopted **WestStretchers versus EastWalkers** priority in Z07, before E4. Both groups are mixed faction and both reach safety. West provides access to the recovery cache; East provides an earlier flank shutter. These are alternatives, not cumulative rewards, and the east route later opens normally for both. CP4b precedes the prompt, one acknowledgment commits the choice, and CP5 follows it. The aftermath consumes the same saved choice. Named cast survival and the containment/evidence spine never branch on this decision.

Cage destruction is fixed: Tarrik destroys the Dominion resonator and Selene the Reformation cage. Both unauthorized-fire reviews and isolated threat-track interoperability are recorded. The shared-threat bridge does not merge command authority. The final combat happens before quarantine; the Fifth chamber and aftermath contain no added battle.

The full mission definitions deliberately reject old serialized canonical beat orders. After updating native code, inspect existing M12/M13 scaffold assets in the owned `Scaffolds` folder. Back up content-bound work and explicitly reconcile or recreate only those canonical assets from the new classes. Never silently replace them, discard their authored scenes, or rebuild the unchanged technical preparation map as a migration shortcut.

## Morning qualification sequence

1. Compile the exact branch in the complete UE 5.7 work project and run the complete source-discovered native selection. The earlier 424-pass result belongs to the baseline and does not qualify these changes.
2. Validate the full layout manifest. Build the shared Z05–Z12 skeleton first, then add entrances Z00–Z04. Supply actual assets, participant identities, wave producers, receivers, choice support and scene/companion anchors. The technical prep setup is an optional separate harness, not the full-layout builder.
3. Prove ordinary input, perception and hostility on a cold encounter start. Record the AI timeline before initialization; do not infer absent events from an observer attached late.
4. Exercise hostile victory, protected-participant death, player death, retry, late callbacks, save/load, and repeated completion. Require one durable success fact for one successful encounter.
5. Qualify every full-layout boundary: isolated entry cut, delayed shared companion activation, both E2 receivers, E3 rescue, exclusive Z07 support, both E4 phases, quarantine, independent assent, grammar consequence, reciprocal evidence and separate departures. Test actual content and native proof together.
6. Cook and play the authored route with physical keyboard/controller input. Preserve whole-campaign preflight failures until fixed; a selected-map cook is not a full-campaign shipping pass.

The final slice gate still requires the TDD's player-research thresholds, checkpoint/reload success above 99.5% in automation/soak, representative performance captures and measured production cost. This source package cannot replace those gates.
