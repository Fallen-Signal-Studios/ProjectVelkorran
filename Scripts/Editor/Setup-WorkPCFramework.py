"""Author project-owned framework copies and two playable combat setup maps.

Run inside UE 5.7 with PythonScriptPlugin and EditorScriptingUtilities enabled.
Existing Narrative source assets and CombatGreybox remain the source templates.
This is combat integration, not completion of the campaign's authored missions.
"""
import gc
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Validation" / "WorkPCSetup" / "framework-setup.json"
OUT.parent.mkdir(parents=True, exist_ok=True)
FRAMEWORK = "/NarrativePro/Pro/Core/BP/Framework/"
CHARACTERS = "/NarrativePro/Pro/Core/Character/BP/"
DEFINITIONS = "/NarrativePro/Pro/Demo/Character/Definitions/Player/"
report = {"created": [], "configured": [], "heroes": {}, "campaign_missions_completed": False}

def asset(path):
    result = unreal.EditorAssetLibrary.load_asset(path)
    if not result:
        raise RuntimeError("Required asset missing: " + path)
    return result

def duplicate(source, destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return asset(destination)
    result = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if not result:
        raise RuntimeError("Could not duplicate " + destination)
    report["created"].append(destination)
    return result

def save(obj):
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + obj.get_path_name())
    report["configured"].append(obj.get_path_name())

def compile_bp(bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not bp.generated_class():
        raise RuntimeError("Blueprint has no generated class: " + bp.get_path_name())
    return unreal.get_default_object(bp.generated_class())

def tag(name):
    result = unreal.GameplayTag()
    if not result.import_text('(TagName="{}")'.format(name)):
        raise RuntimeError("Gameplay tag is not registered: " + name)
    return result

try:
    # Resolve dependencies and verify native identity before creating any assets.
    templates = {}
    for hero, native in [("Selene", unreal.SovSeleneCharacter), ("Tarrik", unreal.SovTarrikCharacter)]:
        bp = asset(CHARACTERS + "BP_Protagonist_" + hero)
        cdo = unreal.get_default_object(bp.generated_class())
        if not isinstance(cdo, native):
            raise RuntimeError(hero + " source needs native protagonist migration before setup")
        templates[hero] = bp
    for name in ("BP_NarrativePlayerController", "BP_NarrativePlayerState", "BP_NarrativeGameMode"):
        asset(FRAMEWORK + name)
    asset(DEFINITIONS + "CD_Selene")
    asset(DEFINITIONS + "CD_DefaultPlayer")

    pc = duplicate(FRAMEWORK + "BP_NarrativePlayerController", "/Game/Framework/BP_SovPlayerController")
    unreal.BlueprintEditorLibrary.reparent_blueprint(pc, unreal.SovPlayerController)
    pc_cdo = compile_bp(pc)
    original_pc = unreal.get_default_object(asset(FRAMEWORK + "BP_NarrativePlayerController").generated_class())
    for prop in ("default_mapping_context", "ability_input_mappings", "gameplay_hud_class", "look_action", "player_camera_manager_class"):
        original = original_pc.get_editor_property(prop)
        pc_cdo.set_editor_property(prop, original)
        if pc_cdo.get_editor_property(prop) != original:
            raise RuntimeError("Controller did not preserve " + prop)
    save(pc)

    ps = duplicate(FRAMEWORK + "BP_NarrativePlayerState", "/Game/Framework/BP_SovPlayerState")
    unreal.BlueprintEditorLibrary.reparent_blueprint(ps, unreal.SovPlayerState)
    compile_bp(ps)
    save(ps)

    for hero in ("Selene", "Tarrik"):
        pawn = duplicate(CHARACTERS + "BP_Protagonist_" + hero, "/Game/PlayerCharacters/BP_Sov" + hero)
        pawn_cdo = compile_bp(pawn)
        source_definition = DEFINITIONS + ("CD_Selene" if hero == "Selene" else "CD_DefaultPlayer")
        definition = duplicate(source_definition, "/Game/Characters/Definitions/PD_" + hero)
        tags = list(definition.get_editor_property("default_owned_tags").get_editor_property("gameplay_tags"))
        identity = tag("Sov.Character.Player." + hero)
        if identity not in tags:
            tags.append(identity)
        definition.set_editor_property("default_owned_tags", unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array(tags))
        pawn_cdo.set_editor_property("player_definition", definition)
        save(definition)
        compile_bp(pawn)
        save(pawn)

        mode = duplicate(FRAMEWORK + "BP_NarrativeGameMode", "/Game/Framework/BP_SovGameMode_" + hero)
        unreal.BlueprintEditorLibrary.reparent_blueprint(mode, unreal.SovCampaignGameMode)
        mode_cdo = compile_bp(mode)
        for prop, val in {"player_controller_class": pc.generated_class(), "player_state_class": ps.generated_class(),
                          "default_pawn_class": pawn.generated_class(), "player_definitions": [definition],
                          "initial_mission": None, "use_seamless_travel": False}.items():
            mode_cdo.set_editor_property(prop, val)
        compile_bp(mode)
        save(mode)

        map_path = "/Game/Maps/Development/L_" + hero + "Combat"
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if unreal.EditorAssetLibrary.does_asset_exist(map_path):
            opened = editor.load_level(map_path)
        else:
            opened = editor.new_level_from_template(map_path, "/Game/Maps/CombatGreybox")
            if opened:
                report["created"].append(map_path)
        if not opened:
            raise RuntimeError("Could not open map " + map_path)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
        starts = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
        if len(starts) != 1:
            raise RuntimeError("Expected one combat-map PlayerStart")
        starts[0].set_editor_property("player_start_tag", unreal.Name("CombatEntry_" + hero))
        if not editor.save_current_level():
            raise RuntimeError("Could not save map " + map_path)
        report["heroes"][hero] = {"pawn": pawn.get_path_name(), "definition": definition.get_path_name(), "game_mode": mode.get_path_name(), "map": map_path}
        starts = []
        world = None
        gc.collect()

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Maps/Development/L_SeleneCombat")
    report["status"] = "authored; Blueprint compiler output and PIE readiness must be verified"
except Exception as exc:
    report["error"] = str(exc)
    raise
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("VELKORRAN_FRAMEWORK_SETUP_REPORT " + str(OUT))
