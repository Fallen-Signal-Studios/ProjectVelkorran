"""Read attachment transforms, ammo defaults and Verity tag mapping; no writes."""
import json
import os
from pathlib import Path
import unreal

OUTPUT_DIR = Path(os.environ.get("VELKORRAN_SETUP_OUTPUT",
    str(Path(unreal.Paths.project_saved_dir()).resolve() / "Validation" / "WorkPCSetup")))
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUT = OUTPUT_DIR / "weapon-presentation-health.json"
report = {"read_only": True, "templates": {}, "live_items": [], "meshes": [], "animation": {}}


def value(v):
    if v is None or isinstance(v, (str, bool, int, float)):
        return v
    if isinstance(v, unreal.Object):
        return v.get_path_name()
    if hasattr(v, "export_text"):
        return v.export_text()
    return str(v)


def read(row, label, fn):
    try:
        row[label] = value(fn())
    except Exception as exc:
        row[label + "_error"] = str(exc)


def item_data(item):
    row = {"path": item.get_path_name(), "class": item.get_class().get_path_name()}
    for key in ("clip_size", "required_ammo", "weapon_clip_state", "weapon_visual_class", "allow_manual_reload"):
        read(row, key, lambda key=key: item.get_editor_property(key))
    for key in ("holster_attachment_configs", "wield_attachment_configs"):
        try:
            row[key] = {str(unreal.GameplayTagLibrary.get_tag_name(k)): v.export_text()
                        for k, v in item.get_editor_property(key).items()}
        except Exception as exc:
            row[key + "_error"] = str(exc)
    for key in ("get_ammo_in_clip", "get_spare_ammo", "get_ammo_source"):
        read(row, key, lambda key=key: getattr(item, key)())
    return row


for name, source in (("Staccato", "/Game/WeaponMeshes/Weapon_Staccato"),
                     ("Axiom", "/Game/WeaponMeshes/Weapon_Axiom")):
    report["templates"][name] = {}
    for role, path in (("source", source), ("project", "/Game/Items/Weapons/WI_" + name)):
        bp = unreal.load_asset(path)
        report["templates"][name][role] = item_data(unreal.get_default_object(bp.generated_class()))

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    report["pawn"] = value(pawn)
    for item in unreal.ObjectIterator(unreal.WeaponItem):
        try:
            if pawn and item.get_owning_pawn() == pawn:
                report["live_items"].append(item_data(item))
        except Exception:
            pass
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WeaponVisual):
        if "Staccato" not in actor.get_class().get_path_name():
            continue
        for mesh in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            row = {"path": mesh.get_path_name(), "actor": actor.get_path_name()}
            for field in ("relative_location", "relative_rotation", "relative_scale3d", "skeletal_mesh_asset",
                          "anim_class", "always_create_physics_state"):
                read(row, field, lambda field=field: mesh.get_editor_property(field))
            read(row, "simulate_physics", lambda: mesh.is_simulating_physics())
            read(row, "relative_transform", lambda: mesh.get_relative_transform())
            read(row, "world_transform", lambda: mesh.get_world_transform())
            read(row, "frame_world", lambda: mesh.get_socket_transform("frame", unreal.RelativeTransformSpace.RTS_WORLD))
            parent = mesh.get_attach_parent()
            socket = mesh.get_attach_socket_name()
            row["attach_parent"] = value(parent)
            row["attach_socket"] = str(socket)
            if parent:
                read(row, "parent_world", lambda: parent.get_world_transform())
                read(row, "parent_socket_world", lambda: parent.get_socket_transform(socket, unreal.RelativeTransformSpace.RTS_WORLD))
            report["meshes"].append(row)

abp_path = "/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Overlays/Weapons/ABP_Biped_Overlay_Verity"
abp = unreal.load_asset(abp_path)
if abp:
    cdo = unreal.get_default_object(abp.generated_class())
    report["animation"]["path"] = abp_path
    read(report["animation"], "gameplay_tag_property_map", lambda: cdo.get_editor_property("gameplay_tag_property_map"))
    # The struct's official text export retains the property name even when Python cannot expose its private mappings.
OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("VELKORRAN_WEAPON_PRESENTATION_HEALTH " + str(OUT))
