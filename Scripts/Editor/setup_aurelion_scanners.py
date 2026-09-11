"""Stopped-editor authoring of the two Z03 sensors; no gameplay or map save on import.

Call build_scanners(ctx, encounters, output_dir) after build_encounters. The root
authoring transaction owns the current M12 world, source-map guards and map save.
Only the copied contaminated-drone controller opts into finite network sensing.
"""
import json
from pathlib import Path
import unreal

ROOT = "/Game/Aurelion/Enemies/"
CONTROLLER = ROOT + "BP_AurelionContaminatedDroneController"
STAMP = "Sov.Aurelion.ScannerSchema"
SCHEMA = "2026-09-07.1"


def _path(obj):
    return obj.get_path_name() if obj else None


def _stopped():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("Stop PIE before scanner authoring")


def configure_network_controller(enemy_definitions, output_dir):
    """Duplicate the real stock controller and assign only the owned drone BP.

    Controller graphs, perception and ordinary Narrative activity ownership are
    preserved. There is no new goal loop or scripted target assignment.
    """
    _stopped()
    from setup_aurelion_enemy_roles import SEEDS, _load, _disk_files
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    report = {"status": "preflight", "saved": [], "source_unchanged": False}
    source_definition = _load(SEEDS["ContaminatedDrone"][0])
    source_class = _load(source_definition.get_editor_property("npc_class_path"))
    source_npc_bp = _load(_path(source_class).removesuffix("_C").split(".")[0])
    source_cdo = unreal.get_default_object(source_class)
    source_controller = source_cdo.get_editor_property("ai_controller_class")
    if not source_controller:
        source_controller = _load("/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController").generated_class()
    source_bp = _load(_path(source_controller).removesuffix("_C").split(".")[0])
    if not isinstance(source_bp, unreal.Blueprint):
        raise RuntimeError("Contaminated drone needs the actual stock Blueprint perception controller")
    unreal.get_default_object(source_controller)
    enemy_bp = _load(enemy_definitions["ContaminatedDrone"]["blueprint"])
    if _path(enemy_bp).split(".")[0] != ROOT + "BP_AurelionContaminatedDrone":
        raise RuntimeError("Only the project-owned contaminated drone may opt into network sensing")
    if unreal.EditorAssetLibrary.get_metadata_tag(enemy_bp, "Sov.Aurelion.EnemyRoleSchema") != SCHEMA:
        raise RuntimeError("Contaminated drone is not the owned enemy-builder copy")
    source_packages = {_path(obj).split(".")[0] for obj in (source_definition, source_npc_bp, source_bp)}
    unreal.collect_garbage()
    before_disk = _disk_files(source_packages)
    before_memory = { _path(bp): unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
                      for bp in (source_npc_bp, source_bp) }

    def preserve():
        after_disk = _disk_files(source_packages)
        after_memory = { _path(bp): unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp)
                         for bp in (source_npc_bp, source_bp) }
        report.update(source_disk_before=before_disk, source_disk_after=after_disk,
                      source_memory_before=before_memory, source_memory_after=after_memory)
        report["source_unchanged"] = before_disk == after_disk and before_memory == after_memory
        if not report["source_unchanged"]:
            raise RuntimeError("Original controller/NPC changed; no further saves")

    try:
        if unreal.EditorAssetLibrary.does_asset_exist(CONTROLLER):
            controller_bp = unreal.load_asset(CONTROLLER)
            if unreal.EditorAssetLibrary.get_metadata_tag(controller_bp, STAMP) != SCHEMA:
                raise RuntimeError("Existing controller copy is unowned or from another schema")
        else:
            controller_bp = unreal.EditorAssetLibrary.duplicate_asset(_path(source_bp).split(".")[0], CONTROLLER)
            if not controller_bp:
                raise RuntimeError("Could not duplicate the stock controller")
        helper = unreal.SovAurelionEnemyAuthoringLibrary
        if not helper.compile_owned_blueprint(controller_bp):
            raise RuntimeError("Copied sensor controller failed compilation")
        cdo = unreal.get_default_object(controller_bp.generated_class())
        if not isinstance(cdo, unreal.NarrativeNPCController):
            raise RuntimeError("Copied controller lost the native Narrative contract")
        cdo.set_editor_property("accept_network_threats", True)
        if not helper.compile_owned_blueprint(controller_bp):
            raise RuntimeError("Copied sensor controller failed final compilation")
        enemy_cdo = unreal.get_default_object(enemy_bp.generated_class())
        enemy_cdo.set_editor_property("ai_controller_class", controller_bp.generated_class())
        if not helper.compile_owned_blueprint(enemy_bp):
            raise RuntimeError("Copied contaminated drone failed compilation")
        if not unreal.get_default_object(controller_bp.generated_class()).get_editor_property("accept_network_threats"):
            raise RuntimeError("Network observation opt-in did not survive compilation")
        if unreal.get_default_object(enemy_bp.generated_class()).get_editor_property("ai_controller_class") != controller_bp.generated_class():
            raise RuntimeError("Contaminated drone controller assignment did not survive compilation")
        preserve()
        for bp in (controller_bp, enemy_bp):
            _stopped()
            preserve()
            unreal.EditorAssetLibrary.set_metadata_tag(bp, STAMP, SCHEMA)
            if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
                raise RuntimeError("Could not save owned scanner dependency: " + _path(bp))
            report["saved"].append(_path(bp))
        preserve()
        report.update(status="authored; live sensor behavior awaits validation",
                      source_controller=_path(source_controller), controller=_path(controller_bp.generated_class()))
        enemy_definitions["ContaminatedDrone"]["controller"] = _path(controller_bp.generated_class())
        return controller_bp.generated_class()
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        raise
    finally:
        (output / "scanner-controller-authoring.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


def build_scanners(ctx, encounters, output_dir):
    """Author physical scanning heads bound to the exact two existing E2 drones.

    Does not save/load a map, start PIE, enable enemies, emit observations or run
    campaign operations. Root must retain its normal source/map-save guards.
    """
    _stopped()
    if ctx["chapter"] == 13:
        return []
    api, world = ctx["api"], ctx["world"]
    current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if ctx["chapter"] != 12 or world != current or world.get_path_name().split(".")[0] != api.BASE + "Maps/L_Aurelion_M12":
        raise RuntimeError("Scanners may be authored only in the owned M12 world")
    drones, director = encounters["scanner_targets"], encounters["scanner_director"]
    if len(drones) != 2 or len({_path(drone) for drone in drones}) != 2:
        raise RuntimeError("Scanner must alert exactly two existing relay drones")
    ids = [director.find_participant_id(drone) for drone in drones]
    if {str(value) for value in ids} != {"E2.Drone1", "E2.Drone2"}:
        raise RuntimeError("Scanner recipients do not match the authored E2 roster")
    for drone, identity in zip(drones, ids):
        if not isinstance(drone, unreal.SovDroneNPCBase) or drone.get_world() != world or director.get_participant(identity) != drone:
            raise RuntimeError("Scanner recipient identity changed")
    controller = ctx.get("scanner_controller")
    if controller is None:
        # Compiling a Blueprint with placed actors can reinstance the roster.
        # Standalone callers must configure before they create any drone actors.
        if any(isinstance(a, unreal.SovDroneNPCBase) for a in
               unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()):
            raise RuntimeError("Configure the scanner controller before placing drones, then pass ctx['scanner_controller']")
        controller = configure_network_controller(ctx["enemy_definitions"], output_dir)
    expected_class = CONTROLLER + "." + CONTROLLER.rsplit("/", 1)[1] + "_C"
    if _path(controller) != expected_class:
        raise RuntimeError("Scanner controller is not the exact owned generated class")
    controller_bp = unreal.load_asset(CONTROLLER)
    enemy_bp = unreal.load_asset(ctx["enemy_definitions"]["ContaminatedDrone"]["blueprint"])
    if (not controller_bp or not enemy_bp or
            unreal.EditorAssetLibrary.get_metadata_tag(controller_bp, STAMP) != SCHEMA or
            _path(enemy_bp).split(".")[0] != ROOT + "BP_AurelionContaminatedDrone" or
            unreal.EditorAssetLibrary.get_metadata_tag(enemy_bp, "Sov.Aurelion.EnemyRoleSchema") != SCHEMA):
        raise RuntimeError("Preconfigured scanner dependencies lost their exact owned schemas")
    controller_cdo = unreal.get_default_object(controller)
    if (not isinstance(controller_cdo, unreal.NarrativeNPCController) or
            not controller_cdo.get_editor_property("accept_network_threats") or
            unreal.get_default_object(enemy_bp.generated_class()).get_editor_property("ai_controller_class") != controller):
        raise RuntimeError("Preconfigured scanner controller defaults are not intact")
    for drone in drones:
        # Existing placed actors also store the default controller class. The
        # saved BP assignment covers retry replacements and future spawns.
        drone.set_editor_property("ai_controller_class", controller)
    actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    result = []
    for index, spec in enumerate(encounters["scanner_specs"]):
        name = "Z03_SweepScanner" + str(index + 1)
        label = "Aurelion_" + name
        matches = [a for a in actor_system.get_all_level_actors() if a.get_actor_label() == label]
        if len(matches) > 1:
            raise RuntimeError("Ambiguous scanner actor: " + label)
        if matches:
            scanner = matches[0]
            if not isinstance(scanner, unreal.SovAurelionSweepScanner) or unreal.Name(STAMP) not in scanner.tags:
                raise RuntimeError("Existing scanner actor is unowned or has the wrong native class")
            scanner.set_actor_location(api.vector(spec["position"]), False, False)
            scanner.set_actor_rotation(api.rot(0), False)
        else:
            scanner = api.spawn(unreal.SovAurelionSweepScanner, name, spec["position"], 0, folder="Aurelion/Scanners")
            scanner.set_editor_property("tags", list(scanner.tags) + [unreal.Name(STAMP)])
        api.prop(scanner, scanner_id=unreal.Name(spec["scanner_id"]), mission_id=unreal.Name("M12_FireAndFrost"),
                 relay_director=director, relay_drone_ids=ids, enabled=True, scan_range=2400., cone_half_angle=18.,
                 sweep_half_arc=55., sweep_period=6., pitch=-12., phase_offset=index * .5, observation_lifetime=30.)
        checked = scanner.validate_configuration()
        # UE may expose bool+out FString as the string on success / None on
        # failure, or a (bool, string) tuple. An empty success string is valid.
        success = (checked[0] is True if isinstance(checked, tuple) and len(checked) == 2
                   else isinstance(checked, str) or checked is True)
        if not success:
            raise RuntimeError("Native scanner configuration rejected: " + str(checked))
        for old in ctx["existing"].values():
            if old.get_actor_label() in ("Z03_Scanner_17_StandIn", "Z03_Scanner_32_StandIn"):
                if old.get_world() != world:
                    raise RuntimeError("Scanner proxy belongs to another map")
                old.set_actor_hidden_in_game(True)
                old.set_actor_enable_collision(False)
        result.append(scanner)
    if len(result) != 2:
        raise RuntimeError("Z03 must contain exactly the two authored sweep scanners")
    return result
