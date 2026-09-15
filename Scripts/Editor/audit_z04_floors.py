"""Record the saved floor batch before replacing its vendor presentation."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
census=json.loads((root/'Saved/Validation/Aurelion/RemainingEnvironmentAudit-20260914-193105-57b30f38/remaining-environment.json').read_text())
row=next(r for r in census['components'] if r['actor']=='Aurelion_Art_M12_Z04_16_543730')
actor=next(a for a in actors if a.get_actor_label()==row['actor'])
comp=actor.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert actor.get_path_name()==row['path'] and comp.static_mesh.get_path_name()==row['mesh']
assert actor.get_actor_transform().export_text()==row['actor_transform'] and comp.get_world_transform().export_text()==row['component_transform']
assert comp.get_instance_count()==186 and len(row['instances'])==186
row['all_instance_transforms']=[comp.get_instance_transform(i,world_space=True).export_text() for i in range(186)]
room=json.loads((root/'Art/Source/Aurelion/Z04WallKit/room-baseline.json').read_text())
previous=next(r for r in room['components'] if r['actor']==row['actor'])
assert row['all_instance_transforms']==previous['all_instance_transforms']
(out/'floor-baseline.json').write_text(json.dumps(row,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
unreal.log('Z04_FLOOR_BASELINE_PASS')
