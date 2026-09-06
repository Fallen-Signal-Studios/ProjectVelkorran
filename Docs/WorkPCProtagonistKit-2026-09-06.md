# Work PC protagonist kit integration — 2026-09-06

Both development protagonists now use project-owned ability configurations, input maps and loadouts with all ten native Echo abilities. Their existing primary weapon attacks, interaction and Narrative HUD remain available. This is a playable development kit; campaign missions and final Echo animation/art are not claimed complete.

## Reproduce and preserve

Build the complete UE 5.7.4 project with its Narrative plugin and existing art assets. Run the project framework setup first to create the copied player definitions and controller. Then, outside PIE, run `Scripts/Editor/verify_protagonist_setup_source_templates.py`; it executes the kit and projectile authoring scripts and checks all 14 loaded source templates before and after. It neither saves nor reloads originals to conceal changes. Reports default to `Saved/Validation/WorkPCSetup`; `VELKORRAN_SETUP_OUTPUT` may override that directory.

Next run the project controller UI setup. Run `probe_weapon_display_metadata_readonly.py` then `setup_project_weapon_display.py` outside PIE to configure canonical names and fill three confirmed missing icons from existing stock textures. For the HUD, enter PIE and wield a weapon, run `probe_combat_hud_layout_readonly.py`, end PIE, then run `setup_combat_hud_layout.py`. It requires the observed parent/slot structure, exports and checks visibility bindings through the official T3D exporter, preserves source fingerprints, and saves only the copied HUD/controller. Run `probe_weapon_presentation_health_readonly.py` then `setup_project_weapon_presentation_repairs.py` outside PIE for the two targeted weapon presentation corrections. A fresh editor session is required to visually verify saved widget changes.

The kit now imports adjacent `protagonist_equipment_order.py`. It verifies native first-free-slot rules and grants Verity before Staccato: Verity uses its only authored BackA slot, Staccato uses BackB, Axiom uses HipLeft. Every existing item and quantity is retained. Tarrik's Cinderline/Velkorran order already assigns BackA/HipLeft and stays unchanged. `probe_loadout_slot_rules_readonly.py` diagnoses those rules, and `setup_project_loadout_order.py` repairs an older generated copied loadout without rebuilding the kit.

The package manifest records all touched project packages, file sizes, current SHA-256 hashes, scripts, input/ability contracts and evidence hashes. Content is ignored by Git: retain the final asset bundle as well as these scripts. The manifest covers this kit's targets; the root task's broader bundle also covers copied menus, framework and development maps plus necessary dependencies. It is not a standalone engine or licensed-plugin distribution.

## Gameplay ownership and inputs

All ten Echo children and contextual defense abilities inherit current native gameplay classes without old Blueprint payload/cost/timer graphs. Tarrik's character grants Cinder Sticky Grenade; Velkorran grants Hunger and Cinder Slam; Cinderline grants Judgement and Requiem. Selene's character grants Stillpoint and Dispatch; Verity, Staccato and Axiom grant their corresponding weapon technique. Exact allowlists target copied item classes, and Dispatch references copied Verity for its returning visual.

Native Sprint/Evade and exertion components own movement costs and stamina regeneration. Only copied startup effects are changed: the periodic stamina-only effect is omitted, and the MaxStamina/StaminaRegenRate Override rows are removed after verifying native prototype defaults are enabled. All other attribute modifier exports are preserved. Tarrik's live maximum/rate is 120/32; Selene's native profile is 100/38. Health still resolves to 100 through ordinary attribute processing.

The kit removes null/duplicate grants and validates combined character-plus-weapon input contexts, including dual Axiom. Only copied Axiom opts into the symmetric exact-class dual-wield restriction; Verity+Axiom is unsupported because their primary and technique grants overlap. Existing ownership, slot, visual and hand checks still apply. Stock item defaults remain permissive unless explicitly opted in.

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Attack | Left mouse | Right trigger |
| Aim / contextual defense | Right mouse | Left trigger |
| Native evade | Q | Right stick click |
| Grenade / Ability1 | G | Right shoulder |
| Weapon technique / Ability2 | X | D-pad up |
| Signature / Ability3 | F | Top face button |
| Bash / heavy input | Middle mouse | D-pad down |
| Sprint | Left Shift | Left stick click |
| Cover | 1 | No dedicated binding |
| Weapon wheel | T | Left shoulder |

Existing interaction/reload, menus, movement, crouch and jump bindings remain. Mouse-wheel camera controls remain; D-pad up/down serve combat techniques.

## Presentation and observed validation

Native combat vitals show Health, Shield, Stamina, Poise and Echo from the current ready pawn's canonical components. The widget makes no gameplay writes, takes no input focus, clears on handoff/invalid ownership, and honors counted `WantsHideHUD` tags including child tags. UI scale/high contrast apply. It is opt-in on the copied controller, with a default of off elsewhere. Focused readiness/handoff and counted hide/resume tests cover its projection.

The copied Narrative HUD collapses only its old PlayerInfo resource group and moves the existing weapon/ammo branch to the lower right. Crosshair layout and ammo/weapon binding graphs remain unchanged. GUI 09 visually confirmed a readable five-resource panel on the left and canonical weapon names on the right. All five item display names are explicit; missing Velkorran/Verity/Cinderline icons use existing greatsword/sword/rifle silhouettes.

Cinder Grenade and Hunger use copied authored presentation on native children. Stillpoint, Wake and Dispatch use visible cyan prototype forms. Native code continues to own collision, sweeps, faction filtering, damage and completion. Existing primary attacks and Deflection montage presentation remain; new Echo choreography is still pending.

Live mapped input exercised all five Echo abilities for each protagonist with expected cost/activity checks, and applicable projectiles were observed. Selene primary checks consumed Axiom ammo 12→11 and Staccato ammo 4→3 after reload; both first- and third-person Verity attack montages were observed. Staccato's source and copied ClipSize are both 4, so that reload result is correct. Tarrik's Cinderline firing consumed ammo 32→28→25. These checks establish activation and resource behavior, not complete damage/animation/encounter acceptance.

Two actual presentation warnings led to narrow copied-asset corrections. Both source Staccato holster offsets had zero scale, and switching to unit-scale wielding passed infinity into physics. The copied item now uses unit scale for both holsters while preserving every other attachment field; original zero-scale source data remains untouched. Verity's animation property map contained exactly one Guard.Broken row with no target property. A copied overlay removes that ineffective row and preserves the other five exact mappings. A copied Verity weapon visual uses that overlay, and copied WI_Verity points to the visual. The ordinary kit rerun retains these corrections. Separate root reports record fresh wield/holster checks; no generic mesh physics was disabled.

The complete repaired authoring rerun preserved all 14 source templates with no dirty packages before or after. The first input-authoring attempt had exposed a live mutable struct view; both editor-saved and original-disk versions were preserved before the original was restored byte-for-byte. Current scripts read mutable mapping/loadout structs from project copies. Later HUD, metadata and loadout scripts independently verify their original source properties and file hashes.

Legacy Tarrik serialized-null component slots are narrowly repaired from exact native-named owned default subobjects. Nonnull mismatches and duplicate/non-native alternatives remain rejected. Both migration regressions passed, and fresh live Tarrik initialized Health/Shield/Poise at 100 with Stamina 120 and Echo 25.
