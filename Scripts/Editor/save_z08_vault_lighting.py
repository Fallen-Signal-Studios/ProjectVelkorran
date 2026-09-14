"""Persist the reviewed inward wash and capture both room approaches."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3140
fit=json.loads((root/'Art/Source/Aurelion/Z08VaultKit/lighting-fit.json').read_text());changed={r['actor'] for r in fit['lights']}
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));unchanged=[a for a in original if a.get_actor_label() not in changed];before=helpers['snapshot_actor_state'](unchanged)
for row in fit['lights']:
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.RectLightComponent)
    assert c.get_world_transform().export_text()==row['transform']
    assert {k:str(c.get_editor_property(k)) for k in row['properties']}==row['properties']
    a.modify();c.modify();a.set_actor_rotation(unreal.Rotator(pitch=fit['pitch'],yaw=row['yaw']),False);c.set_source_width(fit['source_width']);c.set_source_height(fit['source_height'])
assert helpers['snapshot_actor_state'](unchanged)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_vault_lighting.py'))['check_z08_vault_lighting'](original)
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-vault-lighting-save.json').write_text(json.dumps(dict(status='saved',geometry=geometry,preserved_actor_states=len(unchanged)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('lit-entry',unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=20,yaw=75),90)")
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('lit-return',unreal.Vector(500,22400,-950),unreal.Rotator(pitch=18,yaw=-105),90)").replace('z01-','z08-')
exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'z08_lighting_review','exec'))
