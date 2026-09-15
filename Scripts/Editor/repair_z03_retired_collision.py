"""Persist explicit NoCollision profiles on the six already retired decorations."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
fit=json.loads((root/'Art/Source/Aurelion/Z03WayfindingKit/presentation-baseline.json').read_text());before=[]
for row in fit['decorations']:
    c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
    before.append(dict(actor=row['actor'],visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name())))
(out/'retired-before.json').write_text(json.dumps(before,indent=2))
for row in fit['decorations']:
    c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
    assert c.get_path_name()==row['component'] and c.get_world_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh'] and c.get_editor_property('hidden_in_game')
    c.modify();c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
result=runpy.run_path(str(root/'Scripts/Editor/check_z03_wayfinding.py'))['check_z03_wayfinding'](actors)
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'retired-collision-repair.json').write_text(json.dumps(dict(status='saved',before=before,settings=result),indent=2))
