# Engine implementation handoff

Written 18 September 2026, revised 19 September.

Everything here is a **content and editor task**. The C++ side is finished, committed and covered by
tests; none of it does anything visible until the work below lands. Nothing in this document requires
source changes, and if a task seems to, stop and say so rather than editing C++ to fit the content.

Current at `eb6e28ab` on `codex/aurelion-tdd-content-20260913`. 719 automation tests pass:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -DisableAura
```

Run that **without** `-SkipBuild` at least once before you start and once when you finish. A
`-SkipBuild` run will not recompile untouched files, which is how an include-order break sat on the
branch green for an hour (fixed in `29d44739`).

## If you have an hour, do these two

The creator is playing the M12 slice and reporting what breaks. Two things in this list affect that
loop directly, and the rest can wait:

1. **Task 2, the HUD layout.** He is currently playing with no health, no shield, no ammo, no radar
   and no pips. Everything else he reports is filtered through flying blind.
2. **Task 4, the lethal-floor cue.** On 18 September the floor was widened from the Elite to the
   command-link carriers, so there are now several enemies that visibly refuse to die with no
   explanation at all. This fixed a worse bug - killing a carrier used to dead-end the encounter -
   but it traded an unexplained failure for an unexplained invulnerability, and he will hit it.

Tasks 3, 5, 6, 7 and 9 are real but nothing is waiting on them today.

## Read this first

> **Never open `BT_AurelionSecurityCrossfire` in the Behaviour Tree editor.** It is authored by
> script and exists only at runtime. Opening and saving it strips every task, decorator and EQS
> reference, and the enemies stop attacking. This has already happened once and cost a restore from
> `83a8c241~1`. The same caution applies to any asset a `Scripts/Editor/*.py` file writes.

Two other standing constraints:

- `*.uasset` and `*.umap` are gitignored by default. Anything you author that must be tracked needs
  an allowlist entry in `.gitignore`, as `Content/Input` and `Content/Aurelion` already have.
- The repository tracks source plus `Content/Aurelion` only. Marketplace content stays untracked; the
  customised Narrative plugin under `Plugins/` stays tracked.

## Tasks

| # | Task | Asset | What it unblocks |
|---|---|---|---|
| 1 | ~~Point the frontend at the HUD widget~~ **done** | `BP_SovPlayerController` | The entire holographic HUD |
| 2 | Lay out the HUD widget | `WBP_SovHolographicHUD` | Bars, ammo, radar, arc |
| 3 | Apply the camera state | `BP_SovTarrik`, `BP_SovSelene` | Aim and threat-focus framing |
| 4 | Author the lethal-floor cue | Both Aurelion phase directors | "Cannot be finished yet" reads on the Elite |
| 5 | Enter the decided gamepad chords | `IMC_Combat` | Threat focus on a controller |
| 6 | Author a shoulder-swap rig parameter | Narrative camera rig | Shoulder swap (blocked until this exists) |
| 7 | Raise `ContentRevision` when you revise a mission | Mission definitions | Saves surviving your content edits |
| 8 | Localization gather **run**; pseudo-localization still open | `Content/Localization` | Text that can be translated at all |
| 9 | Check whether the legacy melee abilities are still granted | Mission definitions | Two melee paths, or one |

---

### 1. Point the frontend at the HUD widget — done

`USovFrontendComponent::HolographicHUDSurfaceClass` was unset, so the HUD never spawned. It is now
set to `WBP_SovHolographicHUD` on `Content/Framework/BP_SovPlayerController.uasset`.

Done by script (`Scripts/Editor/bind_holographic_hud_surface.py`) rather than by hand, because the
frontend is a C++ default subobject and this is a default-value set — no graph was touched. The
script verifies the widget really derives from `USovHolographicHUDSurface` before assigning, and
re-reads the asset from disk afterwards, because an assignment that silently did not take looks
exactly like one that did until the HUD fails to appear. The readback reported `None` before and
`WBP_SovHolographicHUD_C` after.

That asset is gitignored, so this is an untracked change in the working tree. A backup of the
original was taken first and can be restored on request.

**Done when:** entering PIE shows the surface at all. It will look unfinished until task 2 — that is
expected, and it is the right order, because a surface that never spawns and a surface with nothing
in it fail identically from the outside.

### 2. Lay out the HUD widget

`WBP_SovHolographicHUD` is reparented to `USovHolographicHUDSurface` and already exists. The base
class feeds it; it does not compute anything itself.

Every binding is **optional** — name a widget to receive data, omit it to ignore that data. Names are
matched exactly:

| Widget name | Type | Receives |
|---|---|---|
| `PlateRegion` | `UPanelWidget` | Positioned into the plate layout rect |
| `AmmoRegion` | `UPanelWidget` | Positioned into the ammo layout rect |
| `ArcRegion` | `UPanelWidget` | Positioned into the arc layout rect |
| `RadarRegion` | `UPanelWidget` | Positioned into the radar layout rect |
| `HealthBar` | `UProgressBar` | Health fraction |
| `ShieldBar` | `UProgressBar` | Shield fraction |
| `StaminaBar` | `UProgressBar` | Stamina fraction |
| `EchoBar` | `UProgressBar` | Echo fraction |
| `AmmoText` | `UTextBlock` | Ammo readout |
| `ArcFill` | `UImage` | Scalar `Fill` on its dynamic material instance, from the Echo fraction |

The four `*Region` panels are placed automatically into the layout rectangles the rest of the
presentation already avoids, so subtitles and captions will not collide with the HUD. If you would
rather place them by hand, clear **Place Regions From Layout** on the widget's class defaults.

For anything the bindings do not cover, implement **On Holographic HUD Updated**. It hands you the
whole `FSovHolographicHUDView` — bars, pips, contacts, palette and layout rects — once per update.
`ArcFillParameter` defaults to `Fill`; change it on class defaults if your material uses another name.

Both palettes come from the view, so **author one layout, not two**. Tarrik is amber/gold/red and
Selene cyan/teal/white, and the surface supplies whichever applies.

**Done when:** both protagonists show correct bars in PIE, and the radar drops its contacts under
modifier blackout. Blackout is asserted in `ProjectVelkorran.UI.HolographicHUD.*`, so if the C++ side
regresses a test will say so — but only content can prove it looks right.

### 3. Apply the camera state

> **A checker is ready for this.** After wiring it, run:
> ```powershell
> .\Scripts\Validation\Aurelion\run-editor-script.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -ScriptPath 'Scripts\Editor\check_camera_wiring_readonly.py'
> ```
> Read-only. It reports whether `ApplyCameraState` is implemented on each protagonist and whether
> anything still references the camera interface or enums — the two-owner case described below. It
> reads the asset's references, not the graph, so treat a report as "look here", not a verdict.

This is the one with a real failure mode. Read the whole section before opening the Blueprint.

`USovCameraControlComponent` now decides who owns the camera. It is a default subobject named
**`SovCameraControl`** on `ASovPlayerCharacterBase`, so both protagonists already have one. Systems
take a lease; the strongest live claim wins; releasing falls back to the next claim down and
ultimately to the protagonist's profile, which is always present.

**It never moves a camera.** It calls `ApplyCameraState` and stops.

- Assets: `Content/PlayerCharacters/BP_SovTarrik.uasset`, `Content/PlayerCharacters/BP_SovSelene.uasset`
- Implement the component's **Apply Camera State** event. It gives you an `FSovCameraState`:

| Field | Meaning |
|---|---|
| `Mode` | `Unchanged` / `FreeCam` / `Strafe` — maps to `E_CameraMode` |
| `Style` | `Unchanged` / `Far` / `Balanced` / `Close` / `FirstPerson` — maps to `E_CameraStyle` |
| `Shoulder` | `Unchanged` / `Right` / `Left` — nothing consumes this yet, see task 6 |
| `Priority` | Who won: `Profile` / `Traversal` / `ThreatFocus` / `Aim` / `Finisher` / `Cinematic` |
| `Reason` | The claimant's own name, for diagnostics |

Route `Mode` and `Style` to `BPI_GameplayCamera`. The interface and both enums live at
`/NarrativePro/Pro/Core/Character/Biped/Camera/` — I confirmed that by loading them, but I could not
enumerate the interface's function signatures from Python, so read them in the editor rather than
trusting a name from me.

> **The failure mode:** whatever currently sets camera mode or style in those graphs must move onto
> this path. If the old logic stays and the new event is added alongside it, there are two owners
> again — which is the exact defect this component was built to remove, and it will look like an
> intermittent camera fight rather than a wiring mistake.

`ApplyCameraState` fires **only when the resolved state actually changes**, so you can drive authored
blends from it without them restarting every time a losing claim comes and goes.

The resting framing is already decided in C++ and needs no content: Tarrik `Far`, Selene `Balanced`,
both `FreeCam`, both right shoulder. If you want to override per protagonist, fill the **Profiles**
array on the component — an entry whose `Protagonist` tag matches wins, an entry with no tag is the
fallback, and either overrides the built-in completely.

**Done when:** raising the weapon pulls in and strafes; lowering it returns to the threat-focus
framing; dropping the focus returns to the protagonist's resting framing. If a camera ever sticks,
call **Describe Claims** on the component — it prints one line per live claim with its priority,
reason and handle, strongest first.

### 4. Author the lethal-floor cue

> **Raised in priority on 19 September.** The floor now applies to the command-link carriers as well
> as the Elite. That fixed a dead end - killing a carrier destroyed its link and failed the encounter
> outright - but it means several ordinary enemies now shrug off damage with nothing on screen saying
> why. Of everything in this document, this is the one most likely to be the next thing reported as
> broken. It is not broken; it is unexplained, which plays the same.


`USovLethalFloorComponent` holds the Aurelion Elite above a health floor while the phase mechanic is
still outstanding, so the player can no longer lose a run by out-damaging the boss. The rule works;
it is currently invisible.

- Author a GameplayCue for "held above a lethal floor / cannot be finished yet".
- Set **Floor Held Gameplay Cue Tag** on the lethal floor component used by both
  `ASovAurelionLinkPhaseDirector` and `ASovAurelionThermalPhaseDirector`. The property is filtered to
  the `GameplayCue` category. Leaving it unset plays nothing and is not an error.

The floor also adds `Sov.State.Target.Unfinishable` to the Elite while held, and `bFloorHeld`
replicates, so presentation can read the state directly instead of inferring it.

`EliteLethalFloorFraction` is `0.12` on both directors — 12% of current max health. Tune on the
director if the floor reads wrong in play; it clamps to 0.01–0.5.

**Done when:** hits still land and poise still breaks while the floor is held — it is a floor, not
invulnerability — but the Elite visibly cannot be finished, and the cue clears the moment the phase
mechanic resolves.

### 5. Enter the decided gamepad chords

> **Note on 19 September.** This design was settled before the interaction work landed on the 18th.
> The chord layout below still stands - the button map has not changed - but nothing has re-checked
> whether the weapon wheel claims the same face buttons, which is the open question flagged further
> down. Check that first; it decides whether the layer works at all.


The design is settled; entering it is about ten minutes in the Input editor. I could not script it —
see the note at the end — so `IMC_Combat` is **unmodified**.

All 19 gamepad inputs were already bound, both shoulders included, so targeting gets a **layer held
under the left shoulder** rather than buttons of its own. In `Content/Input/IMC_Combat.uasset`, add
four mappings, each carrying an **Input Trigger Chord Action** whose Chord Action is
`IA_WeaponWheel`:

| Hold LB, then | Action | Semantic tag |
|---|---|---|
| Y / FaceButton_Top | `IA_ThreatFocus` | `Narrative.Input.ThreatFocus` |
| X / FaceButton_Left | `IA_Designate` | `Narrative.Input.Designate` |
| D-pad Left | `IA_CycleTargetLeft` | `Narrative.Input.CycleTargetLeft` |
| D-pad Right | `IA_CycleTargetRight` | `Narrative.Input.CycleTargetRight` |

Then add an **Input Trigger Chord Blocker** to the *existing* mappings on those same four keys —
`IA_Ability3` (Y), `IA_Interact` and `IA_Reload` (X), `IA_OpenInventory` (D-pad Left),
`IA_QuickUseItems` (D-pad Right). Without the blocker the base action and the chorded one both fire,
which is not a layer, it is a double input. This does mean interact and reload are unavailable while
LB is held; that is what a chord layer costs.

**`IA_SkipCinematic` is deliberately left off the pad.** It fires during cinematics, where a chord
built on a combat weapon wheel is the wrong home for it. It wants its own context, which is a piece
of design rather than a binding.

> **Test the weapon wheel first.** LB opens the wheel, and I could not tell whether the wheel itself
> claims the face buttons — nothing in C++ references it, so it is handled entirely in Blueprint and
> the graph is not readable from script. If the wheel does claim them, the chord and the wheel's own
> selection will fight, and the fix is to move the wheel's selection or split the two by context.
> Check this before judging whether the layer feels right.

> **UE 5.7 note:** `UInputMappingContext::Mappings` is deprecated and reads empty; the live rows are
> in `DefaultKeyMappings.Mappings`. A script that writes the deprecated array reports success and
> changes nothing. `Scripts/Editor/inspect_gamepad_bindings_readonly.py` prints the live map.

> **Why this was not scripted:** an `InputTriggerChordAction` created inside the asset counts as a
> template as far as Python is concerned, and the editor then refuses to set its `ChordAction`.
> `Scripts/Editor/author_targeting_gamepad_chords.py` is kept because it encodes exactly the design
> above and fails before writing anything; if a way past the template guard turns up, the rest works.

**Done when:** holding LB and pressing Y takes a threat focus, the D-pad cycles, X designates, and
releasing LB returns every button to its normal job.

### 6. Author a shoulder-swap rig parameter

**20 September inspection update:** the shared `CameraRig_ThirdPerson` already
exposes a vector `Offset` bound to its final `OffsetCameraNode_0.TranslationOffset`.
Far, Balanced and Close aim rigs all override it with `(-100, 60, 60)` cm.
The missing integration is propagating resolved `Shoulder` into that lateral
offset, not absence of every usable rig parameter. The current camera director
export contains no shoulder or native camera-state reader. The exported private
variable ID is not a public runtime binding and must not be hardcoded as one.
See [the current rig inspection](AurelionShoulderRigInspection-2026-09-20.md).
Shoulder swapping remains unimplemented and unverified. The original note below
is retained as historical context rather than a current statement about the rig.

**Blocked, and deliberately so.** The camera arbiter already carries `Shoulder` in its resolved
state, but the authored rig has no parameter that can act on it, so nothing is bound to a key — a
bound key that visibly does nothing is worse than an unbound one.

When you author the rig or parameter, the remaining work is additive: no resolver change, no new
claim plumbing. Say so and the input and claim can be added in an hour.

---

### 7. Raise `ContentRevision` when you revise a mission

**This one is ongoing, not a one-off, and it is the task most likely to cost a player their save.**

A campaign save records a journal of what happened, and loading replays that journal against the
*current* mission definition demanding exact equality. So an edit to a mission that has already been
played — rewording a consequence, changing a beat's relationship memories or required protagonist,
removing a beat — invalidates every save taken in that mission. Until today there was no way to tell
that apart from a tampered file, and no way to carry a save across it at all.

`USovCampaignDefinition` now carries **Content Revision** (default 1). When you make a change of that
kind:

1. Raise `ContentRevision` by one on that mission definition.
2. Tell whoever is on the C++ side, so a migration is registered for that step. A migration is a small
   function that rewrites what the journal recorded into what the mission now authors — for a reworded
   consequence, it is a couple of lines.

If you raise the revision and no migration is registered, saves in that mission are refused — but
refused *legibly*: the player is told their save predates a change to this mission rather than that it
is damaged, and `GetRestoreFailure()` returns `MissionContentRevisionUnsupported` with the mission
named. That is the safe failure, not the goal.

If you change content **without** raising the revision, you get the old behaviour: saves silently
refuse and report as invalid, indistinguishable from corruption. Raising it costs nothing and is the
only signal the system has.

Additive changes are usually safe — a new optional beat nobody has reached, a new mission — because
the journal references beats by ID. Raise the revision anyway if you are unsure; an unnecessary
revision with no migration registered is caught immediately by the test suite, whereas a missed one is
found by a player.

### 8. Localization — the gather is run; the expansion check is still open

The project had **no localization pipeline at all**. `Config/Localization/Game.ini` now defines the
target, and **the gather has been run**: `Content/Localization/Game/` holds the manifest, the `en`
archive and a compiled `en/Game.locres`, from **897 gathered entries**. It can be re-run headlessly:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'F:\ProjectVelkorran\ProjectVelkorran.uproject' -run=GatherText -config=Config/Localization/Game.ini -unattended -nop4 -NullRHI -nosplash -DisablePlugins=Fab,Aura
```

Or from the editor: **Tools ▸ Localization Dashboard**, the **Game** target, **Gather Text** then
**Compile Text**.

**`Content/Localization/` is generated and gitignored, and has not been committed.** Whether it
belongs in the repository is the creator's call.

**Pseudo-localization is still to do, and my first attempt at it was wrong.** I configured `qla` as a
culture; it gathers and archives fine and then crashes `GenerateTextLocalizationResource` outright,
because Unreal applies pseudo-localization at runtime over a real culture rather than compiling it as
one of its own. The culture is removed from the config. Doing the expansion check properly needs the
engine's own mechanism, which I have not verified — please find it rather than trusting a guess from
me. It is worth doing, because it is what surfaces:

- **Any string that does not change** — a literal that escaped `LOCTEXT`, or an asset outside the
  gather paths. Those are the ones a translator would never see.
- **Any string that overflows or clips** — German and Russian run 30–40% longer than English.

**The gather already found nine real problems** without any of that: nine `Text conflict` warnings,
where two different strings share one namespace and key, so one silently wins and the other can never
be translated. All nine are in the Narrative plugin (`EnvQueryTest_AttackTokens`, `NarrativeItem`,
`NarrativeActorProvider`, `AssetTypeActions_NPCDefinition`, `NarrativeEditorSaveMenus`). They are
pre-existing and none are ours, but the plugin is customised and tracked, so they are fixable. Search
the gather log for `Text conflict` for the exact lines.

**Do not commit `Content/Localization/` without asking.** It is generated, it is large, and whether it
belongs in the repository is a call for the creator, not a default.

**What is deliberately not done yet**, so you do not think it is missing by accident: save and
settings error messages still reach the UI as raw `FString`, so they will not translate even after a
gather — converting those is source work and is queued. Width heuristics still assume 27 px per
character, which is wrong for any non-Latin script. Aurelion story cues still have no string-table
keys; giving them keys is content work and is worth doing while you are in there.

### 9. Check whether the legacy melee abilities are still granted

Small, and only worth doing because two live paths would be worse than either one.

Both protagonists' weapons now grant melee on the native framework: `WI_Velkorran` grants
`GA_Tarrik_MeleeLight` and `GA_Tarrik_MeleeHeavy`, `WI_Verity` grants `GA_Selene_MeleeLight` and
`GA_Selene_MeleeHeavy`, all four deriving from `SovGameplayAbility_Melee` with matching
`SovMeleeAttackDefinition` data. That is the right state and nothing needs changing there.

But the superseded abilities still exist and are still referenced:

| Legacy ability | Still referenced by |
|---|---|
| `GA_Attack_Melee_Sword_1H_Tarrik` (Narrative demo content) | `DA_M12_FireAndFrost`, `DA_M13_ContraryWitness`, `NWI_Velkorran` |
| `GA_SovVerityTwinAttack` | `DA_M12_FireAndFrost`, `DA_M13_ContraryWitness` |

Open those three assets and see what the references are for. If a mission definition grants them
alongside the weapon's abilities, a protagonist has two melee systems active at once — one with the
swept socket path, per-attack ledger, 0.22 s buffer and Echo receipts, and one without — and which
one answers an input is then a matter of activation order. If they are vestigial, clearing them keeps
`NWI_Velkorran` from being mistaken for the live weapon item later.

`Scripts/Editor/inspect_melee_grants_readonly.py` prints the current picture and is read-only.

## Hazards and things not to undo

**The golden save.** `Source/ProjectVelkorranTests/Fixtures/GoldenSaves/Campaign_Schema1_0.sovsave`
is a real save written by this build, kept as bytes. It is the only thing in the suite that can catch
a save-format change that leaves the writer and reader consistent while orphaning saves already on
disk. Regeneration is behind `SOV_REGENERATE_GOLDEN_SAVES=1` **on purpose** — a test that rewrites
its own fixture can never fail. If it starts failing, the answer is a migration and a new fixture, not
a regeneration.

**Uncommitted in the working tree.** These are the creator's and were deliberately left alone:

- `Content/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade.uasset` (modified)
- `Content/Aurelion/Maps/L_Aurelion_M12.umap` (modified)
- `Content/Characters/Animation/ProtagonistCasts/AM_Tarrik_CinderStickyGrenade_A1.uasset` (untracked)

**Aura is off** and should stay off — its indexing crashed the editor and it force-enables remote
Python. Pass `-DisableAura` to the validation script, as the commands here do.

## What changed on the source side since this was written

Landed, and relevant to you only where noted:

- **AR2-17** — a save from a newer build is no longer treated as damage and its bank is no longer
  reused. Nothing for you to do.
- **PC2-02** — Poise break, Fatal and Frozen now gate and interrupt *every* combat ability, including
  Blueprint ones, rather than only the two written in C++. **If an attack is meant to have super
  armour, clear `bHonoursCombatInterruptions` on it**; otherwise breaking its poise will now cancel it
  mid-animation, which is a real behaviour change for authored enemy attacks.
- **CN2-10** — mission content revisions, which is task 7 above.

- **AR2-06** — listing save slots no longer re-reads and re-deserializes every bank. Internal only.
- **UX2-08** — plural forms, culture-aware dates, grapheme-safe letterspacing on the identity plate,
  and the gather config, which is task 8 above.

**The adversarial audit's P1s are closed; the rest is not.** The 2026-09-17 audit carries 76 findings
across four files. All five P1s are now verified closed, but seventeen P2s are verified still open and
eleven P2s plus every P3 have not been looked at. Read
`Docs/AdversarialAudit-2026-09-17/Dispositions-2026-09-18.md` first — it records what each finding is
actually worth now, and the audit itself is a 17 September snapshot that was partly stale the day it
was filed.
