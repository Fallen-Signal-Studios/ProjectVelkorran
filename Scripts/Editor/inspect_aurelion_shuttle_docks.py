"""Read original ship visibility and place only the editor review camera."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if 'Ship_' not in a.get_actor_label() and a.get_actor_label() not in ('Z12_Dominion_Dock','Z12_Reformation_Dock'): continue
    rows.append(dict(label=a.get_actor_label(),position=a.get_actor_location().export_text(),rotation=a.get_actor_rotation().export_text(),scale=a.get_actor_scale3d().export_text(),hidden=a.get_editor_property('hidden'),components=[dict(name=c.get_name(),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game'),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,collision=str(c.get_collision_enabled())) for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/Shuttles-20260913'; out.mkdir(parents=True,exist_ok=True)
(out/'original-docks.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-1800,45500,500),unreal.Rotator(pitch=-4,yaw=135))
unreal.log('AURELION_DOCKS_INSPECTED')
instances=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,world_space=True); p=t.translation
            if abs(abs(p.x)-3800)<1100 and abs(p.y-47500)<850 and p.z>40:
                instances.append(dict(actor=a.get_actor_label(),component=c.get_name(),index=i,transform=t.export_text(),mesh=c.static_mesh.get_path_name(),collision=str(c.get_collision_enabled()),tags=[str(x) for x in a.tags]))
(out/'overlapping-instances.json').write_text(json.dumps(instances,indent=2),encoding='utf8')
detail=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label()!='Aurelion_Art_M13_Z12_5_9dba0c': continue
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        detail.append(dict(name=c.get_name(),count=c.get_instance_count() if isinstance(c,unreal.InstancedStaticMeshComponent) else None,visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),collision=str(c.get_collision_enabled()),tags=[str(t) for t in c.component_tags],transform=c.get_world_transform().export_text()))
(out/'dressing-components.json').write_text(json.dumps(detail,indent=2),encoding='utf8')
