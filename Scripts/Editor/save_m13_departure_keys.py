"""Save the reviewed local departure keys in M13 only, with exact reload readback."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
preview=root/'Saved/Validation/Aurelion/DepartureKeyLowOutput-20260920-220849-5133a5f0'
evidence=json.loads((preview/'earned-m13-reload.json').read_text())
assert evidence['status']=='passed_requires_visual_review' and evidence['error'] is None and evidence['maps_unchanged']
review=evidence['departure_key_review'];assert review['temporary_actors_removed'] and len(review['frames'])==6
keys=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_keys.py'))
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';m13=root/'Content/Aurelion/Maps/L_Aurelion_M13.umap'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
protected=digest(m12)
assert digest(m13)=='634fd9172c6b2a54cb2903ba3b1993914d2f6d0c6e218d87d0d61f563bb6e71e'
before=helpers['snapshot_actor_state'](actors.get_all_level_actors())
assert not any(a.get_actor_label().startswith(keys['PREFIX']) for a in actors.get_all_level_actors())
shutil.copy2(m13,out/'L_Aurelion_M13.before.umap')
new=[];expected=[]
for name,position in keys['SPECS']:
    power=keys['INTENSITIES'][name]
    frame=next(f for f in review['frames'] if f['hero']==name and f['intensity']==power)
    assert (preview/frame['file']).exists()
    reviewed=next(light for light in frame['lights'] if light['label']==keys['PREFIX']+name)
    actor=actors.spawn_actor_from_class(unreal.RectLight,unreal.Vector(*position));assert actor
    keys['configure'](actor,name,position);actor.set_folder_path('Aurelion/00_Lighting')
    description=keys['describe'](actor);assert description==reviewed
    new.append(actor);expected.append(description)
assert helpers['snapshot_actor_state']([a for a in actors.get_all_level_actors() if a not in new])==before
assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
current=list(actors.get_all_level_actors());saved=[a for a in current if a.get_actor_label().startswith(keys['PREFIX'])]
assert len(saved)==2
assert sorted([keys['describe'](a) for a in saved],key=lambda v:v['label'])==sorted(expected,key=lambda v:v['label'])
assert helpers['snapshot_actor_state']([a for a in current if a not in saved])==before
assert digest(m12)==protected and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'departure-keys-saved.json').write_text(json.dumps(dict(status='saved_reloaded',lights=expected,
    m12_unchanged=True,m13_sha256=digest(m13),other_actor_transforms_collision_preserved=True,preview=str(preview)),indent=2))
