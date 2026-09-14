"""Fitted gallery consoles while retaining native request and priority owners."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2923
source=root/'Art/Source/Aurelion/Z07ConsoleKit';fit=json.loads((source/'console-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold','M_Aurelion_UplightLens':'UplightLens'}.items()}
meshes={s['asset']:helpers['import_owned_mesh'](s,source,dest+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
request_mesh=unreal.load_asset(dest+'/Meshes/SM_Aurelion_KIT_RequestConsoleRaisedBase');assert request_mesh
for row in fit['requests']:
    a=by_label[row['actor']];v=a.visual;old=next(c for c in row['components'] if c['name']=='Visual');assert v.static_mesh.get_path_name()==old['mesh']
    children=[(c,c.get_world_transform()) for c in a.get_components_by_class(unreal.SceneComponent) if c.get_attach_parent()==v]
    v.modify();v.set_static_mesh(request_mesh);v.set_relative_location(unreal.Vector(0,12.068254,-5 if 'HandoffToSeleneCage' in row['actor'] else 0),False,False);v.set_relative_rotation(unreal.Rotator(),False,False);v.set_relative_scale3d(unreal.Vector(1,1,1));v.set_editor_property('override_materials',[])
    for c,t in children:
        assert c.get_name()=='StaticMesh';c.modify();c.set_world_transform(t,False,False);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
for row in fit['art']:
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);desk='_67_' in row['actor']
    assert c.static_mesh.get_path_name()==row['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
    c.modify();c.set_static_mesh(meshes['SM_Aurelion_KIT_Z07'+('ControlDesk' if desk else 'PriorityConsole')]);c.set_editor_property('override_materials',[])
    for i,x in enumerate((-100,100) if desk else (-120,120)):
        assert c.update_instance_transform(i,unreal.Transform(location=unreal.Vector(x,15200 if desk else 15700,-900 if desk else -839),rotation=unreal.Rotator(yaw=180 if desk else 90)),True,True,True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_consoles.py'))['check_z07_consoles'](world,original)
persist=bool(globals().get('PERSIST',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z07-consoles-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_CONSOLE_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('story-controls',unreal.Vector(-520,14880,-620),unreal.Rotator(pitch=-15,yaw=70),85)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('priority-controls',unreal.Vector(400,15420,-640),unreal.Rotator(pitch=-20,yaw=145),75)").replace('z01-','z07-')
    exec(compile("p=by_label['Z07_Entry_StandIn']"+capture,'z07_console_review','exec'))
