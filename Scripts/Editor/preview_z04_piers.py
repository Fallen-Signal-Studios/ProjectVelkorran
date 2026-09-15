"""Replace only the four remaining vendor pier visuals in the relay overlook."""
from pathlib import Path
import hashlib, json, os, runpy, shutil, unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
check = runpy.run_path(str(root / 'Scripts/Editor/check_z04_piers.py'))
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
old = check['baseline']()
mesh_file = root / 'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08EngagedPier.uasset'
mesh_hash = hashlib.sha256(mesh_file.read_bytes()).hexdigest()
untouched = [a for a in actors if a.get_actor_label() != check['LABEL']]
before = helper['snapshot_actor_state'](untouched)
actor = next(a for a in actors if a.get_actor_label() == check['LABEL'])
c = actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name() == old['mesh']
assert [c.get_instance_transform(i, world_space=True).export_text() for i in range(c.get_instance_count())] == old['all_instance_transforms']
c.modify()
c.clear_instances()
c.set_static_mesh(unreal.load_asset(check['MESH']))
c.set_editor_property('override_materials', [])
c.set_collision_profile_name('NoCollision')
c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for p, yaw in check['placements']():
    c.add_instance(unreal.Transform(location=unreal.Vector(*p), rotation=unreal.Rotator(yaw=yaw), scale=unreal.Vector(1, 1, 1)), world_space=True)
assert helper['snapshot_actor_state'](untouched) == before
settings = check['check_z04_piers'](actors)
persist = bool(globals().get('PERSIST_Z04_PIERS', False))
if persist:
    shutil.copy2(root / 'Content/Aurelion/Maps/L_Aurelion_M12.umap', out / 'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
assert hashlib.sha256(mesh_file.read_bytes()).hexdigest() == mesh_hash
(out / 'z04-piers-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview', settings=settings), indent=2))
capture = (root / 'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix, suffix = capture.split('views=[', 1)[0], capture.split('state=dict', 1)[1]
views = "views=[('west-base',(4450,-12000,165),(2,145),65),('west-capital',(4450,-12000,500),(15,145),65),('east',(9400,-12100,180),(18,34),75),('room',(7100,-12600,165),(10,90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix, 'z04_pier_review', 'exec'), globals())
