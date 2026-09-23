"""Read-only fresh-load checks for Z11's vista and the retained furniture fit."""
import hashlib
import json
import os
import runpy
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ["SOV_AURELION_RUN_DIRECTORY"])
maps = {name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
        for name in ("L_Aurelion_M12.umap", "L_Aurelion_M13.umap")}
assert maps["L_Aurelion_M12.umap"] == "23555516889b0078ea29323f375b1e2d43e5edf1132ad903e68989bb51da61f1"
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == "L_Aurelion_M13"
runpy.run_path(str(root / "Scripts/Editor/verify_m13_observation_furniture.py"))

source = root / "Art/Source/Aurelion/Z11ObservationVista/T_Aurelion_Z11WoundVista_Source.png"
assert hashlib.sha256(source.read_bytes()).hexdigest() == "5990abf37066f688ecdcaee8741e00f10e1f7724cb95125f077cedb4df5b0379"
actors = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
          if actor.get_actor_label() == "Aurelion_Custom_Z11_CelestialVista"]
assert len(actors) == 1
actor = actors[0]
component = actor.get_component_by_class(unreal.StaticMeshComponent)
assert component.static_mesh.get_path_name() == "/Engine/BasicShapes/Plane.Plane"
assert component.get_material(0).get_path_name() == (
    "/Game/Aurelion/Environment/ObservationVista/Materials/"
    "M_Aurelion_Z11WoundVista.M_Aurelion_Z11WoundVista"
)
assert component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
assert not component.get_editor_property("can_ever_affect_navigation")
assert not actor.get_actor_enable_collision()
assert not component.get_editor_property("cast_shadow")
assert component.get_editor_property("visible") and not component.get_editor_property("hidden_in_game")
location = actor.get_actor_location()
scale = actor.get_actor_scale3d()
assert all(abs(a-b) < .1 for a, b in zip((location.x, location.y, location.z), (-1500, 43100, 450)))
assert all(abs(a-b) < .01 for a, b in zip((scale.x, scale.y, scale.z), (10.13, 18, 1)))
origin, extent = actor.get_actor_bounds(False)
assert abs(origin.x + 1500) < .1 and 899 <= extent.y <= 901 and 505 <= extent.z <= 508
material = component.get_material(0)
assert material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_UNLIT
assert material.get_editor_property("two_sided")
sample = unreal.MaterialEditingLibrary.get_material_property_input_node(
    material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(sample, unreal.MaterialExpressionTextureSample)
assert sample.get_editor_property("texture").get_path_name() == (
    "/Game/Aurelion/Environment/ObservationVista/Textures/"
    "T_Aurelion_Z11WoundVista.T_Aurelion_Z11WoundVista"
)
inputs = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, sample)
assert any(isinstance(node, unreal.MaterialExpressionCustom) and
           "float2(UV.y, 1.0-UV.x)" in node.get_editor_property("code") for node in inputs)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert maps == {name: hashlib.sha256((root / "Content/Aurelion/Maps" / name).read_bytes()).hexdigest()
                for name in maps}
(out / "observation-vista-verify.json").write_text(json.dumps(dict(
    status="passed", map_hashes=maps, vista_actor_count=1,
    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    furniture_status="passed", dirty_map_packages=0,
), indent=2))
