"""Create a small technical-review campaign route in UE 5.7; run manually with PIE stopped.

Requires the existing WorkPC framework/native kits plus SovCampaignInteractionTerminal
and the editor-only SovReviewRouteAuthoringLibrary. Only TDDReview copies are saved.
This authors ordinary review objectives, not canonical M12/M13 story or a full slice.
"""
import gc
import difflib
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT", str(
    Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Validation" / "TDDReviewSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "tdd-review-route-setup.json"
DATA = "/Game/Campaign/Development/TDDReview/"
MAPS = "/Game/Maps/Development/TDDReview/"
TABLE = DATA + "ST_ReviewObjectives"
HEROES = ("Tarrik", "Selene")
STRINGS = {
    "TarrikTitle": "Technical review - Tarrik checkpoint route",
    "SeleneTitle": "Technical review - Selene checkpoint route",
    "TarrikCheckpoint": "Use the checkpoint terminal ahead.",
    "TarrikTravel": "Use the route terminal to save and open Selene's review.",
    "SeleneCheckpoint": "Use the terminal ahead to save this review checkpoint.",
    "CheckpointAction": "Record review checkpoint",
    "TravelAction": "Save and open Selene review",
}
report = {"status": "preflight", "created": [], "saved": [], "maps": {}, "missions": {},
          "canonical_campaign_complete": False, "same_map_handoff_qualified": False,
          "live_input_and_travel_qualified": False, "prototype_only": True}
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.AssetToolsHelpers.get_asset_tools()
source_objects = {}
source_memory = {}
source_disk = {}


def package_path(obj):
    return obj.get_path_name().split(".")[0]


def required(path):
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if obj is None:
        raise RuntimeError("Required asset missing: " + path)
    return obj


def owned(path):
    if not path.startswith((DATA, MAPS)):
        raise RuntimeError("Refusing mutation outside technical-review folders: " + path)


def save(obj):
    package = package_path(obj)
    owned(package)
    assert_sources_unchanged()
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + package)
    report["saved"].append(package)


def duplicate(source, destination):
    owned(destination)
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return required(destination)
    result = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if result is None or result == required(source):
        raise RuntimeError("Distinct project copy was not created: " + destination)
    report["created"].append(destination)
    return result


def new_asset(path, native_class, factory):
    owned(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        result = required(path)
    else:
        directory, name = path.rsplit("/", 1)
        result = assets.create_asset(name, directory, native_class, factory)
        if result is None:
            raise RuntimeError("Could not create " + path)
        report["created"].append(path)
    if not isinstance(result, native_class):
        raise RuntimeError("Unexpected asset class at " + path)
    return result


def tag(name):
    result = unreal.GameplayTag()
    if not result.import_text('(TagName="' + name + '")'):
        raise RuntimeError("Missing native identity tag: " + name)
    return result


def text(key):
    value = unreal.TextLibrary.text_from_string_table(unreal.Name(TABLE + ".ST_ReviewObjectives"), key)
    if not unreal.TextLibrary.text_is_from_string_table(value):
        raise RuntimeError("Objective did not retain its string table: " + key)
    return value


def hashes():
    # Framework, kits, UI, and original combat maps are inputs, never writable outputs.
    folders = ("Framework", "PlayerCharacters", "Characters/Definitions", "Abilities", "Items", "Input", "UI", "Weapons", "CharactersAnimation", "Effects")
    files = []
    for folder in folders:
        base = ROOT / "Content" / folder
        if base.exists():
            files.extend(base.rglob("*.uasset"))
    for hero in HEROES:
        files.append(ROOT / "Content/Maps/Development" / ("L_" + hero + "Combat.umap"))
    files.append(ROOT / "Content/Maps/CombatGreybox.umap")
    return {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(set(files)) if p.is_file()}


def assert_sources_unchanged():
    memory_after = {path: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj)
                    for path, obj in source_objects.items() if isinstance(obj, unreal.Blueprint)}
    report["source_memory_snapshot_after"] = memory_after
    report["source_memory_diffs"] = {path: list(difflib.unified_diff(source_memory[path].splitlines(), after.splitlines(),
        fromfile="before", tofile="after", lineterm="")) for path, after in memory_after.items() if after != source_memory[path]}
    report["source_memory_unchanged"] = memory_after == source_memory
    report["source_disk_unchanged"] = hashes() == source_disk
    if not report["source_memory_unchanged"] or not report["source_disk_unchanged"]:
        raise RuntimeError("Source preservation check failed; no additional packages will be saved")


def current_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def assert_current_review_world(destination):
    owned(destination)
    world = current_world()
    if world is None or package_path(world) != destination or editor.is_in_play_in_editor():
        raise RuntimeError("Current editor world is not the exact writable review map: " + destination)
    return world


def marker(label, message, location, rotation, size=12.0):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, location, rotation)
    if actor is None:
        raise RuntimeError("Could not place technical-review sign")
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(unreal.Text(message))
    component.set_world_size(size)
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return actor


def terminal(label, mission, beat_id, terminal_id, action_key, location, rotation, destination=None):
    actor = actors.spawn_actor_from_class(unreal.SovCampaignInteractionTerminal, location, rotation)
    if actor is None:
        raise RuntimeError("Could not place native campaign terminal")
    actor.set_actor_label(label)
    for prop, value in {"terminal_id": unreal.Name(terminal_id), "mission_id": mission.get_editor_property("mission_id"),
                        "completion_beat": unreal.Name(beat_id), "destination_mission": destination,
                        "write_checkpoint": True, "action_text": text(action_key)}.items():
        actor.set_editor_property(prop, value)
    visual = actor.get_editor_property("visual")
    visual.set_static_mesh(required("/Engine/BasicShapes/Cube"))
    visual.set_relative_scale3d(unreal.Vector(.7, 1.1, 1.3))
    label_component = actor.get_editor_property("label")
    label_component.set_text(text(action_key))
    label_component.set_world_size(8.0)
    interactable = actor.get_editor_property("interactable")
    if abs(interactable.get_editor_property("interaction_time") - .35) > .001:
        raise RuntimeError("Native authored hold duration changed")
    actual_location = actor.get_actor_location()
    actual_rotation = actor.get_actor_rotation()
    return {"actor": actor.get_path_name(), "terminal_id": terminal_id, "beat_id": beat_id,
            "destination": package_path(destination) if destination else None,
            "hold_seconds": .35, "checkpoint": True,
            "requested_location": [location.x, location.y, location.z],
            "location": [actual_location.x, actual_location.y, actual_location.z],
            "rotation": {"pitch": actual_rotation.pitch, "yaw": actual_rotation.yaw, "roll": actual_rotation.roll},
            "label_world_size": label_component.get_editor_property("world_size")}


try:
    if editor.is_in_play_in_editor():
        raise RuntimeError("Stop Play In Editor before authoring the review route")
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report["initial_dirty_maps"] = [package.get_path_name() for package in dirty_maps]
    if dirty_maps:
        raise RuntimeError("Save or discard the existing dirty map explicitly before review authoring; no map switch attempted")
    if not hasattr(unreal, "SovReviewRouteAuthoringLibrary"):
        raise RuntimeError("Compile the scoped editor string-table helper first")
    source_disk = hashes()
    report["source_sha256_before_warmup"] = source_disk
    for hero in HEROES:
        for path in ("/Game/Framework/BP_SovGameMode_" + hero,
                     "/Game/PlayerCharacters/BP_Sov" + hero,
                     "/Game/Characters/Definitions/PD_" + hero):
            source_objects[path] = required(path)
        required("/Game/Maps/Development/L_" + hero + "Combat")
    source_objects["/Game/Framework/BP_SovPlayerController"] = required("/Game/Framework/BP_SovPlayerController")
    source_objects["/Game/Framework/BP_SovPlayerState"] = required("/Game/Framework/BP_SovPlayerState")
    # Materialize generated defaults and native component templates before the baseline.
    # These are read-only loads, not Blueprint compilation or source-package saves.
    for source_obj in source_objects.values():
        if isinstance(source_obj, unreal.Blueprint):
            source_cdo = unreal.get_default_object(source_obj.generated_class())
            if isinstance(source_cdo, unreal.Actor):
                source_cdo.get_components_by_class(unreal.ActorComponent)
    source_obj, source_cdo = None, None
    memory_before_collection = {path: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj)
                               for path, obj in source_objects.items() if isinstance(obj, unreal.Blueprint)}
    report["source_memory_snapshot_before_warmup_collection"] = memory_before_collection
    # Python GC only drops wrappers. This native Python API runs Unreal GC now;
    # SystemLibrary.collect_garbage() instead queues it until the end of the frame.
    # Retire detached load/compiler objects before establishing the strict baseline.
    gc.collect()
    unreal.collect_garbage()
    if hashes() != source_disk:
        raise RuntimeError("Source disk changed during read-only warmup or garbage collection")
    source_memory = {path: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj)
                     for path, obj in source_objects.items() if isinstance(obj, unreal.Blueprint)}
    report["source_warmup_collection_diffs"] = {path: list(difflib.unified_diff(before.splitlines(), source_memory[path].splitlines(),
        fromfile="before_warmup_collection", tofile="strict_baseline", lineterm=""))
        for path, before in memory_before_collection.items() if before != source_memory[path]}
    report["source_warmup_collection"] = "Synchronous unreal.collect_garbage after read-only source/CDO loads; source disk unchanged."
    report["source_sha256_before"] = source_disk
    report["source_memory_snapshot_before"] = source_memory
    report["source_memory_before"] = {path: hashlib.sha256(value.encode()).hexdigest() for path, value in source_memory.items()}

    # Duplicate map packages before assigning mission soft references. Never open a source for mutation.
    for hero in HEROES:
        destination = MAPS + "L_" + hero + "Review"
        if not unreal.EditorAssetLibrary.does_asset_exist(destination):
            gc.collect()
            if not editor.new_level_from_template(destination, "/Game/Maps/Development/L_" + hero + "Combat"):
                raise RuntimeError("Could not copy review map: " + destination)
            report["created"].append(destination)
        gc.collect()

    table = new_asset(TABLE, unreal.StringTable, unreal.StringTableFactory())
    result = unreal.SovReviewRouteAuthoringLibrary.set_review_objective_strings(table, STRINGS)
    report["string_table_authoring"] = {"succeeded": result.get_editor_property("succeeded"), "report": result.get_editor_property("report")}
    if not result.get_editor_property("succeeded"):
        raise RuntimeError(result.get_editor_property("report"))
    for key, value in STRINGS.items():
        if unreal.StringTableLibrary.get_table_entry_source_string(unreal.Name(TABLE + ".ST_ReviewObjectives"), key) != value:
            raise RuntimeError("String-table readback mismatch: " + key)
    save(table)
    missions = {}
    for hero in HEROES:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.SovCampaignDefinition)
        mission = new_asset(DATA + "DA_" + hero + "Review", unreal.SovCampaignDefinition, factory)
        mission_id = "M12_" + hero + "EntryReview"
        entries = [("ReviewCheckpoint", hero + "Checkpoint", [])]
        if hero == "Tarrik":
            entries.append(("TravelToSelene", "TarrikTravel", [unreal.Name("ReviewCheckpoint")]))
        beats = []
        for beat_id, key, prerequisites in entries:
            beat = unreal.SovCampaignBeatDefinition()
            for prop, value in {"beat_id": unreal.Name(beat_id), "required_protagonist": tag("Sov.Character.Player." + hero),
                                "objective_text": text(key), "prerequisite_beats": prerequisites}.items():
                beat.set_editor_property(prop, value)
            beats.append(beat)
        for prop, value in {"mission_id": unreal.Name(mission_id), "display_name": text(hero + "Title"),
                            "protagonist": tag("Sov.Character.Player." + hero), "alternate_protagonists": [],
                            "completes_campaign": False, "pawn_class": source_objects["/Game/PlayerCharacters/BP_Sov" + hero].generated_class(),
                            "player_definition": source_objects["/Game/Characters/Definitions/PD_" + hero],
                            "map": required(MAPS + "L_" + hero + "Review"),
                            "entry_player_start_tag": unreal.Name("TDDReview_" + hero),
                            "allowed_successor_missions": [unreal.Name("M12_SeleneEntryReview")] if hero == "Tarrik" else [],
                            "required_prior_consequence_ids": [], "beats": beats, "choice_groups": [],
                            "allow_joint_resonance": False, "entry_echo_reserve": 25.0}.items():
            mission.set_editor_property(prop, value)
        validation_error = mission.validate_definition()
        # Unreal's bool + single output wrapper returns the string on success, None on failure.
        if validation_error is None:
            raise RuntimeError("Native mission definition rejected: " + mission_id)
        missions[hero] = mission
        report["missions"][hero] = {"asset": package_path(mission), "mission_id": mission_id,
                                     "objective_ids": [row[0] for row in entries], "native_validation": str(validation_error),
                                     "string_table_backed": True, "campaign_completion": False}
        save(mission)

    for hero in HEROES:
        source = source_objects["/Game/Framework/BP_SovGameMode_" + hero]
        mode = duplicate(package_path(source), DATA + "BP_" + hero + "ReviewGameMode")
        if mode.generated_class() == source.generated_class():
            raise RuntimeError("Review GameMode aliases the source class")
        cdo = unreal.get_default_object(mode.generated_class())
        cdo.set_editor_property("initial_mission", missions[hero])
        cdo.set_editor_property("use_seamless_travel", False)
        compile_result = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([mode], [source], [mode])
        report["missions"][hero]["game_mode_compilation"] = compile_result.get_editor_property("report")
        if not compile_result.get_editor_property("succeeded"):
            raise RuntimeError("Review GameMode compilation failed")
        cdo = unreal.get_default_object(mode.generated_class())
        if cdo.get_editor_property("initial_mission") != missions[hero]:
            raise RuntimeError("GameMode lost initial mission during compilation")
        assert_sources_unchanged()
        save(mode)
        destination = MAPS + "L_" + hero + "Review"
        gc.collect()
        if not editor.load_level(destination):
            raise RuntimeError("Cannot open review map: " + destination)
        world = assert_current_review_world(destination)
        world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
        removed = []
        removed_counts = {"NPCSpawner": 0, "SovDominionPackCoordinator": 0, "ReviewDecoration": 0}
        # This quiet route has no enemy spawners. Remove their copied pack owner first,
        # so the saved review map cannot retain a coordinator with a missing HandlerSpawner.
        assert_current_review_world(destination)
        for actor in sorted(actors.get_all_level_actors(), key=lambda item: 0 if isinstance(item, unreal.SovDominionPackCoordinator) else 1):
            if not actor.get_path_name().startswith(world.get_path_name() + ":"):
                raise RuntimeError("Actor inventory escaped the current review world")
            removal_kind = ("SovDominionPackCoordinator" if isinstance(actor, unreal.SovDominionPackCoordinator)
                            else "NPCSpawner" if isinstance(actor, unreal.NPCSpawner)
                            else "ReviewDecoration" if actor.get_actor_label().startswith("TDDReview_") else None)
            if removal_kind:
                assert_current_review_world(destination)
                removed.append({"name": actor.get_name(), "path": actor.get_path_name(), "label": actor.get_actor_label(),
                                "class": actor.get_class().get_path_name(), "kind": removal_kind})
                if not actors.destroy_actor(actor):
                    raise RuntimeError("Could not remove copied review-lane actor")
                removed_counts[removal_kind] += 1
        actor = None
        remaining_encounter_counts = {
            "NPCSpawner": sum(isinstance(item, unreal.NPCSpawner) for item in actors.get_all_level_actors()),
            "SovDominionPackCoordinator": sum(isinstance(item, unreal.SovDominionPackCoordinator) for item in actors.get_all_level_actors()),
        }
        if any(remaining_encounter_counts.values()):
            raise RuntimeError("Quiet review map still contains encounter spawners or their pack coordinator")
        starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
        if len(starts) != 1:
            raise RuntimeError("Review map requires exactly one explicit PlayerStart")
        start = starts[0]
        start.set_editor_property("player_start_tag", unreal.Name("TDDReview_" + hero))
        origin = start.get_actor_location()
        # The original combat entry faces the encounter pack; this quiet review lane
        # follows the actual bridge along X, between its Y=480 and Y=1020 fences.
        original_entry_rotation = start.get_actor_rotation()
        start.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), teleport_physics=False)
        rotation = start.get_actor_rotation()
        if abs(rotation.pitch) > .0001 or abs(rotation.yaw) > .0001 or abs(rotation.roll) > .0001:
            raise RuntimeError("Copied review PlayerStart did not accept its bridge-aligned orientation")
        forward = unreal.MathLibrary.get_forward_vector(unreal.Rotator(pitch=0.0, yaw=rotation.yaw, roll=0.0))
        if abs(forward.z) > .0001 or abs(forward.x * forward.x + forward.y * forward.y - 1.0) > .0001:
            raise RuntimeError("Review placement heading is not a horizontal unit vector")
        right = unreal.Vector(-forward.y, forward.x, 0.0)
        facing = unreal.Rotator(pitch=0.0, yaw=rotation.yaw + 180.0, roll=0.0)
        if abs(facing.pitch) > .0001 or abs(facing.roll) > .0001:
            raise RuntimeError("Review terminal and sign rotation is not upright")
        assert_current_review_world(destination)
        checkpoint_position = origin + forward * 230.0 + unreal.Vector(0.0, 0.0, -25.0)
        placed = [terminal("TDDReview_Checkpoint", missions[hero], "ReviewCheckpoint", hero + "Review_Checkpoint",
                           "CheckpointAction", checkpoint_position, facing)]
        if hero == "Tarrik":
            placed.append(terminal("TDDReview_Travel", missions[hero], "TravelToSelene", "TarrikReview_ToSelene",
                                   "TravelAction", origin + forward * 500.0 + right * 160.0 + unreal.Vector(0.0, 0.0, -25.0),
                                   facing, missions["Selene"]))
        sign_position = origin + forward * 850.0 - right * 250.0 + unreal.Vector(0.0, 0.0, 150.0)
        sign = marker("TDDReview_PrototypeSign", "Technical review route - prototype\n" + hero + " checkpoint segment\nInteract with the terminal\nNo canonical story completion",
                      sign_position, facing)
        sign_location = sign.get_actor_location()
        sign_rotation = sign.get_actor_rotation()
        assert_sources_unchanged()
        assert_current_review_world(destination)
        if not editor.save_current_level():
            raise RuntimeError("Could not save review map")
        report["saved"].append(destination)
        report["maps"][hero] = {"map": destination, "game_mode": package_path(mode), "removed_copied_actors": removed,
                                 "removed_actor_counts": removed_counts, "remaining_encounter_counts": remaining_encounter_counts,
                                 "terminals": placed, "entry_tag": "TDDReview_" + hero,
                                 "prototype_sign": {"actor": sign.get_path_name(), "requested_location": [sign_position.x, sign_position.y, sign_position.z],
                                     "location": [sign_location.x, sign_location.y, sign_location.z],
                                     "rotation": {"pitch": sign_rotation.pitch, "yaw": sign_rotation.yaw, "roll": sign_rotation.roll}, "world_size": 12.0},
                                 "entry_location": [origin.x, origin.y, origin.z],
                                 "entry_rotation": {"pitch": rotation.pitch, "yaw": rotation.yaw, "roll": rotation.roll},
                                 "entry_rotation_before": {"pitch": original_entry_rotation.pitch, "yaw": original_entry_rotation.yaw, "roll": original_entry_rotation.roll},
                                 "entry_orientation_note": "Only this copied review PlayerStart is deliberately aligned to the bridge X axis; original combat entry stays unchanged.",
                                 "horizontal_heading": [forward.x, forward.y, forward.z],
                                 "terminal_rotation": {"pitch": facing.pitch, "yaw": facing.yaw, "roll": facing.roll},
                                 "quiet_interaction_lane": True, "source_map_modified": False}
        starts, start, world, actor, cdo, mode, sign = [], None, None, None, None, None, None
        gc.collect()
    assert_sources_unchanged()
    report["package_manifest"] = sorted(set(report["created"] + report["saved"]))
    report["status"] = "Authored and saved. Fresh load, actual E, durable checkpoint readback and cross-map travel still require live qualification."
    editor.load_level(MAPS + "L_TarrikReview")
except Exception as exc:
    report["error"] = str(exc)
    report["traceback"] = traceback.format_exc()
    report["status"] = "Stopped; inspect partial review assets before rerunning."
    raise
finally:
    if source_disk:
        report["source_disk_unchanged"] = hashes() == source_disk
    OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SOV_TDD_REVIEW_ROUTE_REPORT " + str(OUT))
