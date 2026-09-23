"""Fresh-load, read-only acceptance checks for the saved Z06 wayfinding mount."""
from pathlib import Path
import hashlib
import json
import os
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels={a.get_actor_label():a for a in actors}
panel=labels['KIT_Z06_Wayfinding_BreachRescue']
sign=labels['Aurelion_Art_Sign_Z06_2351aa']
mesh=panel.static_mesh_component.static_mesh
assert mesh and mesh.get_name()=='SM_Aurelion_KIT_Z06WayfindingPanel'
assert panel.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
assert not panel.get_actor_enable_collision()
assert sign.get_name()=='TextRenderActor_113'
text=sign.get_component_by_class(unreal.TextRenderComponent)
assert str(text.text)=='BREACH RESCUE\nCAPTURE GALLERY'
assert text.world_size==32
assert text.horizontal_alignment==unreal.HorizTextAligment.EHTA_CENTER
assert text.vertical_alignment==unreal.VerticalTextAligment.EVRTA_TEXT_CENTER
assert not text.get_editor_property('hidden_in_game')
assert abs(panel.get_actor_location().x-1645)<.1
assert abs(panel.get_actor_location().y-7600)<.1
assert abs(panel.get_actor_location().z+425)<.1
assert abs(sign.get_actor_location().x-1631)<.1
assert abs(sign.get_actor_location().y-7600)<.1
origin,extent=panel.get_actor_bounds(False)
assert origin.x-extent.x>1500 and origin.x+extent.x<1675
assert origin.y-extent.y>7200 and origin.y+extent.y<8400
assert origin.z-extent.z>-600 and origin.z+extent.z<100
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
assert sm.get_num_uv_channels(mesh,0)==2
assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
(out/'z06-wayfinding-fresh-load.json').write_text(json.dumps(dict(
    status='fresh_load_pass',map_sha256=before,actor_count=len(actors),
    panel=panel.get_actor_transform().export_text(),text=sign.get_actor_transform().export_text(),
    text_value=str(text.text),text_world_size=text.world_size,
    panel_bounds_center=origin.export_text(),panel_bounds_extent=extent.export_text(),
    mesh=mesh.get_path_name(),uv_channels=sm.get_num_uv_channels(mesh,0),
    collision='none',qualification='Editor load and geometry checks; combat gameplay not exercised.'),indent=2))
print('Z06_WAYFINDING_FRESH_LOAD_PASS')
