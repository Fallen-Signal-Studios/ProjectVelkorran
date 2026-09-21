"""Persist the visually reviewed departure fills; save only M13 and verify reload."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
preview=root/'Saved/Validation/Aurelion/DepartureFaceFill-20260920-185322-4a9895f7'
evidence=json.loads((preview/'face-streaming.json').read_text())
assert evidence['status']=='captured_requires_visual_review' and evidence['error'] is None
assert evidence['maps_unchanged'] and evidence['preview_lights_removed']
assert len(evidence['light_paths'])==4 and not any(r['blocked'] for r in evidence['light_paths'])
assert all((preview/name).exists() for name in ('portrait-before.png','portrait-fill.png','player-before.png','player-fill.png'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
lighting=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_fill.py'))
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';m13=root/'Content/Aurelion/Maps/L_Aurelion_M13.umap'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected=digest(m12)
assert digest(m13)=='ccaf63f21731315b8a19644d455f439222d3ae4f2577a826fb0d1c807cce52cb','M13 changed since the reviewed preview'
before=helpers['snapshot_actor_state'](actors.get_all_level_actors())
assert not any(a.get_actor_label().startswith(lighting['PREFIX']) for a in actors.get_all_level_actors())
new=[]
for name,position,yaw in lighting['SPECS']:
    actor=actors.spawn_actor_from_class(unreal.RectLight,unreal.Vector(*position))
    assert actor
    lighting['configure'](actor,name,position,yaw)
    actor.set_folder_path('Aurelion/00_Lighting')
    new.append(actor)
expected=[lighting['describe'](a) for a in new]
assert expected==evidence['preview_lights'],'Authored values differ from the inspected preview'
assert helpers['snapshot_actor_state']([a for a in actors.get_all_level_actors() if a not in new])==before
shutil.copy2(m13,out/'L_Aurelion_M13.before.umap')
assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
current=list(actors.get_all_level_actors())
saved=[a for a in current if a.get_actor_label().startswith(lighting['PREFIX'])]
assert len(saved)==2
assert sorted([lighting['describe'](a) for a in saved],key=lambda r:r['label'])==sorted(expected,key=lambda r:r['label'])
assert helpers['snapshot_actor_state']([a for a in current if a not in saved])==before
assert digest(m12)==protected and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'departure-fill-saved.json').write_text(json.dumps(dict(status='saved_reloaded',lights=expected,
    m12_unchanged=True,m13_sha256=digest(m13),other_actor_transforms_collision_preserved=True,
    preview=str(preview),qualification='Visual lighting change; saved-map PIE confirmation remains required.'),indent=2))
