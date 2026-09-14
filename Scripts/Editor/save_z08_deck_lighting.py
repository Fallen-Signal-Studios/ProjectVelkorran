"""Repurpose only the two measured ceiling-bounce lights, with backup."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
snapshot=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))['snapshot_actor_state'];before=snapshot(actors)
fit=json.loads((root/'Art/Source/Aurelion/Z08VaultKit/deck-lighting-fit.json').read_text(encoding='utf-8-sig'));labels={a.get_actor_label():a for a in actors};changed=set()
for row in fit['lights']:
    a=labels[row['actor']];c=a.get_component_by_class(unreal.RectLightComponent)
    assert a.get_actor_transform().export_text()==row['transform']
    for key,value in row['properties'].items():assert str(c.get_editor_property(key))==value,(row['actor'],key)
    a.modify();c.modify();a.set_actor_location(unreal.Vector(row['position'][0],row['position'][1],fit['height']),False,False);a.set_actor_rotation(unreal.Rotator(pitch=fit['pitch']),False)
    c.set_intensity(fit['intensity']);c.set_attenuation_radius(fit['attenuation_radius']);changed.add(a.get_path_name())
after=snapshot(actors)
assert before.keys()==after.keys()
for key in before:assert before[key][1:]==after[key][1:] if key in changed else before[key]==after[key],key
result=runpy.run_path(str(root/'Scripts/Editor/check_z08_deck_lighting.py'))['check_z08_deck_lighting'](actors)
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-deck-light-fit.json').write_text(json.dumps(dict(status='saved',settings=result,preserved_actor_states=len(actors)-len(changed),preserved_collision_states=len(actors)),indent=2))
exec(compile((root/'Scripts/Editor/review_z08_deck_lighting.py').read_text(),'deck_light_capture','exec'),globals())
