"""Read-only census of Survivor Bend and its approach, including instanced art."""
import json
import os
from pathlib import Path
import time
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); actors=list(subsystem.get_all_level_actors())
by_label={a.get_actor_label():a for a in actors}
lo=(-8700,-11900,-150); hi=(-5300,-7900,1500)
def inside(p):return all(a<=v<=b for v,a,b in zip((p.x,p.y,p.z),lo,hi))
rows=[]
for a in actors:
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh:continue
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(c)
        instances=[]
        if isinstance(c,unreal.InstancedStaticMeshComponent):
            for i in range(c.get_instance_count()):
                t=c.get_instance_transform(i,world_space=True)
                if inside(t.translation):instances.append(dict(index=i,transform=t.export_text()))
            if not instances:continue
        elif not all(o+e>=low and o-e<=high for o,e,low,high in zip((origin.x,origin.y,origin.z),(extent.x,extent.y,extent.z),lo,hi)):continue
        rows.append(dict(actor=a.get_actor_label(),actor_class=a.get_class().get_name(),path=a.get_path_name(),component=c.get_name(),mesh=mesh.get_path_name(),
            actor_transform=a.get_actor_transform().export_text(),transform=c.get_world_transform().export_text(),bounds_origin=origin.export_text(),bounds_extent=extent.export_text(),
            actor_hidden=a.get_editor_property('hidden'),visible=c.get_editor_property('visible'),hidden_in_game=c.get_editor_property('hidden_in_game'),
            collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
            materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],instances=instances))
(out/'z02-architecture.json').write_text(json.dumps(dict(scope='Z02 plus approach inventory only; no save or gameplay acceptance',bounds=[lo,hi],components=rows),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
editor.editor_set_game_view(True)
root=Path(unreal.Paths.project_dir())
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('survivor-bench',unreal.Vector(-6900,-9400,165),unreal.Rotator(pitch=0,yaw=55),80)")
capture=capture.replace('z01-','z02-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_audit_capture','exec'))
