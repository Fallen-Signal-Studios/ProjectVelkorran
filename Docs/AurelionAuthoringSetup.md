# Aurelion first-encounter authoring setup

This setup prepares a **development Tarrik corridor encounter**, backed by the native Aurelion mission and encounter contracts. The full canonical M12/M13 assets are created separately as incomplete scaffolds. The script never assigns those scaffolds to the playable GameMode or manufactures story completion.

For the full adopted layout, start with [AurelionLayoutContract-2026-09-07.md](AurelionLayoutContract-2026-09-07.md) and its machine-readable manifest. **Build shared zones Z05–Z12 before entrances Z00–Z04.** This script remains an optional technical harness and does not construct the thirteen-zone layout, canonical E1/E2/E3/E4 rosters, wave producers, receivers, choice support or story actors.

The source/configuration changes can be reviewed and tested without Unreal. Creating the assets, compiling the Editor target, validating actual NPC readiness, and playing the route require the full work-PC checkout. No Unreal execution is claimed by the host configuration tests.

## Morning preparation

1. Pull the branch and build `ProjectVelkorranEditor` with the Editor closed. The setup requires the new reflected Aurelion mission classes, `SovCampaignEncounterObjective` and the existing Blueprint authoring helper.
2. Copy `Scripts/Editor/Manifests/AurelionPreparation.example.json` to a local path such as `Saved/Validation/AurelionPreparation.local.json`. The example intentionally has missing actor names, NPC definitions and positions; it is usable for inventory and cannot author a pretend encounter.
3. Save or discard existing dirty maps and content. Stop PIE. Set `VELKORRAN_AURELION_CONFIG` to the absolute local config path and `VELKORRAN_AURELION_MODE` to `inventory` in Unreal's Python environment. Execute `Scripts/Editor/setup_aurelion_slice.py`.
4. Read `Saved/Validation/AurelionSetup/inventory-report.json`. It records exact actor **object names**, display labels, classes, NPC definitions, PlayerStarts and spawners. Copy object names into the config, never labels.
5. Prepare a dedicated **source template copy** outside the two fixed output folders below. Use a simple persistent-level map, one PlayerStart, placed instances of the existing role classes derived from `SovNPCCharacterBase`, and no NPC spawners, campaign terminals, existing encounter directors or encounter bridges. Do not edit the original combat maps to satisfy this step. The existing combat template is a useful inventory starting point; it is not automatically an acceptable encounter template.
6. For each placed role character, assign **Authored Placed Definition** to its actual existing Narrative NPC definition in the Details panel. Use the matching class from that definition so existing Hound, Handler or other role behavior is preserved. This opt-in native property initializes through Narrative at runtime and preserves a definition already assigned by encounter recovery. An ordinary placed character with this field unset does not acquire a definition automatically. No reparenting is required. Bind every placed Sov NPC in the config. Require at least one enemy and two protected survivors, one from each Dominion/Reformation background. Supply each character's actual existing NPC-definition package path. `background` records the intended story background only; it does **not** set factions, behavior trees, materials, allegiance, health or abilities. Those are real content authoring work.
7. Set absolute map-space positions for arrival and secure-route terminals and an axis-aligned start volume. Rotation arrays are `[pitch, yaw, roll]`; extents are half sizes in Unreal units. The entry volume must be separated from both the PlayerStart and arrival terminal by at least a 100-unit margin. Keep the playable path obvious and the terminals accessible.
8. Run host validation, then switch `VELKORRAN_AURELION_MODE` to `apply` and execute the setup script again. Host validation checks configuration only; the Editor separately checks asset/actor identity before writing output.

```powershell
python Scripts/Editor/setup_aurelion_slice.py --validate-config Saved/Validation/AurelionPreparation.local.json
```

For an already-open Editor, the environment values can be set from its Python console before executing the script through **Tools → Execute Python Script**:

```python
import os
os.environ["VELKORRAN_AURELION_CONFIG"] = r"F:\ProjectVelkorran\Saved\Validation\AurelionPreparation.local.json"
os.environ["VELKORRAN_AURELION_MODE"] = "inventory"
```

Replace the example drive/path with the real checkout path. Use `apply` only after the authored config and template are complete.

**Stage the actors so combat cannot start before entry capture.** Participants must be ready and quiescent when the player enters the trigger. Enemy perception, ranged attacks, survivor hostility or an active player ability can correctly block capture. Use appropriate source-template staging, occlusion, readiness and faction setup. The script checks identity and geometry separation; it does not prove navigation, line of sight, AI readiness or a safe combat layout.

## Fixed outputs and scope

| Output | Package |
|---|---|
| Playable preparation map | `/Game/Maps/Development/Aurelion/L_TarrikPreparation` |
| Three-beat development mission | `/Game/Campaign/Development/Aurelion/DA_TarrikPreparation` |
| Copied project GameMode | `/Game/Campaign/Development/Aurelion/BP_TarrikPreparationGameMode` |
| Incomplete canonical M12 scaffold | `/Game/Campaign/Development/Aurelion/Scaffolds/DA_M12_FireAndFrost` |
| Incomplete canonical M13 scaffold | `/Game/Campaign/Development/Aurelion/Scaffolds/DA_M13_ContraryWitness` |

The technical mission is `M12_AurelionTarrikPreparation`. Its arbitrary protected-survivor roster is neither the six-drone E1 nor the authored E3 rescue. Its exact progression is:

1. `TarrikArrival`: operate the first terminal. This does **not** write a checkpoint before the encounter owns and freezes its participants.
2. `HoldMixedSurvivorCorridor`: cross the encounter-entry volume. The native bridge captures entry, then `BeginEncounter` writes its verified ArenaEntry checkpoint before starting combat. Its director is `M12_MixedSurvivorCorridor`. Enemies are required for victory; protected survivors are registered non-victory participants. A protected death fails the attempt. Success comes from the registered encounter, never a generic terminal.
3. `SecureTarrikRoute`: operate the secure-route terminal after actual encounter success and write the post-encounter checkpoint.

Leaving and re-entering the start volume after a failed attempt invokes the bridge's native retry path if the player and campaign remain eligible. Player death may instead enter the existing fatal-recovery/checkpoint flow. Verify both paths in Unreal.

No companion rescue, Mass conversion, local-choice consequence, Selene handoff, command formation coordination, Resonance, witness, assent or evidence exchange is authored into this first technical map. Selecting an actual coordinated enemy formation and configuring its link/weak-point behavior remains content integration work; defeating an arbitrary configured enemy roster is not proof of formation-breaking gameplay. Canonical scaffolds retain their guarded native definitions but lack real profiles, maps, scenes and evidence. Their validation is expected to fail until that content is supplied. They are deliberately excluded from the preparation-only mission manifest.

## Preservation and reruns

Setup refuses dirty packages, source/output aliases, unknown participant identities, empty victory rosters, unsupported external-actor/sublevel maps, and pre-existing encounter producers in the source template. It loads source assets read-only, fingerprints the source Blueprints, hashes explicit source packages before and after authoring, and only saves the five fixed output packages.

A rerun with the same config and all output ownership stamps **preserves existing output without modifying it**. Matching stamps are not gameplay or asset-validation receipts. If the config changed or only partial outputs exist, setup stops and reports the packages. Inspect and back up any useful work; move or delete only the listed generated outputs through Unreal before rerunning. The tool does not delete assets or silently rebuild over hand-authored changes.

The updated full M12/M13 constructor order may intentionally reject canonical assets serialized against the earlier preparation contract. Inspect and back up only those owned `Scaffolds` assets; explicitly reconcile their properties or recreate them from the revised native classes. Preserve content-bound scene/evidence work. Do not delete the unchanged technical map or use this script's all-output stamp check as a canonical asset migration.

A failure can leave partial generated assets, including a map created by Unreal's template-copy operation. Inspect the saved report before restarting. Original combat/framework assets are inputs and must remain unchanged.

## Qualification and content handoff

After successful authoring, inspect `Saved/Validation/AurelionSetup/apply-report.json` and `preparation-mission-manifest.json`. The report intentionally leaves `gameplay_qualified`, `canonical_slice_complete` and `unreal_build_verified` false. Its manifest includes the development mission only.

The next Editor session must establish:

- A fresh load retains the copied GameMode, mission, exact participant references and source preservation.
- Actual participant readiness, factions, AI attack initiation and navigation work. The earlier intermittent AI startup issue is not fixed by this authoring script.
- Arrival interaction works; crossing the entry trigger starts only the eligible nonempty encounter and produces the entry checkpoint.
- Protected-survivor death fails; retry reconstructs the correct participants and does not duplicate rewards or objective journal entries.
- Killing the required enemies while both survivors live completes the hold exactly once. Early use of the secure terminal cannot skip the encounter.
- The secure-route checkpoint reloads correctly. Test player death/recovery separately from a surviving player's explicit encounter retry.
- Both keyboard and controller input work in the packaged route, with captured build/automation/preflight results.

`Content/` and Unreal binaries are excluded from Git. Preserve the resulting content files and report together in the work-PC content overlay or existing binary backup process. A source-only pull elsewhere cannot load these newly authored assets. The script and manifest are reproducible preparation, not a substitute for the full authored content or full 50–60-minute Aurelion slice.
