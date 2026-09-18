# Engine implementation handoff, 18 September 2026

Everything here is a **content and editor task**. The C++ side is finished, committed and covered by
tests; none of it does anything visible until the work below lands. Nothing in this document requires
source changes, and if a task seems to, stop and say so rather than editing C++ to fit the content.

Current at `HEAD` on `codex/aurelion-tdd-content-20260913`. 712 automation tests pass:

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -DisableAura
```

Run that **without** `-SkipBuild` at least once before you start and once when you finish. A
`-SkipBuild` run will not recompile untouched files, which is how an include-order break sat on the
branch green for an hour today (fixed in `29d44739`).

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
| 1 | Point the frontend at the HUD widget | `BP_SovPlayerController` | The entire holographic HUD |
| 2 | Lay out the HUD widget | `WBP_SovHolographicHUD` | Bars, ammo, radar, arc |
| 3 | Apply the camera state | `BP_SovTarrik`, `BP_SovSelene` | Aim and threat-focus framing |
| 4 | Author the lethal-floor cue | Both Aurelion phase directors | "Cannot be finished yet" reads on the Elite |
| 5 | Bind the five new inputs on gamepad | `IMC_Combat` | Threat focus on a controller |
| 6 | Author a shoulder-swap rig parameter | Narrative camera rig | Shoulder swap (blocked until this exists) |
| 7 | Raise `ContentRevision` when you revise a mission | Mission definitions | Saves surviving your content edits |

---

### 1. Point the frontend at the HUD widget

`USovFrontendComponent::HolographicHUDSurfaceClass` is a `TSoftClassPtr<USovHolographicHUDSurface>`
and is currently unset, so the HUD never spawns.

- Asset: `Content/Framework/BP_SovPlayerController.uasset`
- Set **Holographic HUD Surface Class** to `Content/Aurelion/UI/HUD/WBP_SovHolographicHUD`.

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

### 5. Bind the five new inputs on gamepad

Five input actions exist and are bound **keyboard and mouse only**, because every gamepad key was
already taken and I would not guess at a rebind:

| Action | Semantic tag |
|---|---|
| `IA_ThreatFocus` | `Narrative.Input.ThreatFocus` |
| `IA_CycleTargetLeft` | `Narrative.Input.CycleTargetLeft` |
| `IA_CycleTargetRight` | `Narrative.Input.CycleTargetRight` |
| `IA_Designate` | `Narrative.Input.Designate` |
| `IA_SkipCinematic` | `Narrative.Input.SkipCinematic` |

- Assets: `Content/Input/IMC_Combat.uasset`, `Content/Input/DA_CombatInputs.uasset`
- Decide the gamepad chords and add the mappings.

> **UE 5.7 note:** `UInputMappingContext::Mappings` is deprecated and reads empty. The live rows are
> in `DefaultKeyMappings.Mappings`. If you script this rather than doing it by hand, use the
> `default_key_mappings` accessors, and read the asset back to confirm the count — a script that
> writes the deprecated array reports success and changes nothing.

**Done when:** threat focus, cycling and designation work on a controller without breaking an
existing binding. Skip-cinematic is a hold, not a press.

### 6. Author a shoulder-swap rig parameter

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

Still to come, listed so you do not duplicate it: AR2-06 checkpoint reads off the game thread, UX2-08
localization.
