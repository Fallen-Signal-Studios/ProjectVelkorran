"""Preview or save the chapter-26 celestial vista behind M13 Z11's window."""
import hashlib
import json
import os
import runpy
import shutil
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
source = root / "Art/Source/Aurelion/Z11ObservationVista/T_Aurelion_Z11WoundVista_Source.png"
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
assert source_hash == "5990abf37066f688ecdcaee8741e00f10e1f7724cb95125f077cedb4df5b0379"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == "L_Aurelion_M13"
maps = {name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
        for name in ("L_Aurelion_M12.umap", "L_Aurelion_M13.umap")}
assert maps == {
    "L_Aurelion_M12.umap": "23555516889b0078ea29323f375b1e2d43e5edf1132ad903e68989bb51da61f1",
    "L_Aurelion_M13.umap": "3ac15e513db149c6e9799276633d3597eeb4d7b60b0f197eb7d3eb8369a47293",
}
save = os.environ.get("SOV_Z11_VISTA_SAVE") == "1"
review = None
if save:
    review = json.loads(Path(os.environ["SOV_Z11_VISTA_REVIEWED_PREVIEW"]).read_text())
    assert review["status"] == "unsaved_preview"
    assert review["maps_before"] == maps and review["source_sha256"] == source_hash

actorsub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actorsub.get_all_level_actors()
labels = {actor.get_actor_label() for actor in actors}
assert "Aurelion_Custom_Z11_CelestialVista" not in labels
count_before = len(actors)
texture_path = "/Game/Aurelion/Environment/ObservationVista/Textures/T_Aurelion_Z11WoundVista"
material_path = "/Game/Aurelion/Environment/ObservationVista/Materials/M_Aurelion_Z11WoundVista"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
if save or (assets.does_asset_exist(texture_path) and assets.does_asset_exist(material_path)):
    texture = unreal.load_asset(texture_path)
    material = unreal.load_asset(material_path)
else:
    assert not assets.does_asset_exist(texture_path)
    assert not assets.does_asset_exist(material_path)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = texture_path.rsplit("/", 1)[0]
    task.destination_name = texture_path.rsplit("/", 1)[1]
    task.automated = True
    task.replace_existing = False
    task.save = False
    task.factory = unreal.TextureFactory()
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(texture_path)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_property("srgb", True)
    assert assets.save_loaded_asset(texture)
    material = tools.create_asset(material_path.rsplit("/", 1)[1],
                                  material_path.rsplit("/", 1)[0],
                                  unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    edit = unreal.MaterialEditingLibrary
    sample = edit.create_material_expression(material, unreal.MaterialExpressionTextureSample, -300, 0)
    sample.set_editor_property("texture", texture)
    assert edit.connect_material_property(sample, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(material)
    assert assets.save_loaded_asset(material)
assert isinstance(texture, unreal.Texture2D) and isinstance(material, unreal.Material)
assert material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_UNLIT
assert material.get_editor_property("two_sided")
edit = unreal.MaterialEditingLibrary
sample = edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(sample, unreal.MaterialExpressionTextureSample)
if not save and not any(isinstance(node, unreal.MaterialExpressionCustom)
                        for node in edit.get_inputs_for_material_expression(material, sample)):
    uv = edit.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, 0)
    orient = edit.create_material_expression(material, unreal.MaterialExpressionCustom, -500, 0)
    pin = unreal.CustomInput()
    pin.set_editor_property("input_name", "UV")
    orient.set_editor_property("inputs", [pin])
    orient.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    orient.set_editor_property("code", "return float2(UV.y, 1.0-UV.x);")
    assert edit.connect_material_expressions(uv, "", orient, "UV")
    assert edit.connect_material_expressions(orient, "", sample, "UVs")
    edit.recompile_material(material)
    assert assets.save_loaded_asset(material)
assert any(isinstance(node, unreal.MaterialExpressionCustom)
           for node in edit.get_inputs_for_material_expression(material, sample))
texture_file = root / ("Content" + texture_path.removeprefix("/Game") + ".uasset")
material_file = root / ("Content" + material_path.removeprefix("/Game") + ".uasset")
asset_hashes = {"texture": hashlib.sha256(texture_file.read_bytes()).hexdigest(),
                "material": hashlib.sha256(material_file.read_bytes()).hexdigest()}
if save:
    assert asset_hashes == review["asset_hashes"]

plane = actorsub.spawn_actor_from_class(unreal.StaticMeshActor,
    unreal.Vector(-1500, 43100, 450), unreal.Rotator(pitch=90, yaw=180))
assert plane
plane.set_actor_label("Aurelion_Custom_Z11_CelestialVista")
plane.set_actor_scale3d(unreal.Vector(10.13, 18, 1))
component = plane.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
component.set_material(0, material)
component.set_collision_profile_name("NoCollision")
component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
component.set_editor_property("can_ever_affect_navigation", False)
component.set_cast_shadow(False)
component.set_editor_property("receives_decals", False)
plane.set_actor_enable_collision(False)
assert len(actorsub.get_all_level_actors()) == count_before + 1
origin, extent = plane.get_actor_bounds(False)
assert 899 <= extent.y <= 901 and 505 <= extent.z <= 508
row = dict(transform=plane.get_actor_transform().export_text(),
           origin=origin.export_text(), extent=extent.export_text(),
           mesh=component.static_mesh.get_path_name(), material=component.get_material(0).get_path_name(),
           collision=str(component.get_collision_enabled()))
report = dict(status="unsaved_preview", source_sha256=source_hash,
              maps_before=maps, asset_hashes=asset_hashes, actor=row)
if save:
    assert row == review["actor"]
    shutil.copy2(root / "Content/Aurelion/Maps/L_Aurelion_M13.umap", out / "L_Aurelion_M13.before.umap")
    assert level.save_current_level()
    assert level.load_level("/Game/Aurelion/Maps/L_Aurelion_M13")
    restored = [a for a in actorsub.get_all_level_actors()
                if a.get_actor_label() == "Aurelion_Custom_Z11_CelestialVista"]
    assert len(restored) == 1
    restored_component = restored[0].get_component_by_class(unreal.StaticMeshComponent)
    assert restored[0].get_actor_transform().export_text() == row["transform"]
    assert restored_component.get_material(0).get_path_name() == material_path + "." + material_path.rsplit("/", 1)[1]
    assert restored_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert hashlib.sha256((root / "Content/Aurelion/Maps/L_Aurelion_M12.umap").read_bytes()).hexdigest() == maps["L_Aurelion_M12.umap"]
    report["status"] = "saved_reloaded"
(out / "observation-vista-fit.json").write_text(json.dumps(report, indent=2))
runpy.run_path(str(root / "Scripts/Editor/preview_m13_route.py"), init_globals={
    "ALLOW_DIRTY_PREVIEW": not save,
    "M13_ROUTE_VIEWS": [
        ("z11-vista-center", (750, 43100, 190)),
        ("z11-vista-left", (450, 42650, 190)),
    ],
    "M13_ROUTE_YAWS": {"z11-vista-center": 180, "z11-vista-left": 180},
    "M13_ROUTE_PITCHES": {"z11-vista-center": -8, "z11-vista-left": -8},
})
