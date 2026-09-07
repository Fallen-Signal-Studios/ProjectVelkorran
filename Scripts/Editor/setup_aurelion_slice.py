"""Author the isolated Aurelion first-encounter preparation map with PIE stopped.

Host: python Scripts/Editor/setup_aurelion_slice.py --validate-config path.json
UE: set VELKORRAN_AURELION_CONFIG and VELKORRAN_AURELION_MODE=inventory|apply,
then execute this file. Inventory is the read-only default. Apply requires explicit
placed participants and positions. Existing output with matching setup stamps is preserved, never overwritten.
The full M12/M13 assets remain incomplete scaffolds and are not the playable mission.
"""
import argparse
import gc
import hashlib
import json
import math
import os
from pathlib import Path
import re
import traceback

DATA = "/Game/Campaign/Development/Aurelion/"
MAPS = "/Game/Maps/Development/Aurelion/"
OUTPUTS = {
    "map": MAPS + "L_TarrikPreparation",
    "mission": DATA + "DA_TarrikPreparation",
    "game_mode": DATA + "BP_TarrikPreparationGameMode",
    "m12_scaffold": DATA + "Scaffolds/DA_M12_FireAndFrost",
    "m13_scaffold": DATA + "Scaffolds/DA_M13_ContraryWitness",
}
MISSION_ID = "M12_AurelionTarrikPreparation"
ENCOUNTER_ID = "M12_MixedSurvivorCorridor"
BEAT_IDS = ["TarrikArrival", "HoldMixedSurvivorCorridor", "SecureTarrikRoute"]
ENTRY_TAG = "AurelionPreparation_Tarrik"
PREFIX = "AurelionPreparation_"
STAMP = "Sov.AurelionPreparation.ConfigSHA256"
PACKAGE_RE = re.compile(r"/Game/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\Z")
ID_RE = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")


def owned(path):
    return isinstance(path, str) and path.casefold().startswith((DATA.casefold(), MAPS.casefold())) and bool(PACKAGE_RE.fullmatch(path))


def config_digest(config):
    return hashlib.sha256(json.dumps(config, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()).hexdigest()


def validate_config(config, inventory_only=False):
    """Fail closed before Unreal mutation; this does not qualify content or gameplay."""
    errors = []
    if not isinstance(config, dict):
        return ["Configuration must be a JSON object."]
    expected = {"schema_version", "sources", "player_start_actor", "participants", "placement"}
    if set(config) != expected:
        errors.append("Top-level keys must be exactly: " + ", ".join(sorted(expected)))
    if type(config.get("schema_version")) is not int or config["schema_version"] != 1:
        errors.append("schema_version must be integer 1.")
    sources = config.get("sources")
    source_keys = {"map", "game_mode", "pawn", "player_definition"}
    if not isinstance(sources, dict) or set(sources) != source_keys:
        errors.append("sources must contain exactly map, game_mode, pawn, player_definition.")
    else:
        for key, path in sources.items():
            if not isinstance(path, str) or not PACKAGE_RE.fullmatch(path) or owned(path):
                errors.append("sources." + key + " must be an external /Game package path, without an object suffix.")
        if len(set(str(v).casefold() for v in sources.values())) != len(sources):
            errors.append("Source packages must be distinct.")
    if inventory_only:
        return errors
    start = config.get("player_start_actor")
    if not isinstance(start, str) or not ID_RE.fullmatch(start):
        errors.append("player_start_actor must be the exact object name from inventory, not its label.")
    rows = config.get("participants")
    ids, names, sides, enemies = set(), set(), set(), 0
    if not isinstance(rows, list) or not 3 <= len(rows) <= 32:
        errors.append("participants must contain 3..32 placed NPCs: an enemy and both survivor backgrounds.")
        rows = []
    for index, row in enumerate(rows):
        key = "participants[" + str(index) + "]"
        if not isinstance(row, dict) or set(row) != {"participant_id", "actor_name", "npc_definition", "role", "background"}:
            errors.append(key + " has missing or unexpected keys.")
            continue
        for field, seen in (("participant_id", ids), ("actor_name", names)):
            value = row[field]
            if not isinstance(value, str) or not ID_RE.fullmatch(value):
                errors.append(key + "." + field + " must be a stable nonempty identifier.")
            elif value.casefold() in seen:
                errors.append(key + "." + field + " is duplicated (Unreal names are case insensitive).")
            else:
                seen.add(value.casefold())
        path = row["npc_definition"]
        if not isinstance(path, str) or not PACKAGE_RE.fullmatch(path) or owned(path):
            errors.append(key + ".npc_definition requires an explicit existing external /Game package.")
        if row["role"] == "enemy":
            enemies += 1
            if row["background"] != "hostile":
                errors.append(key + ": enemy background must be hostile.")
        elif row["role"] == "protected_survivor":
            if row["background"] not in ("dominion", "reformation"):
                errors.append(key + ": survivor background must be dominion or reformation.")
            else:
                sides.add(row["background"])
        else:
            errors.append(key + ".role must be enemy or protected_survivor.")
    if not enemies:
        errors.append("At least one actual enemy must be required for victory.")
    if sides != {"dominion", "reformation"}:
        errors.append("Both Dominion and Reformation protected survivors must be explicit. Background labels do not set combat factions.")
    if isinstance(start, str) and start.casefold() in names:
        errors.append("PlayerStart cannot also be an encounter participant.")
    placements = config.get("placement")
    if not isinstance(placements, dict) or set(placements) != {"arrival_terminal", "start_volume", "secure_terminal"}:
        errors.append("placement must contain arrival_terminal, start_volume and secure_terminal.")
    else:
        for key, placement in placements.items():
            keys = {"location", "rotation", "extent"} if key == "start_volume" else {"location", "rotation"}
            if not isinstance(placement, dict) or set(placement) != keys:
                errors.append("placement." + key + " has missing or unexpected keys.")
                continue
            for field, values in placement.items():
                if not isinstance(values, list) or len(values) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) for v in values):
                    errors.append("placement." + key + "." + field + " must contain three finite numbers.")
                elif field == "extent" and any(v <= 0 for v in values):
                    errors.append("start_volume extent must be positive on all axes.")
            if key == "start_volume" and placement.get("rotation") != [0, 0, 0]:
                errors.append("start_volume rotation must be [0,0,0] for deterministic entry separation checks.")
    return errors


def run_editor(config, mode):
    import unreal
    if mode not in ("inventory", "apply"):
        raise ValueError("VELKORRAN_AURELION_MODE must be inventory or apply.")
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    report_dir = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Validation/AurelionSetup"
    report_dir.mkdir(parents=True, exist_ok=True)
    report = {"mode": mode, "status": "preflight", "created": [], "saved": [], "outputs": OUTPUTS,
              "canonical_slice_complete": False, "unreal_build_verified": False,
              "gameplay_qualified": False, "canonical_scaffolds_complete": False}
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    source_objects, source_files, source_memory = {}, {}, {}

    def package(obj):
        return obj.get_path_name().split(".")[0]

    def load(path):
        obj = unreal.EditorAssetLibrary.load_asset(path)
        if obj is None:
            raise RuntimeError("Required asset missing: " + path)
        return obj

    def current_world(path):
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if editor.is_in_play_in_editor() or world is None or package(world) != path:
            raise RuntimeError("Expected exact stopped editor world: " + path)
        return world

    def inventory(path):
        world = current_world(path)
        rows = []
        for actor in actors.get_all_level_actors():
            if not actor.get_path_name().startswith(world.get_path_name() + ":"):
                raise RuntimeError("Streaming/sublevel actors are unsupported; prepare a single persistent-level template.")
            definition = actor.get_npc_definition() if isinstance(actor, unreal.SovNPCCharacterBase) else None
            placed_definition = actor.get_authored_placed_definition() if isinstance(actor, unreal.SovNPCCharacterBase) else None
            placed_initializer = placed_definition is not None
            if definition is None:
                definition = placed_definition
            rows.append({"actor_name": actor.get_name(), "label": actor.get_actor_label(), "class": actor.get_class().get_path_name(),
                         "placed_sov_npc": isinstance(actor, unreal.SovNPCCharacterBase),
                         "native_placed_initialization": placed_initializer,
                         "npc_definition": package(definition) if definition else None,
                         "player_start": isinstance(actor, unreal.PlayerStart),
                         "spawner": isinstance(actor, unreal.NPCSpawner),
                         "existing_director": isinstance(actor, unreal.SovEncounterDirector),
                         "existing_bridge": isinstance(actor, unreal.SovCampaignEncounterObjective),
                         "existing_terminal": isinstance(actor, unreal.SovCampaignInteractionTerminal)})
        return rows

    def files_now():
        result = {}
        for path in sorted(set(config["sources"].values()) | {row["npc_definition"] for row in config["participants"] if isinstance(row, dict) and isinstance(row.get("npc_definition"), str) and PACKAGE_RE.fullmatch(row["npc_definition"])}):
            relative = "Content/" + path[len("/Game/"):]
            candidates = [root / (relative + suffix) for suffix in (".uasset", ".umap", ".uexp", ".ubulk")]
            for file in candidates:
                if file.is_file():
                    digest = hashlib.sha256()
                    with file.open("rb") as handle:
                        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                            digest.update(chunk)
                    result[str(file.relative_to(root))] = digest.hexdigest()
        return result

    def sources_unchanged():
        after = files_now()
        memory = {path: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj) for path, obj in source_objects.items() if isinstance(obj, unreal.Blueprint)}
        report["source_disk_unchanged"] = after == source_files
        report["source_blueprint_defaults_unchanged"] = memory == source_memory
        if not report["source_disk_unchanged"] or not report["source_blueprint_defaults_unchanged"]:
            raise RuntimeError("Input preservation check failed; no more packages will be saved.")

    def save(obj):
        path = package(obj)
        if not owned(path):
            raise RuntimeError("Refusing save outside fixed Aurelion output folders: " + path)
        sources_unchanged()
        unreal.EditorAssetLibrary.set_metadata_tag(obj, STAMP, digest)
        if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
            raise RuntimeError("Save failed: " + path)
        report["saved"].append(path)

    def create(path, cls):
        if path not in OUTPUTS.values() or unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError("Refusing to overwrite an existing or unowned asset: " + path)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        directory, name = path.rsplit("/", 1)
        obj = asset_tools.create_asset(name, directory, cls, factory)
        if obj is None or not isinstance(obj, cls):
            raise RuntimeError("Native mission asset creation failed: " + path)
        report["created"].append(path)
        return obj

    def vector(values):
        return unreal.Vector(*values)

    def rotation(values):
        return unreal.Rotator(pitch=values[0], yaw=values[1], roll=values[2])

    def spawn(cls, name, placement):
        current_world(OUTPUTS["map"])
        actor = actors.spawn_actor_from_class(cls, vector(placement["location"]), rotation(placement["rotation"]))
        if actor is None:
            raise RuntimeError("Could not spawn " + name)
        actor.set_actor_label(PREFIX + name)
        actor.set_editor_property("tags", [unreal.Name(PREFIX + name)])
        return actor

    try:
        errors = validate_config(config, inventory_only=mode == "inventory")
        if errors:
            raise ValueError("\n".join(errors))
        if editor.is_in_play_in_editor():
            raise RuntimeError("Stop PIE before setup.")
        dirty = list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        if dirty:
            report["initial_dirty_packages"] = [p.get_path_name() for p in dirty]
            raise RuntimeError("Save or discard your dirty maps/content explicitly first. Setup will not save them or switch maps.")
        required_types = ("SovAurelionTarrikPreparationMissionDefinition", "SovAurelionFireAndFrostMissionDefinition", "SovAurelionContraryWitnessMissionDefinition", "SovCampaignEncounterObjective", "SovBlueprintAuthoringLibrary")
        if any(not hasattr(unreal, name) for name in required_types):
            raise RuntimeError("Compile the new native Editor target before running setup.")
        if mode == "apply":
            source_files = files_now()
            report["source_sha256_before_load"] = source_files
        for kind, path in config["sources"].items():
            if kind != "map":
                source_objects[path] = load(path)
        # Source maps are read only. Native placed participants carry an explicit
        # AuthoredPlacedDefinition that initializes through Narrative at BeginPlay.
        source_map = config["sources"]["map"]
        map_relative = source_map[len("/Game/"):]
        for directory in ("__ExternalActors__", "__ExternalObjects__"):
            if (root / "Content" / directory / map_relative).exists():
                raise RuntimeError("External-actor maps are unsupported. Supply a simple persistent-level template.")
        if not editor.load_level(source_map):
            raise RuntimeError("Cannot load source map: " + source_map)
        report["source_actor_inventory"] = inventory(source_map)
        if mode == "inventory":
            report["status"] = "inventory_only_no_assets_changed"
            return report
        digest = config_digest(config)
        report["config_sha256"] = digest
        # Validate every authored participant and unexpected autonomous producer
        # before creating any output. Exact actor object names survive duplication.
        by_name = {row["actor_name"]: row for row in report["source_actor_inventory"]}
        if config["player_start_actor"] not in by_name or not by_name[config["player_start_actor"]]["player_start"]:
            raise RuntimeError("Configured PlayerStart was not found.")
        if sum(row["player_start"] for row in by_name.values()) != 1:
            raise RuntimeError("The preparation template must contain exactly one PlayerStart.")
        if any(row["spawner"] or row["existing_director"] or row["existing_bridge"] or row["existing_terminal"] for row in by_name.values()):
            raise RuntimeError("Template contains spawners/directors/bridges/campaign terminals. Prepare a dedicated copy with only explicitly placed participants; this script does not delete source content.")
        expected_names = {row["actor_name"] for row in config["participants"]}
        actual_names = {row["actor_name"] for row in by_name.values() if row["placed_sov_npc"]}
        if expected_names != actual_names:
            raise RuntimeError("Every placed Sov NPC must have exactly one participant row; missing or extra NPCs detected.")
        for row in config["participants"]:
            actual = by_name[row["actor_name"]]
            if actual["npc_definition"] != row["npc_definition"]:
                raise RuntimeError("NPC definition mismatch or missing authored placed definition: " + row["actor_name"])
            load(row["npc_definition"])
        # Start trigger cannot already overlap the PlayerStart or arrival terminal.
        start = next(a for a in actors.get_all_level_actors() if a.get_name() == config["player_start_actor"])
        start_position = start.get_actor_location()
        trigger = config["placement"]["start_volume"]
        for point in ([start_position.x, start_position.y, start_position.z], config["placement"]["arrival_terminal"]["location"]):
            if all(abs(point[i] - trigger["location"][i]) <= trigger["extent"][i] + 100 for i in range(3)):
                raise RuntimeError("Start volume must be separated from PlayerStart and arrival terminal by a 100-unit margin.")
        start = None
        for obj in source_objects.values():
            if isinstance(obj, unreal.Blueprint):
                cdo = unreal.get_default_object(obj.generated_class())
                if isinstance(cdo, unreal.Actor):
                    cdo.get_components_by_class(unreal.ActorComponent)
        obj, cdo = None, None
        gc.collect()
        unreal.collect_garbage()
        if files_now() != source_files:
            raise RuntimeError("Source files changed during read-only preflight/warmup.")
        source_memory = {path: unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj) for path, obj in source_objects.items() if isinstance(obj, unreal.Blueprint)}
        report["source_sha256_before"] = source_files
        present = {key: unreal.EditorAssetLibrary.does_asset_exist(path) for key, path in OUTPUTS.items()}
        if any(present.values()):
            if not all(present.values()):
                raise RuntimeError("Partial prior outputs exist. Inspect/back them up and move/delete only the listed Aurelion outputs before retrying; no overwrite attempted.")
            for path in OUTPUTS.values():
                if unreal.EditorAssetLibrary.get_metadata_tag(load(path), STAMP) != digest:
                    raise RuntimeError("Existing output lacks this exact configuration ownership stamp: " + path)
            report["status"] = "existing_outputs_preserved_no_mutation"
            report["note"] = "Matching setup stamps found. User-authored changes are preserved. Run native preflight and gameplay qualification; stamps are not validation receipts."
            sources_unchanged()
            return report
        if not isinstance(source_objects[config["sources"]["game_mode"]], unreal.Blueprint) or not isinstance(source_objects[config["sources"]["pawn"]], unreal.Blueprint):
            raise RuntimeError("game_mode and pawn sources must be the existing project Blueprints.")
        if not isinstance(unreal.get_default_object(source_objects[config["sources"]["game_mode"]].generated_class()), unreal.SovCampaignGameMode):
            raise RuntimeError("Source GameMode must derive from SovCampaignGameMode.")
        if not isinstance(unreal.get_default_object(source_objects[config["sources"]["pawn"]].generated_class()), unreal.SovPlayerCharacterBase):
            raise RuntimeError("Source pawn must derive from SovPlayerCharacterBase.")
        if not editor.new_level_from_template(OUTPUTS["map"], source_map):
            raise RuntimeError("Could not duplicate the preparation map.")
        report["created"].append(OUTPUTS["map"])
        world = current_world(OUTPUTS["map"])
        mission = create(OUTPUTS["mission"], unreal.SovAurelionTarrikPreparationMissionDefinition)
        for prop, value in {"pawn_class": source_objects[config["sources"]["pawn"]].generated_class(), "player_definition": source_objects[config["sources"]["player_definition"]], "map": world, "entry_player_start_tag": unreal.Name(ENTRY_TAG)}.items():
            mission.set_editor_property(prop, value)
        if mission.validate_definition() is None:
            raise RuntimeError("Native preparation mission validation failed.")
        if str(mission.get_editor_property("mission_id")) != MISSION_ID or [str(b.get_editor_property("beat_id")) for b in mission.get_editor_property("beats")] != BEAT_IDS:
            raise RuntimeError("Native preparation mission contract changed; update the script deliberately.")
        save(mission)
        # These deliberately incomplete scaffolds preserve their native guarded beats.
        # No scene, witness, companion, release or exchange receipt is fabricated.
        for key, cls in (("m12_scaffold", unreal.SovAurelionFireAndFrostMissionDefinition), ("m13_scaffold", unreal.SovAurelionContraryWitnessMissionDefinition)):
            scaffold = create(OUTPUTS[key], cls)
            save(scaffold)
        report["canonical_scaffold_note"] = "Full M12/M13 scaffolds are intentionally unassigned and invalid until actual profiles, maps, story/evidence and companion content are supplied. They are excluded from the preparation manifest."
        source_mode = source_objects[config["sources"]["game_mode"]]
        mode_asset = unreal.EditorAssetLibrary.duplicate_asset(config["sources"]["game_mode"], OUTPUTS["game_mode"])
        if mode_asset is None or mode_asset.generated_class() == source_mode.generated_class():
            raise RuntimeError("Failed to create a distinct preparation GameMode.")
        report["created"].append(OUTPUTS["game_mode"])
        cdo = unreal.get_default_object(mode_asset.generated_class())
        cdo.set_editor_property("initial_mission", mission)
        cdo.set_editor_property("use_seamless_travel", False)
        compiled = unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references([mode_asset], [source_mode], [mode_asset])
        if not compiled.get_editor_property("succeeded") or unreal.get_default_object(mode_asset.generated_class()).get_editor_property("initial_mission") != mission:
            raise RuntimeError("Preparation GameMode compilation/readback failed.")
        save(mode_asset)
        current_world(OUTPUTS["map"]).get_world_settings().set_editor_property("default_game_mode", mode_asset.generated_class())
        copied = {a.get_name(): a for a in actors.get_all_level_actors()}
        copied[config["player_start_actor"]].set_editor_property("player_start_tag", unreal.Name(ENTRY_TAG))
        director = spawn(unreal.SovEncounterDirector, "Director", trigger)
        director.set_editor_property("encounter_id", unreal.Name(ENCOUNTER_ID))
        director.set_editor_property("allow_companion_rescue", False)
        participants, protected = [], []
        for row in config["participants"]:
            participant = unreal.SovEncounterParticipant()
            participant.set_editor_property("participant_id", unreal.Name(row["participant_id"]))
            participant.set_editor_property("character", copied[row["actor_name"]])
            participant.set_editor_property("required_for_victory", row["role"] == "enemy")
            participant.set_editor_property("allow_mass_representation", False)
            participants.append(participant)
            if row["role"] == "protected_survivor":
                protected.append(unreal.Name(row["participant_id"]))
        director.set_editor_property("participants", participants)
        director.set_editor_property("protected_participant_ids", protected)
        bridge = spawn(unreal.SovCampaignEncounterObjective, "EncounterEntry", trigger)
        bridge.set_editor_property("encounter_director", director)
        bridge.set_editor_property("mission_id", unreal.Name(MISSION_ID))
        bridge.set_editor_property("completion_beat", unreal.Name(BEAT_IDS[1]))
        bridge.set_editor_property("start_on_player_overlap", True)
        bridge.get_editor_property("start_volume").set_box_extent(vector(trigger["extent"]), update_overlaps=False)
        for name, beat_id, placement in (("Arrival", BEAT_IDS[0], config["placement"]["arrival_terminal"]), ("SecureRoute", BEAT_IDS[2], config["placement"]["secure_terminal"])):
            terminal = spawn(unreal.SovCampaignInteractionTerminal, name, placement)
            terminal.set_editor_property("terminal_id", unreal.Name(PREFIX + name))
            terminal.set_editor_property("mission_id", unreal.Name(MISSION_ID))
            terminal.set_editor_property("completion_beat", unreal.Name(beat_id))
            terminal.set_editor_property("write_checkpoint", name == "SecureRoute")
            terminal.get_editor_property("visual").set_static_mesh(load("/Engine/BasicShapes/Cube"))
            terminal.get_editor_property("visual").set_relative_scale3d(unreal.Vector(.7, 1.1, 1.3))
            terminal.get_editor_property("label").set_text(next(b.get_editor_property("objective_text") for b in mission.get_editor_property("beats") if str(b.get_editor_property("beat_id")) == beat_id))
            terminal.get_editor_property("label").set_world_size(8)
        sources_unchanged()
        current_world(OUTPUTS["map"])
        unreal.EditorAssetLibrary.set_metadata_tag(world, STAMP, digest)
        if not editor.save_current_level():
            raise RuntimeError("Preparation map save failed.")
        report["saved"].append(OUTPUTS["map"])
        sources_unchanged()
        report["mission_manifest"] = [OUTPUTS["mission"] + ".DA_TarrikPreparation"]
        (report_dir / "preparation-mission-manifest.json").write_text(json.dumps(report["mission_manifest"], indent=2), encoding="utf-8")
        report["status"] = "authored_not_gameplay_qualified"
        report["required_engine_checks"] = ["Fresh Editor reload and mission/content validation", "Actual NPC readiness, factions and hostile threat behavior", "Arrival interaction, entry overlap and nonempty encounter start", "Protected survivor death fails; retry restores participants; all enemies defeated commits once", "Secure-route terminal blocked before encounter success; durable checkpoint reload", "Keyboard/controller packaged playthrough and full M12/M13 authoring remain outstanding"]
        return report
    except Exception as exc:
        report["status"] = "stopped_inspect_partial_outputs"
        report["error"] = str(exc)
        report["traceback"] = traceback.format_exc()
        raise
    finally:
        if source_files:
            report["source_disk_unchanged"] = files_now() == source_files
        path = report_dir / (mode + "-report.json")
        path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("SOV_AURELION_SETUP_REPORT " + str(path))


def main():
    try:
        import unreal  # noqa: F401
    except ImportError:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--validate-config", required=True, type=Path)
        args = parser.parse_args()
        config = json.loads(args.validate_config.read_text(encoding="utf-8-sig"))
        errors = validate_config(config)
        print(json.dumps({"status": "invalid" if errors else "host_config_valid_unreal_unexecuted", "errors": errors}, indent=2))
        return 2 if errors else 0
    configured = os.environ.get("VELKORRAN_AURELION_CONFIG")
    if not configured:
        raise RuntimeError("Set VELKORRAN_AURELION_CONFIG to an explicit reviewed JSON path.")
    config = json.loads(Path(configured).read_text(encoding="utf-8-sig"))
    run_editor(config, os.environ.get("VELKORRAN_AURELION_MODE", "inventory"))
    return 0


if __name__ == "__main__":
    exit_code = main()
    if exit_code:
        raise SystemExit(exit_code)
