"""Read-only sensor gallery annotations and decorative component state."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
code=(root/'Scripts/Editor/audit_z08_presentation.py').read_text().replace('audit_z08_presentation','audit_z03_presentation').replace('-5000<p.x<5000 and 18000<p.y<23500 and -1600<p.z<1600','5800<p.x<8200 and -20000<p.y<-14800 and -100<p.z<1000')
scope={};exec(compile(code,'z03_presentation_inventory','exec'),scope)
result=scope['audit_z03_presentation'](actors)
result['decorations']=[]
for a in actors:
    if not a.get_actor_label().startswith('Z03__'):continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        result['decorations'].append(dict(actor=a.get_actor_label(),component=c.get_path_name(),transform=c.get_world_transform().export_text(),mesh=c.static_mesh.get_path_name(),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled())))
(out/'z03-presentation.json').write_text(json.dumps(result,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
