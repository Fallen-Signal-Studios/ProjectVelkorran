# Sovereign Call: Origins
## Technical Design Document v2.0

**Product direction:** Cinematic third-person narrative action-adventure with selective RPG progression  
**Primary campaign:** Single-player, two authored protagonists  
**Engine:** Unreal Engine 5  
**Document status:** Pre-production baseline  
**Revision date:** 14 August 2026  
**Supersedes:** December 2025 Technical Design Document for the campaign product  

---

# 0. Document Authority and Use

## 0.1 Purpose

This document defines the intended player experience, campaign structure, gameplay systems, content architecture, technical foundations, production sequence, and validation criteria for *Sovereign Call: Origins*. It converts the current creative direction into a production-facing specification.

The central change from the December 2025 design is categorical: *Origins* is no longer an open-ended class RPG organized around loot, character builds, morality scores, and broad world simulation. It is a cinematic third-person action-adventure built around Tarrik Walcur and Selene Veyne as specific people. The campaign must preserve the manuscript's canon, embody the protagonists' ideological and physical differences, and make consequential local choices without allowing the player to fracture the story's required outcomes.

This TDD is the authority for the campaign game. It does not replace the final manuscript as story canon, and it does not authorize a multiplayer or class-based product at launch.

## 0.2 Source hierarchy

When two sources disagree, use this order:

1. The final print-ready *Sovereign Call: Origins* manuscript dated 9 August 2026.
2. Explicit creator decisions recorded after the manuscript.
3. This TDD and its approved change log.
4. Approved mission scripts, combat specifications, and art bibles derived from this TDD.
5. The December 2025 TDD, only for reusable technical foundations that do not conflict with the new direction.
6. Earlier summaries, outlines, and prototypes.

No gameplay document may silently overwrite a fixed story event. If adaptation pressure requires a change to the final manuscript's causality, relationships, locations, or outcome, the narrative director must raise a canon-change request rather than treating the change as ordinary implementation.

## 0.3 Decision labels

The document uses four status labels:

| Label | Meaning | Change authority |
|---|---|---|
| **LOCKED** | Product identity, canon, or core experience requirement | Creative director and game director together |
| **BASELINE** | Production target adopted for planning and implementation | Discipline lead may propose; game director approves |
| **PROTOTYPE** | Initial value or behavior that must be tested | Owning lead may tune within the stated intent |
| **DEFERRED** | Intentionally outside the launch campaign | Requires separate greenlight and scope |

Numeric values in this TDD are **PROTOTYPE** unless explicitly marked otherwise. A prototype value is not optional merely because it may change; the specified system and its design purpose remain required.

## 0.4 Change-control rule

Every material design change must identify:

- the player problem being solved;
- the affected pillar and acceptance criterion;
- the systems, missions, assets, save data, and test plans touched;
- whether the change alters canon, product scope, or only tuning;
- the proposed cut or schedule impact if it adds work;
- the person who owns the final decision.

The project must maintain one authoritative integration branch and one approved playable build. This is especially important for Unreal levels using World Partition and External Actors. Feature work may branch, but map ownership, integration windows, and merge responsibility must be explicit.

## 0.5 Design mandate

**LOCKED:** *Origins* should feel physically immediate and cinematic in the way a modern third-person war game does, while retaining the character density, ideological conflict, companion intimacy, and reflective pacing associated with a strong authored RPG.

The shorthand is:

> The bodily conviction and battlefield scale of *Space Marine 2*, carrying the soul and character consequence of *Knights of the Old Republic*, told through the fixed story of *Sovereign Call: Origins*.

This is an experiential reference, not a content-copying brief. The game must develop its own combat rhythm, visual language, faction behaviors, and narrative interface from the setting's rules.

## 0.6 Product thesis

The player alternates between two people trained by rival civilizations:

- **Tarrik** meets pressure by holding ground, carrying weight, protecting others, and making committed decisions.
- **Selene** meets pressure by reading systems, creating openings, moving precisely, and refusing false certainty.

The campaign eventually brings them into cooperation without sanding away their differences. Combat, traversal, camera, ability economy, dialogue, environment, and animation must all express that premise.

## 0.7 Explicit non-goals

The launch campaign is not:

- a nine-class character-switching RPG;
- a loot treadmill or rarity chase;
- an open world or galaxy ownership simulation;
- a morality-grid game;
- a power-fantasy avatar creator;
- a romance-management or approval-score simulator;
- a dialogue-heavy game in which combat is secondary;
- a horde shooter without authored character work;
- a live-service campaign;
- a co-op campaign whose cinematics and encounter design must accommodate two human players;
- a branching rewrite of the novel's major outcomes.

Systems serving those goals must not enter the campaign through incremental feature creep.

---

# 1. Product Definition

## 1.1 High-level product

| Property | Target | Status |
|---|---|---|
| Genre | Cinematic third-person narrative action-adventure with selective RPG progression | **LOCKED** |
| Mode | Single-player campaign | **LOCKED** |
| Protagonists | Tarrik Walcur and Selene Veyne, in authored alternation | **LOCKED** |
| Perspective | Third person; combat-centered over-the-shoulder camera | **LOCKED** |
| World structure | Wide-linear missions, compact authored hubs, controlled revisits | **LOCKED** |
| Combat | Melee/ranged hybrid, character-specific defense and Echo generation | **LOCKED** |
| Player agency | Local tactics, dialogue posture, optional priorities, and bounded consequences | **LOCKED** |
| Major story outcomes | Canonical | **LOCKED** |
| Launch multiplayer | None | **LOCKED** |
| Engine | Unreal Engine 5 with Gameplay Ability System | **BASELINE** |
| Target platforms | Windows PC, PlayStation 5, Xbox Series X|S | **BASELINE** |
| Critical-path duration | 18–22 hours | **BASELINE** |
| Completionist duration | 22–27 hours | **BASELINE** |
| Rating target | ESRB M / PEGI 18 equivalent | **BASELINE** |
| Primary performance target | 60 fps gameplay mode; 30 fps quality mode where supported | **BASELINE** |

Platform commitments remain subject to financing, publisher requirements, and certification planning. The gameplay architecture must remain platform-neutral within the baseline console generation.

## 1.2 Audience

The primary audience values one or more of the following:

- forceful third-person combat with readable impact;
- authored science-fantasy characters and political conflict;
- narrative consequence without an unmanageably branching plot;
- a substantial single-player campaign without live-service obligations;
- mastery expressed through timing, space, target priority, and character knowledge;
- quiet character scenes that give action emotional context.

The game assumes no prior knowledge of the novels. Existing readers should recognize events, relationships, places, and themes, but the adaptation must teach every term required for play through action, context, and optional codex material.

## 1.3 Player promise

Within the first hour, the player must understand that:

1. Tarrik and Selene are both highly capable, but capability feels different in their hands.
2. Combat rewards decisive rhythm rather than passive stat comparison.
3. The Dominion and Reformation each contain real virtues and structural dangers.
4. the Echo is experienced through timing, will, precision, and relation—not as generic magic fuel;
5. choices matter to people and situations even though the central history remains authored.

By campaign end, the player should feel they have inhabited both sides of an ideological fracture and helped two contrary witnesses recognize one another without becoming interchangeable.

## 1.4 Campaign content budget

The planning baseline is:

- 15 major playable missions plus prologue and epilogue sequences;
- 5 compact social or operational hubs, several revisited in changed states;
- 8–10 major boss or command encounters;
- 25–35 authored combat arenas with lighter connective encounters between them;
- 20–30 optional investigation spaces or rescue/prioritization opportunities;
- 3 progression branches per protagonist, each with 8–10 nodes including cross-links;
- 4 equipped active abilities per protagonist by late campaign;
- 2 signature weapons or combat tools per protagonist, plus contextual equipment;
- no randomized weapons, armor tiers, or procedural missions in the campaign.

Content budgets must be revised after the vertical slice using measured authoring cost, not aspiration.

## 1.5 Session rhythm

A typical 45–75 minute session should contain:

- one purposeful setup or investigation beat;
- two to four combat encounters of different densities;
- one traversal, environmental, or infiltration challenge;
- at least one authored character exchange;
- one consequence, reveal, or tactical escalation;
- a clean checkpoint and natural stopping point.

Missions may intentionally break this rhythm for spectacle or grief, but no two consecutive missions should consist only of sustained combat or only of dialogue and walking.

## 1.6 Content and tone

The campaign contains military violence, assassination, battlefield death, political coercion, mass-casualty history, body horror associated with Eclipse corruption, grief, and institutional abuse. Violence should be forceful and consequential without becoming gleeful dismemberment spectacle. Eclipse imagery may be disturbing, but corruption is primarily a threat to distinction, agency, and identity.

---

# 2. Experience Pillars and Loops

## 2.1 Pillar: Embodied war

The player must feel armor mass, blade commitment, weapon report, formation pressure, and the danger of being surrounded. Inputs produce legible bodily action. Enemies react according to mass, poise, shield state, armor, and hit location. Large battles are staged so the player perceives a war larger than the active high-fidelity AI budget.

**Must be true:**

- Tarrik can advance through pressure when played well, but cannot ignore control threats.
- Selene can dismantle a stronger formation through information and precision, but cannot absorb sustained pressure.
- ranged and melee actions form one combat language rather than separate minigames;
- finishers resolve earned combat states and do not replace the core move set;
- feedback communicates why an attack succeeded, glanced, broke poise, or failed.

## 2.2 Pillar: Difference without erasure

Shared controls minimize relearning, while timing, distance, defense, animation, camera, and resource gain preserve identity.

The convergence fantasy is not that Tarrik learns to play like Selene or Selene becomes a lighter Tarrik. Each learns how the other's different judgment can complete what their own cannot.

**Failure conditions:**

- their optimal rotations are functionally identical;
- the same upgrade set can be copied between them;
- animation retargeting makes them share silhouettes and cadence;
- narrative scenes allow the player to assign beliefs contrary to their established characters;
- co-presence turns one protagonist into an ordinary companion with no agency.

## 2.3 Pillar: Consequence without canon fracture

The player may choose how a fixed person handles an uncertain moment, not replace that person with an avatar. Choices can affect casualties, trust, optional evidence, tactical resources, dialogue, subsequent assistance, and the emotional framing of required events. Choices cannot prevent Caelus's death, erase Lyric's fate, avert the Aurelion containment failure, or otherwise invalidate the manuscript's causal spine.

The design test for every choice is:

> Could this version of Tarrik or Selene sincerely do this now, and can the story absorb the consequence without lying about what happens next?

## 2.4 Pillar: Ideology made playable

Dominion and Reformation ideology must appear in mechanics and spaces, not only dialogue.

- Dominion doctrine favors visible command, disciplined formation, decisive protection, and the danger of authority becoming entitlement.
- Reformation doctrine favors distributed verification, adaptive systems, procedural restraint, and the danger of responsibility disappearing inside process.
- Eclipse pressure offers the seductive removal of conflict by eliminating distinct wills.
- Alethian truth, where present, preserves freely aligned distinction rather than uniformity.

No faction is reduced to a color palette or elemental damage type.

## 2.5 Pillar: Spectacle with breathing room

The campaign needs battles, collapsing structures, fleet-scale conflict, and encounters with impossible King-era machinery. It also needs cabins, archives, medical spaces, quiet travel, restrained conversations, and moments in which the player can look at what an action cost.

Every major spectacle must produce an aftermath. Every extended quiet sequence must either deepen relation, sharpen a decision, reveal a system, or build pressure toward action.

## 2.6 Core loops

### Campaign loop

1. Enter an authored mission as Tarrik or Selene.
2. Establish the immediate duty, uncertainty, or pursuit.
3. Traverse, investigate, and encounter faction systems.
4. Fight using the protagonist's specific rhythm.
5. make a bounded decision or establish a consequence.
6. Return to a ship, fortress, archive, or temporary refuge.
7. reflect with companions, review evidence, and spend Technique Points.
8. transition to the other protagonist or escalate toward convergence.

### Combat loop

1. Read formation, threat roles, weak points, cover, and escape space.
2. create or absorb an opening according to the protagonist.
3. build Echo by performing character-defining actions.
4. spend Echo to disrupt the encounter's structure.
5. exploit broken poise, disabled systems, or exposed priority targets.
6. finish, reposition, or protect before the formation recovers.

### Narrative loop

1. Encounter an incomplete account.
2. inspect evidence and hear competing interpretations.
3. choose posture, priority, or trust.
4. act under incomplete information.
5. carry the local consequence forward.
6. encounter later evidence that complicates the original decision.

### Progression loop

1. Complete a major story beat, optional objective, or mastery challenge.
2. receive a story unlock, Technique Point, or signature-equipment refinement.
3. choose a small number of legible mechanical changes.
4. test the change immediately in authored combat.
5. respec freely at quiet-space workbenches so experimentation is encouraged.

---

# 3. Campaign and Adaptation Structure

## 3.1 Adaptation rule

The game adapts the novel rather than transcribing it. Internal monologue must become performance, environment, player action, combat objective, investigation, or optional conversation. A chapter may become a mission, part of a mission, an interstitial, or an unlockable record. Several viewpoints may be intercut where the game benefits from dramatic contrast.

The adaptation must preserve:

- the order and causality of the five Crownmarks and Witness revelations;
- Caelus naming Tarrik heir and surviving Selene's opening shot;
- Tarrik's decision not to take the market shot during pursuit;
- Tarrik and Lyessa's relationship, the Cauldron truth, and the rupture of trust;
- Selene's relationship with Lyric and Tharne;
- the altered Admonition and Record 7283;
- the role of Glass Citadel as independent authentication;
- Tarrik and Selene becoming contrary witnesses at Aurelion;
- Caelus's death by Tarrik's hand and Lyessa's continuation of Final Dawn;
- the Reformation's acceptance of truth while proceeding with Terminal Denial;
- Lyric's trial and death;
- the reciprocal escalation that opens containment;
- the epilogue discovery at Molahs.

## 3.2 Three-act game structure

| Act | Manuscript basis | Play purpose | Structural turn |
|---|---|---|---|
| I — The King's Road | Prologue and Chapters 1–11 | Teach both protagonists, factions, signature weapons, Crownmarks, and the cost of obedience | Tarrik and Selene independently awaken the Meridian and begin withholding truth from their own authorities |
| II — What They Buried | Chapters 12–23 | Expand investigation, reveal institutional falsification, fracture relationships, and authenticate the complete danger | Four remote anchors become release-ready; the road to Aurelion is known |
| III — The Last Wound | Chapters 24–31 and Epilogue | Converge protagonists, stage fleet-scale war, then make both home systems repeat the same failure in different language | Final Dawn and Terminal Denial teach the lock to open; containment is lost |

## 3.3 Baseline mission manifest

This manifest is the production baseline, not a scene-by-scene script. Mission boundaries may move during adaptation, but canon gates, protagonist ownership, and required outcomes may not move without approval.

| ID | Working title | Lead | Manuscript coverage | Primary play | Required outcome |
|---|---|---|---|---|---|
| P00 | The Beggar King | Cinematic | Prologue | Tone, Sundering imagery, prophecy | Establish the Good King's warning and Aurelion wound without fully explaining either |
| M01 | The Mantle | Tarrik | Ch. 1, 3, portions of 5 | Crownwatch tutorial, ceremonial exploration, assassination response, aerial/market pursuit | Caelus names Tarrik heir; Selene wounds Caelus; Tarrik refuses the unsafe shot; Crownmark pursuit begins |
| M02 | One Degree | Selene | Ch. 2, 4 | Infiltration tutorial, precision shot, close-quarters escape, covert extraction | Selene escapes with Lyric; Voss orders Caerion II and the Crownmark hunt |
| M03 | The Real Target | Selene | Ch. 4, 7 | Reformation hub, evidence preservation, surveillance evasion, ship departure | Selene preserves data marked for deletion and commits to Caerion II with Lyric and Tharne |
| M04 | Scars Under Neon | Tarrik | Ch. 5, 6, 10, 11 | Neon Veil pursuit, Dead Hand confrontation, undercity descent, seal encounter | Tarrik removes the Crownmark, weakens the seal, meets a Witness, and withholds the full truth from Caelus |
| M05 | Where Two Lights Die | Selene | Ch. 8, 9, 12 | Caerion II expedition, survival traversal, Silent Crown puzzle-combat, Witness vision | Selene claims the Crownmark, receives “Do not let him kneel,” and identifies the missing record beneath Mestria |
| M06 | Sublevel IX | Selene | Ch. 14, 16, 18 | Mestria infiltration/social stealth, archival investigation, defensive escape | The unabridged record is recovered; Selene rejects Voss's order and chooses external proof |
| M07 | The Truth Box | Tarrik | Ch. 13, 15, 17, 19 | Cauldron insertion, disaster-zone combat, truth recorder, Gate Nine, Witness chamber | Tarrik learns Cauldron's recent atrocity, Lyessa confesses, trust breaks, fourth anchor releases, Glass Citadel route appears |
| M08 | The Immutable Record | Selene | Ch. 20, portions of 22 | Neutral-border travel, Glass Citadel trials, archive defense, authentication | The altered record and Aurelion prison are independently authenticated; Selene carries proof home |
| M09 | Put My Name Beside It | Tarrik | Ch. 21 and Cauldron/Rathis material | Urban crisis, command-under-pressure encounters, rescue-versus-pursuit prioritization | Tarrik publicly owns consequence, prevents another massacre, receives limited authority to reach Glass Citadel; Dominion learns the Aurelion route |
| M10 | Eleven Minutes | Selene | Ch. 22 | Public testimony, security pressure, protected transmission, escape | The King's words enter independent records and cannot be cleanly erased |
| I11 | Last Throne, Last Wound | Interstitial | Ch. 23 | Parallel fleet preparation, optional conversations, loadout lock-in | Four anchors are release-ready; both civilizations move toward Aurelion |
| M12 | Fire and Frost | Alternating, then Tarrik/Selene | Ch. 24 | Fleet-boarding spectacle, opposed fronts, terminal entry, first forced cooperation | Both protagonists enter the terminal and are identified as contrary witnesses |
| M13 | Contrary Witness | Tarrik and Selene in authored handoffs | Ch. 25–26 | Dual-protagonist combat puzzles, Eclipse encounter, convergence mechanics, retreat | They preserve containment by concurrence and voluntarily remain together long enough to understand the warning |
| M14 | The Heir / Regicide | Tarrik | Ch. 27–28 | Crownwatch hub under pressure, investigation, duel/command confrontation | Tarrik refuses Caelus, kills him, survives gravely wounded; Lyessa becomes Regent and continues Final Dawn |
| M15 | Payment / Silencing | Selene | Ch. 29–30 | Mestria return, procedural confinement, evidence route, Lyric trial crisis | Reformation accepts the record but removes Selene; Lyric dies; Terminal Denial continues |
| M16 | Apocalypse | Selene, then cinematic convergence | Ch. 31 | exile transport crisis, systems race, limited intervention under collapse | Final Dawn and Terminal Denial recursively escalate; Aurelion containment is lost |
| E17 | The Door Is Open | Cinematic/explorable epilogue | Epilogue | Molahs discovery and sequel hook | Ancient chamber opens and the white-armored man is revealed |

## 3.4 Mission ownership and handoff

Most missions have one playable lead. Alternation is authored, never selected from a character menu. The active protagonist may change during M12 and M13 only at controlled transitions: cinematics, separated objectives, terminal mechanisms, or combat handoffs with explicit framing.

The player may never switch protagonists at will during the campaign. Free switching would make mission composition, story causality, traversal locks, companion behavior, and combat tuning unreliable, and it would weaken the authored experience of separation and convergence.

## 3.5 Mission grammar

Every mission brief must specify:

1. **Canon gate:** the fixed event the mission must deliver.
2. **Player question:** the uncertainty the player actively explores.
3. **Lead fantasy:** why this mission belongs to this protagonist.
4. **Start state:** location, companions, equipment, authority, known evidence.
5. **Beats:** traversal, investigation, combat, quiet scene, escalation, climax, aftermath.
6. **Encounter ecology:** factions, archetypes, arena density, civilian/allied presence.
7. **Choice envelope:** permitted choices and forbidden canon changes.
8. **Optional content:** evidence, rescues, alternate routes, conversations, mastery goals.
9. **State writes:** flags, relationship memories, casualties, recovered records, upgrades.
10. **Checkpoint plan:** restart boundaries and irreversible-choice boundaries.
11. **Technical risks:** streaming, crowd, destruction, bespoke mechanics, cinematics.
12. **Exit state:** the exact facts carried into the next mission.

## 3.6 Hubs and quiet spaces

Hubs are compact authored environments, not service towns. They provide conversation, evidence review, progression, foreshadowing, and visible reaction to prior events.

Baseline hubs:

- **Crownwatch / Supremacy** for Tarrik;
- **Resolver and Reformation continuity spaces** for Selene;
- **Central Information Integrity / Mestria civic spaces** in controlled states;
- **temporary safe rooms at Cauldron and Glass Citadel**;
- **Aurelion staging and terminal spaces** during convergence.

Hub rules:

- no vendor loops, crafting benches, or cluttered quest kiosks;
- progression and loadout changes occur at one readable station or menu;
- companion conversations are authored and finite;
- hub state changes visibly after major missions;
- optional dialogue may be missed, but canon-critical information may not;
- walking distances must serve staging and tone, not pad runtime.

## 3.7 Choice envelope by tier

| Tier | Can change | Cannot change | Typical persistence |
|---|---|---|---|
| Tactical | route, target order, stealth versus assault, resource use | mission outcome | current encounter or mission |
| Human | who is protected first, whether evidence is exposed, tone of confrontation, who is trusted with a task | protagonist identity or fixed deaths | later scenes, assistance, casualties, dialogue |
| Institutional | public/private framing, who receives a copy, whether a lawful process is challenged locally | the manuscript's major political sequence | act-level reactions and epilogue details |
| Canon gate | presentation and player effort around the event | whether the event occurs | permanent and fixed |

No hidden morality score converts these choices into “good” or “evil.” The game records what happened and who remembers it.

---

# 4. Input, Camera, Movement, and Traversal

## 4.1 Shared control grammar

Controller is the lead design device; keyboard and mouse must have complete parity and rebinding.

| Action | Controller baseline | Keyboard/mouse baseline | Notes |
|---|---|---|---|
| Move / look | Left / right stick | WASD / mouse | Independent camera except authored constraints |
| Sprint | Left-stick press | Shift | Toggle/hold option; no out-of-combat stamina drain |
| Evade / combat step | Face button | Space | Character-specific distance and invulnerability |
| Light melee | Right bumper | Left mouse | Selene's rapid precision / Tarrik's committed strike |
| Heavy melee | Right trigger | Mouse 4 or configurable | Hold for charge where valid |
| Guard / tactical stance | Left bumper | Right mouse or configurable | Tarrik guards; Selene enters deflection/read stance |
| Aim | Left trigger | Right mouse | Shoulder aim; may conflict with stance only by context |
| Fire / ranged tool | Right trigger | Left mouse | When aiming or ranged-ready |
| Ability modifier | Face/shoulder modifier | Q | Combines with four mapped actions |
| Interact | Face button | E | Context prioritization required |
| Lock-on / threat focus | Right-stick press | Middle mouse | Soft lock preferred for groups |
| Companion command | D-pad / radial hold | C / radial | Context-sensitive; not squad micromanagement |
| Evidence / viewmaker | D-pad | Tab | Selene-focused but shared investigation layer |

Exact bindings are **PROTOTYPE** and must pass accessibility review. Core combat cannot require simultaneous holds that are physically difficult on a standard controller.

## 4.2 Input architecture

Use Unreal Enhanced Input with mapping contexts:

- `IMC_Global`
- `IMC_Exploration`
- `IMC_Combat_Tarrik`
- `IMC_Combat_Selene`
- `IMC_Aim`
- `IMC_Dialogue`
- `IMC_CinematicLimited`
- `IMC_UI`
- `IMC_AccessibilityOverrides`

Mapping contexts are layered by game state. Gameplay abilities consume semantic input tags rather than device keys. Buffered actions carry timestamps and may be accepted only during explicit cancel windows.

Required input features:

- full remapping for controller and keyboard/mouse;
- separate aim and camera sensitivity;
- X/Y inversion;
- dead-zone and acceleration controls;
- hold/toggle options for aim, sprint, guard, and ability modifier;
- input buffering visualization in debug builds;
- device hot-swap without losing prompts;
- prompt glyphs driven by the current hardware family;
- no gameplay logic keyed directly to a physical button.

## 4.3 Camera goals

The camera must communicate mass, threat, and authorship while remaining playable in close melee and dense encounters. It is not a purely cinematic camera and must never hide a lethal untelegraphed threat for composition.

Camera modes:

- exploration follow;
- combat free camera;
- soft target focus;
- hard lock for duels and small elites;
- over-the-shoulder aim;
- execution/finisher;
- squeeze-space and contextual traversal;
- dialogue/cinematic handoff;
- corruption disturbance;
- accessibility-reduced-motion override.

`USovCameraControlComponent` owns mode arbitration, spring-arm behavior, collision, shoulder swap, aim transition, target framing, FOV, camera lag, and effect requests. Missions may request constrained composition but cannot directly override collision or accessibility settings.

## 4.4 Character camera profiles

| Property | Tarrik baseline | Selene baseline |
|---|---|---|
| Exploration distance | 390 cm | 350 cm |
| Combat distance | 430 cm | 380 cm |
| Horizontal FOV | 78° | 82° |
| Shoulder offset | broader, slightly lower | tighter, higher precision bias |
| Combat lag | heavier, damped | quicker response |
| Aim FOV | 58° | 52° |
| Target framing | preserves frontal pressure and guard plane | preserves marked targets and escape vectors |
| Hit emphasis | short impulse, mass-oriented | sharp impulse, precision-oriented |

Values are **PROTOTYPE**. Playtests must validate motion sickness, target visibility, wall collision, large-enemy framing, and multi-directional threat awareness.

## 4.5 Camera safety rules

- Camera collision uses predictive sweeps and immediate recovery from penetrations.
- The camera never crosses the character's forward attack plane during player control.
- Finishers may change lens and position but must return control within 0.25 seconds of the final actionable frame.
- Off-screen lethal attacks require a directional warning and longer anticipation than on-screen equivalents.
- Large bosses use encounter-specific framing volumes, not hard-coded global behavior.
- Corruption effects may distort edges, color, focus, or audio-camera relation; they may not create involuntary sustained rotation.
- Reduced camera shake at 0% must remove nonessential shake without removing gameplay hit confirmation.

## 4.6 Movement model

Movement is responsive enough for action combat but retains visible inertia. Root motion is used for authored attacks, finishers, mantles, and contextual traversal; ordinary locomotion remains movement-component driven.

| Parameter | Tarrik | Selene | Status |
|---|---:|---:|---|
| Walk speed | 210 cm/s | 225 cm/s | **PROTOTYPE** |
| Run speed | 540 cm/s | 600 cm/s | **PROTOTYPE** |
| Sprint speed | 730 cm/s | 820 cm/s | **PROTOTYPE** |
| Combat acceleration | deliberate | fast | Intent **LOCKED** |
| Turn rate under heavy attack | limited | moderate | Intent **LOCKED** |
| Evade distance | short armored step/roll | long directional evade | Intent **LOCKED** |
| Combat sprint stamina | yes | yes | **BASELINE** |
| Out-of-combat sprint stamina | no | no | **BASELINE** |

Supported actions:

- walk, run, sprint;
- combat strafe;
- short contextual jump or gap-crossing where authored;
- mantle and vault;
- low crouch for Selene-authored infiltration spaces;
- ladder, ledge, squeeze, and drop transitions;
- knockback recovery and contextual stumble;
- protagonist-specific evade;
- authored zero/low-gravity sequences only if mission-scoped.

This is not a platformer. There is no universal free-climb system, double jump, grappling hook, or traversal skill tree.

## 4.7 Defensive movement

Tarrik's evade is a committed armored displacement. It covers less distance, has fewer invulnerable frames, and retains facing toward a threat. His main defense is guard, perfect guard, poise, and decisive counterattack.

Selene's evade covers more distance and may pass through narrow threat gaps. Her deflection window is tighter and cannot sustain heavy pressure. Her main defense is anticipation, disruption, spacing, and precision.

Prototype timing:

| Rule | Tarrik | Selene |
|---|---:|---:|
| Evade invulnerability | 0.18 s | 0.27 s |
| Perfect defense window | 0.16 s | 0.11 s |
| Guard stamina cost | low/continuous by impact | high against heavy attacks | 
| Red/unblockable response | evade, interrupt, or ability | evade, disrupt, or precision counter |

Difficulty settings may widen timing windows but may not convert unblockable attacks into ordinary guardable attacks.

## 4.8 Traversal authoring

Traversal exists to control pacing, reveal scale, stage conversation, and express character method.

- Tarrik opens heavy routes, stabilizes structures, commands access, and withstands hazardous crossings.
- Selene finds service paths, reconstructs systems through her viewmaker, bypasses sensors, and moves through tighter spaces.
- Shared locations should occasionally present different routes, reinforcing that the protagonists read the world differently.
- Route differences may reconverge at canon gates; they do not require duplicate whole levels.
- Traversal affordances use consistent shape, light, animation pose, and optional accessibility highlights.

---
# 5. Playable Protagonists

## 5.1 Shared character contract

Both protagonists support:

- seamless melee/ranged transitions;
- light, heavy, charged, defensive, evade, finisher, and four active-ability inputs;
- shields, health, stamina, poise, Echo, status effects, and mission-scoped corruption exposure;
- soft targeting, hard lock, weak-point aiming, contextual takedowns, and environmental interactions;
- three focused progression branches with free respec at safe points;
- companion commands and authored co-action prompts;
- the same accessibility, checkpoint, difficulty, and control-remapping features.

They do not share exact animation timings, ability effects, resource triggers, defense rules, weapon profiles, progression nodes, or optimal encounter roles.

## 5.2 Tarrik Walcur

### 5.2.1 Player fantasy

Tarrik is an armored commander who turns pressure into resolve. He is strongest when he reads an attack, commits to a line, protects someone, breaks the structure in front of him, and advances through the opening. He should feel heavy without feeling sluggish.

### 5.2.2 Signature equipment

| Slot | Equipment | Function |
|---|---|---|
| Melee | **Velkorran** | High-commitment blade; broad arcs, guard counters, armor and poise pressure |
| Ranged | **Cinderline rifle** | Controlled military rifle; short-to-mid-range suppression, precision bursts, priority-target pressure |
| Armor | Dominion command armor | High shield and poise profile, visible battle damage, command-system integration |
| Tactical | Command emitter | Companion orders, threat challenge, protection and rally abilities |

Equipment identity is fixed. Upgrades alter function, response, or authored appearance details; they do not replace Velkorran with random superior loot.

### 5.2.3 Combat cadence

- light chains have 3–4 committed links with optional directional variants;
- heavy attacks cleave formations and damage poise;
- holding guard establishes a frontal defense plane;
- perfect guard creates a fast counter window and generates Echo;
- armored steps preserve proximity rather than escaping the entire formation;
- Cinderline bursts interrupt exposed specialists and maintain pressure during separation;
- attack recovery is longer than Selene's but may be shortened by successful impact, perfect defense, or specific progression nodes;
- failed commitment should be punishable.

### 5.2.4 Core abilities

Names are working names; mechanical roles are **BASELINE**.

| Ability ID | Working name | Cost/cooldown baseline | Role | Rules |
|---|---|---|---|---|
| `GA_Tarrik_Breach` | Linebreaker | 25 Echo | forward armored breach | Interrupts light/medium actions, damages poise, ends in selectable slash or guard; cannot ignore boss red attacks |
| `GA_Tarrik_Stand` | Holdfast | 35 Echo, duration-limited | protection and ground control | Reinforces frontal guard, reduces ally damage in a rear cone, converts absorbed pressure into a release strike |
| `GA_Tarrik_Rally` | Command Pulse | 30 Echo, cooldown | ally tempo and threat control | Clears minor stagger on allies, calls focus fire, challenges eligible enemies; effect varies with companion context |
| `GA_Tarrik_VelkorranSurge` | Decisive Edge | 100 Echo | signature release | Short authored sequence of player-steered heavy cuts; high poise break, no automatic boss kill, camera remains readable |

Tarrik begins with Breach and unlocks the rest through story/tutorial gates. The player has all four available by the final third unless an accessibility or narrative mode changes the schedule.

### 5.2.5 Progression branches

| Branch | Intent | Example node effects |
|---|---|---|
| **Blade** | Commitment, counters, poise break | new heavy follow-up; counter cleave; Cinderline-to-Velkorran cancel; bonus poise damage after a perfect guard |
| **Bulwark** | Guard, protection, shield economy | wider protection cone; lower guard stamina cost; shield recovery on ally intercept; Holdfast release retains more absorbed force |
| **Commander** | Threat control, ally coordination, tactical range | extra command charge; faster marked-target swap; ally focus fire builds Echo; rally cleanses one additional status tier |

The tree must not offer a “light agile Tarrik” branch that removes his commitment. Build diversity should change what he commits to and how he converts pressure, not erase his body or history.

### 5.2.6 Tarrik acceptance criteria

- New players can survive ordinary pressure by guarding but improve substantially through perfect guards and target prioritization.
- Skilled players can maintain forward momentum without becoming invulnerable.
- His blade has stronger perceived mass than Selene's at identical camera-shake settings.
- His ranged tool remains useful but cannot replace melee engagement across normal encounter distances.
- Protecting an ally is mechanically rewarding rather than a pure escort penalty.
- At least 70% of test players describe him using one of: heavy, steadfast, commanding, committed, protective.

## 5.3 Selene Veyne

### 5.3.1 Player fantasy

Selene is a precise investigator and combatant who reads the system beneath the confrontation. She creates asymmetry: identify the real target, break the connection, cross the danger before it closes, and leave an enemy formation unable to coordinate. She should feel fast and exact, not weightless.

### 5.3.2 Signature equipment

| Slot | Equipment | Function |
|---|---|---|
| Melee | **Verity** | Circular, double-sided Echo blade; rapid angle changes, deflections, cuts, and targeted finishers |
| Ranged | Precision rifle configuration | Deliberate shots, weak-point breaks, sensor and device targeting |
| Armor | Reformation Keeper armor | Lower poise, strong information layer, adaptive protection, navy visual identity |
| Tactical | Viewmaker and cloak/disruption systems | Threat analysis, evidence reconstruction, sensor evasion, remote interference |

Verity's silhouette, center grip, deployment, and two-sided danger require bespoke animation and collision. It may not be treated as a reskinned conventional sword.

### 5.3.3 Combat cadence

- light chains contain short, angle-changing sequences and rapid exits;
- heavy attacks are targeted commitments, not broad strength swings;
- tactical stance reads attack vectors and enables a tight deflection window;
- successful deflection repositions or exposes rather than simply tanking force;
- evade distance is longer, recovery is shorter, and collision prediction must protect against passing inside enemies;
- precision shots disable equipment, expose weak points, and interrupt coordination;
- marked targets feed both ranged and melee decision-making;
- sustained frontal pressure should force movement or ability use.

### 5.3.4 Core abilities

| Ability ID | Working name | Cost/cooldown baseline | Role | Rules |
|---|---|---|---|---|
| `GA_Selene_VectorStep` | Vector Step | 20 Echo | precision reposition | Directional displacement through a selected threat line; next hit gains angle/weak-point benefit; not a universal teleport |
| `GA_Selene_Disrupt` | Sever | 30 Echo | system and formation disruption | Breaks links, disables eligible devices, interrupts specialist abilities, applies only limited stagger to brute enemies |
| `GA_Selene_Veil` | Veil | 35 Echo, duration-limited | sensor denial and setup | Reduces detection, breaks eligible ranged tracking, strengthens next precision action; combat use is brief and cannot trivialize bosses |
| `GA_Selene_VeritySequence` | Proven Vector | 100 Echo | signature release | Player selects up to several marked points; Selene executes a readable route, ending where chosen if navigation remains valid |

### 5.3.5 Progression branches

| Branch | Intent | Example node effects |
|---|---|---|
| **Precision** | weak points, deflections, targeted damage | additional mark; deflection shot; weak-point chain; Verity heavy attack consumes a mark for poise damage |
| **Cipher** | disruption, devices, network control | Sever chains to linked targets; disabled turret conversion; longer exposed-system window; viewmaker reveals hidden command link |
| **Veil** | mobility, detection, entry/exit control | silent Vector Step; post-evade sensor blur; Veil duration from nonlethal bypass; takedown restores limited Echo |

The tree must not turn Selene into a permanent-invisibility assassin. Investigation and disruption remain useful in open combat, and enemies retain counterplay through area denial, nonvisual senses, formation coverage, and boss rules.

### 5.3.6 Selene acceptance criteria

- Information gathered by the player changes an immediate tactical decision.
- Precision play is rewarded deterministically; success is not hidden behind random critical chance.
- A skilled player can cross and dismantle a formation quickly, but mistakes under sustained pressure are dangerous.
- Verity reads clearly at gameplay distance and never appears to pass harmlessly through valid targets.
- At least 70% of test players describe her using one of: precise, fast, analytical, disruptive, controlled.

## 5.4 Convergence play

During M12 and M13, the non-controlled protagonist is governed by bespoke companion logic and the player may trigger limited Resonance actions. Convergence is authored around contrary contribution:

- Tarrik holds, attracts, breaks, or protects while Selene exposes, routes, severs, or crosses.
- Selene creates the exact opening Tarrik can exploit; Tarrik creates the safe interval Selene needs to complete a vulnerable action.
- joint actions require distinct inputs or states from each protagonist and never resolve as a generic “team ultimate.”

Baseline Resonance interactions:

| Setup | Payoff |
|---|---|
| Tarrik perfect-guards a marked heavy | Selene may Sever the attacker's support link, extending the counter window |
| Selene exposes a command node | Tarrik may Breach through the formation toward it without ally collision |
| Tarrik protects Selene during a terminal action | accumulated guard pressure powers a joint release on completion |
| Selene routes hostile fire into a shielded lane | Tarrik reflects or absorbs it to create an advance corridor |

Convergence mechanics appear only after both base kits are learned. They must enrich, not replace, each kit.

---

# 6. Combat System

## 6.1 Combat goals

Combat must be:

- readable under battlefield density;
- forceful at both melee and ranged distance;
- deterministic enough for mastery;
- character-specific in resource generation and defense;
- capable of intimate duels and perceived mass battle;
- hostile to passive ability rotation without context;
- integrated with story stakes, allies, civilians, objectives, and environments.

## 6.2 Combat state stack

Every combatant may carry:

1. **Health** — persistent bodily integrity within the encounter.
2. **Shield** — regenerating first layer where equipment or species supports it.
3. **Stamina** — immediate exertion for defense, sprint, and selected cancels.
4. **Poise** — resistance to stagger and posture break.
5. **Echo** — player-facing momentum resource for signature actions.
6. **Status effects** — authored conditions with explicit source, duration, stacks, and cleanse rules.
7. **Threat and target state** — AI and player focus relationships.
8. **Corruption exposure** — mission-scoped pressure when Eclipse systems are present.

Health, shield, stamina, poise, and Echo live in `USovAttributeSet_Core`. Protagonist-specific or corruption attributes may live in narrow additional sets if needed; duplicate sources of truth are forbidden.

## 6.3 Health and shields

### Core rules

- Damage applies to shield before health unless tagged to bypass or partially pierce shields.
- Shield regeneration begins **3.0 seconds** after the last shield-damaging event.
- Baseline regeneration is **5% of maximum shield per second**.
- Taking health-only environmental damage does not necessarily restart shield delay; the damage definition decides.
- Health does not naturally regenerate during ordinary gameplay.
- Healing comes from authored stations, scarce field charges, companion actions, mission transitions, or difficulty assists.
- Overhealing is not supported in the campaign.
- Shield break produces a distinct audio/visual event and a short, tunable vulnerability response.
- Maximum health and shield change only through story equipment refinements and selected progression nodes, not randomized gear.

### Damage routing

`USovDamageExecution` evaluates, in order:

1. invulnerability and immunity tags;
2. source and target team rules;
3. hit zone and attack classification;
4. resistance, armor, guard, and ability modifiers;
5. shield bypass ratio;
6. shield damage and shield-break event;
7. remaining health damage;
8. poise damage and status application;
9. death/fatal-state transition;
10. combat telemetry and narrative listeners.

## 6.4 Damage model

Baseline formula:

`FinalDamage = BaseDamage × AbilityScalar × SourceModifier × HitZoneModifier × DifficultyScalar × MitigationResult`

Where:

- `MitigationResult` is bounded and never allows accidental negative damage;
- guard is evaluated as an action state, not passive armor;
- deterministic weak-point modifiers replace random critical hits;
- attacks can define separate shield, health, and poise coefficients;
- damage numbers are hidden by default but may be enabled as an accessibility/gameplay option;
- enemies never gain health solely because the player selected a higher difficulty unless encounter-specific tuning requires it. Difficulty should prioritize aggression, coordination, resource pressure, and timing before health inflation.

### Damage channels

| Channel | Primary use | Typical response |
|---|---|---|
| Kinetic | projectiles, impacts, blunt force | shield/armor mitigation, stagger |
| Edge | blades and cutting fields | health pressure after shields, limb/armor reactions |
| Thermal | Cinderline, hazards, burning machinery | lingering heat, shield delay |
| Echo | signature abilities and King-era systems | faction-specific resistance, poise and system effects |
| Disruption | EMP, network sever, sensor attack | equipment disable rather than raw health |
| Corruption | Eclipse contact or contamination | exposure, identity pressure, scripted state risk |
| Environmental | falls, vacuum, crush, fire | authored bypass and checkpoint logic |

Channels are semantic and combinable. They must not become a color-matching elemental weakness chart.

## 6.5 Stamina

Stamina governs short-term defensive and exertion decisions.

It is consumed by:

- guard impacts;
- combat sprint;
- evade and selected cancel actions;
- charged attacks if released above a configured charge tier;
- struggle or hazard interactions;
- protagonist-specific abilities only when explicitly designed.

It regenerates after a short delay and regenerates faster while not attacking, guarding, or sprinting. Stamina cannot be spent below zero. An attempted action that cannot pay its cost either fails cleanly or uses a defined exhausted variant; it never partially activates unpredictably.

Prototype baselines:

| Parameter | Tarrik | Selene |
|---|---:|---:|
| Max stamina | 120 | 100 |
| Regen delay | 0.65 s | 0.55 s |
| Regen per second | 32 | 38 |
| Evade cost | 24 | 20 |
| Standard guard impact | 8–20 | 14–28 |

An exhausted guard breaks posture. An exhausted evade cannot start. Accessibility assists may reduce costs without removing state feedback.

## 6.6 Poise and stagger

Poise represents temporary resistance to interruption. It is not a second health bar for all characters.

States:

- **Stable:** ordinary hit reactions only.
- **Pressured:** high recent poise damage; stronger reactions and warning feedback.
- **Broken:** brief stagger or finisher eligibility.
- **Recovering:** resistance to immediate re-break, preventing stun loops.

Poise regenerates quickly after the combatant avoids poise damage. Heavy attacks, guard counters, explosions, collision, weak-point breaks, and selected Echo abilities deal high poise damage. Light attacks may maintain pressure but do not indefinitely lock valid enemies.

Tarrik has high base poise and several super-armor windows during committed actions. Selene has lower base poise and fewer such windows. Enemy poise is communicated through animation, armor, stance, audio, and optional UI—not only a bar.

## 6.7 Status-effect framework

Every status definition includes:

- gameplay tag;
- source and instigator;
- duration policy;
- maximum stacks;
- periodic interval if any;
- immunity and resistance tags;
- cleanse rules;
- UI priority;
- VFX/audio presentation;
- save/checkpoint behavior;
- AI reaction and animation constraints.

Launch campaign status families:

- burning/overheated;
- shocked/disrupted;
- slowed/immobilized;
- exposed/marked;
- suppressed;
- staggered/poise-broken;
- cloaked/sensor-blurred;
- silenced/ability-locked where narratively appropriate;
- corrupted/contaminated states managed by the corruption system.

Hard crowd control must respect diminishing returns or immunity windows on players, companions, elites, and bosses.

## 6.8 Melee framework

`USovCombatComponent` and GAS abilities coordinate melee state. Animation montages provide authored motion and event notifies; damage is never inferred only from animation time.

### Attack definition

Each attack data asset defines:

- montage and section;
- input branch and buffer window;
- startup, active, recovery, and cancel windows;
- movement or root-motion profile;
- trace shapes and sockets;
- health, shield, and poise coefficients;
- guard class and unblockable rules;
- hit-stop, camera, controller, VFX, and audio requests;
- target correction limits;
- Echo gain rule;
- allowed follow-ups;
- AI/nav safety behavior;
- finisher or execution eligibility.

### Hit detection

- Use swept shapes between previous and current weapon-socket transforms.
- Maintain a per-attack hit ledger to prevent unintended repeated hits.
- High-speed traces may substep.
- Network prediction is not required for campaign launch, but code must avoid frame-rate-dependent hit results.
- Environmental contacts are filtered separately from damageable targets.
- Weapon trail VFX follow the same socket path but are not the collision source.

### Combo and cancel rules

- Input buffer baseline: 0.22 seconds.
- Normal follow-up windows occur late enough to preserve commitment.
- Defensive cancels are character- and node-specific.
- Hit confirmation may open a slightly earlier branch than whiffed recovery.
- No infinite light-attack chain.
- Heavy and ranged transition branches must be explicitly authored.
- A rejected input produces optional debug feedback but no consumer-facing error noise.

## 6.9 Ranged framework

Ranged combat supports aim, hip/context fire where authored, recoil, spread, heat or ammunition, weak points, suppression, cover response, and device targeting.

### Cinderline rifle

- reliable short-to-mid-range burst weapon;
- meaningful report and recoil without excessive camera climb;
- heat-based or magazine behavior chosen in prototype, not both as competing maintenance loops;
- effective for interruption, shield pressure, and finishing exposed targets;
- lower weak-point multiplier than Selene's precision configuration;
- can fire a controlled burst from selected melee transition nodes.

### Selene precision rifle

- slower deliberate cadence;
- high deterministic weak-point and device effectiveness;
- aim stability rewards brief commitment rather than long stationary scope time;
- mark and viewmaker information integrate with reticle;
- ammunition/charge pacing prevents solving every formation from maximum distance;
- firing breaks Veil unless a specific progression node changes the first shot.

### Projectile policy

Use physical projectiles where travel time, interception, and spectacle matter. Use validated hitscan or very-fast simulated projectiles where precision and responsiveness matter. Visual tracers may not contradict gameplay collision. Friendly-fire rules are data-driven by mission and faction state.

## 6.10 Targeting

The game supports:

- camera-relative free attack targeting;
- soft magnetism within a narrow angle and distance;
- manual hard lock for duels and selected elites;
- target cycling based on screen position and stick direction;
- weak-point targeting while aiming;
- priority mark selection for Selene;
- command target selection for Tarrik;
- accessibility aim assistance with separate melee and ranged strength.

Target correction may rotate or translate attacks only within per-attack bounds. The player must not be pulled across hazards, through enemies, or away from an intentional target. Hard lock disengages on distance, invalid nav relationship, death, occlusion timeout, or explicit player cancel.

## 6.11 Finishers and executions

Finishers are available when an enemy enters a defined vulnerable state, usually poise break or authored low-health threshold.

Rules:

- normal enemies may die from a finisher; elites and bosses use phase-specific outcomes;
- target and player receive interruption protection appropriate to the sequence;
- nearby combat is slowed or controlled only minimally; other enemies remain dangerous;
- finisher duration target is 0.8–1.8 seconds for repeatable actions;
- long bespoke executions occur sparingly and only once per major encounter state;
- the entry pose, nav position, and exit position are validated before activation;
- failed alignment falls back to a short non-cinematic strike;
- finishers may restore a small resource based on protagonist identity, but cannot become the only safe healing loop;
- gore, dismemberment, and corruption reaction follow rating and art-direction rules.

## 6.12 Threat readability

Attack classes:

| Class | Presentation | Defensive answer |
|---|---|---|
| Standard | ordinary anticipation | guard, deflect, evade, interrupt |
| Heavy | strong wind-up and weight cue | perfect defense, evade, high-priority interrupt |
| Unblockable | red-shape/audio convention plus animation | evade, disrupt, terrain, specific ability |
| Grab/control | unique silhouette and proximity cue | evade or preempt; contextual escape only if caught |
| Area denial | ground/space telegraph with faction language | reposition or disable source |
| Command | visible link, callout, or formation change | kill/disrupt commander, survive altered pattern |

Color alone may not communicate an attack class.

## 6.13 Death, failure, and revive

At zero health, the protagonist enters `State.Fatal` and normal input ends. The game then:

1. resolves any already-committed authored rescue;
2. if a mission allows one companion rescue and its conditions are met, plays a short rescue and returns the player with limited health;
3. otherwise transitions to checkpoint reload.

Baseline companion rescue conditions:

- enabled on Story and Standard difficulty;
- at most once per encounter segment;
- companion is present, alive, reachable, and not narratively prevented;
- not valid for lethal hazards, duel conclusions, or canon failure states.

There is no prolonged solo downed crawl. Checkpoint reload should begin within 3 seconds of an unrecoverable death unless the player selects an immediate-retry option sooner.

Respawn grants brief invulnerability tagged `State.Invulnerable.Respawn` and places enemies/objects in a validated snapshot state. It never respawns the player inside an active hazard.

## 6.14 Difficulty

Launch presets:

| Preset | Audience | Main adjustments |
|---|---|---|
| Story | narrative-first | wider defense windows, lower enemy damage/aggression, stronger aim assist, companion rescue, optional auto-completion for repeated traversal failure |
| Standard | intended first play | baseline behavior |
| Veteran | action mastery | faster coordination, more dangerous specialist overlap, tighter recovery pressure, fewer resources |
| Sovereign | post-completion challenge | aggressive composition rules, limited checkpoints/resources, full enemy move sets; no canon/content changes |

Players may separately tune aim assist, defense-window assistance, damage received, navigation, and puzzle hints through accessibility/custom difficulty. Achievements must not pressure disabled players to use inaccessible settings.

Difficulty cannot:

- hide story scenes or codex entries;
- add random one-hit deaths;
- make standard enemies immune to core kit functions;
- change canon;
- turn bosses into health-only endurance tests.

## 6.15 Combat telemetry

Development telemetry must capture:

- encounter start/end and retries;
- damage sources and fatal source;
- shield breaks, health recovery, and resource starvation;
- attack use, hit rate, whiff rate, cancel type, and finisher availability/use;
- perfect-defense attempts and success;
- Echo gain/spend by source;
- target priority and time-to-specialist-kill;
- companion rescue triggers;
- difficulty and accessibility modifiers;
- camera collision and lock-on failures.

Telemetry is for design validation and may be disabled or anonymized in shipping builds according to privacy requirements.

---

# 7. Echo, Resonance, and Corruption

## 7.1 Canon rule

The Echo is not a Dominion invention, resource monopoly, or generic spell system. Dominion doctrine interprets and trains it through martial language, but the underlying phenomenon exceeds every current faction's ownership.

In play, Echo represents alignment among will, action, rhythm, relation, and consequence. It is generated most strongly when the player performs actions true to the current protagonist's method.

## 7.2 Echo resource

Core range: **0–100**.

Rules:

- Echo does not passively regenerate in combat.
- It is earned through defined actions, not raw time or damage alone.
- Basic abilities cost 20–40 Echo; signature release costs 100.
- Echo is clamped at 100. There is no hidden overflow stat in the campaign.
- After 6 seconds without dealing, receiving, guarding, disrupting, or otherwise participating in combat, Echo above 25 decays toward 25 at 8 points per second.
- Leaving an encounter normalizes Echo to a mission-defined reserve, normally 25.
- Checkpoint restore uses the authored checkpoint value, not a player-exploitable pre-death value.
- UI shows the resource and major gain feedback without floating a number for every event by default.

The decay floor prevents empty opening moments while discouraging hoarding a full meter across quiet exploration.

## 7.3 Tarrik generation

| Action | Prototype Echo |
|---|---:|
| Perfect guard | +12 |
| Guard counter lands | +10 |
| Break enemy poise | +15 |
| Intercept damage intended for ally/civilian | +15, per-source cooldown |
| Heavy attack hits 3+ valid targets | +8 |
| Execute command target within window | +8 |
| Take unguarded health damage | 0 |

Tarrik does not gain Echo simply for being hit. The resource rewards holding intentionally, not passive damage absorption.

## 7.4 Selene generation

| Action | Prototype Echo |
|---|---:|
| Hit an unbroken weak point | +8 |
| Perfect deflection | +10 |
| Sever an active command/system link | +12 |
| Defeat target during mark/exposure window | +6 |
| Complete a precision chain across distinct targets | +4 per link after first, capped |
| Bypass or disable a live threat without detection | +15, encounter-limited |
| Ordinary body shot | 0–1 |

Selene earns Echo by proving the read, not by firing rapidly into health.

## 7.5 Resonant state

At 75+ Echo, the protagonist enters a readable **Resonant** state:

- subtle audio harmonic and equipment response;
- clearer ability readiness;
- no automatic damage multiplier;
- selected progression nodes may modify a move while Resonant;
- corruption pressure may imitate or distort the signal in authored missions, but false feedback must remain learnable.

At 100 Echo, the signature release becomes available. The player chooses when to spend it. It does not auto-trigger and does not grant blanket invulnerability.

## 7.6 Resonance between protagonists

Joint Resonance is a relationship condition and encounter mechanic, not a permanent combined meter. It becomes available only during authored convergence sequences and requires complementary states. Technical implementation uses short-lived gameplay tags and interaction offers rather than directly merging both Ability System Components.

Example handshake:

1. active protagonist produces `Event.Resonance.Setup.[Type]`;
2. companion StateTree evaluates position, state, cooldown, and story permission;
3. valid partner offers `State.Resonance.Available.[Type]`;
4. player confirms or continues normal play;
5. both characters commit abilities with a shared interaction ID;
6. result writes combat events and any narrative observation;
7. fail-safe exits both characters if navigation or target validity changes.

## 7.7 Eclipse corruption

Corruption expresses coerced sameness, invasive coordination, identity erosion, and the cost of contact with Eclipse systems. It must never read as an ordinary poison bar with purple VFX.

Corruption appears through:

- environmental exposure and proximity;
- specific enemy attacks or links;
- contaminated allies and story states;
- interaction with compromised King-era machinery;
- scripted pressure during Aurelion and later events.

### Exposure bands

| Band | Gameplay effect | Presentation |
|---|---|---|
| Trace | information only; no control change | faint harmonic disagreement, edge artifacts |
| Intrusion | altered target cues, ability pressure, minor status vulnerability | doubled audio intent, misaligned afterimage |
| Contest | escalating combat constraint; player must break link, reach anchor, or complete authored action | UI elements attempt synchronized movement, companion voices overlap |
| Overwrite risk | mission-specific failure clock or boss phase | strong but accessibility-bounded distortion; never silent loss of agency |

The player must always know the mechanical remedy: break a link, leave a field, destroy a node, protect a distinct signal, use an authored countermeasure, or complete the relevant objective.

### Agency protections

- No randomized reversal of movement controls.
- No long involuntary friendly fire.
- No fake pause menu, save corruption, or deceptive accessibility UI.
- Hallucinated targets may appear only when clearly tied to a mechanic and never require color alone to distinguish.
- Input delay effects are prohibited.
- Story corruption may constrain available choices, but it may not select a dialogue choice on the player's behalf without explicit narrative staging.

## 7.8 Corruption architecture

`USovCorruptionComponent` owns exposure accumulation, source handles, band transitions, presentation requests, mission permissions, and save data. Combat effects are applied through GAS. Narrative state is written separately through consequence records so cleansing a gameplay effect does not erase a story fact.

Every corruption source must define:

- exposure rate or instant amount;
- falloff and occlusion;
- allowed target types;
- gameplay band cap;
- cleanse/escape condition;
- UI/audio/VFX profile;
- accessibility substitute;
- behavior at checkpoint and load;
- whether the exposure is canon-persistent.

---

# 8. Enemies, Factions, AI, and Encounters

## 8.1 Faction combat languages

### Dominion: power made visible

Dominion units use formation, command hierarchy, durable frontal presence, suppression, and decisive pushes.

Gameplay signals:

- officers visibly issue orders and change nearby behavior;
- shields overlap into lanes rather than acting as isolated health layers;
- heavy units claim space and make retreat costly;
- loss of command may create anger, disorder, or independent heroics depending on unit type;
- Dominion encounters challenge Tarrik's relation to familiar doctrine and Selene's ability to split a coherent front.

### Reformation: order distributed through systems

Reformation units use sensor coverage, crossfire, drones, adaptive route control, redundant command, and procedural containment.

Gameplay signals:

- information nodes and shared marks improve accuracy or reaction;
- disabling one leader is less decisive than severing the network;
- barriers and denial fields reshape routes;
- units rotate roles according to system state;
- Reformation encounters challenge Selene's knowledge of the system and Tarrik's ability to impose a line before the system adapts.

### Eclipse: harmony by erasure

Eclipse forces use synchronization, shared intent, conversion pressure, deceptive unity, and scale.

Gameplay signals:

- groups may share anticipation or damage response through visible links;
- a coordinator can cause multiple bodies to act as one pattern;
- broken individuals may briefly become distinct, frightened, or unpredictable;
- corruption zones reward separation of nodes and protection of autonomous allies;
- mass is represented through layered simulation, not dozens of identical full AI thinkers.

### Remnant and neutral threats

Remnant threats are rare, local, and historically specific. They may use obsolete King-era machinery, environmental guardians, custodial tests, scavenged technology, or independent doctrine. They exist to complicate assumptions, not provide a fourth army of interchangeable fodder.

## 8.2 Enemy role taxonomy

| Role | Purpose | Required counterplay |
|---|---|---|
| Line | maintains basic pressure and formation | spacing, light/heavy fundamentals |
| Shield | protects lane or priority unit | flank, poise break, disruption, guard counter |
| Marksman | punishes exposed stationary play | cover, interrupt, mark, route change |
| Controller | creates mines, fields, tethers, or route denial | identify source, disable, move |
| Commander | modifies allied decisions and formations | prioritize, sever, isolate |
| Duelist | tests timing and defense | read pattern, counter, avoid greed |
| Brute | high mass and poise; controls close space | weak point, heavy commitment, hazard use |
| Swarm | creates density and direction pressure | cleave, area control, movement discipline |
| Corruptor | applies identity/link pressure | break connection, protect anchor, cleanse |
| Objective unit | interacts with mission state | stop, escort, capture, or allow according to objective |

Every faction implements a subset with its own logic. A Commander is not the same behavior with a different mesh.

## 8.3 AI architecture

Use Unreal StateTree for high-level states and tasks, AI Perception for sensed stimuli, Environment Query System for tactical queries where cost-effective, and Mass Entity or lightweight agents for background battle populations.

### Full AI stack

- `ASovAIController`
- `USovFactionComponent`
- `USovThreatComponent`
- `USovCombatRoleComponent`
- `USovAbilitySystemComponent`
- StateTree asset by archetype/faction family
- perception configuration
- smart-object and cover interfaces
- animation/warping integration
- encounter-director registration

High-level states:

- unaware/ambient;
- suspicious/investigating;
- acquiring;
- engaging;
- repositioning;
- executing role task;
- reacting/recovering;
- retreating/surrendering where allowed;
- corrupted/linked;
- scripted mission state;
- dead/despawn eligible.

## 8.4 AI decision budget

Perceived battle scale uses tiers:

| Tier | Use | Behavior budget |
|---|---|---|
| A — Combatant | current arena threats | full perception, StateTree, navigation, abilities, hit reactions |
| B — Supporting | near-field battle not currently engaged | reduced decision rate, simplified targeting and cover |
| C — Mass actor | distant ranks, corridors, fleet/deck activity | Mass processing, authored lanes, limited collision |
| D — Presentation | distant battle illusion | animation, Niagara, audio, destruction, no combat logic |

Baseline maximum on console performance mode:

- 12–16 Tier A ordinary combatants, fewer with expensive elites;
- 12–24 Tier B supporting actors;
- 40–100 Tier C visible agents depending on environment;
- Tier D according to GPU/CPU effects budget.

The encounter director promotes and demotes eligible agents at controlled boundaries. A promoted actor must preserve identity, health state where relevant, position, faction, animation pose, and mission ownership.

## 8.5 Threat and coordination

`USovThreatComponent` tracks stimulus, damage, objective relevance, challenge/command effects, current attacker slots, and memory. Threat is not a simple MMO aggro number.

Coordination rules:

- melee attacker slots limit unreadable simultaneous strikes;
- ranged units maintain pressure but respect off-screen lethal-attack rules;
- commanders may temporarily expand or reorganize slots;
- Tarrik's challenge alters eligible intent, not mind-controls story-critical enemies;
- cloak breaks direct target confidence but enemies may suppress the last known area;
- civilians and objective actors can create protection priorities without all enemies irrationally ignoring the protagonist.

## 8.6 Perception

Supported stimuli:

- sight with lighting/stance modifiers only where authored and legible;
- hearing from weapons, impacts, movement, alarms, and scripted events;
- damage and ally alerts;
- network/sensor information for Reformation;
- Echo/corruption sense for selected units;
- command broadcasts;
- viewmaker spoof or disruption.

Perception debugging must display source, strength, confidence, last-known position, faction sharing, and expiry.

## 8.7 Encounter director

`ASovEncounterDirector` owns:

- encounter activation and boundaries;
- spawn/promotion waves;
- AI role quotas and attacker slots;
- objective phase;
- music and intensity requests;
- checkpoint eligibility;
- reinforcements and cut conditions;
- civilian/allied state;
- completion and cleanup;
- telemetry identifiers.

It does not decide individual attack timing. It controls composition and escalation.

Encounter data must define a recoverable low-pressure state if the player is nearly out of health/resources, except where the encounter's declared challenge intentionally removes that safety.

## 8.8 Encounter patterns

Campaign encounters should rotate among:

- frontal formation break;
- mobile crossfire;
- command-node hunt;
- defend a vulnerable action;
- escort without tethering the player to slow walking;
- pursuit or escape;
- civilian protection under competing objectives;
- duel with battlefield interference;
- corruption-link separation;
- layered battle in which near and far actors exchange roles;
- low-combat infiltration with failure escalation;
- environmental collapse or hazard migration.

No pattern should repeat with only a faction skin and higher health.

## 8.9 Boss design

Bosses are authored tests of character method and story relation.

Every boss specification includes:

- narrative question and relationship;
- protagonist ownership;
- arena topology and camera plan;
- phase goals and transition rules;
- attack vocabulary introduced before combination;
- poise and vulnerability model;
- ranged/melee balance;
- status and ability immunities with diegetic explanation;
- companion role;
- checkpoint and retry behavior;
- accessibility accommodations;
- cinematic entry/exit and canon gate;
- exploit tests.

Boss rules:

- no unexplained immunity to signature systems;
- no phase whose only change is more damage and health;
- no mandatory parry-only win without an alternate assisted answer;
- attacks remain readable at reduced VFX and color-vision settings;
- story dialogue during combat may repeat or delay but cannot be lost beneath audio chaos;
- final interaction cannot fail because of ambiguous prompt timing.

## 8.10 Allies and civilians

Allied soldiers follow simplified faction tactics, respect player lanes, and avoid stealing required finishers or boss transitions. Civilians use panic destinations, shelter objects, follow volumes, and authored rescue states rather than full life simulation.

The player is never punished for an allied AI pathfinding failure. Civilian-death consequences apply only when:

- the risk was communicated;
- the player had a meaningful response window;
- protection behavior was reliable;
- the casualty state can be reproduced from checkpoint.

## 8.11 AI acceptance criteria

- Players can identify faction and combat role from behavior before reading a nameplate.
- At least 90% of lethal attacks in review footage are visible or carry compliant off-screen warning.
- Full AI count stays within the performance budget in worst-case arenas.
- Selene's disruption changes Reformation/Eclipse coordination in an observable way.
- Tarrik's pressure tools change Dominion-style formation space without turning every enemy into a taunted dummy.
- Companions reach required co-action positions within the allotted window in 99.5% of automated traversal tests.

---

# 9. Narrative Systems and Player Agency

## 9.1 Narrative-system goals

The narrative layer must:

- preserve canon while letting the player inhabit judgment;
- support voiced authored protagonists rather than blank-slate dialogue;
- remember concrete actions and confidences without reducing them to alignment points;
- make evidence and incomplete records playable;
- integrate conversations with exploration, hubs, combat aftermath, and mission objectives;
- allow the narrative team to audit all dependencies and unreachable states;
- survive checkpoint reload, save migration, localization, and cinematic revision.

## 9.2 No morality grid

The December 2025 morality system and “King's Measure” are removed. The game does not calculate good/evil, order/chaos, Dominion/Reformation, or any other two-axis identity score.

The setting's central question is not whether a player has accumulated enough correct moral points. It concerns responsibility, authority, distinction, witness, and what people do when every available system can justify itself. A morality meter would flatten those conflicts and encourage players to optimize for an ending rather than attend to the people involved.

## 9.3 Consequence records

The game stores discrete facts in `FSovConsequenceRecord`:

| Field | Purpose |
|---|---|
| `ConsequenceId` | stable namespaced identifier |
| `MissionId` | originating mission |
| `BeatId` | exact narrative beat |
| `InstigatorId` | Tarrik, Selene, companion, or system |
| `SubjectIds` | people, factions, locations, records affected |
| `ChoiceTag` | semantic action, not moral value |
| `OutcomeTag` | resolved local result |
| `WitnessIds` | characters or systems that know it |
| `Publicity` | private, shared, institutional, public |
| `Timestamp` | campaign order and play time |
| `Payload` | bounded typed values such as casualty count or evidence copy target |
| `CanonClass` | optional, variable, or fixed presentation |
| `Persistence` | encounter, mission, act, campaign, sequel export candidate |

Examples:

- `Consequence.M01.Tarrik.WithheldMarketShot`
- `Consequence.M04.Evidence.SharedWithLyessaOnly`
- `Consequence.M06.Tharne.CarriedSecondWafer`
- `Consequence.M09.Rathis.MedicalDistrictPrioritized`
- `Consequence.M12.DominionCrew.RescuedByReformation`

A consequence record describes what occurred. Reactions derive from whether a character witnessed, learned, inferred, or was told about it.

## 9.4 Choice categories

Choice prompts should arise from one or more categories:

- **Obedience / refusal:** whether and how a specific order is challenged.
- **Truth / containment:** who receives evidence, when, and with what framing.
- **Protection / mission priority:** whose immediate risk receives attention.
- **Mercy / security:** how a defeated or compromised person is handled.
- **Trust / unilateral action:** whether responsibility is shared or retained.
- **Institution / person:** whether procedure is followed, bent, exposed, or bypassed in a local case.
- **Witness / victory:** whether the player records cost or allows action to be remembered only as success.

These are writing lenses, not meters. The game may record repeated behavior for bespoke dialogue, but it may not display or calculate a universal personality profile.

## 9.5 Authored protagonist boundaries

Each dialogue choice represents a plausible expression of the protagonist at that story moment.

Tarrik options may vary among direct command, restrained challenge, protection, confession, or silence. They cannot make him casually cruel, cowardly, anti-Dominion without cause, or indifferent to command responsibility.

Selene options may vary among precise questioning, procedural challenge, guarded trust, disclosure, or silence. They cannot make her careless with evidence, theatrically chaotic, unquestioningly loyal after a revealed contradiction, or dismissive of distributed responsibility.

When no plausible choice exists, the scene is authored without a prompt. A mandatory false choice damages trust more than a well-performed fixed action.

## 9.6 Dialogue interface

Dialogue modes:

- **Cinematic choice:** 2–4 paraphrased responses during staged scenes.
- **Walk-and-talk response:** brief posture choice without stopping movement.
- **Investigation question:** choose which lead or contradiction to pursue first.
- **Timed pressure:** used rarely, with timer accessibility controls and a valid silence outcome.
- **Companion check-in:** optional hub conversation topics.
- **Combat bark response:** automatic, driven by state; not player-selected.

Response text must accurately preview intent. It may compress delivery but cannot conceal a materially different action. Tone icons are avoided unless testing shows paraphrases remain ambiguous after revision.

The default cinematic choice timer is none. If a scene requires pressure, the timer begins only after text and screen-reader output are available, pauses with the game, and may be extended or disabled.

## 9.7 Dialogue graph architecture

`USovDialogueSubsystem` loads `USovDialogueGraph` primary data assets. A graph contains:

- entry requirements;
- speaker and listener roles;
- localized line IDs;
- performance/cinematic references;
- optional player choices;
- condition queries;
- consequence writes;
- relationship-memory writes;
- mission events;
- interruption and resume behavior;
- fallback nodes;
- exit reasons;
- analytics ID.

Conditions query immutable interfaces rather than arbitrary Blueprint casts:

- mission/beat state;
- consequence existence and payload;
- evidence ownership and witness knowledge;
- companion presence and availability;
- protagonist progression only when the line concerns demonstrated technique;
- difficulty/accessibility only for presentation, never story content;
- prior conversation completion.

Graph validation must detect missing localization IDs, unreachable nodes, cycles without exits, choices with no legal result, writes to unknown flags, absent fallback lines, and cinematic references with incompatible participants.

## 9.8 Relationship memories

There is no numeric approval bar. Relationships use authored memories:

| Memory type | Example effect |
|---|---|
| Trust given | character volunteers information or accepts a task later |
| Trust withheld | character asks directly, acts independently, or comments on the pattern |
| Protection | changes a private scene, combat assistance, or later willingness |
| Public exposure | changes institutional risk and how a companion frames loyalty |
| Contradiction acknowledged | opens a more honest branch |
| Boundary crossed | creates distance, refusal, or explicit repair scene |

Memories are asymmetrical. Tarrik's belief that he protected Lyessa and Lyessa's experience of the same act may be stored separately. Relationships may change without becoming a reward track, romance route, or alternate canon ending.

## 9.9 Evidence and witness system

Evidence is story information with provenance, not collectible lore currency.

`USovEvidenceSubsystem` tracks:

- record ID and canonical content;
- source location and custodian;
- authentication level;
- known alterations or omissions;
- who has seen it;
- copies and their destinations;
- public/private status;
- links to claims and contradictions;
- player-accessible summary and full optional text/audio;
- mission relevance.

Evidence states:

1. **Observed** — player has found or heard it.
2. **Questioned** — inconsistency is recognized.
3. **Corroborated** — independent support exists.
4. **Authenticated** — a valid authority or provenance chain confirms it.
5. **Distributed** — copies exist beyond one institution's control.

The main story cannot stall because the player missed optional evidence. Required evidence is acquired through the critical path; optional evidence changes context, dialogue, route confidence, or local consequence.

## 9.10 Viewmaker gameplay

Selene's viewmaker is an active investigation tool with bounded functions:

- identify live systems and command links;
- reconstruct recent device use from authored traces;
- compare visible records with known evidence;
- reveal weak points and network relationships;
- tag an object for companion analysis;
- visualize a route or architectural inconsistency;
- provide accessibility navigation assistance.

It is not detective vision that paints all interactables through walls. Scans require proximity, angle, trace evidence, or a known query. The player should decide what a signal means, not simply hold a button until the answer appears.

Tarrik can inspect command records and use military interfaces, but he does not receive Selene's full reconstruction layer. His investigations emphasize testimony, orders, physical consequence, and chain of command.

## 9.11 Mission and quest state

The campaign uses authored mission state rather than a generic quest economy.

State hierarchy:

- Campaign
  - Act
    - Mission
      - Sequence
        - Beat
          - Objective

Each state has `Inactive`, `Available`, `Active`, `Succeeded`, `Failed`, `Skipped`, or `Superseded` where valid. Canon gates cannot be skipped by free travel.

`USovMissionSubsystem` owns state transitions. Level Blueprints may request a transition but may not directly set campaign flags. Every transition is validated against prerequisites and writes a journal event.

Objective types:

- reach/escape;
- eliminate/disable;
- protect/hold;
- investigate/authenticate;
- retrieve/deliver;
- choose/prioritize;
- survive;
- interact/operate;
- follow/escort with pace adaptation;
- optional mastery or rescue.

## 9.12 Objective presentation

- Display the immediate actionable goal, not a full future checklist.
- Update objectives when the protagonist learns new information.
- Avoid UI wording that reveals a betrayal, survival, or location before the character knows it.
- Allow the player to hide objective text and markers.
- Use world language, dialogue, lighting, and geometry before relying on markers.
- If an objective is mechanically unusual, state its success/failure rule before the player can fail it.
- Optional objectives must be marked optional and cannot secretly determine canon.

## 9.13 Narrative interruption and recovery

Walk-and-talk dialogue supports priority tiers:

- ambient lines may cancel permanently;
- character lines may pause and resume after combat;
- canon-critical lines defer until safe or replay automatically at the next valid point;
- choice prompts never begin while combat input is required unless explicitly designed as a pressure scene;
- subtitles retain speaker, direction, and context after interruption.

The game logs unheard critical lines for replay in the Evidence/Record interface only when doing so makes diegetic sense.

## 9.14 Narrative validation

Automated and editorial tools must verify:

- every canon gate is reachable from every legal prior choice state;
- fixed outcomes cannot be bypassed by optional route or failure recovery;
- character knowledge never precedes acquisition;
- dialogue does not reference dead/absent/missing characters incorrectly;
- save/load inside a conversation resumes or safely restarts at a declared boundary;
- localization expansion does not obscure response intent;
- subtitle timing covers complete spoken lines;
- consequence writes have downstream consumers or an explicit archival-only purpose;
- no obsolete morality or approval fields remain in shipping campaign data.

---

# 10. Progression, Equipment, and Rewards

## 10.1 Progression philosophy

Progression deepens a known person. It does not turn Tarrik or Selene into a chosen class, replace their signature weapons, or make late-game numbers invalidate early combat.

The player progresses through:

- story-granted abilities and equipment changes;
- Technique Points spent on focused protagonist-specific trees;
- optional refinements earned through mastery, investigation, or rescue;
- increased player understanding;
- new encounter relationships and Resonance actions.

There are no character levels, XP bar, gear score, rarity colors, random perks, level-scaled loot, or stat requirements for dialogue.

## 10.2 Technique Points

Technique Points are a UI term, not a lore currency. They are awarded for:

- major mission completion;
- selected optional objectives that demonstrate the protagonist's method;
- a small number of challenge encounters;
- story reflection beats where an existing technique is consciously revised.

Baseline economy:

- 18–22 points earnable per protagonist;
- 24–30 points of total nodes per protagonist;
- a normal first playthrough completes one branch and meaningfully invests in a second;
- no repeatable farming;
- optional content accelerates or broadens builds but is not required for core abilities;
- free respec at safe points, with no currency cost.

## 10.3 Node rules

Every node must:

- change an action, decision, relationship among actions, or resource rule the player can perceive;
- name the affected move and condition;
- avoid bonuses smaller than the player can feel;
- avoid generic “+2% damage” filler;
- include telemetry hooks;
- have a test case and UI preview;
- preserve protagonist identity;
- declare incompatibilities or mutually exclusive capstones clearly.

Allowed node types:

- new branch/follow-up;
- timing-window modification;
- resource conversion;
- target or area behavior;
- risk/reward trade;
- ability augment selected in loadout;
- defensive interaction;
- companion or Resonance interaction;
- signature equipment refinement.

## 10.4 Tree structure

Each protagonist has three branches. A branch contains:

- 1 entry node teaching its intent;
- 3 tier-one nodes;
- 3 tier-two nodes requiring relevant investment;
- 1–2 cross-branch nodes;
- 1 capstone;
- at most one mutually exclusive choice per branch.

The interface displays mechanical previews and a short authored reflection, but it does not invent lore claims that the character has not reached in the story.

## 10.5 Ability augments and loadout

Core abilities remain equipped once unlocked. At safe points, the player may select one augment for each active ability from unlocked options. Augments alter behavior rather than replace the input.

Example:

- Tarrik's Breach may favor wider formation opening, higher single-target poise, or ally passage.
- Selene's Sever may favor network chaining, device duration, or a precision follow-up.

The active combat HUD should never exceed four abilities plus signature release. Loadout changes are disabled during combat and at narrative points where equipment is unavailable.

## 10.6 Signature equipment refinement

Equipment refinements are authored and deterministic. Categories:

| Category | Tarrik examples | Selene examples |
|---|---|---|
| Weapon handling | Cinderline burst recovery; Velkorran guard transition | rifle stability; Verity angle recovery |
| Defensive systems | shield-delay reduction after perfect guard | brief shield recovery after successful disengage |
| Tactical interface | command range or extra order behavior | mark capacity or viewmaker query speed |
| Echo response | gain-source conversion | chain or disruption conversion |

Refinements may change visible modules, blade channels, armor damage repair, or VFX intensity where story-appropriate. The equipment silhouette remains canonical.

## 10.7 Inventory policy

The campaign has no general grid/list inventory. It tracks only:

- signature equipment state;
- selected ability augments;
- limited field-healing charges;
- mission-specific keys/tools;
- evidence and records;
- optional cosmetic presentation items if separately approved.

Mission items are removed, converted, or archived automatically when no longer relevant. The player is not asked to sell, dismantle, compare, or manage encumbrance.

## 10.8 Healing and consumables

Baseline:

- 2 field recovery charges on Standard;
- charges restore a fixed health percentage over a short interruptible animation;
- charges refill at major checkpoints or authored supply stations;
- selected difficulty/accessibility settings may increase capacity or refill frequency;
- no consumable damage buffs, crafting ingredients, or temporary stat potion economy;
- mission-specific countermeasures may occupy a contextual input without entering permanent inventory.

## 10.9 Rewards

Valid rewards include:

- Technique Point;
- ability or augment;
- equipment refinement;
- new companion conversation;
- evidence or authenticated record;
- changed assistance in a later mission;
- alternate route or staging advantage;
- visible saved lives or recovered names;
- concept art/model viewer/codex archive outside the fiction, if approved.

Invalid campaign rewards include random gear, duplicate weapons, cash, crafting ore, rarity upgrades, booster packs, and daily-login benefits.

## 10.10 Progression balance

- Enemy fundamentals do not scale to match the player's unlocked nodes.
- Later enemies add behavior and composition complexity.
- Damage growth across the full campaign remains bounded so early archetypes do not become irrelevant.
- All mandatory content can be completed with story-granted abilities and no optional points on Standard.
- A fully optimized build should improve expression and efficiency, not erase boss mechanics.
- Respec may not duplicate rewards or reset story-derived equipment state.

---

# 11. World, Level, and Interaction Design

## 11.1 World structure

The campaign is a sequence of authored wide-linear environments connected by cinematic or diegetic travel. The player cannot freely pilot between worlds or revisit every prior mission from an in-fiction galaxy map.

World categories:

- **combat mission:** directed route with tactical width and optional pockets;
- **investigation mission:** denser spatial clues, controlled confrontation, lower sustained combat;
- **hub:** compact social/evidence/progression space;
- **spectacle route:** highly authored battle, flight, collapse, or terminal sequence;
- **convergence space:** bespoke dual-protagonist traversal and encounter design;
- **epilogue space:** controlled exploration or cinematic reveal.

A non-canon replay menu may unlock completed missions from the main menu. Replay restores a mission snapshot and does not write back into the active canonical save unless explicitly using New Game Plus, which is **DEFERRED** pending scope.

## 11.2 Level-design pillars

### Read the system

Spaces reveal how power works: who is elevated, who is watched, which records are hidden, how formations move, and what infrastructure values. The player should infer faction priorities from access, sight lines, redundancies, monuments, and maintenance.

### Fight across layers

Strong arenas include:

- near/mid/far threat relationships;
- at least two viable movement paths;
- cover that supports but does not freeze combat;
- a priority target or state that changes the geometry;
- protected recovery space that enemies can pressure rather than instantly invalidate;
- vertical or lateral information the camera can display;
- an aftermath route through the changed space.

### Preserve pace

Traversal length, door timing, lifts, squeezes, and conversations are budgeted. Repeated traversal hides loading only when the audiovisual and narrative content makes it worthwhile.

### Make consequence visible

Rescued civilians remain present or are acknowledged. Damaged facilities stay damaged. A public record spreads. A command decision changes who occupies a later corridor. Consequence is strongest when the player sees it without a summary screen.

## 11.3 Environmental languages

### Dominion — glory through power

- bronze, crimson, amber engine light, monumental black stone;
- vertical command sight lines, ceremonial axes, visible weapons and infrastructure;
- architecture built to make authority legible and protection impressive;
- warmth and grandeur that can become oppressive scale;
- damaged spaces reveal labor, maintenance, and the people beneath imperial imagery.

### Reformation — order through control

- gray, navy, pale blue-white, measured modularity;
- distributed signage, redundancies, transparent public functions, concealed exceptional systems;
- accessibility and civic efficiency alongside surveillance and procedural barriers;
- architecture that avoids personal spectacle but can hide responsibility across interfaces;
- restricted layers should feel like continuations of the same rational system, not a villain dungeon.

### King-era / Alethian inheritance

- white stone, functional gold, living or responsive geometry;
- difference held in precise relation rather than flattened repetition;
- light behaves as information and recognition, not decoration;
- scale can be intimate and immense without modern faction branding;
- mechanisms test witness, concurrence, or responsibility rather than arbitrary symbol puzzles.

### Eclipse

- violet intrusion, synchronization, repeated forms that become too exact;
- boundaries and silhouettes begin to agree against their nature;
- environments seem to anticipate movement or erase individual traces;
- corruption overlays existing places rather than replacing them with a generic alien biome;
- accessibility substitutes preserve mechanics if distortion intensity is reduced.

### Remnant / neutral

- local histories and adaptation rather than one universal style;
- visible reuse, repair, custodianship, ritual, or contested interpretation;
- strong contrast between what a place was built for and what its current people need.

## 11.4 Exploration

Optional exploration should take 15–25% of a typical mission's critical-path time and reward attention rather than exhaustive wall-following.

Optional spaces may contain:

- witness records or contradictory accounts;
- civilian/crew rescue;
- tactical overlook or disabled reinforcement route;
- companion exchange;
- equipment refinement;
- mastery encounter;
- environmental storytelling with a named human consequence;
- alternate entry into a mandatory arena.

Collectibles must be limited, authored, and relevant. There are no hundreds-of-icons completion maps.

## 11.5 Navigation

Navigation hierarchy:

1. composition, light, motion, landmarks, and dialogue;
2. faction signage and diegetic route indicators;
3. optional compass/objective direction;
4. temporary path pulse or high-contrast navigation assist;
5. explicit objective marker.

Accessibility may elevate a lower item in the hierarchy. The default does not require constant minimap reading.

## 11.6 Interaction system

`USovInteractionComponent` queries nearby `ISovInteractable` actors using view angle, distance, occlusion, priority, mission state, and protagonist capability.

Interaction types:

- immediate use;
- hold-to-confirm for irreversible or high-risk actions;
- paired/cooperative interaction;
- contextual traversal;
- evidence inspect;
- conversation;
- carry/drag/rescue;
- command/authorize;
- viewmaker analyze/disrupt;
- heavy force/stabilize.

Rules:

- only one primary prompt appears at a time;
- prompt text names the action, especially if irreversible;
- combat interactions have short, validated animations;
- critical interactions use generous alignment volumes;
- failure to align never consumes a resource or writes state;
- hold durations are adjustable or replaceable with toggle;
- interactions remain usable at low frame rate and after device hot-swap.

## 11.7 Doors, lifts, and travel

Doors and lifts are stateful mission actors with explicit authority, lock reason, power state, damage state, nav link, and checkpoint behavior. They may hide streaming but must not block indefinitely waiting for an unreported load.

Travel transitions:

- validate destination chunk before committing;
- write checkpoint before an irreversible long transition;
- support cinematic skip only after destination readiness is guaranteed;
- maintain protagonist, equipment, mission, evidence, companion, and consequence state;
- recover to the origin checkpoint if load fails.

## 11.8 Checkpoints

Checkpoint goals are fast retry, stable narrative state, and no lost irreversible choice.

Automatic checkpoints occur:

- at mission start;
- before and after major combat arenas;
- before an irreversible choice, with the choice not written until resolved;
- after a canon gate and its required state writes;
- before boss phases only when phase retry is intended;
- after long traversal or cinematic transitions;
- on hub exit after progression changes.

Checkpoint snapshot includes:

- campaign/mission/sequence/beat state;
- protagonist transform and movement mode;
- health, shield, stamina, Echo reserve, consumables;
- ability unlocks, augments, and progression;
- companion identity, health/state policy, and position anchor;
- encounter director phase and actor snapshot policy;
- interactable, door, hazard, and destruction states;
- consequence and evidence writes committed before the boundary;
- difficulty/accessibility settings;
- RNG seed for any permitted presentation randomness.

Enemies are restored to authored snapshot states rather than saving every transient AI thought. Destruction uses checkpointable state groups.

## 11.9 Manual saves

Baseline save model:

- three rolling autosaves;
- one checkpoint quick-retry slot;
- up to ten manual campaign slots;
- manual save allowed in hubs and safe exploration states, not during combat, cinematics, traversal moves, or unresolved choices;
- manual load returns to the nearest compatible serialized state and clearly displays location/mission/time;
- platform account owns settings and save namespace;
- cloud-save integration follows platform requirements.

The game never claims to have saved until serialization and platform write both succeed.

## 11.10 World Partition and streaming

Use World Partition selectively for large missions and hubs; compact interiors may use level instances or conventional streaming where simpler.

Requirements:

- Data Layers separate mission phases, damage states, civilian populations, and performance variants;
- External Actor ownership follows the authoritative map integration process;
- HLOD is built and validated for every shipping large environment;
- streaming source follows the player, camera needs, scripted action, and destination prefetch;
- no gameplay-critical actor may unload while owning unresolved mission state;
- cinematics declare required cells and block cleanly before playback;
- AI outside active cells demotes or serializes through encounter policy;
- memory peaks during cell transition stay within platform budget.

## 11.11 Level acceptance criteria

- Critical path can be completed without objective markers by an informed internal tester.
- Optional route return points do not cause more than 20 seconds of empty backtracking.
- Combat camera collision remains below the agreed failure threshold in every arena.
- Major choice and canon-gate spaces have reliable checkpoint recovery.
- Faction identity is readable from untextured blockout through scale, route, and spatial organization before final art.
- Every required traversal interaction passes 100 consecutive automated starts from its entry volume for both supported frame-rate profiles.

---

# 12. Companions and Allied Characters

## 12.1 Companion philosophy

Companions are authored people with tactical value, not collectible party roles. Their presence follows canon. The player does not build a party from a roster, equip companion loot, assign skill points, or pursue approval thresholds.

Primary companion relationships include:

- Tarrik and Lyessa;
- Selene, Lyric, and Tharne;
- Tarrik and Selene during convergence;
- mission-scoped Dominion/Reformation officers, crew, or civilians.

## 12.2 Companion combat contract

Companions must:

- contribute visibly without completing the encounter alone;
- preserve their signature weapon, movement, and temperament;
- avoid blocking the player's camera, aim, retreat route, or finisher alignment;
- follow or lead according to scene intent;
- respond to a small set of context-sensitive commands;
- enter bespoke co-actions reliably;
- recover from nav failure through hidden correction only when off-camera or diegetically acceptable;
- never permanently die through ordinary simulation if canon requires survival;
- never become invulnerable-looking damage sponges without feedback.

## 12.3 Commands

Campaign commands are limited to:

- focus selected target;
- hold/defend position or person;
- move to contextual anchor;
- interact with highlighted device/objective;
- execute available co-action;
- regroup.

Commands appear only when valid. The player is not asked to manage formations, ability rotations, inventory, or companion stance menus.

## 12.4 Companion state architecture

`USovCompanionComponent` coordinates:

- leader/follow relationship;
- mission role and required anchor;
- command availability;
- combat contribution budget;
- co-action offers;
- rescue availability;
- narrative bark permissions;
- teleport/recovery policy;
- canon-protected health behavior;
- separation and reunion state.

StateTree branches include Follow, Lead, CombatRole, CommandTask, CoAction, NarrativeAnchor, Rescue, Separated, RecoverPath, and Scripted.

## 12.5 Health and defeat

Companion health is real enough to create protection pressure, but failure rules are authored.

- Ordinary companions may enter a disabled state and recover after encounter completion.
- A canon-protected companion cannot die from incidental combat.
- Disabled companions stop dealing damage and may require a safe revive interaction if the mission uses that pressure.
- If a companion's defeat would invalidate the encounter, the encounter fails clearly and reloads.
- Difficulty changes companion survivability and recovery frequency only within narrative plausibility.

## 12.6 Non-player protagonist

When Tarrik or Selene is AI-controlled in convergence:

- use a bespoke protagonist StateTree, not the generic companion asset;
- retain their core defense and signature abilities on controlled cooldowns;
- do not spend the player's progression choices invisibly; AI behavior reads the actual unlocked kit but uses a curated subset;
- reserve high-impact opportunities for player-confirmed Resonance actions;
- avoid killing command/boss targets during required player interaction windows;
- display clear intent when preparing a co-action;
- recover to authored anchor if separated by 25 meters or more and not in a valid split phase.

## 12.7 Bark system

Barks have priority, cooldown, context, speaker availability, and interruption policy.

Priorities:

1. lethal mechanic warning;
2. objective state or canon-critical direction;
3. companion disabled/rescue;
4. tactical callout;
5. authored relationship reaction;
6. ambient observation;

Two lines may not overlap unless intentionally written as overlap. Repeated tactical lines require variant pools and escalating cooldown. Subtitle direction indicators support off-screen speakers.

## 12.8 Companion acceptance criteria

- Companion contribution on Standard is 15–25% of ordinary encounter damage, adjusted by role and mission.
- A companion does not obstruct the player's movement for more than 0.4 seconds in 99% of measured contacts.
- Required co-actions initiate within 1.0 second of a valid player request in 99.5% of tests.
- The companion never crosses into a cinematic mark before the mission state permits it.
- Relationship reactions match knowledge and consequence records in every legal branch.

---

# 13. User Interface, User Experience, and Accessibility

## 13.1 UX principles

The interface should expose the next meaningful decision without turning the world into a dashboard.

Principles:

- **Character first:** Tarrik and Selene have distinct HUD expression but identical information hierarchy.
- **World before widget:** use animation, sound, equipment response, and environment before text or icon.
- **Readable under pressure:** critical signals remain legible during dense combat, VFX, and camera motion.
- **Truthful language:** prompts describe the action and consequence the protagonist understands.
- **Layered depth:** basic play requires little menu use; evidence and detailed combat data remain available.
- **Accessible equivalence:** settings change presentation or assistance without withholding content.
- **No system residue:** removed loot, class, morality, currency, vendor, and approval systems leave no empty tabs or inherited terminology.

## 13.2 HUD hierarchy

### Always available in combat

- health and shield;
- stamina when changing or below full;
- Echo meter and ability readiness;
- four ability icons and cooldown/availability state;
- reticle or melee focus indicator as context requires;
- critical status/corruption state;
- immediate objective only when updated or enabled persistently;
- companion state when a companion is tactically relevant.

### Contextual

- enemy health/poise for current focus or boss;
- marks, weak points, command links, and disruption state;
- interaction prompt;
- off-screen threat indicators;
- field-healing charges;
- objective progress;
- evidence comparison;
- Resonance offer;
- subtitles and speaker direction.

### Hidden outside need

Combat elements fade after 6 seconds without combat participation or a valid threat. Health may remain if damaged. Navigation and objective display follow player settings.

## 13.3 Protagonist HUD identity

Tarrik's interface emphasizes:

- shield plane, guard pressure, command target, ally protection cone, and poise break;
- warmer bronze/amber response and broader, weightier motion;
- Echo pulses synchronized with committed defense and impact.

Selene's interface emphasizes:

- marks, weak points, network links, sensor confidence, viewmaker query, and disruption windows;
- navy/cool-white response and fine geometric motion;
- Echo pulses synchronized with confirmed precision and severed relationships.

Both use the same placement zones so switching protagonists does not require relearning. Color is never the sole distinction.

## 13.4 HUD layout baseline

| Zone | Information |
|---|---|
| Lower left | health, shield, stamina, field recovery |
| Lower right | abilities, Echo, signature release |
| Center | reticle, target response, interaction, immediate threat information |
| Upper center | boss or command encounter state |
| Upper left | objective update and optional task |
| Screen edges | off-screen threats, damage direction, subtitle direction |
| Companion sidecar | companion health/disabled state, current command, co-action readiness |

Safe-zone support must cover 80–100% display areas. UI scales independently from subtitle size.

## 13.5 Enemy UI

Ordinary enemy health is shown only when damaged, focused, or explicitly enabled. Poise may be represented through a compact posture indicator for elites and bosses; ordinary enemies rely primarily on animation and effects.

Enemy UI may show:

- name/role where known;
- health/shield;
- current mark or command link;
- vulnerable/poise-broken state;
- interruptible ability;
- boss phase/objective.

It does not show hidden numerical resistances, randomized levels, rarity frames, or unexplained immunity icons.

## 13.6 Menus

Primary pause menu:

- Resume
- Current Mission
- Techniques
- Equipment
- Evidence & Records
- Map / Location
- Tutorials
- Photo Mode, if approved
- Settings
- Save / Load where legal
- Return to Main Menu

### Current Mission

Shows current actionable objective, completed key beats, known optional objectives, and relevant evidence. It does not expose future beats.

### Techniques

Shows the active protagonist's three branches, Technique Points, node preview, respec, and ability augments. The inactive protagonist can be reviewed only from a safe hub or main-menu archive, not modified during a mission that cannot support the change.

### Equipment

Shows signature gear, fixed loadout, selected refinements, appearance state, and field recovery. No comparison columns or item rarity.

### Evidence & Records

Organized by claims, people, places, Crownmarks/Witnesses, and provenance—not by hundreds of collectible slots. Each entry distinguishes observed fact, interpretation, contradiction, and authenticated record.

### Map

A local orientation tool, not a galaxy checklist. It shows discovered route structure, current goal, known safe point, selected optional leads, and accessibility path guidance. Undiscovered optional rooms are not outlined.

## 13.7 Tutorials

Tutorial delivery order:

1. animation and encounter affordance;
2. brief contextual prompt;
3. optional expanded explanation;
4. replayable tutorial card/video;
5. safe practice or immediate low-risk test;
6. later mastery challenge.

Tutorials must:

- teach the protagonists separately;
- explain why the same input produces a different defensive answer;
- avoid pausing during lethal motion unless necessary;
- be replayable from the menu;
- adapt prompts to current bindings;
- distinguish story term from mechanic term;
- never teach a feature several missions before use.

## 13.8 Subtitles and dialogue UX

Subtitle options:

- font scale with large and extra-large targets;
- opaque or adjustable background;
- speaker name and speaker color/pattern;
- directional indicator;
- important non-speech audio captions;
- maximum characters per line and line-count constraints validated per language;
- separate cinematic and ambient subtitle placement where needed;
- dialogue-history review for the current scene where technically safe.

Choice text uses a minimum readable duration before pressure timers can expire. Screen readers announce speaker, choice count, selected choice, and timer state.

## 13.9 Accessibility feature set

### Input and motor

- complete remapping;
- hold/toggle/tap alternatives;
- adjustable hold duration;
- simultaneous-input reduction;
- combat input buffering assistance;
- rapid-press replacement;
- one-stick camera assistance and auto-camera strength;
- aim assist, snap, friction, and projectile lead options;
- defense-window assistance;
- optional auto-sprint;
- menu navigation wrap and focus memory;
- controller vibration intensity by channel.

### Vision

- scalable UI and text;
- high-contrast HUD;
- color-vision presets plus independent team/threat colors;
- non-color attack and status signals;
- navigation contrast/pulse;
- interactable and weak-point outline options;
- reduced bloom, lens effects, chromatic aberration, depth-of-field, and film grain;
- brightness/HDR calibration;
- screen-reader support for menus, evidence summaries, and dialogue choices where platform APIs allow.

### Hearing

- complete subtitles for speech;
- closed captions for gameplay-critical sound;
- direction indicators;
- separate sliders for dialogue, music, effects, ambience, tinnitus-like tones, and controller audio;
- visual alternatives for parry, mark, corruption, shield-break, and off-screen attack cues;
- dynamic-range presets.

### Cognitive and comfort

- objective reminder and path assist;
- tutorial replay;
- evidence summaries separating fact from interpretation;
- puzzle hint cadence and direct-solution option after repeated failure;
- pause during single-player cinematics and gameplay;
- motion blur, camera shake, head bob, hit flash, and corruption distortion controls;
- FOV where platform/performance permits;
- arachnid-like or other specific phobia accommodation only if relevant content review identifies need;
- content warning and reduced body-horror presentation where feasible without changing mechanics.

### Difficulty decomposition

Players can alter incoming damage, enemy aggression, timing assistance, aim assistance, navigation, and puzzle support separately. Presets remain convenient starting points.

## 13.10 Accessibility validation

- All menus are operable without pointer precision.
- No mandatory information relies on color alone.
- Every gameplay-critical audio cue has a visual substitute and vice versa where practical.
- All required rapid presses and holds have alternatives.
- Reduced corruption effects preserve exposure band and remedy information.
- Subtitle collision is tested against HUD, ultrawide safe areas, and every supported language.
- Accessibility settings are available before the opening cinematic and are saved immediately.

## 13.11 Localization

All consumer-facing text uses stable string-table IDs. Concatenated sentence fragments are prohibited. UI allocates for 30–40% expansion where possible. Gender, plurality, and grammar use full localized variants.

Voice localization scope is a production decision; subtitle/text localization is required for all launch markets. Lip sync and cinematic timing must tolerate localized line duration through shot handles, performance edits, or approved time compression—not by cutting meaning.

---

# 14. Visual, Animation, VFX, Audio, and Cinematic Direction

## 14.1 Visual target

The target is premium cinematic realism shaped by strong faction silhouettes and readable combat. Material complexity, damage, crowds, and scale should support presence, but readability outranks visual noise.

The game needs a coherent relationship among:

- human-scale faces and restrained performance;
- armor that carries history and weight;
- legible weapons and Echo channels;
- faction architecture visible at gameplay speed;
- mass battle layers;
- King-era white-gold wonder;
- Eclipse corruption that threatens form without becoming an indistinct VFX cloud.

## 14.2 Character art rules

- Protagonist faces, skin, hair, body, armor, and signature equipment are canonical assets with controlled variants.
- Tarrik and Selene maintain distinct silhouettes even when later imagery places both in white armor or aligned visual language.
- Damage accumulates at authored thresholds and persists through the intended sequence.
- Clothing/armor state matches cinematics, gameplay, and save state.
- Companion silhouettes remain readable beside the protagonist.
- Facial performance prioritizes eyes, breath, hesitation, and controlled posture; scenes should not depend on exaggerated gesturing.
- Armor never behaves like weightless cloth during combat.

## 14.3 Weapon art and readability

Velkorran:

- broad enough to communicate commitment without becoming an oversized fantasy slab;
- edge/Echo behavior visible through lighting but not permanently blooming;
- grip, guard, and scabbard/deployment support all animation transitions;
- contact point reads at combat distance.

Verity:

- circular collapsed hilt and double-sided deployed form remain unmistakable;
- blade planes are readable from front, side, and motion without unsafe strobing;
- center-grip hand placement never appears to intersect the active edge;
- trails explain arc and danger but collision is grounded in the actual weapon path;
- folded/deployed states match every cinematic cut.

Rifles and tactical devices must remain recognizable in silhouette and have animation-ready moving parts defined before final rig lock.

## 14.4 Animation architecture

Core components:

- protagonist-specific locomotion state machines;
- motion matching or distance matching where it improves transitions without erasing authorship;
- layered upper-body ranged aim;
- authored montage framework for attacks and abilities;
- root-motion warping for target-aligned actions;
- control rig for alignment, hands, feet, and cinematics;
- physical animation for impacts and recovery;
- full-body IK and foot placement;
- additive damage, fatigue, corruption, and emotion layers;
- sync markers for companion actions and cinematics.

The same base skeleton may be used where efficient, but protagonist animation sets must preserve cadence, center of gravity, guard shape, and recovery. Retargeting is a starting point, not a shipping result.

## 14.5 Combat animation rules

Every combat animation must declare:

- gameplay startup, active, recovery, and cancel frames;
- movement authority and root-motion policy;
- hit trace notifies;
- super-armor/guard state windows;
- input buffer and branch markers;
- weapon state;
- audio/VFX/camera requests;
- target-warp bounds;
- interruption response;
- mirrored/left-right behavior;
- low- and high-frame-rate verification.

Animation anticipation may be shortened for responsive player moves but cannot make silhouettes unreadable. Enemy lethal attacks receive stronger anticipation and follow-through than ordinary attacks.

## 14.6 Hit reaction and physicality

Reaction selection considers:

- source direction;
- damage/poise magnitude;
- combatant mass and current stance;
- shield versus health hit;
- armor/weak point;
- airborne or grounded state;
- fatal/finisher eligibility;
- surrounding collision and nav safety.

Hit stop baseline is 35–90 ms depending on impact and accessibility setting. It affects animation/gameplay presentation according to a controlled time-dilation policy and must not cause input loss, audio desync, or companion AI instability.

Ragdolls blend from authored death where appropriate. Story deaths use bespoke performance and do not accidentally inherit systemic impulse.

## 14.7 VFX language

VFX communicates event first, faction second, spectacle third.

Priority order:

1. lethal/unblockable threat;
2. shield break, weak point, poise break, interrupt, and valid remedy;
3. player ability footprint;
4. faction and narrative identity;
5. atmosphere and spectacle.

VFX budgets define maximum overdraw, lights, translucency, particles, decals, and persistent fields per encounter. Performance variants preserve timing and footprint even when reducing density.

### Echo

- responds to rhythm and relation along body/equipment channels;
- different characters and factions interpret it differently;
- avoids generic glowing aura at all times;
- high state uses restrained harmonic light and material response.

### Corruption

- begins as disagreement and invasive alignment;
- uses violet as one cue among shape, timing, audio, and motion;
- may synchronize separate objects unnaturally;
- reduced-effects mode substitutes clean outlines, meter state, and directional signals.

## 14.8 Audio pillars

### Weight

Armor, footsteps, weapon handling, impact, shield stress, and room response communicate body and material.

### Distinction

Dominion, Reformation, King-era, Eclipse, and local cultures have different instrumentation, signal design, machinery, spatial behavior, and command language.

### Witness

Silence, breath, distant machinery, and the persistence of a voice after action support the story's reflective passages.

### Readability

Every combat-critical event has a priority, concurrency rule, ducking behavior, and caption/visual counterpart.

## 14.9 Combat audio

Required event families:

- attack anticipation by class;
- guard, perfect guard/deflection, shield impact and break;
- health hit by material and severity;
- poise pressure and break;
- weak-point acquire/hit/break;
- ability ready/insufficient/activate/end;
- Echo gain threshold and 100-ready;
- corruption band and link direction;
- off-screen threat;
- command and companion request;
- objective state.

Audio events use semantic gameplay messages, not animation-only timing where a game state is required. Mix priority prevents dialogue and lethal cues from being buried by explosions or crowd loops.

## 14.10 Music

Music is state-driven but authored per mission. Layers may respond to exploration, suspicion, combat intensity, command enemy, boss phase, Echo state, corruption, and aftermath.

Rules:

- protagonist themes retain distinct identity through convergence;
- faction themes can share or contest motifs without announcing a simplistic moral judgment;
- major narrative lines receive space rather than continuous scoring;
- combat loops have safe musical transition points to avoid obvious repetition;
- checkpoint restart restores a coherent musical state;
- licensed music is not a baseline dependency.

## 14.11 Dialogue recording and implementation

- Record principal performance with scene context and playable-beat timing.
- Capture exertion families by protagonist, weapon, severity, and narrative state.
- Avoid overusing reactive barks that undermine reserved characters.
- Implement facial animation with performance capture for principal cinematics and high-quality procedural/hand-authored support for systemic scenes.
- Store line IDs, takes, subtitle timing, facial asset, and localization state in one searchable record.
- Combat dialogue has short, medium, and interrupted variants where meaning must survive action.

## 14.12 Cinematic tiers

| Tier | Use | Production approach |
|---|---|---|
| A | major canon gates, intimate emotional turns, opening/ending | full performance capture, bespoke cameras/lighting/animation |
| B | mission transitions, companion confrontations, boss entry/exit | staged real-time scene with selective bespoke performance |
| C | hub exchanges, evidence presentation, operational briefings | systemic staging with authored cameras and facial performance |
| D | walk-and-talk, radio, combat dialogue | gameplay-integrated |

Cinematic scale must not be assigned by explosion size alone. A quiet trust rupture may require Tier A treatment.

## 14.13 Cinematic integration

`USovCinematicSubsystem` coordinates Sequencer, participant binding, world state, input, camera, subtitles, audio, skip, checkpoint, and recovery.

Requirements:

- validate participant equipment and damage state before playback;
- prestream required cells, animation, audio, and facial assets;
- transition from gameplay pose and position without visible snapping where possible;
- define input lock and limited-input moments explicitly;
- allow pause;
- allow skip after the first complete viewing where platform policy permits, except interactive choices;
- on skip, apply all required state writes, transforms, inventory/equipment changes, and checkpoints;
- recover safely if a referenced actor is missing;
- maintain subtitle/audio synchronization at 30 and 60 fps;
- provide handles for localization edits.

## 14.14 Cinematic canon gates

The following require Tier A or strong Tier B treatment and dedicated gameplay handoff review:

- Caelus naming Tarrik heir and Selene's shot;
- Tarrik's withheld market shot and its immediate consequence;
- first Crownmark/Witness activations for each protagonist;
- Lyessa's Cauldron confession and Tarrik's trust rupture;
- Glass Citadel authentication and the route to Aurelion;
- contrary-witness recognition;
- Tarrik and Selene choosing to remain together after the terminal;
- Tarrik's confrontation with and killing of Caelus;
- Lyric's trial/death and Selene's response;
- containment loss;
- Molahs epilogue reveal.

## 14.15 Photo mode

Photo mode is **BASELINE OPTIONAL** and may be cut before accessibility or gameplay polish. If retained:

- unavailable during protected story frames and competitive timing is irrelevant because campaign is offline;
- pauses simulation;
- respects spoiler and licensed-content rules;
- exposes camera, focal, exposure, and modest filter controls;
- may hide UI but not alter world state;
- meets platform capture policy.

---

# 15. Technical Architecture

## 15.1 Architecture principles

- C++ owns durable state, interfaces, performance-critical logic, validation, and save contracts.
- Blueprints author content, composition, tuning, and mission-specific behavior through supported interfaces.
- Gameplay Ability System owns abilities, attributes, effects, costs, cooldowns, and tag-driven combat state.
- Primary data assets and validated tables own content definitions.
- Gameplay tags express semantic state; raw strings and ad hoc booleans do not become cross-system contracts.
- Subsystems communicate through narrow interfaces and typed gameplay messages.
- No circular module dependencies.
- Single-player authority is still explicit so simulation, replay, and future reuse remain deterministic.
- Shipping code must not depend on a possible future Operations mode.

## 15.2 Module layout

| Module | Responsibility | May depend on |
|---|---|---|
| `SovereignCore` | shared types, tags, logging, settings, save interfaces, asset manager | Engine, Core UE modules |
| `SovereignAbility` | GAS components, attributes, executions, common abilities/effects | SovereignCore, GameplayAbilities |
| `SovereignCombat` | player/enemy combat, weapons, targeting, damage, interaction with animation | Core, Ability |
| `SovereignAI` | controllers, StateTree tasks, perception, threat, encounter director | Core, Ability, Combat |
| `SovereignNarrative` | dialogue, consequences, evidence, missions, relationships | Core; message interfaces to other modules |
| `SovereignWorld` | interactions, checkpoints, doors, streaming, world-state actors | Core, Narrative interfaces |
| `SovereignUI` | HUD, menus, settings UI, accessibility presentation | public interfaces from all runtime modules |
| `SovereignCinematic` | Sequencer integration, participant binding, skip/recovery | Core, Narrative, World |
| `SovereignAudio` | semantic audio routing and mix state | Core, public gameplay messages |
| `SovereignEditor` | validators, graph editors, mission tools, audit commands | editor-only access to runtime asset types |

`SovereignCore` cannot depend on higher modules. Narrative must not cast to concrete combat classes; it receives typed events and queries interfaces.

## 15.3 Core runtime classes

### Game framework

- `USovGameInstance`
- `ASovGameMode_Campaign`
- `ASovGameState_Campaign`
- `ASovPlayerController`
- `ASovPlayerState`
- `USovCampaignSubsystem`
- `USovMissionSubsystem`
- `USovSaveSubsystem`
- `USovCinematicSubsystem`
- `USovDialogueSubsystem`
- `USovEvidenceSubsystem`
- `USovAudioStateSubsystem`

### Characters

- `ASovCharacterBase`
- `ASovPlayerCharacterBase`
- `ASovTarrikCharacter`
- `ASovSeleneCharacter`
- `ASovAICharacterBase`
- `ASovCompanionCharacterBase`
- `USovCharacterMovementComponent`
- `USovAbilitySystemComponent`
- `USovAttributeSet_Core`
- `USovCombatComponent`
- `USovEchoComponent`
- `USovTargetingComponent`
- `USovInteractionComponent`
- `USovCameraControlComponent`
- `USovEquipmentComponent`
- `USovCorruptionComponent`
- `USovCompanionComponent`

### World and encounter

- `ASovEncounterDirector`
- `ASovCheckpointActor`
- `ASovWorldStateActor`
- `ASovDoorBase`
- `ASovMissionVolume`
- `ASovSpawnAnchor`
- `ASovCinematicAnchor`
- `USovFactionComponent`
- `USovThreatComponent`
- `USovCombatRoleComponent`

## 15.4 Character initialization

Initialization order:

1. actor and components constructed;
2. player state/owner established;
3. Ability System Component initialized with owner/avatar;
4. canonical protagonist/faction tags applied;
5. base attribute effect applied;
6. story equipment state loaded;
7. ability set granted from protagonist data;
8. progression nodes/augments applied;
9. mission-specific ability/equipment overrides applied;
10. saved health/resource policy applied;
11. UI and event listeners receive `Event.Character.Ready`;
12. input mapping context activates when possession and cinematic state permit.

Initialization must be idempotent across possession, checkpoint restore, cinematic rebind, and seamless level transition.

## 15.5 Gameplay Ability System

### Ability policy

- Instanced-per-actor for stateful protagonist abilities.
- Instanced-per-execution or non-instanced only for simple validated effects.
- Costs and cooldowns use Gameplay Effects.
- Abilities declare activation-owned, block, cancel, and required tags.
- Animation notifies emit events; they do not directly spend resource or alter mission state.
- Prediction keys may remain available in code, but launch campaign behavior is local authority.
- Ability cancellation always runs cleanup for camera, movement, collision, VFX, audio, and companion reservations.

### Ability sets

`USovAbilitySet` grants:

- gameplay abilities;
- attribute initialization effects;
- passive effects;
- input-tag bindings;
- ability augments;
- source object/equipment references.

Separate sets exist for shared movement/combat, each protagonist, signature equipment, story phase, mission override, companion behavior, and debug.

## 15.6 Gameplay tag taxonomy

Top-level namespaces:

- `Ability.*`
- `Character.*`
- `Faction.*`
- `State.*`
- `Status.*`
- `Damage.*`
- `Weapon.*`
- `Input.*`
- `Event.*`
- `Mission.*`
- `Narrative.*`
- `Evidence.*`
- `Interaction.*`
- `AI.*`
- `UI.*`
- `Audio.*`
- `Camera.*`
- `Data.*`

Examples:

- `Character.Player.Tarrik`
- `Character.Player.Selene`
- `Faction.Dominion`
- `Faction.Reformation`
- `Faction.Eclipse`
- `State.Combat.Guarding`
- `State.Combat.PerfectDefenseWindow`
- `State.Poise.Broken`
- `State.Invulnerable.Dodge`
- `State.Invulnerable.Ability`
- `State.Invulnerable.Respawn`
- `Status.Corruption.Contest`
- `Damage.BypassShield.Partial`
- `Event.Echo.Gained.PerfectGuard`
- `Narrative.CanonGate.Fixed`

Tags must be registered centrally, documented, validated against naming rules, and referenced through native tag declarations where used in C++.

## 15.7 Data assets and tables

Primary data assets:

- `USovProtagonistDefinition`
- `USovAbilitySet`
- `USovAttackDefinition`
- `USovWeaponDefinition`
- `USovEquipmentRefinementDefinition`
- `USovTechniqueTreeDefinition`
- `USovStatusDefinition`
- `USovEnemyArchetypeDefinition`
- `USovEncounterDefinition`
- `USovMissionDefinition`
- `USovDialogueGraph`
- `USovEvidenceDefinition`
- `USovCinematicDefinition`
- `USovAudioProfile`
- `USovCameraProfile`
- `USovAccessibilityProfile`

Data Tables are appropriate for dense homogeneous tuning such as damage coefficients, difficulty scalars, localized UI mappings, and telemetry IDs. Assets with object references or complex validation use primary data assets.

Every shipping data type carries a schema version. Editor migration functions or commandlets update old assets; runtime code does not accumulate indefinite compatibility branches for pre-production prototypes.

## 15.8 Typed gameplay messages

Use the Gameplay Message Subsystem or a narrow project wrapper for one-to-many semantic events.

Required message families:

- combat damage/shield/poise/death;
- ability begin/commit/end/cancel;
- Echo gain/spend/threshold;
- interaction offered/begun/completed;
- encounter phase/completion;
- mission objective/state;
- consequence/evidence update;
- companion command/co-action/rescue;
- cinematic begin/end/skip;
- accessibility/settings change;
- audio and UI presentation requests.

Messages contain typed structs and stable source IDs. Persistent state does not live only in an event stream; owning systems store authoritative values.

## 15.9 Save architecture

`USovSaveSubsystem` writes a versioned `USovCampaignSaveGame` containing:

- header: build, schema, platform user, timestamp, playtime, mission label, difficulty;
- campaign state and canon gates;
- consequence and evidence records;
- protagonist progression/equipment;
- companion/relationship memories;
- mission snapshot;
- checkpoint actor states;
- settings reference or embedded portable subset;
- integrity hash/checksum and migration history.

### Save rules

- Serialize IDs and compact values, not raw actor pointers.
- Use stable GUIDs for checkpointable actors.
- Write to a temporary slot and atomically replace the previous valid save where platform APIs permit.
- Retain the last known-good autosave if a write fails.
- Validate required assets and schema before applying loaded state.
- Unknown optional records are ignored with logging; unknown required canon state blocks load with recovery guidance.
- Migrations are deterministic and covered by golden-file tests.
- Save during patch-version changes must be supported within the shipped major product.

## 15.10 Asset management

Unreal Asset Manager labels primary assets by mission, protagonist, faction, shared system, cinematic tier, and localization/audio package.

Requirements:

- mission manifests enumerate hard and soft dependencies;
- no uncontrolled synchronous load during active combat;
- startup loads only global UI, settings, common player shell, and required front-end assets;
- protagonist swap preloads the next kit and animation package during transition;
- memory audit identifies duplicate textures, meshes, and audio banks;
- redirectors are fixed before content-lock builds;
- unused December-system assets are excluded from cook unless reserved in a separate plugin.

## 15.11 Streaming and world-state architecture

`USovWorldStateSubsystem` maps stable actor GUIDs to compact state records. Actors implement `ISovCheckpointable` with:

- `CaptureState()`;
- `ApplyState()`;
- `GetStateVersion()`;
- `MigrateState()` where needed;
- `GetRestorePhase()` to order doors, hazards, AI, and presentation.

Restore phases:

1. world/data layers;
2. structural actors and doors;
3. hazards and interactables;
4. encounters and AI spawn state;
5. companions;
6. player and resources;
7. UI/audio/camera presentation;
8. mission resume event.

## 15.12 Performance targets

### Frame targets

| Mode | Resolution strategy | Frame target | Frame time |
|---|---|---:|---:|
| Console performance | dynamic resolution/upscaling | 60 fps | 16.67 ms |
| Console quality | higher image/fidelity | 30 fps | 33.33 ms |
| PC | scalable | 30–120+ fps | settings-dependent |

Gameplay timing and hit detection must remain correct across 30–120 fps.

### Performance-mode frame budget baseline

| Area | Game-thread budget |
|---|---:|
| Player, abilities, combat | 2.0 ms |
| AI and encounter direction | 3.0 ms |
| Animation | 2.5 ms |
| World/physics/interaction | 2.0 ms |
| UI/audio submission/other | 1.2 ms |
| Engine/render submission margin | remaining CPU frame |

GPU budgets are platform- and scene-specific, but every level receives targets for geometry, shadows, translucency, particles, lighting, post-processing, and resolution floor.

### Memory baseline

Exact platform budgets require hardware profiling. Planning caps:

- no mission may rely on the full campaign asset set in memory;
- protagonist swap peak must fit without evicting global UI or causing a visible stall;
- cinematic preloads require measured peak budget and fallback quality;
- audio banks stream by mission and conversation set;
- texture pools maintain at least 10% safety margin in worst-case certified paths;
- memory leaks are tested through repeated mission reload and hub transitions.

## 15.13 Scalability

Scalable categories:

- resolution/upscaler and antialiasing;
- shadow resolution/distance;
- global illumination/reflection quality;
- geometry/Nanite detail;
- effects and translucency density;
- crowd tiers and distant battle density;
- foliage;
- post-processing;
- animation update rate for noncombat background actors.

Scalability may not alter:

- active enemy mechanics or attacker timing;
- collision and hit traces;
- required weak-point visibility;
- interaction availability;
- narrative participants;
- corruption remedy information.

## 15.14 Build configurations

- **Development:** diagnostics, cheats, visualizers, validation warnings.
- **Test:** shipping-like optimization with QA console and telemetry.
- **Shipping:** no debug commands or development data exposure.
- **Performance:** deterministic capture route, stat groups, fixed camera/encounter scripts.
- **Cinematic review:** shot/line IDs, skip and state diagnostics.
- **Accessibility review:** rapid switching among saved accessibility profiles.

Continuous integration cooks representative missions for every target platform, runs data validation, save migration, automation, map checks, and forbidden-reference scans.

## 15.15 Logging and diagnostics

Log categories:

- `LogSovAbility`
- `LogSovCombat`
- `LogSovAI`
- `LogSovEncounter`
- `LogSovNarrative`
- `LogSovMission`
- `LogSovSave`
- `LogSovWorld`
- `LogSovUI`
- `LogSovCinematic`
- `LogSovPerformance`

Every mission, encounter, dialogue, consequence, and evidence item has a stable debug ID. On failure, QA can export a compact state report without exposing private user data.

## 15.16 Error handling

Shipping behavior must fail safely:

- missing optional cosmetic asset: use fallback;
- missing critical mission asset: stop transition, preserve save, provide recoverable error;
- invalid interaction target: cancel without state write;
- failed cinematic participant bind: use declared fallback staging or stop before canon write;
- companion path failure: recovery anchor or encounter reset according to policy;
- save write failure: retain old save, notify clearly, continue only with user's informed choice;
- corrupted save: offer last known-good autosave and preserve the file for support;
- streaming timeout: remain at safe origin or transition screen, not unloaded gameplay.

## 15.17 Online and privacy

The campaign is offline-capable. Platform services may provide achievements, cloud saves, crash reporting, and optional analytics. No network connection is required to play after installation and entitlement checks governed by platform policy.

Analytics must:

- disclose collection according to law/platform policy;
- avoid dialogue free text or personal identifiers;
- use pseudonymous session/player IDs where enabled;
- support opt-out where required;
- separate development telemetry from consumer analytics;
- define retention and access responsibility before launch.

## 15.18 Technical acceptance criteria

- No cross-module circular dependency.
- All protagonist abilities clean up correctly on cancel, death, cinematic, and checkpoint restore.
- Save migration passes every supported golden save.
- Worst-case combat remains within performance target at the certified dynamic-resolution floor.
- Mission critical path can run from clean boot through completion without synchronous-load hitch above the agreed threshold.
- Gameplay behavior is frame-rate independent across 30, 60, and unlocked PC test profiles.
- Data validation reports zero shipping errors and no unapproved warnings.
- Removed campaign systems are absent from cooked runtime references.

---

# 16. Content Authoring and Production Pipeline

## 16.1 Pipeline goals

The project must let designers build and revise missions without bypassing durable technical contracts. Content must remain searchable, reviewable, testable, localizable, performant, and canon-auditable.

The pipeline should answer, for any shipping beat:

- who owns it;
- which canon gate or player question it serves;
- which assets and systems it requires;
- what state it reads and writes;
- how it checkpoints and recovers;
- what it costs on target hardware;
- whether VO, cinematic, localization, and accessibility work are complete;
- how QA reaches it quickly;
- what happens if it is cut.

## 16.2 Naming convention

Baseline pattern:

`[Type]_[SystemOrMission]_[Subject]_[Variant]`

Examples:

- `M_M04_NeonVeil_Undercity`
- `BP_M04_CrownmarkPedestal`
- `DA_Attack_Tarrik_Velkorran_Heavy_02`
- `GA_Selene_Disrupt`
- `GE_Status_Disrupted_Medium`
- `ST_AI_Dominion_ShieldLine`
- `SEQ_M07_LyessaConfession`
- `DLG_M06_Voss_SublevelOrder`
- `WAV_M14_Tarrik_Caelus_Refusal_003`
- `T_Eclipse_CorruptionVein_N`

Folders follow product/domain/mission ownership. Personal “misc,” “new,” “test2,” or developer-name shipping folders are prohibited. Experiments live in an explicitly excluded developer/plugin area.

## 16.3 Source control and map ownership

Required practices:

- one authoritative integration owner per mission map;
- External Actor changes assigned through task/region ownership;
- exclusive checkout for nonmergeable binary assets where supported;
- daily integration windows during active cross-discipline work;
- no mass redirector or asset-move operation without a reviewed plan;
- build verification before map handoff;
- automated report of unsubmitted dependencies and missing source assets;
- changelists reference mission, beat, issue, and test route;
- high-risk cinematic, map, and data migrations land behind backups and integration review.

## 16.4 Mission package

Each mission has:

- `USovMissionDefinition`;
- approved mission brief and beat sheet;
- persistent/root map and streaming structure;
- gameplay blockout and encounter definitions;
- state/flag matrix;
- checkpoint plan;
- dialogue and evidence graphs;
- cinematic list and participant matrix;
- art/audio/animation asset list;
- performance route and worst-case test save;
- accessibility review checklist;
- canon review record;
- localization/VO status;
- test plan and known-risk register;
- cut-down plan.

## 16.5 Mission authoring flow

1. **Canon brief:** narrative defines fixed events, knowledge state, characters, and prohibited branches.
2. **Player question:** design defines what the player is actively deciding or mastering.
3. **Paper beat map:** mission disciplines agree on pace, protagonist fantasy, consequence, and scope.
4. **State skeleton:** IDs, transitions, checkpoint boundaries, dialogue/evidence placeholders, and exit state are authored before set dressing.
5. **Greybox:** full critical path playable with representative encounter and traversal timings.
6. **First complete:** all beats, choices, recovery, and required AI operate with temporary presentation.
7. **Production pass:** final art/animation/audio/cinematic work enters through locked interfaces.
8. **Alpha pass:** no missing content; balance, performance, accessibility, localization, and bugs remain.
9. **Beta pass:** content and schema locked; optimization and defect resolution only.
10. **Gold candidate:** certification, continuity, save, crash, and performance gates pass.

No mission proceeds to expensive final art because one impressive arena exists while its state flow, checkpoint, or ending remains undefined.

## 16.6 Encounter authoring flow

1. declare story/tactical purpose;
2. choose faction roles and player lesson;
3. block arena and recovery space;
4. author encounter phases and role quotas;
5. validate with unupgraded protagonist on Standard;
6. validate protagonist-specific alternate build approaches;
7. add allies/civilians/objective complications;
8. add presentation layers and background battle tiers;
9. tune difficulty/scalability variants;
10. capture telemetry and review video;
11. lock checkpoint snapshot;
12. run performance, exploit, accessibility, and AI soak tests.

Encounter definitions may reference archetype pools, but wave composition is authored. Random variation may alter a small support slot only if it cannot break dialogue, checkpoint state, difficulty, or performance.

## 16.7 Narrative content pipeline

Narrative assets move through:

- canon outline;
- playable-beat script;
- graph/state implementation;
- table read with gameplay timing;
- temporary VO and subtitle pass;
- in-engine staging;
- performance script lock;
- recording/performance capture;
- editorial and cinematic implementation;
- localization handoff;
- final continuity and knowledge-state audit.

Every line has a stable ID before recording. Rewritten recorded lines preserve history and downstream references. Narrative never encodes critical logic only in Sequencer; mission state writes occur through the narrative/mission interface.

## 16.8 Cinematic pipeline

For each cinematic:

- identify tier, canon gate, skip behavior, and gameplay entry/exit;
- lock participants, equipment state, damage state, and environment phase;
- create previs using actual gameplay spatial constraints;
- reserve localization handles;
- record performance only after action and intent review;
- implement cameras, animation, facial, lighting, audio, VFX;
- test from every legal prior consequence state;
- test skip before, during, and after streaming readiness;
- validate subtitles, accessibility, 30/60 fps, and HDR;
- capture a golden review movie with shot and line IDs.

## 16.9 Art and asset gates

Asset stages:

- brief/reference;
- blockout;
- gameplay-approved dimensions and sockets;
- first art pass;
- material/VFX/animation integration;
- LOD/Nanite/collision/performance pass;
- damage/variant pass;
- final lighting/context review;
- shipping validation.

An asset is not final until collision, sockets, pivots, scale, gameplay readability, naming, memory, LOD, platform render, and source-file archival all pass.

## 16.10 Audio implementation pipeline

- gameplay event contract approved;
- source recording/design;
- middleware/engine event created;
- priority/concurrency/routing assigned;
- spatialization and surface/faction variants implemented;
- captions/visual equivalents linked where required;
- combat mix tested at density extremes;
- localization banks validated;
- memory and streaming budget passed;
- checkpoint/restart behavior verified.

## 16.11 Data validation commandlet

The project validation commandlet must report:

- missing primary asset IDs;
- invalid gameplay tags;
- circular/illegal module-content references;
- missing localization strings;
- dialogue graph errors;
- mission transitions to unknown states;
- consequences with invalid IDs or payload schema;
- abilities without cleanup/animation/cost declarations;
- attacks with missing trace or timing data;
- status effects without cleanse/UI/accessibility rules;
- encounters over declared role budgets;
- checkpointable actors without stable GUIDs;
- cinematics without skip/recovery policy;
- assets referenced from excluded prototype/Operations plugins;
- legacy class, morality, rarity, vendor, or crafting references in campaign content.

Errors block candidate builds. Warnings require an owner and waiver date.

## 16.12 Definition of content-complete

A mission is content-complete when:

- critical and optional paths work from clean save;
- all dialogue, cinematics, combat, traversal, evidence, and choices are present;
- state writes and checkpoint recovery pass;
- no placeholder asset affects meaning, timing, collision, or performance;
- VO and subtitles exist in source language;
- localization source is locked;
- accessibility features function;
- performance is within 10% of final target with a credible optimization plan;
- canon review is approved;
- known bugs are triaged and no blocker/critical remains.

---

# 17. Production Strategy

## 17.1 Risk order

Prove the unique game in this order:

1. Tarrik and Selene feel distinct and satisfying.
2. Camera and movement survive real melee density.
3. Combat impact, defense, targeting, and resource rhythm work without loot.
4. Faction behavior is identifiable.
5. Echo rewards character method.
6. Corruption communicates identity/control pressure without frustrating agency.
7. Narrative choice and gameplay consequence share one state system.
8. Cinematic/gameplay handoffs preserve control, state, and performance.
9. Mission pace sustains action and reflection.
10. Production can build the required content at measured cost.

Progression breadth, optional exploration volume, and spectacle scale come after these proofs.

## 17.2 Vertical slice selection

The vertical slice is a 50–60 minute production-quality excerpt derived from the lower Aurelion terminal in M12–M13. It is built as shippable campaign content but staged so reviewers can play it without full-campaign prerequisites.

Slice flow:

1. **Tarrik entry (10–12 min):** Dominion/Reformation battle aftermath; hold a corridor, protect mixed survivors, break a disciplined formation.
2. **Selene entry (10–12 min):** alternate route; analyze terminal systems, evade sensors, sever a command network, precision encounter.
3. **Contrary-witness handoff (5–7 min):** Tier A/B cinematic and controlled protagonist switch.
4. **Eclipse escalation (15–18 min):** synchronized enemy behavior, corruption bands, companion protagonist, one Resonance setup/payoff.
5. **Local decision (3–5 min):** choose whom to trust with a terminal action or which vulnerable group receives immediate protection; consequence is visible in the escape.
6. **Quiet aftermath (5–8 min):** Tarrik and Selene remain together voluntarily; conversation reflects the local decision while preserving canon.

The slice uses a curated late-game ability subset exposed through a temporary review save. It does not attempt the fleet battle, final Aurelion scale, Caelus confrontation, or full skill trees.

## 17.3 Vertical-slice deliverables

- final-quality protagonist models, one armor state each, Velkorran, Verity, and both ranged tools;
- representative Tarrik/Selene locomotion, defense, melee, ranged, four basic abilities total, and one joint Resonance;
- one Dominion and one Reformation combat role in the opening, plus three Eclipse roles and one elite;
- Tier A/B contrary-witness scene and Tier C aftermath dialogue;
- one consequence record with immediate and aftermath consumers;
- functional Technique/Equipment subset;
- representative HUD, subtitles, remapping, and key accessibility options;
- checkpoint/save/retry across every beat;
- console performance profile with background battle tiers;
- production metrics by discipline and asset class;
- QA, canon, and user-research reports.

## 17.4 Slice non-requirements

- full campaign front end;
- complete Evidence archive;
- all three progression branches;
- every faction archetype;
- final fleet/space rendering;
- mission replay/New Game Plus;
- photo mode;
- Operations, co-op, PvP, loot, vendors, crafting, classes, or open world.

## 17.5 Slice acceptance gate

The slice passes only if:

- 80% or more of target players identify Tarrik and Selene correctly from unlabelled control footage or hands-on feel;
- median player satisfaction for both kits meets the project's greenlight threshold, with neither protagonist more than one rating point behind the other on a ten-point scale;
- core combat remains engaging when Technique Point and reward notifications are disabled;
- each represented faction is identified from behavior by a majority of players after one session;
- at least 70% of players can explain how each protagonist earns Echo;
- corruption remedy is understood without developer explanation by at least 80% after one prompted tutorial exposure;
- the local decision is remembered and its consequence noticed in debrief interviews;
- no canon contradiction survives review;
- checkpoint/reload success exceeds 99.5% in automation/soak;
- performance meets the approved target in worst-case slice capture;
- measured mission cost supports the revised campaign budget.

Failure produces a scoped corrective prototype or product re-evaluation—not automatic full production.

## 17.6 Production phases and gates

### Phase 0 — Product and canon lock

Deliver:

- approved TDD;
- final manuscript adaptation matrix;
- platform/performance baseline;
- staffing/budget model;
- protagonist kit specifications;
- vertical-slice brief;
- source-control/integration process.

Gate: creative, game, narrative, technical, art, production, and finance owners approve the same product.

### Phase 1 — Core prototype

Deliver:

- greybox camera/movement;
- primary melee/ranged/defense for both protagonists;
- one formation encounter and one network encounter;
- Echo prototype;
- corruption prototype;
- dialogue/consequence/checkpoint integration;
- performance spike for battle tiers.

Gate: fun, readability, identity, and technical-risk review.

### Phase 2 — Vertical slice

Deliver the scope in 17.3 at representative quality.

Gate: acceptance criteria in 17.5 and revised bottom-up production forecast.

### Phase 3 — Full-production preparation

Deliver:

- final mission count and beat map;
- staffing ramp and outsource plan;
- content pipelines and validators;
- complete shared-system backlog;
- all mission pods through greybox scheduling;
- VO/performance capture calendar;
- accessibility and localization plan;
- certification and release plan.

Gate: funding and greenlight against measured cost.

### Phase 4 — Campaign production

Build missions in playable order where practical. Maintain a continuously playable critical path. Review monthly for protagonist quality, canon, pace, performance, and content cost.

Gate: feature complete, then content complete.

### Phase 5 — Alpha

- full game playable start to finish;
- all systems and content present;
- no placeholder affecting evaluation;
- save migration and all platforms functioning;
- localization in integration;
- optimization and balance focused.

Gate: zero blockers in critical path and approved remaining defect curve.

### Phase 6 — Beta/certification

- content and schemas locked;
- final VO/localization integrated;
- platform compliance, accessibility, performance, crash, and soak testing;
- only approved critical fixes and optimization.

Gate: release-candidate criteria.

### Phase 7 — Gold and launch support

- candidate preservation and symbol/source archival;
- day-one patch only if explicitly approved;
- support runbooks for crash/save/progression blockers;
- analytics and feedback review;
- no Operations commitment until campaign quality and commercial gate are reviewed.

## 17.7 Staffing reality

The target presentation, dual bespoke combat kits, performance-captured narrative, multi-world art, and mass-battle illusion require a multidisciplinary team. Production must create a bottom-up forecast after the slice.

Planning ranges, not commitments:

| Model | Core team | Likely scope |
|---|---:|---|
| Full target | 90–140 internal-equivalent at peak plus specialized outsource | 15–16 missions, premium cinematics, representative large battles |
| Lean target | 45–75 at peak plus focused outsource | 10–12 missions, fewer unique worlds/bosses, constrained crowd and cinematic tiers |
| Small independent | under 35 | requires material redesign: shorter campaign, stylized presentation, fewer worlds, encounters, and bespoke cinematics |

Attempting the full target with a small team and no scope change is a primary project risk, not an efficiency challenge.

## 17.8 Mission-pod model

Each active mission pod includes, full- or part-time:

- mission/level designer;
- narrative designer/writer;
- environment art owner;
- gameplay/encounter designer;
- technical design support;
- cinematic/audio/animation support according to beat plan;
- QA owner;
- producer.

Shared strike teams own protagonists/combat, AI, narrative technology, UI/accessibility, cinematics, performance, tools, and platform. Pods consume stable versions and contribute requirements through controlled planning.

## 17.9 Cut hierarchy

Cut first:

1. Operations, PvP, or campaign co-op;
2. photo mode and New Game Plus if not already low-cost;
3. low-impact collectibles and mastery variants;
4. redundant optional routes or hub revisits;
5. enemy variants that do not change behavior;
6. secondary spectacle that does not advance character, theme, or mechanic;
7. cosmetic variation beyond canon needs;
8. progression nodes that only provide small numbers.

Protect:

- distinct protagonist feel;
- complete canonical spine;
- Tarrik/Lyessa and Selene/Lyric/Tharne relationship turns;
- Crownmark/Witness causality;
- faction combat identity;
- Echo and corruption as thematic mechanics;
- contrary-witness convergence;
- Caelus, Lyric, containment-loss, and Molahs sequences;
- accessibility, checkpoint stability, performance, and save integrity.

## 17.10 Schedule-health rules

- New feature after vertical-slice lock requires an equal or larger identified cut.
- A missed greybox gate is escalated before final art begins.
- Repeated overtime is treated as a planning defect.
- Outsource work requires internal specification, review bandwidth, source ownership, and integration budget.
- Performance debt is tracked per mission from greybox.
- VO/cinematic changes after performance lock carry explicit cost and continuity review.
- No late reintroduction of removed RPG systems to increase perceived “content.”

---

# 18. Quality Assurance and Validation

## 18.1 Test strategy

Testing combines:

- unit and low-level automation;
- functional/integration automation;
- deterministic mission routes;
- human exploratory testing;
- combat and user research;
- canon/editorial review;
- accessibility evaluation with disabled players and specialist review;
- performance/memory profiling;
- platform certification;
- localization QA;
- long-duration soak and save migration.

Quality is built into each mission gate, not deferred to a final bug-fix phase.

## 18.2 Test ownership

| Area | Primary owner | Required partners |
|---|---|---|
| Combat feel/balance | combat design | QA, animation, audio, VFX, user research |
| AI/encounters | AI/encounter design | QA, performance, level design |
| Canon/knowledge | narrative | QA, cinematic, mission design |
| Save/checkpoint | engineering | QA, mission design, platform |
| Accessibility | UX/accessibility lead | QA, design, audio, art, external players |
| Performance | technical art/engineering | mission pods, QA, platform |
| Localization | localization owner | narrative, UI, cinematic, QA |
| Certification | release/platform owner | all disciplines |

## 18.3 Automated tests

### Unit/data

- attribute clamping and damage routing;
- shield delay/regen;
- stamina costs and exhausted action rejection;
- poise break/recovery immunity;
- Echo gain, decay, spend, and checkpoint policy;
- status stacking/cleanse;
- gameplay-tag queries;
- progression prerequisites and respec;
- consequence/evidence serialization;
- dialogue condition evaluation;
- save schema migration.

### Functional

- each ability activation/cancel/death/reload path;
- melee traces at 30/60/120 fps;
- targeting around walls, hazards, and invalid targets;
- companion commands/co-actions/path recovery;
- encounter phase and checkpoint restore;
- doors/lifts/stream transitions;
- cinematic skip at multiple timestamps;
- every mission state transition;
- every canon gate from all legal input states;
- controller/keyboard hot-swap and remapping;
- accessibility preset application.

### Content validation

- no broken references;
- no missing subtitle/VO IDs;
- no unreachable dialogue;
- no unknown consequences or evidence;
- no checkpointable actor without GUID;
- no legacy prohibited system in campaign cook;
- no mission exceeding declared role or memory budgets without waiver.

## 18.4 Combat test matrix

Every protagonist ability and core action is tested against:

- shielded and unshielded target;
- line, shield, marksman, controller, commander, duelist, brute, swarm, and corruptor where applicable;
- ordinary, elite, and boss immunity policy;
- airborne/ledge/wall/door/crowd proximity;
- no stamina/Echo, exact cost, full resource;
- status and corruption bands;
- companion present/absent/disabled;
- 30/60/120 fps;
- Story/Standard/Veteran/Sovereign;
- maximum and minimum aim assistance;
- checkpoint reload during adjacent legal states.

## 18.5 Save and checkpoint testing

Required cases:

- save at every legal mission beat;
- kill process during write;
- fill storage and deny write;
- load after patch migration;
- load with missing optional DLC/plugin;
- corrupted newest autosave;
- repeated death/retry 100 times;
- cinematic skip immediately after checkpoint;
- protagonist transition;
- companion disabled/rescue state;
- destruction and streaming states;
- manual save before/after progression respec;
- platform user change and cloud conflict.

Release blockers include loss of progress, canon-state corruption, unrecoverable save, false save-success message, and reproducible spawn inside lethal geometry.

## 18.6 Canon and continuity testing

Maintain a machine-readable knowledge/state matrix for each major character by mission beat. Review:

- what each character knows;
- what evidence is authenticated;
- relationship state and witnessed actions;
- equipment/injury/location;
- public institutional knowledge;
- Crownmark/Witness state;
- Final Dawn/Terminal Denial state;
- fixed future events.

Every choice branch rejoins canon with an explicit reconciliation note. “The next scene assumes it” is not sufficient.

## 18.7 Accessibility testing

- recruit players with relevant motor, vision, hearing, and cognitive access needs during prototype, slice, alpha, and beta;
- test all required actions with holds/toggles/rapid-press alternatives;
- test without color, without audio, and with reduced VFX/camera motion;
- validate screen reader and focus order on supported platforms;
- validate captions and directional cues during maximum combat density;
- ensure assists survive checkpoint/load and protagonist switch;
- review one-handed and remapped control configurations;
- test seizure-risk content and flash frequency according to applicable guidance;
- test corruption presentation at minimum effects.

## 18.8 Performance testing

Each mission provides:

- deterministic critical-path capture;
- worst-case combat capture;
- worst-case streaming transition;
- highest cinematic memory peak;
- hub soak;
- protagonist/quality/scalability profiles;
- automated frame-time percentile report.

Performance gate uses percentiles, not average only. Baseline candidate requirement:

- performance mode meets frame target for at least 99% of measured gameplay frames on target kit excluding approved loading transitions;
- no sustained CPU/GPU spike above 50 ms during player-controlled combat;
- dynamic resolution does not remain below certified image-quality floor;
- no growing memory trend across three consecutive mission reloads;
- no shader compilation during normal shipping play.

Exact thresholds may be revised by platform and final rendering strategy.

## 18.9 Soak and stability

- 8-hour mixed campaign autoplay;
- 24-hour front-end/hub cycle;
- repeated mission reload and checkpoint death loops;
- repeated save/load and settings changes;
- suspend/resume and controller disconnect;
- network loss while using optional platform cloud/achievement service;
- cinematics repeatedly skipped/not skipped;
- low-storage, thermal, and background-download platform conditions.

Crash-free and hang-free thresholds are set before beta and reported per platform/build.

## 18.10 User research questions

At prototype and slice:

- Can players describe the protagonists' difference without UI labels?
- Does Tarrik feel heavy or merely slow?
- Does Selene feel precise or merely fragile?
- Are melee and ranged one coherent loop?
- Does Echo reward understood behavior?
- Does corruption feel like an agency/identity threat with fair counterplay?
- Can players identify faction logic?
- Do they understand what a dialogue choice intends?
- Do local consequences feel meaningful despite fixed canon?
- Does quiet pacing deepen investment or feel like delay?

At alpha/beta:

- Does alternation create anticipation or frustration?
- Does progression meaningfully vary play without obscuring character identity?
- Are optional spaces valuable without completion anxiety?
- Is the Aurelion convergence mechanically and emotionally earned?
- Do players understand why both institutions continue after accepting evidence?
- Do accessibility assists preserve information and satisfaction?

## 18.11 Defect severity

| Severity | Definition |
|---|---|
| Blocker | cannot build, boot, progress, load, or certify; widespread data loss/security issue |
| Critical | reproducible crash, save corruption, canon break, major accessibility barrier, progression blocker with limited workaround |
| Major | substantial mechanic, mission, performance, localization, or presentation failure with workaround |
| Minor | limited defect that does not materially block play or meaning |
| Polish | improvement request with no functional failure |

Canon contradiction, missing required subtitle, or inaccessible mandatory input may be Critical even if the executable does not crash.

## 18.12 Release-candidate gates

- full campaign completion on every platform from clean install and migrated save;
- zero Blocker and zero unwaived Critical defects;
- approved Major-defect count and risk ownership;
- platform certification pass or accepted resubmission plan;
- performance, crash, memory, and save thresholds met;
- final canon/continuity approval;
- localization and subtitle completion;
- accessibility conformance review;
- credits and legal review;
- support runbooks and build archival complete;
- Operations/legacy plugins confirmed excluded from campaign build unless separately approved.

---

# 19. Migration from the December 2025 TDD

## 19.1 Migration rule

Reuse implementation only when it serves the new product without carrying obsolete assumptions. A technically functional system is not automatically a good foundation if its data model, UI, save schema, or dependencies pull the campaign back toward the rejected game.

## 19.2 Retain and adapt

| December foundation | v2 action | Notes |
|---|---|---|
| Unreal Engine 5 / C++ module approach | retain | reorganize around current module boundaries |
| Gameplay Ability System | retain | abilities, effects, attributes, tags, event-driven combat |
| `USovAttributeSet_Core` health/shield basis | retain/adapt | add protagonist balance and current damage policy |
| shield-first damage and delayed regeneration | retain | 3 s / 5% baseline remains prototype-tunable |
| stamina and poise concepts | retain/adapt | remove class dependence; tune by protagonist and archetype |
| melee attack/montage/trace/cancel framework | retain/adapt | build bespoke Velkorran and Verity data/animation |
| ranged aim/recoil/projectile framework | retain/adapt | signature rifle profiles only |
| status-effect framework | retain/adapt | add explicit accessibility, cleanse, save, AI contracts |
| camera component and mode arbitration | retain/adapt | distinct protagonist profiles and dense-combat safety |
| Enhanced Input | retain | semantic input tags and full accessibility/remap |
| dialogue graph concepts | retain/adapt | authored protagonists, knowledge, consequences, no morality |
| cinematic trigger/Sequencer integration | retain/adapt | add skip, recovery, state, streaming contracts |
| quest/flag/save foundations | retain/adapt | mission hierarchy, consequence/evidence records, schema migration |
| AI Perception / StateTree | retain/adapt | faction behaviors, role logic, battle tiers |
| companion and civilian foundations | retain/adapt | authored companions, no roster/approval economy |
| doors, interactions, checkpoints, streaming | retain/adapt | wide-linear mission requirements |
| Gameplay Tags and data assets/tables | retain | replace obsolete tag/data taxonomies |
| event/delegate architecture | retain/adapt | typed messages and clear persistent ownership |
| animation pipeline | retain/adapt | protagonist-specific cadence and convergence sync |

## 19.3 Redesign completely

| December system | v2 replacement |
|---|---|
| Echo thresholds/overflow as generalized class resource | character-specific 0–100 action-earned Echo with Resonant state and signature spend |
| class-driven movement/camera/combat modifiers | two fixed protagonist profiles |
| generic weapon/equip framework | canonical signature-equipment state and deterministic refinements |
| broad faction-ownership logic | mission-authored faction/world state |
| hotbar/ability wheel | four mapped actives plus signature release and context commands |
| inventory/map/quest UI designed for broad RPG | compact mission, Techniques, Equipment, Evidence, local map |
| companion approval | discrete relationship memories and witnessed consequences |
| generic death/down/revive loop | rapid solo checkpoint retry with authored companion rescue policy |
| content-after-systems roadmap | playable protagonist prototype and vertical slice before broad production |

## 19.4 Remove from campaign

Delete or exclude from the campaign plugin/cook:

- nine-class and specialization switching;
- class-restricted weapon tables;
- XP and character levels;
- inventory grid and item stacks unrelated to mission/evidence state;
- gear rarity, item level, randomized perks, set bonuses, and gear score;
- vendor types, stock refresh, pricing, buy/sell, and currencies;
- crafting stations, materials, recipes, and upgrade RNG;
- loot drops and drop tables;
- morality grid and King's Measure;
- numeric companion approval and romance progression;
- open-world faction ownership and systemic territory control;
- multiplayer authority assumptions embedded in campaign UI or mission flow;
- daily/rotating/legendary stock or live-service hooks.

Removal requires reference auditing. Dead Blueprint nodes, tags, save fields, UI strings, tables, icons, and tutorial text must be removed or isolated, not merely hidden.

## 19.5 Reserve outside campaign

Potentially reusable for a future Operations product:

- class/spec framework;
- broader inventory and loadout architecture;
- repeatable progression;
- multiplayer replication scaffolding;
- encounter randomization;
- additional weapons and archetypal kits;
- economy only if Operations establishes a clear, nonexploitative reason.

Reserved code lives in a separate disabled plugin or archival branch with explicit ownership. The campaign cannot depend on it.

## 19.6 Migration sequence

1. Tag December assets and code as Retain, Adapt, Remove, Reserve, or Unknown.
2. Freeze new work on Remove/Reserve systems.
3. Establish v2 modules, tags, core player classes, and save schema.
4. Port health/shield/stamina/poise/damage foundations with automated tests.
5. Port camera/movement/input and replace class queries with protagonist data.
6. Port melee/ranged/GAS foundations into minimal Tarrik and Selene kits.
7. Replace Echo and status/corruption rules.
8. Port AI, interaction, checkpoint, mission, dialogue, and cinematic foundations through interfaces.
9. build the core prototype and slice without removed assets in cook.
10. migrate useful content only after the system it depends on passes its v2 gate.
11. archive or delete obsolete campaign references after verification and source-control backup.

## 19.7 Legacy-save policy

December 2025 prototype saves are not supported. They belong to a materially different, unreleased product model. The first external v2 build establishes save schema 1.0; compatibility commitments begin only when production design and distribution require them.

---

# 20. Deferred Operations Product Track

## 20.1 Scope boundary

Operations is not a launch mode, not a campaign checkbox, and not justification for campaign complexity. It is a separate possible product track that may reuse setting, combat technology, enemies, and selected class ideas after the campaign proves quality and capacity.

Status: **DEFERRED**.

## 20.2 Candidate concept

A future Operations mode could provide replayable squad missions featuring non-canon or supporting combatants, broader classes, builds, and co-op. It would carry battlefield replayability without asking Tarrik and Selene's authored campaign to support drop-in partners or divergent canon.

Candidate boundaries:

- separate executable mode/front-end or clearly isolated module set;
- separate progression and save namespace;
- no effect on campaign balance or story flags;
- no requirement to play for campaign completion;
- no campaign cinematics rewritten for co-op;
- no class switching imported into Tarrik/Selene missions;
- transparent business model approved separately.

## 20.3 Greenlight conditions

Operations may enter pre-production only if:

- campaign vertical slice passes;
- campaign schedule, budget, and staffing are protected;
- core combat is stable and reusable;
- network/replication feasibility is proven separately;
- product purpose and audience are documented;
- ongoing content expectations are funded;
- legal, platform, moderation, privacy, and service operations are planned;
- its existence does not weaken the single-player promise.

## 20.4 Technical isolation

If explored, Operations should use separate plugins such as `SovereignOperations`, `SovereignOnline`, and `SovereignClasses`. Shared modules expose neutral combat interfaces. Campaign modules do not import Operations types. Automated cook validation ensures Operations assets are absent from campaign builds.

---

# 21. Risks, Open Decisions, and Approval Gates

## 21.1 Risk register

| Risk | Likelihood | Impact | Early signal | Mitigation | Owner |
|---|---|---|---|---|---|
| “Space Marine” scale exceeds team/budget | High until funded | Critical | slice asset/mission cost above forecast | bottom-up slice forecast; lean-scope alternative; battle-tier simulation | Executive producer |
| Protagonists feel like reskins | Medium | Critical | playtest language and identical rotations | bespoke animation/timing/resource/defense; identity acceptance gate | Game/combat director |
| Tarrik feels slow | Medium | High | high whiff/frustration, low forward control | hit-confirm branches, guard mobility, camera tuning, enemy composition | Combat design |
| Selene feels fragile or stealth-dependent | Medium | High | avoidance of open combat | strong disruption, deterministic precision, escape tools, fair pressure | Combat design |
| Fixed canon makes choices feel fake | Medium | High | players cannot recall consequences | local visible outcomes, relationship memory, accurate prompts, fewer stronger choices | Narrative director |
| Combat overwhelms character pacing | Medium | High | quiet scenes skipped/disliked, emotional beats forgotten | mission rhythm review, aftermath requirement, restrained duration | Game/narrative director |
| Narrative overwhelms action pacing | Medium | High | low replay/session completion | integrate investigation with action; compact hubs; tier dialogue | Mission design |
| Echo reads as generic mana | Medium | High | players build it through incidental damage | strict character actions, distinct feedback, tutorial/telemetry | Systems design |
| Corruption frustrates or harms accessibility | Medium | Critical | remedy confusion, motion sickness | agency rules, reduced-effects parity, access testing | UX/VFX/design |
| Dense battles break camera/readability | High | High | off-screen deaths, collision, target loss | tiered AI, attacker slots, camera safety, arena gates | Combat/level design |
| Companion AI breaks canon beats | Medium | Critical | missed co-actions/markers | bespoke state, anchors, automation, fallbacks | AI/narrative tech |
| External Actors/map integration corruption | Medium | Critical | missing actors, merge conflicts | single integration owner, region locks, backups, validation | Technical director |
| Late cinematic rewrite cascades | High | High | repeated VO/shot changes | playable script/table read, lock gates, handles, change control | Cinematic producer |
| Save/state complexity causes blockers | Medium | Critical | unrecoverable test saves | versioned state, golden saves, atomic writes, early automation | Lead engineer |
| Too many unique worlds dilute quality | High | High | asset reuse mismatch, late greyboxes | reuse by faction/ship, combine mission boundaries, cut hierarchy | Art/production |
| Legacy systems creep back in | Medium | High | rarity/class/currency references | cook validation, plugin isolation, product review | Game director |
| Final manuscript changes during production | Low/Medium | Critical | revised canon after performance lock | canon-change process, cost report, sequel/record alternative | Creative director |

## 21.2 Open product decisions

The following must be resolved no later than the stated gate:

| Decision | Current baseline | Deadline | Owner |
|---|---|---|---|
| Funding/staffing model | full target described in TDD | before slice staffing lock | Executive leadership |
| Final platform list | PC, PS5, Xbox Series X|S | before full-production tools/cert plan | Executive producer |
| 60 fps commitment on all consoles | primary gameplay target | before slice art budget lock | Game/technical director |
| Campaign mission count | 16 plus prologue/epilogue | after slice cost review | Game/narrative/production |
| Critical-path length | 18–22 hours | after slice and greybox forecast | Game director |
| Exact vertical-slice terminal excerpt | lower Aurelion M12–M13 | before Phase 1 content work | Narrative/game director |
| Cinderline ammunition model | heat or magazine, not both | core combat prototype | Combat director |
| Field-healing fiction/interface | 2-charge baseline | core combat prototype | Creative/combat |
| Companion rescue policy | Story/Standard, once per segment | slice | Game/UX |
| Photo mode | baseline optional | content-complete scope review | Production |
| Mission replay / New Game Plus | deferred | post-alpha or post-launch gate | Production/game director |
| Voice-localization languages | TBD | before VO contract | Publishing/localization |
| Rating detail and reduced-gore option | M/18 baseline | art/cinematic production lock | Creative/legal/accessibility |
| Consumer analytics/telemetry | optional, privacy-bound | platform/privacy design gate | Product/legal |

## 21.3 Locked decisions not to reopen casually

- third-person action-adventure campaign direction;
- two fixed protagonists;
- authored alternation rather than free switching;
- no campaign classes, loot rarity, XP levels, vendors, crafting, morality grid, or approval bar;
- no launch campaign co-op;
- wide-linear mission structure;
- signature equipment;
- character-specific Echo generation;
- canonical major outcomes with bounded local agency;
- Operations isolated and deferred.

Reopening one requires evidence that the current product cannot meet its player promise, plus a full scope, canon, schedule, and architecture review.

## 21.4 Approval matrix

| Change type | Required approval |
|---|---|
| Numeric tuning within intent | owning design lead |
| Ability behavior or progression node | combat/systems lead; narrative if identity affected |
| Mission beat order within canon | mission + narrative directors |
| Mission boundary or protagonist ownership | game + narrative + production directors |
| Canon event, relationship, or outcome | creative + narrative + game directors |
| Platform/performance target | technical + game + executive production |
| New system or mode | game director + technical director + executive producer, with equal cut/funding |
| Accessibility feature removal | UX/accessibility lead + game director + production, with documented alternative |
| Operations greenlight | executive/product gate after campaign protection review |

---

# Appendix A. Prototype Tuning Baseline

All values are starting points for instrumented playtest.

## A.1 Shared combat

| Parameter | Baseline |
|---|---:|
| Shield regeneration delay | 3.0 s |
| Shield regeneration | 5% max/s |
| Health natural regeneration | none |
| Echo range | 0–100 |
| Post-combat Echo floor | 25 |
| Echo decay delay/rate | 6 s / 8 per s above floor |
| Melee input buffer | 0.22 s |
| Standard finisher duration | 0.8–1.8 s |
| Respawn invulnerability | 1.5 s or until offensive action, whichever first |
| Hard-control recovery immunity | 1.0–2.5 s by severity |
| Out-of-combat HUD fade | 6 s |

## A.2 Tarrik

| Parameter | Baseline |
|---|---:|
| Max stamina | 120 |
| Stamina regen | 32/s after 0.65 s |
| Evade cost / invulnerability | 24 / 0.18 s |
| Perfect guard window | 0.16 s |
| Echo: perfect guard | +12 |
| Echo: guard counter | +10 |
| Echo: poise break | +15 |
| Echo: protection intercept | +15 with source cooldown |
| Basic ability cost band | 25–35 |
| Signature release cost | 100 |

## A.3 Selene

| Parameter | Baseline |
|---|---:|
| Max stamina | 100 |
| Stamina regen | 38/s after 0.55 s |
| Evade cost / invulnerability | 20 / 0.27 s |
| Perfect deflection window | 0.11 s |
| Echo: weak point | +8 |
| Echo: deflection | +10 |
| Echo: sever link | +12 |
| Echo: precision chain | +4 per additional target, capped |
| Basic ability cost band | 20–35 |
| Signature release cost | 100 |

## A.4 Encounter density

| Tier | Baseline visible count |
|---|---:|
| Full Tier A AI | 12–16 ordinary equivalents |
| Tier B support | 12–24 |
| Tier C Mass actors | 40–100 |
| Simultaneous melee attack slots | 2–4 ordinary, modified by difficulty/commander |
| Off-screen lethal anticipation bonus | +25–50% |

---

# Appendix B. Core Data Contracts

## B.1 Attack definition minimum fields

- ID and display/debug name;
- owner/protagonist/archetype permissions;
- input branch;
- montage/section and timing profile;
- movement/root-motion profile;
- trace set and hit ledger policy;
- damage/poise/status specification;
- guard class and interruption class;
- target correction;
- cost/cooldown where applicable;
- Echo gain/spend;
- tags owned/granted/required/blocked;
- VFX/audio/camera/haptic request IDs;
- AI use rules;
- accessibility annotations;
- telemetry ID;
- schema version.

## B.2 Encounter definition minimum fields

- encounter ID and mission beat;
- activation/completion/failure bounds;
- faction and role pools;
- phase graph;
- spawn/promotion anchors;
- attacker-slot policy;
- objectives and protected actors;
- difficulty/scalability variants;
- resource/checkpoint policy;
- music/intensity state;
- dialogue/bark hooks;
- consequence writes;
- performance budget;
- test route and telemetry ID.

## B.3 Dialogue node minimum fields

- graph/node/line ID;
- speaker/listener roles;
- localized text and audio;
- entry conditions;
- cinematic/staging reference;
- interruption/resume priority;
- choices and intent previews;
- state/consequence/evidence writes;
- next/fallback nodes;
- subtitle/caption metadata;
- localization and performance status;
- schema version.

## B.4 Checkpoint actor contract

- stable GUID;
- state version;
- capture policy;
- restore phase;
- compact serialized state;
- required asset dependencies;
- migration behavior;
- failure fallback;
- automation coverage.

---

# Appendix C. Mission Brief Template

## C.1 Identity

- Mission ID / working title:
- Manuscript chapter coverage:
- Lead protagonist:
- Companions:
- Locations:
- Target duration:
- Mission owner:

## C.2 Narrative contract

- Canon gate:
- Start knowledge/state:
- Exit knowledge/state:
- Player question:
- Relationship turn:
- Required evidence:
- Prohibited branches:

## C.3 Experience contract

- Protagonist fantasy:
- New mechanic or mastery:
- Faction language:
- Quiet beat:
- Spectacle beat:
- Expected session break:

## C.4 Beat table

| Beat ID | Duration | Play mode | Objective | State read/write | Checkpoint | Content/risk |
|---|---:|---|---|---|---|---|
| | | | | | | |

## C.5 Choice envelope

- prompt and intent;
- legal options;
- immediate outcome;
- witnesses;
- later consumers;
- canon reconciliation;
- failure/reload boundary.

## C.6 Content budget

- arenas and encounter roles;
- traversal interactions;
- new/reused environments;
- cinematics by tier;
- VO line estimate;
- animation/VFX/audio needs;
- unique enemies/bosses;
- optional content;
- performance risks;
- cut-down plan.

## C.7 Acceptance tests

- critical path;
- optional paths;
- canon/state;
- checkpoint/save;
- combat/AI;
- accessibility;
- performance;
- localization;
- cinematic skip/recovery.

---

# Appendix D. Enemy and Boss Specification Template

## D.1 Archetype identity

- ID/name/faction/role;
- player problem created;
- silhouette and audio signature;
- ordinary and elite variants;
- expected encounter partners;
- protagonist-specific counterplay.

## D.2 State and moves

| Move | Class | Range | Anticipation | Defense/counter | Damage/poise | Cooldown/conditions |
|---|---|---:|---:|---|---|---|
| | | | | | | |

## D.3 AI

- perception and information sharing;
- preferred distance/formation;
- attacker-slot behavior;
- objective behavior;
- response to mark, sever, challenge, cloak, guard, and poise break;
- retreat/surrender/corruption behavior;
- tier demotion/promotion rules.

## D.4 Production and test

- animation list;
- VFX/audio/caption cues;
- hit zones/collision;
- performance cost;
- accessibility substitutes;
- checkpoint state;
- exploit tests;
- telemetry questions.

---

# Appendix E. Canon Guardrails for Gameplay

1. The Dominion did not create or own the Echo; Dominion martial interpretation is doctrine.
2. Eclipse corruption threatens distinction, agency, and control; it is not generic violet damage.
3. Alethian alignment preserves freely chosen difference rather than imposing sameness.
4. Tarrik carries Velkorran and the Cinderline rifle; his combat language is committed, protective, and commanding.
5. Selene carries Verity, a circular double-sided Echo blade, and uses precision, viewmaker, cloak, and disruption systems.
6. Tarrik and Selene are authored people, not class shells.
7. Lyessa, Lyric, and Tharne are relationships, not party-role dispensers.
8. The Crownmarks and Witnesses are story machinery with specific causality, not repeatable dungeon keys.
9. Dominion and Reformation each embody genuine goods and dangerous evasions of responsibility.
10. The final containment failure arises through mutually reinforcing guardianship and restraint, not a simple villain button.
11. White armor and later alignment must preserve Tarrik's and Selene's distinct silhouettes and identities.
12. Fixed events may be made playable in method and consequence but not removed for branch count.

---

# Appendix F. Campaign Definition of Done

The campaign is done only when:

- all missions, hubs, prologue, and epilogue run from clean boot to credits;
- every canon gate matches the approved adaptation;
- Tarrik and Selene meet identity acceptance targets;
- combat, Echo, corruption, factions, companions, progression, narrative choice, and world systems pass their stated criteria;
- all supported platforms meet performance, stability, save, suspend/resume, and certification requirements;
- all accessibility settings work from first boot through credits;
- every spoken line has approved subtitle/caption coverage and localization status;
- no obsolete class, loot, XP, rarity, vendor, crafting, morality, or approval system remains in the campaign cook;
- save migration and checkpoint recovery are proven;
- high-severity defects meet release gate;
- legal, credits, privacy, and support materials are complete;
- source, symbols, build, data schema, and final review assets are archived;
- the game delivers the closing design test below.

---

# Closing Design Test

At every major review, ask:

> Does this feature, mission, or scene make the player more fully inhabit Tarrik and Selene as two distinct witnesses under pressure—and does it preserve the human consequence of what they choose?

If the answer is no, the work must justify its cost through a necessary technical, accessibility, or continuity function. If it does neither, it does not belong in *Sovereign Call: Origins*.
